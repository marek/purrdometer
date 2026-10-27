#!/bin/bash
# links the agent files kept under docs/ into .claude/ (which is git-ignored): skills and roles.
# the hooks are wired in .claude/settings.json (tracked) and run from tools/hooks/: this removes what an older version wrote for them into the git-ignored .claude/hooks and .claude/settings.local.json, so no hook runs twice.
# safe to run again; removes links whose target is gone.
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1
mkdir -p .claude/skills .claude/agents
for d in .claude/skills .claude/agents; do
  for l in "$d"/*; do [ -L "$l" ] && [ ! -e "$l" ] && rm "$l"; done
done
for s in docs/skills/*/; do
  n=$(basename "$s"); [ -f "$s/SKILL.md" ] && ln -sfn "../../docs/skills/$n" ".claude/skills/$n"
done
for r in docs/roles/*.md; do
  n=$(basename "$r"); [ "$n" = "README.md" ] && continue
  ln -sfn "../../docs/roles/$n" ".claude/agents/$n"
done
chmod +x tools/hooks/*.sh
if [ -d .claude/hooks ]; then
  find .claude/hooks -maxdepth 1 -type l -delete
  rmdir .claude/hooks 2>/dev/null
fi
OLD='reinforce-collaboration-rules\.sh'
LOCAL=.claude/settings.local.json
if [ -f "$LOCAL" ] && grep -qE "$OLD" "$LOCAL"; then
  if ! command -v jq >/dev/null; then
    echo "jq is needed to remove the hooks an older agent-setup.sh wrote into $LOCAL; they now run twice" >&2
  else
    TMP=$(mktemp "$LOCAL.XXXXXX") || exit 1
    jq --arg old "$OLD" '
      def ours: [.hooks[]?.command // empty] | length > 0 and all(test($old));
      if .hooks then
        .hooks |= (map_values(map(select(ours | not))) | with_entries(select(.value | length > 0)))
        | if .hooks == {} then del(.hooks) else . end
      else . end' "$LOCAL" >"$TMP" && chmod --reference="$LOCAL" "$TMP" && mv "$TMP" "$LOCAL" || { rm -f "$TMP"; exit 1; }
    jq -e 'length == 0' "$LOCAL" >/dev/null && rm "$LOCAL"
    echo "removed the hooks an older agent-setup.sh wrote into $LOCAL"
  fi
fi
echo "skills: $(ls .claude/skills | wc -l | tr -d ' ')  roles: $(ls .claude/agents | wc -l | tr -d ' ')"
