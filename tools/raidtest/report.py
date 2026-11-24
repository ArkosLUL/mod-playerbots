"""The cycle's report: markdown on stdout, built from the DC runs and the traces they produced, one
section per roster pool and then the pools together. A window prints its own header and load line
around one such report per job."""
from __future__ import annotations

import collections
import datetime
import math
import pathlib
import statistics
import subprocess
import sys
from dataclasses import dataclass, field
from typing import Callable, TextIO

import dc
from pools import COMPOSITIONS, PullRow, composition
from pulls import BOTOBS, DIFF_IDS, TraceFacts, vetoes_inside
from raidobs.encounter import canonical_boss

Run = Callable[..., subprocess.CompletedProcess]

VETO_ROWS_SHOWN = 10
LOAD_LIMIT_MS = 400
LOAD_REFERENCE = "one to six parallel 25-man fights read p95 about 307 ms, max under 460"
DONE_DEATHS = 5
DEATHS_IN_FULL = 8
MECHANICS_SHOWN = 12
TREND_BUILDS = 4


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
    label: str = ""
    history: list[tuple[str, TraceFacts]] = field(default_factory=list)


@dataclass
class JobLine:
    number: int
    label: str
    boss: str
    diff: str
    head: str
    verdict: str
    # (pool, valid pulls after the job's head, pulls this window)
    pools: list[tuple[str, int, int]]


@dataclass
class WindowState:
    build_sha: str
    build_time: datetime.datetime | None
    build_note: str
    max_raids: int
    jobs: list[JobLine] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)
    stopped: str | None = None
    most_in_flight: int = 0
    traces: list[TraceFacts] = field(default_factory=list)


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
    if row.engaged_first:
        return "no: the boss engaged before the start step, no refresh"
    if row.aborted:
        return "no: stopped by .dc test stop"
    if facts.warnings:
        return "no: " + ", ".join(kind for kind, _ in facts.warnings)
    if facts.built is None:
        return "no: build unknown"
    if facts.dc_interfered:
        return "no: DC vetoed actions in the fight"
    if facts.froze:
        return f"no: the server froze {facts.froze / 1000:.1f} s in the fight"
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


def pool_label(pool: str) -> str:
    comp = composition(pool)
    return f"{pool} ({comp})" if comp else pool


def mechanic_counts(facts: TraceFacts) -> collections.Counter:
    return collections.Counter(death.mechanic for death in facts.death_rows)


def section_digest_pulls(rows: list[PullRow], state: State, out: TextIO) -> None:
    print("\n## Pulls\n", file=out)
    if not rows:
        print("No pulls.", file=out)
        return
    print("| # | pool | run | outcome | duration | hard mode | deaths | counts | trace |", file=out)
    print("|---|---|---|---|---|---|---|---|---|", file=out)
    for index, row in enumerate(rows, 1):
        facts = row.trace
        duration = row.window[1] - row.window[0] if row.window else (facts.duration_ms if facts else None)
        outcome = facts.outcome if facts else (row.dc_outcome or "-")
        print(f"| {index} | {pool_label(row.pool)} | {row.run_id or '-'} | {outcome} | {minutes(duration)} "
              f"| {hardmode(facts)} | {len(facts.deaths) if facts else '-'} "
              f"| {verdict(row, state.diff, state.head_time)} | {facts.name if facts else '-'} |", file=out)
    flagged = [(index, row) for index, row in enumerate(rows, 1) if row.trace and row.trace.warnings]
    for index, row in flagged:
        for _, text in row.trace.warnings:
            print(f"\n- pull {index} doesn't count: {text}", file=out)


