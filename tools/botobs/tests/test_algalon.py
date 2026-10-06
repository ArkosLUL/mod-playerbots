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
    rows += [note(8000, MERCY, "algalon.tankhealer", 1), note(8000, MERCY, "algalon.tankhealslots", 1),
             note(8000, MERCY, "algalon.tankheal", "full")]
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
            "Cosmic Smash @ 24.8s: dodged, then brought back inside 15 yd by the impact: Trueshot 12.0"
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
                                   " algalon.starwindow, algalon.swappair, algalon.tankhealer, algalon.tankrole")
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

    def test_tankdef_keeps_a_spell_named_in_several_words(self):
        self.assertEqual(al.parse_tankdef("window 72 8101 Hand of Sacrifice"),
                         ("window", 72.0, 8101, "Hand of Sacrifice"))
        self.assertEqual(al.parse_tankdef("loh 18 8102 Lay on Hands"), ("loh", 18.0, 8102, "Lay on Hands"))
        self.assertEqual(al.parse_tankdef(None), ("", None, 0, ""))

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


TH_TANK, TH_PARTNER, TH_HOLY, TH_DISC, TH_RESTO, TH_SHAMAN, TH_DPS = range(8001, 8008)
TH_DEATH = 20000
TH_END = 30000


def heal(when: int, source: int, target: int, amount: int, over: int = 0) -> dict:
    return {"t": when, "e": "heal", "s": source, "d": target, "sp": 48782, "a": amount, "oh": over, "hp": 50.0}


def absorb(when: int, caster: int, target: int, amount: int) -> dict:
    return {"t": when, "e": "abs", "d": target, "s": caster, "sp": 48066, "a": amount}


def tankheal_pull() -> list[dict]:
    """Two bot tank healers and a human one on four healers; the holder dies with the paladin 10 yd off and
    the priest phased."""
    roster = [
        {"g": TH_TANK, "n": "Tanka", "r": "tank", "c": "paladin", "h": 0},
        {"g": TH_PARTNER, "n": "Tankb", "r": "tank", "c": "paladin", "h": 0},
        {"g": TH_HOLY, "n": "Holy", "r": "heal", "c": "paladin", "h": 0},
        {"g": TH_DISC, "n": "Disc", "r": "heal", "c": "priest", "h": 0},
        {"g": TH_RESTO, "n": "Resto", "r": "heal", "c": "druid", "h": 1},
        {"g": TH_SHAMAN, "n": "Shaman", "r": "heal", "c": "shaman", "h": 0},
        {"g": TH_DPS, "n": "Dps", "r": "ranged", "c": "mage", "h": 0},
    ]
    home = {TH_TANK: (0, 0), TH_PARTNER: (3, 0), TH_HOLY: (0, 10), TH_DISC: (6, 12), TH_RESTO: (0, 50),
            TH_SHAMAN: (-8, 12), TH_DPS: (10, 20)}
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 603, "inst": 4, "diff": 1,
         "boss": "algalon-the-observer", "roster": roster},
        {"t": 0, "e": "pull", "boss": "algalon-the-observer", "src": "engage"},
        {"t": 1, "e": "unit", "g": ALGALON, "en": al.NPC_ALGALON, "n": "Algalon the Observer", "b": 1},
        *({"t": 1, "e": "unit", "g": member["g"], "n": member["n"], "mhp": 30000} for member in roster),
        note(1000, TH_TANK, "algalon.swappair", 1), note(1000, TH_PARTNER, "algalon.swappair", 1),
        *(note(1000, guid, "algalon.tankhealer", 1) for guid in (TH_HOLY, TH_DISC, TH_RESTO)),
        note(1000, TH_HOLY, "algalon.tankhealslots", 2),
        note(16000, TH_HOLY, "algalon.tankheal", "holder"),
        # Beacon on the partner for 10 s, then on the holder for 2.
        {"t": 2000, "e": "aura", "d": TH_PARTNER, "s": TH_HOLY, "sp": al.SPELL_BEACON_OF_LIGHT, "r": 0, "st": 1,
         "dur": 60000, "p": 1},
        {"t": 12000, "e": "aura", "d": TH_PARTNER, "s": TH_HOLY, "sp": al.SPELL_BEACON_OF_LIGHT, "r": 1, "st": 1,
         "dur": 0, "p": 1},
        {"t": 12000, "e": "aura", "d": TH_TANK, "s": TH_HOLY, "sp": al.SPELL_BEACON_OF_LIGHT, "r": 0, "st": 1,
         "dur": 60000, "p": 1},
        {"t": 14000, "e": "aura", "d": TH_TANK, "s": TH_HOLY, "sp": al.SPELL_BEACON_OF_LIGHT, "r": 1, "st": 1,
         "dur": 0, "p": 1},
        *phased(TH_DISC, al.SPELL_BLACK_HOLE_PHASE, 18000, 25000),
        # 83k in the 6 s before the death, 63k of it in the last 2.
        dmg(15000, ALGALON, TH_TANK, 0, 20000), dmg(18500, ALGALON, TH_TANK, 0, 25000),
        dmg(19200, ALGALON, TH_TANK, al.SPELL_QUANTUM_STRIKE[1], 18000),
        dmg(TH_DEATH, ALGALON, TH_TANK, 0, 20000, hp=10.0, over=5000),
        death(TH_DEATH, TH_TANK, ALGALON, 20000, home[TH_TANK]),
        heal(16000, TH_HOLY, TH_TANK, 8000), heal(17500, TH_HOLY, TH_HOLY, 2000),
        heal(17000, TH_DISC, TH_DPS, 12000), absorb(15500, TH_DISC, TH_TANK, 5000),
        heal(19000, TH_SHAMAN, TH_TANK, 4000, over=1000),
        {"t": 17000, "e": "veto", "g": TH_HOLY, "m": al.TANKHEAL_VETO, "a": "flash of light on party"},
        {"t": TH_END, "e": "end", "out": "wipe"},
    ]
    for when in range(0, TH_END, 500):
        units = [row(guid, offset, 0.0 if guid == TH_TANK and when > TH_DEATH else 100.0, ALGALON)
                 for guid, offset in home.items()]
        units.append(row(ALGALON, (0, 4), 90.0, TH_TANK if when <= TH_DEATH else TH_PARTNER))
        records.append({"t": when, "e": "snap", "u": units, "hz": []})
    return records


