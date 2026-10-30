"""Tests for the Lich King reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25H pull and the same pull on 25N pin each section. The banner reads a fixed declared set,
so the source can gain or lose keys under it, and the shared v14 fixture shows every section reads
empty on a trace that is not the Lich King rather than raising.
"""
from __future__ import annotations

import contextlib
import io
import json
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

BOTOBS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BOTOBS))

from bosses import the_lich_king as lk  # noqa: E402
from raidobs import probes  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
LICH_KING = CREATURE | 970
GHOUL = CREATURE | 971
SHAMBLING = CREATURE | 972
TRAP = CREATURE | 973
SPHERE = CREATURE | 974
RAGING = CREATURE | 975
VALKYR_A = CREATURE | 976
VALKYR_B = CREATURE | 977
PUDDLE = CREATURE | 978
VILE = CREATURE | 979
WICKED = CREATURE | 980
BOMB = CREATURE | 981
BULWARK = 7801  # tank
BASTION = 7802  # tank
MEND = 7803     # healer
SHIV = 7804     # melee
ARCANE = 7805   # ranged
ARKOS = 7806    # human melee

ROSTER = ((BULWARK, "Bulwark", "tank", 0), (BASTION, "Bastion", "tank", 0), (MEND, "Mend", "heal", 0),
          (SHIV, "Shiv", "melee", 0), (ARCANE, "Arcane", "ranged", 0), (ARKOS, "Arkos", "melee", 1))
UNITS = ((GHOUL, 37695, "Drudge Ghoul"), (SHAMBLING, 37698, "Shambling Horror"), (TRAP, 39137, "Shadow Trap"),
         (SPHERE, 36633, "Ice Sphere"), (RAGING, 36701, "Raging Spirit"), (VALKYR_A, 36609, "Val'kyr Shadowguard"),
         (VALKYR_B, 36609, "Val'kyr Shadowguard"), (PUDDLE, 38757, "Defile"), (VILE, 37799, "Vile Spirit"),
         (WICKED, 39190, "Wicked Spirit"), (BOMB, 39189, "Spirit Bomb"))

KEYS = ("lk.phase", "lk.tank", "lk.taunt", "lk.defensive", "lk.humancall", "lk.brake", "lk.stand", "lk.plague",
        "lk.cure", "lk.enrage", "lk.stun", "lk.trap", "lk.silence", "lk.target", "lk.stack", "lk.defile",
        "lk.defilerun", "lk.valkyr", "lk.siphon", "lk.side", "lk.vile", "lk.harvest", "lk.bombdodge")
UNUSED_KEYS = ("lk.winterinbound", "lk.mtinbound", "lk.rangedinbound", "lk.sharedslot")
DECLARED = {key: (probes.HOLDER, "ICCHelpers_LichKing.cpp:1") for key in KEYS + UNUSED_KEYS}


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def counter(guid: int) -> int:
    """The guid counter a probe value names a unit by."""
    return guid & 0xFFFFFFFF


def tag(rule: str, unit: int) -> str:
    return f"{rule} {counter(unit)}"


def aura(when: int, guid: int, spell: int, removed: int = 0) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": LICH_KING, "sp": spell, "r": removed, "st": 1}


def dmg(when: int, guid: int, spell: int, source: int = LICH_KING) -> dict:
    return {"t": when, "e": "dmg", "s": source, "d": guid, "sp": spell, "a": 10000}


def cast(when: int, source: int, spell: int, target: int | None = None) -> dict:
    return {"t": when, "e": "cast", "s": source, "sp": spell, "tgt": target or source}


def cheat(when: int, guid: int, key: str, uses: int, target: int | None = None) -> dict:
    rec = {"t": when, "e": "cheat", "g": guid, "k": key, "n": uses}
    if target:
        rec["tgt"] = target
    return rec


def opening(diff: int) -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": diff, "boss": "the-lich-king",
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 1},
         "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": human}
                    for guid, name, role, human in ROSTER]},
        {"t": 0, "e": "pull", "boss": "the-lich-king", "src": "bossstate"},
        {"t": 1, "e": "spell", "sp": 871, "n": "Shield Wall"},
        {"t": 1, "e": "unit", "g": LICH_KING, "en": 36597, "n": "The Lich King", "lvl": 83, "b": 1},
        *({"t": 1, "e": "unit", "g": guid, "en": entry, "n": name} for guid, entry, name in UNITS),
    ]


