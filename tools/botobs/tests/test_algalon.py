"""Tests for the Algalon reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull pins the arithmetic behind each section, the same pull stripped of its notes shows
each section falling back to what the records alone can say, and the shared fixture shows every
section reads empty on a trace that is not Algalon rather than raising.
"""
from __future__ import annotations

import contextlib
import io
import json
import pathlib
import sys
import tempfile
import types
import unittest
from unittest import mock

BOTOBS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BOTOBS))

from bosses import algalon as al  # noqa: E402
from raidobs import probes  # noqa: E402
from raidobs.probes import HOLDER, LATCH  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v13.ndjson"

BULWARK = 5001   # tank, soaks both Big Bangs
AEGIS = 5002     # tank, the handler, holds the constellation out through Big Bang 1
MERCY = 5003     # heal
TREE = 5004      # heal
AGONY = 5005     # ranged, the star team
SHIV = 5006      # melee
TRUESHOT = 5007  # ranged, still waiting when Big Bang 1 lands
BRAWL = 5008     # melee, one-shot when the intro ends

CREATURE = 1 << 32
ALGALON = CREATURE | 1
STAR_1 = CREATURE | 11
STAR_2 = CREATURE | 12
HOLE_1 = CREATURE | 21
HOLE_2 = CREATURE | 22
CONSTELLATION = CREATURE | 31
DORMANT = CREATURE | 32
MARKER = CREATURE | 41
MARKER_TWIN = CREATURE | 42
WORM = CREATURE | 51
UNLEASHED = CREATURE | 61
PHASED_MATTER = CREATURE | 71

END = 102100

# Snapshots that carry the roster only, as on traces whose sweep anchor sat phased.
SWEEP_GAPS = ((62000, 64000), (86000, 88000))

ROSTER = [
    {"g": BULWARK, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
    {"g": AEGIS, "n": "Aegis", "r": "tank", "c": "paladin", "h": 0},
    {"g": MERCY, "n": "Mercy", "r": "heal", "c": "priest", "h": 0},
    {"g": TREE, "n": "Tree", "r": "heal", "c": "druid", "h": 0},
    {"g": AGONY, "n": "Agony", "r": "ranged", "c": "warlock", "h": 0},
    {"g": SHIV, "n": "Shiv", "r": "melee", "c": "rogue", "h": 0},
    {"g": TRUESHOT, "n": "Trueshot", "r": "ranged", "c": "hunter", "h": 0},
    {"g": BRAWL, "n": "Brawl", "r": "melee", "c": "warrior", "h": 0},
]

# Offsets in yards from ULDUAR_ALGALON_TANK_SLOT.
HOME = {BULWARK: (0, 0), AEGIS: (3, 0), MERCY: (0, 14), TREE: (8, 12), AGONY: (-10, 18), SHIV: (2, 3),
        TRUESHOT: (10, 18), BRAWL: (0, 9)}

# At 85% everyone is still above the 10-man explosion's top roll of 16,913.
MAX_HP = {BULWARK: 40000, AEGIS: 40000, MERCY: 21000, TREE: 21000, AGONY: 22000, SHIV: 24000, TRUESHOT: 22000,
          BRAWL: 24000}


def spot(dx: float, dy: float) -> tuple[float, float]:
    return al.TANK_SLOT[0] + dx, al.TANK_SLOT[1] + dy


def row(guid: int, offset, hp: float, target: int) -> list:
    x, y = spot(*offset)
    return [guid, x, y, 417.3, 0.0, hp, 80.0, target, 0, 0, 0, 0, 0, 0.0]


def player_hp(guid: int, when: int) -> float:
    if guid == TRUESHOT and when >= 48000:
        return 0.0
    if guid == BRAWL and when >= 8100:
        return 0.0
    if guid == BULWARK and 48000 <= when < 55000:
        return 35.0
    return 85.0 if 30000 <= when < 40000 else 100.0


def player_target(guid: int, when: int) -> int:
    if guid in (MERCY, TREE):
        return 0
    if guid == AGONY and 20000 <= when < 33000:
        return STAR_1 if when < 30000 else STAR_2
    if guid == TRUESHOT and 25000 <= when < 30000:
        return STAR_1
    if guid == SHIV and 30000 <= when < 34000:
        return STAR_2
    return ALGALON


# Nobody during the intro, Brawl for his first swing, Aegis on threat for a second, Shiv on threat off
# Aegis, and Mercy once the fifth stack phases Bulwark out.
BOSS_TARGETS = ((8000, 0), (8500, BRAWL), (20000, BULWARK), (21000, AEGIS), (60000, BULWARK), (75000, AEGIS),
                (77000, SHIV), (89000, BULWARK), (90000, MERCY))


def boss_target(when: int) -> int:
    return next((victim for until, victim in BOSS_TARGETS if when < until), AEGIS)


def snap(when: int) -> dict:
    units = []
    for guid, home in HOME.items():
        # Trueshot steps 12 yd off the Cosmic Smash marker dropped on him.
        offset = (22, 18) if guid == TRUESHOT and 21000 <= when < 26000 else home
        units.append(row(guid, offset, player_hp(guid, when), player_target(guid, when)))
    if any(low <= when < high for low, high in SWEEP_GAPS):
        return {"t": when, "e": "snap", "u": units, "hz": []}

    units.append(row(ALGALON, (0, 4), max(5.0, 100.0 - when / 1000.0), boss_target(when)))
    if 10000 <= when <= 29500:
        units.append(row(STAR_1, (-20, 30), 100.0 - (when - 10000) / 200.0, 0))
    if 10000 <= when <= 32500:
        units.append(row(STAR_2, (20, 30), 100.0 - (when - 10000) / 250.0, 0))
    if 30500 <= when <= 70000:
        units.append(row(HOLE_1, (-20, 30), 100.0, 0))
    if 33500 <= when <= 79500:
        units.append(row(HOLE_2, (20, 30), 100.0, 0))
    if when <= 70000:
        target = 0 if when < 36000 else AGONY if when < 38000 else AEGIS
        units.append(row(CONSTELLATION, (-25, 30) if when < 69000 else (-20, 30), 100.0, target))
    # Beside HOLE_1, and out of the sweep over the very scans the hole leaves in.
    if when <= 69500 or 72000 <= when <= 79500:
        units.append(row(DORMANT, (-18, 30), 100.0, 0))
    if 20000 <= when <= 24500:
        units.append(row(MARKER, HOME[TRUESHOT], 100.0, 0))
        units.append(row(MARKER_TWIN, HOME[TRUESHOT], 100.0, 0))
    if 44000 <= when <= 60000:
        units.append(row(PHASED_MATTER, (-20, 32), 100.0, 0))
    # The Ascend despawns the room before the evade closes the file.
    if 80000 <= when <= 99500:
        units.append(row(WORM, (0, 10), 100.0, 0))
    if 85000 <= when <= 99500:
        units.append(row(UNLEASHED, (5, 5), 100.0, AGONY if when < 90000 else AEGIS))
    return {"t": when, "e": "snap", "u": units, "hz": []}


def note(when: int, guid: int, key: str, text) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": str(text)}


def aura(when: int, guid: int, spell: int, removed: bool = False, stacks: int = 1) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": ALGALON, "sp": spell, "r": int(removed), "st": stacks,
            "dur": 0 if removed else 10000, "p": 0}


def phased(guid: int, spell: int, start: int, stop: int, pulses_until: int | None = None) -> list[dict]:
    """Applied at `start`, refreshed every second by the field until `pulses_until` (by default up to
    the removal), removed at `stop`."""
    last = stop - 1 if pulses_until is None else pulses_until
    return ([aura(start, guid, spell)] + [aura(when, guid, spell) for when in range(start + 1000, last + 1, 1000)]
            + [aura(stop, guid, spell, True)])


def dmg(when: int, source: int, victim: int, spell: int, amount: int, hp: float = 100.0, over: int = 0) -> dict:
    rec = {"t": when, "e": "dmg", "s": source, "d": victim, "sp": spell, "a": amount, "hp": hp}
    if over:
        rec["ok"] = over
    return rec


def cast(when: int, caster: int, spell: int, target: int = 0, cast_ms: int = 0) -> dict:
    return {"t": when, "e": "cast", "s": caster, "sp": spell, "tgt": target, "ct": cast_ms}


def move(when: int, guid: int, by: str, ok: int = 1) -> dict:
    return {"t": when, "e": "move", "g": guid, "k": "point", "x": 0.0, "y": 0.0, "z": 417.3, "ok": ok, "r": "",
            "by": by, "pr": 0}


def death(when: int, guid: int, killer: int, amount: int, offset) -> dict:
    x, y = spot(*offset)
    return {"t": when, "e": "death", "g": guid, "killer": killer, "blow": [killer, amount], "x": x, "y": y}


def probe_notes(handler: int) -> list[dict]:
    rows = [
        note(0, MERCY, "algalon.phase", 1), note(8000, MERCY, "algalon.phase", 2),
        note(80000, MERCY, "algalon.phase", 3),
        note(8000, AEGIS, "algalon.handler", handler),
        note(8000, AGONY, "algalon.starteam", 1),
        note(8000, AGONY, "algalon.focusstar", 0), note(20000, AGONY, "algalon.focusstar", STAR_1),
        note(30000, AGONY, "algalon.focusstar", STAR_2), note(33000, AGONY, "algalon.focusstar", 0),
        note(8000, AGONY, "algalon.starwindow", "none"), note(20000, AGONY, "algalon.starwindow", "open"),
        note(30000, AGONY, "algalon.starwindow", "gap"), note(38000, AGONY, "algalon.starwindow", "open"),
        note(80000, AGONY, "algalon.starwindow", "none"),
        note(8000, MERCY, "algalon.holes", 0), note(30500, MERCY, "algalon.holes", 1),
        note(33500, MERCY, "algalon.holes", 2), note(70000, MERCY, "algalon.holes", 1),
        note(8000, MERCY, "algalon.urgent", 0), note(10000, MERCY, "algalon.urgent", 1),
        note(30500, MERCY, "algalon.urgent", 0),
        note(8000, MERCY, "algalon.bigbang", 0), note(40000, MERCY, "algalon.bigbang", 1),
        note(48000, MERCY, "algalon.bigbang", 0), note(90000, MERCY, "algalon.bigbang", 2),
        note(98000, MERCY, "algalon.bigbang", 0),
        note(8000, MERCY, "algalon.soaker", 0), note(40000, MERCY, "algalon.soaker", BULWARK),
        note(49000, MERCY, "algalon.soaker", 0), note(90000, MERCY, "algalon.soaker", BULWARK),
        note(98500, MERCY, "algalon.soaker", 0),
        note(45000, BULWARK, "algalon.defensive", "shield wall"),
        note(70000, AEGIS, "algalon.holelost", "kite"), note(80000, MERCY, "algalon.holelost", "phase2"),
        note(40000, BULWARK, "algalon.hide", "soak"), note(49000, BULWARK, "algalon.hide", "none"),
        note(90000, BULWARK, "algalon.hide", "soak"),
        note(40000, AEGIS, "algalon.hide", "hold"), note(49000, AEGIS, "algalon.hide", "none"),
        note(40000, TRUESHOT, "algalon.hide", "wait"), note(48000, TRUESHOT, "algalon.hide", "none"),
    ]
    if handler:
        rows += [note(36000, AEGIS, "algalon.kite", "taunt"), note(38000, AEGIS, "algalon.kite", "kept"),
                 note(62000, AEGIS, "algalon.kite", "hole"), note(70000, AEGIS, "algalon.kite", "none")]
    for guid, ran, hid in ((AGONY, 42500, 44000), (SHIV, 42500, 44000), (MERCY, 43000, 45000), (TREE, 44500, 46000)):
        rows += [note(40000, guid, "algalon.hide", "wait"), note(ran, guid, "algalon.hide", "run"),
                 note(hid, guid, "algalon.hide", "in"), note(57000, guid, "algalon.hide", "none")]
    for guid, slot in ((MERCY, 0), (TREE, 1), (AGONY, 4), (TRUESHOT, 5)):
        rows += [note(8000, guid, "algalon.slot", slot), note(8000, guid, "algalon.spot", "slot")]
    rows += [note(20000, TRUESHOT, "algalon.spot", "smash"), note(26000, TRUESHOT, "algalon.spot", "slot")]
    rows += [note(0, SHIV, "algalon.spot", "stage"), note(6000, SHIV, "algalon.spot", "none"),
             note(0, BRAWL, "algalon.spot", "stage")]
    rows += [note(8000, BULWARK, "algalon.swappair", 1), note(8000, AEGIS, "algalon.swappair", 1),
             note(8000, MERCY, "algalon.pull", f"inreach=4 victim={BRAWL}"),
             note(40000, MERCY, "algalon.call", "bigbang"), note(90000, MERCY, "algalon.call", "bigbang"),
             note(30000, AGONY, "algalon.stardeath", "killed 15.6"),
             note(33000, AGONY, "algalon.stardeath", "collapsed -"),
             note(20400, TRUESHOT, "algalon.dodge", "clear"),
             note(20000, TRUESHOT, "algalon.hazard", "smash"), note(26000, TRUESHOT, "algalon.hazard", "none"),
             note(40000, BULWARK, "algalon.soakkit", "def=shield wall forbearance=0"),
             note(90000, BULWARK, "algalon.soakkit", "def=none forbearance=0")]
    for guid, changes in ((BULWARK, ((8000, "hold"), (59000, "yield"), (76800, "take"), (77000, "hold"),
                                     (89000, "out"))),
                          (AEGIS, ((8000, "yield"), (59800, "take"), (60000, "hold"), (77000, "yield"),
                                   (89900, "take"), (90000, "hold")))):
        rows += [note(when, guid, "algalon.tankrole", value) for when, value in changes]
    return rows


