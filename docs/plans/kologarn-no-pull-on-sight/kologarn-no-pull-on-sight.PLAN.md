# Kologarn: stop bots pulling the boss on sight

## Context

Bots engage Kologarn as soon as he enters their target scan range, without the master pulling. The
raid starts the fight on its own from up to 100 yards away.

The user's first guess was that the strategy marks the boss with a raid target icon (RTI/skull) and
that the generic `RtiTargetValue` → `dps target` → `dps assist` chain drags everyone in. That is the
real mechanism for the VoA bosses (`BossMarkSkullAction`), but **Kologarn does not mark anything**.
It already follows the SWP targeting model the user asked for:

- Per-role targets picked in code — `KologarnDpsTargetAction`, `KologarnBodyTankAction`,
  `KologarnOffTankAction`, `KologarnRubbleTankAction` — each calling `Attack(target)` directly
  (`src/Ai/Raid/Uld/Action/UldActions_Kologarn.cpp`).
- `KologarnDisableAutomaticTargetingMultiplier` zeroes `DpsAssistAction` / `TankAssistAction` /
  `CastDebuffSpellOnAttackerAction` while the boss is up
  (`src/Ai/Raid/Uld/UldMultipliers.cpp:337`), the same guard as
  `EredarTwinsDisableAutomaticTargetingMultiplier` in SWP.

So there is nothing to convert. The actual defect is that **none of the Kologarn triggers require the
encounter to have started.**

Every Kologarn trigger opens with `GetKologarn(botAI)`, which is
`GetFirstAliveUnitByEntry` (`src/Ai/Raid/RaidBossHelpers.cpp:224`) — it walks
`"possible targets no los"`, a pure proximity scan over `AiPlayerbot.SightDistance` (100.0, ignoring
line of sight). It answers "is Kologarn nearby", never "is Kologarn engaged". The moment the raid
walks within 100 yards:

1. `KologarnDpsTargetTrigger::IsActive()` → true → `KologarnDpsTargetAction` → `Attack(body/arm)`.
2. `KologarnBodyTankTrigger` / `KologarnOffTankTrigger` do the same for the tanks.
3. `KologarnBodyUncoveredTrigger` fires too — with no tank in melee it sends the nearest tank or
   melee at the body.

`AttackAction::Attack()` has no out-of-combat guard, so the pull happens.

For comparison, SWP resolves bosses through `AI_VALUE2(Unit*, "find target", …)`
(`src/Ai/Base/Value/TargetValue.cpp:159`), which only walks the bot's own threatened-by-me list and
therefore cannot resolve before engagement — the combat gate is implicit. Kologarn deliberately
cannot use that: a tank parked on the body never has the arms on its threat list, and the whole
per-role focus split collapses (documented at `src/Ai/Raid/Uld/Util/UldBossHelper.h:683`). Resolving
by entry is correct here; it just needs the combat gate that `find target` would have given for free.

VoA already has the right shape for this: `EmalonEncounterActive()` is
`boss && boss->IsInCombat()` (`src/Ai/Raid/VoA/VoAHelpers.cpp`).

Intended outcome: bots stand where the master puts them until the master (or anything else) engages
Kologarn, then the existing per-role targeting takes over unchanged.

## Changes

### 1. New encounter gate — `src/Ai/Raid/Uld/Util/UldBossHelper.h` / `.cpp`

Declare next to the other Kologarn helpers (header ~line 686) and define next to `GetKologarn`
(`UldBossHelper.cpp:1043`):

```cpp
bool KologarnEncounterActive(PlayerbotAI* botAI);
```

```cpp
bool KologarnEncounterActive(PlayerbotAI* botAI)
{
    Unit* kologarn = GetKologarn(botAI);
    return kologarn && kologarn->IsInCombat();
}
```

Mirror of `EmalonEncounterActive` in `src/Ai/Raid/VoA/VoAHelpers.cpp`. The body creature is the one
that enters combat; the arms and rubble are its summons, so the body is the single authority.
`boss_kologarn.cpp` never calls `SetInCombatWithZone`, so this flips exactly when someone actually
engages.

Header comment should record why the gate exists — the entry scan is proximity-only, so without it
the strategy pulls on sight.

### 2. Gate the triggers — `src/Ai/Raid/Uld/Trigger/UldTriggers_Kologarn.cpp`

