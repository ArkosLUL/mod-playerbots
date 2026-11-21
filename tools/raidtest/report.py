"""The cycle's report: markdown on stdout, built from the DC runs and the traces they produced, one
section per roster pool and then the pools together."""
from __future__ import annotations

import collections
import datetime
import pathlib
import subprocess
import sys
from dataclasses import dataclass, field
from typing import Callable, TextIO

import dc
from pools import PullRow
from pulls import BOTOBS, DIFF_IDS, TraceFacts, vetoes_inside
from raidobs.encounter import canonical_boss

Run = Callable[..., subprocess.CompletedProcess]

VETO_ROWS_SHOWN = 10


@dataclass
class State:
    boss: str
    diff: str
    head_sha: str
    head_time: datetime.datetime
    wanted: int
    pools: list[str] = field(default_factory=list)
    cap: int = 0
    runs: list[tuple[str, dc.Run]] = field(default_factory=list)
    rows: list[PullRow] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)
    stopped: str | None = None
    counted: dict[str, list[TraceFacts]] = field(default_factory=dict)
    baseline: dict[str, list[TraceFacts]] = field(default_factory=dict)


def minutes(ms: int | None) -> str:
    if ms is None:
        return "-"
    seconds = max(0, int(ms)) // 1000
    return f"{seconds // 60}:{seconds % 60:02d}"


def verdict(row: PullRow, diff: str, head_time: datetime.datetime) -> str:
    facts = row.trace
    if facts is None:
        return "no trace"
    if facts.diff != DIFF_IDS[diff]:
        return f"no: difficulty {facts.diff}"
    if facts.warnings:
        return "no: " + ", ".join(kind for kind, _ in facts.warnings)
    if facts.built is None:
        return "no: build unknown"
    return "counts" if facts.built > head_time else "baseline"


def hardmode(facts: TraceFacts | None) -> str:
    if facts is None or facts.hardmode is None:
        return "-"
    return "on" if facts.hardmode else "OFF"


def window_of(row: PullRow) -> tuple[int, int] | None:
    if row.window:
        return row.window
    facts = row.trace
    if facts and facts.start_ms is not None and facts.end_ms is not None:
        return facts.start_ms, facts.end_ms
    return None


def counted_of(state: State, pool: str) -> list[TraceFacts]:
    return state.counted.get(pool, [])[: state.wanted]


def headline(state: State) -> str:
    counts = [(pool, len(counted_of(state, pool))) for pool in state.pools]
    got = sum(count for _, count in counts)
    wanted = state.wanted * len(state.pools)
    per_pool = ", ".join(f"{pool} {count}/{state.wanted}" for pool, count in counts)
    line = f"**{got}/{wanted} valid pulls** after `{state.head_sha}` ({per_pool})"
    test_pulls = sum(1 for row in state.rows if row.phase == "test")
    if state.cap:
        line += f", {test_pulls} pulled this cycle (cap {state.cap} per pool)"
    if state.stopped:
        line += f". **Stopped:** {state.stopped}"
    elif got < wanted and state.cap:
        line += ". The cap ran out first."
    return line


def section_runs(state: State, out: TextIO) -> None:
    if not state.runs:
        return
    print("\n## DC runs\n", file=out)
    print("| phase | pool | run | instance | verdict | pulls | reason |", file=out)
    print("|---|---|---|---|---|---|---|", file=out)
    for phase, run in state.runs:
        pulls = "?" if run.pulls is None else str(len(run.pulls))
        reason = run.fail_reason.replace("|", "/") or "-"
        print(f"| {phase} | {run.pool or '?'} | {run.run_id} | {run.instance_id or '-'} | {run.result or '?'} "
              f"| {pulls} | {reason} |", file=out)
    if any(run.pulls is None for _, run in state.runs):
        print("\nA run record without a pull list was matched to the traces of its instance inside the run.",
              file=out)


def section_pulls(rows: list[PullRow], state: State, out: TextIO) -> None:
    print("\n### Pulls\n", file=out)
    if not rows:
        print("No pulls.", file=out)
        return
    print("| # | phase | run | DC | trace | duration | hard mode | deaths | counts | file |", file=out)
    print("|---|---|---|---|---|---|---|---|---|---|", file=out)
    for index, row in enumerate(rows, 1):
        facts = row.trace
        duration = row.window[1] - row.window[0] if row.window else (facts.duration_ms if facts else None)
        print(f"| {index} | {row.phase} | {row.run_id or '-'} | {row.dc_outcome or '-'} "
              f"| {facts.outcome if facts else '-'} | {minutes(duration)} | {hardmode(facts)} "
              f"| {len(facts.deaths) if facts else '-'} | {verdict(row, state.diff, state.head_time)} "
              f"| {facts.name if facts else '-'} |", file=out)

    flagged = [(index, row) for index, row in enumerate(rows, 1) if row.trace and row.trace.warnings]
    if flagged:
        print("\nWhy a pull doesn't count:\n", file=out)
        for index, row in flagged:
            for _, text in row.trace.warnings:
                print(f"- pull {index}: {text}", file=out)


