# Mimiron Firefighter: hold the boss on the anchor, survive Shock Blast, rotate off the fire

## Context

Three Firefighter attempts on 2026-09-10, the first pulls after `cd83b829a` (west phase-1 anchor,
ranged stack, fire dodge at `MOVEMENT_FORCED`). Traces
`603_1_mimiron_1789063575` (**a1**), `1789063939` (**a2**), `1789064624` (**a3**) in
`/azerothcore/env/dist/logs/botobs/` inside `ac-worldserver`.

**The west reposition did exactly what it was built to do.** Phase-1 flame damage taken inside 20 yd
of the room centre went from 70 % to **0.0 %** in all three pulls, and **zero** flame nodes were
within 24 yd of the room centre at the phase-1/2 handover. Both pulls that survived phase 1 reached
phase 2 on clean ground. Raid DPS in phase 1 is now **98,485 / 98,845** (a2 / a3), and the MK II
died at 119.3 s in both.

It also cost more than it bought, in three linked ways.

| | a1 | a2 | a3 |
|---|---|---|---|
| outcome | **wipe in phase 1** @112.9 s | phase 2 wipe @304.4 s | phase 2 wipe @272.5 s |
| phase-1 damage taken /s | 18,368 | 15,840 | 19,689 |
| phase-1 deaths | 31 | 9 | 9 |
| phase-1 raid dps | 57,397 | 98,485 | 98,845 |
| ranged %cast, phase 1 | 34.8 % | 58.3 % | 48.8 % |

Yesterday phase 1 took 9,910-12,071 /s. It now takes 15,840-19,689 /s. Ranged cast uptime fell from
63.5-65.5 % to 34.8-58.3 %.

### Finding 1 — Shock Blast one-shots the stack, and the fire dodge is why

Shock Blast (63631) does **82,450-109,125 per hit** to bots with 22,000-30,000 HP. Measured hit
radius: **max 15.2 yd** from the MK II, so the existing `ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST = 18`
is the right number. Only 1-3 casts land per pull, but each one that lands is a mass casualty:

- a1 t=38.4: **11 of 25 dead in one cast**, victims 5.2-14.6 yd out. That is the whole pull.
- a3 t=68.0: 7 dead, all ranged, 9.0-15.2 yd out.

The bots did try to leave. The move stream for a3's seven victims:

```
64.5-65.8  mimiron shock blast action   forced  ok=1     <- escape issued, they are running
66.2-67.5  mimiron dodge flames action  forced  ok=1     <- fire dodge cancels it, they turn round
67.5-68.1  mimiron shock blast action   forced  ok=1     <- re-issued, too late
68.0       Shock Blast lands                             <- all seven die
```

`dodge flames` is `ACTION_RAID + 4`; `shock blast` is `ACTION_RAID + 3`. Both issue at
`MOVEMENT_FORCED`, and `IsWaitingForLastMove` needs *strictly greater* priority, so neither can
preempt the other and the higher-relevance node wins every contested tick. Flames tick for a few
thousand and fire ~2,500 times a pull; Shock Blast one-shots and fires 5 times. The ordering is
backwards. In a3's fatal window ranged logged **30 `shock locked`** flee notes.

**This is the regression risk flagged as check 7 of the previous plan, and it landed.** Raising the
fire dodge to `MOVEMENT_FORCED` put it in contention with every other killer: `rapidburst locked`
327 (a2) / 194 (a3) against 175 / 120 issued; `rocket locked` 58 / 74; `frostbomb locked` 85.

### Finding 2 — the geometry has no margin, because the boss is never on the anchor

`sPlayerbotAIConfig.spellDistance` is **28.5** (conf default; no `AC_*` override exists), minus
`ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN` 4.0 = **24.5 yd of reach**. Shock Blast reaches 15. Ranged
therefore have a ~6 yd band to live in, and the stack was placed 20.2 yd from the *tank anchor* on
the assumption the boss sits on it. It does not:

| | a1 | a2 | a3 |
|---|---|---|---|
| MK II distance off the tank anchor | med 6.9, p90 18.2, max 48.3 | med 5.0, p90 22.8, max 40.0 | med 10.0, p90 16.8, max 37.8 |
| MK II to the stack point | med 22.0, **min 15.3** | med 19.6, **min 14.7** | med 16.7, **min 12.5** |
| ranged within 15 yd of the MK II | 30 % of samples | 34 % | **69 %** |

### Finding 3 — the tank never establishes threat, so the boss will not follow

The tank engages at 13-15 s and starts walking to the anchor at ~16 s with about three seconds of
threat. The MK II immediately switches to a melee DPS and **stops**:

```
a1  13.5 MKII engaged, Bulwark on it at 2.4-6.9 yd
    16.2 MKII -> Justice (melee dps)
    16.4 Bulwark leaves for the anchor
    16.9 MKII parks at (2737.8, 2582.8), 15 yd from the room CENTRE, and stays there
    21.4 Bulwark reaches the anchor, 48.3 yd from a boss that never came
```

Time on target across the pull: Bulwark **64.7 % / 25.6 % / 20.4 %**. The boss bounces to Justice,
Obliteration, Ecoterrorist, Shadow, Trueshot and the two human players. The tank then yo-yos —
`mimiron arc spread action` pulls him to the anchor, `reach melee` pulls him back — 8/8/4 `reach
melee` moves refused with `wait` in phase 1.

**Tricks of the Trade is causing this, not merely failing to prevent it.** The rogues cast it on the
top melee DPS at 13.5-15.5 s in every pull, and that exact bot pulls the boss 2-3 s later:

| pull | Tricks target | pulls the boss at |
|---|---|---|
| a1 | Justice @13.5, 15.5 | Justice @16.2 |
| a2 | Obliteration @15.3 | Obliteration @17.4 |
| a3 | Justice @14.9 | Justice @17.8 |

Cause: `TricksOfTheTradeTargetValue::TankNeedsRedirect`
(`src/Ai/Class/Rogue/RogueValues.cpp:15`) has an opener branch keyed on `AI_VALUE(time_t, "combat
start time")`, which `PlayerbotAI::ChangeEngineOnCombat` (`src/Bot/PlayerbotAI.cpp:1462`) only sets
when the `wait for attack` strategy is running. In a raid it is always 0, so the opener branch never
fires, the fallback `myThreat > tankThreat * 0.5` is false for a rogue who has not attacked yet, and
Tricks goes to the hardest-hitting melee.

**Misdirection is never cast at all.** Both hunters know 34477 (verified in `character_spell`). It
is vetoed: `mimiron threat redirect guard` 9/12/11 times and `uld threat redirect` 0/3/5 times.
`Engine::DoNextAction` reports a multiplier-zeroed action as `IMPOSSIBLE` (`Engine.cpp:232-283`),
which is why the act stream looked like a broken spell.

### Finding 4 — the fire converges on the stack, exactly as designed, and nobody can leave

The stack is a fixed clump, and `npc_ulduar_flames_initial::SpreadFlame` grows every chain toward
whoever is nearest its head. Phase-1 Flames damage in a3 was **538,816 (25.8 % of everything taken)
with the median victim 3.4 yd from the stack point**; 5 nodes within 10 yd of it and 11 within 20 yd
at t=73. Ranged took 329k of that, healers 191k, melee 15.7k.

The dodge throws them out and the formation walks them back. There is no mechanism to move the
clump itself.

### What did *not* go wrong

Napalm Shell victims per cast: **1.20 / 1.79 / 1.33** (1.38 / 2.20 before). The 3 yd disperse is
fine and is not the tuning knob it was flagged as. Phase 2 remains a Heat Wave + Rapid Burst
healing problem (78 % / 78 % of phase-2 damage) and is out of scope here.

---

## Change 1 — the tank holds the boss before dragging it

`DeriveMimironSpreadSlot`'s `p1tank` branch (`UldEncounter_Mimiron.cpp:895`) hands the anchor back
from the first tick of phase 1. Gate it on threat instead:

- Return **false** for the main tank in phase 1 until he owns the MK II — `leviathanMkII->GetVictim()
  == bot` **and** his threat is at least `ULDUAR_MIMIRON_TANK_THREAT_LEAD` (1.3) times the highest
  non-tank threat on it. `ThreatManager::GetThreat` is the same API `LowTankThreatTrigger`
  (`src/Ai/Base/Trigger/GenericTriggers.cpp:250`) already uses.
- **Latch it one-way** per pull in the shared Mimiron encounter state, so a mid-phase threat dip
  does not send him back and restart the drag. Clear it where the other Mimiron per-pull state is
  cleared (`mimiron reset encounter state action`).
- Cap the hold with `ULDUAR_MIMIRON_TANK_HOLD_MAX_MS` (10000) so a threat table that never resolves
  cannot stall the fight at the pull spot forever.
- While unlatched the tank has no slot, so `reach melee` owns him and he stands on the boss — which
  is the wanted behaviour.

