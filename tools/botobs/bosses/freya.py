#!/usr/bin/env python3
"""Read a Freya pull from a RaidObs trace: hard mode, add waves, who held Freya and the Snaplasher, deaths.

    freya.py <file>          every section
    freya.py <file> --tanks  who Freya and each Snaplasher were on, how long the add tank's own buttons and
                             the healers' externals covered him under it, every area taunt or Righteous
                             Defense a tank cast, each stretch a Snaplasher spent loose on a non-tank, and
                             each lasher pile on the add tank with the buttons held and pressed under it
    freya.py <file> --beams  Unstable Sun Beam dodges per role, and how many healers had the
                             Snaplasher's holder in heal range
    freya.py <file> --ghouls while ghouls hold the lasher pack, where the ranged DPS stand and shoot
    freya.py <file> --blasts per lasher wave, Detonate on the melee, their walks into a low lasher's blast,
                             their bails from a low lasher chasing them, and the chases themselves
    freya.py <file> --lows   per lasher wave, lashers left under the bail line, what the ranged in reach of one
                             hitting a player shot (the Gift, while one was up), and the back line's bails from
                             one hitting them
    freya.py <file> --submerge  per lasher wave, the back line near the pack while it is submerged, the camp's
                             orders then, the Army's ghouls on the lashers that woke onto the back line, and
                             the lasher damage on the back line after it wakes
    freya.py <file> --bombs  Nature Bomb walks and charges into a live bomb per action, how soon a bot a bomb landed
                             on ordered its escape, and how each bot a bomb killed got there

What the generic views miss here:

- **Hard mode is the Elders, not the toggle.** `cfg.hardmode` has no Freya key, so the report's column reads
  `-`. Freya counts the Elders alive at her engage, and each one found casts its essence channel into her
  at once (62483 Stonebark, 62484 Ironbranch, 62485 Brightleaf). A kill also casts the chest the core picks
  by that count (25-man 62952 for three). A trace without the channels falls back to the empowers: Ground
  Tremor casts, Strengthened Iron Roots, Sun Beam stalkers.
- **The trace files under `elder-stonebark`.** Freya's engage pulls each living Elder into combat first, in
  the order Stonebark, Ironbranch, Brightleaf, and the recorder names the pull after the first boss-flagged
  creature to engage; a later name never replaces it. Only a pull with no Elder alive files as `freya`.
- **A wave is the adds first seen together.** It ends when its last member is last sampled alive, and a
  member still alive at the last snapshot never died. Each Eonar's Gift line gives the most ranged and melee DPS
  that had it as their target in one snapshot, and the `freya.giftcut` notes in its life: a bot that took it
  stopped a walk leading away from it.
- **`--threat` folds both tanks into "tank".** Freya belongs on the main tank and the Snaplasher on the
  add tank, so `--tanks` names the holder.
- **A dodge is a move, not a hit.** `--moves` lists every re-issued order of a walk; `--beams` counts the
  first of each, measures it from the nearest beam at that moment, and says which side of Freya it took.
- **Cover is read off auras, not the cooldown notes.** A healer notes `freya.addext` before its cast, so a cast that
  fails (out of range) still writes the name. The cover line takes the tank who held the Snaplasher longest and measures
  his own buttons' auras and the externals' auras from then to his death or its last sample: `both` is overlap, time
  two layers spent on one stretch, and how much of it lay on a damage cut of his own (Barkskin, Icebound Fortitude,
  Shield Wall, Divine Protection) rather than a health button; `two externals` the time two ran at once;
  `no damage cut` the time with neither such a button nor an external up; `none` the time he held it bare. The trace
  logs the auras a death strips 1-2 ms before the death row itself, so "died under" also counts an aura that ended up
  to `DEATH_AURA_SLACK_MS` before the death.
- **A loose Snaplasher is read off the snapshots.** A stretch runs while its target is a raid member who is not a
  tank, and ends at the first snapshot it is on a tank or gone. Each line counts the deaths to it, the hits on its
  bark (Hardened Bark stack casts), the `freya kite snaplasher` orders, and the `freya.kite` notes: the chased
  bot writes "<Snaplasher run speed> vs <own run speed>" whenever either changes, and kites only when the first
  is lower. Traces from before the kite (rounds 3-7 ran a hold instead) show the stretches without kite rows.
- **"Held by ghouls" is the pack focus.** The lasher with the most others within 8 yd (lowest guid on ties), held
  while its target is an Army ghoul or a ghoul's target is it. The finish's lowest-lasher focus is not modelled.
- **A low lasher is a stretch under the bail line.** `--lows` ends a stretch when a heal lifts the lasher back over it,
  calls a stretch of 8 s or more lingering, and counts a lasher as hitting a player when its victim is a raid member.
- **A walk into a blast is an order's destination.** `--blasts` counts a melee bot's `reach melee` or `set behind`
  order whose point lies inside the blast edge of a lasher under the bail line in the snapshot before it. Tanks
  are left out: they never bail. A bail is "from a low lasher chasing that bot" when such a lasher inside the
  blast edge has the bailing bot as its target in the snapshot before the order. A chase is the run of snapshots in
  which one lasher under the bail line keeps the same melee bot as its target; the victim's walk sums its steps.
- **A lasher pile is the add tank's, not the main tank's.** `--tanks` takes every tank but the one who held Freya
  longest, runs a pile while lashers target him (gaps up to `HOLD_MIN_MS` bridged), and lists it when his health
  went under the lasher last-resort line or a button was held (`freya add tank defensive hold` veto rows, first per
  name) or pressed (an own button's aura) in it, or he died. Health is the nearest snapshot's, so a fast drop can
  read one sample high or low.
- **A bomb is live for its fuse from the snapshot that first shows it.** `--bombs` counts an order given from
  outside every live bomb's avoid radius whose straight walk passes inside one, and a charge whose landing (the
  snapshot `CHARGE_LAND_MS` after the cast) is inside one, once per cast: the triggered stun or leap carries the
  charge's name too and is skipped; "then hit" is a Nature Bomb hit from such a bomb on that
  bot within the fuse. A bomb death "walked in" when such an order or charge reached a bomb that hit it. A bomb
  landed at a bot's feet when that snapshot has it within `ON_BOMB_YD`; its escape is the bot's first
  `freya move away nature bomb` order in the fuse, once per bot per volley.
- **A pack is submerged for `SUBMERGE_MS` from its first snapshot.** `--submerge` measures the ranged and
  healers' nearest lasher in that snapshot and the last one before the wake, and the lashers' melee and Flame
  Lash on them over `WAKE_WINDOW_MS` from the wake. A lasher woke onto the back line when its first target within
  `WAKE_PICK_MS` of the wake is a ranged DPS or healer. The wave's Army is the first cast from `ARMY_LEAD_MS`
  before its spawn to its end; its caster's distance is read in the first snapshot of that window showing him, and
  the ghouls' hold at `ARMY_OUT_MS` after the cast or `WAKE_CHECK_MS` after the wake, whichever is later.
"""
from __future__ import annotations

import bisect
import collections
import pathlib
import statistics
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import Unknown, at, dist2, frames, radius  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, own_blow_spell  # noqa: E402

ENCOUNTER = "freya"

