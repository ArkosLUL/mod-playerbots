# Lord Jaraxxus

Raid-wide facts and the code layout: [README.md](README.md). Mechanics source:
`boss_lord_jaraxxus.cpp`; ids are 10N/25N/10H/25H, all DBC-only rows.

## Pull and wipe

- **Intro** (stage 2): he is summoned at `LOC_CENTER` (`ARENA_CENTER`), walks 10 yd south and stays
  `UNIT_FLAG_NON_ATTACKABLE` and passive for the ~50 s scene. The release (stage 3) clears the flag
  and attacks `SelectNearestTarget(200)`, so whoever stands nearest takes the first swings.
- He is a `BossAI` on boss state `BOSS_JARAXXUS` (1): `_JustEngagedWith` sets it `IN_PROGRESS`.
  `MoveInLineOfSight` is empty, so he never aggros by proximity.
- **Wipe**: evade despawns his summons, sets `NON_ATTACKABLE` and `TYPE_FAILED`; the cleanup (Tirion)
  respawns him at the centre chained (67924), unattackable and passive, and sets `FAIL`. The stage
  stays 3, so the gate stays open; the retry gossip frees him straight into a forced attack on the
  nearest player, so bots stage round the centre meanwhile and the main tank's hold waits for combat. `Reset` despawns every portal and volcano within 500 yd.
- **Kill**: `summons.DespawnAll` takes the portals and volcanoes, not the Mistresses and Infernals
  they summoned, which live on into stage 4 (heroic Mistresses keep kissing). Multipliers that
  concern the adds gate on "open", not "live".
- **Berserk** 64238 at 10 min (`ScheduleEnrageTimer`). Touch of Jaraxxus (25H) is commented out.

## Boss abilities

Timers from engage; events run only while he is not casting, so they slip.

| Ability | Ids | Timer | Effect |
|---|---|---|---|
| Fel Fireball | 66532/66963/66964/66965 | 5 s, then 10-15 s | on his victim; cast 2.5/2.0/2.5/2.0 s; 15.1k/24.4k/18.0k/34.1k fire plus a Magic DoT 5.8k/9.3k/7.8k/10.7k a second for 5 s |
| Fel Lightning | 66528/67029/67030/67031 | 10-15 s, repeat | instant, random player, chains to 3/5/3/5 targets; 7.9-9.1k/9.7-11.3k/9.7-11.3k/11.7-12.3k, each jump ×0.8/0.9/1.1/1.1 |
| Incinerate Flesh | 66237/67049/67050/67051 | 24-26 s, then 20-25 s | random player; undispellable heal absorb 30k/60k/40k/85k for 15/15/12/12 s |
| Burning Inferno | 66242/67059/67060/67061 | Incinerate expiring unhealed | every player, 2.4k/2.4k/3.9k/7.8k a second for 5 s |
| Nether Power | 66228/67106/67107/67108 | 25-45 s, repeat; delays every event 5 s | full 5/10/5/10 stacks on himself, 30 s, +20% spell damage per stack, Magic |
| Legion Flame | 66197/68123/68124/68125 | 30 s, repeat | random player; 2 s later 66199/68126/68127/68128 for 6 s |
| Nether Portal | 66269/67898/67899/67900 | 20 s | 15 yd to his left; schedules the Volcano 60 s later |
| Infernal Eruption | 66258/67901/67902/67903 | Portal + 60 s | 15 yd to his left; schedules the Portal 60 s later |

**Random picks skip the marked.** He, every Mistress and every Infernal carry Hittin' Ya 66327, which
puts 66326 (10 s) on each melee victim, and Legion Flame and Fel Lightning put it on their own target.
Legion Flame, Incinerate Flesh, Fel Lightning and Spinning Pain Spike pick `SelectTarget(Random, 0, 0,
playerOnly, withTank = true, -66326)`, so a tank is eligible only 10 s after its last swing taken. The
guide's Portal/Volcano times (60 s, 120 s, …) are not this core's.

**Interrupts.** His immunity set (`creature_immunities` −286) holds SILENCE and STUN but not
INTERRUPT: Kick, Pummel, Shield Bash, Counterspell, Wind Shear, Mind Freeze and a Felhunter's Spell
Lock land, Silencing Shot and Silence do not. The Fel Fireball DoT is Magic, so the generic party
dispels strip it.