def phase_one() -> list[dict]:
    return [
        note(100, BULWARK, "lk.phase", "one"),
        note(110, MEND, "lk.phase", "one"),
        # Tanks: a taunt each, Shiv braked once, one Shield Wall window called to the human too.
        note(50, BULWARK, "lk.tank", "1"),
        note(50, BASTION, "lk.tank", "2"),
        note(100, BULWARK, "lk.defensive", "idle"),
        note(5000, BULWARK, "lk.taunt", tag("back", LICH_KING)),
        note(6000, BULWARK, "lk.taunt", "none"),
        note(15000, BASTION, "lk.taunt", tag("add", GHOUL)),
        note(16000, BASTION, "lk.taunt", "none"),
        note(30000, SHIV, "lk.brake", "1"),
        note(32000, SHIV, "lk.brake", "0"),
        note(50000, BULWARK, "lk.defensive", "shield wall"),
        note(50000, BULWARK, "lk.humancall", "Shield Wall"),
        cast(50500, BULWARK, 871),
        note(53000, BULWARK, "lk.defensive", "idle"),
        note(50, BULWARK, "lk.stand", "opening"),
        note(50, BASTION, "lk.stand", "opening"),
        note(50, MEND, "lk.stand", "opening"),
        note(50, ARCANE, "lk.stand", "opening"),
        note(4000, BULWARK, "lk.stand", "holder"),
        note(4000, BASTION, "lk.stand", "addtank"),
        note(4000, MEND, "lk.stand", "ranged 1"),
        note(4000, ARCANE, "lk.stand", "ranged 2"),
        note(100, MEND, "lk.target", "none"),
        note(4000, BULWARK, "lk.target", tag("boss", LICH_KING)),
        note(5000, SHIV, "lk.target", tag("boss", LICH_KING)),
        note(5000, ARCANE, "lk.target", tag("boss", LICH_KING)),
        note(15000, BASTION, "lk.target", tag("add", GHOUL)),
        # Plague: Mend delivers to a Shambling, Shiv is cured in an emergency, Bastion takes an add's.
        note(30200, MEND, "lk.plague", tag("walk", SHAMBLING)),
        note(33000, MEND, "lk.plague", tag("delivered", SHAMBLING)),
        note(33000, BASTION, "lk.cure", f"delivered {MEND}"),
        note(33500, BASTION, "lk.cure", "none"),
        note(34000, MEND, "lk.plague", "none"),
        note(45200, SHIV, "lk.plague", tag("walk", SHAMBLING)),
        dmg(46000, SHIV, 73914),
        note(46500, MEND, "lk.cure", f"emergency {SHIV}"),
        note(47000, MEND, "lk.cure", "none"),
        note(47000, SHIV, "lk.plague", "none"),
        note(50000, MEND, "lk.cure", f"addtank {BASTION}"),
        note(50500, MEND, "lk.cure", "none"),
        note(51500, MEND, "lk.cure", "totem"),
        note(52000, MEND, "lk.cure", "none"),
        dmg(51000, BASTION, 73914),
        # Adds: two Enrages called to Arcane, one Tranquilizing Shot, Shiv on the Frenzied Shambling.
        aura(20000, SHAMBLING, 72148),
        note(20100, ARCANE, "lk.enrage", str(counter(SHAMBLING))),
        cast(20500, ARCANE, 19801, SHAMBLING),
        note(20600, ARCANE, "lk.enrage", "none"),
        aura(26000, SHAMBLING, 72148, removed=1),
        aura(42000, SHAMBLING, 72148),
        note(42100, ARCANE, "lk.enrage", str(counter(SHAMBLING))),
        note(42600, ARCANE, "lk.enrage", "none"),
        note(52000, SHIV, "lk.target", tag("frenzy", SHAMBLING)),
        note(56000, SHIV, "lk.target", tag("boss", LICH_KING)),
    ]


