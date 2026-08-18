# Mimiron round 3: dodge interlock, phase 3 formation drift, grounded ACU, Bomb Bot snares

## Context

Two rounds of Mimiron work are committed (`30a4d29ee` positioning, `46695c8c9` docs); the tree is
clean. A further 25-man test surfaced eight defects. Several share a root cause, and two of them kill
bots outright.

The cheat-free constraint stands: no `HasCheat`, no `TeleportTo`, no `->Kill(`.

Reported, in the user's numbering:

1. Pets ignore the Aerial Command Unit in phase 3 while a Magnetic Core has it on the ground.
2. A melee bot died to the Laser Barrage in phase 2 despite the ranged dodge working.
3. Ranged oscillate through phase 3 instead of doing damage.
4. Bomb Bots are not immune to snares and nothing exploits that.
5. The phase 3 ranged wedge sits too close to the Aerial Command Unit, which is where Bomb Bots spawn.
6. Bots oscillate while taking up positions between phases.
7. Melee died to the barrage in phase 4 while sidestepping Proximity Mines. Proposal: melee stop
   dodging mines entirely.
8. A balance druid died to Rocket Strike while casting, without dodging.

---

## Verified facts

| Fact | Source |
|---|---|
| `PointMovementGenerator<T>::DoInitialize` returns without launching a spline when `unit->IsMovementPreventedByCasting()`, and `DoUpdate` calls `unit->StopMoving()` and returns early on the same test. A bot mid-cast **cannot be moved at all** | `PointMovementGenerator.cpp:36-42`, `:117-122` |
| `Unit::IsMovementPreventedByCasting()` is true for any `UNIT_STATE_CASTING` except a channel with `IsActionAllowedChannel()`. Instants never set it | `Unit.cpp:4372-4393` |
| `MovementAction::MoveTo` has its `CastStop`/`InterruptSpell` block **commented out**, and still returns `true` and stamps `LastMovement` with the full travel delay when the spline never starts | `MovementActions.cpp:169-237` |
| `IsWaitingForLastMove` only lets a move through on **strictly greater** priority, so one `MOVEMENT_FORCED` leg blocks every other `MOVEMENT_FORCED` move until its delay expires. `MaxWaitForMove` is 5000 ms, no `AC_*` override | `MovementActions.cpp:951-963`, `playerbots.conf:593` |
| `MovementPriority` has no band above `MOVEMENT_FORCED` | `LastMovementValue.h:18-25` |
| `PlayerbotAI::InterruptSpell()` cancels melee, generic and channelled casts; `SpellInterrupted` has no side effect beyond a redundant interrupt, so calling it repeatedly is free. Kara, Gruul, Mag and Naxx dodges all call it; no Mimiron node does | `PlayerbotAI.cpp:1395-1412`, `:4264-4290` |
| Rocket Strike 63041: base **4999999**, radius index 15 = **3 yd** | `spell.reference.csv`, `spellradius.reference.csv` |
| Shock Blast 63631: base **99999**, radius index 18 = **15 yd** | same |
| Mine Explosion 66351: base **8999**, 3 yd. Mines trigger on a player inside 1.9 yd and self-destruct after 35 s | `spell.reference.csv`, `boss_mimiron.cpp:1784-1830` |
| Bomb Bot detonation 63009: base **11999**, 3 yd | `spell.reference.csv` |
| `EVENT_SPELL_SHOCK_BLAST` schedules `EVENT_PROXIMITY_MINES_1` 8 s later, summoning **10** mines through 65347, radius index 18 = **15 yd** scatter around the MK II. Shock Blast repeats every 30 s. A stationary melee therefore expects ~0.4 mine hits per cycle, about **120 dps** | `boss_mimiron.cpp:1136-1147`, geometry |
| Magnetic Core aura 64436: duration index 18 = **20000 ms**, effect 2 is aura 87 (`MOD_DAMAGE_PERCENT_TAKEN`) base **49** → **+50 % damage taken** | `spell.reference.csv`, `spellduration.reference.csv` |
| Applying 64436 runs `DO_DISABLE_AERIAL`: `CastStop`, `AttackStop`, `REACT_PASSIVE`, hover cleared, `MoveFall`, `_events.DelayEvents(25s)`. The ACU's `UpdateAI` returns early for the whole aura, so **no adds spawn during the window** | `boss_mimiron.cpp:1586-1600`, `:1673` |
| A grounded ACU carries neither `UNIT_FLAG_NOT_SELECTABLE` nor `UNIT_FLAG_NON_ATTACKABLE`, so it is a normal attackable target | `boss_mimiron.cpp:1673`, `AttackersValue.cpp:131-156` |
| The phase 3 ACU has **no attack**. Its entire event list is `EVENT_SUMMON_{BOMB,ASSAULT,JUNK}_BOT` and the Firefighter fire bots; Plasma Ball is scheduled only in phase 4. It chases with `AttackStartCaster(who, 30.0f)`, holding **30 yd** from whoever holds threat | `boss_mimiron.cpp:1538-1543`, `:1690-1716` |
| The Magnetic Core is looted from an Assault Bot corpse that lasts **25 s**, and 64444 places its summon by nearest entry, so the core only lands from directly under the ACU | `boss_mimiron.cpp`, `UldActions_Mimiron.cpp:730-780` |
| Effective `AiPlayerbot.SpellDistance` = **28.5**, no `AC_*` override | `playerbots.conf:643`, worldserver env |
| `AnchorMimironFormation` slides the anchor toward the focus by `excess = dist + extent − maxRange`, `maxRange = 24.5`, and never clamps `excess` to `dist` — so the anchor can pass **through** the focus | `UldBossHelper.cpp:2226-2247` |
| Phase 3 wedge: `rangedDepth = 28.5 − 4 − 18 = 6.5` → `maxRows = 2` → `extent = 24`, so `excess = dist − 0.5`. The wedge anchor lands **0.5 yd from the Aerial Command Unit** whatever the room centre says | `UldBossHelper.cpp:2374-2392` |
| Ring path: anchor starts at the focus with `extent = 22 < 24.5`, so its slide branch never fires — only the room-radius clamp does anything | `UldBossHelper.cpp:2482-2489` |
| `GetMimironStagingMeleeSlot` builds its ring on `focus->GetPosition()`, and the phase 3→4 staging focus rides the chassis through its charge to (2755.77, 2574.95) and then the room centre | `UldBossHelper.cpp:2314-2345`, `boss_mimiron.cpp:618-704` |
| All three handovers converge on the room centre: VX-001 is summoned there, `ACUSummonPos` is (2744.650, 2569.460, 380.0), a defeated ACU is walked to (2744.65, 2569.46, 381.34), the chassis ends there, and `ULDUAR_MIMIRON_PHASE4_TANK_SPOT` is 1.4 yd off it | `boss_mimiron.cpp:280`, `:491-704`, `:1608-1615`, `UldBossHelper.cpp:56-58` |
| `SPELL_ELEVATOR_KNOCKBACK` fires at the room centre 11 s into the phase 1 handover, but VX-001 is not summoned until 17 s, so `GetMimironStagingFocus` returns null and nobody is staged there yet | `boss_mimiron.cpp:502-511` |
| Bomb Bot 33836 immunity set **−263** omits GRIP, ROOT, SNARE, STUN, FREEZE and KNOCKOUT; Assault Bot **−285** carries every one. Bomb Bot has **20000** HP and `speed_run` 1.14286 → 8.0 yd/s. Bomb Bots are phase 3 only | `acore_world` `creature_immunities`, `creature_template`, `creature_classlevelstats` |
| `CastSpellAction::GetTargetName()` is `"current target"`, so `DoSpecificAction(<spell>)` lands on whatever `mimiron set dps priority` gave the bot. `"concussive shot"`, `"frost shock"` and `"curse of exhaustion"` are all registered actions | `GenericSpellActions.h:25`, `HunterAiObjectContext.cpp:164`, `ShamanAiObjectContext.cpp:301`, `WarlockAiObjectContext.cpp:309` |
| `SnareTargetValue` already reads a chaser's victim through `GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE` and `ServerFacade::GetChaseTarget` | `SnareTargetValue.cpp` |
| `PlayerbotAI::IsMelee` is `!IsRanged`, and `IsRanged` defaults to `ContainsStrategy(STRATEGY_TYPE_RANGED)`. **Every healer spec is ranged**; melee is melee DPS plus warrior, DK, protection paladin and feral tanks | `PlayerbotAI.cpp:1921-1958` |
| Node relevances: barrage `RAID+5`, rocket strike / flames / frost bomb `RAID+4`, shock blast `RAID+3`, bomb bot `RAID+2`, magnetic core / plasma blast `RAID+1`, spread / focus / priority / pets `RAID`, proximity mine `RAID−1` | `UldStrategy.cpp:426-491` |
| `MimironP3Wx2LaserBarrageAction::Execute` returns **false** for a bot outside the danger band, handing the tick to every node below it, and ignores `MoveTo`'s return value | `UldActions_Mimiron.cpp:152-155`, `:222-232` |
| `ULDUAR_MIMIRON_BARRAGE_STEP` is 40°, which at 24 yd is a 16.8 yd arc — a **~2.4 s** leg. The cone sweeps ~10.6 °/s, so ~25° of sweep passes while the bot is committed to a fixed world point it cannot revise | `UldBossHelper.h:416`, `UldActions_Mimiron.cpp:222-232` |
| `MimironFrostBombAction` and `MimironBombBotAction` inherit `MoveAwayFromCreatureAction::Execute`; neither defines its own | `UldActions_Mimiron.h:157-194` |

