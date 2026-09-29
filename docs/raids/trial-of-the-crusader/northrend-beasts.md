# Northrend Beasts

One continuous fight in three stages. Raid-wide facts and the code layout: [README.md](README.md).
Mechanics below are from `boss_northrend_beasts.cpp`, `instance_trial_of_the_crusader.cpp`, the DBC
and `SpellInfoCorrections.cpp`.

## Stages

- **Normal is kill-driven.** 2.5 s after Gormok dies the worms' scene starts, and they attack 15 s
  into it; 2.5 s after the second worm dies Icehowl's does, and he attacks 13 s into it. Nothing is in
  combat during a walk-in, so `ToCEncounterIsLive` drops between beasts.
- **Heroic runs on timers**, counted from the intro's end (stage 1): Gormok attacks at 11 s, the worms
  at 165 s (scene at 150), Icehowl at 353 s (scene at 340), and Berserk 26662 lands on every live
  beast at 520 s. A kill before its timer reschedules that scene 2.5 s out; a timer that fires first
  stops the kill from summoning. So a slow stage overlaps the next beast, and the tank model has to
  hold Gormok with the worms, or the worms with Icehowl.
- Stage 2 is set only once all three are dead.
- All four beasts carry `CREATURE_FLAG_EXTRA_OBEYS_TAUNT_DIMINISHING_RETURNS` (0x80000): repeated
  taunts on one beast diminish.

## Stage model and tank duties

`Util/ToCHelpers_NorthrendBeasts.{h,cpp}` reads the world once per instance per ms into a
`RaidInstanceState`, so every bot gets one answer. Nothing resolves off map 649.

- **Beasts.** Gormok, Dreadscale and Acidmaw by `GetGuidData` 4, 6, 7; Icehowl by entry within 200 yd
  of the reading bot, his guid cached while it resolves and re-searched at most once a second
  (he walks in for 10 s first). Engaged = alive and in combat. The mask of engaged beasts,
  1 Gormok, 2 a worm, 4 Icehowl, is `nb.stage`.
- **Worm form** by native display id (`IsWormMobile`; Submerge's transform shows an invisible
  model). Under ground (`IsWormSubmerged`, `NOT_SELECTABLE`) a worm reads the form it comes up in:
  every emerge swaps the form, and a duty dealt by the display would send each holder to the wrong
  worm for 8.5 s. While the other is still up, it reads the opposite of that one's display (the two
  always differ), so the duties swap once both are under, not at each worm's own submerge. With
  neither mobile, Acidmaw counts as the mobile one. A lone survivor is the mobile one whatever its
  form, and reads mobile under ground once enraged: the script sends it under at once and it comes
  up mobile.
- **Duties**, in priority order `Gormok`, `Icehowl`, `WormMobile`, `GormokSwap`, `WormStationary`,
  each only while its beast is engaged (`GormokSwap` with Gormok).
- **Roster**: living bot tanks on the instance, by strategy or spec, the group's flagged main tank
  first, then by guid.
- **Pass 1** leaves a duty on its beast's current victim when that is a tank (bot or human) holding
  no duty yet, so a swap or a form change moves only the tanks that must move. A human gets a duty
  only this way: bots can't hand him one.
- **Pass 2**: each unfilled duty, in order, takes the first free roster tank; with none left, the
  bot on the lowest-priority filled duty below it, skipping any a human holds.

One deal covers 10- and 25-man tank counts and the heroic overlap, where `IsMainTank` and the assist
index gave the main tank Gormok and the mobile worm at once. The hold, swap and worm-hold triggers
key on their duty.

- **`northrend beasts taunt guard`** zeroes a class taunt (`IsTauntAction`) while the encounter is
  live and the bot's current target is a beast whose duty holder is someone else: a generic taunt
  fights the deal and burns diminishing returns ([thorim.md](../ulduar/thorim.md)). The encounter's
  own taunts cast inside `Execute` and pass.
- **`northrend beasts control tank movement multiplier`** zeroes `CombatFormationMoveAction` for a
  tank whose victim is an engaged beast.

## Arena bounds

