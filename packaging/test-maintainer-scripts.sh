#!/bin/bash
# Runs the package's maintainer scripts against a copy of /etc/bluetooth and
# /var/lib/bluetooth in a scratch folder, with systemctl and the rest stubbed:
# configure, upgrade, remove, purge and reinstall, each BlueZ setting repaired
# when it was changed, removed or duplicated, and purge removing only what the
# package wrote. Needs the GNU sed, awk and chmod of Debian; no root.
#
#   packaging/test-maintainer-scripts.sh
set -uo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
SCRATCH=${PURR_TMP:-$ROOT/tmp}/maintainer-scripts
rm -rf "$SCRATCH"
mkdir -p "$SCRATCH/bin" "$SCRATCH/bluetooth/AA/11" "$SCRATCH/state"
failures=0

check() { # description, then a command that succeeds when the claim holds
    local what=$1
    shift
    if "$@"; then echo "ok   $what"; else echo "FAIL $what"; failures=$((failures + 1)); fi
}

# The scripts with the system's paths swapped for the scratch ones.
for script in preinst postinst postrm; do
    sed -e "s|^MAIN_CONF=/etc/bluetooth/main.conf|MAIN_CONF=$SCRATCH/main.conf|" \
        -e "s|^BLUEZ_STATE=/var/lib/bluetooth|BLUEZ_STATE=$SCRATCH/bluetooth|" \
        -e "s|^STATE=/var/lib/purrdometer|STATE=$SCRATCH/state|" \
        "$ROOT/packaging/debian/$script" > "$SCRATCH/$script"
done
MAIN=$SCRATCH/main.conf
INFO=$SCRATCH/bluetooth/AA/11/info
check "the scripts name the paths the test swaps" \
    bash -c "grep -q 'main.conf' '$SCRATCH/postinst' && ! grep -q '/etc/bluetooth' '$SCRATCH/postinst' '$SCRATCH/postrm' &&
             ! grep -q '/var/lib/bluetooth' '$SCRATCH/postinst'"

# What the system programs would do, logged.
LOG=$SCRATCH/calls.log
for cmd in systemctl getent adduser install; do
    printf '#!/bin/sh\necho "%s $*" >> "%s"\n' "$cmd" "$LOG" > "$SCRATCH/bin/$cmd"
