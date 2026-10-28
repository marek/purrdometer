#!/bin/bash
# Build the purrdometer .deb. Run on the target architecture (arm64): in
# packaging/build-in-container.sh, in the release workflow, or on the Pi.
#
#   packaging/build-deb.sh
#
# The version is the git tag (tools/version.sh). Output: dist/purrdometer_<version>_<arch>.deb
# Install: sudo apt install ./dist/purrdometer_<version>_<arch>.deb
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

VERSION=$(tools/version.sh)
ARCH=$(dpkg --print-architecture)
PKG=purrdometer
OUT="$ROOT/dist/${PKG}_${VERSION}_${ARCH}.deb"

echo "== build + test"
# The directories the package installs to: the programs are built with the same ones.
DIRS=(PREFIX=/usr SYSCONFDIR=/etc LOCALSTATEDIR=/var RUNSTATEDIR=/run)
make "${DIRS[@]}" all
make "${DIRS[@]}" test

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
chmod 755 "$STAGE"

echo "== stage"
for bin in purrdometer-logger purrdometer-web purrdometer-display purrdometer-setup; do
    install -D -m 755 "bin/$bin" "$STAGE/usr/bin/$bin"
    strip --strip-unneeded "$STAGE/usr/bin/$bin"
done
install -D -m 644 data/wheels.conf "$STAGE/usr/share/purrdometer/wheels.conf"
install -D -m 755 system/btdiag.sh "$STAGE/usr/sbin/purrdometer-btdiag"

install -D -m 644 packaging/systemd/purrdometer-logger.service "$STAGE/lib/systemd/system/purrdometer-logger.service"
install -D -m 644 packaging/systemd/purrdometer-display.service "$STAGE/lib/systemd/system/purrdometer-display.service"
install -D -m 644 packaging/systemd/purrdometer-web.service "$STAGE/lib/systemd/system/purrdometer-web.service"
install -D -m 644 packaging/systemd/purrdometer-setup.service "$STAGE/lib/systemd/system/purrdometer-setup.service"
install -D -m 644 packaging/etc/purrdometer.conf "$STAGE/etc/purrdometer/purrdometer.conf"
install -D -m 644 system/NetworkManager-wifi-powersave.conf \
    "$STAGE/etc/NetworkManager/conf.d/purrdometer-wifi-powersave.conf"
install -D -m 644 system/NetworkManager-dnsmasq-shared.conf \
    "$STAGE/etc/NetworkManager/dnsmasq-shared.d/purrdometer.conf"
install -D -m 644 README.md "$STAGE/usr/share/doc/$PKG/README.md"
{ printf "Upstream-Name: purrdometer\nLicense: Apache-2.0\n\n"; cat LICENSE.md; } > "$STAGE/usr/share/doc/$PKG/copyright"
chmod 644 "$STAGE/usr/share/doc/$PKG/copyright"

echo "== control"
install -d -m 755 "$STAGE/DEBIAN"
install -m 755 packaging/debian/preinst packaging/debian/postinst packaging/debian/prerm packaging/debian/postrm "$STAGE/DEBIAN/"
install -m 644 packaging/debian/conffiles "$STAGE/DEBIAN/"

# Shared-library dependencies of the binaries, computed the Debian way.
SHLIBS=$(
    tmp=$(mktemp -d)
    mkdir "$tmp/debian"
    printf 'Source: %s\n\nPackage: %s\nArchitecture: any\n' "$PKG" "$PKG" > "$tmp/debian/control"
    (cd "$tmp" && dpkg-shlibdeps -O "$STAGE"/usr/bin/purrdometer-* 2>/dev/null) | sed -n 's/^shlibs:Depends=//p'
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
