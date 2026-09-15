"""Loading a RaidObs trace: the NDJSON reader, and the guid and spell lookups every view needs.

A trace is one record per line, `t` in milliseconds relative to the pull and negative during the
pre-roll. Schema and field meanings live in docs/systems/observability.md.
"""
from __future__ import annotations

import datetime
import json
import pathlib
import re
import sys

SUPPORTED_SCHEMA = 12

# Columns of a `cov` row after the node id, in order. Rows are written with trailing zeros trimmed, so
# a short row is padded back out here and a column appended in a later schema reads as zero on an
# older trace.
COVERAGE_COLUMNS = ("checks", "fires", "pushes", "won", "shared", "throttled", "minimal", "dead")

# Old traces stay readable: every addition through v6 is a new field or a new record, so an older file
# only loses the detail those carry. v7 gave an existing column a -1 sentinel, but what it replaces was
# nonsense in older files too, so one render serves both. v8 appends to the end of a snapshot row and
# adds an optional cast field, so a pre-v8 row is just a short one. v10 adds pet rows to the snapshot
# and an owner field on unit, so a pre-v10 file simply has no pets in it. v11 adds hdr.bin and
# hdr.cfg, so a pre-v11 file only cannot say which build or settings produced it. v12 adds the covdef
# and cov records, so --coverage is the one view a pre-v12 trace cannot answer.
READABLE_SCHEMAS = (4, 5, 6, 7, 8, 9, 10, 11, 12)


def clock(ms: int) -> str:
    sign = "-" if ms < 0 else ""
    ms = abs(int(ms))
    return f"{sign}{ms // 60000:d}:{(ms % 60000) / 1000:06.3f}"


class Trace:
    def __init__(self, path: pathlib.Path):
        self.path = path
        self.header: dict = {}
        self.records: list[dict] = []
        self.names: dict[int, str] = {}
        # creature_template entry per guid. Names repeat across unrelated creatures and change with
        # locale, so anything keying off "which creature is this" wants the entry instead.
        self.entries: dict[int, int] = {}
        self.spells: dict[int, str] = {}
        self.roles: dict[int, str] = {}
        # Owner guid per pet/guardian/totem, v10 and up. Pet names are picked at summon time and repeat
        # across owners, so this is the only reliable way to say whose Wolf a row belongs to.
        self.owners: dict[int, int] = {}
        # Max health per guid, for anything that has to turn a percentage back into hit points - the
        # snapshot and every combat row carry hp as a percentage only.
        self.maxhp: dict[int, int] = {}
        self.humans: set[int] = set()
        self.bosses: set[int] = set()
        # Node id -> {node, strategy, engine, alias}, from the covdef dictionary. Empty before v12.
        self.covnodes: dict[int, dict] = {}
        self.truncated = False
        self._load()

    def _load(self) -> None:
        bad = 0
        with self.path.open("r", encoding="utf-8", errors="replace") as fh:
            for line in fh:
                line = line.strip()
                if not line:
                    continue
                try:
                    rec = json.loads(line)
                except json.JSONDecodeError:
                    # A trace truncated by a crash or the size cap ends mid-line; everything before it
                    # is still good, so keep it rather than failing the whole read.
                    bad += 1
                    continue

                if rec.get("e") == "hdr":
                    self.header = rec
                    for member in rec.get("roster", []):
                        self.names[member["g"]] = member["n"]
                        self.roles[member["g"]] = member.get("r", "?")
                        if member.get("h"):
                            self.humans.add(member["g"])
                    continue

                if rec.get("e") == "unit":
                    self.names[rec["g"]] = rec.get("n", "?")
                    if rec.get("en"):
                        self.entries[rec["g"]] = rec["en"]
                    # Players carry a role; a creature does not. Somebody who zoned in after the
                    # header was written only ever appears here.
                    if "r" in rec:
                        self.roles[rec["g"]] = rec["r"]
                    if rec.get("h"):
                        self.humans.add(rec["g"])
                    if rec.get("own"):
                        self.owners[rec["g"]] = rec["own"]
                    if rec.get("mhp"):
                        self.maxhp[rec["g"]] = rec["mhp"]
                    if rec.get("b"):
                        self.bosses.add(rec["g"])
                    continue

                if rec.get("e") == "spell":
                    self.spells[rec["sp"]] = rec.get("n", "?")
                    continue

                if rec.get("e") == "covdef":
                    # Hoisted like unit and spell: the dictionary is written in chunks and a `cov`
                    # block may refer back to a chunk several records earlier.
                    for row in rec.get("d", []):
                        self.covnodes[row[0]] = {
                            "node": row[1],
                            "strategy": row[2] if len(row) > 2 else "",
                            "engine": row[3] if len(row) > 3 else "?",
                            "alias": row[4] if len(row) > 4 else "",
                        }
                    continue

                if rec.get("e") == "truncated":
                    self.truncated = True
                    continue

                self.records.append(rec)

        # Verdicts are written when the engine pass that produced them ends, so their line order can
        # trail their own timestamps by a tick. Sorting once here keeps every view chronological.
        self.records.sort(key=lambda r: r.get("t", 0))

        if bad:
            print(f"note: skipped {bad} unparsable line(s) (trace likely cut short)", file=sys.stderr)

        version = self.header.get("v")
        if version is not None and version not in READABLE_SCHEMAS:
            print(
                f"warning: trace schema v{version}, this tool understands v{SUPPORTED_SCHEMA}",
                file=sys.stderr,
            )

    def name(self, guid) -> str:
        if not guid:
            return "-"
        if guid in self.names:
            owner = self.owners.get(guid)
            # Owner in parentheses, because "Wolf did 40k" is useless and "Wolf (Trueshot)" is not.
            if owner and owner in self.names:
                return f"{self.names[guid]} ({self.names[owner]})"
            return self.names[guid]
        # A guid is a type tag in the high half plus the counter. Printing the bare counter would make
        # a creature and a player that share one look like the same unit.
        return f"#{guid >> 32}:{guid & 0xFFFFFFFF}"

    def role(self, guid) -> str:
        """Tank, heal, melee or ranged, and `?` for anything the roster never placed. An accessor
        because the default is the half that matters and four call sites each re-spelled it."""
        return self.roles.get(guid, "?")

    def spell(self, spell_id) -> str:
        """A spell as a reader recognises it. v4 traces carry no names, so those stay bare numbers."""
        if not spell_id:
            return "melee"
        name = self.spells.get(spell_id)
        return f"{name} {spell_id}" if name else f"spell {spell_id}"

    def of(self, *events: str) -> list[dict]:
        return [r for r in self.records if r.get("e") in events]

    def guid_by_name(self, needle: str) -> int | None:
        needle = needle.lower()
        for guid, name in self.names.items():
            if name.lower() == needle:
                return guid
        for guid, name in self.names.items():
            if needle in name.lower():
                return guid
        return None


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


