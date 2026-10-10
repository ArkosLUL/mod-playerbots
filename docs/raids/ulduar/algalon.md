# Algalon

One AI for 10 and 25-man, no heroic. Difficulty-mapped: Big Bang `64443 → 64584`, Black Hole Explosion
`64122 → 65108`, Quantum Strike `64395 → 64592`, Cosmic Smash `62311 → 64596`, Arcane Barrage
`64599 → 64607`. Phase Punch (64412) and the phase auras are not, so the strategy is difficulty-agnostic
apart from the Big Bang id pair and the star team size.

Every timer is offset by an intro that runs **26 s on a first pull and 8.5 s afterwards**, with him
unselectable throughout; `IN_PROGRESS` lands 14 s / 1.5 s after the engage. Nothing counts from combat
start: the first Big Bang is predicted 90 s after `UNIT_FLAG_NOT_SELECTABLE` drops and re-latched on
every cast. The room is a **47 yd disc** around `(1632.668, -302.7656)` with a floor at `z >= 410`;
leaving it casts Ascend, so nothing may pull him or a tank past the edge. His despawn timer is
**300 min** in this fork (`ulduar.h`), started by the first engage.

| Mechanic | Ids and cadence | Handling |
|---|---|---|
| His melee | combat reach 9 (model 28641), so **11.83 yd** on a player; runs 15 yd/s | Nobody but the swap tanks inside it |
| Quantum Strike | 64395, every 3-4.5 s, 15.7-17.3 k / 34-36 k physical | Two tanks or nothing |
| Phase Punch | 64412, every 15.5 s, 45 s aura, 5th stack phases the tank 10 s (64417) | Swap at **4** |
| Collapsing Star | 32955, 117 k / 234 k, every 60 s, tops up to 4 alive | Timed to die under Big Bang, else the star team kills one at a time; each death is Black Hole Explosion on everyone unphased, 16.1-16.9 k / 20.5-21.5 k shadow |
| Black Hole / Worm Hole | 32953 / 34099, 6 yd field (62168 / 65250) pulsing every second, no target cap | Phases for 10 s plus the 62169 DoT (~17.5 k) |
| Cosmic Smash | markers 33104/33105, impact ~4.8 s later, 1 / 3 meteors, 41.4-43.6 k / 53.6-56.4 k fire | `< 6` yd full, `6-10` `dmg/dist*2`, `>= 10` `dmg/dist` |
| Living Constellation | 33052, 11 total, 3 activate every 50 s | Handler collects them, spends spare holes on them |
| Big Bang | 64443 / 64584, every 90.5 s, 8 s cast | 76-89 k / 107-113 k **physical** at 50 000 yd; pierces immunity, Divine Shield included |
| Phase 2 | at 20 % HP | Stars, constellations and holes despawn; 4 Worm Holes spawn on the fixed square |
| Unleashed Dark Matter | 34097, one per Worm Hole per 30 s, random target, never despawns | Handler collects them; 10 yd/s, so tanked, never kited |
| Ascend | 64487 at 6 min | Also the zero-target Big Bang path |

**His hits are retail, not inflated.** Upstream PR #26675 (`2026_07_18_04.sql`) raised `DamageModifier`
35→190 / 70→285, so a 25-man main hand rolls 66.9-92.1 k before armor, off hand half, both every 1.0 s;
23 pulls measured 53-61 k per swing before armor against Wrath Classic's ~66 k. Spells are stock Spell.dbc,
and mod-dungeon-scale only adds ×1.2 health (41.8 M, retail by chance). Tank deaths are a healing problem.

## He resets the moment nobody unphased is left

`EnterEvadeMode` looks for an **alive player in his phase** within 120 yd, and `Creature::SelectVictim`
evades once every threat entry is phased. Either way he goes `FAIL`, despawns and is resummoned 2 s later
under a new guid. A corpse is no Big Bang target either. Every Big Bang therefore needs someone alive
and unphased until the others return, and the whole strategy is built around that.

**Nothing may depend on one bot's view of the room.** `"nearest npcs"` and `"find target"` are phase
filtered: a bot inside a hole sees neither Algalon nor the holes. So Algalon comes from
`instance->GetCreature(ULD_BOSS_ALGALON)`, and stars, holes, constellations, markers and Dark Matter
from one room scan per instance every 250 ms, run with Algalon as the searcher so it sees the room's
phase whoever ticks. Markers are dropped 5.5 s after they appear: the creatures live 10 s, the meteor
lands at ~4.8. The wipe reset keys on the instance too: boss gone, guid changed, or out of combat
after being in it.

## Big Bang: a shadow priest soaks

Every guide keeps one player out. The **soaker** is a bot whose Dispersion (47585, −90%, 6 s; 120 s
cooldown, 75 s glyphed against the 90.5 s cycle) is back by the time it casts it, ≤ 5 s before the hit; a
second one is the **backup** and stays out too. Inside it a priest took ~6 k of the hit, so **both swap tanks
hide** with the raid and keep their buttons for his swings. For the cast and 3 s after it he skips
`UpdateVictim`, can't move, and holds Quantum Strike and Phase Punch (`boss_algalon_the_observer.cpp:649`,
`:754-764`): a hiding holder stays his victim and nobody is swung at. Outside the cast the tick latches the
next hit's **disperser**; that latch picks between this and the tank soak below. The soak is latched per cast
too, and every role reads that latch, never the live cast: in the tick's first 250 ms of a cast nobody is the
soaker yet, and every bot would run for a hole.

Dispersion is checked by spell id and cooldown: `CanCastSpell` refuses during a channel, and a shadow priest
is nearly always in Mind Flay. The button follows what the bot can cast, not its slot. The disperser skips the far-hole approach; no external or prep goes on it. Mid
cast a dead or phased priest soaker is replaced by the backup, else the bot swap tank with ≤ 3 stacks, whose
run into a hole is stopped. A swap tank or handler that can't be in its hole 1.5 s before the hit (one field
pulse and slack) stays put (`noshelter`) and casts its physical button at ≤ 3 s.

Big Bang's second effect applies **64445, which strips every phase a second after the hit**, Phase
Punch's included; it ignores phase, so it reaches everyone. The soaker is alone for about a second. Back out
of the field, a healer's first cast for 4 s after the hit goes on the soaker while under 60%, instants first,
Lay on Hands under 30% (Forbearance only blocks a self-cast).

**Without a disperser a tank soaks**: his victim at cast start (the pickup tank if that is no swap tank). It
casts `NextTankDefensive(..., physicalOnly)` at ≤ 3 s remaining, skipping Anti-Magic Shell: the hit plus his
next swings took a tank with none from full to 21-24%, and the swing 1.2-2.6 s later killed it while the
healers were still phased. Then:

- A human holder with no physical button for the hit (none back in time, none running past it) hands it to
  the bot partner if that has one and ≤ 2 stacks. The elected soaker taunts him off the other swap tank,
  Righteous Defense included. Mid cast the bot partner (≤ 3 stacks) replaces a lost soaker before the backup.
