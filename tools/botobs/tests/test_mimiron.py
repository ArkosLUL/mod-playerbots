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
        {"t": 9200, "e": "cast", "s": BULWARK, "sp": 64205, "tgt": 0, "ct": 0},
        {"t": 9300, "e": "cast", "s": AGONY, "sp": 32182, "tgt": 0, "ct": 0},

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
    # Bulwark stands under the model for the window, where the beams hit at any bearing.
    if guid == BULWARK and SPIN_AT <= when < SPIN_AT + mm.SPIN_WINDOW_MS:
        return (-2.0, 0.0)
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
        # Down on the floor for the first second of the core window, then back up with the aura still
        # on, which is what a second core inside a live one does to it.
        z = 364.0 if 53000 <= when < 54000 else (385.0 if 54000 <= when < 57000 else 380.0)
        rows.append([LATE_ACU, 0.0, 0.0, z, 0.0, late_acu_hp(when), 0.0, BULWARK, 0, 0, 0, 0, 0, 0.0])
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
        # a mine on the healer counts, the same mine on ranged doesn't
        {"t": 18000, "e": "dmg", "s": LATE_VX, "d": TREE, "sp": 63009, "a": 9600},
        {"t": 18000, "e": "dmg", "s": LATE_VX, "d": AGONY, "sp": 63009, "a": 9600},
        {"t": 21000, "e": "dmg", "s": LATE_VX, "d": BULWARK, "sp": mm.SPELL_LASER_BARRAGE, "a": 20000},
        {"t": 21000, "e": "dmg", "s": LATE_VX, "d": SHADOW, "sp": mm.SPELL_LASER_BARRAGE, "a": 20000},

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


# A phase 2 for --fire: Agony dodges twice away from VX-001, once just after a tick and once clean at full
# health, Tree once toward it, and the formation notes one of each approach kind.
def fire_snap(when: int) -> dict:
    rows = [[VX, 0.0, 0.0, 364.0, 0.0, 90.0, 0.0, BULWARK, 0, 0, 0, 0, 0, 0.0]]
    for guid, (x, y) in HOME.items():
        rows.append([guid, x, y, 364.0, 0.0, 100.0, 80.0, VX, 0, 0, 0, 0, 0, 0.0])
    return {"t": when, "e": "snap", "u": rows, "hz": []}


def fire_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 14, "ts": 1789700000000, "map": 603, "inst": 4, "diff": 1, "boss": "mimiron",
         "roster": ROSTER},
        {"t": 0, "e": "pull", "boss": "mimiron", "src": "engage"},
        {"t": 1, "e": "unit", "g": VX, "en": mm.NPC_VX001, "n": "VX-001", "b": 1},
        {"t": 0, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "2"},
        {"t": 0, "e": "note", "g": AGONY, "k": "mimiron.slot", "txt": "hmwedge 0/2 20,0,364"},

        {"t": 4500, "e": "dmg", "s": VX, "d": AGONY, "sp": mm.SPELL_FLAMES, "a": 3000},
        # one dodge in two legs, then a clean one; Tree's goes toward VX-001
        move(5000, AGONY, mm.FLAMES_DODGE, 30.0, 0.0),
        move(5200, AGONY, mm.FLAMES_DODGE, 31.0, 0.0),
        move(10000, AGONY, mm.FLAMES_DODGE, 26.0, 0.0),
        move(12000, TREE, mm.FLAMES_DODGE, 0.0, 15.0),

        # a detour from (22, 0) via (22, 6) to the slot at (20, 0)
        {"t": 14000, "e": "note", "g": AGONY, "k": "mimiron.approach", "txt": "detour +35"},
        move(14000, AGONY, mm.FORMATION, 22.0, 6.0),
        {"t": 15000, "e": "note", "g": AGONY, "k": "mimiron.approach", "txt": "cross 3.2"},
        {"t": 16000, "e": "note", "g": TREE, "k": "mimiron.approach", "txt": "none"},
        {"t": 20000, "e": "end", "out": "wipe"},
    ]
    records += [fire_snap(when) for when in range(0, 20000, 250)]
    return records


