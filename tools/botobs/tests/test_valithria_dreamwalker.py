"""Tests for the Valithria Dreamwalker reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25H pull pins each section, and the shared v14 fixture shows every section reads empty on
a trace that is not Valithria rather than raising.
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

from bosses import valithria_dreamwalker as vd  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
VALITHRIA = CREATURE | 900
PORTAL1 = CREATURE | 901
PORTAL2 = CREATURE | 902
CLOUD1 = CREATURE | 903
CLOUD2 = CREATURE | 904
SKELETON = CREATURE | 905
ZOMBIE = CREATURE | 906
ARCHMAGE = CREATURE | 907
BULWARK = 7401  # tank
MEND = 7402     # healer, takes portals
LUMEN = 7403    # healer, heals the raid until the second wave
SHIV = 7404     # melee
ARCANE = 7405   # ranged
ARKOS = 7406    # human healer

ROSTER = ((BULWARK, "Bulwark", "tank", 0), (MEND, "Mend", "heal", 0), (LUMEN, "Lumen", "heal", 0),
          (SHIV, "Shiv", "melee", 0), (ARCANE, "Arcane", "ranged", 0), (ARKOS, "Arkos", "heal", 1))


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def dream(when: int, guid: int, removed: int = 0) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": guid, "sp": 70766, "r": removed, "p": 1}


def unit(guid: int, entry: int, name: str) -> dict:
    return {"t": 1, "e": "unit", "g": guid, "en": entry, "n": name}


def pull(boss: str = "valithria-dreamwalker", diff: int = 3) -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": diff, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "priest", "h": human}
                    for guid, name, role, human in ROSTER]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        unit(VALITHRIA, 36789, "Valithria Dreamwalker"),
        unit(PORTAL1, 38429, "Nightmare Portal (Pre-effect)"),
        unit(PORTAL2, 38429, "Nightmare Portal (Pre-effect)"),
        unit(CLOUD1, 38421, "Nightmare Cloud"),
        unit(CLOUD2, 38421, "Nightmare Cloud"),
        unit(SKELETON, 36791, "Blazing Skeleton"),
        unit(ZOMBIE, 37934, "Blistering Zombie"),
        unit(ARCHMAGE, 37868, "Risen Archmage"),
        note(100, MEND, "vd.raidheal", "boss"),
        note(100, LUMEN, "vd.raidheal", "raid"),
        note(90000, LUMEN, "vd.raidheal", "boss"),
        # The first wave: Mend claims and goes in beside Arkos, a human, so it follows him.
        note(46000, MEND, "vd.wave", "pre"),
        note(46010, LUMEN, "vd.wave", "pre"),
        {"t": 46100, "e": "note", "g": MEND, "k": "vd.portalclaim", "txt": str(PORTAL1)},
        note(61000, MEND, "vd.wave", "open"),
        dream(61500, MEND),
        dream(62000, ARKOS),
        note(62100, MEND, "vd.cloud", f"human {ARKOS}"),
        note(71000, MEND, "vd.wave", "none"),
        {"t": 72000, "e": "note", "g": MEND, "k": "vd.portalclaim", "txt": "0"},
        dream(81500, MEND, 1),
        dream(82000, ARKOS, 1),
        # The second wave: Lumen goes in alone and walks the bot cloud sync.
        note(92000, LUMEN, "vd.wave", "pre"),
        {"t": 92100, "e": "note", "g": LUMEN, "k": "vd.portalclaim", "txt": str(PORTAL2)},
        note(107000, LUMEN, "vd.wave", "open"),
        dream(107500, LUMEN),
        note(108000, LUMEN, "vd.cloud", f"cloud {CLOUD1}"),
        note(109000, LUMEN, "vd.cloud", f"cloud {CLOUD2}"),
        note(115000, LUMEN, "vd.cloud", "none"),
        note(117000, LUMEN, "vd.wave", "none"),
        {"t": 120000, "e": "death", "g": LUMEN, "killer": VALITHRIA},
        # Marks: the skull goes to a skeleton, is cleared, then takes a zombie off the other half.
        note(20000, BULWARK, "vd.group", "skull"),
        note(20000, SHIV, "vd.group", "cross"),
        note(20000, ARCANE, "vd.group", "single"),
        note(30000, ARCANE, "vd.group", "cross"),
        note(30000, BULWARK, "vd.skull", f"skeleton {SKELETON}"),
        note(35000, ARCANE, "vd.cross", f"archmage {ARCHMAGE}"),
        note(40000, SHIV, "vd.skull", "clear"),
        note(50000, BULWARK, "vd.skull", f"zombie other {ZOMBIE}"),
        {"t": 62200, "e": "cheat", "g": MEND, "k": "vd.dreamteleport", "n": 12, "tgt": ARKOS},
        {"t": 108100, "e": "cheat", "g": LUMEN, "k": "vd.cloudteleport", "n": 5, "tgt": CLOUD1},
        {"t": 130000, "e": "cheat", "g": BULWARK, "k": "lk.hoj", "n": 2, "tgt": SKELETON},
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


class Waves(unittest.TestCase):
    def test_rows_join_claims_and_dream_state(self):
        rows = vd.waves(load(pull()))
        self.assertEqual([(row["pre"], row["open"], row["stop"], row["claims"], row["dreamers"]) for row in rows], [
            (46000, 61000, 71000, [MEND], [MEND, ARKOS]),
            (92000, 107000, 117000, [LUMEN], [LUMEN]),
        ])

    def test_section(self):
        self.assertEqual(printed(vd.show_waves, load(pull())), (
            "WAVES\n"
            "  2 wave(s); pre and open from the first healer to note each\n"
            "\n"
            "          pre       open        end  claimed / went in\n"
            "     0:46.000   1:01.000   1:11.000  Mend / Mend, Arkos\n"
            "     1:32.000   1:47.000   1:57.000  Lumen / Lumen\n"))

    def test_a_wave_first_noted_open(self):
        records = [rec for rec in pull() if not (rec.get("k") == "vd.wave" and rec.get("txt") == "pre")]
        rows = vd.waves(load(records))
        self.assertEqual([(row["pre"], row["open"]) for row in rows], [(None, 61000), (None, 107000)])


class Dream(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(vd.show_dream, load(pull())), (
            "DREAM\n"
            "  3 Dream State window(s)\n"
            "\n"
            "     1:01.500   0:20.000  Mend\n"
            "     1:02.000   0:20.000  Arkos human\n"
            "     1:47.500   3:12.500  Lumen  died\n"
            "\n"
            "  vd.cloud: cloud 2, human 1, none 1\n"))


class Heal(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(vd.show_heal, load(pull())), (
            "HEAL\n"
            "  vd.raidheal per healer, each change\n"
            "\n"
            "    Lumen           raid@0:00.100, boss@1:30.000\n"
            "    Mend            boss@0:00.100\n"))


class Marks(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(vd.show_marks, load(pull())), (
            "MARKS\n"
            "     0:30.000  skull  Bulwark         skeleton Blazing Skeleton\n"
            "     0:35.000  cross  Arcane          archmage Risen Archmage\n"
            "     0:40.000  skull  Shiv            clear\n"
            "     0:50.000  skull  Bulwark         zombie other Blistering Zombie\n"
            "\n"
            "  vd.group per bot, each change\n"
            "    Arcane          single@0:20.000, cross@0:30.000\n"
            "    Bulwark         skull@0:20.000\n"
            "    Shiv            cross@0:20.000\n"))

    def test_ten_man_spares_the_group(self):
        records = [rec for rec in pull(diff=2) if rec.get("k") not in ("vd.group", "vd.cross")]
        trace = load(records)
        self.assertNotIn("vd.group", printed(vd.show_marks, trace))
        self.assertIn("all vd.* probes present", printed(vd.show_banner, trace))


class Cheats(unittest.TestCase):
    def test_own_keys(self):
        self.assertEqual(printed(vd.show_cheats, load(pull())), (
            "CHEATS\n"
            "  vd.dreamteleport  12 use(s) from 1:02.200\n"
            "    by  Mend 12\n"
            "    on  Arkos human\n"
            "  vd.cloudteleport  5 use(s) from 1:48.100\n"
            "    by  Lumen 5\n"
            "    on  Nightmare Cloud\n"
            "  2 use(s) under other encounters' keys: icecrown_citadel.py --cheats\n"))


class Banner(unittest.TestCase):
    def test_every_probe_present(self):
        self.assertIn("all vd.* probes present", printed(vd.show_banner, load(pull())))

    def test_a_missing_probe_is_named(self):
        records = [rec for rec in pull() if rec.get("k") != "vd.wave"]
        self.assertIn("probes absent from this trace: vd.wave", printed(vd.show_banner, load(records)))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            vd.show_banner(trace)
            for _, _, section in vd.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Valithria Dreamwalker pull"), len(vd.SECTIONS))


if __name__ == "__main__":
    unittest.main()
