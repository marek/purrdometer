#!/usr/bin/env bash
# UserPromptSubmit hook: repeats the collaboration rules (full detail in CLAUDE.md) with every user message,
# rather than relying on the file being read once at session start. Needs jq.
set -euo pipefail
jq -n '{
  hookSpecificOutput: {
    hookEventName: "UserPromptSubmit",
    additionalContext: "Standing reminder (full detail in CLAUDE.md and AGENTS.md): (1) Trust the user'"'"'s bug reports and stated facts as ground truth - do not ask them to reverify, do not explain a repeated report away. (2) When an instruction is explicit and literal, do exactly that - do not substitute your own design. A question gets an answer and never a change: answer it first, in a few lines, then carry on with work already under way (the answering-questions skill). Answer every point of a burst of messages. (3) Before writing into a file governed by a convention (a plan in docs/plans, an intent doc, a skill), check the actual rule text now; after an edit made in response to a correction, look at the actual result before calling it resolved. (4) Plans are docs/plans/YYYY-MM-DD-HHMM-<slug>.md with checkboxes, ticked and committed per task. (5) Never push unless the user asks for that push now, and never commit without the user'"'"'s go-ahead. (6) Work is attributed to the humans who direct it: commits carry the contributor'"'"'s own git identity, and branches are <first name>/<short-description> from git config user.name - never claude/..., never an AI attribution trailer."
  }
}'
