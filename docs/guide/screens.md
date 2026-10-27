# The screens

Four screens, chosen from the touch bar along the bottom edge.

| Tab | Shows |
|---|---|
| **NOW** | Total rotations and kilometres since the first rotation; today's distance and rotations; while the wheel turns, speed in km/h and rpm, otherwise how long ago the last run was; whether the sensor is connected, and its battery |
| **30 DAYS** | A bar per day for the last 30 days (today hatched while it is still going), the 30-day total, the daily average, active days and the best day |
| **CALENDAR** | The last 19 weeks as a grid of days, Monday at the top; darker means further; a dot is a day without running; today is outlined |
| **ALL TIME** | Everything since the first day: bars per day, week or month (whichever fits), with a line showing the total distance growing; active days and the best day |

![NOW](../media/0-now.png)
![30 DAYS](../media/1-30days.png)
![CALENDAR](../media/2-calendar.png)
![ALL TIME](../media/3-alltime.png)

*Rendered from made-up history.*

## The touch bar

Tap a tab to switch to it. The current tab is shown in white on black. If the
picture and the touch are upside down for how the screen is mounted, set
`rotate180 = true` ([configuration](configuration.md#display)).

## How often it redraws

E-paper holds its picture without power and is slow to change, so the screen
redraws only when something on it changed:

- every 5 seconds while the wheel is turning;
- right away when you tap a tab;
- otherwise at most once a minute, and only if the picture changed.

Now and then it does a full refresh that flashes black and white for a couple
of seconds. That clears the faint traces fast updates leave behind. Between
updates the panel sleeps.