def heroic_one() -> list[dict]:
    return [
        # lk.stun is a traced map, so it names the whole guid
        aura(52000, SHAMBLING, 28747),
        note(52100, SHIV, "lk.stun", str(SHAMBLING)),
        note(53500, BULWARK, "lk.stun", str(SHAMBLING)),
        note(54000, SHIV, "lk.stun", "0"),
        note(56000, BULWARK, "lk.stun", "0"),
        note(25000, MEND, "lk.trap", tag("dodge", TRAP)),
        note(26500, MEND, "lk.trap", "none"),
        note(40000, ARCANE, "lk.trap", tag("dodge", TRAP)),
        note(41000, ARCANE, "lk.trap", "none"),
        note(41500, ARCANE, "lk.trap", tag("dodge", TRAP)),
        note(42500, ARCANE, "lk.trap", "none"),
        # Shiv's reach to him crosses the trap: it walks round, then waits with no clear spot
        note(43000, SHIV, "lk.trap", tag("reach", TRAP)),
        note(44500, SHIV, "lk.trap", tag("hold", TRAP)),
        note(46000, SHIV, "lk.trap", "none"),
        dmg(44000, ARKOS, 73529, TRAP),
        cheat(1000, BULWARK, "lk.heroicbuff", 4, ARKOS),
    ]


def winter_one() -> list[dict]:
    return [
        note(60000, BULWARK, "lk.phase", "winter1"),
        note(60500, BULWARK, "lk.stand", "winter tank 1"),
        note(60500, BASTION, "lk.stand", "winter tank 4"),
        note(60500, MEND, "lk.stand", "winter tank 2"),
        note(60500, SHIV, "lk.stand", "winter tank 3"),
        note(60500, ARCANE, "lk.stand", "winter guard 5"),
        note(61200, ARCANE, "lk.plague", "isolate"),
        note(64000, MEND, "lk.cure", f"isolated {ARCANE}"),
        note(64500, MEND, "lk.cure", "none"),
        note(64500, ARCANE, "lk.plague", "none"),
        # Both tanks take a spirit; Arcane shoots spheres, Shiv the spirits, Mend dispels two silences.
        note(64000, BULWARK, "lk.taunt", tag("spirit", RAGING)),
        note(64000, BULWARK, "lk.stand", "spirit"),
        note(64000, BULWARK, "lk.target", tag("spirit", RAGING)),
        note(65000, BULWARK, "lk.taunt", "none"),
        note(65000, SHIV, "lk.target", tag("spirit", RAGING)),
        note(66000, BASTION, "lk.stand", "spirit"),
        note(69000, ARCANE, "lk.target", tag("sphere", SPHERE)),
        note(70000, MEND, "lk.silence", str(ARCANE)),
        note(70500, MEND, "lk.silence", "none"),
        note(71000, ARCANE, "lk.target", "none"),
        note(72000, MEND, "lk.silence", str(SHIV)),
        note(72500, MEND, "lk.silence", "none"),
        note(76000, ARCANE, "lk.target", tag("sphere", SPHERE)),
        note(80000, SHIV, "lk.target", "none"),
        note(84000, BASTION, "lk.taunt", tag("spirit", RAGING)),
        note(85000, BASTION, "lk.taunt", "none"),
        note(85000, SHIV, "lk.target", tag("spirit", RAGING)),
        note(118000, BULWARK, "lk.stand", "quake"),
        note(118000, SHIV, "lk.stand", "quake"),
        note(118200, ARCANE, "lk.stand", "quake"),
    ]


