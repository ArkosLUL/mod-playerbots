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
guards back it. `ThorimArenaLeashMultiplier` holds the generic movers while a bot is outside its leash
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
the tank holds `ULDUAR_THORIM_NEAR_ARENA_CENTER`, ranged and healers take a ring slot at 10 or 14 yd
(clear of the Champion's Whirlwind on the pile, close enough that anything in it is in range), and
melee get the centre only **out of combat** — in the fight they run free on the 24 yd leash. Slots come
from the latched squad in **roster order**, not from the survivors: bots die in here, and ranking by
who is still standing renumbers everyone behind the corpse and shuffles the formation mid-fight. Ring
points are computed and then validated with `GetMapWaterOrGroundLevel` and
`CheckCollisionAndGetValidCoords`, because raw ring geometry is the shape that lands off the navmesh
and `MoveTo` fails silently there. Slot 0 sits on the bearing from the lever gate to the centre, so the
formation opens away from the corridor. `ThorimArenaAnchorGuardMultiplier` holds the generic movers
once a bot is settled, exempting the chase — an add at 24 yd is up to 38 yd from an outer slot, and a
ranged bot that cannot step into range is silent.

**`GetThorim` must not be sight-limited.** `GetFirstAliveUnitByEntry` walks `"possible targets no los"`,
capped at `AiPlayerbot.SightDistance` 100 (~110 effective with the boss's bounding radius). Measured on
the balcony trigger: active at **102.2-106.8 yd**, useless at **110.9-113.1**. The upper hallway is
100-145 yd from Thorim's platform, so all 20 Thorim entry points went blind the moment the squad
climbed the ramp. The companion trap is that a range-free boss handle re-opens
`ThorimEncounterStateIsStale`, which had recorded **178 spurious resets during a single Hodir pull** —
hence its `NearThorimEncounter` gate.

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
centre → east wall → centre.

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
top pair of waypoints, past the 100 yd `AiPlayerbot.SightDistance` cap, so the telegraph is read through a
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
is keyed by slot. Melee take a **dynamic ring of radius 8 around Thorim's live position**, three slots
at the ring anchor bearing +90 / +180 / +270 degrees, which leaves the whole tank side clear and puts
the stacks 11.3 yd apart. The off-tank sits at +20 degrees, inside taunt range for the Unbalancing
Strike swap. Slots are sticky per guid, and **`RingAnchorBearing` reads only the tank spot** — the live
tank was deliberately removed as an input, so the ring cannot rotate as the boss shuffles. That is the
rule, not a fallback. Arrival uses a 3 yd / 5 yd deadband, because a tight one against a ring
recomputed from a moving boss leaves the bot sliding in place — and a moving bot casts nothing.

Every melee DPS carries the `behind` strategy from `AiFactory`, so `SetBehindTargetAction` would walk all
three stacks into one arc behind the boss the moment the ring node yields. `ThorimMovementGuardMultiplier`
holds the generic movers, scoped to a **settled** ring holder and exempting `AttackAction`,
`ReachTargetAction` and `AvoidAoeAction` — a permanent movement freeze is the Void Reaver failure.

### Only the covered slot moves

**The whole-ring rotation was built, measured and deleted.** `RingRotation` reset when the orb went
dark — `ThorimChargedThunderOrb` returns null, so it handed back 0 and the ring snapped to its latched
bearing — making every charge cycle two whole-ring turns: **307 of 352 melee destination changes turned
more than 60°, median 90, p90 178, with the boss having moved 0.0 yd**, an 11.3 yd run each way. It
also solved coverage against the **live** anchor bearing while each bot's destination used
`LatchedRingBearing`, so it was solving for a ring the bots were not standing on. And it did not work:
**every Lightning Charge hit on a melee bot landed while that bot was running** — 7 of 9 and 4 of 4,
against 0 of 8 hits on stationary ranged. The dodge was causing the damage.

What replaced it is a closed-form per-slot step-out (`LightningChargeOffset`) to the nearer cone edge at
`CONE/2 + MARGIN + RING_CONE_CLEARANCE` (37.5 + 15 + 5°), **solved from the latched bearing rather than
from wherever the last cone left the bot**: over 20,000 simulated random cones, chaining offsets drifts
the three slots out of their 90° spacing until two share a point, which is what the rigid turn was
really buying. Result: **1.49 slot-moves per cone against 3.00**, median 4.3 yd and p90 7.3 against
11.31 yd every time, and nobody ever left inside the cone. The accepted cost is a displaced slot sitting
4.5 yd off a neighbour instead of 11.3 — inside the jump — 16.6% of the time.

A rotation always exists: sampling the r=8 ring every 10° across 890 snapshots with a Blizzard up, it is
never fully covered, the clear fraction bottoms at 53%, and the arc to the nearest clear bearing is a
median 3-7 yd, p90 6-10, max 15.1 — **blocked zero times**.

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

Effective `AiPlayerbot.SpellDistance` **28.5** and `HealDistance` **38.5** mean `"reach spell"` fires
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
`ULDUAR_THORIM_CAMP_MAX_BOSS_RANGE` (32) are for.

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

**A taunt forces the target for 3 s and then hands the boss to the highest *real* threat — and Thorim's
threat table is empty at the phase change**, because he sits at 100% untouched through phase 1. So the
pickup taunt hands him straight back: at 2:56.706, exactly 3 s after the tank's Hand of Reckoning,
Unbalancing Strike went to a hunter **44 yd out**. That is why the pickup fires 6-7 times a pull against
13-31 victim changes, why the answer is "keep the tank parked and swinging" rather than re-taunting, and
why re-taunt cadence is still open.

**The class taunts fight the encounter for the boss.** `TankPaladinStrategy.cpp:114-121` wires
`"lose aggro"` to `hand of reckoning` at `ACTION_HIGH + 7`, falling back to `righteous defense`; it
fires every ~2 s at `rel 27.0`, so a bot paladin rips the boss off a human tank — **34 and 45 victim
changes a phase** against 16 Dark Commands. `ThorimTauntGuardMultiplier` silences them, modelled on
`IsHodirTauntAction` + `HodirGuardMultiplier` and gated on the bot's **current target** being Thorim,
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
times a second. Assign only on change.

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
raid clears the gauntlet fast enough she joins instead and casts Frostbolt Volley (**62604**, raid-wide
and unavoidable — healed through), Blizzard and Frost Nova (62605, teleport then point-blank). Her
single-target **Frostbolt is 62601**.

**Three of her casts are not worth building against, and here is why.** Frostbolt Volley 62604 is
instant with `InterruptFlags 0`. Chain Lightning **64390** carries `InterruptFlags 1`, so only Thorim's
own movement breaks it and **it cannot be kicked**. Frostbolt 62601 *is* kickable (`InterruptFlags 15`)
but has never been a counted death, so no kick node was built.

**Blizzard is a trail, not a circle.** Every 36-41s a `NPC_SIF_BLIZZARD` 32879 spawns at
(2108.7, -280.04) and walks a fixed eight-waypoint loop for 30s. Its aura (62577/62603) drops a 10s, 8 yd
zone (62576 10-man, 62602 25-man) every 2s: up to six live, a median 26 yd behind it. **Test the zones
(`GetDynamicObjectPositions`), not the bunny** — the bunny alone missed 16 of 20 melee hits in one pull.
Every camp home spot clears the loop by 12.9 yd or more.

**The two Blizzard radii mean different things.** `ULDUAR_THORIM_SIF_BLIZZARD_RADIUS = 15.0` is the
generic dodge's **trigger** radius, not a damage radius. The damage-side constant is
`ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE = 11.0` — the measured 9.8 yd reach (corrected DBC 8 plus both
combat reaches through `IsWithinDistInMap`) plus a yard. `ULDUAR_THORIM_SIF_FROST_NOVA_RADIUS` stays at
**12**, matching the DBC damage effect; its victims are mostly a consequence of Sif teleporting on top
of people rather than of short clearance.

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

**Hard mode changes what a trace means.** The 6:31 "kill" used as a yardstick **was not hard mode** —
Sif vanished at 3:25 and dealt zero, against 520k-1.04M in hard-mode pulls. Any metric compared across
the two is comparing hard mode to normal.

## The arena adds, and what to kill first

`GetThorimDpsTarget` buckets the encounter's creatures by entry in one sweep — acolytes, evokers,
champions, warbringers, commoners, and the corridor's Iron Ring and Iron Honor Guard — and hands each
bot a pick. It is deliberately **not** a raid icon: an icon is a sticky override that `RtiTargetValue`
hands back before the smart picker runs, so a wrong mark cannot be corrected until the bot leaves
combat (see [../../engine/pitfalls.md](../../engine/pitfalls.md)).

Measured share of arena damage taken: Dark Rune Warbringer 21.2% (melee 14.0%, Runic Strike **62322**
7.2%), Dark Rune Evoker 16.5% (Runic Lightning **62445**). Champions are melee, top of the list and
already standing on the bots — the generic picker used to send melee straight past them at an Evoker.

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
`ThorimDisableAutomaticTargetingMultiplier` returns 1.0 rather than strand the bot, and the only hostile
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

## Aspect of the Wild has exactly one holder

Thorim's nature damage earns the aura, but 49071 is `APPLY_AREA_AURA_RAID` + `MOD_RESISTANCE_EXCLUSIVE`,
so a second hunter adds nothing and pays Aspect of the Viper for it. `GetNatureResistanceHunter` picks
one — whoever already holds the aura, else the first hunter at or above the Viper entry threshold — and
`BossNatureAspectHoldMultiplier` pins that hunter's aspect slot. Every other hunter keeps Dragonhawk and
stays free to Viper.

Eligibility reuses the Viper band, so the role cannot flap: a hunter that hands off at 30% is not
eligible again until Viper drops at 60%. When nobody is eligible the first hunter is pinned regardless —
raid-wide resistance outranks one hunter's mana — so **a raid with a single hunter pins that hunter for
the whole fight**. Intended, not a bug.

Kologarn, Freya, and VoA's Emalon and Archavon share the mechanism.

## Known gaps

**The wipe is a damage race, and nothing positional answers it.** Tank intake reaches **190-260k per
20 s by +100 s** (Thorim's melee 110-150k of it) against **180-210k healed**; the Lightning Charge stack
buff takes his melee from **6.8k to 15.6k a swing by +140 s**, Lightning Charge **13k → 28k**, and Chain
Lightning peaks at **31.5k**. The tanks fall at **+155-170 s** while the kill needs about **225 s** at
the observed pace. This is the encounter's actual remaining problem.

**Chain Lightning kills from hop 5.** The multipliers are 1, 1.5, 2.25, 3.4, 5.1, 7.6, 11.4, 17.1 on a
4625-5375 base — hops 6-8 kill outright and hop 5 kills cloth. Melee ring slots are 90° apart (11.3 yd
at r 8) but cone offsets reach 57.5° and Blizzard slides ±180°, so the ring chains as one; steady risk
is **0.35-0.56 lethal hops per cast**. The camp is stacked by construction — 14 bots share 6 slots, so
the tightest occupied pair is **0.00 yd in every pull** — which is pre-existing and accepted.

**Pets as chain links are unmeasurable today.** A median of 6 and p90 of 25 live pets and guardians are
up in phase 2, and they are the most likely reason observed chains reach 7-8 where an idealised
formation caps at 3 — but the recorder writes `dmg` only for raid members, so no trace can confirm it.
Extending `Bot/Obs` to log pet damage is the prerequisite.

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
because it is the difference between missing and making the 170 s window.

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
