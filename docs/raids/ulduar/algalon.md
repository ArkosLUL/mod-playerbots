# Algalon

One AI for 10N and 25N. Only two spells are difficulty-mapped — Big Bang `64443 → 64584` and Black
Hole Explosion `64122 → 65108` — and neither Phase Punch (64412) nor the phase aura (62169) is, so the
strategy carries one extra spell id and is otherwise difficulty-agnostic.

Every timer in the encounter is offset by an intro that runs **26 s on a first pull and 8.5 s
afterwards**, which is why nothing here counts from combat start. The room is a **47 yd disc** around
`(1632.668, -302.7656, 417.32)` with a floor at `z >= 410`; `IsInRoom` calls `ACTION_ASCEND` the moment
Algalon leaves it, so nothing may pull him or a tank past the edge.

| Mechanic | Ids and cadence | Handling |
|---|---|---|
| Quantum Strike | every 3-4.5 s, ~27 k / ~15 k on 25-man | Two tanks or nothing — see below |
| Phase Punch | 64412, every 15.5 s, 45 s aura, 5 stacks | Swap at **4**; the 5th stack phases the tank out for 10 s |
| Collapsing Star | 32955, every 60 s, tops up to 4 alive | Killed one at a time; each death is 16-21 k to the raid |
| Black Hole | 32953, 6 yd field, no target cap | Big Bang shelter and the constellation sink |
| Cosmic Smash | markers 33104/33105, impact 4 s later, 41 437 base | `< 6` yd full, `6-10` `dmg/dist*2`, `>= 10` `dmg/dist` |
| Living Constellation | 33052, 3 activate every 50 s | **Not a kill target** — `HealthModifier = 20` |
| Big Bang | 64443 / 64584, every 90.5 s, 8 s cast | 76 312 / 107 249 at 50 000 yd radius; position is irrelevant |
| Phase 2 | at 20 % HP | Stars, constellations and holes despawn; 4 Worm Holes (34099) spawn on the fixed square |
| Unleashed Dark Matter | 34097, one per Worm Hole per 30 s | `speed_run 1.42857` — faster than players, so it is tanked, never kited |
| Ascend / enrage | 64487 at 6 min | Also fires on the Big Bang evade |

## Big Bang, and the evade that used to end the attempt

`spell_algalon_big_bang::CheckTargets` calls `ACTION_ASCEND` when the spell hits **zero** targets, and
the boss evades about 4 s later. A raid where everyone hides therefore resets him. **Big Bang is also
unavoidable and cannot be immuned** — Divine Shield does not stop it — so the one bot left standing has
to *mitigate* it.

Dispersion's 90 % reduction does that, but its cooldown is **120 000 ms** against a 90.5 s cadence, so
no single bot covers consecutive casts unglyphed. The duty therefore **rotates**: the lowest-guid bot
whose soak is actually off cooldown, Dispersion first, then Guardian Spirit, then a damage dealer who
stays out and probably dies. A corpse still counts as a target, which is strictly better than a reset.
`AlgalonSoakCooldownReserveMultiplier` reserves the chosen bot's cooldown by returning `0.0f` for it
outside the cast, or the priest spends it on the normal `low mana` / `critical health` nodes and has
nothing when it matters. Every other priest behaves normally.

The soaker is latched per cast in `algalonEncounterStates`, because the trigger that exempts a bot from
hiding and the action that spends the cooldown must agree — a per-tick re-derivation strands whoever was
exempted half a second ago.

**Threat survives the hide, and this is not a bug to re-audit.** `CombatManager.cpp:53` gates only
*entering* combat on `InSamePhase`, and `ThreatManager` never purges entries on a phase change: Algalon
simply cannot select a phased target and re-picks the main tank when the phase drops.

## Black Holes are where a star died, not where it spawned

