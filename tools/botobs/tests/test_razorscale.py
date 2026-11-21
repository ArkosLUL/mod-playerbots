"""Tests for the Razorscale reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull pins one knockdown where the stack mover walks a ranged onto a live patch that then
kills it, and a final landing; the shared v14 fixture shows every section reads empty elsewhere.
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

from bosses import razorscale as rs  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

TANK = 7701
CASTER = 7702
HEALER = 7703
RAZORSCALE = (1 << 32) | 1
FLAME = (1 << 32) | 2
GUARDIAN = (1 << 32) | 3


def snap(t: int, boss_z: float, hp: float, flame: bool = False) -> dict:
    units = [
        [TANK, 570.0, -190.0, 391.5, 0, 100, 0, RAZORSCALE],
        [CASTER, 599.5, -175.0, 391.5, 0, 100, 100, RAZORSCALE],
        [HEALER, 580.0, -170.0, 391.5, 0, 100, 100, 0],
        [RAZORSCALE, 585.4, -173.5, boss_z, 0, hp, 0, TANK],
    ]
    if flame:
        units.append([FLAME, 600.0, -176.0, 391.5, 0, 100, 0, 0])
    return {"t": t, "e": "snap", "u": units}


def move(t: int, guid: int, x: float, y: float, ok: bool = True) -> dict:
    return {"t": t, "e": "move", "g": guid, "k": "point", "x": x, "y": y, "z": 391.5, "tgt": 0,
            "ok": 1 if ok else 0, "r": "" if ok else "wait", "by": rs.STACK_MOVER, "pr": "combat"}


def pull() -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1795295777000, "map": 603, "inst": 19, "diff": 1, "boss": "razorscale",
         "bin": 1795294271000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 6, "hardmode": {}, "iccbuffs": 0},
         "roster": [{"g": TANK, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
                    {"g": CASTER, "n": "Caster", "r": "ranged", "c": 9, "h": 0},
                    {"g": HEALER, "n": "Mender", "r": "heal", "c": 5, "h": 0}]},
        {"t": 0, "e": "unit", "g": RAZORSCALE, "en": 33186, "n": "Razorscale", "b": 1},
        {"t": 0, "e": "unit", "g": FLAME, "en": 34188, "n": "Razorscale Devouring Flame Stalker"},
        {"t": 0, "e": "unit", "g": GUARDIAN, "en": 33388, "n": "Dark Rune Guardian"},
        {"t": 0, "e": "pull", "boss": "razorscale", "src": "engage"},
        snap(5000, 475.2, 100.0),
        {"t": 6000, "e": "dmg", "s": FLAME, "d": HEALER, "sp": 64733, "a": 6000, "ok": 0, "hp": 100.0},
        snap(20000, 391.6, 90.0, flame=True),
        move(21000, CASTER, 599.54, -175.04),
        move(21500, HEALER, 580.0, -170.0),
        {"t": 22000, "e": "dmg", "s": FLAME, "d": CASTER, "sp": 64733, "a": 7000, "ok": 0, "hp": 40.0},
        {"t": 25000, "e": "death", "g": CASTER, "killer": FLAME, "x": 599.5, "y": -175.0, "z": 391.5,
         "rewind": [[24900, FLAME, 64733, 6626]]},
        snap(30000, 391.6, 80.0, flame=True),
        snap(40000, 475.2, 75.0),
        {"t": 41000, "e": "death", "g": HEALER, "killer": GUARDIAN, "x": 610.0, "y": -217.0, "z": 391.5,
         "rewind": [[40900, GUARDIAN, 0, 4982]]},
        snap(50000, 391.6, 49.0),
        move(51000, TANK, 570.0, -190.0),
        {"t": 60000, "e": "end", "out": "kill"},
    ]


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / "603_19_razorscale_1795295777.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


def printed(show, *args) -> str:
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
        show(*args)
    return out.getvalue()


class Phases(unittest.TestCase):
    def test_knockdown_then_final_landing(self):
        self.assertEqual(printed(rs.show_phases, load(pull())), (
            "PHASES\n"
            "  0:05.000 .. 0:05.000  air    from 100.0%\n"
            "  0:20.000 .. 0:30.000  ground from 90.0%, knockdown\n"
            "  0:40.000 .. 0:40.000  air    from 75.0%\n"
            "  0:50.000 .. 0:50.000  ground from 49.0%, final\n"))


class Stack(unittest.TestCase):
    def test_walk_onto_a_live_patch_is_counted(self):
        self.assertEqual(printed(rs.show_stack, load(pull())), (
            "RANGED STACK\n"
            "  0:20.000 knockdown  2 walks by 2 bots to 2 spot(s), busiest (600, -175) x1; onto a live patch 1\n"
            "  whole pull: 1 of 2 walks ended within 10 yd of a live patch\n"))

    def test_refused_rows_are_not_walks(self):
        records = pull()
        at = next(i for i, rec in enumerate(records) if rec.get("e") == "move")
        records.insert(at + 1, move(21100, CASTER, 599.54, -175.04, ok=False))
        self.assertIn("whole pull: 1 of 2 walks ended", printed(rs.show_stack, load(records)))

    def test_tank_walks_are_not_the_stack(self):
        records = [rec for rec in pull() if not (rec.get("e") == "move" and rec.get("g") != TANK)]
        self.assertIn("no stack walks", printed(rs.show_stack, load(records)))


class Fire(unittest.TestCase):
    def test_damage_split_by_phase_and_role(self):
        self.assertEqual(printed(rs.show_fire, load(pull())), (
            "DEVOURING FLAME\n"
            "  air    heal 6k\n"
            "  ground ranged 7k\n"
            "  2 hits on the raid\n"))


class Deaths(unittest.TestCase):
    def test_by_mechanic_and_phase(self):
        text = printed(rs.show_deaths, load(pull()))
        self.assertIn("1  Devouring Flame (ground)", text)
        self.assertIn("1  Dark Rune Guardian (air)", text)

    def test_banner(self):
        self.assertIn("outcome kill at 1:00.000, 2 death(s)", printed(rs.show_banner, load(pull())))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
            rs.show_banner(trace)
            for _, _, section in rs.SECTIONS:
                section(trace)
        self.assertIn("not a Razorscale pull", out.getvalue())


if __name__ == "__main__":
    unittest.main()