def algalon_pull(with_notes: bool = True, handler: int = AEGIS) -> list[dict]:
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 603, "inst": 4, "diff": 0,
         "boss": "algalon-the-observer", "roster": ROSTER},
        {"t": 0, "e": "pull", "boss": "algalon-the-observer", "src": "engage"},
        {"t": 1, "e": "unit", "g": ALGALON, "en": al.NPC_ALGALON, "n": "Algalon the Observer", "b": 1},
        {"t": 1, "e": "unit", "g": STAR_1, "en": al.NPC_COLLAPSING_STAR, "n": "Collapsing Star"},
        {"t": 1, "e": "unit", "g": STAR_2, "en": al.NPC_COLLAPSING_STAR, "n": "Collapsing Star"},
        {"t": 1, "e": "unit", "g": HOLE_1, "en": al.NPC_BLACK_HOLE, "n": "Black Hole"},
        {"t": 1, "e": "unit", "g": HOLE_2, "en": al.NPC_BLACK_HOLE, "n": "Black Hole"},
        {"t": 1, "e": "unit", "g": CONSTELLATION, "en": al.NPC_LIVING_CONSTELLATION, "n": "Living Constellation"},
        {"t": 1, "e": "unit", "g": DORMANT, "en": al.NPC_LIVING_CONSTELLATION, "n": "Living Constellation"},
        {"t": 1, "e": "unit", "g": MARKER, "en": al.NPC_COSMIC_SMASH_MARKERS[0], "n": "Algalon Asteroid Target"},
        {"t": 1, "e": "unit", "g": MARKER_TWIN, "en": al.NPC_COSMIC_SMASH_MARKERS[1],
         "n": "Algalon Asteroid Target"},
        {"t": 1, "e": "unit", "g": WORM, "en": al.NPC_WORM_HOLE, "n": "Worm Hole"},
        {"t": 1, "e": "unit", "g": UNLEASHED, "en": al.NPC_UNLEASHED_DARK_MATTER, "n": "Unleashed Dark Matter"},
        {"t": 1, "e": "unit", "g": PHASED_MATTER, "en": al.NPC_DARK_MATTER, "n": "Dark Matter"},
        *({"t": 1, "e": "unit", "g": member["g"], "n": member["n"], "mhp": MAX_HP[member["g"]]} for member in ROSTER),

        # His first swing one-shots Brawl, who stood in his reach when the intro ended.
        dmg(8100, ALGALON, BRAWL, 0, 40000, over=16000),
        death(8100, BRAWL, ALGALON, 40000, HOME[BRAWL]),

        # Aegis steals him on threat at 20 s and Bulwark taunts him back. Then taunts for the swap, off
        # Shiv, and off Mercy after Bulwark's fifth stack.
        cast(20900, BULWARK, 355, ALGALON), cast(59800, AEGIS, 62124, ALGALON),
        cast(76800, BULWARK, 355, ALGALON), cast(89900, AEGIS, 62124, ALGALON),
        dmg(89400, ALGALON, MERCY, 0, 15000),

        # Trueshot dodges the meteor dropped on him, then the ring walks him back inside 15 yd.
        move(20400, TRUESHOT, al.SMASH_MOVER), move(22000, TRUESHOT, "algalon raid position action"),
        move(64000, TREE, "algalon raid position action"),

        cast(9000, ALGALON, al.SPELL_QUANTUM_STRIKE[0], BULWARK),
        dmg(9000, ALGALON, BULWARK, al.SPELL_QUANTUM_STRIKE[0], 27000),
        dmg(76000, ALGALON, SHIV, al.SPELL_QUANTUM_STRIKE[0], 27000),

        # Phase Punch climbs on Bulwark, the fifth stack phases him out a second before Big Bang 2.
        aura(10000, BULWARK, al.SPELL_PHASE_PUNCH, stacks=1), aura(25500, BULWARK, al.SPELL_PHASE_PUNCH, stacks=2),
        aura(41000, BULWARK, al.SPELL_PHASE_PUNCH, stacks=3), aura(56500, BULWARK, al.SPELL_PHASE_PUNCH, stacks=4),
        aura(61000, AEGIS, al.SPELL_PHASE_PUNCH, stacks=1), aura(89000, BULWARK, al.SPELL_PHASE_PUNCH, stacks=5),

        dmg(24800, ALGALON, TREE, al.SPELL_COSMIC_SMASH[0], 9000),

        # Two stars 3 s apart.
        dmg(30000, STAR_1, BULWARK, al.SPELL_BLACK_HOLE_EXPLOSION[0], 16000),
        dmg(30000, STAR_1, MERCY, al.SPELL_BLACK_HOLE_EXPLOSION[0], 16000),
        dmg(30000, STAR_1, AGONY, al.SPELL_BLACK_HOLE_EXPLOSION[0], 16000),
        dmg(33000, STAR_2, BULWARK, al.SPELL_BLACK_HOLE_EXPLOSION[0], 16000, hp=85.0),
        dmg(33000, STAR_2, TREE, al.SPELL_BLACK_HOLE_EXPLOSION[0], 16000, hp=85.0),

        # Big Bang 1: Bulwark soaks it, Aegis holds the constellation out, Trueshot is still waiting
        # and dies. 64445 strips every phase at 49 s, and Shiv, still in the hole, is phased again.
        cast(40000, ALGALON, al.SPELL_BIG_BANG[0], cast_ms=8000),
        *phased(AGONY, al.SPELL_BLACK_HOLE_PHASE, 44000, 49000),
        *phased(SHIV, al.SPELL_BLACK_HOLE_PHASE, 44000, 49000),
        aura(49000, SHIV, al.SPELL_BIG_BANG_UNPHASE),
        *phased(SHIV, al.SPELL_BLACK_HOLE_PHASE, 49500, 60500, pulses_until=50500),
        *phased(MERCY, al.SPELL_BLACK_HOLE_PHASE, 45000, 49000),
        *phased(TREE, al.SPELL_BLACK_HOLE_PHASE, 46000, 49000),
        cast(46000, MERCY, al.SPELL_PAIN_SUPPRESSION, BULWARK),
        dmg(47000, PHASED_MATTER, TREE, 0, 3000),
        dmg(48000, ALGALON, BULWARK, al.SPELL_BIG_BANG[0], 45000, hp=100.0),
        dmg(48000, ALGALON, AEGIS, al.SPELL_BIG_BANG[0], 40000, hp=100.0),
        dmg(48000, ALGALON, TRUESHOT, al.SPELL_BIG_BANG[0], 76000, hp=100.0, over=50000),
        {"t": 48000, "e": "death", "g": TRUESHOT, "killer": ALGALON, "blow": [ALGALON, 76000],
         "x": spot(10, 18)[0], "y": spot(10, 18)[1]},

        # Tree crosses HOLE_2 outside any Big Bang: one pulse keeps him phased 11 s.
        *phased(TREE, al.SPELL_BLACK_HOLE_PHASE, 65000, 76000, pulses_until=66000),

        dmg(86000, UNLEASHED, AGONY, 0, 5000),
        dmg(91000, UNLEASHED, AEGIS, 0, 4000),

        # Big Bang 2 with the soaker punched out and everyone else in the Worm Hole: it hits nobody,
        # and 64445 strips 64417 along with the Worm Hole phase.
        aura(89000, BULWARK, al.SPELL_PHASE_PUNCH_PHASE), aura(99000, BULWARK, al.SPELL_PHASE_PUNCH_PHASE, True),
        cast(90000, ALGALON, al.SPELL_BIG_BANG[0], cast_ms=8000),
        *(rec for guid in (MERCY, TREE, AGONY, SHIV) for rec in phased(guid, al.SPELL_WORM_HOLE_PHASE, 93000, 99000)),
        *phased(AEGIS, al.SPELL_WORM_HOLE_PHASE, 94000, 99000),
        cast(98300, ALGALON, al.SPELL_ASCEND),
        {"t": END, "e": "end", "out": "reset"},
    ]
    if with_notes:
        x, y = spot(*HOME[TRUESHOT])
        records += probe_notes(handler) + [
            {"t": 20000, "e": "haz", "sp": al.SPELL_COSMIC_SMASH[0], "shape": "circle", "x": x, "y": y, "z": 417.3,
             "ttl": 5500, "rad": 6.0}]
    records += [snap(when) for when in range(0, END, 500)]
    return records


DECLARED = {
    "algalon.phase": (LATCH, "fixture"),
    "algalon.backup": (LATCH, "fixture"),
    "algalon.kitehole": (HOLDER, "fixture"),
    "algalon.hide": (HOLDER, "fixture"),
    "vezax.slot": (HOLDER, "fixture"),
}


def load(folder: str, records: list[dict]) -> Trace:
    path = pathlib.Path(folder) / "603_4_algalon-the-observer_1789500000.ndjson"
    path.write_text("\n".join(json.dumps(rec) for rec in records) + "\n", encoding="utf-8")
    return Trace(path)


def render(trace: Trace, *sections: str) -> str:
    out = io.StringIO()
    with mock.patch.object(probes, "declared_keys", return_value=DECLARED), contextlib.redirect_stdout(out):
        al.show_banner(trace)
        for flag, _, section in al.SECTIONS:
            if not sections or flag in sections:
                section(trace)
    return out.getvalue()


class SyntheticPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, algalon_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_phase_spans_come_from_the_probe(self):
        self.assertEqual(al.phases(self.trace),
                         ([(0, 8000, "intro"), (8000, 80000, "P1"), (80000, END, "P2")], "algalon.phase"))

    def test_the_banner_names_only_algalon_keys_this_pull_never_wrote(self):
        with mock.patch.object(probes, "declared_keys", return_value=DECLARED):
            self.assertEqual(al.missing_probes(self.trace), ["algalon.backup", "algalon.kitehole"])
        text = render(self.trace, "phases")
        self.assertIn("algalon  10-man normal  reset at 1:42.100", text)
        self.assertIn("probes absent from this trace: algalon.backup, algalon.kitehole", text)

    def test_big_bang_names_the_soaker_and_the_one_who_stayed_out(self):
        first = al.big_bang_rows(self.trace)[0]
        self.assertEqual((first["start"], first["end"], first["src"]), (40000, 48000, "cast"))
        self.assertEqual(first["holes"], [HOLE_1, HOLE_2])
        self.assertEqual((first["soaker"], first["soaker_from"], first["backup"]), (BULWARK, "algalon.soaker", 0))
        self.assertEqual(first["soaker_hp"], (100.0, 35.0))
        self.assertIsNone(first["soaker_death"])
        self.assertEqual(first["stray"], [TRUESHOT])
        self.assertEqual(first["killed"], {TRUESHOT})
        self.assertEqual(first["externals"], [(46000, MERCY, al.SPELL_PAIN_SUPPRESSION, BULWARK)])
        self.assertEqual(first["defensives"], [(45000, BULWARK, "shield wall")])
        self.assertEqual(first["matter"], {TREE: 3000})

    def test_a_handler_on_hold_may_take_big_bang(self):
        first = al.big_bang_rows(self.trace)[0]
        self.assertEqual(set(first["victims"]), {BULWARK, AEGIS, TRUESHOT})
        self.assertEqual(first["holding"], {AEGIS})
        self.assertNotIn(AEGIS, first["stray"])
        self.assertEqual(first["hide"], {"in": 4, "soak": 1, "hold": 1, "wait": 1})

    def test_wait_reads_how_long_before_the_impact_each_bot_set_off(self):
        first = al.big_bang_rows(self.trace)[0]
        self.assertEqual(first["waited"], {AGONY: 5500, SHIV: 5500, MERCY: 5000, TREE: 3500, TRUESHOT: None})
        self.assertIn("left the wait: 4, the latest Tree 3.5 s before the impact; still waiting at the impact:"
                      " Trueshot", render(self.trace, "bigbang"))

    def test_64445_ends_every_phase_a_second_after_the_impact(self):
        spans = al.phased_spans(self.trace)
        self.assertEqual(spans[MERCY], [[45000, 49000, 1], [93000, 99000, 1]])
        self.assertEqual(spans[BULWARK], [[89000, 99000, 1]])
        first = al.big_bang_rows(self.trace)[0]
        self.assertEqual(first["back"] - first["end"], 1000)
        self.assertFalse(first["soaker_phased"])
        self.assertIsNone(first["nobody"])
        self.assertIsNone(first["ended"])

    def test_a_big_bang_that_hits_nobody_is_followed_by_the_evade(self):
        second = al.big_bang_rows(self.trace)[1]
        self.assertEqual(second["holes"], [WORM])
        self.assertEqual(second["victims"], {})
        self.assertTrue(second["soaker_phased"])
        self.assertEqual(second["back"], 99000)
        self.assertEqual(second["nobody"], 98000)
        self.assertEqual((second["ascend"], second["ended"]), (98300, (END, "reset")))

    def test_field_refreshes_are_not_applications(self):
        self.assertEqual(al.phased_spans(self.trace, al.HOLE_PHASE)[AGONY], [[44000, 49000, 1], [93000, 99000, 1]])
        self.assertEqual(al.rephase_loops(self.trace), [
            {"guid": SHIV, "start": 44000, "stop": 60500, "applications": 2},
            {"guid": TREE, "start": 65000, "stop": 76000, "applications": 1}])

    def test_star_deaths_carry_their_spacing_and_raid_health(self):
        deaths = al.star_deaths(self.trace)
        self.assertEqual([(d["t"], d["star"], d["how"], d["hits"], d["gap"], d["raid_min"]) for d in deaths],
                         [(30000, STAR_1, "explosion", 3, None, 100.0), (33000, STAR_2, "explosion", 2, 3000, 85.0)])

    def test_ranged_on_a_star_while_urgent_are_on_the_team(self):
        team = al.star_team(self.trace)
        self.assertEqual((team["members"], team["drafted"]), ([AGONY], {TRUESHOT}))
        self.assertEqual((team["on"][AGONY], team["open"][AGONY]), (13000, 13000))
        self.assertEqual((team["on"][TRUESHOT], team["open"][TRUESHOT]), (5000, 10500))
        self.assertEqual(dict(team["off"]), {SHIV: 4000})

    def test_hole_stock_bridges_scans_the_sweep_missed(self):
        samples = al.samples_of(self.trace)
        self.assertNotIn(HOLE_1, samples.frame(62000))
        self.assertEqual(al.holes_at(samples, al.hole_guids(self.trace), 62000), [HOLE_1, HOLE_2])
        self.assertEqual(al.hole_stock(self.trace), [(0, 0), (30500, 1), (33500, 2), (70500, 1), (100000, 0)])
        counts = al.hole_counts(self.trace)
        self.assertEqual(al.empty_before(self.trace, counts, 40000), 20500)
        self.assertEqual(al.empty_before(self.trace, counts, 90000), 0)

    def test_the_constellation_left_through_the_hole_it_was_dragged_to(self):
        row = {row["guid"]: row for row in al.constellation_rows(self.trace)}[CONSTELLATION]
        self.assertEqual((row["active"], row["removed"], row["how"], row["spent"]), (36000, 70000, "hole", [HOLE_1]))
        self.assertEqual(row["victims"], [(36000, AGONY), (38000, AEGIS)])
        self.assertEqual((row["last_victim"], row["handler"]), (AEGIS, AEGIS))

    def test_one_that_drops_out_with_the_hole_and_comes_back_was_not_removed_through_it(self):
        row = {row["guid"]: row for row in al.constellation_rows(self.trace)}[DORMANT]
        self.assertEqual((row["active"], row["removed"], row["how"], row["spent"]), (None, 79500, "phase2", []))

    def test_a_meteor_wave_times_the_dodge_and_names_the_walk_back(self):
        self.assertEqual(al.meteors(self.trace)[1], "haz")
        (wave,) = al.smash_waves(self.trace)
        self.assertEqual((wave["seen"], wave["impact"], len(wave["meteors"])), (20000, 24800, 1))
        rows = {row["guid"]: row for row in al.wave_rows(self.trace, wave)}
        self.assertNotIn(BULWARK, rows)
        trueshot = rows[TRUESHOT]
        self.assertAlmostEqual(trueshot["at_seen"], 0.0)
        self.assertAlmostEqual(trueshot["at_impact"], 12.0)
        self.assertEqual((trueshot["moved"], trueshot["dodge"], trueshot["back"]),
                         (400, "clear", "algalon raid position action"))
        self.assertEqual(rows[TREE]["damage"], 9000)
        self.assertEqual(al.band(rows[TREE]["at_impact"]), "6-10")

    def test_tanks_read_the_punch_the_swap_and_the_pickup(self):
        self.assertEqual(al.punched_out(self.trace), [(89000, BULWARK)])
        self.assertEqual(al.boss_victims(self.trace), [
            (0, 0), (8000, BRAWL), (8500, BULWARK), (20000, AEGIS), (21000, BULWARK), (60000, AEGIS),
            (75000, SHIV), (77000, BULWARK), (89000, MERCY), (90000, AEGIS)])
        self.assertEqual(al.pickups(self.trace), [(8000, 8500, BRAWL), (75000, 77000, SHIV), (89000, 90000, MERCY)])
        self.assertEqual(max(count for _, count, _ in al.phase_punch(self.trace)[BULWARK]), 5)

    def test_formation_measures_each_ring_bot_from_its_slot_and_from_him(self):
        rows = {row["guid"]: row for row in al.ring_rows(self.trace)}
        self.assertEqual(set(rows), {MERCY, TREE, AGONY, TRUESHOT})
        self.assertEqual(rows[MERCY]["slot"], 0)
        self.assertAlmostEqual(rows[MERCY]["to_hold"], al.dist2(spot(0, 14), al.HOLD_SPOT))
        self.assertAlmostEqual(rows[MERCY]["to_boss"], 10.0)
        self.assertEqual((rows[MERCY]["in_reach"], rows[AGONY]["in_reach"]), (1.0, 0.0))
        if al.ring_slots():
            self.assertAlmostEqual(rows[MERCY]["to_slot"], al.dist2(spot(0, 14), al.slot_position(0)))

    def test_dark_matter_stays_loose_across_a_scan_it_missed(self):
        (row,) = al.dark_matter_rows(self.trace)
        self.assertEqual((row["spawn"], dict(row["loose"]), row["loose_from"]), (85000, {AGONY: 5000}, 85000))
        self.assertEqual((row["on_handler"], row["elsewhere"]), (4000, 5000))

    def test_the_room_despawning_after_the_ascend_is_not_a_kill(self):
        self.assertEqual(al.final_despawn(self.trace), 99500)
        (row,) = al.dark_matter_rows(self.trace)
        self.assertEqual((row["killed"], row["despawned"]), (None, 99500))

    def test_the_pull_names_who_stood_in_his_reach_and_whom_he_took(self):
        row = al.pull_row(self.trace)
        self.assertEqual((row["t"], row["probe"], row["inreach"], row["victim"]), (8000, True, 4, BRAWL))
        self.assertEqual(row["inside"], [SHIV, BRAWL, MERCY, TREE])
        self.assertEqual(row["tanked"], (8500, BULWARK))
        self.assertEqual([rec["g"] for rec in row["deaths"]], [BRAWL])
        self.assertEqual(row["stage"], {SHIV: 0.75, BRAWL: 1.0})

    def test_loose_windows_carry_their_cause_and_the_pickup(self):
        windows = al.loose_windows(self.trace)
        self.assertEqual([(w["start"], w["stop"], w["cause"], w["detail"]) for w in windows], [
            (8000, 8500, "intro end", ""), (75000, 77000, "threat", "off Aegis"),
            (89000, 90000, "5th stack", "Bulwark")])
        self.assertEqual([[rec["d"] for rec in w["hits"]] for w in windows], [[BRAWL], [SHIV], [MERCY]])
        self.assertEqual([[rec["g"] for rec in w["deaths"]] for w in windows], [[BRAWL], [], []])
        self.assertEqual([(w["ender"], w["taunt"]) for w in windows],
                         [(BULWARK, None), (BULWARK, 76800), (AEGIS, 89900)])
        self.assertIn("3 window(s), 3.5 s loose, 1 death(s) while loose, 1 of them his", render(self.trace, "loose"))

    def test_swaps_time_the_due_swap_and_find_the_threat_steal(self):
        self.assertEqual(al.swap_pair(self.trace), (frozenset({BULWARK, AEGIS}), "algalon.swappair"))
        self.assertEqual(al.swap_dues(self.trace),
                         [{"t": 56500, "tank": BULWARK, "taken": 60000, "by": AEGIS, "missed": None}])
        self.assertEqual(al.threat_steals(self.trace), [(20000, BULWARK, AEGIS)])
        text = render(self.trace, "swaps")
        self.assertIn("swap due 0:56.500: Bulwark at 4 stacks, Aegis had him 3.5 s later", text)
        self.assertIn("algalon.call: 0:40.000 bigbang (Mercy), 1:30.000 bigbang (Mercy)", text)
        self.assertIn("algalon.tankrole Bulwark: hold 63.0 s", text)

    def test_reach_splits_the_time_the_pair_held_him_from_the_loose_time(self):
        rows = al.reach_rows(self.trace)
        self.assertEqual(set(rows), {SHIV, BRAWL, MERCY, TREE})
        self.assertEqual(rows[BRAWL], [0, 500])
        # Tree is still phased when Shiv's window opens
        self.assertEqual((rows[SHIV][1], rows[MERCY][1], rows[TREE][1]), (3500, 3500, 2500))

    def test_phasing_tells_big_bang_from_stray_and_names_the_stray_cause(self):
        rows = al.phasing_rows(self.trace)
        stray = [(row["guid"], row["start"], row["cause"]) for row in rows if row["label"] == "stray"]
        self.assertEqual(stray, [(SHIV, 49500, "re-phase"), (TREE, 65000, "walked in (algalon raid position action)")])
        self.assertEqual(sum(1 for row in rows if row["label"] == "Big Bang"), 9)
        tree = next(row for row in rows if row["guid"] == TREE and row["start"] == 46000)
        self.assertEqual((tree["matter"], tree["died"]), (3000, False))

    def test_stars_carry_what_each_death_landed_into(self):
        first, second = al.star_rows(self.trace)
        self.assertEqual((first["note"], first["window"], first["meteor"], first["bang"], first["below"]),
                         (("killed", 15.6), "open", False, None, []))
        self.assertEqual(first["nearest"][1], AGONY)
        self.assertAlmostEqual(first["nearest"][0], 15.62, places=2)
        self.assertEqual((second["note"], second["window"], second["weakest"]), (("collapsed", None), "gap",
                                                                                 (17850, MERCY, 85.0)))
        self.assertEqual(second["below"], [])

    def test_deaths_are_read_by_the_mechanic_that_killed(self):
        self.assertEqual([(row["guid"], row["cause"]) for row in al.death_rows(self.trace)],
                         [(BRAWL, "Algalon melee (intro end)"), (TRUESHOT, "Big Bang")])

    def test_the_verdict_lists_every_failure_in_time_order(self):
        self.assertEqual([line for _, line in al.failures(self.trace)], [
            "4 player(s) inside 11.83 yd of him when the intro ended @ 8.0s (Shiv, Brawl, Mercy, Tree)",
            "Algalon loose 0.5 s @ 8.0s (intro end): 1 death",
            "star chewed: star 1 lost 40.0% beyond its drain while not the focus star, 40.0% of it in 10 s from 10.0s",
            "star chewed: star 2 lost 60.0% beyond its drain while not the focus star, 30.0% of it in 10 s from 10.0s",
            "threat steal @ 20.0s: Aegis pulled him off Bulwark with no taunt",
            "Cosmic Smash @ 24.8s: inside 10 yd at the impact: Tree 6.3",
            "Cosmic Smash @ 24.8s: dodged, then walked back inside 15 yd by the impact: Trueshot 12.0"
            " (algalon raid position action)",
            "Shiv on stars for 4.0 s outside the star team, from 30.0s",
            "stars died 3.0 s apart @ 33.0s",
            "Big Bang 1 @ 40.0s: 1 non-soaker hit unphased (Trueshot)",
            "re-phase loop: Shiv @ 44.0s (2 applications, 16.5 s phased)",
            "re-phase loop: Tree @ 65.0s (1 application, 11.0 s phased)",
            "Algalon loose 2.0 s @ 75.0s (threat, off Aegis): 0 deaths",
            "Algalon on Shiv (melee) for 2.0 s @ 75.0s",
            "Unleashed Dark Matter loose for 5.0 s (mostly on Agony) @ 85.0s",
            "Algalon loose 1.0 s @ 89.0s (5th stack, Bulwark): 0 deaths",
            "Phase Punch phased Bulwark out @ 89.0s",
            "Algalon on Mercy (heal) for 1.0 s @ 89.0s",
            "Big Bang 2 @ 90.0s: soaker Bulwark phased at the impact",
            "Big Bang 2 @ 90.0s: nobody alive and unphased within 120 yd at +0.0 s",
            "evade 4.1 s after Big Bang 2 (Ascend at +0.3 s, pull ended reset)",
        ])

    def test_every_section_renders(self):
        text = render(self.trace)
        self.assertIn("hit unphased: Aegis (hold) 40,000, Bulwark (soaker) 45,000, Trueshot (ranged) 76,000 DIED",
                      text)
        self.assertIn("1 non-soaker(s) hit: Trueshot (hide wait)", text)
        self.assertIn("tank alone 1.0 s", text)
        self.assertIn("re-phase loop: Shiv 2 applications, 16.5 s phased from 0:44.000", text)
        self.assertRegex(text, r"0:33\.000 Collapsing Star +explosion +2 +32,000 +3\.0s +85% <8s")
        self.assertIn("+Trueshot (ranged)", text)
        self.assertIn("despawned 1:39.500", text)
        self.assertIn("absent from this trace: algalon.backup", text)
        self.assertIn("evade 4.1 s after Big Bang 2", text)
        self.assertEqual(text.count("wave 0:20.000: 1 meteor(s), impact 0:24.800"), 1)
        self.assertNotIn("no failures detected", text)
        self.assertIn("this trace predates the edge hold", text)
        self.assertIn("kit: def=shield wall", text)