The Collapsing Star spawns on one of four fixed points, then `MoveRandom(25.0f)`; the Black Hole is cast
on **its own position** when Collapse finishes it. So phase 1 holes land anywhere in the room, and any
formation slot can end up buried under one. Only the phase 2 Worm Holes sit on the fixed
`CollapsingStarPos` square. `TryGetAlgalonSlot` therefore steps a bot around a hole on its slot rather
than abandoning the formation, and `algalon leave black hole` outranks the formation node so a bot that
ends up standing in one leaves instead of paying 1531 a tick for nothing.

## Star pacing, which is not optional

Collapse drains **1 % of max health per second**, so an untouched star kills itself after ~100 s, and the
60 s summon only tops up to four alive. Four ignored stars therefore explode within seconds of each other
about 143 s in, for 64-84 k of unavoidable raid damage at once. The kills are deliberately staggered:
focus the **lowest-health** star (health percent *is* the remaining-lifetime clock, so lowest-first
spaces the deaths for free), require the raid's weakest member above 80 % and 8 s since the last
explosion, and override that gate when a star drops under 15 % — a death nobody chose is a death that
lands on top of the next one. `AlgalonCollapsingStarAoeMultiplier` vetoes `DpsAoeAction` while two or
more stars are alive so splash cannot undo the pacing. Phase 2 has no stars, which leaves Dark Matter
cleave untouched.

## Holes are a resource, and the raid can run out

Holes exist only where a star was killed, and every constellation eats one — three constellations per
50 s against four stars per 60 s consumes them faster than they appear. Inside the 30 s before a Big
Bang the kite refuses to spend the last hole, and if there are none at all
`AlgalonTargetGuardMultiplier` takes non-tanks off Algalon entirely until a star dies. A skull mark is
advice; the veto is what makes the star actually die in time.

## Constellations are kited by whoever they already chase

A constellation picks its victim at activation via `AddThreat(target, 100.0f)` and chases it. The
strategy does not appoint a kiter — it uses the bot the constellation already picked, which is the only
bot that can lead it anywhere without a taunt. The single exception is a constellation parked on whoever
is holding Algalon: the other tank pulls that one off, since one bot cannot kite and tank at once.
`AlgalonTargetGuardMultiplier` keeps everyone else off them, because 20× base health inside a six minute
enrage is not a fight anyone wins — the kite through a hole's 65509 (radius 6, one target) is the only
removal there is.

The kiter parks **9 yd past the hole** on the far side from the constellation, outside the 6 yd field.
The spot converges as the constellation closes, so the node yields once parked and gives its tick back
to instants, heals and the class interrupts.

## Formation

The raid enters from +Y — the planetarium console sits ~128 yd that way — so the tank slot is on the
**−Y** edge of the worm hole square at `(1632.7, -321.5)`, 18.7 yd out from Algalon's home position and
10.2 yd clear of the nearest hole spot. There is no drag action: the main tank simply has a slot, and the
boss follows through normal chase.

Ranged and healers ring that slot at 14 / 20.5 / 27 yd, healers innermost, 18 slots in all, filled
centre-out and latched per instance so one death does not renumber everyone behind the corpse. The arcs
are trimmed rather than full half circles because the two −Y hole spots sit level with the tank slot; the
trims keep every slot at least 7.7 yd from all four. Spacing runs 8.9-12.1 yd, which puts a marked bot's
neighbours in Cosmic Smash's cheap `dmg/dist` band. All 19 points are navprobe-verified on map 603, flat
at Z 417.321.

The formation yields while a Cosmic Smash marker is within 12 yd of the slot. This is the Mimiron trap in
different clothes: testing the *bot's* surroundings passes trivially for a bot that already dodged, and
only testing the **slot** stops it walking back under the meteor.

## What is deliberately not supported

**Single-tank raids.** The swap trigger stays inert when the group has no second tank rather than
drafting one, because a damage dealer taking Quantum Strike dies in two swings and a fake off-tank would
hide the failure instead of fixing it.

**Bloodlust stays on the pull.** Phase 1 is 80 % of the health bar and where every mechanic steals damage
time; phase 2 is 18 % and short.