class TankHeal(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, tankheal_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_tank_death_reads_damage_against_healing_over_the_six_seconds_before(self):
        (row,) = al.tank_death_rows(self.trace)
        self.assertEqual({key: row[key] for key in ("t", "guid", "taken", "burst", "healed", "by_tank_healers",
                                                    "absorbed", "elsewhere", "up")},
                         {"t": TH_DEATH, "guid": TH_TANK, "taken": 83000, "burst": 63000, "healed": 11000,
                          "by_tank_healers": 8000, "absorbed": 5000, "elsewhere": 14000, "up": 3})
        states = {state["guid"]: state for state in row["healers"]}
        self.assertEqual(set(states), {TH_HOLY, TH_DISC, TH_RESTO})
        holy = states[TH_HOLY]
        self.assertEqual((holy["alive"], holy["phased"], round(holy["distance"], 1), holy["branch"], holy["vetoes"],
                          holy["healed"]), (True, False, 10.0, "holder", 1, 8000))
        self.assertTrue(states[TH_DISC]["phased"])

    def test_shares_split_the_pair_self_and_everyone_else(self):
        shares = {row["guid"]: row for row in al.healer_shares(self.trace)}
        self.assertEqual(dict(shares[TH_HOLY]["all"]), {"tanks": 8000, "self": 2000})
        self.assertEqual(dict(shares[TH_DISC]["assigned"]), {"tanks": 5000, "others": 12000})
        self.assertEqual(dict(shares[TH_SHAMAN]["assigned"]), {})

    def test_beacon_time_splits_on_the_tank_he_hit(self):
        self.assertEqual(al.beacon_seconds(self.trace), (2000, 10000))

    def test_faults_name_the_idle_tank_healer_the_spread_the_human_and_the_cap(self):
        self.assertEqual([line for _, line in al.tankheal_faults(self.trace)], [
            "Tanka died @ 20.0s with tank healer Holy alive, unphased and 10 yd away: healed 11,000 of 83,000 taken"
            " in 6 s",
            "tank healer Disc put 71% of its healing on others",
            "human Resto assigned as a tank healer @ 1.0s",
            "3 tank healers @ 1.0s with 4 healers in the raid",
        ])

    def test_the_section_ends_on_the_summary_line(self):
        text = render(self.trace, "tankheal")
        self.assertIn("tank healer: Resto (heal)* 0:01.000-0:30.000", text)
        self.assertIn("Beacon of Light: 2.0 s on the tank he was hitting, 10.0 s on the other", text)
        self.assertIn("1 tank death; medians over the 6 s before: took 83k (63k in 2 s), healed 11k + 5k shields,"
                      " 14k into others; every healer alive and unphased in 0 of 1; healers' share on the pair"
                      " 29%-100%", text)

    def test_the_probes_are_owed_once_a_bot_could_hold_the_pair(self):
        expected = al.expected_probes(self.trace)
        self.assertIn("algalon.tankhealer", expected)
        self.assertIn("algalon.tankheal", expected)


DF_TANK, DF_PARTNER, DF_HOLY, DF_RET, DF_DISC, DF_SHADE, DF_MAGE = range(8101, 8108)
DF_STAR_A, DF_STAR_B, DF_HOLE_A, DF_HOLE_B = (CREATURE | n for n in (301, 302, 321, 322))
DF_DEATH = 30000
DF_BANG = 36000
DF_END = 60000
DF_HOME = {DF_TANK: (0, 0), DF_PARTNER: (3, 0), DF_HOLY: (0, 12), DF_RET: (2, 3), DF_DISC: (-6, 12),
           DF_SHADE: (8, 15), DF_MAGE: (-8, 18)}
DF_HOLE_B_SPOT = (20, 30)
DF_NEW_KEYS = ("algalon.disperser", "algalon.shadowaura", "algalon.tankdef", "algalon.cover")
DF_SPELLS = {64592: "Quantum Strike", 6940: "Hand of Sacrifice", 498: "Divine Protection", 871: "Shield Wall",
             33206: "Pain Suppression", 48788: "Lay on Hands", 64205: "Divine Sacrifice", 70940: "Divine Guardian",
             31821: "Aura Mastery", 47585: "Dispersion", 64122: "Black Hole Explosion", 64443: "Big Bang"}


def buff(when: int, target: int, spell: int, caster: int, removed: bool = False) -> dict:
    return {"t": when, "e": "aura", "d": target, "s": caster, "sp": spell, "r": int(removed), "st": 1,
            "dur": 0 if removed else 10000, "p": 1}


def worn(target: int, spell: int, caster: int, start: int, stop: int) -> list[dict]:
    return [buff(start, target, spell, caster), buff(stop, target, spell, caster, True)]


def blast(when: int, star: int, victim: int, amount: int, resisted: int, absorbed: int = 0) -> dict:
    return {**dmg(when, star, victim, al.SPELL_BLACK_HOLE_EXPLOSION[0], amount), "ab": absorbed, "rs": resisted}


def df_hp(guid: int, when: int) -> float:
    if guid == DF_TANK:
        return 0.0 if when >= DF_DEATH else 72.0 if when < 12000 else 55.0
    if guid == DF_PARTNER:
        return 0.0 if when >= 55000 else 20.0 if when >= 50000 else 80.0
    return 30.0 if guid == DF_MAGE and when >= 44000 else 100.0


def defensives_pull(drop: tuple[str, ...] = ()) -> list[dict]:
    """A paladin holder whose Hand of Sacrifice and Divine Protection run out before his swings kill it; the
    warrior partner takes him, holds the constellation through Big Bang and goes last into a hole that
    closes behind it, then dies after Lay on Hands. A shadow priest soaks, the mage is caught out. One
    explosion lands under the ret's Divine Guardian, whose Divine Sacrifice stays up 1.7 s; the other lands
    uncovered with the holy paladin's Aura Mastery ready. `drop` leaves out the notes of those keys."""
    roster = [
        {"g": DF_TANK, "n": "Tanka", "r": "tank", "c": "paladin", "h": 0},
        {"g": DF_PARTNER, "n": "Tankb", "r": "tank", "c": "warrior", "h": 0},
        {"g": DF_HOLY, "n": "Holy", "r": "heal", "c": "paladin", "h": 0},
        {"g": DF_RET, "n": "Ret", "r": "melee", "c": "paladin", "h": 0},
        {"g": DF_DISC, "n": "Disc", "r": "heal", "c": "priest", "h": 0},
        {"g": DF_SHADE, "n": "Shade", "r": "ranged", "c": "priest", "h": 0},
        {"g": DF_MAGE, "n": "Mage", "r": "ranged", "c": "mage", "h": 0},
    ]
    covered = (DF_RET, DF_MAGE, DF_SHADE, DF_HOLY, DF_DISC)
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 603, "inst": 4, "diff": 1,
         "boss": "algalon-the-observer", "roster": roster},
        {"t": 0, "e": "pull", "boss": "algalon-the-observer", "src": "engage"},
        {"t": 1, "e": "unit", "g": ALGALON, "en": al.NPC_ALGALON, "n": "Algalon the Observer", "b": 1},
        {"t": 1, "e": "unit", "g": DF_STAR_A, "en": al.NPC_COLLAPSING_STAR, "n": "Collapsing Star"},
        {"t": 1, "e": "unit", "g": DF_STAR_B, "en": al.NPC_COLLAPSING_STAR, "n": "Collapsing Star"},
        {"t": 1, "e": "unit", "g": DF_HOLE_A, "en": al.NPC_BLACK_HOLE, "n": "Black Hole"},
        {"t": 1, "e": "unit", "g": DF_HOLE_B, "en": al.NPC_BLACK_HOLE, "n": "Black Hole"},
        *({"t": 1, "e": "unit", "g": member["g"], "n": member["n"], "mhp": 30000} for member in roster),
        *({"t": 1, "e": "spell", "sp": spell, "n": name} for spell, name in DF_SPELLS.items()),

        note(1000, DF_TANK, "algalon.swappair", 1), note(1000, DF_PARTNER, "algalon.swappair", 1),
        note(1000, DF_HOLY, "algalon.disperser", DF_SHADE), note(1000, DF_HOLY, "algalon.shadowaura", DF_HOLY),
        note(1000, DF_HOLY, "algalon.handler", DF_PARTNER),

        # The holder's cutters, a triggered echo and a holy paladin's own Divine Protection; a Pain
        # Suppression claimed at 26 s never goes out.
        cast(8000, DF_RET, al.SPELL_HAND_OF_SACRIFICE, DF_TANK),
        {**cast(8000, DF_RET, al.SPELL_HAND_OF_SACRIFICE, DF_TANK), "tr": 1},
        *worn(DF_TANK, al.SPELL_HAND_OF_SACRIFICE, DF_RET, 8000, 20000),
        cast(9000, DF_HOLY, 498),
        cast(15000, DF_TANK, 498), *worn(DF_TANK, 498, DF_TANK, 15000, 27000),
        cast(25000, DF_PARTNER, 871), *worn(DF_PARTNER, 871, DF_PARTNER, 25000, 37000),
        note(8000, DF_RET, "algalon.tankdef", f"window 72 {DF_TANK} Hand of Sacrifice"),
        note(15000, DF_TANK, "algalon.tankdef", f"low 55 {DF_TANK} Divine Protection"),
        note(26000, DF_DISC, "algalon.tankdef", f"window 60 {DF_TANK} Pain Suppression"),
        dmg(23000, ALGALON, DF_TANK, 0, 9000, hp=70.0), dmg(25000, ALGALON, DF_TANK, 0, 15000, hp=60.0),
        {**dmg(27500, ALGALON, DF_TANK, al.SPELL_QUANTUM_STRIKE[1], 12000, hp=35.0), "ab": 2000},
        dmg(DF_DEATH, ALGALON, DF_TANK, 0, 14000, hp=0.0, over=3000),
        death(DF_DEATH, DF_TANK, ALGALON, 14000, DF_HOME[DF_TANK]),

        # Star A under Divine Guardian, star B with nothing up.
        cast(18500, DF_RET, al.SPELL_DIVINE_SACRIFICE), note(18500, DF_RET, "algalon.cover", "divine sacrifice"),
        *(rec for guid in (DF_RET, DF_MAGE) for rec in worn(guid, al.SPELL_DIVINE_SACRIFICE, DF_RET, 18500, 20200)),
        *(rec for guid in covered for rec in worn(guid, al.SPELL_DIVINE_GUARDIAN, DF_RET, 18500, 24500)),
        blast(20000, DF_STAR_A, DF_MAGE, 8000, 3000), blast(20000, DF_STAR_A, DF_SHADE, 8000, 3000),
        blast(20000, DF_STAR_A, DF_HOLY, 8000, 3000), blast(20000, DF_STAR_A, DF_DISC, 6000, 3000, absorbed=2000),
        blast(35000, DF_STAR_B, DF_MAGE, 12000, 2000), blast(35000, DF_STAR_B, DF_SHADE, 12000, 2000),
        blast(35000, DF_STAR_B, DF_RET, 12000, 2000),

        cast(DF_BANG, ALGALON, al.SPELL_BIG_BANG[0], cast_ms=8000),
        note(DF_BANG, DF_HOLY, "algalon.bigbang", 1), note(44500, DF_HOLY, "algalon.bigbang", 0),
        note(DF_BANG, DF_HOLY, "algalon.soaker", DF_SHADE), note(44500, DF_HOLY, "algalon.soaker", 0),
        note(DF_BANG, DF_SHADE, "algalon.soakkit", "def=dispersion forbearance=0"),
        note(DF_BANG, DF_SHADE, "algalon.hide", "soak"), note(44500, DF_SHADE, "algalon.hide", "none"),
        note(DF_BANG, DF_MAGE, "algalon.hide", "wait"), note(41000, DF_MAGE, "algalon.hide", "run"),
        note(DF_BANG, DF_PARTNER, "algalon.hide", "hold"), note(42500, DF_PARTNER, "algalon.hide", "run"),
        note(43200, DF_PARTNER, "algalon.hide", "in"), note(45000, DF_PARTNER, "algalon.hide", "none"),
        note(42500, DF_PARTNER, "algalon.shelter", DF_HOLE_B), note(43300, DF_HOLY, "algalon.holelost", "bigbang"),
        *(note(when, guid, "algalon.hide", value) for guid in (DF_HOLY, DF_DISC, DF_RET)
          for when, value in ((DF_BANG, "wait"), (40000, "run"), (41000, "in"), (45000, "none"))),
        *(rec for guid in (DF_HOLY, DF_DISC, DF_RET) for rec in phased(guid, al.SPELL_BLACK_HOLE_PHASE, 41000, 45000)),
        *phased(DF_PARTNER, al.SPELL_BLACK_HOLE_PHASE, 43200, 45000),
        cast(43000, DF_SHADE, al.SPELL_DISPERSION),
        dmg(44000, ALGALON, DF_SHADE, al.SPELL_BIG_BANG[0], 6000, hp=80.0),
        dmg(44000, ALGALON, DF_MAGE, al.SPELL_BIG_BANG[0], 20000, hp=30.0),

        # The partner, after the hit.
        cast(46000, DF_DISC, al.SPELL_PAIN_SUPPRESSION, DF_PARTNER),
        *worn(DF_PARTNER, al.SPELL_PAIN_SUPPRESSION, DF_DISC, 46000, 54000),
        note(46000, DF_DISC, "algalon.tankdef", f"window 80 {DF_PARTNER} Pain Suppression"),
        cast(50000, DF_HOLY, 48788, DF_PARTNER),
        note(50000, DF_HOLY, "algalon.tankdef", f"loh 20 {DF_PARTNER} Lay on Hands"),
        cast(50000, DF_HOLY, al.SPELL_AURA_MASTERY), *worn(DF_HOLY, al.SPELL_AURA_MASTERY, DF_HOLY, 50000, 56000),
        dmg(54000, ALGALON, DF_PARTNER, 0, 15000, hp=5.0), dmg(55000, ALGALON, DF_PARTNER, 0, 12000, hp=0.0, over=4000),
        death(55000, DF_PARTNER, ALGALON, 12000, DF_HOME[DF_PARTNER]),
        {"t": DF_END, "e": "end", "out": "wipe"},
    ]
    for when in range(0, DF_END, 500):
        units = [row(guid, offset, df_hp(guid, when), ALGALON) for guid, offset in DF_HOME.items()]
        units.append(row(ALGALON, (0, 4), 90.0, DF_TANK if when < DF_DEATH else DF_PARTNER))
        if 20500 <= when:
            units.append(row(DF_HOLE_A, (-20, 30), 100.0, 0))
        if 35500 <= when <= 43000:
            units.append(row(DF_HOLE_B, DF_HOLE_B_SPOT, 100.0, 0))
        records.append({"t": when, "e": "snap", "u": units, "hz": []})
    return [rec for rec in records if rec.get("e") != "note" or rec.get("k") not in drop]


