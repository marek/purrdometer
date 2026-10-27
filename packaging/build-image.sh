#!/bin/bash
# Build a Raspberry Pi image: Raspberry Pi OS Lite (trixie, arm64, pinned in
# packaging/image-base) with the purrdometer package installed, SPI and I2C
# on, the root filesystem shrunk to fit, compressed with xz. Raspberry Pi OS
# grows the root filesystem to fill the SD card on first boot.
#
#   sudo packaging/build-image.sh dist/purrdometer_<version>_arm64.deb
#
# Output: dist/purrdometer_<version>_raspios-trixie-arm64.img.xz and .sha256.
# Needs root, an arm64 Linux host (or qemu-user-static), losetup, sfdisk,
# e2fsprogs, zerofree, curl and xz. Downloads are cached in tmp/dl/.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
die() { echo "${0##*/}: $*" >&2; exit 2; }
[[ $# -eq 1 && -f $1 ]] || die "usage: sudo ${0##*/} <purrdometer .deb>"
[[ $EUID -eq 0 ]] || die "needs root (loop devices, mounts, chroot)"
DEB=$(realpath "$1")
. "$ROOT/packaging/image-base"

VERSION=$(dpkg-deb -f "$DEB" Version)
TMP=${PURR_TMP:-$ROOT/tmp}
DL=$TMP/dl WORK=$TMP/image MNT=$TMP/image/root
OUT=$ROOT/dist/purrdometer_${VERSION}_raspios-trixie-arm64.img
HEADROOM=768M          # room for the package and its dependencies before shrinking
SLACK_BYTES=$((128 << 20)) # left free after shrinking, for the first boot before the root grows
mkdir -p "$DL" "$WORK" "$ROOT/dist"

LOOP=
cleanup() {
    for m in dev/pts dev proc sys boot/firmware ""; do
        mountpoint -q "$MNT/$m" && umount "$MNT/$m"
    done
    [[ -n $LOOP ]] && losetup -d "$LOOP"
    LOOP=
}
trap cleanup EXIT

echo "== base image $BASE_NAME"
[[ -f $DL/$BASE_NAME.img.xz ]] || curl -fL --retry 3 -o "$DL/$BASE_NAME.img.xz.part" "$BASE_URL" &&
    { [[ -f $DL/$BASE_NAME.img.xz ]] || mv "$DL/$BASE_NAME.img.xz.part" "$DL/$BASE_NAME.img.xz"; }
xz -dc "$DL/$BASE_NAME.img.xz" > "$WORK/image.img"
echo "$BASE_SHA256  $WORK/image.img" | sha256sum -c --quiet || die "the base image does not match packaging/image-base"

echo "== grow the root partition by $HEADROOM"
truncate -s +"$HEADROOM" "$WORK/image.img"
echo ", +" | sfdisk -q -N 2 --no-reread "$WORK/image.img"
LOOP=$(losetup -fP --show "$WORK/image.img")
e2fsck -fy "${LOOP}p2" >/dev/null || [[ $? -le 1 ]]
resize2fs -p "${LOOP}p2" >/dev/null

echo "== install the package"
mkdir -p "$MNT"
mount "${LOOP}p2" "$MNT"
mount "${LOOP}p1" "$MNT/boot/firmware"
for m in proc sys dev dev/pts; do mount --bind "/$m" "$MNT/$m"; done
mv "$MNT/etc/resolv.conf" "$MNT/etc/resolv.conf.purr"
cp -L /etc/resolv.conf "$MNT/etc/resolv.conf"
printf '#!/bin/sh\nexit 101\n' > "$MNT/usr/sbin/policy-rc.d" # start nothing in the chroot
chmod 755 "$MNT/usr/sbin/policy-rc.d"
cp "$DEB" "$MNT/tmp/purrdometer.deb"
chroot "$MNT" /bin/sh -ec '
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y -qq --no-install-recommends /tmp/purrdometer.deb
    apt-get clean
    rm -rf /var/lib/apt/lists/* /tmp/purrdometer.deb
'
rm "$MNT/usr/sbin/policy-rc.d"
mv -f "$MNT/etc/resolv.conf.purr" "$MNT/etc/resolv.conf"

echo "== SPI and I2C on, for the e-paper HAT"
CONFIG=$MNT/boot/firmware/config.txt
for param in spi i2c_arm; do
    if grep -qE "^#?dtparam=$param=" "$CONFIG"; then
        sed -i -E "s/^#?dtparam=$param=.*/dtparam=$param=on/" "$CONFIG"
    else
        echo "dtparam=$param=on" >> "$CONFIG"
    fi
done
grep -E "^dtparam=(spi|i2c_arm)=on" "$CONFIG"
chroot "$MNT" dpkg -s purrdometer | grep -E "^(Status|Version)"
cleanup

echo "== shrink"
LOOP=$(losetup -fP --show "$WORK/image.img")
blocks() { dumpe2fs -h "${LOOP}p2" 2>/dev/null | awk -F: "/^Block $1/ { print \$2 + 0 }"; }
BLOCK=$(blocks size)
# resize2fs -M estimates its minimum generously; repeat until it stops shrinking.
while true; do
    e2fsck -fy "${LOOP}p2" >/dev/null || [[ $? -le 1 ]]
    before=$(blocks count)
    resize2fs -M "${LOOP}p2" >/dev/null 2>&1
    [[ $(blocks count) -lt $before ]] || break
done
resize2fs "${LOOP}p2" $(($(blocks count) + SLACK_BYTES / BLOCK)) >/dev/null
BLOCKS=$(blocks count)
# Zeroed free blocks compress to nothing; leftovers from the install do not.
zerofree "${LOOP}p2"
echo "   $(( $(blocks count) * BLOCK >> 20 )) MiB root filesystem, $(( $(dumpe2fs -h "${LOOP}p2" 2>/dev/null | awk -F: '/^Free blocks/ { print $2 + 0 }') * BLOCK >> 20 )) MiB free"
cleanup
START=$(sfdisk -d "$WORK/image.img" | awk -F'[=,]' '/image.img2/ { gsub(/ /, "", $2); print $2 }')
SECTORS=$(((BLOCKS * BLOCK + 511) / 512))
echo "$START,$SECTORS" | sfdisk -q -N 2 --no-reread "$WORK/image.img"
truncate -s $(((START + SECTORS) * 512)) "$WORK/image.img"
echo "   root filesystem $((BLOCKS * BLOCK >> 20)) MiB, image $(( (START + SECTORS) * 512 >> 20 )) MiB"

echo "== compress"
mv "$WORK/image.img" "$OUT"
xz -f -T0 -6 "$OUT"
(cd "$ROOT/dist" && sha256sum "${OUT##*/}.xz" > "${OUT##*/}.xz.sha256")
ls -l "$OUT.xz"
echo "built $OUT.xz"