### Root causes

**Casting freezes every dodge (8, and a contributor to 2).** No Mimiron node interrupts. A bot with a
cast in flight has its spline discarded by `PointMovementGenerator`, while `MoveTo` reports success and
stamps a `LastMovement` delay that blocks retries for a leg it never walked. Rocket Strike is 5,000,000
damage in 3 yd, so this is fatal on the first occurrence.

**The barrage yields the tick, and what runs next is cone-blind (2, 7).** The barrage action returns
`false` when the bot is clear. Rocket strike (`RAID+4`) and shock blast (`RAID+3`) then flee on a
bearing derived only from the hazard they are escaping, and the mine node (`RAID−1`) sidesteps to any
bearing at all. Worse, rocket strike and shock blast issue at `MOVEMENT_FORCED`, and
`IsWaitingForLastMove` needs *strictly greater* priority, so their leg blocks the barrage dodge's own
`MOVEMENT_FORCED` move for its whole duration — up to ~1.4 s for a 10 yd rocket step and ~2.6 s for an
18 yd shock-blast flee. The barrage action ignores `MoveTo`'s return, holds the tick and re-checks
every 100 ms, so the bot stands inside the cone taking 20000 every 250 ms until the lock expires.

**Melee live in the minefield (7).** Ten mines scatter within 15 yd of the MK II every 30 s, which is
where melee have to stand. The mine node fires more or less continuously, costs uptime, and in phase 4
walks them into the barrage — for about 120 dps of avoided damage.