def phase_two() -> list[dict]:
    return [
        note(125000, BULWARK, "lk.phase", "two"),
        *(note(125000, guid, "lk.stand", "none") for guid in (BULWARK, BASTION, MEND, SHIV, ARCANE)),
        *(note(125000, guid, "lk.target", tag("boss", LICH_KING)) for guid in (BULWARK, BASTION, SHIV, ARCANE)),
        note(125500, ARCANE, "lk.stack", "p2 1"),
        note(125500, MEND, "lk.stack", "p2 2"),
        note(125500, SHIV, "lk.stack", "p2 3"),
        note(130000, ARCANE, "lk.target", tag("spirit", RAGING)),
        note(131000, SHIV, "lk.stack", "p2 spirit"),
        note(135000, SHIV, "lk.stack", "p2 3"),
        note(140000, ARCANE, "lk.brake", "1"),
        note(140000, ARCANE, "lk.target", tag("boss", LICH_KING)),
        note(141500, ARCANE, "lk.brake", "0"),
        note(150000, ARCANE, "lk.brake", "1"),
        note(151000, ARCANE, "lk.brake", "0"),
        # Two Val'kyrs: Arcane and Shiv on the first, Bastion on the second.
        note(150000, ARCANE, "lk.valkyr", tag("dps", VALKYR_A)),
        note(150000, SHIV, "lk.valkyr", tag("dps", VALKYR_A)),
        note(150000, BASTION, "lk.valkyr", tag("dps", VALKYR_B)),
        note(150000, ARCANE, "lk.target", tag("valkyr", VALKYR_A)),
        note(150000, SHIV, "lk.target", tag("valkyr", VALKYR_A)),
        note(151000, SHIV, "lk.valkyr", tag("stun", VALKYR_A)),
        note(151500, ARCANE, "lk.valkyr", tag("slow", VALKYR_A)),
        note(152000, SHIV, "lk.valkyr", tag("dps", VALKYR_A)),
        note(152500, ARCANE, "lk.valkyr", tag("dps", VALKYR_A)),
        note(153000, BASTION, "lk.valkyr", tag("stun", VALKYR_B)),
        note(153500, SHIV, "lk.valkyr", tag("round", VALKYR_A)),
        note(154000, BASTION, "lk.valkyr", tag("dps", VALKYR_B)),
        *(note(160000, guid, "lk.valkyr", "none") for guid in (ARCANE, SHIV, BASTION)),
        note(160000, ARCANE, "lk.target", tag("boss", LICH_KING)),
        note(160000, SHIV, "lk.target", tag("boss", LICH_KING)),
        # Defile on Arcane: it runs, Mend and Shiv clear off it, the human stands in the puddle.
        note(170000, ARCANE, "lk.defile", "72762"),
        note(170100, ARCANE, "lk.defilerun", "run"),
        note(170200, MEND, "lk.defilerun", "clear"),
        note(170250, SHIV, "lk.defilerun", "clear"),
        note(172000, MEND, "lk.defilerun", "none"),
        note(172000, SHIV, "lk.defilerun", "none"),
        note(172500, ARCANE, "lk.defilerun", "none"),
        dmg(176000, ARKOS, 73710, PUDDLE),
        dmg(177000, ARKOS, 73710, PUDDLE),
        note(180000, ARCANE, "lk.defilerun", tag("puddle", PUDDLE)),
        note(181000, ARCANE, "lk.defilerun", "none"),
        # Soul Reaper with no button ready: Bastion taunts and the roles flip.
        note(200000, BASTION, "lk.taunt", tag("reaper", LICH_KING)),
        note(200000, BASTION, "lk.tank", "1"),
        note(200000, BULWARK, "lk.tank", "2"),
        note(200500, BASTION, "lk.taunt", "none"),
    ]


def heroic_two() -> list[dict]:
    return [
        cheat(130000, BULWARK, "lk.empoweredblood", 2, ARKOS),
        cheat(150500, MEND, "lk.valkyrstun", 2, VALKYR_A),
        note(175000, BASTION, "lk.siphon", str(counter(VALKYR_A))),
        note(175500, BASTION, "lk.siphon", "none"),
        note(178000, BASTION, "lk.siphon", str(counter(VALKYR_B))),
        note(178500, BASTION, "lk.siphon", "none"),
    ]


