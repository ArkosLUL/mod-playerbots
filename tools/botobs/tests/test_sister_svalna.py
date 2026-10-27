"""Tests for the Sister Svalna reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25N pull pins each section, and the shared v14 fixture shows every section reads empty on
a trace that is not Svalna rather than raising.
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

from bosses import sister_svalna as ss  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
SVALNA = CREATURE | 970
BULWARK = 7401  # tank
MEND = 7402     # healer
SHIV = 7403     # melee
ARCANE = 7404   # ranged

ROSTER = ((BULWARK, "Bulwark", "tank", 0), (MEND, "Mend", "heal", 0), (SHIV, "Shiv", "melee", 0),
          (ARCANE, "Arcane", "ranged", 0))


def note(when: int, guid: int, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": "ss.shield", "txt": value}


def impale(when: int, guid: int, removed: int = 0) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": SVALNA, "sp": 71443, "r": removed}


def pull(boss: str = "sister-svalna") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": 1, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": human}
                    for guid, name, role, human in ROSTER]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        {"t": 1, "e": "unit", "g": SVALNA, "en": 37126, "n": "Sister Svalna", "lvl": 82},
        note(100, BULWARK, "down"),
        note(110, SHIV, "down"),
        # Arcane is impaled and freed; Shiv throws the spear that breaks the shield.
        impale(19000, ARCANE),
        note(20000, BULWARK, "up"),
        impale(24000, ARCANE, 1),
        {"t": 24900, "e": "cast", "s": SHIV, "sp": 71466, "tgt": SVALNA, "tr": 1},
        note(25000, SHIV, "down"),
        note(25100, BULWARK, "down"),
        # Mend is never freed and dies on the spear; the shield holds to the end.
        impale(44000, MEND),
        note(45000, BULWARK, "up"),
        {"t": 50000, "e": "death", "g": MEND, "killer": SVALNA},
        {"t": 60000, "e": "end", "out": "wipe"},
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


class Shield(unittest.TestCase):
    def test_spans_merge_across_bots(self):
        self.assertEqual(ss.shields(load(pull())), [(20000, 25000), (45000, 60000)])

    def test_section(self):
        self.assertEqual(printed(ss.show_shield, load(pull())), (
            "SHIELD\n"
            "  2 shield(s), from the first bot to note each edge\n"
            "\n"
            "     0:20.000  held  0:05.000\n"
            "     0:45.000  held  0:15.000\n"))


class Spears(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(ss.show_spears, load(pull())), (
            "SPEARS\n"
            "  2 impale(s)\n"
            "\n"
            "     0:19.000   0:05.000  Arcane\n"
            "     0:44.000   0:16.000  Mend  died\n"
            "\n"
            "  Hurl Spear casts\n"
            "     0:24.900  Shiv            on Sister Svalna\n"))


class Cheats(unittest.TestCase):
    def test_none(self):
        self.assertEqual(printed(ss.show_cheats, load(pull())), "CHEATS\n  no cheat fired\n")

    def test_another_encounters_code(self):
        records = pull() + [{"t": 30000, "e": "cheat", "g": SHIV, "k": "vd.nitro", "n": 3}]
        self.assertEqual(printed(ss.show_cheats, load(records)),
                         "CHEATS\n  3 use(s), none of them Svalna's: icecrown_citadel.py --cheats\n")


class Banner(unittest.TestCase):
    def test_every_probe_present(self):
        self.assertIn("all ss.* probes present", printed(ss.show_banner, load(pull())))

    def test_a_missing_probe_is_named(self):
        records = [rec for rec in pull() if rec.get("k") != "ss.shield"]
        self.assertIn("probes absent from this trace: ss.shield", printed(ss.show_banner, load(records)))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ss.show_banner(trace)
            for _, _, section in ss.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Sister Svalna pull"), len(ss.SECTIONS))


if __name__ == "__main__":
    unittest.main()
