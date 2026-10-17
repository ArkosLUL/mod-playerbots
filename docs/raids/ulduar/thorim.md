# Thorim

Two halves that share nothing: a corridor gauntlet past the Runic Colossus and the Ancient Rune Giant,
then a stationary fight on the arena floor once Thorim drops off his balcony. `GetPositionZ() < 429.6`
separates the two everywhere in the strategy.

**The whole fight is on a ~170 s clock.** `EVENT_THORIM_START_PHASE1` fires 20 s after the pull and
sends `ACTION_SIF_START_DOMINION`; `EVENT_SIF_FINISH_DOMINION` is scheduled 150 s later, and on it
Sif despawns. Reach Thorim before it and she joins the fight instead — that is the hard mode. Every
gauntlet decision below is sized against that budget, not against the 150 s timer alone.

## Phase 1 split

The raid fights phase 1 in two halves, and the arena half must never empty out.
`ThorimAI::GetArenaPlayer()` scans for one **living** player inside

```
x 2085..2185   y -305..-214   z < 425
```

**every 5 seconds** from the start of phase 1. The first scan that finds nobody is terminal: `SAY_WIPE`
and a `Lightning Orb` (33138) that kills the raid. There is no grace period and no recovery, so this
is a hard constraint rather than a preference. A separate 5 minute timer
(`EVENT_THORIM_NOT_REACH_IN_TIME`) fires the same orb regardless, which bounds the whole of phase 1.

The split is **latched once per pull** and held until Thorim drops to the floor. Recomputing it per
tick is what let a role predicate flipping mid-fight walk the arena squad into the corridor. Quotas
are `dpsQuota = twentyFive ? 9 : 4`, so 1 tank + 1 healer + 4 DPS (10 man) and 1 tank + 2 healers +
9 DPS (25 man) — **a twelve-bot gauntlet on 25 man**, raised deliberately to make the 170 s window.
A roster that cannot fill them shrinks the gauntlet rather than emptying the arena, and assignment
stops entirely once the arena would drop below 3. The main tank never leaves the arena.

**The quota is a measurement, not a guess.** About 3.87 M of gated corridor health stands between the
pull and phase 2, and ten gauntlet bots measured **23,430 dps** — 7 DPS-role bots at ~3,075 each,
against 4,214 for arena DPS over the same window — which is 231 s. Two more bots buy roughly 30-34 s,
which is the difference between missing and making the deadline. The DPS loop picks in raw roster
order with no melee/ranged split, so the two extra may both be ranged and thin the arena ring.

Every gauntlet pick is **bot-only**. A human still occupies their role but nothing here can walk them
anywhere, so spending the single gauntlet tank slot on a human off-tank just leaves the corridor a body
short. **Set `MEMBER_FLAG_MAINTANK` in the raid frame**: without it `GetMainTankGuid` falls back to the
first tank in roster order, so a human tank ahead of the bot main tank silently takes the role — which
sends the bot main tank down the corridor and leaves the arena untanked. Human squad labels are
written from position on a 500 ms tick — **label only, no bot ever moves a human** — and the bot
gauntlet is deliberately **not** sized down when a human walks the corridor, because squads latch at
pull time while the human is still in the arena.

Arena adds all land 19-24 yd from the centre and the nearest box edge is 42 yd out, so the leash is
**30 yd from `ULDUAR_THORIM_NEAR_ARENA_CENTER`**, 15.8 yd short of the corridor mouth at the lever gate.
Melee get a tighter **24 yd**, which is the furthest an add ever lands, so it costs no uptime. Two
guards back it. The `thorim arena leash` rule holds the generic movers while a bot is outside its leash
— and, alone among the Ulduar guards, does **not** exempt `AttackAction` or `ReachTargetAction`,
because corridor mobs sit ~92 yd out, inside the 100 yd sight cap, and the chase is exactly what walks
a bot out of the box. `ThorimArenaTargetGuardMultiplier` drops a target outside the box, or the leash
and the chase would take turns at the gate forever. **`ThorimArenaLeashBreached` returns early unless
`ThorimSplitActive`**, so the 30 yd leash never sees the phase 2 ring — recorded so nobody "fixes" it
for phase 2.

**A fence is not a formation.** Left with only the leash, the squad diffuses outward until it is parked
against the east edge at x ≈ 2165, which is the gateway. East is the worst direction to drift:
`boss_thorim_arena_npcs::CanAIAttack` is `GetPositionX() < 2180 && GetPositionZ() < 425`, so a bot that
gets there stops being attackable at all and the add re-rolls — and `SelectT()` picks a **random**
arena-side player and gives it 500 threat, so a squad spread over 50 yd puts adds on people no healer
is in range of. Threat is chaotic here by design; being in one place is the only counter.

So the squad is anchored. `GetThorimArenaAnchor` is the single answer trigger and action both read:
the tank holds `ULDUAR_THORIM_NEAR_ARENA_CENTER`, ranged and healers take a ring slot at 13 or 18 yd
(`_ARENA_RING_INNER` / `_OUTER`, five inner slots; clear of the Champion's Whirlwind on the pile, close
enough that anything in it is in range, and as wide as the room allows: adds land at 19-24 and the mesh
gives out past ~26 on the south side), and
melee get the centre only **out of combat** — in the fight they run free on the 24 yd leash. Slots come
from the latched squad in **roster order**, not from the survivors: bots die in here, and ranking by
who is still standing renumbers everyone behind the corpse and shuffles the formation mid-fight. Ring
points are computed and then validated with `GetMapWaterOrGroundLevel` and
`CheckCollisionAndGetValidCoords`, because raw ring geometry is the shape that lands off the navmesh
and `MoveTo` fails silently there. **`ThorimSettleArenaPoint` returning false means "use the centre", and
that fallback is load-bearing**: `CheckCollisionAndGetValidCoords` drags a point back toward the bot, so a
bot outside the pit was handed its own spot, latched it as arrived while still outside the leash, and the
anchor guard and the leash between them froze it for good. `ThorimArenaAnchorNeedsMove` never counts a
spot outside the leash as arrival for the same reason. Slot 0 sits on the bearing from the lever gate to
the centre, so the formation opens away from the corridor. The `thorim arena anchor guard` rule holds the generic movers
once a bot is settled, exempting the chase — an add at 24 yd is up to 38 yd from an outer slot, and a
ranged bot that cannot step into range is silent.

