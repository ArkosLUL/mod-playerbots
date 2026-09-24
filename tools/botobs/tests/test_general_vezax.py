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

        # Tricks goes out half a second before the Animus, at a dps. Bulwark switches onto it at once,
        # Agony a second and a half later, and Heroism waits until 14.5 s.
        {"t": 11500, "e": "cast", "s": FEL, "sp": gv.SPELL_TRICKS, "tgt": AGONY, "ct": 0},
        {"t": 12200, "e": "act", "g": BULWARK, "a": gv.ANIMUS_SWITCH, "rel": 65.0, "vd": "OK"},
        {"t": 13500, "e": "act", "g": AGONY, "a": gv.ANIMUS_SWITCH, "rel": 65.0, "vd": "OK"},
        {"t": 13600, "e": "act", "g": AGONY, "a": gv.ANIMUS_SWITCH, "rel": 65.0, "vd": "OK"},
        {"t": 14500, "e": "cast", "s": BULWARK, "sp": 32182, "tgt": 0, "ct": 0},
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

    def test_a_crash_names_its_target_and_both_blocks_that_moved(self):
        rows = gv.crashes(self.trace)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["target"], AGONY)
        self.assertEqual(rows[0]["block"], "L")
        self.assertEqual(rows[0]["dodged"], {"L": {AGONY}, "R": {TREE}})

    def test_a_crash_names_what_else_he_cast_while_it_flew(self):
        # The 6 s Searing Flames goes out inside the 5 s to 8 s flight; the 2 s and 10 s ones don't.
        self.assertEqual(gv.crashes(self.trace)[0]["masked"], [gv.SPELL_SEARING_FLAMES])

    def test_a_star_on_a_vapor_is_counted_though_nobody_targeted_it(self):
        self.assertEqual(gv.vapor_hits(self.trace), {(TREE, 53190): [9500]})

    def test_hard_mode_reads_the_first_bot_to_see_each_value(self):
        self.assertEqual(gv.hard_mode_changes(self.trace), {"pending": 10, "lost": 9600})

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

    def test_a_kicked_searing_flames_is_not_counted_as_one_that_landed(self):
        starts, landed, hits = gv.searing_flames(self.trace, BOSS)
        self.assertEqual(len(starts), 3)
        self.assertEqual(landed, [6000])
        self.assertEqual(len(hits), 2)

    def test_per_bot_latch_spans_do_not_pool_bots(self):
        spans = gv.latch_spans_for(self.trace, "vezax.formation", "on")
        self.assertEqual(set(spans), {AGONY, TREE})
        self.assertEqual(spans[AGONY], [(10, 15000)])


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
