"""One function per query mode: --bot, --track, --notes, --stalls, --clump, --verify.

Each returns the process exit code, so main() can hand it straight to sys.exit.
"""
from __future__ import annotations

import math
import statistics
import sys
from collections import defaultdict

from analysis import position_runs, roster_guids
from obstrace import Trace, clock
from records import move_line, note_text

def show_bot(trace: Trace, name: str) -> int:
    guid = trace.guid_by_name(name)
    if guid is None:
        print(f"no unit matching '{name}'", file=sys.stderr)
        return 1

    print(f"timeline for {trace.name(guid)} ({trace.roles.get(guid, '?')})")
    for rec in trace.records:
        event = rec.get("e")
        stamp = clock(rec["t"])
        if event == "act" and rec.get("g") == guid:
            print(f"{stamp}  act    {rec['vd']:<10} rel {rec['rel']:<8} {rec['a']}")
        elif event == "veto" and rec.get("g") == guid:
            print(f"{stamp}  VETO   {rec['m']} killed {rec['a']}")
        elif event == "move" and rec.get("g") == guid:
            print(f"{stamp}  move   {move_line(trace, rec)}")
        elif event == "note" and rec.get("g") == guid:
            print(f"{stamp}  note   {rec['k']} = {note_text(trace, rec)}")
        elif event == "dmg" and rec.get("d") == guid:
            # "at", not "->": a damage log goes out before the core applies the hit, so this is the
            # health the hit landed on. An arrow here reads as the outcome and shifts the whole
            # trajectory down one row, hiding the killing blow. See docs/systems/observability.md.
            lethal = " LETHAL" if rec.get("ok") else ""
            print(
                f"{stamp}  dmg    {rec['a']:>7} from {trace.name(rec['s'])} {trace.spell(rec['sp'])}"
                f" (at {rec['hp']}%){lethal}"
            )
        elif event == "heal" and rec.get("d") == guid:
            print(f"{stamp}  heal   {rec['a']:>7} from {trace.name(rec['s'])} {trace.spell(rec['sp'])} -> {rec['hp']}%")
        elif event == "abs" and rec.get("d") == guid:
            print(f"{stamp}  absorb {rec['a']:>7} by {trace.name(rec['s'])} shield {trace.spell(rec['sp'])}")
        elif event == "aura" and rec.get("d") == guid:
            verb = "lost" if rec.get("r") else "got"
            stacks = f" x{rec['st']}" if "st" in rec else ""
            print(f"{stamp}  aura   {verb} {trace.spell(rec['sp'])}{stacks} from {trace.name(rec['s'])}")
        elif event == "death" and rec.get("g") == guid:
            print(f"{stamp}  DIED   killed by {trace.name(rec.get('killer'))}")

    return 0


def show_track(trace: Trace, name: str) -> int:
    guid = trace.guid_by_name(name)
    if guid is None:
        print(f"no unit matching '{name}'", file=sys.stderr)
        return 1

    print(f"track for {trace.name(guid)}  (step = distance moved since previous sample)")
    header = "time      x        y        z       hp    step   " + "  ".join(
        f"d:{trace.name(b)[:10]}" for b in sorted(trace.bosses)
    )
    print(header)

    previous = None
    for snap in trace.of("snap"):
        rows = {row[0]: row for row in snap.get("u", [])}
        row = rows.get(guid)
        if not row:
            continue

        _, x, y, z, _o, hp = row[0], row[1], row[2], row[3], row[4], row[5]
        step = 0.0 if previous is None else math.dist((x, y, z), previous)
        previous = (x, y, z)

        dists = []
        for boss in sorted(trace.bosses):
            brow = rows.get(boss)
            dists.append(f"{math.dist((x, y, z), (brow[1], brow[2], brow[3])):8.1f}" if brow else "       -")

        print(f"{clock(snap['t']):>9} {x:8.1f} {y:8.1f} {z:7.1f} {hp:5.1f} {step:6.2f}  " + " ".join(dists))

    return 0


def show_notes(trace: Trace, prefix: str | None = None) -> int:
    for rec in trace.records:
        event = rec.get("e")
        # A note key is prefixed with the encounter that owns it, so one raid's latches can be read
        # without the others - Iron Assembly legitimately spans three bosses under one slug.
        if prefix and event == "note" and not rec.get("k", "").startswith(prefix):
            continue

        if event in ("pull", "end"):
            detail = rec.get("boss") or rec.get("out")
            print(f"{clock(rec['t']):>9}  {event.upper():<6} {detail}")
        elif event == "note":
            print(
                f"{clock(rec['t']):>9}  note   {trace.name(rec.get('g')):<16} {rec['k']} = {note_text(trace, rec)}"
            )
        elif event == "haz":
            shape = rec.get("shape", "?")
            # Shape fields differ per shape, so print whatever the probe attached rather than a
            # fixed set: "side" for a lane, "rad" for a circle, "lead"/"sweep"/"rate" for a sweep.
            extra = " ".join(
                f"{k}={v}"
                for k, v in rec.items()
                if k not in ("t", "e", "sp", "shape", "x", "y", "z", "ttl")
            )
            place = f"({rec.get('x')}, {rec.get('y')}, {rec.get('z')})"
            print(f"{clock(rec['t']):>9}  haz    {trace.spell(rec['sp'])} {shape} at {place} ttl {rec.get('ttl')} {extra}")

    return 0


ANSWERED_MOVE_YD = 5.0