class Defensives(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, defensives_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_saves_on_a_swap_tank_read_its_health_its_fate_and_the_note(self):
        self.assertEqual([(save["t"], save["caster"], save["target"], save["name"], save["hp"], save["died"],
                           save["why"]) for save in al.tank_saves(self.trace)], [
            (8000, DF_RET, DF_TANK, "Hand of Sacrifice", 72.0, None, "window"),
            (15000, DF_TANK, DF_TANK, "Divine Protection", 55.0, None, "low"),
            (25000, DF_PARTNER, DF_PARTNER, "Shield Wall", 80.0, None, None),
            (46000, DF_DISC, DF_PARTNER, "Pain Suppression", 80.0, 55000, "window"),
            (50000, DF_HOLY, DF_PARTNER, "Lay on Hands", 20.0, 55000, "loh")])

    def test_a_tank_death_reads_its_hits_the_cutters_up_and_the_notes(self):
        first, second = al.tank_deaths(self.trace)
        self.assertEqual((first["guid"], first["cause"], first["phased"], first["up"], first["cast"]),
                         (DF_TANK, "Algalon on tank", False, [], []))
        self.assertEqual([rec["t"] for rec in first["hits"]], [25000, 27500, DF_DEATH])
        self.assertEqual([(note["t"], note["caster"], note["why"], note["pct"], note["name"])
                          for note in first["notes"]], [(26000, DF_DISC, "window", 60.0, "Pain Suppression")])
        self.assertEqual([save["name"] for save in second["cast"]], ["Lay on Hands"])
        self.assertEqual([note["why"] for note in second["notes"]], ["window", "loh"])

    def test_an_explosion_is_covered_by_divine_guardian_or_aura_mastery(self):
        covered, bare = al.explosion_rows(self.trace)
        self.assertEqual((covered["t"], len(covered["guardians"]), covered["mastery"], covered["covered"],
                          covered["notes"], covered["ready"]),
                         (20000, 5, [], True, [(18500, DF_RET, "divine sacrifice")], []))
        self.assertEqual((al.hit_means(covered["hits"]), al.hit_means(bare["hits"])), ((8000, 3000), (12000, 2000)))
        # Ret's Divine Sacrifice is on cooldown, the holy paladin's Aura Mastery is not.
        self.assertEqual((bare["covered"], bare["ready"]), (False, [(DF_HOLY, al.SPELL_AURA_MASTERY)]))

    def test_the_faults_name_the_bare_death_the_held_sacrifice_and_the_uncovered_explosion(self):
        self.assertEqual([line for _, line in al.defensive_faults(self.trace)], [
            "Tanka died @ 30.0s with no damage cutter up and none cast on it in the 6 s before",
            "Divine Sacrifice from Ret up 1.7 s @ 18.5s on Mage, Ret",
            "star explosion @ 35.0s with no cover up, ready: Holy's Aura Mastery"])

    def test_big_bang_reads_the_dispersion_soak_and_the_handler_going_in_last(self):
        (row,) = al.big_bang_rows(self.trace)
        self.assertEqual((row["soaker"], row["kind"], row["stray"], row["lost"]),
                         (DF_SHADE, "dispersion", [DF_MAGE], [(43300, "bigbang")]))
        (handler,) = row["handlers"]
        self.assertEqual((handler["guid"], handler["hold"], handler["went"], handler["hole"], handler["phased"]),
                         (DF_PARTNER, DF_BANG, 42500, DF_HOLE_B, True))
        hole = spot(*DF_HOLE_B_SPOT)
        bang = render(self.trace, "bigbang")
        self.assertIn(f"on hold: Tankb from 0:36.000, went 0:42.500, 1.5 s before the impact, hole"
                      f" ({hole[0]:.0f},{hole[1]:.0f}) 34.5 yd; phased at the impact", bang)
        self.assertIn("algalon.holelost during the cast: 0:43.300 bigbang", bang)
        soak = render(self.trace, "soak")
        self.assertIn("soaker Shade (ranged), dispersion soak:", soak)
        self.assertIn("hit unphased besides the soaker and backup: Mage 20,000 (hide run)", soak)

    def test_the_verdict_takes_the_new_lines_and_frees_what_the_reserve_let_through(self):
        lines = [line for _, line in al.failures(self.trace)]
        for line in ("Tanka died @ 30.0s with no damage cutter up and none cast on it in the 6 s before",
                     "Big Bang 1 @ 36.0s: 1 non-soaker hit unphased (Mage)",
                     "star explosion @ 35.0s with no cover up, ready: Holy's Aura Mastery"):
            self.assertIn(line, lines)
        self.assertEqual(al.soak_faults(self.trace), [])

    def test_the_new_probes_are_owed_and_written(self):
        expected = al.expected_probes(self.trace)
        for key in DF_NEW_KEYS:
            self.assertIn(key, expected)
        missing = next((line for _, line in al.failures(self.trace) if line.startswith("missing probes")), "")
        self.assertFalse([key for key in DF_NEW_KEYS if key in missing])

    def test_the_section_lists_saves_deaths_explosions_and_the_held_sacrifice(self):
        text = render(self.trace, "defensives")
        for line in ("0:08.000 Ret Hand of Sacrifice -> Tanka at 72%, lived 10 s, algalon.tankdef window",
                     "0:50.000 Holy Lay on Hands -> Tankb at 20%, died +5.0 s, algalon.tankdef loh",
                     "0:30.000 Tanka (tank), Algalon on tank",
                     "cutters up: none; cast on it in the 6 s before: none",
                     "algalon.tankdef in the 10 s before: 0:26.000 Disc window 60% Pain Suppression",
                     "cover providers: Holy Aura Mastery; Ret Divine Sacrifice",
                     "0:20.000   4 hits, per hit 8,000 landed, 3,000 resisted; Divine Guardian on 5; algalon.cover"
                     " 0:18.500 divine sacrifice (Ret)",
                     "covered:   1 explosion(s), 4 hits, per hit 8,000 landed (absorbs included), 3,000 resisted",
                     "uncovered: 1 explosion(s), 3 hits, per hit 12,000 landed (absorbs included), 2,000 resisted",
                     "Divine Sacrifice up past 1 s on a bot: 0:18.500 Ret's 1.7 s on Mage, Ret"):
            self.assertIn(line, text)
        self.assertNotIn("\n  absent from this trace", text)


class OlderDefensives(unittest.TestCase):
    """The same pull as a trace from before the Dispersion soak, the tank defensive node and the cover."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, defensives_pull(drop=DF_NEW_KEYS + ("algalon.soakkit",)))

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_the_records_still_name_the_soak_the_saves_and_the_cover(self):
        (row,) = al.big_bang_rows(self.trace)
        self.assertEqual(row["kind"], "dispersion")
        self.assertEqual([save["why"] for save in al.tank_saves(self.trace)], [None] * 5)
        self.assertEqual(al.cover_providers(self.trace),
                         {DF_HOLY: [al.SPELL_AURA_MASTERY], DF_RET: [al.SPELL_DIVINE_SACRIFICE]})
        self.assertIn("star explosion @ 35.0s with no cover up, ready: Holy's Aura Mastery",
                      [line for _, line in al.defensive_faults(self.trace)])

    def test_without_the_disperser_the_reserve_holds_the_tank_buttons(self):
        self.assertEqual([line for _, line in al.soak_faults(self.trace)], [
            "Tanka spent Divine Protection @ 15.0s, outside a Big Bang soak",
            "Tankb spent Shield Wall @ 25.0s, outside a Big Bang soak"])

    def test_the_verdict_owes_every_new_key(self):
        missing = next(line for _, line in al.failures(self.trace) if line.startswith("missing probes"))
        for key in DF_NEW_KEYS:
            self.assertIn(key, missing)
        self.assertIn("absent from this trace: algalon.tankdef, algalon.cover, algalon.shadowaura",
                      render(self.trace, "defensives"))


class DispersionEra(unittest.TestCase):
    """The synthetic pull written after the Dispersion soak: `hold` no longer keeps a handler out."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, algalon_pull() + [note(8000, MERCY, "algalon.disperser", 0)])

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_handler_hit_on_hold_counts(self):
        first = al.big_bang_rows(self.trace)[0]
        self.assertEqual((first["kind"], first["stray"]), ("tank", [AEGIS, TRUESHOT]))
        self.assertEqual([(handler["guid"], handler["hold"], handler["went"], handler["phased"])
                          for handler in first["handlers"]], [(AEGIS, 40000, None, False)])
        self.assertIn("Big Bang 1 @ 40.0s: 2 non-soakers hit unphased (Aegis, Trueshot)",
                      [line for _, line in al.failures(self.trace)])
        self.assertIn("on hold: Aegis from 0:40.000, still on hold at the impact, hole -; unphased at the impact",
                      render(self.trace, "bigbang"))


UN_TANK, UN_PARTNER, UN_HOLY, UN_WARL, UN_ROGUE, UN_MAGE, UN_BOW = range(8201, 8208)
UN_STAR_A, UN_STAR_B, UN_STAR_C, UN_HOLE_A, UN_HOLE_B, UN_CON_A, UN_CON_B = (
    CREATURE | n for n in (401, 402, 403, 421, 422, 431, 432))
UN_END = 60000
UN_HOME = {UN_TANK: (0, 0), UN_PARTNER: (3, 0), UN_HOLY: (0, 12), UN_WARL: (8, 15), UN_ROGUE: (2, 3),
           UN_MAGE: (-8, 18), UN_BOW: (6, 14)}
UN_HOLE_A_SPOT = (-20, 30)
UN_HOLE_B_SPOT = (20, 30)
UN_DEATHS = {UN_WARL: 34000, UN_ROGUE: 28000}
UN_NEW_KEYS = ("algalon.nitro", "algalon.personal", "algalon.trim", "algalon.kite", "algalon.starplan",
               "algalon.stardeath")
UN_SPELLS = {47891: "Shadow Ward", 45438: "Ice Block", 642: "Divine Shield", 31224: "Cloak of Shadows",
             6940: "Hand of Sacrifice", 64122: "Black Hole Explosion", 64443: "Big Bang"}


def un_hp(guid: int, when: int) -> float:
    return 0.0 if when >= UN_DEATHS.get(guid, 1 << 30) else 100.0


def un_offset(guid: int, when: int):
    """The partner kites 8 yd off the centre of the hole it was sent to, once it has crossed."""
    if guid == UN_PARTNER and 9000 <= when < 13000:
        return UN_HOLE_A_SPOT[0], UN_HOLE_A_SPOT[1] - 8
    if guid == UN_PARTNER and 43000 <= when < 49000:
        return UN_HOLE_B_SPOT[0], UN_HOLE_B_SPOT[1] - 8
    return UN_HOME[guid]


def under_pull(drop: tuple[str, ...] = ()) -> list[dict]:
    """Three stars planned to die under Big Bang 1, the window 0:32 .. 0:40: one dies inside it, one 4 s
    early, one 4.5 s late. A kite that eats a constellation, and one that parks 6 s beside a hole with nothing
    eaten while the other constellation chases a tank at the cast start. The warlock dies to an explosion
    with Shadow Ward ready, the rogue after pressing Cloak of Shadows; the mage casts Ice Block inside a
    hole's phase. `drop` leaves out the notes of those keys."""
    roster = [
        {"g": UN_TANK, "n": "Tanka", "r": "tank", "c": "warrior", "h": 0},
        {"g": UN_PARTNER, "n": "Tankb", "r": "tank", "c": "paladin", "h": 0},
        {"g": UN_HOLY, "n": "Holy", "r": "heal", "c": "paladin", "h": 0},
        {"g": UN_WARL, "n": "Warl", "r": "ranged", "c": "warlock", "h": 0},
        {"g": UN_ROGUE, "n": "Rogue", "r": "melee", "c": "rogue", "h": 0},
        {"g": UN_MAGE, "n": "Mage", "r": "ranged", "c": "mage", "h": 0},
        {"g": UN_BOW, "n": "Bow", "r": "ranged", "c": "hunter", "h": 0},
    ]
    one, two, three = (guid & 0xFFFFFFFF for guid in (UN_STAR_A, UN_STAR_B, UN_STAR_C))
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 603, "inst": 4, "diff": 0,
         "boss": "algalon-the-observer", "roster": roster},
        {"t": 0, "e": "pull", "boss": "algalon-the-observer", "src": "engage"},
        {"t": 1, "e": "unit", "g": ALGALON, "en": al.NPC_ALGALON, "n": "Algalon the Observer", "b": 1},
        *({"t": 1, "e": "unit", "g": guid, "en": al.NPC_COLLAPSING_STAR, "n": "Collapsing Star"}
          for guid in (UN_STAR_A, UN_STAR_B, UN_STAR_C)),
        *({"t": 1, "e": "unit", "g": guid, "en": al.NPC_BLACK_HOLE, "n": "Black Hole"}
          for guid in (UN_HOLE_A, UN_HOLE_B)),
        *({"t": 1, "e": "unit", "g": guid, "en": al.NPC_LIVING_CONSTELLATION, "n": "Living Constellation"}
          for guid in (UN_CON_A, UN_CON_B)),
        *({"t": 1, "e": "unit", "g": member["g"], "n": member["n"], "mhp": 30000} for member in roster),
        *({"t": 1, "e": "spell", "sp": spell, "n": name} for spell, name in UN_SPELLS.items()),

        note(1000, UN_HOLY, "algalon.swappair", 1), note(1000, UN_PARTNER, "algalon.swappair", 1),
        note(1000, UN_HOLY, "algalon.handler", UN_PARTNER),
        note(5000, UN_PARTNER, "algalon.kitehole", UN_HOLE_A), note(40000, UN_PARTNER, "algalon.kitehole", UN_HOLE_B),

        # Window cutters on a holder: only the first is spent at 80% or more.
        note(10000, UN_HOLY, "algalon.tankdef", f"window 85 {UN_PARTNER} Hand of Sacrifice"),
        note(12000, UN_HOLY, "algalon.tankdef", f"window 60 {UN_PARTNER} Hand of Sacrifice"),
        note(14000, UN_HOLY, "algalon.tankdef", f"low 90 {UN_PARTNER} Divine Protection"),

        # Forecast deaths at 34 s, 28 s and 40 s; the window opens 12 s after the note and shuts 20 s after.
        note(20000, UN_HOLY, "algalon.starplan",
             f"{two} 80% 8s u, {one} 90% 14s u, {three} 70% 20s u | smash 40..50 | under 12..20"),
        note(22000, UN_BOW, "algalon.trim", f"start {one} 100"), note(26000, UN_BOW, "algalon.trim", f"stop {one} 55"),
        note(28000, UN_HOLY, "algalon.stardeath", "early 9.0"), note(34000, UN_HOLY, "algalon.stardeath", "under 11.0"),
        note(44500, UN_HOLY, "algalon.stardeath", "late 7.0"),

        # The warlock cast Shadow Ward early on; the rogue pressed Cloak of Shadows a second before star B.
        cast(1000, UN_WARL, 47891, UN_WARL),
        note(27000, UN_ROGUE, "algalon.personal", "Cloak of Shadows"),

        # Star B explodes on the rogue and Bow, star A inside the cast on the warlock and the phased mage, star
        # C on the mage, phased again.
        dmg(28000, UN_STAR_B, UN_ROGUE, al.SPELL_BLACK_HOLE_EXPLOSION[0], 20000, hp=0.0, over=4000),
        blast(28000, UN_STAR_B, UN_BOW, 5000, 0),
        death(28000, UN_ROGUE, UN_STAR_B, 20000, UN_HOME[UN_ROGUE]),
        cast(30000, ALGALON, al.SPELL_BIG_BANG[0], cast_ms=8000),
        aura(33000, UN_MAGE, al.SPELL_BLACK_HOLE_PHASE), aura(39000, UN_MAGE, al.SPELL_BLACK_HOLE_PHASE, True),
        dmg(34000, UN_STAR_A, UN_WARL, al.SPELL_BLACK_HOLE_EXPLOSION[0], 20000, hp=0.0, over=4000),
        blast(34000, UN_STAR_A, UN_MAGE, 5000, 0),
        death(34000, UN_WARL, UN_STAR_A, 20000, UN_HOME[UN_WARL]),
        dmg(38000, ALGALON, UN_TANK, al.SPELL_BIG_BANG[0], 45000),
        aura(44000, UN_MAGE, al.SPELL_BLACK_HOLE_PHASE), aura(45000, UN_MAGE, al.SPELL_BLACK_HOLE_PHASE, True),
        cast(44200, UN_MAGE, 45438, UN_MAGE),
        blast(44500, UN_STAR_C, UN_MAGE, 5000, 0),
        cast(50000, UN_HOLY, 642, UN_HOLY),

        # Hide runs through the cast: Holy has a note for its Nitro Boosts, Bow only the aura, Mage neither.
        note(30000, UN_HOLY, "algalon.hide", "wait"), note(32000, UN_HOLY, "algalon.hide", "run"),
        note(35000, UN_HOLY, "algalon.hide", "in"), note(40000, UN_HOLY, "algalon.hide", "none"),
        note(32500, UN_BOW, "algalon.hide", "run"), note(35000, UN_BOW, "algalon.hide", "in"),
        note(40000, UN_BOW, "algalon.hide", "none"),
        note(33000, UN_MAGE, "algalon.hide", "run"), note(35000, UN_MAGE, "algalon.hide", "in"),
        note(40000, UN_MAGE, "algalon.hide", "none"),
        note(32100, UN_HOLY, "algalon.nitro", "hide"), aura(32600, UN_BOW, al.SPELL_NITRO_BOOSTS),

        # Kite 1: taunt, stage, wait, cross, park beside the hole; its constellation leaves with the hole at
        # 12 s. Kite 2 parks 6 s beside the other hole and nothing goes through it.
        note(1000, UN_PARTNER, "algalon.kite", "none"), note(5000, UN_PARTNER, "algalon.kite", "taunt"),
        note(6000, UN_PARTNER, "algalon.kite", "stage"), note(7000, UN_PARTNER, "algalon.kite", "wait"),
        note(8000, UN_PARTNER, "algalon.kite", "cross"), note(9000, UN_PARTNER, "algalon.kite", "parked"),
        note(13000, UN_PARTNER, "algalon.kite", "kept"),
        note(40000, UN_PARTNER, "algalon.kite", "stage"), note(41000, UN_PARTNER, "algalon.kite", "wait"),
        note(42000, UN_PARTNER, "algalon.kite", "cross"), note(43000, UN_PARTNER, "algalon.kite", "parked"),
        note(49000, UN_PARTNER, "algalon.kite", "none"),
        {"t": UN_END, "e": "end", "out": "wipe"},
    ]
    for when in range(0, UN_END, 500):
        units = [row(guid, un_offset(guid, when), un_hp(guid, when), ALGALON) for guid in UN_HOME]
        units.append(row(ALGALON, (0, 4), 90.0, UN_TANK))
        for star, until in ((UN_STAR_A, 34000), (UN_STAR_B, 28000), (UN_STAR_C, 44500)):
            if when <= until:
                units.append(row(star, (0, 30), 100.0 - when / 1000.0, 0))
        if when <= 12000:
            units.append(row(UN_HOLE_A, UN_HOLE_A_SPOT, 100.0, 0))
        if 5000 <= when <= 12000:
            units.append(row(UN_CON_A, UN_HOLE_A_SPOT if when == 12000 else (-20, 24), 100.0, UN_PARTNER))
        if when >= 3000:
            units.append(row(UN_HOLE_B, UN_HOLE_B_SPOT, 100.0, 0))
        if when >= 20000:
            units.append(row(UN_CON_B, (10, 20), 100.0, UN_TANK))
        records.append({"t": when, "e": "snap", "u": units, "hz": []})
    return [rec for rec in records if rec.get("e") != "note" or rec.get("k") not in drop]


