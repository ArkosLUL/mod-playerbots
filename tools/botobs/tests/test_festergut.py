"""Tests for the Festergut reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25H pull pins the stands, two spore windows (a spare carrier sent close, a goo flip of the
ranged stack), the goo dodges and a leg flip, the picks, a swap election with its taunt, the brake, the
tank windows with a Pungent Blight cover, and the cheat filter; the shared v14 fixture shows every
section reads empty on a trace that is not Festergut rather than raising.
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
BULWARK = 7201  # tank, tank spot 1
AEGIS = 7202    # tank, tank spot 2, takes him at 9
MEND = 7203     # healer, slot 1
SHIV = 7204     # melee, leg
ARCANE = 7205   # ranged, slot 4

ROSTER = ((BULWARK, "Bulwark", "tank"), (AEGIS, "Aegis", "tank"), (MEND, "Mend", "heal"),
          (SHIV, "Shiv", "melee"), (ARCANE, "Arcane", "ranged"))


def note(when: int, guid: int, key: str, value) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": str(value)}


def snap(when: int, target: int) -> dict:
    return {"t": when, "e": "snap", "hz": [], "u": [[FESTERGUT, 0.0, 0.0, 0.0, 0.0, 90.0, 0.0, target, 0, 0, 0, 0]]}


def aura(when: int, guid: int, spell: int, stacks: int = 1, removed: int = 0) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": FESTERGUT, "sp": spell, "r": removed, "st": stacks}


def cast(when: int, source: int, spell: int) -> dict:
    return {"t": when, "e": "cast", "s": source, "d": source, "sp": spell}


def pull(boss: str = "festergut", diff: int = 3) -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": diff, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": 0} for guid, name, role in ROSTER]},
        {"t": 0, "e": "pull", "boss": boss, "src": "engage"},
        {"t": 1, "e": "unit", "g": FESTERGUT, "en": fg.NPC_FESTERGUT, "n": "Festergut", "lvl": 83, "b": 1},
        {"t": 1, "e": "spell", "sp": 871, "n": "Shield Wall"},
        {"t": 1, "e": "spell", "sp": 31821, "n": "Aura Mastery"},
        snap(0, BULWARK), snap(1000, BULWARK), snap(30000, AEGIS), snap(31000, AEGIS),
        snap(60000, BULWARK), snap(61000, MEND), snap(62000, BULWARK),
        aura(29000, BULWARK, 72552, 9), aura(59000, AEGIS, 72552, 4),
        note(50, BULWARK, fg.SLOT, 20), note(50, AEGIS, fg.SLOT, 21), note(50, MEND, fg.SLOT, 1),
        note(50, SHIV, fg.SLOT, 19), note(50, ARCANE, fg.SLOT, 4),
        note(100, BULWARK, fg.STAND, "tank"), note(100, AEGIS, fg.STAND, "tank"),
        note(100, MEND, fg.STAND, "slot"), note(100, SHIV, fg.STAND, "leg"), note(100, ARCANE, fg.STAND, "slot"),
        note(100, SHIV, "fg.phase", "normal"),
        note(150, ARCANE, "fg.phase", "normal"),
        note(100, MEND, fg.SPORESPOT, 1),
        note(100, MEND, fg.LEG, 1),
        # Both spores land on the far side: Arcane is sent close; goo flips the ranged stack to spot 2
        aura(19900, ARCANE, fg.SPELL_GAS_SPORE),
        aura(19900, MEND, fg.SPELL_GAS_SPORE),
        note(20000, SHIV, "fg.phase", "spore"),
        note(20100, ARCANE, "fg.phase", "spore"),
        note(20050, ARCANE, fg.CARRIER, 1),
        note(20050, MEND, fg.CARRIER, 2),
        note(20100, ARCANE, fg.STAND, "close"), note(20100, SHIV, fg.STAND, "close"),
        note(20100, MEND, fg.STAND, "far"), note(20100, AEGIS, fg.STAND, "close"),
        note(25000, MEND, fg.SPORESPOT, 2),
        note(32000, ARCANE, "fg.phase", "normal"),
        note(32000, ARCANE, fg.CARRIER, 0), note(32000, MEND, fg.CARRIER, 0),
        note(32050, ARCANE, fg.STAND, "slot"), note(32050, SHIV, fg.STAND, "leg"),
        note(32050, MEND, fg.STAND, "slot"), note(32050, AEGIS, fg.STAND, "tank"),
        aura(59950, SHIV, fg.SPELL_GAS_SPORE),
        note(60000, ARCANE, "fg.phase", "spore"),
        note(60050, SHIV, fg.CARRIER, 1),
        note(72000, ARCANE, "fg.phase", "normal"),
        # Goo: Arcane dodges twice, Shiv once onto the other leg
        {"t": 24000, "e": "haz", "sp": 72296, "shape": "circle", "x": 0.0, "y": 0.0, "z": 0.0, "ttl": 8000,
         "rad": 12.0},
        {"t": 40000, "e": "haz", "sp": 72296, "shape": "circle", "x": 0.0, "y": 0.0, "z": 0.0, "ttl": 8000,
         "rad": 12.0},
        note(40100, ARCANE, fg.GOO, "spread"),
        note(40100, SHIV, fg.GOO, "stand"),
        note(40100, MEND, fg.LEG, 2),
        note(45000, ARCANE, fg.GOO, "tight"),
        # Picks, the swap at 9 and the relieved tank's brake
        note(200, BULWARK, fg.TARGET, "boss"), note(200, SHIV, fg.TARGET, "boss"),
        note(40000, AEGIS, fg.TARGET, "brake"), note(45000, AEGIS, fg.TARGET, "boss"),
        note(29500, AEGIS, fg.SWAP, 1),
        {"t": 29600, "e": "act", "g": AEGIS, "a": fg.SWAP_ACTION, "rel": 67.0, "vd": "OK"},
        note(29900, AEGIS, fg.SWAP, 0),
        note(31000, BULWARK, fg.BRAKE, 1), note(51000, BULWARK, fg.BRAKE, 0),
        # Tank windows: Inhaled Blight on Aegis, Shield Wall in it; Pungent Blight with a cover
        note(35000, AEGIS, fg.DEFENSIVE, "Shield Wall"),
        cast(35100, AEGIS, 871),
        note(36000, AEGIS, fg.DEFENSIVE, "idle"),
        note(36500, AEGIS, fg.HUMAN_CALL, "871"),
        cast(80000, FESTERGUT, 73032),
        note(79000, MEND, fg.COVER, "Aura Mastery"),
        note(79500, ARCANE, fg.PERSONAL, "none"),
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


class Stands(unittest.TestCase):
    def test_roster_and_time_per_kind(self):
        rows = fg.stand_rows(load(pull()))
        self.assertEqual(rows[SHIV]["roster"], [19])
        self.assertEqual(rows[AEGIS]["roster"], [21])
        self.assertEqual(rows[SHIV]["held"]["close"], 11950)
        self.assertEqual(rows[MEND]["held"]["far"], 11950)

    def test_stand_names(self):
        self.assertEqual([fg.stand_name(stand) for stand in (1, 18, 19, 20, 21)],
                         ["slot 1", "slot 18", "leg", "tank 1", "tank 2"])


class Spores(unittest.TestCase):
    def test_each_window_names_its_carriers_their_groups_and_the_stacks(self):
        rows = fg.window_rows(load(pull()))
        self.assertEqual([(row["start"], row["stop"], row["carriers"], row["spots"]) for row in rows], [
            (20000, 32000, [(ARCANE, "close"), (MEND, "far")], ["1", "2"]),
            (60000, 72000, [(SHIV, "close")], ["2"]),
        ])
        self.assertEqual(rows[0]["stood"], {"close": 3, "far": 1})

    def test_section(self):
        self.assertEqual(printed(fg.show_spores, load(pull())), (
            "SPORES\n"
            "  2 window(s); carriers gained Gas Spore from 1 s before the window opened\n"
            "\n"
            "         from       held  far    close   far  carriers\n"
            "     0:20.000   0:12.000  1->2       3     1  Arcane (close), Mend (far)\n"
            "     1:00.000   0:12.000  2          0     0  Shiv (close)\n"))


class Goo(unittest.TestCase):
    def test_branches_per_bot(self):
        rows = fg.goo_rows(load(pull()))
        self.assertEqual(dict(rows[ARCANE]), {"spread": 1, "tight": 1})
        self.assertEqual(dict(rows[SHIV]), {"stand": 1})

    def test_normal_mode_has_no_goo(self):
        self.assertIn("normal mode: Putricide throws no goo at Festergut", printed(fg.show_goo, load(pull(diff=1))))

    def test_section_counts_impacts_and_the_leg_flip(self):
        out = printed(fg.show_goo, load(pull()))
        self.assertIn("  2 impact(s); the bots dodge 6.5 yd round each, to a spot 7.5 yd out\n", out)
        self.assertIn("  melee leg 1 -> 2\n", out)


class Targets(unittest.TestCase):
    def test_time_per_rule(self):
        rows = fg.target_time(load(pull()))
        self.assertEqual(rows["brake"], (1, 5000))
        self.assertEqual(rows["boss"][0], 3)


class Tanks(unittest.TestCase):
    def test_election_brake_and_peaks(self):
        trace = load(pull())
        self.assertEqual(fg.elections(trace), [(29500, AEGIS, 29600)])
        self.assertEqual(fg.brake_time(trace), {BULWARK: (1, 20000)})
        out = printed(fg.show_tanks, trace)
        self.assertIn("2 swap(s) between tanks, at 9 Gastric Bloat by design", out)
        self.assertIn("  0:29.500  Aegis elected, taunted 0.1 s after\n", out)
        self.assertIn("  Bulwark braked 1 time(s), 0:20.000 in all\n", out)
        self.assertIn("  Gastric Bloat peak: Bulwark 9, Aegis 4\n", out)


class Defensives(unittest.TestCase):
    def test_window_cast_call_and_cover(self):
        out = printed(fg.show_defensives, load(pull()))
        self.assertIn("  0:35.000  Aegis       0:01.000  offered Shield Wall; cast Shield Wall +0.1s\n", out)
        self.assertIn("  calls to a human tank  871 0:36.500\n", out)
        self.assertIn("  Pungent Blight 1:20.000: Aura Mastery Mend\n", out)


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
        records = [rec for rec in pull() if rec.get("k") != fg.GOO]
        self.assertIn("fg.goo", printed(fg.show_banner, load(records)))

    def test_normal_mode_does_not_miss_the_heroic_probe(self):
        records = [rec for rec in pull(diff=1) if rec.get("k") != fg.GOO]
        self.assertNotIn("fg.goo", printed(fg.show_banner, load(records)))

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