**`GetThorim` must not be sight-limited.** `GetUldUnitByEntry` walks `"possible targets no los"`,
capped at `Playerbots.SightDistance` 100 (~110 effective with the boss's bounding radius). Measured on
the balcony trigger: active at **102.2-106.8 yd**, useless at **110.9-113.1**. The upper hallway is
100-145 yd from Thorim's platform, so all 20 Thorim entry points went blind the moment the squad
climbed the ramp. The companion trap is that a range-free boss handle re-opens
`ThorimEncounterStateIsStale`, which had recorded **178 spurious resets during a single Hodir pull** —
hence its `NearThorimEncounter` gate. Distance alone cannot tell the wing from Hodir's room, 136-176 yd
from the arena centre against a corridor out to 126: below `ULDUAR_THORIM_WING_MAX_Z` (425) height
separates them (gauntlet z 412, arena 420, Hodir 433), and the ramp and hallway above it, reaching 177 yd
out, are a box (x 2100-2205, y −455..−280, z < 450) where Hodir is at x 1980.

**Every bot resets itself** (`resetDone`, cleared while Thorim is engaged). Gated on the raid-wide
`engagedSeen` alone, the first bot through closed the gate on the other 24, who carried `ringArrived`,
`balconyStep` and their melee slot into the next pull: **ten of thirteen** corridor bots started on a
hallway step from two attempts earlier and walked up the middle at x 2141 instead of the east wall at
x 2151, which is also why `ThorimAdvanceBalconyStep` steps down before it steps up.

## The corridor traps nothing can see

**Paralytic Field 62241 / 63540** is `SPELL_AURA_MOD_STUN`, **15 000 ms**, a **12 yd** persistent
ground area, instant, with no cast bar and no telegraph. It is cast by `Thorim Trap Bunny` entries
**33054** and **33725** (`boss_thorim_trap`, `boss_thorim.cpp:921-946`), each polling
`SelectNearbyTarget(nullptr, 12.0f)` about once a second and re-arming after ~50 s. Observed field
reach is up to **13.6 yd**. The two static spawns are **(2134.900, −390.776, 437.311)** and
**(2134.930, −339.696, 437.311)** — dead centre of the upper hallway.

**They are `UNIT_FLAG_NOT_SELECTABLE`, so nothing in the world tells a bot they exist.** No target
value returns them and no dodge node can see them, which is why the balcony route is a hard-coded
waypoint chain rather than a hazard dodge. The hallway is a ~40 yd chamber with both traps on its
centreline and two necks (south y ≈ −418, north y ≈ −316) forcing x 2126-2144, so the only route is
centre → east wall → centre. The chain's worst leg clears a bunny by **15.8 yd**, but only while
`ThorimBalconyAdvanceAction` issues the walk:

- **The drop to the arena still walks the chain once Thorim is down.** The jump edge sits on the
  centreline with both bunnies, and taken straight, ten bots stood 5-12 yd off a bunny, stunned up to 15 s.
- **The jump fires on distance to `ULDUAR_THORIM_JUMP_START_POINT`, never on the step latch alone**: a
  step carried over from an earlier pull fired it from the top of the ramp, a 180 yd arc. The 6 yd
  `_BALCONY_ARRIVE_TOLERANCE` covers the drop point too; an exact-waypoint `MoveTo` does not reliably hit
  0.5 yd, and a bot asked for that circled the edge for the rest of the fight.
- **Corridor ranged take their balcony hold only past `ULDUAR_THORIM_BALCONY_5`**: from any earlier
  waypoint the straight line runs 7-9 yd from a bunny.
- **A pet chasing down the corridor pulls packs the gauntlet squad has not reached**, which is what
  `ThorimStrayPets` / `ThorimRecallPet` stop.

**Phase 2 only starts when a player above z 430 damages Thorim**, once the Rune Giant's death sets
`_isHitAllowed` (`boss_thorim.cpp:511`). `DisableThorim(true)` applies only
`UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_PACIFIED` and a root — he is hostile and attackable throughout —
so a raid that never hits him from up there leaves him at 100% forever.

The healer split (arena 13 bodies / 1.48 M / 2 healers against gauntlet 11 / 0.69 M / 2) is left
uneven on purpose: the gauntlet's clock is the lever, not its survivability.

## Runic Colossus (32872), spawned at (2227.5, -396.179, 412.176)

| Spell | Id | Detail |
|---|---|---|
| Runic Smash, left hand | 62057 | **5s cast**, lights the left-hand bunnies (33141) at x ~2235 / 2246 |
| Runic Smash, right hand | 62058 | **5s cast**, lights the right-hand bunnies (33140) at x ~2210 / 2221 |
| Runic Smash damage | 62465 | 10 yd per bunny; the wave starts 1s after the cast and marches y -385 → -257 at 16 yd / 500ms |
| Runic Barrier | 62338 | -51% damage taken **and a 2000 arcane damage shield per melee swing**; cast at t+10s, 20s duration, recast every 20s |

`EVENT_RC_RUNIC_SMASH` is scheduled in `Reset()` and **cancelled in `JustEngagedWith`**, so the corridor
smash only happens on the approach and stops the moment the Colossus is tanked. The Ancient Rune Giant
has no damage shield — its Runic Fortification (62942) is a friendly buff on its adds.

The two corridor lanes (left x ~2237-2242, right x ~2212-2219) are **index-matched by y**, so a dodge is
a straight index map: same waypoint number, other lane. Each lane sits 2-9 yd from its own hand's bunnies
and 15.5-22.9 yd from the other's. The bot side latches the hand it sees casting and holds the opposite
lane until the other hand goes up — the gauntlet formation follows that preference rather than the
master's own lane, or it would walk everyone straight back into the blast. The Colossus is 131 yd from the
top pair of waypoints, past the 100 yd `Playerbots.SightDistance` cap, so the telegraph is read through a
targeted 150 yd creature lookup rather than the usual target values.

Runic Barrier is effectively permanent, so "stop attacking while it is up" would mean never attacking.
Non-tank melee instead back out to 14 yd below 55% health and return above 80%, keeping their target the
whole time so ranged and instant abilities keep landing.

## Phase 2

| Spell | Id | Detail |
|---|---|---|
| Chain Lightning | 62131 / 64390 | 8 targets at **×1.5 per hop** (`EffectChainAmplitude`): the eighth takes ~17× the first. Each hop goes to the unhit target nearest the last victim, within **8 yd** (`spell_jump_distance` 5.0, not the DBC 10, plus both combat reaches); pets count. First cast 13 s after the phase trigger, then every 15 s |
| Lightning Charge | 62466 | `spell_cone` **75 degrees**, 150 yd, 17343 base nature, **instant with no cast bar** |
| Lightning Orb Charged | 62186 | Lands on a Thunder Orb (33378). `SpellInfoCorrections` patches the amplitude to 5000ms, so it ticks once **5s before** the cone — the entire warning |
| Lightning Charge buff | 62279 | Permanent, one stack per cast: +15% damage and melee haste, **+10% nature damage per stack** |

**The cone is a five-step chain, and only its last step is readable.**
`EVENT_THORIM_LIGHTNING_CHARGE` casts `SPELL_LIGHTNING_PILLAR_P2` **62976** on Thorim himself;
`spell_thorim_lightning_pillar_P2` retargets it to a random **pillar bunny `NPC_PILLAR` 32892**
("Thorim Event Bunny", seven of them); `boss_thorim_pillar::SpellHit` makes the nearest **Thunder Orb
33378** — directly above its pillar, same x,y — self-cast 62186; one tick 5 s later casts **62278** at
Thorim; his `SpellHit` does `SetOrientation(GetAngle(caster))` and casts 62466 at the orb. Measured
lead from the 62976 cast to the damage is **4.91-5.18 s** over 46 casts across six pulls, and the boss
did not move in 15 of 17 windows (worst cone-bearing drift 5.1°).

**Thorim's recorded orientation is not usable** — he has turned back to his victim by the next
snapshot, so **the lit orb is the only readable bearing**. Exposure is decided by your bearing from
Thorim, not by which side of the room you are on: decoding all 33 hits against the boss→orb line gives
a max of **43.8°** and a mean of **17.3°**, with victims 17.3-56.5 yd from the orb. Scoring a parking
spot by "no cone reaches it" scores everything zero and produces a wrong answer; the metric is **how
many of the seven cones cover it**.

**The orb cadence is 4.55-5.29 s lit, 9.70-10.56 s dark**, with no flicker. Everything below — the
shelter latch and the walk-home budget — is timed off that.

Positioning has to clear 5 yd between stacks. The main tank drags Thorim to
`ULDUAR_THORIM_PHASE2_TANK_SPOT` **(2110.7483, −252.65265, 419.440)**. Ranged and healers take one of
`ULDUAR_THORIM_RANGED_SLOTS` = **6** spots, **latched per bot** (`EnsureRangedSlot`,
`thorim.rangedslot`) — re-deriving it live is the exact bug that was fixed, because the shelter table
is keyed by slot. Melee take a **dynamic ring of radius 8.8 around Thorim's live position**, three slots
at the ring anchor bearing +90 / +180 / +270 degrees, which leaves the whole tank side clear and puts
the stacks 12.4 yd apart. The off-tank sits at +20 degrees, inside taunt range for the Unbalancing
Strike swap. Slots are sticky per guid, and **`RingAnchorBearing` reads only the tank spot** — the live
tank was deliberately removed as an input, so the ring cannot rotate as the boss shuffles. That is the
rule, not a fallback. Arrival uses a 3 yd / 5 yd deadband, because a tight one against a ring
recomputed from a moving boss leaves the bot sliding in place — and a moving bot casts nothing.

**The deadband breaks for a hazard.** At ring radius a whole cone dodge is a chord of about 5 yd, so a
settled bot in the cone never moved: **nine cone hits over two pulls** had a safe spot under 5 yd away.
`RingSpotBeatsTheDeadband` moves a bot standing in the cone or a Blizzard zone when the new spot is
clear, and `ThorimRingWantsMove` ORs that escape outside the tolerance test: **40-60%** of move orders
come back refused, and a nested escape re-latched inside 3 yd and was never retried. It reads the arrival
latch and never writes it: a version that latched inside it answered trigger and action differently in
one tick, and one bot stood in a Blizzard for 71 s.

Every melee DPS carries the `behind` strategy from `AiFactory`, so `SetBehindTargetAction` would walk all
three stacks into one arc behind the boss the moment the ring node yields. The `thorim movement guard` rule
holds the generic movers, scoped to a **settled** ring holder and exempting `AttackAction`,
`ReachTargetAction` and `AvoidAoeAction` — a permanent movement freeze is the Void Reaver failure.
`ThorimMeleeRingSettled` refuses a bot above the floor line: a stale ring latch on the balcony froze two
bots on the hallway for three minutes.

### Only the covered slot moves

**The whole-ring rotation was built, measured and deleted.** `RingRotation` reset when the orb went
dark — `ThorimChargedThunderOrb` returns null, so it handed back 0 and the ring snapped to its latched
bearing — making every charge cycle two whole-ring turns: **307 of 352 melee destination changes turned
more than 60°, median 90, p90 178, with the boss having moved 0.0 yd**, an 11.3 yd run each way. It
also solved coverage against the **live** anchor bearing while each bot's destination used
each bot's latched bearing, so it was solving for a ring the bots were not standing on. And it did not work:
**every Lightning Charge hit on a melee bot landed while that bot was running** — 7 of 9 and 4 of 4,
against 0 of 8 hits on stationary ranged. The dodge was causing the damage.

What replaced it is a closed-form per-slot step-out to the nearer cone edge at
`CONE/2 + MARGIN + RING_CONE_CLEARANCE` (37.5 + 15 + 5°), **solved from the latched bearing rather than
from wherever the last cone left the bot**: over 20,000 simulated random cones, chaining offsets drifts
the three slots out of their 90° spacing until two share a point, which is what the rigid turn was
really buying. Result: **1.49 slot-moves per cone against 3.00**, median 4.3 yd and p90 7.3 against
11.31 yd every time, and nobody ever left inside the cone.

**Solved per bot, that step-out merged the ring anyway.** Each bot latched its own bearing off a live
anchor and then dodged the cone and the Blizzard alone, blind to the other slots: **73-81%** of
`thorim.ringspot` notes carried an offset, spread inside one slot ran 3-33°, and one slot's composed
bearings spanned **166°** and reached its neighbour's median. `SolveRingBearings` answers all three at
once for the instance now — everyone in a slot reads the same point, and no candidate is taken within
`ULDUAR_THORIM_RING_SLOT_SEPARATION` (60°, **8.8 yd at r 8.8**) of another slot or of the tank anchor.
The note reads `slot N bearing B solved S`, and the gap between two slots' `solved` is what Chain
Lightning reads. The latch works: 11 Oct carried **one latched bearing per slot** against September's
3-33° of smear.

**60° was exactly the jump at r 8, which is why the ring is 8.8.** Zero margin meant most honoured
floors still bridged, so the radius carries the margin instead: a 60° gap is 8.8 yd, and one Blizzard
forbids **77° of ring rather than 87**. The ceiling is real melee range **9.083 yd** — his `CombatReach`
6.25 (`creature_model_info`, display 28977) plus a player's 1.5 plus 4/3. **`RingPoint`'s range check is
not a melee-range guard**: `GetDistance` subtracts his 6.25, so it only rejects past **15.3 yd**.

**Zones outrank separation when the arc will not hold everything**, the cone never (~20k in one
instant). A bot holds its bearing while a zone sits on it, so giving zones up cost 6-14 consecutive ticks
at 4.3-4.9k, against 40-90k once for a hop. **The anchor term is still angle-only** — the tank sits at
r 5.5, so 60° off him is **7.1 yd** of ground and a chain still bridges the pair. Unfixed.

**A rotation does not always exist.** The 890-snapshot sample behind that claim was too small. On
11 Oct, with a median **4 zones up at once (max 6)**, the 11 yd clearance left under 105° of ring free —
too little for three slots plus the anchor — in **14.5%** of one wipe's Blizzard snapshots and **31.1%**
of the other's, and the whole ring in **5%**. The kill: **0%**. Where one exists the arc to the nearest
clear bearing is still a median 3-7 yd, p90 6-10, max 15.1.

### The opening

Thorim lands at (2134.68, -263.13), mid-camp, and over the first 13.5 s the melee pile chasing him to
the anchor passes within 8 yd of every camp spot. The first Chain Lightning lands **12.0-12.1 s** after
he drops below the floor line, the first cone at 15.9-16.1 s. That cast killed ranged walking into the
camp beside the pile, and the corridor squad's ranged dropping off the balcony stacked within a yard.

So ranged wait it out: the arena squad two to a spot on four north-east rim spots (12.8-15.6 yd clear
of the pile, in cast and heal range), the corridor squad on six platform spots 12 yd apart. The wait
ends, one way, at 12.5 s once Thorim is within 8 yd of the tank spot or the lit cone covers the wait
spot, and always by 25 s — **which is Sif's clock**: she teleports at +8.3 s, casts Frost Nova at
+10.8 s, the first Blizzard bunny appears at **+27.7 s**, its zones reach x > 2140 at +39.7 s and the
north rim at about +33 s. No trace yet shows whether Lightning Charge reaches players above z 430.
Sif's bunny path runs 2.6-7.7 yd from the four arena wait spots, so they serve the opening only.

Effective `Playerbots.SpellDistance` **28.5** and `HealDistance` **38.5** mean `"reach spell"` fires
past about **36.75 yd** from Thorim, which is why the balcony hold needs a `ReachTargetAction` guard.

### Shelters and Chain Lightning

Chain Lightning is cast within 0.8 s of each orb lighting (both run on 15 s cycles) and lands 0.5 s
later. So the camp shelters only while the orb is lit and walks home the moment it goes dark. Held
until the next orb lit instead, 68 of 71 shelter exits over five pulls were walks home as the cast
landed, and one chain ran from three bots stacked on a shelter through two walking home, killing a
healer at hop 6.

**The camp dodge trades one hazard for a smaller one on purpose.** A Lightning Charge is 10-36k in one
instant against a Blizzard tick's 4-5k, so a camp bot in the lit cone leaves the Blizzard to make its
shelter walk. That is what `ULDUAR_THORIM_CAMP_BLIZZARD_ESCAPE_RADIUS` (15) and
`ULDUAR_THORIM_CAMP_MAX_BOSS_RANGE` (32) are for: the escape is short because the generic flee's 30 yd
runs carried healers along the bunny's path into the cone, and past 32 the shorter caster nukes stop
reaching him.

**Spread alone cannot keep the camp out of the cone, hence the shelters.** Packed into one 70° wedge all
six `…_PHASE2_RANGE*_SPOT`s sat in one Lightning Charge, a 13-target burst for 202k and ten dead inside
2 s. Spread over 88° the worst cone still catches four, and the seven cones leave only a 16-28° wedge
clear at any anchor, nowhere near six spots 11 yd apart.

`ULDUAR_THORIM_SHELTER_SPOTS` has no rows for orb indices 4 and 5 ((2092.95, −263.00) and
(2104.94, −233.44)), whose cones point away from the camp. Its longest run, 25.6 yd, is ~3.7 s against
the 4.9 s worst warning, and a bot commits from wherever it stands, so walking home the moment the orb
goes dark costs an extra run only when two orbs in a row cover one slot. **Blizzard-track clearance is a
preference there, not a rule**: three of orb 2's four rows sit 7.6-8.2 yd from the track, inside the
9.8 yd reach, because the pocket off that cone and 11 yd clear of the track is ~97 sq yd and four points
9 yd apart do not fit; standing on a shelter was 14% of the camp's Blizzard damage, walking to one 63%.
Rows (orb 3, slot 1) and (orb 6, slot 5) sit under 3 yd from home, 34-37° off the cone, and walk only
because the camp trigger arrives at 1 yd, not the ring's 3.

Pushing camp spots outward from the **live** boss was modelled and rejected — it brings camp pairs to
6.4-7.1 yd under orbs 1 and 3, inside the 8 yd jump. The melee ring's cone offset is likewise left
untouched: 25 of 44 ring moves inside the orb→Chain-Lightning window are cone dodges that have to
happen then.

## Somebody has to hold the boss, and a taunt will not do it

**Nothing in the encounter put a tank on Thorim before `8cd13085a`.** `ThorimDpsTargetAllowed` returns
true for everything once he is on the floor, so `ThorimDpsPriorityTrigger`'s tank branch
(`currentTarget && !allowed`) is always false in phase 2; and `ThorimPhase2PositioningTrigger`'s
MainTank branch required `boss->GetVictim() == bot`, so a tank had to *already* hold the boss to be
told to walk him to the anchor. The cost was **15.3 s and 9.6 s of untanked phase 2** with 3 deaths
inside each vacuum, Thorim on a non-tank for **29% of the phase** taking **64% and 73%** of his output
there. It is chronic, not new: the 6:31 "kill" everyone compares against had a 20.2 s vacuum and was
**49% untanked**.

`ThorimTankPickupTrigger` / `ThorimTankPickupAction` close it. The main tank picks up whenever free and
the first assist tank only when the main tank is dead or absent, so **only one bot ever taunts**.

**`ResolveThorimPhase2Role` gives `MainTank` to whichever tank Thorim is swinging at**, ahead of group
order. `GetMainTankGuid` reads the raid frame's flag with no tank check, so a human wearing it took the
slot and assist index 1 fell into the melee ring, orbiting him at 8 yd instead of walking him: Thorim sat
**26 yd** off the anchor for a whole pull. Every tank past the off-tank now waits on the off-tank
bearing. `ThorimTankPicked` takes `tank assist` away from tanks in phase 2 only: left voting, it held a
tank on a Warbringer for the first 30 s while Thorim ate the camp, and in phase 1 holding whatever swings
at the raid beats steering a tank onto the ranged pick.

**A taunt forces the target for 3 s and then hands the boss to the highest *real* threat — and Thorim's
threat table is empty at the phase change**, because he sits at 100% untouched through phase 1. So the
pickup taunt hands him straight back: at 2:56.706, exactly 3 s after the tank's Hand of Reckoning,
Unbalancing Strike went to a hunter **44 yd out**. That is why the pickup fires 6-7 times a pull against
13-31 victim changes, why the answer is "keep the tank parked and swinging" rather than re-taunting, and
why re-taunt cadence is still open.

**The class taunts fight the encounter for the boss.** `TankPaladinStrategy.cpp:114-121` wires
`"lose aggro"` to `hand of reckoning` at `ACTION_HIGH + 7`, falling back to `righteous defense`; it
fires every ~2 s at `rel 27.0`, so a bot paladin rips the boss off a human tank — **34 and 45 victim
changes a phase** against 16 Dark Commands. The `thorim taunt guard` rule silences them, a `Block` on
the `Taunt` family gated on the bot's **current target** being Thorim,
so taunting an add off a healer still works. **`righteous defense` must be in any name-match list** —
it is half the taunts in the trace.

**An anchor measured off the thing you are dragging has no fixed point.** The off-tank's spot used to be
a ring point off the boss's **live** position, so when he held the boss he walked to a point measured
from the boss, the boss followed, the point moved, and the pair drifted with nothing pulling them back:
Thorim walked **256 and 92 yd** in phase 2 (137 and 76 yd/min) against **39 yd** (12.5 yd/min) in the
kill run, sitting a median 21.3 and 29.9 yd off the tank spot. Whoever holds him now gets the absolute
anchor. Two related traps: **a victim change re-seats him at the chase near point and he never moves
again** (`TargetedMovementGenerator.cpp:362`) — in one pull he rested 7.4 yd off the tank spot against
4.1-5.9 in the others, putting the outermost ranged slot 6.9 yd outside the melee ring — and whoever
holds him eats the Blizzard, which is why the dodge was towing him across the arena for **36,283 and
32,799** of avoided damage against 11-38k from a single Thorim swing.

**The anchor is also the Chain Lightning answer.** Boss 18-28 yd off the anchor: 8 targets, **174,932**
damage running 4,695 → 47,024, out of the melee ring and into the ranged camp, **four bots dead in
0.1 s**. Boss on the anchor, fifteen minutes later: 8 targets, **141,198**, all melee and tanks,
**zero deaths**. What decides survival is where the chain *ends*.

## The Charge Orb field is a 32.3 yd circle, not a 35 yd sphere

`EVENT_THORIM_CHARGE_ORB` fires 14 s into phase 1 and repeats every 16 s, casting **Charge Orb 62016**
at one of the seven **Thunder Orbs (33378)** (`conditions` row `(13,1,62016,…,33378)` restricts the
target). 62016 is a periodic trigger, period 1000 ms, **duration 15 000 ms**, trigger spell **62017**,
so the orb carries the aura for the whole window. **Lightning Shock 62017** is base 2830 + die 339 →
**~2831-3170 nature per tick** at `EffectRadiusIndex 21` = **35 yd**, not dispellable and with no cast
bar of its own.

**The radius test is 3D and the orbs float 13.5 yd above the floor**, so the 35 yd sphere cuts the
floor as a **32.3 yd circle** (`sqrt(35² − 13.5²)`) centred under the orb — the derivation behind
`ULDUAR_THORIM_CHARGED_ORB_RADIUS = 32.3f` and its 4.0 yd margin. Reading the DBC radius as a flat
distance puts the boundary 2.7 yd too far out. The trace shows the knife edge: at 1:14 a bot 34.9 yd
from the orb took every tick, while one 3 yd further east at 37.6 yd took none.

The seven Thunder Orb spawns are fixed, all at **z 433.3** and all **42.0 yd** from the arena centre
(`data/sql/base/db_world/creature.sql:148358-148370`): (2105.04, −292.56), (2092.95, −263.00),
(2104.94, −233.44), (2124.30, −222.60), (2145.50, −222.62), (2164.20, −233.47), (2164.55, −293.00).

Before anything read aura 62016, bots stood in the field for its full 15 s: Lightning Shock was
**50.3% of all damage taken** in one trace (503 097 over 340 hits).

**Two orb markers need two cache slots.** `thorim.chargedorb` once toggled between 0 and a guid **78
times in 15 s**, because the helper cleared the guid before every rescan *and* phase 1's Charge Orb
and phase 2's Lightning Orb shared one slot and evicted each other, forcing a 150 yd grid sweep several
times a second. Assign only on change: the melee ring re-solves on any change of the lit orb's guid
(`ringOrb`), so the flapping cache also flipped six melee onto a 90° bearing several times a second.

## One Stormhammer debuffs the whole arena

**Stormhammer 62042** fires every 16 s at one random enemy within 100 yd (2451-2551 damage plus a 2 s
`MECHANIC_STUN`). `data/sql/base/db_world/spell_linked_spell.sql:562` —
`(62042, 62470, 1, 'Thorim - Stormhammer')`, type `SPELL_LINK_HIT` — makes the **hit unit** cast the
linked spell on itself with Thorim as original caster, so the blast is centred on the player the
hammer landed on, not on the boss. **Deafening Thunder 62470** is 4625-5376 nature at
`EffectRadiusIndex 14` = **8 yd**, plus `SPELL_AURA_HASTE_SPELLS` at **−75** at `EffectRadiusIndex 18`
= **15 yd**, duration **8 000 ms**. Four times the cast time, and not dispellable.

It cannot be dodged reactively: the target set is chosen inside `Spell::SelectSpellTargets` and is not
readable from a bot, and the aura is applied before anything can see it. **The only lever is how many
bots one 15 yd blast covers.** Every sample from 0:49 on used to put 11-14 of the 14 arena bots inside
a single 15 yd circle; the same clustering feeds Dark Rune Champion Whirlwind (15578, 8 yd).

## Hard mode

Sif is summoned every pull and normally channels, then despawns after the 150s dominion timer. If the
raid clears the gauntlet fast enough she joins instead and casts Frostbolt Volley (**62580**), Blizzard
and Frost Nova (**62597**, teleport then point-blank). Her single-target **Frostbolt is 62583**.

**Frostbolt Volley is the fight's floor.** `EffectRadiusIndex` 22 is a **200 yd** radius, it is instant,
and `InterruptFlags` is 0, so nothing dodges, kicks or outranges it. It is **25-26% of all incoming
damage in every pull**, only resistance and healing touch it, and it is why this fight has a survival
ceiling at all.

**Her ids do not split by difficulty.** `boss_thorim.cpp:56-59` casts 62580 / 62583 / 62597 flat in both
sizes. The twin rows 62604 / 62601 / 62605 are what this doc carried before and **none of them is ever
cast**; everything below was re-measured against the three that are.

**Only Frost Nova can be kicked, and it is not worth a node.** Frostbolt Volley 62580 and Frostbolt
62583 are both `InterruptFlags 0` — the 62601 twin's `15` is what made Frostbolt look kickable, and it
never lands. Frost Nova 62597 is `15`, but she casts it straight out of a teleport and it is 2-4% of
incoming. Chain Lightning **64390** carries `InterruptFlags 1`, so only Thorim's own movement breaks it.

**Blizzard is a trail, not a circle.** Every 36-41s a `NPC_SIF_BLIZZARD` 32879 spawns at
(2108.7, -280.04) and walks a fixed eight-waypoint loop for 30s. Its aura (62577/62603) drops a 10s, 8 yd
zone (62576 10-man, 62602 25-man) every 2s: up to six live, a median 26 yd behind it. **Test the zones
(`GetDynamicObjectPositions`), not the bunny** — the bunny alone missed 16 of 20 melee hits in one pull.
Every camp home spot clears the loop by 12.9 yd or more.

**The two Blizzard radii mean different things.** `ULDUAR_THORIM_SIF_BLIZZARD_RADIUS = 15.0` is the
generic dodge's **trigger** radius, not a damage radius; at 12 the flee stopped while the bot still stood
in the zone. The damage-side constant is `ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE = 11.0` — the 9.8 yd
reach measured over 161 ticks (corrected DBC 8 plus both combat reaches through `IsWithinDistInMap`)
plus a yard. 15 instead grows the ring's slide from a median 3-7 yd of arc to 9-12 and buys nothing.
`ThorimPhase2PositioningTrigger` stands down while `ThorimSifBlizzardTrigger` is active, as the arena
anchor does: without it the anchor walked bots back into the zone they had left, and the four ranged who
took the most Blizzard were the four who ran furthest.

**That dodge excludes the melee ring outright**, which makes the ring solver melee's only answer to a
Blizzard. While the solver could also give the zone test up, melee had none at all: the Blizzard action's
verdicts name **only ranged and healers** in all three 11 Oct pulls. Ticks are **4,300-4,900 median,
6,187 max** — not the ~3k this doc and the code comments carried — and melee stood **6-14 consecutive**
of them, one bot 14 ticks for **72,853** over 11 s. That cost melee **320k and 457k** in the two wipes,
more than Chain Lightning's whole 188k in one of them. The kill never exceeded **4 ticks**, the cleanest
single difference between it and the wipes.

**Frost Nova is two radii too, and the root is the wider one.** 62597 damages at **12 yd**
(`EffectRadiusIndex` 32) and roots at **15** (index 18). Measured, hits stop at **11.6 yd** and roots
reach **14.9**, last **~6 s**, and **10-62%** land on ranged and healers already clear of the damage.
Rooted, a camp bot cannot walk home, take a shelter or leave a Blizzard zone: the most-rooted 12 Sep
pull refused **31** positioning moves and **11** Blizzard escapes, and was also its worst Blizzard pull.
The damage stays teleport luck: she casts 2.5 s after teleporting, ~8.6 yd of travel under the Volley
slow, so nobody she lands on escapes either radius.

**The 16 yd back-off built for that root is deleted, and it had never once run.**
`thorim sif frost nova trigger` was asked **25,823x per pull and never returned true**, in all four
19 Sep pulls, because it resolved Sif through `GetFirstAliveUnitByEntry` →
`"possible targets no los"`, and `AttackersValue::IsPossibleTarget` drops `UNIT_FLAG_NOT_SELECTABLE`
— which Sif carries in `creature_template.unit_flags` (33587200) and nothing in the boss script clears.
**Anything that resolves Sif through a targets value cannot see her; use `FindNearestCreature`, which
the Blizzard trigger already does.** Repairing it was rejected on its own merits too: ranged sit inside
16 yd of her **20-34%** of the arena phase, so it would have fired constantly against a spell worth 2-4%
of incoming whose damage never reaches past 11.6 yd.

**The generic 30 yd flee was producing the damage it dodged.** `MoveAwayFromCreatureAction` scores 8
compass rays out to 30 yd and takes the **furthest**, so every accepted melee flee asked for the full
30 yd, landed 33-53 yd from the boss, still took another Blizzard tick **23-58%** of the time — the
circuit is a ring, so running outward lands on another arc — and blocked every other move for the walk
(31.7 s and 15.0 s stalls). **34 / 56 / 58% of melee Blizzard damage was taken away from the ring.** Of
199 and 246 attempts, 161 and 204 were refused by the movement gate anyway. Melee now slide around the
trail instead, and tanks are exempt **by role**.

**Detector: Sif (33196) alive AND `GetPositionZ() < 429.6`** — she spawns at the throne and only
`NearTeleportTo`s onto the arena floor when she joins. This reuses the same floor threshold the
normal-mode Thorim strategy already uses.

**Hard mode changes what a trace means, and the boss with it.** Missing the clock puts **62565** on
Thorim: `MOD_DAMAGE_PERCENT_DONE` **-40%**, `MOD_INCREASE_HEALTH_PERCENT` **-30%**. A normal-mode Thorim
is therefore **17.57 M**, not 25.101 M: any figure carried across the two compares different bosses, and
DPS from a normal-mode kill overstates the raid by **~43%**. The 6:31 and 5:34 "kills" used as
yardsticks were both normal mode: Sif vanished and dealt zero, against 520k-1.04M in hard-mode pulls.
**The first real one is `603_1_thorim_1789848059`**, 19 Sep: Sif in the fight, the full 25,101,000
taken, three deaths, 187 s of arena.

## The arena adds, and what to kill first

`GetThorimDpsTarget` buckets the encounter's creatures by entry in one sweep — acolytes, evokers,
champions, warbringers, commoners, and the corridor's Iron Ring and Iron Honor Guard — and hands each
bot a pick. It is deliberately **not** a raid icon: an icon is a sticky override that `RtiTargetValue`
hands back before the smart picker runs, so a wrong mark cannot be corrected until the bot leaves
combat (see [../../engine/pitfalls.md](../../engine/pitfalls.md)).

Measured share of arena damage taken: Dark Rune Champion **41%**, Warbringer 21.2% (melee 14.0%, Runic
Strike **62322** 7.2%), Evoker 16.5% (Runic Lightning **62445**). Champions are melee, top of the list and
already standing on the bots — the generic picker used to send melee straight past them at an Evoker;
ranged take Evokers first.

**`thorim disable automatic targeting` holds the generic pickers for every non-tank in phase 1**, or
they alternate with `ThorimDpsPriorityAction` instead of losing to it: the row goes quiet once the bot
holds its pick, the engine falls through to `dps assist` at 50, and that retargets. A traced arena squad
switched target twice a second all fight.

**Phase 1's elites outlive phase 1.** 7-9 are still standing when Thorim lands — 5 Champions (0.61 M),
7 Warbringers (0.53 M), 6 Evokers (0.41 M) — and deal **113-214k** over the rest of the fight; the
Commoners die on their own (47 units to 9 by +27 s). One Evoker's Lightning Shock was 26k of the 55k that
killed a healer at **+16 s**, so the phase 2 camp clears **Evokers only** and melee and tanks stay on the
boss: all 1.55 M costs ~15 s of boss time to save ~200k, while a camp bot kills an Evoker without
stepping off its spot.

Dark Rune Commoners stack **Low Blow 62326**, whose second effect is
`SPELL_AURA_MOD_DAMAGE_PERCENT_DONE` at **−3% a stack**; one pull peaked at 25 stacks on a single bot,
i.e. −75% damage done. Only late and only on a few bots, so it is a reason not to ignore Commoners
forever rather than an explanation for a whole fight.

**A target guard has to exempt the encounter's own picker by name.** `ThorimIsTargetSelectionAction`
listed only the generic pickers, so `ThorimDpsPriorityAction` — the one node that *clears* a forbidden
target — was vetoed by `ThorimArenaTargetGuardMultiplier` **42 times** in a pull. The guard fired
because the target was wrong, and killed the action that would have changed it.

## Pets

**Gauntlet pets are leashed to 50 yd of their owner.** `ULDUAR_THORIM_PET_OWNER_LEASH_RADIUS` is
`ULDUAR_THORIM_DPS_TARGET_RANGE`, chosen because that is how far the encounter picker looks for a
target, so a pet beyond it is further out than its owner can see anything worth killing. Legitimate
excursions peaked at 42-54 yd; runaways hit 90-164.

The runaways came from the **phase 1 boss latch**. The opening trash dies ~0:25 and the first Dark Rune
wave lands ~0:50; in that gap the encounter picker has nothing,
the `thorim disable automatic targeting` rules let it through rather than strand the bot, and the only hostile
in range is Thorim on his balcony. **18 of 25 bots held him** — one for 113 snapshot rows — and the
acquire→clear→re-acquire loop re-pointed their pets, which took the only walkable route: the 300 yd
corridor (Feral Spirit 164 yd from its owner, Worm 128). The fix whitelists the opening trash as the
**last** DPS tier and holds the suppression open on `ThorimSplitActive`.

`ThorimRecallPet`'s `SetIsFollowing` / `SetIsReturning` pairing is inverted relative to Naxx's
`RecallControlledPetsToBot` and is **deliberately left as written** — the arena pets stayed in the box
100% of the pull.

**Pets are fine here, do not re-audit.** 87-93% in range of their own target, 2-6% orphan targets, no
deaths, no geometry clipping ("through textures" does not reproduce — the largest steps are 8-13 yd of
snapshot jitter and one Shadowfiend Shadowcrawl). Pet Growl is a non-issue: 102 casts, hostiles aimed at
a pet on 52 of 55,171 rows, and across three later pulls the boss's victim **never once** became a pet.
Pets deliberately do not dodge anything — they survive on their own resistances, and a pet AI that steps
out of hazards is a cost with no payoff.

**`thorim pet leash` reads `LOST … ran 0x` in coverage by design.** `won` is only stamped when `Execute`
returns true, and this action returns false so the bot still gets its own node that tick. Its real
evidence is the verdict stream, where it shows `FAILED` 25-31 times a pull.

## Aspect of the Wild has exactly one holder

Thorim's nature damage earns the aura, but 49071 is `APPLY_AREA_AURA_RAID` + `MOD_RESISTANCE_EXCLUSIVE`,
so a second hunter adds nothing and pays Aspect of the Viper for it. `GetNatureResistanceHunter` picks
one — whoever already holds the aura, else the first hunter at or above the Viper entry threshold — and
`BossNatureAspectHoldMultiplier` pins that hunter's aspect slot. Every other hunter keeps Dragonhawk and
stays free to Viper.

**A drained holder still covers the raid**, so when the only hunter wearing the aura is below the Viper
threshold the helper returns `nullptr` — nobody drafted, nobody pinned. Holding costs nothing, and the
outgoing holder drops the aura itself on its next Viper cast because aspects are exclusive. Drafting a
replacement before that is what put two holders on `603_4_elder-stonebark_1789241915`, the second one's
Dragonhawk vetoed for nothing. Locality is only "same map", so a stale holder outside the aura radius
still suppresses the draft.

Eligibility reuses the Viper band, so the role moves on mana swings rather than per tick: a hunter that
hands off at 30% is not eligible again until Viper carries it back to 60%. Two healthy hunters trade the
role every minute or so, each taking its turn on Viper. When nobody is eligible and nobody holds the aura
the first hunter is pinned regardless — raid-wide resistance outranks one hunter's mana — so **a raid
with a single hunter pins that hunter for the whole fight**. Intended, not a bug.

Verified on `603_4_kologarn_1789240634` (one source, 9 recipients, the free hunter cycling Viper) and
`603_4_elder-stonebark_1789241915` (node at `rel 60.00`; `aspect of the dragonhawk` **and** its
`aspect of the hawk` alternative both vetoed at `rel 0.00`, so the veto list has to name the whole chain).

Kologarn, Freya, and VoA's Emalon and Archavon share the mechanism.

## The frost aura needs a bot and a hold

Frost is about **half of all incoming damage** here — one 11 Oct wipe took **~1.57 M** of it — and Frost
Resistance Aura's 130 resistance is **20.4%** off every frost hit: the level-83 constant is
`150+(83-60)·(83-67.5)` = **506.5**, so `130/(130+506.5)`. That wipe ran **no frost resistance at all**,
worth ~320k; the other two had it up the whole fight by luck.

Two things blocked it, both now handled Thorim-side. `BossFrostResistanceTrigger` returns
`member == bot` for the **first alive paladin in group order**, so when that is a human nothing ever acts
on it and no bot paladin can qualify. And Thorim pinned the hunter's aspect but not the paladin's aura,
so the exclusive slot was traded back on the next GCD. `ThorimTickFrostAuraPaladin` latches
`frostAuraPaladin` from **bot paladins only** (lowest guid, must know the spell), the
`thorim frost aura` node casts it, and the `thorim frost aura hold` Block pins the slot against the
competing auras. Nobody is elected in normal mode, where Sif never leaves her balcony.

Nature needs nothing: Aspect of the Wild was up in all three 11 Oct pulls.

## Known gaps

**The wipe is a damage race, and nothing positional answers it.** Tank intake reaches **190-260k per
20 s by +100 s** (Thorim's melee 110-150k of it) against **180-210k healed**; the Lightning Charge stack
buff takes his melee from **6.8k to 15.6k a swing by +140 s**, Lightning Charge **13k → 28k**, and Chain
Lightning peaks at **31.5k**. **The survival ceiling is ~190-200 s**: past it the tanks fall with
healers at 13-17% mean mana and the rest melts in 20-30 s. Boss DPS over the arena's first 150 s
measured **112.8-132.8k** on 19 Sep and **127.4 / 131.9 / 134.8k** on 11 Oct, so 25.101 M needs
**186-197 s** — the 11 Oct kill did it in **196 s of arena**. Damage is no longer the binding
constraint: the 19 Sep wipes died at **3.1% and 3.6%**, but the 11 Oct ones at **34.3% and 37.4%**,
killed by the ceiling rather than falling short of it. Phase 2 intake is **3.0-5.0 M** against normal
mode's **0.64 M**. **Sif is about half of that and most of it is undodgeable** — the Volley also carries
a **-51% slow**, which doubles every reposition. Mana is the wall, and not for want
of cooldowns: healers run **29-30% overheal** with Innervate, Mana Tide, Hymn of Hope, Shadowfiend, gems
and potions all already spent. This is the encounter's actual remaining problem.

**A human tank holding him in phase 2 is called** ([calls to human tanks](../README.md#calls-to-human-tanks)):
once per hold turn from 6 Lightning Charge stacks (62279 on Thorim, ~95 s in, swings about doubled), and
under 50% at any stack count, never while he carries Unbalancing Strike (a bot takes him next). Bot tanks have
no timed rule here, only their class triggers, by the user's choice.

**Chain Lightning is a hop-count problem.** 64390 carries `EffectChainAmplitude` **1.5**, so the
multipliers are 1, 1.5, 2.25, 3.4, 5.1, 7.6, 11.4, 17.1 on a 4625-5375 base: hops 1-4 are noise, hop 5
kills cloth, hops 6-8 kill outright. One measured cast ran 5,574 → 7,831 → 12,683 → 21,153 →
17,923 → 43,189 → 69,855 → **89,760**, and two of those took three and four melee bots. Every jump
seen was ≤ 7.9 yd against the 8.0 yd reach; the seed is `SelectTarget(Random)` off the threat list every
**15 s**. Improved, not fixed: 11 Oct still ran **6-8 hops on players**, peaking
**46,815-65,530**. **The tanks bridge**: at 3.4-7.6 yd off the boss they are inside the jump of more than one
slot, which is how a chain crossed the ring, and why the separation floor counts the anchor too. The
camp is stacked by construction — 14 bots share 6 slots, so the tightest occupied pair is **0.00 yd in every
pull** — which is pre-existing and accepted.

**Pets chain, and the recorder cannot see it.** `dmg` is written for raid members only — **0 of
1,047-1,525 rows per pull** name a pet — so every hop count here is a floor. The core settles what the
trace cannot: `WorldObjectSpellTargetCheck` skips **totems and nothing else** under `TARGET_CHECK_ENEMY`,
so pets and guardians take hops and can be the seed. **24-28 pet bodies** sit inside 10 yd of him at
once, **100% of them within 8 yd of a melee body**, and PetAI parks a pet anywhere in a 90° arc behind
him — straight into the gaps the slot separation opens. They are chased onto a slot bearing instead
(`ThorimPetsOffRingSlot`, on the pet leash node),
which costs no uptime: at ring radius a pet is still inside the 9.1 yd melee reach on any bearing. **A
guardian with no `CharmInfo` still takes a move order**, so Army and Feral Spirits steer here even though
a player cannot command them.

**Every bot's pets get a slot, not just a ring melee's.** The first cut covered `MeleeRing` owners only,
about half the bodies: **ranged owners held 42-48%** and had none placed. A ring melee uses its own slot,
everyone else one off its guid counter modulo 3 — stable per tick and an even spread, so a chain held in
one cluster spends its late hops on pets rather than reaching the next slot's players. **Human-owned pets
are 13-30% of the bodies and are not steerable at all**, no `PlayerbotAI`. Logging pet damage in
`Bot/Obs` is still what it would take to count the real hops.

**A bot-only raid never walks the corridor at all.** `ThorimGauntletPositioningTrigger` opens with
`if (!master) return false;` and derives progress from the master's waypoint.

**The last 67 yd of the lower corridor are unwaypointed** — waypoints stop at y −329 while the Runic
Colossus is at y −396, so `ThorimRunicSmashAction` cannot resolve a lane index on the final approach,
where the smash is most dangerous.

**Both 110 yd gates cut the left lane short.** `ThorimGauntletPositioningTrigger` and
`ThorimFallFromFloorTrigger` (`UldTriggers_Thorim.cpp:54` and `:126`) bail past 110.0f from the arena
centre, while the left lane's last three waypoints are **117.3 / 120.8 / 126.1 yd** out — so a left-lane
squad loses its corridor node for the final stretch and finishes on `follow`.

**The ~28 s opening wait is deliberate.** The squad does not move until the master reaches
`ULDUAR_THORIM_NEAR_ENTRANCE_POSITION` (first accepted corridor move at 0:27.7 / 0:24.8). It is left in
because it is the difference between missing and making the clock.

**The clock is no longer the problem: 3/3 on 11 Oct at +18.6 / +30.0 / +31.0 s, 4/4 on 19 Sep, 3/5 the
week before.** Margins from the Touch of Dominion cast ran **+6.8 / +23.4 / +24.2 / +37.9 s** on 19 Sep,
against **+25.6 / +32.8 / +32.2 / -4.0 / -11.1 s** on 12 Sep. The
spread is corridor-trash time and it is still Iron Honor Guard: **92.8 s** held on the slow pull against
**48.6-51.4 s** on the rest, with Colossus and Rune Giant time identical in all four. That pull is the
one that finished on **+6.8 s**, with a raider already dead before the floor and 2-3x the Champion
damage after it. Snapshot cadence never moved (p50 227-229 ms), so it is bot behaviour, not load.

**The shelter run is uncapped.** Observed distances to the shelter when the note fired were **52.1 and
60.6 yd** against a table solved for 25.6, and a bot that does not make it takes the cone while moving.
Revisit only if the still-walking count grows once the other gaps are closed.

**Arena adds are never tanked** — in any trace, including the kill, 0-8% of them target a tank — so they
free-roam onto the camp and **32-39% of phase 1 add damage lands on ranged**. Chronic, costs almost no
lives, deliberately out of scope. Nothing taunts an add off whoever it rolled, and nothing recovers the
fight once the arena squad is dead; the 5 second scan leaves no room to walk anyone back.

Documented, not implemented: the in-combat `SPELL_SMASH` 62339 frontal cone (60 degrees, 3s cast — a
different spell from the corridor Runic Smash), Rune Detonation 62526, Stomp 62411, and
Runic Fortification 62942. Heals on a freshly swapped-in tank are also thin — one took no heal at all in
the 2.9 s he died over — but that is generic heal AI, not this strategy.

**The squad split counts human tanks.** `AssignThorimSquads` counts tanks with a bare
`PlayerbotAI::IsTank(member)` over the whole roster, so a human protection paladin makes `tankCount`
read 2 and the raid's only *bot* tank is sent down the corridor — leaving the arena tanked by the
human, which is also why a human shows up high on the arena damage meter. The fix is to count and
pick tanks bot-only (`IsBotPlayer(member) && PlayerbotAI::IsTank(member)`) and to skip tanks in the
DPS loop, since `IsDps` is also true for a protection paladin. **Excluded at the user's request,
twice — do not re-audit.**

**Burst cooldowns stay held for the whole of phase 1** — roughly 500 suppression episodes across one
arena squad: tinker 153, trinket 107, Blood Fury 58, Rapid Fire 28, Readiness 27, Blade Flurry 23,
Adrenaline Rush 18, Death Wish 16, Bestial Wrath 14, Army 13, Killing Spree 12, Recklessness 11,
Berserk 10. Recorded so it is not rediscovered.

### Two things that were measured and backed out

**The rigid whole-formation rotation for ranged.** Turning the ranged camp off the lit orb costs about
**500 yd of walking per ranged bot over a 191 s phase 2** — 342 yd even at the spell's own 37.5°
half-width — and buys **647k** of damage taken that the healers' 3.5× margin already covered for free,
against roughly **4.5 M** of ranged output lost. **Worth revisiting only for hard mode**, where that
margin is gone (17.9k incoming against 20.7k hps); `IsThorimHardModeActive` already exists so the gate
is cheap, but it needs its own measurement.

**Moving the tank anchor east.** `(2135.0, −263.0, 419.846)` is a far better spot on every axis that
matters:

| | shipped `_PHASE2_TANK_SPOT` | proposed (2135.0, −263.0) |
|---|---|---|
| clearance to the Blizzard track | **5.1 yd** | **29.7 yd** |
| r=8 melee-ring clearance (needs > 9.8) | **−2.9 yd** | **+21.7 yd** |
| from the arena centre | 15.7 yd | 11.2 yd |

A 0.5 yd grid search bounded to 22 yd off the arena centre puts the optimum at (2134.5, −263.5) at
29.9 yd, so the proposal is **within 0.2 yd of the best available** and there is nothing better to hold
out for. Navprobe agrees: poly distance 0.130, the r=8 ring **24/24 on mesh**, and a 26.37 yd / 8
waypoint walk from the current anchor. The prize is real — tanks took **157k** Blizzard in one pull
(61-126k each), and Blizzard is **16-21% of tank intake**.

**The cost is that it breaks 11 of the 18 camp points.** Home slots 1-5 fall to 6.0-18.3 yd against the
22-32 yd band, and seven shelters land 4.5-17.3 yd from the anchor against the 22 yd floor that stops
the ring bridging Chain Lightning in. It is a full re-solve of six home spots and twelve shelters plus
eighteen navprobes — not a nudge. And **a navprobe-clean point can still be a wrong point**: a first
nine-spot camp solve ignored the Blizzard lane and put two points 5.6 and 6.8 yd from the bunny's path,
and they navprobed clean.