**The phase 3 wedge is pinned to the Aerial Command Unit (3, 5).** `AnchorMimironFormation` slides the
anchor `dist − 0.5` yd toward the ACU, so the "room centre" anchor is a fiction — the wedge tracks the
ACU exactly. The ACU in turn holds 30 yd from its victim, which is a bot in that wedge. Raid and boss
chase each other around the room indefinitely, and Bomb Bots — which spawn on the ACU — are handed a
raid that is closing on them. The same routine can drive `excess` past `dist` and put the anchor on the
far side of the focus.

**Staging rings track a moving mech (6).** `GetMimironStagingMeleeSlot` centres on the focus rather
than the room centre that all three handovers converge on, so melee chase the chassis through its
charge script.

**Nothing uses the Magnetic Core window (1).** For 20 s the ACU is stationary, passive, on the ground
and taking +50 % damage, with add spawns suppressed for 25 s. The pet node only looks for adds, and
`IsAllowedTarget` refuses the ACU to melee outside phase 4 unconditionally.

**Bomb Bots are snareable and nothing snares them (4).**

---

## Decisions taken

From the grilling rounds, all settled:

- **Magnetic Core window**: pets and melee switch to the grounded ACU. Ranged keep the add priority and
  reach it only if it grounds inside range — they do **not** reposition for it, because the phase 3
  ACU has no attack, so range on it matters for nothing else. Tanks stay on the Assault Bot: 189,000 HP
  and nothing else will hold it.
