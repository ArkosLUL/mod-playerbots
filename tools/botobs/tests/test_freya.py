"""Tests for the Freya reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull pins a trio wave where the add tank's Challenging Roar puts Freya on it, the main
tank's Righteous Defense takes the Snaplasher, and the add tank dies; the shared v14 fixture shows every
section reads empty elsewhere.
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

from bosses import freya as fr  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

MAIN = 7701
ADD = 7702
CASTER = 7703
FREYA = (1 << 32) | 1
SNAPLASHER = (1 << 32) | 2
STORM = (1 << 32) | 3
SPIRIT = (1 << 32) | 4
GIFT = (1 << 32) | 5
BEAM = (1 << 32) | 6


def snap(t: int, freya_on: int, snap_on: int, gift: bool = True, trio: bool = True) -> dict:
    units = [
        [MAIN, 2360.0, -43.0, 425.3, 0, 100, 100, FREYA],
        [ADD, 2355.0, -40.0, 425.3, 0, 100, 100, SNAPLASHER],
        [CASTER, 2340.0, -60.0, 425.3, 0, 100, 100, GIFT],
        [FREYA, 2362.0, -43.0, 425.3, 0, 100, 0, freya_on],
    ]
    if trio:
        units += [[SNAPLASHER, 2356.0, -40.0, 425.3, 0, 80, 0, snap_on],
                  [STORM, 2358.0, -41.0, 425.3, 0, 80, 0, MAIN],
                  [SPIRIT, 2357.0, -38.0, 425.3, 0, 80, 0, ADD]]
    if gift:
        units.append([GIFT, 2345.0, -60.0, 425.3, 0, 60, 0, 0])
    return {"t": t, "e": "snap", "u": units}


def cast(t: int, caster: int, spell: int, target: int = 0) -> dict:
    return {"t": t, "e": "cast", "s": caster, "sp": spell, "tgt": target, "ct": 0}


def pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 14, "ts": 1795343672000, "map": 603, "inst": 38, "diff": 1, "boss": "elder-stonebark",
         "bin": 1795338959000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 6, "hardmode": {}, "iccbuffs": 0},
         "roster": [{"g": MAIN, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
                    {"g": ADD, "n": "Bruin", "r": "tank", "c": 11, "h": 0},
                    {"g": CASTER, "n": "Caster", "r": "ranged", "c": 8, "h": 0}]},
        {"t": 0, "e": "unit", "g": FREYA, "en": 32906, "n": "Freya", "b": 1},
        {"t": 0, "e": "unit", "g": SNAPLASHER, "en": 32916, "n": "Snaplasher"},
        {"t": 0, "e": "unit", "g": STORM, "en": 32919, "n": "Storm Lasher"},
        {"t": 0, "e": "unit", "g": SPIRIT, "en": 33202, "n": "Ancient Water Spirit"},
        {"t": 0, "e": "unit", "g": GIFT, "en": 33228, "n": "Eonar's Gift"},
        {"t": 0, "e": "unit", "g": BEAM, "en": 33170, "n": "Sun Beam"},
        {"t": 0, "e": "pull", "boss": "elder-stonebark", "src": "engage"},
        cast(1000, FREYA, 62859),
    ]
    holders = [(10000, MAIN, ADD), (12000, MAIN, ADD), (14000, ADD, ADD), (16000, ADD, ADD), (16500, ADD, MAIN),
               (18000, MAIN, MAIN), (20000, MAIN, ADD), (24000, MAIN, ADD)]
    for t, freya_on, snap_on in holders:
        records.append(snap(t, freya_on, snap_on))
    records += [
        cast(13500, ADD, 5209),
        cast(16200, MAIN, 31789, ADD),
        cast(16200, MAIN, 31789, ADD) | {"tr": 1},
        {"t": 22000, "e": "death", "g": ADD, "killer": SNAPLASHER, "x": 2355.0, "y": -40.0, "z": 425.3,
         "rewind": [[21900, SNAPLASHER, 0, 43481]]},
        {"t": 25000, "e": "death", "g": CASTER, "killer": CASTER, "x": 2340.0, "y": -60.0, "z": 425.3,
         "blow": [CASTER, 4600], "rewind": [[24900, CASTER, 62438, 4600]]},
        snap(30000, MAIN, 0, gift=False, trio=False),
        {"t": 31000, "e": "end", "out": "wipe"},
    ]
    return records


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / "603_38_elder-stonebark_1795343672.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


def printed(show, *args) -> str:
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
        show(*args)
    return out.getvalue()


class Banner(unittest.TestCase):
    def test_elders_from_their_empowers(self):
        self.assertIn("hard mode: 2 of 3 Elders (Stonebark, Brightleaf)", printed(fr.show_banner, load(pull())))


class Waves(unittest.TestCase):
    def test_trio_wave_and_a_gift_that_lived_to_heal(self):
        self.assertEqual(printed(fr.show_waves, load(pull())), (
            "WAVES\n"
            "  0:10.000 .. 0:24.000  trio        3 add(s), 1 raid death(s) meanwhile\n"
            "  0:10.000 Eonar's Gift 14.0s, lived to heal, 60% left\n"))


class Tanks(unittest.TestCase):
    def test_holders_and_area_taunts(self):
        self.assertEqual(printed(fr.show_tanks, load(pull())), (
            "TANKS\n"
            "  Freya on Bulwark/tank 84.8% of the pull\n"
            "    0:14.000 .. 0:16.500 on Bruin/tank\n"
            "  Snaplasher 0:10.000 .. 0:24.000: Bruin/tank 87%, Bulwark/tank 13%\n"
            "  0:13.500 Bruin Challenging Roar\n"
            "  0:16.200 Bulwark Righteous Defense on Bruin/tank\n"))


class Deaths(unittest.TestCase):
    def test_by_killer_and_own_spell(self):
        text = printed(fr.show_deaths, load(pull()))
        self.assertIn("1  Snaplasher melee", text)
        self.assertIn("1  own spell 62438", text)


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
            fr.show_banner(trace)
            for _, _, section in fr.SECTIONS:
                section(trace)
        self.assertIn("not a Freya pull", out.getvalue())


if __name__ == "__main__":
    unittest.main()
