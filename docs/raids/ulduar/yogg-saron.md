# Yogg-Saron
## Phase 1: Sara is friendly, and only Guardians can hurt her

Sara is `FACTION_FRIENDLY` all of P1, and her `DamageTaken` zeroes anything whose attacker is not a
Guardian of Yogg-Saron (33136). The raid's job is to kill Guardians **on top of her** — their death
explosion Shadow Nova Sara (65719, 15 yd) is her only damage source.

That makes P1 a counter, not a damage race: 65719 is a flat **25,000** against her spawn row's
**199,999**, so the phase wants roughly **eight** deaths inside 15 yd of her and a kill further out
buys nothing at all. One of six kills on 2026-09-14 landed 22.0 yd out and was simply wasted. Guardians
have no movement script — `SetInCombatWithZone`, then a vanilla threat table — so **where melee stand
is where a Guardian dies**, which is the whole reason melee are leashed to her.

So no bot ever holds threat on her, and `AI_VALUE2(Unit*, "find target", "sara")` walks
`GetThreatenedByMeList()`. It returned null every tick and took **all 21 Yogg nodes** with it: two
wipes on 2026-09-13, ~6,900 checks per trigger, zero fires. Resolve her with
`FindNearestCreature(NPC_SARA_PHASE_1, …)`. She lives into P2/P3 at 1 health, so "Sara is alive" is no
phase test — `YoggSaronInPhase1` also demands neither P2 nor P3. Yogg himself is no better; resolve
him the same way.

`IsBotMainTank` is false for **every** bot while a human holds main tank, which silently disabled the
phase-3-control node. `IsDesignatedBotTank` falls back to the first living bot tank.

## The two spells that wipe P1

| Spell | Shape | Answer |
|---|---|---|
| Shadow Nova 62714 / 65209 | instant, uninterruptible, DBC 15 yd but **16.2 measured**, on Guardian **death** | ranged and healers stand off; melee and tanks must eat it |
| Dark Volley 63038 / 65330 | 1500 ms cast, 35 yd, `InterruptFlags` 0xF | interrupt it — distance is no answer |

~97% of raid damage across both wipes. Which of the two leads flips with how many Guardians are up:
the nova led at 1,570,355 over 115 hits when two or three were alive, Dark Volley at 957,559 over 182
against the nova's 730,809 once 14 were. 25-man **normal** casts 63038, so the Dark Volley ids do not
split 10/25; test both. The always-on class interrupts only look at the bot's current target and
caught under a third of the volleys, hence `yogg-saron dark volley`, which offsets each interrupter by
its index so the raid does not spend every cooldown on one cast.

The nova is a **death explosion, not a cast** — `boss_yoggsaron_guardian_of_ys::JustDied` →
`DoCastAOE` — so nothing interrupts or outranges it once the guardian dies inside the stack. **Its
kill zone is Sara**, because that is where the raid is supposed to kill Guardians, so the phase works
and the raid dies of it: once every kill landed inside 15 yd, the 12 novas of one pull caught 12-23
raiders each and were **65%** of all damage the raid took. Melee and tanks have no way out. Ranged and
healers do, and stand off (below).

**Sara's Fervor (63138) doubles it, and that is a one-shot.** The DBC gives +20% damage done and
**+100% damage taken** for 15 s; the core implements none of it, so the spellbook and every
server-side grep are silent. Measured over one pull: **13,877** median across 199 ordinary nova hits,
**23,789-34,039** across the 4 that landed on a Fervor holder, against caster and healer pools of
**25,000-30,000**. All four killed, one from full health.

So Fervor gets its own, wider health gate — `ULDUAR_YOGG_SARON_FERVOR_NOVA_HEALTH_PCT` (50%) against
20% for everyone else. Focus fire is what forces that: concentrating damage cuts the window a Guardian
spends at or under 20% from a **median 3.5 s** to **0.9-1.0 s**, and one second is 7 yd of travel
against a 16 yd blast from a start in melee contact. The 20% gate fired at 4 Fervored novas and saved
nobody. The "chasing me" half of the ranged rule needs no timing and is untouched.

## Ominous Clouds