- **Where it grounds**: wherever the ACU happens to be. The carrier does not wait for it to drift near
  the raid — the corpse it loots from lasts 25 s, and losing a core outright costs far more than melee
  jogging 30 yd.
- **Bomb Bot snares**: hunter, shaman, warlock only — concussive shot, frost shock, curse of
  exhaustion. Roots excluded: Entangling Roots and Frost Nova break on the first hit, and hitting it is
  the plan. Target-driven off `"current target"`, with a **15 yd floor measured from the Bomb Bot's own
  victim**, falling back to the caster when it has none.
- **Mines**: melee never dodge them, and the node is suppressed for everyone while the barrage window
  is live.
- **Interrupts**: lethal dodges only — Laser Barrage, Rocket Strike, Shock Blast, Firefighter flames
  and Frost Bomb. Mines and Bomb Bots keep casting through.
- **Cone safety is predictive**, not instantaneous: a destination is judged against the cone as it will
  be when the leg *lands*, not as it is when the move is issued.
- **The barrage step size is re-derived from simulation** before anything ships. See B0.

---

## Work items

### B0 — Re-validate the barrage step against the real movement lock (do this first)

The 497,664-position sweep that cleared the current barrage model stepped at 0.1 s and let the bot
re-evaluate every step. The shipped code commits to a 40° leg — ~2.4 s at 24 yd — during which
`IsWaitingForLastMove` refuses its own next `MOVEMENT_FORCED` move, the cone sweeps ~25°, and in phase
4 the apex itself moves. **The validated model and the shipped one are not the same bot.**

Extend the existing harness (`scratchpad/sim_barrage.py`) to model legs properly: issue a bearing
target, hold it for `MoveDelay(arc) − reactDelay` capped at `MaxWaitForMove`, refuse re-issue during
that window, and move the apex under it for the phase 4 cases. Re-run the full sweep — every DB Target
phase, bot bearing, radius 14–24 yd, chassis offset to 30 yd in eight directions — and pick
`ULDUAR_MIMIRON_BARRAGE_STEP` from the result rather than keeping 40° by default.

If 40° still clears, record the number and leave the constant alone. If it does not, the fix is one
constant plus whatever the sweep says about aiming the leg at the bearing the ring will have on
arrival rather than the one it has now.

### D1 — Lethal dodges interrupt the cast

Add `botAI->InterruptSpell()` immediately before the move in:

- `MimironP3Wx2LaserBarrageAction::Execute`, on the tick that issues a leg.
- `MimironFleeAction::MoveAwayClearOfMines`, gated on a new `bool interrupt` parameter so the mine
  dodge — the only caller passing `MOVEMENT_COMBAT` — keeps its cast. Covers rocket strike and shock
  blast.
- `MimironDodgeFlamesAction::Execute`, before its `FleePosition`.
- `MimironFrostBombAction`: no `Execute` of its own, so add one that interrupts and delegates to
  `MoveAwayFromCreatureAction::Execute`.

Comment to carry, once, at `MoveAwayClearOfMines`: the point movement generator drops the spline
outright for a unit that `IsMovementPreventedByCasting`, and `MoveTo` still reports success and stamps
a `LastMovement` delay, so a casting bot both fails to move and blocks its own retries for a leg it
never walked.

### D2 — Nothing steps into the barrage

New helper beside the existing spot predicates:

```cpp
// True when `dest` is still outside the swept union `travelSeconds` from now. Predictive on purpose:
// a MOVEMENT_FORCED leg holds the lock for its whole duration - up to 2.6 s for an 18 yd Shock Blast
// flee - and the cone turns about 10.6 degrees a second, so "safe when the move was issued" is the
// wrong question. Measured clockwise from the ignition centreline in [0, 2pi), never folded: the band
// is 240 degrees wide at the room centre and wider off it.
bool IsMimironSpotBarrageSafe(Player* bot, Position const& dest, float travelSeconds);
```