class FirePull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "603_4_mimiron_1789700002.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in fire_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_legs_close_together_are_one_dodge_judged_by_the_tick_before_it(self):
        row = mm.fire_rows(self.trace)["P2"]
        self.assertEqual((row["dodges"], row["burned"], row["full_unburned"]), (3, 1, 2))

    def test_a_dodge_is_judged_against_the_bot_target(self):
        row = mm.fire_rows(self.trace)["P2"]
        self.assertEqual((row["farther"], row["closer"], row["judged"]), (2, 1, 3))

    def test_approach_kinds_and_the_yards_a_detour_added(self):
        row = mm.fire_rows(self.trace)["P2"]
        self.assertEqual(dict(row["approach"]), {"detour": 1, "cross": 1, "none": 1})
        self.assertEqual(len(row["detour_extra"]), 1)
        self.assertAlmostEqual(row["detour_extra"][0], 6.0 + 40 ** 0.5 - 2.0)
        self.assertEqual(dict(row["fire"]), {"ranged": 3000})


# A 1 to 2 handover from 11 s to 59 s: both bots hold 40 yd west, walk in from 46 s, and at phase 2
# Agony stands on her wedge slot while Tree is 9 yd short of hers.
CX, CY = mm.ROOM_CENTRE
LEAD = 5009  # a human healer who stays on the room centre throughout


def handover_spot(guid: int, when: int) -> tuple[float, float]:
    if when <= 11000:
        return CX - 30.0, CY
    if when < 50000:
        return (CX - 40.0, CY) if guid == AGONY else (CX - 40.0, CY + 4.0)
    return (2767.0, 2569.0) if guid == AGONY else (2744.0, 2600.0)


def handover_snap(when: int) -> dict:
    rows = []
    for guid in (AGONY, TREE):
        x, y = handover_spot(guid, when)
        mana = 40.0 if when <= 11000 else 90.0
        rows.append([guid, x, y, 364.0, 0.0, 100.0, mana, 0, 0, 0, 0, 0, 0, 0.0])
    return {"t": when, "e": "snap", "u": rows, "hz": []}


def handover_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 14, "ts": 1789700000000, "map": 603, "inst": 6, "diff": 1, "boss": "mimiron",
         "roster": ROSTER},
        {"t": 0, "e": "pull", "boss": "mimiron", "src": "engage"},
        {"t": 0, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "0"},
        {"t": 1000, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "1"},
        {"t": 11000, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "0"},
        {"t": 28000, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "5"},
        {"t": 59000, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "2"},

        {"t": 1000, "e": "note", "g": AGONY, "k": "mimiron.handover", "txt": "0"},
        {"t": 11000, "e": "note", "g": AGONY, "k": "mimiron.handover", "txt": "1"},
        {"t": 11000, "e": "note", "g": AGONY, "k": "mimiron.hold", "txt": "180 deg 0 fire"},
        {"t": 28000, "e": "note", "g": AGONY, "k": "mimiron.handover", "txt": "2"},
        {"t": 46000, "e": "note", "g": AGONY, "k": "mimiron.handover", "txt": "3"},
        {"t": 59000, "e": "note", "g": AGONY, "k": "mimiron.handover", "txt": "0"},

        # a phase 1 slot, then the band, then the walk in
        {"t": 2000, "e": "note", "g": AGONY, "k": "mimiron.slot", "txt": "p1stack 0/2 2716,2580,364"},
        {"t": 11000, "e": "note", "g": AGONY, "k": "mimiron.slot", "txt": "hmhold 0/2 2705,2569,364"},
        {"t": 11000, "e": "note", "g": TREE, "k": "mimiron.slot", "txt": "hmhold 1/2 2705,2573,364"},
        {"t": 46000, "e": "note", "g": AGONY, "k": "mimiron.slot", "txt": "hmwedge 0/2 2767,2569,364"},
        {"t": 46000, "e": "note", "g": TREE, "k": "mimiron.slot", "txt": "hmwedge 1/2 2744,2591,364"},

        {"t": 15000, "e": "act", "g": TREE, "a": "drink", "rel": 4.2, "vd": "OK"},
        {"t": 16000, "e": "act", "g": AGONY, "a": "drink", "rel": 4.2, "vd": "IMPOSSIBLE"},
        {"t": 20000, "e": "dmg", "s": 4294970300, "d": AGONY, "sp": mm.SPELL_FLAMES, "a": 3000},
        {"t": 60000, "e": "dmg", "s": 4294970300, "d": AGONY, "sp": mm.SPELL_FLAMES, "a": 3000},
        {"t": 62000, "e": "end", "out": "wipe"},
    ]
    records += [handover_snap(when) for when in range(0, 62000, 250)]
    return records


class HandoverPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "603_6_mimiron_1789700004.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in handover_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_the_handover_runs_from_phase_1_to_phase_2_with_each_step_once(self):
        row = mm.handover_rows(self.trace)[0]
        self.assertEqual(row["span"], (11000, 59000))
        self.assertEqual(row["steps"], {"1": 11000, "2": 28000, "3": 46000})
        self.assertEqual(row["hold"], "180 deg 0 fire")

    def test_the_raid_is_measured_from_the_room_centre(self):
        spread = mm.handover_rows(self.trace)[0]["spread"]
        self.assertAlmostEqual(spread["walk in"][0], (40.0 + (40.0 ** 2 + 4.0 ** 2) ** 0.5) / 2)
        self.assertAlmostEqual(spread["P1 end"][1], 30.0)

    def test_only_a_handover_slot_counts_and_only_within_the_tolerance(self):
        row = mm.handover_rows(self.trace)[0]
        self.assertEqual((row["on_slot"], row["slotted"], row["living"]), (1, 2, 2))

    def test_mana_drinks_and_fire_inside_the_handover_only(self):
        row = mm.handover_rows(self.trace)[0]
        self.assertEqual(row["mana"], [(40.0, 90.0)])
        self.assertEqual(row["drinks"], [("Tree", 4.0)])
        self.assertEqual((row["fire"], row["dead"]), (3000, []))

    def test_a_phase_1_blip_still_finds_the_handover(self):
        records = handover_pull()
        at = next(index for index, rec in enumerate(records)
                  if rec.get("k") == "mimiron.phase" and rec.get("txt") == "1") + 1
        records[at:at] = [{"t": 5000, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "0"},
                          {"t": 5300, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "1"}]
        path = pathlib.Path(self.folder.name) / "603_6_mimiron_1789700005.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in records) + "\n", encoding="utf-8")
        self.assertEqual([row["span"] for row in mm.handover_rows(Trace(path))], [(11000, 59000)])

    def test_a_human_elsewhere_counts_in_no_position_or_mana(self):
        records = handover_pull()
        records[0] = dict(records[0], roster=ROSTER + [{"g": LEAD, "n": "Lead", "r": "heal", "c": "priest",
                                                         "h": 1}])
        for rec in records:
            if rec.get("e") == "snap":
                rec["u"].append([LEAD, CX, CY, 364.0, 0.0, 100.0, 5.0, 0, 0, 0, 0, 0, 0, 0.0])
        path = pathlib.Path(self.folder.name) / "603_6_mimiron_1789700006.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in records) + "\n", encoding="utf-8")
        row = mm.handover_rows(Trace(path))[0]
        self.assertEqual(row["spread"], mm.handover_rows(self.trace)[0]["spread"])
        self.assertEqual((row["on_slot"], row["slotted"], row["living"]), (1, 2, 2))
        self.assertEqual(row["mana"], [(40.0, 90.0)])


MINE_A, MINE_B, MINE_C, MINE_D = 4294970201, 4294970202, 4294970203, 4294970204
MINES = {MINE_A: (30.0, 0.0), MINE_B: (35.0, 0.5), MINE_C: (10.0, 0.0), MINE_D: (-3.0, 1.0)}


def mine_snap(when: int) -> dict:
    rows = [[guid, x, y, 364.0, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0, 0, 0.0] for guid, (x, y) in MINES.items()]
    for guid, (x, y) in HOME.items():
        walking = guid == AGONY and (1000 <= when < 3000 or 13500 <= when < 14500)
        rows.append([guid, x, y, 364.0, 0.0, 100.0, 80.0, VX, int(walking), 0, 0, 0, 0, 0.0])
    return {"t": when, "e": "snap", "u": rows, "hz": []}


