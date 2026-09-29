"""Tests for the Northrend Beasts reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull pins the arithmetic behind each section, and the shared fixture shows every section
reads empty on a trace that is not the Beasts rather than raising.
"""
from __future__ import annotations

import collections
import contextlib
import io
import json
import pathlib
import sys
import tempfile
import unittest

BOTOBS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BOTOBS))

from bosses import northrend_beasts as nb  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v12.ndjson"

GORMOK = 4294968001
ICEHOWL = 4294968004
SNOBOLD = 4294968010
HOLD = 5001    # tank, Gormok first, then Icehowl
SWAP = 5002    # tank, takes Gormok at 10 s
MENDER = 5003  # heal, carries the snobold
ARROW = 5004   # ranged, inside the first charge's end and dodging
BLADE = 5005   # melee, trampled by the second charge and killed

# Every position is a fixed spot except the two bots standing at a charge's end.
SPOTS = {HOLD: (0.0, 0.0), SWAP: (3.0, 0.0), MENDER: (-20.0, -20.0), ARROW: (-25.0, 0.0), BLADE: (2.0, 2.0)}


def position(guid: int, when: int) -> tuple[float, float]:
    if guid == ARROW and 50000 <= when <= 52000:
        return 0.0, 45.0   # 5 yd off the first lane's end
    if guid == BLADE and 70000 <= when <= 72000:
        return 48.0, 0.0   # 2 yd off the second lane's end
    return SPOTS[guid]