class StarsUnder(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, under_pull())
        cls.lines = [line for _, line in al.failures(cls.trace)]

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_star_plan_reads_the_marks_and_the_under_window(self):
        self.assertEqual(al.parse_starplan("2136 28% 3s u, 2133 85% 83s s, 2134 50% 9s | smash 2..11 | under -1..9"), {
            "stars": {2136: {"hp": 28, "secs": 3, "mark": "u"}, 2133: {"hp": 85, "secs": 83, "mark": "s"},
                      2134: {"hp": 50, "secs": 9, "mark": ""}},
            "under": (-1, 9)})
        self.assertEqual(al.parse_starplan("smash 26..35 | prebang 89..101"), {"stars": {}, "under": None})
        self.assertEqual(al.parse_starplan(None), {"stars": {}, "under": None})

    def test_a_trim_note_reads_the_action_the_star_and_its_health(self):
        self.assertEqual(al.parse_trim("start 1858 100"), ("start", 1858, 100.0))
        self.assertEqual(al.parse_trim("stop 1858 -"), ("stop", 1858, None))
        self.assertEqual(al.parse_trim(""), ("", 0, None))

    def test_a_star_carries_its_plan_window_and_wand_runs(self):
        rows = {row["star"]: row for row in al.star_rows(self.trace)}
        a = rows[UN_STAR_A]
        self.assertEqual((a["plan"]["mark"], a["plan"]["window"], a["plan"]["forecast"]),
                         ("u", (32000, 40000), 34000))
        self.assertEqual((a["cause"], a["bang_hit"]), ("under", {"n": 1, "rel": -4000, "inside": True}))
        self.assertEqual(a["unphased"], [UN_WARL])
        self.assertEqual(a["trims"], [{"bot": UN_BOW, "start": 22000, "start_hp": 100.0, "stop": 26000,
                                       "stop_hp": 55.0}])
        self.assertEqual(rows[UN_STAR_B]["bang_hit"], {"n": 1, "rel": -10000, "inside": False})
        self.assertEqual(rows[UN_STAR_B]["unphased"], [UN_BOW, UN_ROGUE])
        self.assertEqual((rows[UN_STAR_C]["unphased"], rows[UN_STAR_C]["bang_hit"]["rel"]), ([], 6500))

    def test_the_stars_section_shows_what_each_was_planned_to_do(self):
        text = render(self.trace, "stars")
        self.assertIn("plan under window 0:32.000..0:40.000, forecast death +0.0 s off; died under; 4.0 s before"
                      " Big Bang 1's hit, inside its cast; explosion hit unphased: Warl; trim Bow 0:22.000 100%"
                      " .. 0:26.000 55%", text)
        self.assertIn("explosion hit unphased: nobody", text)
        self.assertIn("algalon.trim: 1 wand run(s) on 1 star(s)", text)

    def test_an_under_star_dying_outside_its_window_is_a_failure(self):
        self.assertIn("under-star died early @ 28.0s, 4.0 s before its window 0:32.000..0:40.000", self.lines)
        self.assertIn("under-star died late @ 44.5s, 4.5 s after its window 0:32.000..0:40.000", self.lines)
        self.assertEqual(sum(line.startswith("under-star died") for line in self.lines), 2)
        # dying under the cast is the plan, so it is not a star dying in a Big Bang by accident
        self.assertNotIn("star died during Big Bang 1's cast @ 34.0s", self.lines)

    def test_the_window_decides_the_cause_when_the_probe_never_wrote_one(self):
        with tempfile.TemporaryDirectory() as folder:
            trace = load(folder, under_pull(drop=("algalon.stardeath",)))
            self.assertEqual({row["star"]: row["cause"] for row in al.star_rows(trace)},
                             {UN_STAR_A: "under", UN_STAR_B: "early", UN_STAR_C: "late"})
            self.assertIn("under-star died early @ 28.0s, 4.0 s before its window 0:32.000..0:40.000",
                          [line for _, line in al.failures(trace)])

    def test_a_kite_reads_its_stage_timeline_and_whether_it_ate(self):
        first, second = al.kite_runs(self.trace)
        self.assertEqual([(stage, start) for stage, start, _ in first["stages"]], [
            ("taunt", 5000), ("stage", 6000), ("wait", 7000), ("cross", 8000), ("parked", 9000)])
        self.assertEqual((first["start"], first["stop"], [row["guid"] for row in first["eaten"]], first["retries"]),
                         (5000, 13000, [UN_CON_A], 0))
        self.assertEqual((second["start"], second["stop"], second["eaten"]), (40000, 49000, []))
        text = render(self.trace, "constellations")
        self.assertIn("kite runs, 1 of 2 ended with a constellation eaten:", text)
        self.assertIn("Tankb 0:05.000 .. 0:13.000: taunt 0:05.000 > stage 0:06.000 > wait 0:07.000 > cross 0:08.000"
                      " > parked 0:09.000; eaten 1 at 0:12.000", text)
        self.assertIn("parked 6.0 s from 0:43.000, Tankb 8.0 yd from its kite hole, none eaten", text)

    def test_a_pass_that_missed_counts_a_retry(self):
        extra = [note(10000, UN_PARTNER, "algalon.kite", "stage"), note(11000, UN_PARTNER, "algalon.kite", "parked")]
        with tempfile.TemporaryDirectory() as folder:
            trace = load(folder, under_pull() + extra)
            self.assertEqual([run["retries"] for run in al.kite_runs(trace)], [1, 0])

    def test_a_stretch_parked_beside_the_hole_with_nothing_eaten_is_a_failure(self):
        eaten, bare = al.parked_stretches(self.trace)
        self.assertEqual((eaten["guid"], eaten["eaten"], bare["guid"], bare["eaten"], bare["hole"]),
                         (UN_PARTNER, True, UN_PARTNER, False, UN_HOLE_B))
        self.assertAlmostEqual(bare["gap"], 8.0, places=1)
        self.assertIn("Tankb parked 6.0 s @ 43.0s, 8.0 yd from its kite hole, with no constellation eaten",
                      self.lines)
        self.assertEqual(sum(" parked " in line for line in self.lines), 1)

    def test_a_constellation_on_anyone_but_the_handler_at_the_cast_start_is_a_failure(self):
        self.assertEqual([(row["n"], row["guid"], row["victim"], row["handler"])
                          for row in al.loose_at_bang(self.trace)], [(1, UN_CON_B, UN_TANK, UN_PARTNER)])
        self.assertIn("Big Bang 1 @ 30.0s: a constellation loose on Tanka, not the handler Tankb", self.lines)

    def test_an_immunity_that_took_a_phase_off_is_a_failure(self):
        self.assertEqual([(row["t"], row["guid"], row["name"], row["start"], row["stop"])
                          for row in al.immunity_strips(self.trace)], [(44200, UN_MAGE, "Ice Block", 44000, 45000)])
        self.assertIn("Mage cast Ice Block while phased @ 44.2s and lost its phase aura", self.lines)
        self.assertEqual(sum("while phased" in line for line in self.lines), 1)

    def test_big_bang_names_each_hider_that_used_nitro_boosts(self):
        self.assertEqual(al.nitro_uses(self.trace), {UN_HOLY: [32100], UN_BOW: [32600]})
        self.assertEqual(al.bang_hiders(self.trace, al.big_bang_rows(self.trace)[0]), [UN_BOW, UN_HOLY, UN_MAGE])
        self.assertIn("Nitro Boosts: used by Holy 0:32.100, Bow 0:32.600; hid without: Mage",
                      render(self.trace, "bigbang"))

    def test_an_explosion_death_reads_the_buttons_pressed_and_the_ones_ready(self):
        rogue, warlock = al.explosion_deaths(self.trace)
        self.assertEqual((rogue["guid"], rogue["used"], rogue["ready"]), (UN_ROGUE, [(27000, "Cloak of Shadows")], []))
        self.assertEqual((warlock["guid"], warlock["used"], warlock["ready"]), (UN_WARL, [], ["Shadow Ward"]))
        self.assertIn("Warl died to a star explosion @ 34.0s with Shadow Ward ready and no personal button pressed",
                      self.lines)
        self.assertEqual(sum("no personal button pressed" in line for line in self.lines), 1)
        text = render(self.trace, "defensives")
        self.assertIn("deaths to a star explosion: 2", text)
        self.assertIn("0:28.000 Rogue (melee): personal buttons pressed Cloak of Shadows 0:27.000", text)
        self.assertIn("0:34.000 Warl (ranged): personal buttons pressed none; ready and unpressed: Shadow Ward", text)

    def test_a_button_cast_right_before_the_death_counts_as_pressed(self):
        with tempfile.TemporaryDirectory() as folder:
            trace = load(folder, under_pull() + [cast(32500, UN_WARL, 47891, UN_WARL)])
            warlock = al.explosion_deaths(trace)[1]
            self.assertEqual((warlock["used"], warlock["ready"]), ([(32500, "Shadow Ward")], []))

    def test_an_immunity_is_held_back_when_a_big_bang_is_due(self):
        extra = [cast(2000, UN_WARL, 31224, UN_WARL)]
        moved = [{**rec, "t": 40000} if rec.get("e") == "cast" and rec.get("sp") == al.SPELL_BIG_BANG[0] else rec
                 for rec in under_pull() + extra]
        with tempfile.TemporaryDirectory() as folder, mock.patch.dict(al.PERSONAL_COOLDOWN_MS,
                                                                       {"cloak of shadows": 1000}):
            free = al.explosion_deaths(load(folder, under_pull() + extra))[1]["ready"]
            held = al.explosion_deaths(load(folder, moved))[1]["ready"]
        self.assertEqual((free, held), (["Cloak of Shadows", "Shadow Ward"], ["Shadow Ward"]))

    def test_a_cutter_cast_on_a_holder_at_80_percent_or_more_is_listed(self):
        self.assertEqual([(note["t"], note["pct"], note["name"]) for note in al.window_cuts(self.trace)],
                         [(10000, 85.0, "Hand of Sacrifice")])
        self.assertIn("`window` cutters on a holder at 80% or more: 1", render(self.trace, "defensives"))

    def test_every_section_renders(self):
        self.assertTrue(render(self.trace))


