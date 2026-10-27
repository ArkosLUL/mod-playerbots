"""Tests for the Blood Prince Council reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25H pull pins each section, and the shared v14 fixture shows every section reads empty on
a trace that is not the council rather than raising.
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

from bosses import blood_prince_council as bpc  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
VALANAR = CREATURE | 900
KELESETH = CREATURE | 901
TALDARAM = CREATURE | 902
BOMB_A = CREATURE | 903
BOMB_B = CREATURE | 904
BULWARK = 7501  # main tank
AEGIS = 7502    # assist tank
VOLLEY = 7503   # hunter
ARCANE = 7504   # ranged
SHIV = 7505     # melee

ROSTER = ((BULWARK, "Bulwark", "tank"), (AEGIS, "Aegis", "tank"), (VOLLEY, "Volley", "ranged"),
          (ARCANE, "Arcane", "ranged"), (SHIV, "Shiv", "melee"))


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def pull(boss: str = "blood-prince-council") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": 3, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": 0} for guid, name, role in ROSTER]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        {"t": 1, "e": "unit", "g": VALANAR, "en": 37970, "n": "Prince Valanar", "lvl": 83, "b": 1},
        {"t": 1, "e": "unit", "g": KELESETH, "en": 37972, "n": "Prince Keleseth", "lvl": 83, "b": 1},
        {"t": 1, "e": "unit", "g": TALDARAM, "en": 37973, "n": "Prince Taldaram", "lvl": 83, "b": 1},
        {"t": 1, "e": "unit", "g": BOMB_A, "en": 38454, "n": "Kinetic Bomb"},
        {"t": 1, "e": "unit", "g": BOMB_B, "en": 38454, "n": "Kinetic Bomb"},
        note(100, BULWARK, "bpc.phase", "valanar"),
        note(120, SHIV, "bpc.phase", "valanar"),
        note(45000, SHIV, "bpc.phase", "none"),
        note(45200, SHIV, "bpc.phase", "taldaram"),
        note(45300, BULWARK, "bpc.phase", "taldaram"),
        note(91000, BULWARK, "bpc.phase", "keleseth"),
        # Volley takes the first bomb as the hunter, then flips to the second; Arcane is spare, then gets one.
        note(20000, VOLLEY, "bpc.bomb", f"hunter {BOMB_A}"),
        note(20000, ARCANE, "bpc.bomb", "spare"),
        note(22000, VOLLEY, "bpc.bomb", f"hunter {BOMB_B}"),
        note(23000, ARCANE, "bpc.bomb", f"any {BOMB_A}"),
        note(30000, VOLLEY, "bpc.bomb", "none"),
        note(30000, ARCANE, "bpc.bomb", "none"),
        # Spread rings while Valanar is empowered; the vortex pushes Arcane's slot round once.
        note(15000, ARCANE, "bpc.spread", "ranged 1/3"),
        note(15000, SHIV, "bpc.spread", "none"),
        note(18000, ARCANE, "bpc.spread", "vortex 2/4 nudged"),
        note(18000, SHIV, "bpc.spread", "vortex 3/4"),
        note(21000, ARCANE, "bpc.spread", "ranged 1/3"),
        note(21000, SHIV, "bpc.spread", "none"),
        note(45000, ARCANE, "bpc.spread", "none"),
        # Bulwark marks Valanar at the pull; Aegis moves the skull to Taldaram, Bulwark on to Keleseth.
        note(200, BULWARK, "bpc.skull", f"empowered {VALANAR}"),
        note(45400, AEGIS, "bpc.skull", f"empowered {TALDARAM} from {VALANAR}"),
        note(91100, BULWARK, "bpc.skull", f"empowered {KELESETH} from {TALDARAM}"),
        {"t": 50000, "e": "cheat", "g": SHIV, "k": "pp.heroicbuff", "n": 2, "tgt": SHIV},
        {"t": 120000, "e": "end", "out": "kill"},
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


class Phases(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(bpc.show_phases, load(pull())), (
            "PHASES\n"
            "  bpc.phase, each step from the first bot to note it\n"
            "\n"
            "         from       held  empowered\n"
            "     0:00.100   0:44.900  valanar\n"
            "     0:45.000   0:00.200  none\n"
            "     0:45.200   0:45.800  taldaram\n"
            "     1:31.000   0:29.000  keleseth\n"))


class Bombs(unittest.TestCase):
    def test_rows_count_tiers_switches_and_spare(self):
        rows = {row["guid"]: row for row in bpc.bomb_rows(load(pull()))}
        self.assertEqual(dict(rows[VOLLEY]["tiers"]), {"hunter": 2})
        self.assertEqual((rows[VOLLEY]["bombs"], rows[VOLLEY]["switches"], rows[VOLLEY]["spare"]), (2, 1, 0))
        self.assertEqual(dict(rows[ARCANE]["tiers"]), {"any": 1})
        self.assertEqual((rows[ARCANE]["bombs"], rows[ARCANE]["switches"], rows[ARCANE]["spare"]), (1, 0, 1))

    def test_section(self):
        self.assertEqual(printed(bpc.show_bombs, load(pull())), (
            "BOMBS\n"
            "  per ranged bot: assignments by tier, bombs given, bomb-to-bomb switches, spare spans\n"
            "\n"
            "    Volley  hunter 2  bombs 2  switches 1  spare 0\n"
            "    Arcane  any 1  bombs 1  switches 0  spare 1\n"))


class Spread(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(bpc.show_spread, load(pull())), (
            "SPREAD\n"
            "  per bot and ring: slots taken, of them nudged off a Shock Vortex or blocked\n"
            "\n"
            "    Arcane  vortex 1 (nudged 1, blocked 0)  ranged 2 (nudged 0, blocked 0)\n"
            "    Shiv  vortex 1 (nudged 0, blocked 0)\n"))


class Skull(unittest.TestCase):
    def test_placements_merge_across_tanks(self):
        self.assertEqual(printed(bpc.show_skull, load(pull())), (
            "SKULL\n"
            "  each placement, merged across the tanks that made it\n"
            "\n"
            "     0:00.200   0:45.200  empowered Prince Valanar\n"
            "     0:45.400   0:45.700  empowered Prince Taldaram from Prince Valanar\n"
            "     1:31.100   0:28.900  empowered Prince Keleseth from Prince Taldaram\n"))


class Cheats(unittest.TestCase):
    def test_other_encounters_keys_are_pointed_elsewhere(self):
        self.assertEqual(printed(bpc.show_cheats, load(pull())), (
            "CHEATS\n"
            "  no cheat of this encounter fired\n"
            "  2 use(s) under other encounters' keys: icecrown_citadel.py --cheats\n"))


class Banner(unittest.TestCase):
    def test_every_probe_present(self):
        self.assertIn("all bpc.* probes present", printed(bpc.show_banner, load(pull())))

    def test_a_missing_probe_is_named(self):
        records = [rec for rec in pull() if rec.get("k") != "bpc.bomb"]
        self.assertIn("probes absent from this trace: bpc.bomb", printed(bpc.show_banner, load(records)))

    def test_trash_pull_is_refused(self):
        self.assertIn("not a Blood Prince Council pull", printed(bpc.show_phases, load(pull("blood-prince-trash"))))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            bpc.show_banner(trace)
            for _, _, section in bpc.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Blood Prince Council pull"), len(bpc.SECTIONS))


if __name__ == "__main__":
    unittest.main()