def winter_two_and_three() -> list[dict]:
    return [
        note(300000, BULWARK, "lk.phase", "winter2"),
        *(note(300000, guid, "lk.stack", "none") for guid in (ARCANE, MEND, SHIV)),
        note(300500, ARCANE, "lk.stand", "winter guard 5"),
        note(305000, ARCANE, "lk.target", tag("sphere", SPHERE)),
        note(310000, BASTION, "lk.silence", str(MEND)),
        note(310500, BASTION, "lk.silence", "none"),
        note(358000, ARCANE, "lk.stand", "quake"),
        note(358000, BULWARK, "lk.side", "1"),
        note(365000, ARCANE, "lk.stand", "none"),
        note(365000, BULWARK, "lk.phase", "three"),
        note(365000, ARCANE, "lk.target", tag("boss", LICH_KING)),
        note(366000, ARCANE, "lk.stack", "p3 1 1"),
        note(366000, MEND, "lk.stack", "p3 1 2"),
        # Two Vile Spirits casts, each flipping the side; Bulwark soaks both, Arcane traps the first.
        cast(370000, LICH_KING, 70498),
        note(370200, BULWARK, "lk.vile", "soak"),
        note(370300, ARCANE, "lk.vile", "trap"),
        note(370500, BULWARK, "lk.side", "2"),
        note(371000, ARCANE, "lk.stack", "p3 2 1"),
        note(371500, ARCANE, "lk.vile", "none"),
        note(375000, SHIV, "lk.defile", "72762"),
        note(375100, SHIV, "lk.defilerun", "run"),
        dmg(376000, SHIV, 73808, VILE),
        note(377500, SHIV, "lk.defilerun", "none"),
        note(378000, BULWARK, "lk.vile", "none"),
        note(378500, MEND, "lk.defilerun", tag("puddle", PUDDLE)),
        dmg(379000, MEND, 73710, PUDDLE),
        note(379500, MEND, "lk.defilerun", "none"),
        cheat(402000, SHIV, "lk.floorsnap", 1),
        cast(419500, LICH_KING, 70498),
        note(419600, BULWARK, "lk.vile", "soak"),
        note(420000, BULWARK, "lk.side", "1"),
        note(428000, BULWARK, "lk.vile", "none"),
        note(480000, BULWARK, "lk.phase", "outro"),
        *(note(480500, guid, "lk.target", tag("outro", LICH_KING)) for guid in (BULWARK, SHIV, ARCANE)),
        cheat(200000, SHIV, "sg.flares", 1),
    ]


def heroic_frostmourne() -> list[dict]:
    return [
        note(380000, SHIV, "lk.phase", "frostmourne"),
        *(note(380000, guid, "lk.stack", "fm") for guid in (ARCANE, MEND, SHIV)),
        note(381000, BULWARK, "lk.bombdodge", "safe"),
        note(383000, BULWARK, "lk.bombdodge", "dodge"),
        note(385000, BULWARK, "lk.bombdodge", "safe"),
        note(387000, BULWARK, "lk.bombdodge", "stuck"),
        note(389000, BULWARK, "lk.bombdodge", "safe"),
        dmg(392000, ARKOS, 73805, BOMB),
        note(395000, ARCANE, "lk.target", tag("wicked", WICKED)),
        dmg(395000, MEND, 73808, WICKED),
        note(440000, SHIV, "lk.phase", "three"),
        note(440000, BULWARK, "lk.bombdodge", "none"),
        note(440000, ARCANE, "lk.stack", "p3 2 1"),
        note(440000, ARCANE, "lk.target", tag("boss", LICH_KING)),
    ]


def normal_harvest() -> list[dict]:
    return [
        aura(380000, SHIV, 74325),
        note(386500, SHIV, "lk.harvest", "warden"),
        note(390000, SHIV, "lk.harvest", "kick"),
        note(391000, SHIV, "lk.harvest", "warden"),
        note(420000, SHIV, "lk.harvest", "none"),
        aura(455000, MEND, 74325),
        note(461500, MEND, "lk.harvest", "heal"),
        note(465000, MEND, "lk.harvest", "dispel"),
        note(466000, MEND, "lk.harvest", "heal"),
        note(475000, MEND, "lk.harvest", "none"),
    ]


def stand_in() -> list[dict]:
    """One tank player: Shiv holds the ghouls through phase one."""
    return [
        note(55000, SHIV, "lk.tank", "3"),
        note(56000, SHIV, "lk.taunt", tag("add", GHOUL)),
        note(56500, SHIV, "lk.taunt", "none"),
        note(60000, SHIV, "lk.tank", "0"),
    ]


