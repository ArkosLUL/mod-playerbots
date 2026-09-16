"""Whether a trace keeps the promises its schema makes: --verify, and batch.py's roll-up of it.
"""
from __future__ import annotations

import math
import statistics
from collections import defaultdict

from .trace import COVERAGE_COLUMNS, DUPLICATE_DEATH_MS, Trace, clock, roster_guids


# Every field that emits a guid or a spell id. The doc's "name every id in the file" rule is the one
# that keeps failing - a new emitter has to be added here as well as swept in the recorder.
VERIFY_UNIT_FIELDS = {
    "dmg": ("s", "d"), "heal": ("s", "d"), "abs": ("d", "s"), "aura": ("d", "s"),
    "cast": ("s", "tgt"), "act": ("g",), "veto": ("g",), "move": ("g", "tgt"),
    "note": ("g",), "death": ("g", "killer"),
}
VERIFY_SPELL_FIELDS = {
    "dmg": ("sp",), "heal": ("sp",), "abs": ("sp",), "aura": ("sp",), "cast": ("sp",), "haz": ("sp",),
}
# How far the health percentages may drift from the damage rows overall. Absorbs and damage no log
# hook sees put a tenth of individual pairs adrift even on a good trace, so this is a median, not a
# per-row bound, and 1pp sits an order of magnitude above what a healthy trace measures.
LEDGER_TOLERANCE_PP = 1.0
# Pairs needed before the medians mean anything. A 30-second pull yields a couple of dozen, and at
# that size one absorbed hit moves the median by a point.
LEDGER_MIN_PAIRS = 60
# A snapshot gap this long inside a pull means samples were dropped, not that the interval is slow.
MAX_SNAP_GAP_MS = 2000


