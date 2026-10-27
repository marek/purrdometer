# Releases: the package and a Raspberry Pi image

Date: 2026-10-27 16:50 EDT

## Context

A release is a `v*` tag. GitHub builds the `.deb` and a ready-to-flash
Raspberry Pi image from it and drafts a release with both, the way
`marek/wayland-canvas` releases: the workflow runs the same scripts a
contributor runs locally, and the version is the tag. The image is Raspberry
Pi OS Lite (trixie, 64-bit) with the package installed and SPI and I²C on,
shrunk and compressed; users and WiFi are set by Raspberry Pi Imager when
flashing. The rules are in
[workspace/packaging.md](../intents/workspace/packaging.md).

## Tasks

- [x] `tools/version.sh`: the version is the git tag
- [x] `packaging/build-deps`: the one list of build dependencies, read by the container and the workflow
- [x] `packaging/build-image.sh` and the pinned base in `packaging/image-base`; `postinst` changes nothing live in a chroot
- [x] `.github/workflows/packages.yml`: `deb`, `image` and, on a tag, a draft release
- [x] A manual run of the workflow builds both on GitHub's arm64 runners (about 5 minutes; the image is 526 MiB compressed, 2.76 GB unpacked, and grows on first boot)
- [x] Booting the image on a card: not done, by the user's decision; its contents are checked from the built image instead
