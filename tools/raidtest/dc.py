"""Everything that knows mod-dungeon-clear's console replies and run records.

    .dc test start <scenario> roster=<raid> diff=<10n|10h|25n|25h> pulls=<n>
    .dc test status
    .dc test stop

Run records are JSON lines in dc_testruns.jsonl in the worldserver's working directory. Only this file
parses DC output, so a change on DC's side is a change here.
"""
from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from typing import Callable

DIFFS = ("10n", "10h", "25n", "25h")
RAIDS = ("uld", "toc")

STARTED = "Test run started"
# The driver's not-ready replies all end "retry in a few seconds"
PENDING_MARKERS = ("retry in a few seconds", "is logging in", "still being written to the database")
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
    """The run left `.dc test status` without writing its record, which is what a crash looks like."""


def start_command(scenario: str, raid: str, diff: str, pulls: int) -> str:
    if diff not in DIFFS:
        raise ValueError(f"diff must be one of {', '.join(DIFFS)}")
    if raid not in RAIDS:
        raise ValueError(f"raid must be one of {', '.join(RAIDS)}")
    if pulls < 1:
        raise ValueError("pulls must be at least 1")
    return f".dc test start {scenario} roster={raid} diff={diff} pulls={pulls}"


STATUS_COMMAND = ".dc test status"


def stop_command(run_id: str) -> str:
    return f".dc test stop {run_id}"


def parse_start_reply(text: str) -> tuple[str, str]:
    """("started", run id), ("pending", reply) or ("refused", reply)."""
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
    return "refused", text


def run_active(status_text: str, run_id: str) -> bool:
    return run_id in status_text


@dataclass
class Pull:
    start_ms: int
    end_ms: int
    outcome: str

    @property
    def duration_ms(self) -> int:
        return max(0, self.end_ms - self.start_ms)


@dataclass
class Run:
    run_id: str
    result: str
    fail_reason: str
    started_ms: int
    ended_ms: int
    pulls: list[Pull] | None
    raw: dict = field(repr=False, default_factory=dict)


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
    return Pull(int(start), int(end), str(outcome) if outcome is not None else "?")


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


def run_of(record: dict) -> Run:
    return Run(
        run_id=str(record.get("runId", "")),
        result=str(record.get("result", "")),
        fail_reason=str(record.get("failReason", "")),
        started_ms=int(record.get("startedAtMs") or 0),
        ended_ms=int(record.get("endedAtMs") or 0),
        pulls=pulls_of(record),
        raw=record,
    )


def find_run(records: list[dict], run_id: str) -> Run | None:
    for record in reversed(records):
        if record.get("runId") == run_id:
            return run_of(record)
    return None


Call = Callable[[str], str]


def start_run(call: Call, command: str, clock: Callable[[], float], sleep: Callable[[float], None],
              unavailable: tuple[type[BaseException], ...] = (), window_s: float = 120.0,
              every_s: float = 5.0) -> str:
    """Start a run and return its id, retrying while the driver logs in or SOAP isn't up yet."""
    deadline = clock() + window_s
    last = ""
    while True:
        try:
            kind, value = parse_start_reply(call(command))
        except unavailable as exc:
            kind, value = "pending", str(exc)
        if kind == "started":
            return value
        if kind == "refused":
            raise StartRefused(value.strip())
        last = value.strip()
        if clock() >= deadline:
            raise DriverOffline(f"still not ready after {window_s:.0f} s: {last}")
        sleep(every_s)


def wait_run(call: Call, run_id: str, read_records: Callable[[], str], clock: Callable[[], float],
             sleep: Callable[[float], None], unavailable: tuple[type[BaseException], ...] = (),
             poll_s: float = 20.0, down_after_s: float = 180.0, record_wait_s: float = 120.0) -> Run:
    """Poll status until the run is gone, then return its record."""
    down_since = None
    while True:
        try:
            status = call(STATUS_COMMAND)
        except unavailable as exc:
            now = clock()
            down_since = down_since if down_since is not None else now
            if now - down_since >= down_after_s:
                raise RunVanished(f"worldserver stopped answering during {run_id}: {exc}") from None
        else:
            down_since = None
            if not run_active(status, run_id):
                break
        sleep(poll_s)

    deadline = clock() + record_wait_s
    while True:
        run = find_run(parse_records(read_records()), run_id)
        if run:
            return run
        if clock() >= deadline:
            raise RunVanished(f"{run_id} left .dc test status but never wrote its record")
        sleep(5.0)