NPC_FREYA = 32906
NPC_SNAPLASHER = 32916
NPC_DETONATING_LASHER = 32918
NPC_CONSERVATOR = 33203
NPC_EONARS_GIFT = 33228
NPC_STRENGTHENED_IRON_ROOTS = 33168
NPC_SUN_BEAM = 33170
NPC_BRIGHTLEAF_SUN_BEAM = 33050
TRIO = (NPC_SNAPLASHER, 32919, 33202)
WAVE_KINDS = {NPC_DETONATING_LASHER: "lashers", NPC_CONSERVATOR: "conservator", **{e: "trio" for e in TRIO}}
GROUND_TREMOR = (62437, 62859)
ESSENCE_CHANNELS = {62483: "Stonebark", 62484: "Ironbranch", 62485: "Brightleaf"}
# boss_freya.cpp summonChestSpell: 10-man and 25-man chest per Elder count.
KILL_CHESTS = {62957: 0, 62958: 0, 62955: 1, 62956: 1, 62953: 2, 62954: 2, 62950: 3, 62952: 3}
DETONATE = (62598, 62937)
APPROACH_ACTIONS = ("reach melee", "set behind")
BAIL_ACTION = "freya lasher about to blow"
OUTRANK_LASHERS = (NPC_CONSERVATOR, *TRIO)
# Spells that take every unit near the caster or hitting the target, rather than the caster's own target.
AREA_TAUNTS = {5209: "Challenging Roar", 1161: "Challenging Shout", 31789: "Righteous Defense"}

# One SpawnWave summons a block inside a tick, but sampling every 250 ms can split it across two snapshots.
SAME_WAVE_MS = 2000
# Lifebinder's Gift heals Freya and her adds once the Gift has lived this long.
GIFT_CAST_MS = 12000
# Shorter holds are a swing or two landing between snapshots, not a lost target.
HOLD_MIN_MS = 1000
DODGE_ACTION = "freya dodge unstable sun beam"
# Orders a bot re-issues while it walks one dodge come this close together; a new beam takes longer.
DODGE_EPISODE_MS = 1500
# Playerbots.HealDistance, the range a healer walks to before it casts.
HEAL_RANGE_YD = 38.5
NPC_NATURE_BOMB = 34129
NATURE_BOMB = (64587, 64650)
BOMB_FUSE_MS = 6000
CHARGES = ("Charge", "Intercept", "Feral Charge - Bear", "Feral Charge - Cat")
CHARGE_LAND_MS = 700
# Fewer healers than this on the add tank and one capped Snaplasher swing outruns the heals.
HEALERS_WANTED = 2
KITE_ACTION = "freya kite snaplasher"
KITE_NOTE = "freya.kite"
GIFT_CUT_NOTE = "freya.giftcut"
COOLDOWN_NOTES = {"freya.adddef": "defensive under the Snaplasher", "freya.addext": "external on the Snaplasher's tank"}
# not casts, the probes write these so the same button cast again next wave still gets its own row
COOLDOWN_NO_PICK = {"covered", "none"}
OWN_DEFENSIVES = {22812: "Barkskin", 22842: "Frenzied Regeneration", 61336: "Survival Instincts",
                  48792: "Icebound Fortitude", 55233: "Vampiric Blood", 871: "Shield Wall", 498: "Divine Protection",
                  12976: "Last Stand"}  # Last Stand's buff, its cast 12975 never shows up as an aura
OWN_DAMAGE_CUTS = {22812, 48792, 871, 498}
DEFENSIVE_HOLD = "freya add tank defensive hold"
DEATH_AURA_SLACK_MS = 50
EXTERNALS = {33206: "Pain Suppression", 47788: "Guardian Spirit", 6940: "Hand of Sacrifice"}
# Hardened Bark's stack aura, 10- and 25-man: the Snaplasher casts one for every direct hit it takes.
BARK_STACKS = (62663, 64190)
NPC_ARMY_GHOUL = 24207
REACH_ACTION = "freya reach held pack"
# Playerbots.SpellDistance: past this from every lasher a ranged bot has none to shoot.
SPELL_RANGE_YD = 28.5
# Detonate's searcher adds object size at both ends, so its 15 yd lands out to here.
DETONATE_REACH_YD = 15.5
# The last lashers blow as they die, after their last sample alive.
BLAST_TAIL_MS = 1000
# A lasher under the bail line usually blows within about 4 s; one still up after this is being left alone.
LINGER_MS = 8000
CHASE_MS = 5000
CAMP_ACTION = "freya ranged camp"
GIFT_WALK_ACTION = "freya reach gift"
DOT_SPREAD_SUFFIX = " on attacker"
SPREAD_CAST_SLACK_MS = 50
BOMB_ESCAPE_ACTION = "freya move away nature bomb"
ON_BOMB_YD = 1.5
ESCAPE_EARLY_MS = 500
ESCAPE_QUICK_MS = 1500
ESCAPE_LATE_MS = 4500
SUBMERGE_MS = 5000
WAKE_WINDOW_MS = 30000
WAKE_PICK_MS = 3000
WAKE_CHECK_MS = 4000
ARMY_OF_THE_DEAD = 42650
ARMY_LEAD_MS = 3000
ARMY_OUT_MS = 8500
FLAME_LASH = 62608


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def sightings(trace: Trace, entries) -> dict[int, dict]:
    """Per creature of these entries: first and last snapshot alive, health then, and its targets."""
    seen: dict[int, dict] = {}
    for snap in frames(trace):
        for row in snap.get("u", []):
            if trace.entries.get(row[0]) not in entries or row[5] <= 0:
                continue
            rec = seen.setdefault(row[0], {"g": row[0], "first": snap["t"], "spans": []})
            rec["last"], rec["hp"] = snap["t"], row[5]
            spans = rec["spans"]
            if spans and spans[-1][2] == row[7]:
                spans[-1][1] = snap["t"]
            else:
                spans.append([snap["t"], snap["t"], row[7]])
    return seen


def last_snapshot(trace: Trace) -> int:
    snaps = frames(trace)
    return snaps[-1]["t"] if snaps else 0


def elders(trace: Trace) -> tuple[list[str], str]:
    """The Elders alive at Freya's engage, and what the answer was read from."""
    casts = trace.of("cast")
    channels = {ESSENCE_CHANNELS[rec["sp"]] for rec in casts if rec.get("sp") in ESSENCE_CHANNELS}
    if channels:
        return [name for name in ESSENCE_CHANNELS.values() if name in channels], "essence channels"
    seen = set(trace.entries.values())
    tremor = any(rec.get("sp") in GROUND_TREMOR for rec in casts)
    return [name for name, found in (("Stonebark", tremor), ("Ironbranch", NPC_STRENGTHENED_IRON_ROOTS in seen),
                                     ("Brightleaf", NPC_SUN_BEAM in seen)) if found], "empowers"


def kill_chest(trace: Trace) -> tuple[int, int] | None:
    """(spell, Elder count) of the chest a kill summoned, or None."""
    return next(((rec["sp"], KILL_CHESTS[rec["sp"]]) for rec in trace.of("cast") if rec.get("sp") in KILL_CHESTS), None)


def show_banner(trace: Trace) -> None:
    print(f"Freya  {trace.path.name}")
    if not mine(trace):
        print("  not a Freya pull, every section below reads empty")
        return
    ends = trace.of("end")
    out = ends[-1].get("out", "?") if ends else "?"
    when = f" at {clock(ends[-1]['t'])}" if ends else ""
    found, source = elders(trace)
    chest = kill_chest(trace)
    print(f"  outcome {out}{when}, {len(combat_deaths(trace))} death(s)")
    print(f"  hard mode: {len(found)} of 3 Elders ({', '.join(found) or 'none'}), from their {source}")
    if chest:
        print(f"  kill chest {chest[0]}: {chest[1]} Elder(s)")


def waves(trace: Trace) -> list[dict]:
    """Lasher packs, trio waves and Conservators in spawn order, with when each ended and how many of it
    were still alive when the pull did."""
    end = last_snapshot(trace)
    out: list[dict] = []
    for rec in sorted(sightings(trace, WAVE_KINDS).values(), key=lambda r: r["first"]):
        kind = WAVE_KINDS[trace.entries[rec["g"]]]
        wave = next((w for w in out if w["kind"] == kind and rec["first"] - w["t"] <= SAME_WAVE_MS), None)
        if wave is None:
            wave = {"kind": kind, "t": rec["first"], "members": []}
            out.append(wave)
        wave["members"].append(rec)
    for wave in out:
        wave["end"] = max(m["last"] for m in wave["members"])
        wave["alive"] = sum(1 for m in wave["members"] if m["last"] >= end)
    return out


