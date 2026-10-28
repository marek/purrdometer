#!/bin/sh
# Each program includes from src/common/ and its own folder only, and
# src/common/ from no program (docs/intents/workspace/source-layout.md).
cd "$(dirname "$0")/../src" || exit 1
programs="logger display web setup"
status=0
for dir in $programs common; do
    for other in $programs; do
        [ "$other" = "$dir" ] && continue
        if grep -rn "#include \"$other/" "$dir"; then
            echo "$dir must not include $other/" >&2
            status=1
        fi
    done
done
[ $status = 0 ] && echo "OK       includes"
exit $status
