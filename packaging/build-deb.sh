#!/bin/bash
# Build the purrdometer .deb. Run on the target architecture (the Pi, arm64):
#
#   packaging/build-deb.sh            # dev build: VERSION+git<timestamp>.<sha>
#   RELEASE=1 packaging/build-deb.sh  # release build: exactly VERSION
#
# Output: dist/purrdometer_<version>_<arch>.deb
# Install: sudo apt install ./dist/purrdometer_<version>_<arch>.deb
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

VERSION=$(tr -d '[:space:]' < VERSION)
if [[ ${RELEASE:-0} != 1 ]] && git rev-parse --git-dir >/dev/null 2>&1; then
    sha=$(git rev-parse --short HEAD 2>/dev/null || echo nogit)
    git diff --quiet HEAD 2>/dev/null || sha="$sha.dirty"
    VERSION="$VERSION+git$(date -u +%Y%m%d%H%M%S).$sha"
fi
ARCH=$(dpkg --print-architecture)
PKG=purrdometer
OUT="$ROOT/dist/${PKG}_${VERSION}_${ARCH}.deb"

echo "== build + test"
# A release builds from scratch; a dev build reuses what make already built.
[[ ${RELEASE:-0} == 1 ]] && make clean
make all
make test

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
chmod 755 "$STAGE"

echo "== stage"
for bin in purrdometer-logger purrdometer-web; do
    install -D -m 755 "bin/$bin" "$STAGE/usr/bin/$bin"
    strip --strip-unneeded "$STAGE/usr/bin/$bin"
done
install -D -m 644 data/wheels.ini "$STAGE/usr/share/purrdometer/wheels.ini"

install -d "$STAGE/usr/lib/purrdometer"
(cd python && find purrdometer -name '*.py' -exec install -D -m 644 {} "$STAGE/usr/lib/purrdometer/{}" \;)
cat > "$STAGE/usr/bin/purrdometer-display" <<'EOF'
#!/bin/sh
PYTHONPATH=/usr/lib/purrdometer${PYTHONPATH:+:$PYTHONPATH} exec python3 -m purrdometer.display "$@"
EOF
chmod 755 "$STAGE/usr/bin/purrdometer-display"
install -D -m 755 system/btdiag.sh "$STAGE/usr/sbin/purrdometer-btdiag"

install -D -m 644 packaging/systemd/purrdometer-logger.service "$STAGE/lib/systemd/system/purrdometer-logger.service"
install -D -m 644 packaging/systemd/purrdometer-display.service "$STAGE/lib/systemd/system/purrdometer-display.service"
install -D -m 644 packaging/systemd/purrdometer-web.service "$STAGE/lib/systemd/system/purrdometer-web.service"
install -D -m 644 packaging/etc/purrdometer.conf "$STAGE/etc/purrdometer/purrdometer.conf"
install -D -m 644 system/NetworkManager-wifi-powersave.conf \
    "$STAGE/etc/NetworkManager/conf.d/purrdometer-wifi-powersave.conf"
install -D -m 644 README.md "$STAGE/usr/share/doc/$PKG/README.md"

echo "== control"
install -d -m 755 "$STAGE/DEBIAN"
install -m 755 packaging/debian/postinst packaging/debian/prerm packaging/debian/postrm "$STAGE/DEBIAN/"
install -m 644 packaging/debian/conffiles "$STAGE/DEBIAN/"

# Shared-library dependencies of the binaries, computed the Debian way.
SHLIBS=$(
    tmp=$(mktemp -d)
    mkdir "$tmp/debian"
    printf 'Source: %s\n\nPackage: %s\nArchitecture: any\n' "$PKG" "$PKG" > "$tmp/debian/control"
    (cd "$tmp" && dpkg-shlibdeps -O "$STAGE/usr/bin/purrdometer-logger" "$STAGE/usr/bin/purrdometer-web" 2>/dev/null) | sed -n 's/^shlibs:Depends=//p'
    rm -rf "$tmp"
)
[[ -n $SHLIBS ]] || { echo "dpkg-shlibdeps found no libraries" >&2; exit 1; }
SIZE=$(du -sk --exclude=DEBIAN "$STAGE" | cut -f1)
sed -e "s|@VERSION@|$VERSION|" -e "s|@ARCH@|$ARCH|" -e "s|@SIZE@|$SIZE|" -e "s|@SHLIBS@|$SHLIBS|" \
    packaging/debian/control.in > "$STAGE/DEBIAN/control"
(cd "$STAGE" && find . -type f ! -path './DEBIAN/*' -printf '%P\0' | sort -z | xargs -0 md5sum) > "$STAGE/DEBIAN/md5sums"

echo "== package"
mkdir -p dist
dpkg-deb --root-owner-group -Zxz --build "$STAGE" "$OUT" >/dev/null
dpkg-deb --info "$OUT" | sed -n '/Package:/,/Description:/p'
if command -v lintian >/dev/null; then lintian --no-tag-display-limit "$OUT" || true; fi
echo "built $OUT"
