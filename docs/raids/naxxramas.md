# Naxxramas (map 533)

Cross-raid conventions are in [README.md](README.md). Strategy key `naxx`, auto-applied by map id.

Four bosses were fully commented out at some point (Noth, Heigan, Gothik, Patchwerk) and Noth,
Heigan and Gothik have since been rebuilt rather than un-commented — in each case because the
disabled code contained a fatal detection bug that a naive un-comment would have shipped.

## Threat redirect

Per-boss verdicts, against the rubric in [README.md](README.md):

| Boss | Verdict |
|---|---|
| **Thaddius** | Dedicated action. Thaddius activates after both pets die with a **fresh threat table** — nobody owns him. The pet phase separately wants two redirects (MT on Stalagg, OT on Feugen), which a main-tank-only node cannot express. |
| **Four Horsemen** | Dedicated action, **pull-only** — the attractor rotation afterwards makes every later redirect land on the wrong player. Index → (horseman, tank) mapping; only the two melee horsemen get one, since Zeliek and Blaumeux belong to the rotation. |
| **Gluth** | Dedicated action. Was vetoed because of the MT/OT Mortal Wound swap, but a redirect at whoever *currently* holds Gluth beats nothing, and the Decimate zombie wave wants a redirect at the zombie off-tank. |
| **Patchwerk** | **No** boss-scoped node, deliberately. One tank, no swap, no reset — the generic main-tank node is already right, and *every second* of the fight wants it, not just the pull. A boss-scoped window would have taken it away for most of the fight. |
| **Kel'Thuzad** | Already had a dedicated phase-aware action (phase 2 dumps into the boss, phase 1 into the current add). |
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

`NaxxBurstWindowMultiplier` is a single multiplier holding all the helpers, not six per-boss ones —
the shared `IsBurstCooldownAction` early-out then runs once per action instead of six times, and the
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

**Nothing generic saves the bots here.** `avoid aoe` cannot see Impale or Locust Swarm (see
[../engine/pitfalls.md](../engine/pitfalls.md)), and the generic de-clumper is inert by default
(`DisperseDistanceValue` is `-1.0f`).

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

Predicting the clock was tried and reverted: re-issuing moves inside a 3s pre-warning, plus a
`FleePosition` de-clump nudge, left bots walking instead of casting and cost more DPS than the splash
avoided. `AnubrekhanBossHelper` no longer keeps the clock.

Locust Swarm inverts the spread — it is what puts people in the aura — so for the window non-tanks
drop their slots and **stack on the room centre**, on a 5 yd de-clump ring. Boss-anchored stacking
(25 yd along the bearing to the centre) was tried and reverted: the pile trailed a point that moved
with the boss, so he swept through it and the raid ate the aura anyway. The centre is a fixed
`KiteRadius` 45 from him for the whole window and the raid is standing on it before the aura goes up.

**The pile only holds if the chases are suppressed.** `AnubrekhanPositionAction` returns false once a
bot is parked, so every lower-priority action runs as normal — and nothing is in range from the centre
by design, so `reach melee`, `reach spell` and `reach party member to heal` (the kiting tank) all fire
every tick and walk the bot back out. The multiplier zeroes `ReachTargetAction` for non-tanks for the
window, along with `MeleeAction` and `FleeAction`. Note `MeleeAction` is the autoattack, **not** the
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

The existing bait anchors are good: anchors' midpoint is ≤41 yd from both rear bosses, so crossing
never triggers the punish.

Wipe vectors the rework targets: baiters eating 4-5 stacks before a wall-clock timer flipped;
per-bot `_combat_start_ms` desync leaving a corner briefly empty; the 75s mark aura outliving the
67.5s away-time so a returning baiter ratchets; no front tank swap; Void Zones never dodged; and
dead baiters never replaced because `ignoreDeadPlayers` defaults to `false`.

Swaps are **mark-stack-driven**, not timer-driven: capture a baseline on arrival, flip at
delta ≥ 3, and re-baseline when `stacks < baseline` (the aura expired while away). Mid-cross
applications fold into the next arrival baseline. Front tanks hold at their corner and taunt on a
30 yd **boss-proximity** gate rather than a spot gate, because Thane and Baron chase.

## Gluth

- **Fear Ward is deliberately skipped.** WotLK Gluth casts no fear — Terrifying Roar is classic-only,
  and this core's `boss_gluth.cpp` has only Mortal Wound, Decimate, Frenzy and Berserk. A Fear Ward
  would never be consumed. Positioning drift is handled by anchor-keeping instead.
