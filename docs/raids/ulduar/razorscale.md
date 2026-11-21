# Razorscale

## Harpoons are GameObject state, not auras

`SPELL_CHAIN_1..4` (49679, 49682, 49683, 49684) **no longer exist anywhere in the core**. Harpoons
fire `SPELL_HARPOON_SHOT_1..4` cast by `NPC_RAZORSCALE_CONTROLLER` out of
`go_razorscale_harpoon::OnGossipHello`. Each Harpoon Fire State (33282) owns one turret (two in
10-man, four in 25-man): every take-off breaks them all, the engineers wait out the 22 s fire aura
62696, then rebuild each as a **new GameObject with a new guid**, and at permanent ground (50%) they
are removed. `OnGossipHello` sets `GO_FLAG_NOT_SELECTABLE` only once the harpoon actually fires, so
readiness is that flag plus `GO_STATE_READY` (`IsHarpoonReady`), and the scan re-reads the guids every
tick. Hits count up with no time window: the 2nd (10-man) / 4th (25-man) grounds her however far apart.
`IsHarpoonFired()` and `HarpoonData::chainSpellId` were removed for testing an aura that could never
be present.

The tank debuff also moved: **Fuse Armor is 64821**, not 64771 (64774 is still the 5-stack `Fused
Armor`). 64771 is gone from the core, so the tank-swap check never fired. Threshold stays at 2 stacks.

Airborne, the raid kills the Dark Rune adds while the harpoon crews (`IsRazorscaleHarpoonCrew`: ranged
dps, never healers) fire the harpoons; the last hit grounds her, and the ground phase, knockdowns included, is the only
damage window. **The main tank is picked by max health, not spec**: `AssignRolesBasedOnHealth` flags
the living spec tank with the most, skipping anyone at `FUSEARMOR_THRESHOLD`, and runs while nobody
holds the flag and again once the main tank reaches the threshold. A pick is throttled to one per
`_roleSwapCooldown` (10 s) per bot, held in `RazorscaleInstanceState`, so a re-derivation cannot flip
it mid-phase. Harpoons need no such throttle: the spent flag lands in the same call that fires.

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

**The ranged stack walks to a slot, never onto a patch.** A ranged bot outside the stack circle
(12 yd round the arena centre, 18 yd round the first-landing offset) walks to its own slot on it,
bearing from its place in the group (`GetRazorscaleRangedStackSpot`). Not `MoveInside`: its
`GetFollowAngle` reads the master's group, so a master outside the raid (the DC console driver)
gives every bot angle 0. Window 5 put all 14 ranged and healers on one spot, and a patch there killed
9 of them 14 times. The slot turns along the circle, up to a quarter turn each way, off any patch the
dodge would move the bot out of, at the spot or on the walk; if none is clear the bot stays put. The
dodge fires on `GetDistance2d`, which subtracts both combat reaches: ~10 yd from a patch's centre,
not 7. So the spot clears `DEVOURING_FLAME_CLEAR_RADIUS` + both reaches +
`DEVOURING_FLAME_STAND_MARGIN`; short of that the dodge steps the bot off and the stack walk takes it
straight back, every tick.

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

## What a trace answers

`tools/botobs/bosses/razorscale.py <trace>`, no probes: `--phases` air and ground stretches with her
health; `--stack` per ground phase, where the stack walk sent ranged and healers (spots, and walks
ending within 10 yd of a live patch); `--fire` Devouring Flame damage by role, air and ground;
`--deaths` deaths by mechanic and phase.
