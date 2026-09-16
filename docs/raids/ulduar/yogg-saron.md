# Yogg-Saron
## Phase 1: Sara is friendly, and only Guardians can hurt her

Sara is `FACTION_FRIENDLY` all of P1, and her `DamageTaken` zeroes anything whose attacker is not a
Guardian of Yogg-Saron (33136). The raid's job is to kill Guardians **on top of her** — their death
explosion Shadow Nova Sara (65719, 15 yd) is her only damage source.

That makes P1 a counter, not a damage race: 65719 is a flat **25,000**, so the phase costs her health
divided by it and a kill outside 15 yd buys nothing at all.

**Never take that count from `creature_template`** — her row's 199,999 gives eight, and she measures
**237,500 at 24 raiders** for **ten** ([../../engine/pitfalls.md](../../engine/pitfalls.md), boss
health is scaled and moves with raid size). Read it off Sara's `unit` row; `yogg_saron.py --phases`
does. On 2026-09-15 a pull wiped with **9 of the 10**, having spent four kills 18-22 yd out where they
counted for nothing; the pull that transitioned did so 0.26 s after its tenth.

Guardians have no movement script — `SetInCombatWithZone`, then a vanilla threat table — so **where
melee stand is where a Guardian dies**, which is the whole reason melee are leashed to her.

So no bot ever holds threat on her, and `AI_VALUE2(Unit*, "find target", "sara")` walks
`GetThreatenedByMeList()`. It returned null every tick and took **all 21 Yogg nodes** with it: two
wipes on 2026-09-13, ~6,900 checks per trigger, zero fires. Resolve her with
`FindNearestCreature(NPC_SARA_PHASE_1, …)`; Yogg himself is no better.

**Her presence is not a pull, and neither is her combat.** `LoadAllGrids` makes her findable from
instance creation and `Reset()` leaves her visible with the clouds already orbiting, so a phase read
made of "Sara exists" is true between pulls — and the Ulduar strategy runs in the non-combat engine
too. Her combat flag reads no better and cost **24 s of every pull**: she is `FACTION_FRIENDLY`
through P1, `CombatManager::CanBeginCombat` refuses a combat reference while either side is friendly
and neither hostile, so `InitFight`'s `SetInCombatWithZone` puts her summons in combat and never
touches her. She picks it up only when her own P1 casting first lands on somebody —
`EVENT_SARA_P1_DOORS_CLOSE` at 15 s plus a 4 s cast, so **19 s after the pull at the earliest**,
measured 19.2 / 19.2 / 24.2 s.

**The pull is `GetBossState(BOSS_YOGGSARON) == IN_PROGRESS`**, set on `InitFight`'s first line 5 s
after any player comes within 90 yd, and already read by `UldEncounterIsLive`. The recorder writes
`pull src=bossstate` from it, so **trace t=0 is `InitFight`** — first damage is 20-30 s later and is
no pull marker. Reading it as one produced the claim that 4 of 12 Guardians beat the pull; against t=0
the first Guardian of every pull on record appears at 10.1-10.2 s, and none has ever beaten it.

`YoggSaronPhase` is the single read every node routes through — phase 2 is Yogg with Shadow Barrier,
phase 3 is Yogg without it plus the Brain, phase 1 is Sara, all behind that boss-state test, which
also spares a bot elsewhere in Ulduar two 200 yd sweeps a tick — and it is what writes `yogg.phase`.
She lives into P2/P3 at 1 health, so "Sara is alive" is no phase test by itself.

**Phase 1 positioning waits for the room, not the pull.** With the raid still 72-96 yd out at t=0,
every mover fired at once on 2026-09-16. Most bots took no damage until 25-53 s, and ranged running in
crossed orbits 4 and 3 as clouds passed: two Guardians by 16 s. The station, spacing, leash and
phase 1 control triggers now wait for `YoggSaronInPhase1Room` (outer orbit + reach, 69.3 yd), a
distance read ahead of the phase read. Combat is no gate: every Guardian's `Reset` calls
`SetInCombatWithZone`, so the instance is in combat from ~10 s. Nor does it belong in
`YoggSaronPhase`, whose read writes the shared `yogg.phase` latch. The gate moves *when* bots cross
the orbits, not whether. `yogg_saron.py --phases` counts phase 1 moves begun outside it (159 and 102
before the gate) and Guardian deaths back to back within 3 s.

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

Six clouds, one per orbit, circle Sara at a constant **3.0 yd/s** (`SpawnClouds`, `8 + i*7` on the
diagonal). Measured over four pulls the radii never drift: **11.39-11.86 / 21.25-21.52 / 31.13-31.31 /
40.93-41.07 / 50.81-50.92 / 60.74-60.84 yd**, so laps take **24 / 45 / 65 / 86 / 106 / 127 s**.
`InformCloud` skips clouds closer than 20 yd to Sara, so **the innermost orbit only ever fires from
player contact**.

**The summon is a 10 s aura and the cloud then re-arms.** 63031 is `EffectAura` 23 with period and
duration both 10000, triggering 62979, so a Guardian appears exactly 10 s after its cloud is marked
and attributing one means rewinding by that. `JustSummoned` clears `_isSummoning`, so a bot that stays
in reach summons **one Guardian every 10 s indefinitely**: the innermost cloud alone produced 6 of one
pull's 17. The C++ reads as an instant one-shot summon; both the delay and the repeat are DBC-only.

**A cloud summons on any player within exactly 8.5 yd, not the script's 6.** `SelectNearbyTarget`
goes through `_IsWithinDist`, which adds both `GetObjectSize()` values — and that returns
**`UNIT_FIELD_COMBATREACH`, not the bounding radius**. The cloud's is **1.0**, and every player's is
**1.5** whatever their race, because `Player::SetObjectScale` hands out `DEFAULT_COMBAT_REACH` flat.
Inner-orbit summons, which are provably player-only, measure the approach at 8.53 / 8.64 / 8.71.
Pets cannot trigger it (`who->IsPlayer()`).

Avoiding them starves nothing: `EVENT_SARA_P1_SUMMON` feeds Guardians every 20 s, shrinking 2 s a
summon to a 10 s floor, wherever the raid stands.

**Every Guardian spawns on the cloud that summoned it, so attribution is a measurement rather than an
estimate.** Match its spawn position to a cloud, then rewind 10 s for who was inside 8.5 yd of that
cloud — `yogg_saron.py --clouds` does both. Of 23 on 2026-09-15, by orbit **1: 8 · 2: 4 · 3: 1 · 4: 1
· 5: 2 · 6: 7**, and **11 were the raid's own feet**: every orbit-1 spawn, plus three on orbit 2. The
innermost cloud laps in 24 s against a 190 s phase and collected one on nearly every pass — melee
outside the cloud-free circle in 7 of its 12 contacts, ranged in 3.

Orbit 2's four are the station's designed cost: 4.2 laps, one each, exactly the arithmetic above.
**Not a defect; do not re-audit.**

