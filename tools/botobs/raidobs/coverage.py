"""Which strategy nodes actually did anything, and which were silent for a reason.

A verdict stream says what a bot did. Nothing says what it declined to do, and the ways a node can do
nothing are indistinguishable from outside: a name that resolves to no creator, a check interval that
never elapsed, a condition that was false every pass, and a node that fired into a queue it never won
all leave exactly the same empty space in a trace. Schema v12 counts them separately.

This is the runtime half of tools/pblint/pblint.py and neither subsumes the other. pblint proves the
wiring exists, statically and over every node in the tree. This proves the wiring carried current, on
the nodes one pull actually walked.
"""
from __future__ import annotations

from collections import defaultdict

from .encounter import encounter_of, node_encounter
from .trace import COVERAGE_COLUMNS, Trace

# Below this share of the bots carrying a node, a node that fires is doing so for a subset - a role
# split, or an assignment that only ever lands on the same few. Worth seeing; not a failure.
THIN_SHARE = 0.2


def collect(trace: Trace) -> dict[int, dict]:
    """Per node id: the definition, summed counters, and which bots contributed."""
    totals: dict[int, dict] = {}
    for rec in trace.of("cov"):
        guid = rec.get("g")
        for row in rec.get("r", []):
            node_id = row[0]
            entry = totals.setdefault(node_id, {
                "def": trace.covnodes.get(node_id, {"node": f"#{node_id}", "strategy": "",
                                                    "engine": "?", "alias": ""}),
                "bots": set(),
                "fired_by": set(),
                **{column: 0 for column in COVERAGE_COLUMNS},
            })
            entry["bots"].add(guid)
            for index, column in enumerate(COVERAGE_COLUMNS, start=1):
                entry[column] += row[index] if index < len(row) else 0
            if len(row) > 2 and row[2]:
                entry["fired_by"].add(guid)
    return totals


def bucket(entry: dict, carriers: int) -> str:
    if entry["dead"]:
        return "DEAD"
    if entry["won"]:
        fired = len(entry["fired_by"])
        if carriers and fired and fired / carriers < THIN_SHARE:
            return "THIN"
        return "ok"
    if entry["pushes"]:
        return "LOST"
    if entry["checks"]:
        return "NEVER"
    if entry["throttled"]:
        return "THROT"
    if entry["minimal"]:
        return "MIN"
    return "ok"


# Worst first. DEAD is wiring that cannot work, NEVER and LOST are wiring that works and achieves
# nothing, and the rest are explanations rather than faults.
ORDER = {"DEAD": 0, "NEVER": 1, "LOST": 2, "THIN": 3, "THROT": 4, "MIN": 5, "ok": 6}

EXPLAIN = {
    "DEAD": "no creator resolved it",
    "NEVER": "asked, never true",
    "LOST": "fired, never ran",
    "THIN": "fires for a subset of the raid",
    "THROT": "never got a look in",
    "MIN": "dropped by minimal mode",
    "ok": "",
}


def walked(trace: Trace, prefix: str | None = None) -> tuple[list[dict], int, str]:
    """The nodes this pull actually walked, the count folded away as gated, and the boss.

    An Ulduar node belonging to another encounter was gated off, not silent. Folding these is what
    keeps a Hodir pull from reporting ~150 phantom NEVERs.
    """
    boss = encounter_of(trace)
    gated = 0
    rows = []
    for entry in collect(trace).values():
        node = entry["def"]["node"]
        if prefix and not node.startswith(prefix):
            continue
        owner = node_encounter(node)
        if owner and boss and owner != boss and not entry["won"]:
            gated += 1
            continue
        rows.append(entry)
    return rows, gated, boss


def coverage_metrics(trace: Trace) -> dict[str, float]:
    """Nodes per bucket, for the corpus comparison. A node moving out of LOST is the whole point of
    the view, and it is invisible unless the buckets are counted rather than only printed."""
    rows, _, _ = walked(trace)
    if not rows:
        return {}
    carriers = max((len(e["bots"]) for e in rows), default=0)
    counts: dict[str, float] = defaultdict(float)
    for entry in rows:
        counts[f"cov.{bucket(entry, carriers).lower()}"] += 1
    return dict(counts)


def show_coverage(trace: Trace, prefix: str | None = None, by_bot: bool = False) -> int:
    if not trace.of("cov"):
        version = trace.header.get("v")
        print(f"this trace has no node coverage (schema v{version}, needs v12)")
        return 1

    rows, gated, boss = walked(trace, prefix)
    bots = len({rec.get("g") for rec in trace.of("cov")})
    print(f"node coverage   {boss or 'pull'}   {bots} bot(s)   {len(rows) + gated} node(s) walked\n")

    carriers = max((len(e["bots"]) for e in rows), default=0)
    rows.sort(key=lambda e: (ORDER[bucket(e, carriers)], -e["checks"], e["def"]["node"]))

    width = max((len(e["def"]["node"]) for e in rows), default=20)
    counts: dict[str, int] = defaultdict(int)
    for entry in rows:
        tag = bucket(entry, carriers)
        counts[tag] += 1
        info = entry["def"]
        detail = EXPLAIN[tag]
        if tag in ("NEVER", "THROT"):
            detail = f"asked {entry['checks']}x on {len(entry['bots'])} bot(s), never true" \
                if tag == "NEVER" else f"throttled {entry['throttled']}x, asked {entry['checks']}x"
        elif tag == "LOST":
            detail = f"fired {entry['fires']}x, pushed {entry['pushes']}x, ran 0x"
        elif tag in ("ok", "THIN"):
            detail = (f"fired {entry['fires']}x on {len(entry['fired_by'])}/{len(entry['bots'])}, "
                      f"ran {entry['won']}x")
        elif tag == "DEAD":
            detail = f"no creator resolved it on {len(entry['bots'])} bot(s)"

        alias = f"  (trigger '{info['alias']}')" if info["alias"] else ""
        print(f"  {tag:<6} {info['node']:<{width}}  {info['strategy']:<12} {info['engine']}  "
              f"{detail}{alias}")

        if by_bot:
            for guid in sorted(entry["bots"]):
                print(f"           {trace.name(guid)}")

    if gated:
        print(f"\n  SKIPPED {gated} node(s) belonging to another Ulduar encounter (gate shut this pull)")

    summary = ", ".join(f"{counts[tag]} {tag.lower()}" for tag in ORDER if counts[tag])
    print(f"\n{summary or 'nothing walked'}")
    print("run `tools/pblint/pblint.py src/Ai/Raid` for the static half")
    return 0
