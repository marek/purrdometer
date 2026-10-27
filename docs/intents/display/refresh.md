# Refresh

> Owns when the e-paper panel redraws: partial versus full refreshes,
> ghosting, live updates and sleep.

## Requirement

The screen is always current to within a few seconds while the wheel turns,
and never flashes or wears the panel when nothing changed.

## Rules

| Subject | Rule |
|---|---|
| Change | A frame identical to the one shown is not sent |
| Partial refresh | The normal update: fast, no flash |
| Full refresh | At start, after the panel slept, every `full_refresh_every` partials (20) and at least hourly, to clear ghosting |
| While running | The data is re-read and the screen redrawn every `active_refresh_s` (5 s) |
| Idle | The data is re-read every `idle_refresh_s` (60 s); a redraw happens only if the picture changed (the day rolled over, the sensor connected) |
| Tab tap | Redraws at once with a partial refresh |
| Sleep | After a minute without an update the panel goes into deep sleep; its image stays. The next update wakes it with a full refresh |
| Settings | `[display]` in the config ([../../guide/configuration.md](../../guide/configuration.md)) |

## Why

A partial refresh leaves faint traces of what was there before, which build
up; a full refresh clears them but flashes black and white for about two
seconds. The panel must not be left powered in its high-voltage state for
long, so it sleeps between bursts.
