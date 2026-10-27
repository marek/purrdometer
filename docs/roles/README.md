# Roles

A role is the persona an agent works as for a task. It says what to
read first, what the role does, what it does not do, and what it hands
over when finished. A role narrows the work; it never widens what an
agent may do — `AGENTS.md` binds every role.

## Using a role

- **Adopt it.** In the main session, "as tester, check the BLE
  reconnect path" means: read `tester.md`, read what it lists under
  Reads first, and work inside its Does and Does not.
- **Hand work to it.** `scripts/agent-setup.sh` links every role into
  `.claude/agents`, so a role can be started as a subagent by name. The
  subagent's report is its Hands over section.
- **One role at a time.** A task that needs two (develop, then test)
  switches role at the boundary and says so. The tester does not fix
  what it finds; the developer does not declare its own work tested.

## The roles

| Role | Stance | Typical hand-over |
|---|---|---|
| [developer](developer.md) | Implements against the nearest intent; touches all three strands | A proven commit and ticked plan boxes |
| [tester](tester.md) | Proves or disproves with the venues; changes no code | Pass or fail with the log lines or pictures that show it |
| [reviewer](reviewer.md) | Checks a diff against intent, the BlueZ D-Bus API / Bluetooth CSC spec and code style | Findings, most serious first |
| [planner](planner.md) | Writes plans and intents; collects decisions for the user | A dated plan and updated intent docs |

## Writing a role

One file, `docs/roles/<name>.md`, with `name` and `description`
frontmatter (the description says when to use the role) and these
sections: Purpose, Reads first, Does, Does not, Hands over. Add the row
above and to the table in `AGENTS.md`, then run
`scripts/agent-setup.sh`. A role is a stance, not a subject area:
knowledge about BLE or the display belongs in `docs/intents`, not in a
"BLE engineer" role.