def show_stalls(trace: Trace, min_ms: int) -> int:
    """Ordered to move and didn't.

    A bot that has *arrived* is also motionless, so standing still is not the signal on its own. The
    discriminator is that a mover which parks returns before it ever calls MoveTo and so leaves no
    `move` record at all: a stationary window that still contains accepted moves is one where the
    engine asked for a walk and the unit ignored it. That is what a spline-less POINT generator looks
    like from outside - PointMovementGenerator::DoInitialize returns without launching one while the
    unit is in UNIT_STATE_NOT_MOVE, so `ok` means "MovePoint was called", never "the unit moved".
    """
    roster = roster_guids(trace)
    dead_at: dict = {}
    for rec in trace.of("death"):
        dead_at.setdefault(rec["g"], rec["t"])

    tracks = defaultdict(list)
    for snap in trace.of("snap"):
        if snap["t"] < 0:
            continue
        for row in snap.get("u", []):
            if row[0] in roster:
                tracks[row[0]].append((snap["t"], row[1], row[2], row[5]))

    moves = defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("ok"):
            moves[rec.get("g")].append(rec)

    print(f"stationary windows of at least {min_ms / 1000:.0f}s that still contain accepted moves\n")
    total = 0
    for guid, track in sorted(tracks.items(), key=lambda kv: trace.name(kv[0])):
        for start, end, x0, y0 in position_runs(track, 1.0, min_ms):
            if guid in dead_at and end > dead_at[guid]:
                continue
            issued = [m for m in moves[guid] if start <= m["t"] <= end]
            if not issued:
                continue
            # A goal only a few yards out is indistinguishable from having arrived: every mover has an
            # arrival deadband, and a walk that ends inside it looks identical to one never taken.
            furthest = max(math.hypot(m["x"] - x0, m["y"] - y0) for m in issued)
            if furthest <= ANSWERED_MOVE_YD:
                continue
            total += end - start
            owners = sorted({m.get("by") or "?" for m in issued})
            print(
                f"{trace.name(guid):<16} {clock(start):>9} -> {clock(end):>9}"
                f"  {(end - start) / 1000:6.1f}s at ({x0:7.1f},{y0:7.1f})"
                f"  {len(issued)} move(s) accepted, furthest goal {furthest:.0f} yd"
            )
            print(f"{'':16} {'':9}    {'':9}  wanted by: {', '.join(owners)}")

    print(f"\ntotal: {total / 1000:.0f}s")
    return 0


def show_clump(trace: Trace, radius: float) -> int:
    """How much of the pull had the raid stacked inside one AoE.

    Counts distinct *positions*, not bodies: passengers share their vehicle's coordinates exactly, so
    five riders in one siege engine are one thing an area spell can hit, not five.
    """
    roster = roster_guids(trace)
    dead_at: dict = {}
    for rec in trace.of("death"):
        dead_at.setdefault(rec["g"], rec["t"])

    histogram: dict = defaultdict(int)
    for snap in trace.of("snap"):
        if snap["t"] < 0:
            continue
        spots = {
            (round(row[1], 1), round(row[2], 1))
            for row in snap.get("u", [])
            if row[0] in roster and not (row[0] in dead_at and snap["t"] >= dead_at[row[0]])
        }
        if len(spots) < 2:
            continue
        biggest = max(
            sum(1 for x, y in spots if math.hypot(x - cx, y - cy) <= radius) for cx, cy in spots
        )
        histogram[biggest] += 1

    frames = sum(histogram.values())
    if not frames:
        print("no snapshots with two or more live positions")
        return 0

    print(f"most distinct positions inside one {radius:.0f} yd circle, per snapshot\n")
    running = 0
    for size in sorted(histogram, reverse=True):
        running += histogram[size]
        print(
            f"  {size:3d} together  {histogram[size]:6d} frames  {100 * histogram[size] / frames:5.1f}%"
            f"     >= {size}: {100 * running / frames:5.1f}% of the pull"
        )
    return 0


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


def show_verify(trace: Trace) -> int:
    """Check the trace against the invariants the schema promises, rather than reading it.

    Three audits running rebuilt these as throwaway scripts, and the last one found two defects that
    way. A failure is either a recorder bug or a schema change nobody wrote down.
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
    for snap in snaps:
        now = snap.get("t", 0)
        if previous_t is not None and now >= 0 and now - previous_t > MAX_SNAP_GAP_MS:
            gaps.append(f"{now - previous_t}ms gap ending {clock(now)}")
        previous_t = now
        for row in snap.get("u", []):
            if len(row) != 12:
                shape.append(f"{len(row)} columns at {clock(now)}")
                continue
            if not 0 <= row[5] <= 100 or not 0 <= row[6] <= 100:
                shape.append(f"hp {row[5]} mana {row[6]} for {trace.name(row[0])} at {clock(now)}")
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
        if has_blow and not any(t <= when for t in hurt_at.get(guid, ())):
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

    failed = sum(1 for _, count, _ in findings if count)
    width = max(len(label) for label, _, _ in findings)
    print(f"{trace.path.name}   schema v{trace.header.get('v')}   {len(deaths)} deaths\n")
    for label, count, examples in findings:
        print(f"  {'FAIL' if count else 'ok  '}  {label:<{width}}  {count or ''}")
        for example in examples:
            print(f"          {example}")
    print(f"\n{len(findings) - failed}/{len(findings)} checks passed")
    return 1 if failed else 0
