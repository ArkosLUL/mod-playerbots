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
| Quantum Strike | 64395, every 3-4.5 s, 15.7-17.3 k / 34-36 k physical | Two tanks or nothing |
| Phase Punch | 64412, every 15.5 s, 45 s aura, 5th stack phases the tank 10 s (64417) | Swap at **4** |
| Collapsing Star | 32955, every 60 s, tops up to 4 alive | Star team kills one at a time; each death 16-21 k to the raid |
| Black Hole / Worm Hole | 32953 / 34099, 6 yd field (62168 / 65250) pulsing every second, no target cap | Phases for 10 s plus the 62169 DoT (~17.5 k) |
| Cosmic Smash | markers 33104/33105, impact ~4.8 s later, 1 / 3 markers | `< 6` yd full, `6-10` `dmg/dist*2`, `>= 10` `dmg/dist` |
| Living Constellation | 33052, 11 total, 3 activate every 50 s | Handler collects them, spends spare holes on them |
| Big Bang | 64443 / 64584, every 90.5 s, 8 s cast | 76-89 k / 107-113 k **physical** at 50 000 yd; pierces immunity, Divine Shield included |
| Phase 2 | at 20 % HP | Stars, constellations and holes despawn; 4 Worm Holes spawn on the fixed square |
| Unleashed Dark Matter | 34097, one per Worm Hole per 30 s, random target, never despawns | Handler collects them; 10 yd/s, so tanked, never kited |
| Ascend | 64487 at 6 min | Also the zero-target Big Bang path |

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

## Big Bang: the holder stays out

Every guide keeps one player out; Warcraft Tavern names the main tank with Physical defensives. The
**soaker** is his victim at cast start (the pickup tank if that is no swap tank), latched per cast by
the instance tick. Every role reads that latch, never the live cast: in the tick's first 250 ms of a
cast nobody is the soaker yet, and every bot, tank included, would run for a hole.

Big Bang's second effect applies **64445, which strips every phase a second after the hit**, Phase
Punch's included; it ignores phase, so it reaches everyone. The soaker is alone for about a second.
It casts `NextTankDefensive(..., physicalOnly)` at ≤ 3 s remaining, skipping Anti-Magic Shell. A
**backup** priest with Dispersion also stays out and casts it at ≤ 5 s (it lasts 6 s): a soaker dying
to the hit with everyone else phased is a reset, and the backup is promoted if that happens. The
button follows what the bot can cast, not its slot, and Dispersion is checked by spell id and cooldown:
`CanCastSpell` refuses during a channel, and a shadow priest is nearly always in Mind Flay. A disc or
holy priest puts Pain Suppression or Guardian Spirit on the soaker before hiding. All of these, and the
tank defensives, are held from 20 s before the cast.

The Phase Punch queued behind the cast lands on the soaker while everyone else is still phased, and a
5th stack then leaves nobody unphased. So a holder at 3 stacks with the cast ≤ 15 s away hands over to
a partner at ≤ 1.

Everyone else keeps fighting until the walk to the nearest hole plus 2.5 s is all that is left of the
cast (`wait`): the phase only has to cover the hit, so hiding early buys nothing. The run breaks a cast
pinning the feet, re-issues a stalled walk and holds its tick while walking; an `Exclusive` rule keeps
every other mover off the hider and a `Block` stops Charge, Blink and Disengage. **Once phased, step
8 yd off the hole**: still inside when 64445 strips the phase means phased again within a second for
10 s, and the hole is invisible from inside, so the exit reads the cached position. Phased bots are left
out of the `Exclusive` because phase-16 Dark Matter (33089, 8 static spawns on the star points) attacks
them in the void.

**Threat survives the hide, and this is not a bug to re-audit.** `CombatManager.cpp:53` gates only
*entering* combat on `InSamePhase`, and `ThreatManager` only marks phased entries offline: he re-picks
the top unphased entry when the phase drops.

## Stars: an assigned team, paced

Collapse drains **1 % of max health per second**, so an ignored star dies after ~100 s, and the 60 s
summon only tops up to four: four ignored stars explode within seconds of each other. A latched **star
team** of 2 damage dealers (3 on 25-man), ranged first and melee only to fill a raid short on ranged,
kills them; the target exclusions keep every other bot on Algalon, and an `OwnTargeting` rule keeps
`dps assist` from pulling the team back, since a star is no attacker until something hits it. The team
is cleared (`algalon.focusstar`) onto the **lowest-health** star only when the raid's weakest member is
above 80 % and 8 s have passed since the last explosion, overridden below 15 % star health and while
`urgent`. The tracker marks it with the **star** icon for humans; skull would pull every bot's
`attack rti target`. A skull a player puts on a star or add wins over the exclusions. An AoE multiplier
holds area damage while two or more stars live, heals excepted.

## Holes are a resource

Holes exist only where a star died (it wanders 25 yd from its spawn first, so anywhere in the room), and
every constellation eats one. With a Big Bang ≤ 30 s away one hole is kept out of the kite, and
`urgent` (no hole, cast due or running) puts every ranged DPS on the star team. A slot buried under a
hole is stepped aside from the slot itself; a bot standing in one outside a cast leaves it.