def section_done_rule(state: State, out: TextIO) -> None:
    print("\n## Done rule\n", file=out)
    print(f"On one build, each of {', '.join(COMPOSITIONS)} needs a kill with at most {DONE_DEATHS} deaths; an "
          "Ulduar kill counts only if the boss reader shows hard mode triggered.\n", file=out)
    pulls = [(pool, facts) for pool in state.pools for facts in counted_of(state, pool) if facts.built]
    if not pulls:
        print("No valid pull after HEAD.", file=out)
        return
    newest = max(facts.built for _, facts in pulls)
    pulls = [(pool, facts) for pool, facts in pulls if facts.built == newest]
    print(f"Pulls of the newest build, built {newest.astimezone().strftime('%m-%d %H:%M')}:\n", file=out)
    passed, missing = [], []
    for comp in COMPOSITIONS:
        pools = [pool for pool in state.pools if composition(pool) == comp]
        if not pools:
            missing.append(comp)
            print(f"- {comp}: no pool of it in this report", file=out)
            continue
        mine = [(pool, facts) for pool, facts in pulls if pool in pools]
        good = [(pool, facts) for pool, facts in mine if facts.outcome == "kill" and len(facts.deaths) <= DONE_DEATHS]
        if good:
            pool, facts = min(good, key=lambda pair: len(pair[1].deaths))
            passed.append(comp)
            line = f"kill with {len(facts.deaths)} deaths on {pool} (`{facts.name}`)"
        elif mine:
            line = "no: " + ", ".join(f"{pool} {facts.outcome} {len(facts.deaths)}" for pool, facts in mine)
        else:
            line = "no valid pull on this build"
        print(f"- {comp} ({', '.join(pools)}): {line}", file=out)
    if missing:
        verdict_text = f"can't be judged here: no pool of {', '.join(missing)}"
    elif len(passed) == len(COMPOSITIONS):
        verdict_text = "passes, if the boss reader shows hard mode triggered in each of those kills"
    else:
        verdict_text = "not yet: " + ", ".join(comp for comp in COMPOSITIONS if comp not in passed)
    print(f"\nThis build {verdict_text}.", file=out)


def section_mechanics(rows: list[PullRow], out: TextIO) -> None:
    print("\n## Deaths by mechanic\n", file=out)
    pulls = [row.trace for row in rows if row.trace]
    totals: collections.Counter = collections.Counter()
    seen_in: collections.Counter = collections.Counter()
    first: dict[str, list[int]] = collections.defaultdict(list)
    for facts in pulls:
        counts = mechanic_counts(facts)
        totals.update(counts)
        seen_in.update(counts.keys())
        for mechanic in counts:
            first[mechanic].append(min(d.t_ms for d in facts.death_rows if d.mechanic == mechanic))
    if not totals:
        print("No deaths.", file=out)
        return
    print("| mechanic | deaths | pulls with it | first death (median) |", file=out)
    print("|---|---|---|---|", file=out)
    ranked = totals.most_common()
    for mechanic, total in ranked[:MECHANICS_SHOWN]:
        print(f"| {cell(mechanic)} | {total} | {seen_in[mechanic]}/{len(pulls)} "
              f"| {minutes(int(statistics.median(first[mechanic])))} |", file=out)
    rest = ranked[MECHANICS_SHOWN:]
    if rest:
        print(f"\n{len(rest)} more mechanics, {sum(total for _, total in rest)} deaths.", file=out)


def section_pull_deaths(rows: list[PullRow], out: TextIO) -> None:
    print("\n## Deaths per pull\n", file=out)
    blocks = []
    for index, row in enumerate(rows, 1):
        facts = row.trace
        if facts is None:
            continue
        deaths = sorted(facts.death_rows, key=lambda death: death.t_ms)
        extra = f", plus {facts.wipe_commands} to the wipe command" if facts.wipe_commands else ""
        block = (f"**Pull {index}** {pool_label(row.pool)}, {facts.outcome} at {minutes(facts.duration_ms)}: "
                 f"{len(deaths)} death{'' if len(deaths) == 1 else 's'}{extra}")
        lines = [f"- {minutes(death.t_ms)} {death.name} ({death.role}): {death.mechanic}"
                 for death in deaths[:DEATHS_IN_FULL]]
        rest = deaths[DEATHS_IN_FULL:]
        if rest:
            tally = collections.Counter(death.mechanic for death in rest)
            spread = ", ".join(f"{mechanic} x{count}" for mechanic, count in tally.most_common())
            lines.append(f"- then {len(rest)} more from {minutes(rest[0].t_ms)} to {minutes(rest[-1].t_ms)}: {spread}")
        blocks.append(block + ("\n\n" + "\n".join(lines) if lines else ""))
    print("\n\n".join(blocks) if blocks else "No trace to read deaths from.", file=out)