Six clouds orbit Sara at fixed **11.5 / 21.3 / 31.2 / 41 / 50.8 / 60.8 yd** radii, constant
**3.0 yd/s** (`SpawnClouds`, `8 + i*7` on the diagonal). `InformCloud` skips clouds closer than 20 yd
to Sara, so **the innermost orbit only ever fires from player contact** - and it sweeps straight
through where melee stand.

**The summon is a 10 s aura and the cloud then re-arms.** 63031 is `EffectAura` 23 with period and
duration both 10000, triggering 62979, so a Guardian appears exactly 10 s after its cloud is marked
and attributing one means rewinding by that. `JustSummoned` clears `_isSummoning`, so a bot that stays
in reach summons **one Guardian every 10 s indefinitely**: the innermost cloud alone produced 6 of one
pull's 17. The C++ reads as an instant one-shot summon; both the delay and the repeat are DBC-only.

**A cloud summons on any player within 8.5 yd, not the script's 6** — `SelectNearbyTarget` goes
through `IsWithinDistInMap`, which adds both bounding radii. Calibrated off the innermost orbit
because it is provably player-only: the nearest player to each of its summons sat at **4.6-8.4 yd**.
Pets cannot trigger it (`who->IsPlayer()`).

Avoiding them starves nothing: `EVENT_SARA_P1_SUMMON` feeds Guardians every 20 s, shrinking to a 10 s
floor, wherever the raid stands. That timer is also the yardstick — it can fire at most **7** times in
a 99 s window, so the 20 Guardians of 2026-09-14 put **at least 13** on the raid's own feet.

**No station is ever safe.** Orbit gaps are ~9.8 yd, so clearing every ring at once would need gaps
over **17**, and every point within 69 yd of Sara is swept by something. The dodge has to win on
**warning** instead, and bots sidestep as a cloud comes round.

**The route matters more than the destination here, unlike every other hazard.** Crossing a Death Ray
to leave one still beats standing in it, which is why `FindNearestPositionClearOfHazards` checks no
path; crossing a cloud costs a Guardian, so the far side is worse than standing still. Phase 1 alone
overrides `RouteAcceptable` to reject a candidate whose straight walk passes inside
`ULDUAR_YOGG_SARON_CLOUD_SUMMON_RADIUS` of a cloud.

**And the orbit outruns a sidestep.** A destination clear of a cloud now is under it seconds later, so
each cloud enters the hazard set twice: where it is, and where its own facing and run speed put it
`ULDUAR_YOGG_SARON_CLOUD_LEAD_MS` ahead. Without the lead the dodge is no better than chance —
destinations sat a median 16.3 yd from the nearest cloud against a 15.4 yd baseline for standing
still.

## The spacing nodes

Each phase puts everything it has to dodge into **one** node: two nodes at one relevance trade ticks
and walk the bot down the line between their destinations. Phase 1 takes clouds and Shadow Nova,
phase 2 takes Death Rays and Crush wedges, off a shared latch and sweep. Three things keep them from
oscillating against hazards that never stop moving.

- **Trigger and clear thresholds kept apart** (nova 17 → 20, ray 9 → 14, Crush arc ±8° → ±14°). One
  threshold parks the bot on the boundary and re-fires the moment a hazard drifts a yard in.
- **The cloud pair runs the other way** (**18 → 14**). The action's early-out is "already outside every
  circle", so the *clear* radius is what decides when a bot moves and the trigger only decides whether
  the node is asked at all: a trigger inside the clear radius leaves a band where the bot sits in a
  hazard circle and nothing asks. At 10 against a clear of 14, a bot first moved with the cloud 10 yd
  out — **0.5 s** before it is in reach, against the 1.2 s needed to cover 8.5 yd, so the dodge could
  not win however it was tuned. The nova pair keeps the tight-trigger shape because its own band
  (17-20) is margin, not exposure: 18 yd is already past the blast.
- **The destination is held** until the bot arrives or a hazard drifts onto it, and the in-flight tick is
  claimed by returning true without touching the motion master.
- **`preferNear` is `ULDUAR_YOGG_SARON_MIDDLE`, not the bot.** Every candidate in a ring is the same
  walk away, so a bot-position bias is a no-op tie-break; biasing at the middle makes the dodge a
  sidestep rather than a run for the rim. The `accept` cap holds it inside spell range.

