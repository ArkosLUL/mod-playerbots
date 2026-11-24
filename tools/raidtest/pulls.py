"""Which RaidObs traces count as pulls, judged by raidobs itself so this agrees with batch.py --strict.

A pull counts when the trace has no warning at all (batch.py --strict), its build is newer than the
reference commit and its header's difficulty is the one under test. Baseline pulls are the newest such
traces built before the reference instead. Which pool a trace belongs to is pools.py's business, and
so is a pull DC started without its pre-pull refresh, since only DC's record says so.
"""
from __future__ import annotations

import datetime
import hashlib
import json
import os
import pathlib
import sys
import tempfile
import time
from dataclasses import dataclass, field

BOTOBS = pathlib.Path(__file__).resolve().parents[1] / "botobs"
if str(BOTOBS) not in sys.path:
    sys.path.insert(0, str(BOTOBS))

from raidobs.corpus import find_traces  # noqa: E402
from raidobs.encounter import canonical_boss  # noqa: E402
from raidobs.paths import LOG_ROOT  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, death_records, own_blow, own_blow_spell  # noqa: E402
from raidobs.validity import inspect  # noqa: E402

# RaidObs `hdr.diff`, the core's raid difficulty ids
DIFF_IDS = {"10n": 0, "25n": 1, "10h": 2, "25h": 3}
DC_MULTIPLIERS = ("dungeon clear", "dungeon clear combat")
# Far above any load gap seen (under 1 s with six raids at once). A longer one is the whole world
# thread stopped, usually another raid's run being set up, and the frozen fight then catches up in bursts.
FREEZE_MS = 2000
CACHE_ROOT = pathlib.Path(tempfile.gettempdir()) / "raidtest" / "facts"
CACHE_KEEP_S = 14 * 86400


@dataclass
class DeathRow:
    t_ms: int
    name: str
    role: str
    mechanic: str


@dataclass
class Veto:
    at_ms: int
    bot: str
    multiplier: str
    action: str


@dataclass
class TraceFacts:
    path: pathlib.Path
    encounter: str
    built: datetime.datetime | None
    diff: int | None
    warnings: list[tuple[str, str]]
    hardmode: bool | None
    start_ms: int | None
    end_ms: int | None
    outcome: str
    closed: bool
    deaths: list[str] = field(default_factory=list)
    wipe_commands: int = 0
    vetoes: list[Veto] = field(default_factory=list)
    map_id: int | None = None
    instance: int | None = None
    snap_gaps: list[int] = field(default_factory=list)
    death_rows: list[DeathRow] = field(default_factory=list)

    @property
    def name(self) -> str:
        return self.path.name

    @property
    def duration_ms(self) -> int | None:
        if self.start_ms is None or self.end_ms is None:
            return None
        return self.end_ms - self.start_ms

    @property
    def dc_interfered(self) -> bool:
        """A DC multiplier vetoed an action once the encounter was on, so DC ran part of the fight."""
        return self.start_ms is not None and any(veto.at_ms >= self.start_ms for veto in self.vetoes)

    @property
    def froze(self) -> int:
        """The longest server freeze inside the pull in ms, 0 when none passed FREEZE_MS."""
        longest = max(self.snap_gaps, default=0)
        return longest if longest > FREEZE_MS else 0

    def valid(self, diff_id: int) -> bool:
        return (self.built is not None and not self.warnings and self.diff == diff_id and not self.dc_interfered
                and not self.froze)


def death_cause(trace: Trace, death: dict) -> str:
    """The same reading deathreport gives a death, cut down to one line."""
    cause = death.get("cause")
    if cause == "reset":
        return "the wipe command"
    if cause == "self":
        return "a script kill, no blow recorded"
    if own_blow(death):
        spell = own_blow_spell(death)
        return f"its own {trace.spell(spell)}" if spell else "its own blow: a fall, lava or .die"
    line = f"{trace.name(death.get('killer'))}"
    rewind = sorted(death.get("rewind") or [], key=lambda row: row[0])
    if rewind:
        _, source, spell, amount = rewind[-1][:4]
        line += f", last hit {trace.spell(spell)} for {amount} from {trace.name(source)}"
    return line


def death_mechanic(trace: Trace, death: dict) -> str:
    cause = death.get("cause")
    if cause == "reset":
        return "the wipe command"
    if cause == "self":
        return "a script kill"
    if own_blow(death):
        spell = own_blow_spell(death)
        return f"its own {trace.spell(spell)}" if spell else "its own blow"
    rewind = sorted(death.get("rewind") or [], key=lambda row: row[0])
    if rewind:
        _, source, spell = rewind[-1][:3]
        name = trace.spell(spell)
        return f"melee from {trace.name(source)}" if name == "melee" else name
    return f"killed by {trace.name(death.get('killer'))}"


def snapshot_gaps(trace: Trace) -> list[int]:
    """Milliseconds between consecutive snapshots from the pull on (t >= 0), in file order. The recorder
    aims at 250 ms, so the gaps read how far the world thread falls behind under load."""
    stamps = [int(snap["t"]) for snap in trace.of("snap")
              if isinstance(snap.get("t"), (int, float)) and snap["t"] >= 0]
    return [later - earlier for earlier, later in zip(stamps, stamps[1:])]