Resolve VX-001 with `GetFirstAliveUnitByEntry`, take `GetMimironBarrageWindow`, return `true` when the
window is invalid, otherwise extend the band by `window.rate * travelSeconds` before applying the
action's own test to `dest`'s bearing.

Wire it into `MoveAwayClearOfMines` next to `IsMimironSpotMineSafe`, passing
`distance / bot->GetSpeed(MOVE_RUN)`. Two adjustments to that routine while there:

- Widen the bearing fan from `±90°` to `±112.5°`, since two stacked filters can empty the first
  quadrant and standing still is 5,000,000 damage.
- Reject any destination that is not strictly farther from the hazard than the bot already is. Past
  ~120° off the escape bearing the geometry turns back inward, and the widened fan would otherwise
  offer a step toward the thing being fled.

Both `MoveAwayClearOfMines` callers that matter are covered: rocket strike in phases 2 and 4, and shock
blast in phases 1 and 4. Note for the record — in phase 4 VX-001 rides the chassis on a zero-offset
seat, so fleeing the MK II is already radial from the cone apex and bearing-preserving; the filter is a
no-op there today and is added so that correctness does not rest on that coincidence.

### D3 — Mines: melee never, nobody during the barrage

`MimironProximityMineTrigger::IsActive` gains two gates ahead of the proximity test:

```cpp
// 9000 in 3 yd, and mines self-destruct after 35 s regardless. Ten scatter within 15 yd of the MK II
// every 30 s, which is exactly where melee have to stand, so the sidestep fired more or less
// continuously to avoid about 120 dps - and in phase 4 it walked them into the barrage.
if (botAI->IsMelee(bot))
    return false;

// The barrage action yields the tick to everything below it once a bot is clear, and this node knows
// nothing about the cone. Suppressing it outright beats filtering: a mine is survivable, the cone is
// not.
MimironP3Wx2LaserBarrageTrigger barrage(botAI);
if (barrage.IsActive())
    return false;
```

`IsMimironSpotMineSafe` stays where it is used as a destination *filter* — by `MoveAwayClearOfMines`
and by `IsMimironSpotSafe` for the spread. Filtering a destination costs nothing and cannot oscillate.

### D4 — Phase 3 formation stops chasing the boss

`GetMimironPhase3Slot` anchors on `ULDUAR_MIMIRON_ROOM_CENTER` unconditionally. No slide, no
grounded-ACU special case.

Comment to carry: the wedge used to be slid toward the Aerial Command Unit until its outer row was in
casting range, but `extent` is measured omnidirectionally while the wedge only occupies 120° of it, so
the slide always overshot to within half a yard of the boss and the formation tracked it exactly. The
ACU chases at 30 yd from its victim and its victim stands in that wedge, so raid and boss circled the
room together. It also has no attack in this phase — it only summons — so there is nothing range on it
buys outside the Magnetic Core window, which melee and pets cover on foot.

Fixing the anchor is also the fix for the Bomb Bots. With the wedge held on the room centre the ACU
keeps its own 30 yd standoff, so a Bomb Bot spawning on it has ~30 yd to cross at 8.0 yd/s — about
**3.7 s** of free fire on a 20,000 HP add, against approximately none today.

`AnchorMimironFormation` loses its slide branch and keeps only the room-radius clamp; the ring path is
the sole remaining caller and its slide never fired anyway. That removes the `excess > dist` overshoot
rather than patching it. Rename to match what is left.

`ULDUAR_MIMIRON_PHASE3_MIN_RADIUS` stays at 18: pushing the wedge outward does not buy Bomb Bot time,
because the ACU keeps 30 yd from its victim wherever that victim stands, and 18 yd is what keeps ranged
off the summon arms.

### D5 — Handovers form up on a fixed point

`GetMimironStagingMeleeSlot` builds its ring on `ULDUAR_MIMIRON_ROOM_CENTER`, not
`focus->GetPosition()`. `GetMimironSpreadSlot` likewise passes the room centre as the ring anchor when
`staging` is set.