def pull_time(path: pathlib.Path) -> datetime.datetime | None:
    """When the pull was recorded, off the epoch the recorder puts in the file name.

    Weaker evidence than `hdr.bin`, which says what the binary was: this only says when you played,
    and assumes you rebuilt before you pulled. It is the only thing the 118 pre-v11 traces carry.
    """
    stem = path.stem.rsplit("_", 1)
    if len(stem) != 2 or not stem[1].isdigit():
        return None
    return datetime.datetime.fromtimestamp(int(stem[1]), datetime.timezone.utc)


def find_traces(roots, boss: str | None = None) -> list[pathlib.Path]:
    """Trace paths under `roots`, newest first, deduplicated by resolved path.

    Filtering on the filename means a boss sweep never opens the other files; the corpus runs to
    1.4 GB and individual traces reach 21 MB. A file the name rules out is opened rather than
    dropped, because a pull the recorder never renamed carries the map's name and is otherwise
    unreachable - but only on the miss, so a boss that files itself correctly still costs nothing.
    """
    wanted = canonical_boss(boss) if boss else None
    found: dict[pathlib.Path, float] = {}
    for root in roots:
        root = pathlib.Path(root)
        candidates = [root] if root.is_file() else sorted(root.glob("*.ndjson"))
        for path in candidates:
            if wanted and boss_key(path) != wanted and recover_boss(path) != wanted:
                continue
            resolved = path.resolve()
            if resolved not in found:
                found[resolved] = path.stat().st_mtime
    return sorted(found, key=lambda p: -found[p])


def load_many(roots, boss: str | None = None):
    """Yield one Trace at a time and let each go before the next is read - the corpus does not fit in
    memory, and nothing that sweeps it needs two at once."""
    for path in find_traces(roots, boss):
        yield Trace(path)
