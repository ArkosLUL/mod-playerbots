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
ABOMINATION = CREATURE | 907
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
        unit(ABOMINATION, 37886, "Gluttonous Abomination"),
        note(100, MEND, "vd.healer", "2"),
        note(100, LUMEN, "vd.healer", "1"),
        note(90000, LUMEN, "vd.healer", "2"),
        # The first wave: Mend claims and goes in beside Arkos, a human, so it follows him.
        note(46000, MEND, "vd.wave", "pre"),
        note(46010, LUMEN, "vd.wave", "pre"),
        {"t": 46100, "e": "note", "g": MEND, "k": "vd.portalclaim", "txt": str(PORTAL1)},
        note(61000, MEND, "vd.wave", "open"),
        dream(61500, MEND),
        dream(62000, ARKOS),
        note(61600, MEND, "vd.dream", "refresh"),
        note(62100, MEND, "vd.dream", "human"),
        note(78000, MEND, "vd.dream", "descend"),
        note(71000, MEND, "vd.wave", "none"),
        {"t": 72000, "e": "note", "g": MEND, "k": "vd.portalclaim", "txt": "0"},
        dream(81500, MEND, 1),
        dream(82000, ARKOS, 1),
        note(81600, MEND, "vd.dream", "none"),
        # The second wave: Lumen goes in alone and walks the shared clouds.
        note(92000, LUMEN, "vd.wave", "pre"),
        {"t": 92100, "e": "note", "g": LUMEN, "k": "vd.portalclaim", "txt": str(PORTAL2)},
        note(107000, LUMEN, "vd.wave", "open"),
        dream(107500, LUMEN),
        note(107600, LUMEN, "vd.dream", "refresh"),
        note(108500, LUMEN, "vd.dream", "cloud"),
        note(108000, LUMEN, "vd.cloud", f"cloud {CLOUD1}"),
        note(109000, LUMEN, "vd.cloud", f"cloud {CLOUD2}"),
        note(115000, LUMEN, "vd.cloud", "none"),
        note(117000, LUMEN, "vd.wave", "none"),
        {"t": 120000, "e": "death", "g": LUMEN, "killer": VALITHRIA},
        # Adds: Bulwark holds the abomination, Shiv and Arcane walk the tiers.
        note(5000, BULWARK, "vd.tank", str(ABOMINATION)),
        note(5100, BULWARK, "vd.target", "owned"),
        note(60000, BULWARK, "vd.tank", "0"),
        note(60100, BULWARK, "vd.target", "skeleton"),
        note(30000, SHIV, "vd.target", "skeleton"),
        note(40000, SHIV, "vd.target", "abomination"),
        note(30000, ARCANE, "vd.target", "skeleton"),
        note(40000, ARCANE, "vd.target", "zombie"),
        note(50000, ARCANE, "vd.target", "skull"),
        # Zombies: Arcane kites one, Shiv snares it, Mend steps out of a void and a column.
        note(1000, ARCANE, "vd.kite", "none"),
        note(41000, ARCANE, "vd.kite", "ring ccw"),
        note(44000, ARCANE, "vd.kite", "reverse"),
        note(46000, ARCANE, "vd.kite", "none"),
        note(41000, SHIV, "vd.snare", "chains of ice"),
        note(42000, SHIV, "vd.snare", "none"),
        note(1000, MEND, "vd.dodge", "none"),
        note(33000, MEND, "vd.dodge", "column"),
        note(34000, MEND, "vd.dodge", "none"),
        note(1000, MEND, "vd.void", "none"),
        note(35000, MEND, "vd.void", "step"),
        note(36000, MEND, "vd.void", "none"),
        # Shiv kicks a Volley; one lands, and Mend dispels the slow off Arcane's kite.
        note(31000, SHIV, "vd.interrupt", "volley"),
        note(31500, SHIV, "vd.interrupt", "none"),
        note(43000, MEND, "vd.freedom", "dispel"),
        note(43200, MEND, "vd.freedom", "none"),
        # Bulwark's abomination has it low once; Lumen beacons her while it heals the raid.
        note(20000, BULWARK, "vd.defensive", "idle"),
        note(25000, BULWARK, "vd.defensive", "shield wall"),
        note(26000, BULWARK, "vd.defensive", "idle"),
        note(27000, MEND, "vd.humancall", "Shield Wall"),
        note(5000, LUMEN, "vd.beacon", "her"),
        note(90000, LUMEN, "vd.beacon", "none"),
        {"t": 5100, "e": "cast", "s": LUMEN, "sp": 53563, "tgt": VALITHRIA, "tr": 1},
        # The push opens once the second wave is out; Arcane lusts, Mend and Arkos put Guardian Spirit on her.
        note(200000, MEND, "vd.push", "closed"),
        note(210000, MEND, "vd.push", "open"),
        note(212000, ARCANE, "vd.push", "open"),
        note(250000, MEND, "vd.push", "closed"),
        {"t": 211000, "e": "cast", "s": ARCANE, "sp": 32182, "tr": 1},
        {"t": 211500, "e": "cast", "s": MEND, "sp": 47788, "tgt": VALITHRIA, "tr": 1},
        {"t": 222000, "e": "cast", "s": ARKOS, "sp": 47788, "tgt": VALITHRIA, "tr": 1},
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
            "  vd.dream per bot: spans of each flight rule\n"
            "    Lumen           refresh 1, cloud 1\n"
            "    Mend            refresh 1, human 1, descend 1, none 1\n"
            "\n"
            "  vd.cloud: cloud 2, none 1\n"))