def show_waves(trace: Trace) -> None:
    print("WAVES")
    if not mine(trace):
        return
    deaths = combat_deaths(trace)
    for wave in waves(trace):
        dead = sum(1 for d in deaths if wave["t"] <= d["t"] <= wave["end"])
        left = f", {wave['alive']} alive at the end" if wave["alive"] else ""
        print(f"  {clock(wave['t'])} .. {clock(wave['end'])}  {wave['kind']:<11} {len(wave['members'])} add(s)"
              f"{left}, {dead} raid death(s) meanwhile")
    end = last_snapshot(trace)
    cuts = [rec for rec in trace.of("note") if rec.get("k") == GIFT_CUT_NOTE and rec.get("txt")]
    walks = [rec for rec in trace.of("move") if rec.get("by") == GIFT_WALK_ACTION and rec.get("ok")]
    for gift in sorted(sightings(trace, (NPC_EONARS_GIFT,)).values(), key=lambda r: r["first"]):
        lived = gift["last"] - gift["first"]
        if lived >= GIFT_CAST_MS:
            verdict = f"lived to heal, {gift['hp']:.0f}% left"
        else:
            verdict = "alive at the end" if gift["last"] >= end else "killed"
        takers = gift_takers(trace, gift)
        cut = sum(1 for rec in cuts if gift["first"] <= rec["t"] <= gift["last"])
        walked = sum(1 for rec in walks if gift["first"] <= rec["t"] <= gift["last"])
        print(f"  {clock(gift['first'])} Eonar's Gift {lived / 1000:.1f}s, {verdict}; on it at most "
              f"{takers['ranged']} ranged and {takers['melee']} melee; {cut} walk(s) away from it cut, "
              f"{walked} walk(s) to it round the lashers, {gift_dot_spreads(trace, gift)} DoT spread(s) off it "
              f"by its takers")


def gift_dot_spreads(trace: Trace, gift: dict) -> int:
    snaps = [snap for snap in frames(trace) if gift["first"] <= snap["t"] <= gift["last"]]
    stamps = [snap["t"] for snap in snaps]
    lo, hi = gift["first"] - SPREAD_CAST_SLACK_MS, gift["last"] + SPREAD_CAST_SLACK_MS
    casts = collections.defaultdict(list)
    for rec in trace.of("cast"):
        if not rec.get("tr") and lo <= rec["t"] <= hi:
            casts[rec.get("s")].append(rec)
    count = 0
    for rec in trace.of("act"):
        if not gift["first"] <= rec["t"] <= gift["last"] or rec.get("vd") != "OK":
            continue
        if not rec.get("a", "").endswith(DOT_SPREAD_SUFFIX):
            continue
        index = bisect.bisect_right(stamps, rec["t"]) - 1
        if index < 0 or not any(row[0] == rec.get("g") and row[7] == gift["g"] for row in snaps[index].get("u", [])):
            continue
        landed = [cast for cast in casts[rec.get("g")] if abs(cast["t"] - rec["t"]) <= SPREAD_CAST_SLACK_MS]
        if landed and all(cast.get("tgt") != gift["g"] for cast in landed):
            count += 1
    return count


def gift_takers(trace: Trace, gift: dict) -> dict:
    most = {"ranged": 0, "melee": 0}
    for snap in frames(trace):
        if not gift["first"] <= snap["t"] <= gift["last"]:
            continue
        on = collections.Counter(trace.role(row[0]) for row in snap.get("u", []) if row[7] == gift["g"] and row[5] > 0)
        for role in most:
            most[role] = max(most[role], on[role])
    return most


def holder_label(trace: Trace, guid) -> str:
    role = trace.role(guid)
    return f"{trace.name(guid)}/{role}" if role != "?" else trace.name(guid)


def main_tank(trace: Trace) -> int | None:
    held: collections.Counter = collections.Counter()
    for freya in sightings(trace, (NPC_FREYA,)).values():
        for t0, t1, target in freya["spans"]:
            held[target] += t1 - t0
    return max((g for g in held if trace.role(g) == "tank"), key=held.get, default=None)


def show_tanks(trace: Trace) -> None:
    print("TANKS")
    if not mine(trace):
        return
    for freya in sightings(trace, (NPC_FREYA,)).values():
        held: collections.Counter = collections.Counter()
        for t0, t1, target in freya["spans"]:
            held[target] += t1 - t0
        main = max((g for g in held if trace.role(g) == "tank"), key=held.get, default=None)
        total = sum(held.values()) or 1
        print(f"  Freya on {holder_label(trace, main)} {100.0 * held[main] / total:.1f}% of the pull")
        for t0, t1, target in freya["spans"]:
            if target != main and target and t1 - t0 >= HOLD_MIN_MS:
                print(f"    {clock(t0)} .. {clock(t1)} on {holder_label(trace, target)}")
    for snap in sorted(sightings(trace, (NPC_SNAPLASHER,)).values(), key=lambda r: r["first"]):
        held = collections.Counter()
        for t0, t1, target in snap["spans"]:
            held[target] += t1 - t0
        total = sum(held.values()) or 1
        share = ", ".join(f"{holder_label(trace, g)} {100.0 * ms / total:.0f}%" for g, ms in held.most_common(4))
        print(f"  Snaplasher {clock(snap['first'])} .. {clock(snap['last'])}: {share}")
    for cover in snaplasher_cover(trace):
        line = (f"  Snaplasher {clock(cover['spawn'])} on {holder_label(trace, cover['tank'])} "
                f"{(cover['until'] - cover['t']) / 1000:.1f}s: own {cover['own'] / 1000:.1f}s, external "
                f"{cover['ext'] / 1000:.1f}s, both {cover['both'] / 1000:.1f}s ({cover['both_cut'] / 1000:.1f}s on a "
                f"damage cut), two externals {cover['two_ext'] / 1000:.1f}s, no damage cut "
                f"{cover['no_cut'] / 1000:.1f}s, none {cover['none'] / 1000:.1f}s")
        if cover["died"] is not None:
            line += f"; died {clock(cover['died'])} under {', '.join(cover['under']) or 'nothing'}"
        print(line)
    for rec in trace.of("cast"):
        if rec.get("sp") in AREA_TAUNTS and trace.role(rec.get("s")) == "tank" and not rec.get("tr"):
            target = f" on {holder_label(trace, rec['tgt'])}" if rec.get("tgt") else ""
            print(f"  {clock(rec['t'])} {trace.name(rec['s'])} {AREA_TAUNTS[rec['sp']]}{target}")
    for rec in trace.of("note"):
        if rec.get("k") in COOLDOWN_NOTES and rec.get("txt") not in COOLDOWN_NO_PICK:
            print(f"  {clock(rec['t'])} {trace.name(rec.get('g'))} {COOLDOWN_NOTES[rec['k']]}: {rec.get('txt')}")
    for stretch in loose_stretches(trace):
        victims = ", ".join(holder_label(trace, guid) for guid in stretch["victims"])
        line = (f"  {clock(stretch['t'])} .. {clock(stretch['until'])} Snaplasher of {clock(stretch['spawn'])} loose "
                f"{(stretch['until'] - stretch['t']) / 1000:.1f}s, {stretch['from']:.0f}% to {stretch['to']:.0f}%, "
                f"on {victims}: {stretch['deaths']} death(s) to it, {stretch['bark']} hit(s) on its bark, "
                f"{stretch['kites']} kite order(s)")
        if stretch["reads"]:
            line += f", slower than its victim in {stretch['slower']} of {stretch['reads']} speed read(s)"
        print(line)
    for pile in lasher_piles(trace):
        held = ", ".join(f"{name} at {pct:.0f}%" for name, pct in pile["held"]) or "nothing"
        pressed = ", ".join(f"{name} at {pct:.0f}%" for name, pct in pile["pressed"]) or "nothing"
        line = (f"  {clock(pile['t'])} .. {clock(pile['until'])} {holder_label(trace, pile['g'])} under lashers "
                f"{(pile['until'] - pile['t']) / 1000:.1f}s, up to {pile['most']} on him, {pile['from']:.0f}% to "
                f"{pile['low']:.0f}%: held {held}; pressed {pressed}")
        if pile["died"] is not None:
            line += f"; died {clock(pile['died'])}"
        print(line)