def mine_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 14, "ts": 1789700000000, "map": 603, "inst": 5, "diff": 1, "boss": "mimiron",
         "roster": ROSTER},
        {"t": 0, "e": "pull", "boss": "mimiron", "src": "engage"},
        {"t": 0, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "4"},
    ]
    records += [{"t": 1, "e": "unit", "g": guid, "en": mm.NPC_PROXIMITY_MINE, "n": "Proximity Mine"}
                for guid in MINES]
    records += [
        # past two mines on the way out, and hit by one a second in
        move(1000, AGONY, "mimiron shock blast action", 40.0, 0.0),
        {"t": 2000, "e": "dmg", "s": MINE_A, "d": AGONY, "sp": mm.SPELL_MINE_EXPLOSIONS[0], "a": 10000},
        # standing still, and killed by it
        {"t": 5000, "e": "dmg", "s": MINE_C, "d": TREE, "sp": mm.SPELL_MINE_EXPLOSIONS[1], "a": 9600},
        {"t": 5001, "e": "death", "g": TREE, "killer": MINE_C, "x": 0.0, "y": 22.0, "z": 364.0},
        # ends beside a mine; then a walk off the mine the bot stands by, which counts for neither
        move(6000, SHADOW, mm.FORMATION, 9.0, 2.0),
        move(7000, BULWARK, mm.FORMATION, -3.0, 10.0),
        # under a yard, so no leg at all; then a hit 5 s after it while walking
        move(9000, AGONY, "mimiron shock blast action", 22.5, 0.0),
        {"t": 14000, "e": "dmg", "s": MINE_B, "d": AGONY, "sp": mm.SPELL_MINE_EXPLOSIONS[0], "a": 5000},
        {"t": 20000, "e": "end", "out": "wipe"},
    ]
    records += [mine_snap(when) for when in range(0, 20000, 250)]
    return records


class MinePull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "603_5_mimiron_1789700003.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in mine_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_hit_goes_to_the_recent_leg_of_a_moving_bot_else_standing(self):
        row = mm.mine_rows(self.trace)["P4"]
        self.assertEqual(dict(row["hits"]),
                         {"mimiron shock blast action": 1, mm.STANDING: 1, mm.OLDER_LEG: 1})
        self.assertEqual(dict(row["damage"]),
                         {"mimiron shock blast action": 10000, mm.STANDING: 9600, mm.OLDER_LEG: 5000})
        self.assertEqual(dict(row["kills"]), {mm.STANDING: 1})

    def test_legs_count_mines_passed_and_ended_beside_but_not_the_one_left(self):
        legs = mm.mine_rows(self.trace)["P4"]["legs"]
        self.assertEqual(legs["mimiron shock blast action"], [1, 1, 1, 0])
        self.assertEqual(legs[mm.FORMATION], [2, 1, 0, 1])

    def test_the_view_prints_per_phase(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            mm.show_mines(self.trace)
        self.assertIn("P4   hits 3 for 24k, killing blows 1", out.getvalue())


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

    def test_a_burst_on_ranged_that_hits_melee_is_a_cross_tick(self):
        cross = mm.burst_cross(self.trace, mm.burst_rows(self.trace))
        self.assertEqual(cross[("ranged", "ranged")], 3)
        self.assertEqual(cross[("ranged", "melee")], 1)
        self.assertEqual(sum(cross.values()), 4)

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
        self.assertIn("guardian", first["events"])

    def test_the_lust_lands_in_the_bucket_it_was_cast_in(self):
        rows = mm.heal_rows(self.trace)
        self.assertIn("lust", rows[0]["events"])
        self.assertNotIn("lust", rows[1]["events"])


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
        self.assertEqual(row["heal_mines"], 9600)
        self.assertEqual((row["beam"], row["boundary"]), (2, 1))
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

    def test_a_latched_landing_that_climbs_back_counts_only_the_floor(self):
        landing = mm.p3_rows(self.trace)["landings"][0]
        self.assertEqual(landing["span"], (53000, 57000))
        # 4 of the window's 17 samples are on the floor, and one use fed it.
        self.assertAlmostEqual(landing["floor"], 4.0 * 4 / 17)
        self.assertEqual(landing["cores"], 1)


MKII = 4294970400


def unreach_snap(when: int) -> dict:
    if when < 4000:
        health = 70.0 - when / 1000.0
    elif when < 6000:
        health = 66.0
    elif when < 9000:
        health = 99.0
    elif when < 20000:
        health = 0.5
    else:
        health = 50.0
    rows = [[MKII, 0.0, 0.0, 364.0, 0.0, health, 0.0, BULWARK, 0, 5, 0, 0, 0, 0.0]]
    rows += [[guid, x, y, 364.0, 0.0, 100.0, 80.0, MKII, 0, 0, 0, 0, 0, 0.0] for guid, (x, y) in HOME.items()]
    return {"t": when, "e": "snap", "u": rows, "hz": []}


def unreach_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 13, "ts": 1789700000000, "map": 603, "inst": 6, "diff": 1, "boss": "mimiron",
         "roster": ROSTER},
        {"t": 0, "e": "pull", "boss": "mimiron", "src": "engage"},
        {"t": 1, "e": "unit", "g": MKII, "en": mm.NPC_LEVIATHAN_MKII, "n": "Leviathan Mk II", "b": 1},
        {"t": 0, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "1"},
        {"t": 0, "e": "note", "g": AGONY, "k": "mimiron.unreach", "txt": "0"},
        {"t": 4000, "e": "note", "g": AGONY, "k": "mimiron.unreach", "txt": str(BULWARK)},
        {"t": 4100, "e": "note", "g": BULWARK, "k": "mimiron.flee",
         "txt": "unreach ok +0 (back0 mine0 cone0 fire0 bomb0 rocket0 burst0 shock0 spray0 move0 hold0)"},
        {"t": 4400, "e": "note", "g": BULWARK, "k": "mimiron.flee",
         "txt": "unreach locked (back0 mine0 cone0 fire0 bomb0 rocket0 burst0 shock0 spray0 move1 hold0)"},
        {"t": 7000, "e": "note", "g": AGONY, "k": "mimiron.unreach", "txt": "0"},
        {"t": 25000, "e": "end", "out": "wipe"},
    ]
    records += [unreach_snap(when) for when in range(0, 25000, 250)]
    return records


class UnreachPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "603_6_mimiron_1789700004.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in unreach_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_latch_spans_until_it_names_nobody_and_counts_its_victim_steps(self):
        rows = mm.unreach_rows(self.trace)
        self.assertTrue(rows["probed"])
        self.assertEqual(len(rows["latched"]), 1)
        span = rows["latched"][0]
        self.assertEqual(span["span"], (4000, 7000))
        self.assertEqual(span["victim"], BULWARK)
        self.assertEqual(span["health"], (66.0, 99.0, 99.0))
        self.assertEqual((span["ok"], span["steps"]), (1, 2))

    def test_a_rise_off_the_repair_floor_counts_and_self_repair_does_not(self):
        rises = mm.unreach_rows(self.trace)["rises"]
        self.assertEqual([(rise["t"], rise["from"], rise["to"]) for rise in rises], [(6000, 66.0, 99.0)])

    def test_the_view_names_the_victim(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            mm.show_unreach(self.trace)
        self.assertIn("Bulwark", out.getvalue())
        self.assertIn("rises off the repair floor: 1", out.getvalue())


KEPT_A = 4294970500
KEPT_B = 4294970501


def firebot_snap(when: int) -> dict:
    rows = []
    for guid, (x, y) in HOME.items():
        alive = 0.0 if guid == SHADOW and when >= 17000 else 100.0
        target = KEPT_A if guid == AGONY and 15000 <= when < 16000 else 0
        rows.append([guid, x, y, 364.0, 0.0, alive, 80.0, target, 0, 0, 0, 0, 0, 0.0])
    rows.append([KEPT_A, 10.0, 10.0, 364.0, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0, 0, 0.0])
    if when < 12000:
        rows.append([KEPT_B, -10.0, 10.0, 364.0, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0, 0, 0.0])
    return {"t": when, "e": "snap", "u": rows, "hz": []}


def siren(when: int, guid: int, removed: int) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": KEPT_A, "sp": mm.SPELL_DEAFENING_SIREN, "r": removed,
            "st": 1, "dur": -1, "p": 0}


def veto(when: int, guard: str) -> dict:
    return {"t": when, "e": "veto", "g": AGONY, "m": guard, "a": "rain of fire"}


