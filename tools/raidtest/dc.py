"""Everything that knows mod-dungeon-clear's console replies and run records.

    .dc test start <scenario> roster=<pool> diff=<10n|10h|25n|25h> pulls=<n>
    .dc test status
    .dc test stop <run id>

Run records are JSON lines in DC's dc_testruns.jsonl (path set by DC_TESTRUNS_FILE). Only this file
parses DC output, so a change on DC's side is a change here.
"""
from __future__ import annotations

import json
import pathlib
import re
from dataclasses import dataclass, field
from typing import Callable

DIFFS = ("10n", "10h", "25n", "25h")
RAIDS = ("uld", "toc")
# DcRosterPool::ValidatePoolName: six characters keep dcroster<pool><n> inside an account name
POOL_NAME = re.compile(r"[a-z][a-z0-9]{0,5}")

STARTED = "Test run started"
# The driver's not-ready replies all end "retry in a few seconds"
PENDING_MARKERS = ("retry in a few seconds", "is logging in", "still being written to the database")
BUSY_MARKERS = ("is logged in", "is already in another live test run")
RUN_ID = re.compile(r"\btr-\d{8}-\d{6}(?:-\d+)?\b")

# Where a run record lists its boss pulls, and what each entry calls its fields. `pulls` comes last
# because older records use it for the trash governor's log, whose entries carry none of these.
PULL_LIST_KEYS = ("bossPulls", "attempts", "repulls", "pulls")
PULL_START_KEYS = ("startMs", "startedAtMs", "start", "startedMs")
PULL_END_KEYS = ("endMs", "endedAtMs", "end", "endedMs")
PULL_OUTCOME_KEYS = ("outcome", "result", "out")


class DcError(Exception):
    pass


class StartRefused(DcError):
    """DC answered the start with anything but a started run or a not-ready driver."""


class DriverOffline(DcError):
    """The test driver never got online inside the retry window."""


class RunVanished(DcError):
    """The worldserver stopped answering while runs were going, which is what a crash looks like."""


def valid_pool(pool: str) -> bool:
    return POOL_NAME.fullmatch(pool) is not None


def start_command(scenario: str, pool: str, diff: str, pulls: int) -> str:
    if diff not in DIFFS:
        raise ValueError(f"diff must be one of {', '.join(DIFFS)}")
    if not valid_pool(pool):
        raise ValueError(f"pool {pool!r} must be a lowercase letter then up to 5 lowercase letters or digits")
    if pulls < 1:
        raise ValueError("pulls must be at least 1")
    return f".dc test start {scenario} roster={pool} diff={diff} pulls={pulls}"


STATUS_COMMAND = ".dc test status"


def stop_command(run_id: str) -> str:
    return f".dc test stop {run_id}"


def parse_start_reply(text: str) -> tuple[str, str]:
    """("started", run id), ("pending", reply), ("busy", reply) or ("refused", reply)."""
    lowered = text.lower()
    match = RUN_ID.search(text)
    if match and "started" in lowered and "not started" not in lowered:
        return "started", match.group(0)
    if STARTED in text:
        token = text.split(STARTED, 1)[1].lstrip(": ").split()
        if token:
            return "started", token[0]
        return "refused", text
    if any(marker in lowered for marker in PENDING_MARKERS):
        return "pending", text
    if any(marker in lowered for marker in BUSY_MARKERS):
        return "busy", text
    return "refused", text


def run_active(status_text: str, run_id: str) -> bool:
    # tr-...-1 is a prefix of tr-...-12, a run started in the same second
    return re.search(re.escape(run_id) + r"(?![\w-])", status_text) is not None


@dataclass
class Pull:
    start_ms: int
    end_ms: int
    outcome: str
    # bossPulls[].start from schema 18: "step", or "engaged" when the boss engaged before DC's start step.
    # Empty in older records.
    start: str = ""

    @property
    def duration_ms(self) -> int:
        return max(0, self.end_ms - self.start_ms)

    @property
    def engaged_first(self) -> bool:
        """DC dropped its pre-pull refresh (health, mana, cooldowns, Sated), so the raid went in on
        whatever the last pull left."""
        return self.start == "engaged"


@dataclass
class Run:
    run_id: str
    result: str
    fail_reason: str
    started_ms: int
    ended_ms: int
    pulls: list[Pull] | None
    pool: str = ""
    map_id: int = 0
    instance_id: int = 0
    difficulty: str = ""
    pull_cap: int = 0
    members: list[str] = field(default_factory=list)
    raw: dict = field(repr=False, default_factory=dict)


def read_log(path: str | pathlib.Path) -> str:
    """DC's run log from the host side of the logs volume; empty until the first run writes it."""
    try:
        return pathlib.Path(path).read_text(encoding="utf-8", errors="replace")
    except FileNotFoundError:
        return ""


