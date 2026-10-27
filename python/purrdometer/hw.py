"""Waveshare 2.13" Touch e-Paper HAT (V4 panel, SSD1680) + GT1151 touch.

A small driver based on Waveshare's TP_lib (epd2in13_V4.py, gt1151.py,
epdconfig.py, MIT-style licence). Changes: one class per device, no
busy-polling threads, and touch coordinates mapped to the landscape
framebuffer used by the renderer.

Panel RAM is 122x250 portrait. The renderer draws 250x122 landscape. In
landscape coordinates (lx, ly), the touch panel reports tx == ly and
ty == 249 - lx (same mapping as the vendor demo's button row, with the
touch strip along the bottom). ``rotate180`` flips both the image and touch.
"""

from __future__ import annotations

import logging
import threading
import time

import gpiozero
import spidev
from PIL import Image

try:
    from smbus2 import SMBus, i2c_msg
except ImportError:  # pragma: no cover
    SMBus = None

log = logging.getLogger(__name__)

WIDTH, HEIGHT = 250, 122  # landscape, as drawn by the renderer
PANEL_W, PANEL_H = 122, 250

RST_PIN, DC_PIN, BUSY_PIN = 17, 25, 24
TOUCH_RST_PIN, TOUCH_INT_PIN = 22, 27
TOUCH_ADDR = 0x14


class EPaper:
    def __init__(self, rotate180: bool = False):
        self.rotate180 = rotate180
        self.rst = gpiozero.LED(RST_PIN)
        self.dc = gpiozero.LED(DC_PIN)
        self.busy = gpiozero.Button(BUSY_PIN, pull_up=False)
        self.spi = spidev.SpiDev(0, 0)
        self.spi.max_speed_hz = 10_000_000
        self.spi.mode = 0
        self.asleep = True

    # -- low level ---------------------------------------------------------
    def _cmd(self, c: int, *data: int) -> None:
        self.dc.off()
        self.spi.writebytes([c])
        if data:
            self._data(bytes(data))

    def _data(self, data: bytes) -> None:
        self.dc.on()
        self.spi.writebytes2(data)

    def _wait(self, timeout: float = 10.0) -> None:
        end = time.monotonic() + timeout
        while self.busy.value == 1:
            if time.monotonic() > end:
                log.warning("e-paper busy timeout")
                return
            time.sleep(0.01)

    def _reset(self, hold_ms: float = 2) -> None:
        self.rst.on()
        time.sleep(0.02)
        self.rst.off()
        time.sleep(hold_ms / 1000)
        self.rst.on()
        time.sleep(0.02)

    def _window(self) -> None:
        self._cmd(0x01, 0xF9, 0x00, 0x00)              # driver output control
        self._cmd(0x11, 0x03)                          # data entry mode
        self._cmd(0x44, 0, (PANEL_W - 1) >> 3)         # RAM x range
        self._cmd(0x45, 0, 0, (PANEL_H - 1) & 0xFF, (PANEL_H - 1) >> 8)
        self._cmd(0x4E, 0)                             # RAM x counter
        self._cmd(0x4F, 0, 0)                          # RAM y counter

    def _buffer(self, image: Image.Image) -> bytes:
        img = image.convert("1")
        img = img.rotate(90 if self.rotate180 else 270, expand=True)
        return img.tobytes("raw")

    # -- public ------------------------------------------------------------
    def full(self, image: Image.Image) -> None:
        """Full refresh (flashes, clears ghosting). Also sets the partial base."""
        self._reset()
        self._wait()
        self._cmd(0x12)                                # SW reset
        self._wait()
        self._window()
        self._cmd(0x3C, 0x05)                          # border waveform
        self._cmd(0x21, 0x00, 0x80)                    # display update control
        self._cmd(0x18, 0x80)                          # internal temperature sensor
        self._wait()
        buf = self._buffer(image)
        self._cmd(0x24)
        self._data(buf)
        self._cmd(0x26)                                # base image for partials
        self._data(buf)
        self._cmd(0x22, 0xF7)
        self._cmd(0x20)
        self._wait()
        self.asleep = False

    def partial(self, image: Image.Image) -> None:
        """Fast partial refresh. Needs a prior full() since the last sleep."""
        if self.asleep:
            return self.full(image)
        self._reset(hold_ms=1)
        self._window()
        self._cmd(0x3C, 0x80)
        self._cmd(0x24)
        self._data(self._buffer(image))
        self._cmd(0x22, 0xFF)
        self._cmd(0x20)
        self._wait()

    def sleep(self) -> None:
        if not self.asleep:
            self._cmd(0x10, 0x01)                      # deep sleep
            self.asleep = True

    def close(self) -> None:
        self.sleep()
        self.spi.close()
        for pin in (self.rst, self.dc, self.busy):
            pin.close()


class Touch:
    """GT1151 capacitive touch. Calls on_tap(x, y) in landscape coords."""

    def __init__(self, on_tap, rotate180: bool = False):
        self.on_tap = on_tap
        self.rotate180 = rotate180
        self.bus = SMBus(1)
        self.rst = gpiozero.LED(TOUCH_RST_PIN)
        self.int = gpiozero.Button(TOUCH_INT_PIN, pull_up=False)
        self._event = threading.Event()
        self._down = False
        self._last = 0.0
        self.rst.on(); time.sleep(0.1)
        self.rst.off(); time.sleep(0.1)
        self.rst.on(); time.sleep(0.1)
        log.info("GT1151 product id %s", bytes(self._read(0x8140, 4)))
        self.int.when_deactivated = self._event.set   # INT pulses low on touch
        threading.Thread(target=self._loop, daemon=True).start()

    def _read(self, reg: int, n: int) -> list[int]:
        w = i2c_msg.write(TOUCH_ADDR, [reg >> 8, reg & 0xFF])
        r = i2c_msg.read(TOUCH_ADDR, n)
        self.bus.i2c_rdwr(w, r)
        return list(r)

    def _write(self, reg: int, value: int) -> None:
        self.bus.write_i2c_block_data(TOUCH_ADDR, reg >> 8, [reg & 0xFF, value])

    def _loop(self) -> None:
        while True:
            # Edge wakes us immediately; the timeout catches a missed edge and finger-up.
            self._event.wait(0.05 if self._down else 1.0)
            self._event.clear()
            try:
                self._poll()
            except OSError as e:
                log.warning("touch i2c error: %s", e)
                time.sleep(1)

    def _poll(self) -> None:
        status = self._read(0x814E, 1)[0]
        if not status & 0x80:
            # no new report: treat 300 ms of silence as finger-up
            if self._down and time.monotonic() - self._last > 0.3:
                self._down = False
            return
        self._last = time.monotonic()
        count = status & 0x0F
        if 1 <= count <= 5:
            p = self._read(0x814F, 8)
            tx, ty = p[1] | p[2] << 8, p[3] | p[4] << 8
            if not self._down:
                x, y = 249 - ty, tx
                if self.rotate180:
                    x, y = WIDTH - 1 - x, HEIGHT - 1 - y
                log.debug("tap raw=(%d,%d) landscape=(%d,%d)", tx, ty, x, y)
                self.on_tap(x, y)
            self._down = True
        else:
            self._down = False
        self._write(0x814E, 0)