def pull(diff: int = 3) -> list[dict]:
    heroic = diff in (2, 3)
    return [
        *opening(diff),
        *phase_one(),
        *(heroic_one() if heroic else []),
        *winter_one(),
        *phase_two(),
        *(heroic_two() if heroic else []),
        *winter_two_and_three(),
        *(heroic_frostmourne() if heroic else normal_harvest()),
        {"t": 500000, "e": "end", "out": "kill"},
    ]


def phase_one_only() -> list[dict]:
    return [rec for rec in pull() if rec.get("t", 0) < 60000] + [{"t": 60000, "e": "end", "out": "wipe"}]


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


def banner(records: list[dict], declared: dict = DECLARED) -> str:
    with mock.patch.object(probes, "declared_keys", return_value=declared):
        return printed(lk.show_banner, load(records))


class Units(unittest.TestCase):
    def test_a_counter_or_a_whole_guid_names_the_unit(self):
        trace = load(pull())
        self.assertEqual(lk.unit_of(trace, str(counter(SHAMBLING))), SHAMBLING)
        self.assertEqual(lk.unit_of(trace, str(SHAMBLING)), SHAMBLING)
        self.assertEqual(lk.unit_of(trace, str(MEND), creature=False), MEND)
        self.assertIsNone(lk.unit_of(trace, "spirit"))


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
            "     7:20.000   0:40.000  three\n"
            "     8:00.000   0:20.000  outro\n"))


class Tanks(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(lk.show_tanks, load(pull())), (
            "TANKS\n"
            "  lk.tank per tank: its role, from when\n"
            "    Bastion  free 0:00.050, holder 3:20.000\n"
            "    Bulwark  holder 0:00.050, free 3:20.000\n"
            "  lk.taunt per bot: taunts by rule, the units taunted\n"
            "    Bastion  add 1, reaper 1, spirit 1  on Drudge Ghoul, Raging Spirit, The Lich King\n"
            "    Bulwark  back 1, spirit 1  on The Lich King, Raging Spirit\n"
            "  lk.brake per bot: times braked, time held\n"
            "    Arcane  2, 0:02.500\n"
            "    Shiv  1, 0:02.000\n"
            "  lk.defensive, each window: the tank, how long, what it offered and cast\n"
            "    0:50.000  Bulwark     0:03.000  offered shield wall; cast Shield Wall +0.5s\n"
            "  calls to a human tank  Shield Wall 0:50.000\n"))

    def test_a_stand_in_holds_the_ghouls(self):
        out = printed(lk.show_tanks, load(pull() + stand_in()))
        self.assertIn("    Shiv  stand-in 0:55.000, off 1:00.000\n", out)
        self.assertIn("    Shiv  add 1  on Drudge Ghoul\n", out)


class Stands(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(lk.show_stands, load(pull())), (
            "STANDS\n"
            "  lk.stand per bot: the phase one and winter stands held in order (moves between two)\n"
            "    Arcane  opening/ranged 2/winter guard 5/quake (moved 4)\n"
            "    Bastion  opening/addtank/winter tank 4/spirit (moved 3)\n"
            "    Bulwark  opening/holder/winter tank 1/spirit/quake (moved 4)\n"
            "    Mend  opening/ranged 1/winter tank 2 (moved 2)\n"
            "    Shiv  winter tank 3/quake (moved 1)\n"
            "  lk.stack per bot: the phase two, three and Frostmourne stacks held in order (moves between two)\n"
            "    Arcane  p2 1/p3 1 1/p3 2 1/fm (moved 3)\n"
            "    Mend  p2 2/p3 1 2/fm (moved 1)\n"
            "    Shiv  p2 3/p2 spirit/fm (moved 2)\n"
            "  lk.side, the phase three side: east 5:58.000 -> west 6:10.500 -> east 7:00.000\n"))


