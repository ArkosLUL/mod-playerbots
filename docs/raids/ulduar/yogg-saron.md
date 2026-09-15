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
`FindNearestCreature(NPC_SARA_PHASE_1, …)`; Yogg himself is no better.

**Her presence is not a pull; her combat is.** `LoadAllGrids` makes her findable from instance
creation and `Reset()` leaves her visible with the clouds already orbiting, so a phase read made of
"Sara exists" is true between pulls — and the Ulduar strategy runs in the non-combat engine too. The
raid therefore walked its phase 1 stations before the pull, crossing all six orbits at a run: 76-101
yd out at t=0, a median 20 yd by t=10 s, and **4 of the pull's 12 Guardians summoned before the first
point of damage**, one per orbit it ran through. `InitFight` calls `SetInCombatWithZone` and
`ACTION_YOGG_SARON_APPEAR` calls it again on Yogg, so one combat test covers all three phases.

`YoggSaronPhase` is the single read every node routes through — phase 2 is Yogg with Shadow Barrier,
phase 3 is Yogg without it plus the Brain, phase 1 is Sara, all gated on that combat test — and it is
what writes `yogg.phase`. She lives into P2/P3 at 1 health, so "Sara is alive" is no phase test by
itself.

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

Avoiding them starves nothing: `EVENT_SARA_P1_SUMMON` feeds Guardians every 20 s, shrinking to a 10 s
floor, wherever the raid stands. That timer is also the yardstick — it can fire at most **7** times in
a 99 s window, so the 20 Guardians of 2026-09-14 put **at least 13** on the raid's own feet.

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
  circles in `set.fallback`, so the retry drops the reach and keeps the clearance.

`MoveAwayFromCreatureAction` was rejected for this: no throttle, no latch, and it *maximises* distance
recomputed from the bot's new position every tick, so against six rotating rings the best answer rotates
with them.

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
(15 yd, the nova's own reach to Sara) the bot walks back to the middle, and it is not released until
`ULDUAR_YOGG_SARON_P1_LEASH_RELEASE` — the **2.8 yd cloud-free radius**, not a boundary a step inside
the leash. Anything between the two lets go of the bot somewhere the innermost orbit sweeps, and the
old 12 let go of it on the orbit. The node reads a live Guardian before the phase, which is four 200 yd
sweeps and would otherwise run every tick for melee standing outside the leash in P2 and P3; the price
is no walk back before the first spawn.

Nothing pins melee inside 2.89 yd, though — they get there because a Guardian walks to whoever holds
threat and `meleeDistance` is 0.75. Measured they sat at **2.5-2.9 yd** and triggered no inner-orbit
Guardian at all; all three of one pull's came from the human player at 6.7-13.4 yd. **That is the first
thing to check** now that they hold still instead of orbiting: settle beyond 2.89 and the innermost
cloud harvests one Guardian every 24 s, and the leash has to fire tighter than 15 yd after all.

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

**The brain team is melee first, then exactly one healer, then ranged; tanks never.** The room is a
60 s race on foot, and it doubles as the Crush answer above. Order within each band by GUID so every
bot derives the same team from its own seat. The trigger and the action used to build the list
differently — one skipped the master, the other did not — so they disagreed about who was on it;
`GetYoggSaronBrainTeam` is the single owner now.

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

**The Laughing Skull cannot be killed, only faced away from.** 33990, faction 14, `unit_flags`
`UNIT_FLAG_NOT_SELECTABLE` with `flags_extra 128`: neither targetable nor killable.
`creature_template_addon.auras = 64167` triggers **64168 every 1000 ms — 1750 shadow damage, −2
Sanity, 30 yd** — and its target filter is exactly `target->HasInArc(M_PI, caster)`, so only players
with the skull in their front 180° are hit. Four per room, despawned when the last Influence Tentacle
dies. It was entirely unhandled: **149 gaze hits for 137,815 damage** and 298 Sanity in one wipe.
`yogg-saron laughing skull` faces away from the centroid of the skulls in arc, at `ACTION_EMERGENCY`
beside the Yogg gaze node — the two cannot share a bot and never need to, since Yogg's own Lunatic
Gaze is a P3 self aura on the platform and the skulls only exist below it. Probed as `yogg.skull`.

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

**Fourteen `yogg.` probes and a reader.** `tools/botobs/yogg_saron.py` prints phases, cloud-orbit
exposure, portal waves and assignments, brain-room occupancy and Brain health, Crush and knockback
exposure per role, and Sanity minima — and names any key missing from the whole trace, because a key
declared in source and absent from every trace of its own boss means the recorder is dropping it, not
that the thing never happened. The keys are `yogg.phase`, `yogg.engaged`, `yogg.room`,
`yogg.roomstate`, `yogg.cloudreach`, `yogg.knockback`, `yogg.crush`, `yogg.deathray`, `yogg.wave`,
`yogg.portal`, `yogg.portalslot`, `yogg.brainteam`, `yogg.skull` and `yogg.exit`, beside the older
`yogg.walk`, `yogg.station`, `yogg.p1dodge`, `yogg.p1station` and `yogg.p1leash`. Two hazards go to
the timeline only because nothing can sweep for either: the body's knockback circle, and each
Crusher's wedge carrying facing, arc and range so it can be tested by hand afterwards.

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