Then stop the yo-yo: extend `MimironChargeGuardMultiplier`
(`UldMultipliers_Mimiron.h:26`) — or add a sibling guard — to zero `reach melee` and `reach spell`
for the **main tank** once the latch is set and he has a phase-1 anchor slot. Without this he keeps
being pulled back off the anchor at `ACTION_HIGH`.

Note the drag itself is safe: the tank spot is 51.5 yd from Mimiron's spawn (2742.53, 2560.99) and
he evades past 80 yd (`boss_mimiron.cpp:394-398`), so there is 28 yd of margin.

## Change 2 — a deliberate Mimiron threat redirect

Do **not** lift the two guards. The right shape already exists: `RaidRedirectThreatAction`
(`src/Ai/Raid/RaidRedirectThreat.h:24`) is the shared base that encounters subclass to answer "which
tank should own the threat"; `HodirRedirectThreatAction` (`UldActions_Hodir.h:101`) is the closest
template, and `UldThreatRedirectMultiplier` blocks the generic class nodes on exactly the bosses
that have their own action. Mimiron is on that list but has no action, which is the hole.

- Add `MimironRedirectThreatAction : RaidRedirectThreatAction` and `MimironRedirectThreatTrigger`.
- `isUseful()` — hunter or rogue only, as Freya does.
- `GetRedirectTank()` — **phase 1 only** (`NPC_LEVIATHAN_MKII` alive, `NPC_VX001` and
  `NPC_AERIAL_COMMAND_UNIT` not): return the group main tank. Return `nullptr` in later phases and
  leave `UldThreatRedirectMultiplier`'s VX-001 / Aerial Command Unit entries alone — the phase-3
  two-tank split that reasoning describes is real.
- `GetThreatDumpTarget()` — the MK II.
- Register at `ACTION_RAID + 1`, matching the other Ulduar redirect actions.
- Delete `MimironThreatRedirectGuardMultiplier` and its registration. Its whole reason was the
  Plasma Blast tank swap, which Change 3 retires.
- Suppress the generic smart-target `tricks of the trade` node during Mimiron phase 1 so it cannot
  feed a DPS ahead of the new action. `UldThreatRedirectMultiplier` only matches
  `CastMisdirectionOnMainTankAction` / `CastTricksOfTheTradeOnMainTankAction`, so the plain node is
  not covered today.

The generic `TankNeedsRedirect` opener bug (Finding 3) is **left alone by decision** — it changes
rogue behaviour in every encounter and only Mimiron has a trace to check it against. Worth its own
change later.

## Change 3 — deliberate defensives on Plasma Blast, one at a time

Plasma Blast is a 6-tick, ~5 s burst on a **43,407 HP** tank, on a hard 22 s cycle. Measured windows
and totals:

```
t=27  49  72  95  117            97,363 / 72,978 / 39,381 / 59,444   (a1)
                                 86,941 / 86,907 / 56,999 / 114,301  (a2)
                                115,815 / 66,401 / 53,970 / 118,852 / 50,167  (a3)
tank HP trough:  a1 51/44/56/28 %   a2 55/41/20/44 %   a3 74/51/11/51 %
```

a1's tank died at 53.3, 73.6 and 96.7 s — the tail of three consecutive windows. Today's mitigation
is reactive: Ardent Defender and Divine Protection fire on health thresholds, after the tank is
already low, and Pain Suppression appeared once in three pulls (a2 @26.2 s, which was correctly
timed and is the behaviour to generalise).

- **Promote the existing table.** `TANK_DEFENSIVES`, `NextTankDefensive` and `IsHeldTankDefensive`
  live in `src/Ai/Raid/OS/Util/OSEncounter.{h,cpp}` and already implement exactly the rule asked
  for: weakest-first ordering, and "anything still running means the tank is already covered, and
  stacking the next one on top spends two buttons on one window". Move them to a new
  `src/Ai/Raid/RaidTankDefensive.{h,cpp}` alongside `RaidAntiFear` and `RaidRedirectThreat`, and
  point OS at the new home. No behaviour change to OS.
- **Fire on the cast, not on a health threshold.** `MimironPlasmaBlastTrigger` deliberately fires in
  the *gaps* (`cannon->FindCurrentSpellBySpellId(SPELL_MIMIRON_PLASMA_BLAST)` returns → false,
  `UldTriggers_Mimiron.cpp:349`). Add `MimironPlasmaBlastDefensiveTrigger` that flips that
  condition: active while the cannon is casting 62997 and the MK II's victim is this bot (tank) or
  the bot can reach that victim with an external (healer).
