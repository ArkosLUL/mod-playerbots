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
after any player comes within 90 yd, and already read by `BossStateGateLive`. The recorder writes
`pull src=bossstate` from it, so **trace t=0 is `InitFight`** — first damage is 20-30 s later and is
no pull marker. Reading it as one produced the claim that 4 of 12 Guardians beat the pull; against t=0
the first Guardian of every pull on record appears at 10.1-10.2 s, and none has ever beaten it.

`YoggSaronPhase` is the single read every node routes through — phase 2 is Yogg with Shadow Barrier,
phase 3 is Yogg without it plus the Brain, phase 1 is Sara, all behind that boss-state test, which
also spares a bot elsewhere in Ulduar two 200 yd sweeps a tick — and it is what writes `yogg.phase`.
She lives into P2/P3 at 1 health, so "Sara is alive" is no phase test by itself.
The phase read's sweeps and the other 200 yd reads — Sanity Wells, illusion doors, the tentacle check,
skulls in arc — are answered once per trigger pass through `EncounterTriggerPassId`, and live in actions
and multipliers, where an earlier action can already have moved the phase.

**Phase 1 positioning waits for the room, not the pull.** With the raid still 72-96 yd out at t=0,
every mover fired at once on 2026-09-16. Most bots took no damage until 25-53 s, and ranged running in
crossed orbits 4 and 3 as clouds passed: two Guardians by 16 s. The station, spacing, leash and
phase 1 control triggers now wait for `YoggSaronInPhase1Room` (outer orbit + reach, 69.3 yd), a
distance read ahead of the phase read. Combat is no gate: every Guardian's `Reset` calls
`SetInCombatWithZone`, so the instance is in combat from ~10 s. Nor does it belong in
`YoggSaronPhase`, whose read writes the shared `yogg.phase` latch. The gate moves *when* bots cross
the orbits, not whether. `yogg_saron.py --phases` counts phase 1 moves begun outside it: 136 and 74
before the gate, none after. It interpolates between snapshots and allows 2.5 yd, because
`GetDistance2d` takes off the bot's 1.5 yd combat reach and the move lands a tick after the read.

`IsBotMainTank` is false for **every** bot while a human holds main tank, which silently disabled the
phase-3-control node. `IsDesignatedBotTank` falls back to the first living bot tank.

## The two spells that wipe P1

| Spell | Shape | Answer |
|---|---|---|
| Shadow Nova 62714 / 65209 | instant, uninterruptible, DBC 15 yd but **16.2 measured**, on Guardian **death**, also hitting other Guardians (below) | ranged and healers stand off; melee mitigate it and hold their ground, only a Fervor holder leaves (below) |
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

**The median nova is survivable and the tail is not, so plan against the tail.** Pools run **23,387**
(enhancement shaman) and 24,812-25,492 (rogues) up to **30,549** (warrior). Across five pulls the
ordinary hit held a **17,385-17,631** median either way, but the worst of it did not: **25,025** over
515 hits with the raid's shadow resistance up, **26,692** over 137 with it down, and the unresisted roll
only ever appeared in the second case — **0 of 515** against **9 of 137**.

So `…_NOVA_EXPECTED_DAMAGE` is **28,600**: a ret paladin died from a verified full **28,582** bar on a
recorded 26,692, so 65209's two `SCHOOL_DAMAGE` effects outrun the one row the trace records per raider,
and 28,600 is the smallest figure consistent with every death. It is above the full bar of **22 of 24**
raiders — only the two warriors and the tank clear it.

The raid cannot count on resistance to bring that down. With the aura up no ordinary nova reached a full
bar, and the deaths from full health there came from **two in one tick** instead (the chain rule below);
with it down a single one did. Even the aura-up worst of **25,025** is over the smallest melee pool, so
it would have, had it landed there.

**Keep 130 shadow resistance up; losing it is what uncovers the unresisted roll.** Mean mitigation
barely moves without it (22.2% against 21.4%); what goes is the floor on the roll, and with it up a
later **759 hits over six pulls resisted a median 21-22% and never under 10%**.

Where the 130 comes from barely matters, which is the part worth knowing. In `Spell.dbc` Shadow
Resistance Aura 48943 and Prayer of Shadow Protection 48170 both carry EffectBasePoints **129**, so 130,
both on aura **143** `MOD_RESISTANCE_EXCLUSIVE` (48169 too), and `Unit.cpp` reads them with
`GetMaxPositiveAuraModifierByMiscMask` — **the highest, never the sum**. So the priest buff covers the
raid on its own, and the exclusive aura slot is worth spending only where something can beat 130.

**Aura Mastery 31821 is the only thing that does, so the aura goes to the paladin that has it.** It is
aura **108** `ADD_PCT_MODIFIER` on all three effects with 99 base points, so **+100% for 6 s**: 130
becomes **260**, which is the one state in the fight above the Prayer's floor. Left to the class logic
the two never meet — over six pulls the aura landed on the bot protection tank in 2 of them and it cast
**Devotion Aura** back inside the same pull both times, while the holy paladin pressed Aura Mastery once
a pull holding a different aura, off its own critical-heal node and so after the hit. One earlier pull
ran three paladins and the ret picked **Retribution Aura 54043** over 48943 on the same reasoning gap.