Take the ring radius as `max(ULDUAR_MIMIRON_STAGING_MELEE_RADIUS, focus->GetCombatReach() + 1.0f)` so
melee do not stage inside the chassis model, which is the largest of the three.

Comment to carry: every handover converges on the room centre — VX-001 is summoned there, the ACU
spawns and is walked back there, the chassis ends there — but the focus is mid-script for most of the
window, so a ring pinned to it drags the raid through the charge waypoints.

The staging focus keeps its present job: it decides the shape (wedge for an ACU, ring otherwise) and
whether this is the phase 4 assembly. Only the anchor changes. Nothing is staged into the phase 1
elevator knockback, because the focus is still null when it fires at 11 s and VX-001 does not appear
until 17 s.

### D6 — The Magnetic Core window

Add to the Mimiron enum in `UldBossHelper.h`:

```cpp
SPELL_MIMIRON_MAGNETIC_CORE_AURA = 64436,  // 20 s grounded, +50% damage taken; not 64668
```

New helper:

```cpp
// The 20 s a Magnetic Core buys: the Aerial Command Unit is on the floor, passive, taking +50 % damage,
// and its own UpdateAI is short-circuited so nothing new spawns for 25 s. It is the only stretch of
// phase 3 where the boss can be killed at all.
bool IsMimironAcuGrounded(PlayerbotAI* botAI);
```

Three call sites:

- `MimironSetDpsPriorityAction::IsAllowedTarget`, `NPC_AERIAL_COMMAND_UNIT` branch: melee are allowed
  while it is grounded. The existing `IsMimironPhase4` clause stays.
- `MimironSetDpsPriorityAction::BuildPriorityList`: while grounded, melee get the ACU pushed **ahead
  of** the adds. Ranged keep the existing order and arrive on their own once the surviving adds are
  dead, since no more spawn.
- `MimironPetControlAction::Execute`, phase 3 branch: `CommandPetAttack(acu)` while grounded, ahead of
  the Assault/Junk/Bomb scan.

Tanks are untouched — `MimironSetDpsPriorityTrigger` returns false for them, so they hold the Assault
Bot through the window by construction. That is intended, not an oversight; note it in the comment so
the next reader does not "fix" it.

### D7 — Snare the Bomb Bots

New node `mimiron slow bomb bot`, wired at `ACTION_RAID + 2`.

`MimironSlowBombBotTrigger::IsActive`:

- the bot's class is hunter, shaman or warlock, and it knows the spell;
- `"current target"` is an alive Bomb Bot;
- the Bomb Bot is more than **15 yd** from its own victim — `GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE` then `ServerFacade::GetChaseTarget`, falling back to the caster's own distance when it has no victim yet;
- `botAI->HasAura(spell, target)` is false.

`MimironSlowBombBotAction::Execute`: `botAI->DoSpecificAction(spell, event, true)`, returning its
result so a bot with the spell on cooldown drops through to its rotation instead of eating the tick.

| Class | Spell |
|---|---|
| Hunter | `concussive shot` |
| Shaman | `frost shock` |
| Warlock | `curse of exhaustion` |

Put the class-to-spell lookup in one place and have both trigger and action call it.

Comments to carry: Bomb Bot immunity set −263 omits SNARE, ROOT, STUN, FREEZE, GRIP and KNOCKOUT,
every one of which the Assault Bot's −285 carries — so a Bomb Bot is snareable and the other adds are
not. The distance floor is measured from the Bomb Bot's victim rather than the caster because a hunter
25 yd away would otherwise happily spend a global snaring one that is already two yards from a healer.
No target handling of its own: `mimiron set dps priority` already puts ranged DPS on an in-range Bomb
Bot, and reading `"current target"` is what keeps this node from fighting it — which also means healers
never snare, deliberately.

### D8 — Docs

`docs/raids/ulduar.md`, Mimiron section:

- That a casting bot cannot be moved at all — `PointMovementGenerator` discards the spline while
  `MoveTo` still reports success — so lethal dodges have to interrupt. Worth stating as a general rule.
