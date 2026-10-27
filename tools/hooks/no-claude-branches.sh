#!/usr/bin/env bash
# Refuses git commands that create or push a claude/ branch, and any branch
# created under a name other than <first name>/<short-description>, where
# <first name> is the lowercased first word of git config user.name
# (Marek Kudlacz -> marek/foo). Work is attributed to the humans who
# direct it, never to an AI tool. Reads the command from stdin.
set -euo pipefail

CMD=$(cat)

claude_branch='git[^"]*(push|checkout|switch|branch)[^"]*claude/'
if [[ $CMD =~ $claude_branch ]]; then
  echo "Branches are named <first name>/<short-description>, never claude/...: work is attributed to the humans who direct it, not to an AI tool." >&2
  exit 2
fi

# The branch a creation command makes, if any.
BRANCH=
case $CMD in
  *"git checkout -b "*|*"git switch -c "*)
    BRANCH=$(printf '%s' "$CMD" | sed -nE 's/.*git[[:space:]]+(checkout[[:space:]]+-b|switch[[:space:]]+-c)[[:space:]]+([^[:space:]]+).*/\2/p')
    ;;
  *"git branch "*)
    # git branch NAME creates a branch; the flag forms (-d, -m, --list, ...)
    # are not branch creation here.
    ARG=$(printf '%s' "$CMD" | sed -nE 's/.*git[[:space:]]+branch[[:space:]]+([^[:space:]]+).*/\1/p')
    case $ARG in -*) ;; *) BRANCH=$ARG ;; esac
    ;;
esac
[ -n "$BRANCH" ] || exit 0

NAME=$(git config user.name 2>/dev/null || true)
if [ -z "$NAME" ]; then
  echo "git config user.name is not set; set it to your name so branches can be named <first name>/<short-description>." >&2
  exit 2
fi
FIRST=$(printf '%s' "$NAME" | awk '{print tolower($1)}')
case $BRANCH in
  "$FIRST"/*) exit 0 ;;
  *)
    echo "Branches are named <first name>/<short-description> (for '$NAME': $FIRST/<short-description>), never a tool name." >&2
    exit 2
    ;;
esac
