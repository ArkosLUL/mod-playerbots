# Kologarn — rebuild the strategy to guide parity, SWP-style

> Delete this directory once the work ships; the durable findings belong in `docs/raids/ulduar.md`.

## Context

The current Kologarn strategy (7 triggers / 7 actions, no multiplier) covers ~2 of the 8 lines in the
reference tactic; 3 more are stubbed with `BotCheatMask::raid` cheats (`TeleportTo`, `RemoveAura`).
Investigation also found three bugs, one of which makes the tank-swap stand-in dead code.

It also drives targeting through **raid target icons**, which is the wrong mechanism: Skull means
"everyone DPS this" and Moon is the CC channel, both with engine-wide semantics that Kologarn's
per-role targeting corrupts. SWP is the reference for doing this right — it uses **zero** RTI in
targeting (the only marker in the whole raid is one cosmetic `MarkTargetWithSkull` at
`SWPActions_KJ.cpp:166`).

Decisions taken: implement the full guide; replace the cheats; drop RTI entirely and select targets
in code; reactive mechanics only, no raid formation.

### Verified encounter facts (this core — several differ from retail guides)

Sources: `src/server/scripts/Northrend/Ulduar/Ulduar/boss_kologarn.cpp`, `acore_world`,
`modules/mod-spell-tweaks/data/dbc-reference/`.

| Fact | Value |
|---|---|
| Kologarn / Left Arm / Right Arm / Rubble | 32930 / 32933 / 32934 / 33768 |
| Kologarn spawn | **(1797.15, −24.40, 448.74), o=3.19 ≈ π** — faces −X, so **the entrance is −X** |
| Arm axis | arms split along **Y**: right y=−3.5, left y=−44.8 |
| Arm respawn | **50s**, not 60s (`EVENT_RESTORE_ARM_*`); logic is state-driven so this doesn't matter |
| Killing an arm damages boss | `SPELL_ARM_DEAD` 63629, core-side |
| Rubble spawn | at the **dead arm's** side; right arm → (1777.82, −3.51, 448.9); 5 per death |
| Rubble speed | 8.0 yd/s > player 7.0 → **cannot be kited, must be held** |
| Rubble damage | SmartAI on 33768: Rumble 63818 (10-man) / **Stone Nova 63978 (25-man, 10 yd, ~5550 + knockback)**, every 4–7s / 8–14s |
| Overhead Smash 63356 / One-Armed 63573 | applies **Crunch Armor 63355** (−20% armour, 4 stacks, 45s), on a 14s timer |
| Crunch Armor **64002** | the −25% variant — **never applied by this core**. No script, no SmartAI, no `spelldifficulty_dbc` link |
| Stone Grip | 62166 (10) / 63981 (25); **1 target**, and `spell_ulduar_stone_grip_cast_target` strips `GetCaster()->GetVictim()` → **the body tank is exempt**. Force-casts `SPELL_RIDE_RIGHT_ARM` 62056; victim instakilled on expiry unless the arm releases (`_damageDone` = 80k/380k) |
| Focused Eyebeam | `spell_kologarn_focused_eyebeam` on 63342 takes the 3 most distant players via `NonTankTargetSelector`, then picks **exactly ONE** at random → one runner, always a distant player, **never the tank**. Eyes 33632 / 33802 `MoveChase` at **5.5 yd/s** (< player 7.0, so running works); beam 63346 is a **3 yd** AoE |
| Arm Sweep 63766 → Shockwave 63783 | **200 yd radius**, no core spell script narrowing it → **not positionally avoidable here**. Nature school |
| Petrifying Breath 62030 | fires whenever `kologarn->GetVictim()` is **out of melee range**. Nature, 100 yd |
| Damage schools | Shockwave + Petrifying Breath = **Nature**; Focused Eyebeam = **Shadow**; Stone Nova / Rumble / Crunch Armor = Physical |
| Pit instakill | `boss_kologarn_pit_kill_bunny`: x 1782–1832, y −56…8, z 400–439 |

Because the body tank is exempt from both Stone Grip and Focused Eyebeam, "body tanked at all times"
and "run from the eyebeam" never conflict.

## Architecture — copy the SWP model

