---
name: intent-docs
description: How to write and update a document under docs/intents for purrdometer - what an intent doc is, what it may be written from (the users' decisions, the plans, the plan-time design prose; not a reading of the code), its shape, and what never goes in it (status, history, code walkthroughs, commit hashes). Use before creating, editing or reorganising anything under docs/intents, and whenever a plan or a conversation settles how purrdometer should behave.
---

# Writing intent

`docs/intents/` is one of three co-equal strands (`AGENTS.md`). An
intent doc is the end-state contract for one subject: what it is, why,
and the invariants the code is held to. It is not a manual for the
code and not a diary.

## What an intent doc is written from

1. What the users said and decided, in the working sessions, distilled
   into requirements as the `writing-context` skill describes; an
   intent never quotes the conversation.
2. The plans in `docs/plans/` (their design prose and decisions).
3. The plan-time design prose for the build.
4. The user-facing README and the guide in `docs/guide/`, where they
   state behaviour.

**Not from reading the code.** Until the user lifts this, do not fill a
doc by describing what a source file happens to do: that turns
accidents into contract. The one lookup allowed is a constant's name in
the source, to say where something is set. If a decision was never
made about something, the doc is silent on it. A subject touched once
in conversation gets a short doc; nothing is padded to look complete.

A number goes in only if it was decided (the 250×122 e-paper
resolution, the 1-minute rotation-count interval, the SQLite schema's
column names, the BLE CSC service UUID); tuning thresholds are
referenced as named constants in the source, not listed by value. Where
the code and the doc disagree, one of them drifted: find out which, and
fix that one — in a body of work that touches the tests too.

## Shape

No frontmatter. A title, optionally one `>` line saying what the doc
owns and which doc it defers to, then as many of these sections as
apply:

- `## Requirement` — the invariant that must hold, stated as
  behaviour. "The logger reconnects to the BLE sensor automatically
  after a disconnection."
- `## Design considerations` — why, including the alternative that
  does not work and performance or structural facts where they are
  load-bearing (a per-minute write cadence, SQLite WAL mode, single-row
  display refresh).
- `## Behavior` — what a user, the system or another component
  observes; prose or a table of states.
- `## Error states, corner cases` — what happens when something this
  depends on fails or an edge is hit (the sensor out of range, the
  SQLite file missing, the e-paper controller unresponsive).
- `## Cross-links` — the docs that own the neighbouring subjects.

Tables first, prose second. A domain doc may use its own headings
where these do not fit (a BLE characteristic table, a display screen
table, a Makefile target table). Add to the existing table rather than
appending a paragraph: **integrate**.

## What never goes in

| Never | Where it goes instead |
|---|---|
| Status: "done", "not yet", "planned", "in progress", "TODO" | `docs/plans/` |
| History: "used to", "previously", "was changed to", "the rework" | Commit messages; `docs/intents/bugs/` for a case study |
| A quote of a bug report or a play-test remark | The plan that tracks it |
| Commit hashes, session references, dates of decisions | Plans |
| A walkthrough of functions, files or call order | Nowhere — the code shows it |
| An undecided question | The plan, as an open box for the user |

Name a file, function or constant only to say where a requirement is
enforced or how a user reaches it. Full detail belongs only where
something outside must integrate against it: a BLE characteristic's
semantics, a SQLite schema's fields, a named constant's meaning.

**Out of scope is a statement, not a status.** If something is
deliberately not part of purrdometer (multi-sensor support, remote
data export), the doc says "out of scope" — or is simply silent, when
nothing was ever decided — and names what a future addition must honour.

## Bug case studies

`docs/intents/bugs/` is the one place history is told: how a
non-obvious bug was found — symptoms, hypotheses tried, evidence, root
cause, and a playbook for finding the next one faster. Write one when
the hunt itself is worth keeping. The owning doc still states only the
present-tense fact and links to the case study.

## Workflow

1. Find the nearest doc from `docs/intents/README.md`. Read it and its
   cross-links before changing code or the doc.
2. Change the doc in the same body of work as the tests and the code;
   commit it before or with them.
3. A new doc gets a row in `docs/intents/README.md` and, if it opens a
   new path through the corpus, a reading path.
4. When a plan's prose settles a contract, move that prose here and
   leave a link in the plan.
5. Before finishing, re-read the doc against the "never" table.
   `grep -niE 'status:|not yet|planned|previously|used to|no longer'`
   on the file should find nothing.

The `code-style` skill's rules on concision apply to every sentence
here.