navprobe cannot verify an arena point ([README.md](README.md#arena-floor)), so the script bounds the
floor: Icehowl lands 50 yd from the centre (46 toward the main gate, north, +y), the gate front
sits 38.8 yd north, and the worms surface 10-35 yd out in any direction.

## Gormok the Impaler

Combat reach 6.5, so melee stand within 9.3 yd of his centre.

- **Impale** (66331 / 67477 / 67478 / 67479) on his victim every 9-10 s (2.5 s retries while
  disarmed). Stacks to 10; duration 30 / 40 / 30 / 45 s, refreshed by every application, so stacks
  never decay while tanking and drop all at once a full duration after the last hit. Bleed per stack
  per 2 s: 1.4-1.8k / 2.2-2.8k / 2.6-3.4k / 3.9-5.1k. **A taunter still carrying Impale resumes from
  its stacks**, so a swap resets nothing until the taunter's has expired — at that cadence the holder
  is at ~3 stacks on 10-man and 4-5 on 25-man by then.
- **Staggering Stomp**, first at 15 s then every 20-25 s: the script casts 67648, the 10H member of
  row 66330 / 67647 / 67648 / 67649, which remaps like any other. 0.5 s cast, 15 yd damage and a 20 yd
  8 s school lockout, both centre to centre. Melee eat it; casters stand past 20 yd.
- **Snobold Vassals** (34800): four on his seats on every difficulty (`vehicle_template_accessory`).
  Every 16-24 s he takes one in hand and 2.5 s later seats it on a random alive player, any role, not
  mounted, on a vehicle or carrying one already, stacking Rising Anger 66636 on himself (+15% damage
  each). The rider carries Snobolled! 66406 from 1.5 s after (removed when the snobold dies) and takes
  Batter 66408 every 6-8 s (5 s lockout) and Head Crack 66407 every 30-35 s (2 s stun). A rider's
  death sends the snobold back to a free seat at full health, to be thrown again. When Gormok dies
  the seated snobolds despawn and the riding ones live on.
- **Fire Bomb**: only a snobold still on Gormok throws, every 20-30 s, at a random alive player outside
  his melee range. NPC 34854 (`TEMPSUMMON_TIMED_DESPAWN` 60 s, so its age is `60000 - GetTimer()`)
  appears at the player, and the snobold fires two 14 yd/s missiles at it: the triggered aura 66318
  (`SpellInfoCorrections` gives it the speed) lands after the flight, the 1 s cast 66313 a second
  later, when 66317 hits 8 yd round the NPC, centre to centre (4.8-6.2k, no difficulty row). Impact
  is thus `1 s + distance / 14` after the spawn, measured from the snobold's seat: 1.7 s for a player
  just outside his melee, 2.8 s at 25 yd. 66318 then pulses 66320 (67472 / 67473 / 67475) every
  second, the first tick with the impact, at **2 yd** — `SpellInfoCorrections` cuts the DBC's 5 — and
  on heroic adds a stacking 1k / 2k DoT per 2 s (20 s). The NPC (combat reach 1) is non-selectable
  with that aura, so the generic `avoid aoe` sees it from the aura's landing, fires within 4.5 yd
  (2 yd plus both reaches) and steps 3 yd: it flees the pulse, never the impact.

**What the bots do.**

- **Hold** (`gormok tank duty` → `gormok tank hold boss`): duty `Gormok`; skull and RTI; class taunt
  only while his victim isn't a tank; `DragBossToAnchor` toward `ARENA_CENTER`, except while an
  Icehowl charge is latched: every charge line crosses the centre.
- **Swap** (`gormok tank swap needed` → `gormok tank swap taunt`): duty `GormokSwap`, his victim the
  `Gormok` holder at ≥ 3 Impale stacks (`GORMOK_IMPALE_SWAP_STACKS`), the bot at 0; a failed taunt
  attacks him. 3 on every difficulty (guide: 3, heroic 2-3), and only once the bot's own Impale is
  gone, since a taunter resumes its stacks. `nb.swap`.
- **Defensive** (`gormok tank defensive`): the `Gormok` holder he is hitting, at ≥ 5 stacks or ≥ 3 with
  no `GormokSwap` holder, casts `NextTankDefensive` (guide: rotate defensives at high stacks).
  `nb.defensive`.
- **Snobolds** (`gormok snobold on raid` → `gormok focus snobold`): one pick per DPS, ranked by rider
  healer > ranged > melee > tank, then snobold guid. Healer and ranged riders' snobolds are for every
  DPS, melee and tank riders' for melee only; once Gormok isn't engaged, every rider's for every DPS
  (guide: free healers first, both DPS roles attack them). A candidate is alive and rides a living
  player: the seated ones never count, and a dead rider's snobold returns to Gormok. No raid icon,
  since marks are never cleared in a pull ([pitfalls.md](../../engine/pitfalls.md));
  `GormokSnoboldTargetGuardMultiplier` zeroes `dps assist` for a bot with a pick, so it can't flip
  back to the skull. `nb.snobold`.
- **Carrier** (`gormok snobolled` → `gormok bring snobold to melee`): a ranged DPS or healer carrying
  one while Gormok is engaged walks to 10 yd of his centre for melee to free it (guide);
  `GormokSnoboldCarrierMultiplier` holds its other movers until the snobold dies, `blink` and
  `disengage` included (both fire that close to him), except `AttackAction`, `avoid aoe` and every
  Beasts-prefixed node, so a heroic carrier still dodges the worms and Icehowl's charge.
- **Stomp** (`gormok stomp range` → `gormok leave stomp range`): a ranged DPS or healer with no
  snobold inside 22 yd of his centre steps out to 24, near his victim: the lockout is 20 yd centre to
  centre, and `spellDistance` 28.5 alone doesn't keep casters past it. Its spot also clears young
  bombs (below) by 11 yd where one can, so stomp and dodge don't trade a caster until the impact; a
  bomb landing within 11 of that spot mid walk re-plans it.
- **Fire Bomb** (`gormok fire bomb incoming` → `gormok dodge fire bomb`, `ACTION_EMERGENCY + 2`):
  - **Model**, per instance: a grid scan every 200 ms while Gormok is engaged or a bomb is young:
    short of the impact above plus 0.5 s, the distance taken on first sight from its summoner, else
    Gormok, else a flat 5 s.
  - **Who**: a living bot within 9 yd (2D) of a young bomb, unless an engaged beast is hitting it:
    moving his victim drags the beast and its melee, and 5-6k on a tank is healable.
  - **Where**: the nearest spot within 20 yd clear of young bombs by 11, landed bombs by 6 (past
    `avoid aoe`'s 4.5, whose flee can step back into a young bomb's trigger) and, for a ranged DPS
    or healer with no snobold, Gormok by the stomp's 24. Ties go near Gormok for melee DPS and
    carriers, else near his victim. With no spot it drops Gormok's circle, then the landed bombs,
    then takes young bombs alone at 9.5 (`tight`). 66317 is a creature's 8 yd area, centre to
    centre, and a dodge lands past its own trigger ([pitfalls.md](../../engine/pitfalls.md)).
  - **`MOVEMENT_FORCED`**, to beat a combat walk in flight (stomp, carrier, reach, `avoid aoe`)
    inside the 1.7-3 s budget; its own walk in flight is left alone (`hold`) while its spot clears
    every young bomb by 9.5. `icehowl charge guard` still zeroes it. `nb.bomb`.
- All three walks wait out a pinning cast rather than interrupt it (none is lethal), and re-issue one
  that Head Crack or a channel stopped short ([pitfalls.md](../../engine/pitfalls.md)).

## Acidmaw & Dreadscale

Combat reach 6.5, so melee range is 9.3 yd; they run 9.45 yd/s, faster than a player. Dreadscale
(34799) walks in mobile, Acidmaw (35144) comes up stationary by the gate. Timers restart at every
emerge.

- **Mobile**: chases its victim, melee every 2 s.
  - Bite on the victim, first at 20 s (Acidmaw) or 15 s (Dreadscale), then every 20 s.
  - Spew, first at 15-30 s then every 15-30 s: a 1 s cast (66818 / 66821, no row), then a 2.5 s
    aura ticking every 250 ms in a 55 yd cone along his facing, which follows his victim: **60° on
    10N** (`spell_cone`), 24° on the other three (`TARGET_UNIT_CONE_ENEMY_24`). Ten ticks.
  - Slime Pool, at 15 s then every 30 s: NPC 35176 at his feet for 30 s. Its aura 66882 ticks every
    second and `SpellAuraEffects.cpp` casts each tick with a radius mod, so the pool **grows**:
    `2 + 0.3 × tick` yd, 11 at the end, centre to centre. `avoid aoe` reads the DBC's 30, past
    `MaxAoeAvoidRadius` 15, so it skips the pool.
- **Stationary**: rooted, Spit on its victim every 1.5 s swing (1.1 s cast).
  - Spray, first at 20 s (Acidmaw) or 15 s (Dreadscale), then every 20 s: a 1.1 s cast at a random
    player within 100 yd, landing by 30 yd/s missile on everyone within 10 yd of that spot (centre to
    centre).
  - Sweep, first at 15-30 s then every 15-30 s: 1.5 s cast, a **15 yd circle round the worm**
    (`TARGET_UNIT_SRC_AREA_ENEMY`, centre to centre) with a knockback, not the guide's "behind the
    stationary worm": everyone in melee range eats it.

| Spell | 10N | 25N | 10H | 25H |
|---|---|---|---|---|
| Paralytic / Burning Bite | 66824 / 66879, 7.9-9.1k | 67612 / 67624, 11.1-12.9k / 13-15k | 67613 / 67625, 13-15k / 11.1-12.9k | 67614 / 67626, 18.5-21.5k |
| Acidic / Molten Spew tick | 66819 / 66820, 2.8-3.2k | 67609 / 67635, 3.7-4.3k | 67610 / 67636, 3.7-4.3k | 67611 / 67637, 4.6-5.4k |
| Slime Pool, per second | 66881, 5.1-5.9k | 67638, 5.1-5.9k | 67639, 6.5-7.5k | 67640, 8.3-9.7k |
| Acid / Fire Spit | 66880 / 66796, 5.1-5.9k | 67606 / 67632, 6.5-7.5k | 67607 / 67633, 6.9-8.1k | 67608 / 67634, 10.2-11.8k |
| Paralytic / Burning Spray | 66901 / 66902, 6.9-8.1k | 67615 / 67627, 6.9-8.1k | 67616 / 67628, 8.3-9.7k | 67617 / 67629, 13-15k |
| Sweep | 66794, 6.9-8.1k | 67644, 6.9-8.1k | 67645, 8.3-9.7k | 67646, 10.2-11.8k |
| Burning Bile pulse, per 2 s | 66870, 3.2-3.8k | 67621, 3.2-3.8k | 67622, 5.6-6.5k | 67623, 8.3-9.7k |
| Paralytic Toxin tick | 66823, 3k / 2 s | 67618, 3k / 2 s | 67619, 3k / 1.5 s | 67620, 5k / 1.5 s |

**Submerge.** 45-50 s after each emerge one worm submerges and the other follows 1.5 s later, never
once enraged. From the start of a 2 s cast (53421, a stun) the worm is `NOT_SELECTABLE|NON_ATTACKABLE`.
2.5 s in it moves under ground to 10-35 yd from `ARENA_CENTER`, on the far side of the centre from
where it went down, and `DoResetThreatList` zeroes its whole threat list. 6 s later it comes up in the
other form, attackable at once, casting Emerge (66947) for 3 s before it acts. A stunned creature takes
no heal assist threat (`ThreatManager::ForwardThreatForAssistingMe`), so it comes up on whoever
threatens it first: DoTs, which keep ticking under ground (guide), or the first heals. So the forms
alternate: Dreadscale mobile and Acidmaw stationary, swapped after every submerge. The Churning Ground
trail (66969) is a visual only.

**Toxin and Bile.** Bite and Spray carry no aura: Bite triggers the debuff (`EffectTriggerSpell`),
each Spray id links it in `spell_linked_spell`. Acidmaw's give Paralytic Toxin, Dreadscale's Burning
Bile, so the mobile Dreadscale keeps Bile on its tank, the mobile Acidmaw Toxin on its tank, and the
stationary one sprays its debuff on a random player and everyone within 10 yd.

- **Paralytic Toxin** (row above), 60 s, no dispel type: a snare that starts at 10% and deepens 10% a
  tick (`SpellAuraEffects.cpp`) to a full stop after 18 s, 13.5 s on heroic. A re-application
  recalculates the aura's amounts, so every Bite restarts the ramp.
- **Burning Bile** 66869 (24 s, no row) casts its pulse every 2 s on every ally within 10 yd, the
  carrier and pets included, exact distance (a player caster adds no reach against players). A
  `spell_linked_spell` row strips Paralytic Toxin from each one it hits, so a Toxin carrier within
  10 yd of a Bile carrier is cured on the next pulse.

**First death.** The survivor gets Enrage 68335 (+50% damage done, permanent) at once and never
submerges again: the 10 s in `instance_trial_of_the_crusader.cpp` only times the achievement. A
stationary survivor submerges 1 s later and comes up mobile, one under ground comes up mobile, a
mobile one stays so. A lone Acidmaw's Toxin on its tank has no Bile left to cure it.

**Heroic** has no worm branch of its own beyond the ids: they attack on the 150 s timer with Gormok
alive or not, and Berserk lands at 520 s. Health each before `mod-dungeon-scale`: 1.26M, 5.02M,
1.67M, 6.69M (the trace's `unit` row has the real figure, [pitfalls.md](../../engine/pitfalls.md)).

**What the bots do.**

- **Kill target**: the mobile worm (skull), Dreadscale first and the other after every swap. Guide:
  kill Dreadscale first, or Acidmaw if Dreadscale won't die before the submerge; script: every melee
  on a stationary worm eats Sweep, and a 25-man bot raid rarely kills Dreadscale in its first
  45-50 s. No DPS balance: the survivor enrages the moment the first worm dies, and the 10 s only
  times the achievement.
- **Holds** (`northrend worms mobile tank duty` → `northrend worms tank hold mobile worm`, `… stationary
  tank duty` → `… tank hold stationary worm`, `ACTION_RAID + 1`) take the worm from `GetBeastOfDuty`.
  The mobile hold points RTI skull at its worm always but marks it only while Gormok isn't engaged,
  so a heroic overlap doesn't trade the skull with Gormok's hold. The stationary worm gets no mark:
  nothing reads one, and marks never clear ([pitfalls.md](../../engine/pitfalls.md)).
  - Under ground, the mobile holder closes to 20 yd and the stationary one into melee of the spot
    (it's rooted), at `MOVEMENT_COMBAT`, re-issued only once the worm stands 3 yd off where it was at
    the last issue (it moves once, 2.5 s in): the worm walks to its taunter
    ([xt002.md](../ulduar/xt002.md)). Not while an Icehowl charge is latched: the charge guard spares
    attack actions. `nb.wormmove` `approach`.
  - Up, a holder the worm isn't hitting taunts, then attacks. There's no swap duty, and pass 1 gives
    each tank the worm it already holds, so no holder taunts another's worm. The taunt waits for
    cooldown and range: `CastSpell` turns the bot to its target even when the cast fails.
  - Duties across a submerge stay with the deal, no per-worm latch: the threat wipe keeps the old
    victim until someone gains threat, and a stunned worm takes no heal assist threat, so pass 1
    keeps a tank on its worm unless a DoT took it.
  - A lone living tank holds only the mobile worm: nothing ranks below `WormStationary`, and the
    guide lets any ranged class tank the stationary one.
  - **Drag**: the mobile holder, while its worm's victim and no Icehowl charge is latched, walks the
    worm on once melee at it would stand in a pool 5 s on (the worm within that radius + 9.3) or in
    Sweep (within 27.3 of the stationary worm). It takes the clear spot nearest a point 10 yd ahead,
    searched within 30 yd, about 31 yd off every pool and 38 off the stationary worm (else off pools
    only), latched until it arrives. Both clearances carry a 2 yd pad, so the worm, stopping 8.5 yd
    behind its tank, settles past the trigger instead of firing it again. Walking on keeps the worm's
    front pointing the same way (guide: drag him off the pool so melee can attack). `nb.wormmove`
    `drag`.
- **Redirect** (`northrend worms redirect threat`, `ACTION_RAID + 1`): rogues serve the `WormMobile`
  holder, since Tricks moves the threat off whatever the rogue hits and melee hit the skull. Living
  hunters alternate by index among hunters, even to `WormMobile`, odd to `WormStationary` (mobile
  without one). It fires while that holder lives and the worm is under ground or hitting someone
  else, or while a hunter holds Misdirection charges (35079), spent on its worm, not the rotation's
  skull. Misdirection and Tricks go out from the submerge, the shots wait for the emerge.
  `northrend worms redirect guard` zeroes the generic main-tank Misdirection and Tricks while a worm
  is engaged. The threat wipe is the moment and the holders the targets
  ([README.md](../README.md#threat-redirect--the-rubric): multi-tank assignment at a phase
  transition).
- **Reposition** (`northrend worms misplaced` → `northrend worms reposition`,
  `ACTION_EMERGENCY + 6`): one mover per bot for every worm hazard and the cure walk, since two
  movers clear each other's MotionMaster ([xt002.md](../ulduar/xt002.md),
  [pitfalls.md](../../engine/pitfalls.md)). Holders sit it out unless they're runners; everyone does
  while an Icehowl charge is latched. First match:
  - **Run**: a runner past 8 yd of its stuck carrier. **Cure**: a Toxin carrier, neither stuck nor
    carrying Bile, past 8 yd of the nearest Bile carrier. Both walk to 6 yd of the partner (the
    pulse reaches 10 exact), clear of pools and, but for the `WormStationary` holder, Sweep. When
    the ring search misses that disc (its rays fan 22.5° apart), they walk straight to 5 yd of the
    partner if that spot is safe. Their spots clear only the real Spew cone, and a pair within 8 yd
    triggers on it unpadded: the partner is often a mobile worm's tank, 8.5 yd ahead of it, and on
    10N the 60° cone plus the 12° pad covers the whole 6 yd disc round it.
  - A **stuck** carrier, a holder or snared 70% or more, takes the nearest free runner: a bot
    Bile carrier (a human never answers) holding no duty, or a holder whose worm is under ground. A
    runner already within 8 yd serves that carrier, then last pairs hold while both qualify, so two
    runners don't trade carriers. Guide: free them while the worms are submerged; script: every Bite
    restarts the snare, so a mobile Acidmaw's tank is never cured otherwise.
  - Otherwise the first hazard the spot fails, centre to centre:

    | Hazard | Who keeps clear | Trigger / clearance | Why |
    |---|---|---|---|
    | Pool | everyone | radius now + 1 / radius 5 s on + 2 | the full 11 yd would keep melee off the worm for 30 s |
    | Bile | a bot without Toxin, of every other carrier; a carrier holding no beast and not sent as a runner, of everyone but Toxin carriers | 10.5 / 12 | the pulse's 10 yd exact; guide: spread from everyone, melee too |
    | Sweep | all but the holder, of a stationary worm that is up; a melee whose target isn't a worm (Gormok in a heroic overlap) only during the cast, since `reach melee` would walk it back | 16 / 18 | its 15 yd circle |
    | Spew | everyone, of the up mobile worm's cone, always | cone + 5° / + 12° | a 1 s cast leaves no time to clear 55 yd; guide: never stand in front of the mobile worm |
    | Spread | ranged and healers, of every player but a cure partner, while a stationary worm is engaged and both live | 9 / 11 | the Spray's 10 yd splash; a spread tighter than that buys nothing; guide `/range 10` |

  - Casters' spots also clear Gormok's stomp while he's engaged, so the two nodes don't alternate in
    a heroic overlap.
  - Every spot stays within 35 yd of `ARENA_CENTER` (no coordinates: navprobe can't see the floor)
    and outside the mobile worm's cone + 12°, its width from `GetSpellCone` on the remapped tick,
    else 24°. It prefers the role's reach: melee within 1 yd inside melee range of their target (6
    yd behind a worm), ranged within spell range of it, healers within heal range of both holders.
  - **Urgent**, inside a pool, without Toxin within 10 yd of a Bile carrier, inside a Sweep being
    cast or a Spew cone during the spew: `MOVEMENT_FORCED`; any other move waits a pinning cast out.
    A cure walk moves forced too, the rest at `MOVEMENT_COMBAT`
    ([pitfalls.md](../../engine/pitfalls.md): frequency decides forced ties; interrupt per
    mechanic). The walk is latched until it arrives, its spot turns unsafe or 5 s pass.
    `nb.wormmove`, `nb.cure`.
    - The urgent move breaks a pinning channel itself. A bot mid hard cast runs no triggers, so the
      per-instance read, on any other bot's tick, calls `RequestSpellInterrupt` on it.
    - When no cure spot is safe, the escape drops the partner reach, and a run's escape also clears
      the other Bile carriers the walk ignores; the walk resumes once the bot is clear.
  - **`northrend worms move guard`**, while that walk is in flight, zeroes every other mover but
    attacks, `avoid aoe` and Beasts nodes, spell movers included: charge, Intercept, Feral Charge,
    `blink`, `disengage`.
  - **`northrend worms bile reach guard`** zeroes `reach melee` and the charges for a Bile carrier
    that holds no beast and isn't a runner, so a melee walked off the stack stays off for the
    Bile's 24 s (guide: spread even as melee). Without it the Bile rule and `reach melee` trade the
    bot all Bile long ([pitfalls.md](../../engine/pitfalls.md), the Hodir case); leaving melee out
    of the rule instead would put the pulse, up to 9.7k every 2 s, on the whole stack.

## Icehowl

Combat reach 12, so melee range is 14.8 yd.

- **Whirl** (67345 / 67663 / 67664 / 67665), first at 10-12 s then every 15-20 s: 15 yd centre to
  centre, knockback ~31 yd. **Melee cannot dodge it here** — 14.8 is inside 15 — whatever the guide
  says about max range; ranged already stand outside.
- **Ferocious Butt** (66770 / 67654 / 67655 / 67656) on his victim every 15-30 s: 2.5-3 s stun and
  42k / 69k / 55k / 83k.
- **Arctic Breath** (66689 / 67650 / 67651 / 67652), first at 14 s then every 20-30 s, and 5-8 s after
  a charge ends in rage, 20-23 s after a daze, at a random player within 90 yd off his threat list,
  tank included. An instant 5 s channel: `Spell::_cast` faces him to the target, then picks the cone's
  victims once, so nothing telegraphs it and only standing apart beforehand helps. The cone reaches
  100 yd from his centre and is **60° wide on 10N only** (`spell_cone` row 66689; the other three ids
  have no row, so `TARGET_UNIT_CONE_ENEMY_24` makes it 24°). Its bisector is the target's bearing, so
  another player is hit only within half the width of it, 30° or 12°, at any distance. 5 s stun and
  3k / 4k / 4k / 6k a second, 15k / 20k / 20k / 30k in all.

**The charge**, 30 s after the pull, then 30-50 s after a rage or 45-65 s after a daze, whose
`DelayEvents(15s)` also holds the next jump. He is `REACT_PASSIVE` while in combat only during this
cycle, which makes that an exact read.

1. He jumps to `ARENA_CENTER`, drops his victim and clears his target.
2. 2 s later, Massive Crash (66683 / 67660 / 67661 / 67662), 1 s cast: 150 yd, knockback 40 yd/s out
   and 15 up (~62 yd, so the raid lands at the wall), damage and a stun.
3. 2 s later he gazes at a random player off his threat list: `UNIT_FIELD_TARGET` holds it.
4. 2 s later he fixes the line from himself to that player and jumps 35 yd the other way. The charge
   ends 50 yd out along the line (46 when its angle falls in (1, 2) rad, the gate). Here the crash stun
   comes off every player; normal also grants Surge of Adrenaline 68667 (+50% speed, 8 s), heroic
   does not.
5. 1.5 s later he charges at 65 yd/s, ~1.3 s end to end, target cleared.

**Contact is tested twice, not along the path**: any living player within 12 yd (3D, centre to
centre) about 100 ms into the charge, and again on arrival (`DoTrampleIfValid`). The first check runs
once — `EventMap::ExecuteEvent` erases it, the "no PopEvent" comment is stale — and with a 100 ms map
update (`AC_MAP_UPDATE_INTERVAL`) it lands 100-200 ms in, 6.5-13 yd past his start. A hit casts Trample
66734 (50k, 12 yd) and Frothing Rage (66759 / 67657 / 67658 / 67659: +50 / 50 / 75 / 100% damage and
haste, 15 s, enrage) and he fights from there. A clean charge ends in Staggered Daze 66758 (15 s stun,
+100% damage taken, no difficulty row) with his events held 15 s: the burn window.

On heroic he is immune to `SPELL_EFFECT_DISPEL`, so Tranquilizing Shot cannot remove Frothing Rage;
the hunters' generic `tranquilizing shot enrage` node covers normal only.

**What the bots do.**

- **Charge state**, per instance, active while he is engaged and passive. Phase 1 has no target; 2 (gaze)
  while `GetTarget()` names a living player and he is within 3 yd of `ARENA_CENTER`, the line running
  centre → that player; 3 once he is further out, the line his position → centre, frozen to the
  cycle's end. Not his facing: after the jump back he faces away from the line. The lane runs from
  35 yd behind the centre to 50 ahead (46 for an angle in (1, 2) rad). Leaving passive writes 4 under
  the daze, 5 under Frothing Rage, else 0; a latch older than 12 s drops. `nb.charge`, `nb.gaze`, a
  `lane` hazard row.
- **Dodge** (`icehowl charge incoming` → `icehowl clear charge path`, `ACTION_EMERGENCY + 8`): a bot
  within 14 yd of the lane walks to the nearest spot 16 yd clear of it (13 if none within 30 yd; a
  bot already 13 clear stays put, since a 13 yd spot sits inside the trigger and re-fires it), at
  `MOVEMENT_FORCED`, interrupting a cast that pins it, destination latched
  ([pitfalls.md](../../engine/pitfalls.md): movement lock, cast pin). The whole lane, not the two
  contact points: a server stall moves the first test along it (guide: strafe out of the way).
  `nb.dodge`.
- **`icehowl charge guard`**, gaze to the charge's end, every bot: zeroes every mover but attacks and
  the dodge, and gap closers, `blink` and `disengage` too. Melee chasing his start point meet the first
  contact test.
- **Arctic Breath spread** (`icehowl breath spread` → `icehowl move to breath stand`,
  `ACTION_RAID + 2`), by bearing round him, since the cone ignores distance (guide: a half circle
  behind him, healers apart). Per instance, read once per ms:
  - **On** while he is the only beast engaged, not passive, no charge is latched, and his victim is a
    player in his melee range: a victim out of reach means he is walking after it (Whirl, a charge's
    end), and a heroic overlap is the worm rules'. Off keeps the deal; Icehowl disengaged clears it.
  - **Bearings** step by half the cone plus 6° (18°, 36° on 10N; the cone from the remapped id's
    `spell_cone`, else 24°), the 6° covering 3° of stand error either side. They run outward from
    straight behind him, `k` = 0, 1, -1, 2, …, never within a step plus the 30° re-latch turn of his
    front (±126° at most, ±108° on 10N), so a breath on the tank misses them. Each is probed 34 yd
    out (the ranged band's 25, his 8 yd re-latch drift, 1 spare) along the player collision path
    (the floor is a GO, [README.md](README.md#arena-floor)) and kept with 22.5 yd of room, up to 11
    (5 on 10N), a half circle; a walled one is replaced further round. They re-latch, `k` kept, when
    he moves 8 yd, his victim's bearing turns 30° (Whirl throws the tank ~31 yd every 15-20 s and he
    follows), or his drift cuts a kept bearing's room under 22.5.
  - **Deal**, sticky while its bearing is kept: living non-tank bots of the group (humans can't be
    moved, tanks hold him), rebuilt each second, dealt healers, ranged, then melee, each by guid. A
    healer takes the bearing with fewest healers, then nearest 60° off his back (~27 yd from the tank
    at 17 yd out, ~30 straight behind), then fewest bots; anyone else the fewest bots nearest his
    back. About two share a bearing on 25-man, melee inside ranged: victims per breath step with
    spacing ([pitfalls.md](../../engine/pitfalls.md)), and 20 bots spaced evenly, closer than the
    half-width, would freeze three. Melee take only bearings within 57° of his back (90° less the
    30° turn and 3° of stand error): past 90° he parries them and hastes his next swing, carrying
    neither `NO_PARRY` flag. A melee with none kept gets no bearing, and `set behind` places it.
  - **Stand**: his spot plus the bot's own distance clamped into its band, capped at the bearing's
    room − 1, less his drift along it since the probe
    ([raid-mechanics-lessons.md](../../engine/raid-mechanics-lessons.md): clamp, don't chase).
    Judged in his frame, as the breath aims: fires 3° off the bearing (the margin's share) or 1 yd
    outside the band, arrives at 1.5° and 0.5. Melee 8-13 (inside 14.8), healers 16.5-18.5 (heal
    reach to the tank and the flank ranged), ranged 21.5-25; the healer and ranged floors clear
    Whirl's 15 and a hunter's minimum (5 + 14.8) by more than that 1 yd (park tolerance, same doc).
    `MOVEMENT_COMBAT` and no interrupt: 15-30k over 5 s is healable, and the first breath after a
    charge is 5 s or more out.
  - **Healer yield**: a healer holds off while its `party member to heal` is out of heal reach of
    its stand, by `PartyMemberToHealOutOfSpellRangeTrigger`'s test. Heal picks go out to 38.5 yd and
    `reach party member to heal` walks past heal range + 1, so a stand fighting it stops the
    healing; measured from the bot, the reach walk's arrival would end the yield and walk it back.
  - **`icehowl breath spread guard`** zeroes `CombatFormationMoveAction` for a bot holding a bearing
    while the encounter is live: `set behind` moves melee in his front half to ±108° off his facing,
    off a wrapped bearing, and he faces each breath target for 5 s.
  - `nb.spread`, `nb.arc`.
- **Hold** (`icehowl tank duty` → `icehowl tank hold boss`): RTI; skull only while he is the only
  beast engaged, so in a heroic overlap two holds don't trade it every tick; taunt only while his
  victim isn't a tank and he isn't passive; no drag, since every charge starts at the centre and
  ends at the wall (guide: tank him at a wall).
- **Defensive** (`icehowl frothing rage` → `icehowl tank defensive`): his victim under Frothing Rage
  casts `NextTankDefensive`, the only answer on heroic. `nb.defensive`.
- **Burst row.** Lust, one 10-minute cooldown, is held through Gormok and the worms, then goes at the
  daze or Icehowl ≤ 50%. Other burst cooldowns during Icehowl only in the daze or ≤ 30%, free
  before. The guide spends everything in the stun; the health floors cover a raid that never lands a
  clean charge. The base tank-held gate still ANDs: his victim clears at the jump, so it re-arms
  3-5 s into the daze.

## Known gaps

- No Hand of Freedom or defensive for a Toxin-snared tank, no raid cooldown when a Spray lands on
  several players, and melee still stack, so a Spray on one splashes the rest.
- DoTs aren't refreshed on both worms before a submerge (guide), and nothing chases the achievement
  (both worms within 10 s).
- A healer's reach to both worm holders is only a preference: with the worms on opposite sides its
  spot falls back to safety alone.
- A hunter that can't Steady Shot (moving, or too close) leaves held Misdirection charges to its
  rotation, which can spend them on the skull and hand the `WormStationary` holder threat on the
  mobile worm: the mobile holder re-taunts, at the cost of taunt diminishing returns.
- No Hand of Protection on a snobolled healer or caster, no external on Ferocious Butt (guide).
- Heroic Frothing Rage can't be dispelled; only the holder's defensive answers it.
- Melee eat every Whirl.
- No spread against Fire Bomb in Gormok's stage (guide: stay spread); the impact dodge is the only
  answer.
- A caster pinned by its own cast waits it out and can eat the impact.
- The bomb dodge ignores worm hazards in a heroic Gormok + worms overlap.
- No raid cooldown on Arctic Breath (Divine Sacrifice, Aura Mastery with Frost Resistance Aura).
- Humans and tanks hold no bearing, so a breath on one can catch the bots on that bearing, and with
  about two to a bearing on 25-man a breath still freezes two.
- No spread in a heroic worms + Icehowl overlap.
- Melee with every bearing within 57° of his back walled hold none, and `set behind` stacks them
  behind him, so a breath on one freezes them all.
- With no wall tanking, Whirl moves him every 15-20 s and the whole spread walks after him.
- Unverified live: if every bearing probe fails against the GO floor, `nb.arc` stays 0 and the
  spread never runs.
- The stomp and carrier walks (`MOVEMENT_COMBAT`) can't re-aim while their own walk is in flight
  until the booking runs out or the stall clear fires; a dodge spot `MoveTo` refuses (no path) while
  an earlier dodge walk is in flight notes `locked` until that booking expires.

## What a trace answers

Position, verdicts and movement come from the raid-agnostic streams. The Beasts rows, under
`postmortem.py <file> --notes nb.`:

| Key | Says |
|---|---|
| `stage` | Engaged beasts as a mask, 1 Gormok, 2 a worm, 4 Icehowl; 0 between them. Per instance |
| `tank` | A tank bot's duty, `gormok`, `swap`, `wormmobile`, `wormstationary`, `icehowl` or `none`, then the beast's guid. Written when a trigger asks. Can move during a charge: Icehowl has no victim then, so pass 1 keeps nothing |
| `swap` | The `GormokSwap` tank's read: `go v=<holder stacks>`, `wait mine=<own stacks>`, `wait v=<holder stacks>` |
| `defensive` | The tank defensive picked, `covered` (one running) or `none` |
| `snobold` | A DPS's pick, `<rider guid> <rider role>`, or `none` |
| `charge` | 0 none, 1 crash, 2 gaze, 3 charge, 4 daze, 5 rage. Per instance |
| `gaze` | The gaze target. Per instance |
| `dodge` | The charge dodge's branch: `move <yd>` walk issued, its length; `tight` walk to a 13 yd spot; `hold` its own walk in flight; `clear` arrived, already there, or parked past 13; `stunned` can't move; `locked` another forced walk holds movement; `none` no spot within 30 yd |
| `worm` | 0 no worm engaged, 1 Dreadscale mobile, 2 Acidmaw mobile, 3 a worm under ground, 4 lone Dreadscale, 5 lone Acidmaw. Per instance |
| `cure` | A bot's pairing: `seek <Bile carrier>`, `run <Toxin carrier>` as its runner, `wait <runner>` or `wait` stuck with no runner, `none` |
| `wormmove` | The reposition branch, as `dodge` with a reason (`cure`, `run`, `pool`, `bile`, `sweep`, `spew`, `spread`): `move <yd> <reason>`, `hold <reason>`, `clear`, `none <reason>`, `locked` (also a cast pinning a non-urgent move), `stunned`. Holders: `approach <yd>` under ground, `drag <yd>` |
| `spread` | A non-tank bot's bearing `k` from straight behind Icehowl (`0`, `1`, `-1`, …), or `none` while the layout is off or it holds none. Written when a trigger asks |
| `arc` | Bearings the layout kept, 0 while it is off. Per instance |
| `bomb` | The Fire Bomb dodge's branch: `move <yd>` walk issued, its length; `tight` walk issued to a 9.5 yd spot; `hold` its own walk in flight; `pinned` a cast pins its feet; `stunned` can't move; `none` no spot within 20 yd; `locked` `MoveTo` refused the walk (a walk at `MOVEMENT_FORCED` or higher holds movement, or no path to the spot); `clear` outside every young bomb's trigger, or a beast is hitting it |

The lane is a `haz` row, 66734, shape `lane`: origin 35 yd behind the centre, `ex`/`ey` its end,
`half` 12. Written at the gaze and again when the line freezes at the jump back; the last in a
cycle is his path. It never reaches `snap.hz`, so no death block says STOOD IN for Trample. Nor do
the worms' hazards: Spew is a `haz` row on the remapped tick, shape `wedge`, origin the worm,
`facing` in radians, `arc` degrees either side, `range` 55, once per cast; a Slime Pool is a
creature (35176) whose radius only the reader rebuilds.

Each Fire Bomb is one `haz` row, 66317, shape `circle`, `rad` 8, at the bomb, written by the first bot
to see it young; `ttl` is the modelled time left to the impact, not a measured one. It never reaches
`snap.hz` either.

`tools/botobs/bosses/northrend_beasts.py <file>` reads the rest: stage spans with the deaths in each
and when each beast first targeted someone (`--stage`); duties per tank, every change of Gormok's
victim with both holders' Impale stacks, peak stacks, `nb.swap` branches, `gormok tank swap taunt`
verdicts and defensive picks (`--tanks`); per charge the gaze target, lane, outcome, who stood
within 12 yd of the lane's end at it (nearest snapshot), dodge branches per bot and Trample victims
(`--charge`); per Arctic Breath its target, who froze (applies within 1 s) with their `nb.spread`,
who the cone held in the snapshot nearest the cast (30° or 12° off the target's bearing), `nb.arc`,
and times frozen per bot (`--breath`); Snobolled! 66406 spans per rider with role and seconds
carried, and each DPS's picks by rider role (`--snobold`); per bomb its target (the nearest member),
the 66317 hits up to 1.5 s past the modelled impact, each given to the nearest bomb whose window
holds it, who stood within 8 yd at the impact, and `nb.bomb` branches per bot, `clear` included
(`--bomb`); `worm` spans with their deaths, per emerge the seconds from each worm's own Emerge cast
66947 (the first engage: `worm` leaving 0) until it targeted a tank and the non-tanks it targeted
first, per wedge the non-tanks inside at the nearest snapshot and the Spew tick victims by role over
3.5 s, per pool the bot-seconds inside its radius (`2 + 0.3 ×` whole seconds from its first
snapshot) and its hits, Sweep victims by role and `wormmove` branches (`--worms`); Paralytic Toxin
spans per carrier, ended `cured` (a Bile pulse on it within 250 ms of the removal), `died`,
`expired` (59 s or more), `other` or `open`, with seconds carried and `stuck` past 18 s (13.5
heroic), Burning Bile spans with pulse hits on others carrying no Toxin, and `cure` branches
(`--cure`).

Still invisible: creature auras, so Staggered Daze, Frothing Rage, Rising Anger and a lone worm's
Enrage never get an aura row: `charge` 4 and 5 are the only record of how a charge ended, `worm` 4
and 5 of the Enrage.
