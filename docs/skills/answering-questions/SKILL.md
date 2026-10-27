---
name: answering-questions
description: A question from the user - how far the work has got, why something was done, why something else was not done instead, what something means - gets an answer and never a change. The answer comes first, takes a few lines, and work already under way carries on. Use whenever a message from the user asks something, mid-work or idle, and before acting on a message that reads as both a question and a request.
---

# A question gets an answer, not a change

The user asks questions to understand the work, not to steer it. A
question read as a hidden request (reverting what it asks about,
switching to the alternative it names, starting the work it mentions)
takes the decision away from the user. An answer buried under the
output of the work that follows is an answer they never got.

## Question or request

| The message | Is | What follows |
|---|---|---|
| What is left, how far it has got, is X done, did you run Y | A question | The answer, from the plan files and the task list (the `plan-checklists` skill) |
| Why X was done | A question | The reason X was chosen at the time |
| Why not Y instead | A question | The tradeoff; Y is not started |
| Why X is broken, why X happens | A question | The cause, or what would find it; a fix waits for the user |
| What X means, how X works, where X is | A question | The answer |
| Can you / could you / please do X | A request, asked politely | X is done |
| A question and an instruction together | Both | The instruction is carried out and the question answered |

A message that could be either is a question: answer it, and say what
doing it would take.

## What an answer changes

Nothing.

- No edit, revert, commit, plan box or new task because of the
  question. Looking things up to answer it truthfully (reading files,
  `git log`, the plans, a read-only command) is part of answering.
- A mistake or a better alternative the answer brings to light is
  stated in one line, with what changing it would take. It changes
  when the user says so.
- A question never starts work. Work under way when it arrived carries
  on after the answer as planned; an idle session answers and stops.

## The answer

- **The conclusion is the first line**: yes or no, the reason, the
  state. Evidence follows, only as much as the claim needs.
- **A few lines.** More when more is asked for.
- **True.** The reason actually behind the decision, not one built
  afterwards; "no good reason" when there was none; "I don't know",
  and what would find out, when that is so.
- **Every question in the message** gets its own answer.
- **Neither defended nor given up.** A choice is not wrong because it
  is questioned, nor right because it was made.

## Kept in view

The answer opens the reply. When work carries on in the same turn, the
turn's final message ends with the answer again, so it is the last
thing on screen.

A follow-up question gets its own answer. What the user says about an
answer ("then switch to Y") is an instruction like any other.

The `tools/hooks/reinforce-collaboration-rules.sh` hook repeats the
core of this rule with every user message in Claude Code.
