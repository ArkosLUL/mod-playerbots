"""Tests for the Svalna gauntlet reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25N pull pins each section, and the shared v14 fixture shows every section reads empty on
a trace that is not the gauntlet rather than raising.
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

from bosses import svalna_gauntlet as sgt  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
DEATHBRINGER = CREATURE | 980
FROSTBINDER = CREATURE | 981
WARLORD = CREATURE | 982
MAIDEN = CREATURE | 983
CORRUPTED = CREATURE | 984
BULWARK = 7401  # tank
MEND = 7402     # healer
SHIV = 7403     # melee
ARCANE = 7404   # ranged

ROSTER = ((BULWARK, "Bulwark", "tank", 0), (MEND, "Mend", "heal", 0), (SHIV, "Shiv", "melee", 0),
          (ARCANE, "Arcane", "ranged", 0))


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def unit(guid: int, entry: int, name: str) -> dict:
    return {"t": 1, "e": "unit", "g": guid, "en": entry, "n": name}


def pull(boss: str = "svalna-gauntlet") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": 1, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": human}
                    for guid, name, role, human in ROSTER]},
        {"t": 0, "e": "pull", "boss": boss, "src": "mark"},
        unit(DEATHBRINGER, 38125, "Ymirjar Deathbringer"),
        unit(FROSTBINDER, 37127, "Ymirjar Frostbinder"),
        unit(WARLORD, 37133, "Ymirjar Warlord"),
        unit(MAIDEN, 37132, "Ymirjar Battle-Maiden"),
        unit(CORRUPTED, 38184, "Corrupted Ymirjar"),
        # Arcane opens on the Deathbringer and Shiv on the Frostbinder; Bulwark rescues Mend from the Warlord.
        note(1000, ARCANE, "ss.gauntlet.target", "deathbringer"),
        note(1000, SHIV, "ss.gauntlet.target", "frostbinder"),
        note(9000, ARCANE, "ss.gauntlet.target", "battlemaiden"),
        note(2000, BULWARK, "ss.gauntlet.target", "rescue"),
        note(2500, BULWARK, "ss.gauntlet.target", "held"),
        note(2000, BULWARK, "ss.gauntlet.taunt", "warlord"),
        note(2400, BULWARK, "ss.gauntlet.taunt", "none"),
        # Shiv stuns a Summon Undead and Bulwark a Frozen Orb channel; Arcane kicks a bolt later.
        note(5000, SHIV, "ss.gauntlet.interrupt", "summon"),
        note(5300, SHIV, "ss.gauntlet.interrupt", "none"),
        note(6000, BULWARK, "ss.gauntlet.interrupt", "orb"),
        note(6200, BULWARK, "ss.gauntlet.interrupt", "none"),
        note(7000, ARCANE, "ss.gauntlet.interrupt", "bolt"),
        note(7200, ARCANE, "ss.gauntlet.interrupt", "none"),
        # Shiv leaves the Frozen Orb, Mend a Whirlwind.
        note(10000, SHIV, "ss.gauntlet.dodge", "orb"),
        note(11000, SHIV, "ss.gauntlet.dodge", "none"),
        note(12000, MEND, "ss.gauntlet.dodge", "whirlwind"),
        note(13000, MEND, "ss.gauntlet.dodge", "none"),
        note(14000, BULWARK, "ss.gauntlet.defensive", "idle"),
        note(15000, BULWARK, "ss.gauntlet.defensive", "shield wall"),
        note(16000, BULWARK, "ss.gauntlet.defensive", "idle"),
        note(17000, SHIV, "ss.gauntlet.humancall", "Shield Wall"),
        {"t": 20000, "e": "death", "g": DEATHBRINGER, "killer": ARCANE},
        {"t": 22000, "e": "death", "g": MAIDEN, "killer": ARCANE},
        {"t": 30000, "e": "end", "out": "idle"},
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


class Packs(unittest.TestCase):
    def test_rows(self):
        self.assertEqual(sgt.pack_rows(load(pull())), [
            ("Deathbringer", 1, 1), ("Frostbinder", 1, 0), ("Battle-Maiden", 1, 1), ("Warlord", 1, 0),
            ("Corrupted Ymirjar", 1, 0)])

    def test_section(self):
        self.assertEqual(printed(sgt.show_packs, load(pull())), (
            "PACKS\n"
            "    kind                seen  died\n"
            "    Deathbringer           1     1\n"
            "    Frostbinder            1     0\n"
            "    Battle-Maiden          1     1\n"
            "    Warlord                1     0\n"
            "    Corrupted Ymirjar      1     0\n"))


class Targets(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(sgt.show_targets, load(pull())), (
            "TARGETS\n"
            "  per bot: spans of each resolver rule\n"
            "\n"
            "    Arcane          battlemaiden 1, deathbringer 1\n"
            "    Bulwark         held 1, rescue 1\n"
            "    Shiv            frostbinder 1\n"))


class Taunts(unittest.TestCase):
    def test_none_left_out(self):
        self.assertEqual(printed(sgt.show_taunts, load(pull())), (
            "TAUNTS\n"
            "  per tank: spans of each rescue taunt it owed\n"
            "\n"
            "    Bulwark         warlord 1\n"))


class Kicks(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(sgt.show_kicks, load(pull())), (
            "KICKS\n"
            "  per bot: spans of each cast it was to stun or kick\n"
            "\n"
            "    Arcane          bolt 1\n"
            "    Bulwark         orb 1\n"
            "    Shiv            summon 1\n"))


class Dodges(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(sgt.show_dodges, load(pull())), (
            "DODGES\n"
            "  per bot: spans of each dodge rule\n"
            "\n"
            "    Mend            whirlwind 1\n"
            "    Shiv            orb 1\n"))

    def test_never_written(self):
        records = [rec for rec in pull() if rec.get("k") != "ss.gauntlet.dodge"]
        self.assertEqual(printed(sgt.show_dodges, load(records)), "DODGES\n  ss.gauntlet.dodge never written\n")


class Defensives(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(sgt.show_defensives, load(pull())), (
            "DEFENSIVES\n"
            "    Bulwark         shield wall 0:15.000\n"
            "  calls to a human tank  Shield Wall 0:17.000\n"))


class Cheats(unittest.TestCase):
    def test_none(self):
        self.assertEqual(printed(sgt.show_cheats, load(pull())), "CHEATS\n  no cheat fired\n")


class Banner(unittest.TestCase):
    def test_every_probe_present(self):
        self.assertIn("all ss.gauntlet.* probes present", printed(sgt.show_banner, load(pull())))

    def test_a_missing_probe_is_named(self):
        records = [rec for rec in pull() if rec.get("k") != "ss.gauntlet.taunt"]
        self.assertIn("probes absent from this trace: ss.gauntlet.taunt", printed(sgt.show_banner, load(records)))

    def test_her_own_pull_is_refused(self):
        self.assertIn("not a Svalna gauntlet pull", printed(sgt.show_banner, load(pull("sister-svalna"))))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            sgt.show_banner(trace)
            for _, _, section in sgt.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Svalna gauntlet pull"), len(sgt.SECTIONS))


if __name__ == "__main__":
    unittest.main()