- **Frenzy needs a boss-explicit tranquilizing shot.** The generic `"tranquilizing shot"` action
  targets `"current target"`, which during Gluth is usually a zombie for hunters #0/#1 — and raid
  actions at `ACTION_RAID + 1` starve the generic trigger anyway. The Gluth action sits at
  `ACTION_RAID + 4`, mirroring the Faerlina frenzy precedent. Frenzy is 28371 / 54427.
- **Every tank beyond MT and assist-0 kites zombies**, sharing the same 12-waypoint circle (each
  advances from its own nearest waypoint). Previously only assist index 1 went to zombies and any
  further tanks stood on the boss as DPS.
- The **non-aggro boss tank stages at the door anchor** at radius 5.0 (against the holder's 2.0), so
  a taunt swap does not strand it while keeping both within taunt range.
- All `IsAssistTankOfIndex` calls pass `ignoreDeadPlayers = true` so a dead tank's role promotes.

**Remaining gap:** if *all* extra tanks die, zombies fixate Gluth and get eaten (healing him 5%
each); hunters shooting boss-bound zombies are the only mitigation left. Also: 10-man uses the 25-man
positions (pre-existing).

## Heigan

The disabled code had two independently fatal bugs, which is why it was rebuilt rather than
un-commented:

- **The phase-1 dance never advanced.** It watched the boss cast Eruption (29371), but the boss never
  casts it — the floor GameObjects do. Every bot parked on one waypoint for the whole 90s phase and
  ate ~6 eruptions.
- **The pull was mis-detected as phase 2.** Heigan's DB spawn is *on the platform*, 0.8 yd from the
  positional check point, so bots seeded the fast-dance clock at the pull, danced on the 4s cadence
  during 15s/10s phase-1 ticks, and the multiplier zeroed all combat so nobody attacked. It
  false-positives again after every phase 2, because `StartFightPhase(PHASE_SLOW_DANCE)` does not
  teleport the boss down — it just flips to `REACT_AGGRESSIVE` and lets him walk off the ledge.

**The eruption schedule carries no RNG** — only Spell Disruption and Decrepit Fever are randomised —
so bots need only a reliable phase-start edge and the index follows arithmetically, re-anchoring
every 45-90s:

| Phase | First eruption | Period | Ticks | Length | Safe index sequence |
|---|---|---|---|---|---|
| 1 (slow, arena) | +15s | 10s | 8 | 90s | `0,1,2,3,2,1,0,1` |
| 2 (fast, ledge) | +7s | 4s | 10 | 45s | `0,1,2,3,2,1,0,1,2,3` |

Phase discriminator: `boss->ToCreature()->GetReactState() == REACT_PASSIVE`, with Plague Cloud
(29350) as secondary confirmation. Exact, no lag — the established idiom (Gothik uses it too).
Anchors in order of preference: combat start → any observed react-state edge → **report "unknown"
and bail out rather than guess a zone** (a bot that rezzed mid-fight self-heals at the next edge,
≤90s).

Other verified defects: **25-man Decrepit Fever is 55011, not 29998**, so `HasAura(29998)` dispelled
nothing in 25-man; and `MoveInside(..., distance = 0)` with a dead `IsMainTank(bot) ? 0 : 0` ternary
pinned every non-tank melee on one waypoint centre at zero DPS.

**Verified good — keep as-is:** the four waypoints and their `idx ↔ 3 − section` mapping against the
core's `GetEruptionSection` classifier (clearances 13.63 / 13.60 / 13.65 / **11.36** yd — index 3 is
the tight one, so keep any spread radius under ~5 yd); and the `NextSafe()`/`ResetSafe()` index walk,
whose `uint32 curr_dir = -curr_dir` looks wrong but is correct by unsigned wraparound.

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

Gaps the rebuild had to solve:

- **The blink window was undetectable.** The old helper stamped `_last_blink_ms` only on catching
  `UNIT_STATE_CASTING` with Blink — but the core casts it `triggered, instant`, which never sets that
  state. Detect off the **Cripple** cast that precedes it (non-triggered, so it does occupy the cast
  slot) plus a 25-man-only timer anchored on ground-phase entry.
- **`"cure party member"` is not a registered action name** — `CurePartyMemberAction` is a base class
  whose subclasses register under spell names. The node resolved to nothing and curse handling was
  silently dead.
- **The curse branch zeroed three classes' entire output** while *any* curse was on *any* member,
  boosting a node that never resolved. In 25-man the curse is up ~10s out of every 25s, deleting
  ~40% of druid/shaman/mage uptime and dispelling nothing.
- **Decurse throughput cannot keep up in 25-man**: 10 targets every 25s against a ~10s window, while
  `PartyMemberToDispel` returns **one** target per tick and the class nodes sit below `ACTION_RAID`.
  Duty must be split across decursers by index.