class Plague(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(lk.show_plague, load(pull())), (
            "PLAGUE\n"
            "  lk.plague per carrier: each plague's steps\n"
            "    Arcane  isolate\n"
            "    Mend  walk Shambling Horror -> delivered Shambling Horror\n"
            "    Shiv  walk Shambling Horror\n"
            "  lk.cure per bot: cures by rule, the carriers\n"
            "    Bastion  delivered 1  on Mend\n"
            "    Mend  addtank 1, emergency 1, isolated 1, totem 1  on Shiv, Bastion, Arcane\n"
            "  Necrotic Plague ticks taken: Bastion 1, Shiv 1\n"))


class Adds(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(lk.show_adds, load(pull())), (
            "ADDS\n"
            "  Shambling Enrage 2 time(s), Tranquilizing Shot cast 1 time(s), Frenzy 1 time(s)\n"
            "  lk.enrage per bot: Enrage calls, the Shamblings\n"
            "    Arcane  2  on Shambling Horror\n"
            "  lk.stun per bot: Frenzy stuns called, the units\n"
            "    Bulwark  1  on Shambling Horror\n"
            "    Shiv  1  on Shambling Horror\n"))

    def test_normal_has_no_frenzy_stuns(self):
        self.assertIn("  lk.stun: Frenzy is heroic only\n", printed(lk.show_adds, load(pull(diff=1))))


class Traps(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(lk.show_traps, load(pull())), (
            "TRAPS\n"
            "  lk.trap per bot: dodges, reach walks and holds, the traps\n"
            "    Arcane  dodge 2  on Shadow Trap\n"
            "    Mend  dodge 1  on Shadow Trap\n"
            "    Shiv  hold 1, reach 1  on Shadow Trap\n"
            "  Shadow Trap hits: Arkos human 1\n"))

    def test_normal_has_none(self):
        self.assertEqual(printed(lk.show_traps, load(pull(diff=1))), "TRAPS\n  Shadow Traps are heroic only\n")


class Winters(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(lk.show_winters, load(pull())), (
            "WINTERS\n"
            "  2 winter(s): sphere guards, sphere and spirit picks per bot from lk.target, silence dispels, "
            "the Quake hold\n"
            "\n"
            "  winter1  1:00.000  held 1:05.000\n"
            "    sphere guards  Arcane\n"
            "    sphere picks  Arcane 2\n"
            "    spirit picks  Shiv 2, Bulwark 1\n"
            "    silence dispels  Mend on Arcane, Shiv\n"
            "    Quake hold  Bulwark 1:58.000, Shiv 1:58.000, Arcane 1:58.200\n"
            "\n"
            "  winter2  5:00.000  held 1:05.000\n"
            "    sphere guards  Arcane\n"
            "    sphere picks  Arcane 1\n"
            "    spirit picks  none\n"
            "    silence dispels  Bastion on Mend\n"
            "    Quake hold  Arcane 5:58.000\n"))


class Valkyrs(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(lk.show_valkyrs, load(pull())), (
            "VALKYRS\n"
            "  lk.valkyr, each Val'kyr from its first call: its DPS bucket, then the walks round puddles, stuns and "
            "slows per bot\n"
            "     2:30.000  Val'kyr Shadowguard 976  dps Arcane, Shiv  round Shiv 1  stun Shiv 1  slow Arcane 1\n"
            "     2:30.000  Val'kyr Shadowguard 977  dps Bastion  round -  stun Bastion 1  slow -\n"
            "  lk.siphon per bot: siphon taunts, the Val'kyrs\n"
            "    Bastion  2  on Val'kyr Shadowguard\n"))

    def test_normal_has_no_siphon(self):
        self.assertNotIn("lk.siphon", printed(lk.show_valkyrs, load(pull(diff=1))))


class Defile(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(lk.show_defile, load(pull())), (
            "DEFILE\n"
            "  2 Defile cast(s): the target, its lk.defilerun steps, the others who cleared off it\n"
            "     2:50.000  Arcane  run  cleared Mend, Shiv\n"
            "     6:15.000  Shiv  run  cleared nobody\n"
            "  puddle step-outs: Arcane 1, Mend 1\n"
            "  Defile hits: Arkos human 2, Mend 1\n"))


