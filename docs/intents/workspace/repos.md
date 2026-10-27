# The repo

> Owns what the repo holds, the identity on its commits, branch names, and
> what may be pushed. A cornerstone: it wins when another doc disagrees.

## Requirement

One repo holds the project and everything made for working on it. Nothing
leaves the machine without the user asking for it at that moment.

## Rules

| Subject | Rule |
|---|---|
| Contents | Source (`src/`, `display/`), tests, packaging, tools, the guide, intents, plans, skills and roles, committed together |
| Not committed | `bin/`, `dist/`, `preview/`, `tmp/`, `__pycache__/`, `*.o`; the database and the saved sensor address (machine state under `/var/lib/purrdometer`) |
| Home | The working copy is `~/workspace/purrdometer` on whiskerwheel; builds and on-device checks happen there |
| Remote | None configured. Pushing anywhere, or adding a remote, is the user's call at that moment; a push step inside an approved plan does not count |
| Identity | Every commit carries the contributor's own `user.name` and `user.email`; never an AI tool as author, committer, co-author or trailer |
| Branches | `<first name>/<short-description>`, the first word of `git config user.name` lowercased (`marek/touch-bar`); never named for a tool |
| Commits | Asked for, not assumed: an agent stages and proposes, the user approves the commit |
| Scratch | Temporary files live in `tmp/` at the repo root (git-ignored); `PURR_TMP` overrides a tool's scratch location |

## Why

Work is attributed to the person who directs it. The repo is the one place
the next session can find what was decided, what is left and how to work, so
nothing that matters may live only in a chat or a session folder.