**Fel Lightning.** Each jump takes the nearest player in sight within 10 yd of the last victim (the
magic default; no `spell_jump_distance` row) by `IsWithinDist`, which adds both combat reaches:
13 yd centre to centre, 3D. The jump multiplier is `EffectChainAmplitude`, so at the mean roll a
full chain deals about 21k/43k/35k/73k before mitigation, and on 25H its fifth victim takes 17.6k.
That is with no Nether Power stack: each one left on him adds 20%, ×2 at a full 5, ×3 at a full 10.
Nearest-first keeps a stack's chains inside it: on the 2026-10-16 25N kill all 7 casts hit 5 (35k
each with absorbs, 38% of the raid's damage taken), the 2 melee and tank picks only melee and tanks,
the 5 caster picks the same five casters standing 0.4-3.3 yd apart. It was 98% of what ranged and
healers took; every hit landed on full health and none fell below 59%. On the 2026-10-17 25H kill:
14 casts, 745k (13.5% of the raid's), no lethal hit.

**Nether Power.** His `SpellHit` resets the stacks to full on each cast. A Spellsteal (30449)
strips one: this core marks Nether Power unstealable (SpellInfoCorrections), so only the script's
`SpellHit` removes a stack, casting Nether Power on the mage. Purge and Dispel Magic remove a stack
per effect dispelled.

**Incinerate Flesh.** Effect 0 is the absorb (aura 301, `AuraEffect::GetAmount` is what is left);
effect 2 ticks once, at the aura's end, and casts Burning Inferno while the aura is up. The heal that
eats the last of the absorb removes the aura (`Unit::CalcHealAbsorb`), so no Inferno. The target reads full
health throughout. Burning Inferno is cast by the target on every ally (radius 50000) on Jaraxxus's
behalf, so his Nether Power stacks count; it ticks once a second from 1 s after the cast.

On the 2026-10-17 25H kill 6 of 7 windows ran out, absorbing 47-68k of the 85k each: cast-time heals
were skipped and the druid recast Rejuvenation 11 times a window on a target that had it. The Infernos
did 3.37M of the raid's 5.54M and caused 6 of its 8 deaths. The one window healed through had its
target at 91% and took 81k in 9.6 s, 55k from 2 Healing Waves and 4 Flashes of Light.

**Legion Flame.** 66199 burns its carrier 2.9k/4.4k/3.9k/6.3k a second and summons a Legion Flame
(34784) at its feet every second. Each flame lasts 60 s, is `NOT_SELECTABLE`, and ticks 66877/67070/
67071/67072 every second on everyone within 3 yd of it, centre to centre (creature caster):
2.9k/4.4k/4.9k/7.3k. A flame's first tick lands a second after it spawns, so a carrier that never
stops takes none. `AvoidAoeAction` sees the flames (a non-selectable unit with a periodic damage
trigger) out to 5.5 yd centre to centre (`GetDistance` takes off both combat reaches, 1.0 and 1.5),
and flees by `FleePosition`; `jaraxxus avoid aoe guard` vetoes it while any flame is up.

## Adds

| Unit | Entry | Normal | Heroic |
|---|---|---|---|
| Nether Portal | 34825 | not selectable, lasts 15 s, one Mistress at 8 s | selectable (health ×15/×64), permanent, a Mistress every **6 s** |
| Infernal Volcano | 34813 | not selectable, lasts 18 s (SpellInfoCorrections), an Infernal every 5 s (three) | selectable, permanent, an Infernal every 5 s |

Test `UNIT_FLAG_NOT_SELECTABLE`, not the difficulty. The portal's own aura 66263/67103/67104/67105
summons each Mistress on its periodic tick (one tick at 8 s on normal, every 6 s on heroic), with
66264/68404/68405/68406 hitting everyone within 10 yd of the portal, centre to centre:
6.8k/8.3k/9.3k/10.7k. An Infernal lands at a random point within 15 yd of the volcano and hits 10 yd
around it.

**Mistress of Pain** (34826, health ×25/×70/×25/×70; immune to STUN and INTERRUPT) opens on the nearest
player (`SelectNearestTarget(200)`): Shivan Slash on her victim; Spinning Pain Spike, a jump to a
random player within 140 yd. Heroic adds **Mistress' Kiss**: a 1.5 s cast every 25-35 s after 10-15 s
onto one random mana user, landing 66334/67905/67906/67907 for 15 s. Every 0.5 s, if the target has
`UNIT_STATE_CASTING` — a cast-time spell or a channel, never an instant — it takes
66359/67073/67074/67075 (8.3k, 13.6k on 25H) plus an 8 s school lockout, and the kiss is spent. A bot
mid-cast runs no triggers (`UpdateAIInternal` yields while a spell prepares), so only
`RequestSpellInterrupt` from outside reaches it. On the 2026-10-17 25H kill both heroic Mistresses
arrived 6.0-6.3 s after their portal with 8 melee inside its 10 yd, and her first swings (27.6k,
43.9k) killed two.

**Felflame Infernal** (34815, health ×6/×16/×7/×23; stunnable on normal only, immune to
INTERRUPT): melee. Every 30 s after 7-20 s, Fel Streak: `DoResetThreatList`, 50,000 threat on a
random player within 44 yd, tanks included, a charge there with a 15 yd impact, then Fel Inferno
pulsing 13-15 yd around it for 6 s. A taunt answers the reset by copying the top threat.

## Tactics

The Warcraft Tavern guide, reconciled with the script:

- Boss held at `ARENA_CENTER`. The release hits the nearest player, so the main tank waits 3 yd from
  him on its staging stand ([README.md](README.md#pre-pull-staging)).
- **Kill order**, everyone but tanks and healers, oldest (lowest guid) first: heroic Portal, Volcano,
  Mistress, Infernal; normal Mistress, then Infernal. Portal and Volcano stop the spawns.
- **Tanks**: main tank on the boss; assist tank 0 on Mistresses, assist tank 1 on Infernals, taunting
  its add back off anyone else, tanks included; a lone assist tank takes Mistresses first. The boss
  is not walked to the portal for cleave: it spawns 15 yd from him anyway.
- **Heroic Portal**: melee DPS bots targeting one or inside 12 yd of it keep 14 yd from it, from 1 s
  before its next Mistress (its aura's periodic timer) until each of its Mistresses is on a tank
  (`IsTank` by role or spec) or dead; ranged keep killing it. The guide has no such step; it follows
  from her arrival hit and nearest-player opener.
- **DPS threat cap**: a DPS bot stops damaging him at 90% of his tank victim's threat, area damage
  within 30 yd included, and resumes under 80% (Anub'arak's latch). On the 2026-10-17 25H kill a DPS
  warrior pulled him three times and held him 18-26 s, and the two tanks' 8 taunts in 20 s ran into
  his taunt diminishing returns (flags_extra 0x80000).
- **Fel Fireball**: one interrupter per cast, the lowest guid among bots with an interrupt ready,
  affordable, in range and not mid-cast. It is the backstop: on the 2026-10-16 25N kill the generic
  interrupt nodes kicked all 11 casts 7-103 ms in, one kick each, before the duty holder latched, so
  coverage reads `fel fireball interruptible` NEVER and `interrupter` never `1` (18 of 18 on 25H,
  median 43 ms). `interrupter` is change-only, so a trace can't tell a duty node beaten to the kick
  from a dead one.
- **Fel Lightning**: no spread on any difficulty; the stack heals through it. The guide names the
  stack as its alternative: Portal and Volcano stay in caster range (the heroic DPS check) and AoE
  heals hit harder, at the risk of Legion Flame carriers inside it, which `avoid legion flame` walks
  out. A spread holds every ranged and healer 13+ yd from each other and the melee ball, about one
  30 yd ring of 14 slots on 25, on a floor navprobe cannot verify, against 60 s flames, heal range
  and the adds; a melee or tank pick still chains in full. Reopen on a heroic death with a hit of a
  0-stack Fel Lightning and no Burning Inferno in its last 5 s (`--boss`); one under stacks is a
  remover failure. The 25H kill's three Fel Lightning deaths each took 57-70% from Burning Inferno.
- **Nether Power**: mages Spellsteal, non-healer shamans and priests Purge and Dispel Magic; healers
  only when no other remover lives and no Incinerate Flesh is up.
- **Incinerate Flesh**: every healer in range heals the target, highest throughput first: Holy Shock,
  Riptide, Swiftmend, Penance, then Holy Light, Greater Heal, Healing Wave, Nourish, Healing Touch,
  Flash of Light, Flash Heal, Lesser Healing Wave, Regrowth, then the HoTs, skipping one the bot already
  has on the target. `PlayerbotAI::UpdateAI` cancels a single-target heal still casting on a
  full-health target only when it carries no `SPELL_AURA_SCHOOL_HEAL_ABSORB`, so cast-time heals land
  under the absorb. A kissed healer casts instants only.
- **Burning Inferno** is covered before it lands ([Raid cover](#raid-cover)).
- **Legion Flame**: the carrier keeps moving for its 8 s, away from the boss and the raid and within
  30 yd of a living healer other than itself; everyone else steps clear of the flames. A leg bends
  round the healer ring: the sweep keeps the first ring's clear spot nearest the heading among those in
  reach, and drops the reach only when no clear spot has it. On the 25H kill carriers ended 30-48 yd
  from the nearest healer. A tank carrier takes the boss with it, rare now: the marker keeps a tank off
  Legion Flame while he swings at it.
- **Mistress' Kiss**: a kissed bot casts instants only; a cast already running is broken from outside.
- **Lust** at the pull (default burst row): the heroic Portal at 20 s falls inside it, and Berserk is
  10 min out.
- **Fire Resistance Aura** (the [ResistAura](README.md#pre-pull-staging) election, Jaraxxus wanting
  fire and nature) goes first to a paladin who knows Aura Mastery, live or staged, which its Aura
  Mastery doubles; a kept fire holder without it gives way. On the 25H kill Aura Mastery went out twice
  under Concentration Aura while a ret paladin held fire.

## Raid cover

The shared helper `Util/ToCHelpers_Defensive` answers a hit the encounter predicts as a
`ToCPredictedHit` (start of its first damage, duration, damage per player, school, members who can't
channel). Jaraxxus is its first user; another boss plugs in by predicting its own hit and wiring the
two nodes and the holds below.

- **The Inferno**, as predicted by `GetJaraxxusBurningInferno`: an Incinerate Flesh with 2 s or less
  left and absorb remaining, first damage 1 s after it ends, 5 ticks of the DBC average roll × (1 +
  0.2 × Nether Power stacks); or, once running, a member's Burning Inferno and what its ticks still
  owe. Read once per instance per ms.
- **Raid cooldowns**: dealt on the first read where the hit would leave 3+ members under 25% health,
  taking off each one's average fire resistance (`Unit::GetEffectiveResistChance` against level 83)
  and damage taken modifiers. One bot per cooldown, one cooldown per bot, among bots that know it,
  have it off cooldown, can pay and are in a form that casts it: Aura Mastery (a paladin holding its
  own resistance aura of the hit's school), Divine Sacrifice (Divine Guardian talent, 50%+ health),
  Anti-Magic Zone (3+ members within its 7 yd; by id, the name lookup misses `Anti-Magic Zone`),
  Divine Hymn and Tranquility (healers first; not kissed or on a flame). Latched per hit (starts
  within 2 s are one), cast from 1.5 s before the first damage to 1 s after. Divine Sacrifice comes
  off right after the cast once Divine Guardian (70940, 20% less damage within 30 yd for 6 s) is up:
  its 30% redirect drains the paladin under a hit on everyone. A holder mid-cast inside its window is
  broken from another bot's tick, as the kiss is.
- **Personal**: from 1 s before the first damage to its end, when what the hit leaves the bot is
  under 25% of its health: `NextMagicDefensive` for the rest of the hit; on a lethal hit with no row,
  Dispersion, then `GetSelfImmunitySpell`, then a healthstone (not on the move). Nothing while
  Anti-Magic Shell, Cloak, Dispersion, Ice Block or Divine Shield runs or while it channels a cover.
  A Legion Flame carrier under 50% counts the rest of its trail the same way.
- `jaraxxus raid cover hold` blocks the five class nodes before the first damage, so none goes early
  or twice; `jaraxxus magic button guard` blocks Evasion and Deterrence during an Inferno and on a
  carrier. Never fight-long: after the first damage the class nodes run again.
- The 25H kill's six Infernos drew their first raid cooldown 1.2-1.7 s after the first tick in four,
  none in two.

## Targeting

Direct, no icon or `rti` ([README.md](README.md#targeting-is-direct-no-raid-icons)).

- **DPS** (`IsToCDpsBot`): `GetJaraxxusDpsTarget`, while the gate is open since his adds outlive the
  kill: a player's skull on him or an add, else the kill-order add, else him once in combat.
  `jaraxxus dps target guard` zeroes `dps assist`.
- **Tanks** `Attack()` their unit from their hold. `jaraxxus tank target guard` zeroes `tank assist`
  on the three holds' own trigger tests (`JaraxxusMainTankHolds`, `JaraxxusAssistTankHolds`), so a
  tank between holds keeps it.

## Node ladder

No ties; names drop their `jaraxxus` prefix. Two contests decide the order: the EMERGENCY band
beats every node, and Nether Power beats the DPS target. Tank rows never share a bot (the main tank
is never an assist tank), so their order is arbitrary.

| Trigger → action | Relevance | Why here |
|---|---|---|
| `fel fireball interruptible` → `interrupt fel fireball` | `ACTION_EMERGENCY + 9` | A 2.0-2.5 s window on one bot; the kick moves nobody, so the dodge follows next tick |
| `raid cover` → `cast raid cover` | `+8.6` | Above the flame walk, which holds a carrier's every tick: an instant goes out on the move. Channels go to nobody on a flame |
| `personal defensive` → `cast personal defensive` | `+8.4` | The same, for a carrier under 50% |
| `legion flame nearby` → `avoid legion flame` | `+8` | Beats every hold, so a carrying tank walks and he follows; breaks the bot's own cast first. The only flame mover: `AvoidAoeAction` is vetoed ([Legion Flame](#boss-abilities)) |
| `pinned cast` → `break pinned cast` | `+7` | Breaks a kissed, carrying or cover-due bot's cast from another bot's tick ([Mistress of Pain](#adds)). Returns `false`: costs nothing, and no RAID node returning `true` can skip it |
| `portal spawn` → `leave portal` | `+6` | Under the flame walk, which the leg's `MOVEMENT_COMBAT` yields to; drops a booking of its own priority first. `jaraxxus portal hold` blocks reach melee, set behind, rear flank, formation, follow, move random and charge meanwhile |
| `engaged by main tank` → `main tank hold boss` | `ACTION_RAID + 5` | Only while he is in combat ([Pull and wipe](#pull-and-wipe)). The drag to `ARENA_CENTER` tests only its next 5 yd step, since a tank-carried trail lies along the whole way back; a flame there sends the tank around the trail |
| `add needs assist tank` → `assist tank hold add` | `+4` | Assist tank 0 |
| `second add needs assist tank` → `assist tank hold second add` | `+3` | Assist tank 1 |
| `nether power active` → `remove nether power` | `+2` | A remover is usually a DPS bot too, and each stack is +20% spell damage for 30 s. Its own node, since class dispels sit under `ACTION_RAID` |
| `dps target` → `dps target` | `+1` | Fires only off the pick, so `false` once on it ([Targeting](#targeting)) |
| `incinerate flesh on raid` → `heal incinerate target` | `ACTION_MEDIUM_HEAL + 5` | Above routine heals, below critical ones (30): the target's health reads full under the absorb, so no generic node picks it. Returns `false` with nothing castable, so routine heals still run |

`jaraxxus threat cap` (Spell and Melee families) is wired from the definition.

## Traps

- **The arena floor is a gameobject** ([README.md](README.md#arena-floor)): no floor point can be
  verified offline, so nothing here fixes a coordinate other than `ARENA_CENTER`.

## Known gaps

- One heroic trace (2026-10-17 25H): the Fel Lightning no-spread held there, untested on 10H.
- Adds on non-tanks while a human assist tank holds the boss: no bot cover, by decision. On the
  2026-10-16 25N kill the Mistress spent 89% of her life on non-tanks; on the 25H kill Mistresses
  100%/40%, Infernals 87%/100%/54%.
- Two tanks taunting him off a DPS holder still burn his taunt diminishing returns; the threat cap
  only makes the pull rarer.
- The class nodes may still spend a cover on a hurt raid before an Inferno is predicted, leaving it
  cooling down at the hit.
- No cover against a Volcano's Fel Inferno or for the Kiss (the guide's Divine Sacrifice and Aura
  Mastery with Fire Resistance Aura, Aura Mastery with Concentration Aura); melee take Fel Inferno and
  the Volcano pulses.
- A holder mid-cast at the window's start only casts once the pinned-cast break lands, up to a tick
  late; one dead or out of reach at its window is not replaced.
- The Incinerate target uses no healthstone on its own absorb.
- Assist tank 0 does not wait at the heroic portal, so a Mistress opens on whoever stands nearest, and
  the held melee stand idle until a tank has her or the portal dies.
- Capped melee keep white-swinging: no action carries a swing.
- With a human main tank no bot stands on him through the intro, so the release takes whoever stands
  nearest: an assist tank 8 yd off, unless a human is nearer.
- `toc.cover` and `toc.personal` belong to every ToC boss by prefix, so `batch.py --probes` lists
  them silent on the four that don't call the helper yet.

## What a trace answers

Position, verdicts and moves come from the raid-agnostic streams; `postmortem.py --vetoes` covers
the five multipliers and the five rules. The Jaraxxus rows, under `--notes jaraxxus.` and
`--notes toc.`:

| Key | Shape | Says |
|---|---|---|
| `netherpower` | latch | His Nether Power stacks, the only record: his auras are never traced |
| `focus` | latch | The DPS kill-order add's guid, `0` for none; kept current under a skull |
| `absorb` | latch | The Incinerate Flesh target's absorb left, `0` with none: a heal row reads `a = 0` under it |
| `dps` | per bot | The DPS pick's branch: `skull`, `add`, `boss`, `none`. No reader section |
| `addtank` | per assist tank | The add it holds, `0` for none; absent when every assist tank is human |
| `interrupter` | per bot | `1` holding Fel Fireball duty, only inside that cast: rewritten every tick he lives, so it drops to `0` as the cast ends |
| `flame` | per bot | `carrier`, `relaxed` (nothing clear of raid and boss, so flames only), `tank` (a carrier holding him, flames only), `far` (no clear spot within a healer's reach), `dodge`, `none`. Change-only: counts are switches, not legs |
| `portal` | per melee DPS bot | `hold` while it keeps clear of a heroic portal, `none` |
| `toc.cover` | per holder | The cover dealt for the coming hit, `ToCRaidCover`: 1 Aura Mastery, 2 Divine Sacrifice, 3 Anti-Magic Zone, 4 Divine Hymn, 5 Tranquility; `0` when the deal drops |
| `toc.personal` | per bot | The button picked (spell name or `healthstone`), `covered` (one running), `channel`, `none` |

`tools/botobs/bosses/lord_jaraxxus.py` reads the rest, counting every difficulty id of a row; its
banner names declared probes the pull never wrote.

- `--boss`: each Fel Fireball landed, kicked or neither, by whom, how many ms in, and who held
  `interrupter` inside it; kicks with nobody latched count by bots, with their median delay, and by
  humans. Fel Lightning hits go to the nearest cast either side: an instant with no travel time
  (Speed 0) writes its hits 0-3 ms before its `cast` row, one with a travel speed lands after it,
  late by the flight time. Hits per cast against the chain cap, damage taken with absorbs and its
  share of the raid's, the lowest health a hit landed on and left, lethal hits, deaths with it in
  their last 5 s and, on heroic, those that reopen the spread call; per cast, the first victim,
  damage taken, the longest jump between sampled victims against the 13 yd reach, and his Nether
  Power stacks.
- `--nether`: the stack timeline, seconds at 1+ and at full; Spellsteal, Purge and Dispel Magic at
  him, those cast while stacked and those the count fell after; Nether Power landing on a mage.
- `--incinerate`: per window, the target, how it ended (healed, expired, died), heals landed on the
  target, the absorb it started with and had left, Burning Inferno in the 6 s after.
- `--flame`: per carrier window (debuff and trail joined), yards walked, legs issued, nearest other
  member and the boss against the carrier's 8 and 15 yd (not judged when every value it switched to
  is `tank`, or with no switch the one latched at its start), `flame` switches; flame-tick damage
  per bot, with the nearest sampled flame.
- `--adds`: per add, first and last sample and the share of samples on a non-tank; `focus` and
  `addtank` changes; each Mistress' arrival with her portal, the melee inside its 10 yd, her arrival
  hit, first victim and the `portal` holds then; each Mistress' Kiss, how it ended, its punish and
  `kiss cast hold` vetoes.
- `--cover`: per Burning Inferno against its first tick, hits, damage and deaths; the raid cooldowns
  cast within 5 s either side, the `toc.cover` deal, the personal buttons and `toc.personal` picks,
  and the Evasion or Deterrence spent; how many were covered by 1 s past the first tick.
