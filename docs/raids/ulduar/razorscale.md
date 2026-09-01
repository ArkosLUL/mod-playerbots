# Razorscale
## Harpoons are GameObject state, not auras

`SPELL_CHAIN_1..4` (49679, 49682, 49683, 49684) **no longer exist anywhere in the core**. Harpoons
fire `SPELL_HARPOON_SHOT_1..4` cast by `NPC_RAZORSCALE_CONTROLLER` out of
`go_razorscale_harpoon::OnGossipHello`; the GameObjects are summoned per ground phase by the
controller (two in 10-man, four in 25-man) and a spent one carries `GO_FLAG_NOT_SELECTABLE` until it
is rebuilt. Readiness is that flag — `IsHarpoonFired()` and `HarpoonData::chainSpellId` are gone,
they had been testing an aura that could never be present.

The tank debuff also moved: **Fuse Armor is 64821**, not 64771 (64774 is still the 5-stack `Fused
Armor`). 64771 is gone from the core, so the tank-swap check never fired. Threshold stays at 2 stacks.

## Devouring Flame is 5 yd, and the skull has one owner

The patch is NPC 34188 carrying 64709, a 2 s periodic trigger of 64704 (64733 in 25-man), **whose
damage radius index is 8 = 5.0 yd**. Bots clear `DEVOURING_FLAME_CLEAR_RADIUS` (7 yd) so a step
actually leaves the patch instead of stopping on its edge, and `DevouringFlameBlocks()` rejects any
destination covered by another one — she drops these every 6–12 s and they stack up.

Clearing is only half of it. `razorscale avoid devouring flames` also **holds the tick without moving**
while the spot a melee bot would walk back to is on fire; `RazorscaleMultiplier` zeroes the generic
movers and `avoid aoe` for the same window. Both release the moment the tank drags her clear, so
nobody stands out the patch's full life. A permanent veto here is the freeze bug.

`razorscale kill target action` is the **only** thing that sets the skull: the boss whenever she is
on the floor, otherwise Sentinel → Watcher → Guardian. `DpsTargetValue` prefers the RTI target, so a
skull left on an add is the whole raid left on an add. Moon stays on the boss while she is airborne —
it is excluded from every DPS target scan — and is cleared on landing.

The generic pet-attack node is commented out engine-wide (`CombatStrategy.cpp`), so pets keep whatever
they last hit unless a script re-orders them: `razorscale pet control action` puts them on the adds
while she flies and on her when she lands, and always reports failure so the tick falls through.