def health_at(series: list[tuple[int, float]], when: int) -> float:
    index = bisect.bisect_right(series, (when, float("inf")))
    near = [series[i] for i in (index - 1, index) if 0 <= i < len(series)]
    return min(near, key=lambda sample: abs(sample[0] - when))[1] if near else 0.0


def lasher_piles(trace: Trace) -> list[dict]:
    line = radius("ULDUAR_FREYA_ADD_TANK_LASHER_LAST_RESORT_PCT")
    main = main_tank(trace)
    tanks = {guid for guid, role in trace.roles.items() if role == "tank" and guid != main}
    health: dict[int, list[tuple[int, float]]] = {guid: [] for guid in tanks}
    open_piles: dict[int, dict] = {}
    piles: list[dict] = []
    for snap in frames(trace):
        rows = snap.get("u", [])
        on = collections.Counter(row[7] for row in rows
                                 if trace.entries.get(row[0]) == NPC_DETONATING_LASHER and row[5] > 0)
        for row in rows:
            if row[0] in tanks:
                health[row[0]].append((snap["t"], row[5]))
        for guid in tanks:
            if not on[guid]:
                continue
            pile = open_piles.get(guid)
            if pile is None or snap["t"] - pile["until"] > HOLD_MIN_MS:
                pile = {"g": guid, "t": snap["t"], "most": 0}
                open_piles[guid] = pile
                piles.append(pile)
            pile["until"] = snap["t"]
            pile["most"] = max(pile["most"], on[guid])
    deaths = combat_deaths(trace)
    vetoes = [rec for rec in trace.of("veto") if rec.get("m") == DEFENSIVE_HOLD]
    buttons = [rec for rec in trace.of("aura") if rec.get("sp") in OWN_DEFENSIVES and not rec.get("r")]
    out: list[dict] = []
    for pile in piles:
        guid, t0, t1 = pile["g"], pile["t"], pile["until"] + HOLD_MIN_MS
        series = health[guid]
        held: dict[str, float] = {}
        for rec in vetoes:
            if rec.get("g") == guid and t0 <= rec["t"] <= t1:
                held.setdefault(rec.get("a", "?"), health_at(series, rec["t"]))
        pile["held"] = list(held.items())
        pile["pressed"] = [(OWN_DEFENSIVES[rec["sp"]], health_at(series, rec["t"])) for rec in buttons
                           if rec.get("d") == guid and t0 <= rec["t"] <= t1]
        pile["died"] = next((d["t"] for d in deaths if d.get("g") == guid and t0 <= d["t"] <= t1), None)
        pile["from"] = health_at(series, t0)
        pile["low"] = min((pct for when, pct in series if t0 <= when <= pile["until"]), default=pile["from"])
        if pile["low"] < line or pile["held"] or pile["pressed"] or pile["died"] is not None:
            out.append(pile)
    return out


def aura_spans(trace: Trace, guid, spells: dict) -> list[tuple[int, int, int]]:
    up: dict[int, int] = {}
    out: list[tuple[int, int, int]] = []
    for rec in trace.of("aura"):
        if rec.get("d") != guid or rec.get("sp") not in spells:
            continue
        if not rec.get("r"):
            up.setdefault(rec["sp"], rec["t"])
        elif rec["sp"] in up:
            out.append((up.pop(rec["sp"]), rec["t"], rec["sp"]))
    end = max(last_snapshot(trace), max((t for t in up.values()), default=0))
    out += [(t, end, spell) for spell, t in up.items()]
    return out


def covered_ms(spans: list[tuple[int, int, int]], t0: int, t1: int) -> int:
    cut = sorted((max(s, t0), min(e, t1)) for s, e, _ in spans if e > t0 and s < t1)
    total, start, end = 0, None, None
    for s, e in cut:
        if end is None or s > end:
            total += (end - start) if end is not None else 0
            start, end = s, e
        else:
            end = max(end, e)
    return total + ((end - start) if end is not None else 0)


def snaplasher_cover(trace: Trace) -> list[dict]:
    deaths = combat_deaths(trace)
    out: list[dict] = []
    for snap in sorted(sightings(trace, (NPC_SNAPLASHER,)).values(), key=lambda r: r["first"]):
        held: collections.Counter = collections.Counter()
        for s, e, target in snap["spans"]:
            if trace.role(target) == "tank":
                held[target] += e - s
        if not held:
            continue
        tank = max(held, key=held.get)
        t0 = next(span[0] for span in snap["spans"] if span[2] == tank)
        died = next((d["t"] for d in deaths if d.get("g") == tank and t0 < d["t"] <= snap["last"]), None)
        t1 = died if died is not None else snap["last"]
        own, ext = aura_spans(trace, tank, OWN_DEFENSIVES), aura_spans(trace, tank, EXTERNALS)
        cut = [span for span in own if span[2] in OWN_DAMAGE_CUTS]
        own_ms, ext_ms, any_ms = covered_ms(own, t0, t1), covered_ms(ext, t0, t1), covered_ms(own + ext, t0, t1)
        cut_ms, layer_ms = covered_ms(cut, t0, t1), covered_ms(cut + ext, t0, t1)
        under = [{**OWN_DEFENSIVES, **EXTERNALS}[spell] for s, e, spell in sorted(own + ext)
                 if s <= died <= e + DEATH_AURA_SLACK_MS] if died is not None else []
        out.append({"spawn": snap["first"], "tank": tank, "t": t0, "until": t1, "own": own_ms, "ext": ext_ms,
                    "both": own_ms + ext_ms - any_ms, "both_cut": cut_ms + ext_ms - layer_ms,
                    "two_ext": sum(covered_ms([span], t0, t1) for span in ext) - ext_ms,
                    "no_cut": t1 - t0 - layer_ms, "none": t1 - t0 - any_ms, "died": died, "under": under})
    return out


def slower(text: str) -> bool:
    try:
        chaser, own = (float(part) for part in text.split(" vs "))
    except ValueError:
        return False
    return chaser < own


def loose_stretches(trace: Trace) -> list[dict]:
    health: dict[tuple[int, int], float] = {}
    for snap in frames(trace):
        for row in snap.get("u", []):
            if trace.entries.get(row[0]) == NPC_SNAPLASHER:
                health[(snap["t"], row[0])] = row[5]
    out: list[dict] = []
    for rec in sorted(sightings(trace, (NPC_SNAPLASHER,)).values(), key=lambda r: r["first"]):
        stretch = None
        for t0, t1, target in rec["spans"]:
            if trace.role(target) in ("tank", "?"):
                if stretch is not None:
                    stretch["until"] = t0
                stretch = None
                continue
            if stretch is None:
                stretch = {"g": rec["g"], "spawn": rec["first"], "t": t0, "last": t1, "until": t1, "victims": []}
                out.append(stretch)
            stretch["last"] = stretch["until"] = t1
            if target not in stretch["victims"]:
                stretch["victims"].append(target)
    bark = [rec for rec in trace.of("cast") if rec.get("sp") in BARK_STACKS]
    deaths = combat_deaths(trace)
    kites = [rec for rec in trace.of("move") if rec.get("by") == KITE_ACTION and rec.get("ok")]
    reads = [rec for rec in trace.of("note") if rec.get("k") == KITE_NOTE and rec.get("txt")]
    for stretch in out:
        t0, t1, guid, victims = stretch["t"], stretch["until"], stretch["g"], stretch["victims"]
        stretch["from"] = health.get((t0, guid), 0.0)
        stretch["to"] = health.get((stretch["last"], guid), 0.0)
        stretch["deaths"] = sum(1 for d in deaths if d.get("killer") == guid and t0 < d["t"] <= t1)
        stretch["bark"] = sum(1 for c in bark if c.get("s") == guid and t0 <= c["t"] <= t1)
        stretch["kites"] = sum(1 for m in kites if m.get("g") in victims and t0 <= m["t"] <= t1)
        mine_ = [n for n in reads if n.get("g") in victims and t0 <= n["t"] <= t1]
        stretch["reads"] = len(mine_)
        stretch["slower"] = sum(1 for n in mine_ if slower(n["txt"]))
    return sorted((s for s in out if s["until"] - s["t"] >= HOLD_MIN_MS or s["deaths"] or s["kites"]),
                  key=lambda s: (s["t"], s["spawn"]))