Delete the whole marker layer: `KologarnMarkDpsTargetTrigger`/`Action`,
`KologarnRtiTargetTrigger`/`Action`, and the `"kologarn attack dps target trigger"` →
`"attack rti target"` node. Replace with **role-scoped trigger/action pairs**, each owning one
target, following `EredarTwinsMainAndSecondAssistTanksPositionSacrolashAction`
(`SWP/Action/SWPActions_Twins.cpp:104`):

```cpp
if (AI_VALUE(Unit*, "current target") != target)
    return Attack(target);
// ... positioning for this role, in the same action
```

Targeting and placement stay in one action rather than split across nodes.

**`KologarnDisableAutomaticTargetingMultiplier`** — required, or the generic targeting fights us
every tick. `DpsTargetValue::Calculate()` (`Ai/Base/Value/DpsTargetValue.cpp:281`) falls through to a
smart-target strategy when no RTI is set, so `"dps target"` is never null and
`NotDpsTargetActiveTrigger` stays true. Copy `EredarTwinsDisableAutomaticTargetingMultiplier`
(`SWP/SWPMultipliers.cpp:515`) verbatim in shape: return `1.0f` in `BOT_STATE_NON_COMBAT`, else
`0.0f` for `DpsAssistAction`, `TankAssistAction` **and** `CastDebuffSpellOnAttackerAction` while
Kologarn is alive. The third cast is what stops DoTs landing on the wrong mob.

### Role → target

| Role | Predicate | Target |
|---|---|---|
| Body tank | `kologarn->GetVictim() == bot` | body; stay in melee range |
| Off-tank | the other of {MT, assist0} | rubble if any alive; else right arm while within 30 yd of the body, else the body |
| Melee DPS | `IsMelee && !IsTank` | right arm if alive, else body |
| Ranged DPS | `PlayerbotAI::IsRangedDps` (excludes healers) | rubble if any alive, else right arm if alive, else body |
| Healers | — | untouched |

Rubble ownership is **derived**, never stored: the off-tank is whoever is not currently holding the
body, so the handoff at a taunt swap falls out for free with no state to keep in sync.

## Bug fixes

**B1 — Right Arm is often never resolved.** `AI_VALUE2(Unit*, "find target", "right arm")` walks
`GetThreatenedByMeList()` (`Ai/Base/Value/TargetValue.cpp:170`) — only units *this* bot threatens — so
a tank parked on the body gets `nullptr`. Replace every Kologarn `"find target"` call with
`GetFirstAliveUnitByEntry(botAI, <entry>)` (`Ai/Raid/RaidBossHelpers.h:29`), which scans
`"possible targets no los"`. Add `NPC_LEFT_ARM = 32933` and the eye entries to the Kologarn block in
`Uld/Util/UldBossHelper.h:40-48`.

**B2 — stale Cross marker.** Moot; the marker layer is gone.

**B3 — Crunch Armor id.** Use **63355**. Keep 64002 in the enum as `SPELL_CRUNCH_ARMOR_ALT` behind a
`GetCrunchArmorStacks(Unit*)` helper that sums whichever is present (the all-variants predicate from
the recipe), so the code survives a future core change.

## Mechanics

**Overhead Smash tank swap.** Delete `KologarnCrunchArmorTrigger`/`Action` (the `RemoveAura` cheat —
which never fired anyway, per B3). Add `KologarnSmashSwapTrigger`/`Action` modelled on
`IronAssemblyFusionPunchSwapTrigger` (`Uld/Trigger/UldTriggers_IronAssembly.cpp:91-127`): partners are
MT + assist0 only; bot must be the off-tank; fire when the active tank has **≥2** stacks **and this
bot has strictly fewer**; `Attack(kologarn)` then
`botAI->DoSpecificAction("taunt spell", event, true)`. Priority `ACTION_RAID + 2`.

Strictly-fewer rather than an absolute cap on the incoming tank: with 45s duration against a 14s
Smash timer stacks never fully clear, and an absolute cap deadlocks both tanks at 2 and stops
swapping entirely. Equal stacks correctly means hold.

