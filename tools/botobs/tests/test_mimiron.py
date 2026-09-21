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

ROSTER = [
    {"g": AGONY, "n": "Agony", "r": "ranged", "c": "warlock", "h": 0},
    {"g": TREE, "n": "Tree", "r": "heal", "c": "druid", "h": 0},
    {"g": SHADOW, "n": "Shadow", "r": "melee", "c": "rogue", "h": 0},
    {"g": BULWARK, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
]


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
         "roster": ROSTER},
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


# A second pull for the sections that need a barrage window and a phase 3: the Spinning Up cast at
# 16 s, its 14.5 s window and the 15 s after it all sit inside phase 2, so nothing phase 3 does
# lands in the storm the --spin section measures.
LATE_VX = 4294971000
LATE_ACU = 4294971001
FIREBOT_A = 4294971002
FIREBOT_B = 4294971003
ASSAULT = 4294971004
# A spread flame up and left of fire bot A, so its next spray lane runs straight through Tree.
FLAME = 4294971005

SPIN_AT = 16000
P3_AT = 50000


def late_spot(guid: int, when: int) -> tuple[float, float]:
    # Shadow is inside melee range of VX-001 for exactly the barrage window and out of it otherwise.
    if guid == SHADOW:
        return (9.0, 0.0) if SPIN_AT <= when < SPIN_AT + mm.SPIN_WINDOW_MS else (14.0, 0.0)
    return HOME[guid]


def late_target(guid: int, when: int) -> int:
    return FIREBOT_B if guid == AGONY and 51000 <= when < 53000 else LATE_VX


def late_acu_hp(when: int) -> float:
    """1 point a second in the air, 4 on the floor: grounded runs 53 s to 57 s."""
    if when < 53000:
        return 60.0 - (when - P3_AT) / 1000.0
    if when < 57000:
        return 57.0 - 4.0 * (when - 53000) / 1000.0
    return 41.0 - (when - 57000) / 1000.0


def late_snap(when: int) -> dict:
    rows = [[LATE_VX, 0.0, 0.0, 364.0, 0.0, 90.0, 0.0, BULWARK, 0, 0, 0, 0, 0, 0.0]]
    for guid in HOME:
        x, y = late_spot(guid, when)
        alive = 0.0 if guid == SHADOW and when > 54100 else 100.0
        # Tree walks for the whole 15 s after the barrage, and only Agony deals damage: 10 a second.
        walking = int(guid == TREE and 30500 <= when < 45500)
        dealt = 10 * (when // 1000) if guid == AGONY else 0
        rows.append([guid, x, y, 364.0, 0.0, alive, 80.0, late_target(guid, when), walking, 0, 0,
                     dealt, 0, 0.0])
    if P3_AT <= when <= 65000:
        rows.append([LATE_ACU, 0.0, 0.0, 380.0, 0.0, late_acu_hp(when), 0.0, BULWARK, 0, 0, 0, 0, 0, 0.0])
    if P3_AT <= when < 62000:
        rows.append([FIREBOT_A, 12.0, 12.0, 364.0, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0, 0, 0.0])
        rows.append([FLAME, 6.0, 17.0, 364.0, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0, 0, 0.0])
    if P3_AT <= when < 55000:
        rows.append([FIREBOT_B, -12.0, 12.0, 364.0, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0, 0, 0.0])
    if P3_AT <= when < 54000:
        rows.append([ASSAULT, 5.0, 5.0, 364.0, 0.0, 100.0, 0.0, BULWARK, 0, 0, 0, 0, 0, 0.0])
    return {"t": when, "e": "snap", "u": rows, "hz": []}


def late_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 13, "ts": 1789700000000, "map": 603, "inst": 3, "diff": 1, "boss": "mimiron",
         "roster": ROSTER},
        {"t": 0, "e": "pull", "boss": "mimiron", "src": "engage"},
        {"t": 1, "e": "unit", "g": LATE_VX, "en": mm.NPC_VX001, "n": "VX-001", "b": 1},
        {"t": 1, "e": "unit", "g": LATE_ACU, "en": mm.NPC_AERIAL_COMMAND_UNIT, "n": "Aerial Command Unit",
         "b": 1},
        {"t": 1, "e": "unit", "g": FIREBOT_A, "en": mm.NPC_EMERGENCY_FIRE_BOT, "n": "Emergency Fire Bot"},
        {"t": 1, "e": "unit", "g": FIREBOT_B, "en": mm.NPC_EMERGENCY_FIRE_BOT, "n": "Emergency Fire Bot"},
        {"t": 1, "e": "unit", "g": ASSAULT, "en": mm.NPC_ASSAULT_BOT, "n": "Assault Bot"},
        {"t": 1, "e": "unit", "g": FLAME, "en": mm.NPC_FLAMES_SPREAD, "n": "Flames (Spread)"},

        # phase 4 twice, with a handover between them
        {"t": 0, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "2"},
        {"t": P3_AT, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "3"},
        {"t": 62000, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "4"},
        {"t": 68000, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "5"},
        {"t": 71000, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "4"},

        {"t": SPIN_AT, "e": "cast", "s": LATE_VX, "sp": mm.SPELL_SPINNING_UP, "tgt": 0, "ct": 4000},
        {"t": 17000, "e": "dmg", "s": LATE_VX, "d": TREE, "sp": mm.SPELL_FLAMES, "a": 5000},

        # the storm after the window: one hit, one heal, one formation leg
        {"t": 31000, "e": "dmg", "s": LATE_VX, "d": BULWARK, "sp": mm.SPELL_HEAT_WAVE, "a": 40000},
        {"t": 31500, "e": "heal", "s": TREE, "d": BULWARK, "sp": 48441, "a": 10000, "oh": 2000},
        move(32000, TREE, mm.FORMATION, 0.0, 20.0),

        # two cores banked, the first spent at 52.9 s and the second chained as the landing ends
        {"t": 50500, "e": "note", "g": AGONY, "k": "mimiron.corestep", "txt": "loot"},
        {"t": 51000, "e": "note", "g": AGONY, "k": "mimiron.corestep", "txt": "hold"},
        {"t": 51500, "e": "note", "g": AGONY, "k": "mimiron.corestep", "txt": "loot-second"},
        {"t": 52900, "e": "note", "g": AGONY, "k": "mimiron.corestep", "txt": "use"},
        {"t": 53000, "e": "note", "g": AGONY, "k": "mimiron.core", "txt": "1"},
        {"t": 54000, "e": "note", "g": AGONY, "k": "mimiron.corestep", "txt": "pending"},
        {"t": 57000, "e": "note", "g": AGONY, "k": "mimiron.core", "txt": "0"},
        {"t": 57500, "e": "note", "g": AGONY, "k": "mimiron.corestep", "txt": "chain"},
        {"t": 57500, "e": "note", "g": AGONY, "k": "mimiron.corestep", "txt": "use"},

        {"t": 52000, "e": "aura", "d": TREE, "s": BULWARK, "sp": 48945, "r": 0, "st": 1, "dur": -1, "p": 1},
        {"t": 54000, "e": "dmg", "s": FIREBOT_A, "d": TREE, "sp": mm.SPELL_WATER_SPRAY, "a": 23000,
         "rs": 5000},
        {"t": 54100, "e": "death", "g": SHADOW, "killer": FIREBOT_A, "blow": [FIREBOT_A, 24000],
         "x": 14.0, "y": 0.0},
        {"t": 75000, "e": "end", "out": "wipe"},
    ]
    records += [late_snap(when) for when in range(0, 75000, 250)]
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