So `IsYoggSaronShadowAuraPaladin` elects the lowest-GUID **bot** paladin that knows Aura Mastery and can
cast a resistance aura (a human's aura cannot be doubled by a bot), `yogg-saron shadow aura action` puts
it up, and `yogg-saron shadow aura hold` blocks every other aura on that bot — the slot is exclusive, so
its own buff strategy trades it back on the next GCD. `yogg-saron aura mastery hold` keeps the class node
off the cooldown. The trigger tests `HasAura(…, bot, false, true)`: **its own copy**, because a human's
sits on every raider and otherwise reads as already done, which is why no bot had ever taken the aura
while one was played. The room check is distance only, so the election and both holds end on
`roarExpected` (phase 3, below): Aura Mastery then goes with Concentration Aura.

**And the other paladins have to keep off the slot, or the cover lands half a second late.** The core
lets two copies of one paladin aura coexist per caster and gives the effects to the strongest, ties to
the lower caster GUID (`SpellAuras.cpp`, a local patch). Three paladins on 48943 is therefore not
redundancy but a ping-pong: in one pull the effects changed hands **207 times in 110 s**, and the
elected paladin pressed Aura Mastery 15 ms before a nova that still resisted a median **25.3%** because
the dominance had not flipped to it yet. The next nova 3.2 s on resisted **40.0%**, the pull's highest.
`yogg-saron shadow aura other hold` keeps every non-elected paladin off 48943 and off Aura Mastery, and
names the two generic `shadow resistance action` nodes too, since their action adds a strategy that
re-casts the aura by itself. Nothing else of theirs is held, so their own buff strategy puts Devotion or
Concentration back in the slot on the next GCD. Only while somebody is elected, so a raid with no Aura
Mastery anywhere keeps the generic nodes.

It stops a **cast**, not an aura already in the slot: hand-offs fell **307 to 145** and casters from
three to two, but the two left had 48943 up before the pull, so there was nothing to veto and the
dominance still changed hands 145 times in 128 s.

Healers were already putting 20,000-23,000 HPS into nine melee and out-healing it **1.7 to 1**, and two
novas in one tick killed eight from full: an instant that empties a bar is mitigated or not survived.
All **25** phase 1 deaths across four pulls were this spell, and not one had a cooldown up.

**Sara's Fervor (63138) doubles it, and that is a one-shot.** The DBC gives +20% damage done and
**+100% damage taken** for 15 s; the core implements none of it, so the spellbook and every
server-side grep are silent, and `DispelType 0` means nothing lifts it. Measured at **39,430** and
**45,340** against that 23,537 pool, both from full health and both **0.4-1.1 s after it landed**, with
the bot still walking out.

So a Fervor holder leaves on the widest gate anyone gets, `…_FERVOR_NOVA_HEALTH_PCT` (**50%**), and it
cannot be tighter: over 98 novas a 50% warning ran a **median 9.2 s** and came in under the ~2.5 s a
15 yd walk takes exactly **once**, against **30 of the 98** under 20%. Focus fire is why — it cuts the
window a Guardian spends at or under 20% from a **median 3.5 s** to **0.9-1.0 s**, and one second is
7 yd of travel against a 16 yd blast from a start in melee contact.

Nor wider. Fervor runs 15 s against a nova every **10.4 s**, so a holder meets one whatever the gate,
but "every living Guardian in reach" is satisfied on **100.0%** of melee ticks over seven pulls (23,761
of 23,761) against 72.9% at 50%: the holder left a **median 6.2 s** before the nova against 1.8 s for
everyone else, **3 of its 10** runs dodged nothing at all, and Fervor runners woke 6 Guardians 20-30 yd
out.

**The hold travels with the gate, and that is the live hazard.** `yogg-saron phase 1 walk guard
multiplier` pins a holder outside only while the same gate is satisfied, and **11 of 40** landings over
seven pulls had a Guardian inside 22 yd with none of them under 50%. A warlock was once in exactly that
state: nothing low, the spacing walk put it **3.5 yd from the middle**, and it stood there 5.3 s and
took **41,832**. Watch where an unheld holder ends up.

**Sara's Blessing (63134) is a heal with a 20 s tail, and nothing reads it.** Effect 1 heals 27,000,
effect 2 ticks 6,000 shadow every 2 s for 20 s: 60,000 raw, ~40,000 once resisted, `DispelType 0` so
nothing lifts this one either. Net about -13,000 on a 30,000 pool, so it kills nobody by itself — it is
what leaves the holder unable to take the next nova. One pull: **160,204** over 4 casts, and **44%** of
everything Mighty took before a nova finished it from full. Shadow school, so **48943** cuts it too.

The "chasing me" half of the ranged rule needs no timing, but it is not the whole test: the station
clears a blast on the stack by about a yard, so the back line ignores a low Guardian **the tank still
holds there** (`YoggSaronGuardianOnTheStack`) whoever it is hitting, and dodges one that has left the
stack whoever it is hitting. Measured, a low Guardian sat 16.2-17.0 yd off the back line all through
phase 1 and never reached it; at the handover one came to 2.2 yd.

### The nova cooldown ladder

Per the 25 deaths, a flat cut of 20% saves 14, 30% saves 19, 40% saves 23 and 50% saves all of them.
Cuts **multiply** — `Unit::GetTotalAuraMultiplier` is `AddPct` per aura, read from
`SpellDamageBonusTaken` — and none of these spells shares a `spell_group`, so Pain Suppression over
Shamanistic Rage is `0.6 × 0.7`, a 58% cut that covers every one of them.

**Plan against the unresisted hit or the gate never opens.** `YoggSaronNovaLeavesNothing` is
`health <= expected`, so while `expected` was the resisted median the threshold was 21,750 and **no
raider's pool was that small**. A bot qualified only once already damaged, which is the "I am already
low" reaction the ladder exists to replace: at 1:06 of one pull the buttons fired 0.1-0.4 s *after* the
nova that killed two bots at 100% health. `expected` is `…_NOVA_EXPECTED_DAMAGE` per low Guardian in
reach, at most `…_NOVA_MAX_CHAIN` (2) of them, doubled under Fervor.

**It is the unresisted roll, though, so a bot carrying the 130 is planned against
`…_NOVA_RESISTED_DAMAGE` (25,740).** Over **892** hits a nova landed a median **14,749**, a 90th
percentile 20,798 and a 99th 23,702, and never resisted under **10.0%**. At 28,600 a full-health melee
bot nothing on the table could kill still read as at risk, which is what emptied the externals below.
`YoggSaronNovaDamageFrom` reads `GetResistance(SPELL_SCHOOL_SHADOW)` against `…_NOVA_RESISTED_AT` (130),
covering the Prayer, the paladin aura, Aura Mastery and gear at once: one exclusive aura, highest only.

**A gate that wants this figure has to read it, never a copy.** `YoggSaronNovaHurtCount` spelt "the
whole chain in reach" as `expected >= …_NOVA_MAX_CHAIN * …_NOVA_EXPECTED_DAMAGE`, an identity only while
28,600 was the estimate's own ceiling; the resisted figure dropped that ceiling to **51,480** against a
**57,200** test, unreachable for every bot carrying the 130, which is all of them. `yogg.novaraid` went
**3 rows to 0** over three pulls, **16 of 16** novas had nothing raid-wide up, **9 of 9** deaths landed
on one, and the hold still vetoed **22** Divine Sacrifice presses: the raid held the cooldown the whole
fight and spent none of it. `YoggSaronNovaDamageEach` is the single per-nova figure now, read by the
estimate, by that threshold and by the external gate.

**Two gates, and the tighter one decides whether to spend at all.** Over 48 novas a Guardian first
dropping under **20%** detonated a median 4.8 s later and as late as 11.5; under **10%** a median 2.4 s
and never later than 3.7 across one pull's 11. So `…_NOVA_NOW_HEALTH_PCT` (10) is where a bot spends its
own button and a healer its external, `…_NOVA_NOW_WINDOW_MS` (5 s) always covering the hit;
`…_NOVA_SOON_HEALTH_PCT` (20) and its 12 s window are Divine Sacrifice's alone, because one cast covers
25 raiders for 10 s and is worth starting early. Everything here is instant, so even 0.3 s is castable.
At the wider gate the raid emptied **eight** cooldowns onto one nova whose worst hit was 16,290, then
had nothing for the next eight and lost three bots from full health.

**Melee mitigate or die in phase 1: no pool gets one out of the blast there.**
Surviving 28,600 needs **18%** off for the 23,387 shaman down to 0.1% for a 28,582 paladin, so any 20%
button saves every melee — but supply does not: 13-16 cooldowns exist per phase 1 against 11 novas
hitting 7-10 raiders each. That was the case for sending the three smallest pools out, and measurement
killed it twice over.

**It did not work.** Of 74 melee step-outs over six pulls, **44 took the nova they ran from anyway** (28
for over half the bar) and 30 got clear. The lead is why: a median **0.6-3.5 s** by pull against the
**2.5 s** the walk takes, because focus fire cuts a Guardian's time under 10% to 0.9-1.0 s.

**And it cost the one thing phase 1 cannot spare.** The destination preference is the station band,
which is **0.1 yd** off orbit 2 (21.5 against 21.4), so a step-out that did arrive parked a melee bot on
a summon circle: **133 of 185** melee cloud dodges (**72%**) fell inside 20 s of that bot's own step-out,
and melee feet woke **25 of 85** Guardians, 17 right behind a dodge. Sara needs **10** inside 15 yd and
the raid produced 11-16, so every extra Guardian is another nova rather than progress.

**Standing in it is survivable, which is what makes the trade one-sided.** Over 403 nova hits on melee
inside phase 1, the three pools under 26,000 lost a median **62%** of the bar and **86%** at the 90th,
and the five above it a median 54% with **0 of 301** hits reaching 90%. The one 100% was Totemist taking
23,424 of 23,497 and living. All four phase 1 melee deaths were cases no walk reaches: a Fervor hit of
**154%** that landed 1.7 s after the aura, and three bots caught by **two novas 2.7 s apart**. So the
chain is the hazard left (below), and it is a mitigation problem — the second Guardian is not low when
the first detonates.

**After phase 1 every melee leaves whatever its pool, and only a tank stays in.** The leftover
Guardians took **101** melee hits with **68** of them bare, so leaving is free there. A pool gate let
just **2 of 9** melee out and one of the seven it held back missed by **152 hp**, while what kills there
is **two novas 8-9 s apart** taking a bot from 60-67%, not one from full — so the single-nova question
was the wrong one and `…_NOVA_STEP_OUT_POOL` and the own-button test are gone. The tank is the exception
in every phase: it holds the thing detonating, so walking out only drags the Guardian along behind it,
and its pool is the one in the raid that can eat the blast.

**Confirmed on the next pull, which exposed a different killer.** No melee dodge cited a nova at all,
melee cloud dodges fell from 17-24 a pull to **6**, Guardians to **13** with 10 of 10 inside 15 yd, and
phase 1 ran **110.4 s**, the fastest of seven. But five bots died to novas and **three were the same two
dying again**: a battle rez strips every raid buff, so Totemist and Assasin came back carrying none of
the nine they died with — no Prayer of Fortitude, Gift of the Wild, Blessing of Kings or Sanctuary, no
Prayer of Shadow Protection — on bars at most **83%** and **87%** of the buffed pool, and each then died
**from 100% of the new bar** to a single nova. Nothing re-buffs a raider in combat. `yogg-saron battle
rez hold` holds `rebirth` until the handover, the first stretch of the fight with no nova in it.
Reincarnation cannot be held the same way: it runs from the **dead engine**, which carries only `dead`,
`stay`, `chat`, `default` and `follow`, so no encounter rule reaches it.

`trace.maxhp` keeps the pool from before the first death, so a resurrected bot's percentages are wrong
and the killing blow is the only witness to its real bar.

**The station band survives as the Fervor holder's destination**, and that is who `HazardSet::preferred`
is for now. It is the only-orbit-2 band, so it costs no summon of its own — orbit 2 runs through the
station and 14 bots wake that cloud every lap anyway, and Fervor runners woke **1 Guardian in 85**.
Without it a run ends ~19 yd out, where orbits 1 and 2 both reach.

The blob (`…_P1_RANGED_STACK_RADIUS` of `…_P1_RANGED_SPOT`) came out of the test: asking
for both left `FindNearestPositionClearOfHazards` **0 to 2** acceptable points of the ~200 its grid
offers a bot 0-6 yd from the middle — rings 2 yd apart, 16 bearings, against a 2.3 × 10 yd lens 19 yd
away — so one cloud circle emptied the set and only **11%** and **15%** of step-outs reached it. The
band on its own offers **11 to 16**. Widening the tolerance is not the fix: ±1.0 *is* the band, 1.13 yd
either side of 21.5. **Not the tank**, which holds the Guardians on the pile, nor
a melee healer — `IsMelee` is `!IsRanged`.

**There are two raid-wide covers, and Aura Mastery goes first.** `YoggSaronNovaRaidCover` picks it over
Divine Sacrifice whenever the elected paladin has its own aura up and the cooldown ready, because it
costs the caster nothing where Divine Sacrifice redirects 30% of the raid's damage onto it. Both run
120 s against a nova every 10, and `yogg.novaraid` records which went.

**Both claims key on the Guardian, because every nova death landed on an uncovered nova.** One pull left
**9 of 13** novas with nothing raid-wide up and **4 of its 5** deaths on one of those.
`…_NOVA_WINDOW_MS` was a 10 s stretch of wall clock held by whoever took it: the elected paladin claimed
it and spent Aura Mastery, its only cover since Retribution has no Divine Sacrifice, and the second
Guardian of a **3.2 s** pair then went uncovered while two Divine Sacrifices sat ready. Guardians die
1.8-4.5 s apart and 32 such pairs hit one bar inside 6 s in that pull, 8 for more than a whole bar, so
the claim now names the lowest Guardian in reach — every paladin in the pile sweeps the same 17 yd, so
they agree on which it is.

`ClaimYoggSaronNovaExternal` keys on the same Guardian rather than on the victim. Per victim the raid
emptied itself: **four** Hands of Sacrifice inside **15 s**, because `YoggSaronNovaVictim`'s worst pick
flips between ticks and each flip read as a fresh bot to cover. Three went on novas that killed nobody
and nothing was left for the four that did, while Lay on Hands kept arriving 0.03-0.4 s **after** the
death.

**The raid-wide button waits for a chained nova, or it is never there for one.**
`YoggSaronNovaHurtCount` flags a member on `GetHealth() <= expected`, and one nova's `expected` is the
**unresisted** 28,600 while every pool in the raid is 23,387-31,089 — so a single Guardian low anywhere
flagged the whole melee group and three paladins each spent Divine Sacrifice inside the first **73 s**,
on novas where nobody fell below 8,020. Its 120 s came back at **2:22.4**; the double nova killed five
at **2:14.2**, **8.2 s** short. Resistance is why the figure is wrong: over **234** hits across two
pulls **none** resisted under 10%, the median resisted **21%** and the median landed **14,411**. So the
count now needs `…_NOVA_MAX_CHAIN × …_NOVA_EXPECTED_DAMAGE`, which is what `YoggSaronNovaDamageFrom`
already returns for two Guardians in reach — and for one on a Fervor holder, correctly, since a single
nova does kill one from full. The warning is ample: two Guardians sat at **5.28%** and **0.03%** within
17 yd of the pile from 2:08.7 to 2:14.2.

**Anti-Magic Shell had never once fired for a bot**, on a name rather than a judgement. The ladder
tested `CanCastSpell("anti magic shell")`, which goes through `SpellIdValue` and `Utf8FitTo`'s
substring compare against the DBC string **'Anti-Magic Shell'** — so the only **75%** row, and the
shortest cooldown at 45 s, was skipped in silence. Across twelve pulls: bots **0** casts, the human
death knight **24**. `NextTankDefensive` and `NextMagicDefensive` now return the **spell id** and
`PickDefensive` tests that, which is safe because all eleven rows' ids are single-rank; `castName`
stays the action name `IsHeldTankDefensive` and the class contexts match on. Obliteration had spent
Icebound Fortitude (30%, 120 s) at 1:28.4 and died at 2:14 with the 45 s button it could not reach.
Since the id replaced the name bots have cast it **11 times over seven pulls**, 1-3 a pull, with every
other ladder row still firing.

**Two gaps left, both deliberate.** A **dps warrior has none**: Last Stand is a protection talent and
Shield Wall needs a shield (871 carries `EquippedItemClass` 4, subclass mask 64), so `CanCastSpell`
rejects both, and the pair ate eight novas of 15,258-22,250 with nothing up. **No warrior button closes
it** — Enraged Regeneration (55694, no item requirement) heals 3% of max health a second for 10 s,
against 36,321 arriving in 50 ms. And nothing walks them out of phase 1 either (above), so the pair
stands in every nova there.

Three tiers, in that relevance order, because one cast covering 25 raiders beats any single bot's own:

- **`yogg-saron nova raid cooldown`** — Divine Sacrifice, once the nova threatens `…_NOVA_RAID_COUNT`
  (3) of the stack. Claimed per Guardian by `ClaimYoggSaronNovaWindow`, `…_NOVA_WINDOW_MS` (10 s) being
  that Guardian's window rather than a stretch of clock, and cast through the class node so
  `cancel divine sacrifice` stays its continuer.
- **`yogg-saron nova defensive`** — the bot's own, from `NextMagicDefensive`, by **spell id**:
  shortest cooldown first and never two at once, so Anti-Magic Shell (45 s, and reachable only since
  the id replaced its name) goes ahead of Barkskin and Shamanistic Rage (60 s),
  then Cloak of Shadows (90 s), Icebound Fortitude (120 s), Divine Protection and Survival Instincts
  (180 s). No claim — a personal button covers one bot, so two bots firing is not an overlap.
- **`yogg-saron nova external`** — a healer's, in worth order `pain suppression`, `guardian spirit`,
  `hand of sacrifice`, claimed per Guardian by `ClaimYoggSaronNovaExternal` and only for a chained nova
  or a Fervor holder: one nova against these pools reads as lethal on anything under about **86%** of
  its bar, which spent **5 of a pull's 7** externals before the first nova that killed anybody, against
  a median landed 17,048 and a worst 25,004 without Fervor. `YoggSaronNovaVictim` takes
  whoever the nova leaves furthest past dead **after their own button** (`BestMagicDefensiveCut`), so a
  rogue who dodges it outright comes behind a warrior who has nothing, and a Fervor holder is always
  first because its expected hit doubles. Hand of Protection is out: physical only, and it sheds threat.

Each claim is taken **after** the caster has something castable, or one that claimed on sight would
beat the rest to the victim and then cast nothing.

All of it is held off the class triggers by `YoggSaronNovaHold` — `yogg-saron nova defensive hold`
over the melee row, `yogg-saron nova cooldown hold` over the externals and Divine Sacrifice. That is all
of phase 1, because every death measured in it was a nova, and after it only while a Guardian is within
nova range (`YoggSaronNovaNearby`), so phase 2 spends normally on Crush and the Death Rays. Every one
otherwise fires on "I am already low", which means after the nova: Totemist cast Shamanistic Rage nine
times in one pull and died four times. Tanks and the back line keep theirs, for the Guardians one holds
and the Dark Volley the other eats.

**The Guardians outlive phase 1 and keep the explosion**, so none of this gates on the phase. Nothing in
the core despawns them: `DespawnEntry` is never called with `NPC_GUARDIAN_OF_YS`,
`boss_yoggsaron_guardian_of_ys` has no `DoAction` so `ACTION_DESPAWN_ADDS` misses it, and its summon
type is `TEMPSUMMON_CORPSE_DESPAWN`. Only spawning stops (`EVENT_SARA_P1_SUMMON` is scheduled under
`EVENT_PHASE_ONE`). They keep `SetInCombatWithZone`, Dark Volley and `JustDied` → Shadow Nova, and the
kill order still ranks them first — so the three cooldown nodes and the P2 spacing circles follow the
Guardians instead. `AppendYoggSaronTargetExclusions` does not: it reaches only `TargetValue` pickers,
and the phase 2 resolver finds its own targets. Left phase-1-gated, one pull focused two of
them down beside the back line and lost **eleven raiders in three seconds**.

Three engine facts the spells themselves do not show:

- **Divine Guardian's raid-wide 20% works, and the split is survivable.** 70940 appears nowhere in the
  core's `src` or `data/sql`; talent 53530 effect 1 is aura **231** (`PROC_TRIGGER_SPELL_WITH_VALUE`) at
  **-20**, triggering it as an `APPLY_AREA_AURA_RAID` aura 87, and it reached all **25** raiders on every
  cast measured. Divine Sacrifice's split pool is `40% × caster max health × raid member count`
  (`spell_paladin.cpp:617-624`) and breaks under 20% health, so no caster went below **62%**. That damage
  skips `SpellDamageBonusTaken`, so the paladin's own cuts do not reduce what the split hands back.
- **Cloak of Shadows nearly removes a nova for a rogue.** AoE rolls to hit at `Spell::AddUnitTarget`, the
  magic path reads aura 186 at `Unit.cpp:3568`, and a miss runs no effect at all. 31224 is -90 on school
  mask 126 with no script, which clamps hit chance to the 1% floor, and 65209 has `AttributesEx2`
  through `Ex7` all zero, so neither `ALWAYS_HIT` nor `NO_ATTACK_MISS` is set.
- **Aura Mastery cannot raise shadow resistance** (whose sources are above). It doubles through
  `CalculateSpellMod` → `CalcValue` at aura **creation**, and `Player::AddSpellMod` never recalculates a
  live one, so it cannot double an aura that is already up.

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

**No radius outside the 2.8 yd core is ever clear of every orbit, so moving a station cannot help.**
The orbits are **9.8 yd** apart and `…_CLOUD_SUMMON_REACH` is **8.5**, so a gap between two of them is
9.8 - 17.0, which is negative: every radius past the core is inside some orbit's reach. The station band
is the best there is, the one radius only a single orbit reaches.

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
first two. The `yogg-saron displacement guard multiplier` rules zero all of them in phase 1, and Blink and
Disengage for the whole encounter — those two fire on "something is too close" rather than to close a
gap, so neither is aimed at anything, while the gap-closers come back in P2 and P3 where there is no
orbit to land on - except onto the Brain, which hangs 25 yd over its room's floor with nothing
underneath it and whose 30 yd combat reach hides the gap from `GetDistance`: one feral druid charged it
from 50 yd, stood in the air for 23 s with every room node reading `yogg.room = none`, and was charmed
where it hung. A third rule zeroes the family whenever the current target is the Brain, plus
`killing spree`, which is outside it: it put a rogue on the Brain's model at z 265 for 20 s until
Induce Madness charmed it.

**That druid was a cat, and only the cat's leap strands.** Feral Charge - Cat 49376 is
`SPELL_EFFECT_JUMP_DEST` to a spot behind the target, which under the Brain is air. Warrior Charge
11578 and Intercept 20252 are `SPELL_EFFECT_CHARGE`, and `Spell::CheckCast` paths a target whose
combat reach exceeds melee range to its ring facing the caster at the caster's ground Z: 31.5 yd from
the Brain, and navprobe reads that whole ring as floor (24/24 headings, settled z 239.0-241.2). So
`yogg-saron brain charge` (`ACTION_RAID + 0.25`, over the brain spot walk, which outranks every class
charge node) casts Charge or Intercept itself on the way in: Brain targeted, rti `square`, not
leaving or held, and the ring at least `…_BRAIN_CHARGE_MIN_GAIN` (8 yd) nearer the brain spot than the
bot. It clears `last movement` so the walk re-issues on landing. Worth ~1.5-2 s per warrior per wave.
The block above stays, so no charge fires on the way out and cats never leap; `yogg-saron brain dash`
gives them Dash (+70%, 15 s, 3 min) on the `tobrain` walk instead, never in the Chamber unless they
lack Nitro boots, since both are aura 31 and only the higher counts. Probed as `yogg.charge` and
`yogg.dash`.
 Catch the whole `CastReachTargetSpellAction` family: the Fury chain is charge →
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

- **Both phases also ask by preference.** `set.preferred` is tried ahead of `accept` and dropped when
  nothing satisfies both, so a bot in a hazard still moves. Phase 1 asks melee for the station band
  once a nova is pending and for the cloud-free circle when none is (both below). Phase 2 asks for a
  spot still inside
  `meleeDistance` / `spellDistance` of the current target, which leaves `reach melee` and `reach spell`
  nothing to undo. Without it the 3 s hold lapses, the reach node walks the bot back at its target, the
  bot lands in a hazard and the spacing node walks it out again — **1565 handovers** in one phase 2,
  77 per melee bot and 69 per ranged, 754 yd of melee walking to finish 88 yd from the start.

- **A Death Ray dodge cuts into a walk in flight.** `IsWaitingForLastMove` yields only to a strictly
  higher priority, so a forced dodge is refused behind any forced walk until that walk arrives. A 4.6 s
  body detour walked two ranged into a ray while their dodges sat refused for 1.7 and 2.1 s; that pull
  took 96,860 Death Ray damage behind detours. Inside a ray's clear radius `OverridesWalkInFlight` has
  the action clear `last movement` before moving. Rays only: a Crush wedge or the ring cutting in too
  would trade every tick with the detour.

- **A Death Ray dodge never walks closer to a ray.** A ray turns every 1.6 s at ~5.5 yd/s, so a spot
  clear of its current position can sit across its lane: two pulls lost three bots to dodges that
  picked a spot past the ray or passed 5.9 yd from it. `RouteAcceptable` (and the held-spot latch, each
  tick) rejects a walk whose closest approach to any ray is under min(current distance, 14 yd)
  (`YoggSaronRouteClearOfDeathRays`). The generic `avoid aoe` is blocked within 14 yd of a live ray on
  the platform (`yogg-saron death ray avoid aoe hold`): at relevance 90 over the dodge's 63 it took the
  tick, its combat-priority hop was refused behind any forced walk, and its spots sat beside the ray.
  Other walks (body detour, portal walk, Brain Link) still ignore rays; `--crush` names the last walk
  before each hit.

**Yogg-Saron was the only anchored Ulduar encounter with no movement guard.** Iron Assembly, Mimiron
and Thorim each zero `ReachTargetAction` while a positioning node owns the bot
([../README.md](../README.md)); Yogg had a dps-target guard and a displacement guard, neither of which
sees it. `YoggSaronMovementGuardMultiplier` closes the windows where the generic reach or `set behind`
is simply wrong and no others — it is the only way a bot closes on anything, so a blanket zero strands the raid,
and `reach party member to heal` is exempt outright:

- **The walk out of the knockback ring**, which spans the phase boundary: the window is phase 1 and
  the 6 s hold that outlives it is phase 2. Two nodes at `MOVEMENT_FORCED` and `MOVEMENT_COMBAT` traded
  the bot every ~300 ms for the whole walk.
- **Inside the ring in phases 2 and 3**, at exactly `ULDUAR_YOGG_SARON_BODY_KNOCKBACK_RADIUS` — the
  radius the spacing trigger fires at, so reach stands down only while that node owns the bot and is
  free the moment it is walked clear. A wider band leaves a ring where neither node moves anybody.
- **`set behind` into the ring**, phases 2 and 3. Both spots `SetBehindTargetAction::Execute` picks
  from (±108° off the target's facing, at the bot's current distance) are computed, and the move is
  zeroed if either lands inside the clear radius or its line crosses the ring. Guardians are tanked
  18.5 yd from the middle and are yards across: one phase 3 aimed 67 of 631 inside and was thrown 52
  times. Guardians have no frontal attack, so it costs parries only.
- **A melee bot's reach onto a live Crusher**, unless its stun holds. Melee may not target one at all
  (below), so this covers only what the resolver does not hand out - a raid mark, a fallback, a tank's
  stock picker - and being in the reach is the whole of what makes the tentacle Crush.

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
  dead tank strands every melee bot out of combat. **`reach spell` is in the same rule** for anyone who
  fights from the pile, since a taunt and a judgement are both 30 yd casts: left out of it, it traded
  the tick with the control node's walk back **116 times** in one phase 1 while the tank held a target
  45 yd away. The back line keeps its own, which is all that holds it in range from the 21.5 yd station.
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
cannot disagree about who is running. `YoggSaronNovaSoon` is the opposite read, counting melee *in*:
one answers who should run, the other who is about to be hit regardless.

**A Fervor run has to end out of reach, not at the edge.** The dodge fires inside 17 yd and stops at
20, and the station, the leash or `reach melee` then walked the bot straight back: over 9 traces **31
of 44** runs that got clear were inside 16.2 yd again while still holding Fervor (melee a median 1.1 s
later, ranged 5.3), and 3 of 17 novas on Fervor holders killed. With the hold in place it is **28 of
39** over seven pulls and 1 of 8 novas on a holder killed, so the walk back is narrowed rather than
closed. `yogg-saron phase 1 walk guard multiplier` zeroes the station, the leash (not its handover walk
out), `reach melee`, `reach spell` and
`set behind` for a Fervor holder while a Guardian at or under 50% is within
`ULDUAR_YOGG_SARON_FERVOR_HOLD_RADIUS` (22). Casting is untouched, and the spacing node still steps it
off a cloud. Past phase 1 the same radius holds **any** bot, Fervor or not, and the list carries
`yogg-saron body detour action` and `combat formation move` as well, which are what walked the healers
back into a leftover nova (below).

**Walks back wait for a cloud to pass.** 7 of the 69 Guardians raiders marked in those traces came off
a cloud only Fervor runners touched, each off its own ground: 4 melee at 8-10 yd, three of them walking
back, and 3 ranged or healers at 11.9-27.3 yd. The same multiplier holds the station walk, and the
leash and `reach melee` from outside 6.5 yd, while `YoggSaronWalkCrossesCloud` refuses a line a cloud
will come within 9.5 yd of. It walks the leg in `…_WALK_CLOUD_STEPS` (8) steps and pairs each with where
the clouds will be by then, rather than testing every cloud position against the whole leg: an orbiting
cloud crosses a radial path sooner or later, so that would refuse almost every route into the room.
Capping the lead at one 6 s window instead left the rest of a long walk checked against clouds that had
moved on, and the **17 s** run in from the door woke **3 of 13** Guardians in one pull. Nothing inside
the leash is held, since the inner orbit reaches most of it, nor a bot already standing in a cloud. The station walk ignores clouds within 14.5 yd of the spot: orbit 2
over the stack is its designed cost, and one more bot summons nothing extra.

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

**A melee bot that is neither tanking nor healing gets a tighter leash, because the pile wakes the
inner orbit by standing still.** Orbit 1 is 11.6 yd out and a cloud wakes to anyone inside 8.5, so a
foot past `…_P1_PILE_LEASH` (**3.1 yd**, the difference) summons a Guardian: **5 of 6** melee-foot
wakes in one pull had a melee moving at the mark, and **10 of 10** of those melee stood past 3.1 yd, a
median **4.5 yd** out. The pile's median is **1.79 yd**, but each bot sat outside 3.1 for **27-48%** of
the phase, which is enough. `…_P1_PILE_RELEASE` (2.0) is its hysteresis. The wide 6.5/2.8 pair stays the
tank's, because that one is about where the *Guardian* dies, which follows threat, not where this bot
puts its feet.

**What takes them out of the circle is their own cloud dodge, so the dodge aims back at it.**
`set.preferred` asks for `…_P1_CLOUD_FREE_RADIUS` of the middle, for melee that are neither tank nor
healer: the tank's own control walk already holds it there, and `IsMelee` is `!IsRanged`, so a melee
healer would be pulled into the pile by it. Of **124** melee cloud dodges over seven pulls only **56**
ended inside the circle — **30** stopped in the 2.8-20.1 yd band the innermost orbit sweeps and **38**
ran out onto the second orbit's ring — while a spot in the circle was clear of every cloud circle and
past the route test in **111 of the 124**, and in 32 of the 38 that ran out. So melee feet woke **16 of
102** Guardians, **11** of them inside 20 s of a dodge; the back line woke 20, the tanks 16, Sara's
timer 46, and 4 could not be attributed. Preferred, not required, so it never holds a bot in a cloud.

**The preference was not reaching the core once the step-out went** — **0 of 6** melee cloud dodges
landed inside it and they ended a median **24.6 yd** out. `FindNearestPositionClearOfHazards` only
offers points on rings around the bot, 16 bearings apart, which at 20 yd out are **7.8 yd** from each
other against a 5.6 yd circle, so the core falls between two bearings. `YoggSaronSpacingAction::Execute`
now tries the middle on the bot's own bearing before the rings, short of the edge because Sara stands
there. A finer grid instead would clear the passability cache between the preferred and the plain sweep
and quadruple the map queries on every dodge.

**The tank is in `pileDodger` as well, and its control walk already wanted that circle.** Left out on
the grounds that the walk holds it there, the tank's dodge had no destination at all and landed at
**18.0** and **29.3 yd**; it woke **3 of one pull's 16** Guardians, every one while walking and two
inside a second of its own dodge, one of them an orbit-3 cloud **24.3 yd** out on a Fervor run. So the
preference is melee and not a healer, rather than melee and neither a tank nor a healer.

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

Kill order — **phase 1: one Guardian for the whole raid, no two deaths inside 6 s, never beside a
lower one** (threat section below). Splitting damage is what killed the raid on 2026-09-14: two
Guardians rode down in lockstep from 63.6%/82.3% to 1.4%/2.8% and crossed zero inside one second, and the **double** nova put
228,396 over 16 hits and killed all eight melee in **16 ms**. Four earlier single novas were all
survived. Brain level: Influence Tentacle →
nearest other illusion add → the Brain. Boss room:
**leftover Guardian of Yogg-Saron** → **Marked** Immortal Guardian (36064, only with Thorim; without him
never a target) → Immortal Guardian (33988) →
a Constrictor holding somebody → Crusher (**ranged only**, below; phase 3 ranged take a leftover one first) → Constrictor → Corruptor → Yogg. Phase 3 melee skip Constrictor and
Corruptor and take a Guardian only within 20 yd of the phase 3 melee spot
(`ULDUAR_YOGG_SARON_PHASE_3_MELEE_GUARDIAN_RANGE`).

**The leftovers lead the boss room, and leaving them off it cost a pull.** They survive the transition,
keep casting a 35 yd Dark Volley nothing walks out of, and regenerate to full the moment the raid takes
a portal. With no tier they fell through to the `dps target` fallback, which is nearest-first per bot:
five of them took **~1,978,370** — two Guardians' worth — for **zero kills**, four ending between 11.9%
and 21.2%, while dealing 735,989 back. That is the same split-damage failure phase 1 has a kill order to
prevent.

**Phase 3 splits the raid: ranged kill Guardians, melee stay on Yogg.** Phase 2's tentacles outlive
the transition as well. Ranked above the Immortal Guardians, a Crusher and two Corruptors 38-45 yd out
took one pull's first 50 s of phase 3 (80-100% of raid targets, Guardians ≤ 4%, Yogg ≤ 6%) while five
Guardians spawned untouched; they killed the raid inside 77 s and none went below 74%. A 25-man
Guardian has 527,555 health, so ten ranged reach Weakened in about 7.5 s. Melee wait for guardian control
to bring one to the tank: it holds the tank within 5 yd of the spot, and a full-health Guardian swings
from up to 14 yd.

**Except a leftover Crusher, which phase 3 ranged kill first** (tier 0 beside the leftover Guardians).
Below the Guardians nobody ever hit one: in two later pulls a Crusher and two Corruptors outlived
the whole phase and Diminish Power sat on the average raider 55% and 46% of it, while the Corruptors'
debuffs came to 28-44 raider-seconds in all, so they stay last. The tank parks every Guardian on the
melee spot, so the 20 yd window keeps melee on Guardians all phase; left that way by choice.

**Phase 3 ranged only take a Corruptor or Constrictor their station reaches** (`PhaseThreeStationReaches`), or a Constrictor holding somebody; otherwise they fall to Yogg, whom the
ranged spot reaches. During one beacon the marked Guardians were barred, the Weakened ones under 10%
and the fresh one not yet held, so all nine walked toward a Corruptor 83 yd out through the pile: 72
opposite `flee` / phase 3 positioning walks inside 1 s. The pets take those, below.

**Nobody hits a Guardian a tank does not lead on.** `SetInCombatWithZone` hands a fresh one a
threatless first victim, and heal threat reaches every Guardian in combat, so one pull's left the tank
a median **+1.0 s** after spawning (43 of 45: healer 21, melee 10, pet 7, ranged 5) and put 36 swings of
26-41k on non-tanks, 11 of them killing blows. `YoggSaronGuardianThreatAllows` lets a non-tank start
one only while its victim is a tank and the bot's threat is under
`ULDUAR_YOGG_SARON_GUARDIAN_THREAT_START_SHARE` (0.8) of that tank's, and keep it under `…_KEEP_SHARE`
(0.95); taking one over needs 110% in melee, 130% at range. A bot held off drops to the next tier, Yogg
at worst, and the `dps target` fallback takes the same test. With no tank alive in the group it opens.

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
Guardian already inside the leash or already walking at a tank, and **nothing it cannot reach**: Taunt
355, Hand of Reckoning 62124, Dark Command 56222 and Growl 6795 all share spell range index 4, so
`ULDUAR_YOGG_SARON_P1_TAUNT_RANGE` is **30 yd** - still the whole 21.5 yd back line, so this never
walks the tank out after one.

**An unreachable aim sticks, which is why the range has to be in the pick.** `PlayerbotAI::CastSpell`
selects before it validates range, so a taunt that cannot land still leaves the tank holding what it
aimed at, and nothing takes that back: `YoggSaronSetDpsPriorityTrigger` stands down for a tank outside
phase 2 and the generic picker keeps any live target (**5** casts in 169 s). Lowest health anywhere then
means whatever a hunter pet is grinding down at the wall, which wins forever while out of range. One
pull aimed **12 of 15** taunts at 45-57 yd, dealt nothing for **93.2 s of 168.7** while two nodes took
turns walking it, held the Guardians **8.8%** of the time against **28%** on a non-tank raider, and
never finished phase 1. The trigger is a pet holding a Guardian far out, which ran **54.9%** of samples
there against 1.1% the pull before and 83% of a phase 1 on 2026-09-26: intermittent, so the fix is
measured over pulls rather than one. Of 676 ticks the pick was out of range in **67.5%** while a
candidate inside 30 yd existed in **79.0%**, so the clamp redirects the taunt rather than starving it.
Both pulls after it landed finished phase 1 — **125.2 s** and 140.1 s, 12 and 11 kills against the 10
needed — with the tank frozen 22% and 25% of the phase against 55%, and 1 of 11 and 2 of 8 taunts out
of reach.

**So phase 1's control node ends on an attack**, the way phase 3's does: nothing to fetch or the taunt
still on cooldown, and the tank hits the lowest-health Guardian already inside `IsWithinMeleeRange`
(`YoggSaronPhase1StackTarget`), holding where none is. In reach only, so holding the pile is never a
walk off the middle - one sat in reach through all **331** samples of that stall. `yogg.tankhold`
"attack" records it, the same value phase 3 writes.

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
it. `yogg.p1focus` records each change: `picked`, `parked`, `spaced`, `chain`, `abandoned`, `none`.

**One focus does not space the novas.** On the 17:25 pull the focus held (6.4% of phase 1 on two or
more Guardians, no back-line nova), but an untargeted Guardian on the stack still lost **5.6%/s** to
splash and ground AoE, against 1.2%/s at the station and 13.0%/s for the focus. 4 of 13 deaths were
Guardians at most one bot was on. Two died in the handover at 1:53.98 and 1:55.58, the focus followed
at 1:57.37, and two melee held in the middle went 100% → 62-75% → 15-36% → dead. So:

- **Nova gap.** For `ULDUAR_YOGG_SARON_P1_NOVA_GAP_MS` (6 s) after any Guardian dies, one below
  **30%** (`…_NOVA_GAP_HEALTH_PCT`) is not killable: it is dropped like a parked one and the focus
  moves on (`spaced`). The pull with no phase 1 deaths never had two novas closer than 6 s. It buys
  ~3 s in practice: held Guardians on the stack still lost **4.2-7.9%/s** to the bot tank (Hammer of
  Wrath, Seal of Vengeance: tanks keep the generic picker), humans, running DoTs and cleaves not typed
  `Aoe`, and 2 of 5 died inside 6 s (2.9, 4.3 s), killing nobody. The clock is
  `YoggSaronGuardianDeathListenerScript` on 65719, which only a Guardian's `JustDied` casts. It holds
  without a bot tank, and through the handover, where the phase still reads 1. Two deaths **0.000 s**
  apart on 2026-10-02 put their novas 16 ms apart for 21,542 + 17,337 and killed four melee from full,
  so the gap does not cover a simultaneous pair at all.
- **AoE hold.** `yogg-saron phase 1 aoe hold multiplier` zeroes non-heal `Aoe` actions, tanks included,
  while a Guardian other than the focus is below 30% within 10 yd of the middle or of the focus.
  Replayed, it came on before every splash kill (0:22.2, 1:05.2, 1:48.7) and covered 8.9% of that
  phase 1. That is only 2.6-5.3 s of lead, so the resolver also cancels an area channel already
  running: Blizzard, Hurricane, Volley, Rain of Fire. Mind Sear fails `IsAffectingArea` (a
  single-target aura that triggers the area spell) and ground AoE already placed keeps ticking.
- **Nova chain.** 65209's second effect hits every other Guardian within 15 yd for 25,000-27,501 (a
  `conditions` row on 33136), ~2.7% of 967,182. On the 20:51 kill a spaced Guardian lost the tank,
  chased a ranged to 16 yd and was bled to 0.9% by humans and DoTs, then the focus died 13.8 yd away:
  both in one tick, two melee dead, the far nova on the back line. Over 9 traces a neighbour at ≤3.5%
  died in the same tick 3 times of 3, one at ≤20% within 2 s 2 of 4, one above 30% never in 92. So a
  Guardian below 30% is not killable while a lower one stands within
  `ULDUAR_YOGG_SARON_P1_NOVA_CHAIN_RADIUS` (18, over the 16.2 measured on players): the focus moves to
  the lower one if it is killable, else waits (`chain`). The lowest is never held.

  That only holds the focus, and **7 of 12** Guardian deaths a pull are `splash` — DoTs, pets, cleaves
  and the humans. One pull then had two sit at or under 20% within 12.0 yd of each other for a
  continuous **8.0 s**, die at 2.0% a millisecond apart, and kill **eight melee from full health**;
  every other paired window across four pulls lasted 0.0-0.9 s. So `AppendYoggSaronTargetExclusions`
  hands the higher-health half of such a pair to `RaidUlduarStrategy::AppendTargetExclusions` for
  `TargetValueExclusionType::Dps` and `::Attacker` — every dps, dps-aoe and attacker picker at once,
  pets included. `::Tank` is left out so the tank keeps holding it rather than letting it walk at the
  back line, and the guid breaks an exact health tie so one of the pair is always free to die.