- **One button per window, tank *or* healer, never both.** Claim the window in the shared Mimiron
  encounter state, keyed by the cast's start, so the first claimant wins and everyone else stands
  down. The tank's own cooldown is preferred; a healer external only fires if the tank has none
  ready.
- Healer externals, in order: `pain suppression`, `guardian spirit`, `hand of sacrifice`. Not
  `hand of protection` — it sheds threat and would hand the boss straight back.
- Register at `ACTION_RAID + 6` (above the fire dodge, below Rapid Burst) and add a
  `RaidObs::NoteDerived(bot, "mimiron.plasma", ...)` naming which button and who claimed it, the way
  OS notes `sartharion.defensive`.
- **Retire the tank swap.** Delete `MimironPlasmaBlastTrigger`/`Action` and their registration; with
  defensives answering Plasma Blast the swap has no job, and it needs a second tank bot the raid
  does not have.

## Change 4 — make Shock Blast survivable

Finding 1 is a priority bug, and the fix is independent of any geometry change.

- **Reorder.** `mimiron shock blast trigger` moves from `ACTION_RAID + 3` to `ACTION_RAID + 5.5`,
  above `dodge flames` (+4) and rocket strike (+5), below frost bomb (+6). Shock Blast one-shots;
  flames tick for a few thousand.
- **Stand the fire dodge down during a Shock Blast cast**, the same shape as the barrage stand-down
  already in `MimironDodgeFlamesTrigger::IsActive`. Reordering alone is not enough: once a fire leg
  holds the lock, an equal-priority Shock Blast leg cannot take it.
- **Do the same for Rapid Burst and Rocket Strike.** Both are also `MOVEMENT_FORCED` and both are
  being locked out by fire today (327/194 and 58/74 refusals). One shared "a killer is inbound" test
  in the flames trigger, covering Shock Blast, Rapid Burst, Rocket Strike and the barrage, is
  cheaper than four stand-downs and easier to reason about.
- **Move the stack out to 22 yd** from the tank anchor, replacing the 20.2 yd point. Four extra
  yards of margin against a 15 yd blast, still inside the 24.5 yd reach, and
  `MimironPhase1StackSlot` already slides toward the boss when reach is exceeded so the far side
  self-corrects.

The MT's own anchor does not move: it delivered 0 % centre fire and the problem there is threat
(Change 1), not the coordinate.

## Change 5 — rotate the stack off the fire

Four fixed candidate anchors at 22 yd from the tank anchor, 45° apart on the arc that has floor.
All navprobe-verified on map 603 `--nav 0x09`, reading settled Z:

| id | bearing | x | y | z | to poly | 6 yd ring | 10 yd ring | to room centre |
|---|---|---|---|---|---|---|---|---|
| A | 75° | 2697.270 | 2589.782 | 364.314 | 0.22 | 12/12 | 11/12 | 51.6 |
| B | 30° | 2710.629 | 2579.531 | 364.314 | 0.22 | 12/12 | 12/12 | 35.5 |
| C | 345° | 2712.827 | 2562.837 | 364.314 | 0.22 | 12/12 | 12/12 | 32.5 |
| D | 300° | 2702.576 | 2549.479 | 364.314 | 0.22 | 12/12 | 12/12 | 46.6 |

A is essentially today's spot and stays the default. Adjacent anchors are 16.8 yd apart, which
clears a 5 yd node cluster and a 7 yd chain step. All four are ≥ 32.5 yd from the room centre, well
outside the 24 yd ground phases 2-4 are fought on, so rotating never re-contaminates the centre.
The westward bearings (105-255°) are off-mesh or in the raised doorway alcove and are excluded.

- Score each anchor with the fire data already gathered by `GetMimironFirefighterHazards` /
  `IsMimironSpotFireSafe` (`UldEncounter_Mimiron.cpp:190-220`) — count live flame nodes within
  `ULDUAR_MIMIRON_STACK_FIRE_RADIUS` (10) of the anchor.
- Switch only when the live anchor's count exceeds `ULDUAR_MIMIRON_STACK_FIRE_LIMIT` (2) **and**
  some other anchor is strictly cleaner by at least 2 nodes, then hold the new one for
  `ULDUAR_MIMIRON_STACK_HOLD_MS` (15000) before it may move again. Hysteresis both ways; without it
  the clump thrashes as chains grow.
- Decide **once per instance**, not per bot, in the shared Mimiron encounter state — the same reason
  the OS corridor is raid-wide. Twelve bots each picking their own cleanest anchor is twelve
  clumps.
- Note every switch as `mimiron.stack` with the anchor id and both node counts.

---

## Files

