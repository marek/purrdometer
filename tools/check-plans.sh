#!/usr/bin/env bash
# usage: tools/check-plans.sh [--fix] [plans-dir]
#
# Checks every plan in docs/plans: a dated name, a matching Date line, a
# checklist, a Context without quoted conversation, open boxes that wait for a
# person ending in the marker (the user) or (the user to decide), and a row in
# the index with current counts.
#   --fix  rewrite the Done and Open columns of README.md
# env: ALLOW (space-separated file names exempt from the name rule)
set -euo pipefail
. "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"

FIX=0
case ${1:-} in
	-h | --help)
		usage_from_header
		exit 0
		;;
	--fix)
		FIX=1
		shift
		;;
esac
DIR=${1:-$(repo_root)/docs/plans}
ALLOW=${ALLOW-}
FAIL=0
bad() {
	echo "FAIL $1: $2"
	FAIL=1
}
[ -f "$DIR/README.md" ] || bad README.md "the index is missing"
for f in "$DIR"/*; do
	n=$(basename "$f")
	if [ "$n" = "README.md" ]; then
		continue
	fi
	case " $ALLOW " in *" $n "*) continue ;; esac
	if [[ ! $n =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}-[0-9]{4}-[a-z0-9]+(-[a-z0-9]+)*\.(md|html)$ ]]; then
		bad "$n" "name is not YYYY-MM-DD-HHMM-<slug>.md"
		continue
	fi
	want="Date: ${n:0:10} ${n:11:2}:${n:13:2}"
	head -5 "$f" | grep -q "^$want " || bad "$n" "no line '$want <tz>' under the title"
	grep -qE '^[[:space:]]*- \[[ x]\]' "$f" || bad "$n" "no checkbox list"
	if awk '/^## Context/{c=1; next} /^## /{c=0} c && /^>/' "$f" | grep -q .; then
		bad "$n" "Context quotes the conversation: state the goal instead (writing-context skill)"
	fi
	while IFS=$'\t' read -r loc kind _; do
		if [ "$kind" = stray ]; then
			bad "$loc" "a box that waits for a person or a decision ends with (the user) or (the user to decide), nothing else"
		fi
	done < <("$tools_dir/plan-items.sh" "$f")
	d=$(grep -cE '^[[:space:]]*- \[x\]' "$f" || true)
	o=$(grep -cE '^[[:space:]]*- \[ \]' "$f" || true)
	row=$(grep -F "]($n)" "$DIR/README.md" 2>/dev/null | head -1 || true)
	if [ -z "$row" ]; then
		bad "$n" "not in README.md"
		continue
	fi
	if [ $FIX = 1 ]; then
		N="$n" D="$d" O="$o" perl -0pi -e 's/^(\| \[\Q$ENV{N}\E\]\(\Q$ENV{N}\E\) \|[^|\n]*\|)[^|\n]*\|[^|\n]*\|$/$1 $ENV{D} | $ENV{O} |/m' "$DIR/README.md"
	else
		grep -qE "\| $d \| $o \|[[:space:]]*$" <<<"$row" || bad "$n" "README.md counts are stale (now $d done, $o open): run check-plans.sh --fix"
	fi
done
if [ $FAIL = 0 ]; then
	echo "OK $(find "$DIR" -maxdepth 1 -name '[0-9][0-9][0-9][0-9]-*' | wc -l) plans"
fi
exit $FAIL