- That `IsWaitingForLastMove` requires strictly greater priority, so two `MOVEMENT_FORCED` dodges in
  one encounter can deadlock, and the fix is to make the lower one cone-aware rather than outrank it.
- That cone safety has to be judged at leg *arrival*, with the leg durations that make it matter.
- The measured hazard numbers behind the survivable/lethal split: Rocket Strike 5,000,000 in 3 yd,
  Shock Blast 100,000 in 15 yd, Mine 9,000 in 3 yd self-destructing at 35 s (~120 dps to a stationary
  melee), Bomb Bot 12,000 in 3 yd.
- Why the phase 3 wedge is anchored on the room centre: the slide measured `extent` omnidirectionally
  against a 120° wedge, landed half a yard from the ACU, and the ACU's own 30 yd chase closed the loop.
  Plus the fact that makes it safe — the phase 3 ACU has no attack, only summons.
- The Magnetic Core window as the phase 3 burn: 20 s, +50 % damage, spawns suppressed 25 s; melee and
  pets take it, ranged keep the adds, tanks keep the Assault Bot.
- Bomb Bot immunity set −263 against the Assault Bot's −285, and which three classes exploit it.
- That every handover converges on the room centre, and anchoring staging anywhere else drags the raid
  through the chassis charge script.
- The B0 result: what leg length the barrage dodge was actually validated at, and that the earlier
  figure assumed continuous re-evaluation the movement lock does not allow.

Mirror this plan to `docs/plans/mimiron-cheat-free-rebuild/mimiron-dodge-interlock.PLAN.md`.

---

## Files touched

- `src/Ai/Raid/Uld/Util/UldBossHelper.h` / `.cpp` — D2 helper, D4 anchor and the
  `AnchorMimironFormation` reduction, D5 staging anchors and radius, D6 enum and helper, B0 constant
