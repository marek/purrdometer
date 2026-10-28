# The dashboard on port 80

Date: 2026-10-28 10:39 EDT

## Context

The dashboard was on 8080 only to spare the unprivileged web server the
right to bind a port below 1024. Setup mode's captive portal gave it that
right (`CAP_NET_BIND_SERVICE`), so the reason is gone, and two servers (the
dashboard on 8080, the portal on 80) are one too many. One server on port 80
carries the dashboard, the settings and the setup page; while setup runs,
hotspot clients are sent to `/setup`. 8080 stays as a redirect to port 80, so
old bookmarks work. Settled in [web/dashboard.md](../intents/web/dashboard.md)
and [network/setup.md](../intents/network/setup.md).

## Tasks

- [x] Intents: the dashboard's port, the setup page on the same server, 8080 as a redirect
- [x] One server on `[web] port` (default 80) with every route; hotspot clients in setup redirected to `/setup`
- [x] `[web] legacy_port` (default 8080, 0 for none): any request redirected to the same path on port 80
- [x] The joined screen, the setup page and every doc name `http://whiskerwheel/`
- [x] Self-tests: the captive redirect in and out of setup, the 8080 redirect
- [x] Installed on whiskerwheel: the dashboard on 80, 8080 redirecting, `/etc` taking the new default