- **The dodge keeps melee in their own swing range.** A melee bot that sidesteps a cloud out of reach
  is hauled straight back by `reach melee`, and the two traded the tick **252 times** in one pull while
  the spacing node returned FAILED 582 times against 618 OK. With nothing about to detonate, phase 1
  puts "still within `meleeDistance` of my target" in `set.clear` and the cloud circles in
  `set.fallback`, so the retry drops the reach and keeps the clearance.

`MoveAwayFromCreatureAction` was rejected for this: no throttle, no latch, and it *maximises* distance
recomputed from the bot's new position every tick, so against six rotating rings the best answer rotates
with them.

**Who dodges what, in phase 1.** Everyone dodges clouds. A Guardian's nova is dodged only by a bot that
would not otherwise survive it: ranged and healers when one is at or under
`ULDUAR_YOGG_SARON_GUARDIAN_NOVA_HEALTH_PCT` **and chasing them** — at spell range nothing else reaches
them — and anyone holding Sara's Fervor, on the wider gate above. Running from every Guardian instead is
what scattered the raid to the rim on 2026-09-14, where seven bots were picked off one at a time between
1:26 and 1:30. `GetYoggSaronNovaThreats` is the single owner of that rule, so the trigger and the action
cannot disagree about who is running.

**Ranged and healers stand off Sara, stacked.** Guardians die on her, so her spot is a standing nova
hazard for everyone who need not be in it — ranged and healers were inside it for 37% and 49% of one
phase 1. `ULDUAR_YOGG_SARON_P1_STANDOFF` is 20 yd, clearing the 16.2 measured blast, released at 22:
the mirror of the melee leash, for the same reason.

They **stack** out there rather than spread. Against a point hazard that sweeps a circle and re-arms
every 10 s, a blob is passed once per orbit while a spread-out line hands it somebody in reach for most
of one — **1.5 summons per orbit against 7.3**. That inverts the usual raid rule and holds only because
the nova cannot reach the standoff. 20-22 yd is also the least-swept band inside spell range: one orbit
reaches it, 13% of the time, against two and 17% at 26 yd.

**Melee and tanks are leashed to Sara, not stationed on her.** Beyond `ULDUAR_YOGG_SARON_P1_LEASH`
(15 yd, the nova's own reach to Sara) the bot walks back to the middle, and it is not released until
`ULDUAR_YOGG_SARON_P1_LEASH_RELEASE` (12 yd): let go on the leash itself, `reach melee` drags it
straight out again and the two trade the tick. The node reads a live Guardian before the phase, which
is four 200 yd sweeps and would otherwise run every tick for melee standing outside the leash in P2 and
P3; the price is no walk back before the first spawn. The leash keeps melee inside the 11.5 yd cloud
orbit by design — a station tight enough to clear it would give up chasing entirely, so **melee still
trigger clouds, and that is the trade to check first** if spawns still outrun the timer.

## Targeting is direct, never a raid icon

Yogg-Saron was the last Ulduar encounter targeting through raid icons. `yogg-saron set dps priority`
now owns every non-tank's target for the **whole** encounter, paired with a multiplier zeroing
`DpsAssistAction` — the idiom the rest of the raid already uses. Both read `IsYoggSaronFight`, the same
call rather than merely the same phases: a multiplier zeroing the assist over a wider window than the
resolver covers leaves a bot with no target source at all. `AttackRtiTargetAction` is left alone so a **human's** mark still wins,
and `rti` survives only as a per-bot room tag.

An icon is a sticky override: `RtiTargetValue` hands it back before the smart picker runs and
`IsHighPriority` pins it, so a wrong mark cannot be corrected until the bot leaves combat. Reading one
back also fails whenever the marking bot holds a different `rti` string from the reader — exactly what
the illusion rooms do, one tag per room. Measured: `attack rti target` never fired once across a whole
pull, and the bots waiting on it dropped out of combat into `clean quest log` and `apply oil`.

`RtiTargetValue::Calculate` returns null on LOS failure and beyond `sightDistance` (100) in 2D. A
direct `Attack()` has **no distance cap, only LOS**, which is what makes the Brain reachable at all.

Kill order — **phase 1: one tier, the lowest-health Guardian of Yogg-Saron**. Splitting damage is what
killed the raid on 2026-09-14: two Guardians rode down in lockstep from 63.6%/82.3% to 1.4%/2.8% and
crossed zero inside one second, and the **double** nova put 228,396 over 16 hits and killed all eight
melee in **16 ms**. Four earlier single novas were all survived. Brain level: Influence Tentacle →
nearest other illusion add → the Brain. Boss room:
**leftover Guardian of Yogg-Saron** → Crusher (**ranged only**, below) → Constrictor → Corruptor →
**Marked** Immortal Guardian (36064) → Immortal Guardian (33988) → Yogg.

**The leftovers lead the boss room, and leaving them off it cost a pull.** They survive the transition,
keep casting a 35 yd Dark Volley nothing walks out of, and regenerate to full the moment the raid takes
a portal. With no tier they fell through to the `dps target` fallback, which is nearest-first per bot:
five of them took **~1,978,370** — two Guardians' worth — for **zero kills**, four ending between 11.9%
and 21.2%, while dealing 735,989 back. That is the same split-damage failure phase 1 has a kill order to
prevent.

Every Guardian tier is picked lowest-health first and then held outright: an order that flips
mid-fight resets every swing and cast timer in the raid.
 Below 10% a guardian is Weakened and only Thorim's Titanic Storm can finish it, so it stops
being a target at all.

## Crush is a ±5° cone that tracks its victim

`64146 Crush` is a self-buff applied in the Crusher Tentacle's constructor, `procFlags 4` (melee swing
done), 100% chance, firing **`64147`** (25-man `65201`): physical, base 15199, 23 yd, and `spell_cone`
row `(64147, 10)` with implicit target 104 makes it a **10° cone, ±5° off the tentacle's current
facing**. `64147` appears nowhere in the server `.cpp` — it is reachable only through
`EffectTriggerSpell` off `64146`.