`--phases` tags each phase 1 Guardian death `focus`, `split` or `splash` by how many bot non-tanks
were on it a second before, and lists pairs under the gap: 9/0/4 and 3 pairs on the 17:25 pull,
12/2/0 and 3 on the 14:10 wipe. It also lists deaths with another Guardian under 30% inside 18 yd:
1:13.58 twice and 1:56.89 on the 20:51 kill.

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
arc. The bot the tentacle is currently hitting is exempt from the wedge, since inside the reach it
is hit wherever it stands. Leaving the reach is what stops it, below.

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
tank's own Crush line. The `dps target` fallback handed it to them anyway (~150 bot-seconds in one
pull), so a fallback Crusher now takes the same test.

**A tank is melee, and nothing was saying so.** `IsAllowedTarget` rejects 33966 on `IsMelee`, which a
protection paladin is, but the resolver never owned a tank's target outside phase 3:
`YoggSaronSetDpsPriorityTrigger` stood down for every tank and the definition only took the stock
picker away while a Guardian could be held. `FindTankTargetSmartStrategy` ranks by "not attacking me"
then nearest by `GetDistance`, which takes off the Crusher's 8 yd combat reach, and `reach melee` walks
the rest: one bot tank held a Crusher for **55%** of a phase 2 at a median 8.7 yd and took **53 of that
pull's 59 Crushes, 807,919 damage**, dying to one, while the human tank at 19.6 yd took three. The
resolver owns a tank's target in phase 2 too now, which drops it to the Guardian, Constrictor and
Corruptor tiers, and the movement guard covers the targets it does not hand out. The Crusher loses the
tank's swings breaking Diminish Power, which is what the Judgement node below is for.

**Shattered Illusion is the one exception.** `64173` stuns every platform tentacle (its `conditions`
rows) from the moment a brain room clears until Induce Madness lands, and
`UNIT_STATE_CANNOT_AUTOATTACK` includes stunned: over 171 s of stuns one pull had 0 of 710 Diminish
Power applications and 2 of 215 Crushes, both 41 ms before a stun lifted. The Crusher keeps its victim,
though, and swings the tick the stun ends, at Induce Madness end or the instant phase 3 starts. So
`YoggSaronCrusherStunHolds` lets melee and pets onto one only while the Brain's `64059` cast has more
than `ULDUAR_YOGG_SARON_STUN_EXIT_LEAD_MS` (4 s) left, exact since creatures take no pushback, and the
Brain is above `ULDUAR_YOGG_SARON_STUN_BRAIN_FLOOR_PCT` (35%; its fastest loss was 5.34% in 5 s). While
that holds the wedge is off. Once it stops, melee drop that Crusher and leave its reach. Probed as
`yogg.stunned` (`engage` / `clear`).

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
victim or inside its melee range, measured from each pet and skipping a Crusher whose stun holds. Either
half of that test is enough on its own: `SetInCombatWithZone` hands the tentacle a threat list holding
the whole raid, so a pet that never attacked can still come up as the victim. Measured from the owner
it never fired for two pets on a stunned Crusher with their owners 38 yd out, and they took 8 Crushes
in 4 s once the stun lifted, 13 across the pull.

**Pulling a pet off is too late, and it never sticks.** All 5 bot Crush deaths in one pull were cones
aimed at a felguard or hunter pet in the reach, 99 pet-seconds in a swinging Crusher's reach between
them. A pulled pet stays the victim while it walks out and the cone follows it: Worm, sent to a
Constrictor beside Prayer, swept it through Prayer. PetAI puts a pet back three ways, `OwnerAttacked` on
every harmful spell the owner casts (`Spell.cpp`), `KilledUnit` → `SelectNextTarget` (the pet's
attacker, then `owner->GetVictim()`, both the Crusher) when its add dies, and `AttackedBy`; a first
version that only stopped and recalled had four pets back at 0.0-0.4 yd before the bot's next tick.
All three skip a passive pet, which still attacks while `IsCommandAttack` is set (`PetAI::CanAttack`).
So the guard hushes a pet, remembering its stance, whenever it touches, targets, or its owner targets a
Crusher whose stun does not hold, and drives it by `SetIsCommandAttack` + `AttackStart`: its own add if
it is on one outside the reach, else the owner's target, else `YoggSaronPetFallbackTarget`'s nearest
live phase 2 add that is not a Crusher, else follow. The trigger also fires on either targeting a
Crusher or on any passive pet, so the stance comes back once the danger is gone, only for pets this node
hushed; one still hushed when the gate closes stays passive until re-summoned. It stays a per-tick node
rather than a latch: an uncommandable guardian can walk back in at any point. `yogg.petguard` reads
`pulled`, `held` (hushed before contact) or `clear`. `yogg-saron crusher pet spell hold` blocks
`pet acid spit` (cast for the pet at the hunter's target) and `shadowfiend` (spawns 3 yd from its
target) while the target is a swinging Crusher.

