"""Tests for the Lich King reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25H pull pins each section, and the shared v14 fixture shows every section reads empty on
a trace that is not the Lich King rather than raising.
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

from bosses import the_lich_king as lk  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
LICH_KING = CREATURE | 960
VALKYR_A = CREATURE | 961
VALKYR_B = CREATURE | 962
VALKYR_C = CREATURE | 963
RAGING = CREATURE | 964
SHAMBLING = CREATURE | 965
WICKED = CREATURE | 966
SPHERE = CREATURE | 967
BULWARK = 7501  # main tank
BASTION = 7502  # assist tank
MEND = 7503     # healer
SHIV = 7504     # melee
ARCANE = 7505   # ranged
ARKOS = 7506    # human melee

ROSTER = ((BULWARK, "Bulwark", "tank", 0), (BASTION, "Bastion", "tank", 0), (MEND, "Mend", "heal", 0),
          (SHIV, "Shiv", "melee", 0), (ARCANE, "Arcane", "ranged", 0), (ARKOS, "Arkos", "melee", 1))
UNITS = ((LICH_KING, 36597, "The Lich King"), (VALKYR_A, 36609, "Val'kyr Shadowguard"),
         (VALKYR_B, 36609, "Val'kyr Shadowguard"), (VALKYR_C, 36609, "Val'kyr Shadowguard"),
         (RAGING, 36701, "Raging Spirit"), (SHAMBLING, 37698, "Shambling Horror"),
         (WICKED, 39190, "Wicked Spirit"), (SPHERE, 36633, "Ice Sphere"))


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def cheat(when: int, guid: int, key: str, uses: int, target: int | None = None) -> dict:
    rec = {"t": when, "e": "cheat", "g": guid, "k": key, "n": uses}
    if target:
        rec["tgt"] = target
    return rec


def pull(boss: str = "the-lich-king", diff: int = 3) -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": diff, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 1},
         "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": human}
                    for guid, name, role, human in ROSTER]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        *({"t": 1, "e": "unit", "g": guid, "en": entry, "n": name} for guid, entry, name in UNITS),
        note(100, BULWARK, "lk.phase", "one"),
        note(110, MEND, "lk.phase", "one"),
        note(1000, BULWARK, "lk.skull", f"boss {LICH_KING}"),
        # The first winter: the staging anchor, the walks it started, its icons.
        note(60000, BULWARK, "lk.phase", "winter1"),
        note(61000, SHIV, "lk.winterstage", "winter1 spirit1"),
        note(61000, SHIV, "lk.winterinbound", "1"),
        note(61000, ARCANE, "lk.winterinbound", "1"),
        note(61500, BULWARK, "lk.mtinbound", "1"),
        note(62000, SHIV, "lk.winterinbound", "0"),
        note(62000, SHIV, "lk.skull", f"shambling {SHAMBLING}"),
        note(64000, ARCANE, "lk.rangedinbound", "0"),
        note(70000, ARCANE, "lk.circle", f"sphere {SPHERE}"),
        # Phase two: Val'kyr waves.
        note(125000, BULWARK, "lk.phase", "two"),
        note(130000, ARCANE, "lk.skull", f"spirit {RAGING}"),
        note(150000, SHIV, "lk.valkyr", f"skull {VALKYR_A}"),
        note(150000, ARCANE, "lk.valkyr", f"cross {VALKYR_B}"),
        note(150000, MEND, "lk.cross", f"valkyr {VALKYR_B}"),
        note(150000, MEND, "lk.skull", f"valkyr {VALKYR_A}"),
        note(150100, MEND, "lk.valkyr", f"skull {VALKYR_A}"),
        note(160000, SHIV, "lk.valkyr", "none"),
        note(195000, SHIV, "lk.valkyr", f"skull {VALKYR_C}"),
        # The second winter, then phase three and heroic's Frostmourne.
        note(300000, BULWARK, "lk.phase", "winter2"),
        note(301000, ARCANE, "lk.winterstage", "winter2 none"),
        note(338000, ARCANE, "lk.defile", "72762"),
        note(365000, BULWARK, "lk.phase", "three"),
        note(366000, ARCANE, "lk.stackslot", "spirit1"),
        note(366000, BULWARK, "lk.sharedslot", "-1"),
        note(368000, MEND, "lk.stackslot", "spirit1"),
        note(370000, BULWARK, "lk.cross", "clear"),
        note(371000, SHIV, "lk.defile", "72762"),
        note(380000, SHIV, "lk.phase", "frostmourne"),
        note(381000, BULWARK, "lk.bombdodge", "safe"),
        note(382000, BULWARK, "lk.bombdodge", "dodge"),
        note(383000, BULWARK, "lk.bombdodge", "safe"),
        note(384000, BULWARK, "lk.bombdodge", "escape"),
        note(385000, BULWARK, "lk.sharedslot", "0"),
        note(390000, ARCANE, "lk.star", f"chasing {WICKED}"),
        note(400000, SHIV, "lk.stackslot", "spirit3"),
        note(404000, ARCANE, "lk.defile", "72762"),
        note(410000, BULWARK, "lk.sharedslot", "-1"),
        note(440000, BULWARK, "lk.phase", "three"),
        cheat(1000, BULWARK, "lk.heroicbuff", 4, ARKOS),
        cheat(59000, BASTION, "lk.hoj", 3, SHAMBLING),
        cheat(70000, BULWARK, "lk.ragingspiritkill", 2, RAGING),
        cheat(200000, SHIV, "sg.flares", 1),
        {"t": 500000, "e": "end", "out": "kill"},
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
        self.assertEqual(printed(lk.show_phases, load(pull())), (
            "PHASES\n"
            "  lk.phase, each step from the first bot to note it\n"
            "\n"
            "         from       held  phase\n"
            "     0:00.100   0:59.900  one\n"
            "     1:00.000   1:05.000  winter1\n"
            "     2:05.000   2:55.000  two\n"
            "     5:00.000   1:05.000  winter2\n"
            "     6:05.000   0:15.000  three\n"
            "     6:20.000   1:00.000  frostmourne\n"
            "     7:20.000   1:00.000  three\n"))


class Assignments(unittest.TestCase):
    def test_buckets_group_bots_per_valkyr(self):
        rows = lk.valkyr_buckets(load(pull()))
        self.assertEqual([(row["t"], row["icon"], row["bots"]) for row in rows],
                         [(150000, "skull", {SHIV, MEND}), (150000, "cross", {ARCANE}), (195000, "skull", {SHIV})])

    def test_inbound_counts_only_walks(self):
        self.assertEqual(lk.inbound_counts(load(pull())), {"lk.winterinbound": 2, "lk.mtinbound": 1})

    def test_section(self):
        self.assertEqual(printed(lk.show_assignments, load(pull())), (
            "ASSIGNMENTS\n"
            "  lk.winterstage, the staging anchor each winter latched\n"
            "     1:01.000  Shiv            winter1 spirit1\n"
            "     5:01.000  Arcane          winter2 none\n"
            "  sent walking: lk.winterinbound 2 bot(s), lk.mtinbound 1 bot(s)\n"
            "\n"
            "  lk.valkyr, each Val'kyr's bucket\n"
            "     2:30.000  skull  Val'kyr Shadowguard  2: Mend, Shiv\n"
            "     2:30.000  cross  Val'kyr Shadowguard  1: Arcane\n"
            "     3:15.000  skull  Val'kyr Shadowguard  1: Shiv\n"
            "\n"
            "  lk.stackslot, the phase three stack\n"
            "     6:06.000   0:34.000  spirit1\n"
            "     6:40.000   1:40.000  spirit3\n"
            "\n"
            "  lk.sharedslot, the Vile Spirit slot\n"
            "     6:06.000   0:19.000  -1\n"
            "     6:25.000   0:25.000  0\n"
            "     6:50.000   1:30.000  -1\n"
            "\n"
            "  lk.defile targets: Arcane 2, Shiv 1\n"
            "  lk.bombdodge switches: safe 2, dodge 1, escape 1\n"))


class Icons(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(lk.show_icons, load(pull())), (
            "ICONS\n"
            "     0:01.000  skull   Bulwark         boss The Lich King\n"
            "     1:02.000  skull   Shiv            shambling Shambling Horror\n"
            "     1:10.000  circle  Arcane          sphere Ice Sphere\n"
            "     2:10.000  skull   Arcane          spirit Raging Spirit\n"
            "     2:30.000  cross   Mend            valkyr Val'kyr Shadowguard\n"
            "     2:30.000  skull   Mend            valkyr Val'kyr Shadowguard\n"
            "     6:10.000  cross   Bulwark         clear\n"
            "     6:30.000  star    Arcane          chasing Wicked Spirit\n"))


class Cheats(unittest.TestCase):
    def test_own_keys(self):
        self.assertEqual(printed(lk.show_cheats, load(pull())), (
            "CHEATS\n"
            "  lk.heroicbuff  4 use(s) from 0:01.000\n"
            "    by  Bulwark 4\n"
            "    on  Arkos human\n"
            "  lk.hoj  3 use(s) from 0:59.000\n"
            "    by  Bastion 3\n"
            "    on  Shambling Horror\n"
            "  lk.ragingspiritkill  2 use(s) from 1:10.000\n"
            "    by  Bulwark 2\n"
            "    on  Raging Spirit\n"
            "  1 use(s) under other encounters' keys: icecrown_citadel.py --cheats\n"))


class Banner(unittest.TestCase):
    def test_every_probe_present(self):
        self.assertIn("all lk.* probes present", printed(lk.show_banner, load(pull())))

    def test_ten_normal_spares_the_cross_star_and_bombs(self):
        spared = {"lk.cross", "lk.star", "lk.bombdodge"}
        records = [rec for rec in pull(diff=0) if rec.get("k") not in spared]
        self.assertIn("all lk.* probes present", printed(lk.show_banner, load(records)))

    def test_a_missing_probe_is_named(self):
        records = [rec for rec in pull() if rec.get("k") != "lk.valkyr"]
        self.assertIn("probes absent from this trace: lk.valkyr", printed(lk.show_banner, load(records)))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            lk.show_banner(trace)
            for _, _, section in lk.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Lich King pull"), len(lk.SECTIONS))


if __name__ == "__main__":
    unittest.main()