- `src/Ai/Raid/RaidTankDefensive.{h,cpp}` — **new**; the table and two helpers moved out of
  `src/Ai/Raid/OS/Util/OSEncounter.{h,cpp}`, which is edited only to point at the new home
- `src/Ai/Raid/Uld/Util/UldEncounter_Mimiron.{h,cpp}` — the four stack anchors, the new constants,
  the anchor selector, the `p1tank` threat gate, the per-instance state additions
- `src/Ai/Raid/Uld/Action/UldActions_Mimiron.{h,cpp}` — `MimironRedirectThreatAction`,
  `MimironPlasmaBlastDefensiveAction`; delete `MimironPlasmaBlastAction`
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.{h,cpp}` — the two new triggers, the killer
  stand-down in `MimironDodgeFlamesTrigger`; delete `MimironPlasmaBlastTrigger`
- `src/Ai/Raid/Uld/Multiplier/UldMultipliers_Mimiron.{h,cpp}` — delete
  `MimironThreatRedirectGuardMultiplier`, add the main-tank `reach melee` guard and the generic
  `tricks of the trade` suppression
- `src/Ai/Raid/Uld/UldStrategy.cpp`, `UldActionContext.h`, `UldTriggerContext.h` — registration, and
  the Shock Blast relevance bump to `ACTION_RAID + 5.5`

Other sessions edit Thorim and Iron Assembly in this tree — stage only Mimiron, OS and shared-raid
files.

## Verification

Per-TU `-fsyntax-only` on every changed TU against the build image's `compile_commands.json`:

```
docker run --rm -v <repo>/modules/mod-playerbots/src:/azerothcore/modules/mod-playerbots/src:ro \
  -v <scratch>/syntax.sh:/tmp/syntax.sh:ro --entrypoint sh acore/ac-wotlk-build:master \
  /tmp/syntax.sh <files>
```

Then the worldserver Docker rebuild — the only compile path — then one Firefighter pull. Read in
this order, all pass/fail:

1. **The tank holds, then drags.** `mimiron.slot` shows no `p1tank` for the first few seconds, then
   it latches once and never clears. The MK II's time on the main tank rises from 20-65 % to above
   90 %, and it ends phase 1 within ~5 yd of (2691.58, 2568.53) instead of a median 5-10 yd off with
   a p90 of 17-23.
2. **The redirects fire.** Misdirection and Tricks casts appear on the **main tank** in phase 1, and
   `mimiron threat redirect guard` is gone from the veto stream. No Tricks on a melee DPS before
   20 s.
3. **Shock Blast stops killing.** Zero mass-casualty casts. `shock locked` falls from 58/63/127 to
   near zero, and no `mimiron dodge flames action` move lands between a `shock` escape and the
   blast.
4. **The other killers stop being locked out.** `rapidburst locked` falls from 327/194, `rocket
   locked` from 58/74, `frostbomb locked` from 85.
5. **Plasma Blast is covered.** `mimiron.plasma` shows exactly one button per window, tank or healer
   and never both, fired while the cannon is casting rather than at a health threshold. Tank HP
   trough per window rises from 11-56 % to above 50 %, and no tank death in a Plasma window.
6. **The stack leaves the fire.** `mimiron.stack` records at least one switch per pull once nodes
   build up, all bots on the same anchor, and no switch inside 15 s of the previous one. Phase-1
   Flames damage falls from 25.8 % (a3) / 12.6 % (a2) of damage taken, and the median flame victim is
   no longer 3.4 yd from a stack anchor.
7. **Ranged cast again.** Phase-1 ranged `%cast` recovers from 34.8/58.3/48.8 % toward the 63-65 %
   the centre fight managed. `reach spell` does not appear in the move stream — that would mean the
   22 yd anchors outran casting range and the `reach spell` / formation deadlock is live.
8. **No regression on what already worked.** Phase-1 flame damage within 20 yd of the room centre
   stays at 0 %, zero flame nodes within 24 yd of the centre at the handover, Napalm victims per
   cast stays under ~2, and phase-1 raid dps stays near 98,000.
9. **Normal mode.** Only Changes 1-4 apply; the stack rotation and the west anchor are
   Firefighter-gated. Confirm the MK II still gets tanked on the room centre and nothing regresses.

The cleanse (`Flame Suppressant` 64570 at 75.8-76.7 s, i.e. 62-63 s into phase 1) is still missed —
the MK II dies at 119.3 s. Killing it inside the window needs it dead by ~100 s, about 20 % more
phase-1 damage. Nothing here targets that; melee range uptime remains the only lever big enough, and
should be re-measured against a pull where the boss actually stays on the anchor.