**Nobody dodges a cloud. Two places in the room it cannot reach, and you stand in one.** Against the
8.5 yd reach the orbits leave exactly these:

- inside **2.89 yd** of Sara (11.39 - 8.5) nothing reaches at all, which is where melee already stand
- between **20.36 and 22.63 yd** (11.86 + 8.5 up to 31.13 - 8.5) only the second orbit reaches

Nowhere is clear of every orbit — gaps are 9.4-9.9 yd against the 17 yd a cloud sweeps — and no
sidestep outruns one, because a spot clear of an orbit now is under it seconds later.

**Dodging anyway cost a phase.** With the trigger widened to 18 yd to buy warning, the node was asked
on nearly every tick against a clear radius unsatisfiable within 25 yd of Sara: it fired **8,260x**,
ran **4,987x**, and **4,225 of 4,298** phase 1 moves cited the cloud. Ranged walked a median 4.0 yd
for a median change in distance from Sara of **-0.1**, and 53% of their destinations landed 25+ yd out
where the third orbit catches them, smearing the back line over a **109 degree** arc. Melee and tanks
held their ground by orbiting Sara antipodal to the innermost cloud (median **163 degrees** off it), a
continuous 24 s lap that cost **43% of melee damage and 91% of the tank's**. Raid output fell
97.5k to 74.0k dps, 7 Guardians died against the previous pull's 12, phase 2 never came and the raid
wiped with 14 alive.

**So the cloud circle is a filter, not a dodge.** `ULDUAR_YOGG_SARON_CLOUD_AVOID_RADIUS` (9.5, the
reach plus a yard of slop) does three jobs and no more: the trigger reads it as "I am standing in
one", which is the case that must never be dropped because the aura re-arms; the hazard set keeps a
bot moving for some *other* reason from landing in one; and phase 1 alone overrides `RouteAcceptable`
to reject a candidate whose straight walk passes inside it. Only phase 1 checks the route, because
crossing a Death Ray to leave one still beats standing in it while crossing a cloud costs a Guardian.
Each cloud enters the set twice — where it is, and `ULDUAR_YOGG_SARON_CLOUD_LEAD_MS` ahead along its
orbit. Without the lead, destinations sat a median 16.3 yd from the nearest cloud against a 15.4 yd
baseline for standing still, which is chance.

**Nothing may jump in this room.** Blink, Disengage and the gap-closers put a bot somewhere nobody
picked, and in a room made of six fixed rings that means onto one: of 20 phase 1 casts in one pull,
**13 left the bot with more orbits inside the 8.5 yd reach than it started with**. Blink took bots
from the 21.5 yd station to 39.2-40.0, onto the fourth orbit; Disengage to 31.1-33.7, onto the third;
Charge, Intercept and Feral Charge fired straight out of the 2.89 yd cloud-free circle through the
first two. `YoggSaronDisplacementGuardMultiplier` zeroes all of them in phase 1, and Blink and
Disengage for the whole encounter — those two fire on "something is too close" rather than to close a
gap, so neither is aimed at anything, while the gap-closers come back in P2 and P3 where there is no
orbit to land on. Catch the whole `CastReachTargetSpellAction` family: the Fury chain is charge →
intercept → `reach melee`, so a partial veto only moves the problem down the list, and `reach melee`
surviving is what walks melee in on foot. Blink and Disengage are plain `CastSpellAction`s and have to
be named.

## The spacing nodes

Each phase puts everything it has to dodge into **one** node: two nodes at one relevance trade ticks
and walk the bot down the line between their destinations. Phase 1 takes clouds and Shadow Nova,
phase 2 takes Death Rays and Crush wedges, off a shared latch and sweep. What keeps them from
oscillating against hazards that never stop moving:

- **Trigger and clear thresholds kept apart** (nova 17 → 20, ray 9 → 14, Crush arc ±8° → ±14°). One
  threshold parks the bot on the boundary and re-fires the moment a hazard drifts a yard in.
- **A trigger inside its own clear radius is a band nobody asks about.** The action's early-out is
  "already outside every circle", so the *clear* radius decides when a bot moves and the trigger only
  decides whether the node is asked at all. Clouds at 10 against a clear of 14 first moved a bot with
  the cloud 10 yd out — **0.5 s** before reach, against the 1.2 s needed to cover 8.5 yd. Widening the
  trigger to 18 closed that band and broke the phase instead, which is why clouds are no longer
  dodged. The nova pair keeps the tight-trigger shape deliberately: its band (17-20) is margin, not
  exposure, since 18 yd is already past the blast.
- **The destination is held** until the bot arrives or a hazard drifts onto it, and the in-flight tick is
  claimed by returning true without touching the motion master.
- **`preferNear` is `ULDUAR_YOGG_SARON_MIDDLE`, not the bot.** Every candidate in a ring is the same
  walk away, so a bot-position bias is a no-op tie-break; biasing at the middle makes the dodge a
  sidestep rather than a run for the rim. The `accept` cap holds it inside spell range.

- **A melee move keeps the bot in its own swing range.** A melee bot that steps out of reach of its
  target is hauled straight back by `reach melee`, and the two traded the tick **252 times** in one
  pull while the spacing node returned FAILED 582 times against 618 OK. With nothing about to
  detonate, phase 1 puts "still within `meleeDistance` of my target" in `set.clear` and the cloud
  circles in `set.fallback`, so the retry drops the reach and keeps the clearance. **Only toward a
  target on the stack**: toward one further out the dodge *is* the chase. On 2026-09-16, 49 of 61 and
  69 of 86 melee spacing moves that left the leash had a target outside it, and landed at a median
  12-14 yd, on the innermost orbit.

- **Phase 2 does the same by preference.** `set.preferred` is tried ahead of `accept` and dropped when
  nothing satisfies both, so a bot in a hazard still moves: it asks for a spot still inside
  `meleeDistance` / `spellDistance` of the current target, which leaves `reach melee` and `reach spell`
  nothing to undo. Without it the 3 s hold lapses, the reach node walks the bot back at its target, the
  bot lands in a hazard and the spacing node walks it out again — **1565 handovers** in one phase 2,
  77 per melee bot and 69 per ranged, 754 yd of melee walking to finish 88 yd from the start.

**Yogg-Saron was the only anchored Ulduar encounter with no movement guard.** Iron Assembly, Mimiron
and Thorim each zero `ReachTargetAction` while a positioning node owns the bot
([../README.md](../README.md)); Yogg had a dps-target guard and a displacement guard, neither of which
sees it. `YoggSaronMovementGuardMultiplier` closes the two windows where the generic reach is simply
wrong and no others — it is the only way a bot closes on anything, so a blanket zero strands the raid,
and `reach party member to heal` is exempt outright:

- **The walk out of the knockback ring**, which spans the phase boundary: the window is phase 1 and
  the 6 s hold that outlives it is phase 2. Two nodes at `MOVEMENT_FORCED` and `MOVEMENT_COMBAT` traded
  the bot every ~300 ms for the whole walk.