class HumanHandler(unittest.TestCase):
    """A human off-tank: no bot is ever elected handler, so nobody kites."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, algalon_pull(handler=0))

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_kite_is_not_expected_without_a_handler(self):
        self.assertIn("algalon.handler", al.expected_probes(self.trace))
        self.assertNotIn("algalon.kite", al.expected_probes(self.trace))
        self.assertFalse(any(line.startswith("missing probes") for _, line in al.failures(self.trace)))

    def test_any_tank_holds_dark_matter_when_there_is_no_handler(self):
        (row,) = al.dark_matter_rows(self.trace)
        self.assertEqual((dict(row["loose"]), row["on_handler"]), ({AGONY: 5000}, 4000))


class WithoutProbes(unittest.TestCase):
    """The same pull as a trace from before the probes existed."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, algalon_pull(with_notes=False))

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_phases_fall_back_to_his_first_cast_and_twenty_percent(self):
        self.assertEqual(al.phases(self.trace),
                         ([(0, 9000, "intro"), (9000, 80000, "P1"), (80000, END, "P2")], "fallback"))

    def test_the_soaker_falls_back_to_the_tank_he_was_hitting(self):
        first = al.big_bang_rows(self.trace)[0]
        self.assertEqual((first["soaker"], first["soaker_from"]), (BULWARK, "his victim"))
        # without algalon.hide nothing says Aegis was holding
        self.assertEqual(first["stray"], [AEGIS, TRUESHOT])

    def test_the_verdict_names_the_probes_the_events_imply(self):
        lines = [line for _, line in al.failures(self.trace)]
        self.assertEqual(lines[0], "missing probes: algalon.bigbang, algalon.dodge, algalon.handler,"
                                   " algalon.hazard, algalon.hide, algalon.holes, algalon.phase, algalon.pull,"
                                   " algalon.slot, algalon.soaker, algalon.soakkit, algalon.spot, algalon.stardeath,"
                                   " algalon.starwindow, algalon.swappair, algalon.tankrole")
        self.assertIn("Big Bang 1 @ 40.0s: 2 non-soakers hit unphased (Aegis, Trueshot)", lines)
        self.assertIn("evade 4.1 s after Big Bang 2 (Ascend at +0.3 s, pull ended reset)", lines)

    def test_the_intro_end_and_the_pair_fall_back_to_his_first_victim_and_the_roster(self):
        self.assertEqual(al.swap_pair(self.trace), (frozenset({BULWARK, AEGIS}), "roster tanks"))
        row = al.pull_row(self.trace)
        self.assertEqual((row["t"], row["probe"], row["inreach"], row["victim"]), (8000, False, None, BRAWL))
        self.assertEqual([w["cause"] for w in al.loose_windows(self.trace)], ["intro end", "threat", "5th stack"])

    def test_meteors_fall_back_to_the_33105_markers(self):
        self.assertEqual(al.meteors(self.trace)[1], "33105 markers")
        (wave,) = al.smash_waves(self.trace)
        self.assertEqual((wave["seen"], wave["impact"]), (20000, 24800))
        rows = {row["guid"]: row for row in al.wave_rows(self.trace, wave)}
        self.assertEqual((rows[TRUESHOT]["moved"], rows[TRUESHOT]["dodge"]), (400, None))

    def test_every_section_says_what_it_is_missing(self):
        text = render(self.trace)
        self.assertIn("absent from this trace: algalon.phase; P1 from his first cast", text)
        self.assertIn("absent from this trace: haz meteors; read off 33105 markers", text)
        self.assertIn("absent from this trace: algalon.swappair, algalon.tankrole, algalon.call", text)
        self.assertIn("absent from this trace: algalon.bigbang, algalon.soaker, algalon.backup, algalon.hide,"
                      " algalon.defensive, algalon.shelter", text)
        self.assertIn("absent from this trace: algalon.slot, algalon.spot", text)
        self.assertIn("no star team recorded", text)


TANK_A, TANK_B = 6101, 6102
OPENER, LOOSE, BURST, SMASHED, STRAY, BANGED, UNPHASED, BARRAGED, DOTTED, WIPED = range(6111, 6121)
D_BOSS, D_HOLE, D_MATTER, D_CONSTELLATION, D_METEOR = (CREATURE | n for n in range(101, 106))

# (guid, name, role, dies at, killed by, spell, offset from the tank slot, the cause it reads as)
DEATH_CASES = (
    (OPENER, "Opener", "melee", 1200, D_BOSS, 0, (3, 0), "Algalon melee (intro end)"),
    (LOOSE, "Loose", "ranged", 20000, D_BOSS, 0, (-30, 10), "Algalon melee (loose)"),
    (BURST, "Burst", "heal", 25000, D_METEOR, al.SPELL_COSMIC_SMASH[0], (-30, 12), "star explosion"),
    (TANK_A, "Tanka", "tank", 30000, D_BOSS, 0, (0, -2), "Algalon on tank"),
    (SMASHED, "Smashed", "melee", 40000, D_METEOR, al.SPELL_COSMIC_SMASH[0], (37, 0), "Cosmic Smash"),
    (STRAY, "Stray", "ranged", 50000, D_MATTER, 0, (-30, 14), "Dark Matter while phased (stray)"),
    (BANGED, "Banged", "ranged", 61000, D_MATTER, 0, (-30, 16), "Dark Matter while phased (Big Bang)"),
    (UNPHASED, "Unphased", "heal", 63000, D_BOSS, al.SPELL_BIG_BANG[0], (-30, 18), "Big Bang"),
    (BARRAGED, "Barraged", "melee", 70000, D_CONSTELLATION, al.SPELL_CONSTELLATION_BARRAGE[0], (-30, 20),
     "Constellation Barrage"),
    (DOTTED, "Dotted", "ranged", 71000, D_HOLE, al.SPELL_BLACK_HOLE_DOT, (-30, 22), "Black Hole DoT"),
    (WIPED, "Wiped", "heal", 80000, WIPED, None, (-30, 24), "wipe command"),
)
DEATH_TARGETS = ((1000, 0), (1500, OPENER), (19500, TANK_A), (20500, LOOSE), (30000, TANK_A))