- A disc or holy priest puts Pain Suppression or Guardian Spirit on it before hiding, a paladin Hand of
  Sacrifice. Healers spend one global cooldown on it before hiding (shield, renew, rejuvenation, lifebloom,
  riptide, earth shield, sacred shield): only what is already on it ticks in the second it is alone.
- A bot holder with no physical button back by the time the soak needs it (`ReadyTankDefensive`: cooldown
  and, for Divine Protection, Forbearance run out in time) hands him to a bot partner that has one and ≤ 2
  stacks, 15 s out. A holder at 3 stacks with the cast ≤ 15 s away hands over to a partner at ≤ 1: the
  Phase Punch behind the cast lands on the soaker, and a 5th stack leaves nobody unphased.
- The soak buttons are reserved (see "Cooldowns").

Humans get two raid warnings: at cast start, who soaks (a human holder handed the soak is named: "Dragon,
hide too: Bulwark takes it") and that everyone else should stand by a hole; with 3 s left, everyone but the
soaker into the hole and stay in it. Inside for the whole cast they are fed to the phase-16
Dark Matter, and stepping out early lets the 10 s phase lapse before the hit.

Everyone else keeps fighting until the walk to its hole plus 2.5 s is all that is left of the
cast (`wait`). Its hole is the nearest one not claimed by the kite, and the wait, the run and the far-hole
approach all read that same hole: timed off a nearer kite hole, 13 bots left 2.4-4.7 s before the hit for one
35-52 yd away and died on the way: the phase only has to cover the hit, so hiding early buys nothing, bar a star
dying under it (Stars). The run breaks a cast
pinning the feet, re-issues a stalled walk and holds its tick while walking; an `Exclusive` rule keeps
every other mover off the hider and a `Block` stops Charge, Blink and Disengage. **Once phased, step
8 yd off the hole**: still inside when 64445 strips the phase means phased again within a second for
10 s, and the hole is invisible from inside, so the exit reads the cached position. Phased bots are left
out of the `Exclusive` because phase-16 Dark Matter (33089, 8 static spawns on the star points) attacks
them in the void. Until 8 yd out only the exit runs (`AlgalonLeaveFieldMultiplier`; an `Exclusive` holds
movement alone): a Chain Heal started on phasing and a mana gem stopping the walk left 11 bots phased again.
A hider whose walk or hole lies within 15 yd of a meteor landing before it would be phased waits where it
dodged until the impact, unless that leaves less than its walk plus 1.5 s (the hide walked two back into one,
44 k and 50 k). The exit, the hide run (not over a meteor dodge under way) and a taunt drop a movement booking another
action made, since an equal priority waits it out.

A bot only thinks again when its cast ends, so during the cast a cast or channel longer than its slack is held
(`AlgalonHideCastMultiplier`): a 1.9 s Shadow Bolt started just before the slack ran out made a warlock late.
Ice Block, Divine Shield, Cloak of Shadows and Anti-Magic Shell are held 20 s before the cast, through it and
while phased ("algalon phase immunity hold"): magic immunity strips 62168 (arcane) or keeps it off, and two
rogues who Cloaked inside were hit. The hide also cancels Ice Block, which pins the mage.

**Nitro Boosts** (boots enchant 3606, use 55004, +150% for 5 s, 3 min; `EncounterHelpers`): bots whose boots
are ready at the cast start are latched, time their walk boosted (never under the 10 yd walk unboosted), use
them on a run over 10 yd (a refused use retries next tick; the latch drops once they can't fire) and skip the
far-hole approach. The swap tanks have none.

**Threat survives the hide, and this is not a bug to re-audit.** `CombatManager.cpp:53` gates only
*entering* combat on `InSamePhase`, and `ThreatManager` only marks phased entries offline: he re-picks
the top unphased entry when the phase drops.

## Stars: under Big Bang, else paced

**Stars die under Big Bang** ("killing stars under"): Black Hole Explosion only hits its caster's phase, so one
dying while the raid hides hits the soaker alone, ~2 k in Dispersion. Across three pulls explosions killed 42 of
77, each lost 8-13 to one dying after the first cast, and with 4 healers the raid never refilled between them.
The drain is exact: Collapse (62018) deals `CountPctFromMaxHealth(1)` a second on a clock from the spawn, so the
scan reads health, that tick and the time to the next (`GetPeriodicTimer`) and knows the second it dies
untouched. The window is hit − 4 s to hit + 0.6 s (64445 strips the phase a second after the hit), re-anchored
on the real cast and kept until 1.2 s after the hit; a star's band is the health (lo, hi] that dies on a tick in
it. It can't open sooner: the tanks hold him until the cast starts or he resets, the soaker's Dispersion (up
≤ 5.4 s before the hit) lasts 6 s, and each second in the void costs everyone 1.3-1.9 k of 62169.

| Wave (T0 = intro end) | Spawn | Untouched death | Plan |
|---|---|---|---|
| 1 | 16.5 s, 4 | 116.5 | one shelter kill, 3 under Big Bang 1 (hit 98), ~18.5% extra each |
| 2 | 76.5 s, 1 | 176.5, before Big Bang 2's hit (188.5) | under Big Bang 1 (~78% in ~20 s), else paced |
| 3 | 136.5 s, 4 | 236.5 | under Big Bang 2, ~48% each |
| 4 | 196.5 s, 4 | 296.5 | under Big Bang 3, ~17.5% each |

- **Plan**, per tick outside the cast, only with a disperser latched (a tank soaker would take each explosion
  whole), started afresh at each cast: a new star joins if its drain death isn't before the window and the
  team's work ends 5 s before the cast (at 6 %/s, or the measured speed when lower, less the drain, which moves
  the band: the bulk's own, averaged over a span from past its first casts and dropped after a cycle with none,
  else a paced kill's), smallest first; the rest are paced. **A planned star stays planned**:
  re-planned each tick, all were dropped 6 s before the cast, and three died together after the strip and killed
  18. It is released for good (`algalon.underdrop`) only `early`, dying before hit − 5 s (the stretch's reach);
  `late`, between cast −15 and −8 s once its bulk, behind every star the bulk takes first (latched, then most
  left), has overrun the cast for 1 s, so pacing kills it first; or
  `nosoak`, no disperser for
  2 s (a shorter gap is a phase or a re-election).
- **Shelter**: with no hole at all, the star nearest the hold spot is paced dead before the cast.
- Pacing and `urgent` ignore under-stars (`urgent` takes one only when nothing else is left).
- **Bulk** (`algalon.starwindow` `under`): with no paced kill, the team takes an under-star to half a tick under
  its band's top and stays on it until done (re-picked each tick, it moved every ~3 s and only matched the drain),
  the one with most left next; a bulk starts once the work left plus 35 s reaches the cast. A spell landing after
  the line is 1-2% of a 4-5 tick band; one overshooting below it is released `early`. No wands: they took
  0.2-0.3 %/s each, a damage dealer off him against the enrage, and a far star nobody could wand died after the
  strip and killed 5. The DoTs' ticks to come count as done (they tick on once the team stops); at the line the
  tick interrupts any cast, channel or wand on the star, a paced kill's focus excepted, and the target guard holds
  a new DoT within 10% of it (Curse of Agony is ~6%; a nuke with a DoT rider like Fireball is a nuke). Holding
  DoTs and channels outright, the bulk ran +1.9 %/s beyond the drain against +5 planned.
- **Hide**: with a star under, every margin (handler and tanks included) gains 3 s, stretched so all are in
  1.5 s before the earliest drain death, capped at 4 s; the handler no longer waits on stragglers.
- Phased bots keep 8 yd off an under-star in its last 3 s (exit and leave trigger): its hole lands under them at
  the strip.

**Paced**, the rest:

Collapse drains **1 % of max health per second**, so an untouched star dies after ~100 s, and the 60 s
summon only tops up to four: four ignored stars explode within seconds of each other. A latched **star
team** of 2 damage dealers (3 on 25-man), ranged first and melee only to fill a raid short on ranged,
kills them; the target exclusions keep every other bot on Algalon, and an `OwnTargeting` rule keeps
`dps assist` from pulling the team back, since a star is no attacker until something hits it. The team
is cleared (`algalon.focusstar`) onto a star only when its death is **planned**. Splash near him takes
1-2.4 %/s on top of the drain, so a star's forecast death is its health over its measured loss (the slope of a
4 s log, never under the drain), not its health. A kill lands at its health over 9-5.5 %/s (whole kills ran
6.2-7.8 %/s on 25-man) plus the team's walk (mean distance past 28 yd at 7 yd/s) and a 2 s start (every kill's first
2 s only matched the drain; none for a star already losing 3.5 %/s), or over its own slope once the team is on
it. Any other star is planned up to the speed the team last showed on its focus (capped
at 12 %/s, `…_STAR_KILL_PEAK_PCT_PER_SEC`: a last 15% once went at 16 %/s, but only for a second): one
just dropped keeps dying that fast, and planned at 9 %/s it was opened again every other tick.
A death is **clear** outside 1 s before Big Bang to
3 s after the hit and 4 s before to 5 s after a meteor impact (6-11 k on everyone, then the explosion: 3-4 died
each time one landed 0.9-1.3 s after), and 8 s from the last explosion and every other star's forecast death.
Meteors are planned off the clock, not only their markers (4.8 s ahead): the first lands 30.9 s after the intro,
then every 25.5 s (`Repeat(25s + 500ms)`, ±0.15 s); a sighting resets the next one and the two after are
forecast too. The serial deadline sums blackouts merged, or it counted one meteor six times (three impacts,
two marker creatures each) and turned far stars due. In order, `urgent` overriding all:

- not during the Big Bang cast (the raid is running for holes);
- the kill under way continues while its finish stays 6 s clear, or while dropping it wouldn't help: dropped,
  a star keeps dying at its own slope, capped at 5 %/s, from casts in flight, channels and DoTs (22 % went in
  3.8 s), as one did into an impact and one 1.5 s into a cast; one barely hit only drains. So a kill is dropped
  only if the star would then die clear, or sooner than the kill lands. Its log is kept: a flat 5 %/s made a star
  never hit look due, and the team was sent back to it 4 times in 2 s;
- not within 8 s of the last explosion. A "finish stars under 15 %" bypass put two explosions 3.2 s apart
  and killed 12;
- a **due** star is opened whatever the raid's health, soonest death first: its own death isn't clear and is
  ≤ 30 s off (a fresh wave forecasts four deaths together; the next rule paces those), or it dies no later
  than an i-th soonest within i × 16 s (a gap plus a kill each) plus the walk and any blackout before it. Never one within 9 yd
  of a swap tank or the handler (a hole under the holder sets him loose; it did twice), given a choice not
  within 16 yd of him, and only with a clear finish. When the soonest can't be touched, nothing lands near
  its death: killing another instead put two explosions 0.2 and 1.3 s apart and killed 30. With no due star
  clear, the one landing furthest from the rest is still taken if that beats their own deaths;
- not while any living, unphased raid member is below the explosion's top roll (16.9 k / 21.5 k), or
  below full health if their maximum is lower. A percentage gate can't work: 80 % of a 26 k clothie is
  below one 25-man explosion;
- the star soonest to die at least 7 yd from every raid member, since its hole lands where it dies and
  phases whoever is in the field, 9 yd from the swap tanks and the handler, and 16 yd from him (his melee
  ring), with a clear finish. The star the team is already on keeps it down to 5 yd from the raid, so a
  drift past the edge doesn't flip the team between stars.

A wave's four stars spawn and drain together; without the deadline the gates could hold until all four went
off in the same second. `urgent` takes the lowest star anywhere.

**Splash breaks the pacing.** Stars died chewed, 75% gone in 11 s with nobody sent, two 1.4 s apart, from
Starfall, Blade Flurry, Death and Decay, Pestilence diseases, warrior cleaves, Divine Storm and Avenger's
Shield; the Aoe threat type covers almost none of them. `AlgalonSplashHoldMultiplier` holds Starfall, Blade
Flurry, Sweeping Strikes and Bladestorm all of phase 1 (self auras that hit whatever comes near; one started
before a wave spawns still runs when it lands). While a star lives, other splash (`SpellSplashes`,
`MULTI_TARGET_ACTIONS`, HotR, Avenger's Shield, Seed of Corruption, Mind Sear, Living Bomb, Corpse Explosion,
the Aoe threat type) is held while a star is within 15 yd of the bot or its target (glyphed Pestilence; diseases
landed on one 12.5 yd out), and ground AoE that sits 8-10 s while stars wander in within 25 yd of where it
lands: Consecration, Magma Totem and Explosive Trap round the bot; Death and Decay, Blizzard, Flamestrike,
Hurricane, Rain of Fire and Volley round its target. The star the bot is hitting doesn't count; friendly spells
are never held. Humans' Consecration, HotR and Death and Decay still chew them.

**The team works a far star from a spot of its own**: from its ring slot, `reach spell` walks half the gap and
the slot node walks it back, so a deadline star 35-68 yd out drained untouched. A ranged team member whose focus
star is over 28 yd from its slot stands 25 yd from the star towards the slot (`star`), outside his reach.

**A star at 5% or less, or forecast dead within 4 s, is a hazard** for the dodge: anyone unphased within 8 yd steps off it, since it dies
within seconds whoever is on it (`algalon.hazard`). As with a meteor, the generic movers are held within 12 yd
of it and a slot within 8 yd of it waits (`smash`).

The tracker marks the focus star with the **star** icon for humans; skull would pull every bot's
`attack rti target`. A skull a player puts on a star or add wins over the exclusions.

## Holes are a resource

Holes exist only where a star died (it wanders 25 yd from its spawn first, so anywhere in the room), and
every constellation eats one. With a Big Bang ≤ 30 s away one hole is kept out of the kite, never the one
nearest the hold spot (the raid's; a latched kite hole there is dropped), and
`urgent` (no hole, cast due or running) puts every ranged DPS on the star team. A slot buried under a
hole is stepped aside from the slot itself; a bot standing in one outside a cast leaves it.

A bot phased outside a cast is alone in the void with the phase-16 Dark Matter, which kills a healer in
about 3 s. So no walk may clip a field: dodges and exits reject ground whose straight path passes within
7.5 yd of a hole, and a bot already beside one may not walk closer to it. Only a bot inside a field, or
phased, may cross the one it is in, no other. When every path is blocked the dodge and the exit first take
one kept 6.3 yd off every hole (`field`, just outside the 6 yd field), and only then one across a hole: the
void is still better than a meteor or staying phased. A slot walk with a hole in the way goes round it
via a point 9.5 yd off the hole that has a clear line on to the slot (`blocked`); phased bots too, all but
the hole they leave: Big Bang's strip can land mid-walk, and a tank healer walking 5 yd past a hole was
phased again for 10 s while its tank died.

The generic movers know nothing of holes, and holes land beside him where melee stand: 34 stray phases in
four pulls came off `reach melee`, `set behind` and `reach spell`. `AlgalonWalkGuardMultiplier` holds reach,
reach-to-heal, set behind and rear flank when the stop point (melee range of the target, 28 yd for spells, both
set-behind spots) is within 7.5 yd of a hole or the walk passes within 6.3 yd of one; for a phased bot within
8 yd, the hole it left included, since reach walked melee back in before the strip. A held bot waits; holes
live until a constellation eats one or phase 2.

**A far hole is approached before the cast.** Within 15 s of it or during it, a bot whose hole is over
35 yd from its own spot (a melee bot's is him) waits 30 yd from that hole (`nearhole`; with a star under, over 22 yd and at 22, `…_UNDER_HOLE_REACH`, since from 28-34 yd the walk alone took up to 5 s), with the generic movers
held: three ranged 45-64 yd from the only hole couldn't make it in 8 s. With that spot in his reach (healers
ring him), the nearest point 30 yd round the hole that isn't. Swap tanks and the handler are left out.

## Constellations: the handler collects them

They never melee and Arcane Barrage hits a random raid member, so a constellation costs damage only
while alive, and who it chases matters only at Big Bang: it follows its victim into a hole and closes it
on the raid. They are killable (260 k / 521 k) but 11 of them cost 16-34 % of Algalon's health against
the enrage, and guides agree nobody damages them. The **handler**, a third tank or else the swap tank
not holding Algalon, and always a bot, taunts every loose one onto itself. One counts only while its script
has it active: one spawned 53 yd out and 26 yd up never came below 16 yd, evaded home ~8.5 s after each wave
woke it, still selectable, and held the handler on `taunt` 80 s with the rest held and holes spare. More than 5 yd
above the floor, where no hole takes one (2-23 s of descent after waking), it isn't kited, nor taunted outside
the gather. At Big Bang it **hides last**,
since they follow it in and close the hole (65509, 6 yd) while those already inside stay phased: 62168 is
owned by each target, and a despawning caster strips only area and single-target auras. It takes its kite
hole, nobody else's, else its shelter, and (with no star under) waits there (`hold`) until every hider bound for that hole is in
(bots by shelter, humans by the nearest hole, kite hole included), or until its walk plus 2 s is left: a tank
is caught out under its walk plus 1.5 s, and with the same threshold a 250 ms tick read that first. From 10 s
before the cast it only taunts (`gather`), keeping its kite hole unless that became the raid's: a constellation
keeps its victim 9.5 s from the cast start, and one chasing a healer closed a hole a ranged was walking to.

**A spare hole eats one, only if it comes across the hole at the handler.** It chases at 7 yd/s (run speed),
rests 3-3.5 yd off its victim, and is eaten within about 6.5 yd of the centre, never at 7.4-9.8 yd even after
20 s there. Parked 9 yd past the hole and circling it at 9.5 yd, the handler kept them trailing outside the
field; in a pursuit simulation circling with them behind never got one eaten. But Arcane Barrage is a 2 s cast
every 5 s, standing still, and a wave casts in sync: starting round at their cast, they cut across at 2.4-4.3 yd.
So, per handler (`algalon.kite`), with the walk going round any hole it would clip:

- `stage`: to 11 yd out on their side (from the centre of the held ones);
- `wait`: there until all of them cast, or any after 6 s;
- `cross`: round to 8 yd past the hole, just outside the 7.5 yd that phases a player (straight there when
  already past it from them);
- `parked`: waits; one resting beside it outside the field after 3 s means the pass missed, and it stages again.
  A walk still under way after 12 s starts over.

A spot clamped into the room (44 yd) must stay 7.8 yd from the hole, on its side, and 8 yd off every other
hole, else ±30°; a hole needs both spots. A loose one is taunted from the far spot of the hole nearest it
(`behind`) when that is within 28 yd of it, the handler holding there while the taunt cools, so it comes straight
across; not while gathering or with no hole spare. Loose ones go first only while the class taunt is ready:
taunted one per 8 s after every swap, they held the kite 71 s of a pull. With it cooling, held ones go to a spare
hole if one fits, and a pass past its stage walk is finished first (taunting from behind latches a new kite and
drops it). Never kited: the raid's hole with Big Bang due, and the only
hole within 35 yd of the hold spot, the swap tanks' reach. The walks run at combat priority: a straight forced
walk crossed the field, and its booking refused both the tank slot walk and the exit when the handler took him
as Big Bang began. Its movement rule still passes its taunts, the pickup and the swap: all are
`AttackAction`s, and a swap it can't make phases the holder out. A pickup or swap due on it lifts the rule, the
kite and its constellation taunts, and the slot walk takes it back to him: kiting 50-60 yd out, it never came
back and the holder died at 4 stacks. Constellations are excluded from
everyone else's pickers.

## Phase 2

The Worm Holes are the Big Bang shelter. Unleashed Dark Matter picks a random player; the handler
taunts the loose one hurting the most fragile victim first, everyone else is kept on Algalon, and AoE
is free with no stars left. With no bot handler, Dark Matter is left to whoever it chases.

## Cosmic Smash

The dodge clears **15 yd** from every marker (10 yd if nothing clears), 8 yd from every hole, stays
44 yd inside the room and prefers ground near the bot's slot. It fires anywhere inside 15 yd, and an
`OwnMovement` rule holds every generic mover for bots within 19 yd of a live marker: melee started the
dodge within 0.3 s, then `reach melee` walked them back to 6-12 yd before the impact. Melee charges
(Charge, Intercept, Feral Charge) are blocked there too, and wherever the bot stands while its target is
within 15 yd plus that target's reach of a marker: he can stand on the impact, and a cat's Feral Charge
from its slot 27 yd out landed behind him, 1.9 yd from it.

## He is held at the edge

At the end of the intro the core makes him `AttackStart` a **random player** within 150 yd, and he does
the same to whoever is next on threat whenever he loses his tank. Anyone inside his reach is one swing
(25-67 k on cloth). Held mid-room he sat 20-24 yd from all four star spawns, so stars drifted past the melee
and got cleaved; he is walked out to the −Y edge, bearing −82° (the middle of the gap between the two spawns
there):

- **Pickup**: both swap tanks wait 8 yd from his landing spot, in his reach, and the pickup tank taunts on
  the first selectable tick. It keeps that spot until a swap tank holds him, so a missed taunt doesn't send
  it walking away from him while he is loose, and the pickup's holder (a pair tank holding him within 3 s of
  the intro) 3 s more: facing him it builds threat and blocks, while on the walk out every swing lands on its
  back.
- **Hold**: the holder walks to the tank slot, 38 yd out, and drags him there. A chase stops 1.5-12 yd short
  of its target (0.5 + his reach 9 + 1.5), so he stands 27-32 yd out, about the hold spot at 30. The other
  swap tank's slot is 4.1-7.2 yd from him anywhere in that range, so a swap taunt never moves him; with him
  held elsewhere (a human holder, the walk out) it stands 7 yd from him towards the tank slot.
- **Rings** around the hold spot at 19 yd (4 healer slots), 25 yd (6) and 31 yd (8), on the room side, each
  ordered to alternate sides so a short raid still spreads.
- **Melee** wait 15-17 yd from the hold spot, either side of his path, until a swap tank holds him and either
  stands on the tank slot or 8 s have passed since the intro; not onto a loose boss unless 20 s pass. An `OwnMovement` rule
  and a Charge block keep reach and charges from taking them in early.

Every ring slot and waiting spot is 13.5 yd from the whole line from his landing spot to the tank slot (his
landing, his drag and any stop point), at least 8 yd from the four worm hole spots and within 44 yd of the
centre; healers are within 22 yd of the tank slot. Ranged at 31 yd are still in range: spell range adds
both combat reaches. All 36 points, including probes 40-44 yd out on that bearing, are navprobe-verified,
flat at Z 417.321.
The formation yields while a marker is within 15 yd of the **slot**, not the bot: a bot that already
dodged passes trivially and walks back under it.

## Tanks: the swap pair

The pair is **two bot tanks when the raid has them**, so a human tank is free to do what they like;
otherwise the group's main tank and first assist, one of whom may be human. It is re-derived until the
pull and latched at engage; during the pull a member who dies or leaves is replaced by the next living
tank, bots first, or the survivor eats the 5th Phase Punch with nobody to take him. Bots swap by the
tank not holding him taunting at the holder's 4th stack, so a human partner never would; the bot holder
raid-warns them instead ("`<name>`, taunt Algalon now") every 5 s while the swap is due, and a human
who is the only one able to pick him up while he is loose is told the same. With a human partner the
bot still taunts him back at the human's 4th stack.

Every swap tank or handler not holding him **yields** while a swap tank does: no damage on him (target guard and
exclusions, a node that drops auto-attack, and a block on `attack rti target` for a skull on him).
After a taunt both tanks build threat at the same rate, so the one carrying the stacks would take him
straight back; without this the bot took him off a human tank by threat within 1-15 s in every pull. A
tank outside the pair would take him from both. The yielding swap tank works the adds and otherwise
keeps its slot in his reach. Its threat then trails the melee, so when the holder drops him his next
swing can land on a damage dealer until the pickup taunt lands.

The pickup prefers a bot, then fewer stacks, but never a bot already at 4 stacks over a human with
fewer. The class taunt nodes are blocked for everyone while he is engaged: an Arms warrior's snare chain ends
in Mocking Blow, which `ClassifyAction` now counts as a taunt, and one killed its caster. The swap, pickup,
handler and Dark Matter nodes taunt through `CastClassTaunt`; when the pickup's taunt missed or is cooling down
while he is off a swap tank, the class's second one follows (Righteous Defense on his victim, Challenging Shout
or Roar), since a missed Hand of Reckoning left him loose for 1.5 s. **A taunt moves him, not the taker's own
target**, so every take ends in `Attack(him)`, and a holder that got him by threat with another target fires the
pickup node, which then only retargets: a former handler kept its constellation, so `reach melee` and the slot
walk took turns, the target guard zeroed every attack and he hit its back (20 k/s against 12 k), killing a
holder in four of six pulls. The handler is a bot tank outside the pair, else the pair bot not holding him.