The tentacle is stationary and nobody tanks it, but **melee range exempts nobody**: the cone's
proximity bypass in `WorldObjectSpellConeTargetCheck` is a 2.0 yd centre-to-centre test against
`MIN_MELEE_REACH`, while melee range here is ~10.8 yd (`CombatReach` 8 on display 28814). The danger is
being **collinear with the tentacle and whoever it is hitting**, not being close — a four-yard sidestep
at 20 yd clears it. One volley caught ten raiders standing in a line behind the tank and killed two;
267,908 damage over 14 hits, 6.1% of the raid's total taken.

So the ranged dodge is by **angle**: reject any spot within 25 yd of a live Crusher and inside its
arc. The bot the tentacle is currently hitting is exempt — it is hit wherever it stands, and moving
only drags the cone around behind it.

**At melee range no angle works, so melee do not target the Crusher at all.** Fixing the ranged dodge
inverted the shape underneath: collateral hits moved from a median 23.5 yd to **3.6 yd**, Crush went
14 hits / 267,908 → 39 / 555,805, and seven of nine deaths were melee on a Crusher. At 3.6 yd the ±8°
wedge is ±0.5 yd wide; four hits landed at 0.8 yd, **inside the 2.0 yd bypass where the cone test
never runs at all**; and the cone re-aims onto whoever the tentacle is swinging at, so a bot that was
clear becomes collinear without moving. `IsAllowedTarget` rejects 33966 on `IsMelee` rather than
admitting on `IsRanged`, which are not strict complements. Healers stay eligible, being at range.

**Named risk:** Crushers died in 26 / 48 / 43 s with the whole raid on them against a 50-60 s respawn.
Two concurrent Crushers is −37.6% raid damage and means the trade failed; bar melee only while another
tentacle is alive instead. P2 can also leave melee with **nothing**: Yogg is rejected behind Shadow
Barrier, so a window with only a Crusher up gives them no allowed target. That idle is deliberate — a
lone Crusher means the tank is its only other candidate, so every melee bot on it is collateral on the
tank's own Crush line.

## Diminish Power is why the Crusher dies first

