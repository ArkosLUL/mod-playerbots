"""Tests for the Kologarn reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull pins an eyebeam run that leaves the walkway, the trash pack it brings up from the
antechamber, and the deaths that follow. The shared v14 fixture shows every section reads empty
elsewhere.
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

from bosses import kologarn as kolo  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

TANK = 7701
RUNNER = 7702
HEALER = 7703
KOLOGARN = (1 << 32) | 1
EYE = (1 << 32) | 2
EYE_RIGHT = (1 << 32) | 3
GOLEM = (1 << 32) | 4
STALKER = (1 << 32) | 5


def snap(t: int, runner: tuple, eyes: bool = True, golem_target: int = 0) -> dict:
    units = [
        [TANK, 1790.0, -24.0, 448.8, 0, 100, 100, KOLOGARN],
        [RUNNER, runner[0], runner[1], runner[2], 0, 100, 100, KOLOGARN],
        [HEALER, 1758.0, -24.0, 448.8, 0, 100, 100, 0],
        [KOLOGARN, 1797.0, -24.0, 448.7, 0, 90, 0, TANK],
        [GOLEM, 1684.0, -11.0, 428.0, 0, 100, 0, golem_target],
    ]
    if eyes:
        units.append([EYE, runner[0] + 4.0, runner[1], 448.8, 0, 100, 0, RUNNER])
        units.append([EYE_RIGHT, runner[0] + 4.0, runner[1] + 1.0, 448.8, 0, 100, 0, RUNNER])
    return {"t": t, "e": "snap", "u": units}


def pull(run_west_to: float = 1700.0) -> list[dict]:
    off = run_west_to < kolo.DOOR_X
    return [
        {"e": "hdr", "v": 14, "ts": 1795287512000, "map": 603, "inst": 5, "diff": 1, "boss": "kologarn",
         "bin": 1795287345000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 6, "hardmode": {}, "iccbuffs": 0},
         "roster": [{"g": TANK, "n": "Bulwark", "r": "tank", "c": 1, "h": 0},
                    {"g": RUNNER, "n": "Sprinter", "r": "ranged", "c": 5, "h": 0},
                    {"g": HEALER, "n": "Mender", "r": "heal", "c": 5, "h": 0}]},
        {"t": 0, "e": "unit", "g": KOLOGARN, "en": 32930, "n": "Kologarn", "b": 1},
        {"t": 0, "e": "unit", "g": EYE, "en": 33632, "n": "Focused Eyebeam"},
        {"t": 0, "e": "unit", "g": EYE_RIGHT, "en": 33802, "n": "Focused Eyebeam Right"},
        {"t": 0, "e": "unit", "g": GOLEM, "en": 34190, "n": "Hardened Iron Golem"},
        {"t": 0, "e": "unit", "g": STALKER, "en": 33661, "n": "Armsweep Stalker Kologarn"},
        {"t": 0, "e": "pull", "boss": "kologarn", "src": "engage"},
        snap(5000, (1758.0, -24.0, 448.8), eyes=False),
        snap(11000, (1758.0, -24.0, 448.8)),
        {"t": 14000, "e": "dmg", "s": EYE, "d": RUNNER, "sp": 63976, "a": 3000, "ok": 0, "hp": 100.0},
        {"t": 14000, "e": "dmg", "s": EYE, "d": HEALER, "sp": 63976, "a": 3000, "ok": 0, "hp": 100.0},
        snap(16000, (1748.0, -15.0, 448.8)),
        snap(19000, (max(run_west_to, 1745.0) if not off else 1738.0, -15.0, 448.8)),
        snap(24000, (run_west_to, -7.0, 434.0 if off else 448.8), eyes=False,
             golem_target=RUNNER if off else 0),
        {"t": 30000, "e": "death", "g": RUNNER, "killer": GOLEM, "x": 1690.0, "y": -7.0, "z": 430.0,
         "rewind": [[29900, GOLEM, 64967, 11000]]},
        {"t": 40000, "e": "death", "g": HEALER, "killer": STALKER, "x": 1758.0, "y": -24.0, "z": 448.8,
         "rewind": [[39900, STALKER, 63982, 14000]]},
        {"t": 60000, "e": "end", "out": "wipe"},
    ]


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / "603_5_kologarn_1795287512.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


def printed(show, *args) -> str:
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
        show(*args)
    return out.getvalue()


class Eyebeam(unittest.TestCase):
    def test_run_down_the_stairs_is_flagged(self):
        self.assertEqual(printed(kolo.show_eyebeam, load(pull())), (
            "FOCUSED EYEBEAM\n"
            "  0:11.000  chased Sprinter (ranged) from (1758, -24, 449), LEFT the walkway at 0:19.000"
            " (1738, -15, 449); beam hits on it 1, on others 1\n"
            "  runs that left the walkway: 1 of 1 begun on it\n"))

    def test_run_that_turns_at_the_corridor_stays_on(self):
        text = printed(kolo.show_eyebeam, load(pull(run_west_to=1747.0)))
        self.assertIn("stayed on the walkway, westmost x 1747", text)
        self.assertIn("runs that left the walkway: 0 of 1 begun on it", text)


class Trash(unittest.TestCase):
    def test_pack_is_tied_to_the_bot_that_went_down(self):
        self.assertEqual(printed(kolo.show_trash, load(pull())), (
            "ANTECHAMBER TRASH\n"
            "  0:24.000  1 on Sprinter, standing at (1700, -7, 434) off the walkway\n"
            "  deaths to trash: 1\n"))

    def test_no_pack_without_a_runner(self):
        self.assertIn("never engaged", printed(kolo.show_trash, load(pull(run_west_to=1747.0))))


class Deaths(unittest.TestCase):
    def test_by_mechanic_with_the_trash_split(self):
        text = printed(kolo.show_deaths, load(pull()))
        self.assertIn("1  antechamber trash   (1 after the trash joined)", text)
        self.assertIn("1  Shockwave   (1 after the trash joined)", text)

    def test_banner(self):
        self.assertIn("outcome wipe at 1:00.000, 2 death(s)", printed(kolo.show_banner, load(pull())))

    def test_a_stone_grip_tick_from_the_victim_is_not_a_fall(self):
        trace = load(pull())
        trace.spells[64292] = "Stone Grip"
        grip = {"t": 50000, "g": RUNNER, "killer": RUNNER, "blow": [RUNNER, 2458],
                "rewind": [[48900, RUNNER, 64292, 2458], [49900, RUNNER, 64292, 2458]]}
        self.assertEqual(kolo.mechanic(trace, grip), "Stone Grip")
        fall = {"t": 50000, "g": RUNNER, "killer": RUNNER, "blow": [RUNNER, 30000],
                "rewind": [[49900, RUNNER, 64292, 2458]]}
        self.assertEqual(kolo.mechanic(trace, fall), "fall")


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
            kolo.show_banner(trace)
            for _, _, section in kolo.SECTIONS:
                section(trace)
        self.assertIn("not a Kologarn pull", out.getvalue())


if __name__ == "__main__":
    unittest.main()
