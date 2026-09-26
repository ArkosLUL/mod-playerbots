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
- **Worm form** by display id (`IsWormMobile`); with neither mobile, Acidmaw counts as the mobile
  one. A lone survivor is the mobile one whatever its form: the script sends it under at once and it
  comes up mobile.
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

## The arena floor is a gameobject

The floor is GO 195527 (Argent Coliseum Floor, destroyed for Anub'arak), so the static mesh has
nothing there: navprobe at `ARENA_CENTER` finds no poly within {3,50,3} and no height, and rings out to
55 yd find only the stands at Z 417-458. **navprobe cannot verify an arena point.** A bot's `MoveTo`
there takes `PathGenerator`'s player shortcut (`PATHFIND_NORMAL|PATHFIND_NOT_USING_PATH`, a straight
line), and `CheckCollisionAndGetValidCoords` clips a point at the wall with the static LOS ray. The
script bounds the floor instead: Icehowl lands 50 yd from the centre (46 toward the main gate, north,
+y), and the gate front sits 38.8 yd north.

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
  his melee range. NPC 34854 (60 s) appears at the player; a 1 s cast and a 14 yd/s missile later,
  66317 hits 8 yd around it (4.8-6.2k, no difficulty row). Then 66320 (67472 / 67473 / 67475) pulses
  every second at **2 yd** — `SpellInfoCorrections` cuts the DBC's 5 — and on heroic adds a stacking
  1k / 2k DoT per 2 s (20 s). The NPC is non-selectable with a periodic damage aura, so the generic
  `avoid aoe` flees the pulse; the 8 yd impact lands before that aura and nothing flees it.

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
  centre, and `spellDistance` 28.5 alone doesn't keep casters past it.
- Both walks wait out a pinning cast rather than interrupt it (neither mechanic is lethal), and
  re-issue one that Head Crack or a channel stopped short ([pitfalls.md](../../engine/pitfalls.md)).

## Acidmaw & Dreadscale

Submerge/emerge form swap, avoid slime pools and churning ground, focus the mobile worm, spread for
sprays.

**Bite and spray carry no aura.** Burning Bite and Paralytic Bite trigger the debuff
(`EffectTriggerSpell`); each Burning/Paralytic Spray id links it in `spell_linked_spell`. The debuffs
are Burning Bile 66869 (24 s, no difficulty row) and Paralytic Toxin 66823 (remaps, README table).
Burning Bile pulses 66870 every 2 s: damage within 10 yd, and a `spell_linked_spell` row strips
Paralytic Toxin from everyone it hits, so Bile carriers cleanse Toxin carriers by standing next to
them.

## Icehowl

Combat reach 12, so melee range is 14.8 yd.

- **Whirl** (67345 / 67663 / 67664 / 67665), first at 10-12 s then every 15-20 s: 15 yd centre to
  centre, knockback ~31 yd. **Melee cannot dodge it here** — 14.8 is inside 15 — whatever the guide
  says about max range; ranged already stand outside.
- **Ferocious Butt** (66770 / 67654 / 67655 / 67656) on his victim every 15-30 s: 2.5-3 s stun and
  42k / 69k / 55k / 83k.
- **Arctic Breath** (66689 / 67650 / 67651 / 67652), first at 14 s then every 20-30 s, at a random
  target within 90 yd: a 100 yd cone, 5 s freeze and 3k / 4k / 4k / 6k per second. The cone is **60°
  on 10N only** (`spell_cone` row 66689); the other three ids have no row, so
  `TARGET_UNIT_CONE_ENEMY_24` makes it 24°.

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

- `northrend worms afflicted by burning` keys on Burning Bite/Spray auras, so it has never fired,
  and its keep-moving remedy is not the mechanic above.
- The worm holds pick their worm by form, not by `GetBeastOfDuty`, so with neither worm mobile, or a
  lone survivor not yet up mobile, the `WormMobile` holder has nothing to hold.
- Arctic Breath has no spread and Fire Bomb's 8 yd impact no dodge.
- No Hand of Protection on a snobolled healer or caster, no external on Ferocious Butt (guide).
- Heroic Frothing Rage can't be dispelled; only the holder's defensive answers it.
- Melee eat every Whirl.

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

The lane is a `haz` row, 66734, shape `lane`: origin 35 yd behind the centre, `ex`/`ey` its end,
`half` 12. Written at the gaze and again when the line freezes at the jump back; the last in a
cycle is his path. It never reaches `snap.hz`, so no death block says STOOD IN for Trample.

`tools/botobs/bosses/northrend_beasts.py <file>` reads the rest: stage spans with the deaths in each
and when each beast first targeted someone (`--stage`); duties per tank, every change of Gormok's
victim with both holders' Impale stacks, peak stacks, `nb.swap` branches, `gormok tank swap taunt`
verdicts and defensive picks (`--tanks`); per charge the gaze target, lane, outcome, who stood within
12 yd of the lane's end at it (nearest snapshot), dodge branches per bot and Trample victims
(`--charge`); Snobolled! 66406 spans per rider with role and seconds carried, and each DPS's picks by
rider role (`--snobold`).

Still invisible: creature auras, so Staggered Daze, Frothing Rage and Rising Anger never get an aura
row, and `charge` 4 and 5 are the only record of how a charge ended.