class LaterPhases(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "603_3_mimiron_1789700001.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in late_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_phase_that_comes_round_twice_gets_its_own_label(self):
        self.assertEqual(mm.phases(self.trace),
                         [(0, 50000, "P2"), (50000, 62000, "P3"), (62000, 68000, "P4"),
                          (68000, 71000, "H1"), (71000, 75000, "P4b")])

    def test_the_barrage_window_is_measured_against_the_time_before_it(self):
        rows = mm.spin_rows(self.trace)
        self.assertEqual(len(rows), 1)
        row = rows[0]
        self.assertEqual((row["dealt_before"], row["dealt_inside"]), (150, 140))
        self.assertEqual(row["melee_in_range"], row["melee_rows"])
        self.assertEqual(row["fire"], 5000)
        self.assertEqual(row["deaths"], [])

    def test_the_storm_after_the_window_is_its_own_count(self):
        row = mm.spin_rows(self.trace)[0]
        self.assertEqual((row["taken"], row["healed"]), (40000, 8000))
        self.assertEqual(row["heal_moving"], 1.0)
        self.assertEqual((row["legs"], row["yards"]), (1, 2.0))

    def test_phase_3_counts_every_fire_bot_and_who_shot_one(self):
        rows = mm.p3_rows(self.trace)
        self.assertEqual(rows["alive_max"], 2)
        self.assertAlmostEqual(rows["alive_mean"], 68 / 48)
        self.assertEqual(rows["firebots"][FIREBOT_B]["aimed"], 8)
        self.assertEqual(rows["aimed_seconds"], 2.0)
        self.assertEqual(rows["assaults"], [3750])

    def test_water_spray_damage_and_the_death_it_caused(self):
        rows = mm.p3_rows(self.trace)
        self.assertEqual((rows["spray"], rows["spray_deaths"]), (23000, 1))
        self.assertEqual(dict(rows["coresteps"]),
                         {"loot": 1, "hold": 1, "loot-second": 1, "use": 2, "pending": 1, "chain": 1})

    def test_a_spray_reads_its_resist_the_aura_and_the_lane_it_came_down(self):
        rows = mm.p3_rows(self.trace)
        self.assertEqual(rows["spray_hits"], 1)
        self.assertAlmostEqual(rows["spray_resisted"], 5000 / 28000)
        self.assertEqual(rows["spray_on_aura"], 1)
        self.assertEqual(rows["spray_in_lane"], 1)

    def test_the_core_ledger_pairs_each_use_with_the_oldest_core_held(self):
        cores = mm.p3_rows(self.trace)["cores"]
        self.assertEqual((cores["looted"], cores["used"], cores["lost"], cores["chained"]), (2, 2, 0, 1))
        self.assertEqual(cores["held"], [2400, 6000])

    def test_the_unit_only_dies_on_the_floor(self):
        rows = mm.p3_rows(self.trace)
        self.assertEqual(rows["grounded"], (4000, 16.0))
        self.assertEqual(rows["airborne"], (8000, 8.0))


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