`64145 Diminish Power` is a **5-minute channel** (`DurationIndex 5`), **−21% damage done raid-wide**
with no distance pruning, **multiplicative** across tentacles (two up = −37.6%, three = −50.7%),
`DispelType 0` so undispellable, and unkickable twice over — the tentacle is immune to
`MECHANIC_INTERRUPT` and `MECHANIC_SILENCE` via `CreatureImmunitiesId -361`, and the channel carries
flag `0x10`, not the `0x08` that makes one interruptible. It ignores LOS and immunities, and **it
reaches the brain room**: the retail exemption keyed on aura `63988` is in the DBC but that aura is
never cast in this build.

Only two things stop it. A melee hit buys ~1.5 s, because `_diminishReady` is set once at 6 s and never
reset. Or the tentacle dies — **2,000,001 HP** in 25-man, which the whole raid clears in 25-34 s even
debuffed, against a respawn of 45-54 s tightening to 25-30 s by the sixth portal wave. Ten ranged alone
would spend 50 s of that window debuffed. So the Crusher is the **top** target for everyone who can
safely stand there — every ranged and healer — rather than something to keep away from.

## The brain room

**Induce Madness `64059` is a 60,000 ms cast.** On completion everyone at **z ≤ 300** loses all 100
Sanity and is teleported out; anyone above takes nothing. No Sanity means `63120 Insane`, and
`spell_yogg_saron_insane_aura::OnRemove` calls `Unit::Kill(owner, owner)` — **a mind control is always
a death.** A bot ending a window ~120 yd from the nearest exit portal needs ~17 s to walk it.

Every millisecond of lead is damage the Brain does not take, so the lead is **measured, not flat**:
`max(10 s, distance / runSpeed × 2)`. A flat worst-case lead threw away a third of every window for a
bot standing next to a portal.

**The Brain sits at z 265 while its room's floor is z 236-244.** The three portal arrivals are 60.0 /
67.1 / 71.9 yd from it, so a radius-to-the-Brain test is a bad proxy for "am I in the brain room" — it
is that question plus a permanent 25 yd vertical tax, and two of the three portals land outside a 60 yd
gate before the bot takes a step. Use the z band. The split is absolute: 14 visits where the exit node
ran produced 0 mind controls, 17 where it never ran produced 10.

**Damaging the Brain while an Influence Tentacle (33943) lives deals nothing and kills the attacker** —
`boss_yoggsaron_brain::DamageTaken` zeroes the damage and calls `Unit::Kill(who, who)`. So approaching
it is gated on two reads of one server fact, neither of them a human: no 33943 alive, **and** the room's
illusion door (`194635` Chamber / `194636` Icecrown / `194637` Stormwind) reading `GO_STATE_ACTIVE`,
which the Brain sets in the same branch that fires when the last tentacle dies. Reading it wrong leaves
the bot standing still rather than walking in to die.

The old gate waited on the bot's **master**, a human, to stand in the brain room, and Brain health only
ever moved in windows where one did — the windows without a human left it untouched.

Every portal in a wave leads to the same illusion, cycling Stormwind → Chamber → Icecrown.

## The encounter plays its own mechanics

No node in this fight cheats: `HasCheat(BotCheatMask::raid)` appears nowhere under
`src/Ai/Raid/Uld/`. Config is untouched — `raid` stays in `AiPlayerbot.BotCheats` — so a trace header
still reads `food,taxi,raid` and proves nothing either way
([../../systems/observability.md](../../systems/observability.md)); only the body can show it.

**One teleport survives, `yogg-saron fall from floor`, and it is deliberately ungated.** Its trigger
fires only when a bot is below z 300 in the boss room or below z 200 on the brain level: it has fallen
out of the world and cannot walk back. That is an unstuck for a pathing failure of ours, not a
shortcut past a mechanic, so a cheat-free raid keeps the rescue.

Everything else walks, which inherits a blindness: **`MoveTo` returning `ok` says a command was
issued, never that a route exists** ([../../engine/pitfalls.md](../../engine/pitfalls.md)).
`YoggSaronWalkMakingProgress` is the answer — a per-bot, per-(node, destination) latch that gives up
after **6 s** without closing distance, probed as `yogg.walk`. Keyed per destination so testing
several in one tick does not wipe the others, and a gap in the asking starts a fresh attempt — or a
give-up outlives the walk that earned it. Give-up means stand down everywhere except
`move to exit portal`, where standing down is fatal: it cycles to the next of the three permanently
spawned portals and only stands down once all three have failed. `go to brain room` is the one walk
without a latch — it is one-shot behind the `rti` room tag, so there is no repeat to notice a stall in.