def parse_records(text: str) -> list[dict]:
    """Every complete record. A line still being appended doesn't parse and is skipped."""
    records = []
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(record, dict):
            records.append(record)
    return records


def _first(entry: dict, keys: tuple[str, ...]):
    for key in keys:
        if key in entry and entry[key] is not None:
            return entry[key]
    return None


def _pull_of(entry) -> Pull | None:
    if not isinstance(entry, dict):
        return None
    start = _first(entry, PULL_START_KEYS)
    end = _first(entry, PULL_END_KEYS)
    if not isinstance(start, (int, float)) or not isinstance(end, (int, float)):
        return None
    outcome = _first(entry, PULL_OUTCOME_KEYS)
    # older shapes used `start` for the time, which PULL_START_KEYS reads
    kind = entry.get("start")
    return Pull(int(start), int(end), str(outcome) if outcome is not None else "?",
                kind if isinstance(kind, str) else "")


def pulls_of(record: dict) -> list[Pull] | None:
    """The boss pulls a run record lists, or None when it lists none in a shape we know."""
    for key in PULL_LIST_KEYS:
        entries = record.get(key)
        if not isinstance(entries, list) or not entries:
            continue
        pulls = [_pull_of(entry) for entry in entries]
        if all(pull is not None for pull in pulls):
            return pulls
    return None


def _int(record: dict, key: str) -> int:
    value = record.get(key)
    return int(value) if isinstance(value, (int, float)) else 0


def run_of(record: dict) -> Run:
    members = record.get("poolMembers")
    return Run(
        run_id=str(record.get("runId", "")),
        result=str(record.get("result", "")),
        fail_reason=str(record.get("failReason", "")),
        started_ms=_int(record, "startedAtMs"),
        ended_ms=_int(record, "endedAtMs"),
        pulls=pulls_of(record),
        pool=str(record.get("pool") or ""),
        map_id=_int(record, "mapId"),
        instance_id=_int(record, "instanceId"),
        difficulty=str(record.get("difficulty") or ""),
        pull_cap=_int(record, "pullCap"),
        members=[str(name) for name in members] if isinstance(members, list) else [],
        raw=record,
    )


def find_run(records: list[dict], run_id: str) -> Run | None:
    for record in reversed(records):
        if record.get("runId") == run_id:
            return run_of(record)
    return None


def pool_runs(records: list[dict], pools: list[str]) -> list[Run]:
    """The last record of every run fielded from one of `pools`, in the order the runs first appear."""
    latest: dict[str, dict] = {}
    for record in records:
        if record.get("pool") in pools and record.get("runId"):
            latest[record["runId"]] = record
    return [run_of(record) for record in latest.values()]


Call = Callable[[str], str]


def start_run(call: Call, command: str, clock: Callable[[], float], sleep: Callable[[float], None],
              unavailable: tuple[type[BaseException], ...] = (), window_s: float = 120.0,
              every_s: float = 5.0, busy_s: float = 60.0) -> str:
    """Start a run and return its id, retrying while the driver logs in or SOAP isn't up yet, and for
    busy_s while a member is still held by a run that just ended."""
    start = clock()
    deadline = start + window_s
    last = ""
    while True:
        try:
            kind, value = parse_start_reply(call(command))
        except unavailable as exc:
            kind, value = "pending", str(exc)
        if kind == "started":
            return value
        if kind == "refused" or (kind == "busy" and clock() - start >= busy_s):
            raise StartRefused(value.strip())
        last = value.strip()
        if clock() >= deadline:
            raise DriverOffline(f"still not ready after {window_s:.0f} s: {last}")
        sleep(every_s)


def wait_runs(call: Call, run_ids: list[str], read_records: Callable[[], str], clock: Callable[[], float],
              sleep: Callable[[float], None], unavailable: tuple[type[BaseException], ...] = (),
              poll_s: float = 20.0, down_after_s: float = 180.0, record_wait_s: float = 120.0) -> dict[str, Run]:
    """Poll status until none of the runs is in it, then return the records they wrote by run id. A run
    missing from the result left status without writing one."""
    down_since = None
    while True:
        try:
            status = call(STATUS_COMMAND)
        except unavailable as exc:
            now = clock()
            down_since = down_since if down_since is not None else now
            if now - down_since >= down_after_s:
                raise RunVanished(f"worldserver stopped answering during {', '.join(run_ids)}: {exc}") from None
        else:
            down_since = None
            if not any(run_active(status, run_id) for run_id in run_ids):
                break
        sleep(poll_s)

    deadline = clock() + record_wait_s
    while True:
        records = parse_records(read_records())
        found = {run_id: run for run_id in run_ids if (run := find_run(records, run_id)) is not None}
        if len(found) == len(run_ids) or clock() >= deadline:
            return found
        sleep(5.0)