**Rubble.** `KologarnRubbleTankTrigger`/`Action`, owned by the off-tank. Attack the nearest rubble,
taunt any whose victim is not this bot, and hold at a spot displaced **laterally along ±Y** toward
the dead arm's side — `ULDUAR_KOLOGARN_RUBBLE_HOLD_OFFSET ≈ 18 yd` from the raid centroid, **X
unchanged**. Lateral, not backward: −X is the eyebeam escape lane and must stay clear. Rubble outrun
players, so this is a hold, not a kite. Ranged AoE then falls out of the role table plus the engine's
existing `"aoe count"` / `DpsAoeStrategy` thresholds — no separate AoE action. Keep the hunter
`KologarnRubbleSlowdownAction`, retargeted from the (now deleted) Skull icon to the held rubble.

**Focused Eyebeam.** Rewrite `KologarnEyebeamAction` as real movement: drop both hardcoded
`TeleportTo` positions, the `SetNextMovementDelay(5000)` and the `HasCheat(BotCheatMask::raid)` gate.
Resolve the eye by entry (33632 / 33802) rather than the `"Focused Eyebeam"` name prefix —
locale-independent, and it distinguishes victim from bystander:

- `eye->GetVictim() == bot` → run toward the entrance (**−X**), clamping Y to the raid's band and Z to
  448 so the run never crosses the pit box.
- otherwise → `FleePosition(eyePos, ULDUAR_KOLOGARN_EYEBEAM_SAFE_DISTANCE)`.

Raise the trigger radius from 4 yd to ~8 yd so bots move before the 3 yd beam lands. Priority
`ACTION_EMERGENCY`.

**Petrifying Breath guard.** `KologarnBodyUncoveredTrigger` at `ACTION_EMERGENCY`: whenever
`kologarn->GetVictim()` is null or out of melee range, send the nearest tank — then the nearest melee
— into melee range. The exposure that wipes is MT death while the off-tank is deliberately 18 yd out
on rubble; the swap handover alone would be covered by the swap action's `Attack`.

**Stone Grip.** `KologarnMultiplier`, modelled on the Slag Pot clause of `IgnisMultiplier`
(`Uld/UldMultipliers.cpp:130-150`): while a bot holds 62166 / 63981 it is a stunned vehicle
passenger, so zero out `MovementAction` — movement orders only fight the ride. Freeing victims needs
no code: DPS already focus the right arm, and B1 is what makes that actually happen.

**Healers.** No new code. `PartyMemberToHeal::Calculate` (`Ai/Base/Value/PartyMemberToHeal.cpp:30`)
is lowest-HP + distance and does not filter vehicle passengers, so gripped victims and smashed tanks
are already picked up. Do **not** use `"focus heal targets"` — it is an exclusive filter that would
starve the rest of the raid.

**Resistance auras.** No boss-specific shadow node: priests already run `shadow protection` /
`shadow protection on party` as an always-on group buff (`PriestNonCombatStrategy.cpp:70-74`). Keep
the existing `kologarn nature resistance` node — Shockwave and Petrifying Breath are both Nature, so
Aspect of the Wild is genuinely wanted.

Only one hunter ever casts it, and that is already handled: `BossNatureResistanceTrigger`
(`BossAuraTriggers.cpp:169-187`) elects the **first alive hunter in the raid** and returns false for
every other. Aspect of the Wild 49071 is effect 65 `SPELL_EFFECT_APPLY_AREA_AURA_RAID` (not the
party variant 35), aura 143 `MOD_RESISTANCE_EXCLUSIVE`, +130 nature, **30 yd** — raid-scoped and
non-stacking, so a second hunter would add nothing anyway. Known limit, out of scope here: bots
further than 30 yd from the elected hunter get no coverage.

**Shared lookup fix.** `BossNatureResistanceTrigger` resolves the boss through the same threat-list
path as B1, so hunters parked on the arm never raise the aspect. Fix it **inside the shared trigger**
in `Ai/Base/Trigger/BossAuraTriggers.cpp`: keep the string-name API exactly as-is, but resolve by
scanning `"possible targets no los"` by name instead of the threat list. One function body, no
call-site churn, and it repairs the same bug in ~20 nodes across MC, VoA and Ulduar
(`BossNatureResistance`, `BossShadowResistance`, `BossFrostResistance`, `BossMarkSkull`). The
existing exact-length name match already guards against collisions in the wider scan.

