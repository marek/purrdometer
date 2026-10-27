#!/usr/bin/env bash
# usage: tools/version.sh
#
# The project version, from the git tag (docs/intents/workspace/packaging.md).
# The package and the image read it here and nowhere else:
#
#   a commit tagged v<version>      <version>, the highest of several tags
#   past the last tag               <version>.r<commits since>.g<short hash>
#   no tag behind the commit        0.0.0.r<commits>.g<short hash>
#
# A version dpkg cannot take (a hyphen, an underscore) fails here rather than
# in the middle of a package build.
set -euo pipefail
. "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"

REPO=$(repo_root)

case ${1:-} in
	-h | --help)
		usage_from_header
		exit 0
		;;
	?*) die "unknown argument: $1 (try --help)" ;;
esac

git_() { git -C "$REPO" "$@"; }

[ "$(git_ rev-parse --show-toplevel 2>/dev/null)" = "$REPO" ] || die "$REPO is not a git checkout"

if base=$(git_ describe --tags --match 'v[0-9]*' --abbrev=0 2>/dev/null); then
	commit=$(git_ rev-list -n 1 "$base")
	tag=$(git_ tag --points-at "$commit" --list 'v[0-9]*' | sort -V | tail -n 1)
	count=$(git_ rev-list --count "$commit..HEAD")
	if [ "$count" -eq 0 ]; then
		version=${tag#v}
	else
		version="${tag#v}.r$count.g$(git_ rev-parse --short=7 HEAD)"
	fi
else
	version="0.0.0.r$(git_ rev-list --count HEAD).g$(git_ rev-parse --short=7 HEAD)"
fi

if [[ ! $version =~ ^[0-9][0-9A-Za-z.+~]*$ ]]; then
	die "'$version' is not a version dpkg takes: a digit first, then only letters, digits, '.', '+' and '~'"
fi
printf '%s\n' "$version"