**In phase 3 the pets kill the far leftovers.** Owners rank every Guardian over them, and before this
nothing sent a pet: Worm, Wolf and Ruirin followed their owners to the Guardians, and two felhunters
reached a 745k Corruptor only through the Crusher fallback, sitting on it 63-86 yd from their owners
at ~2k/s together; a human did most of its damage, it died at +68 s, and its 37 debuffs were dispelled
in a median 0.1-0.8 s. `YoggSaronPhase3PetLeftover` picks one from Yogg within 70 yd — a holding
Constrictor, then lowest health, then GUID — skipping any within 25 yd of a swinging Crusher, or the
guard would pull the pet straight back. The trigger fires on it for a bot with a proper pet, and the
action commands only those (`IsPet`: hunter, warlock, unholy ghoul), since a 30 s Treant or Spirit
Wolf would spend its life walking; it is also `elsewhere` ahead of the owner's target when a pet is
pulled off a Crusher. `yogg.petguard` reads `leftover` when one is sent.

So `GetYoggSaronCrushWedges` raises a wedge **only for a Crusher that currently has something inside
its melee range**, which keeps it armed for exactly the case the guard cannot close — an
uncommandable guardian that got back in — and switches it off the rest of the time. It costs the
Diminish Power interrupts that pet melee was buying, ~1.5 s apiece.

**For ranged and healers the reach itself is the hazard.** The tentacle's melee range on a player is
`CombatReach` 8 + 1.5 + 4/3 = **10.83 yd**, no leeway since it never moves. Inside it a ranged bot
hits it in melee: a caster's weapon swing (visible only as its imbue proc, Flametongue Attack), or a
hunter's Raptor Strike and Mongoose Bite, since Auto Shot's dead zone is min range + melee range =
15.8 yd. Any melee hit makes that bot the victim through `DamageTaken`, and the next swing Crushes
it: 25-31k plus the swing, a one-shot. **3 of 3 Crush kills in one pull** were this, all at full
health: Stormweaver at 6.8 yd twice (Reincarnation stood him up on the spot) and Trueshot at 5.8,
each walked in by `yogg-saron phase 2 spacing action` dodging a Death Ray, which only tested the
wedge's angle. The same pull aimed 46 ranged and healer walks inside a reach. The wedge skipping its
victim then left the trigger quiet for exactly the bot being hit, though walking out ends it:
`UpdateAI` swings only at a victim in melee range.

So for `!IsMelee`, phase 2 spacing adds every live Crusher as a circle at
`ULDUAR_YOGG_SARON_CRUSHER_REACH_CLEAR_RADIUS` 13 (trigger 11), in the hazards and the retry both,
with **no victim exemption and no swing gate**. Melee get it only for a stun about to lift: they are
otherwise off 33966, and a Constrictor beside a Crusher would have the circle and `reach melee` trade
them every tick. The probe reads `yogg.crush=reach`.

**The phase 3 ranged station is a forced walk onto a fixed point, so it has to dodge for itself.** A
wave 4 Crusher alive at phase 3 stood 7.3 yd from `ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT`: positioning
walked the ranged onto it (spacing can't cut a forced walk, and the release only frees a bot whose target
is out of the station's reach), Shattered Illusion lifted at phase 3 start, and cones killed seven ranged
at +3.6-5.9 s. `YoggSaronPhaseThreeSpot` now gives ranged the first of the ranged spot and four standby
spots — the ranged spot turned −15, +15, −30, +30° round Yogg at 39.06 yd, navprobe on mesh with straight paths,
21.6-25.2 yd from the melee spot — clear of every live Crusher by
`ULDUAR_YOGG_SARON_PHASE_3_RANGED_CRUSHER_CLEARANCE` 18 (reach 10.83, a hunter's 15.8 yd Auto Shot dead
zone, 2 slack), else the furthest. Same answer for every bot; back to the ranged spot once the Crusher
dies. That pull's pick was −30°, 27.5 yd off it. Probe `yogg.station` `walking` / `shifted`.

## Diminish Power is why the Crusher dies first

`64145 Diminish Power` is a **5-minute channel** (`DurationIndex 5`), **−21% damage done raid-wide**
with no distance pruning, **multiplicative** across tentacles (two up = −37.6%, three = −50.7%),
`DispelType 0` so undispellable, and unkickable twice over — the tentacle is immune to
`MECHANIC_INTERRUPT` and `MECHANIC_SILENCE` via `CreatureImmunitiesId -361`, and the channel carries
flag `0x10`, not the `0x08` that makes one interruptible. It ignores LOS and immunities, and **it
reaches the brain room**: the retail exemption keyed on aura `63988` is in the DBC but that aura is
never cast in this build.

Only two things stop it. A **melee hit or melee-class spell** buys ~1.5 s: `64148` on the tentacle
has `ProcTypeMask 0x28` (taken melee swing, taken melee-class spell),
`spell_yogg_saron_diminish_power_aura` breaks the channel once it is channelling but never during
its 1.5 s cast, and `_diminishReady` is set once at 6 s and never reset, so it re-casts straight
away. Shattered Illusion only pauses it. Or the tentacle dies — **2,000,001 HP** in 25-man, which
the whole raid clears in 25-34 s even debuffed, against a respawn of 45-54 s tightening to 25-30 s by
the sixth portal wave. Ten ranged alone would spend 50 s of that window debuffed. So the Crusher is the **top** target for everyone
who can safely stand there — every ranged and healer — rather than something to keep away from.

**Judgement breaks it from outside the reach.** Every Judgement damage spell (54158, 20187, 20467,
31804, 53726, 53733) is `DmgClass 2`. They are triggered casts, but they carry
`SPELL_ATTR3_NOT_A_PROC` and the generated proc entry for a taken-flag `PROC_TRIGGER_SPELL` aura
gets `PROC_ATTR_TRIGGERED_CAN_PROC`, so either clears `Aura::GetProcEffectMask`. The damage is
`SPELL_DIRECT_DAMAGE`, so `DamageTaken`'s aggro grab never fires, and 10 yd through
`IsWithinCombatRange` is ~19.5 yd centre to centre. Nothing asked for it: 10 Judgements reached a
Crusher in one pull, all incidental, none mid-channel. `yogg-saron diminish power judgement` (59.5)
has any paladin with a channelling Crusher already in reach judge it (wisdom, light, justice, one
shared cooldown), with no walk and no target change. `SPELL_STATE_CASTING` is the gate. No stagger
is needed, since the Judgement lands inside `Spell::cast` and the next paladin finds no channel.
Probe `yogg.judgement`. It works: the first pull with the node had 2 node casts and 6 incidental
Judgements land mid-channel, each dropping 64145 raid-wide in the same millisecond, re-cast 60-80 ms
later.

**Read the channel off the aura, never off cast starts.** After a break the tentacle re-casts only
once its victim is out of melee range, and otherwise swings, so a break followed by Crushes leaves
no cast row. One pull had 121 s of 64145 on the raid against 348 s with a Crusher alive, held down
mostly by the tank and pets keeping it swinging. Uptime will *rise* once ranged stop hitting it in
melee, so judge the Judgement node on channel drops within 300 ms of a Judgement instead. The
recorder writes that drop about 1 ms *before* the Judgement's cast row, so read the channel 50 ms
before the cast or every break reads as a Judgement on a dead channel.

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
`EVENT_SARA_P2_START`, measured 18.0-18.3 s. The phase reads 1 throughout and Guardians are usually still
alive, so the leash kept hauling melee and the tank onto the middle: before the walk out, in every
pull on record **9 of 9 melee and the tank stood in the ring when it lit**, against **0 of 10 ranged
and 0 of 4 healers**, already 8.2 yd clear of it on the 21.5 yd station. So the walk out belongs to
melee and the tank alone, and it goes to **one anchor**.

**A fan of bearings is the wrong shape, twice over.** Sending each bot out along the bearing it
already held spread the front line over **37.0** and **40.4 yd** at the ring (57.7 five seconds later),
centred on bearing 34° and 75° against the station's 180°, which left **10 of 11** past every healer's
38.5 yd `AiPlayerbot.HealDistance` at some point in the next 15 s — median gap 32.5 and 37.5 yd, worst
50.7 and 55.0. The bearing also had to be bent off the back line, because at 15 yd a westward one ends
*between* ring and station: the tank once walked to **6.6 yd** of the stack with three live Guardians
taunted onto it, two detonated **2.9 yd** away, and eleven raiders died in three seconds, three of the
four healers among them.

**So `ULDUAR_YOGG_SARON_HANDOVER_MELEE_SPOT` (1973.315, -5.246, 324.889) replaces both**, every term
of it from a constant already here:

- on `…_P1_RANGED_STATION_RADIUS` — the one radius only orbit 2 reaches, and the orbit the back line
  already wakes. The 15 yd ring sat inside orbit 1's 20.1 yd reach, which nothing but a player's feet
  can wake, for the whole 18 s.
- **25 yd round from `…_P1_RANGED_SPOT`**, which is `…_SHADOW_NOVA_CLEAR_RADIUS` (20) plus
  `…_P1_RANGED_STACK_RADIUS` (5), so a Guardian dying anywhere in the melee group stays 20 yd off the
  nearest bot in the back line's blob, which is the 51.9° cone's purpose met by construction.
- 6.5 yd clear of the knock back and of a nova left on the pile; 20-25 yd from the healers; on the +y
  half, where melee measured at bearings 27-77° already stand. Floor by navprobe, `settledZ` 324.889,
  the station's own Z.

**Phase 1 is closed; the handover is what is left.** The two pulls after the taunt clamp both end it in **123.6** and
**121.0 s** with 10 and 12 kills inside 15 yd against the 10 Sara needs, the tank holding Guardians
**65.0%** and **77.5%** of the time, **0 of 13** taunts out of reach in each (the remaining
`IMPOSSIBLE` verdicts at 0.2-3.9 yd are the 8 s Hand of Reckoning cooldown), **0 of 234** novas
resisting nothing and no back-line bot caught by one. Every nova that would have killed anybody landed
**after** the ring: 3 then 0. What is left there is the pair dying together — two Guardians 0 ms apart
at 13.4 and 14.5 yd took Mighty from **100%** for 13,035 + 23,286 against a 30,339 pool, and five melee
parked on that ring died at once. **The anchor does not fix that**; it makes the group reachable and
keeps the pair off the back line. The two that had a button up, both rogues under Cloak of Shadows,
took **zero** nova rows.

**With none alive the raid drops combat and eats where it stands.** The `food` cheat's eat and drink sit
the bot and `SetNextCheckDelay(18000 × missing%)`: no AI at all. On the 20:51 kill the last Guardian
died 8.5 s before the ring, four melee sat down 3.3-4.4 yd out, and two were still eating when it threw
them. Bots ate on the stack in phase 1 lulls too. `yogg-saron stack food guard multiplier` zeroes
`food` and `drink` inside 15 yd of the middle for the whole encounter; the station keeps eating.
`--phases` prints how long before the ring the last Guardian died, or how many were alive **and how
far out each stood**, every meal inside 15 yd, and the handover's own shape: the front line's spread,
its centre's bearing and distance from the station spot, its distance from the anchor, and how many
bots spent any of the next 15 s past every healer's reach, plus each bot tank's crossings between the
middle and the anchor and the moves it had refused. `--novas` prints each Divine Sacrifice press with
when its 120 s returns against the first nova that killed, what a nova actually lands against the
28,600 the gate assumes, the warning every health gate gave against the 2.5 s a step-out walks for, and
which ladder rows a bot cast — plus any a human pressed and no bot did, which is the only way a row
nothing can pick shows up at all. `--clouds` buckets melee cloud dodges by what can summon where they
landed, tags each foot on a cloud with the dodge that put it there, and splits the melee feet out of
the raid's own; it also prints how long before its nova each step-out left, by reason, keyed on the nova
rather than on health, since the Guardian that opened the window is often dead by the next snapshot.

**Yogg without Shadow Barrier is the whole window**, and `SetVisible(false)` does not hide him from a
grid search, so the encounter's tick latches it from the first tick and `YoggSaronHandoverState` reads
that. P3 strips the barrier again and
the Brain separates the two: it is summoned in the tick the barrier first lands, so it is up for
everything after the window and absent for all of it. Nothing in the world counts the dialogue down.

**The walk is immediate, and `clearing` is a role test rather than a radius.** It used to open on a
lead of `max(4 s, walk ÷ speed × 2)` so the raid could hold the middle: leftover Guardians stop
counting for Sara the moment she dies (`DamageTaken` returns early on `_secondPhase`) but their novas
still land for 25k, and one dying at the 15 yd clearance radius reaches the back line where one dying
on the leash cannot. The anchor's 25 yd buys that outright, and the lead cost more than it bought —
**2 and 3 of 11** were still in the ring when it lit, the first 14 s of the window were spent walking
melee onto the middle they had to leave, and a bot already outside 15 yd got no `clearing` at all, so
`reach melee` dragged Shadow back in and its walk opened with **2.0 s** left and was refused six ticks
running (it ended **1.1 yd** out). Now any bot the positioning trigger owns — `IsTank || IsMelee`, the
same test it opens with — walks the moment Sara dies, with 18 s of slack. The radius is gone on
purpose: the anchor is outside the ring, so a radius test would release a bot the instant it arrived.

**The anchor holds, and the tank was the one bot that could not get to it.** Over seven pulls the
median bot stands **0.0-2.1 yd** from the anchor when the ring lights, the bot front line spans
**7.7-26.7 yd** centred at 17.2-21.5 yd out on bearing **109-113°** (the anchor's own), **0 of 9** are
past every healer's 38.5 yd, and **melee 0 of 9 are in the 13.3 yd ring in every pull** — against
1 and 3 of 9 before it, and a centre on bearing 25 and 65° a median 17.7-24.0 yd off the anchor. Those
are bots only and alive only: a corpse keeps its last spot and then releases to the entrance, which
read as a 90 yd front line in the pull that lost eight.

What did not reach it was `yogg-saron guardian control action`, which walks the tank back to the middle
from outside the 2.8 yd cloud-free circle. Both walks are `MOVEMENT_FORCED`, so each cleared the
other's hold and neither arrived: per pull the anchor walk was refused **52-97** times and the control
walk **46-95**, four moves landed, and the tank **crossed between the middle and the anchor one to four
times** a window, ending it at 0.0-21.5 yd out. It now stands the **walk** down for the window and
keeps taunting, because `…_P1_TAUNT_RANGE` (30) covers the room from 21.5 yd out, which also keeps the
Guardians the 25 yd off the back line the anchor was chosen for.

Confirmed again over six later pulls: the median bot **0.0-0.2 yd** from the anchor, the bot front line
**0.0-10.3 yd** across on bearing 108-117°, melee 0 of 9 in the ring every pull, and the tank at
**21.5 yd** when the ring lit with **0 or 1** crossings against one to four, the two walks' refusals
down from 46-97 each to **15-16** in total.

**A front line that stacks perfectly is uniformly far from the healers, which is open.** After the ring
lit, **9 of 11** melee and tanks left every healer's 38.5 yd at some point against **0-2 of 11** in the
pulls before, a median gap of **25.9 yd**. The anchor is 21.5 yd out on bearing 109° and the healers
stand at 21.5 on bearing 180°, so the two are **25.0 yd** apart before anyone moves; it never showed
while melee were scattered, because some of them were always near a healer. The handover walk itself
wakes nothing, though: over seven pulls **no** cloud was marked inside the window and **none** was
within 8.5 yd of the anchor when the ring lit, since a cloud marks its Guardian 10 s ahead and phase 1
ends first.

**Three to six Guardians are alive at the ring in every pull, and the pile is one blast.** A nova on
the anchor catches all nine melee where the fan caught a few: one pull's 2:05.6 nova killed four with
nothing up, 1.3 s before the ring. The anchor bought reachability and the back line's safety, not this.

**The leftovers keep phase 1's nova rules past the barrier.** Once it lands `YoggSaronPhase` reads 2, and
the resolver had them tier 0, lowest first, with no gap, chain rule or AoE hold, while melee Whirlwind,
Fan of Knives, Swipe and Divine Storm worked the pile: deaths came 1.9 / 1.0 / 2.1 s apart where earlier
pulls had 3.3-5.8, and the second nova killed 4, 3 and 7 melee in three pulls. Now
`YoggSaronLeftoverGuardianKillable` (gap and chain, no park floor — no stack left) gates them in
`IsAllowedTarget` and in `Execute`'s drop, since a null resolve keeps the old target (`yogg.leftover`
`spaced` / `chain`), and `YoggSaronLeftoverAoeHold` — a non-target leftover under 30% within 10 yd of the
bot or its target — holds AoE and cuts area channels. `lastGuardianDeathMs` still stamps: 65719 goes out
on every leftover death. First pull with it: 16.2 and 6.8 s apart, no handover nova deaths.

**Spacing the deaths in time was half of it, and nothing decides where.** One pull left **five** alive
at the ring and all five went off in phase 2 — gaps 18.8 / 7.8 / 6.8 / 9.3 / 8.2 s, none under the 6 s
rule — and they are still **9 of that pull's 10 deaths**, the 2:24.790 nova killing **all four healers**
from a Guardian **21.2 yd** out, standing in the back line stack. Phase 1 keeps them off the raid by
geometry: the leash holds the Guardian near the middle, they die a median 3.1 yd out, and the back line
was caught **0 of 16** times. All three rules that do it stop at the ring — the leash
(`YoggSaronGuardianPositioningTrigger` reads `YoggSaronInPhase1`), the tank's control node (phases 1 and
3 only) and the walk guard. After it the raid holds a **90-210°** arc at a median **24.2 yd** out and
the Guardians hold the same arc at **20.6**, about 4 yd inside it, so a nova lands on somebody whoever
is holding it: back line caught **9** times, and in phase 2 the Guardians were on a tank 68.5% of the
time and on **ranged 31.5%**.

