# Thorim

Two halves that share nothing: a corridor gauntlet past the Runic Colossus and the Ancient Rune Giant,
then a stationary fight on the arena floor once Thorim drops off his balcony. `GetPositionZ() < 429.6`
separates the two everywhere in the strategy but the Rune Giant's ramp
([balcony walk](#the-corridor-traps-nothing-can-see)).

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
- **The walk opens at the Giant's death, on his ramp too** (`ThorimBalconyOpen`): above the floor line,
  or below it inside the ramp box (`ULDUAR_THORIM_RAMP_BOX_*`: x 2150-2235, y −445..−385) once the second
  Colossus doors (GO 194558, (2135.08, −417.905, 440.0)) stand open, which only his death does. The
  [hold](#known-gaps) kills him at z ~426 with the squad at z 412-426; on height alone, 8 of 11 bots of a pull
  whose Giant died at z 427.6 walked back down the corridor. Below the line the doors back up the
  Giant scan, whose cached miss reads "Giant dead". A balcony waypoint counts as reached only above the line:
  the y-passed test would put a ramp bot at y > −408 past `BALCONY_1` (> −396: `BALCONY_2`) and aim it
  at a balcony waypoint from below.
- **`thorim balcony guard` holds the generic movers** for a corridor bot on that side while the
  walk has somewhere to go: `follow` would take the only walkable way down to the master, 300 yd back
  through the corridor. `AvoidAoe` stays, since Rune Detonation lands up there. In phase 2
  `ThorimBalconyGuardMultiplier` also zeroes `ReachTargetAction` on that side: a reach walks the bot back
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
| Smash | 62339 | 3s cast at his target, first 12-35s after the engage, then every 15-22s: 37k physical in a 10 yd frontal cone (healers hit 11 yd out, 12° off his facing), and on anyone within ~2 yd of him whatever the facing |

`EVENT_RC_RUNIC_SMASH` is scheduled in `Reset()` and **cancelled in `JustEngagedWith`**, so the corridor
smash only happens on the approach and stops the moment the Colossus is tanked. The Ancient Rune Giant
has no damage shield: its Runic Fortification (62942) is a friendly buff on its adds.

The two corridor lanes (left x ~2237-2242, right x ~2212-2219) are **index-matched by y**, so a dodge is
the same waypoint number in the other lane. Each lane sits 2-9 yd from its own hand's bunnies and
15.5-22.9 yd from the other's. A smash hits within 10 yd of a bunny plus the target's reach (11.3 yd 2D
at most over 605 hits), and a waypoint's radius is net of the bot's own reach, so the last pair (narrow
floor, **The last 36 yd** below) clears the other hand's bunnies by only 15.7-16.0 yd and takes a 2 yd
radius: with 10, a bot on the centre line, which both rows hit, would read as in either lane and never
dodge. The bot side latches the hand it sees casting and holds the opposite lane
until the other hand goes up; the gauntlet formation follows that preference rather than the master's
own lane, or it would walk everyone back into the blast. **Switching the moment the next cast shows is
safe, do not delay it**: the last hit landed 3s earlier, and the 5s cast is all the time there is to
cross the centre line, which both rows reach. The dodge stays armed `ULDUAR_THORIM_RUNIC_SMASH_LATCH_MS`
(10 s) from the last scan that saw a cast, outlasting the hit and the 3s gap before the next cast, so
bots keep off the centre line the whole approach. The Colossus is 131 yd from the top pair of
waypoints, past the 100 yd `Playerbots.SightDistance` cap, so the telegraph is read through a targeted
150 yd creature lookup rather than the usual target values.

**That lookup keeps him once found, and only a bot within 150 yd of his spawn scans**
(`GetThorimRunicColossus`). Every bot runs `TickRunicSmash` and arena bots stand 158-175 yd out: their
misses cleared the shared latch 13-22 times a pull, leaving it off 27-52% of the approach, and 81 of 93
corridor smash hits over six pulls landed in a cast during which it had dropped.

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

It also sets the Colossus' clock. Bot-led, it landed 11-18 s after the engage in five of six pulls,
with him at 21-81%, and the rest took 7-26 s; the one Colossus dead in 20 s never got one.

**Gauntlet healers trail to 15 yd short of the leader, never onto him** (`ThorimGauntletTrailSpot`: the
path to the leader cut at `ULDUAR_THORIM_GAUNTLET_HEALER_STANDOFF`, under the 20 yd tether so the trail
doesn't re-fire; the leader's own spot when no normal path is found). The leader walks into the Colossus
and tanks him from ~4.4 yd south, inside the Smash cone, and healers tethered from the y −360 pull line
stood on the leader: in 10 of 53 pulls with a Smash both were hit, 12 healers dead, while healers 7-13 yd
behind the Colossus were never hit. In the six at hard mode the first phase 2 tank death came at
+45-137 s (p50 77, against 119 without): a gauntlet healer lost costs the race. Replayed over those ten,
each stop lands 10.6 yd behind the Colossus.

## Phase 2

| Spell | Id | Detail |
|---|---|---|
| Chain Lightning | 62131 / 64390 | 8 targets at **×1.5 per hop** (`EffectChainAmplitude`): the eighth takes ~17× the first. Each hop goes to the unhit target nearest the last victim, within **8 yd** (`spell_jump_distance` 5.0, not the DBC 10, plus both combat reaches); players only (`SPELL_ATTR3_ONLY_ON_PLAYER`, below). First cast 13 s after the phase trigger, then every 15 s |
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
degrees, which leaves the whole tank side clear and puts the stacks 12.4 yd apart. Slots are sticky
per guid, handed out least-loaded so the melee spread evenly, and kept while a bot is dead, or a
rezzed bot's slot comes back a quarter turn off. **`RingAnchorBearing` reads only the two tank spots**
(the bearing to their midpoint): the live tank is deliberately not an input, as the rule rather than a
fallback, so the ring does not follow each shuffle. Arrival uses a 3 yd / 5 yd deadband, because a
tight one against a ring recomputed from a moving boss leaves the bot sliding in place, and a moving bot
casts nothing. The trigger sets the arrival latch on a "stay" answer and the action clears it right
before it moves, since the action only runs when there is a move; set in a trigger it only widens the
tolerance, so a second ask that tick still says stay.

**The anchor latches where he lands and re-latches only when that ring reaches a tank.** A taunt from
the west off-tank spot can re-seat him south-west of the tank spot for good
([trap](#somebody-has-to-hold-the-boss-and-a-taunt-will-not-do-it)), up to 4.5 yd off it and 3.2 off
the off-tank spot. The landing anchor then put a slot point inside 8 yd of the holder (median 4.9-5.4
yd) in 61-94% of settled snapshots in four pulls, and an 8-hop Chain Lightning through that slot killed
both tanks at P2 +57 s (62k and 92k): a wipe. `RelatchRingAnchor` re-latches while he stands still on a
tank, when a latched slot point lies within 8 yd (the 5 yd jump plus both 1.5 reaches) of either tank
spot and the live anchor's ring clears them by `ULDUAR_THORIM_RING_ANCHOR_GAIN` (0.5 yd) more. Each slot
takes the new bearing that keeps the largest turn smallest, the arrival latches drop, and the
`thorim.ringspot` bearing moves. Over 18 replayed pulls: a slot point within 8 yd of a tank spot in 42%
of settled snapshots, 2% with the re-latch, at 0-3 re-latches a pull and none in eight of nine clear pulls.
A bearing threshold can't separate the cases (4-6 yd from him, 20° of drift already puts a slot in the
jump; a 25° rule left 16%), and the tank spot alone as anchor leaves the waiting tank 6.4-7.0 yd from a
slot whenever he sits south-west (midpoint: 8.2-9.0). At his deepest the pair spans 50° of his circle
and the best ring still leaves a slot 7.7 yd from the waiting tank.

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
points). The bridge counts that point too, not only where the bots stand: a re-solve on the lit orb puts
it beside a body resting in the deadband, its bots get there with the chain ~1 s later, and a test on
bodies alone moved the resting one after the cast. The 1 yd floor stops a bot on its point reissuing the
move while a body that won't leave bridges it.

**A walk across the lit cone waits for it.** Walking back from a Blizzard step, or to a point past the
cone, a ring bot was still in it when it landed: over 37 hard-mode pulls 26 of 60 such walks were hit,
10 dying, 4 before the first tank death. `ThorimRingWantsMove` answers stay while the orb is lit, the
bot stands outside the ring's cone and its straight walk to the point enters it. Inside the ring's
line but past 45°, where the cone step leaves a bot, it stays only when the walk comes within 45°: at
the cast nobody 45-52.5° off the cone was hit (0 of 350), yet 14 of 20 walks from there into 45° were,
7 dying. A bot within 45° always walks. The stay sets the arrival latch, so the movement guard holds
the generic movers while `reach melee`, straight at him along one bearing, stays free. A bot in a
Blizzard zone holds too: at its 2.4 yd/s the crossing outlasts the warning. Replayed, a held walk waits
up to 5 s, about 4 bot-s a pull.

**A ring move under the lit orb waits for the chain** (the user's call, like the
[camp's](#shelters-and-chain-lightning)). Over 58 hard-mode pulls 27 of 38 ring-crossing chain deaths (the
opening aside) came in casts whose bridge closed in the 2.5 s before the landing, from walks and steps
ordered on the lit orb. So while `ThorimChainWaitDue` holds, `ThorimRingChainWait` stands a ring bot whose
positioning walk or Blizzard step would end within the jump (5.5 yd plus both reaches) of another group's
floor body (a tank, the camp, a human, another slot's bot) or another slot's point, until the bar ends or,
with none started, the cap passes. Released, it is not held again under that orb; a walk already under
way is not stopped. It holds only:

- **from a clear spot:** a bot already in such a body's jump walks, since it bridges either way; held
  regardless, 7 replayed chains grew, against 1.
- **while the cone stays escapable:** a bot inside the ring's cone line waits only if its destination is
  in reach setting off at the bar's end or the cap (`ThorimWalkBeatsCone`, the camp's test).
- **outside Blizzard:** a bot in a zone (aura 62576/62602) moves at once, and a zone landing on a waiting
  bot releases it: the bunny walks the 8 yd lead in about a second.

While it waits the movement guard counts it settled, `thorim ring chain wait` blocks `Reach` and `Charge`,
and `ThorimChainWaitCastHoldMultiplier` holds cast-time spells; `thorim.ringchainwait` reads `wait walk` or
`wait step`, then `none`. Replayed on those pulls (the player-only hop model, [below](#known-gaps); a held
bot stands where ordered, then walks straight at its speed): 703 of 3,646 ring orders under a lit orb
wait, p50 0.86 s, max 1.6, ~10 bot-s a pull, 71 starting out of reach; hits at hop 5 or later fall 48 →
31, crossings between groups 96 → 74, and on the 360 chains the model reproduces 12 of 44 chain deaths
go (9 of 29 before the first tank death); 2 held walks newly end in the cone, 8 leave it, and the cone
test lets 16 walk.

**Killing Spree waits for the dark orb.** `51690` blinks a rogue to his back, ~3.3 yd from him, for 2.5 s,
inside the jump of the holder and of every slot point within ~65° of his back. A combat rogue spends it on
burst release, at the first chain (+12 s), and again every 75 s (glyphed: five orb cycles), so it keeps
landing on the chain: over 64 hard-mode pulls 53 of the 120 casts after a lit began within 2 s of it, 15
chains hit a spree rogue, 13 crossing groups through it (8 dead past it, 6 before the first tank death),
all 9 opening-chain deaths came in such casts, and the cone hit 10. `thorim killing spree hold` blocks
`killing spree` for a ring bot on the floor except 5.5-9.0 s after the last lit stamp
(`ULDUAR_THORIM_KILLING_SPREE_OPEN_MS` / `_CLOSE_MS` on `lightningOrbLastLitMs`, which the dark orb leaves
set): past the cone even on a stamp one scan late, back on its point before the next lit 14.25 s on. The 18
casts that fell in that window met no chain and no cone. With no lit yet in phase 2 it holds, so the
opening spree comes after the first chain.

Every melee DPS carries the `behind` strategy from `AiFactory`, so `SetBehindTargetAction` would walk all
three stacks into one arc behind the boss the moment the ring node yields. The `thorim movement guard` rule
holds the generic movers, scoped to a **settled**, [stepped-out](#a-covered-slot-steps-out) or
chain-waiting ring holder and exempting `AttackAction` and `ReachTargetAction`: a permanent
movement freeze is the Void Reaver failure.
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
A slot tries the side it answered on last time first, so one new cone cannot flip it across the ring.
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

**Separation gives way when the arc will not hold it, the cone never** (~20k in one instant). **The
anchor term is still angle-only** — the tank sits at r 5.5, so 60° off him is **7.1 yd** of ground and a
chain still bridges the pair. Unfixed. Nor does it count the waiting tank, whose
[spot](#somebody-has-to-hold-the-boss-and-a-taunt-will-not-do-it) is placed off the ring instead; only
the anchor re-latch measures yards to both spots.

### A covered slot steps out

**The solver never trades a bearing for Blizzard.** When it did, a covered slot swung onto another slot
or the tank side as the trail moved (203 → 177 → 153 → 281° in 1.7 s), and generic `avoid aoe` hopped
its melee 3-5 yd at a time through overlapping zones, its combat-priority bookings refusing the ring's
own walk for ~5 s: **55 ticks, 248k, 3 deaths** and **40, 185k, 3** in two hard-mode wipes. No rotation
fixes it: a median **4 zones up (max 6)** leaves under 105° of ring 11 yd clear, too little for three
slots plus the anchor, in **14.5-31.1%** of a wipe's Blizzard snapshots, and none in 5% (0% in the kill).

**Covered melee step out past the ring and walk back when it fades.** `ThorimRingBlizzardCovered`: the
slot point within `ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE` of a zone, the bunny or its next 8 yd of path
(below), or, for a bot standing clear more than 3 yd off it, the straight walk there passing a zone or the
bunny. Each change flips `thorim.ringstep` and drops the arrival latch, so the walk back takes the 3 yd
tolerance, not the 5 that would leave it out of reach. While covered the positioning row stands down: a
bot clear of all three, the lit cone and another slot's jump (below) holds where it stands, one in any
takes `ThorimRingBlizzardStep` through the Blizzard node at `MOVEMENT_FORCED`, to the nearest point 11 yd
clear of all three, outside the cone, in the arena box, ties to the one nearest its slot point. In order:
past the ring, 8 yd (the jump plus both reaches) off both tank spots and out of the jump (5.5 yd plus both
reaches) of every floor body but the tanks and its own slot, each other slot also at its point; the same
clear of the slots but not the camp; of neither; past the ring alone. These four run twice, first within
20 yd and 11 yd clear of the bunny's walk ahead (below), then within 15 yd without it; then anywhere
within 20. Slot 0 sits south-west of him between the west wall (x ~2100), the tank spots and the trail,
so a hard tank clearance found no point for 14-79% of the covered time in a six-pull replay.

**Blind to the other bodies, the step was a bridge.** Over ten hard-mode wipes 125 of 452 step points
landed in another slot's jump, and 11 of their 44 Chain Lightning deaths crossed slot to slot through a
slot on its step point or walking back from it, slot 0 once 1.9 yd from slot 1. Another slot counts where
its bots stand and at its point, since they walk back to it. The camp is every other floor body but the
tanks, a human melee included, and leads only the first pass of each round: clear of both, a replay found
no point for half the steps, against a third for the slots alone. Without that pass the slots-clear
points stood in a camp body's jump twice as often as the old ones (118 against 59 of 295 steps); with it
66, for a walk 6 yd longer than the old at p90, against 4 without.

**Clear at the step is not clear for the stay.** The slots keep moving while a bot stands stepped out: a
re-solve on the lit orb or a walk back puts another slot's point in its jump, and over 58 hard-mode pulls
four casts crossed into a slot through a stepped bot already standing there ≥ 0.8 s, 9 dead (slot 2 to
slot 1 at 2.5 and 8.0 yd, 3 each). So a stepped bot clear of all three and the cone re-steps while
another slot's bot or point (the camp aside) lies in its jump, through the passes that clear the slots
only, never walking across the lit cone (above), and holds when they find nothing.

**Blind to where the bunny goes, the step ran ahead of it.** Cleared of the live zones and the bunny
alone, the nearest point lay within 11 yd of the bunny's next legs for 41-84% of a pull's steps, and at
run pace the bunny caught the bot there: it stepped again, ahead again, and twice in one window a whole
slot died that way at +75-81 s, its bots 8.8-10.1 s in the zones. So the first round also clears
`ThorimBlizzardPathAhead`: the bunny's walk from where it stands through the rest of
`ULDUAR_THORIM_SIF_BLIZZARD_PATH` (the core's waypoints). Replayed on 641 steps over fourteen hard-mode
pulls (no navmesh, the floor cut at x 2100 and y −288), points a zone covered within 8 s fell from 305 to
32 and every step still found one; the walk grew p50 6 → 12 yd, p90 14 → 20, and points in a camp
body's jump 35 → 120, which a 15 yd round would have left at 173. It is the whole walk, not a lookahead:
30 yd of it scored the same, 20 yd left 52 points covered, 10 yd 230.

**In the lit cone with no clear point, the step leaves the cone anyway.** Every pass wants 11 yd off
every zone and 52.5° off the cone, so far out a bot can find nothing: slot 2, stepped to the north wall
20-27 yd from him, had wall and zones west and orb 6's cone east, and stood in the cone 3-5 s, two of
three dying. Then a bot within 45° of the lit cone, as the camp's cone step reads it, takes the nearest
point past 45° within 20 yd, in the arena box, ties to its slot point: clear of every zone's 9.8 yd
reach if any is, else inside one, since a tick is 4.3-4.9k and the cone 10-36k. Past 45° it stands, or
it would re-step every tick against the ring's wider line.

**A walk across the bunny's path leaves before the bunny gets there.** Two slot points sit about 5 yd off
the trail's west and north legs, and their near side has no chain-clear point (the camp, the next slot),
so the step crossed to the far side with the bunny p50 ~8 yd short of the crossing and walked into its
fresh zones at 2.4 yd/s (a zone's or Frostbolt's 65% slow). Over 49 hard-mode pulls before the first tank
death, 341 of 1,318 step orders crossed the path ahead, for 486 ticks and 11 deaths against 7 ticks on 582
walks clear of it, and 16 of the 20 ring Blizzard deaths came within 10 s of such a step. So the bunny's
next `ULDUAR_THORIM_RING_BLIZZARD_LEAD` (8) yd of path counts as Blizzard for the ring: within 11 yd of it
a slot point is covered, a ring bot steps, and no step point lies. Replayed on each bot's first step per
bunny (the same point, 7 yd/s, 2.45 inside a zone's reach): zone time 1,128 → 147 bot-s on crossing walks,
122 of it within 4 s of the bunny's spawn, which leaves no lead; 323 → 229 on the others; ~1.7 s more out
of reach a step, ~26 bot-s a pull. A 10 yd lead bought 37 bot-s for 385 more out; 6 yd left 271. Kept on its
own side instead, the step put 204 of 341 points in the camp's jump and 68 in another slot's; that side
preferred only at equal chain clearance changed 110 orders, the mildest (111 ticks).

**Two holds keep it.** `thorim ring blizzard step` blocks `Reach` and `Charge` while stepped, or `reach
melee` walks the bot straight back in, and the movement guard counts a stepped bot as settled.
`thorim ring aoe hold` blocks `AvoidAoe` for ring melee on the floor all phase 2: its hops were the damage.

**The cost is uptime.** Replayed per slot on six hard-mode pulls at 7 yd/s: ring bot-seconds within the
9.8 yd damage reach 8-51 → 0-6 a pull, melee in reach up to 7 points less than the ring as fought (a covered
slot sits out up to a zone's 10 s), walk back p50 0.7-2.5 s, max 4.5. Pets hold their slot bearing under
it: [pets dodge nothing](#pets).

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

**The table's apex is the tank spot; the cone's is Thorim.** He stands 4.5-4.8 yd off the tank spot, and
on the west seat (x ~2107, by the off-tank spot; 17% of cones before the first tank death) orb 2's rows for
slots 0 and 2 sit 37.2° and 35.8° off the live axis: 7 of the 25 bots on them at the cast were hit, 4
died, against none of 38 on rows 39.2° or more off. So a row the camp's live test covers
(`ThorimCampSpotInLitCone`, 40°) takes the cone step as a slow walk does, a bot already on it too, and the
chain wait counts it as out of reach. Slot 2's home lies ~23 yd from any point 45° off that cone, past the
step's 20, so it still walks its row.

`ULDUAR_THORIM_SHELTER_SPOTS` has no rows for orb indices 4 and 5 ((2092.95, −263.00) and
(2104.94, −233.44)), whose cones point away from the camp. Its longest run, 25.6 yd, is ~3.7 s at full speed
against the 4.9 s worst warning, and a bot commits from wherever it stands, so walking home the moment the orb
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

**Sif's slows break that budget, so a walker short of time takes a cone step.** Her Blizzard zone slows
**65%** (62602/62576, aura 33), as does her Frostbolt; the Volley slows 50% and Frost Nova roots; a bot
mid-cast sets off only when the cast ends, and a channel holds it in place through its move. Over 19
hard-mode pulls, of 146 shelter walks before the first tank death (the opening left out) 51 ended short,
24 were hit and 13 died, 11 of those on slot 5's rows for orbs 0 and 1 (25.6 and 21.8 yd, across the
Blizzard track), walked at 2.4-3.5 yd/s or begun 1-3 s late; an Evocation held one 4.4 s.
`ThorimWalkBeatsCone` sets the row's distance against run speed times the time left (lit plus 4.8 s
less one scan), cut to 0.35 when the straight walk crosses a zone not yet slowing the bot. Short,
unrooted and in the lit cone, `ThorimCampConeStep` searches once per orb for the nearest point in the
uncut reach (at most 20 yd) **45°** off the cone (7.5° past its edge: hits decode to 43.8°), 12-32 yd
from Thorim, 11 yd clear of every zone, its walk clear of zones unless the bot already stands in one,
ties to the row; the note reads `slot N orb M step`. It needs no body clearance: Chain Lightning lands
0.4-1.4 s after the orb lights, before any walker arrives. The positioning action breaks a channel while
the camp bot stands in the lit cone. Replayed on those walks, the step took 11 of 25 hit walkers out of
the cone and 6 of 16 dying ones (deaths within 1.5 s); a rooted or late bot it cannot save.

**A walk that passes another slot's body waits for the chain.** Measured from the lit stamp, Chain
Lightning's cast starts at p50 0.5 s, p90 1.0, max 1.5 and lands at p50 1.05 s (395 hard-mode casts), so
it lands mid-walk, and crossing walks bridge it: on the 53 hard-mode pulls on disk, 16 of the 20 camp chain
deaths before the first tank death came under orbs 1 and 2, whose walks cross (slots 3 and 4 under orb 1,
slots 0-4 under orb 2). There (`ULDUAR_THORIM_CHAIN_WAIT_ORBS`, the user's call) `ThorimCampChainWait`
decides once per orb: a camp bot whose straight walk to its row passes within the jump (5 yd plus both
reaches) of any floor member outside its own ranged slot stands until Thorim's Chain Lightning cast bar
ends; the note reads `slot N orb M wait`.

- **No cast, no wait.** 191 of 586 lit orbs had no Chain Lightning: the core drops a cast that fails its
  checks and still repeats the event in 15 s. With no cast bar 1 s after the lit
  (`ULDUAR_THORIM_CHAIN_WAIT_CAP_MS`) the bot walks; a cast that ended up to 5 s before the lit
  (`ULDUAR_THORIM_CHAIN_WAIT_LEAD_MS`) counts as this orb's, so the bot never waits.
- **Only when the late start is affordable:** the bot waits only if, setting off at the bar's end or the
  cap, its row or a cone step is still in reach, or it stands outside the lit cone. After the wait the
  in-time test and the step run as above.
- **No cast starts while it waits:** `ThorimChainWaitCastHoldMultiplier` zeroes spells with a cast time or
  channel, or a cast begun in the wait would hold the walk past the landing.

Replayed on those pulls (1,251 orb 1 and 2 walks; the chain re-run hop by hop to the nearest unhit body
within 8 yd, pets wrongly included): 651 walks wait, 548 of them set off later; the reach test lets 63
walk (2 chain hits, none dead); all 14 walkers a chain killed stand when it lands; on the 80 chains the
hop model reproduces, camp hits at hop 5 or later fall from 14 to 4. Cone hits (37.5° off its axis, where every
decoded hit lies): 11 walks newly end inside, 11 others leave it.

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
arrive/reposition path in the positioning trigger and action either. The holder backs into its spot
when the spot lies away from him and walks in forward otherwise, so it always faces the boss it drags.
`ThorimTankPicked` takes
`tank assist` away from tanks in phase 2 only: left voting, it held a tank on a Warbringer for the first
30 s while Thorim ate the camp, and in phase 1 holding whatever swings at the raid beats steering a tank
onto the ranged pick.

**A tank's spot, not just its role, decides which way he faces.** The anchor tank holds him squarely on
the design axis (**0°** off the boss→tank-spot bearing over 406 snapshots), while an off-tank holding
him from the off-tank spot turns him **39°** off it over 399, half the phase each. The ring is solved
against that axis, so the swing is what drags the outer slots into his front arc. **Whichever tank
holds him now takes the anchor**, every other tank the off-tank spot, and both are fixed points: at
3.5 yd apart a trade steps him inside `ULDUAR_THORIM_RING_REPOSITION_TOLERANCE` (5), so it no longer
unlatches the ring the way the old 7.07 yd off-tank spot did. **That includes the anchor tank while it
waits**: keyed on its role, it kept the anchor and stood on the holder for 40-58% of the waiting time in
nine 25-man pulls. It keeps the anchor only while no tank holds him, for the pickup.

**Only while its class taunt is ready.** The anchor spends it on arena adds, and in 37 of 43 phase 2s it
was still cooling at the landing: the pickup's `taunt spell` failed and the spot walked the anchor 26 yd
off a loose Thorim. A tank took him at a median 1.7 s, 2.2-5.6 s in 16 of the 37 (the anchor 14-39 yd off
him in 14): 348k of his melee on non-tanks before a tank had him and 3 deaths (taunt ready: 6 pulls, all
within 0.74 s, no such melee). So with its taunt cooling (`ClassTauntReady`) and him on a non-tank or
nobody, the anchor gets no spot unless it carries Unbalancing Strike: it closes on him and builds threat,
and the spot comes back with the taunt or once he hits it.

**The tank he hits has arrived only within 1 yd exact** (`ULDUAR_THORIM_PHASE2_HOLDER_TOLERANCE`,
`ThorimTankSpotNeedsMove`). The tanks' 1 yd `GetDistance` test nets the bot's combat reach, 1.5 yd times its
model scale, so it passes at 2.5 yd exact, and at 2.95 under Heroism or Bloodlust (32182 / 2825, +30%
`SPELL_AURA_MOD_SCALE`): an anchor tank that had stepped in from the off-tank spot took him back 2.1-2.8 yd
short of the tank spot and stayed there, the waiting tank within 2 yd of it for 8-51% of the waiting time
in four of nine pulls. The waiting tank keeps the net test, room for its reach step; the holder needs
none, since Thorim chases his victim.

**A swap taunt fires only within `ULDUAR_THORIM_SWAP_TANK_SPOT_RADIUS` (8 yd) of the tank spot.**
Ungated, the corridor tank answered the first Unbalancing Strike (+7-14 s) from the balcony or mid-jump,
37-44 yd out, in 32 of 37 phase 2s: Thorim followed it 16-36 yd off the tank spot and lost 0.10-0.39 %/s
until walked back, against 0.46-0.92 over the next 30 s, and melee chasing him died to Lightning Charge and
Chain Lightning. The struck holder now keeps him until the corridor tank comes in, inside the strike's 15 s.
8 yd still covers the waiting tank's reach step and a tank taking him back toward the spots from 6-7 yd.

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
still works. It holds every taunt while another tank has him, and **one from past
`ULDUAR_THORIM_SWAP_TANK_SPOT_RADIUS` of the tank spot (`ThorimAtTankSpots`) while a non-tank or nobody
does**: in 5 of 40 phase 2s the corridor tank's `lose aggro` Dark Command or Growl answered his opening
flick onto a non-tank from the balcony or mid-jump, 38-45 yd out, and dragged him 14-41 yd off the spot.
**The anchor tank is exempt**: the pickup and the swap taunt through `DoSpecificAction`, which no `Block`
stops but which runs no alternative, so with the pickup's `taunt spell` on cooldown the anchor's class
fallback is its only answer (Righteous Defense from 10-11 yd took him off two melee he had held since the
landing). **`righteous defense` must be in any name-match list**: it is half the taunts in the trace.

**Both tank spots are absolute**, because an anchor measured off the thing you are dragging has no
fixed point: an off-tank spot off the boss's live position walked the pair **256 and 92 yd** in phase
2 against **39 yd** in a kill. The off-tank's is **(2107.25, −252.65, 419.371), 3.5 yd west** of the tank
spot, inside the ring's 5.0 yd reposition tolerance, so a taunt trade does not unlatch the ring;
navprobe settles that Z, with a straight path from the tank spot. The pair still bridges (3.5 yd against
the 8 yd jump), but **west keeps the chain off the ring**. Thorim rested 4.8-7.4 yd south-east of the
tank spot, so the old spot 3.5 yd east stood 2.9-5.7 yd from his centre and 7.1 yd from slot 2's default
point: a chain reaching either tank ran on into slot 2, three lethal casts in three hard-mode pulls, two of
them tank deaths, and the bridge escape can't clear it (slot 2's point isn't clear). Replayed on six
25-man pulls, a non-tank body stood within 8 yd of the old spot in 84-100% of settled phase 2 snapshots
(slot 2's in 67-100%), of the new one in 18-36% (one pull 88%; slot 2's 0-46%). Its costs:

- **Sif's Blizzard**: 1.0 yd off the bunny's path (leg (2104.3, −256.3) → (2109.3, −246.4)), against 4.1
  for the tank spot and 7.2 for the old one; replayed zone drops give it the tank spot's 36-61 ticks a
  hard-mode pull, the old spot 30-50. Every sweep covers it for ~12.4 s, and **the waiting tank holds it
  anyway: no step out stays clear of Chain Lightning.** At the 7 casts landing under a sweep (four
  hard-mode phase 2s), the nearest point 11 yd clear lay 8.5-9 yd east, toward Thorim, 2.5-6.4 yd from a
  pet (which the chain never hits), the holder or ring melee; a point also beyond the 8 yd hop from every
  body lay 15.5-18.8 yd out, and in only 3 of 13 sweeps did one fixed point stay both for the whole sweep
  (1 at Heroism's 8.9 yd).
- **Melee reach**: 7.1-10.2 yd from his centre, past the 9.0 yd where `reach melee` fires in most
  snapshots of two pulls of six. Reach is exempt from the tank anchor guard, so the waiting tank steps in;
  the positioning row walks him back only past 2.5 yd (its 1 yd is net of his 1.5 reach).
- **His side**: a taunt from it can leave him south-west of the tank spot, for most of the phase in four
  of fifteen pulls, where a ring anchored at his landing sits a slot on the holder; the ring
  [re-latches](#phase-2) for it.

Sif's Frost Nova lands farther off: her floor spots came 8.1-9.8 yd from it at nearest, the old spot
2.5-8.0.

Two related traps: **a victim change re-seats him at the chase near point and he never moves again**
(`ChaseMovementGenerator::DoUpdate`; in one pull he rested 7.4 yd off the tank spot against 4.1-5.9 in
the others, putting the outermost ranged slot 6.9 yd outside the melee ring), and whoever holds him
eats the Blizzard, which is why the dodge was towing him across the arena for **36,283 and 32,799** of
avoided damage against 11-38k from a single Thorim swing.

**A failed chase path makes him untouchable**, as it does
[Mimiron's MK II](mimiron.md#an-mk-ii-that-cannot-reach-its-tank-is-immune-and-heals): the core flags
his victim unreachable (`Creature::GetCannotReachTarget`), every hit and taunt on him evades, and 5 s
later he regenerates. The flag clears only on a chase move that dispatches or a cast with a cast time
(Chain Lightning), never while the victim stands still in his reach. Once in 31 phase 2s: the corridor
tank dragged him onto the tank spot at 2.4 yd, and from +16.5 to +41.7 s he took nothing, went
97.8 → 100% and evaded three swap taunts, about 29 s of the race. A bot the flag has named for
`ULDUAR_THORIM_UNREACH_GRACE_MS` (1 s), on the floor, steps out of his chase range
(`thorim unreachable step action`, which both movement guards pass): both reaches plus
`ULDUAR_THORIM_UNREACH_STEP_MARGIN` (4) from his centre, 11.75 yd against the core's 8.25, straight
away from him or turned in 30° steps until the point, pulled back onto the navmesh, still stands
9.75 yd from him and within `ULDUAR_THORIM_UNREACH_ARENA_RADIUS` (30) of the arena centre, clear of
Blizzard where one is. The disc is not the floor: under the balcony, the side he and the corridor
squad come down on, no trace has a player on the floor past 22.5-26.6 yd from the centre. The chase
re-paths, and the positioning row walks it back.

**The anchor is also the Chain Lightning answer.** Boss 18-28 yd off the anchor: 8 targets, **174,932**
damage running 4,695 → 47,024, out of the melee ring and into the ranged camp, **four bots dead in
0.1 s**. Boss on the anchor: 8 targets, **141,198**, all melee and tanks, **zero deaths**. What decides
survival is where the chain *ends*.

**The generic movers fight the positioning row over a tank.** `ThorimMeleeRingSettled` is false for
every role but `MeleeRing`, so the `thorim movement guard` never gates a tank: `avoid aoe` aimed **70-74**
orders a pull at the bot tank and `tank face` **15-30**, alternating with the positioning row
**153-176** times over a median **7.6-7.9 yd** gap between their destinations. `thorim tank anchor guard`
closes it, holding a tank that is within 3 yd of its own spot and keeping `AvoidAoe` **out** of the pass
mask. That only enforces what `ThorimSifBlizzardTrigger` already decides, since it refuses both tank roles
in phase 2 on the grounds that a tank's 30 yd flee tows the boss into the ranged camp. One oscillating
tank took its destinations from 1 distinct point to 10 and towed the ring into itself (minimum pairwise
slot gap p10 60° → 10°), and Chain Lightning's worst cast from 6 victims for 119,057 to 8 for
**190,060**, whose 69,788 top hit killed two melee.

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
(2108.7, -280.04), walks the core's eight fixed waypoints once at ~7 yd/s (run pace), west then north of
the tank spot and on round the camp, and stands on the last until it despawns at 30s. Its aura
(62577/62603) drops a 10s, 8 yd zone (62576 10-man, 62602 25-man) every 2s: up to six live, a median 26
yd behind it. **Test the zones
(`GetDynamicObjectPositions`), not the bunny**: the bunny alone missed 16 of 20 melee hits in one pull.
`ThorimBlizzardSpots` is the live zones plus the bunny, which marks where the next zone drops within
2 s; its sweep reaches `ULDUAR_THORIM_BLIZZARD_SCAN_RANGE` (70 yd), since every bot reads whichever
bot's sweep filled the cache and the bunny's path spans 61 by 48 yd. Every camp home spot clears the
loop by 12.9 yd or more.

**The two Blizzard radii mean different things.** `ULDUAR_THORIM_SIF_BLIZZARD_RADIUS = 15.0` is the
generic dodge's **trigger** radius, not a damage radius; at 12 the flee stopped while the bot still stood
in the zone. The damage-side constant is `ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE = 11.0`, the 9.8 yd
reach measured over 161 ticks (corrected DBC 8 plus both combat reaches through `IsWithinDistInMap`)
plus a yard. 15 instead grew the ring's old slide from a median 3-7 yd of arc to 9-12 and bought nothing.
`ThorimPhase2PositioningTrigger` stands down while `ThorimSifBlizzardTrigger` is active, as the arena
anchor does: without it the anchor walked bots back into the zone they had left, and the four ranged who
took the most Blizzard were the four who ran furthest.

**That dodge excludes the melee ring outright**: the same node steps ring melee out
([how](#a-covered-slot-steps-out)). Ticks are **4,300-4,900 median, 6,187 max**; melee left standing in
zones took **6-14 consecutive**, one bot 14 for **72,853** over 11 s, **320k and 457k** in two wipes, more
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
199 and 246 attempts, 161 and 204 were refused by the movement gate anyway. Melee take the nearest
clear point instead, and tanks are exempt **by role**, since a holder check alone drops out for the half
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
Colossus, then the Rune Giant, **one pack at a time**: while a guard, gauntlet Acolyte, Colossus or Giant
is in combat in its 50 yd sweep, the idle ones drop out (`DropIdleGauntletMobsWhileFighting`). Tiers alone
handed DPS the second corridor pack's Acolyte 39-50 yd out the moment the first pack's died; that or the
bot leader walking on mid-fight (**Who leads the gauntlet**, below) opened the second pack on the first's
guards in eight of nine 25-man pulls traced, and four Iron Ring Guards against one tank killed ranged with
Impale and Whirling Trip. In the ninth a hunter's Disengage left him 22 yd from it and it turned on him,
which neither gate stops. Each loss carries through the gauntlet: two dead ranged made a Colossus 40 s
against 24 s.

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

**No gauntlet hunter revives a pet before phase 2** (`thorim revive pet hold`: gauntlet squad during the
split). Revive Pet is a ~9 s cast standing still, and the leader's 30 yd step hold and the Colossus
gather wait for him: one revive held the first step 6.7 s at the lever, another the pull ~10 s. A hunter
whose cast completes with no pet back (`pet dead` reads a `character_pet` row) recasts it all fight. Arena
hunters and phase 2 keep it, off the clock.

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

**On those rosters the race is lost every time**: all 15 hard-mode phase 2s traced wiped. From +30 s to the
first tank death the raid takes him **0.41-0.55 %/s**, and that tank falls at **+82-196 s** with him at
**26-65%**, to dry healers or to swings the stacks lift from a median 7-9k to 17-18k by +120-150 s, three
landing in 1.8 s; only a Chain Lightning since fixed (+57 s) and a raid already collapsed (+48 s, 0.39 %/s)
dropped one sooner.
The cleanest, all 19 DPS alive to +120 s and every strike swapped in 0.1-0.3 s, still had him at **35.2%**
when the first tank fell at +134.9 s: a kill by then needs ~0.74 %/s, about 1.5x.

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

**Pets never chain.** 62131 and 64390 carry `SPELL_ATTR3_ONLY_ON_PLAYER` (AttributesEx3 0x100): the
chain search takes players only (`Spell::GetSearcherTypeMask`) and `SpellInfo::CheckTarget` fails any
other unit, so pets, guardians and totems take no hop, and a seed pick on one fails `CheckCast` while
the event still repeats in 15 s. So the trace's chains are complete: hop by hop to the nearest unhit
player within 8 yd reproduces 360 of 446 hard-mode chains' group order, with pets as bodies 281.

`ThorimPetsOffRingSlot` (on the pet leash node) still chases pets onto a slot bearing, a guard against
hops they cannot take; it costs no uptime, since at ring radius a pet is still inside the 9.1 yd melee
reach on any bearing. Army and Feral Spirits steer too, though a player cannot command them
([pitfalls](../../engine/pitfalls.md#things-the-bot-cannot-see)). Only pets already on him are steered:
one on an add is doing its job, and one out of the room is the leash's, which runs first and shares the
throttle, so a pet just sent home isn't redirected. The chase is range 0 like PetAI's, with no command
or stop, within `ULDUAR_THORIM_PET_SLOT_TOLERANCE` (22.5°, about 3 yd of arc), its bearing converted
from his live facing every time, since a chase angle is relative to it and he turns with his tank. A
ring melee's pets take its slot, everyone else's one off its guid counter modulo 3; human-owned pets
are not steerable, no `PlayerbotAI`.

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
- **Lever:** a bot leader walks to the lever (GO 194264, (2173.3, −252.9)) at the pull, once no start-pack
  mob attacks him, and waits there while the main tank holds the pack (`thorim lever wait` holds his pickers,
  taunts, charges (no mover to the rules) and every mover but the lever walk); the moment it loses
  `GO_FLAG_NOT_SELECTABLE` (six start-pack deaths), still `GO_STATE_READY`, he sends `CMSG_GAMEOBJ_USE`.
  Walking there after the sixth death cost 4.5-7.7 s of Sif's clock a pull. A real player leading pulls it
  himself. Followers gather on a leader at the gate, except a non-healer whose target is a live start-pack
  mob within 50 yd (`ThorimFightingStartPack`), which also skips the 25 yd tether while he walks there: it
  finishes the pack, or it would shuttle between pack and gate.
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
  at most 15 s per step, so a stuck bot cannot eat the 150 s clock. He also holds the step and the Colossus
  walk while he fights (`ThorimGauntletLeaderFighting`: his target is a gauntlet mob in combat within 50 yd,
  or one attacks him): every corridor step of nine 25-man pulls came mid-fight, leaving the first pack's
  guards on ranged, and the right lane's fifth waypoint, 20.0 yd from the second pack's west guard, pulls
  that pack. The Giant walk never holds (below). The last waypoint pair (user, 2026-11-23; placed from
  trace positions), (2237.2, −360.5) and (2219.5, −360.57), is the pull line: 36.5-37 yd from the
  Colossus, `GetDistance` 29-29.5 (his combat reach ~6). Farther than the 30 yd pull test, he walks on to
  29 yd, a yard (`ULDUAR_THORIM_LEAD_COLOSSUS_STOP_MARGIN`) inside it: a stop on the test itself lands a
  hair either side, and one left just over it re-issued its own point as a duplicate and stood 5.2 s
  until a DPS pulled. There, once no Iron Ring Guard or corridor Acolyte lives within 50 yd and every squad
  non-healer stands within 20 yd of him (`ULDUAR_THORIM_LEAD_COLOSSUS_GATHER_DISTANCE`, under the step's
  15 s cap), he attacks him (`ThorimGauntletColossusPull`, through the encounter picker) and walks on into
  him until he is in combat, which also ends the Runic Smash; no trace shows his proximity aggro, as
  nobody got within 32 yd of him idle. Waypoint and gather put the squad in cast range at the pull (the
  step's 30 yd passes with trailing followers 25 yd behind); with the lanes ending at y −329 it stood
  ~69 yd out and walked in after the pull, under 10k/s on him for 6-9 s. No follower opens him idle
  (`GetThorimDpsTarget` offers him out of combat only through `ThorimGauntletColossusPull`): the pull line
  is inside their 50 yd pick, and a real player leading pulls him himself; waiting for a follower's pick
  cost 1-10 s a pull. Once the Colossus is dead he
  walks on to the Rune Giant (`ThorimGauntletLeadRuneGiant`), ~100 yd up a ramp and past the 50 yd pick
  range: in all nine bot-led pulls of 2026-11-22 the squad otherwise stood at the dead Colossus until the
  orb. Gated on the first doors (GO 194557, (2227.7, −413.3)) standing open, which the Colossus' death does
  in both sizes. The Giant's `UNIT_FLAG_IMMUNE_TO_PC` is on the 10-man template only
  ([pitfalls](../../engine/pitfalls.md#detection-that-never-fires)): gated on it, all three 25-man
  leaders walked past an untouched Colossus, shut doors and all, into the Iron Honor Guards and the Giant
  (24 deaths in the two pulls that wiped). The Colossus lookup can't gate it either: it reads empty until a
  bot within 150 yd of him has found him
  ([Runic Colossus](#runic-colossus-32872-spawned-at-22275--396179-412176)). Also gated on the leader past the
  portcullis and within 150 yd of the Giant's spawn, since a scan from farther out (the arena, the left
  lane's end at ~165 yd) misses and that cached miss reads "Giant dead" to the balcony walk. Pathed to his
  own spot (a point on the straight line can land off the mesh) and all the way in, until he is in combat:
  once an Iron Honor Guard engages, the Giant spawns another every 10-20 s until he is pulled, and the squad
  picks guards first. **Once the Giant is in combat on him, he walks him back down** to
  `ULDUAR_THORIM_RUNE_GIANT_HOLD_SPOT` (2187.79, −432.78, 426.39) and holds him there, so squad and healers
  fight adds and Giant in one place (user, 2026-11-23). His walk up books a combat-priority movement lock
  (up to 5 s) toward the Giant's spawn, which refused the hold walk 2.7-3.0 s after the Giant engaged, so
  he ran 15-20 yd on to the spawn and back with the Honor Guards on him; the hold walk first lowers that
  booking to normal priority (`ThorimGiantWalkBooked`;
  [pitfalls](../../engine/pitfalls.md#movement-that-silently-no-ops)). Left at the top, the Giant stood
  40-62 yd from it while the squad killed the foot Honor Guard and the Acolyte at the ramp foot and then
  walked ~65 yd up: 84-101 DPS bot-seconds a pull without a target. The spot is 15 yd up the ramp from the
  median of the adds fought off the leader over fifteen 25-man pulls, (2202.6, −430.7, 418.8), and the
  Giant and players have stood on it. Stomp (62413: 10 yd around the Giant, a ~3 yd knockback every 8 s)
  pushes the tank down the ramp, so he walks back past 3 yd (`ULDUAR_THORIM_RUNE_GIANT_HOLD_TOLERANCE`) and
  the Giant settles just above the spot. While he holds the Giant he targets him (`ThorimGauntletGiantHeld`,
  through the encounter picker), and `thorim giant hold` keeps every mover but the lead walk, `tank assist`,
  the DPS pickers and charges off him: `tank face`, `reach melee` or a Feral Charge would drag the Giant into
  the adds. Taunts stay. Costs: Stomp and Rune Detonation (15 yd around a random player) land nearer the
  squad, and the balcony walk starts ~30 yd lower, +4 s from his death to the balcony hit. Followers, behind
  a bot or a real player alike, take the leader's waypoint, also from an earlier waypoint of the lane
  unless fighting: placing only bots off the lane left a squad on waypoints 0-1 31-43 yd behind a leader
  on 3, whose 30 yd hold stood 7.5 s until a Runic Smash dodge moved everyone. Between waypoints they walk
  to him once more than 25 yd (healers 20 yd, and only to 15 yd of him) behind, except a non-healer
  fighting, which means targeting a gauntlet mob in combat within the 50 yd pick range
  (`ThorimGauntletFighting`): that tether fought `reach melee` whenever he walked on mid-fight, 116-185 DPS
  bot-seconds a pull of moving without damage, three quarters of it on the Honor Guards and Giant. The
  gate walk re-adds follow-master only when the leader is the bot's master.
- **Clock:** the walk is about 57 yd from the portcullis to the first waypoint and 65 yd to the last, so
  roughly 20 s of running at 7 yd/s, against the ~28 s opening wait a human led squad accepts. The corridor
  fights set the clock. Measured on 25-man (fifteen bot-led pulls, before the lever wait and the Colossus
  pull): the lever pulled 4.5-7.7 s after the sixth start-pack death, leaving 163-165 s to Sif's 62565; from
  it, the walk to the trash 12-17 s, the trash 33-52 s, on to the Colossus 2-17 s, the Colossus 18-40 s, the
  ramp 5-10 s (the walk to the Giant starts 0.4-0.8 s after the Colossus dies), Honor Guards, Acolyte and
  Giant 43-63 s, his death to the balcony hit 15-20 s. Phase 2 came 156-204 s after the lever: hard mode in
  five pulls, 4.9-39.7 s late in the other ten. With both (three pulls, before the Giant hold): the lever
  0.1-0.2 s after the sixth death, so 170 s to 62565, and the Colossus engaged 0.5-1.8 s after the leader's
  stop, but the squad, held on the last lane waypoints ~69 yd out until then, walks in after the pull
  (under 10k/s on him for 6-9 s), so the Colossus took 31-36 s; phase 2 172-183 s after the lever, 2-13 s
  late. With the Giant hold (three pulls, its lock still refusing the walk down): the Giant p50 3-5 yd
  from the spot, but adds and Giant still 53-60 s, since the squad kills the adds first and the Giant
  alone then takes 20-23 s, and his death to the balcony hit 20.5-22.8 s; phase 2 179-186 s after the
  lever, 8.5-17 s late. Walk-down unlocked (three pulls): the leader on the hold spot 4.3-10.7 s after the
  Giant took him (12.2-15.1 before), adds and Giant 51-56 s; phase 2 162-207 s after the lever, one hard
  mode with 8 s to spare, the others 10.6 and 36.7 s late (the slowest lost 21 s in the corridor, 7.5 of
  them to the follower stall above). Pulling from the y −360 line (three pulls): squad non-healers within
  15.6-22.1 yd of the leader at the pull, the Colossus down 8-12% in the first 6 s (1.5-2.1% from y −329),
  then 20-38 s on Runic Barrier; phase 2 149-205 s after the lever, one hard mode with 21 s to spare, the
  others 14 and 36 s late, each losing 15-16 s in the corridor (12-13 Runic Smash hits against 1, two
  dead ranged, a second pack's Acolyte healing itself under 8 DPS for 22 s).

**The last 36 yd of the lower corridor are unwaypointed**: waypoints stop at the pull line, y −360.5
(the Colossus at y −396), so past it `ThorimRunicSmashAction` cannot resolve a lane index; the pull has
ended the smash by then. Between y −344 and −366 the floor narrows to x ~2216-2240, so the right lane's
last waypoint sits at x 2219.5, not 2212. The formation hits the same wall: trigger and action both
place the squad off `ThorimGauntletLaneIndex(leader, …)`, which needs the leader **standing inside a
waypoint radius**, so once he walks past the chain the row goes quiet for everyone. DPS do not notice, since
`reach melee` and `reach spell` chase their targets the rest of the way, but a healer chases nothing and
`follow` cannot cover it, being `BOT_STATE_NON_COMBAT` in a running fight. Measured: the 11 non-healers
reached **183-191 yd** out while the two healers stalled at **148 and 129**, one of them with **no move
order at all** across 22 s, leaving the human 68.8 and 101.1 yd behind against the 40 yd heal range and
his last outside heal 21 s before he died. `ULDUAR_THORIM_GAUNTLET_TETHER_DISTANCE` (20 yd) now walks a
trailing healer to 15 yd of the leader once the lane index fails
([Runic Colossus](#runic-colossus-32872-spawned-at-22275--396179-412176)); non-healers trail as in **Walk** above.

**The fall rescue still stops at 110 yd.** `ThorimFallFromFloorTrigger` bails past
`ULDUAR_THORIM_FLOOR_RANGE` (110 yd) from the arena centre, short of the left lane's last four
waypoints at **117.3 / 120.8 / 126.1 / 141.4 yd**, the right lane's last at **129.2** and the corridor's
far end, and it `NearTeleportTo`s onto the master, same map only
([pitfalls](../../engine/pitfalls.md#before-the-pull-and-out-of-combat)). The
gauntlet row no longer shares that gate: it reads `ULDUAR_THORIM_ENCOUNTER_PROXIMITY` (200), which
covers the whole corridor.

**Corridor bots never flee** (`thorim corridor flee hold`: gauntlet squad, past the portcullis, during the
split). The generic `flee` (`enemy too close for spell`, a hunter's `enemy too close for auto shot`: the
bot's target within its melee reach, held by someone else or rooted) steps 4-6 yd off a mob tanked on the
lane waypoint the squad shares, and the first two lane pairs sit 1-2 yd inside the floor's edges (x
2216-2240 at y −276, 2218-2238 at y −295). Over 36 pulls: 1072 corridor flees, two thirds by healers, 174
dipping under z 411. Inside 110 yd the rescue's z 410 is 2 yd under the corridor floor (412.1; the arena's
is 419.5), so a dip teleports the bot to the raid leader in the arena: 47 teleports in 14 pulls, each
13-22 s from its squad, once three of four ranged and both healers mid-pack. Farther out, flees west at
(2219, −354) and the ramp foot (2217, −423) sank to z ~390 and back over 6.5-7 s, and every deep sink in
46 traces (five, to z 388 down to −28, some never back) was a westward `flee` or barrier bail from x
2215-2220 aimed past the floor's edge at y −344..−361 (above). The barrier bail still can. Disengage stays:
in 58 corridor casts it sank no hunter without a flee after it. Not navprobed.

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

Documented, not implemented: a dodge of the in-combat `SPELL_SMASH` 62339 frontal cone (60 degrees, 3s
cast, a different spell from the corridor Runic Smash; healers stand clear by their trail,
[Runic Colossus](#runic-colossus-32872-spawned-at-22275--396179-412176)), Rune Detonation 62526, Stomp
62411, and Runic Fortification 62942. Heals on a freshly swapped-in tank are also thin (one took no heal at all in the
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

## What a trace answers

`tools/botobs/bosses/thorim.py <trace>`, reading `thorim.p2start`, `thorim.sifjoined` (hard mode),
`thorim.ringspot` (each ring bot's slot), `thorim.ringstep` (a ring bot stepped out of Blizzard),
`thorim.unreach` (whom his cannot-reach flag names), `thorim.lightningorb` (the lit orb, counted where it
turns from 0: any instance's trace opening or closing re-emits it unchanged), `thorim.rangedslot` (the
camp), `thorim.shelter` (a camp bot's home, row, cone step or chain wait),
`thorim.ringconehold` (a ring walk held for crossing the lit cone) and `thorim.ringchainwait` (a ring walk
or step held for Chain Lightning):

- `--deaths`: deaths by mechanic, phase 1 and phase 2 apart.
- `--phase2`: what the tank spots are judged on. Chain Lightning casts, hits on the tanks and each ring
  slot, tank-to-ring hops and deaths; tank deaths; his cannot-reach spans and step moves, and stretches
  of 3 s or more a tank held him without his health falling (old traces too); the pickup (when a tank
  first held him, his melee on non-tanks before it); the opening (first trade,
  the taker's distance off the tank spot, his farthest from it over the next 8 s, health lost per second
  over the first 30 s); Thorim's distance off the tank spot and facing off its axis; trades (his move
  across one, strike to swap, his time on a non-tank); the waiting tank's offset, time on the holder,
  distance from him, share past 9.0 yd and moves; who stands within 8 yd of each tank; ring spacing,
  front-arc share, uptime and moves; camp spacing; Blizzard, Lightning Charge and Frost Nova on holder
  and waiting tank; his health lost per second.
- `--blizzard`: per Sif Blizzard (its bunny's first snapshot to its last plus a zone's 10 s, cut at the
  next bunny), each slot point's covered seconds; ring bots within the 9.8 yd damage reach (seconds,
  stays, longest, ticks, damage); deaths; step moves, those whose point lay under 11 yd off the bunny's
  walk ahead or under a zone's 9.8 yd reach within 8 s, those whose walk crossed that walk ahead (the
  bunny's p50 path yards short of the crossing, ticks until the bot's next order), stepped bot-seconds,
  those past 9.0 yd of him and those within 8.5 yd of another living slot's bot or point; the walk back
  from a step's end to within 9.0 yd, ticks on the way. Traces without `thorim.ringstep` read all but the
  steps.
- `--chains`: each Chain Lightning hop by hop as groups (tank, camp, a ring slot, marked stepped on its
  step point or within 3 s of leaving it), the yards between groups, `(lust)` when both bodies carried
  Heroism or Bloodlust (8.9 yd jump), deaths per group; then deaths by the crossing that reached them,
  those before the first tank death, and crossings by kind, each kind also counted where it entered or
  left a stepped slot.
- `--shelters`: per lit orb with a cone 3-7 s later, the camp bots that walked to a row or a cone step,
  how many got within 2 yd of their last move's point, and each one the cone hit: walk, yards short,
  Sif's slows on it from 4 s before the lit, death; camp bots hit off a walk; then totals and those
  before the first tank death.
- `--ringcone`: per lit orb with a cone 3-7 s later, each ring bot the cone hit and what it was doing:
  unable to move (a `blocked` move), walking (an accepted move since the lit, naming the last mover),
  held or standing; past 12 yd of him; death; the walks held; then totals and those before the first
  tank death. Traces without `thorim.ringconehold` read all but the holds.
- `--colossus`: per Runic Colossus Smash, each healer within 20 yd of him: yards from him and off his
  facing, yards from his target, its last accepted move's mover, hit or death; then totals, with those
  within 6 yd of his target.
- `--chainwait`: per lit orb 1 or 2 (indexed by the orb's position), the chain's camp hops and deaths and
  the walks that waited for it: released by the landing or the cap, waiters the chain hit standing or
  walking, cone hits and deaths on them, steps after; then totals, with chains of 4+ camp hits and camp
  chain deaths, which traces from before the wait read too.
- `--ringwait`: per lit orb, the ring walks and steps that waited for Chain Lightning: released by the
  landing or the cap, waiters the chain hit standing or walking, cone hits and deaths on them; then
  totals with wait p50 and max, deaths also before the first tank death. Traces without
  `thorim.ringchainwait` say so.
- `--spree`: each Killing Spree (aura 51690) a ring bot started in phase 2: seconds after the last lit
  orb, the Chain Lightning that hit the rogue from 0.3 s before it to 5.1 s after (its hop, whether that
  hop crossed groups, deaths in other groups past it), a cone hit in that span; then totals, with those
  started in the hold's 5.5-9.0 s window.

Per snapshot the holder is the tank he hits and every other living tank on the floor waits; spacing
counts from 15 s after the drop. Damage rows come in hop order (each hop does 1.5 times the last), every
hop a player: a gap past the jump is snapshot lag.