def section_trend(state: State, out: TextIO) -> None:
    print(f"\n## Trend over the last {TREND_BUILDS} builds\n", file=out)
    builds: dict[datetime.datetime, list[tuple[str, TraceFacts]]] = collections.defaultdict(list)
    for pool, facts in state.history:
        if facts.built:
            builds[facts.built].append((pool, facts))
    if not builds:
        print("No valid pulls of these pools yet.", file=out)
        return
    print("| build | pulls | kills per composition | median deaths | median duration | top deaths per pull |",
          file=out)
    print("|---|---|---|---|---|---|", file=out)
    for built in sorted(builds, reverse=True)[:TREND_BUILDS]:
        pulls = builds[built]
        stamp = built.astimezone().strftime("%m-%d %H:%M")
        mark = " (after HEAD)" if built > state.head_time else ""
        kills = []
        for comp in COMPOSITIONS:
            mine = [facts for pool, facts in pulls if composition(pool) == comp]
            if mine:
                kills.append(f"{comp} {sum(1 for facts in mine if facts.outcome == 'kill')}/{len(mine)}")
        tally: collections.Counter = collections.Counter()
        for _, facts in pulls:
            tally.update(mechanic_counts(facts))
        top = ", ".join(f"{cell(mechanic)} {count / len(pulls):.1f}" for mechanic, count in tally.most_common(3))
        deaths = statistics.median(len(facts.deaths) for _, facts in pulls)
        durations = [facts.duration_ms for _, facts in pulls if facts.duration_ms is not None]
        duration = minutes(int(statistics.median(durations))) if durations else "-"
        print(f"| {stamp}{mark} | {len(pulls)} | {', '.join(kills) or '-'} | {deaths:g} | {duration} "
              f"| {top or '-'} |", file=out)


def section_vetoes_digest(rows: list[PullRow], out: TextIO) -> None:
    lines = []
    for index, row in enumerate(rows, 1):
        window = window_of(row)
        if row.trace is None or window is None:
            continue
        vetoes = vetoes_inside(row.trace, *window)
        if vetoes:
            tally = collections.Counter((veto.multiplier, veto.action) for veto in vetoes)
            lines.append(f"- pull {index}: " + ", ".join(f"{m} zeroed {a} x{n}" for (m, a), n in tally.most_common()))
    if lines:
        print("\n## DC multiplier vetoes inside a pull\n", file=out)
        print("\n".join(lines), file=out)


def section_reader_summaries(state: State, run: Run, out: TextIO) -> None:
    reader = boss_reader(state.boss)
    if not reader:
        print(f"\nNo boss reader for {state.boss} under `bosses/`.", file=out)
        return
    print("\n## Boss reader summaries\n", file=out)
    print(f"Each valid pull's `--summary`; run `bosses/{reader.name} <trace> --<section>` for more.", file=out)
    for pool in state.pools:
        for facts in counted_of(state, pool):
            tool_output(f"{pool_label(pool)}: {facts.name}", [sys.executable, str(reader), str(facts.path),
                                                               "--summary"], run, out)


def render_digest(state: State, run: Run, out: TextIO) -> None:
    rows = state.rows
    section_digest_pulls(rows, state, out)
    section_done_rule(state, out)
    section_mechanics(rows, out)
    section_pull_deaths(rows, out)
    section_trend(state, out)
    section_vetoes_digest(rows, out)
    section_reader_summaries(state, run, out)