def death_snap(when: int) -> dict:
    units = [row(guid, offset, 0.0 if when >= dies else 100.0, 0) for guid, _, _, dies, _, _, offset, _ in DEATH_CASES]
    units.append(row(TANK_B, (2, -2), 100.0, 0))
    units.append(row(D_BOSS, (0, 0), 100.0, next((victim for until, victim in DEATH_TARGETS if when < until), TANK_B)))
    if 35000 <= when <= 40000:
        units.append(row(D_METEOR, (30, 0), 100.0, 0))
    return {"t": when, "e": "snap", "u": units, "hz": []}


def death_pull() -> list[dict]:
    roster = [{"g": guid, "n": name, "r": role, "c": "warrior", "h": 0} for guid, name, role, *_ in DEATH_CASES]
    roster.append({"g": TANK_B, "n": "Tankb", "r": "tank", "c": "warrior", "h": 0})
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 603, "inst": 4, "diff": 0,
         "boss": "algalon-the-observer", "roster": roster},
        {"t": 0, "e": "pull", "boss": "algalon-the-observer", "src": "engage"},
        {"t": 1, "e": "unit", "g": D_BOSS, "en": al.NPC_ALGALON, "n": "Algalon the Observer", "b": 1},
        {"t": 1, "e": "unit", "g": D_HOLE, "en": al.NPC_BLACK_HOLE, "n": "Black Hole"},
        {"t": 1, "e": "unit", "g": D_MATTER, "en": al.NPC_DARK_MATTER, "n": "Dark Matter"},
        {"t": 1, "e": "unit", "g": D_CONSTELLATION, "en": al.NPC_LIVING_CONSTELLATION, "n": "Living Constellation"},
        {"t": 1, "e": "unit", "g": D_METEOR, "en": al.NPC_COSMIC_SMASH_MARKERS[1], "n": "Algalon Asteroid Target"},
        note(0, OPENER, "algalon.phase", 1), note(1000, OPENER, "algalon.phase", 2),
        cast(55000, D_BOSS, al.SPELL_BIG_BANG[0], cast_ms=8000),
        # a star's burst does most of Burst's damage before a meteor lands the blow
        dmg(23000, D_HOLE, BURST, al.SPELL_BLACK_HOLE_EXPLOSION[0], 20000),
        *phased(STRAY, al.SPELL_BLACK_HOLE_PHASE, 48000, 50000),
        {"t": 80000, "e": "death", "g": WIPED, "killer": WIPED, "cause": "reset"},
    ]
    for guid, _, _, dies, source, spell, offset, _ in DEATH_CASES:
        if spell is not None:
            records += [dmg(dies, source, guid, spell, 3000, over=1000), death(dies, guid, source, 3000, offset)]
    records += [death_snap(when) for when in range(0, 90000, 500)]
    records.append({"t": 90000, "e": "end", "out": "wipe"})
    return records


