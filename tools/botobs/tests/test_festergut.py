"""Tests for the Festergut reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25H pull pins each section, and the shared v14 fixture shows every section reads empty on
a trace that is not Festergut rather than raising.
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

from bosses import festergut as fg  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

FESTERGUT = (1 << 32) | 600
BULWARK = 7201  # tank
AEGIS = 7202    # tank
MEND = 7203     # healer
SHIV = 7204     # melee
ARCANE = 7205   # ranged

ROSTER = ((BULWARK, "Bulwark", "tank"), (AEGIS, "Aegis", "tank"), (MEND, "Mend", "heal"),
          (SHIV, "Shiv", "melee"), (ARCANE, "Arcane", "ranged"))


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def snap(when: int, target: int) -> dict:
    return {"t": when, "e": "snap", "hz": [], "u": [[FESTERGUT, 0.0, 0.0, 0.0, 0.0, 90.0, 0.0, target, 0, 0, 0, 0]]}


def aura(when: int, guid: int, spell: int, stacks: int = 1, removed: int = 0) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": FESTERGUT, "sp": spell, "r": removed, "st": stacks}


def pull(boss: str = "festergut", diff: int = 3) -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": diff, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": 0} for guid, name, role in ROSTER]},
        {"t": 0, "e": "pull", "boss": boss, "src": "engage"},
        {"t": 1, "e": "unit", "g": FESTERGUT, "en": fg.NPC_FESTERGUT, "n": "Festergut", "lvl": 83, "b": 1},
        snap(0, BULWARK), snap(1000, BULWARK), snap(30000, AEGIS), snap(31000, AEGIS),
        snap(60000, BULWARK), snap(61000, MEND), snap(62000, BULWARK),
        aura(29000, BULWARK, 72551, 6), aura(59000, AEGIS, 72551, 7),
        # Two spore windows; goo lands near the first spot in the first, flipping the ranged to spot 2.
        note(100, SHIV, "fg.phase", "normal"),
        note(150, ARCANE, "fg.phase", "normal"),
        aura(19900, SHIV, fg.SPELL_GAS_SPORE),
        note(20000, SHIV, "fg.phase", "spore"),
        note(20100, ARCANE, "fg.phase", "spore"),
        note(20050, ARCANE, "fg.sporespot", "1"),
        note(25000, ARCANE, "fg.sporespot", "2"),
        note(32000, ARCANE, "fg.phase", "normal"),
        aura(59950, MEND, fg.SPELL_GAS_SPORE),
        note(60000, ARCANE, "fg.phase", "spore"),
        note(60050, ARCANE, "fg.sporespot", "1"),
        note(72000, ARCANE, "fg.phase", "normal"),
        # Goo outside a spore window: Arcane dodges and holds, Mend only dodges.
        {"t": 24000, "e": "haz", "sp": 72296, "shape": "circle", "x": 0.0, "y": 0.0, "z": 0.0, "ttl": 8000,
         "rad": 12.0},
        {"t": 40000, "e": "haz", "sp": 72296, "shape": "circle", "x": 0.0, "y": 0.0, "z": 0.0, "ttl": 8000,
         "rad": 12.0},
        note(40100, ARCANE, "fg.goohold", "dodge"),
        note(41000, ARCANE, "fg.goohold", "hold"),
        note(48000, ARCANE, "fg.goohold", "none"),
        note(40200, MEND, "fg.goohold", "dodge"),
        note(48200, MEND, "fg.goohold", "none"),
        {"t": 31500, "e": "cheat", "g": AEGIS, "k": "icc.tauntfixate", "n": 2, "tgt": FESTERGUT},
        {"t": 50000, "e": "cheat", "g": SHIV, "k": "rf.nitro", "n": 1},
        {"t": 90000, "e": "end", "out": "kill"},
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


class Spores(unittest.TestCase):
    def test_each_window_names_its_carrier_and_its_spots(self):
        rows = fg.window_rows(load(pull()))
        self.assertEqual([(row["start"], row["stop"], row["carriers"], row["spots"]) for row in rows], [
            (20000, 32000, [SHIV], ["1", "2"]),
            (60000, 72000, [MEND], ["1"]),
        ])

    def test_section(self):
        self.assertEqual(printed(fg.show_spores, load(pull())), (
            "SPORES\n"
            "  2 window(s); carriers gained Gas Spore from 1 s before the window opened\n"
            "\n"
            "         from       held  spot    carriers\n"
            "     0:20.000   0:12.000  1->2    Shiv\n"
            "     1:00.000   0:12.000  1       Mend\n"))


class Goo(unittest.TestCase):
    def test_dodges_and_hold_time_per_bot(self):
        self.assertEqual(fg.goo_rows(load(pull())), {
            ARCANE: {"dodges": 1, "held": 7000},
            MEND: {"dodges": 1, "held": 0},
        })

    def test_normal_mode_has_no_goo(self):
        self.assertIn("normal mode: Putricide throws no goo at Festergut", printed(fg.show_goo, load(pull(diff=1))))

    def test_section_counts_the_impacts(self):
        self.assertIn("  2 impact(s), each a 12 yd circle for 8 s\n", printed(fg.show_goo, load(pull())))


class Tanks(unittest.TestCase):
    def test_runs_swaps_and_peaks(self):
        self.assertEqual(printed(fg.show_tanks, load(pull())), (
            "TANKS\n"
            "  who he hit, each run; 2 swap(s) between tanks, at 6 Gastric Bloat by design\n"
            "\n"
            "     0:00.000   0:30.000  Bulwark (tank)\n"
            "     0:30.000   0:30.000  Aegis (tank)\n"
            "     1:00.000   0:01.000  Bulwark (tank)\n"
            "     1:01.000   0:01.000  Mend (heal)\n"
            "     1:02.000   0:00.000  Bulwark (tank)\n"
            "\n"
            "  Gastric Bloat peak: Aegis 7, Bulwark 6\n"))


class Phases(unittest.TestCase):
    def test_steps(self):
        self.assertEqual(fg.icc.phase_steps(load(pull())), [
            ("normal", 100, 20000), ("spore", 20000, 32000), ("normal", 32000, 60000),
            ("spore", 60000, 72000), ("normal", 72000, 90000),
        ])


class Cheats(unittest.TestCase):
    def test_own_keys_only_and_the_rest_counted(self):
        self.assertEqual(printed(fg.show_cheats, load(pull())), (
            "CHEATS\n"
            "  icc.tauntfixate  2 use(s) from 0:31.500\n"
            "    by  Aegis 2\n"
            "    on  Festergut\n"
            "  1 use(s) under other encounters' keys: icecrown_citadel.py --cheats\n"))


class Banner(unittest.TestCase):
    def test_names_heroic_probes_it_misses(self):
        records = [rec for rec in pull() if rec.get("k") != "fg.goohold"]
        self.assertIn("probes absent from this trace: fg.goohold", printed(fg.show_banner, load(records)))

    def test_normal_mode_does_not_miss_the_heroic_probe(self):
        records = [rec for rec in pull(diff=1) if rec.get("k") != "fg.goohold"]
        self.assertIn("all fg.* probes present", printed(fg.show_banner, load(records)))

    def test_another_icc_boss_is_refused(self):
        self.assertIn("  not a Festergut pull, every section below reads empty",
                      printed(fg.show_banner, load(pull(boss="rotface"))))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            fg.show_banner(trace)
            for _, _, section in fg.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Festergut pull"), len(fg.SECTIONS))


if __name__ == "__main__":
    unittest.main()