done
printf '#!/bin/sh\nexit 0\n' > "$SCRATCH/bin/systemd-detect-virt" # a chroot: nothing live is touched
chmod +x "$SCRATCH"/bin/*
export PATH="$SCRATCH/bin:$PATH"

sample() {
    cat <<'CONF'
# Default configuration file for the BlueZ daemon.
[General]
#Name = BlueZ
Privacy = off
Privacy = network/on

[BR]
#PageTimeout=8192

[LE]
#MinConnectionInterval=7
ConnectionSupervisionTimeout=42
#ScanIntervalAutoConnect=300

[GATT]
#Cache = always
CONF
}
cached() { printf '[General]\nName=sensor\n\n[ConnectionParameters]\nMinInterval=6\nMaxInterval=9\n\n[LongTermKey]\nKey=1\n' > "$INFO"; }
run() { : > "$LOG"; sh "$SCRATCH/$1" "${@:2}"; }
count() { grep -c -- "$1" "$MAIN" || true; }
section_of() { # which section holds the line matching $1
    awk -v pat="$1" '/^\[/ { cur = $0 } $0 ~ pat { print cur }' "$MAIN"
}
exactly_once() {
    [ "$(count '^ConnectionSupervisionTimeout *= *600$')" = 1 ] && [ "$(count '^Privacy *= *device$')" = 1 ] &&
        [ "$(count '^ConnectionSupervisionTimeout')" = 1 ] && [ "$(count '^Privacy')" = 1 ] &&
        [ "$(count '^# purrdometer:')" = 2 ] &&
        [ "$(section_of '^ConnectionSupervisionTimeout')" = "[LE]" ] && [ "$(section_of '^Privacy')" = "[General]" ]
}
setting_repaired() { run postinst configure && exactly_once; }
called() { grep -q -- "$1" "$LOG"; }

sample > "$MAIN"
cached
run postinst configure
check "first install: each BlueZ setting once, in its section, under its marker" exactly_once
check "first install: other settings of the keys are replaced, comments kept" \
    bash -c "grep -q '^#MinConnectionInterval=7' '$MAIN' && grep -q '^#Name = BlueZ' '$MAIN' && grep -q '^\[GATT\]' '$MAIN' &&
             ! grep -q 'network/on' '$MAIN' && ! grep -q 'ConnectionSupervisionTimeout=42' '$MAIN'"
check "first install: BlueZ is restarted and its cached connection parameters cleared" \
    bash -c "grep -q 'systemctl stop bluetooth' '$LOG' && grep -q 'systemctl start bluetooth' '$LOG' &&
             ! grep -q ConnectionParameters '$INFO' && grep -q '^\[LongTermKey\]' '$INFO' && grep -q 'Name=sensor' '$INFO'"
check "first install: the user, the state directory and the services" \
    bash -c "grep -q 'getent passwd purrdometer' '$LOG' && grep -q 'install -d' '$LOG' && grep -q 'systemctl enable' '$LOG'"
first=$(cat "$MAIN")

cached
run postinst configure
check "upgrade: the file is unchanged" [ "$(cat "$MAIN")" = "$first" ]
check "upgrade: BlueZ is left running and its cache alone" \
    bash -c "! grep -q 'stop bluetooth' '$LOG' && grep -q ConnectionParameters '$INFO'"

sed -i 's/^ConnectionSupervisionTimeout = 600/ConnectionSupervisionTimeout = 300/' "$MAIN"
cached
check "upgrade: a value edited under its marker is put back" setting_repaired
check "upgrade: and BlueZ's cache is cleared for it" bash -c "! grep -q ConnectionParameters '$INFO'"

sed -i '/^Privacy = device/d' "$MAIN"
check "upgrade: a setting removed, its marker left, is put back" setting_repaired

sed -i '/^# purrdometer: connect with a random address/d' "$MAIN"
check "upgrade: a marker removed, its setting left, is put back once" setting_repaired

sed -i 's/^Privacy = device/Privacy = device\nPrivacy = off/' "$MAIN"
check "upgrade: a setting duplicated is made one" setting_repaired

sed -i '/^# purrdometer:/d' "$MAIN"
check "upgrade: both markers gone, both settings left: markers come back, settings stay one" setting_repaired

printf '[LE]\nConnectionSupervisionTimeout = 600\n[General]\nPrivacy = device' > "$MAIN"
check "upgrade: a file without a final newline" setting_repaired

: > "$MAIN"
check "upgrade: an empty file" setting_repaired

sample > "$MAIN"
run postinst configure
cached
run postrm remove
check "remove: the settings are kept" exactly_once
check "remove: nothing of BlueZ is touched" bash -c "! grep -q bluetooth '$LOG' && grep -q ConnectionParameters '$INFO'"

cached
printf '\n[Policy]\n# purrdometer: hand written\nAutoEnable=true\n' >> "$MAIN"
run postrm purge
check "purge: every marker and the setting under it are gone" \
    bash -c "! grep -q 'purrdometer' '$MAIN' && ! grep -q '^ConnectionSupervisionTimeout' '$MAIN' && ! grep -q '^Privacy' '$MAIN'"
check "purge: only a setting of the package's own goes with its marker" grep -q '^AutoEnable=true' "$MAIN"
check "purge: the rest of the file is as the package found it" \
    bash -c "grep -q '^#MinConnectionInterval=7' '$MAIN' && grep -q '^\[GATT\]' '$MAIN' && grep -q '^#Cache = always' '$MAIN'"
check "purge: BlueZ is restarted; its cache is left as it is" \
    bash -c "grep -q 'systemctl restart bluetooth' '$LOG' && grep -q ConnectionParameters '$INFO'"

sample > "$MAIN"
printf '[LE]\n# purrdometer: 6 s LE supervision timeout (x)\n[GATT]\n' > "$SCRATCH/odd.conf"
cp "$SCRATCH/odd.conf" "$MAIN"
run postrm purge
check "purge: a marker with no setting under it takes no other line" bash -c "[ \"\$(cat '$MAIN')\" = \"\$(printf '[LE]\n[GATT]')\" ]"

sample > "$MAIN"
run postinst configure
run postrm purge
run postinst configure
check "reinstall after purge: as a first install" [ "$(cat "$MAIN")" = "$first" ]

sample > "$MAIN"
cp "$MAIN" "$SCRATCH/before.conf"
printf '#!/bin/sh\nexit 3\n' > "$SCRATCH/bin/awk"
chmod +x "$SCRATCH/bin/awk"
run postinst configure 2> "$SCRATCH/err.log"
status=$?
check "a setting that cannot be written stops the install, loudly, and changes nothing" \
    bash -c "[ $status -ne 0 ] && grep -q 'purrdometer: cannot set' '$SCRATCH/err.log' && cmp -s '$MAIN' '$SCRATCH/before.conf' &&
             ! ls '$MAIN'.* >/dev/null 2>&1"
rm -f "$SCRATCH/bin/awk"

sample > "$MAIN"
run postinst configure
cp "$MAIN" "$SCRATCH/installed.conf"
printf '#!/bin/sh\nexit 3\n' > "$SCRATCH/bin/awk"
chmod +x "$SCRATCH/bin/awk"
run postrm purge 2> "$SCRATCH/purge-err.log"
status=$?
rm -f "$SCRATCH/bin/awk"
check "purge that cannot rewrite main.conf warns and still finishes" \
    bash -c "[ $status -eq 0 ] && grep -q 'warning: could not remove' '$SCRATCH/purge-err.log' && cmp -s '$MAIN' '$SCRATCH/installed.conf' &&
             ! ls '$MAIN'.* >/dev/null 2>&1"
check "purge that cannot rewrite main.conf leaves BlueZ unrestarted" bash -c "! grep -q 'restart bluetooth' '$LOG'"

rm -f "$MAIN"
run postinst configure
check "no BlueZ settings file: nothing to configure, nothing created" bash -c "[ ! -e '$MAIN' ]"

echo x > "$SCRATCH/state/settings.ini"
run preinst upgrade 1.0
check "upgrade from settings.ini: renamed once" bash -c "[ -f '$SCRATCH/state/settings.conf' ] && [ ! -e '$SCRATCH/state/settings.ini' ]"
echo old > "$SCRATCH/state/settings.ini"
run preinst upgrade 1.0
check "upgrade from settings.ini: never over a settings.conf" bash -c "[ \"\$(cat '$SCRATCH/state/settings.conf')\" = x ] && [ -f '$SCRATCH/state/settings.ini' ]"

rm -rf "$SCRATCH"
if [ "$failures" -eq 0 ]; then echo "all passed"; else echo "FAILED"; fi
[ "$failures" -eq 0 ]
