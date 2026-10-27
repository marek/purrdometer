---
name: incremental-commits
description: How work is committed in purrdometer - plans, intents, skills and tools as phase-sized commits; source as one proven commit per phase; brief sentence-case messages; explicit pathspecs; the right author; never a push. Use whenever starting or continuing multi-step work that will be committed, and before running any git commit or git push in this repo.
---

# Commit in phases, and never push

The working agreement (`AGENTS.md`) defines the phases a body of work
moves through: a plan, the intent it settles, then tests and
implementation. The history should show that shape rather than one
end-of-task commit that flattens it.

**Commits need the user's go-ahead** ([../../intents/workspace/repos.md](../../intents/workspace/repos.md)).
Stage each phase by explicit pathspec, show what is staged and the proposed
message, and commit when the user says so. A go-ahead for a body of work
covers its phase commits; it never extends to pushing, to rewriting history,
to destructive operations, or to committing other dirty state in the tree
that is not part of the current task.

## What lands where

One repo, so the split is by kind, not by repository:

| What | Shape |
|---|---|
| Plans, intents, skills, roles, tools, hooks, scripts | One commit per phase as it lands |
| Source (C++, Python), Makefile, packaging | One commit per phase of work, once proven with the venues |
| Work on a branch (a worktree, an agent's slice) | One commit per plan box, proven by the tests that cover it |
| A branch promoted to `main` | Squashed: one commit per phase, or per coherent part of a phase, never one per box. Plan-tick and plan-index commits fold into the work they record |

## Branches and main

On a work branch every box is its own commit: small, proven, easy to
review and to back out. Promoting the branch to `main` squashes those
commits into one per phase, or per coherent part of a phase. The
squashed commit's summary names what the phase achieved, and its body
lists the box commits it holds as bullets. The squash keeps the boxes'
order and the final tree byte for byte; a merge that rebases branch
commits onto `main` squashes in the same step. Box-level commits never
reach `main`, and are never pushed.

Squash against the branch's own base, `git reset --soft $(git
merge-base main HEAD)`, never against `main` itself: once `main` has
moved past that base, a soft reset onto it commits the branch's tree,
which silently reverts whatever landed on `main` in between. Against
the merge base, a moved `main` shows up as a fast-forward that fails.

## The phases

1. **Plan** — the dated file in `docs/plans/` (see
   `plan-checklists`), committed once it exists, before the work it
   describes.
2. **Intent** — the `docs/intents/` change that states the target
   end-state, committed before or with the code it governs, never as an
   afterthought.
3. **Code and tests** — one commit for the phase once the tests that
   cover it pass (the `verification` skill; `make test` and `make preview`
   are run once per batch of work), plus the on-device check the change
   calls for (anything that changes BLE handling, display or packaging
   must be tested on the Pi before it is called done). A mechanical
   change that is scripted and verified on its own is its own commit.
4. **Tick** — the plan boxes for what just landed, committed with or
   right after it.

A small fix that is one phase needs one commit. This is about not
collapsing real phases, not manufacturing them.

## Messages

- One summary line: brief and direct, starts with a capital, no full
  stop, at most 72 characters, says what changed in whatever wording is
  clearest (`Display wraps long distance values cleanly`); it does not
  have to start with a verb. GitHub shows only this line. A name keeps
  its own spelling even when it comes first (`sdbus-c++ 2.0 dependency
  added`): do not reorder a sentence to get a capital in front.
- No word is banned; what matters is that each message says something.
  Everything in a commit is a change, so do not put "Changed" in front
  of every summary by habit. A verb that tells the reader something is
  fine: `Fix BLE reconnect loop on sensor disconnect`, `Extend calendar
  screen to show monthly totals`, `Remove legacy polling path`.
- A body only where the summary is not enough. Anything that is a list
  is written as bullets, one item each, each starting with a capital.
- No sign-offs, no attribution trailers, no session links.

## Before every commit

- **Author.** `git config user.name` and `git config user.email` must
  print the contributor's own identity, never an AI tool's (a
  repo-local `user.email` such as `marek@kudlacz.com` is one
  contributor's clone setting, not a project rule). A fresh clone has
  no repo-level setting — set it before the first commit, stop if it
  prints anything else.
- **Scope.** `git status --short` first. Stage only this phase's files
  by path (`git add <path> ...`), never `git add -A` or `git add .`,
  and commit with a trailing pathspec:
  `git commit -m "..." -- <path> <path>`. Another session may have
  files staged or dirty in the same tree; they must not ride along.
- **No strays.** No debug prints, temporary commands, scratch files or
  `tmp/` output in the diff.
- **A swept-in file** is fixed with `git reset --soft HEAD~1` and a
  correctly scoped commit, never `reset --hard`.

## Pushing

- **Never push** unless the user asks for that push at that moment. A push
  step inside an approved plan is not that.
- No force push, no history rewrite, without an explicit ask from the
  user at that moment; never drop another contributor's commits.
