#!/bin/sh
# Render the editable GitHub social-preview SVGs to their uploadable 1280x640 PNGs.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
MEDIA="$ROOT/docs/media"
TMPDIR=${PURR_TMP:-$ROOT/tmp}
mkdir -p "$TMPDIR"

if ! command -v rsvg-convert >/dev/null 2>&1; then
    echo "render-social-previews: rsvg-convert is required (librsvg)" >&2
    exit 1
fi

for name in normal weird; do
    svg="$MEDIA/social-preview-$name.svg"
    png="$MEDIA/social-preview-$name.png"
    tmp=$(mktemp "$TMPDIR/social-preview-$name.XXXXXX")
    trap 'rm -f "$tmp"' EXIT HUP INT TERM
    rsvg-convert --width 1280 --height 640 "$svg" --output "$tmp"
    mv "$tmp" "$png"
    trap - EXIT HUP INT TERM
    echo "wrote ${png#$ROOT/}"
done
