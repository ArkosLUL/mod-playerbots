# Yogg-Saron
## Phase 1: Sara is friendly, and only Guardians can hurt her

Sara is `FACTION_FRIENDLY` all of P1, and her `DamageTaken` zeroes anything whose attacker is not a
Guardian of Yogg-Saron (33136). The raid's job is to kill Guardians **on top of her** — their death
explosion Shadow Nova Sara (65719, 15 yd) is her only damage source.

So no bot ever holds threat on her, and `AI_VALUE2(Unit*, "find target", "sara")` walks
`GetThreatenedByMeList()`. It returned null every tick and took **all 21 Yogg nodes** with it: two
wipes on 2026-09-13, ~6,900 checks per trigger, zero fires. Resolve her with
`FindNearestCreature(NPC_SARA_PHASE_1, …)`. She lives into P2/P3 at 1 health, so "Sara is alive" is no
phase test — `YoggSaronInPhase1` also demands neither P2 nor P3. Yogg himself is no better; resolve
him the same way.

`IsBotMainTank` is false for **every** bot while a human holds main tank, which silently disabled the
cloud-cheat and phase-3-control nodes. `IsDesignatedBotTank` falls back to the first living bot tank.

## The two spells that wipe P1

| Spell | Shape | Answer |
|---|---|---|
| Shadow Nova 62714 / 65209 | instant, uninterruptible, 15 yd, on Guardian **death** | ranged and healers stay out; melee and tanks must eat it |
| Dark Volley 63038 / 65330 | 1500 ms cast, 35 yd, `InterruptFlags` 0xF | interrupt it — distance is no answer |

~97% of raid damage across both wipes. 25-man **normal** casts 63038, so the Dark Volley ids do not
split 10/25; test both. The always-on class interrupts only look at the bot's current target and
caught under a third of the volleys, hence `yogg-saron dark volley`, which offsets each interrupter by
its index so the raid does not spend every cooldown on one cast.

The nova's kill zone and the raid's camp are the same place: ranged and healers sat at a median 11 yd
from Sara while every burst caught 20-25 of 25 raiders. **Do not gate the retreat on Guardian health**
— the median Guardian is below 15% for 1.05 s, about 7 yd of travel. Spacing is a standing rule.

## Ominous Clouds

Six clouds orbit Sara at fixed **11 / 21 / 31 / 41 / 51 / 61 yd** radii, constant **3.0 yd/s**. One
summons a Guardian whenever a player comes within 6 yd, which is how bots spawned ~23 Guardians against
a scripted ~11-12. `InformCloud` skips clouds closer than 20 yd to Sara, so **the innermost orbit only
ever fires from player contact** - and it sweeps straight through where melee stand.

Avoiding them starves nothing: `EVENT_SARA_P1_SUMMON` feeds Guardians every 20 s, shrinking to a 10 s
floor, wherever the raid stands.

No fixed radius is safe. Orbit gaps are ~10 yd, so the best clearance any ring holds is 5 yd against
its two neighbours, inside the 6 yd summon check. Bots sidestep as a cloud comes round.

## The spacing nodes

Each phase puts everything it has to dodge into **one** node: two nodes at one relevance trade ticks
and walk the bot down the line between their destinations. Phase 1 takes clouds and Shadow Nova,
phase 2 takes Death Rays and Crush wedges, off a shared latch and sweep. Three things keep them from
oscillating against hazards that never stop moving.

- **Trigger and clear thresholds kept apart** (cloud 10 → 14, nova 17 → 20, ray 9 → 14, Crush arc
  ±8° → ±14°). One threshold parks the bot on the boundary and re-fires the moment a hazard drifts a
  yard in.
- **The destination is held** until the bot arrives or a hazard drifts onto it, and the in-flight tick is
  claimed by returning true without touching the motion master.
- **`preferNear` is `ULDUAR_YOGG_SARON_MIDDLE`, not the bot.** Every candidate in a ring is the same
  walk away, so a bot-position bias is a no-op tie-break; biasing at the middle makes the dodge a
  sidestep rather than a run for the rim. The `accept` cap holds it inside spell range.

`MoveAwayFromCreatureAction` was rejected for this: no throttle, no latch, and it *maximises* distance
recomputed from the bot's new position every tick, so against six rotating rings the best answer rotates
with them.

## Targeting is direct, never a raid icon