- **The main tank locks onto an add and never returns.** Noth leaves `"attackers"` for the whole
  balcony phase (`UNIT_FLAG_NOT_SELECTABLE`), so the MT correctly retargets — but coming back,
  `FindTankTargetSmartStrategy::IsBetter` makes an explicit main tank in a >1-tank group stick to
  `current target`. With Warriors spawning every 30s the MT can stay off the boss for the rest of the
  fight.
- **Ground-phase targeting oscillated between Noth and the adds** — the generic assists were only
  muted inside the blink window. See [README.md](README.md) for the mechanism.
- **Nobody killed the Warriors.** Ground-phase DPS were sent `{guardian, boss}`, so 2-3 adds every
  30s piled on the add tank for the whole phase. Split by role instead: **ranged burn the adds**
  (type order, lowest GUID inside a type), **melee stay on Noth**, and melee join the adds for the
  blink window only. `DoResetThreatList()` empties Noth's table alone, so that window no longer mutes
  bots who are on an add.
- **Ranged kiting the Champions made them ungatherable.** The old position action ran a 25 yd flee
  at the *nearest* Champion, whoever it was fighting, so a bot backed off adds the off-tank already
  held, and an add chasing a bot followed it away in a straight line with the tank stuck behind.
  Ranged now walk the add **into** the add tank (assist tank, else main tank) and stop
  `AddTankHandoffDistance` = 10 yd short of it — outside `CleaveSpread`, so the tank does not step
  away from the bot that just delivered it. Warriors and Guardians are left to the tank's own chase
  in `PositionAssistTank`.
- Position moves had **no room clamp** (rectangle 2618..2754, −3557.43..−3450) — Kel'Thuzad's
  `ClampToRoom` / `ComputeEscapeFromPoint` are the primitives to copy.

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

Wipe causes fixed: P1 target selection had **no activity check**, so tanks and melee body-pulled
parked perimeter adds; the fissure escape moved exactly 10 yd and `ClampToRoom` projected outward
escapes back onto the ≤24 yd circle, often back inside the blast; and the ranged spread used rings
18/21/24 only 3 yd apart, giving ~6.6 yd cross-ring neighbours at 11+ ranged — inside Frost Blast's
10 yd chain. Melee DPS had **no P2 positioning at all** and stacked behind the boss within 10 yd of
each other and the MT.

The rewritten spread is two rings with angles relative to the tank bearing: outer r=24 with up to 12
slots (chord 12.42 yd), inner r=14 only past 12 ranged, with candidate angles staggered off the
outer grid and ±90/±150 offered first to protect the MT. Melee get a rear arc excluding a
±100°-minimum wedge around the MT.

Also found and fixed: Detonate Mana clamped into the 20-24 yd band — **exactly the ranged ring** — so
a detonating melee exploded on ranged slots; and the 25-man interrupt boost was dead because it
checked Frost Bolt 28478 only.

The fissure dodge is its own emergency node at `ACTION_EMERGENCY + 6`, which fixes the ordering by
construction: it previously sat as step 4 of a chain, so bots handling Detonate or Frost Blast
returned earlier and walked into fissures. Every computed movement destination is additionally
vetoed against nearby fissures.

## Thaddius (phase 1)

Two encounter rules must hold: **each add stays pinned at its tesla coil** (dragging it off overloads
the coil and blasts the raid), and **both adds die within ~5s** or both revive at full HP.

Failures fixed: the split sent the *first* N healers to the primary side and DPS by a hardcoded
count, so 3 healers in 10-man went 1 vs 2; pet engagement was **hard-gated on RTI marks**, so an
unmarked raid ignored phase 1 entirely and fell back to geographic nearest-pet; tanks only moved to
the coil spot *after* getting aggro, dragging the add off the coil before that; and the sync rule was
a threshold heuristic with no hard floor.

Decisions: split **evenly by per-role index parity** (≥1 tank, ~half DPS, ~half healers per add);
tanks **hard-hold at the coil, never chase**; target the ~5s window with a symmetric balance margin
plus a hard floor, so neither add can be pushed to 0 until the other is within the floor band.
**Tanks and healing are never suppressed.**

## Sapphiron

The air-phase healer starvation is the canonical cast-while-moving case — see
[../engine/pitfalls.md](../engine/pitfalls.md). `WaitForExplosion()` is true for essentially the whole
phase because Icebolts persist, so the position action owns every tick.

Separately, `SapphironGroundPositionAction` uses `boss->isInFront(bot) || boss->isInBack(bot)` with
the default `arc = M_PI`, which is **always true**, so melee are permanently commanded to one point
5 yd from the boss centre — reach-unaware — and the priority-61 action starves the rotation for the
whole ground phase.
