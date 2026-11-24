"""Tests for the Vezax reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull pins the arithmetic behind each section, and the shared fixture shows every section
reads empty on a trace that is not Vezax rather than raising.
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

from bosses import general_vezax as gv  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v12.ndjson"

BOSS = 4294968202
VAPOR = 4294969154
AGONY = 5001  # ranged, block L
TREE = 5002   # heal, block R
BULWARK = 5003
FEL = 5004    # ranged, slotted in block R, never inside the gate
ANIMUS = 4294969200


def snap(when: int, agony_target: int, field: bool) -> dict:
    units = [
        [BOSS, 0.0, 0.0, 0.0, 0.0, 90.0, 0.0, BULWARK, 0, 0, 0, 0],
        [AGONY, 30.0, 0.0, 0.0, 0.0, 100.0, 80.0 - when / 1000.0, agony_target, 0, 0, 0, 0],
        [TREE, 0.0, 30.0, 0.0, 0.0, 100.0, 90.0, 0, 0, 0, 0, 0],
        [BULWARK, 1.0, 0.0, 0.0, 0.0, 100.0, 0.0, BOSS, 0, 0, 0, 0],
    ]
    # The Animus from 12 s: on Agony for two samples, then on Bulwark.
    if when >= 12000:
        units.append([ANIMUS, 1.0, 1.0, 0.0, 0.0, 100.0 - (when - 12000) / 1000.0, 0.0,
                      AGONY if when < 14000 else BULWARK, 0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units,
            "hz": [[gv.SPELL_FIELD, 30.0, 2.0, 0.0, 8.0, 1]] if field else []}


def vezax_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 12, "ts": 1789500000000, "map": 603, "inst": 4, "diff": 1,
         "boss": "general-vezax", "roster": [
             {"g": AGONY, "n": "Agony", "r": "ranged", "c": "warlock", "h": 0},
             {"g": TREE, "n": "Tree", "r": "heal", "c": "druid", "h": 0},
             {"g": BULWARK, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
             {"g": FEL, "n": "Fel", "r": "ranged", "c": "warlock", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "general-vezax", "src": "bossstate"},
        {"t": 1, "e": "unit", "g": BOSS, "en": gv.NPC_VEZAX, "n": "General Vezax", "b": 1},
        {"t": 1, "e": "unit", "g": VAPOR, "en": gv.NPC_SARONITE_VAPORS, "n": "Saronite Vapors"},
        {"t": 1, "e": "unit", "g": ANIMUS, "en": gv.NPC_SARONITE_ANIMUS, "n": "Saronite Animus"},
        {"t": 10, "e": "note", "g": AGONY, "k": "vezax.block", "txt": "L"},
        {"t": 10, "e": "note", "g": TREE, "k": "vezax.block", "txt": "R"},
        {"t": 10, "e": "note", "g": BULWARK, "k": "vezax.block", "txt": "tank"},
        {"t": 10, "e": "note", "g": AGONY, "k": "vezax.formation", "txt": "on"},
        {"t": 10, "e": "note", "g": TREE, "k": "vezax.formation", "txt": "on"},
        {"t": 4000, "e": "note", "g": TREE, "k": "vezax.formation", "txt": "kept"},
        {"t": 6000, "e": "note", "g": TREE, "k": "vezax.formation", "txt": "outside"},
        {"t": 8000, "e": "note", "g": TREE, "k": "vezax.formation", "txt": "on"},
        # Fel holds slot 12 and never gets a block row. The wipe reset erases it, written as "0".
        {"t": 10, "e": "note", "g": FEL, "k": "vezax.slot", "txt": "12"},
        {"t": 10, "e": "note", "g": FEL, "k": "vezax.formation", "txt": "outside"},
        {"t": 14900, "e": "note", "g": FEL, "k": "vezax.slot", "txt": "0"},

        # Tree carries the mark 1 s to 11 s. The 12.5 s tick is the late one that trails the aura.
        {"t": 1000, "e": "aura", "d": TREE, "s": BOSS, "sp": gv.SPELL_MARK_OF_THE_FACELESS, "r": 0},
        {"t": 11000, "e": "aura", "d": TREE, "s": BOSS, "sp": gv.SPELL_MARK_OF_THE_FACELESS, "r": 1},
        {"t": 3000, "e": "dmg", "s": BOSS, "d": AGONY, "sp": gv.SPELL_MARK_LEECH, "a": 5000},
        {"t": 12500, "e": "dmg", "s": BOSS, "d": AGONY, "sp": gv.SPELL_MARK_LEECH, "a": 4000},
        {"t": 14000, "e": "dmg", "s": BOSS, "d": AGONY, "sp": gv.SPELL_MARK_LEECH, "a": 3000},

        # One crash on Agony, and one bot from each block dodges it.
        # Three Searing Flames, only the middle one gets through.
        {"t": 2000, "e": "cast", "s": BOSS, "sp": gv.SPELL_SEARING_FLAMES, "tgt": 0, "ct": 2000},
        {"t": 6000, "e": "cast", "s": BOSS, "sp": gv.SPELL_SEARING_FLAMES, "tgt": 0, "ct": 2000},
        {"t": 8000, "e": "dmg", "s": BOSS, "d": AGONY, "sp": gv.SPELL_SEARING_FLAMES, "a": 14000},
        {"t": 8000, "e": "dmg", "s": BOSS, "d": TREE, "sp": gv.SPELL_SEARING_FLAMES, "a": 13000},
        {"t": 10000, "e": "cast", "s": BOSS, "sp": gv.SPELL_SEARING_FLAMES, "tgt": 0, "ct": 2000},

        {"t": 5000, "e": "haz", "sp": gv.SPELL_SHADOW_CRASH_IMPACT, "shape": "circle",
         "x": 30.0, "y": 0.0, "z": 0.0, "ttl": 3000, "rad": 10.0},
        {"t": 5500, "e": "move", "g": AGONY, "k": "point", "x": 45.0, "y": 0.0, "z": 0.0,
         "ok": 1, "r": "", "by": gv.DODGE},
        {"t": 5600, "e": "move", "g": TREE, "k": "point", "x": 0.0, "y": 45.0, "z": 0.0,
         "ok": 1, "r": "", "by": gv.DODGE},
        {"t": 6500, "e": "move", "g": TREE, "k": "point", "x": 22.0, "y": 5.0, "z": 0.0,
         "ok": 1, "r": "", "by": gv.REACH_SPELL},
        {"t": 7000, "e": "move", "g": AGONY, "k": "point", "x": 50.0, "y": 0.0, "z": 0.0,
         "ok": 1, "r": "", "by": gv.REACH_SPELL},
        {"t": 7100, "e": "move", "g": BULWARK, "k": "point", "x": 1.0, "y": -5.0, "z": 0.0,
         "ok": 1, "r": "", "by": "vezax raid position action"},

        {"t": 7000, "e": "aura", "d": AGONY, "s": BOSS, "sp": gv.SPELL_FIELD, "r": 0},
        {"t": 12000, "e": "aura", "d": AGONY, "s": BOSS, "sp": gv.SPELL_FIELD, "r": 1},
        {"t": 7200, "e": "aura", "d": AGONY, "s": BOSS, "sp": gv.SPELL_FIELD_COST, "r": 0},
        {"t": 11000, "e": "aura", "d": AGONY, "s": BOSS, "sp": gv.SPELL_FIELD_COST, "r": 1},
        {"t": 2000, "e": "cast", "s": AGONY, "sp": 47809, "tgt": BOSS, "ct": 0},
        {"t": 8000, "e": "cast", "s": AGONY, "sp": 47809, "tgt": BOSS, "ct": 0},
        {"t": 13000, "e": "cast", "s": AGONY, "sp": 47809, "tgt": BOSS, "ct": 0},
        {"t": 8500, "e": "cast", "s": AGONY, "sp": 47809, "tgt": BOSS, "ct": 0, "tr": 1},
        {"t": 9000, "e": "cast", "s": AGONY, "sp": gv.SPELL_LIFE_TAP, "tgt": AGONY, "ct": 0},

        # A Starfall star lands on the vapor, and hard mode goes from pending to lost.
        {"t": 9500, "e": "cast", "s": TREE, "sp": 53190, "tgt": VAPOR, "ct": 0, "tr": 1},
        {"t": 10, "e": "note", "g": AGONY, "k": "vezax.hardmode", "txt": "pending"},
        {"t": 9600, "e": "note", "g": AGONY, "k": "vezax.hardmode", "txt": "lost"},
        {"t": 9700, "e": "note", "g": TREE, "k": "vezax.hardmode", "txt": "lost"},
        # Agony and Tree hit the stop line, and the lost hard mode lets them go.
        {"t": 9000, "e": "note", "g": AGONY, "k": "vezax.hold", "txt": "on"},
        {"t": 9100, "e": "note", "g": TREE, "k": "vezax.hold", "txt": "on"},
        {"t": 9600, "e": "note", "g": AGONY, "k": "vezax.hold", "txt": "off"},

        # Tricks goes out half a second before the Animus, at a dps. Bulwark switches onto it at once,
        # Agony a second and a half later, and Heroism waits until 14.5 s.
        {"t": 11500, "e": "cast", "s": FEL, "sp": gv.SPELL_TRICKS, "tgt": AGONY, "ct": 0},
        {"t": 12200, "e": "act", "g": BULWARK, "a": gv.ANIMUS_SWITCH, "rel": 65.0, "vd": "OK"},
        {"t": 13500, "e": "act", "g": AGONY, "a": gv.ANIMUS_SWITCH, "rel": 65.0, "vd": "OK"},
        {"t": 13600, "e": "act", "g": AGONY, "a": gv.ANIMUS_SWITCH, "rel": 65.0, "vd": "OK"},
        {"t": 14500, "e": "cast", "s": BULWARK, "sp": 32182, "tgt": 0, "ct": 0},
        {"t": 13500, "e": "veto", "g": AGONY, "m": gv.HOLD, "a": "shadow bolt"},
        {"t": 15000, "e": "end", "out": "wipe"},
    ]
    # Agony holds the vapor on the first half of the samples only.
    records += [snap(when, VAPOR if when < 8000 else BOSS, when >= 6000) for when in range(0, 16000, 1000)]
    return records


class SyntheticPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "603_4_general-vezax_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in vezax_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_the_window_keeps_the_trailing_tick_and_drops_the_stray(self):
        windows = gv.mark_windows(self.trace)
        self.assertEqual(len(windows), 1)
        self.assertEqual(windows[0]["guid"], TREE)
        self.assertEqual(windows[0]["ticks"], 2)
        self.assertEqual(windows[0]["damage"], 9000)
        self.assertEqual(windows[0]["early"], 1)

    def test_far_counts_the_living_past_the_mark_floor_from_him(self):
        # Agony and Tree stand 30 yd out, past 29.1; Bulwark is on him and Fel never sampled.
        self.assertEqual(gv.mark_windows(self.trace)[0]["far"], 2)
        self.assertFalse(gv.mark_windows(self.trace)[0]["surge"])
        self.assertEqual(gv.far_pool_minimum(self.trace), 9)

    def test_a_mark_inside_surge_of_darkness_counts_far_past_the_grown_floor(self):
        records = vezax_pull() + [{"t": 500, "e": "cast", "s": BOSS, "sp": gv.SPELL_SURGE_OF_DARKNESS,
                                   "tgt": 0, "ct": 0}]
        path = pathlib.Path(self.folder.name) / "603_5_general-vezax_1789500001.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in records) + "\n", encoding="utf-8")
        window = gv.mark_windows(Trace(path))[0]
        self.assertTrue(window["surge"])
        self.assertEqual(window["far"], 0)

    def test_a_crash_names_its_target_and_both_blocks_that_moved(self):
        rows = gv.crashes(self.trace)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["target"], AGONY)
        self.assertEqual(rows[0]["block"], "L")
        self.assertEqual(rows[0]["dodged"], {"L": {AGONY}, "R": {TREE}})

    def test_a_crash_names_what_else_he_cast_while_it_flew(self):
        # The 6 s Searing Flames goes out inside the 5 s to 8 s flight; the 2 s and 10 s ones don't.
        self.assertEqual(gv.crashes(self.trace)[0]["masked"], [gv.SPELL_SEARING_FLAMES])

    def test_a_walk_in_ends_or_passes_near_the_impact_and_walking_away_is_not_one(self):
        walk_ins = gv.crashes(self.trace)[0]["walk_ins"]
        self.assertEqual([(rec["g"], rec["by"]) for rec in walk_ins], [(TREE, gv.REACH_SPELL)])
        self.assertTrue(gv.walk_meets_impact((30.0, 0.0), (15.0, 5.0), (45.0, 5.0)))
        self.assertFalse(gv.walk_meets_impact((30.0, 0.0), (15.0, 15.0), (45.0, 15.0)))
        self.assertFalse(gv.walk_meets_impact((30.0, 0.0), (25.0, 5.0), (15.0, 15.0)))

    def test_a_late_dodge_leaves_the_blast_only_after_the_landing(self):
        self.assertEqual(gv.crashes(self.trace)[0]["late"], [])
        self.assertAlmostEqual(gv.blast_exit_walk((21.0, 0.0), (45.0, 0.0), (30.0, 0.0)), 19.0)
        self.assertAlmostEqual(gv.blast_exit_walk((21.0, 0.0), (5.0, 0.0), (30.0, 0.0)), 1.0)
        self.assertEqual(gv.blast_exit_walk((45.0, 0.0), (60.0, 0.0), (30.0, 0.0)), 0.0)
        self.assertEqual(gv.blast_exit_walk((21.0, 0.0), (35.0, 0.0), (30.0, 0.0)), float("inf"))

    def test_a_star_on_a_vapor_is_counted_though_nobody_targeted_it(self):
        self.assertEqual(gv.vapor_hits(self.trace), {(TREE, 53190): [9500]})

    def test_hard_mode_reads_the_first_bot_to_see_each_value(self):
        self.assertEqual(gv.hard_mode_changes(self.trace), {"pending": 10, "lost": 9600})

    def test_the_hold_reads_the_first_bot_at_each_line(self):
        self.assertEqual(gv.hold_changes(self.trace), {"on": 9000, "off": 9600})

    def test_the_animus_victims_are_its_changes_not_its_samples(self):
        self.assertEqual(gv.animus_victims(self.trace), [(12000, AGONY), (14000, BULWARK)])

    def test_a_redirect_before_the_spawn_reads_negative(self):
        self.assertEqual(gv.animus_redirects(self.trace, 12000),
                         [{"t": 11500, "rel": -500, "caster": FEL, "spell": gv.SPELL_TRICKS,
                           "target": AGONY}])

    def test_each_bot_switches_once(self):
        self.assertEqual(gv.animus_switches(self.trace, 12000), {BULWARK: 200, AGONY: 1500})

    def test_in_field_share_skips_triggered_casts(self):
        agony = next(row for row in gv.field_rows(self.trace) if row["guid"] == AGONY)
        self.assertEqual(agony["casts"], 4)  # three bolts and the tap, not the triggered one
        self.assertEqual(agony["in_cost"], 2)  # the 8 s bolt and the 9 s tap
        self.assertEqual(agony["field"], 5000)
        self.assertEqual(agony["cost"], 3800)

    def test_vapor_share_reads_the_target_column(self):
        self.assertAlmostEqual(gv.vapor_share(self.trace)[AGONY], 8 / 16)
        self.assertEqual(gv.vapor_share(self.trace)[TREE], 0.0)

    def test_a_tap_that_returns_nothing_is_not_a_rise(self):
        agony = next(row for row in gv.mana_rows(self.trace) if row["guid"] == AGONY)
        self.assertEqual(agony["taps"], 1)
        self.assertEqual(agony["rises"], 0)

    def test_band_measures_from_the_boss_not_the_anchor(self):
        _, rows = gv.band_rows(self.trace)
        agony = next(row for row in rows if row["guid"] == AGONY)
        self.assertAlmostEqual(agony["median"], 30.0)
        self.assertEqual(agony["inside"], 0.0)

    def test_a_slotted_bot_that_never_got_inside_the_gate_still_counts(self):
        self.assertEqual(gv.blocks(self.trace)[FEL], "R")
        _, rows = gv.band_rows(self.trace)
        fel = next(row for row in rows if row["guid"] == FEL)
        self.assertIsNone(fel["median"])
        self.assertEqual(fel["on"], 0.0)
        agony = next(row for row in rows if row["guid"] == AGONY)
        self.assertAlmostEqual(agony["on"], 1.0, places=2)

    def test_the_latch_keeps_the_gate_open_and_a_shut_out_spell_after_joining_counts(self):
        _, rows = gv.band_rows(self.trace)
        tree = next(row for row in rows if row["guid"] == TREE)
        self.assertAlmostEqual(tree["kept"], 2000 / 15000)
        self.assertAlmostEqual(tree["on"], (15000 - 10 - 2000) / 15000)
        self.assertEqual(tree["locked"], 2000)
        fel = next(row for row in rows if row["guid"] == FEL)
        self.assertEqual(fel["locked"], 0)

    def test_a_kicked_searing_flames_is_not_counted_as_one_that_landed(self):
        starts, landed, hits = gv.searing_flames(self.trace, BOSS)
        self.assertEqual(len(starts), 3)
        self.assertEqual(landed, [6000])
        self.assertEqual(len(hits), 2)

    def test_per_bot_latch_spans_do_not_pool_bots(self):
        spans = gv.latch_spans_for(self.trace, "vezax.formation", "on")
        self.assertEqual(set(spans), {AGONY, TREE})
        self.assertEqual(spans[AGONY], [(10, 15000)])

    def test_the_animus_fight_runs_to_the_pull_end_while_the_raid_stands(self):
        # Fel is never sampled, so it neither counts as dead nor gets a row.
        fight = gv.animus_casters(self.trace)
        self.assertEqual((fight["spawn"], fight["stop"], fight["why"]), (12000, 15000, "end"))
        self.assertEqual(fight["health"], (100.0, 97.0))
        self.assertEqual([row["guid"] for row in fight["rows"]], [TREE, AGONY])
        agony = fight["rows"][1]
        # The 13 s bolt is aimed at Vezax a second after the field ran out.
        self.assertEqual((agony["casts"], agony["on_animus"], agony["outside"], agony["field"]), (1, 0, 1, 0))
        self.assertEqual(agony["mana"], (68.0, 65.0))
        # Agony's samples never target the Animus, so its 13.5 s veto is held but not held on it.
        self.assertEqual((agony["holds"], agony["held_on"]), (1, 0))


DART = 6001   # ranged
CHOPS = [6002, 6003, 6004, 6005]
LATE_ANIMUS = 4294969300


def animus_pull(collapse: bool, dart_x: float = 30.0) -> list[dict]:
    """Five bots and an Animus from 2 s. With `collapse` Dart and two others die at 6 s, else the
    Animus leaves the snapshots at 2% after 4 s and the raid stands to the 10 s end. Dart stands at
    `dart_x` on the x axis, the Animus at (1, 1)."""
    records = [
        {"e": "hdr", "v": 12, "ts": 1789500000000, "map": 603, "inst": 5, "diff": 1,
         "boss": "general-vezax", "roster": [{"g": DART, "n": "Dart", "r": "ranged", "c": "mage", "h": 0}]
         + [{"g": guid, "n": f"Chop{guid}", "r": "melee", "c": "rogue", "h": 0} for guid in CHOPS]},
        {"t": 0, "e": "pull", "boss": "general-vezax", "src": "bossstate"},
        {"t": 1, "e": "unit", "g": BOSS, "en": gv.NPC_VEZAX, "n": "General Vezax", "b": 1},
        {"t": 1, "e": "unit", "g": LATE_ANIMUS, "en": gv.NPC_SARONITE_ANIMUS, "n": "Saronite Animus"},
        {"t": 3500, "e": "veto", "g": DART, "m": gv.HOLD, "a": "frostbolt"},
        {"t": 1500, "e": "move", "g": DART, "k": "point", "x": 20.0, "y": 0.0, "z": 0.0,
         "ok": 1, "r": "", "by": gv.REACH_SPELL},
        {"t": 3000, "e": "move", "g": DART, "k": "point", "x": 20.0, "y": 0.0, "z": 0.0,
         "ok": 1, "r": "", "by": gv.REACH_SPELL},
        {"t": 3200, "e": "move", "g": DART, "k": "point", "x": 20.0, "y": 0.0, "z": 0.0,
         "ok": 1, "r": "dup", "by": gv.REACH_SPELL},
        {"t": 2500, "e": "move", "g": DART, "k": "point", "x": 40.0, "y": 0.0, "z": 0.0,
         "ok": 1, "r": "", "by": gv.DODGE},
        {"t": 2600, "e": "move", "g": DART, "k": "point", "x": 40.0, "y": 0.0, "z": 0.0,
         "ok": 0, "r": "dup", "by": gv.DODGE},
        {"t": 3500, "e": "move", "g": DART, "k": "point", "x": 20.0, "y": 0.0, "z": 0.0,
         "ok": 1, "r": "", "by": gv.DODGE},
        {"t": 1500, "e": "note", "g": DART, "k": "vezax.reach", "txt": "off"},
        {"t": 2500, "e": "note", "g": DART, "k": "vezax.reach", "txt": "pulled"},
        {"t": 3500, "e": "note", "g": DART, "k": "vezax.reach", "txt": "floor"},
        {"t": 10000, "e": "end", "out": "wipe" if collapse else "kill"},
    ]
    for when in range(0, 11000, 1000):
        dead = collapse and when >= 6000
        units = [[BOSS, 0.0, 0.0, 0.0, 0.0, 20.0, 0.0, 0, 0, 0, 0, 0],
                 [DART, dart_x, 0.0, 0.0, 0.0, 0.0 if dead else 100.0, 0.0 if dead else 60.0 - when / 500.0,
                  LATE_ANIMUS, 0, 0, 0, when * 10.0]]
        units += [[guid, 1.0, 1.0, 0.0, 0.0, 0.0 if dead and index < 2 else 100.0, 0.0, LATE_ANIMUS, 0, 0, 0, 0]
                  for index, guid in enumerate(CHOPS)]
        if when >= 2000 and (collapse or when <= 4000):
            health = 100.0 - (when - 2000) / 1000.0 * (10.0 if collapse else 49.0)
            units.append([LATE_ANIMUS, 1.0, 1.0, 0.0, 0.0, health, 0.0, CHOPS[0], 0, 0, 0, 0])
        records.append({"t": when, "e": "snap", "u": units})
    return records


class AnimusWindow(unittest.TestCase):
    def trace_of(self, collapse: bool, dart_x: float = 30.0) -> Trace:
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        path = pathlib.Path(folder.name) / "603_5_general-vezax_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in animus_pull(collapse, dart_x)) + "\n",
                        encoding="utf-8")
        return Trace(path)

    def test_reach_counts_living_time_past_the_ceiling_and_accepted_walks_in_the_window(self):
        near = gv.animus_casters(self.trace_of(collapse=True))["rows"][0]
        far = gv.animus_casters(self.trace_of(collapse=True, dart_x=40.0))["rows"][0]
        self.assertEqual(near["past_reach"], 0.0)
        self.assertEqual(far["past_reach"], 1.0)
        self.assertEqual(far["reach_walks"], 1)

    def test_dodges_count_accepted_ones_and_those_ending_past_the_ceiling(self):
        dart = gv.animus_casters(self.trace_of(collapse=True))["rows"][0]
        self.assertEqual(dart["dodges"], (2, 1))

    def test_reach_states_are_the_bots_writing_each_value_from_the_spawn(self):
        self.assertEqual(gv.reach_states(self.trace_of(collapse=True), 2000), {"floor": {DART}, "pulled": {DART}})

    def test_a_collapse_reads_mana_from_the_last_living_sample(self):
        fight = gv.animus_casters(self.trace_of(collapse=True))
        self.assertEqual((fight["stop"], fight["why"]), (6000, "collapse"))
        dart = fight["rows"][0]
        self.assertEqual(dart["mana"], (56.0, 50.0))
        self.assertEqual(dart["dealt"], 40000.0)
        self.assertEqual((dart["holds"], dart["held_on"]), (1, 1))

    def test_an_animus_gone_at_a_sliver_is_a_kill(self):
        fight = gv.animus_casters(self.trace_of(collapse=False))
        self.assertEqual((fight["stop"], fight["why"]), (4000, "kill"))
        self.assertEqual(fight["health"], (100.0, 0.0))


HOLDER = 7001
SPARE = 7002
LIGHT = 7003
TANK_ANIMUS = 4294969400


def tank_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 12, "ts": 1789500000000, "map": 603, "inst": 6, "diff": 1,
         "boss": "general-vezax", "roster": [
             {"g": HOLDER, "n": "Holder", "r": "tank", "c": "warrior", "h": 0},
             {"g": SPARE, "n": "Spare", "r": "tank", "c": "druid", "h": 0},
             {"g": LIGHT, "n": "Light", "r": "heal", "c": "paladin", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "general-vezax", "src": "bossstate"},
        {"t": 1, "e": "unit", "g": BOSS, "en": gv.NPC_VEZAX, "n": "General Vezax", "b": 1},
        {"t": 1, "e": "unit", "g": TANK_ANIMUS, "en": gv.NPC_SARONITE_ANIMUS, "n": "Saronite Animus"},
        {"t": 0, "e": "cast", "s": LIGHT, "sp": gv.SPELL_BEACON, "tgt": SPARE, "ct": 0},
        {"t": 0, "e": "aura", "d": SPARE, "s": LIGHT, "sp": gv.SPELL_BEACON, "r": 0},
        {"t": 6000, "e": "aura", "d": SPARE, "s": LIGHT, "sp": gv.SPELL_BEACON, "r": 1},
        {"t": 6000, "e": "cast", "s": LIGHT, "sp": gv.SPELL_BEACON, "tgt": HOLDER, "ct": 0},
        {"t": 6000, "e": "aura", "d": HOLDER, "s": LIGHT, "sp": gv.SPELL_BEACON, "r": 0},
        {"t": 3000, "e": "heal", "s": LIGHT, "d": SPARE, "sp": 53652, "a": 2000, "oh": 1500},
        {"t": 7000, "e": "heal", "s": LIGHT, "d": HOLDER, "sp": 53654, "a": 3000, "oh": 0},
        {"t": 9500, "e": "heal", "s": LIGHT, "d": HOLDER, "sp": 53652, "a": 4000, "oh": 0},
        {"t": 2000, "e": "cast", "s": LIGHT, "sp": gv.SPELL_SACRED_SHIELD, "tgt": SPARE, "ct": 0},
        {"t": 8000, "e": "cast", "s": LIGHT, "sp": gv.SPELL_SACRED_SHIELD, "tgt": HOLDER, "ct": 0},
        {"t": 6500, "e": "veto", "g": LIGHT, "m": gv.TANK_BUFFS_HOLD, "a": "beacon of light on tank"},
        {"t": 9000, "e": "death", "g": HOLDER, "killer": BOSS},
        {"t": 10000, "e": "end", "out": "wipe"},
    ]
    for when in range(0, 11000, 1000):
        units = [[BOSS, 0.0, 0.0, 0.0, 0.0, 50.0, 0.0, SPARE if when < 1000 else HOLDER, 0, 0, 0, 0],
                 [HOLDER, 1.0, 0.0, 0.0, 0.0, 0.0 if when >= 9000 else 100.0, 0.0, BOSS, 0, 0, 0, 0],
                 [SPARE, 2.0, 0.0, 0.0, 0.0, 100.0, 0.0, BOSS, 0, 0, 0, 0],
                 [LIGHT, 30.0, 0.0, 0.0, 0.0, 100.0, 50.0, 0, 0, 0, 0, 0]]
        if when >= 4000:
            units.append([TANK_ANIMUS, 1.0, 1.0, 0.0, 0.0, 100.0, 0.0, HOLDER, 0, 0, 0, 0])
        records.append({"t": when, "e": "snap", "u": units})
    return records


class TankBuffs(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        path = pathlib.Path(folder.name) / "603_6_general-vezax_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in tank_pull()) + "\n", encoding="utf-8")
        self.trace = Trace(path)

    def test_the_main_tank_is_the_one_he_hit_longest(self):
        self.assertEqual(gv.main_tank(self.trace), HOLDER)

    def test_beacon_shares_split_at_the_spawn_and_stop_at_his_death(self):
        buffs = gv.tank_buffs(self.trace)
        self.assertEqual(buffs["death"], 9000)
        self.assertEqual(buffs["before"], (0.0, 1.0))
        self.assertAlmostEqual(buffs["after"][0], 3000 / 5000)
        self.assertAlmostEqual(buffs["after"][1], 2000 / 5000)

    def test_copies_count_effective_healing_up_to_his_death(self):
        copies = gv.tank_buffs(self.trace)["copies"]
        self.assertEqual((copies["him"], copies["others"], copies["cast"]), (3000, 500, 5000))

    def test_sacred_shield_and_the_hold_are_counted(self):
        buffs = gv.tank_buffs(self.trace)
        self.assertEqual(buffs["shields"], {"tank": 1, "him": 1})
        self.assertEqual((buffs["beacons"], buffs["holds"]), (2, 1))


MARKED = 8001
MATE = 8002
LEFTIE = 8003
MARK_ANIMUS = 4294969500


def mark_crash_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 12, "ts": 1789500000000, "map": 603, "inst": 7, "diff": 1,
         "boss": "general-vezax", "roster": [
             {"g": MARKED, "n": "Marked", "r": "ranged", "c": "mage", "h": 0},
             {"g": MATE, "n": "Mate", "r": "heal", "c": "priest", "h": 0},
             {"g": LEFTIE, "n": "Leftie", "r": "ranged", "c": "hunter", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "general-vezax", "src": "bossstate"},
        {"t": 1, "e": "unit", "g": BOSS, "en": gv.NPC_VEZAX, "n": "General Vezax", "b": 1},
        {"t": 1, "e": "unit", "g": MARK_ANIMUS, "en": gv.NPC_SARONITE_ANIMUS, "n": "Saronite Animus"},
        {"t": 10, "e": "note", "g": MARKED, "k": "vezax.block", "txt": "R"},
        {"t": 10, "e": "note", "g": MATE, "k": "vezax.block", "txt": "R"},
        {"t": 10, "e": "note", "g": LEFTIE, "k": "vezax.block", "txt": "L"},
        {"t": 1000, "e": "aura", "d": MARKED, "s": BOSS, "sp": gv.SPELL_MARK_OF_THE_FACELESS, "r": 0},
        {"t": 11000, "e": "aura", "d": MARKED, "s": BOSS, "sp": gv.SPELL_MARK_OF_THE_FACELESS, "r": 1},
        {"t": 2000, "e": "dmg", "s": BOSS, "d": MATE, "sp": gv.SPELL_MARK_LEECH, "a": 5000},
        {"t": 3000, "e": "dmg", "s": BOSS, "d": LEFTIE, "sp": gv.SPELL_MARK_LEECH, "a": 5000},
        {"t": 4000, "e": "dmg", "s": BOSS, "d": MATE, "sp": gv.SPELL_MARK_LEECH, "a": 5000},
        {"t": 2500, "e": "haz", "sp": gv.SPELL_SHADOW_CRASH_IMPACT, "shape": "circle",
         "x": -7.5, "y": 33.0, "z": 0.0, "ttl": 3000, "rad": 10.0},
        {"t": 12000, "e": "haz", "sp": gv.SPELL_SHADOW_CRASH_IMPACT, "shape": "circle",
         "x": -7.5, "y": 33.0, "z": 0.0, "ttl": 3000, "rad": 10.0},
        {"t": 2800, "e": "move", "g": MARKED, "k": "point", "x": -31.0, "y": 15.0, "z": 0.0,
         "ok": 1, "r": "", "by": gv.DODGE},
        {"t": 2900, "e": "move", "g": MATE, "k": "point", "x": 8.0, "y": 32.75, "z": 0.0,
         "ok": 1, "r": "", "by": gv.DODGE},
        {"t": 3000, "e": "move", "g": LEFTIE, "k": "point", "x": 19.0, "y": 35.0, "z": 0.0,
         "ok": 1, "r": "", "by": gv.DODGE},
        {"t": 15000, "e": "end", "out": "wipe"},
    ]
    for when in range(0, 16000, 1000):
        records.append({"t": when, "e": "snap", "u": [
            [BOSS, 0.0, 0.0, 0.0, 0.0, 20.0, 0.0, 0, 0, 0, 0, 0],
            [MARKED, -4.0, 35.0, 0.0, 0.0, 100.0, 50.0, MARK_ANIMUS, 0, 0, 0, 0],
            [MATE, -7.0, 32.75, 0.0, 0.0, 100.0, 50.0, MARK_ANIMUS, 0, 0, 0, 0],
            [LEFTIE, 4.0, 35.0, 0.0, 0.0, 100.0, 50.0, MARK_ANIMUS, 0, 0, 0, 0],
            [MARK_ANIMUS, 1.0, 1.0, 0.0, 0.0, 90.0, 0.0, 0, 0, 0, 0, 0]]})
    return records


class MarkCrash(unittest.TestCase):
    def setUp(self):
        self.trace = self.write(mark_crash_pull())

    def write(self, records: list[dict]) -> Trace:
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        path = pathlib.Path(folder.name) / "603_7_general-vezax_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in records) + "\n", encoding="utf-8")
        return Trace(path)

    def test_a_dodge_refused_as_dup_still_reads_the_marked_bots_walk(self):
        records = mark_crash_pull()
        for rec in records:
            if rec.get("e") == "move" and rec["g"] == MARKED:
                rec.update(ok=0, r="dup")
        row = gv.mark_crashes(self.write(records))[0]
        self.assertEqual(row["walk"], "spot")

    def test_only_a_crash_cast_inside_the_mark_is_a_case(self):
        rows = gv.mark_crashes(self.trace)
        self.assertEqual([(row["guid"], row["cast"], row["phase"], row["on"]) for row in rows],
                         [(MARKED, 2500, "animus", "R")])

    def test_group_ticks_are_the_leech_on_the_marked_bots_block(self):
        row = gv.mark_crashes(self.trace)[0]
        self.assertEqual((row["ticks"], row["group"], row["group_after"]), (3, 2, 1))

    def test_the_marked_bot_heads_for_its_arc_spot_and_its_blockmate_goes_the_other_way(self):
        row = gv.mark_crashes(self.trace)[0]
        self.assertEqual(row["walk"], "spot")
        self.assertEqual((row["with"], row["away"]), (0, 1))

    def test_a_crash_before_the_spawn_or_after_the_animus_left_is_not_its_phase(self):
        samples = gv.Samples(self.trace)
        self.assertEqual(gv.crash_phase(samples, MARK_ANIMUS, 2000, 1000), "pre")
        self.assertEqual(gv.crash_phase(samples, MARK_ANIMUS, 0, 30000), "post")
        self.assertEqual(gv.crash_phase(samples, None, None, 2500), "pre")


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            gv.show_banner(trace)
            for _, _, section in gv.SECTIONS:
                section(trace)
        self.assertIn("not a Vezax pull", out.getvalue())


if __name__ == "__main__":
    unittest.main()
