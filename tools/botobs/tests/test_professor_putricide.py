"""Tests for the Professor Putricide reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25H pull pins each section, and the shared v14 fixture shows every section reads empty on
a trace that is not Putricide rather than raising.
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

from bosses import professor_putricide as pp  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
PUTRICIDE = CREATURE | 800
OOZE = CREATURE | 801
CLOUD = CREATURE | 802
BULWARK = 7401  # tank
MEND = 7402     # healer
SHIV = 7403     # melee
ARCANE = 7404   # ranged
ARKOS = 7405    # human melee

ROSTER = ((BULWARK, "Bulwark", "tank", 0), (MEND, "Mend", "heal", 0), (SHIV, "Shiv", "melee", 0),
          (ARCANE, "Arcane", "ranged", 0), (ARKOS, "Arkos", "melee", 1))


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def bloat(when: int, guid: int, removed: int = 0) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": CLOUD, "sp": 72455, "r": removed, "st": 10}


def pull(boss: str = "professor-putricide") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": 3, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 1},
         "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": human}
                    for guid, name, role, human in ROSTER]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        {"t": 1, "e": "unit", "g": PUTRICIDE, "en": 36678, "n": "Professor Putricide", "lvl": 83, "b": 1},
        {"t": 1, "e": "unit", "g": OOZE, "en": 37697, "n": "Volatile Ooze"},
        {"t": 1, "e": "unit", "g": CLOUD, "en": 37562, "n": "Gas Cloud"},
        note(100, BULWARK, "pp.phase", "1"),
        note(110, MEND, "pp.phase", "1"),
        note(90000, MEND, "pp.phase", "2"),
        note(90100, BULWARK, "pp.phase", "2"),
        note(200000, BULWARK, "pp.phase", "3"),
        # Arcane is bloated twice and runs the second one out of corners; Shiv dies to his.
        bloat(30000, ARCANE),
        note(30100, ARCANE, "pp.bloatflee", "open"),
        note(32000, ARCANE, "pp.bloatflee", "loose"),
        note(33000, ARCANE, "pp.bloatflee", "open"),
        bloat(40000, ARCANE, 1),
        note(40100, ARCANE, "pp.bloatflee", "none"),
        bloat(110000, SHIV),
        note(110100, SHIV, "pp.bloatflee", "corner"),
        note(111000, SHIV, "pp.bloatflee", "wedge"),
        {"t": 118000, "e": "death", "g": SHIV, "killer": CLOUD},
        bloat(118000, SHIV, 1),
        bloat(150000, ARKOS),
        # The ooze, then the cloud once it is dead, then the boss; Mend repeats the cloud placement.
        note(20000, BULWARK, "pp.skull", f"ooze {OOZE}"),
        note(60000, SHIV, "pp.skull", "clear"),
        note(60000, MEND, "pp.skull", f"cloud {CLOUD}"),
        note(61000, BULWARK, "pp.skull", f"cloud {CLOUD}"),
        note(100000, BULWARK, "pp.skull", f"boss {PUTRICIDE}"),
        {"t": 25000, "e": "cheat", "g": SHIV, "k": "pp.oozekill", "n": 1, "tgt": OOZE},
        {"t": 26000, "e": "cheat", "g": BULWARK, "k": "pp.heroicbuff", "n": 4, "tgt": ARKOS},
        {"t": 240000, "e": "end", "out": "kill"},
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
        self.assertEqual(printed(pp.show_phases, load(pull())), (
            "PHASES\n"
            "  pp.phase, each step from the first bot to note it\n"
            "\n"
            "         from       held  phase\n"
            "     0:00.100   1:29.900  1\n"
            "     1:30.000   1:50.000  2\n"
            "     3:20.000   0:40.000  3\n"))


class Bloat(unittest.TestCase):
    def test_windows_count_branches_inside_each(self):
        rows = pp.bloat_windows(load(pull()))
        self.assertEqual([(row["victim"], row["start"], row["stop"], row["died"], dict(row["flee"])) for row in rows], [
            (ARCANE, 30000, 40000, False, {"open": 2, "loose": 1}),
            (SHIV, 110000, 118000, True, {"corner": 1, "wedge": 1}),
            (ARKOS, 150000, 240000, False, {}),
        ])

    def test_section(self):
        self.assertEqual(printed(pp.show_bloat, load(pull())), (
            "BLOAT\n"
            "  3 bloat(s); flee counts the branch switches pp.bloatflee noted inside each\n"
            "\n"
            "     0:30.000   0:10.000  Arcane  open 2, loose 1\n"
            "     1:50.000   0:08.000  Shiv  corner 1, wedge 1  died\n"
            "     2:30.000   1:30.000  Arkos human  -\n"))


class Skull(unittest.TestCase):
    def test_placements_merge_across_bots(self):
        self.assertEqual(printed(pp.show_skull, load(pull())), (
            "SKULL\n"
            "  each placement, merged across the bots that made it\n"
            "\n"
            "     0:20.000   0:40.000  ooze Volatile Ooze\n"
            "     1:00.000   0:00.000  clear\n"
            "     1:00.000   0:40.000  cloud Gas Cloud\n"
            "     1:40.000   2:20.000  boss Professor Putricide\n"))


class Cheats(unittest.TestCase):
    def test_own_keys(self):
        self.assertEqual(printed(pp.show_cheats, load(pull())), (
            "CHEATS\n"
            "  pp.oozekill  1 use(s) from 0:25.000\n"
            "    by  Shiv 1\n"
            "    on  Volatile Ooze\n"
            "  pp.heroicbuff  4 use(s) from 0:26.000\n"
            "    by  Bulwark 4\n"
            "    on  Arkos human\n"))


class Banner(unittest.TestCase):
    def test_every_probe_present(self):
        self.assertIn("all pp.* probes present", printed(pp.show_banner, load(pull())))

    def test_a_missing_probe_is_named(self):
        records = [rec for rec in pull() if rec.get("k") != "pp.skull"]
        self.assertIn("probes absent from this trace: pp.skull", printed(pp.show_banner, load(records)))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            pp.show_banner(trace)
            for _, _, section in pp.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Professor Putricide pull"), len(pp.SECTIONS))


if __name__ == "__main__":
    unittest.main()