- **Inside the ring in phase 2**, at exactly `ULDUAR_YOGG_SARON_BODY_KNOCKBACK_RADIUS` — the radius the
  spacing trigger fires at, so reach stands down only while that node owns the bot and is free the
  moment it is walked clear. A wider band leaves a ring where neither node moves anybody. Phase 3 is
  left out: its spacing node does not run, so nothing would walk the bot out in its place.

**A third window was tried below the platform and cost a whole room.** Zeroing reach while a
Laughing Skull was in arc left five bots in the Chamber holding a tentacle at 87% for **fifty
seconds**: reach sat at relevance 0.0, every melee ability beside it read `USELESS` for range, and
the facing node that was meant to do the walking instead returned false on all of it. A veto is only
ever as good as the node that moves the bot in its place, so make that node incapable of leaving the
bot somewhere reach would want to undo, and skip the veto - which is what the illusion facing node
does below. The early return for anything under z 300 is load-bearing on its own account: the brain
room middle is **2.3 yd** from the platform middle in 2d, so without it the ring test would veto the
reach of every bot standing on the Brain, 93 yd underneath it.

`MoveAwayFromCreatureAction` was rejected for this: no throttle, no latch, and it *maximises* distance
recomputed from the bot's new position every tick, so against six rotating rings the best answer rotates
with them.

**Two more generic movers own feet in phase 1, and neither was guarded.** Between them they put the
raid on the clouds:

- **`reach melee` walks melee *out*.** The displacement guard leaves it alive on purpose so melee walk
  in on foot, but its destination is the target, and 16 of 23 Guardians on 2026-09-15 lived outside the
  leash — eight parked on the 21.5 yd station. Median destination **10.5 yd** from the middle, which is
  the innermost orbit. Now zeroed in phase 1 while the current target is outside
  `ULDUAR_YOGG_SARON_P1_LEASH` **and a bot tank is alive to fetch it**; without that second condition a
  dead tank strands every melee bot out of combat.
- **`flee` walks ranged and healers off the station.** Not the panic route: median health when fleeing
  was **96.9%** and 74% of the moves were above 80%. It is `RangedCombatStrategy`'s `"enemy too close
  for spell"` at `ACTION_MOVE + 4`, true whenever a Guardian stands on a caster — and the 3 yd it gives
  up escapes nothing, since Dark Volley is 35 yd and the nova is a death explosion. **957** moves in one
  phase 1, **96.4%** ending further from the band, 36.5% inside 20.1 yd where the innermost orbit
  reaches. Zeroed for ranged and healers in phase 1; melee and the tank keep it, because the leash
  already owns them and no melee bot fled at all.

Relevance is why both ran. Each loses to the station node at `ACTION_RAID` — but only while that node
is *active*, and it stands down the moment the bot is in band and stacked. **A node that yields once
satisfied owns nothing between its own firings; only a guard does.**

**Who dodges what, in phase 1.** Nobody dodges clouds; the only cloud move is stepping off one a bot
is already standing in. A Guardian's nova is dodged only by a bot that
would not otherwise survive it: ranged and healers when one is at or under
`ULDUAR_YOGG_SARON_GUARDIAN_NOVA_HEALTH_PCT` **and chasing them** — at spell range nothing else reaches
them — and anyone holding Sara's Fervor, on the wider gate above. Running from every Guardian instead is
what scattered the raid to the rim on 2026-09-14, where seven bots were picked off one at a time between
1:26 and 1:30. `GetYoggSaronNovaThreats` is the single owner of that rule, so the trigger and the action
cannot disagree about who is running.

**Ranged and healers hold one spot on the second orbit.** Guardians die on Sara, so her spot is a
standing nova hazard for everyone who need not be in it — ranged and healers were inside it for 37%
and 49% of one phase 1, and a 20 yd standoff alone took both to **0%**. The station is
`ULDUAR_YOGG_SARON_P1_RANGED_SPOT` **(1958.78, -25.587, 324.889)**, due west because the raid stages
there, navprobe-clean on the WMO floor — and so is the whole 21.5 yd ring, 24/24 headings, if it ever
needs moving. 21.5 is the midpoint of the single-orbit band, hence the second orbit itself: standing
*on* a ring is what buys the most room from its neighbours, 1.13 yd either way, which is all
`ULDUAR_YOGG_SARON_P1_RANGED_BAND_TOLERANCE` (1.0) has to give. The third orbit would be the cheaper
station — one Guardian per 65 s against 45 — but it sits 31.2 yd out against a 28.5 yd `spellDistance`,
so a bot posted there walks itself back in.

They **stack** on it rather than spread, which inverts the usual raid rule and holds only because the
nova cannot reach the station. A cloud is in contact for `(8.5 + blob + 8.5) / 3` seconds and re-arms
10 s after each summon, so any blob under 13 yd across costs exactly one Guardian per pass:
`ULDUAR_YOGG_SARON_P1_RANGED_STACK_RADIUS` (5) gives 9 s of contact and **one Guardian per 45 s orbit
for the whole back line**. Spread instead over the 109 degree arc a phase of dodging produced, the
cloud has somebody in reach for most of every orbit. The parked latch widens only the stack radius,
never the band: there is no margin there to spend.

**Melee and tanks are leashed to Sara, not stationed on her.** Beyond `ULDUAR_YOGG_SARON_P1_LEASH`
(**6.5 yd**) the bot walks back to the middle, and it is not released until
`ULDUAR_YOGG_SARON_P1_LEASH_RELEASE` — the **2.8 yd cloud-free radius**, not a boundary a step inside
the leash. Anything between the two lets go of the bot somewhere the innermost orbit sweeps, and the
old 12 let go of it on the orbit. The node reads a live Guardian before the phase, which is four 200 yd
sweeps and would otherwise run every tick for melee standing outside the leash in P2 and P3; the price
is no walk back before the first spawn.

**Why 6.5, and not the 15 the nova needs to reach Sara.** Two different radii are both 15:
65719 reaching her, and 65209/62714 reaching the raid. The back line stands at 21.5, so a Guardian
dying more than **21.5 − 15 = 6.5 yd** out catches everyone rather than the melee pile — measured,
novas at 2.2-4.0 yd hit 9-10 players and novas at 8.5-14.8 yd hit 22-24. A leash at 15 was set to the
wrong one of the two and let a third of the novas through; at 6.5 a kill still counts for Sara and
cannot reach the station. The cost is a narrower band against `reach melee`, and `yogg.p1leash` counts
the flips.

Nothing pins melee inside 2.89 yd, though — they get there because a Guardian walks to whoever holds
threat and `meleeDistance` is 0.75. Measured they sat at **2.5-2.9 yd** and triggered no inner-orbit
Guardian at all; all three of one pull's came from the human player at 6.7-13.4 yd. **That is the first
thing to check** now that they hold still instead of orbiting: settle beyond 2.89 and the innermost
cloud harvests one Guardian every 24 s, and the leash has to come in past 6.5 as well.

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

