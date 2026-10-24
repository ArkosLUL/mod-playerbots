# Naxxramas (map 533)

Cross-raid conventions are in [README.md](README.md). Strategy key `naxx`, auto-applied by map id.
Every boss but Patchwerk (no nodes) is declared in `Definition/NaxxDefinition_<Boss>.cpp`, behind
its encounter gate; `NaxxThreatRedirectMultiplier` and `NaxxBurstWindowMultiplier` span bosses, so
they stay raid-wide.

Noth, Heigan and Gothik were once fully commented out and were rebuilt rather than un-commented: the
disabled code of each held a fatal detection bug. Patchwerk has no nodes.

## Threat redirect

Per-boss verdicts, against the rubric in [README.md](README.md#threat-redirect--the-rubric):

| Boss | Verdict |
|---|---|
| **Thaddius** | Dedicated action. Thaddius activates after both pets die with a **fresh threat table** — nobody owns him. The pet phase separately wants two redirects (MT on Stalagg, OT on Feugen), which a main-tank-only node cannot express. |
| **Four Horsemen** | Dedicated action, **pull-only** — the attractor rotation afterwards makes every later redirect land on the wrong player. Index → (horseman, tank) mapping; only the two melee horsemen get one, since Zeliek and Blaumeux belong to the rotation. |
| **Gluth** | Dedicated action. A redirect at whoever *currently* holds Gluth beats vetoing it over the MT/OT Mortal Wound swap, and the Decimate zombie wave wants a redirect at the zombie off-tank. |
| **Patchwerk** | **No** boss-scoped node, deliberately. One tank, no swap, no reset — the generic main-tank node is already right, and *every second* of the fight wants it, not just the pull. A boss-scoped window would take it away for most of the fight. |
| **Kel'Thuzad** | Dedicated phase-aware action (phase 2 dumps into the boss, phase 1 into the current add). |
| **Instructor Razuvious** | **Keep the veto.** Mind-controlled Understudies tank him; any player-targeted redirect rips him off the Understudy and wipes the raid. |
| **Gothik** | Keeps the blanket veto. |
| **Loatheb** | Generic node is enough — single tank, no threat reset, no add wave, and `"neglect threat"` is already forced. |
| **Heigan** | Generic node. The multiplier zeroes both generic redirects while Heigan is `REACT_PASSIVE` on his ledge, so the charges are available for the arena return — the moment the tank has to rebuild through. |
| Anub'rekhan, Faerlina, Maexxna, Grobbulus | Tier 2, unimplemented. The value is redirecting the **add** waves at the assist tank that picks them up, not the boss. `GetRedirectTank()` wants `GetTankHolding()` on the add rather than a tank index. |
| Sapphiron | Lowest value in the instance — threat survives the air phase, so only the pull matters. |

Once Thaddius is up there is **no dump shot**: Polarity Shift can land at any moment and a bot
standing still to finish a Steady Shot dies to it, so the action only applies the buff and lets the
rotation spend the charges.

## Burst windows

`NaxxBurstWindowMultiplier` is one multiplier holding all the helpers, not six per-boss ones — the
shared `IsBurstCooldownAction` early-out then runs once per action instead of six times, and the
result is cached per millisecond so a tick with several queued cooldowns sweeps the bosses once.

| Boss | Suppress while | Release when |
|---|---|---|
| Kel'Thuzad | phase 1, or phase 2 above 45% HP | `IsPhaseTwo() && HealthPct <= 45` — where the Guardians start spawning |
| Sapphiron | flight phase | ground phase — never burn burst while the boss is untargetable |
| Thaddius | pet phase or transition | Thaddius phase — saved for the 5-minute enrage race |
| Loatheb | no Fungal Creep on the bot **and** < 45s into the fight | spore buff, or the 45s fallback |
| Noth | balcony (`UNIT_FLAG_NOT_SELECTABLE`) | ground phase |
| Gothik | balcony / add waves (`UNIT_FLAG_DISABLE_MOVE`) | phase 2, boss landed |

Patchwerk and Four Horsemen deliberately get nothing — they are pure enrage races, so "burst once the
MT has hold" is already correct. Anub'rekhan, Faerlina, Maexxna, Grobbulus, Gluth, Heigan and
Razuvious have no meaningful DPS check.

**The Loatheb fallback is load-bearing**: Fungal Creep (29232) is *not referenced anywhere* in this
core's `boss_loatheb.cpp` — only `SPELL_SUMMON_SPORE = 29234`. Without the 45s timer the cooldowns
would be held for the entire fight. `EvaluateWindow()` must call `loatheb.UpdateBossAI()` **first**
and clear `loathebFightStartMs` when it is false, before any other boss can take an early return —
otherwise the timer survives into a later attempt and is already expired at the pull.

## Anub'rekhan

| Fact | Detail |
|---|---|
| Impale | 28783 / 56090. `SelectTarget(Random, playerOnly, withMainTank=true)` — **uniformly random living player, tank not excluded, no range filter.** Damage lands in an area around the victim, which is why a stack dies together. |
| Impale clock | Exactly 15s after engage, then exactly every 20s. Fully deterministic — and deliberately unused, see below. |
| Locust Swarm | 28785 / 54021. Self-cast ~15 yd aura, ~20s. First cast random 70-120s, then exactly every 90s. `EMOTE_LOCUST` fires on the same tick as the cast — **zero warning**, so the 90s repeat is modelled off the first cast the raid observes and only casts 2+ get the 3s pre-warning. The boss is not slowed, rooted or threat-wiped, so this is a kite. |
| Crypt Guards | 16573. 25-man pre-spawns 2 on reset; 10-man gets 1 at engage +17.5s. Both modes get 1 more at every Locust Swarm +3s. |
| Corpse Scarabs | 10 per dead Crypt Guard (28864), **5 from every dead player** (29105) — a wipe cascades. |

**Nothing generic saves the bots here.** `avoid aoe` cannot see Impale or Locust Swarm
([../engine/pitfalls.md](../engine/pitfalls.md#things-the-bot-cannot-see)), and the generic
de-clumper is inert by default.

**Impale is unavoidable, so nothing may react to it.** Ranged and healers take one slot on a ring
(Hyjal/Loatheb style) anchored on the bearing **from the boss to the room centre**, which keeps the
arc inside the room. **The angle and radius are latched on first use, never re-read**: that bearing
swings hard whenever the boss is near the centre, so re-solving it per tick threw bots between
opposite ends of the arc. Only a swarm clears the latch. The slot then tracks the boss purely
radially, and an 8 yd deadband (`AnchorDriftDistance`) against the last issued destination keeps a
parked bot parked.

Melee are not positioned at all: they stay on their target and eat the splash.
`CombatFormationMoveAction` stays on for melee alone, zeroed for everyone holding a slot. **The
melee branch must still restore the chase** — the swarm stack leaves a point-move behind and no raid
action re-issues one, so without an explicit `ChaseTo` melee stand where the stack left them.

Predicting the Impale clock was tried and reverted: re-issuing moves inside a 3s pre-warning, plus a
`FleePosition` de-clump nudge, left bots walking instead of casting and cost more DPS than the splash
avoided. `AnubrekhanBossHelper` keeps only the Locust Swarm clock.

Locust Swarm inverts the spread — it is what puts people in the aura — so for the window non-tanks
drop their slots and **stack on the room centre**, on a 5 yd de-clump ring. Boss-anchored stacking
(25 yd along the bearing to the centre) was tried and reverted: the pile trailed a point that moved
with the boss, so he swept through it and the raid ate the aura anyway. The centre is a fixed
`KiteRadius` 45 from him for the whole window and the raid is standing on it before the aura goes up.

**The pile only holds if the chases are suppressed.** `AnubrekhanPositionAction` returns false once a
bot is parked, so every lower-priority action runs as normal — and nothing is in range from the centre
by design, so `reach melee`, `reach spell` and `reach party member to heal` (the kiting tank) all fire
every tick and walk the bot back out. The multiplier zeroes `ReachTargetAction` for non-tanks for the
window, along with `MeleeAction` and `FleeAction`. `MeleeAction` is the autoattack, **not** the
chase — zeroing it alone does nothing for positioning.

Accepted costs, not defects: **one Impale lands on the stack per swarm** (20s window against a 20s
period), melee lose the window, and the MT is 45 yd from the healers while kiting — unhealable, but a
kiting tank is not being hit either. `KiteRadius` is the single knob for both.

## Four Horsemen

| Fact | Detail |
|---|---|
| Marks | 28832 Korth'azz / 28833 Blaumeux / 28834 Rivendare / 28835 Zeliek. **Same ids in both difficulties**; damage is script-computed per stack (0/500/1500/4000/12500/20000/…). |
| Cadence | First mark 24s after that boss reaches its corner, then 15s (Lady/Sir) / 12s (Thane/Baron). **Per-boss timers — deltas self-sync per corner only.** |
| Lady/Sir | Stationary; victim is `SelectNearestPlayer(300yd)` **every tick**. ≤45 yd → bolt at the victim, else **raid-wide punish every tick**. Threat is irrelevant — baiting *is* being nearest, so both rear corners must always be covered. |
| Void Zone | 28863 summons NPC **16697**, which casts Consumption via SmartAI every 2.5s. Victim-targeted, so it spawns at the baiter's feet. `AvoidAoeAction` **cannot see it**. |
| Corners | Thane (2542.9, −3015.0), Lady (2469.4, −2947.6), Baron (2583.9, −2971.6), Sir (2517.8, −2896.6), z ≈ 241.3 |

The bait anchors' midpoint is ≤41 yd from both rear bosses, so crossing never triggers the punish.

Wipe vectors the strategy is built against: baiters eating 4-5 stacks before a wall-clock timer
flipped; per-bot `_combat_start_ms` desync leaving a corner briefly empty; the 75s mark aura
outliving the 67.5s away-time so a returning baiter ratchets; no front tank swap; Void Zones never
dodged; and dead baiters never replaced because `ignoreDeadPlayers` defaults to `false`.

Swaps are **mark-stack-driven**, not timer-driven: capture a baseline on arrival, flip at
delta ≥ 3, and re-baseline when `stacks < baseline` (the aura expired while away). Mid-cross
applications fold into the next arrival baseline. Front tanks hold at their corner and taunt on a
30 yd **boss-proximity** gate rather than a spot gate, because Thane and Baron chase.

**Open gap:** `FourhorsemanGenericMultiplier` keys on `find target "sir zeliek"`, which resolves only
with the bot on his threat list, so a bot parked on Thane keeps generic targeting and threat checks.

## Gluth

- **Fear Ward is deliberately skipped.** WotLK Gluth casts no fear — Terrifying Roar is classic-only,
  and this core's `boss_gluth.cpp` has only Mortal Wound, Decimate, Frenzy and Berserk. A Fear Ward
  would never be consumed. Positioning drift is handled by anchor-keeping instead.
- **Frenzy needs a boss-explicit tranquilizing shot.** The generic `"tranquilizing shot"` action
  targets `"current target"`, which during Gluth is usually a zombie for hunters #0/#1 — and raid
  actions at `ACTION_RAID + 1` starve the generic trigger anyway. The Gluth action sits at
  `ACTION_RAID + 4`, mirroring the Faerlina frenzy precedent. Frenzy is 28371 / 54427.
- **Every tank beyond MT and assist-0 kites zombies**, sharing the same 12-waypoint circle (each
  advances from its own nearest waypoint), rather than standing on the boss as DPS.
- The **non-aggro boss tank stages at the door anchor** at radius 5.0 (against the holder's 2.0), so
  a taunt swap does not strand it while keeping both within taunt range.
- All `IsAssistTankOfIndex` calls pass `ignoreDeadPlayers = true` so a dead tank's role promotes.

**Remaining gap:** if *all* extra tanks die, zombies fixate Gluth and get eaten (healing him 5%
each); hunters shooting boss-bound zombies are the only mitigation left. Also: 10-man uses the 25-man
positions.

## Heigan

Both detection traps the disabled code shipped — the dance keyed on a boss Eruption cast that never
happens, and the positional phase check that fired at the pull — are in
[../engine/pitfalls.md](../engine/pitfalls.md#detection-that-never-fires). The positional check also
false-positives after every phase 2: `StartFightPhase(PHASE_SLOW_DANCE)` does not teleport the boss
down, it flips him to `REACT_AGGRESSIVE` and lets him walk off the ledge.

**The eruption schedule carries no RNG** — only Spell Disruption and Decrepit Fever are randomised —
so the safe zone follows arithmetically from a phase-start edge (#2671): `SafeIndex()` reads the
*next* eruption tick off `HeiganBossHelper`'s fight clock (`tick % 6`, mirrored past 3), so bots step
into the new zone the moment the previous one has landed.

| Phase | First eruption | Period | Ticks | Length | Safe index sequence |
|---|---|---|---|---|---|
| 1 (slow, arena) | +15s | 10s | 8 | 90s | `0,1,2,3,2,1,0,1` |
| 2 (fast, ledge) | +7s | 4s | 10 | 45s | `0,1,2,3,2,1,0,1,2,3` |

Phase discriminator: `GetReactState() == REACT_PASSIVE` or Plague Cloud (29350) on the boss, exact
and lag-free. The clock lives per instance (`PhaseStateFor`), re-anchored on every react-state edge
and after 10 s unwatched. Only a pull (slow phase, boss above 99%) or an observed edge gives a
trustworthy anchor; without one a bot does not guess a zone: ranged and non-main-tank melee wait on
the ledge, where nothing erupts, while Heigan is down in the arena, and the main tank stays put
(dragging Heigan up the ramp would evade the encounter). Ranged also hold the ledge through the slow
dance and leave 12 s before it ends (`LedgeDepartureMs`): the ramp is a long walk and Plague Cloud
lands on the ledge a second after Heigan teleports up.

The four waypoints map `idx ↔ 3 − section` against the core's `GetEruptionSection` classifier
(`instance_naxxramas.cpp`). Their clearance to the nearest section divider is 12.0 / 3.9 / 11.6 /
8.8 yd, so the off-centre park `MoveInside` gives each bot stays under index 1's (`SafeZoneSpread`
3 yd, 2 for the main tank). Decrepit Fever is dispelled by its 10- and 25-man ids at
`ACTION_RAID + 5`, main tank first.

## Noth

Phases alternate on **fixed timers with no HP gating**: ground 110s, balcony 70s, forever.

| Ground (110s) | |
|---|---|
| t=14/44/74/104 | Plagued Warriors spawn, `RAID_MODE(2,3)` |
| t=15/40/65/90 | Curse of the Plaguebringer 29213, max targets `RAID_MODE(3,10)` |
| t=26/56/86 | **25-man only**: `DoResetThreatList()` → Cripple 29212 (non-triggered) → Blink 29208 (triggered) |
| t=110 | Teleport to balcony |

Balcony waves fire at t=8/38/68 and their composition is keyed on `timesInBalcony`, which only
increments on **exit** — so all three waves in one phase are identical. 1st balcony: 2 Champions ×3
(4 in 25-man); 2nd: 1 Champion + 1 Guardian ×3 (2+2); 3rd+: 2 Guardians ×3 (4). Berserk 68378 fires
once on leaving the **third** balcony, ≈540s.

Adds: Plagued Warrior 16984 (Cleave), Champion 16983 (Mortal Strike), Guardian 16981 (AoE nuke). All
SmartAI, and `JustSummoned` calls `SetInCombatWithZone()` — **every add aggroes the whole raid
instantly**, spawning from 5 fixed alcoves 25-50 yd out. Hard leash: evades past 80 yd from
(2684.8, −3502.5, 261.3).

Balcony state reads straight off the boss: `UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_DISABLE_MOVE`,
rooted, `REACT_PASSIVE`.

- **The blink window comes off Cripple.** Blink is cast triggered and instant, so it is never
  observable; the helper watches for the hard-cast Cripple that precedes it, plus a 25-man-only timer
  anchored on ground-phase entry.
- **`"cure party member"` is not a registered action name** — `CurePartyMemberAction` is a base
  class whose subclasses register under spell names, so a node naming it resolves to nothing.
- **Decurse duty is split across decursers by index**, on its own node at `ACTION_RAID + 5`, above
  positioning. 25-man puts the curse on 10 targets every 25s against a ~10s window, while
  `PartyMemberToDispel` returns **one** target per tick and the class nodes sit below `ACTION_RAID`.
  Nothing zeroes class output while a curse is up: a branch that did deleted ~40% of
  druid/shaman/mage uptime in 25-man and dispelled nothing.
- **The main tank locks onto an add and never returns.** Noth leaves `"attackers"` for the whole
  balcony phase (`UNIT_FLAG_NOT_SELECTABLE`), so the MT correctly retargets, but coming back an
  explicit main tank sticks to `current target`
  ([../engine/pitfalls.md](../engine/pitfalls.md#role-and-index-traps)); with Warriors every 30s he
  can stay off the boss for the rest of the fight.
- **Ground-phase targeting is split by role**, and the generic assists are muted all fight, not only
  inside the blink window ([README.md](README.md#targeting-the-whole-fight)). **Ranged burn the adds**
  (type order, lowest GUID inside a type), **melee stay on Noth** and join the adds for the blink
  window only. `DoResetThreatList()` empties Noth's table alone, so that window does not mute bots on
  an add.
- **Ranged walk a Champion into the add tank** (assist tank, else main tank) and stop
  `AddTankHandoffDistance` = 10 yd short of it — outside `CleaveSpread`, so the tank does not step
  away from the bot that delivered it. Kiting the nearest Champion made them ungatherable: a chasing
  add followed the bot away in a straight line with the tank stuck behind. Warriors and Guardians are
  left to the tank's own chase in `PositionAssistTank`. Every position move is clamped to the room
  (`NothPositionAction::MoveToClamped`).

**Verified working — do not "fix":** the burst-window hold resolves the boss via `FindTargetValue`,
which walks `GetThreatenedByMeList()` rather than `"attackers"` — that list keeps Noth through the
balcony phase *and* through `DoResetThreatList()`, so the check holds in both phases.

## Gothik

| Fact | Source |
|---|---|
| Living side is `Y < -3360.78` | `IN_LIVE_SIDE` |
| Boss takes **zero damage** until phase 2 | `DamageTaken` |
| `UNIT_FLAG_DISABLE_MOVE` set on pull, removed at phase 2 | the phase discriminator |
| Phase 2 starts after the 24-wave table (~4:45) and teleports him to the **living** side first | |
| In phase 2 he teleports side-to-side every 20s until below 30% | |
| Gate opens at 30% HP **or** at the 2-minute check if the raid is *not* split | |
| Entry 16060 | |

**Consequence of the living-side-only tactic**: `CheckGroupSplitted()` returns false when everyone
stands on one side, so **the gate opens ~2 minutes in** and every dead-side add walks over at once,
long before phase 2. The strategy therefore has to handle mixed living/dead adds — which is why the
kill-priority table covers all seven entries:

| Prio | Entry | Add | Why |
|---|---|---|---|
| 70 | 16150 | Dead Rider | Drain Life self-heal + Unholy Frenzy snowball |
| 60 | 16126 | Living Rider | Shadow Bolt Volley, biggest P1 raid damage |
| 50 | 16148 | Dead Knight | Whirlwind |
| 40 | 16149 | Dead Horse | Stomp |
| 30 | 16125 | Living Knight | Shadow Mark |
| 20 | 16127 | Dead Trainee | Arcane Explosion |
| 10 | 16124 | Living Trainee | Death Plague |

Add selection walks `"possible targets no los"` — the **no-los** variant, because the gate wall would
otherwise hide adds the bot is about to face; side bookkeeping is done on coordinates anyway, so an
add that crosses becomes a valid target automatically with no gate-state lookup.

`"find target"` resolves him from the pull despite `REACT_PASSIVE` on the balcony, because
`JustEngagedWith` calls `SetInCombatWithZone()`. Fall back to `GetFirstAliveUnitByEntry` for a bot
that battle-rezzed and never made the threat list.

The old `GothikGenericMultiplier` referenced `boss_botAI->GetEvents()` and `Action::GetTarget()`,
neither of which exists — **it never compiled. Dead code, not a reference.**

Deliberately out of scope: skull-marking the priority add (adds die every few seconds, so the mark
would thrash) and any side-splitting.

## Kel'Thuzad

| Fact | Detail |
|---|---|
| Difficulty variants | **Only Frost Bolt has them** — 55802 / 55807. FrostBlast 27808, DetonateMana 27819, Chains 28410, ShadowFissure 27810, VoidBlast 27812 have none. |
| Shadow Fissure | NPC entry **16129**; Void Blast radius is 10 yd **+ target combat reach**, so a 10 yd escape is not enough |
| P1 add activation | Activated adds are `AttackStart()`ed immediately; parked adds are stationary decoration with proximity aggro. `IsInCombat() \|\| GetVictim() \|\| isMoving()` discriminates cleanly |
| MT hold | `tank_pos` is 7.21 yd from centre, world angle ≈166° |

Design constraints, each a former wipe cause: P1 target selection checks add activity, or tanks and
melee body-pull parked perimeter adds; the fissure escape clears more than 10 yd, and `ClampToRoom`
must not project an outward escape back onto the ≤24 yd circle inside the blast; ranged rings 3 yd
apart (18/21/24) gave ~6.6 yd cross-ring neighbours at 11+ ranged, inside Frost Blast's 10 yd chain;
and melee DPS with no P2 positioning stacked behind the boss within 10 yd of each other and the MT.

The spread is two rings with angles relative to the tank bearing: outer r=24 with up to 12 slots
(chord 12.42 yd), inner r=14 only past 12 ranged, with candidate angles staggered off the outer grid
and ±90/±150 offered first to protect the MT. Melee get a rear arc excluding a ±100°-minimum wedge
around the MT. A Detonate Mana carrier parks in the 20-24 yd band, **exactly the ranged ring**, so it
takes the widest gap from the rest of the group among 24 headings on the 24 yd ring, skipping
fissures: clamped there blind, a detonating melee exploded on ranged slots. The 25-man interrupt boost
checks Frost Bolt by both ids, 28478 and 55802.

The fissure dodge is its own emergency node at `ACTION_EMERGENCY + 6`, which fixes the ordering by
construction: as step 4 of a chain, bots handling Detonate or Frost Blast returned earlier and walked
into fissures. Every computed movement destination is additionally vetoed against nearby fissures.

**Chains (28410) is answered with Cyclone (33786)** by a Balance or Restoration druid —
`kel'thuzad cyclone chained` at `ACTION_EMERGENCY + 5`, below the fissure dodge. Only **one** druid
commits: every candidate sorts the alive Balance/Resto druids within 40 yd of the charmed player by
`(distance, GUID)` and acts only if it is first, which is deterministic across bots and stops the
whole druid roster burning diminishing returns at once. Three constraints worth keeping:

- **Do not route this through `"rti cc target"` / `"cc target"` / `"possible targets"`.**
  `AttackersValue::IsPossibleTarget` drops unflagged group members, so all of those resolve to
  `nullptr` on a charmed raider. Walk the group directly — the same thing
  `KaelthasSunstriderBreakMindControlAction` does.
- **`CanCastSpell` is not a range check** — it passes `SPELL_FAILED_OUT_OF_RANGE`
  ([../engine/action-selection.md](../engine/action-selection.md#casting)), so Cyclone's 20 yd is
  measured by hand (close to within 18 yd first).
- **It only covers about half the MC.** Cyclone has full PvP diminishing returns against a player
  (6 → 3 → 1.5 s → immune) while Chains runs 20 s. Resto druids also only join in if their talent
  template actually took Cyclone; otherwise `CanCastSpell` is false and they no-op.

## Thaddius (phase 1)

Two encounter rules must hold: **each add stays pinned at its tesla coil** (dragging it off overloads
the coil and blasts the raid), and **both adds die within ~5s** or both revive at full HP.

Decisions: split **evenly by per-role index parity** (≥1 tank, ~half DPS, ~half healers per add), not
the first N healers (3 healers in 10-man went 1 vs 2); pet engagement does not depend on RTI marks, so
an unmarked raid still plays phase 1; tanks **hard-hold at the coil, never chase**, and walk there
before aggro rather than dragging the add off it; target the ~5s window with a symmetric balance
margin plus a hard floor, so neither add can be pushed to 0 until the other is within the floor band.
**Tanks and healing are never suppressed.**

**Stalagg and Feugen never actually die during the pet phase.** Upstream clamps fatal damage to
`health - 1` and drops the add into feign death (`UNIT_FLAG_NOT_SELECTABLE`, `REACT_PASSIVE`,
`UNIT_STAND_STATE_DEAD`, rooted); the real death is `KillSelf()` from Thaddius' overload 12 s later,
about 1.75 s before he engages. Asking `IsAlive()` parked bots on the pet platforms for the whole
revive window, leaving ~1.7 s to reach their polarity spots, or kept them swinging at an unattackable
1-HP add. Everything goes through the shared `IsDownOrFeigning()` in `EncounterHelpers`, and
`PetSyncSuppress` bails as soon as either pet is down so the survivor is not throttled against a
corpse.

### Pre-pull staging

Bots walk to their assigned side **before** the pull and wait there, so the 5-minute enrage race does
not start with the raid scrambling out of one blob. Raid strategies run out of combat (map 533 is on
both engines, and nothing filters raid triggers by bot state), so this needs no extra wiring. The
out-of-combat lookup has to be `GetFirstAliveUnitByEntry`; `"find target"` walks the bot's own
(empty) threat list. Ulduar's Thorim gauntlet positioning is the precedent for the shape.

**There is no on-platform staging solution.** The adds spawn at `(3450.45, -2931.42)` and
`(3508.14, -2988.65)`, the platforms are only ~20 yd across (confirmed by the Frozen Rune GO spawns),
and a level-83 elite aggroes a level-80 player at ~23 yd — every walkable point on a platform is
12–20 yd from its add. Staging is therefore on the **catwalk**, ~5 yd below platform level, at
measured `.gps` coordinates that each carry their own Z:

| Side | Spot | To own add |
|---|---|---|
| Stalagg | `3422.97, -2959.07, 307.40` | 39.0 yd |
| Feugen | `3480.12, -3017.49, 306.88` | 40.2 yd |

`IsPrepullStagingSafe` rejects any spot within `PREPULL_AGGRO_SAFE_DIST` (28 yd) of either add or
whose ground height misses its own Z by more than 2 yd, and an unsafe spot means that side simply
does not stage. `ROOM_RADIUS` is 100, not 80: the Thaddius Door is 80.55 yd from the room anchor, so
a leader standing in the doorway failed the check by half a yard; the Construct Quarter trash is
141 yd out, so 100 does not reach back into it.

Four things have to hold, all found in live testing:

- **`FollowAction` is zeroed while staging.** The action returns false once parked (so bots can
  still buff and drink), which hands the tick to `follow` at relevance 1.0 and starts an oscillation
  on the ramp. A dedicated `ThaddiusPrepullMultiplier` re-decides this every tick from the same
  `IsPrepullStagingActive()` gate the trigger uses, so unlike a strategy removal it cannot leak.
- **`UpdateBossAI()` needs an entry lookup, accepted only when the resolved add is already in
  combat.** A bot parked 40 yd out has no threat and, unmarked, no RTI icon, so it never joined the
  fight. The in-combat condition is what keeps `IsPhasePet()` false for idle adds — otherwise
  `ThaddiusGenericMultiplier` kills follow-master for anyone walking past the room.
- **Non-tank side assignments are latched.** The parity walk skips dead members, so one death
  re-parities everyone behind it and half the raid changes sides mid-fight. `LatchedPrimarySide`
  freezes the first answer per player in a file-static GUID-keyed map (the
  `HeiganBossHelper::PhaseStateFor` convention), stale after 30 s. **Tanks deliberately keep the live
  answer** — Magnetic Pull teleports them across and they must follow their add.
- **`GetAssignedPetForBot` does not fall back** to the sibling add or nearest pet for non-tanks; both
  fallbacks sent half the raid across the room the moment their own add feigned. It returns null and
  the bot holds. If the death sync fails badly that side idles, which is what `PetSyncSuppress` exists
  to prevent.

Pet resolution caches each add's `ObjectGuid` (revalidated through `botAI->GetUnit`) because
`sightDistance` is 100 yd while the coils are ~120 yd apart — a bot on its own staging spot loses the
grid search on the far add. Residual risk: a heavily lowered `Playerbots.SightDistance` leaves a
non-tank idle rather than falling back.

## Sapphiron

The air-phase healer starvation is the canonical cast-while-moving case
([../engine/pitfalls.md](../engine/pitfalls.md#a-moving-bot-cannot-cast)). `WaitForExplosion()` is
true for essentially the whole phase because Icebolts persist, so the shelter action reaches then
holds: it owns the tick while moving, yields to heals once a healer is sheltered, and drops its latch
when the bot itself becomes an ice block.

`SapphironGroundPositionAction` tests `isInFront` / `isInBack` with an `M_PI / 2` arc: the default
`M_PI` makes their union the whole circle
([../engine/pitfalls.md](../engine/pitfalls.md#arc-and-geometry-defaults)), which once commanded
every melee to one point 5 yd from the boss centre for the whole ground phase. Bots in his front or
rear wedge, or carrying Life Drain, move to his side (orientation + π/2): ranged, healers and Life
Drain carriers at 30 yd, melee at 5 yd.
