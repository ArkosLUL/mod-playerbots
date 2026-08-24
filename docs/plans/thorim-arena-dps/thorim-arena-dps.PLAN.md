# Thorim phase 1: direct add targeting, and stop the arena squad leaking bots

## Context

Two 25-man Thorim attempts both wiped in phase 1. Traces:

- `env/dist/logs/botobs/603_3_thorim_1787596597.ndjson` — wipe at 3:57. Arena squad wholly dead by
  2:50; Thorim dropped to the floor at 3:06 with nobody left to meet him.
- `env/dist/logs/botobs/603_3_thorim_1787596946.ndjson` — wipe at 2:35. Phase 1 never ended. The arena
  emptied at ~2:33 and `boss_thorim.cpp` summoned the Lightning Orb, which did 291k in 12 hits and
  finished the gauntlet group.

Read with `python modules/mod-playerbots/tools/botobs/postmortem.py <file> [--bot NAME|--track NAME]`.

The reported symptom — "bots on arena don't deal enough damage, Nightwarrior just stayed in place" —
reproduces in both traces. It is not a rotation problem. Arena bots are walked out of the arena,
frozen on unreachable anchors, or **deadlocked onto Thorim himself** by the encounter's own code.

Damage taken is concentrated in Dark Rune Champion (25-26%) and Warbringer (13-14%), which reach 6
alive each by the end of attempt 1 while arena hostiles grow 10 → 40. Commoners pile to 19 but deal so
little they never enter the top-8 damage sources — they are noise, not a target priority.

### The four confirmed failures

**A. Bots deadlock onto Thorim on his balcony.** At 25.3s in attempt 2 Hellflame's target died,
`dps assist` picked Thorim, and the next tick:

```
0:25.578  VETO   thorim arena target guard killed reach spell
```

`ThorimArenaTargetGuardMultiplier` zeroes `AttackAction` and `ReachTargetAction` whenever the current
target is outside the arena box, and Thorim's balcony (z 438.3) is above the box `MAX_Z` of 425. But
`DpsAssistAction : AttackAction`, so the guard also kills the one action that could switch away. The
multiplier's comment — "Gated on the current target, so switching to anything inside the arena
releases this for free" — is wrong. The trace shows the guard killing `dps assist` **30 times** in
attempt 2.

Hellflame's victim went `Captured Mercenary Captain → Thorim` at 25.4s and **never changed again**
until death at 144s. Cost, attempt 2:

| bot | victim = Thorim | casts thrown at Thorim | total casts |
|---|---|---|---|
| Hellflame | 82% of samples | 157 | 303 |
| Smartface | 81% | 126 | 343 |
| Prayer | 81% | 32 | 510 |

Over half of Hellflame's entire output went into an invulnerable phase-1 boss.

**B. The Runic Smash dodge is not squad-gated.** `ThorimRunicSmashTrigger::IsActive()` checks the
bot's z-height but never its squad, and `ThorimResolveGauntletIndex` falls back to the master's
waypoint — and the master is in the corridor. Nightwarrior, attempt 2:

```
1:17.643  move point -> (2242.12, -310.15, 412.13) by 'thorim runic smash action' [ok, combat]
```

A corridor waypoint 110 yd outside the arena. It ended parked at the east gate (2171.0, -269.0) with
`step 0.00` for the last 60 s, regenerating 87% → 99% because nothing could reach it, then died.
Arena-squad bots issuing corridor moves: 8 in attempt 1 (including the tank Bulwark, 31 times), 5 in
attempt 2.

**C. Arena ring anchors land outside the leash and freeze the bot.** `GetThorimArenaRingSlot` ends with
`CheckCollisionAndGetValidCoords`, which drags the ring point back toward the bot when the line
collides. For a bot outside the pit the result is roughly its own position — against a ring radius of
10 / 14 yd:

| attempt | bot | anchor | yd from arena centre |
|---|---|---|---|
| 1 | Smartface | (2182.3, -267.8) | 47.5 |
| 1 | Stormweaver | (2130.0, -222.2) | 41.2 |
| 1 | Holylight | (2171.2, -269.1) | 36.7 |
| 2 | Holylight | (2140.1, -220.6) | 42.8 |
| 2 | Nightwarrior | (2171.0, -269.0) | 36.5 |