## Constellations: the handler collects them

They never melee and Arcane Barrage hits a random raid member, so a constellation costs damage only
while alive, and who it chases matters only at Big Bang: it follows its victim into a hole and closes it
on the raid. They are killable (260 k / 521 k) but 11 of them cost 16-34 % of Algalon's health against
the enrage, and guides agree nobody damages them. The **handler**, a third tank or else the swap tank
not holding Algalon, and always a bot, taunts every loose one onto itself and **stays out** of Big Bang
holding them (`hold`), with a physical defensive like the soaker. With a spare hole it parks 9 yd past
that hole on the far side (clamped 44 yd from centre), so the chase drags one through the field, then
yields its tick. Its movement rule still passes its taunts, the pickup and the swap: all are
`AttackAction`s, and a swap it can't make phases the holder out. Constellations are excluded from
everyone else's pickers.

## Phase 2

The Worm Holes are the Big Bang shelter. Unleashed Dark Matter picks a random player; the handler
taunts the loose one hurting the most fragile victim first, everyone else is kept on Algalon, and AoE
is free with no stars left. With no bot handler, Dark Matter is left to whoever it chases.

## Cosmic Smash and the formation

The dodge clears **15 yd** from every marker (10 yd if nothing clears), 8 yd from every hole, stays
44 yd inside the room and prefers ground near the bot's slot.

The raid enters from +Y, so the tank slot is on the **−Y** edge of the worm hole square at
`(1632.7, -321.5)`, 18.7 yd from home and 10.2 yd clear of the nearest hole spot, and belongs to whoever
holds him; there is no drag action, he follows through normal chase. Ranged and healers ring it at
14 / 20.5 / 27 yd, healers innermost, 18 slots, filled centre-out and latched per instance. The trimmed
arcs keep every slot ≥ 7.7 yd from all four hole spots; all 19 points are navprobe-verified, flat at
Z 417.321. The formation yields while a marker is within 15 yd of the **slot**, not the bot: a bot
that already dodged passes trivially and walks back under it.

The class taunt nodes are blocked for both swap tanks; the swap, pickup, handler and Dark Matter nodes
taunt through `CastClassTaunt`. The pickup covers his random target at the end of the intro and a
phased holder.

## The trace

Pulls file under `algalon-the-observer`, aliased to `algalon` in `BOSS_ALIASES`.

Raid-wide values, all written by the instance tick: `algalon.phase` (0 idle, 1 intro, 2 phase 1,
3 phase 2, 4 won), `algalon.bigbang` (the cast's ordinal while latched, else 0), `algalon.soaker`,
`algalon.backup`, `algalon.urgent`, `algalon.holes`, `algalon.focusstar`, `algalon.handler`. Per guid:
`algalon.slot`, `algalon.shelter`, `algalon.kitehole`, `algalon.starteam` (urgent recruits aren't
recorded). Derived per bot, each inside the helper that derives it: `algalon.hide`
(`none`/`wait`/`run`/`in`/`exit`/`soak`/`backup`/`hold`/`noshelter`), `algalon.kite`
(`taunt`/`hole`/`parked`/`kept`/`none`), `algalon.spot` (`slot`/`buried`/`smash`/`none`),
`algalon.defensive` (the pick, on change only). `algalon.starwindow`
(`urgent`/`finishing`/`gap`/`raidhp`/`open`/`none`) is computed in the tick but written on the tracker
bot, since the tick runs on whichever bot comes first. `algalon.holelost` (`kite`/`stray`/`phase2`) is
an event, one per hole, skipped when every summon goes at once on a kill or Ascend.

Slots are handed out before any trace opens, so the tick restates `algalon.slot` into each new trace.
`algalon.hide` stays silent until he has been seen, or every earlier Ulduar trace carries a row of it
per bot.

No probe duplicates the generic streams: his casts (watched once engaged; unsampled while neutral
before the pull), Phase Punch stacks and the 62168 / 65250 / 64417 / 62169 auras on players, holes,
markers, stars and constellations as swept `snap.u` rows, and the damage rows. The sweep anchors on an
unphased roster player; on older traces a phased anchor drops the room's units from the rows.

**`tools/botobs/bosses/algalon.py` reads all of it**; `--verdict` is the entry point, one line per
detected failure: `--phases`, `--bigbang` (holes at the cast, soaker and backup health, who was hit
unphased, tank-alone seconds, re-phase loops, void Dark Matter damage, evade after), `--stars`,
`--holes`, `--constellations`, `--smash`, `--tanks`, `--formation`, `--darkmatter`. Its banner names
declared keys the pull never wrote.

## Not supported

**Single-tank raids.** The swap stays inert without a second tank rather than drafting one: a damage
dealer taking Quantum Strike dies in two swings, and a fake off-tank would hide the failure.

**Bloodlust stays on the pull.** Phase 1 is 80 % of the health bar; guides are silent, traces decide.
