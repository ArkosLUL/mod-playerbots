"""Tests for the Lord Marrowgar reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull pins the duties, the storm anchors and stands, the spike rules, the dodge counts and
the cheat filter, and the shared v14 fixture shows every section reads empty on a trace that is not
Marrowgar.
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

from bosses import lord_marrowgar as lm  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v14.ndjson"

WALL = 6100   # main tank, anchors at 120
ARROW = 6101  # hunter bait, anchors at 0
FEL = 6102    # warlock bait, on the stack, dies before the second storm
SHIV = 6104   # melee inside


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def pull(boss: str = "lord-marrowgar") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789900000000, "map": 631, "inst": 3, "diff": 1, "boss": boss,
         "cfg": {"cheats": "food,taxi,raid", "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "x", "h": 0}
                    for guid, name, role in ((WALL, "Wall", "tank"), (ARROW, "Arrow", "ranged"),
                                             (FEL, "Fel", "ranged"), (SHIV, "Shiv", "melee"))]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        note(100, SHIV, lm.PHASE, "normal"),
        note(100, WALL, lm.DUTY, "1"),
        note(100, ARROW, lm.DUTY, "4"),
        note(100, FEL, lm.DUTY, "4"),
        note(100, SHIV, lm.DUTY, "3"),
        note(100, WALL, lm.ANCHOR, "2"),
        note(100, ARROW, lm.ANCHOR, "1"),
        note(200, ARROW, lm.PHASE, "normal"),
        note(300, SHIV, lm.STAND, "inner"),
        note(5000, ARROW, lm.DODGE, "move 7"),
        note(5600, ARROW, lm.DODGE, "hold"),
        note(6400, ARROW, lm.DODGE, "clear"),
        note(12000, ARROW, lm.DODGE, "tight"),
        note(12900, ARROW, lm.DODGE, "clear"),
        note(42000, ARROW, lm.PHASE, "stormcast"),
        note(42100, WALL, lm.STAND, "anchor"),
        note(42100, FEL, lm.STAND, "stack"),
        note(45000, ARROW, lm.PHASE, "bonestorm"),
        note(45050, ARROW, lm.STAND, "anchor"),
        note(65000, ARROW, lm.PHASE, "normal"),
        note(65100, WALL, lm.STAND, "tank"),
        note(70000, SHIV, lm.SPIKE, "melee"),
        note(70100, ARROW, lm.SPIKE, "far"),
        note(76000, SHIV, lm.SPIKE, "boss"),
        note(90000, FEL, lm.DUTY, "0"),
        note(132000, SHIV, lm.PHASE, "stormcast"),
        note(135000, SHIV, lm.PHASE, "bonestorm"),
        note(155000, SHIV, lm.PHASE, "normal"),
        {"t": 50000, "e": "cheat", "g": SHIV, "k": "lm.example", "n": 2},
        {"t": 60000, "e": "cheat", "g": SHIV, "k": "rf.nitro", "n": 1},
        {"t": 160000, "e": "end", "out": "kill"},
    ]


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / f"631_3_{records[0]['boss']}_1789900000.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


class SyntheticPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.trace = load(pull())

    def test_phases_merge_the_bots(self):
        self.assertEqual(lm.phase_steps(self.trace), [
            ("normal", 100, 42000), ("stormcast", 42000, 45000), ("bonestorm", 45000, 65000),
            ("normal", 65000, 132000), ("stormcast", 132000, 135000), ("bonestorm", 135000, 155000),
            ("normal", 155000, 160000)])

    def test_storms_are_the_spins_only(self):
        self.assertEqual(lm.storms(self.trace), [(45000, 65000), (135000, 155000)])

    def test_anchors_carry_over_without_a_new_row(self):
        first, second = lm.storms(self.trace)
        self.assertEqual(lm.anchors(self.trace, *first), {"0": ARROW, "120": WALL})
        self.assertEqual(lm.anchors(self.trace, *second), {"0": ARROW, "120": WALL})

    def test_storm_stands_overlap_the_storm(self):
        first, second = lm.storms(self.trace)
        self.assertEqual(lm.storm_stands(self.trace, *first), {"anchor": {WALL, ARROW}, "stack": {FEL}})
        self.assertEqual(lm.storm_stands(self.trace, *second), {"anchor": {ARROW}, "stack": {FEL}})

    def test_duties_name_the_numbers_and_skip_a_lost_one(self):
        self.assertEqual(lm.duties(self.trace), {"main tank": {WALL}, "bait": {ARROW, FEL}, "inside": {SHIV}})

    def test_spike_rules(self):
        self.assertEqual(lm.held_values(self.trace, lm.SPIKE), {"melee": {SHIV}, "far": {ARROW}, "boss": {SHIV}})

    def test_dodges_count_branches_not_clear(self):
        self.assertEqual(lm.dodges(self.trace), {ARROW: {"move": 1, "hold": 1, "tight": 1}})

    def test_only_this_encounters_cheats(self):
        self.assertEqual([key for key, _ in lm.own_cheats(self.trace)], ["lm.example"])

    def test_storm_section(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            lm.show_storms(self.trace)
        self.assertEqual(out.getvalue(), (
            "BONE STORMS\n"
            "  1  0:45.000 for 0:20.000  anchors 0: Arrow, 120: Wall\n"
            "       on the anchor: Arrow, Wall\n"
            "       on the stack: Fel\n"
            "  2  2:15.000 for 0:20.000  anchors 0: Arrow, 120: Wall\n"
            "       on the anchor: Arrow\n"
            "       on the stack: Fel\n"))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        for trace in (Trace(FULL), load(pull(boss="lady-deathwhisper"))):
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                lm.show_banner(trace)
                for _, _, section in lm.SECTIONS:
                    section(trace)
            self.assertIn("not a Lord Marrowgar pull", out.getvalue())
            self.assertNotIn("anchors", out.getvalue())


if __name__ == "__main__":
    unittest.main()