class Heal(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(vd.show_heal, load(pull())), (
            "HEAL\n"
            "  vd.healer per bot healer, each change\n"
            "\n"
            "    Lumen           raid@0:00.100, portal@1:30.000\n"
            "    Mend            portal@0:00.100\n"))


class Adds(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(vd.show_adds, load(pull())), (
            "ADDS\n"
            "  vd.target per bot: spans of each pick\n"
            "\n"
            "    Arcane          skull 1, skeleton 1, zombie 1\n"
            "    Bulwark         owned 1, skeleton 1\n"
            "    Shiv            skeleton 1, abomination 1\n"
            "\n"
            "  vd.tank per bot tank: the abominations it held, from when\n"
            "    Bulwark         Gluttonous Abomination 0:05.000, released 1:00.000\n"))


class Zombies(unittest.TestCase):
    def test_rows_drop_none(self):
        rows = vd.zombie_rows(load(pull()))
        self.assertEqual([(row["guid"], *(dict(row[key]) for key in vd.ZOMBIE_KEYS)) for row in rows], [
            (ARCANE, {"ring ccw": 1, "reverse": 1}, {}, {}, {}, {}, {}),
            (MEND, {}, {}, {}, {"dispel": 1}, {"column": 1}, {"step": 1}),
            (SHIV, {}, {"chains of ice": 1}, {"volley": 1}, {}, {}, {}),
        ])

    def test_section(self):
        self.assertEqual(printed(vd.show_zombies, load(pull())), (
            "ZOMBIES\n"
            "  per bot: spans of each kite leg rule, snare spell, kick, freedom, dodge and void step\n"
            "\n"
            "    Arcane          kite reverse 1, ring ccw 1; snare -; kick -; free -; dodge -; void -\n"
            "    Mend            kite -; snare -; kick -; free dispel 1; dodge column 1; void step 1\n"
            "    Shiv            kite -; snare chains of ice 1; kick volley 1; free -; dodge -; void -\n"))


class Push(unittest.TestCase):
    def test_windows_merge_across_bots(self):
        self.assertEqual(vd.push_windows(load(pull())), [(210000, 250000)])

    def test_casts_skip_humans(self):
        self.assertEqual([(when, caster, spell) for when, caster, spell, _ in vd.push_casts(load(pull()))],
                         [(5100, LUMEN, "Beacon of Light"), (211000, ARCANE, "Heroism"),
                          (211500, MEND, "Guardian Spirit")])

    def test_section(self):
        self.assertEqual(printed(vd.show_push, load(pull())), (
            "PUSH\n"
            "  vd.push open: 3:30.000 for 0:40.000\n"
            "\n"
            "  casts\n"
            "     0:05.100  Lumen           Beacon of Light on Valithria Dreamwalker\n"
            "     3:31.000  Arcane          Heroism\n"
            "     3:31.500  Mend            Guardian Spirit on Valithria Dreamwalker\n"
            "\n"
            "  vd.beacon per paladin healer: spans of each rule\n"
            "    Lumen           her 1, none 1\n"))


class Defensives(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(vd.show_defensives, load(pull())), (
            "DEFENSIVES\n"
            "    Bulwark         shield wall 0:25.000\n"
            "  calls to a human tank  Shield Wall 0:27.000\n"))


class Cheats(unittest.TestCase):
    def test_no_own_keys(self):
        self.assertEqual(printed(vd.show_cheats, load(pull())), (
            "CHEATS\n"
            "  no cheat of this encounter fired\n"
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
