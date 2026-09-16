"""Which encounter a trace, a file name, a probe key or a strategy node belongs to.

The recorder names a pull after whichever creature engaged, and every join in the readers
(selection, the hard-mode toggle, coverage gating, silent probes) runs on the encounter
instead. One place decides it, so the joins cannot disagree.
"""
from __future__ import annotations

import json
import pathlib
import re

from .trace import Trace


def boss_from_path(path: pathlib.Path) -> str:
    """The boss slug out of the filename, which the recorder writes as
    <map>_<instance>_<boss-slug>_<epoch>.ndjson and renames when it learns a better name. Joining the
    middle back together rather than taking one field, because a slug may hold the separator."""
    parts = path.stem.split("_")
    return "_".join(parts[2:-1]) if len(parts) >= 4 else ""


# One encounter, one name. The recorder names a pull after whichever creature engaged, so a fight with
# several bosses in it lands under two or three slugs and splits its own sample: the Iron Assembly's
# 30 pulls read as 16 Brundir and 14 Molgeim, and Freya's 11 three-elder pulls read as Stonebark.
# `hdr.cfg.hardmode` is keyed by the conf's names, which are these, so an unmapped slug also means the
# hard-mode disqualifier silently never applies. Only slugs that differ need an entry.
BOSS_ALIASES = {
    "assembly-of-iron": "iron-assembly",
    "runemaster-molgeim": "iron-assembly",
    "steelbreaker": "iron-assembly",
    "stormcaller-brundir": "iron-assembly",
    "elder-brightleaf": "freya",
    "elder-ironbranch": "freya",
    "elder-stonebark": "freya",
    "xt002": "xt-002",
    "xt-002-deconstructor": "xt-002",
    "general-vezax": "vezax",
    "yogg-saron-": "yogg-saron",
    "sara": "yogg-saron",
}


def canonical_boss(slug: str) -> str:
    return BOSS_ALIASES.get(slug, slug)


def slugify(name: str) -> str:
    """A creature name as the recorder would have filed it."""
    return re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")


# Every instanced map's Map.dbc name, slugged the way the recorder slugs it. A session that opens
# before it can see a boss files itself under this name, and the recorder only ever renames that one.
MAP_SLUGS = {
    33: "shadowfang-keep", 34: "stormwind-stockade", 36: "deadmines", 43: "wailing-caverns",
    47: "razorfen-kraul", 48: "blackfathom-deeps", 70: "uldaman", 90: "gnomeregan",
    109: "sunken-temple", 129: "razorfen-downs", 169: "emerald-dream", 189: "scarlet-monastery",
    209: "zul-farrak", 229: "blackrock-spire", 230: "blackrock-depths", 249: "onyxia-s-lair",
    269: "opening-of-the-dark-portal", 289: "scholomance", 309: "zul-gurub", 329: "stratholme",
    349: "maraudon", 389: "ragefire-chasm", 409: "molten-core", 429: "dire-maul",
    469: "blackwing-lair", 509: "ruins-of-ahn-qiraj", 531: "ahn-qiraj-temple", 532: "karazhan",
    533: "naxxramas", 534: "the-battle-for-mount-hyjal",
    540: "hellfire-citadel-the-shattered-halls", 542: "hellfire-citadel-the-blood-furnace",
    543: "hellfire-citadel-ramparts", 544: "magtheridon-s-lair", 545: "coilfang-the-steamvault",
    546: "coilfang-the-underbog", 547: "coilfang-the-slave-pens",
    548: "coilfang-serpentshrine-cavern", 550: "tempest-keep", 552: "tempest-keep-the-arcatraz",
    553: "tempest-keep-the-botanica", 554: "tempest-keep-the-mechanar",
    555: "auchindoun-shadow-labyrinth", 556: "auchindoun-sethekk-halls",
    557: "auchindoun-mana-tombs", 558: "auchindoun-auchenai-crypts",
    560: "the-escape-from-durnholde", 564: "black-temple", 565: "gruul-s-lair", 568: "zul-aman",
    574: "utgarde-keep", 575: "utgarde-pinnacle", 576: "the-nexus", 578: "the-oculus",
    580: "the-sunwell", 585: "magister-s-terrace", 595: "the-culling-of-stratholme",
    599: "halls-of-stone", 600: "drak-tharon-keep", 601: "azjol-nerub", 602: "halls-of-lightning",
    603: "ulduar", 604: "gundrak", 608: "violet-hold", 615: "the-obsidian-sanctum",
    616: "the-eye-of-eternity", 619: "ahn-kahet-the-old-kingdom", 624: "vault-of-archavon",
    631: "icecrown-citadel", 632: "the-forge-of-souls", 649: "trial-of-the-crusader",
    650: "trial-of-the-champion", 658: "pit-of-saron", 668: "halls-of-reflection",
    724: "the-ruby-sanctum",
}


def map_of(path: pathlib.Path) -> int | None:
    """The map id the recorder puts first in the file name."""
    head = path.stem.split("_", 1)[0]
    return int(head) if head.isdigit() else None