Kill order — **phase 1: one Guardian for the whole raid** (threat section below). Splitting damage is what killed the raid on 2026-09-14: two Guardians rode down in
lockstep from 63.6%/82.3% to 1.4%/2.8% and crossed zero inside one second, and the **double** nova put
228,396 over 16 hits and killed all eight melee in **16 ms**. Four earlier single novas were all
survived. Brain level: Influence Tentacle →
nearest other illusion add → the Brain. Boss room:
**leftover Guardian of Yogg-Saron** → Crusher (**ranged only**, below) → Constrictor → Corruptor →
**Marked** Immortal Guardian (36064) → Immortal Guardian (33988) → Yogg.

**The leftovers lead the boss room, and leaving them off it cost a pull.** They survive the transition,
keep casting a 35 yd Dark Volley nothing walks out of, and regenerate to full the moment the raid takes
a portal. With no tier they fell through to the `dps target` fallback, which is nearest-first per bot:
five of them took **~1,978,370** — two Guardians' worth — for **zero kills**, four ending between 11.9%
and 21.2%, while dealing 735,989 back. That is the same split-damage failure phase 1 has a kill order to
prevent.

Every Guardian tier is picked lowest-health first and then held outright: an order that flips mid-fight
resets every swing and cast timer in the raid. Phase 1 replaces both with a shared focus, below. An
Immortal Guardian below 10% is Weakened and only Thorim's Titanic Storm can finish it, so it stops
being a target at all.

## Threat: the redirects are fine, the taunt budget does not stretch

**Do not re-investigate whether bots redirect threat in P1 — they do, correctly.** Over one phase 1:
Misdirection **6 casts, all on the tank**; Hand of Reckoning 12; Righteous Defense 11, six of them
aimed at a ranged or healer being hit. Tricks of the Trade went to a melee dps 6 of 6, and that is also
right: `TricksOfTheTradeTargetValue` prefers the main tank and falls back to the highest attack-power
melee when `TankNeedsRedirect` is false — when the tank already out-threatens the rogue on that target.
Leave all three alone.

None of it governs the Guardians the raid is *not* on. Guardian time on target: **ranged 39.8%, tank
35.2%, melee 17.3%, heal 5.9%** — **63% on a non-tank**, with 11 of 23 living mostly on a ranged bot.
One tank, ~12 single-target taunts in 190 s, 23 independent threat tables from `SetInCombatWithZone`,
Hand of Reckoning on an 8 s cooldown: the budget is about **one taunt per Guardian death** and cannot
cover the room.

So spend it on the Guardian whose death location decides the phase. The generic paladin taunt is
reactive — whoever last hit a raid member — and its aim decays as they pile up: on the focus Guardian
**6 of 13 times**, all six with 1-5 alive, then rank 3, 3, 4, 6 and **7 of 9**. `yogg-saron guardian
control` runs in phase 1 as well now, holding the tank inside the **2.8 yd** cloud-free circle rather
than phase 3's 5.0 and taunting `YoggSaronPhase1TauntTarget`: the raid's focus first, skipping any
Guardian already inside the leash or already walking at a tank. Taunt reaches 30 yd, which covers the
whole 21.5 yd back line, so this never walks the tank out after one.

**A taunt only pays if the raid waits for the walk**, and per-bot kill orders cannot wait. One pull
taunted the focus at 22.4 yd and killed it at **18.0 yd, 6.4 s later**. Preferring a Guardian inside
the leash, with each bot holding its own lock, then failed both ways on 2026-09-16:

- **Split.** Ranged dropped a Guardian that chased a caster to 17 yd. Six melee kept it, busy in the
  spacing node (62) instead of the resolver (61), and once it walked back inside 15 yd nothing
  re-merged them. The two rode down 65/65% → 1/3% and died **17 ms** apart. Bot non-tanks spent
  **28.6%** of that phase 1 on two or more Guardians, against 8.0% the pull before (`--threat`).
- **Fallback.** With nothing on the stack, lowest health anywhere won. One Guardian was burned 16 yd
  out 4 s after the tank's taunt landed, and its nova one-shot a Fervor holder. The next was already
  pulled by Righteous Defense, died 1.7 s later at 18 yd, and two back-line novas **2.5 s** apart
  killed five.

So phase 1 has **one focus per instance**, `YoggSaronPhase1Focus`. Every non-tank attacks it, and only
a bot inside the room re-picks it. A Guardian outside the leash is worn down only to **35%**
(`ULDUAR_YOGG_SARON_P1_PARK_HEALTH_PCT`) and then parked: the focus moves on, a bot still on it drops it
(pets too), and there is no `dps target` fallback. The taunt already takes the lowest-health Guardian
outside the leash, which is the parked one, and inside 6.5 yd it is killable again. The floor has to
cover the ~12 s fetch (taunt cooldown, the walk, and a taunted Guardian that stood still for 2 s)
against the 1.7-1.8%/s, peaking near 3%/s, that an untargeted Guardian near the station still loses to
splash. Without a living bot tank nothing fetches, so the floor is off. The focus is still given up
past **15 yd** for a killable one inside **6.5**, a gap that stops a Guardian on the boundary flipping
it. `yogg.p1focus` records each change: `picked`, `parked`, `abandoned`, `none`. AoE is untouched, so
cleave on two Guardians on the stack can still bring both down together.

`YoggSaronPhase1GuardianPreferred` orders both the focus and the taunt, so the two cannot disagree.

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

**The dodge needs somewhere legal to go, and the retry has to keep the wedge.** The wedge model is
right: 18 of 19 Crush hits landed within 8° of the tentacle's facing, exactly
`ULDUAR_YOGG_SARON_CRUSH_TRIGGER_ARC`. What failed around it was the box and the retry. Phase 2 shared
phase 1's 35 yd cap from the body while melee were already beyond it in **37-50%** of samples and
tanks in **64-81%**, so every outward candidate was rejected and a bot needing a three-yard sidestep
had to walk *inward* along a 25 yd wedge; `ULDUAR_YOGG_SARON_P2_SPACING_MAX_FROM_MIDDLE` is 55, still
inside the room's 60.8 yd outer orbit. And when nothing cleared both shapes the retry used to keep the
Death Ray circles and drop the wedge, on the reasoning that a ray is certain death and a wedge a
coin-flip. The pulls say otherwise — Death Ray **1 death** over 11 hits for 150,504, Crush **4** over
19 for 485,964, ~25k a hit against melee pools — so the retry keeps the wedge and gives up the rays.

Melee exposure is the root cause, and the brain team below is most of the answer: melee sit within
25 yd of a live Crusher in **36.2%** of samples against 15.5% for ranged, and inside an 8° wedge in
**6.5%** against **1.3%**, so sending melee down the portals takes them out of the arena for the
length of every window. The tentacle cannot be tanked either — `DamageTaken` does
`DoResetThreatList(); AddThreat(who, 100000); AttackStart(who)` on any direct damage, so it re-faces
whoever hit it last, in both traces a hunter pet. No bot controls where the wedge points.