class DeathCauses(unittest.TestCase):
    """One death per mechanic the classifier tells apart."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, death_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_each_death_reads_as_its_mechanic(self):
        rows = {row["guid"]: row for row in al.death_rows(self.trace)}
        self.assertEqual({guid: rows[guid]["cause"] for guid, *_ in DEATH_CASES},
                         {guid: cause for guid, *_, cause in DEATH_CASES})
        self.assertEqual(rows[LOOSE]["detail"], "threat, off Tanka")
        self.assertEqual(rows[BURST]["detail"], "87% of the last 3 s")
        self.assertEqual(rows[SMASHED]["detail"], "6-10 yd")

    def test_the_table_counts_cause_by_role(self):
        text = render(self.trace, "deaths")
        self.assertRegex(text, r"Algalon melee \(loose\) +- +- +- +1 +1")
        self.assertRegex(text, r"Algalon on tank +1 +- +- +- +1")

    def test_a_stray_phase_that_ended_in_a_death_is_a_failure(self):
        self.assertIn("stray phase ended in a death: Stray @ 48.0s (walked in)",
                      [line for _, line in al.failures(self.trace)])


HOLDER, PARTNER, PRIEST, PALADIN, CASTER, ROGUE, HUNTER, WARRIOR, MAGE = range(7001, 7010)
WOLF = (2 << 32) | 7101
E_BOSS, E_STAR_A, E_STAR_B, E_HOLE = (CREATURE | n for n in (201, 211, 212, 221))
EDGE_END = 60000

# (guid, name, class, role, human, offset from the tank slot when it stands still)
EDGE_ROSTER = (
    (HOLDER, "Bulwark", 2, "tank", 0, None),
    (PARTNER, "Dragon", 2, "tank", 1, (4, 4)),
    (PRIEST, "Prayer", 5, "heal", 0, (-25, 0)),
    (PALADIN, "Holylight", 2, "heal", 0, (-25, 0)),
    (CASTER, "Druidica", 11, "ranged", 0, (-25, 40)),
    (ROGUE, "Assasin", 4, "melee", 0, (-22, 0)),
    (HUNTER, "Trueshot", 3, "ranged", 0, (-20, 30)),
    (WARRIOR, "Mighty", 1, "melee", 0, (0, 30)),
    (MAGE, "Smartface", 8, "ranged", 0, (-70, 0)),
)
HOLE_SPOT = (-25, 0)
STAR_A_SPOT, STAR_B_SPOT = (30, -30), (5, 10)

EDGE_SPELLS = {62124: "Hand of Reckoning", 2649: "Growl", 694: "Mocking Blow", 31789: "Righteous Defense",
               33206: "Pain Suppression", 498: "Divine Protection", 6940: "Hand of Sacrifice", 53601: "Sacred Shield",
               48066: "Power Word: Shield", 20267: "Judgement of Light", 45438: "Ice Block", 48461: "Wrath",
               53190: "Starfall", 22482: "Blade Flurry", 31803: "Holy Vengeance", 64584: "Big Bang"}


def holder_offset(when: int) -> tuple[float, float]:
    """20 yd out on the pickup spot, a 5 s walk onto the tank slot, then off it for 11 s from 19 s."""
    if when < 8000:
        return 0, 20
    if when < 13000:
        return 0, 20 - 4 * (when - 8000) / 1000
    return (0, 10) if 19000 <= when < 30000 else (0, 0)


def edge_boss_offset(when: int) -> tuple[tuple[float, float], int]:
    """Where he stands and whether he moves: dragged 22 yd from 8 s to 13 s, then held."""
    if when < 8000:
        return (0, 30), 0
    if when < 13000:
        return (0, 30 - 4.4 * (when - 8000) / 1000), 1
    return (0, 8), 0


# Nobody in the intro, the holder, a second on the warrior's Mocking Blow, the holder again until he dies
# after Big Bang, then the human partner.
EDGE_TARGETS = ((8000, 0), (20000, HOLDER), (21000, WARRIOR), (39500, HOLDER))


def star_a_hp(when: int) -> float:
    """Drain only until the team focuses it at 12 s, then killed by 20 s."""
    return 100 - (when - 10000) / 1000 if when < 12000 else 98 - 96 * (when - 12000) / 7500


def star_b_hp(when: int) -> float:
    """Drain only, then 5 %/s of splash on top from 14 s to 18 s while unfocused, then focused at 21 s."""
    if when <= 14000:
        return 100 - (when - 10000) / 1000
    if when <= 18000:
        return 96 - 6 * (when - 14000) / 1000
    if when <= 21000:
        return 72 - (when - 18000) / 1000
    return 69 - 68 * (when - 21000) / 3500


def edge_row(guid: int, offset, hp: float, target: int = 0, moving: int = 0) -> list:
    x, y = spot(*offset)
    return [guid, x, y, 417.3, 0.0, hp, 80.0, target, moving, 0, 0, 0, 0, 0.0]


def edge_snap(when: int) -> dict:
    units = []
    for guid, _, _, _, _, offset in EDGE_ROSTER:
        if guid == HOLDER:
            hp = 100.0 if when < 38500 else 25.0 if when < 39200 else 0.0
            units.append(edge_row(guid, holder_offset(when), hp))
        else:
            units.append(edge_row(guid, offset, 100.0, E_STAR_A if guid == CASTER and 12000 <= when < 20000 else 0))
    units.append(edge_row(WOLF, (0, 9), 100.0))
    offset, moving = edge_boss_offset(when)
    victim = next((victim for until, victim in EDGE_TARGETS if when < until), PARTNER)
    units.append(edge_row(E_BOSS, offset, 90.0, victim, moving))
    if 10000 <= when <= 19500:
        units.append(edge_row(E_STAR_A, STAR_A_SPOT, star_a_hp(when)))
    if 10000 <= when <= 24500:
        units.append(edge_row(E_STAR_B, STAR_B_SPOT, star_b_hp(when)))
    if when >= 22000:
        units.append(edge_row(E_HOLE, HOLE_SPOT, 100.0))
    return {"t": when, "e": "snap", "u": units, "hz": []}


def veto(when: int, guid: int, multiplier: str, action: str) -> dict:
    return {"t": when, "e": "veto", "g": guid, "m": multiplier, "a": action}


def edge_pull() -> list[dict]:
    """A pull on the edge hold: the pickup tank drags him onto the tank slot, wanders off it after a
    Mocking Blow pickup, soaks Big Bang with nothing up and dies to his next swings; two stars, one
    chewed by splash; a mage in Ice Block far from the only hole."""
    roster = [{"g": guid, "n": name, "c": klass, "r": role, "h": human}
              for guid, name, klass, role, human, _ in EDGE_ROSTER]
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 603, "inst": 4, "diff": 0,
         "boss": "algalon-the-observer", "roster": roster},
        {"t": 0, "e": "pull", "boss": "algalon-the-observer", "src": "engage"},
        {"t": 1, "e": "unit", "g": E_BOSS, "en": al.NPC_ALGALON, "n": "Algalon the Observer", "b": 1},
        {"t": 1, "e": "unit", "g": E_STAR_A, "en": al.NPC_COLLAPSING_STAR, "n": "Collapsing Star"},
        {"t": 1, "e": "unit", "g": E_STAR_B, "en": al.NPC_COLLAPSING_STAR, "n": "Collapsing Star"},
        {"t": 1, "e": "unit", "g": E_HOLE, "en": al.NPC_BLACK_HOLE, "n": "Black Hole"},
        {"t": 1, "e": "unit", "g": WOLF, "en": 3825, "n": "Wolf", "own": HUNTER},
        *({"t": 1, "e": "spell", "sp": spell, "n": name} for spell, name in EDGE_SPELLS.items()),

        note(0, PRIEST, "algalon.phase", 1), note(8000, PRIEST, "algalon.phase", 2),
        note(8000, PRIEST, "algalon.pull", f"inreach=1 victim={HOLDER}"),
        note(0, HOLDER, "algalon.swappair", 1), note(0, PARTNER, "algalon.swappair", 1),
        note(0, HOLDER, "algalon.spot", "pickup"), note(8000, HOLDER, "algalon.spot", "tank"),
        note(8000, CASTER, "algalon.spot", "slot"), note(20000, CASTER, "algalon.spot", "nearhole"),
        note(30000, CASTER, "algalon.spot", "slot"),
        note(8000, CASTER, "algalon.starteam", 1), note(8000, PRIEST, "algalon.urgent", 0),
        note(8000, PRIEST, "algalon.focusstar", 0), note(12000, PRIEST, "algalon.focusstar", E_STAR_A),
        note(20000, PRIEST, "algalon.focusstar", 0), note(21000, PRIEST, "algalon.focusstar", E_STAR_B),
        note(25000, PRIEST, "algalon.focusstar", 0),
        # Druidica tracks the stars until Big Bang, Prayer after; Druidica's last row is never superseded.
        note(8000, CASTER, "algalon.starwindow", "none"), note(12000, CASTER, "algalon.starwindow", "open"),
        note(20000, CASTER, "algalon.starwindow", "gap"), note(21000, CASTER, "algalon.starwindow", "deadline"),
        note(30000, CASTER, "algalon.starwindow", "bigbang"), note(34000, PRIEST, "algalon.starwindow", "bigbang"),
        note(38000, PRIEST, "algalon.starwindow", "gap"),
        note(20000, CASTER, "algalon.stardeath", "killed 42.4"), note(25000, CASTER, "algalon.stardeath", "killed 5.0"),
        note(30000, PRIEST, "algalon.bigbang", 1), note(38500, PRIEST, "algalon.bigbang", 0),
        note(30000, PRIEST, "algalon.soaker", HOLDER), note(38500, PRIEST, "algalon.soaker", 0),
        note(30000, HOLDER, "algalon.soakkit", "def=none forbearance=1"),
        note(25000, HOLDER, "algalon.handover", "nodef"),

        # Taunts: the pickup, a pet, one while already his, the warrior's Mocking Blow, a Righteous Defense
        # that misses with its own triggered echo, the holder's taunt back, and the partner after the death.
        cast(7900, HOLDER, 62124, E_BOSS), cast(15000, WOLF, 2649, E_BOSS), cast(18000, HOLDER, 62124, E_BOSS),
        cast(20000, WARRIOR, 694, E_BOSS), cast(20200, PARTNER, 31789, WARRIOR),
        {**cast(20200, PARTNER, 31789, E_BOSS), "tr": 1},
        cast(20500, HOLDER, 62124, E_BOSS), cast(39300, PARTNER, 62124, E_BOSS),

        # Reserved buttons spent long before Big Bang, by a priest, the holder and a holy paladin.
        cast(16000, PRIEST, 33206, CASTER), cast(17000, HOLDER, 498), cast(18000, PALADIN, 6940, ROGUE),

        # On stars: the team on its focus, the team's Starfall on the other, and two splashers off the team.
        cast(15000, CASTER, 48461, E_STAR_A),
        {**cast(15000, CASTER, 53190, E_STAR_B), "tr": 1}, {**cast(16000, CASTER, 53190, E_STAR_B), "tr": 1},
        {**cast(16500, ROGUE, 22482, E_STAR_B), "tr": 1}, {**cast(17000, PARTNER, 31803, E_STAR_B), "tr": 1},
        dmg(20000, E_STAR_A, ROGUE, al.SPELL_BLACK_HOLE_EXPLOSION[0], 1000),
        dmg(25000, E_STAR_B, ROGUE, al.SPELL_BLACK_HOLE_EXPLOSION[0], 1000),

        *(veto(when, ROGUE, "algalon splash hold", "blade flurry") for when in (16500, 17500)),
        veto(21000, WARRIOR, "algalon taunt guard", "mocking blow"),
        *(veto(when, ROGUE, "algalon walk guard", "reach melee") for when in (23000, 24000, 25000)),
        veto(23500, WARRIOR, "algalon walk guard", "set behind"),
        veto(26000, HOLDER, "algalon cooldown reserve", "divine protection"),
        veto(29500, MAGE, "algalon ice block hold", "ice block"),

        aura(29000, MAGE, al.SPELL_ICE_BLOCK), aura(31000, MAGE, al.SPELL_ICE_BLOCK, removed=True),

        # Big Bang: shielded during the cast, healers phased until a second after the hit, which takes the
        # holder to a quarter; a heal 0.6 s later, then two swings kill him 1.2 s after the hit.
        cast(30000, E_BOSS, al.SPELL_BIG_BANG[1], cast_ms=8000),
        cast(31000, PALADIN, 53601, HOLDER), cast(31500, PRIEST, 48066, HOLDER),
        {"t": 31500, "e": "act", "g": PRIEST, "a": "algalon soaker prep action", "rel": 0, "vd": "OK"},
        *phased(PRIEST, al.SPELL_BLACK_HOLE_PHASE, 33000, 39000),
        *phased(PALADIN, al.SPELL_BLACK_HOLE_PHASE, 33000, 39000),
        {**dmg(38000, E_BOSS, HOLDER, al.SPELL_BIG_BANG[1], 30000), "ab": 1500},
        {"t": 38600, "e": "heal", "s": PALADIN, "d": HOLDER, "sp": 20267, "a": 937, "oh": 0, "hp": 27.0},
        dmg(38800, E_BOSS, HOLDER, 0, 12000, hp=25.0),
        {"t": 39100, "e": "act", "g": PALADIN, "a": "algalon soaker rescue action", "rel": 0, "vd": "FAILED"},
        dmg(39200, E_BOSS, HOLDER, 0, 9000, hp=20.0, over=3000),
        death(39200, HOLDER, E_BOSS, 9000, (0, 0)),
        {"t": EDGE_END, "e": "end", "out": "wipe"},
    ]
    records += [edge_snap(when) for when in range(0, EDGE_END, 500)]
    return records


class EdgeHold(unittest.TestCase):
    """The views the edge hold added, on a pull that ran it."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, edge_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_each_pickup_times_the_drag_and_names_who_stood_in_his_reach(self):
        self.assertTrue(al.edge_hold(self.trace))
        first, second, third = al.hold_pickups(self.trace)
        self.assertEqual((first["start"], first["stop"], first["holder"], first["before"], first["arrival"]),
                         (8000, 20000, HOLDER, 0, 12500))
        self.assertEqual(first["rest"], spot(0, 8))
        self.assertEqual(first["near"], {WARRIOR: (0.0, 8000)})
        self.assertEqual((second["start"], second["before"], second["arrival"], second["near"]),
                         (21000, WARRIOR, 30000, {}))
        self.assertAlmostEqual(second["due_gap"], 10.0)
        self.assertEqual((third["holder"], third["arrival"]), (PARTNER, None))

    def test_bot_hold_counts_only_bot_holders_and_their_time_on_the_slot(self):
        self.assertEqual(al.bot_hold(self.trace), (30500, 16000))
        rows = {row["t"]: row for row in al.hold_timeline(self.trace)}
        self.assertEqual((rows[8000]["victim"], rows[8000]["to_slot"]), (HOLDER, 20.0))
        self.assertEqual((rows[23000]["to_slot"], rows[43000]["victim"]), (10.0, PARTNER))
        self.assertAlmostEqual(rows[13000]["to_hold"], al.dist2(spot(0, 8), al.HOLD_SPOT))
        self.assertIn("a bot of the pair held him 30.5 s, 52% of it on T", render(self.trace, "hold"))

    def test_a_bot_holder_off_the_slot_after_a_pickup_is_a_failure(self):
        self.assertEqual(al.hold_faults(self.trace),
                         [(21000, "Bulwark not on the tank slot 8 s after picking him up @ 21.0s, 10.0 yd off")])

    def test_taunts_read_whether_his_victim_became_the_caster(self):
        rows = [(row["t"], row["caster"], row["name"], row["before"], row["outcome"])
                for row in al.taunt_casts(self.trace)]
        self.assertEqual(rows, [
            (7900, HOLDER, "Hand of Reckoning", 0, "took"),
            (15000, WOLF, "Growl", HOLDER, "missed"),
            (18000, HOLDER, "Hand of Reckoning", HOLDER, "already"),
            (20000, WARRIOR, "Mocking Blow", HOLDER, "took"),
            (20200, PARTNER, "Righteous Defense", WARRIOR, "missed"),
            (20500, HOLDER, "Hand of Reckoning", WARRIOR, "took"),
            (39300, PARTNER, "Hand of Reckoning", HOLDER, "took")])
        self.assertEqual(sorted(line for _, line in al.taunt_faults(self.trace)), [
            "non-tank taunt at Algalon: Mighty (melee) Mocking Blow 1x from 20.0s",
            "pickup taunt missed @ 20.2s: Dragon Righteous Defense while he was on Mighty"])
        self.assertRegex(render(self.trace, "taunts"), r"Wolf \(Trueshot's pet\) +Growl +on Bulwark; NON-TANK")
        self.assertIn("algalon taunt guard vetoes: Mighty mocking blow 1", render(self.trace, "taunts"))

    def test_soak_reads_the_hit_the_healers_and_the_kit(self):
        (row,) = al.soak_rows(self.trace)
        self.assertEqual((row["impact"], row["hp"], row["hit"]["ab"]), (38000, (100.0, 25.0), 1500))
        self.assertEqual(sum(rec["a"] for rec in row["later"]), 21000)
        self.assertEqual((row["heal"]["s"], row["heal"]["t"]), (PALADIN, 38600))
        self.assertEqual(row["strip"], {PRIEST: 1000, PALADIN: 1000})
        self.assertEqual(row["prep"], [(31000, PALADIN, 53601), (31500, PRIEST, 48066)])
        self.assertEqual((row["kit"], al.kit_label(row)), ({"def": "none", "forbearance": "1"}, "def=none forbearance"))
        self.assertEqual([rec["txt"] for rec in row["handover"]], ["nodef"])
        self.assertEqual(row["acts"], {("algalon soaker prep action", PRIEST, "OK"): 1,
                                       ("algalon soaker rescue action", PALADIN, "FAILED"): 1})
        self.assertEqual(row["died"], 39200)
        self.assertEqual([(press["t"], press["name"]) for press in row["earlier"]],
                         [(16000, "Pain Suppression"), (17000, "Divine Protection"), (18000, "Hand of Sacrifice")])
        self.assertEqual(row["pressed"], [])
        text = render(self.trace, "soak")
        self.assertIn("first heal on him: Holylight Judgement of Light 20267 937 at +0.60 s", text)
        self.assertIn("algalon cooldown reserve vetoes: Bulwark divine protection 1", text)

    def test_soak_failures_name_the_death_and_the_buttons_a_tank_or_priest_spent(self):
        self.assertEqual(al.soak_faults(self.trace), [
            (39200, "Big Bang 1 @ 30.0s: soaker Bulwark died 1.2 s after the hit (def=none forbearance,"
                    " first heal +0.6 s)"),
            (16000, "Prayer spent Pain Suppression on Druidica @ 16.0s, outside a Big Bang soak"),
            (17000, "Bulwark spent Divine Protection @ 17.0s, outside a Big Bang soak")])

    def test_big_bang_measures_every_unphased_member_to_the_nearest_hole(self):
        (row,) = al.big_bang_rows(self.trace)
        self.assertAlmostEqual(row["hole_gaps"][CASTER], 40.0)
        self.assertEqual(row["hole_gaps"][PRIEST], 0.0)
        self.assertEqual([(round(gap, 1), guid) for gap, guid in row["unreachable"]], [(45.0, MAGE)])
        self.assertEqual(row["iceblock"], [(29000, 31000, MAGE)])
        shares = [(guid, round(gap, 1), round(share, 3)) for guid, gap, share in al.nearhole_shares(self.trace, row)]
        self.assertEqual(shares, [(MAGE, 45.0, 0.0), (CASTER, 40.0, 0.667), (WARRIOR, 39.1, 0.0)])
        self.assertEqual(al.bang_faults(self.trace),
                         [(30000, "Big Bang 1 @ 30.0s: nearest hole beyond 43 yd for Smartface 45.0")])
        self.assertIn("Ice Block during the cast: Smartface 0:29.000 .. 0:31.000", render(self.trace, "bigbang"))

    def test_star_window_rows_from_every_tracker_read_as_one_timeline(self):
        self.assertEqual(al.span_totals({0: al.window_spans(self.trace)})["bigbang"], 8000)
        self.assertEqual(al.span_totals(probes.holder_spans(self.trace, "algalon.starwindow", EDGE_END))["bigbang"],
                         34000)
        text = render(self.trace, "stars")
        self.assertIn("bigbang 8.0 s", text)
        # a trace from before the star plan prints nothing for it, not even its absence
        self.assertNotIn("starplan", text)

    def test_window_opens_measure_the_team_against_the_focus_star(self):
        opens = [(row["t"], row["kind"], row["star"], row["members"], round(row["middle"], 2))
                 for row in al.window_opens(self.trace)]
        self.assertEqual(opens, [(12000, "open", E_STAR_A, 1, 89.02), (21000, "deadline", E_STAR_B, 1, 42.43)])
        self.assertEqual(al.window_faults(self.trace),
                         [(21000, "deadline window @ 21.0s: focus star 42.4 yd from the star team's mean position")])

    def test_stars_read_against_their_drain_and_the_pair(self):
        first, second = al.star_rows(self.trace)
        self.assertEqual((first["class"], first["drain"], first["pair"]), ("killed", (2.0, 90.5), []))
        self.assertEqual((second["class"], second["drain"]), ("killed", (1.0, 85.5)))
        self.assertEqual([(round(gap, 2), guid) for gap, guid in second["pair"]], [(5.0, HOLDER), (6.08, PARTNER)])
        lines = [line for _, line in al.star_faults(self.trace)]
        self.assertIn("star died 5.0 yd from Bulwark of the swap pair @ 25.0s", lines)
        self.assertIn("star died 6.1 yd from Dragon of the swap pair @ 25.0s", lines)

    def test_collateral_is_health_lost_beyond_the_drain_while_unfocused(self):
        first, second = al.star_excess(self.trace)
        self.assertEqual((first["star"], second["star"]), (E_STAR_A, E_STAR_B))
        self.assertAlmostEqual(first["excess"], 0.0)
        self.assertAlmostEqual(second["excess"], 20.0)
        self.assertEqual((round(second["worst"], 6), second["worst_from"], second["worst_to"], second["crossed"]),
                         (20.0, 14000, 18000, 16000))
        self.assertEqual(al.collateral_faults(self.trace), [
            (16000, "star chewed: star 2 lost 20.0% beyond its drain while not the focus star, 20.0% of it in 10 s"
                    " from 14.0s")])

    def test_casts_on_stars_split_by_team_and_focus(self):
        groups = al.star_casts(self.trace)
        self.assertEqual(dict(groups), {
            "star team on the focus star": {(CASTER, "Wrath"): 1},
            "star team on another star": {(CASTER, "Starfall"): 2},
            "off the star team": {(ROGUE, "Blade Flurry"): 1, (PARTNER, "Holy Vengeance"): 1}})
        text = render(self.trace, "collateral")
        self.assertIn("Dragon (tank)*", text)
        self.assertIn("algalon splash hold vetoes: Assasin blade flurry 2", text)

    def test_phasing_counts_walk_guard_vetoes_per_bot(self):
        text = render(self.trace, "phasing")
        self.assertRegex(text, r"Assasin \(melee\) +3  reach melee 3")
        self.assertRegex(text, r"Mighty \(melee\) +1  set behind 1")

    def test_formation_and_the_verdict_on_a_pull_that_ran_the_edge_hold(self):
        self.assertNotIn("predates the edge hold", render(self.trace, "formation"))
        lines = [line for _, line in al.failures(self.trace)]
        for line in ("Bulwark not on the tank slot 8 s after picking him up @ 21.0s, 10.0 yd off",
                     "pickup taunt missed @ 20.2s: Dragon Righteous Defense while he was on Mighty",
                     "deadline window @ 21.0s: focus star 42.4 yd from the star team's mean position",
                     "Big Bang 1 @ 30.0s: nearest hole beyond 43 yd for Smartface 45.0"):
            self.assertIn(line, lines)

    def test_every_section_renders(self):
        text = render(self.trace)
        self.assertIn("EDGE HOLD", text)
        self.assertIn("swap pair, MISSED", text)
        self.assertIn("soaker DIED +1.2 s after the hit", text)
        self.assertNotIn("absent from this trace: algalon.spot=pickup", text)


