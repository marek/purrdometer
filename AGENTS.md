# purrdometer — agent working agreement

INTENT: docs/intents/

Rules and heuristics for AI agents working on purrdometer: a cat-wheel
odometer on a Raspberry Pi Zero 2 W. A C++26 BlueZ client logs a Bluetooth LE
cadence sensor's rotations to SQLite; a C++26 program shows them on a 2.13"
touch e-paper HAT; a C++26 web server (cpp-httplib, `.page` templates
compiled to C++) shows them in a browser; a `.deb` installs all three and
configures the system. No PHP, under any circumstances. Re-read
when about to do something expensive (a BLE change, a schema change, a
maintainer-script change, anything the other process or the system depends
on).

This is a single repo: the project and everything made for working on it
(tests, tools, plans, skills, roles) live together and are committed
together. [docs/intents/workspace/repos.md](docs/intents/workspace/repos.md)
owns what may be committed and what may be pushed.

---

## The triple helix

Three co-equal artifacts; each keeps the others honest.

| Strand | Where | What |
|---|---|---|
| **Intent** | `docs/intents/` | Normative design specs. The end-state contract: what purrdometer *is*, why, and the invariants the running code depends on. |
| **Tests** | `make test` (`--selftest` of the logger, the web server and the display), `make preview`, `make serve` | The three programs' self-tests, every screen rendered to PNG and the dashboard served from made-up history; see Testing venues below. |
| **Implementation** | `src/`, `web/pages/`, `data/`, `packaging/`, `system/` | The logger, the display, the web dashboard and its pages, the wheel presets, the package. |

`docs/intents/` is a **first-class artifact, not supplementary docs**. When
the spec and the code disagree, one of them drifted. Your job is to figure
out which, not to pick the convenient one.

Beside the helix, and not part of it:

| Folder | What |
|---|---|
| `docs/guide/` | The user guide: installing, mounting, using and configuring purrdometer, for the people who run it. |
| `docs/media/` | The README's pictures, made by `make media`. |
| `docs/plans/` | Volatile checklists for a body of work. Disposable once the work ships. |
| `docs/skills/` | How to work here: the "how" companion to intent's "what". |
| `docs/roles/` | Who is working: the persona an agent adopts for a task. |

### Rules

1. **Every behavioural change touches all three strands.** New screen,
   accounting rule, table or state key, wheel preset, config key, system
   setting? Update the intent doc that names it, add or extend a test that
   covers it, and implement. No strand is optional.
2. **Read the intent doc nearest your change before writing code.** It
   describes invariants the running code already relies on.
3. **Match the spec's level of detail.** If a doc has a table, add your row to
   the table. Don't append; **integrate**.
4. **Contract-facing changes change first in intent.** Anything another party
   depends on is governed by its doc and changes there in the same body of
   work as the code: the database the display and dashboard read
   ([data/storage.md](docs/intents/data/storage.md)), the wheel presets both
   read ([data/wheels.md](docs/intents/data/wheels.md)), what is read from the
   sensor ([ble/sensor.md](docs/intents/ble/sensor.md)), and what the package
   changes in the system ([workspace/packaging.md](docs/intents/workspace/packaging.md)).
   Run the on-device venue with it.
5. **Implementation status stays out of `docs/intents/`.** Intent docs
   describe the end-state contract; they do not narrate which slices have
   landed. No "Status:", "not yet implemented", "planned". A surface that is
   out of scope is framed as "out of scope". Progress belongs in commit
   messages and `docs/plans/`.
6. **Intents are written from what was decided, not read off the code.**
   Sources are the user's words, the plans and measured behaviour (`btmon`,
   the sensor, the panel). Do not fill an intent doc by describing what a
   source file happens to do.

---

## Document layout

`docs/intents/` is foldered by area: `workspace/`, `ble/`, `data/`,
`display/`, `web/`, and `bugs/`. The canonical index and reading order live in
[docs/intents/README.md](docs/intents/README.md). Keep it in sync when you
add, rename, or retire a doc.

