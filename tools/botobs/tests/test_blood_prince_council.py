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
MEND = 7506     # healer

ROSTER = ((BULWARK, "Bulwark", "tank"), (AEGIS, "Aegis", "tank"), (VOLLEY, "Volley", "ranged"),
          (ARCANE, "Arcane", "ranged"), (SHIV, "Shiv", "melee"), (MEND, "Mend", "healer"))


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
        {"t": 1, "e": "spell", "sp": 871, "n": "Shield Wall"},
        note(100, BULWARK, "bpc.phase", "valanar"),
        note(120, SHIV, "bpc.phase", "valanar"),
        note(45000, SHIV, "bpc.phase", "none"),
        note(45200, SHIV, "bpc.phase", "taldaram"),
        note(45300, BULWARK, "bpc.phase", "taldaram"),
        note(91000, BULWARK, "bpc.phase", "keleseth"),
        # Two tanks: Bulwark owns Valanar and Taldaram, Aegis Keleseth.
        note(50, BULWARK, "bpc.tank", "5"),
        note(50, AEGIS, "bpc.tank", "2"),
        # Bulwark pulls Taldaram back first; Shiv follows a player's skull for a second.
        note(100, BULWARK, "bpc.target", "recover"),
        note(2000, BULWARK, "bpc.target", "owned"),
        note(100, SHIV, "bpc.target", "empowered"),
        note(20000, SHIV, "bpc.target", "skull"),
        note(21000, SHIV, "bpc.target", "empowered"),
        note(100, VOLLEY, "bpc.target", "empowered"),
        note(20100, VOLLEY, "bpc.target", "bomb"),
        note(21500, VOLLEY, "bpc.target", "empowered"),
        # Arcane moves to Keleseth's side, Mend keeps its node, Shiv holds one through the cast only.
        note(100, ARCANE, "bpc.slot", "13"),
        note(45300, ARCANE, "bpc.slot", "21"),
        note(100, MEND, "bpc.slot", "20"),
        note(15000, SHIV, "bpc.slot", "8"),
        note(19500, SHIV, "bpc.slot", "0"),
        note(30000, SHIV, "bpc.dodge", "melee"),
        note(31000, SHIV, "bpc.dodge", "none"),
        # Volley juggles the first bomb, then the second; Arcane takes the first once Volley moves on.
        note(20000, VOLLEY, "bpc.bomb", str(BOMB_A)),
        note(22000, VOLLEY, "bpc.bomb", str(BOMB_B)),
        note(40000, VOLLEY, "bpc.bomb", "0"),
        note(23000, ARCANE, "bpc.bomb", str(BOMB_A)),
        note(30000, ARCANE, "bpc.bomb", "0"),
        note(20000, VOLLEY, "bpc.juggle", "wait"),
        note(20100, VOLLEY, "bpc.juggle", "shoot"),
        note(21500, VOLLEY, "bpc.juggle", "wait"),
        note(60000, ARCANE, "bpc.ball", "plain step"),
        note(61000, ARCANE, "bpc.ball", "plain clear"),
        note(95000, MEND, "bpc.ball", "inferno none"),
        note(95000, SHIV, "bpc.soak", "1"),
        note(97000, SHIV, "bpc.soak", "0"),
        note(11000, AEGIS, "bpc.nucleus", "taunt 0"),
        note(12000, AEGIS, "bpc.nucleus", "cooldown"),
        note(22000, AEGIS, "bpc.nucleus", "taunt 1"),
        note(30000, AEGIS, "bpc.nucleus", "walk"),
        note(33000, AEGIS, "bpc.nucleus", "full"),
        # Keleseth's empowered Shadow Lance on Aegis: Shield Wall inside the window, then a human tank's call
        note(91200, AEGIS, "bpc.defensive", "Shield Wall"),
        {"t": 91300, "e": "cast", "s": AEGIS, "d": AEGIS, "sp": 871},
        note(92000, AEGIS, "bpc.defensive", "idle"),
        note(93000, BULWARK, "bpc.humancall", "871"),
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


class Targets(unittest.TestCase):
    def test_section_counts_branches_in_resolver_order(self):
        self.assertEqual(printed(bpc.show_targets, load(pull())), (
            "TARGETS\n"
            "  per bot: spans of each resolver branch\n"
            "\n"
            "    Bulwark  owned 1, recover 1\n"
            "    Shiv  skull 1, empowered 2\n"
            "    Volley  bomb 1, empowered 2\n"))


class Tanks(unittest.TestCase):
    def test_masks_read_as_princes(self):
        self.assertEqual(printed(bpc.show_tanks, load(pull())), (
            "TANKS\n"
            "  per tank: the princes it owned, from when\n"
            "\n"
            "    Aegis  keleseth 0:00.050\n"
            "    Bulwark  valanar+taldaram 0:00.050\n"))

    def test_zero_mask_reads_none(self):
        self.assertEqual(bpc.prince_names("0"), "none")


class Spread(unittest.TestCase):
    def test_rows_count_moves_releases_and_dodges(self):
        rows = {row["guid"]: row for row in bpc.spread_rows(load(pull()))}
        self.assertEqual((rows[ARCANE]["nodes"], rows[ARCANE]["moves"], rows[ARCANE]["released"]), (2, 1, 0))
        self.assertEqual((rows[SHIV]["nodes"], rows[SHIV]["moves"], rows[SHIV]["released"]), (1, 0, 1))
        self.assertEqual(dict(rows[SHIV]["dodges"]), {"melee": 1})

    def test_section(self):
        self.assertEqual(printed(bpc.show_spread, load(pull())), (
            "SPREAD\n"
            "  per bot: nodes held, node-to-node moves, releases, vortex dodges\n"
            "\n"
            "    Arcane  nodes 2  moves 1  released 0  dodges -\n"
            "    Mend  nodes 1  moves 0  released 0  dodges -\n"
            "    Shiv  nodes 1  moves 0  released 1  dodges melee 1\n"))


class Bombs(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(bpc.show_bombs, load(pull())), (
            "BOMBS\n"
            "  per duty bot: bombs juggled, bomb-to-bomb switches, shot windows\n"
            "\n"
            "    Volley  bombs 2  switches 1  shots 1\n"
            "    Arcane  bombs 1  switches 0  shots 0\n"))


class Balls(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(bpc.show_balls, load(pull())), (
            "BALLS\n"
            "  per bot: ball victim spans by kind and answer, Inferno Flame soaks\n"
            "\n"
            "    Arcane  plain clear 1, plain step 1  soaks 0\n"
            "    Mend  inferno none 1  soaks 0\n"
            "    Shiv  -  soaks 1\n"))


class Nucleus(unittest.TestCase):
    def test_section_groups_taunts_by_word(self):
        self.assertEqual(printed(bpc.show_nucleus, load(pull())), (
            "NUCLEUS\n"
            "  per tank: spans of each nucleus answer\n"
            "\n"
            "    Aegis  taunt 2, walk 1, cooldown 1, full 1\n"))


class Defensives(unittest.TestCase):
    def test_window_cast_and_call(self):
        self.assertEqual(printed(bpc.show_defensives, load(pull())), (
            "DEFENSIVES\n"
            "  1:31.200  Aegis       0:00.800  offered Shield Wall; cast Shield Wall +0.1s\n"
            "  calls to a human tank  871 1:33.000\n"))

    def test_no_window(self):
        records = [rec for rec in pull() if rec.get("k") != "bpc.defensive"]
        self.assertIn("bpc.defensive never offered a button", printed(bpc.show_defensives, load(records)))


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
