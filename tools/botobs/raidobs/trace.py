"""Loading a RaidObs trace: the NDJSON reader, and the guid and spell lookups every view needs.

A trace is one record per line, `t` in milliseconds relative to the pull and negative during the
pre-roll. Schema and field meanings live in docs/systems/observability.md.
"""
from __future__ import annotations

import json
import pathlib
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


def roster_guids(trace: Trace) -> set:
    return {member["g"] for member in trace.header.get("roster", [])}


# An aura that kills its owner when it comes off - Yogg's Insane - is stripped by Unit::Kill and calls
# it again, so older recorders wrote that death twice, 0-2 ms apart. The quickest real second death on
# disk, a Reincarnation, took 2 s.
DUPLICATE_DEATH_MS = 50


def death_records(trace: Trace) -> list[dict]:
    """Death records, with a death written twice folded into one.

    The nested kill is written first and carries the blow and the cause, but names the victim as its
    own killer; the outer one names the real killer. Keep the first, with the second's killer.
    """
    kept: list[dict] = []
    last: dict[int, int] = {}
    for rec in trace.of("death"):
        guid = rec.get("g")
        index = last.get(guid)
        if index is not None and rec["t"] - kept[index]["t"] <= DUPLICATE_DEATH_MS:
            if kept[index].get("killer") == guid and rec.get("killer") not in (None, 0, guid):
                kept[index] = {**kept[index], "killer": rec["killer"]}
            continue
        last[guid] = len(kept)
        kept.append(rec)
    return kept


def combat_deaths(trace: Trace) -> list[dict]:
    """Deaths worth reading. The master's `wipe` command kills through Unit::Kill, which never reaches
    DealDamage, so those records carry no blow and name the bot as its own killer - 16 of one Freya
    attempt's 29. Numbering over these keeps --death N pointing at deaths that have a cause."""
    return [d for d in death_records(trace) if d.get("cause") != "reset"]


def first_deaths(trace: Trace) -> dict[int, int]:
    """guid -> `t` of its first death record."""
    dead_at: dict[int, int] = {}
    for rec in trace.of("death"):
        dead_at.setdefault(rec["g"], rec["t"])
    return dead_at


def notes(trace: Trace, key: str) -> list[dict]:
    return [rec for rec in trace.of("note") if rec.get("k") == key]