def handover_pull() -> list[dict]:
    """The edge pull with the soak handed on twice mid-cast, Smartface dying and then Dragon phased by a
    hole; the wolf's second Growl taking him; and the star tracker writing the planner's labels."""
    records = [rec for rec in edge_pull()
               if rec.get("e") != "note" or rec.get("k") not in ("algalon.soaker", "algalon.starwindow")]
    return records + [
        note(30000, PRIEST, "algalon.soaker", MAGE), note(31100, PRIEST, "algalon.soaker", PARTNER),
        note(33050, PRIEST, "algalon.soaker", HOLDER), note(38500, PRIEST, "algalon.soaker", 0),
        note(30000, PRIEST, "algalon.backup", PRIEST), note(38500, PRIEST, "algalon.backup", 0),
        death(31000, MAGE, E_BOSS, 9000, (-70, 0)),
        *phased(PARTNER, al.SPELL_BLACK_HOLE_PHASE, 33000, 39000),
        cast(45000, WOLF, 2649, E_BOSS), dmg(45300, E_BOSS, WOLF, 0, 5000),
        note(8000, CASTER, "algalon.starwindow", "none"), note(12000, CASTER, "algalon.starwindow", "open"),
        note(20000, CASTER, "algalon.starwindow", "spacing"), note(21000, CASTER, "algalon.starwindow", "deadline"),
        note(25000, CASTER, "algalon.starwindow", "prebang"), note(30000, CASTER, "algalon.starwindow", "bigbang"),
        note(38000, PRIEST, "algalon.starwindow", "smash"),
    ]


