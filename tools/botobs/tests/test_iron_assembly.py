"""Tests for the Assembly of Iron reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic hard mode pull pins the kill order verdict, healer mana at each member's death, an
Overwhelming Power carrier's Meltdown, a Fusion Punch kill and the deaths per phase. The shared v14
fixture shows every section reads empty elsewhere.
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

from bosses import iron_assembly as ia  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

TANK = 7801
PARTNER = 7802
PRIEST = 7803
STEEL = (1 << 32) | 1
MOLGEIM = (1 << 32) | 2
BRUNDIR = (1 << 32) | 3


def snap(t: int, priest_mana: float, steel_hp: float = 100.0) -> dict:
    return {"t": t, "e": "snap", "u": [
        [TANK, 1608.0, 101.0, 427.3, 0, 100, 100, STEEL],
        [PARTNER, 1575.9, 132.3, 427.3, 0, 100, 100, STEEL],
        [PRIEST, 1577.2, 121.0, 427.3, 0, 100, priest_mana, 0],
        [STEEL, 1574.0, 134.0, 427.3, 0, steel_hp, 0, PARTNER],
    ]}


def alive(t: int, mask: int) -> dict:
    return {"t": t, "e": "note", "g": PRIEST, "k": "ironassembly.alive", "txt": str(mask)}


def pull(last_mask: int = 1, out: str = "wipe") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1795290000000, "map": 603, "inst": 4, "diff": 1,
         "boss": "stormcaller-brundir", "bin": 1795289000000,
         "cfg": {"cheats": "food,taxi,raid", "mapthreads": 6, "hardmode": {"iron-assembly": 1}, "iccbuffs": 0},
         "roster": [{"g": TANK, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
                    {"g": PARTNER, "n": "Rampart", "r": "tank", "c": 6, "h": 0},
                    {"g": PRIEST, "n": "Mender", "r": "heal", "c": 5, "h": 0}]},
        {"t": 0, "e": "unit", "g": STEEL, "en": 32867, "n": "Steelbreaker", "b": 1},
        {"t": 0, "e": "unit", "g": MOLGEIM, "en": 32927, "n": "Runemaster Molgeim", "b": 1},
        {"t": 0, "e": "unit", "g": BRUNDIR, "en": 32857, "n": "Stormcaller Brundir", "b": 1},
        {"t": 0, "e": "spell", "sp": 63493, "n": "Fusion Punch"},
        {"t": 0, "e": "spell", "sp": 63526, "n": "High Voltage"},
        {"t": 0, "e": "pull", "boss": "stormcaller-brundir", "src": "engage"},
        alive(10, 7),
        {"t": 9000, "e": "cast", "s": PRIEST, "sp": 34433, "tgt": BRUNDIR},
        snap(5000, 95.0),
        alive(100000, 3),
        snap(100000, 40.0),
        alive(200000, last_mask),
        snap(200000, 5.0, 99.0),
        {"t": 208000, "e": "aura", "d": TANK, "s": STEEL, "sp": 61888, "st": 1, "dur": 35000},
        {"t": 243000, "e": "aura", "d": TANK, "s": STEEL, "sp": 61888, "r": 1, "st": 1, "dur": 0},
        {"t": 243000, "e": "death", "g": TANK, "killer": TANK, "x": 1607.0, "y": 101.0, "z": 427.3,
         "auras": [[61888, 1, 0, STEEL, 208000, -1, 0]], "rewind": []},
        {"t": 243000, "e": "cast", "s": STEEL, "sp": 61902},
        snap(250000, 3.0, 60.0),
        {"t": 260000, "e": "dmg", "s": STEEL, "d": PARTNER, "sp": 63493, "a": 43618, "hp": 90.9, "rs": 10904},
        {"t": 260010, "e": "death", "g": PARTNER, "killer": STEEL, "x": 1575.9, "y": 132.3, "z": 427.3,
         "rewind": [[260000, STEEL, 63493, 43618]]},
        {"t": 270000, "e": "death", "g": PRIEST, "killer": STEEL, "x": 1577.2, "y": 121.0, "z": 427.3,
         "rewind": [[270000, STEEL, 63526, 6111]]},
        {"t": 280000, "e": "end", "out": out},
    ]


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / "603_4_stormcaller-brundir_1795290000.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


def printed(show, *args) -> str:
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
        show(*args)
    return out.getvalue()


class Banner(unittest.TestCase):
    def test_steelbreaker_last_on_a_kill_is_hard_mode(self):
        text = printed(ia.show_banner, load(pull(out="kill")))
        self.assertIn("outcome kill, hard mode triggered: Steelbreaker died last (toggle on)", text)

    def test_a_wipe_only_names_the_order(self):
        self.assertIn("hard mode order: Steelbreaker was the last one up", printed(ia.show_banner, load(pull())))

    def test_brundir_last_is_normal_mode(self):
        text = printed(ia.show_banner, load(pull(last_mask=4, out="kill")))
        self.assertIn("normal mode order: Brundir was the last one up", text)


class Phases(unittest.TestCase):
    def test_healer_mana_at_each_death_and_the_mana_returns(self):
        text = printed(ia.show_phases, load(pull()))
        self.assertIn("1:40.000  Brundir died, Steelbreaker + Molgeim left, healer mana Mender 40%", text)
        self.assertIn("3:20.000  Molgeim died, Steelbreaker left, healer mana Mender 5%", text)
        self.assertIn("Steelbreaker alone for 80s, lowest 60.0%, 60.0% at the end", text)
        self.assertIn("healers under 10% mana from: Mender 3:20.000", text)
        self.assertIn("0:09.000  Shadowfiend from Mender at 95% mana", text)


class Empowered(unittest.TestCase):
    def test_meltdown_and_punch(self):
        text = printed(ia.show_empowered, load(pull()))
        self.assertIn("3:28.000  Overwhelming Power on Bulwark: Meltdown at 4:03.000, 2 yd from the Meltdown point",
                      text)
        self.assertIn("4:20.000  Fusion Punch Rampart 43.6k at 91%, 1 charge(s), 10.9k resisted, no defensive  KILLED",
                      text)

    def test_never_reached(self):
        self.assertIn("never reached", printed(ia.show_empowered, load(pull(last_mask=3))))


class Deaths(unittest.TestCase):
    def test_by_mechanic_and_the_first_one_that_was_not_a_meltdown(self):
        text = printed(ia.show_deaths, load(pull()))
        self.assertIn("Steelbreaker up: 3", text)
        self.assertIn("1  Meltdown", text)
        self.assertIn("1  Fusion Punch", text)
        self.assertIn("1  High Voltage", text)
        self.assertIn("not a Meltdown: Rampart (tank) at 4:20.010, Fusion Punch", text)


class OtherFights(unittest.TestCase):
    def test_every_section_reads_empty(self):
        trace = Trace(FULL_V14)
        self.assertIn("not an Assembly of Iron pull", printed(ia.show_banner, trace))
        for show in (ia.show_phases, ia.show_empowered, ia.show_deaths):
            self.assertEqual(len(printed(show, trace).splitlines()), 1)


if __name__ == "__main__":
    unittest.main()
