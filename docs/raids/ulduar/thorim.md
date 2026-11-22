# Thorim

Two halves that share nothing: a corridor gauntlet past the Runic Colossus and the Ancient Rune Giant,
then a stationary fight on the arena floor once Thorim drops off his balcony. `GetPositionZ() < 429.6`
separates the two everywhere in the strategy.

**The whole fight is on a ~170 s clock.** `EVENT_THORIM_START_PHASE1` fires 20 s after the sixth
start-pack death (26-30 s into a bot-led pull) and sends `ACTION_SIF_START_DOMINION`, which a trace
shows as her Touch of Dominion (62507) cast; 150 s later `EVENT_SIF_FINISH_DOMINION` despawns Sif.
Reach Thorim before it and she joins the fight instead: that is hard mode. Every gauntlet decision
below is sized against the whole budget, not the 150 s timer alone.

## Phase 1 split

The arena half must never empty out. From the start of phase 1, `ThorimAI::GetArenaPlayer()` scans
**every 5 seconds** for one **living** player inside

```
x 2085..2185   y -305..-214   z < 425
```

The first scan that finds nobody is terminal, with no grace period or recovery: `SAY_WIPE` and a
`Lightning Orb` (33138) that kills the raid. A separate 5 minute timer (`EVENT_THORIM_NOT_REACH_IN_TIME`)
fires the same orb regardless, which bounds phase 1.

The split is **latched once per pull** and held until Thorim drops to the floor: recomputed per tick,
a role predicate flipping mid-fight walked the arena squad into the corridor. Quotas are
`dpsQuota = twentyFive ? 9 : 4`: 1 tank + 1 healer + 4 DPS (10 man), 1 tank + 2 healers + 9 DPS
(25 man), **a twelve-bot gauntlet on 25 man**, raised deliberately to make the 170 s window. A roster
that cannot fill them shrinks the gauntlet, and assignment stops once the arena would drop below 3. The
main tank never leaves the arena, and a one-tank raid has no tank to spare, so the gauntlet takes one
more DPS instead. An empty roster (the raid still zoning in) latches nothing. Assignment waits while
`GetMainTankGuid` reads empty: its fallback
only sees tanks already alive in the instance, so a split latched then sends the only tank down the
corridor. A late arrival with no entry counts as arena, the side that cannot wipe the raid by being
short. The split is struck before the pull, when RaidObs has no session, so it is noted again once one
opens (`squadsNoted`).

**The quota is a measurement.** About 3.87 M of gated corridor health stands between the pull and
phase 2, and ten gauntlet bots measured **23,430 dps** (7 DPS-role bots at ~3,075 each, against 4,214
for arena DPS over the same window): 231 s. Two more bots buy roughly 30-34 s, the difference between
missing and making the deadline. The DPS loop picks in raw roster order with no melee/ranged split, so
the two extra may both be ranged and thin the arena ring.

