# Razorscale

## Harpoons are GameObject state, not auras

`SPELL_CHAIN_1..4` (49679, 49682, 49683, 49684) **no longer exist anywhere in the core**. Harpoons
fire `SPELL_HARPOON_SHOT_1..4` cast by `NPC_RAZORSCALE_CONTROLLER` out of
`go_razorscale_harpoon::OnGossipHello`; the GameObjects are summoned per ground phase by the
controller (two in 10-man, four in 25-man) and a spent one carries `GO_FLAG_NOT_SELECTABLE` until it
is rebuilt. Readiness is that flag (`IsHarpoonReady`); `IsHarpoonFired()` and
`HarpoonData::chainSpellId` were removed for testing an aura that could never be present.

The tank debuff also moved: **Fuse Armor is 64821**, not 64771 (64774 is still the 5-stack `Fused
Armor`). 64771 is gone from the core, so the tank-swap check never fired. Threshold stays at 2 stacks.

Airborne, the raid kills the Dark Rune adds while the harpoon crews (`IsRazorscaleHarpoonCrew`: ranged
dps, never healers) reload; a volley grounds her, and the ground phase, knockdowns included, is the only
damage window. **The main tank is picked by max health, not spec**: `AssignRolesBasedOnHealth` flags
the living spec tank with the most, skipping anyone at `FUSEARMOR_THRESHOLD`, and runs while nobody
holds the flag and again once the main tank reaches the threshold. A pick is throttled to one per
`_roleSwapCooldown` (10 s) per bot and a fired harpoon sits out `HARPOON_COOLDOWN_DURATION` (5 s), held
in `RazorscaleInstanceState`, so a re-derivation cannot flip either mid-phase.

## Devouring Flame is 5 yd, and the skull has one owner

The patch is NPC 34188 carrying 64709, a 2 s periodic trigger of 64704 (64733 in 25-man), **whose
damage radius index is 8 = 5.0 yd**. Bots clear `DEVOURING_FLAME_CLEAR_RADIUS` (7 yd) so a step
actually leaves the patch instead of stopping on its edge, and `DevouringFlameBlocks()` rejects any
destination covered by another one — she drops these every 6–12 s and they stack up.

Clearing is only half of it. **The hold lives in a rule, not the action** —
`razorscale avoid devouring flames` is useful only while a patch is live
(`return !Scan().flame.IsEmpty();`), and standing clear is the `razorscale` `Block` rule's job: it
zeroes the generic movers and `avoid aoe` while the spot a melee bot would walk back to is on fire
(`avoid aoe` sits at `ACTION_EMERGENCY`, over every row, and picks its bearing blind to the other
patches). `StepClearOfFlames` tries 9 bearings, straight away then ±π/8 out to ±π/2, against patches
collected once round the bot (`CollectDevouringFlames`), not a sweep per candidate.
Both release the moment the tank drags her clear, so nobody stands out the patch's full life. A permanent veto here
is the freeze bug.

**The per-tick lookups go through one scan.** `RazorscaleScan` is the
[per-bot `getMSTime()`-stamped scan](../../engine/raid-mechanics-lessons.md#what-a-strategy-costs-per-raid),
held as a `ManualSetValue<RazorscaleScan*>` in `RaidUlduarValueContext` (`UldValueContext.h`) and
registered in `AiObjectContext::BuildSharedValueContexts`: a value because that is the one per-bot store
triggers, actions and multipliers can all reach. Without it the harpoons alone cost **12 GameObject
sweeps of 200 yd (37 cells) per bot per tick**, and `"nearest hostile npcs"` is a 100 yd sweep plus a LOS
ray per NPC on every read.

**The patch is safe to gate on the encounter.** 63236 is a trigger-missile (effect 32) into 63308,
which summons NPC 34188 for **22 s**; she is the summoner, so every patch sits in `BossAI::summons`
and both `_JustDied` and `EnterEvadeMode` `DespawnAll()` in the same call that sets the state. No
patch can outlive the encounter flag.

`razorscale kill target action` is the **only** thing that sets the skull: the boss whenever she is
on the floor, otherwise Sentinel → Watcher → Guardian. `DpsTargetValue` prefers the RTI target, so a
skull left on an add is the whole raid left on an add. Moon stays on the boss while she is airborne —
it is excluded from every DPS target scan — and is cleared on landing.

No generic strategy commands pets ([action-selection](../../engine/action-selection.md#multiplier-semantics)),
so they keep whatever they last hit: `razorscale pet control action` puts them on the adds
while she flies and on her when she lands, and always reports failure so the tick falls through.

