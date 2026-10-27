"""Tests for the Blood-Queen Lana'thel reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25N pull pins each section, and the shared v14 fixture shows every section reads empty on
a trace that is not the Blood-Queen rather than raising.
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

from bosses import blood_queen_lana_thel as bql  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
QUEEN = CREATURE | 950
BULWARK = 7601  # main tank
AEGIS = 7602    # assist tank
MEND = 7603     # healer
ARCANE = 7604   # ranged
SHIV = 7605     # melee
ARKOS = 7606    # human ranged

ROSTER = ((BULWARK, "Bulwark", "tank", 0), (AEGIS, "Aegis", "tank", 0), (MEND, "Mend", "heal", 0),
          (ARCANE, "Arcane", "ranged", 0), (SHIV, "Shiv", "melee", 0), (ARKOS, "Arkos", "ranged", 1))


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def aura(when: int, guid: int, spell: int, removed: int = 0) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": QUEEN, "sp": spell, "r": removed}


def pull(boss: str = "blood-queen-lana-thel") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": 1, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "mage", "h": human}
                    for guid, name, role, human in ROSTER]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        {"t": 1, "e": "unit", "g": QUEEN, "en": 37955, "n": "Blood-Queen Lana'thel", "lvl": 83, "b": 1},
        note(100, BULWARK, "bql.phase", "ground"),
        note(127000, BULWARK, "bql.phase", "center"),
        note(127100, MEND, "bql.phase", "center"),
        note(135000, MEND, "bql.phase", "air"),
        note(147000, BULWARK, "bql.phase", "ground"),
        # Shiv bites Arcane after one switch; Mend never gets a bite in and is mind controlled.
        aura(15000, SHIV, 71474),
        note(15100, SHIV, "bql.bite", f"dps {MEND}"),
        note(15500, SHIV, "bql.bite", f"dps {ARCANE}"),
        aura(18000, SHIV, 71474, 1),
        aura(18000, SHIV, 71525),
        note(18100, SHIV, "bql.bite", "none"),
        aura(75000, MEND, 71474),
        note(75100, MEND, "bql.bite", f"tank {AEGIS}"),
        aura(90000, MEND, 71474, 1),
        aura(90000, MEND, 70923),
        # One Pact: Arcane walks to Aegis, who holds, the human carries none of the probes.
        aura(20000, ARCANE, 71340),
        aura(20000, AEGIS, 71340),
        aura(20000, ARKOS, 71340),
        note(20100, ARCANE, "bql.pact", f"tank {AEGIS}"),
        note(20100, AEGIS, "bql.pact", "hold"),
        aura(24000, ARCANE, 71340, 1),
        aura(24000, AEGIS, 71340, 1),
        aura(24000, ARKOS, 71340, 1),
        note(24100, ARCANE, "bql.pact", "none"),
        # Swarming Shadows on Arcane: warning aura, then the trail; it switches lanes once.
        aura(30000, ARCANE, 71264),
        aura(32000, ARCANE, 71264, 1),
        aura(32000, ARCANE, 71265),
        note(30100, ARCANE, "bql.curve", "0"),
        note(33000, ARCANE, "bql.curve", "1"),
        aura(38000, ARCANE, 71265, 1),
        note(38200, ARCANE, "bql.curve", "0"),
        note(10, ARCANE, "bql.slot", "3"),
        note(40000, ARCANE, "bql.slot", "5"),
        note(135100, ARCANE, "bql.airslot", "12"),
        note(10, MEND, "bql.slot", "4"),
        {"t": 160000, "e": "end", "out": "kill"},
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
        self.assertEqual(printed(bql.show_phases, load(pull())), (
            "PHASES\n"
            "  bql.phase, each step from the first bot to note it\n"
            "\n"
            "         from       held  phase\n"
            "     0:00.100   2:06.900  ground\n"
            "     2:07.000   0:08.000  center\n"
            "     2:15.000   0:12.000  air\n"
            "     2:27.000   0:13.000  ground\n"))


class Bites(unittest.TestCase):
    def test_rows_read_picks_and_outcome(self):
        rows = bql.bite_rows(load(pull()))
        self.assertEqual([(row["biter"], row["start"], row["stop"], row["picks"], row["outcome"]) for row in rows], [
            (SHIV, 15000, 18000, [f"dps {MEND}", f"dps {ARCANE}"], "bit"),
            (MEND, 75000, 90000, [f"tank {AEGIS}"], "frenzy"),
        ])

    def test_section(self):
        self.assertEqual(printed(bql.show_bites, load(pull())), (
            "BITES\n"
            "  2 frenzy window(s): the biter, each pick in order, how it ended\n"
            "\n"
            "     0:15.000   0:03.000  Shiv  dps Mend -> dps Arcane  bit\n"
            "     1:15.000   0:15.000  Mend  tank Aegis  frenzy\n"))


class Pact(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(bql.show_pact, load(pull())), (
            "PACT\n"
            "  3 Pact member window(s): each member and the rules bql.pact noted for it\n"
            "\n"
            "     0:20.000   0:04.000  Aegis  hold\n"
            "     0:20.000   0:04.000  Arcane  tank Aegis\n"
            "     0:20.000   0:04.000  Arkos human  -\n"))


class Shadows(unittest.TestCase):
    def test_warning_and_trail_read_as_one_window(self):
        self.assertEqual(printed(bql.show_shadows, load(pull())), (
            "SHADOWS\n"
            "  1 Swarming Shadows: the victim and the lanes bql.curve noted inside\n"
            "\n"
            "     0:30.000   0:08.000  Arcane  lane 0 -> 1\n"))


class Slots(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(bql.show_slots, load(pull())), (
            "SLOTS\n"
            "  per bot: first slot and how many times it moved, on the ground and in the air\n"
            "\n"
            "    Arcane  ground 3 (moved 1)  air 12 (moved 0)\n"
            "    Mend  ground 4 (moved 0)\n"))


class Cheats(unittest.TestCase):
    def test_none_fired(self):
        self.assertEqual(printed(bql.show_cheats, load(pull())), "CHEATS\n  no cheat of this encounter fired\n")


class Banner(unittest.TestCase):
    def test_every_probe_present(self):
        self.assertIn("all bql.* probes present", printed(bql.show_banner, load(pull())))

    def test_a_missing_probe_is_named(self):
        records = [rec for rec in pull() if rec.get("k") != "bql.pact"]
        self.assertIn("probes absent from this trace: bql.pact", printed(bql.show_banner, load(records)))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            bql.show_banner(trace)
            for _, _, section in bql.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Blood-Queen Lana'thel pull"), len(bql.SECTIONS))


if __name__ == "__main__":
    unittest.main()
