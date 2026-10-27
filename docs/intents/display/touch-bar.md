# The touch bar

> Owns the tab strip that chooses the screen: placement, hit areas,
> orientation and the touch controller.

## Requirement

Four tabs along the bottom edge of the screen, NOW, 30 DAYS, CALENDAR and ALL
TIME, choose the screen with one tap. The user likes the touch bar; it stays
the way to move between screens.

## Rules

| Subject | Rule |
|---|---|
| Placement | The bottom 17 px of the landscape screen, on the same physical edge as the button row in Waveshare's demo |
| Tabs | Equal widths across the full 250 px; the current tab drawn inverted (white on black) |
| Hit area | A tap counts for the tab under it from 6 px above the bar to the bottom edge; anywhere else does nothing |
| Tap | One tap per touch: the first report of a touch switches, holding or sliding does not repeat it |
| Same tab | Tapping the current tab does nothing |
| Orientation | `[display] rotate180` flips image and touch together for a HAT mounted the other way up |
| Controller | GT1151 on I²C address `0x14`, interrupt on GPIO 27, reset on GPIO 22. The interrupt wakes the reader; a slow poll catches a missed edge and finger-up |
| Coordinates | The controller reports panel (portrait) coordinates; they are mapped to landscape so the screen code deals only in what it draws |