Following a master is wrong in every part of this fight — the illusion rooms are exactly where bots
idled behind a human on `clean quest log`, `apply oil` and `loot roll` — so `yogg-saron stop
following` removes `FollowMasterStrategy` and nothing adds it back.

## The Death Orb is not the hazard

`NPC_DEATH_ORB` (33882) is a marker parked at z 353 while the raid stands at z ≈ 326 — **27 yards
overhead, permanently** — so a proximity test on it can never fire. The hazard is `NPC_DEATH_RAY`
(33881), which falls to the floor, warns 5 s with `63882`, then runs `63883 → 63884`: **3 yd radius**,
base 19999, walking **eight 9-yard legs every 1625 ms** on a re-rolled cardinal axis. Four rays per
orb, one orb every 22 s.

## `64163 Lunatic Gaze` is an aura, not a channel

Yogg puts it on himself for 4 s, ticking `64164` once a second for 5699 damage and 4 sanity at 130 yd
through his front 180°. `AttributesEx` 200 carries no `SPELL_ATTR1_IS_CHANNELED` bit, so
`GetCurrentSpell(CURRENT_CHANNELED_SPELL)` never saw it — detect it with `HasAura`.

Facing away is a real defence (`spell_yogg_saron_lunatic_gaze::FilterTargets` needs
`HasInArc(M_PI, caster)`), which is why P3 swaps the designated tank off `TankFaceStrategy` — that
strategy would turn it back into the gaze. **Known-open:** a bot facing away from Yogg is facing away
from its target and cannot cast, and the gaze was the #2 damage source at 660,442 over 244 hits. That
tension is a design question, not a defect.

**`rti` is a room tag, so never write one as a targeting hint.** `"cross"` is Stormwind's tag: a
boss-room bot given it is teleported into Stormwind by `yogg-saron fall from floor` the moment it dips
below z 300, and is disqualified from `move to enter portal`, which requires `"skull"`.

## Phase 3: a positive phase test, and a station that is not a leash

**A phase test made only of absences is true between the phases it separates.** `IsPhase3` read "Yogg
alive, no Shadow Barrier, no phase-1 Guardian" and was therefore true for the **9.5 s** between the
last Guardian dying (186.6 s) and the barrier landing with the first tentacle wave (196.2 s), and the
whole raid ran its phase-3 positioning through the gap. The Brain (33890) is the positive fact: it
spawns with that wave and lives to the end, and swapping the Guardian sweep for a Brain sweep shrinks
the bad window to 0.5 s at the same cost.

A trace carries no phase note, but P3 starts when the Brain reaches 30% and the script then forces
Yogg's health to exactly 30% — which is how a trace dates the transition.

**A fixed position with a radius is a leash, and a leash is a fence whenever the bot's target sits
further than radius + range from it.** The P3 ranged spot is 39.1 yd from Yogg against 28.5 yd of
spell range and a 15 yd radius — reachable by 4.4 yd, and **nothing else in the room is**. The last
Corruptor of a pull sat 60.3 yd out: the resolver handed it to 17 bots and the positioning node fenced
every one off it, leaving exactly one raid member ever within 28.5 yd. Measured cost: **1.11 M** raid
damage in the fenced minute against **5.37 M** in the next, Yogg falling 0.024 %/s against
0.181 %/s.

So the spot is a **station**, not a leash: `PhaseThreeStationReaches` claims a bot only while standing
there costs it nothing and releases it otherwise, probed as `yogg.station`. Naming no entries is
deliberate — a rule written as a list goes stale the first time a new add appears. Tanks keep a 30 yd
leash instead of a station, because `yogg-saron guardian control` outranks this node and brings them
back whenever a guardian is loose; the leash is only the way home once nothing is.

A sanity well the bot cannot reach is the same failure by another route: `yogg-saron sanity` parked
three bots stationary for 202, 164 and 96 s on a well they never got to, suppressing every node below
it for as long as it lasted. It now stands down on the walk latch.