Yogg-Saron was the last Ulduar encounter targeting through raid icons. `yogg-saron set dps priority`
now owns every non-tank's target, paired with a multiplier zeroing `DpsAssistAction` — the idiom the
rest of the raid already uses. `AttackRtiTargetAction` is left alone so a **human's** mark still wins,
and `rti` survives only as a per-bot room tag.

An icon is a sticky override: `RtiTargetValue` hands it back before the smart picker runs and
`IsHighPriority` pins it, so a wrong mark cannot be corrected until the bot leaves combat. Reading one
back also fails whenever the marking bot holds a different `rti` string from the reader — exactly what
the illusion rooms do, one tag per room. Measured: `attack rti target` never fired once across a whole
pull, and the bots waiting on it dropped out of combat into `clean quest log` and `apply oil`.

`RtiTargetValue::Calculate` returns null on LOS failure and beyond `sightDistance` (100) in 2D. A
direct `Attack()` has **no distance cap, only LOS**, which is what makes the Brain reachable at all.

Kill order — brain level: Influence Tentacle → nearest other illusion add → the Brain. Boss room:
Crusher → Constrictor → Corruptor → Immortal Guardian → Yogg. Guardians are picked lowest-health
first and then held outright: that tier is ordered by health, and an order that flips mid-fight resets
every swing and cast timer in the raid. Below 10% a guardian is Weakened and only Thorim's Titanic
Storm can finish it, so it stops being a target at all.

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

So the dodge is by **angle**: reject any spot within 25 yd of a live Crusher and inside its arc. The
bot the tentacle is currently hitting is exempt — it is hit wherever it stands, and moving only drags
the cone around behind it.

**Known-open:** at 10 yd the ±8° trigger wedge is ±1.4 yd, so melee orbiting a tentacle may churn in
and out of it. The 3-second latch should absorb that; watch `--probes` for churn pairs.

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
would spend 50 s of that window debuffed. So the Crusher is the **top** target for every non-tank,
melee included, rather than something to keep away from.

## The brain room

**Induce Madness `64059` is a 60,000 ms cast.** On completion everyone at **z ≤ 300** loses all 100
Sanity and is teleported out; anyone above takes nothing. No Sanity means `63120 Insane`, and
`spell_yogg_saron_insane_aura::OnRemove` calls `Unit::Kill(owner, owner)` — **a mind control is always
a death.** A bot ending a window ~120 yd from the nearest exit portal needs ~17 s to walk it.

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

**`rti` is a room tag, so never write one as a targeting hint.** `"cross"` is Stormwind's tag: a
boss-room bot given it is teleported into Stormwind by `yogg-saron fall from floor` the moment it dips
below z 300, and is disqualified from `move to enter portal`, which requires `"skull"`.

## Reduced Keepers

The raid frees fewer than 4 Keepers, losing that Keeper's support. Tuned for the hardest single-Keeper
case, **Thorim only**. The bitmask lives in `PERSISTENT_DATA_WATCHERS_MASK` — preferred over Sara's
`GetData(DATA_GET_KEEPERS_COUNT)`, which gives a count only, because reading the mask confirms
**which** Keeper.

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

**Thorim's Titanic Storm auto-kills anything carrying `SPELL_WEAKENED` (64162)**, and a guardian
drops Empowered at ~<10% HP. So **melee only need to burn a guardian to Weakened; Thorim finishes
it.** With no Thorim, guardians are effectively unkillable without the cheat — which is why the P3
cheat instakill is suppressed **only when Thorim is a Keeper**, leaving other reduced-Keeper combos
winnable.

Open risks: the removed P3 cheat existed because Lunatic Gaze "freezes" bots, so guardian DPS may
stall between gazes; a guardian spawns up to 48 yd out, so confirm taunt pickup is prompt; and the
sanity-conservation behaviour (stand behind Yogg facing away below 15 stacks) **nearly benches a bot**
— sanity never recovers Thorim-only, so a bot that drops to 15 stays there.

## Squeeze breaks on immunity

Removing the Squeeze aura (64125 / 64126) kills the Constrictor Tentacle and drops the passenger, so
`yogg-saron squeeze escape` at `ACTION_RAID + 1` has a grabbed mage cast Ice Block and a paladin cast
Divine Shield. Hunter Feign Death and rogue Vanish are deliberately not used — neither removes a
periodic damage aura.