## Deliberately not implemented

**Left Arm.** Per the guide it dies to incidental cleave and is never worth focusing. No marker, no
targeting rule, no logic. Killing it deliberately would double the rubble spawns and risk a
both-arms-down Stone Shout window when the two 50s respawn timers drift into phase.

**Shockwave.** 63783 is 200 yd with no core script narrowing it — it hits the platform regardless of
position. A healing check, not a dodge; an avoid action would be dead code.

**Raid formation.** The guide's screenshots show a deliberate layout (healers to one side, ranged
back, melee and tanks at the body), but none of the mechanics depend on it, and fixed offsets on a
narrow bridge over an instakill pit is exactly where bots fall in. Reactive mechanics only this pass.

**`KologarnFallFromFloorAction`.** Kept as-is, teleport and all. It is a pathing workaround, not a
mechanic, and a bot under the bridge dies to the pit bunny in ~1s so there is no walk-back to replace
it with. Document it as such so it doesn't read as an unfixed cheat.

## Files touched

- `Uld/Util/UldBossHelper.h` — Kologarn enum (`NPC_LEFT_ARM`, eye entries, `SPELL_CRUNCH_ARMOR` 63355
  + alt, Stone Grip 10/25), `ULDUAR_KOLOGARN_RUBBLE_HOLD_OFFSET`,
  `ULDUAR_KOLOGARN_EYEBEAM_SAFE_DISTANCE`; drop the two eyebeam teleport positions.
- `Uld/Trigger/UldTriggers_Kologarn.{h,cpp}` and `Uld/Action/UldActions_Kologarn.{h,cpp}` — delete the
  mark / rti / attack-dps / crunch-armor pairs; add the role-scoped targeting pairs,
  `KologarnSmashSwap*`, `KologarnRubbleTank*`, `KologarnBodyUncovered*`; rewrite the eyebeam pair.
- `Uld/UldTriggerContext.h`, `Uld/UldActionContext.h` — register/unregister creators.
- `Uld/UldStrategy.cpp` — replace the Kologarn `TriggerNode` block (lines 195–228); add both
  multipliers to `InitMultipliers` (line 592).
- `Uld/UldMultipliers.{h,cpp}` — `KologarnDisableAutomaticTargetingMultiplier`, `KologarnMultiplier`.
- `Ai/Base/Trigger/BossAuraTriggers.cpp` — shared lookup fix.
- `docs/raids/ulduar.md` — new Kologarn section; update the "Normal-mode gaps still open" entry at
  line 633 which lists Kologarn as cheat-only. **Run `/compact-docs-writer` before editing this doc.**

Ulduar's strategy is already registered, so none of the four cross-cutting wiring sites from the
recipe need touching.

## Verification

1. `python apps/codestyle/codestyle-cpp.py` must pass.
2. Headless compile is **not** possible in this environment — hand the build off, don't promise one.
3. In-game, 25-man (Stone Nova only exists there) and 10-man for the Rumble path:
   - Melee and ranged sit on the Right Arm from the pull with **no raid icons set** — confirms B1 and
     the marker removal.
   - Crunch Armor 63355 reaching 2 stacks is followed by an off-tank taunt; stacks on the outgoing
     tank stop climbing — confirms B3 and the swap.
   - On arm death, rubble converge on the off-tank ~18 yd off to the dead arm's side with the −X lane
     clear; ranged switch and AoE; no Stone Nova reaches the melee stack.
   - Eyebeam: the single target runs −X toward the entrance rather than blinking; bystanders scatter;
     no repeated 63346 ticks.
   - Kill the MT deliberately mid-fight — a tank or melee must close on the body before Petrifying
     Breath goes out.
   - Gripped bots stop issuing movement orders and are freed when the arm releases.
4. Re-run with `raid` stripped from `AiPlayerbot.BotCheats` — behaviour must be identical except the
   fall-from-floor recovery. Effective value can be overridden by `configurationOverrides/*.env`;
   read it with `docker exec ac-worldserver env | grep ^AC_`.