def firebot_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 13, "ts": 1789700000000, "map": 603, "inst": 7, "diff": 1, "boss": "mimiron",
         "roster": ROSTER},
        {"t": 0, "e": "pull", "boss": "mimiron", "src": "engage"},
        {"t": 1, "e": "unit", "g": KEPT_A, "en": mm.NPC_EMERGENCY_FIRE_BOT, "n": "Emergency Fire Bot"},
        {"t": 1, "e": "unit", "g": KEPT_B, "en": mm.NPC_EMERGENCY_FIRE_BOT, "n": "Emergency Fire Bot"},
        {"t": 0, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "3"},
        {"t": 10000, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "5"},
        {"t": 14000, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "4"},

        {"t": 5000, "e": "dmg", "s": KEPT_A, "d": TREE, "sp": mm.SPELL_FLAMES, "a": 3000},
        {"t": 15000, "e": "dmg", "s": KEPT_A, "d": AGONY, "sp": mm.SPELL_FLAMES, "a": 3100},
        {"t": 16000, "e": "dmg", "s": KEPT_A, "d": AGONY, "sp": mm.SPELL_FLAMES, "a": 3100},
        {"t": 16000, "e": "dmg", "s": KEPT_A, "d": KEPT_B, "sp": mm.SPELL_FLAMES, "a": 3100},
        {"t": 17000, "e": "dmg", "s": KEPT_A, "d": SHADOW, "sp": mm.SPELL_WATER_SPRAY, "a": 24000},
        {"t": 17000, "e": "death", "g": SHADOW, "killer": KEPT_A, "blow": [KEPT_A, 24000], "x": 3.0, "y": 0.0},

        siren(9000, AGONY, 0), siren(11000, AGONY, 1),
        siren(15000, TREE, 0), siren(17000, TREE, 1),
        siren(19000, BULWARK, 0),

        veto(8000, mm.FIREBOT_AOE_GUARD),
        veto(15500, mm.FIREBOT_AOE_GUARD), veto(16500, mm.FIREBOT_AOE_GUARD), veto(16600, "mimiron target guard"),

        move(17500, TREE, mm.FIREBOT_STEP, 5.0, 22.0),
        {**move(17600, TREE, mm.FIREBOT_STEP, 6.0, 22.0), "ok": 0},
        move(18000, TREE, mm.FORMATION, 0.0, 22.0),
        {"t": 16500, "e": "cast", "s": VX, "sp": mm.SPELL_SPINNING_UP, "tgt": 0, "ct": 4000},
        {"t": 13500, "e": "note", "g": SHADOW, "k": "mimiron.flee", "txt": "spray ok +0 (back0)"},
        {"t": 16900, "e": "note", "g": SHADOW, "k": "mimiron.flee", "txt": "flames+3 ok +0 (back0)"},
        {"t": 20000, "e": "end", "out": "wipe"},
    ]
    records += [firebot_snap(when) for when in range(0, 20000, 250)]
    return records


class FireBotPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "603_7_mimiron_1789700005.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in firebot_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)
        cls.rows = {row["label"]: row for row in mm.firebot_rows(cls.trace)}

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_one_row_per_phase_from_phase_3_on(self):
        self.assertEqual(list(self.rows), ["P3", "H1", "P4"])

    def test_fire_bots_alive_at_each_open_and_close_and_the_one_gone_in_the_handover(self):
        counts = {label: (row["open"], row["close"], row["max"], row["seen"], row["gone"])
                  for label, row in self.rows.items()}
        self.assertEqual(counts, {"P3": (2, 2, 2, 2, 0), "H1": (2, 1, 2, 2, 1), "P4": (1, 1, 1, 1, 0)})
        self.assertEqual(self.rows["H1"]["mean"], 1.5)

    def test_flames_and_spray_on_the_raid_only(self):
        p4 = self.rows["P4"]
        self.assertEqual((p4["flames"], p4["flames_ticks"]), (6200, 2))
        self.assertEqual((p4["spray"], p4["spray_hits"], p4["spray_deaths"]), (24000, 1, 1))
        self.assertEqual((self.rows["P3"]["flames"], self.rows["P3"]["spray_hits"]), (3000, 0))

    def test_a_silence_counts_where_it_landed_and_its_time_where_it_ran(self):
        self.assertEqual([self.rows[label]["siren"] for label in ("P3", "H1", "P4")], [1, 0, 2])
        self.assertEqual([self.rows[label]["silenced"] for label in ("P3", "H1", "P4")], [1.0, 1.0, 3.0])

    def test_aim_the_splash_guard_and_the_accepted_spray_steps(self):
        p4 = self.rows["P4"]
        self.assertEqual((p4["aimed"], p4["aoe_holds"], p4["steps"]), (1.0, 2, 1))
        self.assertEqual(self.rows["P3"]["aoe_holds"], 1)

    def test_a_spray_inside_a_barrage_window_with_no_spray_note_in_the_3_s_before(self):
        p4 = self.rows["P4"]
        self.assertEqual(p4["barrage_s"], 3.5)
        self.assertEqual((p4["barrage_spray_hits"], p4["barrage_spray_deaths"], p4["barrage_spray_silent"]),
                         (1, 1, 1))
        self.assertEqual(p4["barrage_steps"], 1)
        self.assertEqual(self.rows["P3"]["barrage_s"], 0.0)

    def test_a_spray_note_in_the_3_s_before_means_the_escape_ran(self):
        records = firebot_pull() + [
            {"t": 15000, "e": "note", "g": SHADOW, "k": "mimiron.flee", "txt": "spray locked (move1)"}]
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / "603_7_mimiron_1789700006.ndjson"
            path.write_text("\n".join(json.dumps(rec) for rec in records) + "\n", encoding="utf-8")
            rows = {row["label"]: row for row in mm.firebot_rows(Trace(path))}
        self.assertEqual(rows["P4"]["barrage_spray_silent"], 0)

    def test_the_cone_s_last_sweep_row_ends_the_barrage_window(self):
        records = firebot_pull() + [
            {"t": when, "e": "haz", "sp": mm.SPELL_SPINNING_UP, "shape": "sweep", "x": 0.0, "y": 0.0, "z": 364.0,
             "ttl": 250, "lead": 1.0, "sweep": 1.8, "rate": 0.18, "live": 0.0} for when in (16600, 16850, 17100)]
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / "603_7_mimiron_1789700007.ndjson"
            path.write_text("\n".join(json.dumps(rec) for rec in records) + "\n", encoding="utf-8")
            rows = {row["label"]: row for row in mm.firebot_rows(Trace(path))}
        p4 = rows["P4"]
        self.assertEqual(p4["barrage_s"], 0.601)
        self.assertEqual((p4["barrage_spray_hits"], p4["barrage_spray_deaths"]), (1, 1))
        self.assertEqual(p4["barrage_steps"], 0)

    def test_the_view_prints_each_phase(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            mm.show_firebots(self.trace)
        self.assertIn("P4", out.getvalue())
        self.assertIn("fire bots open 1", out.getvalue())
        self.assertIn("in barrage windows (4 s): Water Spray 1 hits, 1 dead, 1 with no spray note", out.getvalue())


AIM = 4294970600


def burst_target_pull() -> list[dict]:
    records = []
    for rec in mimiron_pull():
        if rec.get("e") == "cast" and rec.get("sp") == mm.SPELL_RAPID_BURST:
            rec = {**rec, "tgt": AIM}
        elif rec.get("e") == "snap" and 20000 <= rec["t"] < 31000:
            rec = {**rec, "u": rec["u"] + [[AIM, 22.5, 0.5, 364.0, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0, 0, 0.0]]}
        records.append(rec)
    records.append({"t": 20000, "e": "unit", "g": AIM, "en": 34211, "n": "Burst Target"})
    return records


class BurstTargetPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "603_8_mimiron_1789700006.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in burst_target_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_the_raid_member_the_target_was_parked_on_carries_the_burst(self):
        rows = mm.burst_rows(self.trace)
        self.assertEqual(len(rows), 1)
        self.assertEqual((rows[0]["carrier"], rows[0]["carrier_ticks"]), (AGONY, 3))
        self.assertEqual(rows[0]["cone"], [(SHADOW, 1, False)])


CHASER = 4294970700
SPENT = 4294970701


def bombbot_snap(when: int) -> dict:
    rows = []
    for guid, (x, y) in HOME.items():
        alive = 0.0 if guid == TREE and when >= 6000 else 100.0
        rows.append([guid, x, y, 364.0, 0.0, alive, 80.0, 0, 0, 0, 0, 0, 0, 0.0])
    if when < 6000:
        rows.append([CHASER, 22.0 - (6000 - when) / 1000.0, 0.0, 364.0, 0.0, 80.0, 0.0, AGONY, 1, 0, 0, 0, 0, 0.0])
    if when < 3000:
        rows.append([SPENT, -10.0, 0.0, 364.0, 0.0, 100.0 - when / 100.0, 0.0, BULWARK, 1, 0, 0, 0, 0, 0.0])
    return {"t": when, "e": "snap", "u": rows, "hz": []}


def bombbot_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 13, "ts": 1789700000000, "map": 603, "inst": 9, "diff": 1, "boss": "mimiron",
         "roster": ROSTER},
        {"t": 0, "e": "pull", "boss": "mimiron", "src": "engage"},
        {"t": 1, "e": "unit", "g": CHASER, "en": mm.NPC_BOMB_BOT, "n": "Bomb Bot"},
        {"t": 1, "e": "unit", "g": SPENT, "en": mm.NPC_BOMB_BOT, "n": "Bomb Bot"},
        {"t": 0, "e": "note", "g": AGONY, "k": "mimiron.phase", "txt": "3"},

        move(2000, SHADOW, mm.BOMB_BOT_STEP, 3.0, 9.0),
        {**move(2100, SHADOW, mm.BOMB_BOT_STEP, 3.0, 10.0), "ok": 0},
        move(2500, TREE, mm.FLAMES_DODGE, 0.0, 30.0),
        {"t": 2000, "e": "note", "g": SHADOW, "k": "mimiron.flee",
         "txt": "bombbot ok +0 (back0 mine0 cone0 fire0 bomb0 rocket0 burst0 shock0 spray0 move0 hold0 bombbot0)"},
        {"t": 2400, "e": "note", "g": TREE, "k": "mimiron.flee",
         "txt": "flames+3 ok +22 (back0 mine0 cone0 fire1 bomb0 rocket0 burst0 shock0 spray0 move0 hold0 bombbot2)"},
        {"t": 5800, "e": "note", "g": BULWARK, "k": "mimiron.flee",
         "txt": "bombbot kite none (back0 mine0 cone0 fire0 bomb0 rocket0 burst0 shock0 spray0 move0 hold0 bombbot0)"},

        {"t": 6000, "e": "dmg", "s": CHASER, "d": AGONY, "sp": mm.SPELL_BOMB_BOT_BLAST, "a": 15000},
        {"t": 6000, "e": "dmg", "s": CHASER, "d": TREE, "sp": mm.SPELL_BOMB_BOT_BLAST, "a": 24000},
        {"t": 6000, "e": "death", "g": TREE, "killer": CHASER, "blow": [CHASER, 24000], "x": 0.0, "y": 22.0},
        {"t": 8000, "e": "end", "out": "wipe"},
    ]
    records += [bombbot_snap(when) for when in range(0, 8000, 250)]
    return records


class BombBotPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "603_9_mimiron_1789700007.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in bombbot_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)
        cls.rows = mm.bombbot_rows(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_each_bomb_lives_until_it_was_last_seen(self):
        lives = self.rows["lives"]
        self.assertEqual((lives[CHASER]["first"], lives[CHASER]["last"]), (0, 5750))
        self.assertEqual((lives[SPENT]["last"], lives[SPENT].get("victims")), (2750, None))

    def test_the_bombs_target_is_chased_and_the_rest_collateral(self):
        victims = self.rows["victims"]
        self.assertEqual((victims["chased"], victims["chased dead"]), (1, 0))
        self.assertEqual((victims["collateral"], victims["collateral dead"]), (1, 1))
        self.assertEqual(self.rows["lives"][CHASER]["chased"], AGONY)

    def test_only_accepted_step_outs_count_with_their_length(self):
        self.assertEqual(self.rows["legs"], [9.0])
        self.assertEqual(self.rows["steps"], {"bystander": 1})

    def test_step_and_kite_outcomes_and_the_other_dodges_blast_refusals(self):
        self.assertEqual(self.rows["outcomes"], {"step ok": 1, "kite none": 1})
        self.assertEqual(self.rows["screened"], 2)

    def test_the_view_prints_victims_and_legs(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            mm.show_bombbots(self.trace)
        self.assertIn("collateral 1 (1 dead)", out.getvalue())
        self.assertIn("median 9.0 yd", out.getvalue())


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
