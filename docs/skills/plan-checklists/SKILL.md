---
name: plan-checklists
description: Every plan for purrdometer is a file in the repo named docs/plans/YYYY-MM-DD-HHMM-<slug>.md, holds a checkbox list, and that list is tracked in the repo and updated as each task finishes. Use when starting any multi-step work, when a plan-mode plan is approved, when asked "what is left" or for a progress check, and before and after every meaningful chunk of progress.
---

# Plans are dated checklist files in the repo, kept current

A plan that exists only in a chat reply, or in a session folder such as
`~/.claude/plans`, is lost to the next session and cannot be followed
by the user while work is running. A plan here is a file they can open,
in the repo, whose boxes show what is done.

## The five rules

1. **Name and place.** Every plan is
   `docs/plans/YYYY-MM-DD-HHMM-<slug>.md` in this repo.
   `<slug>` is lowercase-hyphenated. No other name, no other folder.
2. **Checklist.** Every plan has a `- [ ]` / `- [x]` list, one box per
   concrete, independently finishable task. A plan without checkboxes
   is not a plan; prose sections (context, design, decisions) are
   welcome around the list, not instead of it.
3. **Tracked and updated per task.** The plan is committed when it is
   written. A box is ticked in the file when its task is verified done,
   and that tick is committed with the work it records or right after
   it. Not batched at the end; not ticked because an edit was made
   before it was tested.
4. **The file answers "what is left".** Answer progress questions from
   the current state of the files, in a few lines, and keep working.
   `docs/plans/README.md` is the index: one row per plan with what it
   covers and its open count.
5. **Checked by a tool.** `tools/check-plans.sh` verifies the name
   pattern, the `Date:` line, that a checklist exists, and that the
   plan is in the index with current counts. Run it before committing a
   plan.

## Name and date

The timestamp is local time, taken from `date +%Y-%m-%d-%H%M` at the
moment the plan's content first appears in the session — including a
plan-mode draft written before the file exists in the repo — and is
fixed from then on. Later edits and ticks do not change the name, so
references to the path stay valid.

The line directly under the title is `Date: YYYY-MM-DD HH:MM <tz>`
(for example `Date: 2026-10-14 15:50 CEST`), the same minute as the
file name.

## Context

A plan's `## Context` says what the plan is for: the goal and its
constraints, in plain words, never a transcript of the session. The
`writing-context` skill is the rule; `tools/check-plans.sh` fails a
plan whose Context quotes the conversation.

## A plan that started outside the repo

Plan mode writes its draft to `~/.claude/plans/<random-name>.md`. The
first action after approval is to copy it to `docs/plans/` under its
dated name and commit it; that copy is the one worked from and ticked.
The same goes for a plan that lives in an Artifact or a pasted message:
pull its content into `docs/plans/` as a file, not a link.

## Keeping it current

- **Re-scope openly.** A task that splits in two, a bug found while
  fixing another, a follow-up worth doing: add it to the list when it
  is discovered instead of absorbing it into an existing box.
- **Unverified stays unticked, with the reason.** "(on-device check
  owed)" says why in the middle of a title; a box that only a person can
  close ends its title with the marker `(the user)`, or `(the user to
  decide)` for a decision. The tools skip exactly those, and
  `tools/check-plans.sh` fails any other wording of the same note.
- **Decisions for the user** go in the plan as open boxes that say so,
  and work continues on what does not depend on them.
- **One box, one file.** A task lives in exactly one plan. When a topic
  outgrows its plan, move its boxes to a new dated plan rather than
  copying them.
- **Agree with the task tool.** Where the harness tracks tasks, the
  file and the tracker say the same thing; the file is what the user
  reads.

## What a plan is not

- Not intent. What a plan decides about how purrdometer should behave
  is written into `docs/intents/` (see the `intent-docs` skill); the
  plan links to it and keeps the tasks.
- Not needed for a trivial single step.
- Not a substitute for the `verification` skill: a tick requires having
  done what the box says.

## A body of work gets its own plan, whatever it grew out of

When a report, a question or a decision turns into more than a couple
of tasks, it gets a dated plan of its own that day, named for what it
is; the plan it grew out of keeps the words and links to it. A body of
work that cannot be found in `docs/plans/` does not exist for the next
session.