**A step-out has to outlive the Guardian, not the bot's own clearance.** Four healers reached
**19.8 yd** from one, past both the 17 yd trigger and the 20 yd clear radius, and the body detour
returned them to **4.8 yd** — its own `…_BODY_DETOUR_RADIUS` (24) — before it detonated, because
`YoggSaronPhase2SpacingTrigger` stands down the moment the bot is clear and the next node owns the tick
at `ACTION_RAID - 1` against its `+ 3`. The walk guard now holds past phase 1 for the same reason it
holds a Fervor run (above). **15 of 38** post-phase-1 nova hits had landed on a bot that was already
clear of 20 yd.

**Where the late ones come from stays as it is, by the user's choice.** Two of those five were woken by
the back line in the **last 18 s** of phase 1 (marks at 1:50.7 with 6 bots in reach and 1:52.9 with 11),
so their Guardians arrived 4-7 s before the ring and could not be killed. That is the station's own
costed trade, 21.5 yd against orbit 2 at 21.4 (above), and it is not worth unpicking for this.

**The walk has to outlive the ring.** "Yogg without the barrier" ends on the tick the barrier lands,
and melee released there walk straight back under the knock back — three of nine were inside 13.3 yd
again within 600 ms. `ULDUAR_YOGG_SARON_HANDOVER_HOLD_MS` (6 s) keeps `clearing` true past it, and the
movement guard below keeps `reach melee` off the bot for the same window.

It costs melee nothing — Yogg's `CombatReach` is **30** (display 28817, `BoundingRadius` 0), so melee
range on him is `1.5 + 30 + 2.67` ≈ **34 yd**, and the P3 melee spot is already 18.4 yd out. What it
costs is the *crossing*: **42%** of portal walks routed the bot within 12 yd of the body. So P2 and P3
spacing carry the ring as a standing hazard circle, the trigger reads it too, and both the spacing
walk and the walks to a fixed spot filter the route. A bot already inside the ring is exempt from that filter —
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

