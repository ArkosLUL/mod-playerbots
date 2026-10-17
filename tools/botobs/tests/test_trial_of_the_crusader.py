"""Tests for the raid-wide Trial of the Crusader reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25N Beasts trace pins `--staging` on Gormok's release and both worms', two small ones pin
the twins' shared duty and the first champion in a trace that never wrote `toc.stage`, and the shared
fixture shows the reader reads empty off ToC.
"""
from __future__ import annotations

import contextlib
import io
import json
import pathlib
import re
import sys
import tempfile
import unittest

BOTOBS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BOTOBS))

from bosses import trial_of_the_crusader as toc  # noqa: E402
from raidobs.paths import RAID_ROOT  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v13.ndjson"
CHAMPIONS_HEADER = RAID_ROOT / "ToC" / "Util" / "ToCHelpers_FactionChampions.h"

CREATURE = 1 << 32
GORMOK = CREATURE | 100
DREADSCALE = CREATURE | 101
ACIDMAW = CREATURE | 102
FJOLA = CREATURE | 200
EYDIS = CREATURE | 201
ROGUE_CHAMPION = CREATURE | 300
PALADIN_CHAMPION = CREATURE | 301

BULWARK = 7001  # tank: Gormok, then the mobile worm
AEGIS = 7002    # tank: swap, then the stationary worm
MEND = 7003     # healer, `none` on the worms
FROST = 7004    # ranged
SHIV = 7005     # melee, never notes a stand
ARKOS = 7006    # human melee

WARD = 8001
BASTION = 8002
SPARK = 8003

SHIELD = 9001
BOLT = 9002


def unit_row(guid: int, x: float, y: float, target: int = 0) -> list:
    return [guid, x, y, 394.5, 0.0, 100.0, 100.0, target, 0, 0, 0, 0, 0, 100.0]


def note(when: int, guid: int, text: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": toc.STAGE_KEY, "txt": text}


def hit(when: int, source: int, victim: int, amount: int) -> dict:
    return {"t": when, "e": "dmg", "s": source, "d": victim, "sp": 0, "a": amount, "ok": 0, "sc": 1, "ab": 0,
            "rs": 0, "hp": 100.0}


def header(boss: str, diff: int, roster: list[tuple[int, str, str, int]]) -> dict:
    return {"e": "hdr", "v": 13, "ts": 1789700000000, "map": 649, "inst": 3, "diff": diff, "boss": boss,
            "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": human}
                       for guid, name, role, human in roster]}


