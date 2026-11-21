"""Tests for the XT-002 reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull held at the tank spot pins the banner, the Heart windows, a west pile spawn and the
trash pack its Pummeller calls in. The shared v14 fixture shows every section reads empty elsewhere.
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

from bosses import xt_002_deconstructor as xt  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

TANK = 7701
HEALER = 7702
XT = (1 << 32) | 1
HEART = (1 << 32) | 2
PUMMELLER = (1 << 32) | 3
SALVAGEBOT = (1 << 32) | 4


def snap(t: int, pummeller_target: int = 0, trash_target: int = 0, heart_hp: float = 100.0) -> dict:
    return {"t": t, "e": "snap", "u": [
        [TANK, 895.0, -12.0, 409.7, 0, 100, 100, XT],
        [HEALER, 866.0, -6.0, 409.8, 0, 100, 100, 0],
        [XT, 887.0, -12.0, 409.6, 0, 80, 0, TANK],
        [HEART, 887.0, -12.0, 409.6, 0, heart_hp, 0, 0],
        [PUMMELLER, 801.0, -96.0, 409.9, 0, 100, 0, pummeller_target],
        [SALVAGEBOT, 807.0, -102.0, 409.9, 0, 100, 0, trash_target],
    ]}


def pull(heartbreak: bool = False) -> list[dict]:
    records = [
        {"e": "hdr", "v": 14, "ts": 1795280000000, "map": 603, "inst": 2, "diff": 1,
         "boss": "xt-002-deconstructor", "bin": 1795279000000,
         "cfg": {"cheats": "food,taxi,raid", "mapthreads": 6, "hardmode": {"xt-002": 1}, "iccbuffs": 0},
         "roster": [{"g": TANK, "n": "Bulwark", "r": "tank", "c": 1, "h": 0},
                    {"g": HEALER, "n": "Mender", "r": "heal", "c": 5, "h": 0}]},
        {"t": 0, "e": "unit", "g": XT, "en": 33293, "n": "XT-002 Deconstructor", "b": 1},
        {"t": 0, "e": "unit", "g": HEART, "en": 33329, "n": "Heart of the Deconstructor"},
        {"t": 0, "e": "unit", "g": PUMMELLER, "en": 33344, "n": "XM-024 Pummeller"},
        {"t": 0, "e": "unit", "g": SALVAGEBOT, "en": 34269, "n": "XR-949 Salvagebot"},
        {"t": 0, "e": "pull", "boss": "xt-002-deconstructor", "src": "engage"},
        {"t": 76000, "e": "cast", "s": XT, "sp": 37751},
        snap(31000),
        snap(80000, heart_hp=40.0),
        snap(81000, pummeller_target=HEALER, heart_hp=20.0),
        snap(83200, pummeller_target=HEALER, trash_target=HEALER, heart_hp=20.0),
        {"t": 90000, "e": "death", "g": HEALER, "killer": SALVAGEBOT, "x": 866.0, "y": -6.0, "z": 409.8,
         "rewind": [[89900, SALVAGEBOT, 0, 7000]]},
        {"t": 120000, "e": "end", "out": "wipe"},
    ]
    if heartbreak:
        records.insert(-1, {"t": 100000, "e": "cast", "s": XT, "sp": 64193})
    return records


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / "603_2_xt-002-deconstructor_1795280000.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


def printed(show, *args) -> str:
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
        show(*args)
    return out.getvalue()


class Banner(unittest.TestCase):
    def test_outcome_and_spot(self):
        text = printed(xt.show_banner, load(pull()))
        self.assertIn("outcome wipe, no Heartbreak, normal mode", text)
        self.assertIn("XT at (887, -12) from 0:30", text)

    def test_heartbreak_is_named(self):
        self.assertIn("Heartbreak at 1:40.000", printed(xt.show_banner, load(pull(heartbreak=True))))


class Heart(unittest.TestCase):
    def test_window_reports_the_lowest_hp(self):
        self.assertEqual(printed(xt.show_heart, load(pull())), (
            "HEART\n"
            "  window at 1:16.000  lowest 20%  XT at (887, -12), piles NE 80 SE 78 SW 125 NW 122 yd\n"
            "  Heart survived every window\n"))


class Trash(unittest.TestCase):
    def test_pack_pull_is_tied_to_the_pummeller(self):
        self.assertEqual(printed(xt.show_trash, load(pull())), (
            "SCRAPYARD TRASH\n"
            "  1:23.200  1 at the SW pile on Mender, after a SW Pummeller engaged at 1:21.000 8.5 yd from the pack\n"
            "  deaths to trash: 1\n"))

    def test_adds_are_filed_by_pile(self):
        self.assertIn("SW pile from 0:31.000: 1 Pummeller", printed(xt.show_adds, load(pull())))

    def test_deaths_by_mechanic(self):
        self.assertIn("1  Scrapyard trash", printed(xt.show_deaths, load(pull())))

    def test_unnamed_xt_spell_is_not_melee(self):
        records = pull()
        records[-2] = {"t": 90000, "e": "death", "g": HEALER, "killer": XT, "x": 866.0, "y": -6.0,
                       "z": 409.8, "rewind": [[89900, XT, 62776, 9000]]}
        self.assertIn("1  spell 62776", printed(xt.show_deaths, load(records)))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
            xt.show_banner(trace)
            for _, _, section in xt.SECTIONS:
                section(trace)
        self.assertIn("not an XT-002 pull", out.getvalue())


if __name__ == "__main__":
    unittest.main()
