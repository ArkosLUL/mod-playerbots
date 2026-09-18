"""Tests for the Mimiron reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull pins the arithmetic behind each section, and the shared fixture shows every section
reads empty on a trace that is not Mimiron rather than raising.
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

from bosses import mimiron as mm  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v13.ndjson"

VX = 4294970000
BOMB = 4294970100
AGONY = 5001    # ranged
TREE = 5002     # heal
SHADOW = 5003   # melee
BULWARK = 5004  # tank

HOME = {AGONY: (22.0, 0.0), TREE: (0.0, 22.0), SHADOW: (3.0, 0.0), BULWARK: (-3.0, 0.0)}


def spot(guid: int, when: int) -> tuple[float, float]:
    # Agony is knocked 8 yd off her slot by the Rapid Burst step and walked back by the formation.
    if guid == AGONY and 10000 <= when <= 12000:
        return 22.0, 8.0
    return HOME[guid]


def is_moving(guid: int, when: int) -> int:
    if guid != AGONY:
        return 0
    return int(10000 <= when < 12000 or 12250 <= when < 13250)


def snap(when: int) -> dict:
    rows = [[VX, 0.0, 0.0, 364.0, 0.0, 90.0, 0.0, BULWARK, 0, 0, 0, 0, 0, 0.0]]
    for guid in HOME:
        x, y = spot(guid, when)
        alive = 0.0 if guid == SHADOW and when >= 30000 else 100.0
        rows.append([guid, x, y, 364.0, 0.0, alive, 80.0, VX, is_moving(guid, when), 0, 0, 0, 0, 0.0])
    if 28000 <= when < 38000:
        rows.append([BOMB, 10.0, 0.0, 364.0, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0, 0, 0.0])
    return {"t": when, "e": "snap", "u": rows, "hz": []}


def move(when: int, guid: int, by: str, x: float, y: float) -> dict:
    return {"t": when, "e": "move", "g": guid, "k": "point", "x": x, "y": y, "z": 364.0, "tgt": 0,
            "ok": 1, "r": "", "by": by, "pr": "forced"}


def mimiron_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 13, "ts": 1789700000000, "map": 603, "inst": 2, "diff": 1, "boss": "mimiron",
         "roster": [
             {"g": AGONY, "n": "Agony", "r": "ranged", "c": "warlock", "h": 0},
             {"g": TREE, "n": "Tree", "r": "heal", "c": "druid", "h": 0},
             {"g": SHADOW, "n": "Shadow", "r": "melee", "c": "rogue", "h": 0},
             {"g": BULWARK, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "mimiron", "src": "engage"},
        {"t": 1, "e": "unit", "g": VX, "en": mm.NPC_VX001, "n": "VX-001", "b": 1},
        {"t": 1, "e": "unit", "g": BOMB, "en": mm.NPC_FROST_BOMB, "n": "Frost Bomb"},
        {"t": 0, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "0"},
        {"t": 1000, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "1"},
        {"t": 5000, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "5"},
        {"t": 8000, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "2"},

        # the slot notes: one new shape each, then Agony's slot slides a yard, then a count change
        {"t": 8000, "e": "note", "g": AGONY, "k": "mimiron.slot", "txt": "hmwedge 0/2 22,0,364"},
        {"t": 8000, "e": "note", "g": TREE, "k": "mimiron.slot", "txt": "hmwedge 1/2 0,22,364"},
        {"t": 9000, "e": "note", "g": AGONY, "k": "mimiron.slot", "txt": "hmwedge 0/2 23,0,364"},
        {"t": 9500, "e": "note", "g": AGONY, "k": "mimiron.slot", "txt": "hmwedge 1/3 25,5,364"},

        {"t": 9000, "e": "dmg", "s": VX, "d": TREE, "sp": mm.SPELL_HEAT_WAVE, "a": 2000},
        {"t": 9100, "e": "heal", "s": TREE, "d": AGONY, "sp": 48441, "a": 3000, "oh": 1000},

        # step, walk back, step again: one A-B-A, and the walk back charged to the step
        move(10000, AGONY, mm.BURST_STEP, 22.0, 8.0),
        move(12100, AGONY, mm.FORMATION, 22.0, 0.0),
        move(14000, AGONY, mm.BURST_STEP, 22.0, -8.0),

        # a burst on Agony: Shadow sits on the centreline, Tree and Bulwark well off it
        {"t": 20000, "e": "cast", "s": VX, "sp": mm.SPELL_RAPID_BURST, "tgt": AGONY, "ct": 0},
        {"t": 20500, "e": "dmg", "s": VX, "d": AGONY, "sp": 64531, "a": 2200},
        {"t": 21000, "e": "dmg", "s": VX, "d": AGONY, "sp": 64532, "a": 2200},
        {"t": 21500, "e": "dmg", "s": VX, "d": AGONY, "sp": 64531, "a": 2200},
        {"t": 20500, "e": "dmg", "s": VX, "d": SHADOW, "sp": 64532, "a": 2200},

        # the bomb is summoned 3 s after the cast; one escape short of the clearance, one clear
        {"t": 25000, "e": "cast", "s": VX, "sp": mm.SPELL_FROST_BOMB, "tgt": 0, "ct": 2000},
        move(28500, AGONY, mm.BOMB_DODGE, 43.0, 0.0),
        move(28600, TREE, mm.BOMB_DODGE, 10.0, 45.0),

        {"t": 30000, "e": "death", "g": SHADOW, "killer": VX, "blow": [VX, 2000], "x": 3.0, "y": 0.0},
        {"t": 38200, "e": "dmg", "s": BOMB, "d": BULWARK, "sp": 65333, "a": 40000},
        {"t": 39000, "e": "death", "g": BULWARK, "killer": BULWARK, "cause": "reset", "x": -3.0, "y": 0.0},
        {"t": 40000, "e": "end", "out": "wipe"},
    ]
    records += [snap(when) for when in range(0, 40000, 250)]
    return records


class SyntheticPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "603_2_mimiron_1789700000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in mimiron_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_phases_number_the_handovers_and_the_gaps(self):
        self.assertEqual(mm.phases(self.trace),
                         [(0, 1000, "Z1"), (1000, 5000, "P1"), (5000, 8000, "H1"), (8000, 40000, "P2")])

    def test_the_wipe_command_is_not_a_phase_death(self):
        dead = mm.deaths_by_phase(self.trace)
        self.assertEqual([rec["g"] for rec in dead["P2"]], [SHADOW])
        self.assertEqual(sum(len(rows) for rows in dead.values()), 1)

    def test_walking_is_charged_to_the_move_that_started_it(self):
        cell = mm.walk_table(self.trace)[("P2", "ranged")]
        self.assertEqual(cell["by"][mm.BURST_STEP], 2000)
        self.assertEqual(cell["by"][mm.FORMATION], 1000)
        self.assertEqual(cell["moving"], 3000)

    def test_a_step_walked_back_and_taken_again_is_one_flip(self):
        pair = " <-> ".join(sorted((mm.FORMATION, mm.BURST_STEP)))
        self.assertEqual(mm.flip_counts(self.trace), {("P2", pair): 1})

    def test_formation_yards_go_to_what_moved_the_bot_off(self):
        self.assertEqual(mm.formation_yards(self.trace)[("P2", mm.BURST_STEP)], [1, 8.0])

    def test_a_burst_sorts_the_raid_by_the_carrier_cone(self):
        rows = mm.burst_rows(self.trace)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["carrier"], AGONY)
        self.assertEqual(rows[0]["carrier_ticks"], 3)
        self.assertEqual(rows[0]["cone"], [(SHADOW, 1, False)])
        self.assertEqual({guid for guid, _, _ in rows[0]["outside"]}, {TREE, BULWARK})

    def test_the_fuse_runs_from_the_summon(self):
        rows = mm.bomb_rows(self.trace)
        self.assertEqual(len(rows), 1)
        row = rows[0]
        self.assertEqual(row["seen"], 28000)
        self.assertEqual(sum(row["inside"].values()), 4)
        self.assertEqual((row["escapes"], row["short_stand"], row["short_clearance"]), (2, 0, 1))
        self.assertEqual(row["hits"], 1)

    def test_slot_changes_split_by_cause(self):
        cell = mm.slot_churn(self.trace)["P2"]
        self.assertEqual(cell["new"], 2)
        self.assertEqual(cell["anchor"], [1.0])
        self.assertEqual(len(cell["count"]), 1)
        self.assertAlmostEqual(cell["count"][0], 29 ** 0.5)
        self.assertEqual(cell["index"], [])

    def test_the_healing_race_reads_effective_healing(self):
        first = mm.heal_rows(self.trace)[0]
        self.assertEqual(first["heat"], 2000)
        self.assertEqual(first["healed"], 2000)


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            mm.show_banner(trace)
            for _, _, section in mm.SECTIONS:
                section(trace)
        self.assertIn("not a Mimiron pull", out.getvalue())


if __name__ == "__main__":
    unittest.main()