def dodge_trigger_yd() -> float | None:
    try:
        return radius("ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS")
    except Unknown:
        return None


def dodges(trace: Trace) -> list[dict]:
    """The first order of each Unstable Sun Beam dodge: who, how far from the nearest beam it started, how long
    a walk it ordered, and whether that walk ended farther from Freya."""
    snaps = frames(trace)
    stamps = [snap["t"] for snap in snaps]
    freya = next((guid for guid, entry in trace.entries.items() if entry == NPC_FREYA), None)
    last: dict[int, int] = {}
    out: list[dict] = []
    for rec in trace.of("move"):
        if rec.get("by") != DODGE_ACTION or not rec.get("ok"):
            continue
        guid = rec.get("g")
        fresh = guid not in last or rec["t"] - last[guid] >= DODGE_EPISODE_MS
        last[guid] = rec["t"]
        index = bisect.bisect_right(stamps, rec["t"]) - 1
        start = at(trace, guid, rec["t"])
        if not fresh or index < 0 or start is None:
            continue
        # A beam lands at a bot's feet and the dodge often goes out before the next snapshot shows it.
        beams = [row[1:3] for snap in snaps[index:index + 2] for row in snap.get("u", [])
                 if trace.entries.get(row[0]) in (NPC_SUN_BEAM, NPC_BRIGHTLEAF_SUN_BEAM)]
        if not beams:
            continue
        dest = (rec.get("x", 0.0), rec.get("y", 0.0))
        her = at(trace, freya, rec["t"]) if freya else None
        out.append({"t": rec["t"], "g": guid, "from": min(dist2(start, beam) for beam in beams),
                    "walk": dist2(start, dest), "away": her is not None and dist2(dest, her) > dist2(start, her)})
    return out


def healers_on_holder(trace: Trace, snaplasher: dict) -> list[int]:
    """Per snapshot of one Snaplasher's life, the living healers within heal range of the player it hits."""
    healers = [guid for guid, role in trace.roles.items() if role == "heal"]
    counts: list[int] = []
    for snap in frames(trace):
        if not snaplasher["first"] <= snap["t"] <= snaplasher["last"]:
            continue
        rows = {row[0]: row for row in snap.get("u", [])}
        me = rows.get(snaplasher["g"])
        holder = rows.get(me[7]) if me else None
        if holder is None or holder[5] <= 0 or trace.role(holder[0]) == "?":
            continue
        counts.append(sum(1 for guid in healers if guid in rows and rows[guid][5] > 0 and
                          dist2(rows[guid][1:3], holder[1:3]) <= HEAL_RANGE_YD))
    return counts


def show_beams(trace: Trace) -> None:
    print("BEAMS")
    if not mine(trace):
        return
    trigger = dodge_trigger_yd()
    found = dodges(trace)
    if not found:
        print("  no Unstable Sun Beam dodge")
    by_role: dict[str, list[dict]] = collections.defaultdict(list)
    for dodge in found:
        by_role[trace.role(dodge["g"])].append(dodge)
    for role, items in sorted(by_role.items()):
        walks = sorted(dodge["walk"] for dodge in items)
        far = "" if trigger is None else (
            f", {sum(1 for dodge in items if dodge['from'] > trigger)} from past {trigger:g} yd")
        away = sum(1 for dodge in items if dodge["away"])
        print(f"  {role:<7} {len(items):3d} dodge(s){far}, median walk {walks[len(walks) // 2]:.1f} yd, "
              f"{away} ended farther from Freya")
    for snaplasher in sorted(sightings(trace, (NPC_SNAPLASHER,)).values(), key=lambda r: r["first"]):
        counts = healers_on_holder(trace, snaplasher)
        if not counts:
            continue
        short = sum(1 for n in counts if n < HEALERS_WANTED)
        print(f"  Snaplasher {clock(snaplasher['first'])} .. {clock(snaplasher['last'])}: healers within "
              f"{HEAL_RANGE_YD:g} yd of its holder median {sorted(counts)[len(counts) // 2]}, under {HEALERS_WANTED} "
              f"for {100.0 * short / len(counts):.0f}% of it")


def held_pack_samples(trace: Trace) -> list[dict]:
    """Per snapshot where ghouls hold the lasher pack's focus, per living ranged DPS: its distance to the nearest
    lasher and whether it was on Freya."""
    pack = radius("ULDUAR_FREYA_LASHER_PACK_RADIUS")
    ghouls = {guid for guid, entry in trace.entries.items() if entry == NPC_ARMY_GHOUL}
    out: list[dict] = []
    for snap in frames(trace):
        rows = [row for row in snap.get("u", []) if row[5] > 0]
        lashers = [row for row in rows if trace.entries.get(row[0]) == NPC_DETONATING_LASHER]
        if not lashers:
            continue
        focus = max(lashers, key=lambda row: (sum(1 for other in lashers if dist2(row[1:3], other[1:3]) <= pack),
                                              -row[0]))
        held = focus[7] in ghouls or any(row[0] in ghouls and row[7] == focus[0] for row in rows)
        if not held:
            continue
        for row in rows:
            if trace.role(row[0]) != "ranged":
                continue
            out.append({"t": snap["t"], "g": row[0], "near": min(dist2(row[1:3], l[1:3]) for l in lashers),
                        "freya": trace.entries.get(row[7]) == NPC_FREYA})
    return out


def show_ghouls(trace: Trace) -> None:
    print("GHOULS")
    if not mine(trace):
        return
    samples = held_pack_samples(trace)
    if not samples:
        print("  ghouls never held the pack focus")
        return
    clear = radius("ULDUAR_FREYA_LASHER_CAMP_STANDOFF")
    walks = [rec for rec in trace.of("move") if rec.get("by") == REACH_ACTION and rec.get("ok")]
    for wave in waves(trace):
        if wave["kind"] != "lashers":
            continue
        mine_ = [sample for sample in samples if wave["t"] <= sample["t"] <= wave["end"]]
        if not mine_:
            print(f"  {clock(wave['t'])} lashers: ghouls never held the focus")
            continue
        total = len(mine_)
        far = sum(1 for sample in mine_ if sample["near"] > SPELL_RANGE_YD)
        freya = sum(1 for sample in mine_ if sample["freya"])
        close = sum(1 for sample in mine_ if sample["near"] < clear)
        walked = sum(1 for rec in walks if wave["t"] <= rec["t"] <= wave["end"])
        print(f"  {clock(wave['t'])} lashers, held {clock(mine_[0]['t'])} .. {clock(mine_[-1]['t'])}: of {total} "
              f"ranged DPS samples, no lasher within {SPELL_RANGE_YD:g} yd {100.0 * far / total:.0f}%, on Freya "
              f"{100.0 * freya / total:.0f}%, inside {clear:g} yd of one {100.0 * close / total:.0f}%; "
              f"{walked} '{REACH_ACTION}' order(s)")