Attempt 1, Holylight at (2176.5, -268.1) is handed an anchor 4.2 yd away and 36.7 yd from centre —
the clamp, caught directly. What follows: `ThorimArenaAnchorNeedsMove` sees distance ≈ 0 and latches
`arenaAnchorArrived`; `ThorimArenaAnchorSettled` then lets the anchor guard zero every generic mover;
the spot is past the 30 yd `ULDUAR_THORIM_ARENA_LEASH_RADIUS` so `ThorimArenaLeashMultiplier` zeroes
every `MovementAction` including `dps assist` (veto `thorim arena leash killed dps assist` ×30); and
`ThorimArenaLeashAction` re-derives the same bad anchor, so its move reports `there`. Stormweaver,
attempt 1: **0% of samples with a target, 0 yd travelled, 64 casts, the entire fight.**

**D. The arena can lose its only tank.** Attempt 1 kept Bulwark in the arena (median 0.8 yd from
centre); attempt 2 sent it down the corridor (median 105 yd) and the arena fought 14-bot with no tank.

## Scope

Six fixes. **Burst cooldowns are explicitly out of scope** — do not touch
`HoldBurstUntilTankEngagedMultiplier` or `BurstOnBossOnly`, even though the traces show ~500
suppression episodes on the arena squad in attempt 1.

The user is verifying effective `AC_*` config separately (`docker exec ac-worldserver env | grep ^AC_`).
Nothing here depends on a conf value.

---

## Fix 1 — Replace icon marking with direct targeting

This is the centrepiece. Delete the raid-icon path and resolve the DPS target in the action, the way
SWP does.

**Why icons have to go.** `DpsTargetValue::Calculate()`
(`src/Ai/Base/Value/DpsTargetValue.cpp:281`) calls `RtiTargetValue::Calculate()` first and returns it
unconditionally on a hit — alive, LOS and sight-distance are the only checks. Separately,
`FindTargetStrategy::IsHighPriority` (`src/Ai/Base/Value/TargetValue.cpp:124`) returns true for the
skull icon *and* for anything in `prioritized targets`, which trips `foundHighPriority` and
short-circuits every smart strategy. `prioritized targets` is written by `AttackMyTargetAction`
(`src/Ai/Base/Actions/AttackAction.cpp:47`) and `AttackRtiTargetAction`
(`src/Ai/Base/Actions/ChooseTargetActions.cpp:168`) and is cleared only by `PlayerbotAI::Reset`, i.e.
on leaving combat. Inside one pull the mark never lets go, and a wrong mark is unrecoverable.

Bots do still attack without a skull — `DpsTargetValue` falls through to
`CasterFindTargetSmartStrategy` / `ComboFindTargetSmartStrategy` / `GeneralFindTargetSmartStrategy`
over the `attackers` list — so removing the marking loses nothing. What it buys is a target choice
this encounter controls, that can exclude Thorim outright.

**Model to copy:** `MuruSetDpsPriorityAction` (`src/Ai/Raid/SWP/Action/SWPActions_Muru.cpp:181`), its
resolver `ResolveMuruDpsTarget` (`:212`), its anti-flap picker `SelectMuruEncounterTarget` (same file),
and the entry sweep `GatherMuruEncounterTargets`
(`src/Ai/Raid/SWP/Util/SWPEncounter_Muru.cpp:92`), which walks `"possible targets no los"` and buckets
by `GetEntry()`. `EredarTwinsDpsPrioritizeLadySacrolashAction` is the shorter version of the same
shape.

**Delete:** `ThorimMarkDpsTargetAction` (`src/Ai/Raid/Uld/Action/UldActions_Thorim.{h,cpp}`) and
`ThorimMarkDpsTargetTrigger` (`src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.{h,cpp}`), plus their
context and `UldStrategy.cpp:452` registrations. Note `ThorimMarkDpsTargetTrigger::IsActive()` calls
`SetTargetIcon` — a trigger mutating raid state — which is reason enough on its own.