class OlderStarsUnder(unittest.TestCase):
    """The same pull as a trace from before the under-star plan, the kite stages and the new notes."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, under_pull(drop=UN_NEW_KEYS))
        cls.lines = [line for _, line in al.failures(cls.trace)]

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_the_sections_still_read_what_the_records_say(self):
        text = render(self.trace, "stars", "constellations", "defensives", "bigbang")
        self.assertIn("Nitro Boosts: used by Bow 0:32.600; hid without: Holy, Mage", text)
        self.assertIn("deaths to a star explosion: 2", text)
        self.assertNotIn("kite runs", text)
        self.assertNotIn("algalon.trim:", text)

    def test_no_under_star_line_without_a_plan(self):
        self.assertFalse([line for line in self.lines if line.startswith("under-star")])
        self.assertIn("star died during Big Bang 1's cast @ 34.0s", self.lines)
        self.assertIn("Mage cast Ice Block while phased @ 44.2s and lost its phase aura", self.lines)

    def test_a_trace_without_any_nitro_says_so(self):
        bare = [rec for rec in under_pull(drop=UN_NEW_KEYS) if rec.get("sp") != al.SPELL_NITRO_BOOSTS]
        with tempfile.TemporaryDirectory() as folder:
            self.assertIn("Nitro Boosts: no algalon.nitro row or 54861 aura in this trace, 3 hider(s)",
                          render(load(folder, bare), "bigbang"))


CAT, ARCHER = 7301, 7302
METEOR_SPOT = (0, 30)


def charge_pull() -> list[dict]:
    """A cat dodges the meteor and walks to its slot, then Feral Charge lands it on the impact. A hunter
    dodges and the slot walk alone takes it back inside 15 yd."""
    def offset(guid: int, when: int) -> tuple[float, float]:
        if when < 20400:
            return (0, 18) if guid == CAT else (12, 30)
        if guid == CAT:
            return (0, 4) if when < 23000 else (0, 28)
        return (25, 30) if when < 22000 else (13, 30)

    x, y = spot(*METEOR_SPOT)
    return [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 603, "inst": 4, "diff": 0, "boss": "algalon-the-observer",
         "roster": [{"g": CAT, "n": "Cat", "r": "melee", "c": "druid", "h": 0},
                    {"g": ARCHER, "n": "Archer", "r": "ranged", "c": "hunter", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "algalon-the-observer", "src": "engage"},
        {"t": 1, "e": "unit", "g": ALGALON, "en": al.NPC_ALGALON, "n": "Algalon the Observer", "b": 1},
        {"t": 20000, "e": "haz", "sp": al.SPELL_COSMIC_SMASH[0], "shape": "circle", "x": x, "y": y, "z": 417.3,
         "ttl": 5500, "rad": 6.0},
        move(20400, CAT, al.SMASH_MOVER), move(21000, CAT, "algalon raid position action"),
        move(20400, ARCHER, al.SMASH_MOVER), move(22000, ARCHER, "algalon raid position action"),
        cast(22900, CAT, 49376, ALGALON),
        dmg(24800, ALGALON, CAT, al.SPELL_COSMIC_SMASH[0], 40000),
        dmg(24800, ALGALON, ARCHER, al.SPELL_COSMIC_SMASH[0], 5000),
        *({"t": when, "e": "snap", "u": [row(CAT, offset(CAT, when), 100.0, ALGALON),
                                         row(ARCHER, offset(ARCHER, when), 100.0, ALGALON),
                                         row(ALGALON, (0, 26), 90.0, 0)], "hz": []}
          for when in range(0, 26000, 200)),
        {"t": 26000, "e": "end", "out": "wipe"},
    ]


class SmashCharge(unittest.TestCase):
    """A charge after the dodge is what took the bot back, not the slot walk before it."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, charge_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_charge_that_closed_in_is_named_over_the_walk(self):
        (wave,) = al.smash_waves(self.trace)
        rows = {row["guid"]: row for row in al.wave_rows(self.trace, wave)}
        self.assertEqual((rows[CAT]["back"], rows[ARCHER]["back"]),
                         ("feral charge - cat", "algalon raid position action"))
        self.assertAlmostEqual(rows[CAT]["at_impact"], 2.0)
        self.assertAlmostEqual(rows[ARCHER]["at_impact"], 13.0)
        self.assertIn((24800, "Cosmic Smash @ 24.8s: dodged, then brought back inside 15 yd by the impact:"
                              " Cat 2.0 (feral charge - cat), Archer 13.0 (algalon raid position action)"),
                      al.smash_faults(self.trace))


