---
name: writing-context
description: How context is written in purrdometer - a plan's Context section, an intent's requirement and design considerations, a commit message body, a brief for another agent - as the goal and its constraints in plain words, never a transcript of the session. Use before writing or editing any of these, and whenever a request is turned into a plan or an intent.
---

# Writing context

Context tells a reader who never saw the session what the work is for,
so they can carry it out and judge the result. It is written for that
reader. It is not a record of how the work came about.

## What context contains

| Part | What it says |
|---|---|
| The goal | What exists or has changed when the work is done, from the user's point of view |
| Requirements | What the user set as must and must not, stated as facts about the result |
| Settled decisions | Choices already made that the work rests on, one line each; the normative detail lives in the intent it links to |
| Scope | What is deliberately left out, where a reader would otherwise assume it is in |
| Links | The intents and plans it builds on |

A short paragraph or a few bullets, in present tense and in our own
words.

## What context never contains

- **Quoted messages.** No `>` blocks of the conversation and no
  verbatim requests, however short. The request is restated as the
  goal.
- **Remarks on phrasing.** Typos, missing words, tone, how or when
  something was asked.
- **The back-and-forth.** Questions asked and answered along the way,
  options weighed and dropped, corrections ("X, not Y"). Only the
  settled outcome is written, never the route to it. A dropped option
  that still binds the work is written as a requirement.
- **Session narrative.** Who said what; "then", "while it was under
  way", "after the review".
- **Process instructions.** Where to work, how many agents to run, how
  to report: they steer the session, not the work.
- **Names the user asked to keep out of the repo.** Say what the thing
  is instead.

## Turning a request into context

1. Find the goal: what would the user check to call this done?
2. Turn each "must" and "don't" into a requirement about the result.
3. Apply every correction the user made; write only the corrected form.
4. Anything still undecided is a plan box marked "(the user to
   decide)", not context.
5. Read it back as the newcomer: can they act on it without the
   conversation, and is anything there only because it was said?

## Example

| | |
|---|---|
| The request, as it arrives | can the display show weekly distance?? like a 7-day total. oh and don't show it on the calendar screen, only on the summary |
| The context | The display shows the rolling 7-day total distance on the summary screen only; the calendar screen is unchanged. |

The second row is the whole of it: no quote, nothing about the
correction, the comparison kept only as far as it defines behaviour.

## Where it applies

| Place | Rule |
|---|---|
| A plan's `## Context` | This skill; `tools/check-plans.sh` fails a plan whose Context contains a quoted line |
| An intent's Requirement and Design considerations | Written from what the user decided (the `intent-docs` skill), in these terms |
| A commit message body | What changed and why |
| A brief for another agent | The same: it shapes what that agent writes |