def write(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / f"649_3_{records[0]['boss']}_1789700000.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


def gormok_snap(when: int) -> dict:
    target = 0 if when < 0 else ARKOS if when < 1250 else BULWARK
    units = [
        unit_row(GORMOK, 563.9, 178.4, target),
        unit_row(BULWARK, 567.3, 175.4, GORMOK),
        unit_row(AEGIS, 570.0, 174.0),
        # off the stand only after the release, so a reader taking the next sample would see 5.0 off
        unit_row(FROST, 583.0, 164.0) if when <= 0 else unit_row(FROST, 580.0, 160.0),
        unit_row(SHIV, 565.0, 172.0),
        unit_row(ARKOS, 564.0, 176.0),
    ]
    # missing from the release's own snapshot, so its last pre-roll row places it
    if when < 0:
        units.append(unit_row(MEND, 546.2, 161.6))
    elif when > 0:
        units.append(unit_row(MEND, 530.0, 150.0))
    return {"t": when, "e": "snap", "u": units, "hz": []}


def worms_snap(when: int) -> dict:
    acidmaw = 0 if when < 30250 else FROST if when < 31000 else AEGIS
    units = [
        unit_row(DREADSCALE, 575.7, 171.5, BULWARK if when >= 30000 else 0),
        unit_row(ACIDMAW, 549.9, 171.5, acidmaw),
        unit_row(BULWARK, 579.0, 171.5),
        unit_row(AEGIS, 547.0, 171.0),
        unit_row(FROST, 566.6, 158.8),
        unit_row(MEND, 540.0, 150.0),
        unit_row(SHIV, 560.0, 165.0),
        unit_row(ARKOS, 560.0, 160.0),
    ]
    return {"t": when, "e": "snap", "u": units, "hz": []}


def beasts_pull() -> list[dict]:
    records = [
        header("gormok-the-impaler", 1, [
            (BULWARK, "Bulwark", "tank", 0), (AEGIS, "Aegis", "tank", 0), (MEND, "Mend", "heal", 0),
            (FROST, "Frost", "ranged", 0), (SHIV, "Shiv", "melee", 0), (ARKOS, "Arkos", "melee", 1)]),
        {"t": 0, "e": "pull", "boss": "gormok-the-impaler", "src": "engage"},
        {"t": 1, "e": "unit", "g": GORMOK, "en": toc.NPC_GORMOK, "n": "Gormok the Impaler", "b": 1},
        {"t": 1, "e": "unit", "g": DREADSCALE, "en": toc.NPC_DREADSCALE, "n": "Dreadscale", "b": 1},
        {"t": 1, "e": "unit", "g": ACIDMAW, "en": toc.NPC_ACIDMAW, "n": "Acidmaw", "b": 1},

        # Gormok: the tank's stand moves and gains the human suffix after the open, the ranged stand
        # moves again past release + 2 s
        note(-2000, MEND, "gormok.healer 545,160,395"),
        note(-1000, BULWARK, "gormok.tank 560,175,395"),
        note(-500, AEGIS, "gormok.swap 570,170,395"),
        note(500, BULWARK, "gormok.tank 567,175,395 human"),
        note(1500, FROST, "gormok.ranged 580,160,395"),
        note(2500, FROST, "gormok.ranged 590,150,395"),
        hit(1000, GORMOK, ARKOS, 4200),
        hit(2000, GORMOK, BULWARK, 9000),
        hit(5500, GORMOK, MEND, 3000),

        note(25000, BULWARK, "worms.mobile 579,171,395"),
        note(25000, AEGIS, "worms.stationary 547,171,395"),
        note(25000, FROST, "worms.ranged 566,158,395"),
        note(26000, MEND, "none"),
        hit(30100, DREADSCALE, BULWARK, 8000),
        hit(30500, ACIDMAW, FROST, 3000),

        {"t": 60000, "e": "end", "out": "kill"},
    ]
    records += [gormok_snap(when) for when in range(-1000, 6001, 250)]
    records += [worms_snap(when) for when in range(29000, 32001, 250)]
    return records


def twins_pull() -> list[dict]:
    records = [
        header("fjola-lightbane", 2, [(WARD, "Ward", "tank", 0), (BASTION, "Bastion", "tank", 0),
                                      (SPARK, "Spark", "ranged", 0)]),
        {"t": 0, "e": "pull", "boss": "fjola-lightbane", "src": "engage"},
        {"t": 1, "e": "unit", "g": FJOLA, "en": toc.NPC_FJOLA, "n": "Fjola Lightbane", "b": 1},
        {"t": 1, "e": "unit", "g": EYDIS, "en": toc.NPC_EYDIS, "n": "Eydis Darkbane", "b": 1},
        note(100, WARD, "twins.both 563,150,395"),
        note(100, BASTION, "twins.fjola 580,150,395"),
        {"t": 20000, "e": "end", "out": "wipe"},
    ]
    for when in (-250, 0, 250, 500):
        records.append({"t": when, "e": "snap", "hz": [], "u": [
            unit_row(FJOLA, 585.5, 170.0, BASTION if when >= 0 else 0),
            unit_row(EYDIS, 545.5, 170.0, WARD if when >= 250 else 0),
            unit_row(WARD, 563.0, 150.0),
            unit_row(BASTION, 581.0, 150.0),
            unit_row(SPARK, 560.0, 140.0),
        ]})
    return records


def champions_pull() -> list[dict]:
    records = [
        header("malithas-brightblade", 1, [(SHIELD, "Shield", "tank", 0), (BOLT, "Bolt", "ranged", 0)]),
        {"t": 0, "e": "pull", "boss": "malithas-brightblade", "src": "engage"},
        {"t": 1, "e": "unit", "g": ROGUE_CHAMPION, "en": 34454, "n": "Tyrius Duskblade", "b": 1},
        {"t": 1, "e": "unit", "g": PALADIN_CHAMPION, "en": 34445, "n": "Malithas Brightblade", "b": 1},
        {"t": 30000, "e": "end", "out": "wipe"},
    ]
    for when in (0, 250, 500):
        records.append({"t": when, "e": "snap", "hz": [], "u": [
            unit_row(ROGUE_CHAMPION, 530.0, 120.0, BOLT if when >= 500 else 0),
            unit_row(PALADIN_CHAMPION, 532.0, 125.0, SHIELD if when >= 250 else 0),
            unit_row(SHIELD, 560.0, 130.0),
            unit_row(BOLT, 575.0, 140.0),
        ]})
    return records


def printed(show, *args) -> str:
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        show(*args)
    return out.getvalue()


def table(text: str) -> list[list[str]]:
    return [line.split() for line in text.splitlines()]


class StandNotes(unittest.TestCase):
    def test_a_stand_parses_with_and_without_the_human_suffix(self):
        self.assertEqual(toc.parse_stand("gormok.tank 567,175,395 human"),
                         {"label": "gormok.tank", "arrival": "gormok", "role": "tank",
                          "spot": (567.0, 175.0, 395.0), "human": True})
        self.assertEqual(toc.parse_stand("worms.mobile -3,4,142")["spot"], (-3.0, 4.0, 142.0))
        self.assertFalse(toc.parse_stand("worms.mobile -3,4,142")["human"])

    def test_none_and_junk_are_no_stand(self):
        for text in ("none", "", "gormok.tank", "gormok.tank 1,2", "gormok 1,2,3", "gormok.tank a,b,c"):
            self.assertIsNone(toc.parse_stand(text), text)

    def test_champion_entries_match_the_enum(self):
        text = CHAMPIONS_HEADER.read_text(encoding="utf-8")
        body = re.search(r"enum class ToCFactionChampions\b[^{]*\{(.*?)\};", text, re.S)
        self.assertIsNotNone(body, f"no ToCFactionChampions enum in {CHAMPIONS_HEADER}")
        entries = {int(value) for value in re.findall(r"^\s*\w+\s*=\s*(\d+)", body.group(1), re.M)}
        self.assertEqual(toc.CHAMPION_ENTRIES, entries)


class BeastsStaging(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.trace = write(beasts_pull())
        cls.releases = toc.releases(cls.trace)
        cls.reports = [toc.release_report(cls.trace, release) for release in cls.releases]

    def block(self, index: int) -> str:
        return printed(toc.show_release, self.trace, self.reports[index])

    def test_each_beast_releases_on_its_first_victim(self):
        self.assertEqual([(release["guid"], release["arrival"], release["t"], release["victim"])
                          for release in self.releases],
                         [(GORMOK, "gormok", 0, ARKOS), (DREADSCALE, "worms", 30000, BULWARK),
                          (ACIDMAW, "worms", 30250, FROST)])

    def test_gormok_release(self):
        report = self.reports[0]
        self.assertEqual(report["on_tank"], 1250)
        self.assertEqual([(rec["t"], rec["d"], rec["a"]) for rec in report["hits"]], [(1000, ARKOS, 4200)])
        self.assertEqual([row["guid"] for row in report["holders"]], [BULWARK])
        self.assertEqual((report["within"], report["past"], report["unsampled"], report["none"],
                          report["missing"]), (2, 2, 0, 0, 1))

        text = self.block(0)
        self.assertIn("  Gormok the Impaler (gormok) released at 0:00.000 on Arkos, melee, human\n", text)
        self.assertIn("    duty tank: Bulwark human\n", text)
        self.assertIn("    first on a tank after 1250 ms\n", text)
        self.assertIn("    non-tank hits in 5 s: 1, 4,200 damage: Arkos +1000 ms 4,200\n", text)
        self.assertIn("    4 stand(s), 2 within 3 yd, 2 past it, 0 none, 1 no stand\n", text)
        rows = table(text)
        start = rows.index(["bot", "role", "stand", "at", "yd"])
        self.assertEqual(rows[start + 1:], [
            ["Bulwark", "tank", "tank", "human", "567,175,395", "0.5"],
            ["Mend", "heal", "healer", "545,160,395", "2.0"],
            ["Frost", "ranged", "ranged", "580,160,395", "5.0!"],
            ["Aegis", "tank", "swap", "570,170,395", "4.0!"],
            ["Shiv", "melee", "no", "stand"],
        ])

    def test_mobile_worm_release(self):
        text = self.block(1)
        self.assertIn("  Dreadscale (worms) released at 0:30.000 on Bulwark, tank, bot\n", text)
        self.assertIn("    duty mobile: Bulwark\n", text)
        self.assertIn("    first on a tank after 0 ms\n", text)
        self.assertIn("    non-tank hits in 5 s: none\n", text)
        self.assertIn("    3 stand(s), 3 within 3 yd, 0 past it, 1 none, 1 no stand\n", text)
        rows = table(text)
        start = rows.index(["bot", "role", "stand", "at", "yd"])
        self.assertEqual(rows[start + 1:], [
            ["Bulwark", "tank", "mobile", "579,171,395", "0.5"],
            ["Frost", "ranged", "ranged", "566,158,395", "1.0"],
            ["Aegis", "tank", "stationary", "547,171,395", "0.0"],
            ["Mend", "heal", "none"],
            ["Shiv", "melee", "no", "stand"],
        ])

    def test_stationary_worm_release(self):
        text = self.block(2)
        self.assertIn("  Acidmaw (worms) released at 0:30.250 on Frost, ranged, bot\n", text)
        self.assertIn("    duty stationary: Aegis\n", text)
        self.assertIn("    first on a tank after 750 ms\n", text)
        self.assertIn("    non-tank hits in 5 s: 1, 3,000 damage: Frost +250 ms 3,000\n", text)
        self.assertIn("    3 stand(s), 3 within 3 yd, 0 past it, 1 none, 1 no stand\n", text)
        rows = table(text)
        start = rows.index(["bot", "role", "stand", "at", "yd"])
        self.assertEqual(rows[start + 1:], [
            ["Aegis", "tank", "stationary", "547,171,395", "0.0"],
            ["Bulwark", "tank", "mobile", "579,171,395", "0.5"],
            ["Frost", "ranged", "ranged", "566,158,395", "1.0"],
            ["Mend", "heal", "none"],
            ["Shiv", "melee", "no", "stand"],
        ])

    def test_a_later_none_hides_an_earlier_stand_but_another_arrival_does_not(self):
        self.assertIsNone(toc.stands(self.trace, "worms", 32000)[MEND])
        self.assertEqual(toc.stands(self.trace, "gormok", 2000)[MEND]["role"], "healer")
        # Bulwark's latest by then is a worms stand, so its Gormok stand still answers
        self.assertEqual(toc.stands(self.trace, "gormok", 32000)[BULWARK]["spot"], (567.0, 175.0, 395.0))
        self.assertNotIn(SHIV, toc.stands(self.trace, "gormok", 32000))

    def test_banner_and_section(self):
        banner = printed(toc.show_banner, self.trace)
        self.assertEqual(banner, "northrend-beasts 25N  kill at 1:00.000  0 death(s)\n")
        text = printed(toc.show_staging, self.trace)
        self.assertTrue(text.startswith("STAGING\n  Gormok the Impaler (gormok)"))
        self.assertLess(text.index("Dreadscale (worms)"), text.index("Acidmaw (worms)"))
        self.assertIn("  stand is the bot's latest toc.stage up to 2 s past the release", text)


class TwinsStaging(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.trace = write(twins_pull())

    def test_both_holds_either_twin(self):
        releases = toc.releases(self.trace)
        self.assertEqual([(release["guid"], release["t"], release["victim"]) for release in releases],
                         [(FJOLA, 0, BASTION), (EYDIS, 250, WARD)])

        fjola = printed(toc.show_release, self.trace, toc.release_report(self.trace, releases[0]))
        self.assertIn("  Fjola Lightbane (twins) released at 0:00.000 on Bastion, tank, bot\n", fjola)
        self.assertIn("    duty fjola or both: Ward both, Bastion fjola\n", fjola)
        self.assertIn("    first on a tank after 0 ms\n", fjola)
        self.assertIn("    2 stand(s), 2 within 3 yd, 0 past it, 0 none, 1 no stand\n", fjola)
        rows = table(fjola)
        self.assertIn(["Ward", "tank", "both", "563,150,395", "0.0"], rows)
        self.assertIn(["Bastion", "tank", "fjola", "580,150,395", "1.0"], rows)
        self.assertIn(["Spark", "ranged", "no", "stand"], rows)

        eydis = printed(toc.show_release, self.trace, toc.release_report(self.trace, releases[1]))
        self.assertIn("  Eydis Darkbane (twins) released at 0:00.250 on Ward, tank, bot\n", eydis)
        self.assertIn("    duty eydis or both: Ward both\n", eydis)


class ChampionsStaging(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.trace = write(champions_pull())

    def test_only_the_first_champion_releases(self):
        releases = toc.releases(self.trace)
        self.assertEqual([(release["guid"], release["arrival"], release["t"], release["victim"])
                          for release in releases], [(PALADIN_CHAMPION, "champions", 250, SHIELD)])
        text = printed(toc.show_release, self.trace, toc.release_report(self.trace, releases[0]))
        self.assertIn("  Malithas Brightblade (champions) released at 0:00.250 on Shield, tank, bot\n", text)
        self.assertIn("    no duty stand\n", text)
        self.assertIn("    0 stand(s), 0 within 3 yd, 0 past it, 0 none, 2 no stand\n", text)

    def test_banner_says_the_stage_probe_is_missing(self):
        self.assertEqual(printed(toc.show_banner, self.trace),
                         "faction-champions 25N  wipe at 0:30.000  0 death(s)\n"
                         "  toc.stage never written: a build without staging, or no bot's window opened\n")


class OffToC(unittest.TestCase):
    def test_every_section_reads_empty_off_toc(self):
        trace = Trace(FULL)
        self.assertIn("not a Trial of the Crusader trace, every section below reads empty",
                      printed(toc.show_banner, trace))
        self.assertEqual(printed(toc.show_staging, trace), "STAGING\n  not a Trial of the Crusader trace\n")


if __name__ == "__main__":
    unittest.main()
