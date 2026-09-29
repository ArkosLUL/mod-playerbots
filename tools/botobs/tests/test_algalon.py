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
]

# Offsets in yards from ULDUAR_ALGALON_TANK_SLOT.
HOME = {BULWARK: (0, 0), AEGIS: (3, 0), MERCY: (0, 14), TREE: (8, 12), AGONY: (-10, 18), SHIV: (2, 3),
        TRUESHOT: (10, 18)}


def spot(dx: float, dy: float) -> tuple[float, float]:
    return al.TANK_SLOT[0] + dx, al.TANK_SLOT[1] + dy


def row(guid: int, offset, hp: float, target: int) -> list:
    x, y = spot(*offset)
    return [guid, x, y, 417.3, 0.0, hp, 80.0, target, 0, 0, 0, 0, 0, 0.0]


def player_hp(guid: int, when: int) -> float:
    if guid == TRUESHOT and when >= 48000:
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


def boss_target(when: int) -> int:
    if when < 60000:
        return BULWARK
    if when < 75000:
        return AEGIS
    return SHIV if when < 77000 else BULWARK


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
        records += probe_notes(handler)
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

    def test_both_marker_entries_on_one_spot_are_one_impact(self):
        markers, stray = al.smash_rows(self.trace)
        self.assertEqual(stray, [])
        (marker,) = markers
        self.assertEqual((marker["seen"], marker["impact"]), (20000, 24800))
        self.assertAlmostEqual(marker["near"][TRUESHOT][0], 12.0)
        self.assertAlmostEqual(marker["near"][TRUESHOT][1], 0.0)
        ((hit, distance),) = marker["hits"]
        self.assertEqual(hit["d"], TREE)
        self.assertEqual(al.band(distance), "6-10")

    def test_tanks_read_the_punch_the_swap_and_the_pickup(self):
        self.assertEqual(al.punched_out(self.trace), [(89000, BULWARK)])
        self.assertEqual(al.boss_victims(self.trace), [(0, BULWARK), (60000, AEGIS), (75000, SHIV), (77000, BULWARK)])
        self.assertEqual(al.pickups(self.trace), [(75000, 77000, SHIV)])
        self.assertEqual(max(count for _, count, _ in al.phase_punch(self.trace)[BULWARK]), 5)

    def test_formation_measures_each_ring_bot_from_its_slot(self):
        rows = {row["guid"]: row for row in al.ring_rows(self.trace)}
        self.assertEqual(set(rows), {MERCY, TREE, AGONY, TRUESHOT})
        self.assertEqual(rows[MERCY]["slot"], 0)
        self.assertAlmostEqual(rows[MERCY]["to_tank"], 14.0)
        if al.RINGS:
            self.assertAlmostEqual(rows[MERCY]["to_slot"], al.dist2(spot(0, 14), al.slot_position(0)))

    def test_dark_matter_stays_loose_across_a_scan_it_missed(self):
        (row,) = al.dark_matter_rows(self.trace)
        self.assertEqual((row["spawn"], dict(row["loose"]), row["loose_from"]), (85000, {AGONY: 5000}, 85000))
        self.assertEqual((row["on_handler"], row["elsewhere"]), (4000, 5000))

    def test_the_room_despawning_after_the_ascend_is_not_a_kill(self):
        self.assertEqual(al.final_despawn(self.trace), 99500)
        (row,) = al.dark_matter_rows(self.trace)
        self.assertEqual((row["killed"], row["despawned"]), (None, 99500))

    def test_the_verdict_lists_every_failure_in_time_order(self):
        self.assertEqual([line for _, line in al.failures(self.trace)], [
            "Shiv on stars for 4.0 s outside the star team, from 30.0s",
            "stars died 3.0 s apart @ 33.0s",
            "Big Bang 1 @ 40.0s: 1 non-soaker hit unphased (Trueshot)",
            "re-phase loop: Shiv @ 44.0s (2 applications, 16.5 s phased)",
            "re-phase loop: Tree @ 65.0s (1 application, 11.0 s phased)",
            "Algalon on Shiv (melee) for 2.0 s @ 75.0s",
            "Unleashed Dark Matter loose for 5.0 s (mostly on Agony) @ 85.0s",
            "Phase Punch phased Bulwark out @ 89.0s",
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
        self.assertEqual(text.count(" marker, impact "), 1)
        self.assertNotIn("no failures detected", text)


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
        self.assertEqual(lines[0], "missing probes: algalon.bigbang, algalon.handler, algalon.hide, algalon.holes,"
                                   " algalon.phase, algalon.slot, algalon.soaker, algalon.spot, algalon.starwindow")
        self.assertIn("Big Bang 1 @ 40.0s: 2 non-soakers hit unphased (Aegis, Trueshot)", lines)
        self.assertIn("evade 4.1 s after Big Bang 2 (Ascend at +0.3 s, pull ended reset)", lines)

    def test_every_section_says_what_it_is_missing(self):
        text = render(self.trace)
        self.assertIn("absent from this trace: algalon.phase; P1 from his first cast", text)
        self.assertIn("absent from this trace: algalon.bigbang, algalon.soaker, algalon.backup, algalon.hide,"
                      " algalon.defensive", text)
        self.assertIn("absent from this trace: algalon.slot, algalon.spot", text)
        self.assertIn("no star team recorded", text)


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