def verify_checks(trace: Trace) -> list[tuple[str, int, list[str]]]:
    """Check the trace against the invariants the schema promises, rather than reading it.

    Three audits running rebuilt these as throwaway scripts, and the last one found two defects that
    way. A failure is either a recorder bug or a schema change nobody wrote down.

    Returns one (label, failures, up to 3 examples) per check so a caller can count them without
    parsing the printed form; show_verify renders it.
    """
    roster = roster_guids(trace)
    snaps = trace.of("snap")
    deaths = trace.of("death")
    findings: list[tuple[str, int, list[str]]] = []

    def report(label: str, bad: list) -> None:
        findings.append((label, len(bad), [str(b) for b in bad[:3]]))

    # --- every id is named ------------------------------------------------------------------------
    unnamed: list[str] = []
    unspelled: list[str] = []
    for rec in trace.records:
        event = rec.get("e")
        stamp = clock(rec.get("t", 0))
        for field in VERIFY_UNIT_FIELDS.get(event, ()):
            guid = rec.get(field)
            if guid and guid not in trace.names:
                unnamed.append(f"{event}.{field} {trace.name(guid)} at {stamp}")
        for field in VERIFY_SPELL_FIELDS.get(event, ()):
            spell = rec.get(field)
            if spell and spell not in trace.spells:
                unspelled.append(f"{event}.{field} {spell} at {stamp}")
        if event == "death":
            for row in rec.get("auras", []):
                if row[0] and row[0] not in trace.spells:
                    unspelled.append(f"death.auras {row[0]} at {stamp}")
                if row[3] and row[3] not in trace.names:
                    unnamed.append(f"death.auras.caster {trace.name(row[3])} at {stamp}")
            for row in rec.get("rewind", []):
                if row[1] and row[1] not in trace.names:
                    unnamed.append(f"death.rewind.src {trace.name(row[1])} at {stamp}")
                if row[2] and row[2] not in trace.spells:
                    unspelled.append(f"death.rewind.sp {row[2]} at {stamp}")
        elif event == "snap":
            for row in rec.get("u", []):
                if row[0] not in trace.names:
                    unnamed.append(f"snap.u {trace.name(row[0])} at {stamp}")
                if len(row) > 10 and row[10] and row[10] not in trace.spells:
                    unspelled.append(f"snap.u.cast {row[10]} at {stamp}")
            for row in rec.get("hz", []):
                if row[0] and row[0] not in trace.spells:
                    unspelled.append(f"snap.hz.sp {row[0]} at {stamp}")
    report("every guid referenced is named", unnamed)
    report("every spell id referenced is named", unspelled)

    # --- snapshot rows are well formed ------------------------------------------------------------
    shape: list[str] = []
    dealt_back: list[str] = []
    pet_dealt: list[str] = []
    gaps: list[str] = []
    last_dealt: dict = {}
    previous_t = None
    # v13 appends power type and power % to every row.
    width = 14 if trace.header.get("v", 0) >= 13 else 12
    for snap in snaps:
        now = snap.get("t", 0)
        if previous_t is not None and now >= 0 and now - previous_t > MAX_SNAP_GAP_MS:
            gaps.append(f"{now - previous_t}ms gap ending {clock(now)}")
        previous_t = now
        for row in snap.get("u", []):
            if len(row) != width:
                shape.append(f"{len(row)} columns at {clock(now)}")
                continue
            if not 0 <= row[5] <= 100 or not 0 <= row[6] <= 100:
                shape.append(f"hp {row[5]} mana {row[6]} for {trace.name(row[0])} at {clock(now)}")
            if width > 12 and not 0 <= row[13] <= 100:
                shape.append(f"power {row[13]} for {trace.name(row[0])} at {clock(now)}")
            if any(not math.isfinite(v) for v in row[1:4]) or max(abs(v) for v in row[1:4]) > 20000:
                shape.append(f"position {row[1:4]} for {trace.name(row[0])} at {clock(now)}")
            if row[0] in trace.owners and row[11]:
                pet_dealt.append(f"{trace.name(row[0])} dealt {row[11]} at {clock(now)}")
            if row[0] in last_dealt and row[11] < last_dealt[row[0]]:
                dealt_back.append(f"{trace.name(row[0])} {last_dealt[row[0]]} -> {row[11]} at {clock(now)}")
            last_dealt[row[0]] = row[11]
    report("snapshot rows well formed", shape)
    report("cumulative `dealt` never decreases", dealt_back)
    report("pet rows carry no `dealt` (the owner is credited)", pet_dealt)
    report("no snapshots dropped inside the pull", gaps)

    # --- health reconciles with the combat rows ----------------------------------------------------
    hits: dict = defaultdict(list)
    healed: dict = defaultdict(set)
    for rec in trace.of("dmg"):
        if rec.get("d") and rec.get("hp") is not None:
            hits[rec["d"]].append((rec["t"], rec.get("a", 0), rec["hp"]))
    for rec in trace.of("heal"):
        if rec.get("d"):
            healed[rec["d"]].add(rec["t"])
    # `dmg.hp` is the health a hit landed ON, not what it left behind: the core logs damage before it
    # applies it, and logs a heal after. So consecutive hits differ by the FIRST row's amount. Asking
    # this pair by pair is too noisy to gate on, so it asks which reading the trace as a whole fits -
    # if the post-hit reading ever wins, the field has flipped and every death rewind is off by a hit.
    landed_on, left_behind = [], []
    for guid, rows in hits.items():
        pool = trace.maxhp.get(guid)
        if not pool or guid not in roster:
            continue
        touched = healed.get(guid, set())
        for (t0, first, hp0), (t1, second, hp1) in zip(rows, rows[1:]):
            if t1 - t0 > 500 or hp0 - hp1 <= 0.05:
                continue
            if any(t0 < when <= t1 for when in touched):
                continue
            landed_on.append(abs((hp0 - hp1) - 100.0 * first / pool))
            left_behind.append(abs((hp0 - hp1) - 100.0 * second / pool))
    ledger: list[str] = []
    if len(landed_on) >= LEDGER_MIN_PAIRS:
        fits, other = statistics.median(landed_on), statistics.median(left_behind)
        if fits > other:
            ledger.append(f"fits post-hit health better ({other:.2f}pp) than pre-hit ({fits:.2f}pp)")
        elif fits > LEDGER_TOLERANCE_PP:
            ledger.append(f"health drifts {fits:.2f}pp from the damage rows over {len(landed_on)} pairs")
    report("`dmg.hp` reads as the health the hit landed on", ledger)

    # --- death records ----------------------------------------------------------------------------
    hurt_at: dict = defaultdict(list)
    for rec in trace.of("dmg"):
        if rec.get("d"):
            hurt_at[rec["d"]].append(rec["t"])
    cause: list[str] = []
    blow_unlogged: list[str] = []
    rewind_order: list[str] = []
    aura_order: list[str] = []
    hplast: list[str] = []
    dist: list[str] = []
    for death in deaths:
        guid, when = death["g"], death["t"]
        has_blow = bool(death.get("blow"))
        if death.get("cause") and has_blow:
            cause.append(f"{trace.name(guid)} at {clock(when)} carries both cause and blow")
        if not has_blow and not death.get("cause"):
            cause.append(f"{trace.name(guid)} at {clock(when)} has neither blow nor cause")
        # .die on yourself, falls and lava go through DealDamage with no combat log, so a blow from the
        # victim never has a dmg row to find.
        own_blow = has_blow and death["blow"][0] == guid
        if has_blow and not own_blow and not any(t <= when for t in hurt_at.get(guid, ())):
            blow_unlogged.append(f"{trace.name(guid)} at {clock(when)}")
        rewind = death.get("rewind", [])
        if any(rewind[i][0] > rewind[i + 1][0] for i in range(len(rewind) - 1)):
            rewind_order.append(f"{trace.name(guid)} at {clock(when)}")
        for row in death.get("auras", []):
            if row[4] != -1 and row[5] != -1 and row[5] < row[4]:
                aura_order.append(f"{trace.spell(row[0])} on {trace.name(guid)} removed before applied")
        before = [s for s in snaps if s.get("t", 0) <= when]
        after = [s for s in snaps if s.get("t", 0) > when]
        # hplast names the sample it came from, so check that one. The nearest sample to the death is
        # a different thing and often already reads 0, the bot having died between the two.
        last = death.get("hplast")
        if last and snaps:
            named = min(snaps, key=lambda s: abs(s.get("t", 0) - last[1]))
            sampled = {row[0]: row[5] for row in named.get("u", [])}
            if (
                guid in sampled
                and abs(named.get("t", 0) - last[1]) < 400
                and abs(sampled[guid] - last[0]) > 0.01
            ):
                hplast.append(f"{trace.name(guid)} says {last[0]}% but the sample says {sampled[guid]}%")
        # A reference unit moves too, so the stated range only has to sit between the samples either
        # side of the death - pinning it to one of them would flag every walking bot.
        if before and after and death.get("x") is not None:
            here = (death["x"], death["y"], death["z"])
            edges = [{row[0]: tuple(row[1:4]) for row in s.get("u", [])} for s in (before[-1], after[0])]
            for key, stated in (death.get("dist") or {}).items():
                other = int(key)
                if not all(other in edge for edge in edges):
                    continue
                spans = [math.dist(here, edge[other]) for edge in edges]
                # Widened by how far the reference itself travelled between the two samples: a pet
                # chasing its owner can be anywhere along that path at the instant of the death, and
                # the two endpoints alone would flag every one of them.
                slack = max(1.5, math.dist(edges[0][other], edges[1][other]))
                if not min(spans) - slack <= stated <= max(spans) + slack:
                    band = f"{min(spans) - slack:.1f}-{max(spans) + slack:.1f}"
                    dist.append(f"{trace.name(other)} from {trace.name(guid)}: {stated} not in {band}")
    report("a death carries a blow or a cause, never both", cause)

    # Every reader folds these, but the file still carries them: an aura that kills its owner on removal
    # re-entered Unit::Kill, and the recorder wrote the one death twice.
    twice: list[str] = []
    previous: dict[int, int] = {}
    for death in deaths:
        guid, when = death["g"], death["t"]
        if guid in previous and when - previous[guid] <= DUPLICATE_DEATH_MS:
            twice.append(f"{trace.name(guid)} at {clock(when)}, {when - previous[guid]} ms after the last")
        previous[guid] = when
    report("no death is recorded twice", twice)
    report("every blow has a damage row behind it", blow_unlogged)
    report("`death.rewind` is in time order", rewind_order)
    report("`death.auras` is removed after it is applied", aura_order)
    report("`death.hplast` matches the sample it came from", hplast)
    report("`death.dist` agrees with the snapshots either side", dist)

    # --- nobody dies unrecorded -------------------------------------------------------------------
    # `end.out` is deliberately not checked here: it latches during combat and never downgrades, so a
    # wipe whose raid released and ran back inside IdleCloseSeconds closes with nobody on the floor.
    missing: list[str] = []
    if snaps and roster:
        recorded = {d["g"] for d in deaths}
        # Only someone who was alive at some point can have died here. A raider who was already a
        # corpse when the pre-roll started reads 0 hp throughout and correctly has no death record.
        seen_alive = {row[0] for snap in snaps for row in snap.get("u", []) if row[5] > 0}
        for row in snaps[-1].get("u", []):
            if row[0] in roster and row[5] == 0 and row[0] in seen_alive and row[0] not in recorded:
                missing.append(f"{trace.name(row[0])} ends at 0 hp with no death record")
    report("everyone who ends dead has a death record", missing)

    # --- node coverage, v12 and up ------------------------------------------------------------------
    undefined: list[str] = []
    over_fired: list[str] = []
    over_pushed: list[str] = []
    for rec in trace.of("cov"):
        who = trace.name(rec.get("g"))
        for row in rec.get("r", []):
            node_id = row[0]
            if node_id not in trace.covnodes:
                undefined.append(f"cov id {node_id} for {who} has no covdef entry")
                continue
            padded = list(row[1:]) + [0] * (len(COVERAGE_COLUMNS) - len(row[1:]))
            counts = dict(zip(COVERAGE_COLUMNS, padded))
            name = trace.covnodes[node_id]["node"]
            if counts["fires"] > counts["checks"]:
                over_fired.append(f"{name}: {counts['fires']} fires from {counts['checks']} checks")
            # A node whose Trigger* fired via a sibling, before or after it in the pass, is pushed
            # without firing itself, so `shared` belongs on this side of the comparison.
            if counts["pushes"] > counts["fires"] + counts["shared"]:
                over_pushed.append(
                    f"{name}: {counts['pushes']} pushes from {counts['fires']}+{counts['shared']}")
    report("every cov row id is defined in covdef", undefined)
    report("cov fires never exceeds checks", over_fired)
    report("cov pushes never exceeds fires plus shared", over_pushed)

    return findings


def show_verify(trace: Trace) -> int:
    findings = verify_checks(trace)
    failed = sum(1 for _, count, _ in findings if count)
    width = max(len(label) for label, _, _ in findings)
    print(f"{trace.path.name}   schema v{trace.header.get('v')}   "
          f"{len(trace.of('death'))} deaths\n")
    for label, count, examples in findings:
        print(f"  {'FAIL' if count else 'ok  '}  {label:<{width}}  {count or ''}")
        for example in examples:
            print(f"          {example}")
    print(f"\n{len(findings) - failed}/{len(findings)} checks passed")
    return 1 if failed else 0
