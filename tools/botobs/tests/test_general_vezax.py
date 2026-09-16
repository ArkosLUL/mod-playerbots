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


def snap(when: int, agony_target: int, field: bool) -> dict:
    return {
        "t": when, "e": "snap",
        "u": [
            [BOSS, 0.0, 0.0, 0.0, 0.0, 90.0, 0.0, BULWARK, 0, 0, 0, 0],
            [AGONY, 30.0, 0.0, 0.0, 0.0, 100.0, 80.0 - when / 1000.0, agony_target, 0, 0, 0, 0],
            [TREE, 0.0, 30.0, 0.0, 0.0, 100.0, 90.0, 0, 0, 0, 0, 0],
            [BULWARK, 1.0, 0.0, 0.0, 0.0, 100.0, 0.0, BOSS, 0, 0, 0, 0],
        ],
        "hz": [[gv.SPELL_FIELD, 30.0, 2.0, 0.0, 8.0, 1]] if field else [],
    }


def vezax_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 12, "ts": 1789500000000, "map": 603, "inst": 4, "diff": 1,
         "boss": "general-vezax", "roster": [
             {"g": AGONY, "n": "Agony", "r": "ranged", "c": "warlock", "h": 0},
             {"g": TREE, "n": "Tree", "r": "heal", "c": "druid", "h": 0},
             {"g": BULWARK, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "general-vezax", "src": "bossstate"},
        {"t": 1, "e": "unit", "g": BOSS, "en": gv.NPC_VEZAX, "n": "General Vezax", "b": 1},
        {"t": 1, "e": "unit", "g": VAPOR, "en": gv.NPC_SARONITE_VAPORS, "n": "Saronite Vapors"},
        {"t": 10, "e": "note", "g": AGONY, "k": "vezax.block", "txt": "L"},
        {"t": 10, "e": "note", "g": TREE, "k": "vezax.block", "txt": "R"},
        {"t": 10, "e": "note", "g": BULWARK, "k": "vezax.block", "txt": "tank"},
        {"t": 10, "e": "note", "g": AGONY, "k": "vezax.formation", "txt": "on"},
        {"t": 10, "e": "note", "g": TREE, "k": "vezax.formation", "txt": "on"},

        # Tree carries the mark 1 s to 11 s. The 12.5 s tick is the late one that trails the aura.
        {"t": 1000, "e": "aura", "d": TREE, "s": BOSS, "sp": gv.SPELL_MARK_OF_THE_FACELESS, "r": 0},
        {"t": 11000, "e": "aura", "d": TREE, "s": BOSS, "sp": gv.SPELL_MARK_OF_THE_FACELESS, "r": 1},
        {"t": 3000, "e": "dmg", "s": BOSS, "d": AGONY, "sp": gv.SPELL_MARK_LEECH, "a": 5000},
        {"t": 12500, "e": "dmg", "s": BOSS, "d": AGONY, "sp": gv.SPELL_MARK_LEECH, "a": 4000},
        {"t": 14000, "e": "dmg", "s": BOSS, "d": AGONY, "sp": gv.SPELL_MARK_LEECH, "a": 3000},

        # One crash on Agony, and one bot from each block dodges it.
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