**Add:**

1. `GatherThorimEncounterTargets(PlayerbotAI*, ThorimEncounterTargets&)` in
   `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp`, sweeping `"possible targets no los"` and bucketing
   by entry. All constants already exist in `src/Ai/Raid/Uld/Util/UldBossHelper.h:126-141`:
   `NPC_DARK_RUNE_ACOLYTE_I` 32886, `NPC_DARK_RUNE_ACOLYTE_G` 33110, `NPC_DARK_RUNE_EVOKER` 32878,
   `NPC_DARK_RUNE_CHAMPION` 32876, `NPC_DARK_RUNE_WARBRINGER` 32877, `NPC_DARK_RUNE_COMMONER` 32904,
   `NPC_RUNIC_COLOSSUS` 32872, `NPC_ANCIENT_RUNE_GIANT` 32873, `NPC_IRON_RING_GUARD` 32874,
   `NPC_IRON_HONOR_GUARD` 32875.

   Match by entry, never `AI_VALUE2(Unit*, "find target", "<name>")`. The old code used the latter
   throughout, and `UldEncounter_Thorim.h:97` already warns why: it walks only the bot's own threat
   list and matches a localized name, so a bot on an add goes blind. `GetThorim` is already the
   by-entry form and should be reused.

2. `ThorimArenaDpsPriorityAction : AttackAction` in `UldActions_Thorim.{h,cpp}`, wired in
   `UldStrategy.cpp` at `ACTION_RAID` (60.0, above `dps assist`'s 50.0), covering both squads:

   - **Arena squad, phase 1** — Acolyte → Evoker → Champion → Warbringer → Commoner. Acolyte and
     Evoker first because they heal and shield the wave; Champion and Warbringer next because they are
     39% of all damage the raid takes. Commoner last and only when nothing else is up.
   - **Gauntlet squad** — Acolyte → Iron Ring Guard / Iron Honor Guard → Runic Colossus → Ancient Rune
     Giant.
   - **Phase 2** (`GetThorim(botAI)->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD`) — Thorim.

   **Never return Thorim or Sif while Thorim is at or above `ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD`
   (429.6094).** This is the whole point of the change; failure A is exactly this.

   Gate candidates on `ThorimInArenaBox` for the arena squad so a bot cannot lock onto a corridor add,
   and keep a range cap in the spirit of the old 50 yd check.

   Anti-flap: mirror `SelectMuruEncounterTarget` — hold the current target while it is alive and of
   the wanted entry, and only switch when a candidate is better by a clear margin (distance from
   `ULDUAR_THORIM_NEAR_ARENA_CENTER` for the arena, from the bot for the corridor). Without this the
   action re-picks every tick and the bot never finishes a cast.

3. **Clear the stale marks once per pull.** Because `prioritized targets` and the group icons survive
   inside a combat, a bot already pinned to Thorim stays pinned even after this change lands. In
   `ThorimResetEncounterStateAction`, and once per pull latched behind a new `ObsValue<bool>` on
   `ThorimEncounterState` (so only one bot does it), clear skull / cross / moon via `SetTargetIcon(...,
   ObjectGuid::Empty)` and reset the bot's own `prioritized targets`. Verify against the schema in
   `docs/systems/observability.md` if you add a note for it.

## Fix 2 — Stop the arena target guard from killing target selection

**File:** `src/Ai/Raid/Uld/UldMultipliers.cpp`, `ThorimArenaTargetGuardMultiplier::GetValue`

Failure A's mechanism. The guard must zero the actions that *act on* a bad target, never the ones that
*choose* a target. Exempt the `ChooseTargetActions` family — `DpsAssistAction`, `DpsAoeAction`,
`TankAssistAction`, `AggressiveTargetAction`, `AttackAnythingAction` — and keep zeroing
`ReachTargetAction`, melee and casts. Fix 1 makes the arena squad stop picking Thorim in the first
place; this makes the guard non-trapping for every other case.

Apply the same reading to `ThorimArenaLeashMultiplier`, which zeroes every `MovementAction` and so
takes `dps assist` with it (veto `thorim arena leash killed dps assist` ×30 in attempt 1). A bot
outside the leash should still be allowed to pick a target while it walks back.

## Fix 3 — Squad-gate the Runic Smash dodge

**File:** `src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.cpp`, `ThorimRunicSmashTrigger::IsActive()`

Add, after the existing z check:

```cpp
if (GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
    return false;
```

The node runs at `ACTION_RAID + 3` (63), above arena positioning (60), so it wins the movement gate —
which is why failure B reaches the mover at all. While in the file, audit
`ThorimGauntletPositioningTrigger` and `ThorimRunicBarrierBailTrigger` for the same omission and add
the gate where missing; `ThorimBarrierBailLatched` keys off the Colossus so it may already be safe,
but confirm rather than assume.

Do not rely on the existing guards to catch this: `thorim arena leash killed thorim runic smash
action` (14) and `thorim arena anchor guard killed thorim runic smash action` (18) only fire once the
bot is already breached or settled, which is too late.

## Fix 4 — Reject arena ring anchors that fall outside the leash

**File:** `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp` — `GetThorimArenaRingSlot`,
`ThorimArenaAnchorNeedsMove`

Failure C. Two changes:

- In `GetThorimArenaRingSlot`, after `CheckCollisionAndGetValidCoords`, reject a result further from
  `ULDUAR_THORIM_NEAR_ARENA_CENTER` than `ULDUAR_THORIM_ARENA_LEASH_RADIUS` and return the centre
  instead. The centre is always inside the box and always reachable, and `ThorimArenaLeashAction`
  already treats it as the fallback. A bot standing on the tank is worse formation than a ring slot
  and strictly better than a bot that never acts.
- In `ThorimArenaAnchorNeedsMove`, never insert into `arenaAnchorArrived` for a spot that fails the
  leash test. Arriving is what disables the movers, so a spot the leash rejects must not count as
  arrival. This also covers any future path that produces an out-of-leash anchor.

Leave `ULDUAR_THORIM_ARENA_BOX_*` alone — they are copied from `GetArenaPlayer()` and widening them
summons the Lightning Orb.

## Fix 5 — Stop the arena losing its only tank

**File:** `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp` — `AssignThorimSquads`

`AssignThorimSquads` protects the main tank with `if (gauntletTank && !PlayerbotAI::IsMainTank(gauntletTank))`,
but nothing sets `MEMBER_FLAG_MAINTANK` for Thorim — the only writer in the codebase is
`RazorscaleBossHelper::AssignRolesBasedOnHealth` (`src/Ai/Raid/Uld/Util/UldBossHelper.cpp:407`). So
`PlayerbotAI::GetMainTankGuid` falls through to its second loop, which returns the first group member
satisfying `IsTank(member) && member->IsAlive()`.

The split latches as soon as `roster` is non-empty. The traces show bots still being summoned in
during the pre-roll (Nightwarrior at (2132, -173) at -0:18), so that can happen before the tank has
landed — the alive-tank fallback misses it, `IsMainTank(Bulwark)` reads false, and the guard does not
fire. Harden the latch the same way the function already refuses on an empty roster:

```cpp
// The whole split hangs off which tank holds the arena, and GetMainTankGuid's fallback only sees
// tanks that are already alive and in the instance. Latching before the tank lands sends the raid's
// only tank down the corridor.
if (PlayerbotAI::GetMainTankGuid(group).IsEmpty())
    return;
```

Then make the arena's claim unconditional: count tanks in `roster` before the `take(gauntletTank)`
call and skip it when the count is 1.

Setting `MEMBER_FLAG_MAINTANK` explicitly for Ulduar is the cleaner long-term answer but is wider than
this plan; record it in the findings doc instead.

## Fix 6 — Make the squad split observable

`thorim.squad` and `thorim.squadsassigned` are declared in `ThorimEncounterState`
(`src/Ai/Raid/Uld/Util/UldEncounter_Thorim.h:57-58`) but appear in **neither** trace — grep for
`"k":"thorim.` returns only `arenaarrived`, `barrierbail`, `chargedorb`, `ringarrived`, `slot`,
`smashside`. The most consequential decision in the fight is invisible, which is why Fix 5 had to be
inferred from position medians. Find out why the `ObsGuidMap` / `ObsValue` never flushes for these two
and fix it.

While there, add a note for the chosen DPS target from Fix 1 (`thorim.dpstarget`). The traces cannot
show which unit a bot was *told* to attack, only its `GetVictim()`, and that gap cost real time here.

---

## Verification

Static, before any pull:

- `grep -n "GetThorimSquad" src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.cpp` — every corridor trigger
  gates on `ThorimSquad::Gauntlet`.
- `grep -rn "TargetIcon" src/Ai/Raid/Uld/{Action,Trigger}/*Thorim*` — only the one-shot clear from
  Fix 1 remains.
- `grep -rn "find target" src/Ai/Raid/Uld/{Action,Trigger,Util}/*Thorim*` — no name-based lookups left.
- Confirm the module compiles. Per project history it cannot be built headless in this environment;
  hand the branch off for a build rather than claiming one.

In-game, one normal-mode 25-man pull, then re-run the analysis on the fresh trace in
`env/dist/logs/botobs/`:

1. **Nobody attacks Thorim in phase 1.** No roster `cast` record with `tgt` = Thorim while his z is
   above 429.6, and no arena bot whose `GetVictim()` is Thorim. Baseline to beat: Hellflame 157 casts,
   Smartface 126, Prayer 32.
2. **No `thorim arena target guard killed dps assist` vetoes.** Baseline: 30.
3. **No arena bot issues a corridor move.** No `move ... by 'thorim runic smash action'` for any bot
   whose median distance from (2134.99, -263.12) is under 40 yd.
4. **No anchor outside the leash.** Every `thorim arena positioning action` and `thorim arena leash
   action` destination is within 30 yd of the arena centre; ring slots land at 10 or 14 yd.
5. **No frozen bot.** Every living arena bot holds a target in well over half its samples and travels
   more than a few yards. Baselines: Nightwarrior 42% target / 112 yd, Holylight 0% / 11 yd,
   Stormweaver 0% / 0 yd.
6. **The arena keeps a tank.** Bulwark's median distance from the arena centre stays under 30 yd, and
   `thorim.squad` records now appear showing the split.
7. **Champions and Warbringers stop accumulating** — neither above ~3 alive at any sample. Commoners
   may still pile up; harmless.
8. **No Lightning Orb unit** in the trace.

The `census.py` / `activity.py` / `acts.py` helpers used for this investigation are in the session
scratchpad; re-deriving them is a few minutes' work and they do not need checking in.

## Follow-up, not in this plan

- Write the findings into `docs/raids/ulduar.md` (Thorim section) and copy this plan to
  `docs/plans/thorim-arena-dps/thorim-arena-dps.PLAN.md` when implementation starts.
- Burst cooldowns held for all of phase 1 (~500 suppression episodes on the arena squad in attempt 1:
  tinker 153, trinket 107, Blood Fury 58, Rapid Fire 28, Readiness 27, Blade Flurry 23, Adrenaline
  Rush 18, Death Wish 16, Bestial Wrath 14, Army 13, Killing Spree 12, Recklessness 11, Berserk 10).
  Excluded by the user; record it so it is not rediscovered.
- `prioritized targets` never clearing inside a combat is a base-engine trap, not a Thorim one. Other
  encounters with an untargetable phase will hit it the same way.
- `DpsAoeStrategy` is defined (`src/Ai/Base/Strategy/DpsAssistStrategy.h:23`) and registered in
  `StrategyContext.h:242`, but no `addStrategies*` call anywhere adds it, so `dps aoe` can never run —
  confirmed by both traces, where it appears zero times. Fix 1's explicit priority list makes it
  unnecessary here, but the dead wiring is worth a look.
- Setting `MEMBER_FLAG_MAINTANK` explicitly for Ulduar rather than relying on `GetMainTankGuid`'s
  alive-tank fallback.