UD_TRACKER, UD_LOCK, UD_MAGE = 8301, 8302, 8303
UD_STAR_A, UD_STAR_B, UD_STAR_C = (CREATURE | n for n in (501, 502, 503))
UD_END = 50000
UD_FOCUS = ((12000, UD_STAR_A), (16000, UD_STAR_B), (20000, UD_STAR_A), (24000, UD_STAR_B), (28000, 0))
# what each loses a second beyond the drain while it holds the focus
UD_EXTRA = {UD_STAR_A: 2.0, UD_STAR_B: 3.0}


def ud_star_hp(star: int, when: int) -> float:
    lost = when / 1000.0
    for (start, held), (stop, _) in zip(UD_FOCUS, UD_FOCUS[1:]):
        if held == star:
            lost += max(0, min(when, stop) - start) / 1000.0 * UD_EXTRA[star]
    return 100.0 - lost


def shoot(when: int, caster: int, star: int, fired: bool = False) -> dict:
    rec = cast(when, caster, al.SPELL_WAND_SHOOT, star)
    if fired:
        rec["tr"] = 1
    return rec


def underdrop_pull(drop: tuple[str, ...] = (), shots: bool = True) -> list[dict]:
    """Three stars planned under Big Bang 1 (cast 0:40). The focus goes A, B, A, B; A is released early 14 s
    before the cast, C late 4 s before it. The mage restarts its wand on C three times in 0.4 s, the
    warlock starts once on B after wanding B off the trim. `drop` leaves out the notes of those keys,
    `shots` False every Shoot cast."""
    roster = [
        {"g": UD_TRACKER, "n": "Tracker", "r": "tank", "c": "warrior", "h": 0},
        {"g": UD_LOCK, "n": "Lock", "r": "ranged", "c": "warlock", "h": 0},
        {"g": UD_MAGE, "n": "Mage", "r": "ranged", "c": "mage", "h": 0},
    ]
    a, b, c = (guid & 0xFFFFFFFF for guid in (UD_STAR_A, UD_STAR_B, UD_STAR_C))
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 603, "inst": 4, "diff": 0,
         "boss": "algalon-the-observer", "roster": roster},
        {"t": 0, "e": "pull", "boss": "algalon-the-observer", "src": "engage"},
        {"t": 1, "e": "unit", "g": ALGALON, "en": al.NPC_ALGALON, "n": "Algalon the Observer", "b": 1},
        *({"t": 1, "e": "unit", "g": guid, "en": al.NPC_COLLAPSING_STAR, "n": "Collapsing Star"}
          for guid in (UD_STAR_A, UD_STAR_B, UD_STAR_C)),
        note(10000, UD_TRACKER, "algalon.starplan", f"{a} 78% 30s u, {b} 80% 32s u, {c} 82% 34s u | under 34..39"),
        note(30000, UD_TRACKER, "algalon.starplan", f"{b} 50% 15s u, {c} 70% 20s | under 14..19"),
        note(12000, UD_TRACKER, "algalon.starwindow", "under"), note(28000, UD_TRACKER, "algalon.starwindow", "none"),
        *(note(when, UD_TRACKER, "algalon.focusstar", star) for when, star in UD_FOCUS),
        note(26000, UD_TRACKER, "algalon.underdrop", f"early {a} 60 14000"),
        note(36000, UD_TRACKER, "algalon.underdrop", f"late {c} 64 4000"),
        *(note(when, UD_MAGE, "algalon.trim", f"start {c} 70") for when in (30000, 30200, 30400)),
        note(35000, UD_MAGE, "algalon.trim", f"stop {c} 64"),
        note(30000, UD_LOCK, "algalon.trim", f"start {b} 46"), note(35000, UD_LOCK, "algalon.trim", f"stop {b} 41"),
        cast(40000, ALGALON, al.SPELL_BIG_BANG[0], cast_ms=8000),
        {"t": UD_END, "e": "end", "out": "wipe"},
    ]
    if shots:
        records += [shoot(18000, UD_LOCK, UD_STAR_B), shoot(19000, UD_LOCK, UD_STAR_B, True),
                    *(shoot(when, UD_MAGE, UD_STAR_C) for when in (30000, 30200, 30400)),
                    shoot(31900, UD_MAGE, UD_STAR_C, True), shoot(33400, UD_MAGE, UD_STAR_C, True),
                    shoot(30000, UD_LOCK, UD_STAR_B),
                    *(shoot(when, UD_LOCK, UD_STAR_B, True) for when in (31500, 33000, 34500))]
    for when in range(0, UD_END, 500):
        units = [row(guid, (0, 12), 100.0, ALGALON) for guid in (UD_TRACKER, UD_LOCK, UD_MAGE)]
        units.append(row(ALGALON, (0, 4), 90.0, UD_TRACKER))
        units += [row(star, (index * 10, 30), ud_star_hp(star, when), 0)
                  for index, star in enumerate((UD_STAR_A, UD_STAR_B, UD_STAR_C))]
        records.append({"t": when, "e": "snap", "u": units, "hz": []})
    return [rec for rec in records if rec.get("e") != "note" or rec.get("k") not in drop]


