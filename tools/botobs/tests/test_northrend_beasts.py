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


class EveryView(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
