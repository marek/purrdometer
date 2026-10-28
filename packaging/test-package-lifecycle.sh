#!/bin/bash
# Installs the built .deb into a throwaway Debian trixie arm64 container (podman or docker) and takes it
# through install, upgrade, a hand-edited setting, remove, reinstall, purge and reinstall, checking
# /etc/bluetooth/main.conf, the user, the state directory and the enabled units after each. Nothing on the
# host changes.
#
# The container runs no systemd, and `systemd-detect-virt --chroot` is made to succeed, so the scripts take
# the path they take building the image: files are written and units enabled, nothing is started or
# restarted. Live service behaviour is the on-device check.
#
#   packaging/test-package-lifecycle.sh [dist/purrdometer_<version>_arm64.deb]
set -uo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
ENGINE=$(command -v podman || command -v docker) || { echo "needs podman or docker" >&2; exit 1; }
DEB=${1:-$(ls -t "$ROOT"/dist/purrdometer_*.deb 2>/dev/null | head -1)}
[[ -f $DEB ]] || { echo "no .deb: run packaging/build-in-container.sh first" >&2; exit 1; }

exec "$ENGINE" run --rm --platform linux/arm64 -v "$(dirname "$DEB"):/dist:ro" -e "DEB=/dist/$(basename "$DEB")" \
    docker.io/library/debian:trixie bash -c '
set -u
export DEBIAN_FRONTEND=noninteractive
failures=0
check() { if "${@:2}"; then echo "ok   $1"; else echo "FAIL $1"; failures=$((failures + 1)); fi; }
printf "#!/bin/sh\nexit 101\n" > /usr/sbin/policy-rc.d; chmod +x /usr/sbin/policy-rc.d
apt-get update -qq >/dev/null
apt-get install -y -qq --no-install-recommends bluez network-manager adduser systemd >/dev/null 2>&1 || { echo "prerequisites did not install"; exit 1; }
printf "#!/bin/sh\nexit 0\n" > "$(command -v systemd-detect-virt)"
CONF=/etc/bluetooth/main.conf
cp $CONF /tmp/main.conf.orig
install_deb() { apt-get install -y -qq --no-install-recommends "$@" "$DEB" >/tmp/apt.log 2>&1; }
once() { [ "$(grep -cE "^ConnectionSupervisionTimeout *= *600$" $CONF)" = 1 ] && [ "$(grep -cE "^Privacy *= *device$" $CONF)" = 1 ] &&
         [ "$(grep -cE "^(ConnectionSupervisionTimeout|Privacy) *=" $CONF)" = 2 ] && [ "$(grep -c "^# purrdometer:" $CONF)" = 2 ]; }
units() { for u in logger display web setup; do systemctl is-enabled purrdometer-$u.service >/dev/null 2>&1 || return 1; done; }
state() { getent passwd purrdometer >/dev/null && [ -d /var/lib/purrdometer ]; }

check "install" install_deb
check "install: each BlueZ setting once, under its marker" once
check "install: user, state directory, four units enabled" bash -c "$(declare -f state units); state && units"
cp $CONF /tmp/installed
check "upgrade (same package again)" install_deb --reinstall
check "upgrade: main.conf unchanged" cmp -s $CONF /tmp/installed
sed -i "s/^ConnectionSupervisionTimeout = 600/ConnectionSupervisionTimeout = 9/" $CONF
check "upgrade over an edited value" install_deb --reinstall
check "upgrade: the value is put back" cmp -s $CONF /tmp/installed
apt-get remove -y -qq purrdometer >/dev/null 2>&1
check "remove: settings, user and state kept" bash -c "$(declare -f once state); CONF=$CONF; once && state"
check "remove: units no longer enabled" bash -c "! systemctl is-enabled purrdometer-logger.service >/dev/null 2>&1"
check "reinstall after remove" install_deb
check "reinstall: as a first install" cmp -s $CONF /tmp/installed
echo keep > /var/lib/purrdometer/history-marker
apt-get purge -y -qq purrdometer >/tmp/purge.log 2>&1
check "purge: no marker, no setting left" bash -c "! grep -qE \"^# purrdometer:|^(ConnectionSupervisionTimeout|Privacy) *=\" $CONF"
check "purge: the rest of main.conf as it was" bash -c "diff <(grep -v \"^\\[LE\\]\$\\|^\\[General\\]\$\\|^\$\" /tmp/main.conf.orig) <(grep -v \"^\\[LE\\]\$\\|^\\[General\\]\$\\|^\$\" $CONF) >/dev/null"
check "purge: history and user kept" bash -c "[ -f /var/lib/purrdometer/history-marker ] && getent passwd purrdometer >/dev/null"
check "reinstall after purge" install_deb
check "reinstall after purge: as a first install" cmp -s $CONF /tmp/installed
check "reinstall after purge: history picked up, units enabled" bash -c "$(declare -f units); [ -f /var/lib/purrdometer/history-marker ] && units"
if [ "$failures" -eq 0 ]; then echo "all passed"; else echo "FAILED"; fi
[ "$failures" -eq 0 ]
'