class Vile(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(lk.show_vile, load(pull())), (
            "VILE\n"
            "  Vile Spirits cast 2 time(s), lk.side flipped 2 time(s)\n"
            "  lk.vile per bot: duties by rule\n"
            "    Arcane  trap 1\n"
            "    Bulwark  soak 2\n"
            "  Vile Spirit bursts taken: Shiv 1\n"))


class Frostmourne(unittest.TestCase):
    def test_heroic(self):
        self.assertEqual(printed(lk.show_frostmourne, load(pull())), (
            "FROSTMOURNE\n"
            "  heroic Harvest Souls in lk.phase: 6:20.000 for 1:00.000\n"
            "  lk.bombdodge per bot: bomb rules\n"
            "    Bulwark  dodge 1, safe 3, stuck 1\n"
            "  lk.stack fm, the bots that followed a leader: Arcane, Mend, Shiv\n"
            "  Spirit Bomb hits: Arkos human 1\n"
            "  Wicked Spirit bursts taken: Mend 1\n"))

    def test_normal(self):
        self.assertEqual(printed(lk.show_frostmourne, load(pull(diff=1))), (
            "FROSTMOURNE\n"
            "  Harvest Soul victims: Mend 1, Shiv 1\n"
            "  lk.harvest per bot: duties by rule\n"
            "    Mend  dispel 1, heal 2\n"
            "    Shiv  kick 1, warden 2\n"))


class Targets(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(lk.show_targets, load(pull())), (
            "TARGETS\n"
            "  lk.target per bot: spans of each resolver rule\n"
            "    Arcane  boss 6, none 1, outro 1, sphere 3, spirit 1, valkyr 1, wicked 1\n"
            "    Bastion  add 1, boss 1\n"
            "    Bulwark  boss 2, outro 1, spirit 1\n"
            "    Mend  none 1\n"
            "    Shiv  boss 4, frenzy 1, none 1, outro 1, spirit 2, valkyr 1\n"))


class Cheats(unittest.TestCase):
    def test_own_keys(self):
        self.assertEqual(printed(lk.show_cheats, load(pull())), (
            "CHEATS\n"
            "  lk.heroicbuff  4 use(s) from 0:01.000\n"
            "    by  Bulwark 4\n"
            "    on  Arkos human\n"
            "  lk.empoweredblood  2 use(s) from 2:10.000\n"
            "    by  Bulwark 2\n"
            "    on  Arkos human\n"
            "  lk.valkyrstun  2 use(s) from 2:30.500\n"
            "    by  Mend 2\n"
            "    on  Val'kyr Shadowguard\n"
            "  lk.floorsnap  1 use(s) from 6:42.000\n"
            "    by  Shiv 1\n"
            "    on  self\n"
            "  1 use(s) under other encounters' keys: icecrown_citadel.py --cheats\n"))


class Banner(unittest.TestCase):
    def test_a_heroic_pull_spares_harvest_and_the_unused_keys(self):
        self.assertIn("reached one, winter1, two, winter2, three, frostmourne, outro; all its lk.* probes present\n",
                      banner(pull()))

    def test_a_normal_pull_spares_the_heroic_keys(self):
        self.assertIn("reached one, winter1, two, winter2, three, outro; all its lk.* probes present\n",
                      banner(pull(diff=1)))

    def test_a_missing_probe_is_named(self):
        records = [rec for rec in pull() if rec.get("k") != "lk.vile"]
        self.assertIn("; probes absent from this trace: lk.vile\n", banner(records))

    def test_a_pull_that_never_left_phase_one_spares_the_later_keys(self):
        self.assertIn("reached one; all its lk.* probes present\n", banner(phase_one_only()))

    def test_a_raid_wide_key_belongs_to_every_phase(self):
        declared = {**DECLARED, "icc.state": (probes.LATCH, "ICCScripts.h:1")}
        self.assertIn("reached one; probes absent from this trace: icc.state\n", banner(phase_one_only(), declared))

    def test_no_phase_calls_nothing_absent(self):
        records = [rec for rec in pull() if rec.get("k") != "lk.phase"]
        self.assertIn("lk.phase never written, so no probe is called absent\n", banner(records))


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
