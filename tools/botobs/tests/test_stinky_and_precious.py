"""Tests for the Stinky and Precious reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic Stinky pull, filed under the contract slug and under the creature as older traces were,
pins each section, and the shared v14 fixture shows every section reads empty off ICC.
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

from bosses import stinky_and_precious as dogs  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

STINKY = (1 << 32) | 900
BULWARK = 7501  # tank
AEGIS = 7502    # tank
MEND = 7503     # healer

ROSTER = ((BULWARK, "Bulwark", "tank"), (AEGIS, "Aegis", "tank"), (MEND, "Mend", "heal"))


def snap(when: int, target: int) -> dict:
    return {"t": when, "e": "snap", "hz": [], "u": [[STINKY, 0.0, 0.0, 0.0, 0.0, 50.0, 0.0, target, 0, 0, 0, 0]]}


def wound(when: int, guid: int, stacks: int) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": STINKY, "sp": dogs.SPELL_MORTAL_WOUND, "r": 0, "st": stacks}


def pull(boss: str = "stinky-and-precious") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": 1, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": 0} for guid, name, role in ROSTER]},
        {"t": 0, "e": "pull", "boss": boss, "src": "mark"},
        {"t": 1, "e": "unit", "g": STINKY, "en": 37025, "n": "Stinky", "lvl": 82},
        snap(0, 0), snap(500, BULWARK), snap(20000, AEGIS), snap(40000, BULWARK),
        wound(5000, BULWARK, 4), wound(19000, BULWARK, 8), wound(39000, AEGIS, 9),
        {"t": 21000, "e": "cheat", "g": AEGIS, "k": "icc.tauntreset", "n": 1},
        {"t": 45000, "e": "end", "out": "idle"},
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


class Tanks(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(dogs.show_tanks, load(pull())), (
            "TANKS\n"
            "  Stinky: who it hit, each run; 2 swap(s) between tanks\n"
            "\n"
            "     0:00.500   0:19.500  Bulwark (tank)\n"
            "     0:20.000   0:20.000  Aegis (tank)\n"
            "     0:40.000   0:00.000  Bulwark (tank)\n"
            "\n"
            "  Mortal Wound peak, the swap is at 8: Aegis 9, Bulwark 8\n"))

    def test_a_trace_filed_under_the_creature_is_the_same_encounter(self):
        self.assertIn("Stinky: who it hit", printed(dogs.show_tanks, load(pull(boss="stinky"))))


class Cheats(unittest.TestCase):
    def test_raid_wide_keys_count_as_this_encounters(self):
        self.assertEqual(printed(dogs.show_cheats, load(pull())), (
            "CHEATS\n"
            "  icc.tauntreset  1 use(s) from 0:21.000\n"
            "    by  Aegis 1\n"
            "    on  self\n"))


class Banner(unittest.TestCase):
    def test_another_icc_boss_is_refused(self):
        self.assertIn("  not a Stinky and Precious pull, every section below reads empty",
                      printed(dogs.show_banner, load(pull(boss="festergut"))))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            dogs.show_banner(trace)
            for _, _, section in dogs.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Stinky and Precious pull"), len(dogs.SECTIONS))


if __name__ == "__main__":
    unittest.main()