**An unoccupied Crusher cannot Crush at all, and the pets were the occupants.** `UpdateAI` swings
only at a victim inside melee range and `DoMeleeAttackIfReady` tests it again, so with nothing in
reach there is no white swing, no proc and no cone — it channels Diminish Power instead. Once melee
stopped targeting 33966 the only things left in there were pets: every one of six cones in one pull
fired with **no player inside 12 yd** and a Felguard at 5.5, and the closest approach all fight ran
Shadowfiend 0.4, ghoul 1.1, Felguard 1.2, hunter pet 1.3 — the first actual player at 2.3. It cost
**162,663 damage and four killing blows**, and 251 hazard rows routing 25 bots around floor that was
never dangerous. `yogg-saron pet guard` walks `m_Controlled` and pulls anything that is a Crusher's
victim or inside its melee range. Either half of that test is enough on its own:
`SetInCombatWithZone` hands the tentacle a threat list holding the whole raid, so a pet that never
attacked can still come up as the victim.

**Pulling a pet is half a command; it also has to be given somewhere else to be.** `PetAI::UpdateAI`
re-selects the moment the pet has no victim, and `SelectNextTarget` checks whoever is attacking the
pet first and `owner->GetVictim()` third - both the Crusher, because the only tick this node ever
fires in is one where the owner is on a Crusher itself. A first version stopped the attack and
recalled the pet, and the pet was back in melee range before the bot's next tick: 336, 321, 312 and
165 samples with a Crusher targeted, four pets between 0.0 and 0.4 yd of one. A pet that *has* a
living victim is never re-selected - `UpdateAI` takes the melee branch, and `AttackedBy` and
`OwnerAttacked` both bail on "prevent pet from disengaging" - so `YoggSaronPetFallbackTarget` hands
it the nearest live phase 2 add that is not a Crusher and the one command sticks. `REACT_PASSIVE` is
the fallback for an empty floor, since passive is the one state all three of those honour; restored
to defensive the tick the pet is clear, and only for pets this node silenced. It stays a per-tick
node rather than a latch: an uncommandable guardian can walk back in at any point.

So `GetYoggSaronCrushWedges` raises a wedge **only for a Crusher that currently has something inside
its melee range**, which keeps it armed for exactly the case the guard cannot close — an
uncommandable guardian that got back in — and switches it off the rest of the time. It costs the
Diminish Power interrupts that pet melee was buying, ~1.5 s apiece.

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

## Phase 2: the body is a wall, and the portals run on a clock

**Yogg's body knocks players away once a second, permanently.** `ACTION_YOGG_SARON_APPEAR` casts
`SPELL_KNOCK_AWAY 64022` on him and never removes it: an infinite self aura,
`SPELL_AURA_PERIODIC_TRIGGER_SPELL` every **1000 ms**, firing **64020** — `SPELL_EFFECT_KNOCK_BACK`,
radius index 61 = **14 yd**, horizontal speed 15 — with no `conditions` row, no script filter and no
combat-reach bonus, since `Spell.cpp` gates that on `IsControlledByPlayer` and the caster is a
creature. His model sits at z 329.397 over a floor of 324.89-325.19, so it is a **13.26 yd horizontal
ring that never goes away**. Nothing can sweep for it; it reaches a trace only as a `haz` circle and
as `yogg.knockback`.

**The ring arrives 18 s after Sara dies, on top of the melee pile.** She hits 0 and Yogg is summoned
**invisible** in the same tick; `ACTION_YOGG_SARON_APPEAR` casts Shadow Barrier and 64022 together at
the end of the transformation dialogue — 4 + 5 + 4.5 + 4 s of it plus the 500 ms
`EVENT_SARA_P2_START`, measured 18.0-18.3 s. The phase reads 1 throughout and 2-6 Guardians are still
alive, so the leash kept hauling melee and the tank onto the middle: before the walk out, in every
pull on record **9 of 9 melee and the tank stood in the ring when it lit**, against **0 of 10 ranged
and 0 of 4 healers**, already 8.2 yd clear of it on the 21.5 yd station. With it, 2 of 9. So the walk
out belongs to melee and the tank alone — to `ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS` along
the bearing each already holds, which fans nine of them around the ring instead of stacking them on a
point.

**Yogg without Shadow Barrier is the whole window**, and `SetVisible(false)` does not hide him from a
grid search, so `YoggSaronHandoverState` reads it from the first tick. P3 strips the barrier again and
the Brain separates the two: it is summoned in the tick the barrier first lands, so it is up for
everything after the window and absent for all of it. The walk is **led, not immediate** — leftover
Guardians stop counting for Sara the moment she dies (`DamageTaken` returns early on `_secondPhase`)
but their novas still land for 25k, and one dying at the clearance radius reaches the back line where
one dying on the leash cannot, so the raid holds the middle until
`ULDUAR_YOGG_SARON_HANDOVER_LEAD_FLOOR_MS` out. Nothing in the world counts the dialogue down, so the
clock is predicted off the first sighting, like the portal wave's.

**The walk has to outlive the ring.** "Yogg without the barrier" ends on the tick the barrier lands,
and melee released there walk straight back under the knock back — three of nine were inside 13.3 yd
again within 600 ms. `ULDUAR_YOGG_SARON_HANDOVER_HOLD_MS` (6 s) keeps `clearing` true past it, and the
movement guard below keeps `reach melee` off the bot for the same window.

It costs melee nothing — Yogg's `CombatReach` is **30** (display 28817, `BoundingRadius` 0), so melee
range on him is `1.5 + 30 + 2.67` ≈ **34 yd**, and the P3 melee spot is already 18.4 yd out. What it
costs is the *crossing*: **42%** of portal walks routed the bot within 12 yd of the body. So P2
spacing carries the ring as a standing hazard circle, its trigger reads it too, and both the spacing
walk and the portal walk filter the route. A bot already inside the ring is exempt from that filter —
every short step out passes close to the middle by definition, so judging those would reject the only
walks that end the problem. A blocked crossing gets one waypoint at
`ULDUAR_YOGG_SARON_BODY_DETOUR_RADIUS` (24 yd) on the bisector of the shorter arc: each leg halves the
turn the next one makes, so one is enough, and the worst case — a half-turn — passes
24 × cos 45° = 17.0 yd from the middle.