**Phase 3 kept the body and lost every rule for it:** 106 launches against phase 2's 7, 93 of them on
the eight melee. The brain team surfaces west of Yogg, and `yogg-saron phase 3 positioning` sent it in
one forced straight line to the melee spot east of him, 2.0 yd past his middle. Between throws the
re-issue came back a duplicate, so each bot stood 4 s and walked the same line again. Blamed walks:
`set behind` 52, positioning 31, `reach melee` 11, `reach spell` 9. The spacing trigger, the reach
veto and the detour now run in phases 2 and 3, and every walk to a fixed spot (the P3 station and tank
leash, guardian control's leash, a Sanity Well once Yogg is up, a portal) aims at `YoggSaronBodyRoute`:
the spot, or its detour waypoint, probed `yogg.detour=spot`. The detour trigger skips a target inside
the clear radius, Yogg included, where no arc arrives, and tests reach with combat reach: centre
distance never passes on Yogg or a full-health Guardian.

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
the state machine: `notteam` / `waiting` / `spreading` / `holding` / `late` / `clicking`. The
out-of-phase-2 reset clears that clock but not `waveOrdinal`, so a fresh pull labelled its first wave
**5**; harmless while `slotWave` carries with it, but read `yogg.wave` as a session counter.

**Compare wave times signed, never with `getMSTimeDiff`.** It reads `old > new` as a timer wrap and
returns ~4.29e9, so the roll-forward test fired on every call while the prediction was still ahead and
pushed it another 80 s each time: **no trace ever showed `spreading`**, and every brain team bot walked
only once it saw the portals. Holding needs a rule too, because the portal walk goes quiet on arrival
and reach would take the bot back to its target: `yogg-saron portal spot hold` blocks reach, heal
reach, set behind, rear flank, formation moves, charges, follow and random moves while
`YoggSaronHoldingPortalSpot` holds, slot and distance read before the plan. The Yogg dodges still move
it.

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
bot derives the same team from its own seat, except healers: Val'anyr's wielder (46017, its heals
shield) first — item level can't pick it, Tree and the next priest averaged 227.9 and 228.0; Tree landed
201-394 heals a wave against Prayer's 55-74 alone — then
priest, druid, shaman, paladin: `PlayerbotAI::CanCastSpell` refuses a cast-time spell while moving, and a holy paladin moved
31-62% of its room time with 82% of its heals on a cast time. The trigger and the action used to build the list
differently — one skipped the master, the other did not — so they disagreed about who was on it;
`GetYoggSaronBrainTeam` is the single owner now, and benches low Sanity (see the Sanity Well walk).

**Phase 2 always hands phase 3 some tentacles.** `AddPortals` cuts `_summonSpeed` by 0.1 a wave, so
by the sixth a Crusher spawns every 20-24 s, a Corruptor every 12-16 and a Constrictor every 6-8. The
stun is the only time the platform gains: `DelayEvents` holds every spawn timer, and nothing channels
or swings. One pull, stock when the door opened → left when the stun lifted: 0.97 → 0, 2.09 → 0,
1.68 → 0, 3.89 → 1.47, 2.41 → 0, 4.60 → 3.05M, clear at +45, +54, +54, never, +57, never; 61k/s
removed live, 64k/s stunned. The last push crossed 30% at +49.8 s and cut that stun 10 s short. The
team being every bot melee, below from +5 to +52 s of each wave, leaves the stun to ranged, pets and
the tank, and melee found a stunned Crusher 0 times: they surfaced to none alive in four waves and one
70 yd off in the fifth.

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
Probed as `yogg.brainlink`. It stands down for a brain team bot whose portal intent is spreading,
holding or late (`YoggSaronPortalWalkPending`): the link breaks by itself once the bot is 10 yd below
its partner, and closing it cost one wave its healer, a room that fell to 14%.

It was doing none of that. `TooFarFromPlayerWithAura` had an unconditional
`return !debuffedPlayers.empty();` above its range loop and never read the `range` argument at all;
the action walked to the first group member carrying the aura rather than to the partner; and at
`ACTION_RAID` four nodes outranked it, so it **issued zero moves in a whole fight**. Seven pairs sat
24-60 yd apart for the full 30 s each, for **355,013**.

**Top up Sanity whenever the platform has nothing to kill.** Each well's 64169 is a 6 yd area aura
giving +20 Sanity every 2 s and **-50% damage done**. `yogg-saron sanity` walks a bot to the nearest
well below 40, keeps one inside 1 yd there until 100 unless its portal walk is pending, and in phase 2 also walks it at any Sanity under
100 while no Guardian, Crusher, Constrictor or Corruptor lives, unless it is a brain team bot with a
portal walk pending. One pull had 66 s of that (27, 15, 11, 7 and 6 s) with 20 of 23 raiders under
100 and wells a median 35-42 yd away; a later one had 0 s. Stunned tentacles outlasted the brain team
in 5 of 6 waves, so the idle time mostly comes after it returns.

**A brain team bot under 60 Sanity sits the next wave out.** The team ratchets down: a trip cost a
median 22 Sanity, 64 at worst, and Justice went 100 → 72 → 56 → 40 → 28 → 22 → 10 over six waves.
Topping up between waves doesn't fit: returns land 35 yd from the nearest well ~22 s before the next
portals, and that well is 65 yd from a portal spot, so the spread lead sends the bot off as it
arrives. `GetYoggSaronBrainTeam` skips `ULDUAR_YOGG_SARON_BRAIN_TEAM_MIN_SANITY` (60) and fills from
the next in order, leaving portals empty rather than sending a low bot. The benched set is decided
when the team is back (portals down, the Brain not casting 64059), which adds one plan rebuild per
wave so the replacement has the ~20 s to walk, and the rebuild at portal arrival reuses it. A bot
under 60 with no portal walk pending walks to a well as `refill`, and `topping` holds it to 100.
Probed as `yogg.sanity` (`low`, `topping`, `refill` or `idle`, the walk's reason, not a level) and
`yogg.benched`.

**No res for a corpse without Sanity.** Sanity is death-persistent (AttributesEx3 0x100000), an
Insane death has lost it, and the Voice's Insane Periodic 64554 re-applies Insane to any player
without it: every Rebirth on one in four pulls came back charmed 2-8 s later and died again, three
on one corpse. `FindDeadPlayer` skips a corpse with none while the reviver has it, so the next one
still gets the res (a multiplier would block every corpse ranked behind), and `SelfResurrectTrigger`
stands down the same way.

## The brain room

**Induce Madness `64059` is a 60,000 ms cast.** On completion everyone at **z ≤ 300** loses all 100
Sanity and is teleported out; anyone above takes nothing. No Sanity means `63120 Insane`, and
`spell_yogg_saron_insane_aura::OnRemove` calls `Unit::Kill(owner, owner)` — **a mind control is always
a death.** A bot ending a window ~120 yd from the nearest exit portal needs ~17 s to walk it.

**An uncleared illusion room has no exit at all.** All three Flee portals are in the brain chamber
behind a door the Brain opens only on that room's last tentacle kill, so a room the raid cannot clear
inside the 60 s kills everyone in it: one Chamber wave charmed eight bots in a single tick. Clearing it
faster is the only cure, so every walk below is on that clock.

Every millisecond of lead is damage the Brain does not take, so the lead is **measured, not flat**:
`max(3 s, distance / 7 yd/s × 1.5)`, at **base** run speed; at 2× and 5 s two pulls decided with a
median 6.7 s left and surfaced with 4.7 s (walk median 2.2 s, worst 5.4 s with 2 s stood at the
portal), so the user cut it. Illusion Room 63988 adds 30%
(`SPELL_AURA_MOD_SPEED_ALWAYS`), bots made 4-6 yd/s on the way out, and the boosted figure read
`safe` with no time left: five bots charmed in one pull, each with 34-69 Sanity. **`leaving` latches
until the cast ends** (`YoggSaronLeavingBrainLevel`): asked fresh, the lead shrank as the bot walked,
it flipped back to `safe` 20 times in that pull, and the brain spot walk took it back. **It is
decided only in the brain room**, since the way out of an illusion room runs through it: from the
back of the Chamber room (141-150 yd) the lead read 40-43 s, so eight of nine latched at the door with
36 s left, walked out in 14-15 s, and left the Brain at 30.7%, a whole extra wave. Brain under 30%
still sends a bot out from anywhere.
`yogg-saron exit hold cast multiplier` zeroes cast-time spells and channels while latched below;
a mage channelled Evocation for 5 s 51 yd from a portal. A flat worst-case lead threw away a third of every window for a
bot standing next to a portal. From the Brain the walk out takes 1.5-2.8 s for melee and 2.9-4.2 s for
the healer; a 10 s floor surfaced every bot with 7-8 s unused, ~2.5% of the Brain a wave at the ~41k
DPS the team puts into it. Induce Madness is cast in the tick `AddPortals` spawns the portals, so
`yogg.wave` + 60 s is its end: bots caught by it surface at +60.1 s.

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

The fix is a walk at the **nearest live tentacle**. The room's middle is the centroid of its Influence
Tentacle summon group (`creature_summon_groups`, summonerId 33890: group 1 Chamber, 2 Icecrown,
3 Stormwind), navprobe-clean with `PATHFIND_NORMAL` from every landing spot and therefore the one spot
in the room with nothing standing on it — the ring is 27-29 yd out in Stormwind, 12-29 in Icecrown,
36-44 in the Chamber. Walking there took two melee within 8.4 and 0.0 yd of the centre and back out
again, 117 yd for a 79 yd trip, first swing 16 s after landing against a 24 s room, while `reach melee`
held at `wait` for 5 s aimed at a tentacle the bot was walking away from: the middle walk is
`MOVEMENT_FORCED` and reach is `MOVEMENT_COMBAT`. Icecrown, the tightest ring, never showed it — first
swings 9.9-12.8 s, walked within a yard of direct. `YoggSaronRoomStateOf` owns when to stop and writes
`yogg.roomstate`: `walkingin` / `fighting` / `doorshut` / `tobrain` / `atbrain`.

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
tentacle carrying 8,000 (10) / 40,000 (25), 49,135 in the scaled 25-man traces.

**Spread the team over the tentacles, because nearest stacks it.** Tentacles never move and the team
walks in as one pack from one landing spot, so nearest-first gave every bot the same one, and the first
hit reverting a disguise to 33943, a tier above the disguises, pulled the rest onto it. Per wave in
one pull: one target held in 21-80 of 85-111 snapshots, a median 0.75-1.00 of the team on the
most-held, melee walking 61-72% of room time, 24.5-31.6 s per room. Spawns: Stormwind 8 on a ~30 yd
ring, Icecrown 9 in three clusters of three 16-35 yd out, Chamber 8 on a 39-43 yd ring. `SpreadTarget`
treats revealed and disguised alike and picks what the fewest other room members hold by `GetTarget`
(humans counted, healers not), then nearest. A bot keeps its tentacle while nothing is less held, and
always keeps one no lower GUID is also on, so two that race onto a free one settle next tick. Grim
Reprisal (63305) reflects 60% of every hit at its attacker, so the split changes nobody's incoming
damage. Probed as `yogg.spread`: `free`, `shared` or `kept`.

**In Icecrown and Stormwind a bot clears its share, then waits at the door.** Over nine pulls the whole
team stood 106-130 yd from the melee spot when the door opened, having swarmed the last tentacle, and
took 11-14 s to reach the Brain without Nitro. `YoggSaronTickIllusionShare` latches per bot per tick
what it may take, and `YoggSaronIllusionTentacleAllowed` gates the resolver, its `dps target`
fallback, the drop of a current target and `WalkIntoRoom`:

- **Icecrown:** each bot joins the group (centroids of summon group 2) with the fewest bots planned so
  far, ties east, west, south, so 9 → 3/3/3 and a 10-man's 3 → 1/1/1, reset per `waveOrdinal`. It
  takes only its own group, and another only once its own is clear and nobody planned for that one is
  alive in the room.
- **Stormwind:** a bot whose first tentacle (held when it died) is inside
  `…_STORMWIND_HELPER_RADIUS` (40 yd) of the stage spot, the two at 31 yd, keeps clearing. The rest
  keep the one they are on or take one no other room member targets, so a 10-man or a death never
  leaves one uncovered.
- **Both:** a player's mark (`rti target`) is always allowed; everything lifts under
  `…_SHARE_LIFT_MS` (25 s) of Induce Madness, since the slowest clear left ~29 s and an uncleared room
  charms everyone. Healers, the Chamber and bots off the brain level have no limit.

With nothing allowed and the room alive the room state reads `staged`: the illusion room node walks
the bot to `…_ICECROWN_STAGE_SPOT` / `…_STORMWIND_STAGE_SPOT`, and `yogg-saron illusion stage hold`
blocks the reprisal hold's target pickers except `AttackRti`. The spots sit on the navmesh path from
the landing to the Brain, 6 / 3 yd past the landing: both doors are inside the brain room's 50 yd
circle (Icecrown 49.0, Stormwind 45.7), so a bot at a door would read as already in the brain room.
Arrival is 1.5 yd, since each spot is only ~3 yd inside either circle. The Chamber keeps plain spread.
Probed as `yogg.share`: the group name, `orphan`, `first`, `helper`, `done`, `staged` or `lifted`.

**Grim Reprisal is what kills the brain team, and only a hit triggers it.**
`spell_yogg_saron_grim_reprisal_aura` casts 64039 for 60% of the hit at the attacker, totems skipped;
63305's `ProcTypeMask` 0x222A8 has no `TAKEN_PERIODIC` bit and no `spell_proc` or linked override, so
DoTs never reflect and a bot that stops hitting stops taking it. Six melee died below in one pull still
swinging at 21-39%, the worst burst 35% of max health in 0.2 s. So a damage dealer in an illusion room
holds under **40%** and fights again at **70%**, latched in the tick as `yogg.reprisal`: only while a
healer lives in its room, and never with Induce Madness under 15 s, since an uncleared room charms
everyone in it. The resolver drops the target, `yogg-saron reprisal hold` blocks the other pickers,
melee and charges, the room state reads `reprisal` so the walk-in leaves it alone, and `yogg-saron
reprisal regroup` walks it to 15 yd of the healer past 25: four of those six died with the healer
37-50 yd away.

**Each tentacle a hit lands on reflects separately**, so a cleave on a cluster of three returns 180%:
Sweeping Strikes and Bladestorm took a warrior from full to dead in 2 s, faster than any hold.
`yogg-saron illusion multi target hold multiplier` zeroes, in the rooms, every AoE-threat action and a
name list, since most cleaves (Bladestorm, Cleave, Whirlwind, Divine Storm, Howling Blast, Multi-Shot)
carry no AoE threat type. Heals are never held.

**The healer stations on its room's live tentacles, not on the room middle.** A target is what `reach
spell` walks it to: median 8-39 yd from the middle per wave, with 30% and 57% of healer-to-mate samples
past 40 yd in two of them. The resolver gives a healer in an illusion room no target, dropping one
without interrupting its heal. `WalkIntoRoom` skips it, since a targetless healer reads `walkingin` all
wave and a forced walk every tick starves every heal under `ACTION_RAID`. `yogg-saron illusion healer
station` (`ACTION_RAID`) walks it back past 10 yd while tentacles live, **above** heal reach, which every
healer spec wires at 38-40: reaching the station is what puts the whole room in range, so it comes before
a step toward one mate. The station was the middle, which covered two of the rooms: from within 12 yd of
the Stormwind and Icecrown middles heals landed 33-40 yd out. The Chamber's is 36-44 yd from its own
tentacles against `healDistance` **38.5**, so it covered nothing — the healer landed **17 heals** there
against 137 and 172 in the other two, never coming inside 31 yd of it. It is now the centroid of the live
tentacles, where the melee on them are, carrying the middle's own z because the Chamber floor ramps
239.7 to 244 and an averaged z stations it in the air; the middle is the fallback with nothing alive to
centre on.

**A forced walk below the platform was stopped after a yard or two, and `MoveTo` then refuses the
same point for 5 s.** `IsDuplicateMove` rejects a destination within 0.01 yd of the last one for `MaxWaitForMove`,
so a dead spline cannot be relaunched inside that window and the walk advances in ~2 yd hops: the healer
station covered 24 of its 56 yd in 54 s, **0.45 yd/s** against a run of 7, and `healer arrived` never
fired once in three waves. `ULDUAR_YOGG_SARON_WALK_GIVE_UP_MS` then ended it for good, because its
`fresh` escape wants a gap in the asking and these nodes ask every tick — which left six ranged
motionless where they stopped for 21 s until Induce Madness charmed all six. Two things stopped it,
both outside the walk:

- **The offensive potion.** A walk reading `dup` returns false, so the tick fell through to
  `offensive potion` (armed on a boss target, the Brain counts), and `UseItemAction::UseItem` stops
  a moving bot and fails. Ranged went cleared room to Brain at 3.0-3.9 yd/s standing half the time,
  against 10.3 for melee and healers; every walk under 6 yd/s but one had it failing, and it was
  never drunk. Fixed at the source ([../../systems/consumables-and-burst.md](../../systems/consumables-and-burst.md)).
- **The skull turn** on a bot with no target (healer station, regroup, walk-in), below.

`YoggSaronReleaseStalledWalk` stays for the next one: it drops the booking once the bot has
stood still past 500 ms — the `ReleaseStalledWalk` shape Hodir and Algalon already carry, where
`!isMoving()` is what keeps it off a walk that is still running — the give-up expires after
`WALK_RETRY_MS`, and `yogg.walkstep` records the yards still to go on every release.

**Scope the tentacle read to the room.** The Stormwind and Chamber middles are 200.8 yd apart, so the
old 200 yd sweep was one yard from reading the next room's tentacles — and reading it wrong is not a
wasted tick, it is `Unit::Kill(who, who)`. The Brain's own test is `_tentacleCount < _tentacleTotal`,
a per-wave counter, so per-room is the right scope. **Centre that circle on the room, not on the bot:**
the Chamber is 96 yd deep, so from its doorway a 60 yd read covers half of it. One pull's brain healer
called the room clear with the far tentacles alive, its station node stood down 50 yd short of the
middle, and it stood at the door for 24 s while four melee died 90 yd further in to Grim Reprisal.
`YoggSaronInfluenceTentaclesCleared` measures from `YoggSaronRoomMiddle` inside an illusion room and
from the bot only in the brain chamber, where the question is which room it came out of. It is the one
read behind the healer station, `yogg.roomstate` and the approach gate alike.

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
`yogg-saron laughing skull` turns a bot with no target off the skulls, at `ACTION_EMERGENCY`
beside the Yogg gaze node — the two cannot share a bot and never need to, since Yogg's own Lunatic
Gaze is a P3 self aura on the platform and the skulls only exist below it. Probed as `yogg.skull`.
**Never while moving**: `Unit::SetFacingTo` lays a spline on the spot and ends a point walk. **The
heading is judged on every skull in range**, the one of 16 with fewest ahead, nearest the current on
a tie, turning only for a strict gain: off the centroid of those in arc, the in-arc set changed with
the heading and reprisal-held melee spun 1.13 ↔ 4.79 rad every snapshot, 4.5-10 s apiece.

**Facing away must never cost a tick.** The node returned `true` whenever any skull was in arc, and
the engine ends a tick at the first action returning true, so at `ACTION_EMERGENCY` it starved the dps
resolver, the walk into the room and the exit — while `set facing` (37), `AttackAction` and
`PlayerbotAI::CastSpell` each turned the bot back at its target inside the same tick. A bot flipped
between two orientations about once a second and stood on one coordinate for **48 s** with an
Influence Tentacle 78 yd away; **six of nine melee** went that way in one pull, four of them caught by
Induce Madness at 263.9 s and dead at 323.9 s. With a live target it turns only within 40° of it
(`YoggSaronAngledSkullHeading`, 5° steps, strict gain, probe `yogg.skullturn` `angled`), inside set
facing's 45° so nothing turns it back: a skull off to the side clears that way, one behind the target is
the facing walk's. Yielding outright left a ranged bot 15 s with a skull at +85° and its target at −10°,
64 → 8 Sanity in one Icecrown visit; 29-34% of skull ticks hit a still bot with a target, 62-67% a
walking one; with the turn, 18% and the skull's share of Sanity lost 38.6-41.9% → 33.5%. It corrects
a heading only past `ULDUAR_YOGG_SARON_FACING_TOLERANCE`
(0.1 rad, ~6°), and never claims the tick. The 30 yd read is measured plainly too:
`GetCreatureListWithEntryInGrid` is bounding-radius inclusive, and **164 of 235** probe flips in one
pull had no skull inside 30 yd at all.

**Where the bot stands holds a facing the turn can't.** `YoggSaronFacingClearOfSkulls` asks the same
40° question, so the walk never fights a turn that already fixed the spot. The side of the tentacle it
fights from is the bot's to pick, and the geometry is generous: with the
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

**A human's Flee portal click that does nothing never reached the server.** Nothing server-side gates
194625, and with `autoCloseTime` 0 `GameObject::Use` never sets `GO_FLAG_IN_USE`, so another
player's use cannot lock it. Playerbots hands each master packet to the bots after the core handler,
and `CMSG_GAMEOBJ_USE` makes every bot log `add loot`: one burst of those `act` records is one use the
server processed. Bursts matched the human's exits 1:1 over five pulls, and failed stands of up to 6.4 s at a
portal had none. The case investigated was the client addon RightClick, which calls
`MouselookStop()` on right-button release whenever a unit was hovered in the last 5 s and it or the
current target is a live enemy, so with the Brain targeted it swallowed portal clicks. The Brain's
~76 × 78 × 60 yd model box encloses all three portals but does not take the hover: the gear cursor
still shows.

**Walk onto a spot behind the Brain, because `set behind` cannot get there.** The Brain faces east
(o = 0) all fight with `CombatReach` 30 (display 28951), so melee range is ~32.8 yd in 3D from a floor
~28 yd below it. `set behind` uses the 3D distance as a floor radius: the Chamber team, arriving from
the east in front of it, walked from 15 to 30 yd out, beyond range, and back in, ~4 s per melee. And a
one-shot walk to the brain room middle died on the healer's first heal, leaving `reach spell` to stop
it at the Stormwind doorway 55 yd out, which 30 yd of reach counts as in range. `yogg-saron brain
spot` (`ACTION_RAID`, walk latch) owns the walk while `rti` is `square`, which `GoToBrainRoom` now only
sets: melee go to `ULDUAR_YOGG_SARON_BRAIN_MELEE_SPOT` (1969.0, -25.4, 237.474), 12.3 yd west and
30.1 yd 3D, settled z and `PATHFIND_NORMAL` under `--nav 0x09` from all three entrances.

**The brain team holds short of 30% while the platform has a tentacle**, the guides' line ("get him
to ~32% and wait"): phase 3 lifts Shattered Illusion and leaves every tentacle alive. Two pulls
entered the last wave at 35.3 / 37.5% and hit 30% 44.9 / 43.4 s in, cutting the stun 15-17 s short
against a stunned platform taking 62-67k/s, and carried 2.45M / 3.12M of tentacles into phase 3.
`YoggSaronTickBrainHold` latches per brain-room non-healer while Induce Madness is cast and a
Crusher, Corruptor or Constrictor lives within 70 yd of Yogg: new DoTs stop at 36% (DoTs keep
ticking), all damage at 32%, and the finish starts at 8 s left or the moment the platform is clear,
latched for that cast; the fastest Brain loss seen was 5.34% in 5 s. Enforced by the resolver, its
`dps target` fallback, `DropTarget` on a held Brain, the `brain hold` / `brain dot hold` blocks and
`yogg-saron held target cast multiplier`. `YoggSaronCrusherStunHolds` drops its 35% floor while a hold
is in force and the Brain is over 31% (a human or a pet can still push it), with the lead 8 + 4 s, so
platform melee keep the stunned Crushers. If the Brain survives, the team leaves and finishes it next
wave. Probed as `yogg.brainhold` (`open` / `dots` / `hold` / `finish` / `clear`).

**Ranged and healers stand by their room's exit portal**, not the middle: west (1948.05, -23.76,
241.785) for Stormwind, south (1992.4, -52.8, 240.585) for Icecrown, north (1994.6, -0.1, 240.292)
for the Chamber. Door → spot → portal is 53, 62 and 71 yd against 93-106 through the middle, and the
exit lead drops from 10.6 s to its floor. Settled z, `PATHFIND_NORMAL` from the door, to the
portal and from the melee spot, all within 36.3 yd of it for heals. The room is latched per bot
(`lastIllusionRoom`), and a spot is used only once a ray from it reaches the Brain's hit sphere,
cached per spot (the built navprobe predates `los`); otherwise the middle. Probed as
`yogg.brainspot`.

**The way out outranks that walk, and had to be told so.** Both are `MOVEMENT_FORCED`, and
`YoggSaronMoveToExitPortalAction` returned false until it stood within 2 yd of a portal, so the engine
fell through to the brain spot node in the same tick and it took the bot back the moment the exit walk
stopped holding: one bot crossed 7 of the 21 yd it needed while the two walks alternated every ~100 ms,
another turned round 7.4 yd from a portal, and Induce Madness charmed both. So the brain spot trigger
stands down while `YoggSaronShouldLeaveBrainLevel` holds - cached per trigger pass, since it costs a
game object search and two nodes now ask it - the exit node claims its tick while it is still walking,
and it uses the portal from `INTERACTION_DISTANCE` rather than 2 yd. Losing the rotation for the 3-6 s
of the walk out is the trade, against a charm that is always a death. The exit also clears a forced
hold aimed elsewhere, since `IsWaitingForLastMove` makes a forced move wait out another's (exits sat
4.0 and 4.4 s refused behind the brain spot walk), and releases a stalled spline that otherwise reads
as `dup` (3.5 s standing). The brain spot walk clears one the same way (`YoggSaronTakeOverForcedWalk`):
a bot landing 2.6 s before its door opened was still booked on the room walk to a far tentacle, had
26 brain spot walks refused, ran 98 yd east and 177 back, and reached the Brain 15.9 s after the door.

**Nitro Boosts on the walk out of the Chamber.** Boots enchant 3606 casts 55004 (3 min, no backfire
in a raid) for 54861, +150% run speed for 5 s: about 7.5 s saved. `yogg-saron nitro boosts` fires on
`tobrain` in the Chamber with the brain spot over 40 yd away, never once leaving, and never claims the
tick. Chamber only: rooms cycle every three 80 s waves, so the boots went to whichever room came first
and the Chamber, 155-175 yd from the Brain at door-open, had them in 2 of 9 pulls (14-20 s to the
Brain without, 12.5 s with). A Chamber every 240 s against the 3 min cooldown finds them ready. 15 bots carry it and none had ever used it: `UseTinkerAction` takes stat auras only.
Probed as `yogg.nitro` (`used` or `cooldown`).

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
spawned portals and only stands down once all three have failed.

Following a master is wrong in every part of this fight — the illusion rooms are exactly where bots
idled behind a human on `clean quest log`, `apply oil` and `loot roll` — so `yogg-saron stop
following` removes `FollowMasterStrategy` and nothing adds it back.

**Thirty-four `yogg.` probes and a reader.** `tools/botobs/bosses/yogg_saron.py` prints phases, cloud-orbit
exposure, portal waves and assignments, brain-room occupancy and Brain health, Crush and knockback
exposure per role, and Sanity minima — and names any key missing from the whole trace, because a key
declared in source and absent from every trace of its own boss means the recorder is dropping it,
not that the thing never happened. The keys are `yogg.phase`, `yogg.engaged`, `yogg.room`,
`yogg.roomstate`, `yogg.cloudreach`, `yogg.knockback`, `yogg.crush`, `yogg.deathray`, `yogg.wave`,
`yogg.portal`, `yogg.portalslot`, `yogg.brainteam`, `yogg.skull`, `yogg.exit`, `yogg.handover`,
`yogg.squeeze`, `yogg.brainlink`, `yogg.tentacle`, `yogg.gaze`, `yogg.petguard`, `yogg.detour`,
`yogg.judgement`, `yogg.spread`, `yogg.sanity`, `yogg.stunned`, `yogg.tankhold`, `yogg.walkstep`, `yogg.reprisal`,
`yogg.benched`, `yogg.nitro`, `yogg.brainspot`, `yogg.share`, `yogg.dash` and `yogg.charge`,
beside the older `yogg.walk`, `yogg.station`, `yogg.p1dodge`, `yogg.p1station` and `yogg.p1leash`.
Two hazards go to the timeline only because nothing can sweep for either: the body's knockback
circle, and each Crusher's wedge carrying facing, arc and range so it can be tested by hand
afterwards.

`--portals` tells a bot that walked for a portal and lost the race from one whose node never ran at
all, which is the only way the room-tag leak shows; `--brain` prints what the healer landed per room
beside how close it came to its station, every walk down there that stalled or gave up, the deaths
reflect and gaze did most of, the reprisal holds, and per Induce Madness catch the time left at its
first `leaving`, its flips back to `safe`, refused exit walk and casting, plus walk speed to the Brain
per role, turns on the spot below, brain spot picks and per wave exits decided outside the brain
room (not in the wave that ends phase 2); `--sanity` prints each bot's trips below
(down > lowest, then what came back), the bench, Insane split into caught below and drained to 0,
and Rebirths on a corpse with no Sanity; `--phase3` each phase 2 leftover's fate and Diminish
Power's uptime; `--phases` the Keepers freed, off their passives; `--brain` skull gaze ticks split
walking / still with a target / still without, and `yogg.skullturn`; `--phase3` Heroism against when
80% of the raid was first in position (the trigger's test), Guardian samples at 90%+ health and
Weakened, Drain Life channels cut inside 1.5 s or run full, each beacon's nearest other Guardian,
`yogg.beacondrag`, the ranged station at phase 3 start and `yogg.station`; `--phases` the gaps between
leftover Guardian deaths after the ring with each one's radius and the raiders inside 17 yd of it, the
arc and radius the raid and the leftover Guardians each hold in phase 2, and `yogg.leftover`; `--phase3` the other Guardians each beacon
heal reached with health then and 6 s later (present by a fresh row, not health: a Weakened Guardian's
1 HP reads 0%), `yogg.beaconspot`, and the pets on each phase 2 leftover with its time to die. A
Guardian's spawn is a Simple Teleport (64195) within 3 s of its first sample, else that sample:
every Guardian casts it again at the wipe. `--phase3` also gives each healed Guardian's holder, when a
tank had it and what it did to non-tanks (hits, deaths, buttons, hunter shots), each Deafening Roar's
silences against 64364 and Aura Mastery, and the damage split by role and Guardian health band;
`--brain` a brain hold line per wave. `--phase3` also: station arrival per role and for the brain team
(time to the 15 yd station radius, Nitro, `released` time), each Guardian's first-flip cause (`dot` /
`aoe` / `direct` / `heal threat` / `none`, from the new victim's casts at it in the 3 s before), each
beacon's stacked heals, each tank's 20 s under a heal (taken / healed / buttons / death) and the raid's
damage on the healed ones, and area casts by worth-hitting count with `yogg.p3area` (by centre
distance, so a lower bound: the module takes off each unit's size, several yards on an Empowered
Guardian, which is why 100 of 440 casts read under 3 though every one followed `allowed`). Also: full
Drain Lifes with a bot kick ready and in range and `yogg.kicknudge`; per marked Guardian its taunts
(`took` / `didn't take` / `channelling`, read like the module) and its Drain Life seconds and cuts; per
beacon the stagers' gap to their spot at the mark and the kick reserve with its kicks (`yogg.markkick`);
the damage split is by each bot's target, so a melee bot on Yogg cleaving the pile reads all Yogg (38%
there against 22% in the combat log); Guardians on each tank per 10 s
(all / 30%+), each tank swap with both tanks' counts at +0/4/8 s, taunts back on the other tank 4 s
later, Misdirection targets, each tank's buttons with its health, every Lay on Hands; every Aura Mastery
per roar; per healer mana at +0/60/120/180 s, engine switches, gaze node fires by engine and mana
cooldowns. `--crush` gives each Death Ray hit's last walk and whether the dodge or `avoid aoe` tried.

Three of its views exist because this fight keeps failing in ways the per-mechanic sections cannot
see. **Vetoes**, tallied by multiplier and action, because a zeroed walk with nothing walking in its
place is a bot standing still and nothing else names it. **Frozen bots**, the longest a bot held a
target and cast nothing, which is that same failure from outside. And **launches**, read off the switch to
effect movement near the body (height missed most) and matched to the last walk whose line crossed
the ring rather than to the last walk issued, which is usually the dodge out; per phase, bot and node.
Under `--crush`, **Crush hits** split into the tentacle's victim and the cone, each kill with what the
cone was aimed at (of the samples either side of the hit, the one facing nearest the dead bot: the
tentacle turns the moment anybody hits it), the walk issued in the last 3 s or still in flight, any
melee hit first, and how many cone kills were aimed at a pet, and what every raider took of it with the tanks'
share; **the reach**, ranged and healer samples and
walks inside it; and **Diminish Power**, uptime off the aura and how many Judgements landed
mid-channel and dropped it. Under `--brain`, one row per wave: room clear time, one-target snapshots
and the most-held share, melee walking, the healer's distance from the middle and mates past 40 yd,
`set behind` moves, door to first Brain hit, healer to the Brain, exit seconds left and spare, and
Brain lost; plus one row per bot Induce Madness charmed - where it stood as the cast landed, how far
that was off the floor and from the nearest spot somebody left the level from, and how long it had been
standing still there.
 Under `--phase2`, the windows with nothing on the platform to kill, with Sanity, idle walks
and well arrivals. Under `--tentacles`, per wave: tentacle stock when the door opened and when the
stun lifted, what the stun took, when the platform was clear and when the team surfaced; live vs
stunned removal; the tentacles alive at phase 3; who sat on a stunned Crusher, and any Crush within
5 s of a stun lifting. Under `--phase3`: Yogg's health and each role's target split per 10 s, one row per
Immortal Guardian (first victim, when and to whom it left a tank, first hit, lowest health, share at the
stack, gone at 10% or less), each bot tank's target split and Hand of Reckoning cooldown at every
non-tank swing, how many of those swings came mid-gaze and whether a taunt reached the Guardian
between its turn onto that victim and the swing, `yogg.tankhold`,
each beacon's marked guardians' distance to Yogg and his health over the heal, gaze cost inside vs
outside, and each healer's distance to the melee spot and facing during gazes. Under `--fervor`: each
Fervor run, whether it came back inside 16.2 yd while holding and what walked it back, novas on holders,
and Guardians only runners summoned. Under `--clouds`, beside the orbit shares and the spawn
attribution: where each dodge reason sends each role, how many orbits can reach it there, and how many
of a melee bot's cloud dodges fell inside 20 s of its own step-out, and how many step-outs **took the
nova they ran from anyway**, which is the only thing that says whether leaving worked. A step-out that
wakes the next Guardian leaves no other trace, since the note says only why it moved. Under `--threat`, beside
Guardian time on target and the taunts
that landed: how long each tank's damage stayed frozen **while it was moving** and the node pairs that
took turns on it, then every taunt **attempt** from the action rows with its verdict and how far off it
aimed. The attempts are the point - a cast that failed never reaches a cast row, and those are the ones
that cost the phase. Under `--novas`, for the whole fight rather than phase 1: every
nova with the warning each gate gave, the raiders in reach and how many were back line, the worst hit as
a share of their pool and what it resisted, how many hits resisted nothing, who it would have killed and
what was up on them, the raid-wide auras live at that instant, the deaths, and the back-line bots a nova
caught before and after phase 1 — one is somebody who walked in, the whole stack is somebody who walked
a Guardian to them. Also who carried each of the two 130 sources and whether Aura Mastery was pressed by
one of them, and every pair of novas inside 6 s on one bar with what the pair took off it: that pair is
what kills once melee stop walking, and a removal row counts as evidence of a holder because a buff put
up before the pull shows only when it comes off. Plus which novas had nothing raid-wide up against
which ones killed, every one-target external with whether its nova killed anybody, and which of those
deaths were resurrected straight back into the pile, and how many hits after phase 1 landed on a bot
that had already been clear of the 20 yd blast edge.

`--clouds` says how many melee-foot wakes had a melee **moving** at the mark and how far out those melee
stood, against the 3.1 yd the inner orbit cannot reach; `--phases` says how many Guardians came off a
cloud marked inside the handover window and how many clouds were within reach of the anchor when the
ring lit. Both are zero in every pull so far.

**Do not add a Sanity level probe** (`yogg.sanity` is a walk reason). 63050 is already in the aura stream — 467 and 819 rows across the two
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
on every unit with him in its own front 180°. `AttributesEx` 200 carries no `SPELL_ATTR1_IS_CHANNELED`
bit, so `GetCurrentSpell(CURRENT_CHANNELED_SPELL)` never saw it — detect it with `HasAura`.

**Facing away is the whole defence, and there is no safe side.**
`spell_yogg_saron_lunatic_gaze::FilterTargets` tests only the victim's facing (`HasInArc(M_PI, caster)`),
so standing behind Yogg buys nothing. P3 swaps the designated tank off `TankFaceStrategy`, which would
turn it back into the gaze. One pull took 660,442 from it over 244 hits, its #2 damage source.

**Only a bot with no safe heading stops for a gaze.** The node used to claim every tick for all 4 s:
5 gazes took 19 of one phase 3's 77 s, cast starts fell from ~34 per 4 s to under 2, healing from
10.7k/s to 6.7k/s and damage from 75k/s to 53k/s. Probed as `yogg.gaze`:

- **`healer`**: keeps its back to Yogg all of phase 3 and never claims the tick. Every heal has
  `FacingCasterFlags` 0, the core checks nothing else, and `PlayerbotAI::CastSpell` only turns a bot
  for `SPELL_FACING_FLAG_INFRONT`, so a healer facing away heals anyone in range and sight.
- **`angled`**: a still bot whose target is at least 65° from Yogg as seen from the bot
  (`ULDUAR_YOGG_SARON_GAZE_SAFE_SEPARATION`) turns `(135° − separation) / 2` off its target, away
  from Yogg, and keeps attacking. `set facing` leaves it alone inside ±45° (`IsFacingValue`,
  `HasInArc(M_PI_2)`), `AttackAction` and `CastSpell` inside ±60° (`CAST_ANGLE_IN_FRONT`), and the
  gaze needs Yogg inside ±90°, so both margins are `(separation − 45°) / 2`. Facing the target outright
  needs 100°, which 6-8% of ranged gaze samples had against 23-60% at 60°.
- **`away`**: everyone else, including any moving bot since a walk faces its path, faces away and
  claims the tick.
- **`tankfree`**: a tank with a Guardian loose (`YoggSaronGuardianLoose`: holdable, victim not a tank)
  faces away but leaves the tick to guardian control. Taunt, Growl, Dark Command, Hand of Reckoning
  and Righteous Defense all have `FacingCasterFlags` 0, so it taunts with its back to Yogg. Claiming
  it cost one kill all four of its deaths: each time the tank's Guardian had just died to Titanic
  Storm, a fresh one peeled onto a healer or ranged by heal threat mid-gaze and one-shot them (30-53k)
  with Hand of Reckoning ready; outside gazes 15 other peels were taunted back before a swing.

**`rti` is a room tag, so never write one as a targeting hint.** `"cross"` is Stormwind's tag: a
boss-room bot given it is teleported into Stormwind by `yogg-saron fall from floor` the moment it dips
below z 300, and is disqualified from `move to enter portal`, which requires `"skull"`.

**Clear that tag on the platform, because only the exit portal clears it.** `MoveToExitPortalAction`
sets `"skull"` after `portal->Use` and nothing else did, while `RtiValue` is per-bot AI state no
encounter reset touches — so a bot Induce Madness teleported out, or killed below, kept its room tag for
the rest of the session and `move to enter portal` never fired for it again. One pull ended with twelve
bots tagged that way; on the next, **nine of the ten** on the brain team never walked a step and 2 of 10
portals were taken. The tick clears anything but `"skull"` or `"diamond"` whenever the bot reads `ARENA`:
on the platform it cannot be in a room, whatever the tag says. The same leak kills `WalkIntoRoom`, which
stands down on `"square"`.

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

**Healers take the melee spot and no target.** A target is what the station releases a bot for: one
pull's healers chased tentacles to a median 41-45 yd from the tank and 58-66 yd from the ranged spot.
The resolver gives a phase 3 healer none, dropped without interrupting its heal, and
`YoggSaronPhaseThreeSpot` stations it at `ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT`: 18.5 yd straight
behind Yogg, who faces west (o 3.14) all phase, outside his 14 yd knockback, 20.6 yd from the ranged
spot, and where a Guardian chasing a healer's threat walks into the tank.

A sanity well the bot cannot reach is the same failure by another route: `yogg-saron sanity` parked
three bots stationary for 202, 164 and 96 s on a well they never got to, suppressing every node below
it for as long as it lasted. It now stands down on the walk latch.

**The walk back from a far leftover Crusher stays, by the user's choice.** After one died 70 yd off,
every caster was within 30 yd of its Guardian at +2.6-6.5 s yet walked on to the station until
+13-16 s, at 0.8-6k/s against 10-15k/s parked: ~550k inside Heroism. Stopping in reach would spread
the ranged round Yogg. In the same pull's steady state healers moved 0% of the time, tanks 6.5%, and
melee 23%, switching Guardians 4-6 times in 37 s as each dropped under 10%; the body threw nobody in
phase 3 or the end of phase 2.

## Reduced Keepers

The raid frees fewer than 4 Keepers, losing that Keeper's support: each passive is +10% damage done,
plus Freya +20% healing, Mimiron +20% speed, Thorim +20% health and Titanic Storm. Freya-only rooms ran
4-9 s slower than a three-Keeper kill. Handled cases: **Thorim only** and **Freya only**. The bitmask lives in `PERSISTENT_DATA_WATCHERS_MASK` — preferred over Sara's
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
raid's whole job is Weakened. Observed: one kill's 45 guardians each cast Weakened and 43 left the
snapshots at 10% or less. Thorim's casts never reach the recorder, so `--phase3` counts that as the
Storm's kill.

**Without Thorim nothing kills them, and it is still winnable** — real raids kill with no Keeper at
all. Guardians spawn one per 10 s with no cap and a live one never times out (the
`TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5000` is a *corpse* lifetime); `SPELL_EMPOWERED` gives +180% damage
per stack at `healthPct / 10` capped at 9, so an untouched guardian swings at ×17.2; and anything under
85% starts Drain Life (64160, 5 s channel) — 16,665 damage and **166,650 self-heal, 31.6% of its own
max health**, every ~14.5 s on a *random* player within 40 yd with no LOS and no threat component. So
the phase is: **keep them piled and low, interrupt Drain Life, drag beacons clear, Heroism** — the guides'
plan. Area spells on the pile reach Yogg, since `Spell.cpp`'s area check adds the target's combat reach
(Yogg's 30) for a player-controlled caster only. Two Freya-only wipes:
Guardians at 90%+ health in 51% / 64% of samples and Weakened 13% / 1%, Yogg 12.7% / 3.8% of the
raid's damage, the bot tank dead at +63 / +67 s. **Freya-only is 0-light here**: the guides make it a
race on Yogg (Stars: tanks pick up adds, melee on Yogg; TrueWoW: Guardians kept low, tanked by him).
Yogg 25 has 44.0M, so phase 3 is 13.2M; a Guardian 425k; full swings ~17× a Weakened one, 30% ~6×.

**Melee stay on Yogg, by the user's choice**: a Guardian only during a gaze (outside its arc, above
Weakened) or beacon-healed (64486 / 64468, any caster) over the floor on a tank, within 20 yd of the melee
spot and past the threat gate. Area damage still reaches the pile. Taking every tanked one over the floor
put 9.15M into Guardians and 2.35M into Yogg (combat log): the pile always has one.

**Ranged take Guardians only down to 30%, the rest on Yogg, by the user's choice**
(`ULDUAR_YOGG_SARON_PHASE_3_GUARDIAN_FLOOR_PCT`). Bringing each to Weakened took 52% / 75% of two pulls'
damage against Yogg's 17% / 3% (he went 29.97 → 26.14% in 100 s, → 27.82% in 145 s). Under the floor a
Guardian is allowed only while Lunatic Gaze (64163) is up and it sits 65° or more off Yogg, the arc the
gaze node angles for; with Thorim the 10% rule stays. **A beacon-healed one (64486 / 64468) is fair
game, by the user's choice**, though most of the damage heals back: skipped, three full ones sat on the
tanks for the 20 s heal and killed a tank 17-32 s after it in both of two pulls. Pets on the Guardian left are stopped so they follow the owner.

- **Marked Guardians are not dps targets** (`IsAllowedTarget`): the heal resets them whatever the raid
  does. With Thorim they stay tier 1.
- **`yogg-saron beacon drag`** (`ACTION_RAID + 4`, over guardian control). The heal is one application:
  the 10 s mark ends, the entry reverts to 33988 and Yogg's 64467 missile (30 yd/s) sets it off
  0.3-1.1 s later on every ally within 20 yd of the Guardian. The pile tank can't separate a marked one,
  the pile follows whoever holds it, so a second bot taunts it off and walks it to a lane of its own:
  north (1983.68, 13.27, 327.472) or south (1993.62, −62.23, 327.625), navprobe PATHFIND_NORMAL 39.3 /
  39.8 yd from the melee spot, 39.0 from Yogg, 47.6 / 50.1 from the ranged spot, 28-30 from the nearest
  well, in heal range of the melee spot; or north-east (2017.05, 5.26, 328.781), PATHFIND_NORMAL 33.7 yd
  from the melee spot, 48 from Yogg, 24 from the ranged spot, 34 from north. **Each claim takes the free
  lane whose nearest unmarked Guardian is farthest**, north and south before north-east on a tie (its
  path leaves the pile 55° off north's), then the nearer (`beaconDrags`' lane, `yogg.beaconspot`).
  64486 stacks per caster (`SPELL_ATTR3_DOT_STACKING_RULE`), so marks dragged to one latched side healed
  each other, up to 112.5k/s each: two pulls' second beacons had every marked pair inside 20 yd. West
  and south-west were rejected (53 yd walks round Yogg), east (2028.3, −25.6) sat 11.5 yd from the
  ranged spot, south-east (2010.2, −50.7) 20.2 from south. The heal is an area
  aura applied once, at the cast, so whatever stands in reach then keeps it 20 s. From the old 27 yd
  spots all three draggers took the nearer north one; the tank stood 5.8 yd off the melee spot toward
  it, his two Guardians 7.2 / 12.9 yd out, 17.7-20.1 from the casters, and three stacked heals took
  them 0 → 79% and 54 → 80% in 6 s, the south spot 34-38 yd off. Five Guardians at full plus a fresh
  one: the bot tank carried four, took 102k in 8 hits and died, then 20 deaths in 20 s. Claims
  (`beaconDrags`): bot
  tanks while another tank lives, then non-heal paladins and Death Knights, then hunters, by GUID, taunt
off cooldown,
  **never a bot an unmarked Guardian is hitting** — that one would follow it out. It attacks only once
  standing, since `Attack`'s facing turn ends a walk; releases on the heal aura or 3 s after the mark
  ends. A claimed Guardian isn't holdable, so guardian control leaves it, and the gaze node leaves a
  dragger's tick free (`drag`). Probed as `yogg.beacondrag`. Before it: marked Guardians 5-12 yd from the pile and 15-19 from Yogg, +2.04% / +2.57%. First
  pull with it: two dragged south, all three marked ≥25.8 yd from Yogg, Yogg −0.13% over 20 s; the third
  had no dragger (the bot tank always holds Guardians) and topped a pile Guardian 82 → 99%. Only the two
  draggers and the human tank targeted a marked one. Two Freya-only pulls: one had one free dragger for
  three marks and two healed to full in the pile on the only bot tank, dead 23 s later, then 20 deaths in
  20 s; in the other each dragger died to its own healed Guardian 8 s after the heal (a 36k one-shot;
  10 / 19 / 24k), a tank taking them 5-8 s after or never. Every death after the leftover Crusher's
  was a high-health Guardian on a non-tank, 25-75k.
- **A taunt that didn't take hands the mark on** (`tauntMs`, `missed`): victim not the dragger 1.5 s
  after its taunt (`BEACON_TAUNT_CHECK_MS`; Distracting Shot flies 40 yd/s, all land by 1 s) and the
  claim goes to the next bot with a taunt ready. Not under a Drain Life already running when the taunt
  landed: the Guardian's `UpdateAI` returns while casting, before `UpdateVictim`; a channel started
  later doesn't hide the switch. One pull's wipe: Justice's Hand of Reckoning missed (combat log `MISS`)
  and the claim sat on its cooldown 8 s. Death Grip is impossible (immunity set −286: GRIP,
  PULL_TOWARDS, ROOT, SNARE).
- **Draggers are pre-staged, by the user's choice** (`YoggSaronTickBeacon`, `yogg-saron beacon stage`,
  `ACTION_RAID + 3.5`). Beacons come 45 s into phase 3, then 45 s after each that hits, retried every 5 s
  (`boss_yoggsaron.cpp:1288, 1300, 1354`; seen 45.2 / 46.5 s apart). 5 s before the next is due (off the
  last first mark) the claim's rules and order pick `RAID_MODE(1, 3)` stagers, lanes north / south /
  north-east in order; the claim gives a stager its lane while free. Stagers walk round the body and
  hold, no swing (`walk` / `ready` / `stuck`), until the first mark, or drop 8 s past due (`timeout`).
  Spots 20 yd along each lane's PATHFIND_NORMAL straight line from the melee spot, floor by navprobe
  with Z unchanged: north (1990.94, −4.40, 325.104), south (1996.06, −42.75, 324.889), north-east
  (2009.52, −6.19, 326.989); hunters wait at their hunter spot. A 30 yd taunt still reaches the pile,
  and the walk after it drops from ~39 to ~19 yd.