def filed_under_map(map_id: int | None, slug: str) -> bool:
    """Whether a boss slug is only the map's name. That is the one case where the name says nothing
    about the encounter: any other slug came from a creature the recorder saw engage."""
    return bool(slug) and MAP_SLUGS.get(map_id) == slug


def recover_boss(path: pathlib.Path) -> str:
    """The encounter a trace belongs to, read out of its own units rather than its name.

    A session that opens before its boss engages is filed under the map, and where nothing renames it
    afterwards that name sticks: two full Ulduar pulls on disk are `ulduar`, unreachable by --boss,
    and every strategy node folds into "gate shut this pull" because the coverage join runs on the
    same name. Several creatures can carry the boss flag - Yogg's room has the four Keepers standing
    in it - so the one that traded damage is the encounter, and the flag alone is not enough.
    """
    flagged: dict[int, str] = {}
    try:
        with path.open(encoding="utf-8", errors="replace") as handle:
            for line in handle:
                if '"e":"unit"' in line:
                    rec = json.loads(line)
                    if rec.get("b") and rec.get("n"):
                        flagged[rec.get("g")] = rec["n"]
                elif flagged and '"e":"dmg"' in line:
                    rec = json.loads(line)
                    for guid in (rec.get("s"), rec.get("d")):
                        if guid in flagged:
                            return canonical_boss(slugify(flagged[guid]))
    except (OSError, ValueError):
        return ""
    return ""


def boss_key(path: pathlib.Path) -> str:
    """The encounter a file belongs to, from its name alone. Selection uses this rather than opening
    each trace, which is what keeps a boss sweep off the other 120 files."""
    return canonical_boss(boss_from_path(path))


def boss_of(trace: Trace) -> str:
    """The corrected boss slug. hdr.boss is line one of an append-only file, so a session that opened
    before its boss engaged still carries the map name there; the rename record is authoritative."""
    renames = [p for p in trace.of("pull") if p.get("src") == "rename"]
    if renames:
        return str(renames[-1].get("boss") or "")
    return str(trace.header.get("boss") or "")


def engaged_of(trace: Trace) -> str:
    """The encounter read out of the loaded records: the boss-flagged unit that traded damage.

    The same recovery `recover_boss` does off disk, but free here because the trace is
    already in memory. Several creatures can carry the flag, so the damage is what picks one.
    """
    flagged = {guid for guid in trace.bosses}
    if not flagged:
        return ""
    for rec in trace.of("dmg"):
        for guid in (rec.get("s"), rec.get("d")):
            if guid in flagged and trace.names.get(guid):
                return canonical_boss(slugify(trace.names[guid]))
    return ""


def encounter_of(trace: Trace) -> str:
    """The fight this trace belongs to, which is what a census counts and what the conf keys on. The
    boss slug says which creature engaged, and for a council or an elder pull that is not the same.

    A slug that is still the map's name joins to no encounter, so the units decide there. Nowhere
    else: a pull filed under its encounter never gets a rename, and the first boss-flagged unit to
    trade damage is often an add or a vehicle rather than the boss the encounter is named after.
    """
    filed = boss_of(trace) or boss_from_path(trace.path)
    if filed_under_map(trace.header.get("map", map_of(trace.path)), filed):
        return engaged_of(trace) or canonical_boss(filed)
    return canonical_boss(filed)


# Ulduar keys every trigger to the boss in the room, and a shut gate returns the same empty Event as a
# condition that was false - so without this every other encounter's nodes read as NEVER on every pull.
# Mirrors ENCOUNTER_PREFIXES in src/Ai/Raid/Uld/UldEncounterGate.cpp; change both together. `sara` is
# Yogg-Saron's phase-one form and the one name that does not lead with its encounter.
ULD_PREFIXES = {
    "flame leviathan": "flame-leviathan", "ignis": "ignis", "razorscale": "razorscale",
    "xt002": "xt-002", "iron assembly": "iron-assembly", "kologarn": "kologarn",
    "auriaya": "auriaya", "freya": "freya", "hodir": "hodir", "mimiron": "mimiron",
    "thorim": "thorim", "vezax": "vezax", "yogg-saron": "yogg-saron", "sara": "yogg-saron",
    "algalon": "algalon",
}


def node_encounter(node: str) -> str | None:
    for prefix, boss in ULD_PREFIXES.items():
        if node.startswith(prefix):
            return boss
    return None


def prefix_matches_boss(prefix: str, boss: str) -> bool:
    """Whether a key prefix names the boss a trace fought.

    Prefixes are written three ways and all are regular: `thorim`/`mimiron`/`algalon` spell the slug
    out with the punctuation dropped, `fl` is the slug's initials, and `yogg` is its first word.
    Deriving all three beats a fourth hand-mirrored prefix table - there are already two of those and
    they drift. Without the first-word form no `yogg.*` key matches its boss, because `yoggsaron`
    and `ys` are all the other two forms derive.
    """
    words = [word for word in re.split(r"[^a-z0-9]+", boss.lower()) if word]
    flat = "".join(words)
    initials = "".join(word[0] for word in words)
    return prefix in (flat, initials, words[0] if words else "")