**The walk at a target on the far side of Yogg ends on top of him.** `ReachCombatTo` shortens its
path to `disToGo / 2 + distance`, so half the way to a target across the room is the body, and the
reach nodes are the one walk the route filter above does not cover. One pull had the resolver hand
eight ranged and healers a Crusher **56 yd** away past the middle; `reach spell` put all eight at
**7.6-9.4 yd** of it inside a single second, and the pulse 0.7 s later threw thirteen bots. Every
other read of the ring asks where the bot is standing, which is a tick too late against a hazard
that fires once a second. `yogg-saron body detour` at `ACTION_RAID - 1` asks where the walk is
going: while the bot is out of its own reach of the target and `YoggSaronRouteClearOfBody` says the
straight line passes inside the ring, it owns the approach and walks the same arc waypoint the
portal spread uses, then stands down. Under every raid node and over `charge` and both `reach melee`
entries, so the price is one shadowed gap-closer for the length of the arc leg. Probed as
`yogg.detour`.

The 13.3 yd model held up under test: 23 launches matched a Knock Away cast and the last grounded
sample before each was **7.8 to 12.9 yd** out. Measure that sample, not the first airborne one,
which is already a third of a second into the flight and reads 6 yd too far.

**The portals are creatures, one-use, and on a clock the raid has to beat.**
`NPC_DESCEND_INTO_MADNESS` (34072), `TEMPSUMMON_TIMED_DESPAWN` 25 s, `OnSpellClick` clearing the
npcflags so each takes exactly one passenger. `EVENT_SARA_P2_OPEN_PORTALS` fires **60 s** after P2
starts and repeats every **80 s**; clearing a room delays Sara's other events but explicitly
reschedules this one. `AddPortals` spawns `RAID_MODE(4, 10)`, so **only the first four table entries
exist in 10-man**. They are not hostile, so no snapshot samples one — `yogg.wave` is the only record a
wave happened at all.

The core's table is `yoggPortalLoc`, and all ten of the module's old coordinates were wrong:

```
(1964.60,-42.71,325.08) (1986.94,-46.21,324.98) (1989.50,-6.70,325.08) (1965.52,-8.09,324.95)
(2000.84,-25.40,325.19) (1960.22,-26.14,325.01) (1976.30,-47.83,325.11) (1997.69,-37.46,325.04)
(1998.07,-13.36,325.17) (1976.99,-3.96,325.17)
```

All navprobe-clean on the floor, settled z 324.87-325.04, **20.1-23.2 yd from the body** and so
outside the knockback ring. Keep them in code rather than reading live creatures: the team has to be
*standing* on its spot before any portal exists.

**Spread before the wave, and assign by distance.** By group index the assigned walk ran a median of
**41-44 yd** when the nearest spot was **12-15**. Nearest-first instead, latched once per wave so it
does not churn as bots move, off a wave clock kept per instance: predict the first wave at +60 s,
re-anchor on every wave a bot actually sees, and roll the prediction forward by a period when one
passes unseen — otherwise it reads "any moment now" forever and parks the team on its spots for the
rest of the fight. A bot walks once the next wave is inside `max(8 s, distance / runSpeed × 2)`, the
same adaptive shape as the exit lead, and holds until the portal appears under it. `yogg.portal` is
the state machine: `notteam` / `waiting` / `spreading` / `holding` / `late` / `clicking`.

**Rebuild the plan when the portals spawn, and take one that is still there.** The wave a plan is
for carries the same ordinal before and after the portals appear, so a latch keyed on that alone
never rebuilds a second time: every assignment in one pull was stamped **55 to 61 s** before the
wave it served, off positions taken at the end of the previous one. And a slot is only ever a
suggestion, because the click takes whichever portal is inside
`ULDUAR_YOGG_SARON_PORTAL_CLICK_RADIUS` - in one wave five of six bots used somebody else's, and the
three left over stood **0.2 to 0.5 yd** from a spot whose portal was gone, flipping `holding` to
`late` and back every 0.7 s for the rest of the window while three portals went unused. So the plan
rebuilds once more on the tick the portals are first seen, and a bot whose own spot is empty walks
to the nearest portal still alive. Arriving and clicking share one radius now: 3 yd for arrival
against 2 for the click meant `holding` did not imply clickable. Portals taken per wave ran 7, 6, 2,
0 of 10.

**The brain team is melee first, then exactly one healer, then ranged; tanks never.** The room is a
60 s race on foot, and it doubles as the Crush answer above. Order within each band by GUID so every
bot derives the same team from its own seat. The trigger and the action used to build the list
differently — one skipped the master, the other did not — so they disagreed about who was on it;
`GetYoggSaronBrainTeam` is the single owner now.

**Brain Link's partner is readable from the cast, never from an aura.** 63802 goes on **one**
player: `spell_yogg_saron_brain_link_aura` picks a random living player within 50 yd on apply, keeps
that GUID to itself, and drops the link if either dies or they end up more than 10 yd apart
vertically. Neither 63803 (apart, DBC damage plus −2 Sanity on **both** ends past **20 yd**) nor
63804 (together) leaves an aura behind, so no aura test can name the partner and
`TooFarFromPlayerWithAura` cannot help - it measures the gap to *other holders of the same aura*, of
which there are none. But the aura casts one of those two at the partner **every second** for the
life of the link, so an `AllSpellScript` on `ALLSPELLHOOK_ON_PREPARE` in `UldBotScripts.cpp` latches
the pair, per instance, expiring after three missed ticks. Both ends read it, so both walk - unless
one is under z 300, where there is nothing left to close on.

They walk to their **midpoint**, not at each other: one bot chasing another that is moving away at
the same speed never arrives. Closing on the nearest raider instead, one link ran 17 walks while the
gap grew from 45.8 to 78.5 yd, and nine of ten links in that pull sat past 20 yd for their full 30
s, for **477,727** damage and 676 Sanity. The midpoint is pushed back out to
`ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS` when it lands inside the ring, which is exactly what
two bots on opposite sides of Yogg produce. `yogg-saron brain link` runs at `ACTION_RAID + 1.5`,
over the dps resolver and the Sanity Well walk and under the hazard dodges, since a link costs 2
Sanity and a shared hit a second while a Death Ray costs the bot, and closes to
`ULDUAR_YOGG_SARON_BRAIN_LINK_CLOSE` (15, a margin under the 20 so drift does not re-break it).
Probed as `yogg.brainlink`.

It was doing none of that. `TooFarFromPlayerWithAura` had an unconditional
`return !debuffedPlayers.empty();` above its range loop and never read the `range` argument at all;
the action walked to the first group member carrying the aura rather than to the partner; and at
`ACTION_RAID` four nodes outranked it, so it **issued zero moves in a whole fight**. Seven pairs sat
24-60 yd apart for the full 30 s each, for **355,013**.

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
it is gated on server facts, never on a human: from an illusion room, no live tentacle **and** that
room's illusion door (`194635` Chamber / `194636` Icecrown / `194637` Stormwind) reading
`GO_STATE_ACTIVE`, which the Brain sets in the same branch that fires when the last tentacle dies.
Reading it wrong leaves the bot standing still rather than walking in to die.

**From inside the brain chamber, ask the doors instead.** `DoAction` shuts all three when it
prepares an illusion and opens exactly one on the last kill, so **any** door standing open means
this wave is done — an exact test where a radius cannot be one. The sweep it replaces reached 110 yd
and the Chamber's far tentacles spawn 167 yd out, so a bot on the Brain could be told the room was
clear with six of them alive.