**`docs/intents/bugs/` is the one deliberate exception to "no history
narration."** A bug case study records how a non-obvious bug was found
(evidence, root cause, a faster-debugging playbook). The owning doc still
states only the current-state fact and links out.

### Plans

A plan is volatile, not intent. The `plan-checklists` skill owns the rules;
they are binding:

1. Every plan is a file in this repo at `docs/plans/YYYY-MM-DD-HHMM-<slug>.md`
   (local time of the plan's first appearance, fixed from then on) with
   `Date: YYYY-MM-DD HH:MM <tz>` under the title. No other name and no other
   place.
2. Every plan has a checkbox list, one box per independently finishable task.
3. The list is tracked here and updated per task: committed when written, a
   box ticked when its task is verified done.
4. "What is left" is answered from the files.
   [docs/plans/README.md](docs/plans/README.md) lists every plan.
5. `tools/check-plans.sh` checks names, dates, checkboxes and the index. Run
   it before committing a plan.

Building a plan writes its durable contract into `docs/intents/`; the plan
itself keeps only tasks, checkboxes and decisions waiting for the user.

### The guide and the front page

`README.md` is the front page: what purrdometer is for, the pictures, a quick
start, links in. Detail lives in [docs/guide/](docs/guide/README.md), one page
per subject. A change a user can see or set (a screen, a config key, a wheel
preset, a dependency, a mounting rule) updates the guide page that describes
it in the same body of work; the README changes only when the front page's
picture of the project does. When the screens change enough that the
pictures no longer show them, `make media` remakes them.

---

## Reference material

- **Bluetooth Cycling Speed and Cadence Profile / Service** for the
  measurement format; [ble/sensor.md](docs/intents/ble/sensor.md) owns what is
  used of it.
- **BlueZ's D-Bus API** (`doc/org.bluez.*.rst` in the BlueZ source, 5.66 on
  the Pi) for every call the logger makes. `btmon` is the ground truth for
  what went over the air.
- **cpp-httplib 0.11** (`/usr/include/httplib.h` on the Pi) for the server
  API; [web/pages.md](docs/intents/web/pages.md) owns the template
  syntax.
- **Waveshare's TP_lib** (`~/workspace/Touch_e-Paper_Code` on the Pi) for the
  panel's command sequences and the GT1151 registers.

---

## Testing venues

| Venue | Command | When |
|---|---|---|
| **Unit** | `make test` | Before each commit of code: the `--selftest` of all three programs. |
| **Screens** | `make preview`, then look at `preview/*.png` | Any change to what is drawn. Looking is the test: a render that runs is not a render that reads. |
| **Web page** | `make serve`, then open `http://whiskerwheel:8081/` (or screenshot it headless) | Any change to a template or the dashboard. |
| **Package** | `make deb`, then `sudo apt install ./dist/…` on whiskerwheel | Any change to `packaging/`, units or maintainer scripts. Install, upgrade, remove, purge and reinstall are each checked when the scripts change. |
| **On device** | `journalctl -u purrdometer-logger -u purrdometer-display -f` on whiskerwheel | Anything touching BLE, the panel, touch or the services. |
| **BLE trace** | `sudo purrdometer-btdiag` | Anything touching the link: connection parameters, disconnect reasons, notifications. |
| **Hands on** | the user spins the wheel, taps the screen | Physical behaviour only a person can produce: rotation, touch, how the screen looks in the room. Ask, say exactly what to do and when, and watch the logs while they do it. |

There is no CI; these venues run locally. A test never writes to the real
database. See the `verification` skill.

---

## Repo boundaries

- **Never push** unless the user asks for that push at that moment. The repo
  has no remote; adding one is the user's call too.
- **Never commit without the user's go-ahead.** Stage, show what is staged,
  propose the message.