def low_lashers(snap: dict, trace: Trace, low_pct: float) -> list:
    return [row[1:3] for row in snap.get("u", [])
            if trace.entries.get(row[0]) == NPC_DETONATING_LASHER and 0 < row[5] < low_pct]


def blast_counts(trace: Trace, wave: dict) -> dict:
    """One lasher wave: Detonate hits and deaths on the melee, their approach orders aimed into a low lasher's
    blast, how much of the wave they stood in one, and their bail orders away from a low lasher that had that bot
    as its victim (it follows, so the bail never clears it), split off by whether a trio member or the Conservator
    was alive."""
    low_pct = radius("ULDUAR_FREYA_LASHER_BAIL_PCT")
    edge = radius("ULDUAR_FREYA_DETONATE_RADIUS") + 1.0
    snaps = frames(trace)
    stamps = [snap["t"] for snap in snaps]
    end = wave["end"] + BLAST_TAIL_MS
    melee = {guid for guid, role in trace.roles.items() if role == "melee"}
    hits = [rec for rec in trace.of("dmg")
            if rec.get("sp") in DETONATE and rec.get("d") in melee and wave["t"] <= rec["t"] <= end]
    deaths = sum(1 for death in combat_deaths(trace)
                 if death["g"] in melee and wave["t"] <= death["t"] <= end
                 and (sorted(death.get("rewind") or [], key=lambda row: row[0]) or [[0, 0, 0]])[-1][2] in DETONATE)
    orders = into = 0
    for rec in trace.of("move"):
        if (not rec.get("ok") or rec.get("by") not in APPROACH_ACTIONS or rec.get("g") not in melee
                or not wave["t"] <= rec["t"] <= end):
            continue
        orders += 1
        index = bisect.bisect_right(stamps, rec["t"]) - 1
        dest = (rec.get("x", 0.0), rec.get("y", 0.0))
        if index >= 0 and any(dist2(dest, low) < edge for low in low_lashers(snaps[index], trace, low_pct)):
            into += 1
    samples = inside = 0
    for snap in snaps:
        if not wave["t"] <= snap["t"] <= end:
            continue
        lows = low_lashers(snap, trace, low_pct)
        for row in snap.get("u", []):
            if row[0] in melee and row[5] > 0:
                samples += 1
                inside += any(dist2(row[1:3], low) <= DETONATE_REACH_YD for low in lows)
    bails = chased = lone = 0
    for rec in trace.of("move"):
        if (not rec.get("ok") or rec.get("by") != BAIL_ACTION or rec.get("g") not in melee
                or not wave["t"] <= rec["t"] <= end):
            continue
        bails += 1
        index = bisect.bisect_right(stamps, rec["t"]) - 1
        rows = snaps[index].get("u", []) if index >= 0 else []
        me = next((row for row in rows if row[0] == rec["g"]), None)
        if not me or not any(trace.entries.get(row[0]) == NPC_DETONATING_LASHER and 0 < row[5] < low_pct
                             and row[7] == rec["g"] and dist2(row[1:3], me[1:3]) < edge for row in rows):
            continue
        chased += 1
        lone += not any(trace.entries.get(row[0]) in OUTRANK_LASHERS and row[5] > 0 for row in rows)
    return {"hits": len(hits), "deaths": deaths, "orders": orders, "into": into, "samples": samples,
            "inside": inside, "bails": bails, "chased": chased, "lone": lone,
            "chases": chases(trace, wave, melee, low_pct)}


def chases(trace: Trace, wave: dict, melee: set, low_pct: float) -> list[dict]:
    members = {member["g"] for member in wave["members"]}
    end = wave["end"] + BLAST_TAIL_MS
    open_chases: dict[int, dict] = {}
    out: list[dict] = []
    for snap in frames(trace):
        if not wave["t"] <= snap["t"] <= end:
            continue
        rows = {row[0]: row for row in snap.get("u", [])}
        adds = any(trace.entries.get(row[0]) in OUTRANK_LASHERS and row[5] > 0 for row in rows.values())
        for guid in members:
            row = rows.get(guid)
            victim = row[7] if row and 0 < row[5] < low_pct and row[7] in melee and row[7] in rows else None
            chase = open_chases.get(guid)
            if chase and chase["v"] == victim:
                spot = rows[victim][1:3]
                chase["walk"] += dist2(chase["at"], spot)
                chase["at"], chase["until"] = spot, snap["t"]
                chase["adds"] = chase["adds"] or adds
                continue
            open_chases.pop(guid, None)
            if victim:
                chase = {"v": victim, "t": snap["t"], "until": snap["t"], "at": rows[victim][1:3], "walk": 0.0,
                         "adds": adds}
                open_chases[guid] = chase
                out.append(chase)
    return out


def show_blasts(trace: Trace) -> None:
    print("BLASTS")
    if not mine(trace):
        return
    found = False
    for wave in waves(trace):
        if wave["kind"] != "lashers":
            continue
        found = True
        c = blast_counts(trace, wave)
        share = 100.0 * c["inside"] / c["samples"] if c["samples"] else 0.0
        long = [chase for chase in c["chases"] if chase["until"] - chase["t"] >= CHASE_MS]
        walk = max((chase["walk"] for chase in c["chases"]), default=0.0)
        print(f"  {clock(wave['t'])} lashers: melee took {c['hits']} Detonate hit(s), {c['deaths']} death(s); "
              f"{c['into']} of {c['orders']} approach order(s) into a low lasher's blast; melee inside one "
              f"{share:.0f}% of samples; {c['chased']} of {c['bails']} melee bail order(s) away from a low lasher "
              f"chasing that bot, {c['lone']} with no trio or Conservator up; {len(c['chases'])} chase(s) of a "
              f"melee bot by a low lasher, {len(long)} of them {CHASE_MS // 1000}s+ "
              f"({sum(1 for chase in long if chase['adds'])} with a trio or Conservator up), its victim walking up "
              f"to {walk:.0f} yd")
    if not found:
        print("  no lasher wave")


def low_lasher_counts(trace: Trace, wave: dict) -> dict:
    """One lasher wave: its lashers that went under the bail line and how many stayed there LINGER_MS or more, the
    ranged DPS in reach of a low lasher hitting a player and whether they shot one (or, with Eonar's Gift up, the
    Gift), and the back line's bail orders away from a low lasher hitting one of them. A ranged bot counts once a
    snapshot, however many such lashers it reaches, so shooting the lowest of two is on one, not on another lasher."""
    low_pct = radius("ULDUAR_FREYA_LASHER_BAIL_PCT")
    snaps = frames(trace)
    stamps = [snap["t"] for snap in snaps]
    end = wave["end"] + BLAST_TAIL_MS
    members = {member["g"] for member in wave["members"]}
    spans: dict[int, list[dict]] = {}
    in_reach = on_it = on_other = gift_up = on_gift = 0
    for snap in snaps:
        if not wave["t"] <= snap["t"] <= end:
            continue
        rows = [row for row in snap.get("u", []) if row[5] > 0]
        for row in rows:
            if row[0] not in members:
                continue
            open_spans = spans.setdefault(row[0], [])
            if row[5] >= low_pct:
                if open_spans:
                    open_spans[-1]["closed"] = True
                continue
            if not open_spans or open_spans[-1].get("closed"):
                open_spans.append({"first": snap["t"], "player": 0, "samples": 0})
            span = open_spans[-1]
            span["last"] = snap["t"]
            span["samples"] += 1
            if trace.role(row[7]) != "?":
                span["player"] += 1
        hitting = [row for row in rows if trace.entries.get(row[0]) == NPC_DETONATING_LASHER and row[5] < low_pct
                   and trace.role(row[7]) != "?"]
        gifts = {row[0] for row in rows if trace.entries.get(row[0]) == NPC_EONARS_GIFT}
        for other in rows:
            if trace.role(other[0]) != "ranged":
                continue
            near = {row[0] for row in hitting if dist2(other[1:3], row[1:3]) <= SPELL_RANGE_YD}
            if not near & members:
                continue
            in_reach += 1
            if gifts:
                gift_up += 1
                on_gift += other[7] in gifts
            if other[7] in near:
                on_it += 1
            elif trace.entries.get(other[7]) == NPC_DETONATING_LASHER:
                on_other += 1
    found = [span for lasher in spans.values() for span in lasher]
    lingered = [span for span in found if span["last"] - span["first"] >= LINGER_MS]
    bails = 0
    for rec in trace.of("move"):
        if rec.get("by") != CAMP_ACTION or not rec.get("ok") or not wave["t"] <= rec["t"] <= end:
            continue
        index = bisect.bisect_right(stamps, rec["t"]) - 1
        rows = {row[0]: row for row in snaps[index].get("u", [])} if index >= 0 else {}
        me = rows.get(rec.get("g"))
        bails += bool(me) and any(
            trace.entries.get(row[0]) == NPC_DETONATING_LASHER and 0 < row[5] < low_pct
            and trace.role(row[7]) in ("ranged", "heal") and dist2(row[1:3], me[1:3]) <= DETONATE_REACH_YD
            for row in rows.values())
    return {"low": sum(1 for lasher in spans.values() if lasher), "lingered": len(lingered),
            "on_player": sum(1 for span in lingered if 2 * span["player"] > span["samples"]),
            "in_reach": in_reach, "on_it": on_it, "on_other": on_other, "gift_up": gift_up, "on_gift": on_gift,
            "bails": bails}


