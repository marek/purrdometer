---
name: scratch-space
description: Every temporary file made while working on purrdometer - logs, preview PNGs under review, downloads, clones, scratch scripts, extracted archives - goes under tmp/ at the repo root, which is git-ignored. Never a session-specific scratchpad, never /tmp, and never a path hard-coded into a tool. Use before creating any temporary file or folder, before writing or editing a tool or script that writes output, before downloading or cloning anything, and when cleaning up at the end of a piece of work.
---

# Scratch space is `tmp/` in the repo

Scratch space belongs to the repo, not to a session. A path that only
one session can reach breaks every later session and hides work from
the user and from other agents running at the same time.

## The rule

All temporary files live under `tmp/` at the repo root:

```
<repo-root>/tmp/
```

It is git-ignored (`tmp/` in `.gitignore`) and never pushed. Nothing
else is a scratch location:

| Not here | Why |
|---|---|
| A session scratchpad (`/tmp/claude-…/<session>/scratchpad`) | Gone or unreachable for the next session |
| `/tmp`, the home folder, Downloads | Outside the repo, invisible to the user and to other sessions, never cleaned |
| `src/`, `display/`, `packaging/`, `docs/` | Tracked. A file there is a product, not scratch |
| `preview/` | Git-ignored output of `make preview`, rewritten on every run; not a scratch folder |

Note: `preview/` at the repo root is the git-ignored output of `make
preview`, rewritten on every run. Do not mix your own scratch PNGs into it; put them in
`tmp/shots/` instead.

## Layout

Give each piece of work its own folder so sessions running at once do
not collide, and so clean-up is one `rm -r`:

| Path | For |
|---|---|
| `tmp/<topic>/` | Your own work: `tmp/ble-logs/`, `tmp/display-experiments/` |
| `tmp/shots/` | Preview PNGs to show the user before deciding whether to keep them |
| `tmp/dl/<name>/` | A download or extracted archive, each in its own new folder |

## In tools and scripts

- A tool finds scratch space from its own location, never from a
  literal path:

  ```bash
  T=$(cd "$(dirname "$0")" && pwd -P); S=${PURR_TMP:-$(dirname "$T")/tmp}; mkdir -p "$S"
  ```

  `PURR_TMP` overrides the location for one run.
- A tool calls its helpers from `tools/`, never from scratch space. A
  script worth running twice is moved into `tools/` or `scripts/` and
  committed; scratch holds only what can be thrown away.
- No absolute path to a session folder, a user name or a machine in a
  tracked file.

## Something worth keeping

Scratch is disposable. If a file should outlive the work:

| It is | It goes |
|---|---|
| A tool or script | `tools/` or `scripts/`, committed |
| A plan, a decision, a finding | `docs/plans/` or `docs/intents/` |
| A bug case study worth keeping | `docs/intents/bugs/` |
| A backup made before a risky operation | Tell the user where it is and that it is temporary; ask where they want it kept |

## Hygiene

- **Disk.** `df -h .` before anything large; ask before anything over
  about 1 GB.
- **Delete when done.** Remove your `tmp/<topic>/` at the end of the
  work; delete downloads and extracted archives as soon as they have
  served their purpose. Logs of a failing run are kept until the
  failure is understood, then removed.
- **Only your own.** Other sessions use `tmp/` at the same time. Remove
  the folders you made, not `tmp/*`.
- **Nothing temporary in a tracked tree.** Debug prints and test
  commands come out of the source before the turn ends.
- **Untrusted files** (downloads, archives) each get a new empty
  folder and are never run from.

## Before finishing

- `git status --short` shows nothing you did not mean to create.
- `grep -rn 'scratchpad\|/tmp/claude' tools scripts docs/skills` finds
  no path (this file's own examples aside).
- Your `tmp/<topic>/` is gone or holds only what is still needed, and
  you have said so.