- `src/Ai/Raid/Uld/Action/UldActions_Mimiron.h` / `.cpp` — D1 interrupts, D2 filter and fan changes,
  D6 pet and priority branches, D7 action
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.h` / `.cpp` — D3 mine gates, D7 trigger
- `src/Ai/Raid/Uld/UldActionContext.h`, `UldTriggerContext.h`, `UldStrategy.cpp` — D7 wiring
- `docs/raids/ulduar.md`, `docs/plans/mimiron-cheat-free-rebuild/mimiron-dodge-interlock.PLAN.md` — D8

## Verification

The module cannot be compiled from this checkout; the worldserver build is the only compile path.

1. **B0 sweep** clears at whatever step size it selects, with legs and the movement lock modelled.
   Record the position count and the step.
2. Build, then pull Mimiron.
3. **Casting bots dodge.** Watch a balance druid and a mage through a Rocket Strike, a Shock Blast and
   a barrage. Each time they are in danger mid-cast the cast is cancelled and they move. No Rocket
   Strike deaths at all — 5,000,000 damage leaves no ambiguity about whether this landed.
4. **Nothing flees into the beams.** Through a phase 4 barrage with mines down and a Rocket Strike
   marker out, confirm no escape path crosses the cone and nobody stands frozen inside it waiting out
   another node's movement lock.
5. **Melee and mines.** Melee never sidestep a mine in phase 1 or 4 and stay on the mech. Ranged still
   sidestep, except while Spinning Up or the barrage is running. Healers keep dodging — they are
   `IsRanged`.
6. **Phase 3 formation.** Ranged take a wedge slot and hold it; the raid does not drift behind the ACU.
   Measure ACU-to-nearest-ranged across the phase: it should sit near 30 yd rather than collapsing.
7. **Bomb Bots.** They cross ~30 yd of open floor. Hunters, shamans and warlocks snare the one they are
   shooting while it is still more than 15 yd from whoever it is chasing, and it dies before contact.
   Confirm nobody snares an Assault Bot and healers do not snare at all.
8. **Magnetic Core window.** Melee run in and pets switch for the full 20 s; ranged finish surviving
   adds and then join; the tank stays on the Assault Bot. Compare the ACU's health loss across the
   window against a run before the change. Note how much of the window melee spend travelling — if it
   is most of it, the carrier-wait option comes back on the table.
9. **Handovers.** Bots walk once to their staging spot and stop. No pacing while the chassis charges
   through its waypoints in the phase 3→4 handover, and nobody staged into the elevator knockback.
10. Cheat-free grep: `HasCheat`, `TeleportTo`, `->Kill(` return nothing across `UldTriggers_Mimiron.*`,
    `UldActions_Mimiron.*` and the Mimiron parts of `UldBossHelper.*`.
11. Full clear on normal, then Firefighter.

## Known and deliberately out of scope

- Ranged keep the add priority during the Magnetic Core window and never reposition for it. Costs some
  of the +50 % when it grounds out of range.
- The carrier grounds the ACU wherever it is, rather than waiting for it to drift near the raid. The
  25 s corpse timer is the reason; revisit only if step 8 shows melee spend the window walking.
- Only hunter, shaman and warlock snare. Mage cone of cold needs 10 yd, shadow priest mind flay is a
  channel, DK chains of ice is on a melee class — all defensible, none included.
- `mimiron set dps priority` still only lists Bomb Bots already inside 28.5 yd, so one crossing the room
  is nobody's target until it arrives. That also caps when the snare can fire.
- `MovementAction::MoveTo` still reports success for a move that never starts. Fixing that in shared
  code would touch every strategy in the module; Mimiron works around it by interrupting first.
- Group churn still reshuffles every formation index when a member dies or is resurrected. Not
  reported, not addressed.
- W6 Napalm Shell spread, carried from the first round, still awaiting a decision.


---

## As built — where the implementation departed from the plan

**B0 answered the opposite of what the plan expected.** The step size is not a safety dial at all: the
leg-and-lock sweep gives **0 hits of 62,208** at 40, 30, 20, 15 and 10 degrees alike, and smaller steps
are measurably *worse* under a moving apex rather than better. Every failure the harness can produce
needs sustained apex drift — 0.53 % of positions at 2 yd/s — and raising the margin from 15 to 30
degrees only reaches 0.34 % and then plateaus, so it is not a clearance problem either.
`ULDUAR_MIMIRON_BARRAGE_STEP` stays at 40 and `ULDUAR_MIMIRON_BARRAGE_MARGIN` at 15.

What actually prevents the drift case was already in the tree without its reasoning written down:
`Unit::GetMeleeRange` is `ownerReach + targetReach + 4/3` = 8 + 1.5 + 1.33 = **10.83 yd**, and the
phase 4 main tank's barrage floor is reach + 1.5 = **9.5 yd**, inside it, so the chassis never chases
and the apex holds still. That is now recorded on the constant, because it is load-bearing for the
whole raid rather than a tank convenience.

**`IsMimironSpotBarrageSafe` takes the window instead of reading it.** As planned it resolved VX-001
and called `GetMimironBarrageWindow` itself, which runs a grid scan for the DB Target — and
`MoveAwayClearOfMines` tests up to twelve bearings against the same cast. The window is now resolved
once per flee and passed in.

**The flee fan stops at ±112.5°, not ±180°.** Past roughly 120° off the escape bearing the geometry
turns back inward and the "escape" closes on the hazard. There is also an explicit guard that the
destination must be strictly further from the hazard than the bot already is, since collision handling
can shorten a step to nothing.

**`AnchorMimironFormation` was deleted rather than conditioned.** With the phase 3 wedge hard-anchored
on the room centre and the ring anchored on the focus, the slide branch had no caller whose geometry
could ever trigger it, so what remains is `ClampMimironAnchorToRoom` — the room-radius clamp alone.
That removes the unclamped-excess overshoot rather than patching it. `GetMimironPhase3Slot` lost its
`focus` parameter with it.

**No grounded-ACU wedge anchor.** The plan had the wedge jump to the Aerial Command Unit for the
Magnetic Core window; the grilling settled on ranged not repositioning at all, and the phase 3 unit
having no attack makes that free. `IsMimironAcuGrounded` therefore feeds only the melee and pet
targeting.