The old gate waited on the bot's **master**, a human, to stand in the brain room, and Brain health only
ever moved in windows where one did — the windows without a human left it untouched.

**The illusion is random per attempt, in both start and direction.** `Reset()` does
`_currentIllusion = urand(1, 3); _isIllusionReversed = urand(0, 1);` and steps ±1 with wraparound per
wave, so nothing may assume a fixed cycle. Every portal in one wave leads to the same room.

**The three room radii overlapped, and the brain room lost.** At 150 yd apiece against middles only
190-203 yd apart and a brain room 108-124 yd from each, **every** brain-room sample tested as
Stormwind — it is checked first — and 14-21% of illusion-room samples took the wrong room's name. 60
covers each room's landing spot (48.9 / 54.0 / 55.9 yd from its own middle) and every Laughing Skull
spawn (52.8 at the furthest), while the nearest rival middle is 190 yd away. `YoggSaronRoomOf` is the
single resolver, brain room tested first, and `yogg.room` records what the bot *read* — the difference
between that and where it stood is the whole defect.

**Nothing walked a bot into the room, and that deadlocks.** The dps resolver needs
`IsWithinLOSInMap`, two of the three rooms put their tentacles behind a doorway, and the only mover
inside a room was the walk to the Brain, which needs the tentacles already dead. So the tentacle is
out of sight → nobody walks in → it lives → the door stays shut → the Brain is never touched. Share of
samples parked within 6 yd of the landing spot: **Stormwind 71%, Chamber 51%, Icecrown 6%** — the last
an open courtyard — with **65%** of all brain-level samples carrying no target at all and
`DpsAssistAction` zeroed encounter-wide, so no fallback. One bot stood on the Stormwind landing
coordinate motionless for 50 s, twice, with four Suits of Armor alive 28-82 yd further in.

The fix is a walk to the room's middle, which is the centroid of that room's Influence Tentacle summon
group (`creature_summon_groups`, summonerId 33890: group 1 Chamber, 2 Icecrown, 3 Stormwind) and
navprobe-clean with `PATHFIND_NORMAL` from each landing spot. `YoggSaronRoomStateOf` owns the question
and writes `yogg.roomstate`: `walkingin` / `fighting` / `doorshut` / `tobrain` / `atbrain`.

**Nine of the sixteen entries in the old illusion target list could not be killed.** From
`creature_template`: Alexstrasza, Malygos, Neltharion, Ysera, the Immolated Champion, Garona and King
Llane are faction **35**, friendly RP actors; The Lich King (33441) is faction 14 and hostile but
carries **11.1 M** health, never attacks and cannot be killed — a pure time sink in Icecrown. All
sixteen sat in one tier *above* the Brain, so any decoy in line of sight blocked it forever. The list
is the eight real entries: 33943 plus its six disguises (33433 Suit of Armor, 33567 Deathsworn Zealot,
33716-33720 Consorts). Which disguise a tentacle wears comes from where it spawned — `x ∈ (2000,
2150)` a Consort, else `y ∈ (−150, −90)` a Zealot, else a Suit of Armor; the wiki has Zealots and
Suits the other way round and the script wins. The disguise is `Creature::UpdateEntry`, not an aura,
and `UpdateEntry` **preserves current health**, so a Suit of Armor showing 6% is a full-health
tentacle carrying 8,000 (10) / 40,000 (25).

**Scope the tentacle read to the room.** The Stormwind and Chamber middles are 200.8 yd apart, so the
old 200 yd sweep was one yard from reading the next room's tentacles — and reading it wrong is not a
wasted tick, it is `Unit::Kill(who, who)`. The Brain's own test is `_tentacleCount < _tentacleTotal`,
a per-wave counter, so per-room is the right scope.

**Read the disguises too, or a full room reports empty.** Every spawn in all three summon groups
satisfies one of the three branches above, so **no tentacle is ever entry 33943 when the raid
arrives**, and `boss_yoggsaron_influence_tentacle::DamageTaken` reverts it only once something has
hit it. A sweep for 33943 alone therefore answers *cleared* on arrival, `YoggSaronRoomStateOf`
returns `doorshut` instead of `walkingin`, and `walkingin` is the only state the walk into the room
moves on — which closes the same deadlock from the other side: no walk → no line of sight → no
target → no damage → no reveal. Icecrown and the Chamber escape it on luck, having sight of a
tentacle from the landing spot, so the dps resolver picks a disguise anyway and the first hit
unlocks the room: 3.7 s and 1.8 s to the first cast in one pull. Stormwind's sit behind the Keep
doorway, and there seven bots stood on the landing coordinate — **0.0 yd moved over 80 s**, 0 casts
at a tentacle — until Induce Madness took all 100 Sanity off nine of them.
`YoggSaronLiveIllusionMob` sweeps all eight entries in one grid visit and owns every "is a tentacle
alive" read, the `yogg.tentacle` probe included.

**The Laughing Skull cannot be killed, only faced away from.** 33990, faction 14, `unit_flags`
`UNIT_FLAG_NOT_SELECTABLE` with `flags_extra 128`: neither targetable nor killable.
`creature_template_addon.auras = 64167` triggers **64168 every 1000 ms — 1750 shadow damage, −2
Sanity, 30 yd** — and its target filter is exactly `target->HasInArc(M_PI, caster)`, so only players
with the skull in their front 180° are hit. Four per room, despawned when the last Influence Tentacle
dies. It was entirely unhandled: **149 gaze hits for 137,815 damage** and 298 Sanity in one wipe.
`yogg-saron laughing skull` faces away from the centroid of the skulls in arc, at `ACTION_EMERGENCY`
beside the Yogg gaze node — the two cannot share a bot and never need to, since Yogg's own Lunatic
Gaze is a P3 self aura on the platform and the skulls only exist below it. Probed as `yogg.skull`.

**Facing away must never cost a tick.** The node returned `true` whenever any skull was in arc, and
the engine ends a tick at the first action returning true, so at `ACTION_EMERGENCY` it starved the dps
resolver, the walk into the room and the exit — while `set facing` (37), `AttackAction` and
`PlayerbotAI::CastSpell` each turned the bot back at its target inside the same tick. A bot flipped
between two orientations about once a second and stood on one coordinate for **48 s** with an
Influence Tentacle 78 yd away; **six of nine melee** went that way in one pull, four of them caught by
Induce Madness at 263.9 s and dead at 323.9 s. It now yields outright while the bot has a live target
— fighting is worth more than 1750 damage and 2 Sanity a second, and a bot cannot face away from what
it is attacking in any case — corrects a heading only past `ULDUAR_YOGG_SARON_FACING_TOLERANCE`
(0.1 rad, ~6°), and never claims the tick. The 30 yd read is measured plainly too:
`GetCreatureListWithEntryInGrid` is bounding-radius inclusive, and **164 of 235** probe flips in one
pull had no skull inside 30 yd at all.