def snap(when: int) -> dict:
    units = []
    for guid in SPOTS:
        x, y = position(guid, when)
        dead = guid == BLADE and when >= 72000
        units.append([guid, x, y, 0.0, 0.0, 0.0 if dead else 100.0, 100.0, 0, 0, 0, 0, 0])
    if when < 20000:
        units.append([GORMOK, 1.0, 1.0, 0.0, 0.0, 100.0 - when / 400.0, 0.0, HOLD if when < 10000 else SWAP,
                      0, 0, 0, 0])
    if when >= 40000:
        # passive through each charge, gazing at its target in the middle of it
        target = HOLD
        if 45000 <= when < 51000 or 65000 <= when < 71000:
            target = ARROW if 47000 <= when < 49000 else (BLADE if 67000 <= when < 69000 else 0)
        units.append([ICEHOWL, 1.0, 1.0, 0.0, 0.0, 100.0, 0.0, target, 0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units}


def note(when: int, guid: int, key: str, text: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": text}


def aura(when: int, target: int, spell: int, stacks: int = 1, removed: bool = False) -> dict:
    return {"t": when, "e": "aura", "d": target, "s": GORMOK, "sp": spell, "r": 1 if removed else 0, "st": stacks}


def beasts_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 12, "ts": 1789500000000, "map": 649, "inst": 7, "diff": 0,
         "boss": "gormok-the-impaler", "roster": [
             {"g": HOLD, "n": "Hold", "r": "tank", "c": "warrior", "h": 0},
             {"g": SWAP, "n": "Swap", "r": "tank", "c": "paladin", "h": 0},
             {"g": MENDER, "n": "Mender", "r": "heal", "c": "priest", "h": 0},
             {"g": ARROW, "n": "Arrow", "r": "ranged", "c": "hunter", "h": 0},
             {"g": BLADE, "n": "Blade", "r": "melee", "c": "rogue", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "gormok-the-impaler", "src": "engage"},
        {"t": 1, "e": "unit", "g": GORMOK, "en": nb.NPC_GORMOK, "n": "Gormok the Impaler", "b": 1},
        {"t": 1, "e": "unit", "g": ICEHOWL, "en": nb.NPC_ICEHOWL, "n": "Icehowl", "b": 1},
        {"t": 1, "e": "unit", "g": SNOBOLD, "en": 34800, "n": "Snobold Vassal"},

        note(0, HOLD, "nb.stage", "1"),
        note(20000, SWAP, "nb.stage", "0"),
        note(40000, HOLD, "nb.stage", "4"),

        # Gormok: Hold to 3 stacks under two ids, Swap waits for 2 then goes at 3.
        note(100, HOLD, "nb.tank", f"gormok {GORMOK}"),
        note(100, SWAP, "nb.tank", f"swap {GORMOK}"),
        aura(2000, HOLD, 66331, 1),
        aura(5000, HOLD, 66331, 2),
        aura(8000, HOLD, 67477, 3),
        note(5000, SWAP, "nb.swap", "wait v=2"),
        note(8100, SWAP, "nb.swap", "go v=3"),
        note(9000, HOLD, "nb.defensive", "shield wall"),
        {"t": 9900, "e": "act", "g": SWAP, "a": nb.SWAP_TAUNT, "rel": 105.0, "vd": "OK"},
        note(10000, HOLD, "nb.tank", f"swap {GORMOK}"),
        note(10000, SWAP, "nb.tank", f"gormok {GORMOK}"),
        aura(12000, SWAP, 66331, 1),
        aura(38000, HOLD, 67477, 0, removed=True),

        # Mender carries the snobold for 4 s; both DPS go for it, Arrow lets go once it's dead.
        aura(3000, MENDER, nb.SPELL_SNOBOLLED),
        aura(7000, MENDER, nb.SPELL_SNOBOLLED, removed=True),
        note(3100, ARROW, "nb.snobold", f"{MENDER} heal"),
        note(3100, BLADE, "nb.snobold", f"{MENDER} heal"),
        note(7100, ARROW, "nb.snobold", "none"),

        # Icehowl on Hold, Swap without a duty.
        note(40000, HOLD, "nb.tank", f"icehowl {ICEHOWL}"),
        note(40000, SWAP, "nb.tank", "none"),

        # A clean charge at Arrow, who dodges but is still standing at the end.
        note(45000, HOLD, "nb.charge", "1"),
        note(47000, HOLD, "nb.charge", "2"),
        note(47000, HOLD, "nb.gaze", str(ARROW)),
        # gaze's line, replaced by the frozen one at the jump back
        {"t": 47000, "e": "haz", "sp": nb.SPELL_TRAMPLE, "shape": "lane", "x": -2.0, "y": -35.0, "z": 0.0,
         "ttl": 4000, "ex": 3.0, "ey": 49.9, "half": 12},
        note(49000, HOLD, "nb.charge", "3"),
        {"t": 49000, "e": "haz", "sp": nb.SPELL_TRAMPLE, "shape": "lane", "x": 0.0, "y": -35.0, "z": 0.0,
         "ttl": 4000, "ex": 0.0, "ey": 50.0, "half": 12},
        note(49100, ARROW, "nb.dodge", "move 14"),
        note(49100, HOLD, "nb.dodge", "hold"),
        note(50000, ARROW, "nb.dodge", "clear"),
        note(51000, HOLD, "nb.charge", "4"),
        note(51000, HOLD, "nb.gaze", "0"),
        note(63000, HOLD, "nb.charge", "0"),

        # A missed one: Blade is trampled at the end and dies to the enraged Icehowl.
        note(65000, HOLD, "nb.charge", "1"),
        note(67000, HOLD, "nb.charge", "2"),
        note(67000, HOLD, "nb.gaze", str(BLADE)),
        note(69000, HOLD, "nb.charge", "3"),
        {"t": 69000, "e": "haz", "sp": nb.SPELL_TRAMPLE, "shape": "lane", "x": -35.0, "y": 0.0, "z": 0.0,
         "ttl": 4000, "ex": 50.0, "ey": 0.0, "half": 12},
        note(71000, HOLD, "nb.charge", "5"),
        note(71000, HOLD, "nb.gaze", "0"),
        {"t": 71000, "e": "dmg", "s": ICEHOWL, "d": BLADE, "sp": nb.SPELL_TRAMPLE, "a": 50000},
        {"t": 72000, "e": "death", "g": BLADE, "killer": ICEHOWL, "x": 48.0, "y": 0.0, "z": 0.0},
        {"t": 80000, "e": "end", "out": "wipe"},
    ]
    records += [snap(when) for when in range(0, 81000, 1000)]
    return records


class SyntheticPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "649_7_gormok-the-impaler_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in beasts_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_stage_spans_carry_their_deaths(self):
        spans = nb.stage_spans(self.trace)
        self.assertEqual([(span["value"], span["start"], span["stop"]) for span in spans],
                         [("1", 0, 20000), ("0", 20000, 40000), ("4", 40000, 80000)])
        self.assertEqual([span["deaths"] for span in spans], [[], [], [BLADE]])

    def test_a_stage_mask_names_every_beast_in_it(self):
        self.assertEqual(nb.stage_name("3"), "gormok+worms")
        self.assertEqual(nb.stage_name("0"), "none")

    def test_a_beast_counts_as_engaged_from_its_first_target(self):
        self.assertEqual(nb.first_engaged(self.trace), {nb.NPC_GORMOK: 0, nb.NPC_ICEHOWL: 40000})

    def test_the_swap_reads_both_holders_stacks_across_impale_ids(self):
        self.assertEqual(nb.gormok_victims(self.trace),
                         [{"t": 10000, "old": HOLD, "new": SWAP, "old_stacks": 3, "new_stacks": 0}])

    def test_peak_stacks_per_tank(self):
        self.assertEqual(nb.peak_stacks(self.trace), {HOLD: 3, SWAP: 1})

    def test_swap_branches_drop_the_counts(self):
        self.assertEqual(nb.swap_branches(self.trace), {SWAP: collections.Counter({"wait v": 1, "go v": 1})})

    def test_swap_taunts_and_defensives(self):
        self.assertEqual(nb.swap_taunts(self.trace), {SWAP: collections.Counter({"OK": 1})})
        self.assertEqual(nb.defensive_picks(self.trace), {HOLD: collections.Counter({"shield wall": 1})})

    def test_duty_value_splits_into_duty_and_beast(self):
        self.assertEqual(nb.split_duty(f"gormok {GORMOK}"), ("gormok", GORMOK))
        self.assertEqual(nb.split_duty("none"), ("none", None))

    def test_one_cycle_per_charge_with_its_outcome(self):
        rows = nb.charges(self.trace)
        self.assertEqual([(row["start"], row["stop"], row["outcome"]) for row in rows],
                         [(45000, 51000, "daze"), (65000, 71000, "rage")])
        self.assertEqual([row["gaze"] for row in rows], [ARROW, BLADE])
        self.assertEqual([(row["lane"]["ex"], row["lane"]["ey"]) for row in rows], [(0.0, 50.0), (50.0, 0.0)])

    def test_who_stood_at_the_lane_end_at_the_outcome(self):
        rows = nb.charges(self.trace)
        self.assertEqual(rows[0]["inside"], {ARROW})
        self.assertEqual(rows[1]["inside"], {BLADE})

    def test_dodge_branches_and_trample_victims_per_cycle(self):
        rows = nb.charges(self.trace)
        self.assertEqual(rows[0]["dodges"], {ARROW: collections.Counter({"move": 1, "clear": 1}),
                                             HOLD: collections.Counter({"hold": 1})})
        self.assertEqual(rows[0]["trampled"], [])
        self.assertEqual(rows[1]["trampled"], [BLADE])

    def test_rider_spans_and_seconds(self):
        self.assertEqual(nb.rider_spans(self.trace), {MENDER: [(3000, 7000)]})

    def test_snobold_picks_by_rider_role(self):
        self.assertEqual(nb.snobold_picks(self.trace),
                         {ARROW: collections.Counter({"heal": 1, "none": 1}),
                          BLADE: collections.Counter({"heal": 1})})

    def test_every_section_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            nb.show_banner(self.trace)
            for _, _, section in nb.SECTIONS:
                section(self.trace)
        self.assertNotIn("not a Beasts pull", out.getvalue())


BOLT = 5006    # ranged, in the breath and bomb pull only

# Gormok's stage: two bombs, one on Arrow (who dodges) with Mender 4 yd off, one on Bolt.
BOMB_SPOTS = {HOLD: (0.0, 10.0), SWAP: (2.0, 10.0), MENDER: (24.0, -20.0), ARROW: (20.0, -20.0),
              BLADE: (1.0, 8.0), BOLT: (-20.0, 20.0)}
# Icehowl at the origin facing Hold, so his back is -90°: Arrow and Blade share that bearing, Bolt
# sits one 18° step round, Mender three.
BREATH_SPOTS = {HOLD: (0.0, 10.0), SWAP: (2.0, 10.0), MENDER: (13.753, -9.992), ARROW: (0.0, -22.0),
                BLADE: (0.0, -10.0), BOLT: (6.798, -20.923)}
BREATH_ID = 67650  # 25N


def breath_bomb_position(guid: int, when: int) -> tuple[float, float]:
    if when >= 15000:
        return BREATH_SPOTS[guid]
    if guid == ARROW and when >= 5500:
        return 20.0, -32.0
    return BOMB_SPOTS[guid]


def breath_bomb_snap(when: int) -> dict:
    units = [[guid, *breath_bomb_position(guid, when), 0.0, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0]
             for guid in BOMB_SPOTS]
    if when >= 15000:
        units.append([ICEHOWL, 0.0, 0.0, 0.0, 0.0, 100.0, 0.0, HOLD, 0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units}


def breath(when: int, target: int) -> dict:
    return {"t": when, "e": "cast", "s": ICEHOWL, "sp": BREATH_ID, "tgt": target, "ct": 0}


def frozen(when: int, target: int) -> dict:
    return {"t": when, "e": "aura", "d": target, "s": ICEHOWL, "sp": BREATH_ID, "r": 0, "st": 1}


def bomb(when: int, spot: tuple[float, float], ttl: int) -> dict:
    return {"t": when, "e": "haz", "sp": nb.SPELL_FIRE_BOMB_IMPACT, "shape": "circle", "x": spot[0], "y": spot[1],
            "z": 0.0, "ttl": ttl, "rad": 8.0}


def bomb_hit(when: int, target: int) -> dict:
    return {"t": when, "e": "dmg", "s": SNOBOLD, "d": target, "sp": nb.SPELL_FIRE_BOMB_IMPACT, "a": 5500}


def breath_bomb_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 12, "ts": 1789500000000, "map": 649, "inst": 8, "diff": 1,
         "boss": "gormok-the-impaler", "roster": [
             {"g": HOLD, "n": "Hold", "r": "tank", "c": "warrior", "h": 0},
             {"g": SWAP, "n": "Swap", "r": "tank", "c": "paladin", "h": 0},
             {"g": MENDER, "n": "Mender", "r": "heal", "c": "priest", "h": 0},
             {"g": ARROW, "n": "Arrow", "r": "ranged", "c": "hunter", "h": 0},
             {"g": BLADE, "n": "Blade", "r": "melee", "c": "rogue", "h": 0},
             {"g": BOLT, "n": "Bolt", "r": "ranged", "c": "mage", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "gormok-the-impaler", "src": "engage"},
        {"t": 1, "e": "unit", "g": ICEHOWL, "en": nb.NPC_ICEHOWL, "n": "Icehowl", "b": 1},
        {"t": 1, "e": "unit", "g": SNOBOLD, "en": 34800, "n": "Snobold Vassal"},

        # Both bombs' windows hold both hits, so each hit goes by where its victim stood.
        bomb(5000, BOMB_SPOTS[ARROW], 2000),
        note(5100, ARROW, "nb.bomb", "move 12"),
        note(5100, MENDER, "nb.bomb", "pinned"),
        note(5300, ARROW, "nb.bomb", "hold"),
        note(5900, ARROW, "nb.bomb", "clear"),
        bomb(6000, BOMB_SPOTS[BOLT], 1800),
        bomb_hit(7000, MENDER),
        bomb_hit(7800, BOLT),

        # Mender pinned again on a third bomb: the clear in between is what writes the second row.
        note(8000, MENDER, "nb.bomb", "clear"),
        bomb(10000, BOMB_SPOTS[MENDER], 1500),
        note(10100, MENDER, "nb.bomb", "pinned"),
        bomb_hit(11500, MENDER),

        note(15000, MENDER, "nb.spread", "3"),
        note(15000, ARROW, "nb.spread", "0"),
        note(15000, BLADE, "nb.spread", "none"),
        note(15000, BOLT, "nb.spread", "1"),
        note(15000, MENDER, "nb.arc", "11"),
        note(16000, BLADE, "nb.spread", "0"),

        # On Mender, alone on her bearing.
        breath(20000, MENDER),
        frozen(20000, MENDER),
        {"t": 25000, "e": "aura", "d": MENDER, "s": ICEHOWL, "sp": BREATH_ID, "r": 1, "st": 0},

        # A charge drops the layout, and two bearings are walled on the way back.
        note(30000, ARROW, "nb.arc", "0"),
        note(40000, BOLT, "nb.arc", "9"),

        # On Arrow, with Blade inside on the same bearing.
        breath(45000, ARROW),
        frozen(45000, ARROW),
        frozen(45100, BLADE),
        note(50000, ARROW, "nb.spread", "1"),
        {"t": 60000, "e": "end", "out": "wipe"},
    ]
    records += [breath_bomb_snap(when) for when in range(0, 60500, 500)]
    return records


class BreathAndBombPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "649_8_gormok-the-impaler_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in breath_bomb_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_half_width_is_wider_on_10n_only(self):
        self.assertEqual([nb.breath_half_width(diff) for diff in range(4)], [30.0, 12.0, 12.0, 12.0])

    def test_one_row_per_breath_with_its_target_and_victims(self):
        rows = nb.breaths(self.trace)
        self.assertEqual([(row["t"], row["target"], row["victims"]) for row in rows],
                         [(20000, MENDER, [MENDER]), (45000, ARROW, [ARROW, BLADE])])

    def test_the_cone_predicts_only_the_co_bearing_neighbour(self):
        self.assertEqual([row["predicted"] for row in nb.breaths(self.trace)], [{MENDER}, {ARROW, BLADE}])

    def test_spread_and_arc_are_read_as_of_the_cast(self):
        rows = nb.breaths(self.trace)
        self.assertEqual([row["spread"] for row in rows], [{MENDER: "3"}, {ARROW: "0", BLADE: "0"}])
        self.assertEqual([row["arc"] for row in rows], ["11", "9"])

    def test_times_frozen_per_bot(self):
        self.assertEqual(nb.times_frozen(nb.breaths(self.trace)),
                         collections.Counter({MENDER: 1, ARROW: 1, BLADE: 1}))

    def test_one_row_per_bomb_with_its_target(self):
        rows = nb.bombs(self.trace)
        self.assertEqual([(row["t"], row["ttl"], row["target"], row["gap"]) for row in rows],
                         [(5000, 2000, ARROW, 0.0), (6000, 1800, BOLT, 0.0), (10000, 1500, MENDER, 0.0)])

    def test_each_hit_goes_to_the_bomb_nearest_its_victim(self):
        self.assertEqual([row["hits"] for row in nb.bombs(self.trace)], [[MENDER], [BOLT], [MENDER]])

    def test_who_stood_inside_at_the_impact(self):
        self.assertEqual([row["inside"] for row in nb.bombs(self.trace)], [{MENDER}, {BOLT}, {MENDER}])

    def test_dodge_branches_up_to_the_impact(self):
        rows = nb.bombs(self.trace)
        self.assertEqual(rows[0]["dodges"], {ARROW: collections.Counter({"move": 1, "hold": 1, "clear": 1}),
                                             MENDER: collections.Counter({"pinned": 1})})
        self.assertEqual(rows[1]["dodges"], {})

    def test_a_repeated_branch_shows_on_the_next_bomb(self):
        self.assertEqual(nb.bombs(self.trace)[2]["dodges"], {MENDER: collections.Counter({"pinned": 1})})

    def test_breath_and_bomb_print_their_summaries(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            nb.show_breath(self.trace)
            nb.show_bomb(self.trace)
        text = out.getvalue()
        self.assertIn("2 breath(s), 1.5 frozen a breath, max 2", text)
        self.assertIn("3 bomb(s), 3 hit(s), 1.0 a bomb", text)
        self.assertNotIn("outside every bomb's window", text)


class EveryView(unittest.TestCase):
    def test_breath_and_bomb_read_empty_on_another_boss(self):
        trace = Trace(FULL)
        self.assertEqual(nb.breaths(trace), [])
        self.assertEqual(nb.bombs(trace), [])
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            nb.show_breath(trace)
            nb.show_bomb(trace)
        self.assertIn("no Arctic Breath cast", out.getvalue())
        self.assertIn("no Fire Bomb circle", out.getvalue())

    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            nb.show_banner(trace)
            for _, _, section in nb.SECTIONS:
                section(trace)
        text = out.getvalue()
        self.assertIn("not a Beasts pull", text)
        for empty in ("no nb.stage rows", "no nb.tank rows", "no nb.charge cycle", "nobody carried a snobold"):
            self.assertIn(empty, text)


DREADSCALE = 4294968020
ACIDMAW = 4294968021
POOL = 4294968030
WARD = 5101    # tank, Dreadscale then the mobile Acidmaw, keeps Toxin 60 s
BRACE = 5102   # tank on the stationary worm
SOOTHE = 5103  # heal, Acidmaw's first target after the emerge, carries Bile, stands in the pool
SHOT = 5104    # ranged, inside the Spew wedge, cured by a pulse
EDGE = 5105    # melee, eats a pulse with no Toxin and a Sweep, dies at 105 s

WORM_SPOTS = {WARD: (18.0, 0.0), BRACE: (-5.0, 28.0), SOOTHE: (-20.0, -20.0), SHOT: (40.0, 2.0), EDGE: (4.0, 0.0)}
# 4.9 yd from Soothe, so she is inside from the pool's 10th second
POOL_SPOT = (-20.0, -15.1)


def worm_snap(when: int) -> dict:
    units = []
    for guid, (x, y) in WORM_SPOTS.items():
        dead = guid == EDGE and when >= 105000
        units.append([guid, x, y, 0.0, 0.0, 0.0 if dead else 100.0, 100.0, 0, 0, 0, 0, 0])
    acidmaw_under = 40000 <= when < 48000
    dreadscale_under = 40000 <= when < 50000
    acidmaw_target = 0 if acidmaw_under else BRACE if when < 40000 else SOOTHE if when < 52000 else WARD
    dreadscale_target = 0 if dreadscale_under else WARD if when < 40000 else BRACE
    acidmaw_cast = nb.SPELL_EMERGE if 48000 <= when < 51000 else 0
    dreadscale_cast = nb.SPELL_EMERGE if 50000 <= when < 53000 else 0
    units.append([ACIDMAW, 10.0, 0.0, 0.0, 0.0, 100.0, 0.0, acidmaw_target, 0, 0, acidmaw_cast, 0])
    units.append([DREADSCALE, -5.0, 20.0, 0.0, 0.0, 100.0, 0.0, dreadscale_target, 0, 0, dreadscale_cast, 0])
    if 70000 <= when < 100000:
        units.append([POOL, *POOL_SPOT, 0.0, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units}


def worm_dmg(when: int, source: int, target: int, spell: int) -> dict:
    return {"t": when, "e": "dmg", "s": source, "d": target, "sp": spell, "a": 3000}


def worms_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 12, "ts": 1789600000000, "map": 649, "inst": 8, "diff": 0,
         "boss": "gormok-the-impaler", "roster": [
             {"g": WARD, "n": "Ward", "r": "tank", "c": "warrior", "h": 0},
             {"g": BRACE, "n": "Brace", "r": "tank", "c": "paladin", "h": 0},
             {"g": SOOTHE, "n": "Soothe", "r": "heal", "c": "priest", "h": 0},
             {"g": SHOT, "n": "Shot", "r": "ranged", "c": "hunter", "h": 0},
             {"g": EDGE, "n": "Edge", "r": "melee", "c": "rogue", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "gormok-the-impaler", "src": "engage"},
        {"t": 1, "e": "unit", "g": ACIDMAW, "en": nb.NPC_ACIDMAW, "n": "Acidmaw", "b": 1},
        {"t": 1, "e": "unit", "g": DREADSCALE, "en": nb.NPC_DREADSCALE, "n": "Dreadscale", "b": 1},
        {"t": 1, "e": "unit", "g": POOL, "en": nb.NPC_SLIME_POOL, "n": "Slime Pool"},

        # No worm engaged for 5 s, then Dreadscale mobile, both under at 40 s. Acidmaw comes up mobile
        # at 48 s and Dreadscale at 50 s, when nb.worm leaves 3.
        note(0, WARD, "nb.worm", "0"),
        note(5000, WARD, "nb.worm", "1"),
        note(40000, WARD, "nb.worm", "3"),
        note(50000, WARD, "nb.worm", "2"),

        # Soothe's Bile cures Shot's Toxin at 26 s and lands on Edge, who carries nothing, at 28 s.
        aura(10000, SOOTHE, nb.SPELL_BURNING_BILE),
        aura(20000, SHOT, 66823),
        note(20100, SHOT, "nb.cure", f"seek {SOOTHE}"),
        note(20100, SOOTHE, "nb.cure", f"run {SHOT}"),
        worm_dmg(26000, SOOTHE, SHOT, 66870),
        worm_dmg(26000, SOOTHE, SOOTHE, 66870),
        aura(26000, SHOT, 66823, removed=True),
        note(26100, SHOT, "nb.cure", "none"),
        note(26100, SOOTHE, "nb.cure", "none"),
        worm_dmg(28000, SOOTHE, EDGE, 66870),
        aura(34000, SOOTHE, nb.SPELL_BURNING_BILE, removed=True),

        worm_dmg(30000, ACIDMAW, EDGE, 66794),
        worm_dmg(30000, ACIDMAW, BRACE, 66794),
        note(45000, WARD, "nb.wormmove", "approach 20"),

        # Ward keeps Toxin its full 60 s; Edge dies with it on.
        aura(55000, WARD, 66823),
        note(70000, WARD, "nb.cure", "wait"),
        aura(100000, EDGE, 66823),
        {"t": 105000, "e": "death", "g": EDGE, "killer": DREADSCALE, "x": 4.0, "y": 0.0, "z": 0.0},
        aura(105000, EDGE, 66823, removed=True),
        aura(115000, WARD, 66823, removed=True),

        # A 24 degree Spew from Acidmaw facing +x: Shot inside and ticked, Edge's tick past the 3.5 s.
        note(59000, SHOT, "nb.wormmove", "move 12.5 spew"),
        note(59500, SHOT, "nb.wormmove", "hold spew"),
        {"t": 60000, "e": "haz", "sp": 66819, "shape": "wedge", "x": 10.0, "y": 0.0, "z": 0.0, "ttl": 3500,
         "facing": 0.0, "arc": 12.0, "range": 55.0},
        note(60500, SHOT, "nb.wormmove", "clear"),
        worm_dmg(61000, ACIDMAW, SHOT, 66819),
        worm_dmg(64000, ACIDMAW, EDGE, 66819),

        note(71000, WARD, "nb.wormmove", "drag 14"),
        worm_dmg(81000, POOL, SOOTHE, 66881),
        worm_dmg(82000, POOL, SOOTHE, 66881),
        note(90000, SHOT, "nb.wormmove", "none pool"),
        {"t": 120000, "e": "end", "out": "wipe"},
    ]
    records += [worm_snap(when) for when in range(0, 121000, 1000)]
    return records


class WormPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "649_8_gormok-the-impaler_1789600000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in worms_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_worm_spans_carry_their_deaths(self):
        spans = nb.worm_spans(self.trace)
        self.assertEqual([(span["value"], span["start"], span["stop"]) for span in spans],
                         [("0", 0, 5000), ("1", 5000, 40000), ("3", 40000, 50000), ("2", 50000, 120000)])
        self.assertEqual([span["deaths"] for span in spans], [[], [], [], [EDGE]])

    def test_emerges_are_the_engage_and_every_exit_from_under_ground(self):
        self.assertEqual(nb.emerges(self.trace), [(5000, 40000), (50000, 120000)])

    def test_emerge_times_the_pickup_and_names_the_non_tank_first(self):
        rows = nb.worm_pickups(self.trace)
        self.assertEqual([row["t"] for row in rows], [5000, 50000])
        self.assertEqual(rows[0]["worms"], {ACIDMAW: {"tank_ms": 0, "tank": BRACE, "victims": []},
                                            DREADSCALE: {"tank_ms": 0, "tank": WARD, "victims": []}})
        # Acidmaw is timed from its own Emerge at 48 s, not from nb.worm leaving 3 at 50 s
        self.assertEqual(rows[1]["worms"], {ACIDMAW: {"tank_ms": 4000, "tank": WARD, "victims": [SOOTHE]},
                                            DREADSCALE: {"tank_ms": 0, "tank": BRACE, "victims": []}})

    def test_wedge_names_the_non_tanks_inside_and_its_tick_victims(self):
        rows = nb.spew_wedges(self.trace)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["worm"], "Acidmaw")
        self.assertEqual(rows[0]["inside"], {SHOT})
        self.assertEqual(rows[0]["hit"], {"ranged": collections.Counter({SHOT: 1})})

    def test_a_growing_pool_counts_who_stood_inside_and_its_hits(self):
        pools = nb.slime_pools(self.trace)
        self.assertEqual([(pool["guid"], pool["first"], pool["last"]) for pool in pools], [(POOL, 70000, 99000)])
        self.assertEqual(pools[0]["inside"], collections.Counter({SOOTHE: 20000}))
        self.assertEqual(pools[0]["hits"], collections.Counter({SOOTHE: 2}))
        self.assertEqual(nb.pool_radius(0), 2.0)
        self.assertEqual(nb.pool_radius(45000), 11.0)

    def test_sweep_victims_by_role(self):
        self.assertEqual(nb.sweep_victims(self.trace), {"melee": collections.Counter({EDGE: 1}),
                                                        "tank": collections.Counter({BRACE: 1})})

    def test_wormmove_branches_drop_the_yards(self):
        self.assertEqual(nb.wormmove_branches(self.trace), {
            WARD: collections.Counter({"approach": 1, "drag": 1}),
            SHOT: collections.Counter({"move spew": 1, "hold spew": 1, "clear": 1, "none pool": 1})})

    def test_toxin_spans_end_cured_expired_or_died(self):
        rows = nb.toxin_spans(self.trace)
        self.assertEqual([(row["carrier"], row["start"], row["stop"], row["end"], row["stuck"]) for row in rows],
                         [(SHOT, 20000, 26000, "cured", False), (WARD, 55000, 115000, "expired", True),
                          (EDGE, 100000, 105000, "died", False)])

    def test_heroic_toxin_stops_a_carrier_sooner(self):
        self.assertEqual(nb.toxin_stuck_ms(0), 18000)
        self.assertEqual(nb.toxin_stuck_ms(2), 13500)
        self.assertEqual(nb.toxin_stuck_ms(3), 13500)

    def test_bile_pulses_on_a_bot_with_no_toxin(self):
        rows = nb.bile_spans(self.trace)
        self.assertEqual([(row["carrier"], row["start"], row["stop"]) for row in rows], [(SOOTHE, 10000, 34000)])
        self.assertEqual(rows[0]["clean"], collections.Counter({EDGE: 1}))

    def test_cure_branches_drop_the_guid(self):
        self.assertEqual(nb.cure_branches(self.trace), {
            SHOT: collections.Counter({"seek": 1, "none": 1}),
            SOOTHE: collections.Counter({"run": 1, "none": 1}),
            WARD: collections.Counter({"wait": 1})})

    def test_every_section_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            nb.show_banner(self.trace)
            for _, _, section in nb.SECTIONS:
                section(self.trace)
        self.assertNotIn("not a Beasts pull", out.getvalue())

    def test_worm_sections_read_empty_on_another_boss(self):
        trace = Trace(FULL)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            nb.show_worms(trace)
            nb.show_cure(trace)
        text = out.getvalue()
        for empty in ("no nb.worm rows", "emerges, seconds until each worm targeted a tank: none",
                      "Spew wedges: none", "slime pools: none sampled", "Sweep hits: nobody",
                      "nb.wormmove: none", "no Paralytic Toxin carried", "no Burning Bile carried",
                      "nb.cure: none"):
            self.assertIn(empty, text)


if __name__ == "__main__":
    unittest.main()