- **Healed Guardians get a cooldown first.** One still marked or carrying 64486 / 64468 at 40% or more
  (`BEACON_DANGER_HEALTH_PCT`, ~17k a hit) on a dragger or a non-tank: the victim presses
  `NextPhysicalDefensive` (`ACTION_EMERGENCY + 6`, `yogg.dragdef`); a healer or paladin gives Pain
  Suppression, Guardian Spirit or Hand of Sacrifice to one without a button of its own, draggers first,
  humans included, one per victim per 8 s (`ACTION_EMERGENCY + 5`, `yogg.dragext`). Divine Shield and
  Ice Block are blocked on such a victim: an immune victim sheds aggro (`ThreatManager.cpp:123-134`).
  After the release the dragger walks back to the melee spot until a tank has it or 8 s pass
  (`return`), since the tank is ~39 yd away and a taunt reaches 30, and guardian control taunts loose
  Guardians highest health first. **A tank holding the pile is covered too**: its own button the moment
  2+ Guardians on it carry a 64486 / 64468 application another Guardian cast (`YoggSaronPileHealedOn`,
  `BEACON_PILE_RESET_GUARDIANS`, at any health, by the user's choice), an external under 50%
  (`BEACON_TANK_EXTERNAL_PCT`), draggers still first. A returned mark carries only its own copy, so only
  a heal reaching the pile counts. The 60% line before it was one full-power swing (~40%): both tanks
  spent Divine Protection (2 min, beacons 45 s apart) on a mild first beacon and had none when a failed
  drag healed 11 Guardians (Dragon 291k in 13 s, dead). Before any cover: Dragon took 225k against 174k
  healed in 12 s under four Guardians at 84-91%. The external skips a bot whose own button `ReadyTankDefensive` names, which now
  counts the Avenging Wrath marker (61987, 30 s) as a lock on Divine Protection like Forbearance: the
  immunity script refuses both (`spell_pal_immunities`), and an Avenging Wrath at 11:31 left a tank
  with neither its button nor an external when the pile killed it. Tanks give no external here, and
  `hand of sacrifice on party` is blocked for tanks in phase 3: it moved 30% of a dps's damage onto a
  tank under the pile.
- **Hunters drag only marks nobody else can**, the Stars method, on their claim's lane: spots north
  (1986.0, 7.63, 326.797) and south (1994.44, −55.65, 326.394), north-east its own drag spot, navprobe
  PATHFIND_NORMAL 33.2 / 33.1 yd from the melee spot,
  42 / 44 from the ranged spot, inside Distracting Shot's 35 yd (6 s taunt, 8 s cooldown, no Guardian
  immunity): shot with ≤5 s of the mark left, so it covers the heal. It must be late: 20736 is effect 63,
  a fixed threat for 6 s, where Taunt, Hand of Reckoning and Dark Command are effect 114, which
  `Spell::EffectTaunt` matches to the top threat for good. No damage on it before then (the
  multiplier; Feign Death and Deterrence blocked), and Feign Death if it is still on the hunter 1 s
  into the heal with the taunt gone (`shot` / `feign`).
