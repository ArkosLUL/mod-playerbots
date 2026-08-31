# Vezax — telemetry for the first traced pull

## Context

The Vezax formation rework shipped in `5ecdb8633` (north-facing blocks, healers parked inside the
12.5 yd Shadow Crash exclusion, per-bot missile dodge, vapor handling by mana, renumbered ladder). It
has **never been compiled or run**, and no trace of a Vezax pull exists — every `vezax.*` note in the
files on disk is the 23-row session-open dump of stored probes from an unrelated encounter.

The rework rests on claims that only a pull can settle: that no Shadow Crash ever selects a healer or
a melee, that the two ranged groups each fit under one field, that the tank holds the anchor so every
boss-relative radius stays true, and that the mana economy sources itself from vapor puddles. This
plan adds the minimum telemetry that lets the **first** pull answer those questions from the trace
alone, instead of needing a second pull to work out why the first was unreadable.

Scope is telemetry only. No behaviour changes. Hard mode (Stage 3) stays out of scope.

---

## What the existing streams already answer — do not re-probe these

Verified against the recorder, not assumed. Adding notes for any of these would duplicate a stream
that already latches them, which `docs/systems/observability.md` calls out explicitly.

| Question | Already answered by | Why it works |
|---|---|---|
| Did a crash ever target a healer or melee? | `cast` rows, `sp` 62660, `tgt` | `NoteCast` accepts a caster in `s.watched` ([RaidObs.cpp:1732](../../../src/Bot/Obs/RaidObs.cpp#L1732)), and Vezax is watched the moment he swings at a player ([RaidObs.cpp:1506-1519](../../../src/Bot/Obs/RaidObs.cpp#L1506)). The hook is `ALLSPELLHOOK_ON_PREPARE`, so an instant cast still emits. Join `tgt` against `hdr.roster[].r` |
| Where did Vezax stand? | `snap.u` | `IsDungeonBoss()` gives him an eviction-proof watched slot ([RaidObs.cpp:838](../../../src/Bot/Obs/RaidObs.cpp#L838)) |
| Did the block hold? Did the far group cross? | `snap.u` at 250 ms + `vezax.slot` | positions plus the stored assignment map |
| Where did the field land, and who was under it? | `snap.hz` | 63277 is `Effect 27` `PERSISTENT_AREA_AURA` — a real `DynamicObject`, so `SweepArea` finds it |
| Did the bot clear the missile in time? | `haz` circle + `snap` | already emitted per cast in `UldBotScripts.cpp` |
| Dodge/soak oscillation once the field is down | `act` verdicts with `repeats` | the pass-repeat count *is* the oscillation signal |
| Mana over time; who dipped under 10 | `snap.u` mana% | sampled every 250 ms |
| Was Searing Flames kicked, and with one kick? | `cast` 62661 + `act` on the interrupt + `vezax.interrupter` | a kicked cast leaves no 62661 `dmg` rows |
| Who rode a puddle, at what stack, for how long | `aura` 63322 rows and `death.auras` | stacks and `removedT` are both carried |
| Do assignments survive a wipe and re-pull? | `vezax.slot` erase emissions | `ObsGuidMap::erase(guid)` emits |
| Live vapors' positions | `snap.u` | `SweepArea` appends alive hostile creatures ([RaidObs.cpp:732-747](../../../src/Bot/Obs/RaidObs.cpp#L732)) |

Also verified while here: `ULD_DATA_VEZAX = 11` matches `BOSS_VEZAX = 11` in the core's
`ulduar.h:42`, so the mirrored constant is right and `GetCreature` will resolve.

---

## The four gaps

### G1 — The vapor puddle is invisible in the trace

**63322 is `Effect_1 = 6` (`APPLY_AURA`) with `EffectAura_1 = 4` (`PERIODIC_DAMAGE`), not
`Effect 27`.** It creates no `DynamicObject`, so `SweepArea` can never put it in `snap.hz`. The corpse
carrying it is dropped from `snap.u` by `PruneWatched`, which erases anything `!IsAlive()`
([RaidObs.cpp:890-899](../../../src/Bot/Obs/RaidObs.cpp#L890)). Nothing in the trace records where a
puddle is.

This is the exact case `NoteHazard` exists for, and it matters for three verification questions: did
the handler drop the puddle at its own feet or 30 yd away, did the puddle cover the tank and the melee
stack, and was the formation forced to displace around one.

**Fix** — [UldBotScripts.cpp](../../../src/Ai/Raid/Uld/Util/UldBotScripts.cpp), the existing 63323
branch, which already fires exactly once per puddle, server-side, at the corpse:

```cpp
if (spellInfo->Id == SPELL_VEZAX_SARONITE_VAPORS_SPAWN)
{
    Position const puddle = caster->GetPosition();
    InterruptVezaxCastersNear(caster, puddle, ULDUAR_VEZAX_HAZARD_RADIUS,
                              &GainsNothingFromVaporPuddle);

    // 63322 applies a periodic aura rather than a persistent area aura, so it is never a
    // DynamicObject for the snapshot sweep to find, and the corpse holding it drops out of the
    // watched set the moment it dies. Without this the puddle is nowhere in the trace.
    RaidObs::NoteHazardCircle(caster->GetMap(), SPELL_VEZAX_SARONITE_VAPORS_PUDDLE, puddle,
                              ULDUAR_VEZAX_HAZARD_RADIUS,
                              static_cast<uint32>(std::max(0, spellInfo->GetDuration())));
}
```

Radius index 14 = **8 yd**, which is `ULDUAR_VEZAX_HAZARD_RADIUS` — reuse the constant, do not
hardcode. 63323's duration index 9 = **30000 ms**, the corpse aura's life and therefore the puddle's;
read it from `spellInfo` rather than writing 30000.

No `if (RaidObs::Active())` wrapper, unlike the missile call above it: that one computes a distance
and a division before the call, this one passes a field read. `NoteHazardCircle` gates internally.

Needs `<algorithm>` for `std::max` if not already included.

**Known limit, state it in the doc:** only `snap.hz` feeds the death block's containment test, so a
`haz` row reaches the timeline and never produces a `STOOD IN` line. A puddle death is explained by
the 63322 rows in `death.auras` instead, which carry stacks and `removedT`.

### G2 — The dodge picks a destination and never says which rule produced it

`TryGetVezaxDodgeSpot` ([UldEncounter_Vezax.cpp:659](../../../src/Ai/Raid/Uld/Util/UldEncounter_Vezax.cpp#L659))
has four outcomes and the `move` record distinguishes none of them: it carries the coordinate and the
owning action, not the branch. The reverted-clamp case especially — the band is abandoned because the
clamped spot was still inside the blast — looks identical to a normal dodge in every stream.

This is the newest and least-exercised code in the rework, and `observability.md` names this pattern
directly: *probe the rule, not the coordinate*, as `hodir.shuttle` does.

**Fix** — one `vezax.dodge` probe inside the helper (single call site today, but the helper is still
the right home), emitted on every return so `none` is recorded too:

| value | meaning |
|---|---|
| `none` | `FindNearestPositionClearOfHazards` found nothing; the action fails |
| `heal` | clamped into the healer band, ≤12 yd from the live boss |
| `band` | clamped into the ranged band, 16-36 yd from the anchor |
| `blast` | clamp reverted — the clamped spot was still inside the 10 yd impact |

Values are ≤5 chars, so the `std::string const&` parameter hits SSO and needs no `Active()` guard,
same cost class as the existing `"1"`/`"0"` probes. The helper only runs while a missile is inbound
and the bot is in its footprint, so it is not a hot path either way.

### G3 — The formation gate reports `0` for four different reasons

`VezaxFormationActive` ([UldEncounter_Vezax.cpp:201](../../../src/Ai/Raid/Uld/Util/UldEncounter_Vezax.cpp#L201))
is the gate for two triggers and the movement multiplier, and a flat `0` cannot separate *the bot has
not walked into the room yet* — correct and expected, the multiplier is meant to be inert out there —
from *the boss lookup came back empty*, which silently disables the entire strategy while looking
identical in every other stream. On a first pull that distinction is the difference between one
diagnostic pass and three.

**Fix** — replace the `"1"`/`"0"` payload with the reason. Same probe, same site, same cost:

```cpp
char const* reason = "on";
if (!inRoom)
    reason = "outside";
else if (!vezax)
    reason = "noboss";
else if (!active)
    reason = "idle";

RaidObs::NoteDerived(bot, "vezax.formation", reason);
```

No `SCHEMA_VERSION` bump: `note.txt` is free text the recorder does not inspect, not a schema value.

### G4 — A bot with no slot leaves no record

`TryGetVezaxSlot` has three outcomes that break the formation and none of them is written:

- **unslotted** — `EnsureVezaxSlotAssignments` could not place the bot, because every suitable slot
  is taken (7+ healers, or 19+ non-healer ranged). Verification step 9 asks for exactly this and today
  it is invisible.
- **loose** — every slot is buried under a hazard, so the bot steps to an arbitrary clear point that
  the formation knows nothing about.
- **stuck** — even that found nothing and the action fails.

The main tank's anchor slot is also unprobed, and *the tank standing on the spawn point* is what holds
Vezax still for every boss-relative radius in the design.

**Fix** — extend the existing `vezax.block` vocabulary rather than adding a key, so one note answers
"where does this bot think it belongs" and transitions read as `left` → `loose` → `left`.

**Move the probe.** It currently sits mid-function, right after the assignment lookup succeeds. Emit
it at each final return instead — otherwise a displaced bot writes `left` then `loose` in the same
call and flaps every tick. Five one-line emits, no flap:

| return site | value |
|---|---|
| main-tank anchor | `tank` |
| assignment lookup fails | `unslotted` |
| own slot is clear | block name (existing) |
| displaced to another slot | block name **of the displaced slot**, which is what the bot is actually walking to; `vezax.displaced` already carries the index |
| every slot buried, stepped off the hazard | `loose` |
| nowhere clear at all | `stuck` |

Leave the `!VezaxTakesSlot` return (melee and off-tanks) unprobed — they have no slot by design and
`hdr.roster[].r` already says so.

---

## Deliberately not added

Each of these was considered and rejected, so a later session does not re-add them:

- **Shadow Crash target role** — `cast.tgt` joined against the roster answers it.
- **Interrupt outcome** — `act` OK plus the absence of 62661 `dmg` rows answers it.
- **Boss position / vapor positions** — both are watched units in `snap.u`.
- **Per-tick soak and position declines** — an action that declines is a verdict in the `act` stream;
  a note per attempt re-adds what that stream already latches.
- **10-man under-fill order** — `vezax.slot` indices plus `snap.u` answer it.

## Files

| File | Change |
|---|---|
| [UldBotScripts.cpp](../../../src/Ai/Raid/Uld/Util/UldBotScripts.cpp) | G1: `NoteHazardCircle` for the puddle in the 63323 branch |
| [UldEncounter_Vezax.cpp](../../../src/Ai/Raid/Uld/Util/UldEncounter_Vezax.cpp) | G2 `vezax.dodge`; G3 `vezax.formation` reasons; G4 move and extend `vezax.block` |
| [docs/raids/ulduar.md](../../../docs/raids/ulduar.md) | Update the trace paragraph of the Vezax chapter: the new note vocabulary, the puddle hazard row, and the `haz`-never-produces-`STOOD IN` limit |

No reader change — `postmortem.py --notes vezax.` already filters on the prefix and renders `haz`
rows with the spell name. No `SCHEMA_VERSION` / `SUPPORTED_SCHEMA` bump.

## Verification

1. `py -3 apps/codestyle/codestyle-cpp.py` on the two touched sources. Pre-existing hits under
   `src/server/` and `src/tools/` are not ours.
2. Build is a hand-off — the module cannot be compiled headless here.
3. Confirm the obs knobs are live before pulling:
   `docker exec ac-worldserver env | grep ^AC_AI_PLAYERBOT_OBS_` — never trust the `.conf`.
4. **25-man pull, then read the trace only.** Each row below must be answerable without a second pull:

| Question | Command |
|---|---|
| Did the gate ever fail for the wrong reason? | `--notes vezax.formation` — `outside` before arrival is correct; any `noboss` is a bug |
| Was anyone unslotted or loose? | `--notes vezax.block` — `tank` once, no `unslotted`/`stuck`, `loose` only while a field covers a slot |
| Did a crash ever pick a healer or melee? | `grep '"sp":62660' | join tgt against the roster` — the whole basis of the inner ring |
| Did the dodge fire, and on which rule? | `--notes vezax.dodge` — `none` means the search radius is too small; heavy `blast` means the bands are wrong |
| Did the field land on the vacated footprint? | `haz` circle vs the next `snap.hz` 63277 row |
| Did the mana economy source itself? | `--notes vezax.handler` against `snap.u` mana%, and one `haz` 63322 row per vapor death |
| Did the puddle drop at the handler's feet? | `haz` 63322 position vs that bot's `snap` position at the same `t` |
| Did anyone else ride one? | `aura` 63322 rows for bots that are neither handler nor under `LowMana` (15) |
| Did Vezax hold the anchor? | boss row in `snap.u` vs `(1852.78, 81.39)` |
5. **Wipe and re-pull twice** — `vezax.slot` must show a full erase between pulls, no assignment
   carried over.
6. **10-man** — 15 s Searing Flames cadence, blocks under-fill from each row's centre outward, no
   `unslotted`.