The class "on main tank" redirects stay off for the whole fight (`UldThreatRedirectMultiplier`). For 20 s
after the intro, hunters cast Misdirection (100 yd) and rogues Tricks of the Trade (20 yd, else they wait)
on the swap tank he attacks, never before: he took the other tank, and a Misdirection cast in the intro on the
pickup tank pulled him off at 10-11 s, both pulls. The class Tricks node is blocked all
fight: out of the tank's 20 yd, or with the rogue low on threat, it tricks the strongest melee dps in reach, and
that DK pulled him in 7 of 8 pulls 13.7-21.2 s in.

## Healers: tank healers on the swap pair

Healers pick the lowest health %, and every explosion puts a clothie below a tank losing ~15 k/s: over 71
tank deaths in 23 traces the tank got a median 31 k healing and 15 k shields in its last 6 s against 90 k
taken, while the same healers put 67 k into others. Main hand, off hand and Quantum Strike can land inside
1.5 s, so only what is already on him or an instant saves him.

The tick latches **tank healers**, bots only, ranked holy paladin, discipline priest, resto druid, resto
shaman, holy priest: 2 on 25-man, 1 on 10-man, never more than half the living healers (humans counted),
since the star `raidhp` window waits on the rest topping up the raid. Re-derived until the pull; in it the
dead or departed are replaced and the list trimmed. A phased one stays: Big Bang phases them all.