def read_facts(path: pathlib.Path) -> TraceFacts:
    trace = Trace(path)
    facts, warnings = inspect(trace, None)
    header = trace.header
    start = header.get("ts")
    ends = trace.of("end")
    last_t = ends[-1]["t"] if ends else (trace.records[-1].get("t", 0) if trace.records else 0)
    fought = combat_deaths(trace)
    deaths = [
        f"{trace.name(d['g'])} ({trace.role(d['g'])}) at {clock(d['t'])}: {death_cause(trace, d)}"
        for d in fought
    ]
    death_rows = [DeathRow(int(d["t"]), trace.name(d["g"]), trace.role(d["g"]), death_mechanic(trace, d))
                  for d in fought]
    vetoes = [
        Veto(int(start) + int(rec.get("t", 0)), trace.name(rec.get("g")), rec.get("m", ""), rec.get("a", ""))
        for rec in trace.of("veto")
        if rec.get("m") in DC_MULTIPLIERS and start is not None
    ]
    return TraceFacts(
        path=path,
        encounter=facts["encounter"],
        built=facts["built"],
        diff=header.get("diff"),
        warnings=warnings,
        hardmode=facts["hardmode"],
        start_ms=int(start) if start is not None else None,
        end_ms=int(start) + int(last_t) if start is not None else None,
        outcome=ends[-1].get("out", "?") if ends else "cut short",
        closed=bool(ends),
        deaths=deaths,
        wipe_commands=len(death_records(trace)) - len(combat_deaths(trace)),
        vetoes=vetoes,
        map_id=_int_or_none(header.get("map")),
        instance=_int_or_none(header.get("inst")),
        snap_gaps=snapshot_gaps(trace),
        death_rows=death_rows,
    )


def _int_or_none(value) -> int | None:
    return int(value) if isinstance(value, (int, float)) else None


def facts_to_json(facts: TraceFacts) -> dict:
    return {
        "path": str(facts.path), "encounter": facts.encounter,
        "built": facts.built.isoformat() if facts.built else None, "diff": facts.diff,
        "warnings": [list(pair) for pair in facts.warnings], "hardmode": facts.hardmode,
        "start_ms": facts.start_ms, "end_ms": facts.end_ms, "outcome": facts.outcome, "closed": facts.closed,
        "deaths": facts.deaths, "wipe_commands": facts.wipe_commands,
        "vetoes": [[v.at_ms, v.bot, v.multiplier, v.action] for v in facts.vetoes],
        "map_id": facts.map_id, "instance": facts.instance, "snap_gaps": facts.snap_gaps,
        "death_rows": [[d.t_ms, d.name, d.role, d.mechanic] for d in facts.death_rows],
    }


def facts_from_json(data: dict) -> TraceFacts:
    return TraceFacts(
        path=pathlib.Path(data["path"]), encounter=data["encounter"],
        built=datetime.datetime.fromisoformat(data["built"]) if data["built"] else None, diff=data["diff"],
        warnings=[tuple(pair) for pair in data["warnings"]], hardmode=data["hardmode"],
        start_ms=data["start_ms"], end_ms=data["end_ms"], outcome=data["outcome"], closed=data["closed"],
        deaths=list(data["deaths"]), wipe_commands=data["wipe_commands"],
        vetoes=[Veto(*row) for row in data["vetoes"]], map_id=data["map_id"], instance=data["instance"],
        snap_gaps=list(data["snap_gaps"]), death_rows=[DeathRow(*row) for row in data["death_rows"]],
    )


def code_stamp() -> str:
    digest = hashlib.sha1()
    for source in [pathlib.Path(__file__), *sorted((BOTOBS / "raidobs").glob("*.py"))]:
        digest.update(source.name.encode())
        digest.update(source.read_bytes())
    return digest.hexdigest()[:16]