class UnderBulk(unittest.TestCase):
    """How the team worked the under-stars down: focus switches, releases and the wands."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, underdrop_pull())
        cls.lines = [line for _, line in al.failures(cls.trace)]

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_an_underdrop_note_reads_the_reason_the_star_its_health_and_the_time_to_the_cast(self):
        self.assertEqual(al.parse_underdrop("late 1478 45 6200"), ("late", 1478, 45.0, 6200))
        self.assertEqual(al.parse_underdrop("early 1478"), ("early", 1478, None, None))

    def test_each_focus_span_reads_the_loss_beyond_the_drain(self):
        self.assertEqual([(row["star"], row["start"], row["under"], row["first"], row["last"], row["beyond"])
                          for row in al.focus_rates(self.trace)],
                         [(UD_STAR_A, 12000, True, 88.0, 76.0, 2.0), (UD_STAR_B, 16000, True, 84.0, 68.0, 3.0),
                          (UD_STAR_A, 20000, True, 72.0, 60.0, 2.0), (UD_STAR_B, 24000, True, 64.0, 48.0, 3.0)])

    def test_a_release_inside_8_s_of_the_cast_is_a_failure(self):
        self.assertIn("under-star star 3 (503) released late 4.0 s before Big Bang 1's cast, at 64% @ 36.0s",
                      self.lines)
        self.assertEqual(sum(" released " in line for line in self.lines), 1)

    def test_the_focus_switching_more_than_once_per_under_star_is_a_failure(self):
        (cycle,) = al.under_cycles(self.trace)
        self.assertEqual((cycle["n"], cycle["stars"], cycle["visited"], cycle["switches"], cycle["first_switch"]),
                         (1, [501, 502, 503], 2, 3, 16000))
        self.assertIn("Big Bang 1: the focus switched 3 times between 2 under-stars, from 16.0s", self.lines)

    def test_a_trimmer_restarting_shoot_is_a_failure(self):
        self.assertEqual(al.trim_shots(self.trace), {
            503: [{"bot": UD_MAGE, "starts": [30000, 30200, 30400], "fired": 2}],
            502: [{"bot": UD_LOCK, "starts": [30000], "fired": 3}]})
        self.assertIn("Mage started Shoot 3 times within 2 s on star 3 (503) @ 30.0s (3 starts, 2 shot(s) fired)",
                      self.lines)
        self.assertEqual(sum("started Shoot" in line for line in self.lines), 1)

    def test_the_stars_section_shows_the_spans_the_releases_and_the_trimmers(self):
        text = render(self.trace, "stars")
        self.assertIn("    star 3 (503): Mage 3 start(s), 2 shot(s) fired\n", text)
        self.assertIn("    Big Bang 1: 3 under-star(s), the focus on 2 of them switched 3 time(s) between them\n"
                      "      star 1 (501): released early 0:26.000 at 60%, 14.0 s before the cast\n"
                      "      star 2 (502): not released\n"
                      "      star 3 (503): released late 0:36.000 at 64%, 4.0 s before the cast\n", text)
        self.assertIn(" 0:12.000 ..  0:16.000  under     star 1 (501)     u   88.0% >  76.0% in  4.0 s, +2.0 %/s",
                      text)

    def test_an_older_trace_reads_without_the_new_rows(self):
        with tempfile.TemporaryDirectory() as folder:
            trace = load(folder, underdrop_pull(drop=("algalon.underdrop",), shots=False))
            text = render(trace, "stars")
            self.assertIn("no algalon.underdrop row: nothing released, or a trace from before the probe", text)
            self.assertIn("star 3 (503): Mage 0 start(s), 0 shot(s) fired", text)
            self.assertEqual([line for _, line in al.under_faults(trace)],
                             ["Big Bang 1: the focus switched 3 times between 2 under-stars, from 16.0s"])
        with tempfile.TemporaryDirectory() as folder:
            bare = load(folder, underdrop_pull(drop=("algalon.underdrop", "algalon.starplan", "algalon.focusstar",
                                                     "algalon.trim"), shots=False))
            self.assertEqual(al.under_faults(bare), [])
            self.assertNotIn("under-stars per Big Bang", render(bare, "stars"))


PX_TANK, PX_PARTNER, PX_HOLY, PX_RET, PX_LOCK, PX_ROGUE, PX_TREE, PX_MAGE, PX_FROST = range(8401, 8410)
PX_STAR_A, PX_STAR_B, PX_HOLE_A, PX_HOLE_B = (CREATURE | n for n in (601, 602, 621, 622))
PX_END = 40000
PX_HOME = {PX_TANK: (0, 0), PX_PARTNER: (3, 0), PX_HOLY: (0, 12), PX_RET: (2, 3), PX_LOCK: (8, 15),
           PX_ROGUE: (-2, 3), PX_TREE: (-6, 12), PX_MAGE: (-8, 18), PX_FROST: (4, 4)}
PX_STAR_SPOTS = {PX_STAR_A: (-20, 30), PX_STAR_B: (20, 30)}
PX_SPELLS = {64122: "Black Hole Explosion", 498: "Divine Protection", 25771: "Forbearance", 22812: "Barkskin",
             48792: "Icebound Fortitude", 31224: "Cloak of Shadows", 47877: "Master Healthstone",
             64205: "Divine Sacrifice", 70940: "Divine Guardian", 47891: "Shadow Ward", 45438: "Ice Block"}


def px_hp(guid: int, when: int) -> float:
    if guid == PX_LOCK:
        return 60.0 if when < 20000 else 0.0
    if guid == PX_MAGE:
        return 80.0 if when < 20000 else 20.0
    if guid == PX_TREE:
        return 90.0 if when < 15000 else 75.0
    if guid == PX_FROST:
        return 30.0 if when < 6000 else 70.0
    return 80.0 if guid == PX_HOLY else 100.0


def presses_pull(drop: tuple[str, ...] = ()) -> list[dict]:
    """Two stars explode 0.6 s apart, each from the Black Hole it leaves, the second star gone from the scans
    first. Before the first: the ret's cover fails with the holy paladin's Divine Sacrifice ready, the mage
    uses a healthstone, the druid spends Barkskin in a hole at 90% and the death knight Icebound Fortitude in
    one at 30%. The warlock and the death knight wear Divine Guardian; the warlock dies with Shadow Ward never
    pressed. After the second, the holy paladin covers and the rogue presses Cloak of Shadows, both too late;
    at 30 s the holy paladin's own cover fails with nobody else ready. `drop` leaves out the notes of those
    keys."""
    roster = [
        {"g": PX_TANK, "n": "Tanka", "r": "tank", "c": 1, "h": 0},
        {"g": PX_PARTNER, "n": "Tankb", "r": "tank", "c": 2, "h": 0},
        {"g": PX_HOLY, "n": "Holy", "r": "heal", "c": 2, "h": 0},
        {"g": PX_RET, "n": "Ret", "r": "melee", "c": 2, "h": 0},
        {"g": PX_LOCK, "n": "Lock", "r": "ranged", "c": 9, "h": 0},
        {"g": PX_ROGUE, "n": "Rogue", "r": "melee", "c": 4, "h": 0},
        {"g": PX_TREE, "n": "Tree", "r": "heal", "c": 11, "h": 0},
        {"g": PX_MAGE, "n": "Mage", "r": "ranged", "c": 8, "h": 0},
        {"g": PX_FROST, "n": "Frost", "r": "melee", "c": 6, "h": 0},
    ]
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 603, "inst": 4, "diff": 0,
         "boss": "algalon-the-observer", "roster": roster},
        {"t": 0, "e": "pull", "boss": "algalon-the-observer", "src": "engage"},
        {"t": 1, "e": "unit", "g": ALGALON, "en": al.NPC_ALGALON, "n": "Algalon the Observer", "b": 1},
        *({"t": 1, "e": "unit", "g": guid, "en": al.NPC_COLLAPSING_STAR, "n": "Collapsing Star"}
          for guid in (PX_STAR_A, PX_STAR_B)),
        *({"t": 1, "e": "unit", "g": guid, "en": al.NPC_BLACK_HOLE, "n": "Black Hole"}
          for guid in (PX_HOLE_A, PX_HOLE_B)),
        *({"t": 1, "e": "unit", "g": member["g"], "n": member["n"], "mhp": 20000} for member in roster),
        *({"t": 1, "e": "spell", "sp": spell, "n": name} for spell, name in PX_SPELLS.items()),
        note(1000, PX_TANK, "algalon.swappair", 1), note(1000, PX_PARTNER, "algalon.swappair", 1),

        cast(5000, PX_HOLY, 498), buff(5000, PX_HOLY, al.SPELL_FORBEARANCE, PX_HOLY),
        *phased(PX_FROST, al.SPELL_BLACK_HOLE_PHASE, 2000, 6000), cast(3000, PX_FROST, 48792),
        *phased(PX_TREE, al.SPELL_BLACK_HOLE_PHASE, 10000, 14000), cast(12000, PX_TREE, 22812),
        *worn(PX_FROST, al.SPELL_DIVINE_GUARDIAN, PX_HOLY, 15000, 21000),
        *worn(PX_LOCK, al.SPELL_DIVINE_GUARDIAN, PX_HOLY, 15000, 20000),
        note(18000, PX_MAGE, "algalon.personal", "healthstone"), cast(18000, PX_MAGE, 47877),
        {"t": 19500, "e": "act", "g": PX_RET, "a": al.COVER_NODE, "rel": 91.0, "vd": "FAILED"},

        blast(20000, PX_HOLE_A, PX_LOCK, 15000, 3000), blast(20000, PX_HOLE_A, PX_MAGE, 12000, 3000),
        blast(20000, PX_HOLE_A, PX_HOLY, 10000, 3000), blast(20000, PX_HOLE_A, PX_TREE, 11000, 3000),
        blast(20000, PX_HOLE_A, PX_ROGUE, 9000, 3000),
        death(20000, PX_LOCK, PX_HOLE_A, 15000, PX_HOME[PX_LOCK]),
        blast(20600, PX_HOLE_B, PX_MAGE, 3000, 1000), blast(20600, PX_HOLE_B, PX_ROGUE, 9000, 1000),

        cast(20640, PX_HOLY, al.SPELL_DIVINE_SACRIFICE), note(20640, PX_HOLY, "algalon.cover", "divine sacrifice"),
        *(rec for guid in (PX_HOLY, PX_MAGE, PX_TREE) for rec in worn(guid, al.SPELL_DIVINE_GUARDIAN, PX_HOLY, 20640,
                                                                      26640)),
        note(20700, PX_ROGUE, "algalon.personal", "Cloak of Shadows"), cast(20700, PX_ROGUE, 31224),
        {"t": 30000, "e": "act", "g": PX_HOLY, "a": al.COVER_NODE, "rel": 91.0, "vd": "FAILED"},
        {"t": PX_END, "e": "end", "out": "wipe"},
    ]
    for when in range(0, PX_END, 500):
        units = [row(guid, offset, px_hp(guid, when), ALGALON) for guid, offset in PX_HOME.items()]
        units.append(row(ALGALON, (0, 4), 90.0, PX_TANK))
        for star, gone in ((PX_STAR_A, 20000), (PX_STAR_B, 19500)):
            if when <= gone:
                units.append(row(star, PX_STAR_SPOTS[star], 5.0, 0))
        for hole, star, born in ((PX_HOLE_A, PX_STAR_A, 20500), (PX_HOLE_B, PX_STAR_B, 21000)):
            if when >= born:
                units.append(row(hole, PX_STAR_SPOTS[star], 100.0, 0))
        records.append({"t": when, "e": "snap", "u": units, "hz": []})
    return [rec for rec in records if rec.get("e") != "note" or rec.get("k") not in drop]


class ExplosionPresses(unittest.TestCase):
    """Who stood under each explosion's line, what they pressed, and presses that came too late or in a hole."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, presses_pull())
        cls.lines = [line for _, line in al.failures(cls.trace)]

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_explosions_from_black_holes_stay_apart_and_name_the_star_on_their_spot(self):
        self.assertEqual([(row["t"], row["star"], row["source"], row["hits"]) for row in al.star_deaths(self.trace)],
                         [(20000, PX_STAR_A, PX_HOLE_A, 5), (20600, PX_STAR_B, PX_HOLE_B, 2)])

    def test_each_bot_under_the_line_reads_its_health_hit_presses_and_buttons(self):
        first, second = al.explosion_rows(self.trace)
        # Divine Guardian lowers the warlock's line and lifts the death knight out of it.
        self.assertEqual([(entry["guid"], entry["hp"], entry["line"], entry["landed"], entry["died"],
                           entry["pressed"], entry["consumables"], entry["buttons"]) for entry in first["under"]], [
            (PX_HOLY, 16000, 16913, 10000, False, [], [],
             [("Divine Protection", "cooling", 5000), ("Divine Shield", "forbearance", 5000)]),
            (PX_LOCK, 12000, 13530, 15000, True, [], [], [("Shadow Ward", "ready", None)]),
            (PX_MAGE, 16000, 16913, 12000, False, [], [(18000, "healthstone")], [("Ice Block", "ready", None)]),
            (PX_TREE, 15000, 16913, 11000, False, [], [], [("Barkskin", "cooling", 12000)])])
        self.assertEqual([(entry["guid"], entry["landed"]) for entry in second["under"]],
                         [(PX_HOLY, None), (PX_MAGE, 3000), (PX_TREE, None)])

    def test_every_press_and_cover_reads_its_star_death(self):
        self.assertEqual([(row["t"], row["guid"], row["kind"], al.press_name(row), row["died"])
                          for row in al.press_rows(self.trace)], [
            (18000, PX_MAGE, "personal", "healthstone", 20000),
            (20640, PX_HOLY, "cover", "Divine Sacrifice", 20600),
            (20700, PX_ROGUE, "personal", "Cloak of Shadows", 20600)])

    def test_a_press_or_cover_after_its_star_died_is_a_failure(self):
        self.assertIn("Holy cast Divine Sacrifice for cover @ 20.6s, 0.04 s after its star died", self.lines)
        self.assertIn("Rogue pressed Cloak of Shadows @ 20.7s, 0.10 s after its star died", self.lines)
        self.assertEqual(sum("after its star died" in line for line in self.lines), 2)

    def test_a_held_button_cast_in_a_hole_above_the_line_is_a_failure(self):
        self.assertEqual([(row["t"], row["guid"], row["label"], row["pct"]) for row in al.phased_buttons(self.trace)],
                         [(12000, PX_TREE, "Barkskin", 90.0)])
        self.assertIn("Tree spent Barkskin @ 12.0s inside a hole's phase, at 90%", self.lines)
        self.assertEqual(sum("inside a hole's phase" in line for line in self.lines), 1)

    def test_a_failed_cover_with_another_paladin_ready_is_a_failure(self):
        self.assertEqual([(row["t"], row["guid"], row["ready"]) for row in al.failed_covers(self.trace)],
                         [(19500, PX_RET, [(PX_HOLY, al.SPELL_DIVINE_SACRIFICE)]), (30000, PX_HOLY, [])])
        self.assertIn("Ret's cover failed @ 19.5s with Holy's Divine Sacrifice ready", self.lines)
        self.assertEqual(sum("cover failed" in line for line in self.lines), 1)

    def test_an_explosion_death_counts_its_class_buttons_as_ready(self):
        (lock,) = al.explosion_deaths(self.trace)
        self.assertEqual((lock["guid"], lock["used"], lock["ready"]), (PX_LOCK, [], ["Shadow Ward"]))
        self.assertIn("Lock died to a star explosion @ 20.0s with Shadow Ward ready and no personal button pressed",
                      self.lines)

    def test_the_section_lists_the_line_the_presses_the_failed_covers_and_the_phased_buttons(self):
        text = render(self.trace, "defensives")
        for line in ("star explosions: 2; under each, every unphased bot at or under its line (16,913, 13,530 covered)",
                     "      Holy (heal) 16,000 of 16,913, hit 10,000; pressed none; Divine Protection cooling since"
                     " 0:05.000, Divine Shield forbearance since 0:05.000\n",
                     "      Lock (ranged) 12,000 of 13,530, hit 15,000, died; pressed none; Shadow Ward ready\n",
                     "      Mage (ranged) 16,000 of 16,913, hit 12,000; pressed none; Ice Block ready;"
                     " used healthstone 0:18.000\n",
                     "      Tree (heal) 15,000 of 16,913, hit 11,000; pressed none; Barkskin cooling since 0:12.000\n",
                     "      Tree (heal) 15,000 of 16,913, not hit; pressed none; Barkskin cooling since 0:12.000\n",
                     "personal presses (algalon.personal): 2\n     0:18.000 Mage healthstone: its star died 0:20.000"
                     " (+2.00 s)\n     0:20.700 Rogue Cloak of Shadows: its star died 0:20.600 (-0.10 s), already dead",
                     "explosion covers (algalon.cover): 1\n     0:20.640 Holy Divine Sacrifice: its star died 0:20.600"
                     " (-0.04 s), already dead",
                     "cover node runs that FAILED: 2\n     0:19.500 Ret; other covers ready: Holy's Divine Sacrifice\n"
                     "     0:30.000 Holy; other covers ready: none",
                     "personal buttons cast in a hole's phase above 35%: 1\n     0:12.000 Tree Barkskin at 90%"):
            self.assertIn(line, text)


