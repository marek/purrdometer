#!/bin/bash
# Build, test and package in a Debian trixie arm64 container (podman or docker),
# on any arm64 machine with more memory than a Pi Zero.
#
#   packaging/build-in-container.sh             # make test, then dist/*.deb
#   packaging/build-in-container.sh make test   # any command, in the container
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
ENGINE=$(command -v podman || command -v docker) || { echo "needs podman or docker" >&2; exit 1; }
IMAGE=purrdometer-build:trixie

"$ENGINE" build -q --platform linux/arm64 -t "$IMAGE" -f "$ROOT/packaging/Containerfile" "$ROOT/packaging" >/dev/null
# A podman VM's clock falls behind the Mac's while it sleeps, and make then sees
# sources from the future: step it before building.
if [[ $ENGINE == *podman ]] && podman machine inspect >/dev/null 2>&1; then
    podman machine ssh sudo chronyc -a makestep >/dev/null 2>&1 || true
fi
[[ $# -gt 0 ]] || set -- packaging/build-deb.sh
exec "$ENGINE" run --rm --platform linux/arm64 -v "$ROOT:/src" "$IMAGE" "$@"