**Where the bot stands is the only thing that can hold a facing.** The heading is not the bot's to
keep, but the side of the tentacle it fights from is, and the geometry is generous: with the
tentacle and the nearest skull inside 90° of each other in **459 of 640** samples — median
separation 72° — the bot was nearer the tentacle in **73%** of those, a median 4.3 yd against 22.4
yd to the skull, so a sidestep of a few yards swings the skull behind. `yogg-saron illusion facing`
is a spacing node whose `clear` predicate asks whether the heading a candidate spot *would force*
leaves every skull within 30 yd outside the front 180° **and** whether that spot is still inside
melee or spell range of the target. In range is a requirement, not a preference the sweep may drop:
a spot out of range hands the bot back to `reach melee`, which walks it at the target and undoes the
sidestep. With every spot it can pick already in range, reach is not useful from there
(`ReachTargetAction::isUseful` is false once the bot is within `distance`) and no movement guard is
needed - the one that was tried froze a room for fifty seconds. It has no fallback either: every
spot the sweep rejects is one the bot would be gazed on anyway, so standing still and fighting
through it beats walking for nothing. Anchoring is the one thing the base class could not already
do: its sweep was centred on the boss platform, 93 yd above and up to 124 yd away, so `Anchor()` is
virtual and this node returns the room middle. Untreated it cost **122,980 damage and 272 Sanity**
in one pull, 14% of everything lost.

**There is no way out of an illusion room until its tentacles are dead.** All three Flee to the
Surface goobers (194625) stand in the brain chamber — (2000.65, 5.79), (1943.06, −23.51),
(1998.42, −59.85) — **93 to 109 yd** from an illusion room's middle and behind `GO_*_ILLUSION_DOORS`,
which the Brain sets `GO_STATE_READY` when the illusion starts and `GO_STATE_ACTIVE` only in the same
statement that despawns every skull. Killing all eight tentacles in T ms opens that door **and stuns
everything upstairs for 60000 − T ms**, while nothing interrupts the cast itself — `_induceTimer` is
only a stopwatch for the stun length. So the exit node wants a door gate, not priority: on its own
`YoggSaronShouldLeaveBrainLevel` started hauling bots at a shut door half a minute early and **53 of
62** of its walks came back as the same unreachable point re-issued. The brain chamber is exempt from
the gate, because the portals are in there and the next wave's tentacles must not strand a bot that
already made it through. `yogg.tentacle` records how close anyone actually got: 151 casts across three
waves and eleven bots killed none, and the Brain finished a 4 minute 42 second phase 2 at 100%.

**No Sanity Well reaches the brain level.** All five stand on the platform and nothing restores Sanity
underground, so `yogg-saron sanity` could only ever walk a bot at something it would never get to. It
stands down below the floor.

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

**Twenty-one `yogg.` probes and a reader.** `tools/botobs/bosses/yogg_saron.py` prints phases, cloud-orbit
exposure, portal waves and assignments, brain-room occupancy and Brain health, Crush and knockback
exposure per role, and Sanity minima — and names any key missing from the whole trace, because a key
declared in source and absent from every trace of its own boss means the recorder is dropping it,
not that the thing never happened. The keys are `yogg.phase`, `yogg.engaged`, `yogg.room`,
`yogg.roomstate`, `yogg.cloudreach`, `yogg.knockback`, `yogg.crush`, `yogg.deathray`, `yogg.wave`,
`yogg.portal`, `yogg.portalslot`, `yogg.brainteam`, `yogg.skull`, `yogg.exit`, `yogg.handover`,
`yogg.squeeze`, `yogg.brainlink`, `yogg.tentacle`, `yogg.gaze`, `yogg.petguard` and `yogg.detour`,
beside the older `yogg.walk`, `yogg.station`, `yogg.p1dodge`, `yogg.p1station` and `yogg.p1leash`.
Two hazards go to the timeline only because nothing can sweep for either: the body's knockback
circle, and each Crusher's wedge carrying facing, arc and range so it can be tested by hand
afterwards.

Three of its views exist because this fight keeps failing in ways the per-mechanic sections cannot
see. **Vetoes**, tallied by multiplier and action, because a zeroed walk with nothing walking in its
place is a bot standing still and nothing else names it. **Frozen bots**, the longest a bot held a
target and cast nothing, which is that same failure from outside. And **launches**, matched to the
walk that aimed into the ring rather than to the last walk issued, which is usually the dodge out.

**Do not add a Sanity probe.** 63050 is already in the aura stream — 467 and 819 rows across the two
attempts, with 63752 low-sanity and 63120 Insane beside it — as are Grim Reprisal 64039 and Lunatic
Gaze 64168 in `dmg`. A removal row carries no stack count, so skip `r:1` rows when taking a minimum or
every bot reads as Insane.

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

`yogg.phase` dates the transition directly. Before it existed the tell was the script forcing Yogg's
health to exactly 30% when the Brain reaches 30%.

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

**A paladin can do it for somebody else with Hand of Protection**, which covers the other eight
classes. 10278 grants `SPELL_AURA_SCHOOL_IMMUNITY` over school mask 1 and carries
`SPELL_ATTR1_IMMUNITY_PURGES_EFFECT`, so `HandleAuraModSchoolImmunity` strips every non-positive
physical aura the immunity would cover; Squeeze is physical, not passive and lacks
`SPELL_ATTR0_NO_IMMUNITIES`, so `CanDispelAura` passes and the tentacle dies on removal.
`yogg-saron squeeze rescue` at `ACTION_RAID + 6` — above the Sanity Well retreat, since cutting
somebody out of 7.5k a second beats walking somebody else to a well — takes the lowest-health victim
inside the spell's own **30 yd**, skips one carrying Forbearance (25771, matched by id), and claims
that victim for `ULDUAR_YOGG_SARON_SQUEEZE_CLAIM_MS` so three paladins do not spend three cooldowns on
one tentacle. Squeeze costs no Sanity — `spell_yogg_saron_sanity_reduce` has no case for it — so this
is damage and healer load only: 18 grabs, **477,137**, longest **34.8 s**.

**The main tank is rescued like anyone else.** He rides a vehicle and holds nothing while he is held,
so the threat wipe costs less than the grip; Forbearance locking out his own Divine Shield and Lay on
Hands for two minutes is the accepted price.

**The spell is "Hand of Protection", not "Blessing of Protection".** It was renamed in 3.0 and
`SpellIdValue::Calculate` matches the DBC name by first character, exact length and full string, so
the 2.x spelling resolves to spell id 0. `CastBlessingOfProtectionProtectAction` still asks for the
old one and has therefore never cast. It is left alone: reviving it changes paladin behaviour in every
instance, and it could not serve this anyway — `PartyMemberToProtect` only returns a non-tank under
30% health, and a squeezed bot is near full.