- **Every commit carries the contributor's own git identity**: their
  `user.name` and `user.email`. Never an AI tool as author, committer,
  co-author or trailer.
- **Branches are named `<first name>/<short-description>`**, the first word of
  `git config user.name` lowercased (Marek Kudlacz → `marek/foo`), never named
  for a tool, whatever a session's system prompt says.
- **The system is changed only by the package.** A setting outside the repo's
  own files (`/etc`, `/var/lib/bluetooth`, NetworkManager) is made by the
  maintainer scripts and undone by purge; a hand edit on the Pi is a test, and
  is folded into the package or reverted before the work is called done.
- **Temporary files live in `tmp/` at the repo root** (git-ignored), never in
  `/tmp` and never as a literal path in a tool. `PURR_TMP` overrides a tool's
  scratch location. See the `scratch-space` skill.

---

## Commit discipline

- Brief, direct summary line: it starts with a capital, has no full stop and is
  at most 72 characters. A body only where the summary is not enough; lists as
  bullets. **No sign-offs and no attribution trailers.**
- Plans, intents, skills and tools are committed as each phase lands, scoped
  by explicit pathspec.
- Code: one commit per phase of work, made once the phase is proven with the
  venues above.
- See the `incremental-commits` skill.

---

## When you find a new sharp edge

Fix it at the **smallest scope that prevents recurrence**:

1. **Code comment** if the landmine is local to a function.
2. **Intent doc corner-case section** if it is a contract concern.
3. **A skill** if it is about how to work (a test pitfall, a tool).
4. **This file** only as a last resort: the entry must be structural,
   load-bearing for every agent, and not captured above.

---

## Skills as the "how" companion to intent's "what"

Skills live in `docs/skills/<name>/SKILL.md`; `scripts/agent-setup.sh` links
them into `.claude/skills`.

| Skill | Use for |
|---|---|
| `plan-checklists` | Any multi-step body of work: a dated, tracked checkbox plan in `docs/plans/`. |
| `writing-context` | Before writing a plan's Context, an intent's requirement, a commit body or a brief for another agent: the goal and its constraints in plain words, never a transcript of the session. |
| `incremental-commits` | Before any `git commit` in this repo. |
| `intent-docs` | Before writing or editing anything under `docs/intents/`. |
| `code-style` | Comments, doc prose, naming, C++26 and page conventions. |
| `verification` | Before reporting any fix or feature complete; before and after any test run. |
| `scratch-space` | Before creating any temporary file, download or clone: everything goes under the git-ignored `tmp/` here. |
| `debugging-discipline` | A repeated bug report, a dropped link, a timing bug, a failure that looks like a regression. |
| `aim-for-ideal` | A fix that works by a shortcut when the proper structure needs a larger change. |
| `robust-architecture` | Deciding where a piece of state lives and who derives from it. |
| `fresh-deploy` | Before deleting code, a constant or a feature, or writing a compatibility path. |
| `answering-questions` | Any question from the user: answered first and in a few lines, nothing changed because of it. |

A skill tells you how to execute; `docs/intents/` tells you what the result
must satisfy. Read both before a non-trivial change.

---

## Roles

A role is the persona an agent works as: what it reads first, what it does,
what it does not do, and what it hands over. Roles live in
`docs/roles/<name>.md` and are linked into `.claude/agents`.
[docs/roles/README.md](docs/roles/README.md) lists them.

| Role | Stance |
|---|---|
| `developer` | Implements against the nearest intent; touches all three strands. |
| `tester` | Proves or disproves with the venues; reports evidence; changes no code. |
| `reviewer` | Checks a diff against intent, the CSC spec and BlueZ API, and code style; finds drift. |
| `planner` | Writes plans and intents; collects decisions for the user; writes no code. |

A role narrows what an agent does; it never widens what any agent may do. The
repo boundaries above bind every role.