def section_deaths(rows: list[PullRow], out: TextIO) -> None:
    print("\n### Deaths\n", file=out)
    blocks = []
    for index, row in enumerate(rows, 1):
        facts = row.trace
        if facts is None:
            continue
        extra = f", plus {facts.wipe_commands} to the wipe command" if facts.wipe_commands else ""
        count = f"{len(facts.deaths)} death{'' if len(facts.deaths) == 1 else 's'}" if facts.deaths else "no deaths"
        block = f"**Pull {index}**: {count}{extra}"
        if facts.deaths:
            block += "\n\n" + "\n".join(f"- {line}" for line in facts.deaths)
        blocks.append(block)
    print("\n\n".join(blocks) if blocks else "No trace to read deaths from.", file=out)


def section_contamination(rows: list[PullRow], out: TextIO) -> None:
    print("\n### DC multiplier vetoes inside a pull\n", file=out)
    found = False
    for index, row in enumerate(rows, 1):
        window = window_of(row)
        if row.trace is None or window is None:
            continue
        vetoes = vetoes_inside(row.trace, *window)
        if not vetoes:
            continue
        found = True
        tally = collections.Counter((veto.multiplier, veto.action) for veto in vetoes)
        summary = ", ".join(f"{m} zeroed {a} x{n}" for (m, a), n in tally.most_common())
        print(f"- pull {index}: {len(vetoes)} row(s): {summary}", file=out)
        for veto in vetoes[:VETO_ROWS_SHOWN]:
            offset = minutes(veto.at_ms - window[0])
            print(f"  - {offset} {veto.bot}: {veto.multiplier} zeroed {veto.action}", file=out)
    if not found:
        print("None: no `dungeon clear` or `dungeon clear combat` veto row falls inside a pull.", file=out)


def tool_output(title: str, command: list[str], run: Run, out: TextIO, level: str = "###") -> None:
    print(f"\n{level} {title}\n", file=out)
    script = pathlib.Path(command[1]).relative_to(BOTOBS).as_posix()
    rest = [pathlib.Path(part).name if part.endswith(".ndjson") else part for part in command[2:]]
    print(f"`{' '.join([script, *rest])}`\n", file=out)
    try:
        result = run(command, capture_output=True, text=True, encoding="utf-8", errors="replace",
                     cwd=str(BOTOBS))
    except OSError as exc:
        print(f"could not run it: {exc}", file=out)
        return
    text = (result.stdout or "") + (result.stderr or "")
    print("```text", file=out)
    print(text.rstrip() or "(no output)", file=out)
    print("```", file=out)
    if result.returncode:
        print(f"\nexit {result.returncode}", file=out)


def boss_reader(boss: str) -> pathlib.Path | None:
    """The reader for a canonical boss slug, which can be filed under an alias (general_vezax.py reads vezax)."""
    for path in sorted((BOTOBS / "bosses").glob("*.py")):
        if canonical_boss(path.stem.replace("_", "-")) == boss:
            return path
    return None


def section_pool(pool: str, state: State, run: Run, out: TextIO) -> None:
    rows = [row for row in state.rows if row.pool == pool]
    counted = [str(f.path) for f in counted_of(state, pool)]
    baseline = [str(f.path) for f in state.baseline.get(pool, [])]
    print(f"\n## {pool}\n", file=out)
    print(f"{len(counted)}/{state.wanted} valid after HEAD, {len(baseline)} baseline.", file=out)
    section_pulls(rows, state, out)
    if rows:
        section_deaths(rows, out)
        section_contamination(rows, out)

    python = sys.executable
    batch = str(BOTOBS / "batch.py")
    if counted and baseline:
        tool_output("Baseline against this build",
                    [python, batch, "--boss", state.boss, "--split-at", state.head_sha, *baseline, *counted],
                    run, out)
    else:
        print("\n### Baseline against this build\n", file=out)
        print(f"Skipped: needs valid {pool} pulls on both sides of HEAD "
              f"({len(baseline)} before, {len(counted)} after).", file=out)

    reader = boss_reader(state.boss)
    if reader:
        for path in counted:
            tool_output(f"Boss reader on {pathlib.Path(path).name}", [python, str(reader), path], run, out)


def section_overall(state: State, run: Run, out: TextIO) -> None:
    print("\n## All pools\n", file=out)
    print(headline(state), file=out)
    python = sys.executable
    batch = str(BOTOBS / "batch.py")
    tool_output("Valid pulls since HEAD",
                [python, batch, "--strict", "--since", state.head_sha, "--boss", state.boss], run, out)
    counted = [str(f.path) for pool in state.pools for f in counted_of(state, pool)]
    if counted:
        tool_output("Probes", [python, batch, "--boss", state.boss, "--probes", *counted], run, out)
        if not boss_reader(state.boss):
            print(f"\nNo boss reader for {state.boss} under `bosses/`.", file=out)


def render(state: State, run: Run = subprocess.run, out: TextIO = sys.stdout) -> None:
    print(f"# Raid test: {state.boss} {state.diff}\n", file=out)
    print(headline(state), file=out)
    stamp = state.head_time.astimezone().strftime("%Y-%m-%d %H:%M")
    print(f"\nHEAD `{state.head_sha}` committed {stamp}.", file=out)
    hard = {hardmode(row.trace) for row in state.rows if row.trace and row.trace.hardmode is not None}
    if hard:
        print(f"Hard mode: {', '.join(sorted(hard))}.", file=out)
    for note in state.notes:
        print(f"\n{note}", file=out)
    section_runs(state, out)
    for pool in state.pools:
        section_pool(pool, state, run, out)
    section_overall(state, run, out)
