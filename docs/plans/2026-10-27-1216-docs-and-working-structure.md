# Docs and working structure

Date: 2026-10-27 12:16 EDT

## Context

purrdometer adopts the documentation and working structure of
`marek/wayland-canvas`: a front-page README with pictures and detail moved
into a user guide, an intent corpus as the design contract, dated plans,
skills, roles, and the agent working agreement, all adapted to a BLE logger,
an e-paper display and a Debian package on a Pi.

## Tasks

- [x] README as a front page: purpose, pictures of the screens, quick install, links into the guide
- [x] User guide in `docs/guide/`: install, the sensor, the screens, configuration, troubleshooting, development
- [x] Intent corpus in `docs/intents/`: overview, glossary, workspace, ble, data, display, two bug case studies
- [x] `AGENTS.md` and `CLAUDE.md`
- [x] Plans for the work so far, and the index
- [x] Skills, roles, `tools/check-plans.sh`, `tools/plan-items.sh`, `scripts/agent-setup.sh` adapted
- [x] `make media` renders in `docs/media/`, checked by eye
- [x] `tools/check-plans.sh` passes; every link in the docs resolves
- [x] Hooks wired in `.claude/settings.json`: branch names, and the collaboration rules repeated with every message
