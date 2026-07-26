# VoA Strategy — Stability & Efficiency Improvements

## Context

The mod-playerbots VoA (Vault of Archavon) raid strategy already covers all four bosses
(Archavon, Emalon, Koralon, Toravon). Investigation confirms the core mechanics are wired and
functional — notably that skull-marking **does** drive DPS kills (`DpsTargetValue::Calculate`
→ `RtiTargetValue`, default `rti = "skull"`), so Emalon's overcharge-minion marking correctly
forces a burn.

The goal is to make the existing strategy more **stable** (fewer avoidable deaths) and more
**efficient** (better DPS uptime against the berserk timers), not to rewrite it. This plan
targets a small set of verified, low-risk gaps found during investigation.

### What the investigation ruled OUT (do not implement)
- **Archavon Choking Cloud**: does not exist in this core. `boss_archavon.cpp:141-147`
  (`EVENT_CHOKING_CLOUD`) actually casts `SPELL_CRUSHING_LEAP` (58960) — a random-target leap,
  not a persistent ground DoT. No cloud to dodge.
- **Boss-name string mismatch**: false alarm — token compression dropped the word "the" in
  earlier reads; all strings are consistently `"<boss> the <x> watcher"` (verified by grep).
- **Nature/Shadow resistance trigger naming**: `BossNatureResistanceTrigger` /
  `BossShadowResistanceTrigger` omit the `bossName +` prefix (`BossAuraTriggers.h`), but the
  engine resolves triggers by their unique *registration* name (`Engine.cpp:450`); the internal
  name only feeds log/perf labels (`Engine.cpp:466,475`). Cosmetic only — skip.

## Recommended changes

### 1. Emalon Lightning Nova — add a movement-suppression multiplier (reliability)
Lightning Nova (`SPELL_LIGHTNING_NOVA` 64216 / 65279) is a PBAoE; non-tanks must clear it.
The run-out action (`EmalonLightingNovaAction`, `ACTION_RAID + 1`) is only a `MoveAway`, and —
unlike Koralon Breath and Toravon avoid — has **no multiplier** clamping competing movement.
`ReachTargetAction` / `FollowAction` / `CombatFormationMoveAction` can fight the run-out, so a
bot may fail to fully clear the nova and eat avoidable damage.

- Add `EmalonLightningNovaMultiplier` mirroring `KoralonBurningBreathMultiplier`
  (`VoAMultipliers.cpp:15-29`): when `EmalonLightingNovaTrigger` is active, return `0.0f` for
  `CastReachTargetSpellAction`, `ReachTargetAction`, `CombatFormationMoveAction`, `FollowAction`.
- Files: `VoAMultipliers.h` (declare), `VoAMultipliers.cpp` (implement), `VoAStrategy.cpp:90-97`
  `InitMultipliers` (register).

### 2. Simplify + harden `EmalonOverchargeAction` (robustness + maintainability)
`VoAActions.cpp:40-109` reimplements ~45 lines of skull-marking with bespoke tank-authority
logic, raw `group->SetTargetIcon`, an **unchecked `group` pointer**, and a stale copy-paste
comment ("Eonar's Gift"). The trigger already gates to tank-only, and marking is idempotent.

- Replace the body after the minion is found with the existing helper
  `MarkTargetWithSkull(bot, minion)` (`RaidBossHelpers.h:8`, which null-checks the group and
  only re-sets when the icon differs). Drops the duplicated authority loop and the null-deref
  risk; keeps identical observable behavior (overcharged minion gets skull → DPS burn it).
- While here, null-guard `group` before `GetTargetIcon` in `EmalonMarkBossTrigger`
  (`VoATriggers.cpp:26-28`) and `EmalonOverchargeTrigger` (`VoATriggers.cpp:117-119`) — prevents
  a crash for an ungrouped bot.

### 3. Reduce ranged spread jitter (DPS uptime / berserk-timer efficiency)
`ArchavonRockShardsSpreadTrigger` (`VoATriggers.cpp:144-162`) and
`KoralonFlamingCinderSpreadTrigger` (`:201-219`) fire whenever a ranged bot is within `8.0f` of
**any** groupmate, for the whole fight. This makes ranged reposition continuously (throttled to
1/s), bleeding DPS uptime against Archavon's 5-min and Emalon's 6-min berserk. `FleePosition`
also only flees the single *nearest* player, so a 3-stack can oscillate.

- Establish-and-hold instead of continuous flee: tighten the trigger radius (e.g. ~6y) so bots
  settle into a spread and stop repositioning once spaced, and/or raise the action `minInterval`
  (`VoAActions.cpp:138,203`) to reduce churn. Optionally flee the group centroid rather than the
  single nearest player to avoid oscillation.
- Files: `VoATriggers.cpp` (radii), `VoAActions.cpp` (interval / flee target). Tuning-only; no
  new classes.

## Critical files
- `modules/mod-playerbots/src/Ai/Raid/VoA/VoAMultipliers.{h,cpp}` — new Nova multiplier.
- `modules/mod-playerbots/src/Ai/Raid/VoA/VoAStrategy.cpp` — register the multiplier.
- `modules/mod-playerbots/src/Ai/Raid/VoA/VoAActions.cpp` — simplify overcharge action; spread interval.
- `modules/mod-playerbots/src/Ai/Raid/VoA/VoATriggers.cpp` — group null-guards; spread radii.
- Reuse: `MarkTargetWithSkull` (`RaidBossHelpers.h:8`), existing multiplier pattern
  (`VoAMultipliers.cpp`).

## Verification
- Build worldserver (`RelWithDebInfo`, `-DMODULES=static`) — only if explicitly requested.
- In-game / on a test realm: spawn a bot raid, `voa` strategy on each VoA boss:
  1. **Emalon Nova**: watch non-tank bots fully clear the PBAoE on every cast (no partial
     run-outs, no immediate return mid-cast). Confirm no nova deaths across several casts.
  2. **Emalon Overcharge**: confirm the overcharged Tempest Minion gets skull and dies before
     Overcharged Blast; confirm no crash with an ungrouped/solo bot.
  3. **Archavon / Koralon spread**: confirm ranged reach and hold a spread, then stop moving —
     DPS uptime visibly higher than before; still spaced enough that Rock Shards / Flaming Cinder
     don't chain multiple players.
- Run `python apps/codestyle/codestyle-cpp.py` before finishing.