def render(state: State, run: Run = subprocess.run, out: TextIO = sys.stdout, full: bool = False) -> None:
    label = f" ({state.label})" if state.label else ""
    print(f"# Raid test: {state.boss} {state.diff}{label}\n", file=out)
    print(headline(state), file=out)
    stamp = state.head_time.astimezone().strftime("%Y-%m-%d %H:%M")
    print(f"\nHEAD `{state.head_sha}` committed {stamp}.", file=out)
    hard = {hardmode(row.trace) for row in state.rows if row.trace and row.trace.hardmode is not None}
    if hard:
        print(f"Hard mode: {', '.join(sorted(hard))}.", file=out)
    for note in state.notes:
        print(f"\n{note}", file=out)
    section_runs(state, out)
    if not full:
        render_digest(state, run, out)
        return
    for pool in state.pools:
        section_pool(pool, state, run, out)
    section_overall(state, run, out)


def cell(text: str) -> str:
    return text.replace("|", "/").replace("\n", " ")


def render_window_header(window: WindowState, out: TextIO = sys.stdout) -> None:
    print("# Raid test window\n", file=out)
    stamp = f" committed {window.build_time.astimezone().strftime('%Y-%m-%d %H:%M')}" if window.build_time else ""
    print(f"Build HEAD `{window.build_sha}`{stamp}: {window.build_note}.", file=out)
    binaries = sorted({facts.built for facts in window.traces if facts.built})
    if binaries:
        stamps = ", ".join(when.astimezone().strftime("%Y-%m-%d %H:%M") for when in binaries)
        print(f"The window's traces name the binary built {stamps}.", file=out)
    if window.stopped:
        print(f"\n**Stopped:** {window.stopped}", file=out)
    for note in window.notes:
        print(f"\n{note}", file=out)
    print("\n| job | label | boss | diff | head | verdict | valid/pulled per pool |", file=out)
    print("|---|---|---|---|---|---|---|", file=out)
    for job in window.jobs:
        pools = ", ".join(f"{pool} {valid}/{pulled}" for pool, valid, pulled in job.pools)
        print(f"| {job.number} | {cell(job.label) or '-'} | {job.boss} | {job.diff} | `{job.head}` "
              f"| {cell(job.verdict)} | {pools} |", file=out)
    print("\nValid: the pool's valid pulls built after the job's head. Pulled: the pool's pulls in this window.",
          file=out)


def percentile(ordered: list[int], share: float) -> int:
    """Nearest rank: the smallest value with at least `share` of the values at or below it."""
    return ordered[max(0, math.ceil(share * len(ordered)) - 1)]


def load_line(traces: list[TraceFacts], most_in_flight: int, max_raids: int) -> str:
    gaps = [gap for facts in traces for gap in facts.snap_gaps]
    flight = (f"at most {most_in_flight} DC run{'' if most_in_flight == 1 else 's'} in flight at once "
              f"(cap {max_raids})")
    if not gaps:
        return f"No snapshot gaps to read in this window's traces; {flight}."
    ordered = sorted(gaps)
    over = sum(1 for gap in gaps if gap > LOAD_LIMIT_MS)
    count = f"{len(traces)} trace{'' if len(traces) == 1 else 's'}"
    return (f"RaidObs snapshot gaps over {count} ({len(gaps)} gaps, cadence 250 ms): "
            f"mean {sum(gaps) / len(gaps):.0f} ms, p95 {percentile(ordered, 0.95)}, "
            f"p99 {percentile(ordered, 0.99)}, max {ordered[-1]}, {over} over {LOAD_LIMIT_MS} ms; {flight}. "
            f"For reference, {LOAD_REFERENCE}.")


def render_window_load(window: WindowState, out: TextIO = sys.stdout) -> None:
    print("\n# Window load\n", file=out)
    print(load_line(window.traces, window.most_in_flight, window.max_raids), file=out)