Every gauntlet pick is **bot-only**: nothing here can walk a human, so a human off-tank in the single
gauntlet tank slot leaves the corridor a body short. **Set `MEMBER_FLAG_MAINTANK` in the raid frame**:
without it `GetMainTankGuid` falls back to the first tank in roster order, so a human tank ahead of the
bot main tank silently takes the role, sending the bot main tank down the corridor and leaving the
arena untanked. Human squad labels are written from position on a 500 ms tick, by the test that makes
a human the [gauntlet leader](#known-gaps); **no bot ever moves a human**. The bot gauntlet is
deliberately **not** sized down when a human walks the corridor, because squads latch at pull time
while the human is still in the arena.

Arena adds all land 19-24 yd from the centre and the nearest box edge is 42 yd out, so the leash is
**30 yd from `ULDUAR_THORIM_NEAR_ARENA_CENTER`**, 15.8 yd short of the corridor mouth at the lever gate;
melee get **24 yd**, the furthest an add lands, so it costs no uptime. The radius is the working leash
and the box the backstop, since the box is what the boss script scans. The `thorim arena leash` rule
holds the generic movers while a bot is outside its leash and, alone among the Ulduar guards, does
**not** exempt `AttackAction` or `ReachTargetAction`: corridor mobs sit ~92 yd out, inside the 100 yd
sight cap, and the chase is what walks a bot out of the box. `ThorimArenaTargetGuardMultiplier` drops a
target outside the box, or leash and chase take turns at the gate forever. **`ThorimArenaLeashBreached`
returns early unless `ThorimSplitActive`**, so the 30 yd leash never sees the phase 2 ring; do not
"fix" it for phase 2.

`ThorimArenaLeashAction` walks a bot to its own anchor spot, or every fence trip collapses the
formation; the centre is the fallback, never the nearest box edge, where a rounding error is a wipe. It
also strips the bot's non-combat `follow master` (`thorim.followstripped`), since a master walking off
down the corridor is the usual cause; the reset hands it back, reading that record before it clears
the state, or an arena bot never follows anyone after the kill.

**A fence is not a formation.** Left with only the leash, the squad diffuses outward until parked
against the east edge at x ≈ 2165, the gateway. East is the worst drift:
`boss_thorim_arena_npcs::CanAIAttack` is `GetPositionX() < 2180 && GetPositionZ() < 425`, so a bot
there stops being attackable and the add re-rolls, and `SelectT()` gives a **random** arena-side player
500 threat, so a squad spread over 50 yd puts adds on people no healer reaches. Threat is chaotic by
design; being in one place is the only counter.

So the squad is anchored. `GetThorimArenaAnchor` is the one answer trigger and action both read: the
tank holds `ULDUAR_THORIM_NEAR_ARENA_CENTER`; ranged and healers take a ring slot at 13 or 18 yd
(`_ARENA_RING_INNER` / `_OUTER`, five inner slots: clear of the Champion's Whirlwind on the pile, close
enough to reach anything in it, as wide as the room allows, since adds land at 19-24 and the mesh gives
out past ~26 on the south side); melee get the centre only **out of combat** and in the fight run free
on the 24 yd leash. Slots come from the latched arena squad's ranged bots, **ranged DPS ahead of healers, ties on
guid** (tanks never take one), and the dead stay in: ranking by who still stands renumbers everyone
behind a corpse mid-fight. `ThorimArenaRingOrder` serves the
ring and the Charge Orb dodge alike, so both agree on slots. Slot 0 sits on the bearing from the lever
gate to the centre, so the formation opens away from the corridor.

Ring points are validated with `GetMapWaterOrGroundLevel` and `CheckCollisionAndGetValidCoords`, since
raw ring geometry lands off the navmesh ([pitfalls](../../engine/pitfalls.md#movement-that-silently-no-ops)).
**`ThorimSettleArenaPoint` returning false means "use the centre", and that fallback is
load-bearing**: `CheckCollisionAndGetValidCoords` drags a point back toward the bot, so a bot outside
the pit was handed its own spot, latched it as arrived outside the leash, and the anchor guard and the
leash froze it for good. `ThorimArenaAnchorNeedsMove` never counts a spot outside the leash as arrival
for the same reason. The `thorim arena anchor guard` rule holds the generic movers once a bot is
settled, exempting the chase: an add at 24 yd is up to 38 yd from an outer slot, and a ranged bot that
cannot step into range is silent.

**Nothing is anchored before Thorim is in combat**: the squad stays put, or follows a human master, and
walks in at the pull. Until then the Behemoth, summoned at (2146.6, −266.7), turns Acid Breath
(62315/62415: 50 yd cone, 8-11k plus a DoT) on the Captain every 12 s, across the centre and the ring
slot at (2129.4, −251.4). A bot it hits damages the Behemoth back through a Lightning Shield proc or its
warlock imp, which starts the encounter like any player damage
([start pack](#the-arena-adds-and-what-to-kill-first)). Anchored before the pull, all three later
harness pulls of 2026-11-22 opened that way, 0-12 s after the first breath on the formation; nobody at
the harness drop (2152, −232), 73° off the cone, was ever hit.

**`GetThorim` must not be sight-limited, and must not read `find target`**, which walks only the bot's
own threat list and matches the localized name, so a bot on an add goes blind to the boss.
`GetUldUnitByEntry` walks `"possible targets no los"`,
capped at `Playerbots.SightDistance` 100 (~110 effective with the boss's bounding radius): the balcony
trigger was active at **102.2-106.8 yd** and useless at **110.9-113.1**, and the upper hallway is
100-145 yd from Thorim's platform, so all 20 Thorim entry points went blind once the squad climbed the
ramp. A range-free handle re-opens `ThorimEncounterStateIsStale`, which once recorded **178 spurious
resets during a single Hodir pull**, hence its `NearThorimEncounter` gate. Distance alone cannot tell
the wing from Hodir's room (136-176 yd from the arena centre, against a corridor out to 126): below
`ULDUAR_THORIM_WING_MAX_Z` (425) height separates them (gauntlet z 412, arena 420, Hodir 433), and the
ramp and hallway above it, reaching 177 yd out, are a box (x 2100-2205, y −455..−280, z < 450); Hodir
is at x 1980.

**Every bot resets itself** (`resetDone`, cleared while Thorim is engaged), not the first one through
([pitfalls](../../engine/pitfalls.md#state-does-not-cross-objects)): carried-over `ringArrived`,
`balconyStep` and melee slots started **ten of thirteen** corridor bots on a hallway step from two
attempts earlier, walking up the middle at x 2141 instead of the east wall at x 2151, which is also
why `ThorimAdvanceBalconyStep` steps down before it steps up. The instance-wide half (lane, split, scan
caches) runs once per cycle, from whichever bot resets first; per bot it would clear the split 25
times. Stale means idle at full health after a pull: full health alone is a trap, since he watches the
gauntlet untouched, so combat state separates the two. Before the first pull (`engagedSeen`) idle is
no reset, or it clears the split the corridor just formed. The reset action returns false on purpose:
it is bookkeeping, and the pull needs every node below it to run that same tick.

## The corridor traps nothing can see

**Paralytic Field 62241 / 63540** is `SPELL_AURA_MOD_STUN`, **15 000 ms**, a **12 yd** persistent
ground area, instant, with no cast bar and no telegraph. It is cast by `Thorim Trap Bunny` entries
**33054** and **33725** (`boss_thorim_trap`), each polling `SelectNearbyTarget(nullptr, 12.0f)` about
once a second and re-arming after ~50 s. Observed field reach is up to **13.6 yd**. The two static
spawns are **(2134.900, −390.776, 437.311)** and **(2134.930, −339.696, 437.311)** — dead centre of
the upper hallway.

**They are `UNIT_FLAG_NOT_SELECTABLE`, so nothing in the world tells a bot they exist.** No target
value returns them and no dodge node can see them, hence a hard-coded waypoint chain rather than a
hazard dodge. The hallway is a ~40 yd chamber with both traps on its
centreline and two necks (south y ≈ −418, north y ≈ −316) forcing x 2126-2144, so the only route is
centre → east wall → centre. The chain's worst leg clears a bunny by **15.8 yd**, but only while
`ThorimBalconyAdvanceAction` issues the walk:

- **It walks at combat priority.** At normal, every step loses to `reach melee` on a boss nothing up
  there can reach, which holds the mover up to 5 s and aims straight up the middle over both bunnies.
- **Waypoint and hold Z is the navmesh's settled height**: the walk moves on exact points and never
  corrects it, and a hold counts as reached in 2D, since a Z mismatch reissues the same move every few
  seconds. Passing a waypoint's y counts as reaching it, as the hallway runs one way, and arrival at the
  last one is a latch: on the platform the squad drifts while fighting, and re-anchoring it every tick
  would fight `reach melee`.
- **The drop to the arena still walks the chain once Thorim is down.** The jump edge sits on the
  centreline with both bunnies, and taken straight, ten bots stood 5-12 yd off a bunny, stunned up to 15 s.
- **The jump fires on distance to `ULDUAR_THORIM_JUMP_START_POINT`, never on the step latch alone**: a
  step carried over from an earlier pull fired it from the top of the ramp, a 180 yd arc. The 6 yd
  `_BALCONY_ARRIVE_TOLERANCE` covers the drop point too; an exact-waypoint `MoveTo` does not reliably hit
  0.5 yd, and a bot asked for that circled the edge for the rest of the fight.
- **Corridor ranged take their balcony hold only past `ULDUAR_THORIM_BALCONY_5`**: from any earlier
  waypoint the straight line runs 7-9 yd from a bunny. Hold 2 sits inside the jump-edge tolerance, so
  the hold is tested ahead of the jump.
- **`thorim balcony guard` holds the generic movers** for a corridor bot above the floor line while the
  walk has somewhere to go: `follow` would take the only walkable way down to the master, 300 yd back
  through the corridor. `AvoidAoe` stays, since Rune Detonation lands up there. In phase 2
  `ThorimBalconyGuardMultiplier` also zeroes `ReachTargetAction` up there: a reach walks the bot back
  through the hallway, and pulls a ranged bot off its wait spot. The walk has no master check and sits
  at `ACTION_RAID + 2`, over the corridor walk, which has no waypoint to offer up there.
- **A pet chasing down the corridor pulls packs the gauntlet squad has not reached**, which is what
  `ThorimStrayPets` / `ThorimRecallPet` stop.

**Phase 2 only starts when a player above z 430 damages Thorim** (`boss_thorim::DamageTaken`; a pet's
damage does not count), once the Rune Giant's death has sent `ACTION_ALLOW_HIT`. `DisableThorim(true)`
applies only `UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_PACIFIED` and a root, leaving him hostile and
attackable throughout, so a raid that never hits him from up there leaves him at 100% forever.

**The corridor squad starts it.** `ThorimBalconyHitAllowed` gives him to a corridor bot still above the
floor line that both stands and has latched at or past `ULDUAR_THORIM_BALCONY_5` (until the walk runs
again, a latch left from an earlier pull reads as past it from the Giant's ramp). From any earlier
waypoint a chase to him crosses a Paralytic Field, and from the corridor it pulls a bot 150 yd out of
its lane. The jump edge is 8.9 yd from him, inside his 9.08 yd melee reach, so the walk's end is the
firing spot. Every other bot stays off him and Sif while he is up there (`ThorimDpsTargetAllowed`,
`ThorimBalconyGuardMultiplier`, the arena target guard). A real player changes nothing: the first
corridor bot there starts phase 2 if he has not. Measured once (25-man): the first corridor bot to
latch `BALCONY_5` picked him 0.3 s later, and phase 2 started 3.4 s after the pick.

The healer split (arena 13 bodies / 1.48 M / 2 healers against gauntlet 11 / 0.69 M / 2) is left
uneven on purpose: the gauntlet's clock is the lever, not its survivability.

## Offensive cooldowns in the gauntlet

Two gates hold a burst cooldown off trash. `HoldBurstUntilTankEngagedMultiplier` (strategy `"burst"`,
`Playerbots.BurstOnBossOnly`) blocks a grouped bot whose current target is not a dungeon or world
boss. `UlduarBurstWindowMultiplier`, registered in `UldStrategy.cpp`, opens burst and lust only once
Thorim is on the arena floor, `z <= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD`.

**The Ulduar window was blind up the corridor, and its fall-through is open.** It finds the gated boss
by walking `possible targets no los`, which stops at `Playerbots.SightDistance` (100 yd), while the
gauntlet runs ~190 yd from the balcony, so the corridor squad matched no gated boss and took
`BurstWindow{}`, whose `allowAll` and `allowLust` are both **true**. It now resolves him through
`GetThorim` (a range-free per-instance guid cache over `ObjectAccessor`) behind `NearThorimEncounter`,
as the Razorscale and Mimiron-mech fallbacks in that function do; the gate keeps the read inside the
wing, so a raid on Hodir cannot hold its burst off Thorim's balcony z.

While it was blind, only the generic hold stopped them: over the two 28 Oct pulls **98 and 90** burst
pushes came from bots **101-162 yd** out, all in phase 1, and all but 1 and 3 were caught by `hold
burst until tank engaged`. Rapid Fire 22/18, Readiness 21/17, use trinket 11/12, use tinker 11/12,
Bestial Wrath 11/8, Elemental Mastery 9/9, single figures of Army of the Dead, Blade Flurry,
Combustion, Mirror Image, Icy Veins, Arcane Power, Presence of Mind, Fire Elemental Totem and
Berserk, plus **Bloodlust/Heroism 1 and 3**, a 10 minute raid cooldown. Mostly corridor ranged:
Beastcaller 37/26, Deadeye 23/21, Lavaburst 14/14. The arena half was never affected, being inside
100 yd of the balcony, which is the 52 and 53 `ulduar burst window` vetoes.

**`BurstWindowStrategy` and `BurstOnBossOnly` are Custom-only.** The upstream-adapted Ulduar branch
adds `"boost"` but not `"burst"`, so there the Ulduar window is the only gate and this blind spot was
the whole defect. Check it first against any burst report from such a build.

Two exemptions fire in the corridor by design: priest Shadowfiend, a mana return, and `use trinket`
for non-dps, since survival and mana trinkets have to stay available. The two healer trinkets at
~150 yd are those.

**Nothing requires a bot to be near him when the window opens**, so one running back in phase 2 could
burst early. Self-limiting: `BalancePercentValue` returns 100 with nothing attacking, so a bot running
an empty corridor never pushes a boost, and every phase 2 burst in both pulls went out inside 41 yd of
the arena centre.

## Runic Colossus (32872), spawned at (2227.5, -396.179, 412.176)

| Spell | Id | Detail |
|---|---|---|
| Runic Smash, left hand | 62057 | **5s cast every 8s**, fires the 16 static "Right Hand" bunnies (33140) at x 2235-2257, the left lane |
| Runic Smash, right hand | 62058 | Same, the "Left Hand" bunnies (33141) at x 2198-2222, the right lane |
| Runic Smash damage | 62465 | 10 yd per bunny, **the whole row at once as the cast lands** (core `1702c28b7`; same millisecond in the 2026-10-11 log) |
| Runic Barrier | 62338 | -51% damage taken **and a 2000 arcane damage shield per melee swing**; 20s, first cast 6-15s (10-man) / 10-24s (25-man) after the engage, recast every 45s |

`EVENT_RC_RUNIC_SMASH` is scheduled in `Reset()` and **cancelled in `JustEngagedWith`**, so the corridor
smash only happens on the approach and stops the moment the Colossus is tanked. The Ancient Rune Giant
has no damage shield: its Runic Fortification (62942) is a friendly buff on its adds.

The two corridor lanes (left x ~2237-2242, right x ~2212-2219) are **index-matched by y**, so a dodge is
the same waypoint number in the other lane. Each lane sits 2-9 yd from its own hand's bunnies and
15.5-22.9 yd from the other's. The bot side latches the hand it sees casting and holds the opposite lane
until the other hand goes up; the gauntlet formation follows that preference rather than the master's
own lane, or it would walk everyone back into the blast. **Switching the moment the next cast shows is
safe, do not delay it**: the last hit landed 3s earlier, and the 5s cast is all the time there is to
cross the centre line, which both rows reach. The dodge stays armed `ULDUAR_THORIM_RUNIC_SMASH_LATCH_MS`
(10 s) from the last scan that saw a cast, outlasting the hit and the 3s gap before the next cast, so
bots keep off the centre line the whole approach. The Colossus is 131 yd from the top pair of
waypoints, past the 100 yd `Playerbots.SightDistance` cap, so the telegraph is read through a targeted
150 yd creature lookup rather than the usual target values.

Only the corridor squad answers it: `ThorimResolveGauntletIndex` falls back to the leader's waypoint,
so an arena bot would walk 110 yd down the corridor and never come back. The second waypoint is walked
at combat priority without the shortened delay in both lanes, as it's where the squad rounds into the
Colossus's line of sight.

**Runic Barrier goes up once per pull.** The Colossus died 27-31s after the engage in all three
2026-10-11 pulls, so the 45s recast never comes and a barrier usually lasts until he dies; "stop
attacking while it is up" would idle melee for the rest of the fight. The shield cost 273-347k per
pull, 58-89k on the worst-hit melee, which healers carried. Non-tank melee instead back out to 14 yd
below 55% health and return above 80%, keeping their target the whole time so ranged and instant
abilities keep landing.

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

**Thorim's recorded orientation is not usable**: he has turned back to his victim by the next
snapshot, so **the lit orb is the only readable bearing**. Exposure is decided by bearing from Thorim,
not by side of the room: decoding all 33 hits against the boss→orb line gives a max of **43.8°** and a
mean of **17.3°**, with victims 17.3-56.5 yd from the orb. Score a parking spot by **how many of the
seven cones cover it**; "no cone reaches it" scores everything zero.

**The orb cadence is 4.55-5.29 s lit, 9.70-10.56 s dark**, with no flicker; the shelter latch and the
walk-home budget are timed off it.

Positioning has to clear 5 yd between stacks. The main tank drags Thorim to
`ULDUAR_THORIM_PHASE2_TANK_SPOT` **(2110.7483, −252.65265, 419.440)**. Ranged and healers take one of
`ULDUAR_THORIM_RANGED_SLOTS` = **6** spots (the most the room fits 11 yd apart, 22 off the tank spot
and 12 off Sif's loop), **latched per bot** (`EnsureRangedSlot`, `thorim.rangedslot`), because the
shelter table is keyed by slot and a live re-derivation moves the key. Melee take a **dynamic ring of
radius 8.8 around Thorim's live position**, three slots at the ring anchor bearing +90 / +180 / +270
degrees, which leaves the whole tank side clear and puts the stacks 12.4 yd apart. The off-tank sits at
+20 degrees, inside taunt range for the Unbalancing Strike swap. Slots are sticky per guid, handed out
least-loaded so the melee spread evenly, and kept while a bot is dead, or a rezzed bot's slot comes
back a quarter turn off. **`RingAnchorBearing` reads only the tank spot**: the live tank is deliberately
not an input, as the rule rather than a fallback, so the ring cannot rotate as the boss shuffles.
Arrival uses a 3 yd / 5 yd deadband, because a tight one against a ring recomputed from a moving boss
leaves the bot sliding in place, and a moving bot casts nothing. The trigger sets the arrival latch on
a "stay" answer and the action clears it right before it moves, since the action only runs when there
is a move; set in a trigger it only widens the tolerance, so a second ask that tick still says stay.

**The deadband breaks for a hazard.** At ring radius a whole cone dodge is a chord of about 5 yd, so a
settled bot in the cone never moved: **nine cone hits over two pulls** had a safe spot under 5 yd away.
`RingSpotBeatsTheDeadband` moves a bot standing in the cone or a Blizzard zone when the new spot is
clear, and `ThorimRingWantsMove` ORs that escape outside the tolerance test, reading the arrival latch
and never writing it ([why](../../engine/raid-mechanics-lessons.md#movement-that-oscillates)):
**40-60%** of move orders come back refused, and a nested escape re-latched inside 3 yd and was never
retried.

**It breaks for a Chain Lightning bridge too.** The solver keeps slot points 8.8-12.4 yd apart, but the
deadband lets a body rest up to ~6.5 yd off its point, so two slots' bodies stood inside the jump in
**60-78%** of hard-mode phase 2 snapshots while their points did under 10% of the time, and two casts
crossed slots to kill five melee. So `RingBodyBridgesChain` breaks it for a ring bot more than
`ULDUAR_THORIM_RING_CHAIN_MIN_STEP` (1 yd) off its point and within the jump
(`ULDUAR_THORIM_CHAIN_LIGHTNING_JUMP` 5 + `ULDUAR_THORIM_RING_CHAIN_MARGIN` 0.5, the core's
`IsWithinDist`) of a living floor body outside its slot, when its point clears every such body; if not,
moving would still bridge. **Another slot's ring bot counts at its slot's point**, since it walks back
too: counted where it stands, two slots drifting toward each other block each other's points, and a
replay of three pulls left such a bridge unbroken in 14-36% of phase 2 snapshots (9-13% counted at
points). The 1 yd floor stops a bot on its point reissuing the move while a body that won't leave
bridges it.

Every melee DPS carries the `behind` strategy from `AiFactory`, so `SetBehindTargetAction` would walk all
three stacks into one arc behind the boss the moment the ring node yields. The `thorim movement guard` rule
holds the generic movers, scoped to a **settled** ring holder and exempting `AttackAction`,
`ReachTargetAction` and `AvoidAoeAction`: a permanent movement freeze is the Void Reaver failure.
`ThorimMeleeRingSettled` refuses a bot above the floor line: a stale ring latch on the balcony froze two
bots on the hallway for three minutes.

### Only the covered slot moves

**The whole-ring rotation was built, measured and deleted.** `RingRotation` reset when the orb went
dark (`ThorimChargedThunderOrb` returns null), so every charge cycle was two whole-ring turns: **307 of
352 melee destination changes turned more than 60°**, median 90, p90 178, with the boss not moving,
an 11.3 yd run each way. It solved coverage against the live anchor bearing while each bot walked to
its own latched one, and **the Lightning Charge hits on melee landed while they were running**: 7 of
9 and 4 of 4, against 0 of 8 on stationary ranged. The dodge was causing the damage.

What replaced it is a closed-form per-slot step-out to the nearer cone edge at
`CONE/2 + MARGIN + RING_CONE_CLEARANCE` (37.5 + 15 + 5°: the margin covers Thorim turning onto the orb
while the bot walks, the clearance keeps a bot that just stepped out from reading as still inside),
**solved from the latched bearing rather than from wherever the last cone left the bot**: over 20,000
simulated random cones, chaining offsets drifts the three slots out of their 90° spacing until two
share a point, which is what the rigid turn was really buying. Result: **1.49 slot-moves per cone against 3.00**, median 4.3 yd and p90 7.3 against
11.31 yd every time, and nobody ever left inside the cone.

**Solved per bot, that step-out merged the ring anyway.** Each bot latched its own bearing off a live
anchor and then dodged the cone and the Blizzard alone, blind to the other slots: **73-81%** of
`thorim.ringspot` notes carried an offset, spread inside one slot ran 3-33°, and one slot's composed
bearings spanned **166°** and reached its neighbour's median. `SolveRingBearings` answers all three at
once for the instance — everyone in a slot reads the same point, and no candidate is taken within
`ULDUAR_THORIM_RING_SLOT_SEPARATION` (60°, **8.8 yd at r 8.8**) of another slot or of the tank anchor.
Nor can it be wider: a lit cone forbids 105°, and three slots plus the anchor need 240 of the 255 left.
A slot tries the side it answered on last time first, so one new zone cannot flip it across the ring.
The note reads `slot N bearing B solved S`, and the gap between two slots' `solved` is what Chain
Lightning reads; one latched bearing per slot replaced the 3-33° smear.

**Two of the three slots straddle his front arc by construction.** `SlotBearing` places them at the
anchor bearing **+90°, +180° and +270°**, which empties his tank side but leaves the outer two on the
front/back boundary, where parry and block apply. Measured over 884 phase 2 snapshots within 12 yd of
him, melee stood in his front arc **59.9%** of the time: three bots at 89-95%, three at 26-32%.
Narrowing to 60° (+120/+180/+240) would put all three behind him but cut the slot gap from 12.4 yd to
**8.8** against the 8 yd chain jump, and 75° gives 10.7. **90° is kept** — the chain is what kills
melee, and fixing the facing swing below moves the boundary slots without touching the gap.

**60° was exactly the jump at r 8, which is why the ring is 8.8.** Zero margin meant most honoured
floors still bridged, so the radius carries the margin instead: a 60° gap is 8.8 yd, and one Blizzard
forbids **77° of ring rather than 87**. The ceiling is real melee range **9.083 yd** — his `CombatReach`
6.25 (`creature_model_info`, display 28977) plus a player's 1.5 plus 4/3. **`RingPoint`'s range check is
not a melee-range guard**: `GetDistance` subtracts his 6.25, so it only rejects past **15.3 yd**, which
catches the floor hole south of y −288. With no ring point, a melee bot takes a static spot
(`ULDUAR_THORIM_PHASE2_MELEE*_SPOT`, round the tank spot) only within
`ULDUAR_THORIM_MELEE_RING_RADIUS + ULDUAR_THORIM_RING_ARRIVE_TOLERANCE` of him: parked 25 yd off at zero
DPS is worse than unspread. An r 8 ring round the tank spot is walkable all the way, its west arc on rim
polys.

**Zones outrank separation when the arc will not hold everything**, the cone never (~20k in one
instant). A bot holds its bearing while a zone sits on it, so giving zones up cost 6-14 consecutive ticks
at 4.3-4.9k, against 40-90k once for a hop. **The anchor term is still angle-only** — the tank sits at
r 5.5, so 60° off him is **7.1 yd** of ground and a chain still bridges the pair. Unfixed. Nor does it
count the off-tank: his spot sits **7.1 yd** from slot 2's default point (anchor +270°), so a chain
reaching either tank runs on into slot 2, three lethal casts in three hard-mode pulls, two of them tank
deaths. The bridge escape can't clear it (slot 2's point isn't clear); it takes a navprobed move of a
tank spot or the slot.

**A rotation does not always exist.** With a median **4 zones up at once (max 6)**, the 11 yd
clearance leaves under 105° of ring free — too little for three slots plus the anchor — in
**14.5-31.1%** of a wipe's Blizzard snapshots, and the whole ring in **5%** (0% in the kill). Where one
exists the arc to the nearest clear bearing is a median 3-7 yd, p90 6-10, max 15.1.

### The opening

Thorim lands at (2134.68, -263.13), mid-camp, and over the first 13.5 s the melee pile chasing him to
the anchor passes within 8 yd of every camp spot. The first Chain Lightning lands **12.0-12.1 s** after
he drops below the floor line, the first cone at 15.9-16.1 s. That cast killed ranged walking into the
camp beside the pile, and the corridor squad's ranged dropping off the balcony stacked within a yard.

So ranged wait it out: the arena squad two to a spot on four north-east rim spots (12.8-15.6 yd clear
of the pile, in cast and heal range), the corridor squad on six platform spots 12 yd apart. The wait
ends, one way, at 12.5 s once Thorim is within 8 yd of the tank spot (`ULDUAR_THORIM_OPENING_SETTLED_RADIUS`:
he settles 4.1-6.1 yd off it, and is 12-23 yd off while being dragged) or the lit cone covers the wait
spot, and always by 25 s, **which is Sif's clock**: she teleports at +8.3 s, casts Frost Nova at
+10.8 s, the first Blizzard bunny appears at **+27.7 s**, its zones reach x > 2140 at +39.7 s and the
north rim at about +33 s. No trace yet shows whether Lightning Charge reaches players above z 430.
Sif's bunny path runs 2.6-7.7 yd from the four arena wait spots, so they serve the opening only.

Wait spots go least-loaded, ties to the one nearest the bot: the arena squad starts ringed round the
landing point, and an index pick walks some through it. The phase 2 clock (`ThorimPhase2ElapsedMs`)
runs only while he is in combat, since his walk home after a wipe also crosses below the floor line.
The shelter latch does not run during the wait; it picks the lit orb up on its first call after.

Effective `Playerbots.SpellDistance` **28.5** and `HealDistance` **38.5** mean `"reach spell"` fires
past about **36.75 yd** from Thorim, which is why the balcony hold needs a `ReachTargetAction` guard.

### Shelters and Chain Lightning

Chain Lightning is cast within 0.8 s of each orb lighting (both run on 15 s cycles) and lands 0.5 s
later. So the camp shelters only while the orb is lit and walks home the moment it goes dark. Held
until the next orb lit instead, 68 of 71 shelter exits over five pulls were walks home as the cast
landed, and one chain ran from three bots stacked on a shelter through two walking home, killing a
healer at hop 6.

**Lightning Charge is a burst one-shot, not attrition.** 62466 was **8.8%** of phase 2 damage taken
against Frostbolt Volley's 28.3%, yet it killed **6 of the 16** bots that died before the first tank and
took 4 at once twice (110,163 and 115,700 in a cast); **8 of those 16 died from full health in one
hit**. The tell is reliable: `thorim.lightningorb` goes live **4.6-4.9 s** ahead on every cast and the
shelter row answers within 0.1 s, so what fails is the table or the walk, never the warning.

**The camp dodge trades one hazard for a smaller one on purpose.** A Lightning Charge is 10-36k in one
instant against a Blizzard tick's 4-5k, so a camp bot in the lit cone leaves the Blizzard to make its
shelter walk. That is what `ULDUAR_THORIM_CAMP_BLIZZARD_ESCAPE_RADIUS` (15) and
`ULDUAR_THORIM_CAMP_MAX_BOSS_RANGE` (32) are for: the escape is short because the generic flee's 30 yd
runs carried healers along the bunny's path into the cone, and past 32 the shorter caster nukes stop
reaching him. With nothing clear and cone-safe in reach the bot stands and takes the tick rather than
run the trail. The step is `MOVEMENT_FORCED`, or a walk home already under way at combat priority turns
it away with `wait` until it finishes. The positioning row waits out a zone on the spot or on the
straight walk there (`ThorimWalkUnderBlizzard`) rather than walk in and get flung off, except under the
lit cone. The zone test is flat, so it checks the floor line, or a bot still on the balcony dodges
zones below it.

**Spread alone cannot keep the camp out of the cone, hence the shelters.** Packed into one 70° wedge all
six `…_PHASE2_RANGE*_SPOT`s sat in one Lightning Charge, a 13-target burst for 202k and ten dead inside
2 s. Spread over 88° the worst cone still catches four, and the seven cones leave only a 16-28° wedge
clear at any anchor, nowhere near six spots 11 yd apart.

Shelter rows were solved offline and navmesh-checked: 42.5° off the cone, 22+ yd off the tank spot,
within 32 of the boss, 9+ from every other body. `ThorimThunderOrbIndex` refuses a lit orb more than
`ULDUAR_THORIM_THUNDER_ORB_MATCH_RADIUS` (4 yd) from a known spawn: a wrong index walks the bot into the
cone, staying home doesn't. An orb that does not place is left unlatched, so the next tick tries again
instead of holding a bad index for the 15 s. While the orb is lit and the bot still home, the cover test reruns every
tick, so a boss drifting onto the slot mid-warning still sends it; it only ever flips to sheltered.

`ULDUAR_THORIM_SHELTER_SPOTS` has no rows for orb indices 4 and 5 ((2092.95, −263.00) and
(2104.94, −233.44)), whose cones point away from the camp. Its longest run, 25.6 yd, is ~3.7 s against
the 4.9 s worst warning, and a bot commits from wherever it stands, so walking home the moment the orb
goes dark costs an extra run only when two orbs in a row cover one slot. Four of orb 2's five rows sit
7.6-8.2 yd from the Blizzard track, inside the 9.8 yd reach, because the pocket off that cone and 11 yd
clear of the track is ~97 sq yd and four points 9 yd apart do not fit
([solved tables](../../engine/pitfalls.md#a-solved-table-outlives-the-traces-it-was-solved-from));
standing on a shelter was 14% of the camp's Blizzard damage, walking to one 63%. Rows (orb 3, slot 1)
and (orb 6, slot 5) sit under 3 yd from home, 34-37° off the cone, and walk only because the camp
trigger arrives at 1 yd, not the ring's 3.

**A pair with no row is not a pair that is safe.** `ThorimRangedSpot` returns false when
`ThorimShelterFor` misses and the bot stays home, which is only right where home already clears that
cone. Slot 4's home is covered by orbs **0, 1 and 2**; rows existed for 0 and 1, so with orb 2 lighting
every cast of one pull the camp stood in it. The added row is **(2130.00, −224.00, 420.146)**: 94.3° off
the cone centre, 34.5 yd off the tank spot, 29.6-32.0 from the boss, 9+ from every body, and 8.11 yd off
the Blizzard track against the shipped table's worst of 7.15. Its **29.35 yd** walk is the table's
longest, ~4.3 s against the 4.6-4.9 s warning. No replacement home clears all seven cones (the best is
covered by one and sits 42 yd away), so moving the camp spot was rejected too.

`ULDUAR_THORIM_LIGHTNING_CHARGE_RANGED_MARGIN` (2.5°) is deliberately untouched: it already read that
home as covered, at 36° inside the 40° half-width, so the absent row was the whole defect. It is far
tighter than the ring's because a yard of boss drift is 7° at 8 yd but 2-4° at 15-32 yd, and any wider
leaves no shelter set that fits the room.

**Survival cooldowns are spent on health, not on the tell, so the tell needs its own row.** Bots do use
them: **235** casts over one phase 2 across 15 bots, non-tanks included (Barkskin, Dispersion,
Deterrence, Ice Block, Evasion, Anti-Magic Shell, Pain Suppression, Divine Protection, Hand of
Sacrifice). But **7 of the 13** the cone killed used none in the whole pull, and the 9 casts that did
land inside a warning window all came 2-4 s after it opened, reacting to damage already taken. Only
the tank had buttons up near its cone death, Rune Tap and Vampiric Blood 4 s before, and it died
anyway.

`thorim personal defensive` (`ACTION_RAID + 4`, over the dodges it cannot replace) answers the tell for
**non-tanks**, the first Ulduar user of the shared `RaidDefensive` row (`ULDUAR_THORIM_DEFENSIVE_SCOPE`,
personal picks only, so its cover key is never written). It tests the cone on the camp's margin, not
the ring's, as the closest read of who it covers, so a bot well outside spends nothing. It predicts the
cone as **27,000 pre-resistance nature** and lets `GetRaidPersonalDefensive` pick the bot's own button,
which goes up only where the hit would leave it under
`RAID_DEFENSIVE_FLOOR_PCT` (25%). That figure is the measured pre-mitigation median: one cast spans
**13,017-41,492** before mitigation (p50 26,899, 19.8% resisted and 7.4% absorbed) and
**997-36,738** after, p50 19,362 and p90 31,892 over 25 hits, so a mid figure buys a button for the
bots it would drop and not for the rest.

**The predicted start is one scan interval early, deliberately.** `lightningOrbLitMs` is stamped on the
dark-to-lit rescan and cleared when the orb goes dark, so it is never early and can be a whole
`ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS` late; the row opens the pick only `RAID_PERSONAL_LEAD_MS`
(1 s) before the start it is handed. Without the shift the window could open 0.5 s before the damage,
which is too late for the cast to land. The start is the lit time plus
`ULDUAR_THORIM_LIGHTNING_CHARGE_WARNING_MS` (4800) less that interval, and one already past fails closed
through `getMSTimeDiff`'s wrap.

Pushing camp spots outward from the **live** boss was modelled and rejected: it brings camp pairs to
6.4-7.1 yd under orbs 1 and 3, inside the 8 yd jump. The melee ring's cone offset is likewise left
untouched: 25 of 44 ring moves inside the orb→Chain-Lightning window are cone dodges that have to
happen then.

## Somebody has to hold the boss, and a taunt will not do it

**Nothing else puts a tank on Thorim in phase 2.** `ThorimDpsTargetAllowed` returns true for everything
once he is on the floor, so `ThorimDpsPriorityTrigger`'s tank branch (`currentTarget && !allowed`) is
always false there, and the positioning trigger's tank branch needs the bot to already hold him.
Without a pickup, phase 2 ran 9.6-20.2 s untanked with deaths inside each vacuum, Thorim spent 29% of
the phase on a non-tank and dealt 64-73% of his output there, and one normal-mode "kill" was 49%
untanked. `ThorimTankPickupTrigger` / `ThorimTankPickupAction` close it: the main tank picks up
whenever free and the first assist tank only when the main tank is dead or absent, so **only one bot
ever taunts**.

**A tank's phase 2 role must not follow `boss->GetVictim()`, because the role picks the destination.**
With `ResolveThorimPhase2Role` keyed on the victim, every taunt trade (9 a pull) moved the bot tank
between a fixed spot and a boss-relative one. Which tank counts as "main" is decided outside this encounter anyway:
`GetMainTankGuid` falls back to the **first alive tank in group order** when nobody wears the raid flag,
and `IsTank` takes a human by **spec** when there is no `PlayerbotAI`, so a human protection paladin
wins it on group order alone, and the bot's `maintank` flipped 15 times in one pull once the flag sat
on the human.

**`thorim.anchortank` latches one bot tank for the phase instead.** Elected bots-only (a human cannot
be steered onto a spot), preferring whoever already holds him, then lowest guid; held while it is still
alive, still a tank and still on the floor, so a trade moves nobody. It is cleared whenever phase 2 is
not active, not on reset, so it cannot ride into the next pull. Never the ring for either tank: a ring
point is measured off the boss, so a tank on one orbits him at 8 yd instead of walking him, which once
left Thorim **26 yd** off the anchor for a whole pull. An off-tank does not run the ring's
arrive/reposition path in the positioning trigger and action either. The main tank backs into his
spot, so he keeps facing the boss he drags. `ThorimTankPicked` takes
`tank assist` away from tanks in phase 2 only: left voting, it held a tank on a Warbringer for the first
30 s while Thorim ate the camp, and in phase 1 holding whatever swings at the raid beats steering a tank
onto the ranged pick.

**A tank's spot, not just its role, decides which way he faces.** The anchor tank holds him squarely on
the design axis (**0°** off the boss→tank-spot bearing over 406 snapshots), while an off-tank holding
him from the off-tank spot turns him **39°** off it over 399, half the phase each. The ring is solved
against that axis, so the swing is what drags the outer slots into his front arc. **Whichever tank
holds him now takes the anchor**, every other tank the off-tank spot, and both are fixed points: at
3.5 yd apart a trade steps him inside `ULDUAR_THORIM_RING_REPOSITION_TOLERANCE` (5), so it no longer
unlatches the ring the way the old 7.07 yd off-tank spot did.

**Thorim's threat table is empty at the phase change**, because he sits at 100% untouched through
phase 1, so the pickup taunt hands him straight back
([taunts](../../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)): exactly 3 s after
a tank's Hand of Reckoning, Unbalancing Strike went to a hunter **44 yd out**. That is why the pickup fires 6-7 times a pull against 13-31 victim
changes, why the answer is "keep the tank parked and swinging" rather than re-taunting, and why
re-taunt cadence is still open.

**The class taunts fight the encounter for the boss.** `TankPaladinStrategy::InitTriggers` wires
`"lose aggro"` to `hand of reckoning` at `ACTION_HIGH + 7`, falling back to `righteous defense`; it
fires every ~2 s at `rel 27.0`, so a bot paladin rips the boss off a human tank: **34 and 45 victim
changes a phase** against 16 Dark Commands. The `thorim taunt guard` rule silences them, a `Block` on
the `Taunt` family gated on the bot's **current target** being Thorim, so taunting an add off a healer
still works. **`righteous defense` must be in any name-match list**: it is half the taunts in the
trace.

**Both tank spots are absolute**, because an anchor measured off the thing you are dragging has no
fixed point: an off-tank spot off the boss's live position walked the pair **256 and 92 yd** in phase
2 against **39 yd** in a kill. The off-tank's is **(2114.25, -252.65, 419.508), 3.5 yd** off the tank
spot, inside the ring's 5.0 yd reposition tolerance, so a taunt trade does not unlatch the ring;
navprobe settles that Z and reads **16/16 on mesh** on a 3.5 yd sweep. The old 7.07 yd offset and 3.5
both sit inside the 8 yd jump, so **the tank pair bridges either way**, still unfixed. Two related
traps: **a victim change re-seats him at the chase near point and he never moves again**
(`ChaseMovementGenerator::DoUpdate`; in one pull he rested 7.4 yd off the tank spot against 4.1-5.9 in
the others, putting the outermost ranged slot 6.9 yd outside the melee ring), and whoever holds him
eats the Blizzard, which is why the dodge was towing him across the arena for **36,283 and 32,799** of
avoided damage against 11-38k from a single Thorim swing.

**The anchor is also the Chain Lightning answer.** Boss 18-28 yd off the anchor: 8 targets, **174,932**
damage running 4,695 → 47,024, out of the melee ring and into the ranged camp, **four bots dead in
0.1 s**. Boss on the anchor: 8 targets, **141,198**, all melee and tanks, **zero deaths**. What decides
survival is where the chain *ends*.

**The generic movers fight the positioning row over a tank.** `ThorimMeleeRingSettled` is false for
every role but `MeleeRing`, so the `thorim movement guard` never gates a tank, and `AvoidAoe` sits in
its pass mask regardless: `avoid aoe` aimed **70-74** orders a pull at the bot tank and `tank face`
**15-30**, alternating with the positioning row **153-176** times over a median **7.6-7.9 yd** gap
between their destinations. `thorim tank anchor guard` closes it, holding a tank that is within 3 yd of
its own spot and keeping `AvoidAoe` **out** of the pass mask. That only enforces what
`ThorimSifBlizzardTrigger` already decides, since it refuses both tank roles in phase 2 on the grounds
that a tank's 30 yd flee tows the boss into the ranged camp. One oscillating tank took its
destinations from 1 distinct point to 10 and towed the ring into itself (minimum pairwise slot gap
p10 60° → 10°), and Chain Lightning's worst cast from 6 victims for 119,057 to 8 for **190,060**,
whose 69,788 top hit killed two melee.

## The Charge Orb field is a 32.3 yd circle, not a 35 yd sphere

`EVENT_THORIM_CHARGE_ORB` fires 14 s into phase 1 and repeats every 16 s, casting **Charge Orb 62016**
at one of the seven **Thunder Orbs (33378)** (`conditions` row `(13,1,62016,…,33378)` restricts the
target). 62016 is a periodic trigger, period 1000 ms, **duration 15 000 ms**, trigger spell **62017**,
so the orb carries the aura for the whole window. **Lightning Shock 62017** is base 2830 + die 339 →
**~2831-3170 nature per tick** at `EffectRadiusIndex 21` = **35 yd**, not dispellable and with no cast
bar of its own. Unread, it was **50.3% of all damage taken** in one trace.

**The radius test is 3D and the orbs float 13.5 yd above the floor**, so the 35 yd sphere cuts the
floor as a **32.3 yd circle** (`sqrt(35² − 13.5²)`) centred under the orb, the derivation behind
`ULDUAR_THORIM_CHARGED_ORB_RADIUS = 32.3f` and its 4.0 yd margin. Reading the DBC radius as a flat
distance puts the boundary 2.7 yd too far out. The trace shows the knife edge: a bot 34.9 yd from the
orb took every tick, while one 3 yd further east at 37.6 yd took none.

In the arena the anchor itself is pushed out of the field, not just the bot: the field burns 15 s, and
an anchor left inside walks the bot back in once the dodge lets go. The push runs straight out from the
orb, the shortest way out, keeping each bot's bearing so the ring stays spread; every orb is 42 yd out,
so the centre clears the field by 5.7 yd and is the fallback for a spot the leash rejects or the
collision walk drags back in. The escape has no arrival latch, since the point stops being offered once
the bot is out, and the arena anchor stands down while the orb trigger fires, or it walks a dodging bot
back in. The node sits at `ACTION_RAID + 4`, over the arena ring and the add chase. Lightning Charge has
no dodge node: the phase 2 spot answers the cone, and a second mover would only fight it.

The seven Thunder Orb spawns are fixed (`creature` rows), all at **z 433.3** and all **42.0 yd** from
the arena centre: (2105.04, −292.56), (2092.95, −263.00), (2104.94, −233.44), (2124.30, −222.60),
(2145.50, −222.62), (2164.20, −233.47), (2164.55, −293.00).

**Two orb markers need two cache slots.** `thorim.chargedorb` once toggled between 0 and a guid **78
times in 15 s**, because the helper cleared the guid before every rescan *and* phase 1's Charge Orb
and phase 2's Lightning Orb shared one slot and evicted each other, forcing a 150 yd grid sweep several
times a second. Assign only on change: the melee ring re-solves on any change of the lit orb's guid
(`ringOrb`), so the flapping cache also flipped six melee onto a 90° bearing several times a second.
Raid-wide sweeps (lane, orbs, Blizzard) run once per instance per
`ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS` (500 ms), so no two bots disagree on the hot lane or the lit
orb.

## One Stormhammer debuffs the whole arena

**Stormhammer 62042** fires every 16 s at one random enemy within 100 yd (2451-2551 damage plus a 2 s
`MECHANIC_STUN`). The `spell_linked_spell` row `(62042, 62470, 1, 'Thorim - Stormhammer')`, type
`SPELL_LINK_HIT`, makes the **hit unit** cast the linked spell on itself with Thorim as original
caster, so the blast is centred on the player the hammer landed on, not on the boss. **Deafening
Thunder 62470** is 4625-5376 nature at `EffectRadiusIndex 14` = **8 yd**, plus
`SPELL_AURA_HASTE_SPELLS` at **−75** at `EffectRadiusIndex 18` = **15 yd**, duration **8 000 ms**.
Four times the cast time, and not dispellable.

It cannot be dodged reactively: the target set is chosen inside `Spell::SelectSpellTargets` and is not
readable from a bot, and the aura is applied before anything can see it. **The only lever is how many
bots one 15 yd blast covers.** Unanchored, every sample from 0:49 on put 11-14 of the 14 arena bots
inside a single 15 yd circle; the same clustering feeds Dark Rune Champion Whirlwind (15578, 8 yd).

## Hard mode

Sif is summoned every pull and normally channels, then despawns after the 150s dominion timer. If the
raid clears the gauntlet fast enough she joins instead and casts Frostbolt Volley, Blizzard and Frost
Nova (teleport then point-blank), plus a single-target Frostbolt.

**Frostbolt Volley is the fight's floor.** `EffectRadiusIndex` 22 is a **200 yd** radius, it is instant,
and `InterruptFlags` is 0 in **both** difficulty rows, so nothing dodges, kicks or outranges it. It is
**21-30% of all incoming damage in every pull**, only resistance and healing touch it, and it is why
this fight has a survival ceiling at all.

**Her ids split by difficulty, and the server does it, not the script.** The script's spell enum casts
62580 / 62583 / 62597 flat; `spelldifficulty_dbc` maps each to **62604 / 62601 / 62605** at index 1,
resolved from the **map's** difficulty
([pitfalls](../../engine/pitfalls.md#detection-that-never-fires)), so **both sets get cast**. Index 1 is
**1.6-2x** the damage: Sif's output went **1,283k to 2,566k, exactly double**, and she is **34-44% of
all incoming** on it. Figures here measured before 2026-10-18 came off index 0.

**Read `thorim.sifjoined`, not the postmortem banner, to tell whether hard mode happened.** Since the
hard mode switches were dropped the banner no longer prints "thorim hard mode ON" and `--hardmode`
disqualifies the pull, while the latch still shows Sif joining.

**What can be kicked depends on which difficulty row is cast.** Volley is `InterruptFlags 0` in both.
Frostbolt is **0 at 62583 but 15 at 62601**, so in a 25-man instance her single-target bolt *is*
kickable and worth **7.7-8.4% of all incoming**; no node tries, and that is open. Frost Nova is `15` in
both rows, but she casts it straight out of a teleport and it is 5-6%. Chain Lightning **64390** carries
`InterruptFlags 1`, so only Thorim's own movement breaks it.

**Blizzard is a trail, not a circle.** Every 36-41s a `NPC_SIF_BLIZZARD` 32879 spawns at
(2108.7, -280.04) and walks a fixed eight-waypoint loop for 30s. Its aura (62577/62603) drops a 10s, 8 yd
zone (62576 10-man, 62602 25-man) every 2s: up to six live, a median 26 yd behind it. **Test the zones
(`GetDynamicObjectPositions`), not the bunny**: the bunny alone missed 16 of 20 melee hits in one pull.
`ThorimBlizzardSpots` is the live zones plus the bunny, which marks where the next zone drops within
2 s; its sweep reaches `ULDUAR_THORIM_BLIZZARD_SCAN_RANGE` (70 yd), since every bot reads whichever
bot's sweep filled the cache and the bunny's path spans 61 by 48 yd. Every camp home spot clears the
loop by 12.9 yd or more.

**The two Blizzard radii mean different things.** `ULDUAR_THORIM_SIF_BLIZZARD_RADIUS = 15.0` is the
generic dodge's **trigger** radius, not a damage radius; at 12 the flee stopped while the bot still stood
in the zone. The damage-side constant is `ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE = 11.0`, the 9.8 yd
reach measured over 161 ticks (corrected DBC 8 plus both combat reaches through `IsWithinDistInMap`)
plus a yard. 15 instead grows the ring's slide from a median 3-7 yd of arc to 9-12 and buys nothing.
`ThorimPhase2PositioningTrigger` stands down while `ThorimSifBlizzardTrigger` is active, as the arena
anchor does: without it the anchor walked bots back into the zone they had left, and the four ranged who
took the most Blizzard were the four who ran furthest.

**That dodge excludes the melee ring outright**, which makes the ring solver melee's only answer to a
Blizzard; with the solver free to give the zone test up, the Blizzard action's verdicts named only
ranged and healers. Ticks are **4,300-4,900 median, 6,187 max**, and melee stood **6-14 consecutive**
of them, one bot 14 ticks for **72,853** over 11 s. That cost melee **320k and 457k** in two wipes, more
than Chain Lightning's whole 188k in one of them. The kill never exceeded **4 ticks**, the cleanest
single difference between it and the wipes.

**Frost Nova is two radii too, and the root is the wider one.** 62597 damages at **12 yd**
(`EffectRadiusIndex` 32) and roots at **15** (index 18). Measured, hits stop at **11.6 yd** and roots
reach **14.9**, last **~6 s**, and **10-62%** land on ranged and healers already clear of the damage.
Rooted, a camp bot cannot walk home, take a shelter or leave a Blizzard zone: the most-rooted pull
refused **31** positioning moves and **11** Blizzard escapes, and was also its worst Blizzard pull.
The damage stays teleport luck: she casts 2.5 s after teleporting, ~8.6 yd of travel under the Volley
slow, so nobody she lands on escapes either radius.

**The 16 yd Frost Nova back-off was deleted; it had never once run.** `thorim sif frost nova trigger`
was asked **25,823x per pull and never returned true**, because it resolved Sif through
`GetFirstAliveUnitByEntry`, and `AttackersValue::IsPossibleTarget` drops `UNIT_FLAG_NOT_SELECTABLE`,
which Sif carries in `creature_template.unit_flags` (33587200) and nothing in the boss script clears.
**Anything that resolves Sif through a targets value cannot see her; use `FindNearestCreature`, as the
Blizzard trigger does.** Repairing it was rejected on its own merits too: ranged sit inside 16 yd of
her **20-34%** of the arena phase, so it would have fired constantly against a spell worth 2-4% of
incoming (5-6% on the index-1 row) whose damage never reaches past 11.6 yd.

**The generic 30 yd flee was producing the damage it dodged.** `MoveAwayFromCreatureAction` scores 8
compass rays out to 30 yd and takes the **furthest**, so every accepted melee flee asked for the full
30 yd, landed 33-53 yd from the boss, still took another Blizzard tick **23-58%** of the time (the
circuit is a ring, so running outward lands on another arc), and blocked every other move for the walk
(31.7 s and 15.0 s stalls). **34 / 56 / 58% of melee Blizzard damage was taken away from the ring.** Of
199 and 246 attempts, 161 and 204 were refused by the movement gate anyway. Melee slide around the
trail instead, and tanks are exempt **by role**, since a holder check alone drops out for the half
second after a human tank's taunt.

**Detector: Sif (33196) alive AND `GetPositionZ() < 429.6`**: she spawns at the throne and only
`NearTeleportTo`s onto the arena floor when she joins.

**Hard mode changes what a trace means, and the boss with it.** Missing the clock puts **62565** on
Thorim: `MOD_DAMAGE_PERCENT_DONE` **-40%**, `MOD_INCREASE_HEALTH_PERCENT` **-30%**. A normal-mode Thorim
is therefore **17.57 M**, not 25.101 M: any figure carried across the two compares different bosses, and
DPS from a normal-mode kill overstates the raid by **~43%**. The 6:31 and 5:34 "kills" once used as
yardsticks were both normal mode: Sif vanished and dealt zero, against 520k-1.04M in hard-mode pulls.

## The arena adds, and what to kill first

`GetThorimDpsTarget` buckets the encounter's creatures by entry in one sweep (acolytes, evokers,
champions, warbringers, commoners, and the corridor's Iron Ring and Iron Honor Guard) and hands each
bot a pick. It is deliberately **not** a raid icon, which cannot be corrected once wrong
([pitfalls](../../engine/pitfalls.md#a-targeting-mark-never-clears-inside-a-pull)).
`ThorimDpsPriorityAction` runs `ThorimClearStaleMarks` before reading anything: each bot's `prioritized
targets` is emptied on every call, since `IsHighPriority` lets one stale entry outrank the whole list,
while the skull, cross and moon are cleared once per pull (`marksCleared`), so a human may re-mark
mid-fight. The sweep reads `possible targets no los`: the pile routinely hides one add behind another,
and a bot that cannot see the Evoker still has to count it.

Measured share of arena damage taken: Dark Rune Champion **41%**, Warbringer 21.2% (melee 14.0%, Runic
Strike **62322** 7.2%), Evoker 16.5% (Runic Lightning **62445**). The generic picker used to send melee
straight past the Champions already standing on them at an Evoker.

After the Acolytes, which everyone takes first since they heal the wave, melee go Champion > Warbringer >
Evoker and ranged Evoker > Champion > Warbringer, then Commoners and the walk-in trash. Melee pick nearest
themselves, first within `ULDUAR_THORIM_MELEE_TARGET_REACH` (15 yd; past it a higher-tier add has moved
twice by the time they arrive), then anywhere, so a bot with nothing near still commits; ranged pick
nearest the arena centre, which puts them all on one unit. Within a tier the current target holds unless
another is `ULDUAR_THORIM_TARGET_SWITCH_MARGIN` (8 yd) closer, or the pick flips between equidistant adds
mid-cast. The corridor squad takes Acolytes, Iron Ring and Iron Honor Guards, walk-in trash, then the
Colossus, then the Rune Giant.

**`thorim disable automatic targeting` holds the generic pickers for every non-tank in phase 1**, or
they alternate with `ThorimDpsPriorityAction` instead of losing to it: the row goes quiet once the bot
holds its pick, the engine falls through to `dps assist` at 50, and that retargets. A traced arena squad
switched target twice a second all fight. Healers keep the generic pickers, since a healer's target
drives its wand and offensive dispels; the row only pulls a healer, or a phase 1 tank steered by `tank
assist`, off a forbidden target.

**`ThorimDpsPriorityTrigger` waits for Thorim's combat.** The `ulduar` strategy runs out of combat too
and the gate is open between pulls, while the start pack (Behemoth, Captain, three Soldiers, Acolyte
32886) fights itself until a player or pet damages one; then all six turn on that player and Thorim
zone-combats the raid. Ungated, DPS bots `Attack()`ed the Acolyte, the first tier, from 2-3 s before
the engage, opening the first three harness pulls of 2026-11-22 before DC's start step. Its spawn is
50.6 yd from the drop at (2152, −232), inside the picker's 50 yd since `GetDistance` nets out both
combat reaches. An in-combat filter on the pick would not do: the mock fight keeps the Behemoth,
Captain and Soldiers in combat with each other.

**Phase 1's elites outlive phase 1.** 7-9 are still standing when Thorim lands (5 Champions 0.61 M,
7 Warbringers 0.53 M, 6 Evokers 0.41 M) and deal **113-214k** over the rest of the fight; the
Commoners die on their own (47 units to 9 by +27 s). One Evoker's Lightning Shock was 26k of the 55k that
killed a healer at **+16 s**, so the phase 2 camp clears **Evokers only** and melee and tanks stay on the
boss: all 1.55 M costs ~15 s of boss time to save ~200k, while a camp bot kills an Evoker without
stepping off its spot.

Dark Rune Commoners stack **Low Blow 62326**, whose second effect is
`SPELL_AURA_MOD_DAMAGE_PERCENT_DONE` at **−3% a stack**; one pull peaked at 25 stacks on a single bot
(−75% damage done). Only late and on a few bots: a reason not to ignore Commoners forever, not an
explanation for a whole fight.

**`ThorimIsTargetSelectionAction` exempts the encounter's own picker**
([why](../../engine/action-selection.md#multiplier-semantics)): listing only the generic pickers,
`ThorimArenaTargetGuardMultiplier` vetoed `ThorimDpsPriorityAction`, the one node that *clears* a
forbidden target, **42 times** in a pull.

## Pets

**Gauntlet pets are leashed to 50 yd of their owner.** `ULDUAR_THORIM_PET_OWNER_LEASH_RADIUS` is
`ULDUAR_THORIM_DPS_TARGET_RANGE`, chosen because that is how far the encounter picker looks for a
target, so a pet beyond it is further out than its owner can see anything worth killing. Legitimate
excursions peaked at 42-54 yd; runaways hit 90-164. Arena pets are held to the box and the 30 yd arena
leash, which still covers an add 24 yd out. A recall repeats at most every
`ULDUAR_THORIM_PET_RECALL_INTERVAL_MS` (2 s) per pet: `MoveFollow` restarts the walk, so a command every
tick leaves the pet running on the spot, and a one-shot latch would miss a pet that strays twice.

The runaways came from the **phase 1 boss latch**. The opening trash dies ~0:25 and the first Dark Rune
wave lands ~0:50; in that gap the encounter picker has nothing, the `thorim disable automatic
targeting` rules let it through rather than strand the bot, and the only hostile in range is Thorim on
his balcony. **18 of 25 bots held him**, and the acquire→clear→re-acquire loop re-pointed their pets,
which took the only walkable route: the 300 yd corridor (Feral Spirit 164 yd from its owner, Worm 128).
So the opening trash is whitelisted as the **last** DPS tier and the suppression is held open on
`ThorimSplitActive`.

`ThorimRecallPet`'s `SetIsFollowing` / `SetIsReturning` pairing is inverted relative to Naxx's
`RecallControlledPetsToBot` and is **deliberately left as written**: the arena pets stayed in the box
100% of the pull. `PetAI::CanAttack` lets a `COMMAND_FOLLOW` pet fight again once `IsReturning` clears,
so the recall holds it only for the walk home; with no `CharmInfo` the stop and the follow are the whole
recall. `ThorimPetLeashAction` is no `MovementAction`, since it moves the pet: the movement guards and
leash exemption lists have no business with it.

**Pets are fine here, do not re-audit.** 87-93% in range of their own target, 2-6% orphan targets, no
deaths, no geometry clipping ("through textures" does not reproduce: the largest steps are 8-13 yd of
snapshot jitter and one Shadowfiend Shadowcrawl). Pet Growl is a non-issue: 102 casts, hostiles aimed at
a pet on 52 of 55,171 rows, and across three later pulls the boss's victim **never once** became a pet.
Pets deliberately dodge nothing: they survive on their own resistances, and a pet AI that steps out of
hazards is a cost with no payoff.

**`thorim pet leash` reads `LOST … ran 0x` in coverage by design.** `won` is only stamped when `Execute`
returns true, and this action returns false so the bot still gets its own node that tick. Its real
evidence is the verdict stream, where it shows `FAILED` 25-31 times a pull.

**Spell names come from `trace.spells`, and `trace.of("spell")` is always empty**: the loader folds
those records into that dict as it reads. A filter built on the record stream matches nothing and reads
as "the raid never cast it", which is how a whole phase of survival cooldowns was once reported as
zero.

**Count burst cooldowns off the `act` and `veto` streams, never off aura names.** Those streams carry
the action names `IsBurstCooldownAction` matches, while Berserk and Killing Machine are passive procs
sharing a DBC name with a burst action, so an aura-name filter reports hundreds of casts that never
happened.

Notes that carry what a snapshot cannot: `thorim.smashside` (the hot lane; the blast wave has no world
object), `thorim.orbescape` (where a bot was sent out of the Charge Orb field), `thorim.shelter` (whether
a cone moved a camp bot, and off which orb; a camp bot also walks with nothing lit), `thorim.dpstarget`
(the add a bot was told to burn, where `GetVictim()` is only where it ended up) and `thorim.petrecall`
(the last pet each owner sent home). The gauntlet opens its own trace on its first in-combat tick
(`gauntletTraced`), ahead of the first lane note, since its mobs are trash to the instance script and
nothing else opens one.

## Aspect of the Wild has exactly one holder

Thorim's nature damage earns the aura, but 49071 is `APPLY_AREA_AURA_RAID` + `MOD_RESISTANCE_EXCLUSIVE`,
so a second hunter adds nothing and pays Aspect of the Viper for it. `GetNatureResistanceHunter` picks
one (whoever already holds the aura, else the first hunter at or above the Viper entry threshold), and
`BossNatureAspectHoldMultiplier` pins that hunter's aspect slot. Every other hunter keeps Dragonhawk and
stays free to Viper.

**A drained holder still covers the raid**, so when the only hunter wearing the aura is below the Viper
threshold and another hunter is eligible, the helper returns `nullptr`: nobody drafted, nobody pinned.
Holding costs nothing, and the outgoing holder drops the aura itself on its next Viper cast because
aspects are exclusive. Drafting a replacement before that put two holders up at once, the second one's
Dragonhawk vetoed for nothing. With **nobody eligible**, the drained holder stays pinned: freed, it
went to Viper, the last fallback drafted it back, and the role bounced Wild ↔ Viper every 1-2 s with
nobody refilling. Locality is only "same map", so a stale holder outside the aura radius still
suppresses the draft.

Eligibility reuses the Viper band, so the role moves on mana swings rather than per tick: a hunter that
hands off at 30% is not eligible again until Viper carries it back to 60%. Two healthy hunters trade the
role every minute or so, each taking its turn on Viper. When nobody is eligible and nobody holds the aura
the first hunter is pinned regardless (raid-wide resistance outranks one hunter's mana), so **a raid
with a single hunter pins that hunter for the whole fight**. Intended, not a bug.

**The veto list has to name the whole aspect chain**: `aspect of the dragonhawk` **and** its
`aspect of the hawk` alternative both need vetoing on the holder. Kologarn, Freya, and VoA's Emalon and
Archavon share the mechanism.

## The frost aura needs a bot and a hold

Frost is about **half of all incoming damage** here (one wipe took **~1.57 M** of it), and Frost
Resistance Aura's 130 resistance is **20.4%** off every frost hit: the level-83 constant is
`150+(83-60)·(83-67.5)` = **506.5**, so `130/(130+506.5)`. A wipe with **no frost resistance at all**
paid ~320k for it.

Two things blocked it, both handled Thorim-side. `BossFrostResistanceTrigger` returns
`member == bot` for the **first alive paladin in group order**, so when that is a human nothing ever acts
on it and no bot paladin can qualify. And pinning the hunter's aspect but not the paladin's aura let
the exclusive slot be traded back on the next GCD. `ThorimTickFrostAuraPaladin` latches
`frostAuraPaladin` from **bot paladins only** (lowest guid, must know the spell), the
`thorim frost aura` node casts it, and the `thorim frost aura hold` Block pins the slot against the
competing auras. The shared node is gone: it left `+rfrost` on the first paladin, a tank one too, for
good. Nobody is elected until `sifJoined` latches on Sif's first Blizzard: in normal mode she never
leaves her balcony.

Nature needs nothing more: Aspect of the Wild was up in all three pulls measured.

## Known gaps

**The wipe is a damage race, and nothing positional answers it.** Tank intake reaches **190-260k per
20 s by +100 s** (Thorim's melee 110-150k of it) against **180-210k healed**; the Lightning Charge stack
buff takes his melee from **6.8k to 15.6k a swing by +140 s**, Lightning Charge **13k → 28k**, and Chain
Lightning peaks at **31.5k**. **The survival ceiling is ~190-200 s**: past it the tanks fall with
healers at 13-17% mean mana and the rest melts in 20-30 s. Boss DPS over the arena's first 150 s runs
**112.8-134.8k**, so 25.101 M needs **186-197 s**; a kill did it in **196 s of arena**. Damage is no
longer what binds: the latest wipes died at 34-37% to the ceiling, earlier ones at 3.1-3.6%. Phase 2
intake is **3.0-5.0 M** against normal mode's **0.64 M**. **Sif is about half of that and most of it is
undodgeable**; the Volley also carries a **-51% slow**, which doubles every reposition. Mana is the
wall, and not for want of cooldowns: healers run **29-30% overheal** with Innervate, Mana Tide, Hymn of
Hope, Shadowfiend, gems and potions all already spent. This is the encounter's actual remaining problem.

On the ilvl 226 test rosters (four healers, bot-led gauntlet) hard-mode phase 2 intake was **3.8-6.2 M**,
Sif **52-55%** of it (Volley alone 1.3-1.9 M) and the tanks taking **160-289k** of Blizzard on the tank
spot. Healers entered at 43-100% mana; in two pulls they ran dry by +100-120 s and the tanks fell there,
and the third reached the ceiling at +190 s with Thorim at 15%. The traces read his `mhp` as **27.19 M**,
not 25.101 M.

**A human tank holding him in phase 2 is called**
([calls to human tanks](../README.md#calls-to-human-tanks)): once per hold turn from 6 Lightning Charge
stacks (62279 on Thorim, ~95 s in, swings about doubled), and under 50% at any stack count, never while
he carries Unbalancing Strike (a bot takes him next). A bot's hold counts as a turn too, so a human
taking him back after a bot starts a new one. Bot tanks have no timed rule here, only their class
triggers, by the user's choice.

**Chain Lightning is a hop-count problem.** 64390 carries `EffectChainAmplitude` **1.5**, so the
multipliers are 1, 1.5, 2.25, 3.4, 5.1, 7.6, 11.4, 17.1 on a 4625-5375 base: hops 1-4 are noise, hop 5
kills cloth, hops 6-8 kill outright. One measured cast ran 5,574 → 7,831 → 12,683 → 21,153 →
17,923 → 43,189 → 69,855 → **89,760**, and two of those took three and four melee bots. Every jump
seen was ≤ 7.9 yd against the 8.0 yd reach; the seed is `SelectTarget(Random)` off the threat list every
**15 s**. With the solver in, casts still run **6-8 hops on players**, peaking **46,815-65,530**.
**The tanks bridge**: at 3.4-7.6 yd off the boss they are inside the jump of more than one slot, which
is how a chain crossed the ring, and why the separation floor counts the anchor too. The camp is
stacked by construction, accepted: 14 bots share 6 slots, so the tightest occupied pair is **0.00 yd in
every pull**.

**Pets chain, and the recorder cannot see it**
([observability](../../systems/observability.md#what-a-trace-cannot-tell-you)), so every hop count here
is a floor. The core settles what the trace cannot: `WorldObjectSpellTargetCheck` skips **totems and
nothing else** under `TARGET_CHECK_ENEMY`, so pets and guardians take hops and can be the seed. **24-28
pet bodies** sit inside 10 yd of him at once, **100% of them within 8 yd of a melee body**, and PetAI
parks a pet anywhere in a 90° arc behind him, straight into the gaps the slot separation opens. They
are chased onto a slot bearing instead (`ThorimPetsOffRingSlot`, on the pet leash node), which costs no
uptime: at ring radius a pet is still inside the 9.1 yd melee reach on any bearing. Army and Feral
Spirits steer here too, though a player cannot command them
([pitfalls](../../engine/pitfalls.md#things-the-bot-cannot-see)). Only pets already on him are steered:
one on an add is doing its job, and one out of the room is the leash's, which runs first and shares the
throttle, so a pet just sent home isn't redirected. The chase is range 0 like PetAI's, with no command
or stop, within `ULDUAR_THORIM_PET_SLOT_TOLERANCE` (22.5°, about 3 yd of arc), its bearing converted
from his live facing every time, since a chase angle is relative to it and he turns with his tank.

**Every bot's pets get a slot, not just a ring melee's**: ranged owners hold 42-48% of the bodies. A
ring melee uses its own slot, everyone else one off its guid counter modulo 3 (stable per tick and an
even spread), so a chain held in one cluster spends its late hops on pets rather than reaching the next
slot's players. **Human-owned pets are 13-30% of the bodies and are not steerable at all**, no
`PlayerbotAI`. Logging pet damage in `Bot/Obs` is still what it would take to count the real hops.

**Who leads the gauntlet: a real player in it, else a bot**, the same code in every raid (user,
2026-11-22: unattended pulls must test what real raids run). Keying on the raid leader whenever a real
player was on the instance left the corridor standing while he stayed in the arena.
- **Leader:** `ThorimGauntletLeader` is the first living real player at the gate (within 10 yd of
  `ULDUAR_THORIM_NEAR_ENTRANCE_POSITION`, the lever ~5.5 yd from it) or past the portcullis (east of the
  arena box within its y band, south of it, or in the balcony box; north of it is the way in), else the
  gauntlet-squad tank, else the squad's first living non-healer bot. The split never picks a human, so
  that position is his squad. Nobody leads in phase 2: the tether below would pull the squad off its
  phase 2 spots and walk the balcony holds back down the corridor. The corridor formation
  (`ThorimGauntletPositioningTrigger`/`Action`, `ThorimResolveGauntletIndex`, so the Runic Smash lane swap
  too) keys on the leader. The `ThorimFallFromFloor` teleport still targets the group leader.
- **Lever:** `ThorimGauntletLeverTrigger` is true for a bot leader once the lever (GO 194264,
  (2173.3, −252.9)) has lost `GO_FLAG_NOT_SELECTABLE` (six start-pack deaths) and is still `GO_STATE_READY`;
  the action walks up to it and sends `CMSG_GAMEOBJ_USE`. A real player leading pulls it himself.
  **Walk to touching distance:** `IsAtInteractDistance` is the Opening spell's (68398) 5 yd around the
  model box (display 295, scale 3), only 5.4-5.8 yd from its centre west and sideways, while `MoveTo`'s
  distance nets out both sizes (2.7 yd). Stopping at the interaction distance less a yard left all nine bot
  leaders of 2026-11-22 6.6 yd out on the arena side, pulling 25-200 s late. `thorim.leverpulled` notes the
  latch. The lever resets after 6 s and the portcullis shuts 8 s after opening (template autoclose 6000 / 8000 ms),
  so `leverPulled` latches on the pull and on a lever seen already used (the far corridor is out of the
  150 yd search range). A bot leader past the portcullis latches it too unless the lever reads locked: taking
  over from a human there, it would otherwise walk back to a reset lever, or, out of range, never start the
  walk.
- **Walk:** `ThorimGauntletLeadTrigger` steps a bot leader through the lane waypoints (`leadNext`, monotonic;
  right lane, or the one the last Runic Smash telegraph left safe), at combat priority. Past the portcullis
  it skips any waypoint north of him, since the lanes run south: a bot taking over mid-corridor from a human
  walks on rather than back to the first. He holds while a squad non-healer is more than 30 yd behind, but
  at most 15 s per step, so a stuck bot cannot eat the 150 s clock. Past the last waypoint he walks on to
  30 yd from the Colossus, which the squad's own targeting then engages. Once the Colossus is dead he walks
  on to the Rune Giant (`ThorimGauntletLeadRuneGiant`), ~100 yd up a ramp and past the 50 yd pick range: in
  all nine bot-led pulls of 2026-11-22 the squad otherwise stood at the dead Colossus until the orb. Gated
  on the first doors (GO 194557, (2227.7, −413.3)) standing open, which the Colossus' death does in both
  sizes. The Giant's `UNIT_FLAG_IMMUNE_TO_PC` is on the 10-man template only
  ([pitfalls](../../engine/pitfalls.md#detection-that-never-fires)): gated on it, all three 25-man
  leaders walked past an untouched Colossus, shut doors and all, into the Iron Honor Guards and the Giant
  (24 deaths in the two pulls that wiped). The cached Colossus lookup can't gate it either: arena melee
  scan it from out of range, so it can read empty while he lives. That miss is shared for 500 ms: the
  walk to the Colossus skips a tick (orders stayed at most 2.6 s apart), and `TickRunicSmash`, which
  arena bots run too, drops the Runic Smash latch on it. Also gated on the leader past the
  portcullis and within 150 yd of the Giant's spawn, since a scan from farther out (the arena, the left
  lane's end at ~165 yd) misses and that cached miss reads "Giant dead" to the balcony walk. Pathed to his
  own spot (a point on the straight line can land off the mesh) and all the way in, until he is in combat:
  once an Iron Honor Guard engages, the Giant spawns another every 10-20 s until he is pulled, and the squad
  picks guards first. Followers, behind a bot or a real player alike, take the leader's waypoint; between
  waypoints they walk to him once more than 25 yd (healers 20 yd) behind, except a non-healer fighting a
  gauntlet mob in combat within the 50 yd pick range (`ThorimGauntletFighting`): that tether fought `reach
  melee` whenever he walked on mid-fight, 116-185 DPS bot-seconds a pull of moving without damage, three
  quarters of it on the Honor Guards and Giant. The gate walk re-adds follow-master only when the leader is
  the bot's master.
- **Clock:** the walk is about 57 yd from the portcullis to the first waypoint and 65 yd to the last, so
  roughly 20 s of running at 7 yd/s plus the lever walk (about 6 s), against the ~28 s opening wait a human
  led squad accepts. The corridor fights set the clock. Measured on 25-man (six bot-led pulls): the lever
  pulled 5-8 s after the sixth start-pack death, leaving 163-165 s to Sif's 62565; from it, the walk to the
  trash 12-17 s, the trash 33-44 s, on to the Colossus 3.5-17 s, the Colossus 18-35 s, the ramp 5-10 s
  (the walk to the Giant starts 0.4-0.8 s after the Colossus dies), Honor Guards, Acolyte and Giant
  51-62 s, his death to the balcony hit 16-20 s. Phase 2 came 156-183 s after the lever: hard mode in three
  pulls, 8.6-18.6 s late in the other three.

**The last 67 yd of the lower corridor are unwaypointed**: waypoints stop at y −329 while the Runic
Colossus is at y −396, so `ThorimRunicSmashAction` cannot resolve a lane index on the final approach,
where the smash is most dangerous. The formation hit the same wall: trigger and action both place the
squad off `ThorimGauntletLaneIndex(leader, …)`, which needs the leader **standing inside a waypoint
radius**, so once he walks past the chain the row goes quiet for everyone. DPS do not notice, since
`reach melee` and `reach spell` chase their targets the rest of the way, but a healer chases nothing and
`follow` cannot cover it, being `BOT_STATE_NON_COMBAT` in a running fight. Measured: the 11 non-healers
reached **183-191 yd** out while the two healers stalled at **148 and 129**, one of them with **no move
order at all** across 22 s, leaving the human 68.8 and 101.1 yd behind against the 40 yd heal range and
his last outside heal 21 s before he died. `ULDUAR_THORIM_GAUNTLET_TETHER_DISTANCE` (20 yd) now walks a
trailing healer to the leader once the lane index fails; non-healers trail as in **Walk** above.

**The fall rescue still stops at 110 yd.** `ThorimFallFromFloorTrigger` bails past
`ULDUAR_THORIM_FLOOR_RANGE` (110 yd) from the arena centre, short of the left lane's last three
waypoints at **117.3 / 120.8 / 126.1 yd** and of the corridor's far end, and it `NearTeleportTo`s onto
the master, same map only ([pitfalls](../../engine/pitfalls.md#before-the-pull-and-out-of-combat)). The
gauntlet row no longer shares that gate: it reads `ULDUAR_THORIM_ENCOUNTER_PROXIMITY` (200), which
covers the whole corridor. Bots do sink there: a ranged or healer `flee` of 4-5 yd west in the right lane,
at (2219, −354) and the ramp foot (2217, −423), paths below the floor to z ~390 and back over 6.5-7 s
(1-7 bots in four of six 25-man pulls), once to z 264 for 56 s. Not navprobed.

**The ~28 s opening wait is deliberate.** The squad does not move until the leader reaches
`ULDUAR_THORIM_NEAR_ENTRANCE_POSITION` (first accepted corridor move at 0:27.7 / 0:24.8). It stays: it
is the difference between missing and making the clock.

**Human-led, the clock was not the problem**: 10 of 12 traced pulls (September-October) made it, at
margins of +6.8 to +37.9 s from the Touch of Dominion cast; bot-led it still is (**Clock** above). The
spread is corridor-trash time and it is still Iron Honor Guard:
**92.8 s** held on the slow pull against **48.6-51.4 s** on the rest, with Colossus and Rune Giant time
identical. Snapshot cadence never moved (p50 227-229 ms), so it is bot behaviour, not load.

**The tanks eat the Lightning Charge cone, and are left out of the button on purpose.** They took
**7 of 25** cone hits in one pull and cannot dodge: both tank spots are fixed, and the Blizzard row
already excuses both tank roles for the same reason. Their buttons stay for his melee and Unbalancing
Strike, which is what actually kills them.

**The shelter run is uncapped.** Observed distances to the shelter when the note fired were **52.1 and
60.6 yd** against a table solved for 25.6, and a bot that does not make it takes the cone while moving.
Revisit only if the still-walking count grows once the other gaps are closed.

**Arena adds are never tanked** (in any trace, including the kill, 0-8% of them target a tank), so they
free-roam onto the camp and **32-39% of phase 1 add damage lands on ranged**. Chronic, costs almost no
lives, deliberately out of scope. Nothing taunts an add off whoever it rolled, and nothing recovers the
fight once the arena squad is dead; the 5 second scan leaves no room to walk anyone back.

Documented, not implemented: the in-combat `SPELL_SMASH` 62339 frontal cone (60 degrees, 3s cast, a
different spell from the corridor Runic Smash), Rune Detonation 62526, Stomp 62411, and Runic
Fortification 62942. Heals on a freshly swapped-in tank are also thin (one took no heal at all in the
2.9 s he died over), but that is generic heal AI, not this strategy.

**The squad split counts human tanks.** `AssignThorimSquads` counts tanks with a bare
`PlayerbotAI::IsTank(member)` over the whole roster, so a human protection paladin makes `tankCount`
read 2 and the raid's only *bot* tank is sent down the corridor, leaving the arena tanked by the human,
which is also why a human shows up high on the arena damage meter. The fix is to count and pick tanks
bot-only (`IsBotPlayer(member) && PlayerbotAI::IsTank(member)`) and to skip tanks in the DPS loop,
since `IsDps` is also true for a protection paladin. **Excluded at the user's request, twice: do not
re-audit.**

**Burst cooldowns stay held for the whole of phase 1**: roughly 500 suppression episodes across one
arena squad, tinker 153, trinket 107, Blood Fury 58, Rapid Fire 28, Readiness 27, Blade Flurry 23,
Adrenaline Rush 18, Death Wish 16, Bestial Wrath 14, Army 13, Killing Spree 12, Recklessness 11,
Berserk 10. Recorded so it is not rediscovered.

### Measured and backed out

**A rigid whole-formation rotation for ranged.** Turning the ranged camp off the lit orb costs about
**500 yd of walking per ranged bot over a 191 s phase 2** (342 yd even at the spell's own 37.5°
half-width) to save **647k** of damage the healers' 3.5× margin already covered, against roughly
**4.5 M** of ranged output lost. **Worth revisiting only for hard mode**, where that margin is gone
(17.9k incoming against 20.7k hps); `sifJoined` already latches Sif's first Blizzard, so the gate is
cheap, but it needs its own measurement.

**Moving the tank anchor east, to (2135.0, −263.0, 419.846).** Against the shipped spot it clears the
Blizzard track by **29.7 yd** instead of **5.1**, sits 11.2 yd from the arena centre, lies within 0.2 yd
of a 0.5 yd grid search's optimum, and navprobes clean (poly distance 0.130, a 26.37 yd / 8 waypoint
walk from the current anchor); tanks take **16-21%** of their intake from Blizzard (157k in one pull).
But it **breaks 11 of the 18 camp points**: home slots 1-5 fall to 6.0-18.3 yd against the 22-32 yd
band, and seven shelters land 4.5-17.3 yd from the anchor against the 22 yd floor that stops the ring
bridging Chain Lightning in. It is a full re-solve of six home spots and twelve shelters plus eighteen
navprobes, not a nudge, and each new point needs the hazard check navprobe does not make
([pitfalls](../../engine/pitfalls.md#a-solved-table-outlives-the-traces-it-was-solved-from)).
