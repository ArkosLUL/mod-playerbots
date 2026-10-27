"""Tests for the Sindragosa reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25H pull pins each section, and the shared v14 fixture shows every section reads empty on
a trace that is not Sindragosa rather than raising.
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

from bosses import sindragosa as sg  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
SINDRAGOSA = CREATURE | 950
TOMB1 = CREATURE | 951
TOMB2 = CREATURE | 952
TOMB3 = CREATURE | 953
BULWARK = 7401  # main tank
MEND = 7402     # healer
SHIV = 7403     # melee
ARCANE = 7404   # ranged
ARKOS = 7405    # human melee

ROSTER = ((BULWARK, "Bulwark", "tank", 0), (MEND, "Mend", "heal", 0), (SHIV, "Shiv", "melee", 0),
          (ARCANE, "Arcane", "ranged", 0), (ARKOS, "Arkos", "melee", 1))


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def cheat(when: int, guid: int, key: str, uses: int, target: int | None = None) -> dict:
    rec = {"t": when, "e": "cheat", "g": guid, "k": key, "n": uses}
    if target:
        rec["tgt"] = target
    return rec


def pull(boss: str = "sindragosa", diff: int = 3) -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": diff, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 1},
         "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": human}
                    for guid, name, role, human in ROSTER]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        {"t": 1, "e": "unit", "g": SINDRAGOSA, "en": 36853, "n": "Sindragosa", "lvl": 83, "b": 1},
        {"t": 1, "e": "unit", "g": TOMB1, "en": 36980, "n": "Ice Tomb"},
        {"t": 1, "e": "unit", "g": TOMB2, "en": 36980, "n": "Ice Tomb"},
        {"t": 1, "e": "unit", "g": TOMB3, "en": 36980, "n": "Ice Tomb"},
        note(100, BULWARK, "sg.phase", "ground"),
        note(110, MEND, "sg.phase", "ground"),
        note(50000, BULWARK, "sg.phase", "air"),
        note(50100, MEND, "sg.phase", "air"),
        note(92000, BULWARK, "sg.phase", "ground"),
        note(200000, BULWARK, "sg.phase", "last"),
        note(1000, BULWARK, "sg.skull", f"boss {SINDRAGOSA}"),
        # The air phase: groups, tomb picks, the hide rules and the red flares.
        note(56000, SHIV, "sg.flaredred", "0"),
        note(56500, ARCANE, "sg.flaredred", "0 2"),
        note(57000, SHIV, "sg.group", "0"),
        note(57000, ARCANE, "sg.group", "1"),
        note(57000, MEND, "sg.group", "2"),
        note(55000, SHIV, "sg.losmove", "wait"),
        note(58000, MEND, "sg.losmove", "zone"),
        note(60000, SHIV, "sg.tomb", str(TOMB1)),
        note(60000, ARCANE, "sg.tomb", str(TOMB2)),
        note(60000, SHIV, "sg.losmove", "hide"),
        note(61000, SHIV, "sg.losmove", "parked"),
        note(61000, SHIV, "sg.skull", f"extra {TOMB1}"),
        note(62000, ARCANE, "sg.losmove", "tombed"),
        note(62000, ARCANE, "sg.cross", f"sticky {TOMB2}"),
        note(70000, ARCANE, "sg.freedfallback", "1"),
        note(70000, ARCANE, "sg.losmove", "fallback"),
        note(75000, ARCANE, "sg.freedfallback", "0"),
        note(75000, ARCANE, "sg.losmove", "hide"),
        note(80000, MEND, "sg.star", "clear"),
        note(92000, SHIV, "sg.losmove", "none"),
        note(160000, SHIV, "sg.group", "1"),
        # The last phase: the blue flare and the kill skull.
        note(205000, MEND, "sg.flaredblue", "1"),
        note(210000, BULWARK, "sg.skull", f"kill {TOMB3}"),
        cheat(56000, SHIV, "sg.flares", 2),
        cheat(60000, BULWARK, "sg.magicbarrier", 3),
        cheat(61000, BULWARK, "sg.heroicbuff", 4, ARKOS),
        cheat(120000, BULWARK, "lk.hoj", 1, TOMB1),
        {"t": 300000, "e": "end", "out": "kill"},
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
        self.assertEqual(printed(sg.show_phases, load(pull())), (
            "PHASES\n"
            "  sg.phase, each step from the first bot to note it\n"
            "\n"
            "         from       held  phase\n"
            "     0:00.100   0:49.900  ground\n"
            "     0:50.000   0:42.000  air\n"
            "     1:32.000   1:48.000  ground\n"
            "     3:20.000   1:40.000  last\n"))


class Tombs(unittest.TestCase):
    def test_hides_count_switches_inside_each_air_phase(self):
        rows = sg.air_hides(load(pull()))
        self.assertEqual(rows, [{"start": 50000, "stop": 92000,
                                 "rules": {"hide": 2, "parked": 1, "zone": 1, "fallback": 1, "tombed": 1,
                                           "wait": 1}}])

    def test_section(self):
        self.assertEqual(printed(sg.show_tombs, load(pull())), (
            "TOMBS\n"
            "  sg.group, each bot's last group\n"
            "    1  2: Arcane, Shiv\n"
            "    2  1: Mend\n"
            "  sg.tomb: 2 tomb pick(s) over 2 bot(s)\n"
            "  freed from a tomb and hiding behind the nearest: Arcane\n"
            "\n"
            "  sg.losmove switches per air phase\n"
            "     0:50.000   0:42.000  hide 2, parked 1, zone 1, fallback 1, tombed 1, wait 1\n"))


class Icons(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(sg.show_icons, load(pull())), (
            "ICONS\n"
            "     0:01.000  skull  Bulwark         boss Sindragosa\n"
            "     1:01.000  skull  Shiv            extra Ice Tomb\n"
            "     1:02.000  cross  Arcane          sticky Ice Tomb\n"
            "     1:20.000  star   Mend            clear\n"
            "     3:30.000  skull  Bulwark         kill Ice Tomb\n"))


class Flares(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(sg.show_flares, load(pull())), (
            "FLARES\n"
            "     0:56.000  red   Shiv            0\n"
            "     0:56.500  red   Arcane          0 2\n"
            "     3:25.000  blue  Mend            1\n"))


class Cheats(unittest.TestCase):
    def test_own_keys(self):
        self.assertEqual(printed(sg.show_cheats, load(pull())), (
            "CHEATS\n"
            "  sg.flares  2 use(s) from 0:56.000\n"
            "    by  Shiv 2\n"
            "    on  self\n"
            "  sg.magicbarrier  3 use(s) from 1:00.000\n"
            "    by  Bulwark 3\n"
            "    on  self\n"
            "  sg.heroicbuff  4 use(s) from 1:01.000\n"
            "    by  Bulwark 4\n"
            "    on  Arkos human\n"
            "  1 use(s) under other encounters' keys: icecrown_citadel.py --cheats\n"))


class Banner(unittest.TestCase):
    def test_every_probe_present(self):
        self.assertIn("all sg.* probes present", printed(sg.show_banner, load(pull())))

    def test_ten_man_spares_the_star(self):
        records = [rec for rec in pull(diff=2) if rec.get("k") != "sg.star"]
        self.assertIn("all sg.* probes present", printed(sg.show_banner, load(records)))

    def test_a_missing_probe_is_named(self):
        records = [rec for rec in pull() if rec.get("k") != "sg.losmove"]
        self.assertIn("probes absent from this trace: sg.losmove", printed(sg.show_banner, load(records)))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            sg.show_banner(trace)
            for _, _, section in sg.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Sindragosa pull"), len(sg.SECTIONS))


if __name__ == "__main__":
    unittest.main()
