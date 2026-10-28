# Setup from a phone

> Owns setup mode: when it starts and ends, the hotspot, what the e-paper shows,
> the setup page, and the privileged helper's boundary. The rest of the web
> dashboard is owned by [../web/dashboard.md](../web/dashboard.md).

## Requirement

A purrdometer with no WiFi, or whose owner asks for it, is set up from a
phone: no keyboard, screen or Raspberry Pi Imager. It starts a WiFi hotspot
whose password is made on the device and shown on the e-paper, so only someone
at the wheel can join; the setup page chooses the home WiFi, sets the login
password and the wheel. There is no default user and no default password.
Setup never pushes a working Pi off its network by itself.

## When setup mode starts

| Trigger | Rule |
|---|---|
| First boot with no WiFi | No WiFi connection profile exists other than the hotspot's own: setup starts and stays until a WiFi is joined |
| The boot hold | For 10 s after the screen program starts, the screen shows a WiFi icon and "Hold to set up WiFi"; holding anywhere on the screen for 3 s starts setup |
| The long press | While running, a press on the touch bar held 3 s past a tap starts setup |
| Never | Because a configured WiFi cannot be reached: a router that is down must not push the Pi off its network |

## Holds show that they are working

| Moment | The screen |
|---|---|
| A press passes 0.7 s (a tap is shorter) | The touch bar becomes "Hold for WiFi setup" with a strip that fills over 3 s, redrawn about once a second |
| Released early | The tabs come back; nothing else happens; the tab shown stays |
| Held to the end | "Starting WiFi setup…", then the setup screen |
| Boot hold | The same strip under the WiFi icon |

A tab switches when the finger lifts after less than 0.7 s, so a long press
never switches tabs on its way.

## The hotspot

| Subject | Rule |
|---|---|
| Run by | NetworkManager, in shared mode (its own DHCP and DNS through dnsmasq) |
| Name | `purrdometer-` and the last four hex digits of the Pi's WiFi address |
| Password | 10 characters drawn from `/dev/urandom` over letters and digits that cannot be mistaken for each other, new each time setup starts |
| Address | The Pi is `10.42.0.1`, NetworkManager's shared-mode default |
| DNS | Every name answers `10.42.0.1` (`/etc/NetworkManager/dnsmasq-shared.d/purrdometer.conf`), so a phone's captive-portal check lands on the setup page |
| Before it starts | Networks are scanned: the Pi Zero 2 W's radio cannot scan while it is an access point |
| Radio | 2.4 GHz, the only band the CYW43436 has |

## The e-paper in setup mode

The setup screen replaces the four screens: "WiFi setup", the hotspot name and
password in large text, a QR code that joins the hotspot
(`WIFI:T:WPA;S:<name>;P:<password>;;`), the address `http://10.42.0.1`, and
the minutes left. While joining: "Joining <network>…". After joining: the new
address for a minute, then the normal screens. On failure: why, and setup
goes on.

## The setup page

| Subject | Rule |
|---|---|
| Where | `/setup` on the dashboard server; anything on port 80 redirects there while setup runs, and to the dashboard otherwise |
| Who | Only clients on the hotspot (`10.42.0.0/24`) while setup runs; others get a page saying setup is not running. The form carries a token tied to a cookie |
| WiFi | The networks scanned, strongest first, or a name typed in; its password |
| Login | The login user (the first account with uid 1000–59999) and a new password, twice; optionally SSH on, with a public key |
| Wheel | A preset or a measured diameter, saved as the settings page saves it |
| Submit | The request goes to the helper; the page then shows progress from the helper's state |

## Setup ends

| Case | Result |
|---|---|
| Joined | The new WiFi profile stays, the login and SSH changes apply, the hotspot stops; the screen shows the address the Pi got |
| The WiFi cannot be joined | The new profile is deleted, the hotspot comes back with the reason; nothing else changed |
| 10 minutes with no submit | The hotspot stops and the Pi returns to the WiFi it had. On a first boot with no WiFi there is nothing to return to, so setup restarts |
| Never | A working profile removed before a new one has been joined |

## The helper

`purrdometer-setup` runs as root and does only this: scan, run and stop the
hotspot, add and remove WiFi profiles, set the login user's password and
shell, turn SSH on and add a key. It takes requests from files the
unprivileged programs write in `/var/lib/purrdometer` (`setup-request`, touched
to start; `setup-apply`, the submitted form), and publishes its state in
`/run/purrdometer-setup/` (`state`, `networks`), which the screen and the page
read. The web server and the screen gain no privileges.

## Proven by

`purrdometer-setup --selftest`: NetworkManager output parsing, the password
alphabet, the state files, validation of a submitted form.
`purrdometer-display --selftest`: the hold timing, the setup screen and its QR
code. `purrdometer-web --selftest`: the setup page, its access rule and its
submission. On whiskerwheel: a phone joining the hotspot opens the page, and
setup puts the Pi on the home WiFi.