- **Deafening Roar is covered by an Aura Mastery chain** (25-man, fewer than 4 Keepers: 30 s in, then
  every 60 s, 2.3 s cast, 4 s silence within 50 yd). Two pulls: 21 / 23 / 18 silenced, healing 40k →
  17k/s. 64364 reaches only raiders carrying the presser's own Concentration Aura
  (`spell_paladin.cpp`), and each paladin's aura applies separately (`SpellAuras.cpp:2164-2168`).
  `YoggSaronTickRoar` latches `roarExpected` and the cast start; `YoggSaronRoarPaladin` picks the bot
  paladin with Aura Mastery ready by the roar, not dragging, its own Concentration up, then GUID, so
  Holylight and Justice alternate. It switches to Concentration 10 s ahead (`ACTION_RAID + 1`, other
  auras blocked) and presses during the cast (`ACTION_EMERGENCY + 8`, one per roar, others after
  500 ms); class nodes are off Aura Mastery meanwhile. Probed as `yogg.roar`. Both read the paladin's
  own Concentration by its application (`HasAura(19746, guid)`, see
  [pitfalls](../../engine/pitfalls.md)): read by name, the turn paladin's effectless copy was invisible,
  it recast the aura five times, never pressed, and the second roar silenced 22 in both pulls. The
  roar's start is re-dated only forward: each paladin back-dates it off Yogg's cast bar a few ms apart,
  an earlier start wrapped `getMSTimeDiff`, re-keyed the claim, and a second paladin pressed on roar 1
  with nobody left for roar 2 (13 silenced).
- **Nitro Boosts on the phase 3 station walk** over 40 yd (`NITRO_MIN_WALK`) when ready
  (`yogg.nitro station`); nobody walks far after. The brain team surfaces at (1924.7, −25.4, 328.5),
  74 / 94 yd from the melee / ranged spot, and arrived +6-71 s; a released ranged walk boosts once its
  leftover dies.
- **No non-tank spell on a Guardian the threat gate refuses** (`YoggSaronHeldTargetCastMultiplier`,
  kicks exempt): the "on attacker" DoTs skip the resolver. A leftover Crusher at 28.4% held one pull's
  ranged 38 yd west of Yogg (Crusher first, station `released` 11 s), on the Guardian spawn ring (a
  random angle, 38-48 yd out). The first one left its 4 s root, took Corruption and Immolate, and
  one-shot both warlocks (51k) at +7.3 / +9.4 s with both tanks 56+ yd away.
- **The bot tank skips a Guardian already carrying another tank's taunt** (`SPELL_AURA_MOD_TAUNT`):
  both tanks taunted the same fresh one together three times a pull. Every Guardian still left its
  first tank, 26 of 26 at a median +5 s.
- **Area spells need three units worth hitting, by the user's choice** (`YoggSaronPhase3AreaMultiplier`).
  `AoeTrigger` counts any attacker within 8 yd of the target, distance minus the unit's size, so Yogg
  counts from 38 yd and a lone Guardian got Seed of Corruption (63 of 148) and Volley (56 of 120), each
  target taking less than from a single-target spell. Held: blizzard, flamestrike, blast wave, dragon's
  breath, cone of cold, hurricane, typhoon, volley, mind sear, rain of fire, both seed of corruption
  actions, fan of knives, swipe (cat), death and decay, blood boil, cleave, fire nova, magma totem; each
  needs `PHASE_3_AREA_MIN_WORTH` (Yogg, leftover tentacles, Guardians over the floor a tank holds; with
  Thorim over 10%, marked ones too) within 8 yd of its target, Mind Sear not counting its own, and no
  Guardian there failing the threat gate (`yogg.p3area` `few` / `fresh` / `allowed`). Divine Storm,
  Whirlwind, Chain Lightning, Starfall, Blade Flurry, Sweeping Strikes, Multi-Shot and Howling Blast hit
  the main target in full and stay. Before: 195 of 337 and 282 of 406 bot area casts had fewer than 3.
- **A Brain Link walk stops at Crush range** (25 yd) of a live unstunned Crusher: one walked a bot 18 yd
  from a leftover as phase 3 began, and the cone killed it.
- **Drain Life is kicked on purpose.** The interrupt node's casters include Immortal Guardians
  channelling 64159/64160 in phase 3; immunity set `-286` has SILENCE but not INTERRUPT, so Strangulate,
  Silencing Shot and Arcane Torrent are dropped for them; `interruptClaims` holds a cast for one kicker
  for 1.5 s. By chance alone, 6-7 of 14 were cut inside 1.5 s and 5 ran full; with it, 13 of 13. A
  full Freya-only phase 3 still ran 54 of 219 full (10.7M healed), 29 of them with a bot kick ready and
  in range. Spell Lock is on the list (the felhunter's spell, cast by the bot, so cooldown and range are
  the bot's), and trigger and action ask `YoggSaronInterruptReady` (cooldown, power, stance, range):
  `CanCastSpell` passes a pet spell on cooldown and counts out of range as castable. A bot mid-cast runs
  no kick node, so **as the last resort only, by the user's choice**, `YoggSaronNudgeDrainLifeKicker`
  (the tick, one scan per 200 ms per instance) stops one bot's cast: a channel 1.5 s in with no claim,
  no free bot with a kick ready in range, none finishing its cast within 1 s; never a healer; shortest
  kick cooldown first, then the longest cast left; one per channel (`yogg.kicknudge`).
  **Marks' channels go first**: a channelling Guardian doesn't move, so one kept each hunter's mark in
  the pile on two beacons (7 and 11 others healed). Casters marked or dragged lead the list and skip
  the nudge's 1.5 s wait. **Kicks are saved for them, by the user's choice**: from the first mark until no
  mark or drag is left, `markKickers` holds `MARK_KICKERS` (2) bots not dragging or staging with a kick
  ready within 2 s, longest reach first (Counterspell / Spell Lock 30 yd reach the pile and the
  north-east lane from the ranged station, Wind Shear 25), then shorter cooldown, non-healer, GUID; they
  kick only marks then and the nudge skips them for others (`yogg.markkick reserve` / `kick`).
- **`yogg-saron phase 3 lust`** (`ACTION_RAID + 2`). `ShamanBoostTrigger` and the burst hold both want a
  boss target, and phase 3 shamans are on Guardians and tentacles: none in one pull, +39.7 s and +54.2 s
  in others. It casts once 80% of the living group stands at its phase 3 station (15 yd) or still in
  reach of a live target — nobody below z 300, Heroism reaches 100 yd — or 30 s in; not while Sated or
  Exhausted, nor with Heroism or Bloodlust already up. 80% came at +16.4 / +19.5 / +39.6 s. Probed as
  `yogg.lust` and `yogg.lustready` (`position` / `timeout`). The stock path still cast first at +6.8 s,
  Yogg being the only target until a Guardian spawns, against 80% at +19.7, so
  `UlduarBurstWindowMultiplier`'s Yogg branch holds lust the whole fight and the node owns it (its action
  casts by `CastSpell` under its own name).

**Healers stay on the melee spot, by the user's choice**, at a known cost: Guardians hit healers 5
times a Freya-only pull at ~41k, every Guardian leaves the tanks ~1 s after the first pickup on heal
threat, and 65% of Guardian samples sit at 90%+ health. Unsolved.

**A bot tank holds the pile.** First pull with both tanks bots: Guardians on a tank 86% of samples
until the beacon, no hits on ranged or healers, the first four at 0% by +28 / +35 / +41 / +53 s
against a spawn every ~10.5 s, raid 83k/s (72% on Guardians; 39.8k/s the pull before). Heroism at
+8.5 s (80% in position at +7.5), Drain Life 13 of 18 cut inside 1.5 s and none full. The beacon
wiped it. The ceiling: Yogg lost 2.5% in the 64 s before, ~19k/s, ~13 minutes for his last 30%.

**One pile, swapped on danger, by the user's choice** (two piles rejected). Both tanks share the melee
spot, so Consecration kept the pile on whoever led threat: Dragon held 5-11 Guardians for 90 s against
Bulwark's 0-3, every Misdirection went to Dragon, and 5 of Bulwark's 18 taunts were back on Dragon 4 s
later. When the holder drops under 50% (`TANK_SWAP_PCT`) with 3+ Guardians, more than the other tank
has, a tank at 70%+ not dragging takes the pile (`YoggSaronTankSwapTake`): Righteous Defense on the
holder, then a taunt on its highest Guardian, before the loose loop. For 8 s (`TANK_SWAP_MS`, one
cooldown) the holder sheds: consecration, hammer of the righteous, avenger's shield, hand of reckoning
and righteous defense blocked, loose loop skipped; Holy Shield and Shield of Righteousness stay. It ends
early on the holder's death or the taker under 50%. A tank under 50% also leaves loose Guardians to a
tank at 70%+ (`YoggSaronTankLowHold`): soulstoned Dragon taunted straight back in and died again 14 s
later. It moves the whole pile, so it buys a fresh bar and the holder's cooldown, not a lighter load.
Probed as `yogg.tankswap` (`take`), `yogg.tankshed`, `yogg.tankhold` `swap defense` / `swap taunt`.

**Lay on Hands is kept for the tanks, by the user's choice.** Every pull spent all four on dps in phase 1
(two saves), so none was left in phase 3, where tank deaths ended both pulls. Blocked: `lay on hands on
party` all fight, `lay on hands` for non-tanks all fight and for tanks before phase 3. In phase 3
`yogg-saron tank lay on hands` (`ACTION_EMERGENCY + 4`, under the beacon pair) casts it directly on the
lowest tank under 20% within 40 yd, humans and itself included, one paladin per tank per 3 s
(`yogg.tankloh`). Watch phase 1 Shadow Nova deaths for the cost. **A tank keeps Avenging Wrath in phase 3**
while Divine Protection is ready within 30 s (`yogg-saron tank wrath hold`), since its marker locks it.

**Healers stay on the combat engine.** A bot with no target always drops to the non-combat engine
(`invalid target` → `drop target` at 99, see [pitfalls](../../engine/pitfalls.md)), and phase 3 gives
healers none, so all four ran it the whole phase: no Mana Tide, Hymn of Hope, Divine Plea, Innervate
or mana potion (combat-engine only), `drink` refused in combat, mana 12% / 18% at +180 s, Lunatic Gaze
fires 587 / 373 / 853 / 961 non-combat against 1-2 combat. `yogg-saron healer combat engine`
(`ACTION_EMERGENCY + 9`) switches a healer in combat on the non-combat engine back
(`yogg.healerengine`), and `drop target` is blocked for a healer in combat with no target. Out of combat
it still drops, so drinking works. Other raids with a targetless healer have the same gap, unchecked.

**Shadow Beacon heals the marked guardians, and Yogg when they stand near him.** It marks 3 guardians
in 25-man every 45 s, and 10 s later each casts Empowering Shadows (64486): **37,500 a second for 20 s
on every ally within 20 yd, 750,000 — 176% of a guardian's own max health**.
`spell_yogg_saron_shadow_beacon_aura::OnApply` swaps the marked guardian's entry to 36064 — a free
signal: `TierOf` puts Marked above unmarked so the raid reaches Weakened before the heal lands. The
area check compares centre distance to 20 with no combat reach added for a creature caster, and
Empowered's +50% scale per stack lets a full-health guardian swing from a median 9.6 yd (max 14.0). On
the tank at the P3 melee spot, 19.0 yd from Yogg in 3D, three marked guardians stood 11-16 yd from him
and healed him **+3.84%, 1.69M of 44.0M**, over exactly those 20 s, against −0.98% from the raid in
the 56 s before. Only a tank spot ~34 yd out keeps them clear, and melee cannot reach Yogg from there,
so the spot stays: with Thorim marked guardians die first, without him they are dragged; `--phase3` reads Yogg's health across every beacon.

`yogg-saron guardian control` is **not** gated on Thorim, or on hard mode. Hard mode means *fewer*
Keepers, so demanding both asked for exactly the case where Thorim is least likely to be there — and
guardians parked on a tank beat guardians loose among the casters even where none of them can die.

**The bot tank has to hit the Guardians, not only taunt them.** `FindTankTargetSmartStrategy` ranks
by not attacking the tank, then nearest by `GetDistance`, which takes off Yogg's 30 yd combat reach:
he always reads nearest and never attacks anyone. One kill's bot tank held the leftover Corruptor,
then Yogg, for all of phase 3, a Guardian in 0 snapshots, so each of its 41 Hand of Reckoning taunts
wore off with nothing behind it and 29 of 36 non-tank swings came while it was on its 8 s cooldown.
So in phase 3 `yogg-saron guardian control`, in order:

0. Takes or sheds the pile in a tank swap (above).
1. Taunts the nearest loose guardian (not Weakened, victim not a tank). Failing Hand of Reckoning, a
   paladin casts Righteous Defense on that guardian's victim: 40 yd to the ally, and its dummy effect
   (`SpellEffects.cpp`) casts 31790, range 50,000 yd, triggered on up to 3 of the ally's attackers, so
   it reaches one still walking in.
2. Walks the 5 yd leash home, round the body.
3. `Attack`s the guardian its lead (`YoggSaronTankThreatLead`, over the highest non-tank, pets
   included) is thinnest on, in melee reach before merely near the spot, leaving its current one only
   past `ULDUAR_YOGG_SARON_TANK_THREAT_SWITCH_LEAD` (1.5).

A `yogg-saron dps target guard multiplier` rule zeroes `TankAssist` for tanks in phase 3 while a guardian
that can still be held lives within 70 yd, or the picker hands Yogg straight back. Probed as
`yogg.tankhold` (`taunt` / `defense` / `leash` / `attack`).

Open risks: whether one bot tank keeps pace with a guardian every 10 s alone is untested, since that
kill had a human second tank; and the sanity-conservation behaviour (stand behind Yogg facing away
below 15 stacks) **nearly benches a bot** — sanity never recovers Thorim-only, so a bot that drops to
15 stays there.

## Squeeze breaks on immunity

Removing the Squeeze aura (64125 / 64126) kills the Constrictor Tentacle and drops the passenger, so
`yogg-saron squeeze escape` at `ACTION_RAID + 1` has a grabbed mage cast Ice Block and a paladin cast
Divine Shield. Hunter Feign Death and rogue Vanish are deliberately not used — neither removes a
periodic damage aura.

**A Constrictor holding somebody outranks the Crusher** (`YoggSaronConstrictorHolding`: its vehicle
seat is in use). 99k health dies in about 2 s under the ranged, and one pull left four raiders
squeezed 11.5-23.3 s with all nine ranged on a Crusher.

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
