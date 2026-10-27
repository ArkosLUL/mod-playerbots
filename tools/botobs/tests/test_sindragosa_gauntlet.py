"""Tests for the Sindragosa gauntlet reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 10N pull pins each section, and the shared v14 fixture shows every section reads empty on
a trace that is not the gauntlet rather than raising.
"""
from __future__ import annotations

import contextlib
import io
import json
import pathlib
import sys
import tempfile
import unittest

BOTOBS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BOTOBS))

from bosses import sindragosa_gauntlet as gauntlet  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
CHAMPION = CREATURE | 980
WEBWEAVER = CREATURE | 981
WARDEN = CREATURE | 982
SORCERESS = CREATURE | 983
CHAMPION_AGAIN = CREATURE | 984
BROODLING = CREATURE | 985
BULWARK = 7401

UNITS = ((CHAMPION, 37501, "Nerub'ar Champion"), (WEBWEAVER, 37502, "Nerub'ar Webweaver"),
         (WARDEN, 37228, "Frostwarden Warrior"), (SORCERESS, 37229, "Frostwarden Sorceress"),
         (CHAMPION_AGAIN, 37501, "Nerub'ar Champion"), (BROODLING, 37232, "Nerub'ar Broodling"))

# Each add's sampled span: the first wave's spiders, the Frostwardens, the third wave's spiders.
SAMPLES = {CHAMPION: (0, 20000), WEBWEAVER: (1000, 22000), WARDEN: (30000, 50000), SORCERESS: (31000, 48000),
           CHAMPION_AGAIN: (60000, 80000), BROODLING: (10000, 65000)}


def snaps() -> list[dict]:
    stamps = sorted({stamp for span in SAMPLES.values() for stamp in span})
    return [{"t": when, "e": "snap",
             "u": [[guid, 4170.0, 2480.0, 211.0, 0.0, 100, 0, 0, 0, 0, 0, 0, 0, 0]
                   for guid, (first, last) in SAMPLES.items() if first <= when <= last]}
            for when in stamps]


def pull(boss: str = "sindragosa-gauntlet") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": 0, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 0},
         "roster": [{"g": BULWARK, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0}]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        *({"t": 0, "e": "unit", "g": guid, "en": entry, "n": name} for guid, entry, name in UNITS),
        *snaps(),
        {"t": 90000, "e": "end", "out": "kill"},
    ]


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / f"631_5_{records[0]['boss']}_1789800000.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


def printed(show, *args) -> str:
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
        show(*args)
    return out.getvalue()


class Waves(unittest.TestCase):
    def test_spiders_split_at_the_first_frostwarden(self):
        self.assertEqual(gauntlet.wave_rows(load(pull())), [
            ("spiders, first wave", 2, 0, 22000),
            ("frostwardens", 2, 30000, 50000),
            ("spiders, third wave", 1, 60000, 80000),
            ("broodlings", 1, 10000, 65000),
        ])

    def test_section(self):
        self.assertEqual(printed(gauntlet.show_waves, load(pull())), (
            "WAVES\n"
            "    kind                  units      first       last\n"
            "    spiders, first wave       2   0:00.000   0:22.000\n"
            "    frostwardens              2   0:30.000   0:50.000\n"
            "    spiders, third wave       1   1:00.000   1:20.000\n"
            "    broodlings                1   0:10.000   1:05.000\n"))

    def test_no_frostwarden_keeps_every_spider_in_the_first_wave(self):
        records = [rec for rec in pull() if rec.get("en") not in (37228, 37229)]
        rows = gauntlet.wave_rows(load(records))
        self.assertEqual(rows[0], ("spiders, first wave", 3, 0, 80000))


class Cheats(unittest.TestCase):
    def test_none(self):
        self.assertEqual(printed(gauntlet.show_cheats, load(pull())), "CHEATS\n  no cheat fired\n")

    def test_uses_come_from_other_code(self):
        records = pull() + [{"t": 5000, "e": "cheat", "g": BULWARK, "k": "sg.nitro", "n": 1}]
        self.assertEqual(printed(gauntlet.show_cheats, load(records)),
                         "CHEATS\n  1 use(s), none from gauntlet code: icecrown_citadel.py --cheats\n")


class Banner(unittest.TestCase):
    def test_owns_no_probe(self):
        self.assertIn("no probes of its own", printed(gauntlet.show_banner, load(pull())))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            gauntlet.show_banner(trace)
            for _, _, section in gauntlet.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Sindragosa gauntlet pull"), len(gauntlet.SECTIONS))


if __name__ == "__main__":
    unittest.main()
