"""The cycle's report: markdown on stdout, built from the DC runs and the traces they produced."""
from __future__ import annotations

import collections
import datetime
import pathlib
import subprocess
import sys
from dataclasses import dataclass, field
from typing import Callable, TextIO

import dc
from pulls import BOTOBS, DIFF_IDS, TraceFacts, vetoes_inside

Run = Callable[..., subprocess.CompletedProcess]

VETO_ROWS_SHOWN = 10


@dataclass
class PullRow:
    phase: str
    run_id: str
    number: int
    window: tuple[int, int] | None
    dc_outcome: str
    trace: TraceFacts | None


@dataclass
class State:
    boss: str
    diff: str
    head_sha: str
    head_time: datetime.datetime
    wanted: int
    cap: int = 0
    runs: list[tuple[str, dc.Run]] = field(default_factory=list)
    rows: list[PullRow] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)
    stopped: str | None = None
    counted: list[TraceFacts] = field(default_factory=list)
    baseline: list[TraceFacts] = field(default_factory=list)


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


def headline(state: State) -> str:
    got = len(state.counted)
    line = f"**{min(got, state.wanted)}/{state.wanted} valid pulls** after `{state.head_sha}`"
    test_pulls = sum(1 for row in state.rows if row.phase == "test")
    if state.cap:
        line += f", {test_pulls} pulled this cycle (cap {state.cap})"
    if state.stopped:
        line += f". **Stopped:** {state.stopped}"
    elif got < state.wanted and state.cap:
        line += ". The cap ran out first."
    return line


def section_runs(state: State, out: TextIO) -> None:
    if not state.runs:
        return
    print("\n## DC runs\n", file=out)
    print("| phase | run | verdict | pulls | reason |", file=out)
    print("|---|---|---|---|---|", file=out)
    for phase, run in state.runs:
        pulls = "?" if run.pulls is None else str(len(run.pulls))
        reason = run.fail_reason.replace("|", "/") or "-"
        print(f"| {phase} | {run.run_id} | {run.result or '?'} | {pulls} | {reason} |", file=out)
    if any(run.pulls is None for _, run in state.runs):
        print("\nA run record without a pull list was matched to the traces that opened inside the run.",
              file=out)


def section_pulls(state: State, out: TextIO) -> None:
    print("\n## Pulls\n", file=out)
    if not state.rows:
        print("No pulls.", file=out)
        return
    print("| # | phase | run | DC | trace | duration | hard mode | deaths | counts | file |", file=out)
    print("|---|---|---|---|---|---|---|---|---|---|", file=out)
    for index, row in enumerate(state.rows, 1):
        facts = row.trace
        duration = row.window[1] - row.window[0] if row.window else (facts.duration_ms if facts else None)
        print(f"| {index} | {row.phase} | {row.run_id or '-'} | {row.dc_outcome or '-'} "
              f"| {facts.outcome if facts else '-'} | {minutes(duration)} | {hardmode(facts)} "
              f"| {len(facts.deaths) if facts else '-'} | {verdict(row, state.diff, state.head_time)} "
              f"| {facts.name if facts else '-'} |", file=out)

    flagged = [(index, row) for index, row in enumerate(state.rows, 1) if row.trace and row.trace.warnings]
    if flagged:
        print("\nWhy a pull doesn't count:\n", file=out)
        for index, row in flagged:
            for _, text in row.trace.warnings:
                print(f"- pull {index}: {text}", file=out)


def section_deaths(state: State, out: TextIO) -> None:
    print("\n## Deaths\n", file=out)
    blocks = []
    for index, row in enumerate(state.rows, 1):
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


def section_contamination(state: State, out: TextIO) -> None:
    print("\n## DC multiplier vetoes inside a pull\n", file=out)
    found = False
    for index, row in enumerate(state.rows, 1):
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


def tool_output(title: str, command: list[str], run: Run, out: TextIO) -> None:
    print(f"\n## {title}\n", file=out)
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


def section_tools(state: State, run: Run, out: TextIO) -> None:
    python = sys.executable
    batch = str(BOTOBS / "batch.py")
    tool_output("Valid pulls since HEAD",
                [python, batch, "--strict", "--since", state.head_sha, "--boss", state.boss], run, out)

    counted = [str(f.path) for f in state.counted[: state.wanted]]
    baseline = [str(f.path) for f in state.baseline]
    if counted and baseline:
        tool_output("Baseline against this build",
                    [python, batch, "--boss", state.boss, "--split-at", state.head_sha, *baseline, *counted],
                    run, out)
    else:
        print("\n## Baseline against this build\n", file=out)
        print("Skipped: needs valid pulls on both sides of HEAD "
              f"({len(baseline)} before, {len(counted)} after).", file=out)

    if counted:
        tool_output("Probes", [python, batch, "--boss", state.boss, "--probes", *counted], run, out)

    reader = BOTOBS / "bosses" / f"{state.boss.replace('-', '_')}.py"
    if counted and reader.is_file():
        for path in counted:
            tool_output(f"Boss reader on {pathlib.Path(path).name}", [python, str(reader), path], run, out)
    elif counted:
        print(f"\nNo boss reader for {state.boss} (`bosses/{reader.name}`).", file=out)


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
    section_pulls(state, out)
    section_deaths(state, out)
    section_contamination(state, out)
    section_tools(state, run, out)