class OlderPresses(unittest.TestCase):
    """The same pull from before `algalon.personal` and `algalon.cover`: the records still carry the rest."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, presses_pull(drop=("algalon.personal", "algalon.cover")))
        cls.lines = [line for _, line in al.failures(cls.trace)]

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_the_casts_and_act_rows_still_read(self):
        self.assertEqual(al.press_rows(self.trace), [])
        self.assertFalse([line for line in self.lines if "after its star died" in line])
        self.assertIn("Tree spent Barkskin @ 12.0s inside a hole's phase, at 90%", self.lines)
        self.assertIn("Ret's cover failed @ 19.5s with Holy's Divine Sacrifice ready", self.lines)
        mage = next(entry for entry in al.explosion_rows(self.trace)[0]["under"] if entry["guid"] == PX_MAGE)
        self.assertEqual(mage["consumables"], [(18000, "Master Healthstone")])
        text = render(self.trace, "defensives")
        self.assertIn("personal presses (algalon.personal): 0\n  explosion covers (algalon.cover): 0\n", text)


KB_HELD = CREATURE | 81
KB_STUCK = CREATURE | 82
KB_LOOSE = CREATURE | 83
KB_END = 30000


def kb_snap(when: int) -> dict:
    units = [row(AEGIS, (3, 0), 100.0, ALGALON), row(BULWARK, (0, 0), 100.0, ALGALON),
             row(MERCY, (0, 14), 100.0, 0), row(ALGALON, (0, 4), 90.0, BULWARK),
             row(KB_HELD, (6, 6), 100.0, AEGIS)]
    # Woken on Mercy, then home and evaded: never within 5 yd of the floor.
    stuck = row(KB_STUCK, (40, 0), 100.0, MERCY if 1000 <= when < 1500 else 0)
    stuck[3] = 433.0
    units.append(stuck)
    if 6000 <= when < 12000:
        units.append(row(KB_LOOSE, (2, 14), 100.0, MERCY))
    return {"t": when, "e": "snap", "u": units, "hz": []}


def kite_blocked_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 603, "inst": 4, "diff": 1,
         "boss": "algalon-the-observer", "roster": ROSTER},
        {"t": 0, "e": "pull", "boss": "algalon-the-observer", "src": "engage"},
        {"t": 1, "e": "unit", "g": ALGALON, "en": al.NPC_ALGALON, "n": "Algalon the Observer", "b": 1},
        *({"t": 1, "e": "unit", "g": guid, "en": al.NPC_LIVING_CONSTELLATION, "n": "Living Constellation"}
          for guid in (KB_HELD, KB_STUCK, KB_LOOSE)),
        note(0, MERCY, "algalon.phase", 2), note(1000, AEGIS, "algalon.handler", AEGIS),
        note(1000, MERCY, "algalon.holes", 2),
        # Bulwark's own note runs on, but it never was the handler.
        note(1000, BULWARK, "algalon.kite", "taunt"),
        note(2000, AEGIS, "algalon.kite", "taunt"), note(12000, AEGIS, "algalon.kite", "none"),
        cast(5500, AEGIS, 62124, KB_LOOSE),
        {"t": KB_END, "e": "end", "out": "reset"},
    ]
    return records + [kb_snap(when) for when in range(0, KB_END, 500)]


class KiteBlocked(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, kite_blocked_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_only_the_elected_handler_counts_toward_the_kite_branches(self):
        self.assertEqual([(value, start, stop, guid) for value, start, stop, guid in al.handler_kite_spans(self.trace)],
                         [("taunt", 2000, 12000, AEGIS), ("none", 12000, KB_END, AEGIS)])
        self.assertIn("algalon.kite, the handler's own: none 18.0 s, taunt 10.0 s",
                      render(self.trace, "constellations"))

    def test_a_constellation_that_never_came_down_is_flagged(self):
        self.assertEqual(list(al.stuck_constellations(self.trace)), [KB_STUCK])
        self.assertIn("stuck in the air, never below +15.7 yd", render(self.trace, "constellations"))

    def test_taunting_with_one_held_and_a_hole_spare_is_a_blocked_kite(self):
        self.assertEqual(al.kite_blocked(self.trace), {"stuck": 4000, "cooling": 6000})
        self.assertIn("kite blocked 10.0 s: the handler taunted with one held and a hole spare (none loose but one in"
                      " the air 4.0 s, taunt cooling 6.0 s)", [line for _, line in al.failures(self.trace)])

    def test_a_stale_taunt_with_nothing_in_the_air_is_not_blocked_by_a_stuck_one(self):
        landed = []
        for rec in kite_blocked_pull():
            if rec.get("e") == "snap":
                rec = {**rec, "u": [[*unit[:3], 417.3, *unit[4:]] if unit[0] == KB_STUCK else unit
                                    for unit in rec["u"]]}
            landed.append(rec)
        with tempfile.TemporaryDirectory() as folder:
            trace = load(folder, landed)
            self.assertEqual(al.kite_blocked(trace)["stuck"], 0)
            self.assertEqual(al.stuck_constellations(trace), {})

    def test_one_removed_before_it_could_come_down_is_not_stuck(self):
        cut = [rec for rec in kite_blocked_pull() if rec.get("e") != "snap" or rec["t"] < 10000]
        cut += [{**kb_snap(when), "u": [unit for unit in kb_snap(when)["u"] if unit[0] != KB_STUCK]}
                for when in range(10000, KB_END, 500)]
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(al.stuck_constellations(load(folder, cut)), {})


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
