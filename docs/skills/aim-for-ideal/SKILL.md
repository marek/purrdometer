---
name: aim-for-ideal
description: When a task has both a quick path that resembles what was asked and the proper structure that was actually asked for, build the proper one - and when the proper one is genuinely blocked by something larger, name it and its cost up front instead of silently shipping the shortcut. Use whenever a solution works by a shortcut (polling where notification subscription was asked for, a flat file where SQLite was asked for, a special case where the shared query layer was asked for), and whenever tempted to defer part of a request as "optional" or "for later".
---

# Aim for the ideal solution, don't silently settle

The same miss shows up in every project: asked for a structure, the
first version is a shortcut that resembles it. In this repo the
decided answers all have a proper form:

- Asked for BLE notification-driven rotation counting, polling the
  sensor every second would have resembled it; what was asked for is
  subscribing to CSC GATT notifications — the sensor pushes data only
  when the wheel turns.
- Asked to store rotation counts "in a database", a flat CSV would
  have resembled it; what was asked for is SQLite at
  `/var/lib/purrdometer/purrdometer.db`, with a schema the display can
  query.
- Asked for a 30-day summary screen on the e-paper display, hardcoding
  the last 30 values in the layout would have resembled it; what was
  asked for is a query against the SQLite history that always reflects
  the live data.

**The rule.** When the request names a structure or an outcome, build
that. A thing that compiles and resembles the request is not the
request. If the proper solution is blocked by something outside the
immediate change (a larger restructuring, a missing piece, a decision
that is the user's), say so at the start: name the ideal, say what it
takes, and either do it or put the decision in the plan and carry on
with the rest. Do not ship the shortcut and mention the better option
only when asked.

**The Bluetooth CSC profile specification and the BlueZ D-Bus API are
the default references** for BLE handling. Read them before inventing
something smaller. For e-paper display layout, the Waveshare 2.13"
datasheet and the GT1151 touch controller's specification are the
references.

**Never relabel work as optional.** If the user asked for it, it is in
scope. "Too much work" is not a reason to skip; it is a reason to plan.
What is genuinely the user's call is written into the plan as a
decision, and work continues elsewhere.

**This is not scope creep.** It is not "always do the bigger thing".
It is "do what was asked at the quality it was asked for, and surface a
better option as a real decision rather than a silent default".
