# XT-002: performance bottlenecks and behaviour-identical fixes

## Context

The XT-002 strategy works (25-man hard-mode kill, trace
`env/dist/logs/botobs/603_1_xt-002-deconstructor_1788631900.ndjson`). The user asked for a performance
investigation: a list of bottlenecks, and for each whether it can be fixed **without changing bot
behaviour in any way**. This plan is that list, plus the fixes that pass that bar.

Standing constraints for whoever executes this:
- Another session has uncommitted Mimiron work, including `src/Ai/Raid/Uld/UldStrategy.cpp`. Touch only
  the XT-002 files listed under Implementation; stage nothing else.
- No git writes (commit, stash, …) without an explicit instruction in the conversation.
- Comments follow `no-nonsense-comments.md` and need the `use-conversational-language` skill invoked.
- The module cannot be compiled here (no `compile_commands.json`); the user rebuilds.

## Where the cost comes from (verified in code)

1. **"nearest npcs" is recomputed on every read.** `CalculatedValue::Get` recalculates whenever
   `checkInterval < 2` ([Value.h:71-85](src/Bot/Engine/Value/Value.h#L71-L85)), and `NearestNpcsValue` is
   built with the default interval 1 ([ValueContext.h:428](src/Ai/Base/ValueContext.h#L428),
   [NearestUnitsValue.h:19-23](src/Ai/Base/Value/NearestUnitsValue.h#L19-L23)). Each read is a
   `Cell::VisitObjects` at `SightDistance` 100 yd plus `IsWithinLOSInMap` (a vmap ray) for **every**
   non-player unit found, pets, totems and guardians included
   ([NearestUnitsValue.cpp:10-23](src/Ai/Base/Value/NearestUnitsValue.cpp#L10-L23),
   [NearestNpcsValue.cpp:14-21](src/Ai/Base/Value/NearestNpcsValue.cpp#L14-L21)). One such read is called
   a **scan** below.
2. **Every XT-002 lookup is a scan.** `GetXT002` / `GetXT002ExposedHeart` go through
   `GetFirstAliveNpcByEntry` ([UldEncounter_XT002.cpp:53-66](src/Ai/Raid/Uld/Util/UldEncounter_XT002.cpp#L53-L66));
   `IsXT002HeartbreakActive`, `IsXT002Submerged` and `IsXT002AddEngageable` each call `GetXT002` again;
   `GetXT002EngageableAdd`, `InsideXT002Hazard`, `BuildPriorityList` and `XT002AvoidHazardAction` walk the
   list themselves. The comment in `BuildPriorityList` claiming it "adds no grid work" is wrong.
3. **Multipliers run on every action every bot pops**, whenever `isUseful()` holds
   ([Engine.cpp:216-232](src/Bot/Engine/Engine.cpp#L216-L232)), and they are **not** behind the
   per-encounter gate: only triggers are wrapped in `UldGatedTrigger`
   ([UldTriggerContext.h:199-204](src/Ai/Raid/Uld/UldTriggerContext.h#L199-L204)), multipliers are
   registered bare ([UldStrategy.cpp:880-881](src/Ai/Raid/Uld/UldStrategy.cpp#L880-L881)). XT's two
   multipliers therefore run on every action anywhere in Ulduar.
4. Triggers have interval 1, so all six XT triggers run every tick while their gate is open
   ([Trigger.cpp:41](src/Bot/Engine/Trigger/Trigger.cpp#L41)).

## Measured inputs

- Bot tick ≈ 210 ms during the pull (death-replay timestamps). World update diff 106–289 ms, median 260
  (`Update time diff` lines in `env/dist/logs/Server.log`), with 800–1200 random bots
  (`AC_AI_PLAYERBOT_MIN/MAX_RANDOM_BOTS`) and `AC_MAP_UPDATE_THREADS=6`.
- Actions reaching the multipliers per tick: **mean 4.3–7.5, peak 16–18** (four death replays,
  `postmortem.py <trace> --death N`, rows other than USELESS/VETO grouped by timestamp).
- Non-bot units per snapshot: p50 20, p90 30, max 55 (Void Zones 4.5 avg, Scrapbots 4.4, the rest pets,
  ghouls, Mirror Images, Treants, elementals). A lower bound on a scan's unit count: totems and toy piles
  are not in snapshots.
- `AC_AI_PLAYERBOT_PERF_MON_ENABLED=0`: the time per scan has **not** been measured. Scan counts below are
  exact from the code; converting them to milliseconds is what the baseline pull is for.

## Bottlenecks, ranked

Scans per bot per tick, steady-state combat, hard mode, after Heartbreak, not carrying, no hazard near.

| # | Where | Now | After | Fix | Behaviour-identical? |
|---|---|---|---|---|---|
| B1 | `XT002TargetGuardMultiplier::GetValue` resolves XT before looking at the action ([UldMultipliers_XT002.cpp:79-184](src/Ai/Raid/Uld/Multiplier/UldMultipliers_XT002.cpp#L79-L184)) | 1 per action reaching it: mean 4–8, peak 18; ×2 in normal mode | ~1–3 | Classify the action first, resolve XT only for kinds that can return 0 | **Yes** (proof below) |
| B2 | `XT002SetDpsPriorityAction` runs every tick for every bot: `BuildPriorityList` scans, then `IsAllowedTarget` re-resolves XT through `IsXT002Submerged` / `IsXT002AddEngageable` on each of its 3+ calls ([UldActions_XT002.cpp:674-897](src/Ai/Raid/Uld/Action/UldActions_XT002.cpp#L674-L897)) | 4, +1 per add checked | 1 | Record the first live XT that `BuildPriorityList` already passes over; pass it down | **Yes** |
| B3 | Trigger openers scan for XT before a cheap test that is false for most bots: debuff carrier (two aura lookups), Pummeller taunt (tank check), avoid hazard (6 / 12 yd `FindNearestCreature`) ([UldTriggers_XT002.cpp:46-77, 143-158](src/Ai/Raid/Uld/Trigger/UldTriggers_XT002.cpp#L46-L77)) | 3 | 0 unless the cheap test passes | Cheap test first | **Yes** |
| B4 | `XT002RaidPositionTrigger`: melee DPS and off-tanks run the whole trigger (2 scans + roster build and sort) to reach a guaranteed false; ranged and healers re-resolve XT in the inline hazard trigger and in `GetXT002RangedSlot`'s Heartbreak test ([UldTriggers_XT002.cpp:79-132](src/Ai/Raid/Uld/Trigger/UldTriggers_XT002.cpp#L79-L132), [UldEncounter_XT002.cpp:249-304](src/Ai/Raid/Uld/Util/UldEncounter_XT002.cpp#L249-L304)) | melee 2, ranged/heal 3 | melee 0, ranged/heal 1 | Role pre-check; pass XT down | **Yes** |
| B5 | Redirect threat, hunters and rogues: trigger resolves XT twice; action resolves it in `GetRedirectTank`, again after the cast check, again in `IsXT002Submerged`, plus once per Pummeller inside `GetXT002EngageableAdd` | trigger 2, action 4+ | 1 + 2 | Resolve once per evaluation; `GetXT002EngageableAdd` finds XT in the list it already holds | **Yes** |
| B6 | `XT002BurstWindowMultiplier::EvaluateWindow` ([UldMultipliers_XT002.cpp:59-77](src/Ai/Raid/Uld/Multiplier/UldMultipliers_XT002.cpp#L59-L77)), at most once per ms | 3 | 2 | Pass XT to the Heartbreak test | **Yes** |
| B7 | Formation roster rebuilt per call: `GetXT002RangedSlot` (trigger and action), and `XT002PointClearOfFormation` up to 10× per Searing Light carrier tick; the sort comparator does `GET_PLAYERBOT_AI` + `IsHeal` on both sides of every compare (several hundred `_playerbotsAIMap` lookups per build) | 0 scans | 1 build per carrier tick; role flags once per build | Build once per call; precompute heal flags before sorting | **Yes** |

**Net:** melee ~18–22 → ~4–6 scans per bot-tick, ranged/healers ~16–20 → ~5–8, peaks ~35 → ~8. Outside
the XT fight: during every other Ulduar pull XT's multipliers alone drop from ~5–9 to ~1–3 per bot-tick;
on trash before XT (gate open) triggers plus multipliers drop from ~11–14 to ~2–5.

### Why each fix is identical

- **Pure lookups, unchanged state.** Everything reordered or passed down is a read with no side effects
  (`GetXT002`, `HasAura`, role checks, `FindNearestCreature`, "current target"). Between the original
  lookup and its replacement no action executes; the evaluation runs on the map's update thread, so grid
  contents, the bot's position and every LOS answer are the same and a fresh scan would return the same
  unit. Reordering an AND of pure predicates only changes which ones get evaluated.
- **B1.** The original returns 0 only if XT is present and one of:
  (a) in combat and Misdirection/Tricks; (b) in combat and `AvoidAoeAction`; (c) in combat and Divine
  Shield/Ice Block while carrying; (d) in combat and `DpsAssistAction` for a non-tank non-healer;
  (e) in combat and `TankAssistAction` for a tank; (f) in combat, a `MovementAction` that is not an
  `AttackAction`, bot is a carrier/ranged DPS/healer, name not in the encounter movers;
  (g) not hard mode, current target is the exposed Heart at ≤ 15 %, action is neither movement nor a heal,
  name not in the retargets. Compute the XT-independent part of (a)–(g) as flags; if none holds, return
  1.0 without resolving XT. Otherwise resolve XT and apply the same tests. For (g) the necessary cheap
  test is "current target has the Heart's entry", since `heart` is only ever an alive Heart unit.
- **B4 pre-check.** For a non-main-tank bot the trigger can only return true through
  `GetXT002RangedSlot`, which needs the bot in `BuildXT002RingMembers`, i.e.
  `!IsTank && (IsRangedDps || IsHeal)` by the same role calls. Bots failing that return false before any
  lookup. `GetXT002RangedSlot` already returned before `RaidObs::NoteDerived` for them, so the trace is
  unchanged too.
- **B2/B5 first XT.** `BuildPriorityList` keeps its own `boss` (last match) untouched; the value passed
  down is a separate "first live `NPC_XT002` in the list", which is exactly what `GetXT002` returns from
  the same list.
- **Carrier action.** `MoveToSearingLightSpot` can take Execute's Heartbreak answer: it is only reached
  when the parking branch did not run, so no move was issued before it.
- **B7.** Keys are unique (guid tie-break), so precomputing the heal flag yields the same permutation.

### Checked and not a bottleneck

- Carrier paths: `ParkVoidZone` is ~1.4k float ops plus one 110 yd no-LOS grid visit, `MoveClearOf` ≤ 80
  candidates × ≤ 24 distances, only for the 1–3 current carriers.
- Pathfinding: `TryMoveTo` hits the Duplicate / Waiting short-circuit before any path query
  ([MovementActions.cpp:273-280](src/Ai/Base/Actions/MovementActions.cpp#L273-L280)), so a walking carrier
  does not re-path each tick. `SearchForBestPath`'s 3-query retry only runs on `NoPath`, and the pull had
  0 `nopath` moves.
- `GetXT002BombLots` allocations, `HasAura`, `dynamic_cast` chains (~0.5 µs per action), the RaidObs
  `xt002.slot` string built while tracing: microseconds, and B1 costs a scan where these cost a string.

## Not fixable without changing behaviour

- **Per-tick cache of "nearest npcs" (or of XT) shared across the tick.** A failing action can still
  summon (totem, pet) or move the bot before later reads; staleness would change every consumer, not just
  XT. The engine-wide version affects all content.
- **Find XT through the instance script's stored GUID.** O(1), but drops the 100 yd / LOS condition, so
  the strategy would switch on for bots that cannot see XT today.
- **Throttling XT triggers (`checkInterval` > 1).** Changes reaction latency.
- **One formation roster shared raid-wide per world tick.** Group or role edits between two bots' updates
  would be missed; the per-call dedup (B7) already takes most of the cost.

## Out of scope (engine or other encounters)

- The root cause is the engine semantics in item 1 above; a fix there changes behaviour globally.
- `Action::getName()` returns by value, so name lookups (`IsBurstCooldownAction`, the mover and retarget
  sets) copy a string per action.
- Each issued move paths twice: `SearchForBestPath`, then `MovePoint`'s own `PathGenerator`.
- Other Ulduar multipliers have the same shape and run everywhere in Ulduar too, e.g.
  `MimironTargetGuardMultiplier` → `IsMimironEngaged` does 3 "possible targets no los" scans per action.
- "party member to heal" also recomputes per read; the healer branch of `XT002RaidPositionTrigger` forces
  one more per tick.

## Implementation (B1–B7)

- [UldMultipliers_XT002.cpp](src/Ai/Raid/Uld/Multiplier/UldMultipliers_XT002.cpp): rewrite
  `XT002TargetGuardMultiplier::GetValue` as flags → early 1.0 → lazy `GetXT002` → the same tests; keep each
  existing rationale comment on its condition. `EvaluateWindow`: pass `xt002` to the Heartbreak test.
- [UldHardMode.h](src/Ai/Raid/Uld/Util/UldHardMode.h) / [.cpp](src/Ai/Raid/Uld/Util/UldHardMode.cpp):
  `IsXT002HeartbreakActive(Player* bot, Unit* xt002)` replaces the `botAI` form (all callers are XT-002).
- [UldEncounter_XT002.h](src/Ai/Raid/Uld/Util/UldEncounter_XT002.h) / [.cpp](src/Ai/Raid/Uld/Util/UldEncounter_XT002.cpp):
  `IsXT002Submerged(Unit* xt002)` and `IsXT002AddEngageable(Unit* xt002, Unit* unit)` replace the `botAI`
  forms; `GetXT002EngageableAdd` finds the first live XT in the list it already holds;
  `GetXT002RangedSlot` takes `Unit* xt002`; `BuildXT002RingMembers` sorts on precomputed heal flags; a
  function returning every formation slot but the bot's replaces `XT002PointClearOfFormation` (its only
  caller is the Searing Light spot); a comment on `GetFirstAliveNpcByEntry` that each call is a 100 yd
  grid visit with a LOS ray per npc, so callers resolve once and pass the unit down.
- [UldTriggers_XT002.cpp](src/Ai/Raid/Uld/Trigger/UldTriggers_XT002.cpp): carrier auras first; taunt
  tank check first; avoid hazard does the two `FindNearestCreature` tests first (called directly, no
  `TooCloseToCreatureTrigger` object) and resolves XT only if one hits; raid position role pre-check and
  `xt002` passed to `GetXT002RangedSlot`; redirect passes `xt002` to `IsXT002Submerged`.
- [UldActions_XT002.h](src/Ai/Raid/Uld/Action/UldActions_XT002.h) / [.cpp](src/Ai/Raid/Uld/Action/UldActions_XT002.cpp):
  carrier `Execute` resolves XT once and passes it on (Heartbreak test, `ParkVoidZone`), and
  `MoveToSearingLightSpot` takes the Heartbreak answer and builds the slot list once;
  `BuildPriorityList` reports the first live XT, `IsAllowedTarget` takes it; `XT002RedirectThreatAction`
  resolves XT once and hands it to `GetRedirectTank`; `XT002RaidPositionAction` resolves XT for
  `GetXT002RangedSlot` (one scan, as today). Correct the `BuildPriorityList` comment, and the stale class
  comment above `XT002SetDpsPriorityAction` that still says healers get no target.

## Verification

A CPU-time loop needs a live raid pull, so it cannot run unattended. What can:

1. **Equivalence harness (agent, deterministic, seconds).** Scratchpad mock compiled with
   `g++ -std=c++17 -Wall -Wextra` in `acore/ac-wotlk-build:master` (as for the bomb-lot mock). Old bodies
   copied verbatim from `git show HEAD:<file>`, new ones from the working tree; stub classes mirror the
   real inheritance of every class `GetValue` casts to (look each up in its header, since
   `DpsAssistAction`, `AvoidAoeAction` and the like derive from `MovementAction`); lookups are counting
   stubs.
   - B1: every combination of action kind × encounter-mover/retarget name × role (tank, healer, ranged,
     melee) × carrying × XT {absent, idle, in combat} × hard mode × Heart {absent, ≤ 15 %, > 15 %} ×
     current target {none, the Heart, a Heart-entry unit the lookup misses, other}. Assert old == new for
     all; print mean scans per call, old vs new.
   - B3/B4 triggers: the same over their inputs.
   - B7: random rosters, old comparator vs precomputed flags, assert identical order.
   Must go red if a flag is dropped: flip one flag in a scratch copy and confirm a mismatch is reported.

   **Done, green.** 18.5M combinations, 0 mismatches. Scans per call, old → new: target guard
   1.28 → 0.40 (with XT absent, 1.00 → 0.24), carrier trigger 1.00 → 0.75 (the sweep carries a debuff in
   3 of its 4 states; in a fight almost no bot is carrying, so the real drop is to ~0), avoid hazard
   1.00 → 0.34, raid position 1.21 → 0.71, Pummeller taunt 1.18 → 0.56, redirect 1.31 → 0.67; roster
   order identical over 20k random rosters. Red-capable: dropping `purgingImmunity` from the guard gives
   1,392 mismatches, dropping the main-tank exemption from the raid position pre-check gives 11,520.
2. **Performance A/B (user, one pull each).** On the **currently deployed build, before rebuilding**:
   GM `.playerbots pmon toggle`, `.playerbots pmon reset` at the pull, `.playerbots pmon` after the kill or
   wipe, `.playerbots pmon toggle` to turn it off; output lands in `env/dist/logs/Playerbots.log`. Repeat on
   the new build. Compare:
   - `Trigger : xt002 debuff carrier trigger` avg: before ≈ the cost of one scan (the trigger is one scan
     plus two aura lookups for non-carriers), after a few µs. That row calibrates the rest.
   - `xt002 pummeller taunt / avoid hazard / raid position` trigger rows and
     `Action : xt002 set dps priority action`: expect ≥ 3× lower.
   - `Total : PlayerbotAI::UpdateAIInternal I` per call: includes the multipliers, which have no PerfMon
     row. Prediction: it drops by roughly (scan cost × scans saved per bot-tick). It is noisy: random bots
     in dungeons feed the same row, and any other Ulduar change between the two builds moves it.
   PerfMon's global mutex inflates absolute numbers with ~1000 random bots; only the ratio matters.
3. **Behaviour (same pull, user).** Nothing should move against the 2026-09-05 trace: kill, healer
   targetless share ~2–5 %, `power word: shield` / `beacon of light` evaluations > 0, 0 `nopath` moves,
   Void Zone Consumption ~175k of ~7.2M, Void Zones landing on cells. A difference here means a fix was
   not identical and gets reverted before anything else.

## Status

B1–B7 are implemented and the harness is green. Left: the rebuild, the PerfMon A/B, and the pull that
confirms nothing moved. The harness lives in the session scratchpad (`xt_equiv/extract.sh` pulls both
versions of each body straight out of git and the working tree, `harness.cpp` compiles them together),
so re-running it after any further edit is one command.