class HandOver(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, handover_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_every_soaker_of_the_cast_is_listed_with_what_moved_the_latch(self):
        (row,) = al.big_bang_rows(self.trace)
        self.assertEqual(row["soaker"], HOLDER)
        self.assertEqual(row["soakers"], [(30000, MAGE, ""), (31100, PARTNER, "Smartface died 0:31.000"),
                                          (33050, HOLDER, "Dragon phased 0:33.000"), (38500, 0, "")])
        self.assertEqual(row["backups"], [(30000, PRIEST, ""), (38500, 0, "")])
        chain = ("algalon.soaker during the cast: 0:30.000 Smartface, 0:31.100 Dragon (Smartface died 0:31.000),"
                 " 0:33.050 Bulwark (Dragon phased 0:33.000), 0:38.500 cleared")
        bang = render(self.trace, "bigbang")
        self.assertIn(chain, bang)
        self.assertIn("algalon.backup during the cast: 0:30.000 Prayer, 0:38.500 cleared", bang)
        self.assertIn(chain, render(self.trace, "soak"))

    def test_a_pet_counts_in_the_verdict_only_once_a_growl_took(self):
        self.assertEqual([(row["t"], row["outcome"]) for row in al.taunt_casts(self.trace) if row["caster"] == WOLF],
                         [(15000, "missed"), (45000, "took")])
        self.assertIn("non-tank taunt at Algalon: Wolf (Trueshot's pet) Growl 2x from 15.0s, 1 took",
                      [line for _, line in al.failures(self.trace)])

    def test_the_planner_labels_count_in_the_window_timeline_and_open_nothing(self):
        totals = al.span_totals({0: al.window_spans(self.trace)})
        self.assertEqual((totals["spacing"], totals["prebang"], totals["smash"]), (1000, 5000, 22000))
        self.assertEqual([(row["t"], row["kind"]) for row in al.window_opens(self.trace)],
                         [(12000, "open"), (21000, "deadline")])
        text = render(self.trace, "stars")
        for label in ("spacing 1.0 s (2%)", "prebang 5.0 s (10%)", "smash 22.0 s (42%)"):
            self.assertIn(label, text)


E_CONSTELLATION, E_FAR_HOLE = CREATURE | 231, CREATURE | 222
FAR_HOLE_SPOT = (40, 60)


def holder_aim(when: int) -> int:
    """Bulwark's own target: nothing for 1.5 s after the pickup and for half a second at 33 s, the
    constellation for the second Mighty holds him, for 7 s from 25 s and from 36 s until he dies."""
    if 8000 <= when < 9500 or 33000 <= when < 33500:
        return 0
    if 20000 <= when < 21000 or 25000 <= when < 32000 or when >= 36000:
        return E_CONSTELLATION
    return E_BOSS


def opener_snap(snap: dict) -> dict:
    units = []
    for unit in snap["u"]:
        if unit[0] in (HOLDER, PARTNER):
            unit = unit[:7] + [holder_aim(snap["t"]) if unit[0] == HOLDER else E_BOSS] + unit[8:]
        units.append(unit)
    units.append(edge_row(E_CONSTELLATION, (-10, 20), 100.0))
    if snap["t"] >= 22000:
        units.append(edge_row(E_FAR_HOLE, FAR_HOLE_SPOT, 100.0))
    return {**snap, "u": units}


def opener_pull() -> list[dict]:
    """The edge pull with the holder's own target off him, threat redirects around phase 1's start, two
    bots hit by Big Bang on a hide run, and the star planner's rows."""
    redirect = al.SPELL_TRICKS_OF_THE_TRADE
    records = [opener_snap(rec) if rec.get("e") == "snap" else rec for rec in edge_pull()]
    return records + [
        {"t": 1, "e": "unit", "g": E_CONSTELLATION, "en": al.NPC_LIVING_CONSTELLATION, "n": "Living Constellation"},
        {"t": 1, "e": "unit", "g": E_FAR_HOLE, "en": al.NPC_BLACK_HOLE, "n": "Black Hole"},

        # In the intro, the press, its triggered echo and a second row of it, one on the holder, one on a
        # dps, and one after the opener.
        cast(7000, ROGUE, redirect, WARRIOR), cast(9400, ROGUE, redirect, WARRIOR),
        {**cast(9450, ROGUE, redirect, WARRIOR), "tr": 1}, cast(9900, ROGUE, redirect, WARRIOR),
        cast(10000, HUNTER, al.SPELL_MISDIRECTION, HOLDER), cast(15000, HUNTER, al.SPELL_MISDIRECTION, CASTER),
        cast(29000, HUNTER, al.SPELL_MISDIRECTION, CASTER),

        # Mighty runs to the far hole. Smartface's only shelter row is older than the cast.
        note(20000, MAGE, "algalon.shelter", E_HOLE), note(31000, WARRIOR, "algalon.shelter", E_FAR_HOLE),
        note(30000, WARRIOR, "algalon.hide", "wait"), note(34000, WARRIOR, "algalon.hide", "run"),
        note(30000, MAGE, "algalon.hide", "run"),
        dmg(38000, E_BOSS, WARRIOR, al.SPELL_BIG_BANG[1], 50000), dmg(38000, E_BOSS, MAGE, al.SPELL_BIG_BANG[1], 50000),

        note(12000, CASTER, "algalon.starplan", "s1 98% 8s | s2 98% 60s | bang 18..26"),
        note(20000, CASTER, "algalon.starplan", "s2 72% 4s | bang 10..18"),
        note(21000, PRIEST, "algalon.starplan", "s2 69% 3s | smash 2..11 | bang 9..17"),
    ]


class Opener(unittest.TestCase):
    """The holder's own target, threat redirects at the opener, where Big Bang runners ran, the star plan."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, opener_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_holder_targeting_something_else_is_read_only_while_he_is_on_it(self):
        self.assertEqual(al.holder_target_runs(self.trace), [
            {"start": 8000, "stop": 9500, "tank": HOLDER, "target": 0},
            {"start": 25000, "stop": 32000, "tank": HOLDER, "target": E_CONSTELLATION},
            {"start": 33000, "stop": 33500, "tank": HOLDER, "target": 0},
            # cut at his death, not the next sample
            {"start": 36000, "stop": 39200, "tank": HOLDER, "target": E_CONSTELLATION}])
        self.assertEqual(al.holder_target_faults(self.trace), [
            (8000, "holder Bulwark had no target for 1.5 s @ 8.0s"),
            (25000, "holder Bulwark targeted Living Constellation for 7.0 s @ 25.0s"),
            (36000, "holder Bulwark targeted Living Constellation for 3.2 s @ 36.0s")])

    def test_a_redirect_in_the_opener_on_anyone_outside_the_pair_is_a_failure(self):
        self.assertEqual(al.redirect_faults(self.trace), [
            (9400, "Tricks of the Trade on Mighty (melee), not a swap tank, @ 9.4s (Assasin)"),
            (15000, "Misdirection on Druidica (ranged), not a swap tank, @ 15.0s (Trueshot)")])
        lines = [line for _, line in al.failures(self.trace)]
        self.assertIn("Tricks of the Trade on Mighty (melee), not a swap tank, @ 9.4s (Assasin)", lines)
        self.assertIn("holder Bulwark targeted Living Constellation for 7.0 s @ 25.0s", lines)

    def test_each_runner_hit_names_the_hole_it_ran_to_and_the_nearest(self):
        (row,) = al.big_bang_rows(self.trace)
        far, near = spot(*FAR_HOLE_SPOT), spot(*HOLE_SPOT)
        mighty, smartface = row["runners"]
        self.assertEqual((mighty["guid"], mighty["hole"], mighty["ran"][0], mighty["nearest"][0]),
                         (WARRIOR, E_FAR_HOLE, far, near))
        self.assertAlmostEqual(mighty["ran"][1], 50.0)
        self.assertAlmostEqual(mighty["nearest"][1], 39.05, places=2)
        self.assertEqual((smartface["guid"], smartface["hole"], smartface["ran"]), (MAGE, 0, (None, None)))
        self.assertAlmostEqual(smartface["nearest"][1], 45.0)
        text = render(self.trace, "bigbang")
        self.assertIn(f"Mighty ran to ({far[0]:.0f},{far[1]:.0f}) 50.0 yd,"
                      f" nearest hole ({near[0]:.0f},{near[1]:.0f}) 39.1 yd", text)
        self.assertIn(f"Smartface ran to -, nearest hole ({near[0]:.0f},{near[1]:.0f}) 45.0 yd", text)
        self.assertNotIn("algalon.shelter", text)

    def test_the_star_plan_reads_beside_the_window_it_was_written_under(self):
        text = render(self.trace, "stars")
        self.assertRegex(text, r"0:12\.000 +open +s1 98% 8s \| s2 98% 60s \| bang 18\.\.26")
        self.assertRegex(text, r"0:20\.000 +gap +s2 72% 4s \| bang 10\.\.18")
        self.assertRegex(text, r"0:21\.000 +deadline +s2 69% 3s \| smash 2\.\.11 \| bang 9\.\.17")

    def test_every_section_renders(self):
        self.assertIn("VERDICT", render(self.trace))


SLOT_SOURCE = """
const Position ULDUAR_ALGALON_ROOM_CENTER = Position(1632.668f, -302.7656f, 417.3211f);
const std::array<Position, ULDUAR_ALGALON_TOTAL_SLOTS> ULDUAR_ALGALON_RING_SLOTS = {{
    Position(1651.668f, -302.766f, 417.321f),  // healer, 19 yd at 0 deg
    Position(1613.668f, -302.766f, 417.321f),
    Position(1632.668f, -277.766f, 417.321f),  // inner ranged
}};
const Position ULDUAR_ALGALON_TANK_SLOT = Position(1632.668f, -310.766f, 417.321f);
"""


class Contracts(unittest.TestCase):
    """The payloads and source tables the reader parses."""

    def test_stardeath_reads_how_and_the_nearest_yards(self):
        self.assertEqual(al.parse_stardeath("killed 12.3"), ("killed", 12.3))
        self.assertEqual(al.parse_stardeath("chewed 1.6"), ("chewed", 1.6))
        self.assertEqual(al.parse_stardeath("drained -"), ("drained", None))
        self.assertEqual(al.parse_stardeath("collapsed -"), ("collapsed", None))
        self.assertEqual(al.parse_stardeath(None), ("", None))

    def test_soakkit_keeps_a_defensive_named_in_two_words(self):
        self.assertEqual(al.parse_soakkit("def=divine protection forbearance=0"),
                         {"def": "divine protection", "forbearance": "0"})
        self.assertEqual(al.parse_soakkit("def=none forbearance=1"), {"def": "none", "forbearance": "1"})
        self.assertEqual(al.parse_soakkit(None), {})

    def test_ring_notes_read_both_comment_shapes(self):
        source = """
const std::array<Position, ULDUAR_ALGALON_TOTAL_SLOTS> ULDUAR_ALGALON_RING_SLOTS = {{
    Position(1617.869f, -333.468f, 417.321f),  // healer, 19 yd from the hold at +85 deg
    Position(1655.356f, -328.200f, 417.321f),  // ranged, 31 yd from the hold at -40 deg
    Position(1651.668f, -302.766f, 417.321f),  // healer, 19 yd at 0 deg
    Position(1613.668f, -302.766f, 417.321f),
}};
"""
        self.assertEqual(al.parse_ring_notes(source), [("healer", 19.0, 85.0), ("ranged", 31.0, -40.0),
                                                       ("healer", 19.0, 0.0), None])
        self.assertEqual(len(al.parse_ring_slots(source)), 4)
        self.assertEqual(al.parse_ring_notes(source.replace("RING_SLOTS", "OTHER_SLOTS")), [])

    def test_a_triggered_cast_is_not_a_taunt(self):
        trace = types.SimpleNamespace(spells={67485: "Hand of Reckoning", 2649: "Growl", 355: "Taunt"})
        self.assertEqual(al.taunt_spell(trace, {"sp": 2649}), "Growl")
        self.assertEqual(al.taunt_spell(trace, {"sp": 67485, "tr": 1}), "")
        self.assertEqual(al.taunt_spell(trace, {"sp": 694}), "Mocking Blow")
        self.assertEqual(al.taunt_spell(trace, {"sp": 48461}), "")

    def test_the_worst_window_is_the_tightest_of_equal_rises(self):
        stamps = [0, 1000, 2000, 3000, 12500]
        self.assertEqual(al.worst_window(stamps, [0.0, 0.0, 5.0, 5.0, 9.0]), (5.0, 1000, 2000))
        self.assertEqual(al.worst_window([0], [0.0]), (0.0, None, None))

    def test_a_wave_reads_the_dodge_it_set_not_the_idle_after_it(self):
        wave = {"seen": 1000, "impact": 6000}
        spans = [("idle", 0, 1200), ("clear", 1200, 4000), ("idle", 4000, 9000)]
        self.assertEqual(al.wave_dodge(spans, wave), "clear")
        self.assertIsNone(al.wave_dodge([("idle", 0, 9000)], wave))
        self.assertIsNone(al.wave_dodge([("min", 0, 900)], wave))

    def test_pull_reads_the_reach_count_and_the_victim(self):
        self.assertEqual(al.parse_pull("inreach=3 victim=5008"), (3, 5008))
        self.assertEqual(al.parse_pull("victim=5008"), (None, 5008))
        self.assertEqual(al.parse_pull(None), (None, 0))

    def test_the_slot_table_reads_every_position_in_order(self):
        self.assertEqual(al.parse_ring_slots(SLOT_SOURCE), [
            (1651.668, -302.766, 417.321), (1613.668, -302.766, 417.321), (1632.668, -277.766, 417.321)])
        self.assertEqual(al.parse_ring_slots(SLOT_SOURCE.replace("RING_SLOTS", "OTHER_SLOTS")), [])
        self.assertEqual(al.parse_ring_slots("extern const std::array<Position, ULDUAR_ALGALON_TOTAL_SLOTS>"
                                             " ULDUAR_ALGALON_RING_SLOTS;"), [])

    def test_ring_slots_come_from_the_table_else_from_the_arcs(self):
        arcs = {"HEALER": (14.0, 1.5708, 1.9199), "RANGED_INNER": (20.5, 1.7977, 2.618),
                "RANGED_OUTER": (27.0, 1.5708, 3.1416)}

        def old_radius(name: str) -> float:
            for ring, values in arcs.items():
                for suffix, value in zip(("_RADIUS", "_ARC_CENTER", "_ARC_WIDTH"), values):
                    if name == f"ULDUAR_ALGALON_{ring}{suffix}":
                        return value
            raise al.Unknown(name)

        with tempfile.TemporaryDirectory() as folder:
            table, bare = pathlib.Path(folder) / "table.cpp", pathlib.Path(folder) / "bare.cpp"
            table.write_text(SLOT_SOURCE, encoding="utf-8")
            bare.write_text(SLOT_SOURCE.replace("RING_SLOTS", "OTHER_SLOTS"), encoding="utf-8")
            try:
                with mock.patch.object(al, "ALGALON_SOURCE", table):
                    al.ring_slots.cache_clear()
                    self.assertEqual(al.ring_slots(),
                                     ((1651.668, -302.766), (1613.668, -302.766), (1632.668, -277.766)))
                    self.assertIsNone(al.slot_position(3))
                with mock.patch.object(al, "ALGALON_SOURCE", bare), mock.patch.object(al, "radius", old_radius):
                    al.ring_slots.cache_clear()
                    self.assertEqual(len(al.ring_slots()), 18)
                    self.assertAlmostEqual(al.dist2(al.slot_position(0), al.TANK_SLOT), 14.0)
                with mock.patch.object(al, "ALGALON_SOURCE", bare / "missing.cpp"):
                    al.ring_slots.cache_clear()
                    self.assertEqual(al.ring_slots(), tuple(al.arc_slots()))
            finally:
                al.ring_slots.cache_clear()

    def test_an_explosion_is_survivable_above_its_top_roll_or_at_full_health(self):
        trace = types.SimpleNamespace(header={"diff": 1}, maxhp={1: 23000, 2: 30000, 4: 30000, 5: 20000})
        rows = ((1, 100.0), (2, 80.0), (3, 10.0), (4, 70.0), (5, 99.0))
        up = {guid: [guid, 0.0, 0.0, 0.0, 0.0, hp] for guid, hp in rows}
        self.assertEqual(al.explosion_safe_hp(trace), 21526)
        self.assertEqual(al.below_safe(trace, up), [(19800, 5, 99.0), (21000, 4, 70.0)])
        trace.header = {"diff": 2}
        self.assertEqual((al.explosion_safe_hp(trace), al.below_safe(trace, up)), (16913, []))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            al.show_banner(trace)
            for _, _, section in al.SECTIONS:
                section(trace)
        self.assertIn("not an Algalon pull", out.getvalue())


if __name__ == "__main__":
    unittest.main()
