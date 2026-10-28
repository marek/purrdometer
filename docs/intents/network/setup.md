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
| First boot with no WiFi | NetworkManager says no WiFi connection profile exists other than the hotspot's own: setup starts and stays until a WiFi is joined |
| NetworkManager cannot say | A failed or timed-out `nmcli` is not "no WiFi": nothing starts and the helper asks again every 30 s, until it gets an answer. A setup asked for meanwhile runs as any other; when it ends without a join, the question is asked again, so a Pi that has no WiFi still reaches setup that stays until one is joined |
| The boot hold | For 10 s after the screen program starts, the screen shows a WiFi icon and "Hold to set up WiFi"; holding anywhere on the screen for 3 s starts setup |
| The long press | While running, a press on the touch bar held 3 s past a tap starts setup |
| Never | Because a configured WiFi cannot be reached or cannot be listed: a router that is down, or a NetworkManager that does not answer, must not push the Pi off its network |

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
| Up | Setup is published as running only once NetworkManager has started the hotspot and set it never to start by itself at boot. When it cannot: the hotspot profile is removed, a Pi that had a WiFi goes back to it, and a Pi with none shows why on the starting screen (with the reason a join failed, when that is how it got here) and tries again every 30 s |
| Radio | 2.4 GHz, the only band the CYW43436 has |

## The e-paper in setup mode

The setup screen replaces the four screens: "WiFi setup", the hotspot name and
password in large text, a QR code that joins the hotspot
(`WIFI:T:WPA;S:<name>;P:<password>;;`), the address `http://10.42.0.1`, and
the minutes left. While joining: "Joining <network>…". After joining: the new
address for a minute, and under it what of the form was not applied, if
anything, then the normal screens. On failure: why, and setup
goes on; when the hotspot itself will not start, the starting screen says why.

## The setup page

| Subject | Rule |
|---|---|
| Where | `/setup` on the dashboard's server, on port 80. While setup runs, a hotspot client asking for anything else (a phone's connectivity check, any site) is redirected to `http://10.42.0.1/setup`; everyone else gets the dashboard as always |
| Who | Only clients on the hotspot (`10.42.0.0/24`) while setup runs; others get a page saying setup is not running. The form carries a token tied to a cookie |
| WiFi | The networks scanned, strongest first, or a name typed in; its password |
| Login | The login user (the first account with uid 1000–59999) and a new password, twice; optionally SSH on, with a public key |
| Wheel | A preset or a measured diameter, saved as the settings page saves it |
| Submit | The request goes to the helper; the page then shows progress from the helper's state |

## Setup ends

| Case | Result |
|---|---|
| Joined | The Pi has an address on the new WiFi: the profile stays, the login and SSH changes apply, the hotspot stops; the screen shows the address the Pi got. A join that reports success but leaves no address is not a join |
| Joined, but part of the form was not applied | The WiFi is kept, and the screen says under the address what was not done (the login password, the SSH key, turning SSH on); nothing is reported as done that was not |
| The WiFi cannot be joined | The new profile is deleted, the hotspot comes back with the reason; nothing else changed. When the hotspot cannot come back either, the Pi returns to the WiFi it had |
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

| Subject | Rule |
|---|---|
| Commands | Run with an argument list, never a shell. Each has a deadline that holds however the program behaves, before it has started and while it floods its output: at it the program and everything it started are killed, and the result says it timed out (a program that finished as the deadline came has finished). A program that does not exist, a failed start, a full pipe, a program that never reads its input and a child whose status cannot be collected are failures reported to the caller, never an exit status of 0, and never a hang or a dead helper. Output is read up to a cap |
| Failures | A command that fails is a failure, never an empty answer: every step of setup says whether it worked, and the state published to the screen and the page shows only what was done |
| Login password | Handed to `chpasswd` on its standard input, which reads one account per line: a password with a line break is refused by the page's check and again by the helper |
| The form | `setup-apply` lies in a folder the web server's user writes, so what is at its path may be anything. The helper opens it without following a link and reads and overwrites only a regular file with one name, through the descriptor; a link, a second name for another file, a FIFO, a directory or a file too large to be a form is neither read nor written (a link or an unreadable name is only unlinked). `setup-request` is never opened: the helper reads its modification time, which asks for setup, and nothing else |
| Files it publishes | The state and the networks go to `/run/purrdometer-setup`, written whole. A file that cannot be written is reported and setup goes on: the helper does not stop for it |
| SSH key | The account owns its home, so nothing under it is trusted. The key goes in through descriptors opened without following links: `~/.ssh` and `authorized_keys` that are links, or an `authorized_keys` that is not an ordinary file with one name, are refused, and nothing is written, created or changed through them. Ownership and modes (`0700`, `0600`) are set on the open descriptors, a key already listed (the same type and key field; comments, and a key named only in a comment line or in another key's comment, do not count) is not added again, a key that is not one line starting with its type is refused (options such as `command=` are not ours to grant), and any failure is reported |

## Proven by

`purrdometer-setup --selftest`: NetworkManager output parsing, the password
alphabet, the state files, validation of a submitted form; the form taken from
a path that is a link, a dangling link, a second name, a directory, a FIFO or
too large; the command runner (input and output larger than a pipe, exit
status, a missing program, every way of outliving a deadline, a flood of
output, a child whose status cannot be collected, SIGPIPE); the SSH key installed into homes laid
out to mislead it (links, a second name, a FIFO, a directory, someone else's
home, a key with a second line); and the state machine over a scripted
`nmcli`: a NetworkManager that fails or times out leaves a configured Pi on
its network, a hotspot that does not start is never published, a join that
gives no address fails, and a login that is only partly applied says so.
`purrdometer-display --selftest`: the hold timing, the setup screen and its QR
code. `purrdometer-web --selftest`: the setup page, its access rule and its
submission. On whiskerwheel: a phone joining the hotspot opens the page, and
setup puts the Pi on the home WiFi.