def show_lows(trace: Trace) -> None:
    print("LOWS")
    if not mine(trace):
        return
    found = False
    for wave in waves(trace):
        if wave["kind"] != "lashers":
            continue
        found = True
        c = low_lasher_counts(trace, wave)
        reach = c["in_reach"] or 1
        gift = (f", {c['gift_up']} of them with Eonar's Gift up, {100.0 * c['on_gift'] / c['gift_up']:.0f}% on the "
                f"Gift" if c["gift_up"] else "")
        print(f"  {clock(wave['t'])} lashers: {c['low']} under the bail line, {c['lingered']} of them "
              f"{LINGER_MS // 1000}s+ there ({c['on_player']} hitting a player); ranged DPS in reach of a low lasher "
              f"hitting a player: {c['in_reach']} sample(s), {100.0 * c['on_it'] / reach:.0f}% on one, "
              f"{100.0 * c['on_other'] / reach:.0f}% on another lasher{gift}; {c['bails']} back-line bail order(s) "
              f"from one hitting the back line")
    if not found:
        print("  no lasher wave")


def submerge_counts(trace: Trace, wave: dict) -> dict:
    standoff = radius("ULDUAR_FREYA_LASHER_CAMP_STANDOFF")
    snaps = frames(trace)
    stamps = [snap["t"] for snap in snaps]
    members = {member["g"] for member in wave["members"]}
    wake = wave["t"] + SUBMERGE_MS

    def lashers(index: int) -> list:
        rows = snaps[index].get("u", []) if 0 <= index < len(snaps) else []
        return [row[1:3] for row in rows if row[0] in members and row[5] > 0]

    def back_line(index: int) -> list[float]:
        spots = lashers(index)
        if not spots:
            return []
        return [min(dist2(row[1:3], spot) for spot in spots) for row in snaps[index].get("u", [])
                if row[5] > 0 and trace.role(row[0]) in ("ranged", "heal")]

    spawn = back_line(bisect.bisect_left(stamps, wave["t"]))
    woke = back_line(bisect.bisect_right(stamps, wake) - 1)
    orders = clear = 0
    for rec in trace.of("move"):
        if rec.get("by") != CAMP_ACTION or not rec.get("ok") or not wave["t"] <= rec["t"] < wake:
            continue
        spots = lashers(bisect.bisect_right(stamps, rec["t"]) - 1)
        orders += 1
        dest = (rec.get("x", 0.0), rec.get("y", 0.0))
        clear += bool(spots) and all(dist2(dest, spot) >= standoff for spot in spots)
    damage = sum(rec.get("a", 0) for rec in trace.of("dmg")
                 if rec.get("s") in members and rec.get("sp", 0) in (0, FLAME_LASH)
                 and trace.role(rec.get("d")) in ("ranged", "heal") and wake <= rec["t"] < wake + WAKE_WINDOW_MS)
    return {"standoff": standoff, "spawn": spawn, "wake": woke, "orders": orders, "clear": clear,
            "damage": damage, **army_pickup(trace, wave, members, wake)}


def army_pickup(trace: Trace, wave: dict, members: set, wake: int) -> dict:
    army = next((rec for rec in trace.of("cast") if rec.get("sp") == ARMY_OF_THE_DEAD and not rec.get("tr")
                 and trace.role(rec.get("s")) != "?" and wave["t"] - ARMY_LEAD_MS <= rec["t"] <= wave["end"]), None)
    ghouls = {guid for guid, entry in trace.entries.items() if entry == NPC_ARMY_GHOUL}
    first: dict[int, int] = {}
    gaps: list[float] = []
    for snap in frames(trace):
        if not wake <= snap["t"] <= wake + WAKE_PICK_MS:
            continue
        rows = {row[0]: row for row in snap.get("u", [])}
        for guid in members:
            row = rows.get(guid)
            if row and row[5] > 0 and row[7] and guid not in first:
                first[guid] = row[7]
        caster = rows.get(army["s"]) if army else None
        if caster and not gaps:
            gaps = [dist2(row[1:3], caster[1:3]) for row in rows.values()
                    if row[5] > 0 and trace.role(row[0]) in ("ranged", "heal")]
    woke_on = [guid for guid, target in first.items() if trace.role(target) in ("ranged", "heal")]
    up = held = 0
    if army:
        check = max(army["t"] + ARMY_OUT_MS, wake + WAKE_CHECK_MS)
        snap = next((snap for snap in frames(trace) if snap["t"] >= check), None)
        rows = {row[0]: row for row in snap.get("u", [])} if snap else {}
        alive = [rows[guid] for guid in woke_on if guid in rows and rows[guid][5] > 0]
        up, held = len(alive), sum(1 for row in alive if row[7] in ghouls)
    return {"army": army["t"] - wave["t"] if army else None, "dk_gap": statistics.median(gaps) if gaps else None,
            "picked": len(first), "woke_on": len(woke_on), "up": up, "held": held}


def show_submerge(trace: Trace) -> None:
    print("SUBMERGE")
    if not mine(trace):
        return
    found = False
    for wave in waves(trace):
        if wave["kind"] != "lashers":
            continue
        found = True
        c = submerge_counts(trace, wave)
        near = [sum(1 for gap in gaps if gap < c["standoff"]) for gaps in (c["spawn"], c["wake"])]
        medians = " then ".join(f"{statistics.median(gaps):.1f}" if gaps else "-" for gaps in (c["spawn"], c["wake"]))
        if c["army"] is None:
            army = "no Army"
        else:
            gap = "-" if c["dk_gap"] is None else f"{c['dk_gap']:.0f}"
            army = (f"Army {c['army'] / 1000:+.1f}s, the back line a median {gap} yd from its caster at the wake, "
                    f"ghouls on {c['held']} of the {c['up']} still up once they were out")
        print(f"  {clock(wave['t'])} lashers: back line within {c['standoff']:g} yd of a lasher {near[0]} of "
              f"{len(c['spawn'])} at the spawn, {near[1]} of {len(c['wake'])} at the wake (nearest, median {medians} "
              f"yd); {c['orders']} '{CAMP_ACTION}' order(s) in the submerge, {c['clear']} ending {c['standoff']:g} yd "
              f"clear; {c['woke_on']} of {c['picked']} woke onto the back line; {army}; {c['damage'] / 1000:.0f}k "
              f"lasher melee and Flame Lash on the back line in the {WAKE_WINDOW_MS // 1000}s after the wake")
    if not found:
        print("  no lasher wave")


