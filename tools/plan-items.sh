#!/usr/bin/env bash
# usage: tools/plan-items.sh <plan.md>...
#
# Lists the open boxes of the given plans, one line each: file:line, kind,
# title (tab separated). The title is the box's first line and its wrapped
# lines, joined by single spaces. kind says what the box waits for:
#   user   the title ends with the marker (the user) or (the user to decide):
#          a person, not work
#   stray  a note about the user or about waiting in any other wording:
#          check-plans.sh fails it
#   work   anything else
set -euo pipefail
. "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"

case ${1:-} in
	-h | --help)
		usage_from_header
		exit 0
		;;
	"") die "no plan given (try --help)" ;;
esac

awk '
	function flush(   rest, kind) {
		if (!inbox) return
		inbox = 0
		kind = "work"
		rest = title
		if (sub(/ \((the user|the user to decide)\)$/, "", rest)) kind = "user"
		if (rest ~ /\((the )?user[ ,:;)]|\((waiting|deferred|later|owed)[ )]|the user to decide/) kind = "stray"
		print name ":" start "\t" kind "\t" title
	}
	FNR == 1 {
		flush()
		name = FILENAME
		sub(/.*\//, "", name)
	}
	/^[[:space:]]*- \[ \]/ {
		flush()
		inbox = 1
		start = FNR
		title = $0
		sub(/^[[:space:]]*- \[ \][[:space:]]*/, "", title)
		sub(/[[:space:]]+$/, "", title)
		next
	}
	inbox && /^[[:space:]]+[^[:space:]]/ && !/^[[:space:]]*([-*+]|[0-9]+\.)[[:space:]]/ {
		line = $0
		sub(/^[[:space:]]+/, "", line)
		sub(/[[:space:]]+$/, "", line)
		title = title " " line
		next
	}
	{ flush() }
	END { flush() }
' "$@"
