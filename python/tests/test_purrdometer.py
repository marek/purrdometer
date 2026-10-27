import datetime as dt
import os
import sqlite3
import tempfile
import time
import unittest

from purrdometer import config, data, render, wheels

G5 = wheels.PRESETS["ferris-g5"].circumference_m


def make_db(path, minutes):
    con = sqlite3.connect(path)
    con.executescript(
        "CREATE TABLE minutes (minute INTEGER PRIMARY KEY, revs INTEGER NOT NULL);"
        "CREATE TABLE state (key TEXT PRIMARY KEY, value TEXT);"
    )
    con.executemany("INSERT INTO minutes VALUES (?, ?)", minutes)
    con.execute("INSERT INTO state VALUES ('connected', '1')")
    con.commit()
    con.close()


class DataTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.db = os.path.join(self.dir.name, "t.db")

    def tearDown(self):
        self.dir.cleanup()

    def test_daily_totals_and_today(self):
        today = dt.date.today()
        noon = time.mktime(dt.datetime.combine(today, dt.time(12)).timetuple())
        yesterday = noon - 86400
        make_db(self.db, [(int(noon) // 60, 100), (int(noon) // 60 + 1, 50), (int(yesterday) // 60, 30)])
        s = data.load(self.db, G5)
        self.assertEqual(s.total_revs, 180)
        self.assertEqual(s.today_revs, 150)
        self.assertEqual(s.daily[today - dt.timedelta(1)], 30)
        self.assertEqual(s.first_day, today - dt.timedelta(1))
        self.assertAlmostEqual(s.km(1000), 3.8303, places=3)
        self.assertTrue(s.live.connected)

    def test_missing_database_is_empty(self):
        s = data.load(os.path.join(self.dir.name, "nope.db"), G5)
        self.assertEqual(s.total_revs, 0)


class WheelTest(unittest.TestCase):
    def test_ferris_g5_runs_on_its_48_inch_inner_diameter(self):
        self.assertAlmostEqual(wheels.PRESETS["ferris-g5"].circumference_m, 3.8303, places=3)

    def test_presets_from_the_shared_file(self):
        p = wheels.PRESETS
        self.assertAlmostEqual(p["grand-ferris"].diameter_m, 60 * wheels.INCH)
        self.assertAlmostEqual(p["one-fast-cat"].diameter_m, 43 * wheels.INCH)
        self.assertTrue(p["star-cat-l"].estimated and p["star-cat-xl"].estimated)
        self.assertFalse(p["ferris-g5"].estimated)

    def test_lengths_in_any_unit(self):
        for text in ("48in", '48"', "48 inches", "121.92cm", "1219.2 mm", "1.2192m"):
            self.assertAlmostEqual(wheels.parse_diameter(text), 1.2192, places=4, msg=text)
        for bad in ("48ft", "5in", "", "abc", "3m"):
            with self.assertRaises(ValueError, msg=bad):
                wheels.parse_diameter(bad)

    def test_a_bare_number_is_in_the_chosen_units(self):
        self.assertAlmostEqual(wheels.parse_diameter("123"), 1.23)          # cm by default
        self.assertAlmostEqual(wheels.parse_diameter("48.5", "in"), 48.5 * wheels.INCH)
        self.assertAlmostEqual(wheels.parse_diameter("48.5in", "cm"), 48.5 * wheels.INCH)  # a unit wins
        with self.assertRaisesRegex(ValueError, "did you mean 48.5in"):
            wheels.parse_diameter("48.5", "cm")
        with self.assertRaisesRegex(ValueError, "did you mean 123cm"):
            wheels.parse_diameter("123", "in")
        with self.assertRaises(ValueError):
            wheels.parse_length("48")  # no unit and none chosen

    def test_sizes_shown_in_the_chosen_units(self):
        g5 = wheels.PRESETS["ferris-g5"]
        self.assertIn("running diameter 121.9 cm (48.0 in), outer 129.5 cm", wheels.describe(g5))
        self.assertIn("running diameter 48.0 in (121.9 cm), outer 51.0 in", wheels.describe(g5, "in"))

    def test_custom_diameter_overrides_the_model(self):
        w = wheels.resolve("ferris-g5", "48.5in")
        self.assertAlmostEqual(w.diameter_m, 48.5 * 0.0254)
        self.assertIn("measured", w.name)
        self.assertEqual(wheels.resolve(None, None), wheels.PRESETS[wheels.DEFAULT])
        with self.assertRaises(ValueError):
            wheels.resolve("hamster-3000", None)

    def test_config_file(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "p.conf")
            with open(path, "w") as f:
                f.write("[wheel]\nmodel = ferris-g5\ndiameter = 120  # measured\n[display]\nrotate180 = yes\n")
            cfg = config.load(path)
            self.assertAlmostEqual(cfg.wheel.diameter_m, 1.2)
            self.assertEqual(cfg.units, "cm")
            self.assertTrue(cfg.rotate180)
            self.assertEqual(cfg.errors, [])
            with open(path, "w") as f:
                f.write("[wheel]\nmodel = nope\n")
            cfg = config.load(path)
            self.assertEqual(cfg.wheel, wheels.PRESETS[wheels.DEFAULT])
            self.assertEqual(len(cfg.errors), 1)
            with open(path, "w") as f:
                f.write("[wheel]\nunits = in\ndiameter = 48.5\n")
            cfg = config.load(path)
            self.assertAlmostEqual(cfg.wheel.diameter_m, 48.5 * wheels.INCH)
            self.assertEqual(cfg.errors, [])


class RenderTest(unittest.TestCase):
    def test_every_view_renders_with_and_without_data(self):
        with tempfile.TemporaryDirectory() as d:
            demo = os.path.join(d, "demo.db")
            data.make_demo_db(demo, days=400)
            for stats in (data.load(demo, G5), data.load(os.path.join(d, "none.db"), G5)):
                for view in range(len(render.VIEWS)):
                    img = render.render(stats, view)
                    self.assertEqual(img.size, (250, 122))
                    self.assertEqual(img.mode, "1")

    def test_touch_bar_hit_testing(self):
        self.assertIsNone(render.tab_at(10, 40))
        self.assertEqual(render.tab_at(0, 115), 0)
        self.assertEqual(render.tab_at(249, 115), 3)
        self.assertEqual(render.tab_at(130, 121), 2)


if __name__ == "__main__":
    unittest.main()