## Reduced Keepers

The raid frees fewer than 4 Keepers, losing that Keeper's support. Tuned for the hardest single-Keeper
case, **Thorim only**. The bitmask lives in `PERSISTENT_DATA_WATCHERS_MASK` — preferred over Sara's
`GetData(DATA_GET_KEEPERS_COUNT)`, which gives a count only, because reading the mask confirms
**which** Keeper. It is a **raid choice, unrelated to the `ulduarYoggSaronHardMode` config flag**.

What Thorim-only removes: no Freya means **no Sanity Wells, so Sanity (63050, 100 stacks) is a
one-way drain** — nothing restores it; no Hodir means no Protective Gaze absorb; no Mimiron means no
haste clouds and therefore a slower kill and more total drain.

Sanity drains, and whether anything can be done:

| Source | Spell | Loss | Avoidable |
|---|---|---|---|
| Psychosis | 63795 / 65301 | −9 / −12 | **No** — random target every 3.5s in P2 |
| Malady of the Mind | 63830 / 63881 | −3 | Yes |
| Brain Link | 63803 | −2 | Yes, if the pair stays within 20 yd |
| Lunatic Gaze (P2 skull) | 64168 | −2 | Yes — only players *facing* the caster |
| Lunatic Gaze (P3 Yogg) | 64164 | −4 | Yes |
| Induce Madness | 64059 | −100 | Yes — leave the brain room |

**An Immortal Guardian cannot be killed by damage at all.**
`boss_yoggsaron_immortal_guardian::DamageTaken` clamps every hit to leave it at 1 HP. The only kill
path is Thorim's Titanic Storm, filtered to a target carrying `SPELL_WEAKENED` (64162), applied under
10% health where the Empowered stack count also falls to 0, and armed at the start of P3; its 2 s
period against a 10 s spawn outpaces spawns 5:1 once the raid burns each guardian that low. So the
raid's whole job is Weakened.

**A phase 3 without Thorim is therefore a forced wipe** — accepted, not a defect: the price of playing
it straight everywhere. Guardians spawn one per 10 s with no cap and a live one never
times out (the `TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5000` is a *corpse* lifetime); `SPELL_EMPOWERED`
gives +180% damage per stack at `healthPct / 10` capped at 9, so an untouched guardian swings at
×17.2; and anything under 85% starts Drain Life — 16,665 damage and **166,650 self-heal, 39.2% of its
own max health**, every ~14.5 s on a *random* player within 40 yd with no LOS and no threat component.

**Shadow Beacon heals guardians, not the boss.** It marks 3 guardians in 25-man every 45 s, and
Empowering Shadows is a **20 yd friendly AoE worth 750,000 over 20 s — 176% of a guardian's own max
health**. `spell_yogg_saron_shadow_beacon_aura::OnApply` swaps the marked guardian's entry to 36064 —
a free signal: `TierOf` puts Marked above unmarked so the raid reaches Weakened before the heal
lands. Whether that 20 yd radius also reaches Yogg is **geometric inference,
not an observed log** — his combat reach is 30 yd and guardians sit 28-40 yd from his centre, so by
static reading it does not. Watch for net health gain on a guardian across a beacon.

`yogg-saron guardian control` is **not** gated on Thorim, or on hard mode. Hard mode means *fewer*
Keepers, so demanding both asked for exactly the case where Thorim is least likely to be there — and
guardians parked on a tank beat guardians loose among the casters even where none of them can die.

Open risks: Lunatic Gaze "freezes" bots, so guardian DPS may stall between gazes; a guardian spawns up
to 48 yd out, so confirm taunt pickup is prompt; and the sanity-conservation behaviour (stand behind
Yogg facing away below 15 stacks) **nearly benches a bot** — sanity never recovers Thorim-only, so a
bot that drops to 15 stays there.

## Squeeze breaks on immunity

Removing the Squeeze aura (64125 / 64126) kills the Constrictor Tentacle and drops the passenger, so
`yogg-saron squeeze escape` at `ACTION_RAID + 1` has a grabbed mage cast Ice Block and a paladin cast
Divine Shield. Hunter Feign Death and rogue Vanish are deliberately not used — neither removes a
periodic damage aura.