A tank healer heals the holder (else the lower pair tank), itself under 50 %, then the partner:

- under 50 %, and the holder under 70 %, its fastest spell at `ACTION_RAID + 9`, ahead of the slot walk.
  A holder went from full to dead in 1.3 s (two main hands, Quantum Strike, an off hand: 50 k), after the
  paladin spent its global at 65 % on Judgement. The paladin's order is Holy Shock, Flash of Light only
  if Infusion of Light made it instant, Holy Light, Flash: hard cast, Flash heals a third of Holy Light;
- else under 95 % holder / 85 % partner (the partner first only when 15 % lower), shields and HoTs first
  (Sacred Shield follows the holder), then efficient heals, at `ACTION_MEDIUM_HEAL + 0.75`, under the mana
  tools and, on purpose, Judgement of Light (its procs are a quarter of the paladin's healing).

Over the first 3 pulls tank deaths with under 5 dead fell from 26 in 23 pulls to 2, and those with every
healer up from 37/71 to 1/9.

Never a button the tank cutter sequence spends, nor a cast outlasting its hide
slack; the Big Bang nodes still win. A holy paladin keeps Beacon on the partner. `AlgalonTankHealerMultiplier`
zeroes its party heals and any heal or reach-to-heal on anyone but the pair and itself, and a `Block` stops
the class Beacon, Sacred Shield, Earth Shield and Divine Hymn nodes for it.

Not `"focus heal targets"`: it is a combat strategy saved to the DB, so a bot leaving mid-fight keeps a stale
filter and heals nobody afterwards, and it overwrites a human's setting.

## Cooldowns: the tanks first

Each swing is 8-23 k and Quantum Strike ~10 k on a ~44 k tank, so from 60% one burst kills it in 1.3 s, and
every tank death in the traces came 2-6 s after a star explosion or a meteor, with the healers on the raid.
The class nodes reach a tank late: Pain Suppression and Hand of Sacrifice at ≤ 10% ("protect party member"),
Divine Protection only as Divine Shield's fallback under 25%.

**One damage cutter at a time on the holder** (his victim, a swap tank, unphased; humans too), from
`ACTION_EMERGENCY + 2`, never from the cast's start until 3 s after it (he swings at nothing), unless the
holder soaked it. With none running (`RunningTankDefensive` or a Pain Suppression, Hand of Sacrifice or
Guardian Spirit aura) the next goes up:

- `window`: from 1 s before a raid-wide hit (a star forecast to die, a meteor's impact) to 5 s after it,
  back to back, while the holder is under 80% and no Phase Punch swap is due (by stacks, so a human partner
  counts): at any health, 10 of 35 went on tanks at 76-100% and none were left for the deaths after 2:20;
- `low`: under 60%.

The first provider able takes it, cheapest first: the partner's Hand of Sacrifice (idle, in plate), the
holder's own physical button (counted while back within a GCD, so the externals wait for it), other paladins'
Hand of Sacrifice (non-healers first), Pain Suppression, Guardian Spirit. Able: a bot, alive, unphased, in
spell range and sight, its `CanCastSpell` passing, and its mana (`CanCastSpell` skips both range and cost;
runes aren't checked), a Hand caster at ≥ 50% without its own Hand on him, and with no disperser latched a
button back by the next hit unless the holder is alone. Each bot asks whether it is first; a cast claims 1 s, so
nobody stacks a second while the aura lands. The caster interrupts its own cast. `loh`: under 25%, over a running
cutter: with none running, the holder's own Divine Protection first (it and a self Lay on Hands share
Forbearance, and a self Lay on Hands locked an unspent one), then Lay on Hands from another paladin, else the
holder's own.

**A human holder is called instead** ([calls to human tanks](../README.md#calls-to-human-tanks)): on the same
`window` / `low`, under the same reserve, when his own physical button is ready. The externals still go out for
him unchanged, so he and a healer can both cover one window. A human Big Bang soaker is called once per cast at
4 s left, a second before a bot presses, for reaction time.

**Cooldowns** (`AlgalonCooldownReserveMultiplier`): Dispersion is held whenever casting it now leaves it
cooling down at the next soak. With a disperser latched nothing else is held, except Divine Shield for swap
tanks (he leaves a bubbled paladin). The holder's own buttons, self Lay on Hands included, and every external on
him are the node's: the class nodes would stack a second on the same dip. Pain Suppression, Guardian Spirit,
Hand of Sacrifice and Lay on Hands on party pass only onto the other swap tank. Without one the soak buttons are held by their own cooldown, not a window:
Divine Protection, Shield Wall, Pain Suppression and the rest cool down 2-3 min against a 90.5 s cast, and a
20 s hold saw every one spent on the way. Swap tanks then never cast Divine Shield or Lay on Hands (Forbearance
blocks Divine Protection 2 min), nor Lay on Hands on others; a **lone holder** (the other swap tank dead or
phased; not its stacks, which linger 45 s after every swap) gets its own and the externals back outside the
cast, never Divine Shield. The encounter's own actions cast directly and pass.

**Explosion cover** (`ACTION_EMERGENCY + 1`): with a star forecast to die within 3.5 s (the slope runs ~1 s late
on a paced kill, and a 2 s lead opened 1 s before one; both covers last 6 s), or at ≤ 5% (a chewed one went from
4.8% to dead between scans), and none up, one per star. Both covers and the personal buttons act only on
a star still alive: the scan lags a death by up to 250 ms, the explosion drops a bot under its line, and 11 of 21
presses and an Aura Mastery went on stars already dead, two bots then dying at the next with theirs cooling.
Neither acts on a star dying inside its Big Bang window (it hits the soaker alone). The paladin due to cover takes
no `window` Hand of Sacrifice (both fired in one tick, the Hand first, and the star died uncovered):

- Aura Mastery from the **shadow aura paladin**, a bot with Aura Mastery that runs Shadow Resistance Aura
  while stars live (a `Block` holds its other auras, released in phase 2). Prayer of Shadow Protection already
  gives 130 and both are aura 143, highest only, so the aura alone adds nothing; Aura Mastery takes it to 260
  for 6 s and recalculates the running aura.
- Else Divine Sacrifice from a paladin bot with Divine Guardian (53527/53530, by `HasTalent`: a passive talent
  is never in the spell map `HasSpell` reads, so it never fired before), ≥ 50%, the holder last (one global). The
  tick removes 64205 from bots at once: left up it splits 30% of the party's damage onto the paladin, and the
  procced 70940 (raid within 30 yd −10/20%, 6 s) stays.

The class Divine Sacrifice node is blocked while stars live and Aura Mastery all fight.

**Personal buttons** (`ACTION_EMERGENCY`, after the cover): 23 of 41 explosion deaths had one ready, and bots
press them only once already low. With a star dying within 1.5 s, an unphased bot outside the swap pair under the
top roll (× 0.8 with Divine Guardian on it or within 40 yd of the Aura Mastery paladin; the largest of 87 hits
under Aura Mastery was 79% of it), times the stars dying within 3 s of it up to 3 (three inside a second killed
18 who were each above one hit), each covered only if it dies before the cover ends, casts the cheapest that
leaves it alive (a channel is broken first: it fails every cast check), once per star and none while one
runs: Shadow Ward, Anti-Magic Shell (by id 48707: the name lookup misses "Anti-Magic Shell"), Barkskin,
Shamanistic Rage, Cloak of Shadows (a 90% resist, not a cut), Icebound Fortitude, Dispersion (never the
disperser's, nor one not back for the next soak), Divine Protection, Ice Block, Divine Shield. The immunities
are skipped within 20 s of a cast. With none that saves it, a healthstone (not on the move: it stops the bot),
else a healing potion, when 3.5 k lands whole and lifts it over the line: warriors, hunters, elemental and
restoration shamans and holy and discipline priests have no button (14 of 37 explosion deaths). Failing both,
the strongest button: the top roll is the worst case, and a druid at 11.4 k died with Barkskin ready (13.8 k
left of the top roll, 9.6 k of the 12.0 k that landed). Phased and above 35%, a bot keeps Barkskin, Shadow Ward, Icebound Fortitude, Shamanistic Rage and Divine
Protection (a `Block`): no explosion reaches it there, and Barkskins spent in the void were cooling at the stars
dying after the strip.

## The trace

Pulls file under `algalon-the-observer`, aliased to `algalon` in `BOSS_ALIASES`.

Raid-wide values, all written by the instance tick: `algalon.phase` (0 idle, 1 intro, 2 phase 1,
3 phase 2, 4 won), `algalon.bigbang` (the cast's ordinal while latched, else 0), `algalon.soaker`,
`algalon.backup`, `algalon.disperser`, `algalon.shadowaura`, `algalon.urgent`, `algalon.holes`,
`algalon.focusstar`, `algalon.handler`, `algalon.tankhealslots`. Per guid: `algalon.slot`, `algalon.shelter`, `algalon.kitehole`, `algalon.starteam`
(urgent recruits aren't recorded), `algalon.swappair`, `algalon.tankhealer`. Derived per bot, each inside the
helper that derives it: `algalon.tankheal` (`holder`/`offtank`/`self`/`range`/`full`, tank healers only), `algalon.hide`
(`none`/`wait`/`run`/`in`/`exit`/`soak`/`backup`/`hold`/`noshelter`; `hold` is the handler waiting to go
last), `algalon.kite`
(`taunt`/`behind`/`stage`/`wait`/`cross`/`parked`/`kept`/`gather`/`nohole`/`none`; older traces `hole`), `algalon.spot`
(`pickup`/`tank`/`offtank`/`stage`/`slot`/`star`/`nearhole`/`buried`/`blocked`/`smash`/`none`), `algalon.hazard`
(`smash`/`star`/`none`), `algalon.dodge` (`idle`/`clear`/`min`/`field`/`cross`/`none`, where `cross` means only a
path over a hole was clear),
`algalon.defensive` (the pick, on change only). `algalon.tankrole`
(`hold`/`yield`/`take`/`call`/`out`/`none`) per pair bot and `algalon.starwindow`
(`urgent`/`under`/`bigbang`/`smash`/`prebang`/`spacing`/`gap`/`deadline`/`raidhp`/`near`/`open`/`none`; `smash`, `prebang` and
`spacing` name what a kill's finish would land on: an impact, the Big Bang window, another star's death) are
computed in the tick; the window is
written on the tracker bot, since the tick runs on whichever bot comes first, with `algalon.starplan` on each
change: every star's guid counter, health and forecast death, soonest first, then the merged blackouts, in
seconds from now (`2136 28% 3s, 2133 85% 83s u | smash 2..11 | prebang 59..71 | under 66..69`; `u` an
under-star at its drain death, `s` the shelter star).

Events: `algalon.holelost` (`kite`/`stray`/`phase2`/`bigbang`, the last a hole lost during the cast), one per hole, skipped when every summon goes at
once on a kill or Ascend; `algalon.stardeath` (`under`/`early`/`late` for an under-star inside, before or after
its window, else `killed` as the focus star, `drained` within 3% of its
drain-only health, else `chewed`, and the nearest raid member's distance; older traces say `collapsed`), the
only record of how a star died, since damage rows cover players alone; `algalon.soakkit`
(`def=<button|dispersion|none> forbearance=<0|1>` on the soaker at the cast start); `algalon.tankdef`
(`<window|low|loh> <tank health %> <tank guid> <spell name>` on the caster); `algalon.cover`
(`aura mastery`/`divine sacrifice` on the caster); `algalon.trim` (older traces: `start|stop <star counter> <hp%>` on the
wand); `algalon.underdrop` (`early|late|nosoak <star counter> <hp%> <ms to cast>` on the tracker); `algalon.personal` (the button's spell name, or `healthstone`/`healing potion`);
`algalon.nitro` (`hide` on a bot using its boots on the
run); `algalon.handover` (`nodef` on a
holder handing him over for want of a defensive); `algalon.pull`
(`inreach=N victim=GUID`) at the tick the intro ends; `algalon.call` (`swap`/`loose`/`bigbang`/`hide`)
per raid warning, sent only when a human is in the raid. Each meteor is written as a `haz` circle (62311 / 64596, the impact point, 6 yd): the sweep
keeps the 40 units nearest its anchor and on 25-man drops some markers.

Slots and the swap pair are latched before any trace opens, so the tick restates them into each new
trace.
`algalon.hide` stays silent until he has been seen, or every earlier Ulduar trace carries a row of it
per bot.

No probe duplicates the generic streams: his casts (watched once engaged; unsampled while neutral
before the pull), Phase Punch stacks and the 62168 / 65250 / 64417 / 62169 auras on players, holes,
markers, stars and constellations as swept `snap.u` rows, and the damage rows. The sweep anchors on an
unphased roster player; on older traces a phased anchor drops the room's units from the rows.

**`tools/botobs/bosses/algalon.py` reads all of it**; `--verdict` is the entry point, one line per
detected failure, and `--deaths` the second, every death classified by the mechanic that killed it,
counted by cause and role with humans starred. Then:

- `--pull`: who stood inside his 11.83 yd when the intro ended, his first victim, how soon a pair tank
  had him;
- `--loose`: every window his victim was no swap tank, with its cause (intro end, 5th stack, holder
  died or phased, threat), the deaths in it and how it ended;
- `--swaps`: the pair, stack timelines, swaps due and their latency, missed swaps, calls, threat steals,
  hand-overs;
- `--hold`: his distance from the hold spot and the centre, the holder and its distance from the tank slot, the
  drag after each pickup and who was in his reach on it;
- `--soak`: per Big Bang the soaker's health across the hit, the kit, the prep casts, the first heal after it
  and every reserved button spent earlier;
- `--collateral`: per star the health lost beyond drain while nobody was sent, and every cast on a star by caster
  and spell;
- `--defensives`: every cutter, Lay on Hands and Guardian Spirit on a swap tank with its health and whether it
  lived 10 s; per tank death its last 6 s of hits, cutters up and `algalon.tankdef`; per explosion the cover
  up and damage covered against uncovered; per explosion death the personal buttons pressed or ready; per
  explosion each bot under its line with its buttons and consumables, each press and cover beside its star's
  death, with verdicts for one after the death, a press while phased and a failed cover with another ready;
  `window` cutters on a holder at 80% or more;
- `--stars` also: per star its plan, window, forecast against actual death, offset from the hit, who its
  explosion hit unphased and its trims; per focus span the loss beyond the drain and the switches between
  under-stars, each release, each under-star's health at the cast against its band (h% then dies h−1 to h s
  later), the median bulk speed and, on older traces, each trimmer's starts and shots fired, with verdicts for a
  release within 8 s of the cast, one above its band at the cast, over one switch per star and 3 Shoot starts in
  2 s; `--constellations` the kite runs (stages, retries, eaten) and
  branch time, both from the elected handler alone, parked stretches with none eaten, a constellation stuck in the air, and
  seconds the kite was blocked (verdict from 5 s); `--bigbang` Nitro Boosts per hider;
- `--taunts`: every taunt at him by caster and role, and whether a swap tank's took within 0.6 s;
- `--tankheal`: the tank healers, each healer's share on the pair, itself and others, Beacon time on holder
  and partner, and per tank death the 6 s / 2 s damage against healing, shields and healing elsewhere, with
  each tank healer's state; on older traces the deaths and shares alone;
- `--bigbang` (holes at the cast, soaker and backup health, who was hit unphased, tank-alone seconds,
  re-phase loops, void Dark Matter damage, evade after), `--stars` (how each died, the window, a live
  meteor, raid health against the top roll), `--smash` (per wave and bot: distance, dodge latency and
  branch, distance at impact, what brought it back: a walk or a charge), `--phasing` (each phase application, Big Bang or
  stray, and the stray cause), `--reach`, `--phases`, `--holes`, `--constellations`, `--tanks`,
  `--formation`, `--darkmatter`.

Traces from before these probes fall back to what the records alone say: intro end from his first
swing, the pair from the roster tanks, meteors from the 33105 markers. Its banner names declared keys
the pull never wrote.

What the reader infers, because no record says it outright:

- **Phases**: without `algalon.phase`, P1 opens on his first cast (no record shows the intro's
  unselectable flag), P2 at 20% or the first Worm Hole, and the 360 s enrage counts from P1. The intro
  ends at his first swing when that comes before `algalon.pull` and the latch, which can trail it by a
  tick (`INTRO_LEAD_MS`, 1.5 s). **His victim** is the target column plus his own swings: whoever he
  one-shots dies between two snapshots, so a melee or Quantum Strike `dmg` row names a victim the column
  never shows.
- **Phased is an aura, never a place.** A row while 62168 / 65250 is held is the field's 1 s refresh. One
  within `CHAIN_MS` (1.5 s) of a removal continues the stretch; a chained application, or a stretch past
  `LOOP_MS` (10.5 s), is a bot still in the field. Who a Big Bang could hit comes from these spans, who
  it did from its `dmg` rows; a `hold` handler's hit counts only on traces with `algalon.disperser`.
- **Deaths and presence.** A star died where a Black Hole Explosion names it or a hole spawns on its last
  spot (`star_deaths`); a constellation went through a hole when its rows and the hole's vanish
  together. 33105 is the meteor, 33104 stands ~2 yd off it, and two meteors of one wave can land a yard
  apart, so markers are never merged by place. A hole, constellation or Dark Matter counts from its first
  row to its last, gaps included.
- **Latches.** `algalon.shelter` clears without a row when a Big Bang starts. The soaker a view names is
  the cast's last: `--bigbang` and `--soak` list every soaker and backup change with the previous holder's
  phase or death within `CAUSE_MS` (1.5 s) before it. A kill the tracker keeps going stays under the
  `algalon.starwindow` label it opened with, so a window opens only where the label turns `open` or
  `deadline` or `algalon.urgent` turns 1. `algalon.starplan` rounds to whole seconds (`STAR_PLAN_SLACK_MS`
  1 s either side). `--formation` measures each ring bot from its own slot, against the current source,
  so a trace with no `algalon.spot=pickup` predates the edge hold.
- **Casts and buttons.** A wand's Shoot (5019) writes an untriggered `cast` per start and a triggered one
  per shot. Last Stand's cast 12975 is a dummy whose aura is 12976. No talent reaches the trace: a paladin
  can cover once seen covering or while `algalon.shadowaura` names it; an explosion counts as covered by
  70940 or Aura Mastery, never 64205, which bots cancel as it lands; a button never pressed reads ready.
  `algalon.personal` and `algalon.cover` name no star, so a press counts for the first star dying within
  `PRESS_AHEAD_MS` (5 s; the forecast ran up to 4.2 s late), else one that exploded within
  `PRESS_STALE_MS` (1 s) before it (the scan listed a dead star up to 0.5 s on).

## What it costs

No trigger, multiplier or rule sweeps the map: `ScanRoom` runs once per instance per 250 ms and every
helper reads its cached lists. Half the triggers are paced (250 ms to 2 s). What is dear is sight
raycasts, `CanCastSpell` and the dodge's collision checks, so their order is deliberate:

- **Tank-cut election** (`GetAlgalonTankCut`): the holder and every paladin and priest each walk the
  whole provider list every tick the holder is hurt. Cooldown (`ExternalSpell`) and the Big Bang
  reserve (`usable`) are tested **before** range, sight and the cast check (`ExternalReaches`); a
  reserved or cooling spell is refused again every tick otherwise, after paying for all three.
- **Tank-heal pick**: health before sight, and one sight test per tank per call (it was up to 7).
- **Dodge and hole exit**: the fallback searches of one `Execute` share one `HazardSweepCache`.
- `IsAlgalonHandler` compares guids; `GetAlgalonHandler` walks the group, so keep it out of rule
  predicates.

Each reorder was run against the old body over the states it can tell apart (3.5 M for the pick, 21 M
for the election): same answer every time, never more raycasts.

**Would change behaviour, do not re-propose:**
- `state->engaged` / `state->phase` for the live `AlgalonEngaged`: 250 ms stale at the pull and a wipe.
- A per-ms memo of `AlgalonHideSlackMs` or the hide role across the multipliers: an action that
  returned false may already have used Nitro Boosts or latched a shelter.
- One slot anchor per dodge `Execute`: `RawSlot` reads the clock (Big Bang due in 15 s, Nitro cooldown).
- Sharing the tank-cut election between bots or moving it into the tick: claims change between askers.
- An early return for non-handlers ahead of `GetAlgalonHandlerConstellation`: it erases a stale kite
  hole, which every bot's shelter pick reads.
- Id-based aura tests for the name-based `PlayerbotAI::HasAura`: the name covers every rank and
  same-named procs.

Open: helpers re-fetch the encounter state per call, see
[the plan](../../plans/algalon-perf/algalon-perf.PLAN.md).

## Not supported

**Single-tank raids.** The swap stays inert without a second tank rather than drafting one: a damage
dealer taking Quantum Strike dies in two swings, and a fake off-tank would hide the failure.

**Bloodlust stays on the pull.** Phase 1 is 80 % of the health bar; guides are silent, traces decide.