def segment_gap(start, end, point) -> float:
    dx, dy = end[0] - start[0], end[1] - start[1]
    length = dx * dx + dy * dy
    u = 0.0 if not length else max(0.0, min(1.0, ((point[0] - start[0]) * dx + (point[1] - start[1]) * dy) / length))
    return dist2((start[0] + u * dx, start[1] + u * dy), point)


def bomb_counts(trace: Trace) -> dict:
    avoid = radius("ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS")
    bombs = [(guid, rec["first"], at(trace, guid, rec["first"]))
             for guid, rec in sightings(trace, (NPC_NATURE_BOMB,)).items()]
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") in NATURE_BOMB]

    def live(when: int) -> list:
        return [(guid, spot) for guid, first, spot in bombs if spot and first <= when < first + BOMB_FUSE_MS]

    def hit_after(guid: int, when: int, which: set) -> bool:
        return any(hit["d"] == guid and hit.get("s") in which and when <= hit["t"] <= when + BOMB_FUSE_MS
                   for hit in hits)

    orders: collections.Counter = collections.Counter()
    into: collections.Counter = collections.Counter()
    then_hit: collections.Counter = collections.Counter()
    entries: dict[int, list] = {}

    def count(guid: int, when: int, key: tuple, crossed: set) -> None:
        orders[key] += 1
        if crossed:
            into[key] += 1
            then_hit[key] += hit_after(guid, when, crossed)
            entries.setdefault(guid, []).append((when, key[0], crossed))

    for rec in trace.of("move"):
        lit = live(rec["t"]) if rec.get("ok") else []
        here = at(trace, rec.get("g"), rec["t"]) if lit else None
        if here is None or any(dist2(here, spot) < avoid for _, spot in lit):
            continue
        dest = (rec.get("x", 0.0), rec.get("y", 0.0))
        count(rec["g"], rec["t"], (rec.get("by"), trace.role(rec["g"])),
              {guid for guid, spot in lit if segment_gap(here, dest, spot) < avoid})
    for rec in trace.of("cast"):
        name = trace.spells.get(rec.get("sp"))
        lit = live(rec["t"]) if name in CHARGES and not rec.get("tr") else []
        landed = at(trace, rec.get("s"), rec["t"] + CHARGE_LAND_MS) if lit else None
        if landed is None:
            continue
        count(rec["s"], rec["t"], (name.lower(), trace.role(rec["s"])),
              {guid for guid, spot in lit if dist2(landed, spot) < avoid})

    deaths = []
    for death in combat_deaths(trace):
        killers = {row[1] for row in death.get("rewind") or [] if row[2] in NATURE_BOMB}
        if not killers or (sorted(death["rewind"], key=lambda row: row[0])[-1][2] not in NATURE_BOMB):
            continue
        how = [(when, by) for when, by, crossed in entries.get(death["g"], [])
               if crossed & killers and death["t"] - BOMB_FUSE_MS <= when <= death["t"]]
        deaths.append((death, max(how) if how else None))
    return {"bombs": len(bombs), "hits": len(hits), "orders": orders, "into": into, "hit": then_hit,
            "deaths": deaths, "escapes": bomb_escapes(trace, bombs)}


def bomb_escapes(trace: Trace, bombs: list) -> collections.Counter:
    snaps = {snap["t"]: snap for snap in frames(trace)}
    moves: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("by") == BOMB_ESCAPE_ACTION and rec.get("ok"):
            moves[rec["g"]].append(rec["t"])
    last_on: dict[int, int] = {}
    out: collections.Counter = collections.Counter()
    for _, first, spot in sorted(bombs, key=lambda bomb: bomb[1]):
        snap = snaps.get(first)
        if not spot or not snap:
            continue
        for row in snap.get("u", []):
            if row[5] <= 0 or trace.role(row[0]) in ("?", "tank") or dist2(row[1:3], spot) > ON_BOMB_YD:
                continue
            if first - last_on.get(row[0], -BOMB_FUSE_MS) < BOMB_FUSE_MS:
                continue
            last_on[row[0]] = first
            went = next((max(0, t - first) for t in moves[row[0]]
                         if first - ESCAPE_EARLY_MS <= t <= first + BOMB_FUSE_MS), None)
            out["landed"] += 1
            if went is None:
                out["never"] += 1
            elif went <= ESCAPE_QUICK_MS:
                out["quick"] += 1
            elif went >= ESCAPE_LATE_MS:
                out["late"] += 1
            else:
                out["between"] += 1
    return out


def show_bombs(trace: Trace) -> None:
    print("BOMBS")
    if not mine(trace):
        return
    c = bomb_counts(trace)
    if not c["bombs"]:
        print("  no Nature Bomb")
        return
    print(f"  {c['bombs']} Nature Bomb(s), {c['hits']} hit(s) on the raid, {len(c['deaths'])} death(s) to one")
    e = c["escapes"]
    if e["landed"]:
        print(f"  {e['landed']} landed at a non-tank's feet: escape ordered within {ESCAPE_QUICK_MS / 1000:g}s "
              f"{e['quick']}, later {e['between']}, from {ESCAPE_LATE_MS / 1000:g}s on {e['late']}, never {e['never']}")
    for key in sorted(c["into"], key=lambda key: (-c["into"][key], key)):
        print(f"  {key[0]} ({key[1]}): {c['into'][key]} of {c['orders'][key]} into a live bomb, "
              f"{c['hit'][key]} then hit by it")
    for death, how in c["deaths"]:
        line = f"  {clock(death['t'])} {holder_label(trace, death['g'])} died to one: "
        print(line + (f"walked in, {how[1]} at {clock(how[0])}" if how else "stood in it"))


def mechanic(trace: Trace, death: dict) -> str:
    own = own_blow_spell(death)
    if own:
        return f"own {trace.spells.get(own, f'spell {own}')}"
    rewind = sorted(death.get("rewind") or [], key=lambda row: row[0])
    spell_id = rewind[-1][2] if rewind else 0
    spell = trace.spells.get(spell_id, f"spell {spell_id}") if spell_id else "melee"
    return f"{trace.name(death.get('killer'))} {spell}"


def show_deaths(trace: Trace) -> None:
    print("DEATHS BY MECHANIC")
    if not mine(trace):
        return
    counts = collections.Counter(mechanic(trace, death) for death in combat_deaths(trace))
    if not counts:
        print("  none")
    for name, n in counts.most_common():
        print(f"  {n:3d}  {name}")


SECTIONS = (
    ("waves", "each add wave and Eonar's Gift: when it came, when it ended, raid deaths meanwhile", show_waves),
    ("tanks", "who held Freya and each Snaplasher, the add tank's cover under it, every area taunt or Righteous "
     "Defense, each loose Snaplasher, each lasher pile on the add tank", show_tanks),
    ("beams", "Unstable Sun Beam dodges per role, and healers in range of the Snaplasher's holder", show_beams),
    ("ghouls", "while ghouls hold the lasher pack, ranged DPS out of reach, on Freya, or too close", show_ghouls),
    ("blasts", "per lasher wave, Detonate on the melee, their approach orders into a low lasher's blast, their "
     "bails from one chasing them and the chases", show_blasts),
    ("lows", "per lasher wave, lashers left under the bail line, what the ranged in reach shot, back-line bails",
     show_lows),
    ("submerge", "per lasher wave, the back line near the pack at its spawn and wake, the camp's orders meanwhile, "
     "the Army's ghouls on the lashers that woke onto it, and lasher damage on it after the wake", show_submerge),
    ("bombs", "Nature Bomb walks and charges into a live bomb per action, and how each bomb death got there",
     show_bombs),
    ("deaths", "deaths by killer and spell", show_deaths),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