Replace the opening `GetKologarn(botAI)` presence check with `KologarnEncounterActive(botAI)` in:

- `KologarnBodyTankTrigger`
- `KologarnOffTankTrigger`
- `KologarnRubbleTankTrigger`
- `KologarnDpsTargetTrigger`
- `KologarnSmashSwapTrigger`
- `KologarnBodyUncoveredTrigger`
- `KologarnRubbleSlowdownTrigger`
- `KologarnEyebeamTrigger`

The first four plus `BodyUncovered` are the ones that actually pull. The rest are already
self-gating (rubble and eyebeams only spawn mid-fight, `SmashSwap` needs `kologarn->GetVictim()`),
but converting them keeps one uniform rule for this boss — every Kologarn trigger asks
"is the fight on", not "is the boss nearby".

`KologarnSmashSwapTrigger` and `KologarnBodyUncoveredTrigger` also keep a local `Unit* kologarn`; call
`KologarnEncounterActive` first, then `GetKologarn` for the unit.

**Do not gate `KologarnFallFromFloorTrigger`.** It is pit-pathing recovery
(`z < ULDUAR_KOLOGARN_AXIS_Z_PATHING_ISSUE_DETECT`, above the instakill bunny) and has to work when a
bot falls in before the pull. It issues no attack, so it cannot cause the bug.

### 3. Gate the targeting multiplier — `src/Ai/Raid/Uld/UldMultipliers.cpp:337`

In `KologarnDisableAutomaticTargetingMultiplier::GetValue`, change the final
`return GetKologarn(botAI) ? 0.0f : 1.0f;` to use `KologarnEncounterActive(botAI)`.

Two reasons: the strategy's own targeting actions are now inert until the pull, so silencing the
generic pickers before then would leave bots with no target picker at all; and trash inside the
100-yard scan radius would otherwise be unfightable while Kologarn merely stands there. Keep the
existing early `BOT_STATE_NON_COMBAT` return and keep the encounter check last — it walks the target
list, so it should only run once something would actually be blocked.

`KologarnMultiplier` (stone-grip movement suppression) needs no change: `IsKologarnStoneGripped(bot)`
already implies the fight is on.

## What deliberately does not change

- No raid target icons are added or removed — Kologarn never used them. The "drop marking entirely"
  decision is already the state of this boss.
- No new per-boss disable-targeting multiplier — `KologarnDisableAutomaticTargetingMultiplier`
  already exists and matches the SWP shape.
- No trigger/action/strategy registration changes. `UldActionContext.h`, `UldTriggerContext.h` and
  `UldStrategy.cpp` (lines 202-247, 651-654) stay as they are.

## Verification

No headless build is possible in this environment; changes are header/impl-local and need a normal
worldserver build.

In-game, on both 10 and 25:

1. Walk the raid to the Kologarn walkway and stop 30-80 yards short, well inside the 100-yard scan.
   **Expected:** bots hold formation on the master. Nobody targets or moves at the body or arms.
   Confirm before the fix that they charge in from here — that is the reproduction.
2. Master pulls. **Expected:** body tank takes the body, off-tank takes the right arm, DPS follow
   `GetKologarnDpsTarget` (ranged onto rubble first), i.e. the pre-existing behaviour, unchanged.
3. Let a rubble wave spawn. **Expected:** off-tank picks it up and holds it at the offset spot,
   hunters drop Frost Trap.
4. Let Crunch Armor reach 2 stacks on the active tank. **Expected:** the swap still fires.
5. Kill both arms so the body is briefly uncovered. **Expected:** nearest tank (or melee if no tank
   is left) steps into melee range to suppress Petrifying Breath.
6. Wipe and release. **Expected:** boss leaves combat, bots re-approach without re-pulling.
7. Pull a trash pack within 100 yards of the boss. **Expected:** generic `dps assist` still works —
   this is what the multiplier gate buys.

## Follow-up worth flagging (out of scope here)

The same proximity-not-combat gate is missing on Auriaya: `AuriayaEncounterActive` is
`GetAuriaya(botAI) != nullptr` (`src/Ai/Raid/Uld/Util/UldBossHelper.cpp:429`), so it likely pulls on
sight the same way. Separate change; not touched by this plan.

Once approved, copy this plan to
`modules/mod-playerbots/docs/plans/kologarn-no-pull-on-sight/kologarn-no-pull-on-sight.PLAN.md` to
match the repo's planning-doc layout.
