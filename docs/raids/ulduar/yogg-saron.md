# Yogg-Saron
## Phase 1: Sara is friendly, and only Guardians can hurt her

Sara is `FACTION_FRIENDLY` all of P1, and her `DamageTaken` zeroes anything whose attacker is not a
Guardian of Yogg-Saron (33136). Their death explosion Shadow Nova Sara (65719, 15 yd, a flat **25,000**)
is her only damage source, so P1 is a counter: her health over 25,000 kills **on top of her**, and a kill
outside 15 yd buys nothing.

**Take her health from the trace, never `creature_template`**: its 199,999 gives eight kills, and she
measures **240,000 at 25 raiders** and **237,500 at 24**, so **ten**
([boss health is scaled](../../engine/pitfalls.md#configuration)). `yogg_saron.py --phases` reads her
`unit` row. A pull wiped at 9 of 10 with four kills spent 18-22 yd out.

Guardians have no movement script (`SetInCombatWithZone`, then a vanilla threat table), so **where melee
stand is where a Guardian dies**: that is why melee are leashed to her.

**Sara, Yogg and the Brain are read off the instance script** (`YoggSaronFindNearestCreature`): no bot
holds threat on them, so `"find target"` returns null
([pitfalls](../../engine/pitfalls.md#things-the-bot-cannot-see)) and once left all 21 Yogg nodes dead
through two wipes. The core registers one of each (`BOSS_YOGGSARON`, `DATA_SARA` 760,
`DATA_BRAIN_OF_YOGG_SARON` 761); the read answers as `FindNearestCreature(entry, 200, true)` and runs that
sweep past `|dx| + |dy| < 200`
([why](../../engine/raid-mechanics-lessons.md#what-a-strategy-costs-per-raid)). So the phase read is
cheap, and a phase test belongs **ahead** of any hazard sweep. The other 200 yd reads — Sanity Wells,
illusion doors, the tentacle check, skulls in arc — are cached once per trigger pass
(`EncounterTriggerPassId`) and read live in actions and multipliers, where an earlier action can have
changed them.

**Neither her presence nor her combat is the pull.** `LoadAllGrids` makes her findable from instance
creation and `Reset()` leaves her visible with the clouds orbiting, so "Sara exists" holds between pulls,
and the Ulduar strategy runs in the non-combat engine too. Her combat flag comes 19-24 s late
([a call site is not proof](../../engine/pitfalls.md#detection-that-never-fires)): only her own P1 casting
landing on somebody sets it, `EVENT_SARA_P1_DOORS_CLOSE` at 15 s plus a 4 s cast.

**The pull is `GetBossState(BOSS_YOGGSARON) == IN_PROGRESS`**, set on `InitFight`'s first line 5 s after
any player comes within 90 yd, and read by `BossStateGateLive`. It is trace t=0 (`pull src=bossstate`,
[observability](../../systems/observability.md#lifecycle)); no Guardian has ever beaten it, the first
appearing at 10.1-10.2 s.

`YoggSaronPhase` is the single read every node routes through, behind that boss-state test — phase 1 is
Sara, phase 2 Yogg with Shadow Barrier, phase 3 Yogg without it plus the Brain — and it writes
`yogg.phase`. Sara lives into P2/P3 at 1 health, so "Sara is alive" is no phase test by itself.

**Phase 1 positioning waits for the room, not the pull.** The raid is 72-96 yd out at t=0, and with every
mover firing at once ranged ran in through orbits 4 and 3 as clouds passed: two Guardians by 16 s. The
station, spacing, leash and control triggers wait for `YoggSaronInPhase1Room` (outer orbit + reach,
69.3 yd), a distance read ahead of the phase read. Combat is no gate (every Guardian's `Reset` calls
`SetInCombatWithZone`, so the instance is in combat from ~10 s), and it stays out of `YoggSaronPhase`,
whose read writes the shared `yogg.phase` latch. `--phases` counts P1 moves begun outside it with 2.5 yd
of slack: `GetDistance2d` takes off the bot's 1.5 yd reach and the move lands a tick after the read.

`IsBotMainTank` is false for **every** bot while a human holds main tank, which silently disabled the
phase 3 control node; `IsDesignatedBotTank` falls back to the first living bot tank.

## The two spells that wipe P1

| Spell | Shape | Answer |
|---|---|---|
| Shadow Nova 62714 / 65209 | instant, uninterruptible, DBC 15 yd but **16.2 measured**, on Guardian **death**, also hitting other Guardians (below) | ranged and healers stand off; melee mitigate it and hold their ground, only a Fervor holder leaves (below) |
| Dark Volley 63038 / 65330 | 1500 ms cast, 35 yd, `InterruptFlags` 0xF | interrupt it — distance is no answer |

They were ~97% of raid damage across two wipes; the nova leads with few Guardians up, Dark Volley once
14 are.

**Dark Volley:** 25-man **normal** casts 63038, so the ids do not split 10/25; test both
([difficulty ids](../../engine/pitfalls.md#detection-that-never-fires)). The class interrupts look only at
the bot's current target and caught under a third, hence `yogg-saron dark volley`, which offsets each
interrupter by its index so the raid does not spend every cooldown on one cast. Its trigger answers for
**this** bot — a caster in its own reach, not claimed by another — since a bot that claimed the tick at
`ACTION_EMERGENCY + 1` and then found nothing in reach wasted 63 of 92 attempts.

**The nova is a death explosion, not a cast** (`boss_yoggsaron_guardian_of_ys::JustDied` → `DoCastAOE`),
so nothing interrupts or outranges it once the Guardian dies in the stack, and **its kill zone is Sara**:
once every kill landed inside 15 yd, novas caught 12-23 raiders each and were 65% of damage taken. Melee
and tanks cannot leave; ranged and healers stand off (below).

### The nova's tail, and shadow resistance

**Plan against the tail; the median is survivable.** Pools run **23,387** (enhancement shaman) to
**30,549** (warrior). The ordinary hit holds a ~17,500 median, but the worst ran **25,025** with the
raid's shadow resistance up and **26,692** without, and the unresisted roll appeared only without it
(0 of 515 hits against 9 of 137).

**`…_NOVA_EXPECTED_DAMAGE` is 28,600**, the smallest figure consistent with every death: a ret paladin
died from a full 28,582 on a recorded 26,692, because 65209's two `SCHOOL_DAMAGE` effects outrun the one
row the trace records per raider. Only the two warriors and the tank of 24 raiders have that bar.

**Keep 130 shadow resistance up; losing it uncovers the unresisted roll.** Mean mitigation barely moves
(22.2% against 21.4%); the floor goes. With it up, 759 hits resisted a median 21-22% and never under 10%,
and only two novas in one tick killed from full; without it a single one did.

**Where the 130 comes from barely matters.** Shadow Resistance Aura 48943 and Prayer of Shadow
Protection 48170 both carry EffectBasePoints 129 on aura **143** `MOD_RESISTANCE_EXCLUSIVE` (48169 too),
which `Unit.cpp` reads with `GetMaxPositiveAuraModifierByMiscMask` — **the highest, never the sum**. The
priest buff covers the raid alone; the exclusive aura slot is worth spending only where something beats
130.

**Aura Mastery 31821 does, so the aura goes to the paladin that has it.** Aura **108**
`ADD_PCT_MODIFIER` on all three effects at 99 base points is **+100% for 6 s**: 260, the one state in the
fight above the Prayer's floor. Left to the class logic the two never met (the aura went to the
protection tank, which cast Devotion back; Aura Mastery went off a critical-heal node after the hit; a
ret picked Retribution Aura 54043).

- `IsYoggSaronShadowAuraPaladin` elects the lowest-GUID **bot** paladin that knows Aura Mastery and can
  cast a resistance aura (a bot cannot double a human's aura). `yogg-saron shadow aura action` puts it
  up, `yogg-saron shadow aura hold` blocks every other aura on that bot (the slot is exclusive, so its
  buff strategy trades it back next GCD), and `yogg-saron aura mastery hold` keeps the class node off the
  cooldown.
- The trigger tests `HasAura(…, bot, false, true)`, **its own copy**: a human's sits on every raider and
  otherwise reads as done.
- The room check is distance only, so the election and holds end on `roarExpected` (phase 3), where Aura
  Mastery goes with Concentration Aura.
- **The other paladins keep off the slot.** Copies from two casters coexist and only the strongest keeps
  its effects ([pitfalls](../../engine/pitfalls.md#detection-that-never-fires)), so three on 48943 is a
  ping-pong: 207 hand-offs in 110 s, and the elected press 15 ms before a nova doubled nothing because
  dominance had not flipped to it. `yogg-saron shadow aura other hold` keeps every non-elected paladin off
  48943, Aura Mastery and the two generic `shadow resistance action` nodes (their action adds a strategy
  that re-casts the aura), only while somebody is elected; nothing else of theirs is held, so their buff
  strategy puts Devotion or Concentration back. It stops a **cast**, not an aura already up from before
  the pull, so hand-offs only halved.

**An instant that empties a bar is mitigated or not survived**: healers out-healed the melee 1.7 to 1 and
two novas in one tick still killed eight from full. All 25 P1 deaths over four pulls were this spell, none
with a cooldown up.

### Sara's Fervor and Sara's Blessing

**Sara's Fervor (63138) makes the nova a one-shot.** The DBC gives +20% damage done and **+100% damage
taken** for 15 s; the core implements none of it, so the spellbook and every server grep are silent, and
`DispelType 0` means nothing lifts it. Measured 39,430 and 45,340 against a 23,537 pool.

So a holder leaves on the widest gate anyone gets, `…_FERVOR_NOVA_HEALTH_PCT` (**50%**):

- **Not tighter**: a 50% warning ran a median 9.2 s and came under the ~2.5 s a 15 yd walk takes once in
  98 novas, against 30 of 98 at 20%; focus fire leaves a Guardian at or under 20% for only 0.9-1.0 s.
- **Not wider**: Fervor's 15 s outlasts the 10.4 s between novas, so a holder meets one whatever the
  gate, and "every Guardian in reach" held on 100% of melee ticks, sending holders out early, dodging
  nothing and waking Guardians 20-30 yd out.
- **The hold travels with the gate, and that is the live hazard.** `yogg-saron phase 1 walk guard
  multiplier` pins a holder outside only while the gate holds, and 11 of 40 landings had a Guardian
  inside 22 yd with none under 50%: a warlock walked to 3.5 yd of the middle took 41,832. Watch where an
  unheld holder ends up.

**Sara's Blessing (63134) is a heal with a 20 s tail, and nothing reads it.** Effect 1 heals 27,000,
effect 2 ticks 6,000 shadow every 2 s for 20 s (~40,000 resisted, `DispelType 0`). Net about −13,000 on a
30,000 pool: it kills nobody alone but leaves the holder unable to take the next nova. Shadow school, so
48943 cuts it.

**The ranged "chasing me" rule needs a stack test.** The station clears a blast on the stack by about a
yard, so the back line ignores a low Guardian **the tank still holds there**
(`YoggSaronGuardianOnTheStack`) and dodges one that has left it, whoever either is hitting. A low
Guardian on the stack stays 16.2-17.0 yd off the back line; at the handover one came to 2.2 yd.

### The nova cooldown ladder

A flat cut of 20% saves 14 of the 25 deaths, 30% 19, 40% 23, 50% all. Cuts **multiply**
(`Unit::GetTotalAuraMultiplier` is `AddPct` per aura, read from `SpellDamageBonusTaken`) and none of these
spells shares a `spell_group`, so Pain Suppression over Shamanistic Rage is `0.6 × 0.7`, a 58% cut.

**The estimate is one figure, read and never copied.** `YoggSaronNovaDamageEach` is 28,600 unresisted,
or `…_NOVA_RESISTED_DAMAGE` (**25,740**) for a bot at `…_NOVA_RESISTED_AT` (130,
`GetResistance(SPELL_SCHOOL_SHADOW)`: Prayer, aura, Aura Mastery and gear at once), read by the estimate,
the raid count and the external gate. A resisted nova never landed past 23,702 (p99) over 892 hits, and
planned at 28,600 a full-health melee bot read as at risk and emptied the externals. A raid count spelt
as its own `MAX_CHAIN × EXPECTED_DAMAGE` copy went unreachable once the resisted figure lowered the
ceiling (51,480 against 57,200), and the raid held Divine Sacrifice all fight.

- `YoggSaronNovaDamageFrom`: that per low Guardian in reach, at most `…_NOVA_MAX_CHAIN` (2), doubled
  under Fervor. `YoggSaronNovaLeavesNothing` is `health <= expected`.
- **Plan an unresisted bot against the unresisted hit or the gate never opens**: at the resisted median
  the threshold was 21,750, under every pool, so only an already damaged bot qualified and buttons fired
  after the nova.

**Only a chained nova is worth a cooldown** (`YoggSaronNovaChained`, shared by the raid cover and the
external gate): two low Guardians in reach (one on a Fervor holder), a Sara's Blessing holder, or a
Guardian death in the last **6 s** (`…_P1_NOVA_GAP_MS`), and `YoggSaronNovaLeavesNothing` still has to hold.

- One nova reads lethal on anything under ~86% of these bars: 5 of a pull's 7 externals went before
  anybody died, and three paladins spent Divine Sacrifice in the first 73 s on novas nobody fell below
  8,020 to, so it was 8.2 s short of the double nova that killed five.
- The death clause is because **a pair goes off one after the other**: by the time the second Guardian
  is low the first is dead and the count is one. Not 10 s: Guardians die every 7-10 s in phase 1, so it
  would open on 8 of 9 novas.
- One button may not save a bot that low: under 23,316, Hand of Sacrifice or Pain Suppression alone
  leaves too little; Aura Mastery plus either, or Guardian Spirit, does.

**Two health gates, and the tighter one decides whether to spend.** A Guardian first under **20%**
detonated a median 4.8 s later, as late as 11.5; under **10%** a median 2.4 s, never later than 3.7. So
`…_NOVA_NOW_HEALTH_PCT` (10) is where a bot spends its own button and a healer its external,
`…_NOVA_NOW_WINDOW_MS` (5 s) always covering the hit; `…_NOVA_SOON_HEALTH_PCT` (20) and its 12 s window
are Divine Sacrifice's alone, since one cast covers 25 raiders for 10 s. Everything here is instant, so
0.3 s is castable. At the wider gate the raid emptied eight cooldowns on one mild nova and had nothing
for the next eight.

Three tiers, in relevance order, since one cast covering 25 raiders beats any single bot's own:

- **`yogg-saron nova raid cooldown`** (`ACTION_EMERGENCY + 7`), once a chained nova threatens
  `…_NOVA_RAID_COUNT` (3) of the stack (`YoggSaronNovaHurtCount`). **Aura Mastery first**
  (`YoggSaronNovaRaidCover`) whenever the elected paladin has its own aura up and the cooldown ready: it
  costs the caster nothing, where Divine Sacrifice redirects 30% of the raid's damage onto it. Both run
  120 s against a nova every 10; `yogg.novaraid` records which went. Aura Mastery is cast directly, as the
  roar press is, so the cover needs no class `aura mastery` action (upstream has none); Divine Sacrifice
  goes through its class node so `cancel divine sacrifice` stays its continuer. **Claimed per Guardian**
  (`ClaimYoggSaronNovaWindow`; `…_NOVA_WINDOW_MS`, 10 s, is that Guardian's window), the lowest in reach,
  which every paladin in the pile agrees on since all sweep the same 17 yd. A 10 s stretch of wall clock
  held by whoever took it left the second Guardian of a 3.2 s pair uncovered with two Divine Sacrifices
  ready; Guardians die 1.8-4.5 s apart.
- **`yogg-saron nova defensive`** (`+ 6`): the bot's own, from `NextMagicDefensive`, by **spell id**,
  shortest cooldown first and never two at once — Anti-Magic Shell (45 s), Barkskin and Shamanistic Rage
  (60 s), Cloak of Shadows (90 s), Icebound Fortitude (120 s), Divine Protection and Survival Instincts
  (180 s). No claim: a personal button covers one bot. By id because Anti-Magic Shell by name never cast
  ([pitfalls](../../engine/pitfalls.md#names-fail-silently-at-runtime)): `NextTankDefensive` /
  `NextMagicDefensive` return the id and `PickDefensive` tests it, safe because all eleven rows are
  single-rank, while `castName` stays the action name `IsHeldTankDefensive` and the class contexts match
  on.
- **`yogg-saron nova external`** (`+ 5`): a healer's, in worth order `pain suppression`,
  `guardian spirit`, `hand of sacrifice`, chained novas only, **claimed per Guardian**
  (`ClaimYoggSaronNovaExternal`): claimed per victim, `YoggSaronNovaVictim`'s pick flipped between ticks
  and four Hands of Sacrifice went in 15 s. The victim is whoever the nova leaves furthest past dead
  **after their own button** (`BestMagicDefensiveCut`), so a rogue who dodges it outright comes behind a
  warrior with nothing, and a Fervor holder is always first. Hand of Protection is out: physical only,
  and it sheds threat.

Each claim is taken **after** the caster has something castable, or one that claimed on sight would beat
the rest to the victim and then cast nothing.

**A human holding a Guardian is called** ([calls to human tanks](../README.md#calls-to-human-tanks)) when
the nova would kill him: Anti-Magic Shell at the 10% gate (5 s, so it can't go earlier), any other button
at 20%, which a 12 s button still covers and which leaves him time to react.

**`YoggSaronNovaHold` keeps the class triggers off all of it** — `yogg-saron nova defensive hold` over the
melee row, `yogg-saron nova cooldown hold` over the externals and Divine Sacrifice — for all of phase 1,
where every death measured was a nova, and after it while a Guardian lives inside `…_P1_ROOM_RADIUS`, so
phase 2 spends normally on Crush and the Death Rays. A 17 yd test let go with two still alive, and the
class nodes spent Aura Mastery and a Divine Sacrifice on an already hurt raid. Otherwise every one fires on
"I am already low", after the nova. Tanks and the back line keep theirs, for the Guardians one holds and
the Dark Volley the other eats.

**Two gaps left, both deliberate.** A **dps warrior has no row**: Last Stand is a protection talent and
Shield Wall needs a shield (871: `EquippedItemClass` 4, subclass mask 64), so `CanCastSpell` rejects both.
No warrior button closes it — Enraged Regeneration (55694) heals 3% a second for 10 s against a nova that
lands in 50 ms. And nothing walks them out of phase 1 either (below).

### Melee mitigate in phase 1, and leave after it

**No pool gets a melee bot out of the blast in phase 1.** Surviving 28,600 needs 18% off for the
smallest pool, so any 20% button saves every melee, but supply does not: 13-16 cooldowns per phase 1
against 11 novas hitting 7-10 raiders each. Stepping the smallest pools out was measured and rejected:

- **It did not work**: 44 of 74 melee step-outs took the nova anyway, the lead a median 0.6-3.5 s against
  the 2.5 s walk.
- **It cost Guardians**: the destination, the station band, is 0.1 yd off orbit 2 (21.5 against 21.4),
  so melee feet woke 25 of 85 Guardians, and with Sara needing 10 every extra Guardian is another nova.
- **Standing in it is survivable**: melee under 26,000 lost a median 62% of the bar, those above 54%,
  0 of 301 reaching 90%. Every P1 melee death was a case no walk reaches — a Fervor hit, or two novas
  2.7 s apart — so the chain is the hazard left, a mitigation problem, since the second Guardian is not
  low when the first detonates.

Without the step-out the next pull had the fastest phase 1 of seven, 110.4 s.

**After phase 1 every melee leaves whatever its pool, and only a tank stays in.** Leftovers took 101
melee hits, 68 of them bare, so leaving is free, and what kills there is two novas 8-9 s apart from
60-67%, not one from full, so `…_NOVA_STEP_OUT_POOL` and the own-button test are gone. The tank is the exception in every phase: it holds
the thing detonating, walking out only drags it along, and its pool can eat the blast.

**A battle rez waits for the handover.** It strips every raid buff and nothing re-buffs in combat, so
rezzed bots came back on bars at most 83-87% of the buffed pool and died from full to a single nova.
`yogg-saron battle rez hold` holds `rebirth` until the handover, the first stretch with no nova.
Reincarnation runs from the **dead engine** (`dead`, `stay`, `chat`, `default`, `follow`), which no
encounter rule reaches: one shaman with a 23,757 pool, under one nova's 25,740, made 4 of 9 deaths over
ten pulls. Left alone by the user's choice.

`trace.maxhp` keeps the pool from before the first death, so a resurrected bot's percentages are wrong and
the killing blow is the only witness to its real bar.

**The station band is the Fervor holder's destination**, which is who `HazardSet::preferred` is for. It
is the only-orbit-2 band, so it costs no summon of its own (orbit 2 runs through the station and wakes
anyway; runners woke 1 Guardian in 85); without it a run ends ~19 yd out, where orbits 1 and 2 both reach.
The ranged blob (`…_P1_RANGED_STACK_RADIUS` of `…_P1_RANGED_SPOT`) stays out of the test: asking for both
left `FindNearestPositionClearOfHazards` 0-2 acceptable points of its ~200 (rings 2 yd apart, 16 bearings,
against a 2.3 × 10 yd lens 19 yd away), where the band alone offers 11-16. Widening the tolerance is no
fix: ±1.0 *is* the band, 1.13 yd either side of 21.5. **Not the tank**, which holds the pile, nor a melee
healer — `IsMelee` is `!IsRanged`.

### Facts the spells do not show

- **Divine Guardian's raid-wide 20% works, and the split is survivable.** 70940 appears nowhere in the
  core's `src` or `data/sql`; talent 53530 effect 1 is aura **231** (`PROC_TRIGGER_SPELL_WITH_VALUE`) at
  −20, triggering it as an `APPLY_AREA_AURA_RAID` aura 87, and it reached all 25 raiders every cast.
  Divine Sacrifice's split pool is `40% × caster max health × raid member count`
  (`spell_pal_divine_sacrifice::Load`) and breaks under 20% health, so no caster went below 62%. That
  damage skips `SpellDamageBonusTaken`, so the paladin's own cuts do not reduce what the split hands back.
- **Cloak of Shadows nearly removes a nova for a rogue.** AoE rolls to hit at `Spell::AddUnitTarget`, the
  magic path reads aura 186 in `WorldObject::MagicSpellHitResult`, and a miss runs no effect. 31224 is
  −90 on school mask 126 with no script, clamping hit chance to the 1% floor, and 65209 has
  `AttributesEx2` through `Ex7` all zero, so neither `ALWAYS_HIT` nor `NO_ATTACK_MISS` is set.

**The Guardians outlive phase 1 and keep the explosion**, so none of this gates on the phase. Nothing
despawns them: `DespawnEntry` is never called with `NPC_GUARDIAN_OF_YS`, `boss_yoggsaron_guardian_of_ys`
has no `DoAction` so `ACTION_DESPAWN_ADDS` misses it, and its summon type is
`TEMPSUMMON_CORPSE_DESPAWN`; only spawning stops (`EVENT_SARA_P1_SUMMON` is under `EVENT_PHASE_ONE`). They
keep `SetInCombatWithZone`, Dark Volley and the nova, and the kill order ranks them first, so the cooldown
nodes and the P2 spacing circles follow the Guardians. `AppendYoggSaronTargetExclusions` reaches only
`TargetValue` pickers, not the phase 2 resolver. Left phase-1-gated, one pull focused two down beside the
back line and lost eleven raiders in three seconds.

## Ominous Clouds

Six clouds, one per orbit, circle Sara at a constant **3.0 yd/s** (`SpawnClouds`, `8 + i*7` on the
diagonal). The radii never drift: **11.39-11.86 / 21.25-21.52 / 31.13-31.31 / 40.93-41.07 / 50.81-50.92
/ 60.74-60.84 yd**, laps **24 / 45 / 65 / 86 / 106 / 127 s**. `InformCloud` skips clouds closer than
20 yd to Sara, so **the innermost orbit only ever fires from player contact**.

**A cloud summons on any player within exactly 8.5 yd, not the script's 6.**
`boss_yoggsaron_cloud::UpdateAI` calls `SelectNearbyTarget`, which adds both `GetObjectSize()` values
([reach](../../engine/raid-mechanics-lessons.md#range-and-reach)): the cloud's **1.0** and every
player's flat **1.5** (`Player::SetObjectScale` hands out `DEFAULT_COMBAT_REACH`). Inner-orbit summons
measure 8.53-8.71. Pets cannot trigger it (`who->IsPlayer()`), and it does not wait for the pull.

**The summon is a 10 s aura and the cloud re-arms.** 63031 is `EffectAura` 23 with period and duration
10000, triggering 62979, so a Guardian appears exactly 10 s after its cloud is marked, and `JustSummoned`
clears `_isSummoning`: a bot that stays in reach summons **one Guardian every 10 s indefinitely**. The C++
reads as an instant one-shot; the delay and the repeat are DBC-only. Avoiding them starves nothing:
`EVENT_SARA_P1_SUMMON` feeds Guardians every 20 s, shrinking 2 s a summon to a 10 s floor, wherever the
raid stands.

**Attribution is a measurement**: every Guardian spawns on its cloud, so match the spawn to a cloud and
rewind 10 s for who was within 8.5 yd (`yogg_saron.py --clouds`). One pull's 23 by orbit: **1: 8 · 2: 4
· 3: 1 · 4: 1 · 5: 2 · 6: 7**, 11 of them the raid's own feet. Orbit 2's four are the station's designed
cost, 4.2 laps, one each. **Not a defect; do not re-audit.**

**Nobody dodges a cloud: no radius past the core is clear of every orbit.** Against the 8.5 yd reach the
orbits leave two places:

- inside **2.89 yd** of Sara (11.39 − 8.5) nothing reaches, which is where melee stand
- between **20.36 and 22.63 yd** (11.86 + 8.5 up to 31.13 − 8.5) only the second orbit reaches

Orbits sit ~9.8 yd apart against the 17 yd a cloud sweeps (`…_CLOUD_SUMMON_REACH` 8.5 either side), so every other radius is inside some orbit's
reach, moving a station cannot help, and a spot clear now is under a cloud seconds later. **Dodging
anyway cost a phase**: with the trigger widened to 18 yd, nearly every phase 1 move cited a cloud, ranged
walked for no net change in distance and smeared over a 109° arc, melee orbiting Sara lost 43% of their
damage and the tank 91%, and raid dps fell from 97.5k to 74.0k.

**So the cloud circle is a filter, not a dodge.** `ULDUAR_YOGG_SARON_CLOUD_AVOID_RADIUS` (9.5, the reach
plus a yard) does three jobs: the trigger reads it as "I am standing in one", never to be dropped because
the aura re-arms; the hazard set keeps a bot moving for another reason from landing in one; and phase 1
alone overrides `RouteAcceptable` to reject a straight walk passing inside it — crossing a Death Ray to
leave one beats standing in it, crossing a cloud costs a Guardian. Each cloud enters the set twice, where
it is and `ULDUAR_YOGG_SARON_CLOUD_LEAD_MS` ahead along its orbit; without the lead, destinations were no
further from clouds than standing still.

**Nothing may jump in this room.** Blink, Disengage and the gap-closers land a bot somewhere nobody
picked, here a ring: 13 of 20 phase 1 casts in one pull left the bot with more orbits in reach than
before. `yogg-saron displacement guard multiplier` zeroes:

- **Blink and Disengage all encounter**: they fire on "something is too close", aimed at nothing. Both
  are plain `CastSpellAction`s and are named.
- **The whole `CastReachTargetSpellAction` family plus `killing spree` in phase 1**, since the Fury chain
  is charge → intercept → `reach melee` and a partial veto only moves the problem down the list; `reach
  melee` survives, which walks melee in on foot. The gap-closers return in P2 and P3, where there is no
  orbit to land on —
- — **except onto the Brain**, whenever it is the current target: it hangs 25 yd over its room's floor
  with nothing underneath, and its 30 yd combat reach hides the gap from `GetDistance`. A feral druid
  charged it and stood in the air 23 s until charmed; Killing Spree put a rogue on its model at z 265.

**Only the cat's leap strands.** Feral Charge - Cat 49376 is `SPELL_EFFECT_JUMP_DEST` behind the target,
air under the Brain. Warrior Charge 11578 and Intercept 20252 are `SPELL_EFFECT_CHARGE`, and
`Spell::CheckCast` paths a target whose reach exceeds melee range to its ring facing the caster at the
caster's ground Z: 31.5 yd from the Brain, floor on all 24 navprobe headings (settled z 239.0-241.2). So
`yogg-saron brain charge` (`ACTION_RAID + 0.25`, over the brain spot walk, which outranks every class
charge node) casts Charge or Intercept itself on the way in: Brain targeted, rti `square`, not leaving or
held, the ring at least `…_BRAIN_CHARGE_MIN_GAIN` (8 yd) nearer the brain spot than the bot. It clears
`last movement` so the walk re-issues on landing; ~1.5-2 s per warrior per wave. The block stays, so no
charge fires on the way out and cats never leap; `yogg-saron brain dash` gives cats Dash (+70%, 15 s,
3 min) on the `tobrain` walk instead, never in the Chamber unless they lack Nitro boots (both aura 31,
only the higher counts).

## The spacing nodes

Each phase puts everything it dodges into **one** node, since two at one relevance trade ticks and walk
the bot down the line between their destinations: phase 1 takes clouds and Shadow Nova, phase 2 Death
Rays and Crush wedges, off a shared latch and sweep. The shared latch shapes (held destination, in-flight
tick claimed) are in
[raid-mechanics-lessons](../../engine/raid-mechanics-lessons.md#movement-that-oscillates). Yogg's own:

- **Trigger and clear thresholds apart**: nova 17 → 20, ray 9 → 14, Crush arc ±8° → ±14°. The *clear*
  radius decides when a bot moves (the early-out is "outside every circle"); the trigger only whether
  the node is asked. The nova keeps its tight trigger: its 17-20 band is margin, 18 yd being past the
  blast.
- **`preferNear` is `ULDUAR_YOGG_SARON_MIDDLE`, not the bot.** Every candidate in a ring is the same walk
  away, so a bot bias is a no-op tie-break; a middle bias makes the dodge a sidestep, not a run for the
  rim. The `accept` cap holds it inside spell range.
- **A melee move keeps the bot in its own swing range**, or `reach melee` hauls it straight back and the
  two trade the tick (252 times in one pull). With nothing about to detonate, phase 1 puts "within
  `MeleeDistance` of my target" in `set.clear` and the cloud circles in `set.fallback`, so the retry drops
  the reach and keeps the clearance. **Only toward a target on the stack**: toward one further out the
  dodge *is* the chase, onto the innermost orbit.
- **Both phases ask by preference.** `set.preferred` is tried ahead of `accept` and dropped when nothing
  satisfies both, so a bot in a hazard still moves. Phase 1 asks melee for the station band once a nova
  is pending and the cloud-free circle when none is (below); phase 2 for a spot inside `MeleeDistance` /
  `SpellDistance` of the target, leaving `reach melee` and `reach spell` nothing to undo (without it
  reach and spacing handed the bot back and forth 1565 times in one phase 2).
- **A Death Ray dodge cuts into a walk in flight**, since a forced dodge is refused behind any forced walk
  ([the movement lock](../../engine/pitfalls.md#movement-that-silently-no-ops)) and a body detour once
  walked two ranged into a ray that way. Inside a ray's clear radius `OverridesWalkInFlight` clears `last
  movement` before moving. Rays only: a Crush wedge or the ring cutting in too would trade every tick with
  the detour.
- **A Death Ray dodge never walks closer to a ray.** A ray turns every 1.6 s at ~5.5 yd/s, so a spot clear
  of where it is can sit across its lane. `RouteAcceptable` (and the held-spot latch, each tick) rejects a
  walk whose closest approach to any ray is under min(current distance, 14 yd)
  (`YoggSaronRouteClearOfDeathRays`). `yogg-saron death ray avoid aoe hold` blocks the generic `avoid
  aoe` within 14 yd of a live ray on the platform
  ([its own defects](../../engine/pitfalls.md#things-the-bot-cannot-see)): at 90 over the dodge's 63 it
  took the tick, its combat-priority hop was refused behind any forced walk, and its spots sat beside the
  ray. Other walks (body detour, portal walk, Brain Link) still ignore rays; `--crush` names the last walk
  before each hit.

### Movement guards

Iron Assembly, Mimiron and Thorim zero `ReachTargetAction` while a positioning node owns the bot
([../README.md](../README.md)). `YoggSaronMovementGuardMultiplier` closes only the windows where the
generic reach or `set behind` is simply wrong, since reach is the only way a bot closes on anything
([pitfalls](../../engine/pitfalls.md#movement-that-silently-no-ops)), and `reach party member to heal` is
exempt:

- **The walk out of the knockback ring**, spanning the phase boundary: phase 1 and the 6 s hold that
  outlives it. Two nodes at `MOVEMENT_FORCED` and `MOVEMENT_COMBAT` traded the bot every ~300 ms.
- **Inside the ring in phases 2 and 3**, at exactly `ULDUAR_YOGG_SARON_BODY_KNOCKBACK_RADIUS`, the radius
  the spacing trigger fires at, so reach stands down only while that node owns the bot. A wider band
  leaves a ring where neither moves anybody.
- **`set behind` into the ring**, phases 2 and 3: both spots `SetBehindTargetAction::Execute` picks from
  (±108° off the target's facing, at the bot's distance) are computed, and the move is zeroed if either
  lands inside the clear radius or its line crosses the ring. Guardians tanked 18.5 yd out are yards
  across, so it aimed bots inside; Guardians have no frontal, so it costs parries only.
- **A melee bot's reach onto a live Crusher**, unless its stun holds: what the resolver does not hand out
  (a raid mark, a fallback, a tank's stock picker).
- Nothing under z 300, which is load-bearing: the brain room middle is **2.3 yd** from the platform middle
  in 2D, so the ring test would veto the reach of every bot on the Brain, 93 yd below.

**A veto is only as good as the node that moves the bot in its place.** Zeroing reach while a Laughing
Skull was in arc left five bots in the Chamber on a tentacle for fifty seconds, every melee ability
`USELESS` for range, while the facing node meant to walk them returned false. Make the moving node unable
to leave the bot where reach would undo it, and skip the veto, as the illusion facing node does.
`MoveAwayFromCreatureAction` was rejected: no throttle, no latch, and it *maximises* distance recomputed
every tick, so against six rotating rings the answer rotates with them.

**Two generic movers own feet in phase 1, so both are guarded**: the station node beats them only while
active ([a hold that lets go on arrival](../../engine/raid-mechanics-lessons.md#movement-that-oscillates)).

- **`reach melee` walks melee *out***, to targets outside the leash — many on the 21.5 yd station — via
  the innermost orbit. Zeroed in phase 1 while the target is outside `ULDUAR_YOGG_SARON_P1_LEASH` **and a
  bot tank is alive to fetch it**, or a dead tank strands every melee out of combat. **`reach spell` is in
  the same rule** for anyone who fights from the pile, since a taunt and a judgement are 30 yd casts that
  otherwise traded the tick with the control walk. The back line keeps its own: it is all that holds it
  in range from 21.5 yd.
- **`flee` walks ranged and healers off the station**: `RangedCombatStrategy`'s `"enemy too close for
  spell"` at `ACTION_MOVE + 4`, true whenever a Guardian stands on a caster, not a panic route. Its 3 yd
  escapes nothing (Dark Volley is 35 yd, the nova a death explosion), and 96.4% of 957 moves in one phase
  1 ended further from the band. Zeroed for ranged and healers in phase 1; melee and the tank keep it.

**Who dodges what in phase 1.** Nobody dodges clouds; the only cloud move is stepping off one a bot
stands in. A nova is dodged only by a bot that would not survive it: ranged and healers when a Guardian
at or under `ULDUAR_YOGG_SARON_GUARDIAN_NOVA_HEALTH_PCT` is **chasing them** (at spell range nothing else
reaches them), and a Fervor holder on the wider gate. Running from every Guardian scattered the raid to
the rim, where seven bots were picked off in four seconds. `GetYoggSaronNovaThreats` is the single owner
of that rule, so trigger and action agree on who runs. `YoggSaronNovaNearby` is the opposite read: any
Guardian in nova reach, who is about to be hit regardless.

**A Fervor run has to end out of reach, not at the edge.** The dodge fires inside 17 yd and stops at 20,
and the station, the leash or `reach melee` walked the bot straight back inside 16.2 yd still holding
Fervor in 31 of 44 runs. `yogg-saron phase 1 walk guard multiplier` zeroes the station, the leash (not
its handover walk out), `reach melee`, `reach spell` and `set behind` for a holder while a Guardian at or
under 50% is within `ULDUAR_YOGG_SARON_FERVOR_HOLD_RADIUS` (22). Casting is untouched, and the spacing
node still steps it off a cloud. It narrowed the walk back rather than closing it (28 of 39). Past phase 1
the same radius holds **any** bot, and the list adds `yogg-saron body detour action` and `combat
formation move` (below).

**Walks back wait for a cloud to pass.** The same multiplier holds the station walk, and the leash and
`reach melee` from outside 6.5 yd, while `YoggSaronWalkCrossesCloud` refuses a line a cloud will come
within 9.5 yd of. It walks the leg in `…_WALK_CLOUD_STEPS` (8) steps and pairs each with where the clouds
will be by then: every cloud position against the whole leg would refuse nearly every route in, since an
orbiting cloud crosses a radial path sooner or later, and a lead capped at one 6 s window checked the 17 s
run in from the door against clouds that had moved on. Nothing inside the leash is held, since the inner
orbit reaches most of it, nor a bot already in a cloud. The station walk ignores clouds within 14.5 yd of
the spot: orbit 2 over the stack is its designed cost.

### Stations

**Ranged and healers hold one spot on the second orbit.** Guardians die on Sara, so her spot is a
standing nova hazard for anyone who need not be in it; a 20 yd standoff alone took ranged and healer
exposure to 0%. The station is `ULDUAR_YOGG_SARON_P1_RANGED_SPOT` **(1958.78, −25.587, 324.889)**, due
west where the raid stages, navprobe-clean on the WMO floor, as is the whole 21.5 yd ring (24/24
headings). 21.5 is the midpoint of the single-orbit band, hence orbit 2 itself: standing *on* a ring buys
the most room from its neighbours, 1.13 yd either way, all `ULDUAR_YOGG_SARON_P1_RANGED_BAND_TOLERANCE`
(1.0) can give. Orbit 3 would cost less (a Guardian per 65 s against 45) but sits 31.2 yd out against a
28.5 yd `SpellDistance`, so a bot there walks itself back in.

They **stack** on it, inverting the usual raid rule, which holds only because the nova cannot reach the
station. A cloud is in contact for `(8.5 + blob + 8.5) / 3` seconds and re-arms 10 s after each summon,
so any blob under 13 yd across costs exactly one Guardian per pass:
`ULDUAR_YOGG_SARON_P1_RANGED_STACK_RADIUS` (5) gives 9 s of contact and **one Guardian per 45 s orbit for
the whole back line**. The parked latch widens only the stack radius, never the band: there is no margin
there to spend.

**Melee and tanks are leashed to Sara, not stationed on her.** Beyond `ULDUAR_YOGG_SARON_P1_LEASH`
(**6.5 yd**) the bot walks back to the middle and is released only at
`ULDUAR_YOGG_SARON_P1_LEASH_RELEASE`, the **2.8 yd cloud-free radius**: anything between lets go of it
where the innermost orbit sweeps. The node also needs a live Guardian in sight, so nothing walks back
before the first spawn. **Why 6.5, not 15**: 15 is both 65719 reaching Sara and 65209/62714 reaching the
raid, and the back line stands at 21.5, so a Guardian dying more than **21.5 − 15 = 6.5 yd** out catches
everyone rather than the pile (novas at 2.2-4.0 yd hit 9-10 players, at 8.5-14.8 yd 22-24). The cost is a
narrower band against `reach melee`; `yogg.p1leash` counts the flips.

**A melee bot that is neither tanking nor healing gets a tighter leash, because the pile wakes the inner
orbit by standing still.** Orbit 1 is 11.6 yd out and wakes inside 8.5, so a foot past
`…_P1_PILE_LEASH` (**3.1 yd**, the difference) summons a Guardian: 10 of 10 melee-foot wakes in one pull
had the melee past it. `…_P1_PILE_RELEASE` (2.0) is its hysteresis. The wide 6.5/2.8 pair stays the
tank's, since that one is about where the *Guardian* dies, which follows threat.

**The cloud dodge aims back at the cloud-free circle.** `set.preferred` asks `…_P1_CLOUD_FREE_RADIUS` of
the middle for melee that are not healers (`pileDodger`, tank included; `IsMelee` is `!IsRanged`, so a
melee healer would be pulled into the pile), preferred so it never holds a bot in a cloud. Without it
fewer than half of melee cloud dodges ended in the circle while one was clear nine times in ten.
`FindNearestPositionClearOfHazards` offers only points on rings around the bot, 16 bearings apart —
7.8 yd from each other at 20 yd out against a 5.6 yd circle — so `YoggSaronSpacingAction::Execute` first
tries the middle on the bot's own bearing, short of the edge where Sara stands; a finer grid would clear
the passability cache between the preferred and plain sweeps and quadruple the map queries per dodge.
The tank is in the set because its control walk gives its dodge no destination: left out, it dodged to
18-29 yd and woke Guardians.

## Targeting is direct, never a raid icon

`yogg-saron set dps priority` owns every non-tank's target for the **whole** encounter, paired with an
`OwnTargeting` rule zeroing `DpsAssistAction`; both read `YoggSaronEncounterActive`, the same call rather
than merely the same phases ([why](../../engine/action-selection.md#multiplier-semantics)).
`AttackRtiTargetAction` is left alone so a **human's** mark still wins, and `rti` survives only as a
per-bot room tag.

An icon is a sticky override ([pitfalls](../../engine/pitfalls.md#a-targeting-mark-never-clears-inside-a-pull)),
and reading one back also fails whenever the marking bot holds a different `rti` string from the reader —
exactly the illusion rooms' one tag per room: `attack rti target` never fired once in a pull.
`RtiTargetValue::Calculate` returns null on LOS failure and beyond `SightDistance` (100) in 2D; a direct
`Attack()` has **no distance cap, only LOS**, which is what makes the Brain reachable at all.

Kill order:

- **Phase 1**: one Guardian for the whole raid, no two deaths inside 6 s, never beside a lower one
  (threat section below). Two Guardians split and ridden down in lockstep crossed zero inside a second,
  and the double nova killed all eight melee in 16 ms.
- **Brain level**: Influence Tentacle → nearest other illusion add → the Brain.
- **Boss room**: **leftover Guardian of Yogg-Saron** → **Marked** Immortal Guardian (36064, only with
  Thorim; without him never a target) → Immortal Guardian (33988) → a Constrictor holding somebody →
  Crusher (**ranged only**, below; phase 3 ranged take a leftover one first) → Constrictor → Corruptor →
  Yogg. Phase 3 melee skip Constrictor and Corruptor and take a Guardian only within 20 yd of the phase 3
  melee spot (`ULDUAR_YOGG_SARON_PHASE_3_MELEE_GUARDIAN_RANGE`).

**The leftovers lead the boss room.** They survive the transition, keep a 35 yd Dark Volley, and
regenerate to full the moment the raid takes a portal; untiered, the nearest-first `dps target` fallback
spread two Guardians' worth of damage over five of them for zero kills.

**Phase 3 splits the raid: ranged kill Guardians, melee stay on Yogg.** Ranked above the Immortal
Guardians, far tentacles took one pull's first 50 s of phase 3 while five Guardians spawned untouched and
killed the raid. A 25-man Guardian has 527,555 health, so ten ranged reach Weakened in about 7.5 s. Melee
wait for guardian control to bring one to the tank, which holds within 5 yd of the spot; a full-health
Guardian swings from up to 14 yd.

- **Except a leftover Crusher, which phase 3 ranged kill first** (tier 0 beside the leftover Guardians):
  below the Guardians nobody ever hit one, and it kept Diminish Power on the average raider for half the
  phase, while the Corruptors' debuffs are minor, so those stay last. The tank parks every Guardian on the
  melee spot, so the 20 yd window keeps melee on Guardians all phase; left that way by choice.
- **Phase 3 ranged take only a Corruptor or Constrictor their station reaches**
  (`PhaseThreeStationReaches`), or a Constrictor holding somebody; otherwise Yogg, whom the ranged spot
  reaches. Unscoped, all nine once walked through the pile toward a Corruptor 83 yd out. The pets take
  those (below).

**Nobody hits a Guardian a tank does not lead on.** `SetInCombatWithZone` hands a fresh one a threatless
first victim and heal threat reaches every Guardian in combat, so they left the tank a median **+1.0 s**
after spawning and put 26-41k swings on non-tanks. `YoggSaronGuardianThreatAllows` lets a non-tank start
one only while its victim is a tank and the bot's threat is under
`ULDUAR_YOGG_SARON_GUARDIAN_THREAT_START_SHARE` (0.8) of that tank's, and keep it under `…_KEEP_SHARE`
(0.95); taking one over needs 110% in melee, 130% at range. A bot held off drops to the next tier, Yogg at
worst, and the `dps target` fallback takes the same test. With no tank alive in the group it opens.

Every Guardian tier is picked lowest-health first and then held outright: an order that flips mid-fight
resets every swing and cast timer in the raid. Phase 1 replaces both with a shared focus, and a much lower
leftover takes over (below). An Immortal Guardian below 10% is Weakened and only Thorim's Titanic Storm
can finish it, so it stops being a target.

## Threat: the redirects are fine, the taunt budget does not stretch

**Do not re-investigate whether bots redirect threat in P1 — they do, correctly.** Misdirection goes to
the tank, Hand of Reckoning and Righteous Defense land, the latter often on a ranged or healer being hit,
and Tricks of the Trade going to a melee dps is also right: its value falls back to the highest
attack-power melee when the tank already out-threatens the rogue
([pitfalls](../../engine/pitfalls.md#detection-that-never-fires)). Leave all three alone.

None of it governs the Guardians the raid is *not* on (63% of Guardian time on a non-tank). One tank,
~12 single-target taunts in 190 s, 23 independent threat tables, Hand of Reckoning on an 8 s cooldown:
about **one taunt per Guardian death**. So spend it on the Guardian whose death location decides the
phase; the generic paladin taunt is reactive (whoever last hit a raid member), and its aim decays as they
pile up.

**`yogg-saron guardian control` runs in phase 1**, holding the tank inside the **2.8 yd** cloud-free
circle rather than phase 3's 5.0 (a taunted Guardian dies where the tank stands, and outside 2.89 yd the
inner orbit sweeps the pile) and taunting `YoggSaronPhase1TauntTarget`: the raid's focus first, skipping a
Guardian already inside the leash or walking at a tank, and **nothing it cannot reach**. Taunt 355, Hand
of Reckoning 62124, Dark Command 56222 and Growl 6795 share range index 4, so
`ULDUAR_YOGG_SARON_P1_TAUNT_RANGE` is **30 yd**, still the whole 21.5 yd back line.

**An unreachable aim sticks, so the range has to be in the pick.** `PlayerbotAI::CastSpell` selects before
it validates range, so a taunt that cannot land still leaves the tank holding what it aimed at, and
nothing takes that back: `YoggSaronSetDpsPriorityTrigger` stands down for a tank outside phase 2 and the
generic picker keeps any live target. Lowest health anywhere meant whatever a hunter pet was grinding at
the wall: one pull aimed 12 of 15 taunts at 45-57 yd, dealt nothing for 93 s and never finished phase 1,
while a candidate inside 30 yd existed on 79% of ticks. The trigger, a pet holding a Guardian far out, is
intermittent: judge it over pulls.

**So the control node ends on an attack**, as phase 3's does: nothing to fetch or the taunt on cooldown,
the tank hits the lowest-health Guardian already inside `IsWithinMeleeRange`
(`YoggSaronPhase1StackTarget`), holding where none is — in reach only, so holding the pile is never a walk
off the middle (`yogg.tankhold` `attack`).

**A taunt only pays if the raid waits for the walk**, which per-bot kill orders cannot: per-bot locks
preferring a Guardian inside the leash **split** the raid (melee busy in the spacing node kept a Guardian
ranged dropped, and the two died 17 ms apart) or **fell back** to lowest health anywhere (a Guardian burned
16 yd out one-shot a Fervor holder).

**So phase 1 has one focus per instance, `YoggSaronPhase1Focus`.** Every non-tank attacks it, and only a
bot inside the room re-picks it. `YoggSaronPhase1GuardianPreferred` orders both the focus and the taunt,
so the two cannot disagree. `yogg.p1focus` records each change: `picked`, `parked`, `spaced`, `chain`,
`abandoned`, `lower`, `none`.

- **Park floor.** A Guardian outside the leash is worn down only to **35%**
  (`ULDUAR_YOGG_SARON_P1_PARK_HEALTH_PCT`) and parked: the focus moves on, a bot on it drops it (pets
  too), and there is no `dps target` fallback. The taunt takes the lowest-health Guardian outside the
  leash, the parked one, and inside 6.5 yd it is killable again. The floor covers the ~12 s fetch (taunt
  cooldown, the walk, a taunted Guardian standing 2 s) against the ~1.7-3%/s an untargeted Guardian near
  the station loses to splash. Without a living bot tank nothing fetches, so the floor is off. The focus
  is given up past **15 yd** for a killable one inside **6.5**, a gap that stops a boundary Guardian
  flipping it.
- **The handover has no fetch, so no floor.** The tanks drag every Guardian off the stack to the anchor,
  where each would park at 35% with nothing to bring it back, and pulls killed 0-1 in the 18 s. So while
  `YoggSaronHandoverState(botAI).active` the floor is off and the focus is lowest first, the stack test
  meaning nothing once no kill counts for Sara. They stand 18-21 yd out at bearing 104-117°, 21-26 yd
  from the station, so a kill there reaches melee and tanks only. The raid still works the next Guardian
  while a low one waits out its gap, by the user's choice, but `YoggSaronLowGuardianTakesOver` —
  killable, under 30% and lower — gives it the focus back (`lower`); only under 30%, so two fresh ones
  trading places under splash never flip it.

**One focus does not space the novas**: an untargeted Guardian on the stack still lost 5.6%/s to splash
and ground AoE against the focus's 13.0. So:

- **Nova gap.** For `ULDUAR_YOGG_SARON_P1_NOVA_GAP_MS` (6 s) after any Guardian dies, one below **30%**
  (`…_NOVA_GAP_HEALTH_PCT`) is not killable: dropped like a parked one (`spaced`). It buys ~3 s in
  practice, since held Guardians on the stack still lose 4-8%/s to the bot tank (Hammer of Wrath, Seal
  of Vengeance: tanks keep the generic picker), humans, running DoTs and cleaves not typed `Aoe`. The
  clock is `YoggSaronGuardianDeathListenerScript` on 65719, which only a Guardian's `JustDied` casts; it
  holds without a bot tank and through the handover, where the phase still reads 1. It cannot cover a
  simultaneous pair: two deaths 0.000 s apart killed four melee from full.
- **AoE hold.** `yogg-saron phase 1 aoe hold multiplier` zeroes non-heal `Aoe` actions, tanks included,
  while a Guardian other than the focus is below 30% within 10 yd of the middle or of the focus. Its lead
  is only 2.6-5.3 s, so the resolver also cancels an area channel already running: Blizzard, Hurricane,
  Volley, Rain of Fire. Mind Sear fails `IsAffectingArea` (a single-target aura that triggers the area
  spell), and ground AoE already placed keeps ticking.
- **Nova chain.** 65209's second effect hits every other Guardian within 15 yd for 25,000-27,501 (a
  `conditions` row on 33136), ~2.7% of 967,182: a neighbour at ≤3.5% died in the same tick 3 of 3, one
  above 30% never in 92. So a Guardian below 30% is not killable while a lower one stands within
  `ULDUAR_YOGG_SARON_P1_NOVA_CHAIN_RADIUS` (18, over the 16.2 measured on players): the focus moves to the
  lower one if it is killable, else waits (`chain`). The lowest is never held.
- **Splash pairs.** That only holds the focus, and 7 of 12 Guardian deaths a pull are `splash` — DoTs,
  pets, cleaves and the humans: two sat at or under 20% within 12 yd for 8 s, died a millisecond apart and
  killed eight melee from full. So `AppendYoggSaronTargetExclusions` hands the higher-health half of such
  a pair to `RaidUlduarStrategy::AppendTargetExclusionsFor` (which sweeps only when a Guardian is among
  the candidates) for `TargetValueExclusionType::DpsTarget` and `::DebuffTarget` — every dps, dps-aoe and
  attacker picker at once, pets included. `::TankTarget` is left out so the tank keeps holding it rather
  than letting it walk at the back line, and the guid breaks an exact health tie so one of the pair is
  always free to die.

`--phases` tags each phase 1 Guardian death `focus`, `split` or `splash` by how many bot non-tanks were
on it a second before, and lists pairs under the gap and deaths with another Guardian under 30% inside
18 yd.

## Crush is a ±5° cone that tracks its victim

`64146 Crush` is a self-buff applied in the Crusher Tentacle's constructor, `procFlags 4` (melee swing
done), 100% chance, firing **`64147`** (25-man `65201`): physical, base 15199, 23 yd, and `spell_cone`
row `(64147, 10)` with implicit target 104 makes it a **10° cone, ±5° off the tentacle's current facing**,
a runtime fact ([pitfalls](../../engine/pitfalls.md#arc-and-geometry-defaults)). `64147` appears nowhere
in the server `.cpp`; it is reachable only through `EffectTriggerSpell` off `64146`.

**Melee range exempts nobody**: the cone's proximity bypass in `WorldObjectSpellConeTargetCheck` is a
2.0 yd centre-to-centre test against `MIN_MELEE_REACH`, while melee range here is ~10.8 yd (`CombatReach`
8 on display 28814). The danger is being **collinear with the tentacle and whoever it is hitting**: a
four-yard sidestep at 20 yd clears it, and one volley caught ten raiders lined up behind the tank.

So the ranged dodge is by **angle**: reject any spot within 25 yd of a live Crusher and inside its arc.
The bot the tentacle is hitting is exempt from the wedge, since inside the reach it is hit wherever it
stands; leaving the reach is what stops it (below). The wedge model holds: 18 of 19 hits landed within 8°
of the facing, exactly `ULDUAR_YOGG_SARON_CRUSH_TRIGGER_ARC`.

**At melee range no angle works, so melee do not target the Crusher.** With ranged dodging, collateral
moved to a median 3.6 yd and Crush damage doubled, mostly on melee: there the ±8° wedge is ±0.5 yd wide,
hits landed **inside the 2.0 yd bypass where the cone test never runs**, and the cone re-aims onto
whoever is swung at, so a clear bot becomes collinear without moving. `IsAllowedTarget` rejects 33966 on
`IsMelee` rather than admitting on `IsRanged`, which are not strict complements; healers stay eligible.

- **Named risk:** Crushers died in 26-48 s with the whole raid on them against a 50-60 s respawn. Two
  concurrent Crushers is −37.6% raid damage and means the trade failed; the fallback is barring melee only
  while another tentacle is alive.
- P2 can leave melee with **nothing**: Yogg is rejected behind Shadow Barrier, so with only a Crusher up
  they have no target. Deliberate: the tank is then its only other candidate, so every melee bot on it is
  collateral on the tank's Crush line. A fallback Crusher takes the same test.
- **A tank is melee too.** `FindTankTargetSmartStrategy` ranks "not attacking me" then nearest by
  `GetDistance`, which takes off the Crusher's 8 yd reach, and `reach melee` walks the rest: one bot tank
  took 53 of a pull's 59 Crushes. The resolver owns a tank's target in phase 2 too
  (`YoggSaronTankTargetOwned`), dropping it to the Guardian, Constrictor and Corruptor tiers, and the
  movement guard covers what it does not hand out. The Crusher loses the tank's Diminish Power breaks,
  which the Judgement node replaces.

**The tentacle cannot be tanked or steered**: `DamageTaken` does `DoResetThreatList(); AddThreat(who,
100000); AttackStart(who)` on any direct damage, so it re-faces whoever hit it last. Melee exposure is the
root cause (in reach of a live Crusher in 36% of samples against 16% for ranged), and the brain team takes
melee out of the arena for every window.

**Shattered Illusion is the one exception.** `64173` stuns every platform tentacle (its `conditions`
rows) from the moment a brain room clears until Induce Madness lands, and `UNIT_STATE_CANNOT_AUTOATTACK`
includes stunned: no Diminish Power and no Crush land under it. The Crusher keeps its victim, though, and
swings the tick the stun ends, at Induce Madness end or the instant phase 3 starts. So
`YoggSaronCrusherStunHolds` lets melee and pets onto one only while the Brain's `64059` cast has more than
`ULDUAR_YOGG_SARON_STUN_EXIT_LEAD_MS` (4 s) left, exact since creatures take no pushback, and the Brain is
above `ULDUAR_YOGG_SARON_STUN_BRAIN_FLOOR_PCT` (35%; its fastest loss was 5.34% in 5 s). While that holds
the wedge is off. Once it stops, melee drop that Crusher and leave its reach.

**The dodge needs somewhere legal to go, and the retry keeps the wedge.**
`ULDUAR_YOGG_SARON_P2_SPACING_MAX_FROM_MIDDLE` is 55, inside the 60.8 yd outer orbit: phase 1's 35 yd cap
had melee and tanks already beyond it, rejecting every outward candidate, so a three-yard sidestep became
an inward walk along a 25 yd wedge. When nothing clears both shapes the retry gives up the Death Ray
circles, not the wedge: Death Rays killed 1 over 11 hits, Crush 4 over 19.

**An unoccupied Crusher cannot Crush at all.** `UpdateAI` swings only at a victim inside melee range and
`DoMeleeAttackIfReady` tests it again, so with nothing in reach there is no swing, no proc and no cone —
it channels Diminish Power instead. With melee off it, every cone of one pull was aimed at a pet with no
player inside 12 yd, and the raid routed round floor that was never dangerous. So
`GetYoggSaronCrushWedges` raises a wedge **only for a Crusher with something inside its melee range**,
armed for the case the pet guard cannot close: an uncommandable guardian that got back in. It costs the
Diminish Power interrupts pet melee was buying, ~1.5 s apiece.

**`yogg-saron pet guard` keeps pets out of the reach**, walking `m_Controlled` for anything that is a
Crusher's victim or inside its melee range, measured from each pet (from the owner it missed pets on a
stunned Crusher with owners 38 yd away, which then took 8 Crushes in 4 s), skipping a Crusher whose stun
holds. Either half of the test is enough alone: `SetInCombatWithZone` puts the whole raid on the
tentacle's threat list, so a pet that never attacked can be its victim.

- **Pulling a pet off is too late, and it never sticks.** All 5 bot Crush deaths in one pull were cones
  aimed at a pet in the reach, and a pulled pet stays the victim while it walks out, the cone following
  it. PetAI puts a pet back three ways — `OwnerAttacked` on every harmful spell the owner casts,
  `KilledUnit` → `SelectNextTarget` (attacker, then `owner->GetVictim()`, both the Crusher), and
  `AttackedBy` — and all three skip a passive pet, which still attacks while `IsCommandAttack` is set
  (`PetAI::CanAttack`).
- So the guard **hushes** a pet, remembering its stance, whenever it touches, targets, or its owner targets
  a Crusher whose stun does not hold, and drives it by `SetIsCommandAttack` + `AttackStart`: its own add if
  outside the reach, else the owner's target, else `YoggSaronPetFallbackTarget`'s nearest live phase 2 add
  that is not a Crusher, else follow. The trigger also fires on any passive pet so the stance comes back
  once safe, only for pets this node hushed; one still hushed when the gate closes stays passive until
  re-summoned. A per-tick node, not a latch: an uncommandable guardian can walk back in at any point.
- `yogg-saron crusher pet spell hold` blocks `pet acid spit` (cast for the pet at the hunter's target) and
  `shadowfiend` (spawns 3 yd from its target) while the target is a swinging Crusher.

**In phase 3 the pets kill the far leftovers**, which owners rank under every Guardian: unsent, a 745k
Corruptor lived to +68 s with its debuffs dispelled almost at once. `YoggSaronPhase3PetLeftover` picks one
within 70 yd of Yogg — a holding Constrictor, then without Thorim one a beacon heal reaches (below), then
lowest health, then GUID — skipping any within 25 yd of a swinging Crusher, or the guard would pull the
pet straight back. Only proper pets are commanded (`IsPet`: hunter, warlock, unholy ghoul), since a 30 s
Treant or Spirit Wolf would spend its life walking; the leftover is also the `elsewhere` target, ahead of
the owner's, when a pet is pulled off a Crusher.

**For ranged and healers the reach itself is the hazard.** The tentacle's melee range on a player is
`CombatReach` 8 + 1.5 + 4/3 = **10.83 yd**, no leeway since it never moves. Inside it a ranged bot hits it
in melee — a caster's weapon swing (visible only as its imbue proc, Flametongue Attack), or a hunter's
Raptor Strike and Mongoose Bite, Auto Shot's dead zone being min range + melee range = 15.8 yd — which
makes it the victim through `DamageTaken`, and the next swing Crushes it from full: every Crush kill of one
pull, each walked in by a phase 2 dodge testing only the wedge's angle. So for `!IsMelee` phase 2 spacing
adds every live Crusher as a circle at `ULDUAR_YOGG_SARON_CRUSHER_REACH_CLEAR_RADIUS` 13 (trigger 11), in
the hazards and the retry both, with **no victim exemption and no swing gate**: walking out ends it.
Melee get it only for a stun about to lift: otherwise they are off 33966, and a Constrictor beside a
Crusher would have the circle and `reach melee` trade them every tick.

**The phase 3 ranged station dodges for itself**, being a forced walk onto a fixed point that spacing
cannot cut: a Crusher left 7.3 yd from `ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT` killed seven ranged when
the stun lifted at phase 3 start. `YoggSaronPhaseThreeSpot` gives ranged the first of the ranged spot and
four standbys — it turned −15, +15, −30, +30° round Yogg at 39.06 yd, navprobe on mesh with straight
paths, 21.6-25.2 yd from the melee spot — clear of every live Crusher by
`ULDUAR_YOGG_SARON_PHASE_3_RANGED_CRUSHER_CLEARANCE` 18 (reach 10.83, the 15.8 yd Auto Shot dead zone, 2
slack), else the furthest. Same answer for every bot; back to the ranged spot once the Crusher dies.

## Diminish Power is why the Crusher dies first

`64145 Diminish Power` is a **5-minute channel** (`DurationIndex 5`), **−21% damage done raid-wide** with
no distance pruning, **multiplicative** across tentacles (two up = −37.6%, three = −50.7%), `DispelType 0`,
and unkickable twice over: the tentacle is immune to `MECHANIC_INTERRUPT` and `MECHANIC_SILENCE`
(`CreatureImmunitiesId −361`), and the channel carries flag `0x10`, not the interruptible `0x08`. It
ignores LOS and immunities. **A cast skips anyone underground**: `ExcludeTargetAuraSpell` is Illusion
Room `63988`, which every portal teleport (63989 / 63997 / 63998) force-casts and only `63993` on the way
out removes. A bot debuffed before its portal keeps it until that channel breaks.

Only two things stop it. A **melee hit or melee-class spell** buys ~1.5 s: `64148` on the tentacle has
`ProcTypeMask 0x28` (taken melee swing, taken melee-class spell), `spell_yogg_saron_diminish_power_aura`
breaks the channel once channelling but never during its 1.5 s cast, and `_diminishReady` is set once at
6 s and never reset, so it re-casts straight away. Shattered Illusion only pauses it. Or the tentacle dies
— **2,000,001 HP** in 25-man, 25-34 s for the whole raid even debuffed, against a respawn of 45-54 s
tightening to 25-30 s by the sixth wave. So the Crusher is the **top** target for everyone who can safely
stand there: every ranged and healer.

**Judgement breaks it from outside the reach.** Every Judgement damage spell (54158, 20187, 20467, 31804,
53726, 53733) is `DmgClass 2`. They are triggered, but `SPELL_ATTR3_NOT_A_PROC` and the generated proc
entry for a taken-flag `PROC_TRIGGER_SPELL` aura (`PROC_ATTR_TRIGGERED_CAN_PROC`) each clear
`Aura::GetProcEffectMask`. The damage is `SPELL_DIRECT_DAMAGE`, so `DamageTaken`'s aggro grab never fires,
and 10 yd through `IsWithinCombatRange` is ~19.5 yd centre to centre. `yogg-saron diminish power
judgement` (`ACTION_RAID − 0.5`) has any paladin with a channelling Crusher already in reach judge it
(wisdom, light, justice, one shared cooldown), with no walk and no target change, gated on
`SPELL_STATE_CASTING`. No stagger: the Judgement lands inside `Spell::cast` and the next paladin finds no
channel. Each drops 64145 raid-wide in the same millisecond; it is re-cast 60-80 ms later.

**Read the channel off the aura, never off cast starts.** After a break the tentacle re-casts only once
its victim is out of melee range, and otherwise swings, so a break followed by Crushes leaves no cast
row. Uptime *rises* once ranged stop hitting it in melee, so judge the Judgement node on channel drops
within 300 ms of a Judgement; the recorder writes that drop ~1 ms *before* the Judgement's cast row, so
read the channel 50 ms before the cast.

## Phase 2: the body is a wall, and the portals run on a clock

**Yogg's body knocks players away once a second, permanently.** `ACTION_YOGG_SARON_APPEAR` casts
`SPELL_KNOCK_AWAY 64022` on him and never removes it: an infinite self aura,
`SPELL_AURA_PERIODIC_TRIGGER_SPELL` every **1000 ms**, firing **64020** — `SPELL_EFFECT_KNOCK_BACK`, radius
index 61 = **14 yd**, horizontal speed 15 — with no `conditions` row, no script filter and no
combat-reach bonus, the caster being a creature
([reach](../../engine/raid-mechanics-lessons.md#range-and-reach)). His model sits at z 329.397 over a
floor of 324.89-325.19, so it is a **13.26 yd horizontal ring that never goes away**, reaching a trace only
as a `haz` circle and `yogg.knockback`. Measured, launches started 7.8-12.9 yd out; read the last grounded
sample, since the first airborne one is a third of a second into the flight and 6 yd too far.

**It costs melee nothing; crossing it is what costs.** Yogg's `CombatReach` is **30** (display 28817,
`BoundingRadius` 0), so melee range on him is ≈ **34 yd**, and the P3 melee spot is 18.4 yd out. But 42%
of portal walks routed within 12 yd of the body. So P2 and P3 spacing carry the ring as a standing hazard
circle, the trigger reads it, and the spacing walk and walks to a fixed spot filter the route; a bot
already inside is exempt, since every short step out passes close to the middle. A blocked crossing gets
one waypoint at `ULDUAR_YOGG_SARON_BODY_DETOUR_RADIUS` (24 yd) on the bisector of the shorter arc: each leg
halves the next turn, so one is enough, and a half-turn passes 24 × cos 45° = 17.0 yd from the middle.

**The walk at a target across Yogg ends on top of him.** `ReachCombatTo` shortens its path to
`disToGo / 2 + distance`, so half the way across the room is the body, and the reach nodes are the one walk
the route filter does not cover: eight ranged sent at a Crusher 56 yd away landed at 7.6-9.4 yd and the
next pulse threw thirteen. Every other read asks where the bot stands, a tick too late against a 1 s pulse.
`yogg-saron body detour` (`ACTION_RAID − 1`) asks where the walk goes: while the bot is out of its own reach
of the target and `YoggSaronRouteClearOfBody` says the straight line passes inside the ring, it walks the
arc waypoint, then stands down. Under every raid node and over `charge` and both `reach melee` entries, so
the price is one shadowed gap-closer per arc leg. It skips a target inside the clear radius, Yogg included,
where no arc arrives, and tests reach with combat reach: centre distance never passes on Yogg or a
full-health Guardian.

**Phase 3 keeps the body.** Unhandled there, phase 3 had fifteen times phase 2's launches: the brain team
surfaces west of Yogg and P3 positioning walked it in one forced line to the melee spot east of him. The
spacing trigger, the reach veto and the detour run in phases 2 and 3, and every walk to a fixed spot (the
P3 station and tank leash, guardian control's leash, a Sanity Well once Yogg is up, a portal) aims at
`YoggSaronBodyRoute`: the spot, or its detour waypoint (`yogg.detour` `spot`).

### The handover

**The ring arrives 18 s after Sara dies, on top of the melee pile.** She hits 0 and Yogg is summoned
**invisible** that tick; `ACTION_YOGG_SARON_APPEAR` casts Shadow Barrier and 64022 together at the end of
the dialogue (4 + 5 + 4.5 + 4 s plus the 500 ms `EVENT_SARA_P2_START`, measured 18.0-18.3 s). The phase
reads 1 throughout and Guardians are usually alive, so the leash kept every melee and the tank in the ring
when it lit, while the ranged and healers on the station were clear. So the walk out belongs to melee and
the tank, and goes to **one anchor**.

**Yogg without Shadow Barrier is the whole window**: `SetVisible(false)` does not hide him from a grid
search, so the encounter's tick latches it from the first tick and `YoggSaronHandoverState` reads that.
P3 strips the barrier again and the Brain separates the two: summoned the tick the barrier first lands,
it is up for everything after the window and absent for all of it. Nothing in the world counts the
dialogue down.

**Not a fan of bearings.** Each bot out along its own bearing spread the front line ~40 yd and left nearly
every melee past every healer's 38.5 yd `Playerbots.HealDistance`, and a westward bearing ends between
ring and station: a tank walked three taunted Guardians to 6.6 yd of the stack and two detonations killed
eleven.

**`ULDUAR_YOGG_SARON_HANDOVER_MELEE_SPOT` (1973.315, −5.246, 324.889)**, every term from a constant here:

- on `…_P1_RANGED_STATION_RADIUS`, the one radius only orbit 2 reaches, which the back line already wakes
  (a 15 yd ring sat in orbit 1's 20.1 yd reach, which only feet wake, for the whole 18 s);
- **25 yd round from `…_P1_RANGED_SPOT`**, which is `…_SHADOW_NOVA_CLEAR_RADIUS` (20) plus
  `…_P1_RANGED_STACK_RADIUS` (5), so a Guardian dying anywhere in the melee group stays 20 yd off the
  nearest back-line bot;
- 6.5 yd clear of the knock back and of a nova left on the pile; 20-25 yd from the healers; on the +y
  half, where melee already stand. Floor by navprobe, `settledZ` 324.889, the station's own Z.

**The walk is immediate, and `clearing` is a role test rather than a radius.** Any bot the positioning
trigger owns — `IsTank || IsMelee`, the test it opens with — walks the moment Sara dies, with 18 s of
slack. A lead to hold the middle (`max(4 s, walk ÷ speed × 2)`) cost more than it bought: bots were still
in the ring when it lit, and a bot already outside 15 yd got no `clearing` at all, so `reach melee`
dragged it back. Leftovers stop counting for Sara the moment she dies (`DamageTaken` returns early on
`_secondPhase`) but their novas still land for 25k, which the anchor's 25 yd answers outright. No radius:
the anchor is outside the ring, so a radius test would release a bot on arrival. **The walk outlives the
ring**: "no barrier" ends the tick it lands, and melee released there walked straight back under the
knock back, so `ULDUAR_YOGG_SARON_HANDOVER_HOLD_MS` (6 s) keeps `clearing` true past it and the movement
guard keeps `reach melee` off for the same window.

**Guardian control's walk stands down for the window and keeps taunting.** It walks the tank back to the
middle from outside 2.8 yd, and with both walks `MOVEMENT_FORCED` each cleared the other's hold and the
tank crossed between middle and anchor one to four times a window. `…_P1_TAUNT_RANGE` (30) covers the
room from 21.5 yd out, which also keeps the Guardians the 25 yd off the back line. Since, the median bot
stands 0.0-0.2 yd from the anchor when the ring lights, melee 0 of 9 in the ring every pull, counting live
bots only (a corpse keeps its last spot, then releases to the entrance).

**Open: a front line that stacks perfectly is uniformly far from the healers.** The anchor (21.5 yd,
bearing 109°) and the healers (21.5, bearing 180°) are **25.0 yd** apart before anyone moves, and after the
ring most melee and tanks leave every healer's 38.5 yd at some point. The walk itself wakes nothing: a
cloud marks its Guardian 10 s ahead and phase 1 ends first.

**Three to six Guardians are alive at the ring in every pull, and the pile is one blast**: a nova on the
anchor catches all nine melee, and two Guardians dying 0 ms apart killed five parked melee from full. The
anchor bought reachability and the back line's safety, not this.

**The leftovers keep phase 1's nova rules past the barrier.** Once it lands `YoggSaronPhase` reads 2, and
as plain tier 0, lowest first, with melee cleaves working the pile, they died 1-2 s apart and the second
nova killed several melee. `YoggSaronLeftoverGuardianKillable` (gap and chain, no park floor — no stack
left) gates them in `IsAllowedTarget` and in `Execute`'s drop, since a null resolve keeps the old target
(`yogg.leftover` `spaced` / `chain`), and `YoggSaronLeftoverAoeHold` — a non-target leftover under 30%
within 10 yd of the bot or its target — holds AoE and cuts area channels. `lastGuardianDeathMs` still
stamps, as 65719 goes out on every leftover death. Between two Guardians of Yogg-Saron `ResolveTarget`
gives way to `YoggSaronLowGuardianTakesOver` (`lower`): held outright, ten ranged sat on a 44% one while
the lowest was free at 2.6%. Immortal Guardians still hold outright.

**Nothing decides where the leftovers die.** All three phase 1 rules that keep a death off the raid stop
at the ring: the leash (`YoggSaronGuardianPositioningTrigger` reads `YoggSaronInPhase1`), the tank's
control node (phases 1 and 3 only) and the walk guard's phase 1 rules. After it raid and Guardians hold
the same 90-210° arc, the Guardians ~4 yd inside, so a nova lands on somebody: one pull's five leftovers
were 9 of its 10 deaths, one killing all four healers in the back-line stack.

- **A step-out has to outlive the Guardian, not the bot's own clearance**: four healers clear of 20 yd
  were walked back to 4.8 yd by the body detour, since `YoggSaronPhase2SpacingTrigger` stands down the
  moment the bot is clear. Hence the walk guard's radius past phase 1 (above).
- **Where the late ones come from stays, by the user's choice.** Some are woken by the back line in the
  last 18 s of phase 1 and arrive too late to kill: the station's own costed trade.

**With none alive the raid drops combat and eats**
([eating blinds the bot](../../engine/pitfalls.md#before-the-pull-and-out-of-combat)), and two melee were
still eating when the ring threw them. `yogg-saron stack food guard multiplier` zeroes `food` and `drink`
inside 15 yd of the middle (`…_BODY_KNOCKBACK_CLEAR_RADIUS`) all encounter; the station keeps eating.

### Portals

**The portals are creatures, one-use, and on a clock the raid has to beat.** `NPC_DESCEND_INTO_MADNESS`
(34072), `TEMPSUMMON_TIMED_DESPAWN` 25 s, `OnSpellClick` clearing the npcflags so each takes exactly one
passenger. `EVENT_SARA_P2_OPEN_PORTALS` fires **60 s** after P2 starts and repeats every **80 s**; clearing
a room delays Sara's other events but explicitly reschedules this one. `AddPortals` spawns
`RAID_MODE(4, 10)`, so **only the first four table entries exist in 10-man**. Not hostile, so no snapshot
samples one: `yogg.wave` is the only record
([observability](../../systems/observability.md#what-a-trace-cannot-tell-you)).

The core's table is `yoggPortalLoc` (all ten of the module's old coordinates were wrong):

```
(1964.60,-42.71,325.08) (1986.94,-46.21,324.98) (1989.50,-6.70,325.08) (1965.52,-8.09,324.95)
(2000.84,-25.40,325.19) (1960.22,-26.14,325.01) (1976.30,-47.83,325.11) (1997.69,-37.46,325.04)
(1998.07,-13.36,325.17) (1976.99,-3.96,325.17)
```

All navprobe-clean, settled z 324.87-325.04, **20.1-23.2 yd from the body** and so outside the ring. Keep
them in code rather than reading live creatures: the team has to be *standing* on its spot before any
portal exists.

**Spread before the wave, nearest first.** By group index the walk ran ~3× the distance to the nearest
spot. The plan is latched once per wave so it does not churn, off a per-instance wave clock: predict the
first wave at +60 s, re-anchor on every wave a bot sees, and roll the prediction forward a period when one
passes unseen, or it reads "any moment now" forever. **Compare wave times signed**
([`getMSTimeDiff` wraps](../../engine/pitfalls.md#state-does-not-cross-objects)): read as a wrap, the
roll-forward pushed the prediction 80 s on every call and nobody ever spread. A bot walks once the next
wave is inside `max(8 s, distance / runSpeed × 2)` and holds until the portal appears under it. The walk
goes quiet on arrival and reach would take the bot back, so `yogg-saron portal spot hold` blocks reach,
heal reach, set behind, rear flank, formation moves, charges, follow and random moves while
`YoggSaronHoldingPortalSpot` holds (slot and distance read before the plan); the Yogg dodges still move
it. The out-of-phase-2 reset clears the clock but not `waveOrdinal`, harmless while `slotWave` carries
with it, so read `yogg.wave` as a session counter.

**Rebuild the plan when the portals spawn, and take one that is still there.** A plan keyed on the wave
ordinal alone never rebuilt, since the ordinal is the same before and after the portals appear, and was
built off stale positions. And a slot is only a suggestion: the click takes whichever portal is inside
`ULDUAR_YOGG_SARON_PORTAL_CLICK_RADIUS`, so bots took each other's and left others stranded beside empty
spots. So the plan rebuilds once more on the tick the portals are first seen, a bot whose own spot is
empty walks to the nearest portal still alive, and arriving and clicking share one radius, so `holding`
implies clickable.

**The brain team is melee first, then exactly one healer, then ranged; tanks never.** The room is a 60 s
race on foot, and it doubles as the Crush answer. GUID order within each band so every bot derives the same
team, except healers: Val'anyr's wielder (46017, its heals shield) first, since item level can't pick it,
then priest, druid, shaman, paladin, since a cast-time spell cannot go off on the move
([pitfalls](../../engine/pitfalls.md#a-moving-bot-cannot-cast)) and a holy paladin spends a third to
two-thirds of the room walking with most heals on a cast time. `GetYoggSaronBrainTeam` is the single owner,
read by trigger and action alike, and benches low Sanity (below).

**Phase 2 always hands phase 3 some tentacles.** `AddPortals` cuts `_summonSpeed` by 0.1 a wave, so by the
sixth a Crusher spawns every 20-24 s, a Corruptor every 12-16 and a Constrictor every 6-8. The stun is the
only time the platform gains: `DelayEvents` holds every spawn timer, and nothing channels or swings. With
every melee on the brain team the stun is left to ranged, pets and the tank.

**Brain Link's partner is readable from the cast, never from an aura.** 63802 goes on **one** player:
`spell_yogg_saron_brain_link_aura` picks a random living player within 50 yd on apply, keeps that GUID to
itself, and drops the link if either dies or they end up more than 10 yd apart vertically. Neither 63803
(apart: DBC damage plus −2 Sanity on **both** ends past **20 yd**) nor 63804 (together) leaves an aura, so
no aura test can name the partner, and `TooFarFromPlayerWithAura` cannot help — it measures the gap to
*other holders of the same aura*. But the aura casts one of the two at the partner **every second**, so an
`AllSpellScript` on `ALLSPELLHOOK_ON_PREPARE` in `UldBotScripts.cpp` latches the pair per instance,
expiring after three missed ticks. Both ends walk, unless one is under z 300.

They walk to their **midpoint**: one bot chasing another moving away at the same speed never arrives, and
links left open cost a pull ~480k damage and ~680 Sanity. A midpoint inside the ring, which two bots on
opposite sides of Yogg produce, is pushed out to `ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS`.
`yogg-saron brain link` (`ACTION_RAID + 1.5`) sits over the dps resolver and the Sanity Well walk and under
the hazard dodges — a link costs 2 Sanity and a shared hit a second, a Death Ray costs the bot — and
closes to `ULDUAR_YOGG_SARON_BRAIN_LINK_CLOSE` (15, a margin under the 20). It stands down for a brain team
bot whose portal intent is spreading, holding or late (`YoggSaronPortalWalkPending`), since the link breaks
by itself once the bot is 10 yd below, and closing it cost one wave its healer. In phase 3 the walk stops
at Crush range (25 yd) of a live unstunned Crusher, whose cone otherwise killed the walker.

### Sanity

**Top up Sanity whenever the platform has nothing to kill.** Each well's 64169 is a 6 yd area aura giving
+20 Sanity every 2 s and **−50% damage done**. `yogg-saron sanity` walks a bot to the nearest well below 40
and keeps it inside 1 yd there until 100 unless its portal walk is pending; in phase 2 it also walks it at
any Sanity under 100 while no Guardian, Crusher, Constrictor or Corruptor lives, unless it is a brain team
bot with a portal walk pending. Those windows come mostly after the team returns, stunned tentacles
outlasting it. It stands down on the walk latch (an unreachable well once parked three bots for minutes,
suppressing every node below it) and below the floor, since all five wells stand on the platform and
nothing restores Sanity underground.

**A brain team bot under 60 Sanity sits the next wave out.** A trip costs a median 22 Sanity, so the team
ratchets down, and topping up between waves doesn't fit: returns land 35 yd from the nearest well ~22 s
before the next portals, and that well is 65 yd from a portal spot. `GetYoggSaronBrainTeam` skips
`ULDUAR_YOGG_SARON_BRAIN_TEAM_MIN_SANITY` (60) and fills from the next in order, leaving portals empty
rather than sending a low bot. The bench is decided when the team is back (portals down, the Brain not
casting 64059), which adds one plan rebuild per wave so the replacement has ~20 s to walk; the rebuild at
portal arrival reuses it. A bot under 60 with no portal walk pending walks to a well as `refill`, and
`topping` holds it to 100.

**No res for a corpse without Sanity.** Sanity is death-persistent (AttributesEx3 0x100000), an Insane
death has lost it, and the Voice's Insane Periodic 64554 re-applies Insane to any player without it: every
such Rebirth came back charmed 2-8 s later. `FindDeadPlayer` skips a corpse with none while the reviver has
it, so the next one still gets the res (a multiplier would block every corpse ranked behind), and
`SelfResurrectTrigger` stands down the same way.

## The brain room

**Induce Madness `64059` is a 60,000 ms cast.** On completion everyone at **z ≤ 300** loses all 100 Sanity
and is teleported out; anyone above takes nothing. No Sanity means `63120 Insane`, and
`spell_yogg_saron_insane_aura::OnRemove` calls `Unit::Kill(owner, owner)` — **a mind control is always a
death.** It is cast in the tick `AddPortals` spawns the portals, so `yogg.wave` + 60 s is its end. A bot
ending a window ~120 yd from the nearest exit portal needs ~17 s to walk it, and the exit node is the
difference: visits where it ran produced 0 mind controls in 14, visits where it never ran 10 in 17.

**An uncleared illusion room has no exit at all.** All three Flee to the Surface goobers (194625) stand in
the brain chamber — (2000.65, 5.79), (1943.06, −23.51), (1998.42, −59.85) — **93 to 109 yd** from an
illusion room's middle and behind `GO_*_ILLUSION_DOORS`, which the Brain sets `GO_STATE_READY` when the
illusion starts and `GO_STATE_ACTIVE` only in the statement that despawns every skull, on the room's last
tentacle kill. A room not cleared inside the 60 s kills everyone in it, so every walk below is on that
clock. Killing all eight tentacles in T ms opens the door **and stuns everything upstairs for 60000 − T
ms**; nothing interrupts the cast, `_induceTimer` being only a stopwatch for the stun length.

**Damaging the Brain while an Influence Tentacle (33943) lives deals nothing and kills the attacker** —
`boss_yoggsaron_brain::DamageTaken` zeroes the damage and calls `Unit::Kill(who, who)`. So approaching it
is gated on server facts, never on a human (a gate waiting for the master to stand in the brain room left
the Brain untouched in every window without one):

- **from an illusion room**, no live tentacle **and** that room's door (`194635` Chamber / `194636`
  Icecrown / `194637` Stormwind) reading `GO_STATE_ACTIVE`;
- **from inside the brain chamber**, **any** door standing open, since `DoAction` shuts all three when it
  prepares an illusion and opens exactly one on the last kill: an exact test where a radius cannot be one
  (the Chamber's far tentacles are 167 yd out).

A wrong read leaves the bot standing still rather than walking in to die.

**The illusion is random per attempt, in start and direction.** `Reset()` does
`_currentIllusion = urand(1, 3); _isIllusionReversed = urand(0, 1);` and steps ±1 with wraparound per
wave, so nothing may assume a fixed cycle. Every portal in one wave leads to the same room.

**Where the bot is: a z band, then 60 yd room circles.** The Brain sits at z 265 over a floor of z 236-244,
and the portal arrivals are 60-72 yd from it, so a radius to the Brain is a bad "am I in the brain room"
proxy: use the z band. `YoggSaronRoomOf` is the single resolver, brain room tested first, on 60 yd
circles: they cover each room's landing spot (49-56 yd from its middle) and every Laughing Skull spawn
(52.8 at the furthest), against 190 yd to the nearest rival middle, where 150 yd circles overlapped and
read the brain room as Stormwind. `yogg.room` records what the bot *read*.

**Scope the tentacle read to the room, centred on the room.** The Stormwind and Chamber middles are 200.8 yd
apart, so a 200 yd sweep was one yard from reading the next room, and a wrong read is `Unit::Kill(who,
who)`; the Brain's own test is `_tentacleCount < _tentacleTotal`, a per-wave counter. The Chamber is 96 yd
deep, so from its doorway a 60 yd read round the bot covers half of it: a healer once called the room clear
and stood at the door while melee died 90 yd further in. `YoggSaronInfluenceTentaclesCleared` measures from
`YoggSaronRoomMiddle` inside an illusion room and from the bot only in the brain chamber, where the
question is which room it came out of; it is the one read behind the healer station, `yogg.roomstate` and
the approach gate.

**Read the disguises too, or a full room reports empty.** Every spawn in all three summon groups wears a
disguise (below), so **no tentacle is entry 33943 when the raid arrives**, and
`boss_yoggsaron_influence_tentacle::DamageTaken` reverts it only once hit: a 33943-only sweep answered
*cleared* on arrival, and in Stormwind, whose tentacles sit out of sight behind the Keep doorway, seven
bots stood on the landing until Induce Madness took them. `YoggSaronLiveIllusionMob` sweeps all eight
entries in one grid visit and owns every "is a tentacle alive" read, `yogg.tentacle` included.

**Only eight entries are tentacles.** From `creature_template`: Alexstrasza, Malygos, Neltharion, Ysera,
the Immolated Champion, Garona and King Llane are faction **35**, friendly RP actors; The Lich King (33441)
is faction 14 and hostile but carries **11.1 M** health, never attacks and cannot be killed. The list is
33943 plus its six disguises (33433 Suit of Armor, 33567 Deathsworn Zealot, 33716-33720 Consorts). Which
disguise comes from where it spawned — `x ∈ (2000, 2150)` a Consort, else `y ∈ (−150, −90)` a Zealot,
else a Suit of Armor; the wiki has Zealots and Suits the other way round and the script wins. The disguise
is `Creature::UpdateEntry`, not an aura, and `UpdateEntry` **preserves current health**, so a Suit of
Armor showing 6% is a full-health tentacle carrying 8,000 (10) / 40,000 (25), 49,135 in the scaled 25-man
traces.

**Something must walk the bot into the room, or it deadlocks.** The resolver needs `IsWithinLOSInMap`, two
of the three rooms hide their tentacles behind a doorway, and the walk to the Brain needs them dead: out of
sight → nobody walks in → it lives → the door stays shut, with `DpsAssistAction` zeroed so there is no
fallback. `WalkIntoRoom` walks at the **nearest live tentacle**, not the room's middle (the centroid of its
Influence Tentacle summon group, `creature_summon_groups` summonerId 33890: group 1 Chamber, 2 Icecrown,
3 Stormwind; navprobe-clean `PATHFIND_NORMAL` from every landing): that is the one spot in the room with
nothing on it — the ring is 27-29 yd out in Stormwind, 12-29 in Icecrown, 36-44 in the Chamber — and a walk
there took melee through the centre and back out, while `reach melee` waited under the forced walk.
`YoggSaronRoomStateOf` owns when to stop and writes `yogg.roomstate`.

**Spread the team over the tentacles, because nearest stacks it.** Tentacles never move and the team walks
in as one pack from one landing spot, so nearest-first gave every bot the same one, and the first hit
reverting a disguise to 33943, a tier above, pulled the rest onto it. Spawns: Stormwind 8 on a ~30 yd ring,
Icecrown 9 in three clusters of three 16-35 yd out, Chamber 8 on a 39-43 yd ring. `SpreadTarget` treats
revealed and disguised alike and picks what the fewest other room members hold by `GetTarget` (humans
counted, healers not), then nearest. A bot keeps its tentacle while nothing is less held, and always keeps
one no lower GUID is also on, so two that race onto a free one settle next tick. Grim Reprisal reflects
per hit, so the split changes nobody's incoming damage.

**In Icecrown and Stormwind a bot clears its share, then waits at the door**: swarming the last tentacle
left the whole team 106-130 yd from the melee spot when the door opened. `YoggSaronTickIllusionShare`
latches per bot per tick what it may take, and `YoggSaronIllusionTentacleAllowed` gates the resolver, its
`dps target` fallback, the drop of a current target and `WalkIntoRoom`:

- **Icecrown:** each bot joins the group (centroids of summon group 2) with the fewest bots planned so far,
  ties east, west, south, so 9 → 3/3/3 and a 10-man's 3 → 1/1/1, reset per `waveOrdinal`. It takes only its
  own group, and another only once its own is clear and nobody planned for that one is alive in the room.
- **Stormwind:** a bot whose first tentacle (held when it died) is inside `…_STORMWIND_HELPER_RADIUS`
  (40 yd) of the stage spot — the two at 31 yd — keeps clearing. The rest keep the one they are on or take
  one no other room member targets, so a 10-man or a death never leaves one uncovered.
- **Both:** a player's mark (`rti target`) is always allowed; everything lifts under `…_SHARE_LIFT_MS`
  (25 s) of Induce Madness, since the slowest clear left ~29 s and an uncleared room charms everyone.
  Healers, the Chamber and bots off the brain level have no limit.

With nothing allowed and the room alive the state reads `staged`: the illusion room node walks the bot to
`…_ICECROWN_STAGE_SPOT` / `…_STORMWIND_STAGE_SPOT`, and `yogg-saron illusion stage hold` blocks the
reprisal hold's target pickers except `AttackRti`. The spots sit on the navmesh path from landing to Brain,
6 / 3 yd past the landing, because both doors are inside the brain room's 50 yd circle (Icecrown 49.0,
Stormwind 45.7) and a bot at a door would read as already in the brain room; arrival is 1.5 yd, each spot
being ~3 yd inside either circle. The Chamber keeps plain spread.

**Grim Reprisal is what kills the brain team, and only a hit triggers it.**
`spell_yogg_saron_grim_reprisal_aura` casts 64039 for 60% of the hit at the attacker, totems skipped;
63305's `ProcTypeMask` 0x222A8 has no `TAKEN_PERIODIC` bit and no `spell_proc` or linked override, so DoTs
never reflect and a bot that stops hitting stops taking it. Melee died below still swinging at 21-39%, the
worst burst 35% of max health in 0.2 s. So a damage dealer in an illusion room holds under **40%** and
fights again at **70%**, latched in the tick as `yogg.reprisal`: only while a healer lives in its room, and
never with Induce Madness under 15 s, since an uncleared room charms everyone. The resolver drops the
target, `yogg-saron reprisal hold` blocks the other pickers, melee and charges, the room state reads
`reprisal` so the walk-in leaves it alone, and `yogg-saron reprisal regroup` walks it to 15 yd of the
healer past 25, since most of those deaths had the healer 37-50 yd away.

**Each tentacle a hit lands on reflects separately**, so a cleave on three returns 180% and killed a
warrior from full in 2 s, faster than any hold. `yogg-saron illusion multi target hold multiplier` zeroes,
in the rooms, every AoE-threat action and a name list, since most cleaves (Bladestorm, Cleave, Whirlwind,
Divine Storm, Howling Blast, Multi-Shot) carry no AoE threat type
([RML](../../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)). Heals are never held.

**The healer stations on its room's live tentacles, not the middle.** A target is what `reach spell` walks
it to, which left mates out of range, so the resolver gives a healer in an illusion room none, dropping it
without interrupting its heal. `WalkIntoRoom` skips it, since a targetless healer reads `walkingin` all
wave and a forced walk every tick starves every heal under `ACTION_RAID`. `yogg-saron illusion healer
station` (`ACTION_RAID`) walks it back past 10 yd while tentacles live, **above** heal reach, which every
healer spec wires at 38-40: the station puts the whole room in range, so it comes before a step toward one
mate. It is the centroid of the live tentacles, where the melee are, at the middle's own z (the Chamber
floor ramps 239.7-244, and an averaged z stations it in the air); the middle is the fallback with nothing
alive. The middle itself fails the Chamber: it is 36-44 yd from its own tentacles against `HealDistance`
38.5.

**A forced walk below the platform can stall in ~2 yd hops.** `IsDuplicateMove` refuses the same point for
`MaxWaitForMove` ([a walk stopped short](../../engine/pitfalls.md#movement-that-silently-no-ops)), and
`ULDUAR_YOGG_SARON_WALK_GIVE_UP_MS` then ended the walk for good — its `fresh` escape wants a gap in the
asking, and these nodes ask every tick — leaving ranged motionless until Induce Madness charmed them. The
stops came from outside the walk: the `offensive potion` (armed on a boss target, the Brain counts)
through `UseItemAction::UseItem`, fixed at the source
([../../systems/consumables-and-burst.md](../../systems/consumables-and-burst.md)), and the skull turn on
a targetless bot (below). `YoggSaronReleaseStalledWalk` stays for the next one, the `ReleaseStalledWalk`
shape (drop the booking once the bot has stood still past 500 ms); the give-up expires after
`…_WALK_RETRY_MS`, and `yogg.walkstep` records the yards still to go on every release.

**The Laughing Skull cannot be killed, only faced away from.** 33990, faction 14, `unit_flags`
`UNIT_FLAG_NOT_SELECTABLE` with `flags_extra 128`: neither targetable nor killable.
`creature_template_addon.auras = 64167` triggers **64168 every 1000 ms — 1750 shadow damage, −2 Sanity,
30 yd** — filtered exactly to `target->HasInArc(M_PI, caster)`, so only players with the skull in their
front 180° are hit. Four per room, despawned when the last Influence Tentacle dies. `yogg-saron laughing
skull` turns a bot off the skulls, at `ACTION_EMERGENCY` beside the Yogg gaze node — the two cannot share
a bot and never need to, Yogg's gaze being a P3 platform aura and the skulls only existing below.

- **Never while moving**: `Unit::SetFacingTo` lays a spline on the spot and ends a point walk.
- **The heading is judged on every skull in range**: the one of 16 with fewest ahead, nearest the current
  on a tie, turning only for a strict gain. Off the centroid of those in arc, the in-arc set changed with
  the heading and bots spun every snapshot.
- **It never costs a tick.** The engine ends a tick at the first action returning true, so a node claiming
  every tick with a skull in arc starved the resolver, the walk-in and the exit, while `set facing` (37),
  `AttackAction` and `PlayerbotAI::CastSpell` turned the bot back each tick, and bots stood flipping until
  charmed. With a live target it turns only within 40° of it (`YoggSaronAngledSkullHeading`, 5° steps,
  strict gain, `yogg.skullturn` `angled`), inside set facing's 45°, so nothing turns it back: a skull off to
  the side clears that way, one behind the target is the facing walk's. Yielding outright left a ranged bot
  under a skull for 15 s. It corrects a heading only past `ULDUAR_YOGG_SARON_FACING_TOLERANCE` (0.1 rad).
- **The 30 yd read is measured plainly**: `GetCreatureListWithEntryInGrid` is bounding-radius inclusive,
  and most probe flips once had no skull inside 30 yd.

**Where the bot stands holds a facing the turn can't.** `yogg-saron illusion facing` is a spacing node
whose `clear` predicate asks whether the heading a candidate spot *would force* leaves every skull within
30 yd outside the front 180° (`YoggSaronFacingClearOfSkulls`, the turn's own 40° question) **and** whether
the spot is still inside melee or spell range of the target. The geometry is generous: the bot usually
stands far nearer its tentacle than the skull, so a few yards' sidestep swings the skull behind. In range
is a requirement, not a preference the sweep may drop: a spot out of range hands the bot back to `reach
melee`, which undoes the sidestep, and with every pick in range reach is not useful from there
(`ReachTargetAction::isUseful`), so no movement guard is needed. No fallback either: a rejected spot is one
the bot would be gazed on anyway, so it stands and fights through it. `Anchor()` is virtual and returns the
room middle, since the base sweep was centred on the boss platform, 93 yd above. Untreated, the skull was
14% of everything a pull lost.

**A human's Flee portal click that does nothing never reached the server.** Nothing server-side gates
194625, and with `autoCloseTime` 0 `GameObject::Use` never sets `GO_FLAG_IN_USE`, so another player's use
cannot lock it. Playerbots hands each master packet to the bots after the core handler, and
`CMSG_GAMEOBJ_USE` makes every bot log `add loot`: one burst of those `act` records is one use the server
processed, and failed stands at a portal had none. The case investigated was the client addon RightClick,
which calls `MouselookStop()` on right-button release whenever a unit was hovered in the last 5 s and it
or the current target is a live enemy, so with the Brain targeted it swallowed portal clicks. The Brain's
~76 × 78 × 60 yd model box encloses all three portals but does not take the hover: the gear cursor still
shows.

### Standing at the Brain

**Walk onto a spot behind the Brain, because `set behind` cannot get there.** The Brain faces east (o = 0)
all fight with `CombatReach` 30 (display 28951), so melee range is ~32.8 yd in 3D from a floor ~28 yd
below it, and `set behind` uses that 3D distance as a floor radius, walking melee out of range and back. A
one-shot walk to the brain room middle also died on the healer's first heal, leaving `reach spell` to stop
it at the Stormwind doorway 55 yd out. `yogg-saron brain spot` (`ACTION_RAID`, walk latch) owns the walk
while `rti` is `square`, which `GoToBrainRoom` only sets: melee go to `ULDUAR_YOGG_SARON_BRAIN_MELEE_SPOT`
(1969.0, −25.4, 237.474), 12.3 yd west and 30.1 yd 3D, settled z and `PATHFIND_NORMAL` under `--nav 0x09`
from all three entrances.

**Ranged and healers stand by their room's exit portal**, not the middle: west (1948.05, −23.76, 241.785)
for Stormwind, south (1992.4, −52.8, 240.585) for Icecrown, north (1994.6, −0.1, 240.292) for the Chamber.
Door → spot → portal is 53, 62 and 71 yd against 93-106 through the middle, and the exit lead drops to its
floor. Settled z, `PATHFIND_NORMAL` from the door, to the portal and from the melee spot, all within
36.3 yd of it for heals. The room is latched per bot (`lastIllusionRoom`), and a spot is used only once a
ray from it reaches the Brain's hit sphere, cached per spot (the built navprobe predates `los`); otherwise
the middle.

**The brain team holds short of 30% while the platform has a tentacle**, the guides' line ("get him to
~32% and wait"): phase 3 lifts Shattered Illusion and leaves every tentacle alive, and pushing through cut
the last stun 15-17 s short and carried ~3M of tentacles into phase 3. `YoggSaronTickBrainHold` latches per
brain-room non-healer while Induce Madness is cast and a Crusher, Corruptor or Constrictor lives within
70 yd of Yogg: new DoTs stop at 36% (DoTs keep ticking), all damage at 32%, and the finish starts at 8 s
left or the moment the platform is clear, latched for that cast. Enforced by the resolver, its `dps target`
fallback, `DropTarget` on a held Brain, the `brain hold` / `brain dot hold` blocks and `yogg-saron held
target cast multiplier`. `YoggSaronCrusherStunHolds` drops its 35% floor while a hold is in force and the
Brain is over 31% (a human or a pet can still push it), with the lead 8 + 4 s, so platform melee keep the
stunned Crushers. If the Brain survives, the team leaves and finishes it next wave.

### The way out

**The exit lead is measured, not flat**, since every millisecond of lead is damage the Brain does not
take: `max(3 s, distance / 7 yd/s × 1.5)` at **base** run speed. A 2× lead with a 5 s floor surfaced bots
with seconds to spare, so the user cut it; from the Brain the walk out takes 1.5-2.8 s for melee and
2.9-4.2 s for the healer. Illusion Room 63988 adds 30% (`SPELL_AURA_MOD_SPEED_ALWAYS`), but bots made only
4-6 yd/s on the way out, so the boosted figure read `safe` with no time left and charmed bots.

- **`leaving` latches until the cast ends** (`YoggSaronLeavingBrainLevel`): asked fresh, the lead shrank as
  the bot walked, flipped back to `safe`, and the brain spot walk took it back.
- **It is decided only in the brain room**, since an illusion room's way out runs through it: from the
  back of the Chamber the lead read 40-43 s, bots latched at the door with 36 s left, and the Brain lived
  another wave. Brain under 30% still sends a bot out from anywhere.
- `yogg-saron exit hold cast multiplier` zeroes cast-time spells and channels while latched (a mage
  channelled Evocation 5 s, 51 yd from a portal).
- **Bladestorm and Killing Spree are cancelled at the portal** (`YoggSaronCancelAbilityLock`,
  `yogg.exitcancel`; at the Descend Into Madness click `yogg.portalcancel`) and held by the multiplier while
  latched: the ability lock makes the portal use a silent no-op
  ([pitfalls](../../engine/pitfalls.md#an-ability-lock-makes-a-portal-or-spell-click-a-silent-no-op)), and a
  warrior who left under Bladestorm was charmed.
- **The exit node wants a door gate, not priority**: alone, `YoggSaronShouldLeaveBrainLevel` hauled bots at
  a shut door half a minute early, re-issuing the same unreachable point. The brain chamber is exempt from
  the gate, because the portals are in there and the next wave's tentacles must not strand a bot that
  already made it through.

**The way out outranks the brain spot walk, and had to be told so.** Both are `MOVEMENT_FORCED`, and an exit
returning false until within 2 yd of a portal let the brain spot node take the bot back in the same tick,
the two alternating every ~100 ms until Induce Madness landed. So the brain spot trigger stands down while
`YoggSaronShouldLeaveBrainLevel` holds — cached per trigger pass, since it costs a game object search and
two nodes ask it — `move to exit portal` (`ACTION_RAID + 4`) claims its tick while still walking and uses
the portal from `INTERACTION_DISTANCE` rather than 2 yd, clears a forced hold aimed elsewhere, and releases
a stalled spline that would read `dup`. Losing the rotation for the 3-6 s walk is the trade, against a charm
that is always a death. The brain spot walk takes over another forced walk the same way
(`YoggSaronTakeOverForcedWalk`), or a bot still booked on the room walk to a far tentacle reaches the Brain
16 s late. When the exit walk gives up (below) it cycles to the next of the three permanently spawned
portals, and stands down only once all three have failed.

**Nitro Boosts on the walk out of the Chamber.** Boots enchant 3606 casts 55004 (3 min, no backfire in a
raid) for 54861, +150% run speed for 5 s: about 7.5 s saved. `yogg-saron nitro boosts` fires on `tobrain` in
the Chamber with the brain spot over 40 yd away, never once leaving, and never claims the tick. Chamber
only: rooms cycle every three 80 s waves, so boots spent on whichever room came first were rarely ready for
the Chamber, 155-175 yd from the Brain at door-open, while a Chamber every 240 s finds the 3 min cooldown
ready. No bot had ever used them: `UseTinkerAction` takes stat auras only. Also on the phase 3 station walk
over `…_NITRO_MIN_WALK` (40 yd) when ready: the brain team surfaces at (1924.7, −25.4, 328.5), 74 / 94 yd
from the melee / ranged spot; a released ranged walk boosts once its leftover dies.

## The encounter plays its own mechanics

No node in this fight cheats: `HasCheat(BotCheatMask::raid)` appears nowhere under `src/Ai/Raid/Uld/`.
Config is untouched — `raid` stays in `Playerbots.BotCheats` — so a trace header proves nothing either way
([observability](../../systems/observability.md#what-a-trace-cannot-tell-you)).

**One teleport survives, `yogg-saron fall from floor`, and it is deliberately ungated.** Its trigger fires
only when a bot is below z 300 in the boss room or below z 200 on the brain level: it has fallen out of the
world and cannot walk back. That is an unstuck for a pathing failure of ours, not a shortcut past a
mechanic, so a cheat-free raid keeps the rescue. It `NearTeleportTo`s, so the bot stays in combat
([pitfalls](../../engine/pitfalls.md#before-the-pull-and-out-of-combat)).

**Every walk carries a progress latch**, since `MoveTo` returning `ok` never says a route exists
([pitfalls](../../engine/pitfalls.md#movement-that-silently-no-ops)): `YoggSaronWalkMakingProgress`, per
bot and per (node, destination), gives up after **6 s** without closing distance (`yogg.walk`). Keyed per
destination so testing several in one tick does not wipe the others, and a gap in the asking starts a fresh
attempt, or a give-up outlives the walk that earned it. Give-up means stand down everywhere except `move to
exit portal`, where standing down is fatal (above).

Following a master is wrong in every part of this fight — the illusion rooms are exactly where bots idled
behind a human on `clean quest log`, `apply oil` and `loot roll` — so `yogg-saron stop following` removes
`FollowMasterStrategy` and nothing adds it back.

**`rti` is a room tag, so never write one as a targeting hint.** `"cross"` is Stormwind's tag: a boss-room
bot given it is teleported into Stormwind by `yogg-saron fall from floor` the moment it dips below z 300,
and is disqualified from `move to enter portal`, which requires `"skull"`. **The tick clears it on the
platform**: only `YoggSaronMoveToExitPortalAction` sets `"skull"`, after `portal->Use`, and `RtiValue` is
per-bot AI state no encounter reset touches
([it outlives the session](../../engine/pitfalls.md#state-does-not-cross-objects)), so a bot Induce Madness
teleported out, or killed below, kept its room tag and never walked for a portal again (nine of ten on one
brain team). The tick clears anything but `"skull"` or `"diamond"` whenever the bot reads `ARENA`; on the
platform it cannot be in a room, whatever the tag says. The same leak kills `WalkIntoRoom`, which stands
down on `"square"`.

## The Death Orb is not the hazard

`NPC_DEATH_ORB` (33882) is a marker parked at z 353 while the raid stands at z ≈ 326 — **27 yards overhead,
permanently** — so a proximity test on it can never fire. The hazard is `NPC_DEATH_RAY` (33881), which
falls to the floor, warns 5 s with `63882`, then runs `63883 → 63884`: **3 yd radius**, base 19999, walking
**eight 9-yard legs every 1625 ms** on a re-rolled cardinal axis. Four rays per orb, one orb every 22 s.

## `64163 Lunatic Gaze` is an aura, not a channel

Yogg puts it on himself for 4 s, ticking `64164` once a second for 5699 damage and 4 Sanity at 130 yd on
every unit with him in its own front 180°. `AttributesEx` 200 carries no `SPELL_ATTR1_IS_CHANNELED` bit, so
`GetCurrentSpell(CURRENT_CHANNELED_SPELL)` never sees it — detect it with `HasAura`.

**Facing away is the whole defence, and there is no safe side.**
`spell_yogg_saron_lunatic_gaze::FilterTargets` tests only the victim's facing (`HasInArc(M_PI, caster)`),
so standing behind Yogg buys nothing. P3 swaps the designated tank off `TankFaceStrategy`, which would
turn it back into the gaze. It is the #2 damage source of a pull.

**Only a bot with no safe heading stops for a gaze**: a node claiming every tick for all 4 s cost a
quarter of one phase 3, cast starts falling from ~34 per 4 s to under 2. `yogg.gaze` records the branch:

- **`healer`**: keeps its back to Yogg all of phase 3 and never claims the tick. Every heal has
  `FacingCasterFlags` 0, the core checks nothing else, and `PlayerbotAI::CastSpell` only turns a bot for
  `SPELL_FACING_FLAG_INFRONT`, so a healer facing away heals anyone in range and sight.
- **`angled`**: a still bot whose target is at least 65° from Yogg as seen from the bot
  (`ULDUAR_YOGG_SARON_GAZE_SAFE_SEPARATION`) turns `(135° − separation) / 2` off its target, away from
  Yogg, and keeps attacking. `set facing` leaves it alone inside ±45° (`IsFacingValue`,
  `HasInArc(M_PI_2)`), `AttackAction` and `CastSpell` inside ±60° (`CAST_ANGLE_IN_FRONT`), and the gaze
  needs Yogg inside ±90°, so both margins are `(separation − 45°) / 2`. Facing the target outright needs
  100°, which few ranged samples have.
- **`away`**: everyone else, including any moving bot since a walk faces its path, faces away and claims
  the tick.
- **`tankfree`**: a tank with a Guardian loose (`YoggSaronGuardianLoose`: holdable, victim not a tank)
  faces away but leaves the tick to guardian control. Taunt, Growl, Dark Command, Hand of Reckoning and
  Righteous Defense all have `FacingCasterFlags` 0, so it taunts with its back to Yogg. Claiming the tick
  let a fresh Guardian peeled by heal threat one-shot healers and ranged with Hand of Reckoning ready.
- **`drag`**: a beacon dragger, whose tick is left free (below).

## Phase 3: a positive phase test, and a station that is not a leash

**A phase test made only of absences is true between the phases it separates.** "Yogg alive, no Shadow
Barrier, no phase 1 Guardian" was true for the 9.5 s between the last Guardian dying and the barrier
landing, and the raid ran phase 3 positioning through the gap. The Brain (33890) is the positive fact: it
spawns with that wave and lives to the end (bad window 0.5 s, same cost). The script also forces Yogg's
health to exactly 30% when the Brain reaches 30% (`ACTION_YOGG_SARON_START_P3`).

**A fixed position with a radius is a leash, and a leash is a fence whenever the bot's target sits further
than radius + range from it.** The P3 ranged spot is 39.1 yd from Yogg against 28.5 yd of spell range and
a 15 yd radius — reachable by 4.4 yd, and **nothing else in the room is**: a last Corruptor 60.3 yd out was
fenced off every bot handed it, and the raid's damage that minute fell to a fifth. So the spot is a
**station**: `PhaseThreeStationReaches` claims a bot only while standing there costs it nothing and
releases it otherwise. Naming no entries is deliberate — a rule written as a list goes stale the first time
a new add appears. Tanks keep a 30 yd leash instead, because `yogg-saron guardian control` outranks this
node and brings them back whenever a Guardian is loose; the leash is only the way home once nothing is.

**Healers take the melee spot and no target.** A target is what the station releases a bot for, and
healers chased tentacles to 40+ yd from the tank. The resolver gives a phase 3 healer none, dropped without
interrupting its heal, and `YoggSaronPhaseThreeSpot` stations it at `ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT`:
18.5 yd straight behind Yogg, who faces west (o 3.14) all phase, outside his 14 yd knockback, 20.6 yd from
the ranged spot, where a Guardian chasing a healer's threat walks into the tank. **Kept by the user's
choice, at a known cost**: Guardians hit healers at ~41k, every Guardian leaves the tanks ~1 s after the
first pickup on heal threat, and most Guardian samples sit at 90%+ health. Unsolved.

**A healer with no target stays on the combat engine**
([why it drops](../../engine/action-selection.md#multiplier-semantics)); off it the healers had no Mana
Tide, Hymn of Hope, Divine Plea, Innervate or mana potion, and `drink` is refused in combat.
`yogg-saron healer combat engine` (`ACTION_EMERGENCY + 9`) switches a healer in combat back, and
`yogg-saron healer target hold` blocks `drop target` for one in combat with no target; out of combat it
still drops, so drinking works. **Open: both now duplicate the shared `raid healer engine`**
(`RaidHealerEngine.cpp`, on every raid map, [raids/README](../README.md#targeting-the-whole-fight)), which
does the same while any encounter is in progress, so Yogg's pair is redundant.

**The walk back from a far leftover Crusher stays, by the user's choice.** After one died 70 yd off, the
casters walked on to the station though already in reach of their Guardian, ~550k lost inside Heroism;
stopping in reach would spread the ranged round Yogg.

## Reduced Keepers

The raid frees fewer than 4 Keepers, losing that Keeper's support: each passive is +10% damage done, plus
Freya +20% healing, Mimiron +20% speed, Thorim +20% health and Titanic Storm; Freya-only rooms ran 4-9 s
slower than a three-Keeper kill. Handled cases: **Thorim only**
and **Freya only**. The bitmask lives in `PERSISTENT_DATA_WATCHERS_MASK` — preferred over Sara's
`GetData(DATA_GET_KEEPERS_COUNT)`, which gives a count only, because the mask confirms **which** Keeper. It
is a **raid choice**, with no config option.

What Thorim-only removes: no Freya means **no Sanity Wells, so Sanity (63050, 100 stacks) is a one-way
drain**; no Hodir means no Protective Gaze absorb; no Mimiron means no haste clouds and therefore a slower
kill and more total drain.

| Source | Spell | Loss | Avoidable |
|---|---|---|---|
| Psychosis | 63795 / 65301 | −9 / −12 | **No** — random target every 3.5s in P2 |
| Malady of the Mind | 63830 / 63881 | −3 | Yes |
| Brain Link | 63803 | −2 | Yes, if the pair stays within 20 yd |
| Lunatic Gaze (P2 skull) | 64168 | −2 | Yes — only players *facing* the caster |
| Lunatic Gaze (P3 Yogg) | 64164 | −4 | Yes |
| Induce Madness | 64059 | −100 | Yes — leave the brain room |

**An Immortal Guardian cannot be killed by damage at all.** `boss_yoggsaron_immortal_guardian::DamageTaken`
clamps every hit to leave it at 1 HP. The only kill path is Thorim's Titanic Storm, filtered to a target
carrying `SPELL_WEAKENED` (64162), applied under 10% health where the Empowered stack count also falls to 0,
and armed at the start of P3; its 2 s period against a 10 s spawn outpaces spawns 5:1 once the raid burns
each Guardian that low. So the raid's whole job is Weakened: one kill's 45 Guardians each cast Weakened
and 43 left the snapshots at 10% or less.

**Without Thorim nothing kills them, and it is still winnable** — real raids kill with no Keeper at all.
Guardians spawn one per 10 s with no cap and a live one never times out
(`TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5000` is a *corpse* lifetime); `SPELL_EMPOWERED` gives +180% damage per
stack at `healthPct / 10` capped at 9, so an untouched Guardian swings at ×17.2; and anything under 85%
starts Drain Life (64160, 5 s channel) — 16,665 damage and **166,650 self-heal, 31.6% of its own max
health**, every ~14.5 s on a *random* player within 40 yd with no LOS and no threat component. So the
phase is: **keep them piled and low, interrupt Drain Life, drag beacons clear, Heroism** — the guides'
plan. Area spells on the pile reach Yogg, since an area check adds the target's reach (his 30) for a
player-controlled caster only ([reach](../../engine/raid-mechanics-lessons.md#range-and-reach)).
**Freya-only is 0-light here**: the guides make it a race on Yogg (Stars: tanks pick up adds, melee on Yogg;
TrueWoW: Guardians kept low, tanked by him). Yogg 25 has 44.0M, so phase 3 is 13.2M; a Guardian 425k; full
swings ~17× a Weakened one, 30% ~6×.

**It is a race against the Guardians' healing.** Empowering Shadows and Drain Life together out-heal
everything the spawns add (~10M each per Freya-only pull), and kills stop bringing new Guardians down ~3 min
in: each low one drains every 10.5-14.5 s and kicks cap near one a second. Drain Life starts only under
85%, once per 9.5 s, and heals 8-10× the damage dealt (64160 EffectMultipleValue 10).

**Melee stay on Yogg, by the user's choice**: a Guardian only during a gaze (outside its arc, above
Weakened) or beacon-healed (64486 / 64468, any caster) over the floor on a tank, within 20 yd of the melee
spot and past the threat gate. Area damage still reaches the pile; taking every tanked one put four times
more into Guardians than Yogg, since the pile always has one.

**Ranged take Guardians only down to 30%, the rest on Yogg, by the user's choice**
(`ULDUAR_YOGG_SARON_PHASE_3_GUARDIAN_FLOOR_PCT`): bringing each to Weakened spent most of the raid's damage
on Guardians and Yogg barely moved. Under the floor a Guardian is allowed only while Lunatic Gaze (64163)
is up and it sits 65° or more off Yogg, the arc the gaze node angles for; with Thorim the 10% rule stays.
**A beacon-healed one (64486 / 64468) is fair game, by the user's choice**, though most of the damage heals
back: skipped, three full ones sat on the tanks for the 20 s heal and killed a tank. Pets on the Guardian
left are stopped so they follow the owner.

### Shadow Beacon

**Shadow Beacon heals the marked Guardians, and Yogg when they stand near him.** It marks 3 Guardians in
25-man; 10 s later each casts Empowering Shadows (64486): **37,500 a second for 20 s on every ally within
20 yd, 750,000 — 176% of a Guardian's own max health**. Allies include tentacles, and it has no dispel type.
`spell_yogg_saron_shadow_beacon_aura::OnApply` swaps the marked Guardian's entry to 36064, a free signal:
`TierOf` puts Marked above unmarked so the raid reaches Weakened before the heal lands (with Thorim). The
area check is centre distance against 20 with no combat reach added for a creature caster, and Empowered's
+50% scale per stack lets a full-health Guardian swing from a median 9.6 yd (max 14.0). With the tank at the
P3 melee spot, 19.0 yd from Yogg in 3D, marked Guardians standing 11-16 yd from him healed him **+3.84%**
over the 20 s. Only a tank spot ~34 yd out keeps them clear, and melee cannot reach Yogg from there, so the
spot stays: with Thorim marked Guardians die first, without him they are dragged.

**Timing**: `ACTION_YOGG_SARON_START_P3` schedules `EVENT_YS_SHADOW_BEACON` 45 s into phase 3; it re-tries
every 5 s until it lands, and a Guardian it hits reschedules the next 45 s out
(`ACTION_YOGG_SARON_SHADOW_BEACON`, `boss_yoggsaron.cpp`).

- **Marked Guardians are not dps targets** (`IsAllowedTarget`): the heal resets them whatever the raid
  does. With Thorim they stay tier 1.
- **`yogg-saron beacon drag`** (`ACTION_RAID + 4`, over guardian control). The heal is one application:
  the 10 s mark ends, the entry reverts to 33988 and Yogg's 64467 missile (30 yd/s) sets it off 0.3-1.1 s
  later on every ally within 20 yd of the Guardian, and whatever stands in reach then keeps it 20 s. The
  pile tank can't separate a marked one (the pile follows whoever holds it), so a second bot taunts it off
  and walks it to a lane of its own: north (1983.68, 13.27, 327.472) or south (1993.62, −62.23, 327.625),
  navprobe `PATHFIND_NORMAL` 39.3 / 39.8 yd from the melee spot, 39.0 from Yogg, 47.6 / 50.1 from the
  ranged spot, 28-30 from the nearest well, in heal range of the melee spot; or north-east (2017.05, 5.26,
  328.781), `PATHFIND_NORMAL` 33.7 yd from the melee spot, 48 from Yogg, 24 from the ranged spot, 34 from
  north. **Each claim takes the free lane whose nearest unmarked Guardian is farthest**, north and south
  before north-east on a tie (its path leaves the pile 55° off north's), then the nearer (`beaconDrags`'
  lane, `yogg.beaconspot`): 64486 stacks per caster (`SPELL_ATTR3_DOT_STACKING_RULE`), so marks dragged to
  one latched side healed each other, up to 112.5k/s each. West and south-west were rejected (53 yd walks
  round Yogg), east (2028.3, −25.6) sat 11.5 yd from the ranged spot, south-east (2010.2, −50.7) 20.2 from
  south; closer 27 yd spots let three stacked heals reach the tank's own Guardians.
  - **Claims** (`beaconDrags`): bot tanks while another tank lives, then non-heal paladins and Death
    Knights, then hunters, then feral druids, by GUID, taunt off cooldown, **never a bot an unmarked
    Guardian is hitting** — that one would follow it out. It attacks only once standing, since `Attack`'s
    facing turn ends a walk; it releases on the heal aura or 3 s after the mark ends. A claimed Guardian
    isn't holdable, so guardian control leaves it, and the gaze node leaves a dragger's tick free.
- **A taunt that didn't take hands the mark on** (`tauntMs`, `missed`): the victim is not the dragger 1.5 s
  after its taunt (`…_BEACON_TAUNT_CHECK_MS`; Distracting Shot flies 40 yd/s, all land by 1 s) and the
  claim goes to the next bot with a taunt ready, rather than sitting out an 8 s cooldown after a `MISS`.
  Not under a Drain Life already running when the taunt landed: the Guardian's `UpdateAI` returns while
  casting, before `UpdateVictim`; a channel started later doesn't hide the switch. Death Grip is impossible
  (immunity set −286: GRIP, PULL_TOWARDS, ROOT, SNARE).
- **Draggers are pre-staged, by the user's choice** (`YoggSaronTickBeacon`, `yogg-saron beacon stage`,
  `ACTION_RAID + 3.5`). 5 s before the next beacon is due (off the last first mark) the claim's rules and
  order pick `RAID_MODE(1, 3)` stagers, lanes north / south / north-east in order; the claim gives a stager
  its lane while free. Stagers walk round the body and hold, no swing, until the first mark, or drop 8 s
  past due. Spots 20 yd along each lane's `PATHFIND_NORMAL` straight line from the melee spot, floor by
  navprobe with Z unchanged: north (1990.94, −4.40, 325.104), south (1996.06, −42.75, 324.889), north-east
  (2009.52, −6.19, 326.989); hunters wait at their hunter spot. A 30 yd taunt still reaches the pile, and
  the walk after it drops from ~39 to ~19 yd.
- **Healed Guardians get a cooldown first.** One still marked or carrying 64486 / 64468 at 40% or more
  (`…_BEACON_DANGER_HEALTH_PCT`, ~17k a hit) on a dragger or a non-tank: the victim presses
  `NextPhysicalDefensive` (`ACTION_EMERGENCY + 6`, `yogg.dragdef`); a healer or paladin gives Pain
  Suppression, Guardian Spirit or Hand of Sacrifice to one without a button of its own, draggers first,
  humans included, one per victim per 8 s (`ACTION_EMERGENCY + 5`, `yogg.dragext`). Divine Shield and Ice
  Block are blocked on such a victim (`yogg-saron beacon immunity hold`): an immune victim sheds aggro onto
  somebody else at full power ([RML](../../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)).
  After the release the dragger walks back to the melee spot until a tank has it or 8 s pass (`return`),
  since the tank is ~39 yd away and a taunt reaches 30, and guardian control takes it back first (below).
  - **A tank holding the pile is covered too**: its own button the moment 2+ Guardians on it carry a 64486
    / 64468 application another Guardian cast (`YoggSaronPileHealedOn`, `…_BEACON_PILE_RESET_GUARDIANS`, at
    any health, by the user's choice), an external under 50% (`…_BEACON_TANK_EXTERNAL_PCT`), draggers still
    first. A human tank is called on the same pile, and also at 3+ Guardians under 50% (the swap line),
    since he has no class trigger. A returned mark carries only its own copy, so only a heal reaching the
    pile counts. A 60% line spent Divine Protection (2 min, beacons 45 s apart) on a mild first beacon and
    left none for a failed drag.
  - The external skips a bot whose own button `ReadyTankDefensive` names, which counts the Avenging Wrath
    marker (61987, 30 s) as a lock on Divine Protection like Forbearance, since the immunity script refuses
    both (`spell_pal_immunities`). Tanks give no external here, and `hand of sacrifice on party` is blocked
    for tanks in phase 3 (`yogg-saron tank sacrifice hold`): it moved 30% of a dps's damage onto a tank
    under the pile.
- **Hunters drag only marks nobody else can**, the Stars method, on their claim's lane: spots north
  (1986.0, 7.63, 326.797) and south (1994.44, −55.65, 326.394), north-east its own drag spot, navprobe
  `PATHFIND_NORMAL` 33.2 / 33.1 yd from the melee spot, 42 / 44 from the ranged spot, inside Distracting
  Shot's 35 yd (6 s taunt, 8 s cooldown, no Guardian immunity): shot with ≤5 s of the mark left, so it covers
  the heal. It must be late: 20736 is effect 63, a fixed threat for 6 s, where Taunt, Hand of Reckoning and
  Dark Command are effect 114, which `Spell::EffectTaunt` matches to the top threat for good. No damage on it
  before then (the multiplier; Feign Death and Deterrence blocked by `yogg-saron beacon hunter hold`), and
  Feign Death if it is still on the hunter 1 s into the heal with the taunt gone (`shot` / `feign`). A
  human tank taunting a mark off a hunter before the heal leaves it in the pile.
- **The tank takes a healed Guardian off a paladin, DK or druid dragger first**
  (`YoggSaronLooseGuardianTier`): tier 0 one still on its dragger within 8 s of the release, 1 the rest, 2
  one under its hunter's Distracting Shot, which goes back by itself; then highest health, then nearest
  (`yogg.tankhold` `return taunt` / `return defense`). Only a tank's taunt frees an effect 114 dragger, and
  sorted by health alone fresh spawns near the pile won: paladin and DK draggers waited ~8 s for a tank
  against ~2 s for hunters, and draggers died to their healed Guardian. Rebirth has no encounter hook in
  phase 3.
- **A feral druid drags last, in a bear form, by the user's choice**: after the hunters, a bot druid neither
  heal nor ranged, in a bear form or with the mana for one (`YoggSaronBearReady`: the bot's spell check
  skips cost). Staged, dragging or returning (`YoggSaronBeaconBearHeld`), it drops its form and casts Dire
  Bear Form, else Bear Form (`yogg-saron beacon bear form`, `ACTION_RAID + 4.5`): neither casts while
  shapeshifted. `cat form` / `caster form` are blocked meanwhile (`yogg-saron beacon bear hold`); the cat
  strategy shifts it back after. Growl (6795) needs the form and has no GCD; until the shift is in it fails
  and holds the drag. `IsTank` reads the strategy, so its Guardian stays loose, tier 0.

**A leftover tentacle a mark's heal reaches dies first**, or Empowering Shadows heals it back (2M into one
Corruptor over three beacons). `YoggSaronBeaconHealReaches`: within 20 yd + 5 yd slack + its size of a drag
or hunter spot. Without Thorim, phase 3 ranged rank such a Corruptor or Constrictor tier 0 (`beaconHealFirst`;
~7 s for a 745k Corruptor, inside the first 45 s) where their station reaches it, and the pets take it
first.

### Phase 3 cooldowns and casts

- **Deafening Roar is covered by an Aura Mastery chain** (25-man, fewer than 4 Keepers: 30 s in, then every
  60 s, 2.3 s cast, 4 s silence within 50 yd; uncovered it silences ~20 and halves healing). 64364 reaches
  only raiders carrying the presser's own Concentration Aura
  (`spell_pal_aura_mastery_immune::CheckAreaTarget`), and each paladin's aura applies separately
  (`Aura::CanStackWith`). `YoggSaronTickRoar` latches `roarExpected` and the cast start;
  `YoggSaronRoarPaladin` picks the bot paladin with Aura Mastery ready by the roar, not dragging, its own
  Concentration up, then GUID, so the paladins alternate. It switches to Concentration 10 s ahead
  (`ACTION_RAID + 1`, other auras blocked by `yogg-saron roar aura hold`) and presses during the cast
  (`ACTION_EMERGENCY + 8`, one per roar, others after 500 ms, cast directly past `yogg-saron roar aura
  mastery hold`, which keeps the class nodes off Aura Mastery meanwhile). Both read the paladin's own
  Concentration by its application (`HasAura(19746, guid)`), since read by name an effectless copy is
  invisible ([pitfalls](../../engine/pitfalls.md#detection-that-never-fires)). The roar's start is re-dated
  only forward: each paladin back-dates it off Yogg's cast bar a few ms apart, and an earlier start wrapped
  `getMSTimeDiff`, re-keyed the claim, and spent a second paladin on roar 1.
- **No non-tank spell on a Guardian the threat gate refuses** (`YoggSaronHeldTargetCastMultiplier`, kicks
  exempt): the "on attacker" DoTs skip the resolver, and a held fresh Guardian DoTed by warlocks one-shot
  both with the tanks 56+ yd away.
- **The bot tank skips a Guardian already carrying another tank's taunt, or its own** not yet turned, as
  under Drain Life (`SPELL_AURA_MOD_TAUNT`), or both tanks taunt the same fresh one together.
- **Area spells need three units worth hitting, by the user's choice** (`YoggSaronPhase3AreaMultiplier`).
  `AoeTrigger` counts any attacker within 8 yd of the target, distance minus the unit's size, so Yogg counts
  from 38 yd, and a lone Guardian got Seed of Corruption and Volley that hit each target for less than a
  single-target spell. Held: blizzard, flamestrike, blast wave, dragon's breath, cone of cold, hurricane,
  typhoon, volley, mind sear, rain of fire, both seed of corruption actions, fan of knives, swipe (cat),
  death and decay, blood boil, cleave, fire nova, magma totem; each needs `…_PHASE_3_AREA_MIN_WORTH` (Yogg,
  leftover tentacles, Guardians over the floor a tank holds; with Thorim over 10%, marked ones too) within
  8 yd of its target, Mind Sear not counting its own, and no Guardian there failing the threat gate
  (`yogg.p3area` `few` / `fresh` / `allowed`). Divine Storm, Whirlwind, Chain Lightning, Starfall, Blade
  Flurry, Sweeping Strikes, Multi-Shot and Howling Blast hit the main target in full and stay.
- **Drain Life is kicked on purpose.** The interrupt node's casters include Immortal Guardians channelling
  64159/64160 in phase 3; immunity set −286 has SILENCE but not INTERRUPT, so Strangulate, Silencing Shot
  and Arcane Torrent are dropped for them, and Avenger's Shield is off the list because it picks its own
  target. `interruptClaims` holds a cast for one kicker for 1.5 s, which took cuts inside 1.5 s from half to
  all. Spell Lock is on the list (the felhunter's spell, cast by the bot, so cooldown and range are the
  bot's), and trigger and action ask `YoggSaronInterruptReady` (cooldown, power, stance, range):
  `CanCastSpell` passes a pet spell on cooldown and counts out of range as castable.
  - **A bot mid-cast runs no kick node**
    ([pitfalls](../../engine/pitfalls.md#a-bot-mid-cast-cannot-be-moved-at-all)), so **as the last resort
    only, by the user's choice**, `YoggSaronNudgeDrainLifeKicker` (the tick, one scan per 200 ms per
    instance) stops one bot's cast: a channel 1.5 s in with no claim, no free bot with a kick ready in range,
    none finishing its cast within 1 s; never a healer; shortest kick cooldown first, then the longest cast
    left; one per channel (`yogg.kicknudge`).
  - **Marks' channels go first**: a channelling Guardian doesn't move, so it keeps a hunter's mark in the
    pile. Casters marked or dragged lead the list and skip the nudge's 1.5 s wait. **Kicks are saved for
    them, by the user's choice**: from the first mark until no mark or drag is left, `markKickers` holds
    `…_MARK_KICKERS` (2) bots not dragging or staging with a kick ready within 2 s, longest reach first
    (Counterspell / Spell Lock 30 yd reach the pile and the north-east lane from the ranged station, Wind
    Shear 25), then shorter cooldown, non-healer, GUID; they kick only marks then and the nudge skips them
    for others (`yogg.markkick` `reserve` / `kick`).
  - **Kicks run on cooldown, not reaction**: a ready bot kicks a median 140 ms in, so each saves only the
    ticks its channel has left (1/s, the first at 1 s), and the casters list Drain Life **most time left
    first, none under 1 s left** (`…_KICK_MIN_LEFT_MS`; the slot offset stays for Dark Volley). **Only once
    the channel is CASTING** (`YoggSaronDrainLifeKickable`): a channel has no cast time and
    `Spell::EffectInterruptCast` skips a zero-cast-time spell still PREPARING, so a kick in its first update
    does nothing (a sixth of all kicks were).
  - **Kicks are at capacity**: a kicker ready, alive and in range all through a full channel with no kick
    inside it is rare (1-9 a pull).
- **Mortal Strike halves both heals** (aura 118, −50%; only the strongest cut counts in
  `SpellHealingBonusTaken`). **So `yogg-saron mortal strike`** (`ACTION_RAID − 0.5`, by the user's choice,
  Wound Poison declined): a warrior with it ready and the rage (`CanCastSpell` ignores rage) strikes a
  Guardian in melee reach with no cut, threat allowed, outside a gaze — one carrying the heal first, then one
  channelling, lowest health first, then any under 85% (`low`: it drains every 10.5-14.5 s, mostly inside the
  10 s cut). Never Yogg, which otherwise takes every strike. `yogg.mortalstrike` (`hot` / `drain` / `low`)
  and `yogg.healcut` (`hot a/b drain c/d`: Guardians under a cut out of those healing, sampled once a second
  raid-wide).
- **A warlock gets its demon back in phase 3** (`yogg-saron warlock pet`, `ACTION_RAID − 0.5`): pets are
  summoned only out of combat, so a warlock back from a phase 2 death had no felhunter and no Spell Lock.
  With no live pet it casts Fel Domination if it has it, then the summon its `nc` pet strategy names
  (`YoggSaronWarlockSummon`).
- **`yogg-saron phase 3 lust`** (`ACTION_RAID + 2`). `ShamanBoostTrigger` and the burst hold both want a
  boss target, and phase 3 shamans are on Guardians and tentacles, so lust came 40-54 s in or never. It
  casts once 80% of the living group stands at its phase 3 station (15 yd) or still in reach of a live
  target — nobody below z 300, Heroism reaches 100 yd — or 30 s in; not while Sated or Exhausted, nor with
  Heroism or Bloodlust already up (`yogg.lust`, `yogg.lustready` `position` / `timeout`). The stock path
  cast too early, Yogg being the only target until a Guardian spawns, so `UlduarBurstWindowMultiplier`'s
  Yogg branch holds lust the whole fight and the node owns it, casting by `CastSpell` under its own name.

### The tanks in phase 3

**A bot tank holds the pile**: with both tanks bots, Guardians stayed on a tank until the beacon with no
hits on ranged or healers. The ceiling there: Yogg lost 2.5% a minute, ~13 minutes for his last 30%.

**The bot tank has to hit the Guardians, not only taunt them.** `FindTankTargetSmartStrategy` ranks by not
attacking the tank, then nearest by `GetDistance`, which takes off Yogg's 30 yd combat reach: he always
reads nearest and never attacks anyone, so a bot tank held Yogg all phase and each taunt wore off with
nothing behind it. So in phase 3 `yogg-saron guardian control` (`ACTION_RAID + 3`), in order:

0. Takes or sheds the pile in a tank swap (below).
1. Taunts the nearest loose Guardian (not Weakened, victim not a tank). Failing Hand of Reckoning, a paladin
   casts Righteous Defense on that Guardian's victim: 40 yd to the ally, and its dummy effect
   (`SpellEffects.cpp`) casts 31790, range 50,000 yd, triggered on up to 3 of the ally's attackers, so it
   reaches one still walking in.
2. Walks the 5 yd leash home, round the body.
3. `Attack`s the Guardian its lead (`YoggSaronTankThreatLead`, over the highest non-tank, pets included) is
   thinnest on, in melee reach before merely near the spot, leaving its current one only past
   `ULDUAR_YOGG_SARON_TANK_THREAT_SWITCH_LEAD` (1.5).

`yogg-saron dps target guard multiplier` zeroes `TankAssist` for tanks in phase 3 while a Guardian that can
still be held lives within 70 yd, or the picker hands Yogg straight back. `yogg.tankhold`: `taunt` /
`defense` / `leash` / `attack`.

`yogg-saron guardian control` is **not** gated on Thorim, or on hard mode. Hard mode means *fewer* Keepers,
so demanding both asked for exactly the case where Thorim is least likely to be there — and Guardians parked
on a tank beat Guardians loose among the casters even where none of them can die.

**One pile, swapped on danger, by the user's choice** (two piles rejected). Both tanks share the melee spot,
so Consecration kept the pile on whoever led threat. When the holder drops under 50% (`…_TANK_SWAP_PCT`)
with 3+ Guardians, more than the other tank has, a tank at 70%+ not dragging takes the pile
(`YoggSaronTankSwapTake`): Righteous Defense on the holder, then a taunt on its highest Guardian, before the
loose loop. For 8 s (`…_TANK_SWAP_MS`, one cooldown) the holder sheds: consecration, hammer of the
righteous, avenger's shield, hand of reckoning and righteous defense blocked (`yogg-saron tank swap hold`),
loose loop skipped; Holy Shield and Shield of Righteousness stay. It ends early on the holder's death or the
taker under 50%. A tank under 50% also leaves loose Guardians to a tank at 70%+ (`YoggSaronTankLowHold`), or
a soulstoned tank taunts straight back in and dies again. It moves the whole pile, so it buys a fresh bar
and the holder's cooldown, not a lighter load. `yogg.tankswap` (`take`), `yogg.tankshed`, `yogg.tankhold`
`swap defense` / `swap taunt`.

**Lay on Hands is kept for the tanks, by the user's choice**: pulls spent all four on dps in phase 1 and had
none for phase 3, where tank deaths ended them. Blocked (`yogg-saron lay on hands hold`): `lay on hands on
party` all fight, `lay on hands` for non-tanks all fight and for tanks before phase 3. In phase 3
`yogg-saron tank lay on hands` (`ACTION_EMERGENCY + 4`, under the beacon pair) casts it directly on the
lowest tank under 20% within 40 yd, humans and itself included, one paladin per tank per 3 s
(`yogg.tankloh`). Watch phase 1 Shadow Nova deaths for the cost. **A tank keeps Avenging Wrath in phase 3**
while Divine Protection is ready within 30 s (`yogg-saron tank wrath hold`), since its marker locks it.

**Open risks.** Whether one bot tank keeps pace with a Guardian every 10 s alone is untested, since that kill
had a human second tank; and the sanity-conservation behaviour (`ACTION_RAID + 5`: stand behind Yogg facing
away below 15 stacks) **nearly benches a bot** — Sanity never recovers Thorim-only, so a bot that drops to
15 stays there.

## Squeeze breaks on immunity

Removing the Squeeze aura (64125 / 64126) kills the Constrictor Tentacle and drops the passenger, so
`yogg-saron squeeze escape` at `ACTION_RAID + 1` has a grabbed mage cast Ice Block and a paladin cast Divine
Shield. Hunter Feign Death and rogue Vanish are deliberately not used — neither removes a periodic damage
aura.

**A Constrictor holding somebody outranks the Crusher** (`YoggSaronConstrictorHolding`: its vehicle seat is
in use). 99k health dies in about 2 s under the ranged, where ranking it under the Crusher left raiders
squeezed for 11-23 s.

**A paladin can do it for somebody else with Hand of Protection**, which covers the other eight classes.
10278 grants `SPELL_AURA_SCHOOL_IMMUNITY` over school mask 1 and carries `SPELL_ATTR1_IMMUNITY_PURGES_EFFECT`,
so `HandleAuraModSchoolImmunity` strips every non-positive physical aura the immunity would cover; Squeeze
is physical, not passive and lacks `SPELL_ATTR0_NO_IMMUNITIES`, so `CanDispelAura` passes and the tentacle
dies on removal. `yogg-saron squeeze rescue` at `ACTION_RAID + 6` — above the sanity conservation retreat,
since cutting somebody out of 7.5k a second beats walking somebody else to a well — takes the lowest-health
victim inside the spell's own **30 yd**, skips one carrying Forbearance (25771, matched by id), and claims
that victim for `ULDUAR_YOGG_SARON_SQUEEZE_CLAIM_MS` so three paladins do not spend three cooldowns on one
tentacle. Squeeze costs no Sanity — `spell_yogg_saron_sanity_reduce` has no case for it — so this is damage
and healer load only (~0.5M a pull).

**The main tank is rescued like anyone else.** He rides a vehicle and holds nothing while he is held, so the
threat wipe costs less than the grip; Forbearance locking out his own Divine Shield and Lay on Hands for two
minutes is the accepted price.

`CastBlessingOfProtectionProtectAction` never casts, its spell renamed Hand of Protection in 3.0
([classes/paladin.md](../../classes/paladin.md)), and is left alone: reviving it changes paladin behaviour in every instance, and it could not serve this
anyway, since `PartyMemberToProtect`
only returns a non-tank under 30% health, and a squeezed bot is near full.

## What it costs

Per-raid cost shapes, measurement method and the settled cross-raid negatives are in
[raid-mechanics-lessons](../../engine/raid-mechanics-lessons.md#what-a-strategy-costs-per-raid), which also
holds the open walk-latch growth in `YoggSaronBodyDetourAction`. The encounter gate is open whenever no
other boss is engaged and Yogg is alive, so every Yogg trigger runs for every bot anywhere in Ulduar between
pulls.

**In:** the instance-script lookup for Sara, Yogg and the Brain, the target-exclusion gate, the phase-first
reorders, and the entry-first Immortal Guardian list. Their DB premises hold in the core's base SQL: Sara
is the only one of the three with a spawn, and data type 12 (`BOSS_YOGGSARON`) is not reused, `UlduarData`
starting at 14. **Unverified live**: they passed the syntax check, pblint and codestyle only; the shadow
comparison (old sweep against new lookup for the three creatures and the Guardian list, old exclusion set
against new) and `pmon` before and after were never run. Not done: skipping the whole exclusion gather on
an empty candidate list, since other strategies' exclusion hooks were not checked for side effects.

**Open**, each behaviour-neutral and independent, ranked by counted work since nothing was timed:

- **Evaluate each resolver candidate once.** `IsAllowedTarget` ends in a line-of-sight ray and runs twice
  for the current target every tick in the steady state (`ResolveTarget` / `SpreadTarget`, and again for
  every tentacle `SpreadTarget` rejected). Memo `(Unit*, allowed)` in `BrainApproach`, which already lives
  for one `ResolveTarget` call; nothing between the two evaluations changes the world. Saves a ray plus the
  threat and aura reads per dps bot per tick.
- **One "does any rule care" lookup per action per boss** (framework). Each `EncounterRuleMultiplier` /
  `EncounterGatedMultiplier` (`RaidEncounter.cpp`) owns a `CaresCache`, an `unordered_map<Action*, bool>`,
  and does a find for every action that passes `isUseful`; Ulduar installs 139 of them (38 Yogg's), nearly
  every answer "no". One shared index per bot per definition instead, `Action* → uint64` mask of the rules
  that care (Yogg 38, Algalon 26), with a one-entry last-action memo, built in
  `EncounterDefinition::AddMultipliers` and held by `shared_ptr`; each rule stays its own `Multiplier` and
  tests its bit. Identical because `RuleCares` and the family mask depend only on the action; names, order,
  veto credit and perf buckets are untouched. 139 hash finds per popped action become 14, on every Ulduar
  boss; `tools/nativetest/raid_encounter_rules_test.cpp` covers the rules.
- **Answer once per trigger pass what triggers ask twice.** `YoggSaronPortalPlan` is asked by the
  enter-portal trigger, up to three times by the Sanity trigger and by Brain Link, each phase 2 call a
  100 yd portal search plus state writes: memo `{intent, spot}` in `YoggSaronPassReads` (a repeat inside a
  pass rewrites nothing — the wave debounce is in seconds and the slot rebuild is keyed on the wave).
  `YoggSaronPhase3PositioningTrigger` builds a `YoggSaronSanityTrigger` and calls it, so the Sanity body and
  its walk latch run twice a pass in phase 3: memo the node's answer. `YoggSaronRoomStateOf` sweeps the room
  for tentacles and `YoggSaronBrainRoomApproachable` sweeps it again: split the door test out.
- **Cache the three illusion door guids per instance.** `YoggSaronNearestDoor` is cached only inside a
  trigger pass; outside one (resolver, room state) it is up to three 200 yd game-object sweeps per
  `YoggSaronBrainRoomApproachable` call. Store each guid in `YoggSaronEncounterState` on the first sweep that
  finds it, then resolve by guid with the sweep's own test, the same `|dx| + |dy| < 200` rule and the same
  sweep fallback as `YoggSaronFindNearestCreature`; never cache a miss. Premise: one spawn each of
  194635-194637 on map 603 — true in the core's base SQL, not checked against the live world DB.
- **Small ones.** Objects built every tick to read a constant or call a helper: `FollowMasterStrategy`,
  `TankFaceStrategy` / `TankAssistStrategy` and `TooCloseToPlayerWithDebuffTrigger` in the triggers (every
  bot in phase 2), the strategies again in the actions, and a `YoggSaronTrigger` per Constrictor or Corruptor
  candidate in the resolver; use the name, a member or a free function. `YoggSaronCanInterrupt` builds a
  vector to test emptiness, for every bot every tick. Loop-invariant work: `YoggSaronHandoverState` +
  `YoggSaronBotTankAlive` (a group walk) per parked Guardian in `YoggSaronPhase1Focus`,
  `YoggSaronGuardianDiedWithin` per member, `YoggSaronCrusherStunHolds` per leftover × crusher. `"rti"` read
  several times in `YoggSaronIllusionRoomTrigger::IsActive`; `std::string` temporaries in `YoggSaronKickRank`
  and `YoggSaronInterruptSpellsFor`.

**Looked at and left alone:**

- A timed or per-tick phase cache outside triggers: an action can move the phase mid-tick, and the
  instance-script lookup makes it pointless anyway.
- Gating the Dark Volley trigger, the exclusion sweep or the nova reads on boss state: Guardians spawn from
  clouds before the pull (`boss_yoggsaron_cloud::UpdateAI`).
- Merging the two Immortal Guardian sweeps in `YoggSaronInterruptCastersAround`: it reorders the casters,
  and the kick action takes the first it can reach.
- `"find target" yogg-saron` in the sanity retreat, and the `"nearest npcs"` semantics beyond the Guardian
  list: the value's own filters are part of the answer.
- Holding the `YoggSaronStateFor` reference across a pass (51 call sites, a mutex each): entries can be
  erased, and the gain is a few microseconds.
- Trigger `checkInterval` throttling: it changes reaction time.

## Reading a trace

`tools/botobs/bosses/yogg_saron.py` (views per `--help`) also names any key missing from the whole trace: a
key declared in source and absent from every trace of its own boss means the recorder is dropping it, not
that the thing never happened. Two hazards go to the timeline only because nothing can sweep for either:
the body's knockback circle, and each Crusher's wedge carrying facing, arc and range so it can be tested by
hand.

**69 `yogg.` probe keys**, by topic:

| Topic | Keys (values) |
|---|---|
| Phase and pull | `phase`, `engaged` (`engaged` / `idle`), `handover`, `leftover` (`spaced` / `chain` / `lower`) |
| Phase 1 | `p1dodge`, `p1station`, `p1leash` (`returning` / `held`), `p1focus` (above), `p1taunt` (`counts` / `beyond`: whether the fetched Guardian's death would still count for Sara), `cloudreach`, `novaraid`, `novadef`, `novaext`, `humancall` |
| Platform hazards | `knockback` (`inside` / `clear`), `crush` (incl. `reach`), `deathray`, `detour` (`around` / `spot`), `stunned` (`engage` / `clear`), `petguard` (`pulled` / `held` / `clear` / `leftover`), `judgement`, `walk` (node and branch), `walkstep` |
| Portals and Sanity | `wave`, `portal` (`notteam` / `waiting` / `spreading` / `holding` / `late` / `clicking`), `portalslot`, `brainteam`, `benched`, `portalcancel`, `brainlink` (`none` / `closing` / `clear`), `sanity` (`low` / `topping` / `refill` / `idle`: the walk's reason, not a level), `squeeze` |
| Brain level | `room`, `roomstate` (`walkingin` / `fighting` / `doorshut` / `tobrain` / `atbrain` / `staged` / `reprisal`), `tentacle`, `spread` (`free` / `shared` / `kept`), `share` (group name / `orphan` / `first` / `helper` / `done` / `staged` / `lifted`), `reprisal`, `skull`, `skullturn` (`angled` / `free`), `brainspot`, `brainhold` (`open` / `dots` / `hold` / `finish` / `clear`), `charge`, `dash` (`used` / `cooldown`), `nitro` (`used` / `cooldown` / `station`), `exit`, `exitcancel` |
| Phase 3 | `station` (`walking` / `shifted` / `parked` / `released` / `unreachable`), `gaze` (above), `tankhold` (`taunt` / `defense` / `leash` / `attack` / `return taunt` / `return defense` / `swap taunt` / `swap defense`), `tankswap`, `tankshed`, `tankloh`, `beacondrag` (`claim` / `taunt` / `missed` / `hold` / `shot` / `feign` / `release` / `return`), `beaconspot`, `beaconstage` (`pick` / `walk` / `ready` / `stuck` / `timeout`), `beaconbear`, `dragdef`, `dragext`, `roar` (`preswitch` / `noconc` / `claimed` / `press`), `p3area` (`few` / `fresh` / `allowed`), `kicknudge`, `markkick`, `mortalstrike`, `healcut`, `warlockpet`, `lust`, `lustready`, `healerengine` |

**Do not add a Sanity level probe.** 63050 is already in the aura stream, with 63752 low-sanity and 63120
Insane beside it, as are Grim Reprisal 64039 and Lunatic Gaze 64168 in `dmg`. A removal row carries no stack
count, so skip `r:1` rows when taking a minimum or every bot reads as Insane.

**What the reader infers, because no row records it:**

- A portal taken shows only as the bot's own Illusion Room cast (63988): the portal is never sampled and a
  failed click leaves nothing.
- Induce Madness leaves no damage or aura row, so the Insane aura is its only record and `SANITY_COSTS`
  leaves it out.
- The trace has no DmgClass, so a ranged bot's melee hits on a Crusher are matched by name
  (`MELEE_CLASS_NAMES`).
- The exit walk uses its portal at `INTERACTION_DISTANCE`, the 5 yd that also counts as arrived, so a clean
  exit often leaves no `yogg.walk` row; `--brain`'s `caught by it` measures that walk instead, with the
  seconds under Bladestorm / Killing Spree since the decision.
- A Guardian's spawn is a Simple Teleport (64195) within 3 s of its first sample, else that sample; every
  Guardian casts it again at the wipe.
- A Weakened Guardian's 1 HP reads 0%, so presence is a fresh row, not health; Thorim's casts never reach
  the recorder, so a Guardian leaving at 10% or less counts as the Storm's kill.
- A removal row counts as evidence of a buff's holder, since a buff put up before the pull shows only when
  it comes off.
- A failed taunt never reaches a cast row, so taunts are read as attempts off the action rows — the failures
  are the ones that cost phase 1.
- A step-out that wakes the next Guardian leaves no other trace: the note says only why it moved.
- `--portals` telling a bot that walked for a portal and lost the race from one whose node never ran is
  the only way the room-tag leak shows.
- The damage split is by each bot's target, so a melee bot on Yogg cleaving the pile reads as all Yogg (38%
  there against 22% in the combat log). `yogg.p3area` is by centre distance, a lower bound: the module takes
  off each unit's size, several yards on an Empowered Guardian.

**Three views exist because this fight keeps failing in ways no per-mechanic view sees**: **vetoes** by
multiplier and action, since a zeroed walk with nothing walking in its place is a bot standing still and
nothing else names it; **frozen bots**, the longest a bot held a target and cast nothing; and **launches**,
read off the switch to effect movement near the body and matched to the last walk whose line crossed the
ring rather than the last walk issued, which is usually the dodge out. A Crush kill is attributed to the
facing sample, of those either side of the hit, nearest the dead bot, since the tentacle turns the moment
anybody hits it.