class FactsCache:
    def __init__(self, boss: str, root: pathlib.Path):
        self.root = root
        self.folder = root / code_stamp()
        self.path = self.folder / f"{boss}.json"
        self.entries: dict[str, dict] | None = None
        self.dirty = False

    @staticmethod
    def key(path: pathlib.Path) -> str | None:
        try:
            stat = path.stat()
        except OSError:
            return None
        return f"{path.resolve()}|{stat.st_size}|{stat.st_mtime_ns}"

    def _read(self) -> dict[str, dict]:
        for attempt in range(5):
            try:
                data = json.loads(self.path.read_text(encoding="utf-8"))
                return data if isinstance(data, dict) else {}
            except PermissionError:
                time.sleep(0.2 * (attempt + 1))
            except (OSError, ValueError):
                return {}
        return {}

    def _load(self) -> dict[str, dict]:
        if self.entries is None:
            self.entries = self._read()
        return self.entries

    def get(self, path: pathlib.Path) -> TraceFacts | None:
        key = self.key(path)
        data = self._load().get(key) if key else None
        if data is None:
            return None
        try:
            facts = facts_from_json(data)
        except (KeyError, TypeError, ValueError):
            return None
        facts.path = path
        return facts

    def put(self, path: pathlib.Path, facts: TraceFacts) -> None:
        key = self.key(path)
        if key:
            self._load()[key] = facts_to_json(facts)
            self.dirty = True

    def flush(self) -> None:
        if not self.dirty:
            return
        spare = self.path.with_name(f"{self.path.name}.{os.getpid()}.tmp")
        try:
            self.folder.mkdir(parents=True, exist_ok=True)
            self.entries = {**self._read(), **self._load()}
            spare.write_text(json.dumps(self.entries), encoding="utf-8")
            os.replace(spare, self.path)
            self.dirty = False
            self._prune()
        except OSError:
            try:
                spare.unlink(missing_ok=True)
            except OSError:
                pass

    def _prune(self) -> None:
        cutoff = time.time() - CACHE_KEEP_S
        for folder in self.root.iterdir():
            try:
                if folder != self.folder and folder.is_dir() and folder.stat().st_mtime < cutoff:
                    for item in folder.iterdir():
                        item.unlink()
                    folder.rmdir()
            except OSError:
                continue


def name_parts(path: pathlib.Path) -> tuple[int, int, int] | None:
    """(map, instance id, epoch seconds) out of <map>_<instanceId>_<boss-slug>_<epoch>.ndjson."""
    parts = path.stem.split("_")
    if len(parts) < 4 or not (parts[0].isdigit() and parts[1].isdigit() and parts[-1].isdigit()):
        return None
    return int(parts[0]), int(parts[1]), int(parts[-1])


class Corpus:
    """One boss's traces, read once each. An open trace is read again next time, since its validity can
    still change."""

    def __init__(self, boss: str, roots: list[pathlib.Path] | None = None,
                 cache_root: pathlib.Path | None = None):
        self.boss = canonical_boss(boss)
        self.roots = roots or [LOG_ROOT]
        self._cache: dict[pathlib.Path, TraceFacts] = {}
        self._by_instance: dict[int, list[tuple[int, int, pathlib.Path]]] | None = None
        self._disk = FactsCache(self.boss, cache_root or CACHE_ROOT)

    def facts(self, path: pathlib.Path) -> TraceFacts:
        cached = self._cache.get(path)
        if cached:
            return cached
        facts = self._disk.get(path)
        if facts is None:
            facts = read_facts(path)
            if facts.closed:
                self._disk.put(path, facts)
        if facts.closed:
            self._cache[path] = facts
        return facts

    def flush(self) -> None:
        self._disk.flush()

    def paths(self) -> list[pathlib.Path]:
        return find_traces(self.roots, self.boss)

    def rescan(self) -> None:
        """List the files again, so traces written since the last scan are found."""
        index: dict[int, list[tuple[int, int, pathlib.Path]]] = {}
        for path in self.paths():
            parts = name_parts(path)
            if parts:
                map_id, instance, epoch = parts
                index.setdefault(instance, []).append((map_id, epoch, path))
        self._by_instance = index

    def in_instance(self, map_id: int, instance: int, start_ms: int, end_ms: int) -> list[TraceFacts]:
        """Traces recorded in one instance that opened inside a window, oldest first. The file name says
        where and when, so no other file is opened."""
        if self._by_instance is None:
            self.rescan()
        inside = []
        for file_map, epoch, path in self._by_instance.get(instance, []):
            # the name's epoch is the header's ts cut to whole seconds
            if (map_id and file_map != map_id) or not start_ms - 1000 < epoch * 1000 <= end_ms:
                continue
            facts = self.facts(path)
            if facts.instance not in (None, instance) or facts.start_ms is None:
                continue
            if start_ms <= facts.start_ms <= end_ms:
                inside.append(facts)
        return sorted(inside, key=lambda f: f.start_ms)


def overlap(a: tuple[int, int], b: tuple[int, int]) -> int:
    return min(a[1], b[1]) - max(a[0], b[0])


def match_traces(windows: list[tuple[int, int]], facts: list[TraceFacts], slack_ms: int = 60000) \
        -> list[TraceFacts | None]:
    """The trace each pull window produced: the one overlapping it most, each trace used once. The slack
    covers the gap between DC starting a pull and the recorder opening on it."""
    used: set[pathlib.Path] = set()
    out: list[TraceFacts | None] = []
    for start, end in windows:
        best, best_overlap = None, 0
        for candidate in facts:
            if candidate.path in used or candidate.start_ms is None or candidate.end_ms is None:
                continue
            span = (candidate.start_ms, candidate.end_ms)
            if overlap((start - slack_ms, end + slack_ms), span) <= 0:
                continue
            score = max(overlap((start, end), span), 1)
            if score > best_overlap:
                best, best_overlap = candidate, score
        if best is not None:
            used.add(best.path)
        out.append(best)
    return out


def vetoes_inside(facts: TraceFacts, start_ms: int, end_ms: int) -> list[Veto]:
    return [veto for veto in facts.vetoes if start_ms <= veto.at_ms <= end_ms]
