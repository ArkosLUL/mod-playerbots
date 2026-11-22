# Polish log: Assembly of Iron (25N hard mode)

Trace slug `iron-assembly` (files land under `stormcaller-brundir`), raid `uld`, scenario
`uld-assembly` (drop 40 yd east of Steelbreaker, aggro start on him pulls all three). Hard mode is the
kill order: a kill counts only when Steelbreaker died last, i.e. the last `ironassembly.alive` value is
`1`. Reader: `tools/botobs/bosses/iron_assembly.py <trace>` (`--phases`, `--empowered`, `--deaths`);
its banner prints the verdict.

## Round 1 (2026-11-22, build 0c08a0922 + DC 7495350)

**Data.** Window 5, one pull per pool, all valid, all wipes in hard mode order:
uld1 `tr-20261121-212648-23` (`603_23_stormcaller-brundir_1795296460`, 5:31),
uld2 `tr-20261121-213312-25` (`603_25_stormcaller-brundir_1795296846`, 5:30),
uld3 `tr-20261121-212809-24` (`603_24_stormcaller-brundir_1795296542`, 4:07).
Harness clean: no DC veto inside a pull, healers start at full mana, tanks pick up their assignments
inside 10 s.

**Stop rules.** Not polished (31 deaths each). First round.

**Deaths by mechanic.**

| Pull | Phase timeline | Deaths |
|---|---|---|
| uld1 | Brundir 1:43, Molgeim 3:19, Steelbreaker alone 127 s, lowest 7.0% | 24 High Voltage, 2 Fusion Punch, 1 Meltdown, 3 Steelbreaker melee (1 tank), 1 Static Disruption |
| uld2 | Brundir 1:51, Molgeim 3:57, alone 87 s, lowest 43.6% | 26 High Voltage, 1 Fusion Punch, 1 Meltdown, 2 Steelbreaker melee (1 tank), 1 Static Disruption |
| uld3 | Brundir 1:37, Molgeim 3:43, alone 19 s | Molgeim phase: 2 Static Disruption, 1 Rune of Death, 1 High Voltage; then 25 High Voltage, 2 Steelbreaker melee on tanks |

**Picked: the High Voltage cascade, started by healers out of mana.** HV is 73–76% of all damage
taken and 76 of 93 deaths. Every pull had healers dry within 25 s of Steelbreaker's
phase (`--phases`): uld1 priest and paladin under 10% at 3:37/3:44 (18–25 s in), uld2 two of four
under 10% and two at 12–14% at Molgeim's death, uld3 two healers dead and the third priest at 1%. The first deaths in his phase
follow from it: uld2's partner tank took Fusion Punch at 61% health with all four healers at 1–3%
mana, raid mean health 33–35% when the first ranged died.

Contributing bug: **every healer priest cast its one Shadowfiend 8–23 s in at 80–95% mana** (uld1
Uldxac, uld2 Uldyaf/Uldyad, uld3 Uldzae/Uldzac), the full bar capping the return at 5–19%. Cast low it returns about half the
bar: uld2 Uldyaf's second at 5:08, 1.8% → 54.7% 16 s later. Priests are the first healers dry in every
pool (2:49–3:37). Cause: disc and holy run the class `shadowfiend` trigger, `SpellNoCooldownTrigger`
(`DiscPriestStrategy.cpp:153`, `HolyPriestStrategy.cpp:203`); the encounter exempts it from the
burst hold as a mana return (`UldMultipliers_IronAssembly.cpp:70`) but never gates it on mana.

Fix: `iron assembly shadowfiend hold` blocks `shadowfiend` for healers while their mana is above 50%
during the fight (`UldDefinition_IronAssembly.cpp`, `IronAssemblyShadowfiendHeld`). Mana timing only,
so it holds for 10/25 and both modes.

**Questions (plan changes).**
1. *Five healers in the `uld-assembly` 25-man comp?* Recommended: yes. With 4 at ilvl 226 the fight
   runs 5:20+ and the healing runs out before the last phase in all three pools; the Shadowfiend fix
   buys each priest roughly a minute, not the whole phase.
2. *Spread the ranged from Molgeim's phase (Steelbreaker's phase 2) too, closing the known-open Static
   Disruption gap?* Recommended: yes, after question 1. Phase-2 casts hit 10–17 stacked members and the
   +75% nature vulnerability adds 0.24–0.58M of High Voltage per pull, 5–9% of the healing.

**Also seen, not picked.** Fusion Punch kills the swap partner at one charge (uld1 Uldxam 4:21, 43.6k
on a 41k-HP DK whose IBF went at 0:34 and 2:52, AMS on cooldown; uld2 Uldyab 4:56). Holding tank
defensives for it was considered and left: tanks dropped to 8–38% in Molgeim's phase, where those
buttons were what kept them up. Uld1's rebirthed tank took the Meltdown taunt at 65% health and died
3 s later.

**Next window should show.** `--phases`: each healer priest's Shadowfiend at ≤ 50% mana, and the
first "under 10% mana" times later than 2:49–3:37 for priests. Then fewer HV deaths before the first
Meltdown and a later first non-Meltdown death in his phase.

## Round 2 (2026-11-22, build 61011f323 + DC 962eb10: 5 healers at 25)

**Data.** Window 7, one pull per pool, all valid (`batch.py --strict --since 61011f323`), no DC veto,
all wipes in hard mode order (Steelbreaker last): uld1 `tr-20261122-092452-10`
(`603_11_stormcaller-brundir_1795339602`, 6:13), uld2 `tr-20261122-092512-12`
(`603_12_stormcaller-brundir_1795339632`, 5:47), uld3 `tr-20261122-092452-11`
(`603_10_stormcaller-brundir_1795339623`, 5:18). The shared Shadowfiend gate (`e5fe0cc17`) replaced
round 1's per-boss hold on Custom after these pulls; behaviour is the same.

**Stop rules.** Not polished (31/27/30 deaths). Round 1 picked the healer-mana cascade, this round
picks Meltdown, so no three-round repeat.

**Verdict on round 1 (Shadowfiend held to 50%): works.** Every healer priest's fiend went at 47–50%
mana, 1:07–1:25 in (was 80–95% at 8–23 s). First priest under 10%: uld1 4:23 (Uldxac), uld2 4:01
(Uldyaf), uld3 3:42 (Uldzac), against 2:49–3:37; the fifth healer shares the credit. No death before
the first Meltdown in any pool (round 1 uld3 lost four in Molgeim's phase). Steelbreaker alone
155/127/94 s (was 127/87/19), lowest 0.4%/22.8%/23.7% (was 7.0%/43.6%/–). Priests are still the
first healer dry in every pool.

**Deaths by mechanic.**

| Pull | Phase timeline | Deaths |
|---|---|---|
| uld1 | Brundir 1:49, Molgeim 3:34, alone 155 s, lowest 0.4% | 24 High Voltage, 3 Fusion Punch (swapped carriers), 1 Meltdown (carrier, 3 yd off the point), 2 Steelbreaker melee on non-tanks, 1 on a tank |
| uld2 | Brundir 1:50, Molgeim 3:35, alone 127 s, lowest 22.8% | 16 High Voltage, 8 Meltdown (3 carriers + 5 melee), 1 Static Disruption, 1 Steelbreaker melee on a non-tank, 1 on a tank |
| uld3 | Brundir 1:47, Molgeim 3:40, alone 94 s, lowest 23.7% | 20 High Voltage, 7 Meltdown (2 carriers + 5 melee), 1 Static Disruption, 1 Steelbreaker melee on a tank, 1 on a non-tank |

What started each cascade: uld1 a ranged HV death (Uldxas 5:50.96) with him at 0.9% → 16%, four of
five healers under 10% mana; uld2 the third carrier's blast on the melee (5:34.9, 24% → 82%); uld3 the
second carrier's blast on the melee (4:59.0, 23.7% → 83.4%). Uld1's three Fusion Punch deaths are no
extra deaths: each hit a carrier 0.2–2 s after its swap (Uldxam 4:45.9, Uldxaz 5:20.7, Uldxam
5:56.0), the 3 s cast begun before the taunt, so the carrier died ~8 s early without a blast.

**Picked: Meltdown on the melee** (10 deaths in uld2 and uld3, and the cascade start in both). Two
causes:

1. *uld3, a bug: the parked carrier is pulled back to Steelbreaker.* Uldzab (bear) taunted off at
   4:49.2, walked out, `parked` at 4:55.5 at (1606.4, 102.2). At 4:55.9 `reach melee` moved it toward
   (1600.4, 109.3), at 4:56.2 `feral charge - bear` landed it at (1591.9, 119.4), 22 yd back, the run
   then `REFUSED: blocked`; it re-walked and blew at 4:59.03 at (1599.6, 110.2), 14 yd short, 25 yd from
   Steelbreaker (who held still at (1583.2, 129.7) on Uldzaa throughout, so the swap itself held).
   Every parked carrier shows the tug: `reach melee` fired 3–5 times per parked stretch (uld1 Uldxaa,
   uld2 Uldyab, uld3 Uldzaa: run/parked flipping every ~0.6 s at the point), and warriors' Charge fired mid-run in
   uld2 (Uldyaa 4:11.4, cost ~8 yd: blew 8 yd short) and in window 5 (uld1 Uldxaz 5:07, uld2 Uldyaa
   4:33). Cause: `iron assembly movement guard` is `OwnMovement(..., keep = Attack | Reach | ReachHeal)`
   (`UldDefinition_IronAssembly.cpp:91`), so it passes reach; `MOVEMENT_FORCED` on the run outranks
   reach only while the run action fires, and it stops once `parked`; the charge guard covers only
   Brundir's channels. The findings doc claimed the guard kept reach off the carrier; it never did
   (the pre-port multiplier also exempted `ReachTargetAction`).
2. *uld2, the `nopartner` fallback, a plan question.* Uldyaa died at the point 4:18, Uldyab 4:56;
   neither was battle-rezzed (both druids' `rebirth` `USELESS` every ~10 s from 4:18 to 5:25:
   `CastRebirthAction::isUseful` wants the corpse within `SpellDistance` and has no reach, and the
   point is 46 yd from his spot). So the third carrier Uldyaz had `nopartner` at 5:24.8, held on his
   spot by design, and the blast killed 5 melee. The druids stood 43 and 68 yd from the point. Uld1's
   Rebirths went to carriers Fusion Punch killed near the boss; uld3's to the carrier at the point,
   from a cat druid that happened to stand 26.6 yd from it.

**Fix (cause 1).** `iron assembly meltdown carrier hold`: a `Block` for tanks whose Meltdown role is
`run` or `parked`, zeroing `Family::Reach | ReachHeal | Charge` (`UldDefinition_IronAssembly.cpp`,
`IronAssemblyMeltdownCarrierAway`). `late` and `nopartner` carriers keep their reach, since they stay
on the boss. Role derivation is size- and mode-agnostic (10-man Overwhelming Power 64637 lasts 60 s,
same roles); normal mode never empowers Steelbreaker, so it never fires there. pb-check green.

**Questions (plan changes).**
1. *Battle-rez a tank who died at the Meltdown point?* Recommended: yes, while Steelbreaker is alone a
   druid with Rebirth ready walks into range of a dead bot tank's corpse (tanks only) and rezzes it, so
   the next swap has a partner. Uld2 had no partner for the third Overwhelming Power for that reason
   alone, and that blast cost 5 melee and ~48% of his bar on top of the carrier's own 9.7%.
2. *When a carrier holds anyway (`nopartner` or `late`), should melee step out of its 15 yd for the
   last ~3 s?* Recommended: yes, as the fallback behind question 1. Today they never dodge it by
   design; late in a 25-man phase that blast killed 5 melee at 20–24k each. Staying behind
   Steelbreaker keeps most of them in reach (his melee range is 10.8 yd from his centre, the carrier
   ~10 yd in front of it).

**Also seen, not picked.** HV is still 60 of 88 deaths, all inside the cascades above. Uld1 lost at
0.4% to one HV death with healers dry (Uldxac 4:23, Uldxba 4:49, Uldxab 5:28, Uldxad 5:29 under 10%).

**Next window should show.** `--empowered`: every swapped carrier (`run`/`parked`) blows within ~3 yd
of the Meltdown point, and no melee die to a swapped carrier's Meltdown; `postmortem.py --notes
ironassembly.meltdown` shows `parked` without `run`/`parked` flipping; no `reach melee` move and no
`charge`/`feral charge` cast by a carrier in `run`/`parked` (`--bot <carrier>`, or
`VETO iron assembly meltdown carrier hold` rows instead). Melee Meltdown deaths, if any, only from a
`nopartner` or `late` carrier.

**User answers (2026-11-22) to round 2's questions:** yes to both. (1) While Steelbreaker is the
last one up, a druid with Rebirth ready walks into range of a dead tank's body (tanks only) and
rezzes it, so the next swap has a partner. (2) When a carrier has no partner (`nopartner` or `late`),
the melee step away from it for its last 3 s, staying in reach behind Steelbreaker where they can.

## Round 3 (2026-11-22, on Custom 8e71d146b: the user's two answers)

**Data.** None new: round 2 (`acba88ac1`) has not been pulled. Window 7's view (`raidtest.py report
--head 61011f323`) still holds round 2's three pulls, read above. Stop rules, verdict and failure
ranking skipped; this round builds the two decided plan changes. Geometry re-read from those traces
(`603_12_…_1795339632`, uld2): at the third blast (5:34.9) Uldyaz stood on his tank spot 7.6 yd from
Steelbreaker's centre and all 7 melee hugged him at 2.6–5.0 yd, 10.0–11.5 yd off the carrier; 5 died
to Meltdown 0.03–0.27 s before the carrier's own death. When Uldyab died at the point (4:56.4), the
resto druid Uldyba stood 35.2 yd from Uldyaa's body and 41.8 from Uldyab's, the balance druid 59.8 / 67.0.

**Change 1, tank battle rez** (`iron assembly tank battle rez`, `ACTION_RAID + 7`, Mover row). While
Steelbreaker is alone and fewer than two bot tanks are up and not swapped out (pending res, Soulstone
or Reincarnation counts as up), the closest (non-tank druid bot with Rebirth ready and affordable, bot
tank body in the hall) pair is latched in the instance state; the druid casts Rebirth inside 28 yd,
else walks straight at the body to 26 (out of cat or moonkin form only for the cast). Holds while the
walk's end is within 17 yd of a carrier inside its 10 s swap lead, or a cast would still run within 16
yd of a carrier's last 3 s.
In uld2 it would have sent Uldyba 9.2 yd toward Uldyaa at 4:56, the tank back ~4 s later 22 yd from
the tank spot, ~25 s before the next swap. Kept in Iron Assembly's files (`IronAssemblyBattleRezDuty`):
`CastRebirthAction` is untouched, and the class `rebirth` still casts on any body in range.

**Change 2, melee step out** (`iron assembly meltdown step out`, `ACTION_EMERGENCY + 3`, Mover row).
Meltdown 61889 is a SRC-area search on the carrier: 15 yd 3D centre to centre, no reach added
(`WorldObjectSpellAreaTargetCheck`). For a carrier with ≤ 3 s of Overwhelming Power standing within
17 yd + melee range of him, each non-tank melee bot within 16 yd of it goes straight behind him from
it, at melee range − 1 (9.8 yd: 17.4 off a 7.6 yd carrier), or out to 17 when the carrier is under
~7.2 yd from his centre; kept within 22 yd of his designed spot. Generalised past `nopartner` / `late`
to any carrier blowing next to him (a missed taunt or a stuck runner reads the same); a carrier at the
Meltdown point is 46 yd off and never opens the window. Static Disruption skips anyone within 19.5 yd
of his centre (`SelectTarget(Random, 0, -10)` → `IsWithinCombatRange` adds both reaches), so the stack
behind him is safe from it. For the window `iron assembly meltdown melee guard` (OwnMovement, keeps
Attack, passes only the step out and the Rune of Death escape) and `iron assembly meltdown melee charge
guard` (Charge): reach and set behind would walk them back in. The Meltdown `haz` circle's clearance is
17 now.

Reader: `--empowered` adds "N more killed" per blast (deaths ±0.5 s of the carrier's aura ending) and
every Rebirth with its target's role. pb-check green (compile, pblint, codestyle, botobs).

**Next window should show.** `--empowered`: after a tank dies with Steelbreaker alone and fewer than
two bot tanks left, a "Rebirth from <druid> on <tank> (tank)" within ~10 s, and no `nopartner` blast
(`postmortem.py --notes ironassembly.rez`: `walk` → `cast`, `--notes ironassembly.meltdown`). On any
blast next to him (`nopartner`/`late`): no "N more killed", melee `ironassembly.meltstep` `behind` rows
in its last 3 s, and `VETO iron assembly meltdown melee guard` on reach or set behind. Melee uptime in
those 3 s holds (cast rate on him).

## Round 4 (2026-11-22, window 8 on build c8b94dbce + 02925e260, rounds 2 and 3)

**Data.** Window 8, one pull per pool, all valid (`batch.py --strict --since 02925e260`), no DC veto:
uld1 `tr-20261122-112837-7` (`603_7_stormcaller-brundir_1795346976`, wipe 6:01, 34 deaths), uld2
`tr-20261122-112858-8` (`603_9_…_1795347008`, wipe 6:19, 33), uld3 `tr-20261122-112858-9`
(`603_8_…_1795347027`, **kill 5:29, 3 deaths**). The uld3 kill is the first Iron Assembly kill and a
real hard mode one: the last `ironassembly.alive` is `1`, Steelbreaker died last (banner "hard mode
triggered").

**Stop rules.** Not polished: one kill of three. Picks so far: healer mana (r1), Meltdown on the melee
(r2), the user's two answers (r3), Fusion Punch on the tanks (r4), so no three-round repeat.

**Verdict on round 2 (swapped carrier holds its point): works.** Every carrier that stayed swapped blew
0 yd from the Meltdown point (2 of 2 in each pool; window 7 uld3's was 14 yd short). The hold vetoed a
carrier's `reach melee`/`reach spell` 4-5 times a pull and its Charge or Feral Charge once each in uld2
and uld3. No melee died to Meltdown (window 7: 10). The one blast off the point was uld2's third carrier
Uldyaz: parked at the point, it turned `nopartner` when its partner died 2.7 s before the blast and
walked back, blowing 22 yd off the point (one member hit for 20k, no death). Cause: the
`meltdownRunners` latch is only read inside the `IronAssemblySwappedOut` branch, which needs a tank on
Steelbreaker (`UldEncounter_IronAssembly.cpp`, `DeriveIronAssemblyMeltdownRole`).

**Verdict on round 3 (tank battle rez, melee step out): works.** Five duty rezzes, latch to cast in
0.9-6.1 s (uld1 4:53.0 → 4:56.3 and 5:23.5 instant, uld2 5:10.7 → 5:16.8 through one
`hold:meltdown`, uld3 4:40.0 → 4:41.0 and 5:16.2 → 5:18.4). By design they wait until the second
tank swaps out, 27-32 s after the Meltdown death. Two limits: the budget is the non-tank druids'
Rebirths (2 in rosters B and C, 3 in A), and a class `rebirth` on a tank killed in Molgeim's phase
spends one (uld1 3:02, uld2 3:36), so uld2's third tank death (Uldyab at the point, 5:20.5) read
`none:nodruid`. A rezzed tank stands up without raid buffs: uld2 Uldyaa had no Kings, Fortitude or Gift
of the Wild at his 5:53.7 death, 41.8k max health against 48.5k before. The step out never had a blast
next to him to answer: uld2's walk-back carrier ended 22 yd out and all 7 melee read `meltstep` `clear`.

**Deaths by mechanic.**

| Pull | Phase timeline | Deaths |
|---|---|---|
| uld1 | Brundir 1:44, Molgeim 3:44, alone 133 s, lowest 23.1% (5:18) | Molgeim phase: 1 Fusion Punch (tank). Alone: 22 High Voltage, 3 Fusion Punch (carrier 20 s in at 3 charges, partner at 5, a melee at 32), 3 Static Disruption, 2 Meltdown (carriers at the point), 2 Steelbreaker melee on non-tanks, 1 on a tank |
| uld2 | Brundir 1:51, Molgeim 4:01, alone 134 s, lowest 9.3% (5:53.6) | Molgeim phase: 1 Fusion Punch (tank). Alone: 24 High Voltage, 4 Steelbreaker melee on non-tanks, 3 Meltdown (carriers: 2 at the point, 1 at 22 yd), 1 Fusion Punch (partner, 2 charges) |
| uld3 | Brundir 1:45, Molgeim 3:31, alone 119 s, **kill** | 2 Meltdown (carriers at the point), 1 Fusion Punch (carrier 1.8 s after its swap, 2 charges) |

What started each wipe: uld1, healers dry (4 of 5 under 10% by 5:19, Uldxba from 3:52), a ranged High
Voltage death at 5:18 (2 charges), then Fusion Punch on the carrier before its swap (5:23, 59.1k at
79%) and on the rezzed partner (5:39, 63.3k at 81%). uld2, Steelbreaker at **9.3%** when Fusion Punch
killed partner Uldyaa at 86% (5:53.7, 44.3k, 2 charges) with no button left and no tank behind him;
he went to the ranged and the charges took the raid.

**Picked: Fusion Punch kills tanks** (6 tank deaths over the window, part of both wipes). Two causes:

1. *Before phase 3, a bug: the punch defensive waits for Steelbreaker to be alone.*
   `IronAssemblyFusionPunchIncomingMs` returned 0 unless `IsSteelbreakerEmpowered`, but his punch is
   cast all fight and kills a low tank in Molgeim's phase too. uld1 Uldxaa (paladin, 45.2k) died at
   3:01.4 to 22.8k at 42% with Divine Protection castable through the whole cast (no Forbearance, no
   Avenging Wrath marker until his own Avenging Wrath at 3:00.87): healers put 3.4k on him in the 16 s
   before against 52.8k taken. uld2 Uldyaa (warrior, 48.5k) died at 3:31.6 to 35.2k at 45% with Last
   Stand ready (cast at 3:51.5 after his Rebirth), unresisted because Aspect of the Wild was off him
   3:11-3:37. Each death spent a druid's Rebirth that phase 3 then lacked.
2. *In phase 3, a plan question: no button left.* The node fired on every punch, weakest first, but
   the class nodes spend the same buttons on health alone: uld2 Uldyaa's Shield Wall 3 s after his
   Rebirth (5:19.9, not tanking) and Last Stand at 3:51.5; uld1 Uldxaz's Last Stand (4:57.4) and Shield
   Wall (5:07.9, after the punch it was meant for) in one 10 s stint, so the 3-charge punch at 5:23
   found nothing; uld3's death knight Uldzaz (44.1k, about 25 talent points) had used Anti-Magic Shell
   and Icebound Fortitude on the two punches before.

**Fix (cause 1).** `IronAssemblyFusionPunchIncomingMs` answers in every phase, but only for a punch that
would leave the tank under `RAID_DEFENSIVE_FLOOR_PCT` (25%): the top of the roll (35,000 25-man, 21,150
10-man) × Steelbreaker's `MOD_DAMAGE_PERCENT_DONE` for nature (Supercharge, Electrical Charge, Berserk) ×
(1 − the tank's average nature resist, `Unit::GetEffectiveResistChance`) × its `MOD_DAMAGE_PERCENT_TAKEN`,
absorbs ignored, the floor test `RaidDefensive`'s `EndsUnderFloor` uses. Before phase 3 a punch on a
full tank reads `safe` (expected 22–28k) and spends nothing; it reads `under` below 25% + expected/max
health, 74–91% on these 43–50k tanks; in phase 3 most punches still read `under`, though a full bear
(60k+) should read `safe` at 0 charges and keep its button. New probe `ironassembly.punch` (`under`/`safe` + expected k, `idle` between punches); reader
`--punches` lists every punch in every phase with verdict and button, `--empowered` adds the verdict.
Holds for 10 and 25 (the roll is read off the cast spell) and both modes (normal mode only meets his
first-phase punches). pb-check green.

**Review.** The lane's version judged a punch from its cast start on current health. Replayed on
window 8, that reads `under` on four punches before phase 3 besides the two that killed, all four
landing at 88–100%: uld2 Uldyaa's 2:01.8 punch (77% mid cast, 100% at the hit) would have spent Last
Stand, the button his 3:31.6 death had ready (his Shield Wall went at 2:37). Now judged only in the
cast's last 1.5 s (`ULDUAR_IRON_ASSEMBLY_FUSION_PUNCH_DECIDE_MS`): uld2's 2:01.8 and 3:51.5 read
`safe`, both deaths still `under`, and every traced punch had a tank action in that window (48 of 57
in the last 1 s). Uld1 Uldxaa's 2:45.6 punch still reads `under` (73% in the window; fully absorbed,
which the estimate ignores), so Divine Protection likely goes there and the 3:01.4 punch still finds
none: a Molgeim-phase punch is expected to take about two thirds of his health.

**Questions (plan changes).**
1. *Hold the tanks' class defensive buttons through Iron Assembly (above 25% health), so only the
   Fusion Punch node spends them, as Hodir's round 2 does?* Recommended: yes. uld2 was a likely kill:
   Steelbreaker at 9.3% when the partner died to a punch that a ready Shield Wall (60%) would have left
   at ~18k, and that Shield Wall went 3 s after his Rebirth with nothing on him.
2. *Roster warlocks soulstone the tanks?* Recommended: yes. The roster design says "a soulstone per
   tank", but bots default to `ss self` (`AiFactory.cpp:657`): in uld1 and uld2 every soulstone sat on
   a warlock, and no tank died with one. Three tank soulstones would cover three tank deaths without a Rebirth. A DC roster setting
   (`nc +ss tank`) or the fork's default for roster bots; not this boss's code.
3. *Rebuff a tank the duty rezzed, in combat?* Recommended: yes, after 1 and 2: the class buff nodes are
   non-combat only, so a rezzed partner stays ~14% short on health and loses Gift of the Wild's resist
   for the rest of the pull.

**Also seen, not picked.** The walk-back carrier above (0 deaths; recommended: a latched runner stays
parked when it can't reach Steelbreaker with time to spare). uld1's healer mana (round 1's failure; the
comp is at its 5-healer cap). The roster death knights' missing talents show here twice: DK tanks at
44k with no Vampiric Blood (`IMPOSSIBLE`; uld3's Uldzaz died at 100% to a 2-charge punch), and a raid that needs
119-134 s alone with him, the race the charges decide.

**Next window should show.** `--punches`: before Steelbreaker is alone, a punch on a tank still under
the bar in its cast's last 1.5 s reads `under` and spends a button (`defensive <name>`), or `none`
when nothing was ready; punches on a tank topped up by then read `safe` with no defensive. Fewer tank
deaths to Fusion Punch in Molgeim's phase (window 8: 2 in 3 pulls) and no class `rebirth` spent there on a tank. Phase-3 deaths to punches
on tanks with no button stay until question 1 is answered.

**User answers (2026-11-22) to round 4:** yes to both. (1) Hold the tanks' class defensive buttons
above 25% health through Iron Assembly, so only the Fusion Punch node spends them (as Hodir round
2). (2) Rebuff a battle-rezzed tank in combat.

## Round 5 (2026-11-22, on Custom eda15422d: the user's two answers)

**Data.** None new: round 4 (`ab0d6db9d`) is merged but not pulled, and window 9 pulls Iron Assembly on
`1b45df985` (rounds 2-3 code). Stop rules, verdict and failure ranking skipped; this round builds the
two decided plan changes. Window 8's traces (`603_7`, `603_8`, `603_9`) re-read for both. Seven tank
rises: six at 16-19% health (Rebirth), one at 100%; each had a druid, a priest and a paladin bot within
30 yd 1-6 s later. No class defensive went in a rise's first second; uld2 Uldyaa's Shield Wall 1.2 s
after his 5:18.7 rise went at ~60% health with nothing on him. What the risen tanks lacked at their next
death or the end (new `--revived`): Kings and Might on warrior/DK tanks, plus Wisdom on a bear, plus
Sanctuary and Arcane Intellect on a paladin tank, and Fortitude and Mark of the Wild on all.

**Change 1, tank defensive hold** (`iron assembly tank defensive hold`, Block, `Role::Tank`,
`HeldTankDefensiveNames()`), as Hodir round 2: while the council is engaged, a tank at 25%+ health
gets no class Last Stand, Shield Wall, Divine Protection, Barkskin, Frenzied Regeneration, Survival
Instincts, Anti-Magic Shell, Vampiric Blood or Icebound Fortitude. The Fusion Punch node casts by spell
id, so it's untouched. Holds for 10/25 and both modes.

**Change 2, in-combat rebuff** (`iron assembly tank rebuff`, `ACTION_RAID + 2`,
`IronAssemblyTankRebuffDuty`, probe `ironassembly.rebuff`). Any tank seen dead and up again (Rebirth,
Soulstone, a class rez) is risen for 30 s. In that window, from bots already within 30 yd and in sight
that know the spell, are off cooldown and can pay for it: Mark of the Wild, Power Word: Fortitude and,
on a paladin tank, Arcane Intellect, one bot each (dps before healers, then guid; never a bear); and each
blessing the tank's role gets, from a different paladin (a paladin already blessing it is spent), by the
class planner's per-member wants (role priorities, no Sanctuary under Renewed Hope bar a paladin tank's,
at most one per bot paladin) with paladin tanks last. Not the planner itself: it picks one cast for the
whole raid, so a hunter's pet revived in the window could block a tank's blessing for good. A cat
shifts out for Mark of the Wild; moonkin and Tree of Life cast it in form (stance forms to the core).
Single-target ranks only, no reagents. The window covers DC `4d3978b`'s tank soulstones too.

Reader: `--revived` lists each tank's rise, the raid buffs put back inside the window with their
caster, `none:nobody` / `hold:cast` verdicts on it, and what it still lacked at its next death or the
end against its first death. pb-check green (compile, pblint, codestyle, botobs).

**Shared code.** None changed. The rebuff reads the paladin blessing registry and role priorities
(`PaladinBlessingHelper.h`, `PaladinBlessingTypes.h`) as they are.

**Review.** Window 8's rises re-checked with the new reader: seven, at 16-19% bar Uldyaa's 3:37.8 at
100%, a druid, a priest and a paladin bot within 30 yd of each 1-6 s later; uld2 Uldyaa's Shield Wall
at 5:19.9 (59.6%) and uld3 Uldzab's Barkskin at 5:21.0 (82.6%) are the class casts the hold now stops.
Two fixes. Arcane Intellect keyed on the tank's power type, and a bear rises from Rebirth in caster
form on mana (uld3 Uldzab had a mage 28 yd off 1 s after its rise), so a mage would spend a GCD on it: now
paladin tanks only, as the reader already assumed. A paladin's `ironassembly.rebuff` verdict carried
the previous tank's `none:nobody` onto a later, fully blessed tank (two tanks share a window in uld1,
4:58-5:55); it now names only a tank still lacking a blessing, `none:nobody` over `none:other`.

**Watch.** Round 1 left the hold out because tanks dropped to 8-38% in Molgeim's phase, where those
buttons kept them up; between 25 and 38% they now wait for a dangerous punch.

**Next window should show.** `--revived`: within ~5 s of each rise the tank's Mark of the Wild,
Fortitude and blessings back, and at its next death or the end "every raid buff back" (Arcane
Intellect only on a paladin tank); `none:nobody` only with no buffer within 30 yd. One expected gap:
a paladin tank with three bot paladins (uld1's roster) gets its own wants, Sanctuary, Might and Wisdom, so
"without Blessing of Kings" there is the class-wide greater it died with, not a miss. `postmortem.py`
vetoes: `iron assembly tank defensive hold` on class defensives at 25%+ health, none cast there.
`--punches`: phase-3 punches on tanks find a button (`defensive <name>`), fewer `no defensive` than
window 8, and fewer tank deaths to Fusion Punch. Check tank deaths in Molgeim's phase against window 8
(2 in 3 pulls, both punches) for the risk above.

## Round 6 (2026-11-22, window on build 697052475: rounds 4 and 5, re-specced DKs, tank soulstones)

**Data.** `raidtest.py report --boss iron-assembly --raid uld --head 697052475 --pulls 10`: one pull per
pool, all valid (`batch.py --strict --since 697052475`), no DC veto, hard mode on: uld1
`tr-20261122-163545-7` (`603_10_stormcaller-brundir_1795365408`, **kill 5:27, 3 deaths**, Steelbreaker
last), uld2 `tr-20261122-163606-9` (`603_11_…_1795365440`, **kill 6:06, 4 deaths**, Steelbreaker last),
uld3 `tr-20261122-163545-8` (`603_9_…_1795365427`, wipe 5:43, 32 deaths, Steelbreaker last, lowest 13.0%).

**Stop rules.** Not polished: uld2 has 4 deaths and uld3 wiped. Picks so far: healer mana (r1), Meltdown
on the melee (r2), the user's answers (r3), Fusion Punch on the tanks (r4), the user's answers on it
(r5); this round picks Fusion Punch on the tanks a second time, so a round 7 pick of it that still shows
hits the three-round rule.

**Deaths by mechanic.**

| Pull | Phase timeline | Deaths |
|---|---|---|
| uld1 | Brundir 1:39, Molgeim 3:26, alone 121 s, **kill** | 1 Meltdown (carrier at the point), 2 Fusion Punch on carriers (Uldxam 4:33.5, 24 s into its Overwhelming Power, 1 charge; Uldxaa 5:08.8, 23 s in, 2 charges, High Voltage the last hit) |
| uld2 | Brundir 1:43, Molgeim 3:59, alone 127 s, **kill**, lowest 0.1% | 2 Meltdown (carriers at the point), 1 Fusion Punch on a carrier (Uldyaa 5:32.7, 13 s in, 2 charges), 1 High Voltage (healer Uldyaf 5:57.0, 3 charges) |
| uld3 | Brundir 1:41, Molgeim 3:26, alone 133 s, 17.5% at 5:11 | 2 Meltdown, 2 Fusion Punch on carriers (Uldzaa 5:14.0 running out, 3 charges; Uldzab 5:29.4, 7 s in, 6 charges), 23 High Voltage, 3 Steelbreaker melee on tanks, 1 on a non-tank, 1 Static Disruption |

Every phase-3 punch death was a carrier 7-28 s before its Meltdown: the same one charge it would have
cost there, only earlier (longer High Voltage at the higher count, a soulstone or Rebirth spent sooner).
The deaths past one per Overwhelming Power are healers: uld2's Uldyaf at 5:57, and uld3's cascade from
healer Uldzad at 5:06.0 (Static Disruption 6.8k + High Voltage, priests Uldzac and Uldzae under 10% mana
since 3:49 and 4:06), +1 charge each, then the 6-charge punch and Steelbreaker melee on the third tank.

**Verdict on round 4 (punch defensive in every phase, judged in the cast's last 1.5 s): works.** No tank
died before phase 3 (window 8: 2) and no Rebirth went on a tank there. Before phase 3, uld2 and uld3 read
every punch `safe` and spent nothing; uld1 read three `under`: 1:40.7 cast Anti-Magic Shell, 3:06.9 and
3:22.2 found none ready (Uldxaa's own Avenging Wrath at 3:01.07 put up the 30 s marker that locks Divine
Protection) and were absorbed.

**Verdict on round 5 (tank defensive hold above 25%, rebuff a risen tank): works, one gap.** The hold's
veto rows (3 / 44 / 38: Barkskin, Frenzied Regeneration, Shield Wall, Survival Instincts, Icebound
Fortitude) kept every class cast above 25% off, and the punch node spent the buttons in phase 3 (uld1 Anti-Magic Shell
4:14, Divine Protection 4:48.6, Anti-Magic Shell 5:24; uld2 Barkskin 4:55, Shield Wall 5:13, Divine
Protection 5:50.7; uld3 Barkskin 4:21.8). Rebuff: every rise got its raid buffs back, all "every raid
buff back" at the next death or the end bar uld1 Uldxaa's Kings (the expected three-paladin gap). The
reader had shown uld2 Uldyaa ending without Kings and Might: both landed at 5:34.517, before its first
live sample at 5:34.608, which `--revived` used as the window start; fixed (from the death).

**Verdict on round 3 (tank battle rez, melee step out): not exercised.** `ironassembly.rez` read
`none:tanks` in every pull: soulstones (DC `4d3978b`, `Use Soulstone` 47882) raised 7 of 10 dead tanks
0.3-0.5 s after death with three tanks in the comp, and the three tank Rebirths (uld1 5:09.3, uld2 5:33.0, uld3 5:14.5) were the class
`rebirth` on a punch-killed carrier next to the boss. Every carrier blew 0 yd from the point (5 of 5), so
`meltstep` read only `none`.

**Picked: phase-3 Fusion Punch deaths on tanks with no button left**, the gap in round 5's hold. New
`--defensives` lists six class casts with nothing on the tank (1 / 2 / 3), each under 25%, which the
hold lets through as a last resort:

- uld2 Uldyaa Last Stand 4:43.2 at 13%, 0.4 s after its soulstone rise at the point (Steelbreaker on
  Uldyab). The punch node then spent Shield Wall at 5:13.2, so its 5:31.4 punch (`under 50k` at 100%)
  found nothing and killed it at 5:32.7. With Last Stand kept, 5:13 takes Last Stand, which still runs at
  5:31 (20 s), so the node reads `covered` and the punch lands in its last 0.5 s (see the review below).
- uld3 Uldzaa Divine Shield 4:10.3 at 13%, 0.7 s after its rise (Steelbreaker on Uldzab). Its Forbearance
  (25771, 2 min) locked Divine Protection through the punches at 4:38.3, 4:54.1 and 5:12.7, all `under`
  with no act row from the punch node (its trigger needs a castable button); Ardent Defender (31852)
  absorbed the first, the third killed it at 5:14.0.
- uld3 Uldzab Survival Instincts 4:47.0 at 15%, Uldzaa Divine Protection 5:17.6 at 17%, uld2 Uldyab
  Frenzied Regeneration 5:21.2 at 14%, all just risen; uld1 Uldxaa Divine Shield 3:59.6 at 20% as a
  carrier walking to its point (swapped at 3:58.8, the punch cast before the swap landed at 3:59.58).

Cause: `IronAssemblyTankDefensiveHeld` (`UldDefinition_IronAssembly.cpp:62` before) holds only at 25%+,
and every rise starts at 13-19%. Divine Shield and a self Lay on Hands (`GenericPaladinStrategy.cpp:25-26`,
"critical health") are not in `HeldTankDefensiveNames()` at all, and both put up Forbearance
(`spell_pal_immunities`, `spell_pal_lay_on_hands`). The three true last resorts in the window had
Steelbreaker on the tank: uld2 Uldyab Survival Instincts after a 32.3k punch, uld3 Uldzab Frenzied
Regeneration after a 17.4k swing, uld1 Uldxam Icebound Fortitude at 8%.

**Fix (bug: only the punch node was meant to spend them).** Under 25% the hold now lets a class
defensive through only while a living council member has the tank as victim
(`UldDefinition_IronAssembly.cpp:67`), and its name list adds `divine shield` and `lay on hands` (the
self cast; `lay on hands on party` stays free). Holds for 10/25 and both modes (no size or mode input;
normal mode only meets it before Steelbreaker dies). Reader: `--defensives` (every tank defensive with
health, the member on it, punch or class node, and a count of class casts with nothing on the tank);
`--revived` starts its window at the death. pb-check green.

**Also seen, not picked.**
- *Shared, uld1 Uldxam 4:32.2:* the punch node picked Vampiric Blood with no Blood rune free (Death Strike,
  Heart Strike, Icy Touch all failing that tick); `CanCastSpell` checks no power
  (`PlayerbotAI.cpp:3587`, `TRIGGERED_IGNORE_POWER_AND_REAGENT_COST`), so `PickDefensive`
  (`RaidTankDefensive.cpp:101`) offered it, the cast failed every tick and the ready Icebound Fortitude
  (vetoed 3 times by the hold) never went; a 41.7k punch at 92% killed it 1.3 s later. The fix (skip a
  row the bot can't pay runes or runic power for, as `SideAoeRunesReady` in `ToCHelpers_Anubarak.cpp`
  does) is shared: every boss on `NextTankDefensive` / `NextMagicDefensive` / `NextPhysicalDefensive`.
- A paladin tank casts Avenging Wrath on cooldown (0:01, 3:01; the burst hold skips tanks), and its 30 s
  marker locks Divine Protection through Molgeim's last punches (uld1 3:06.9, 3:22.2; window 8 uld1's
  3:01.4 death). Boss-local candidate for a later round.
- Healers: priests still the first dry (uld3 3:49 and 4:06, 23-40 s into phase 3); uld3's resto shaman
  read Mana Tide Totem `IMPOSSIBLE` at 3:16, 4:39 and 4:49.8 and dropped it at 5:17 at 2% mana. The
  extra deaths now come from here; the likely round 7 pick.

**Next window should show.** `--defensives`: "class casts with nothing on the tank: 0" in every pull
(1 / 2 / 3 here), and `postmortem.py --vetoes` rows `iron assembly tank defensive hold` on `last stand`,
`survival instincts`, `frenzied regeneration`, `divine protection`, `divine shield` or `lay on hands` in
the seconds after a rise. Class casts under 25% with Steelbreaker on the tank still go. `--punches`: a
risen warrior's or paladin's next `under` punches find a button (Last Stand, or `covered` while it runs;
Divine Protection with no Divine Shield before it), and fewer carriers die to a punch before their
Meltdown (5 here). Risk to watch: no risen tank dying to High Voltage before a heal lands, now that the
rise's class casts no longer pad it (none of 10 rises here died inside 5 s).

**Review.** The six class casts, the rises, Forbearance on uld3 Uldzaa (4:10.3 until its 5:14.0 death)
and the true last resorts re-read from the traces; root cause and fix hold. One fix: the hold also
applied with Steelbreaker dead, where no punch is left to save a button for (normal mode's kill order,
Molgeim or Brundir on the tank above 25%); it now needs him alive. Left, shared: `PickDefensive`
(`RaidTankDefensive.cpp:92`) returns any running row as `covered` whatever its remaining time, and Last
Stand (12976, `SPELL_AURA_MOD_INCREASE_HEALTH`) takes its 30% back on fade down to 1 hp
(`HandleAuraModIncreaseHealth`). Punches come 15-20 s apart, so a Last Stand spent on one still covers
the next and fades right after it lands (uld2 Uldyaa's 5:13 / 5:31 pair above). If round 7 shows a
warrior dying just after a `covered` punch, the fix is shared: count a running row only if it outlasts
the landing, and not Last Stand into its fade.

## Round 7 (2026-11-22, window 11 on build d2874f536: round 6)

**Data.** `raidtest.py report --boss iron-assembly --raid uld --head d2874f536 --pulls 10`: one pull per
pool, all valid (`batch.py --strict --since d2874f536`), no DC veto, hard mode on: uld1
`tr-20261122-183602-7` (`603_7_stormcaller-brundir_1795372611`, **kill 5:07, 2 deaths**, Steelbreaker
last, hard mode triggered), uld2 `tr-20261122-183602-8` (`603_8_…_1795372614`, wipe 5:49, 33 deaths),
uld3 `tr-20261122-183643-9` (`603_9_…_1795372644`, wipe 5:34, 33 deaths, lowest 4.4%). Window 10's
traces (`603_10_…_1795365408`, `603_11_…_1795365440`, `603_9_…_1795365427`) re-read for the stall below.

**Stop rules.** Not polished: two wipes. Picks so far: healer mana (r1), Meltdown on the melee (r2), the
user's answers (r3, r5), Fusion Punch on the tanks (r4, r6). Fusion Punch killed 2 tanks outside a
cascade here (window 10: 5 carriers), so it is not this round's pick and the three-round rule doesn't
fire. This round picks a risen tank stranded at the Meltdown point (new) and asks about healer mana.

**Deaths by mechanic.**

| Pull | Phase timeline | Deaths |
|---|---|---|
| uld1 | Brundir 1:44, Molgeim 3:23, alone 104 s, **kill** | 2 Meltdown (carriers at the point) |
| uld2 | Brundir 1:44, Molgeim 3:39, alone 126 s, lowest 24.9% | Molgeim phase: 1 Fusion Punch (tank Uldyaa 3:22.1 at 28% before the hit). Alone: 2 Meltdown, the carrier Uldyaz to three Steelbreaker swings (5:15.6, 17 s into its Overwhelming Power), then 23 High Voltage, 2 Fusion Punch (Uldyab 5:23.8 at 3 charges, Uldyaa at 23), 2 Static Disruption, 1 more Steelbreaker melee on a tank, 1 on a non-tank |
| uld3 | Brundir 1:33, Molgeim 3:16, alone 134 s, lowest 4.4% (5:10.6) | Alone: 3 Meltdown, risen tank Uldzab to High Voltage (4:42.0, 7 s after its Soulstone), then 26 High Voltage, 2 Steelbreaker melee on tanks, 1 Fusion Punch (Uldzaz at 14 charges) |

What started each wipe. uld2, healer mana: priests Uldyad and Uldyaf under 10% from 3:45 and 3:54 (6-15
s into phase 3) to the end; at 5:12-5:15 the carrier Uldyaz took 16.6k, 18.6k and 14.9k swings from 91%
with only HoT ticks and one Prayer of Mending on it since 5:06 (paladin Uldyac 11-16% mana), raid mean
71% → 54%. uld3, the stranded risen tank: Uldzab died at the Meltdown point, +1 charge and +9.7% to a
Steelbreaker who stood at 4.4% when the third carrier blew (5:10.6); the cascade began at 5:18 with a
ranged High Voltage death, raid mean 46%, priests dry since 4:17 and 4:29.

**Verdict on round 6 (class defensives under 25% only while a council member hits the tank): works, one
gap.**
- `--defensives` "class casts with nothing on the tank": 0 / 0 / 1 (window 10: 1 / 2 / 3). The one is uld3
  Uldzaa (paladin) at 3:59.6, 0.5 s after its Soulstone rise at the point at 14%: `lay on hands on party`
  with itself as target. The hold names `lay on hands` (the self node) and leaves `lay on hands on party`
  free, whose target can be the caster. Its Forbearance left the next two punches (4:28.8 `under 41k`,
  4:48.7 `under 46k`) with no button; both landed safe (34.9k at 98%, fully absorbed). It also lifted a
  tank stranded 45 yd from its spot from 14% to full.
- Vetoes after rises: uld1 Uldxam IBF and Vampiric Blood (4:45), uld2 Uldyaa Last Stand and Shield Wall
  (4:51), Uldyab Survival Instincts, Frenzied Regeneration, Barkskin (4:59-5:00), Uldyaz Lay on Hands,
  Divine Shield, Divine Protection (5:16.7), uld3 Uldzab Survival Instincts, Frenzied Regeneration,
  Barkskin (4:35.6-4:36.8), Uldzaz Vampiric Blood (5:26.6). Last resorts with Steelbreaker on the tank
  still went: uld1 Uldxam IBF at 12% (4:25.6), uld2 Uldyab Survival Instincts at 16% (5:19.4).
- Risen tanks' next punch: uld2 Uldyaa (warrior) rose 3:22.5 at 13%, its 3:40.9 punch read `under` and
  got Shield Wall (20.3k at 74%). uld3 Uldzaa (paladin): none, the Lay on Hands above. uld2 Uldyab (bear)
  rose 4:58.6; its 5:23.8 punch (`under 62k`) read covered by its own Survival Instincts and killed it
  (54.9k on 42.2k health).
- Carriers dying to a punch before their Meltdown: 0 (window 10: 5).
- The named risk, a risen tank dying to High Voltage before a heal: uld3 Uldzab at 4:42.0. Not the hold:
  its held buttons were unusable (Survival Instincts in caster form, Frenzied Regeneration at ~10 rage,
  Barkskin on cooldown from the 4:09 punch). Cause below.

**Did round 6 make the wipes worse? No.** Every button it held in uld2 and uld3 was unusable (form,
cooldown, the Avenging Wrath marker), worth little (Frenzied Regeneration on a bear just up with ~10
rage) or held inside the cascade, bar uld2 Uldyaz's Lay on Hands and
Divine Shield at 5:16.7 (its Forbearance dropped at its death): it lived 20 s more and died at 5:36.5 to a
43.9k swing at 6 charges, past what either would have changed.

**Healer mana: confirmed as the background of both cascades, but not a strategy bug.** Priests under 10%:
uld1 4:29 / 4:43, uld2 3:45 / 3:54, uld3 4:17 / 4:29. Every mana return went out: Shadowfiend at 49-51%
(0:52-1:13), Hymn of Hope 2:14-3:35, Mana Tide 3:39 / 4:17 / 3:08 (window 10's late one didn't repeat),
one mana potion per healer, Innervate. Two of seven Innervates reached a priest, both from cat druids
(`innervate on healer`: uld1 4:04, uld3 3:36); balance and resto cast their own on themselves (uld1 3:56,
4:41; uld2 2:53, 4:53; uld3 5:07), so uld2 (roster B: balance and resto, no cat) put none on priests
dry from 3:54. Overheal is ~40% in Brundir's phase (1.0-1.2M on 1.5-1.8M healed on tanks): class healing
logic, shared. Plan question 1.

**Picked: a risen tank stranded at the Meltdown point (bug).** A Soulstone raises a carrier where it blew,
46 yd from his tank spot. uld3 Uldzab: the tank assignment walk went out at 4:35.64, the bot shifted to
bear, `healthstone` fired while it moved and `UseItemAction` stopped it (`UseItemAction.cpp:337`: only a
potion is usable on the move), the healing potion went, and every re-send of the same point read `dup`
(`IsDuplicateMove`, `MaxWaitForMove` 5000 ms) until 4:40.6. It stood at (1606.7, 101.5) from 4:36.0 to
4:41.0 at 8-20%, healers 42-63 yd off, and died at 4:42.0 to the second High Voltage tick, 10 yd from the
point. Every Soulstone rise at the point in the two wipes stalled the same way: uld2 Uldyab 5:00.3-5:03.9
(3.5 s, 42 yd off, a healer happened to be in reach), uld3 Uldzaa 4:00.8-4:04.6 (3.8 s, at 81% after its
Lay on Hands); window 10 uld3 Uldzab 4:48.1-4:51.5 (3.4 s). Shorter stalls of the same walk: 1.2-3.3 s
at the pull (18-34 yd off) and 1-3.3 s mid fight.

**Fix.** `IronAssemblyTankAssignmentAction::WalkToSpot` (`UldActions_IronAssembly.cpp:188`): a `Duplicate`
while the bot stands (not moving, not casting) is re-sent on the second such tick after clearing `last
movement`, as the battle rez walk does. The first still tick goes to the lower nodes: re-sent at once, the
walk (relevance 66) outranks the healthstone (21), which would stop every re-sent walk. On uld3 Uldzab
that walks again at ~4:36.8 instead of 4:41. No size, mode or real-player input. Reader: `--revived` adds
per rise "walk: a healer in 40 yd after X s, stood Y s on the way" (uld3 here: 7.5 / 4.8 s and 6.8 / 4.8
s; every other rise in windows 10-11 at once or within 2.6 s, bar uld2 Uldyaa's 3:22.5 rise at 4.2 / 3.5 s,
held 10 yd off its spot by design: a Rune of Death on the spot from 2:56 to 3:26, the route step refuses
and no `move` row goes out). pb-check green.

**PickDefensive (shared, waiting on the user): 0 deaths this window.** Unaffordable button once: uld1
Uldxam 4:24.2 got Vampiric Blood, the cast failed, the class IBF at 12% saved it. Fading Last Stand as
cover: none (uld1 Uldxaz's ran out 0.2 s before the 5:03 punch and the node cast Shield Wall). Related,
not the two named: a running Survival Instincts read as cover for uld2 Uldyab's 5:23.8 punch with Barkskin
ready (its 20% leaves it ~1.7k short), and weakest-first gave uld2 Uldyaa Last Stand at 28% for the 3:22
punch (died by 1.4k; Shield Wall's 60% without Last Stand's health leaves it ~1.4k short too).

**Also seen, not picked.** uld2's Molgeim-phase tank death: a Rune of Death on the stack point (2:55.9)
sent the stack to the 270° heading (1587.18, 96.02) while Steelbreaker's tank escaped to (1572.0, 141.6),
48 yd apart; no healer within 40 yd of it from ~3:00 to its 3:22 death (88% → 27% on HoTs, then the
punch), and again 8 s after the 3:33 rune. In uld1 and uld3 the runes landed off the point and the healers
stayed in reach (median 28-38 yd). `DisplaceIronAssemblyStackPoint` takes the clear heading nearest the
stack, tanks unread. Plan question 2.

**Questions (plan changes).**
1. *Balance and resto druid bots give Innervate to a healer low on mana in Iron Assembly, as the cat's
   `innervate on healer` does, instead of themselves?* Recommended: yes. Five of seven went on the druid
   itself; uld2 had no cat and its priests sat dry from 3:54 to the wipe. Each one is ~7.9k mana over 10 s,
   close to a third of a priest's bar.
2. *When a Rune of Death moves the stack, prefer a clear heading that keeps Steelbreaker's tank within 40
   yd?* Recommended: yes, in Molgeim's phase: his punch is what kills a tank there. In uld2 the other
   heading equally near the stack (90°) was 16 yd from him, though 43 yd from Molgeim's tank for the rune's
   30 s.
3. *Hold a paladin tank's `lay on hands on party` when it targets itself (round 6's gap)?* Recommended: not
   yet. Its one cast lifted a tank stranded at the point to full, and the two punches its Forbearance left
   without Divine Protection landed safe; judge it once risen tanks walk straight back.

**Next window should show.** `--revived`: every rise "a healer in 40 yd" within ~3 s and "stood" ≤ ~1 s
(uld3 here: 7.5 / 4.8 s and 6.8 / 4.8 s), bar a rise held off its spot by a Rune of Death on it (no
`move` rows from the walk while it lasts); no risen tank dying to High Voltage at the Meltdown point.
`postmortem.py --bot <risen tank>`: `move ... r:dup` from `iron assembly tank assignment action` lasts one
still tick, then an `ok:1` re-send. Healer mana unchanged until question 1.

**Review.** Root cause holds in the trace: uld3 Uldzab's walk went out at 4:35.639, `healthstone` FAILED
at 4:35.986 on the move (`UseItemAction` stops the bot first), the track sits at (1606.7, 101.5) from
4:35.98 to 4:40.77 and every walk tick reads `dup` to 4:40.603. Uld2 Uldyab (4:59.578), uld3 Uldzaa
(3:59.975) and window 10 uld3 Uldzab (4:47.300) start the same way, each with a `healthstone` FAILED on
the move. The fix holds at 10 and 25, both modes, and with a player tank (it re-sends only a bot tank's
own walk; no size, mode or player input). It also shortens another stall: after a Charge or Feral Charge
at the pull or mid fight a tank stood 2-3 s at the boss, 4-34 yd from its spot, with the walk `dup`
(every pull of windows 10-11); it now walks back ~1 s after the charge, as "stand, do not drag" intends.
Watch the pull for a council member leaving its tank in the first 5 s. The branch predates the comment
strip `c163ec11a`: the two source files it touches are stripped here (`commentstrip.py strip --keep-from
c163ec11a`, token check clean, now equal to Custom's bar the fix), so the merge carries no comment and
the other Ulduar files come from Custom as they are.

## Round 8 (2026-11-22, window 12 on build ebe746ad1: round 7)

**Data.** `raidtest.py report --boss iron-assembly --raid uld --head ebe746ad1 --pulls 10`: one pull per
pool, all valid (`batch.py --strict --since ebe746ad1`), no DC veto, hard mode on, Steelbreaker last in
all three: uld1 `tr-20261122-195033-8` (`603_8_stormcaller-brundir_1795377075`, wipe 5:48, 32 deaths),
uld2 `tr-20261122-194932-7` (`603_7_…_1795377014`, wipe 5:22, 32), uld3 `tr-20261122-195053-9`
(`603_9_…_1795377095`, wipe 6:03, 33). Windows 9-11's traces re-read for the falls and round 7's checks
(`603_32_…_1795358187` is window 9's uld3, never read in round 6).

**Stop rules.** Not polished: three wipes. Picks so far: healer mana (r1), Meltdown on the melee (r2),
the user's answers (r3, r5), Fusion Punch on the tanks (r4, r6), a stranded risen tank (r7). This round
picks members walking through a hole in the floor (new), so no three-round repeat.

**Did round 7 cause the wipes? No.** It changes only a bot tank's re-sent walk to its spot.
- First 5 s (council members' victims from the snapshots): every member stays on a tank in all three
  pulls bar Brundir on a melee for 0.2 s (uld1 0:00.99), as windows 10-11 did (0.3-0.5 s). Steelbreaker
  passes between tanks for 3-8 s exactly as before (uld1 3.5 → 4.6 → 8.7 s; window 10 uld1 3.4 → 8.4 s).
- No walk loop: per tank 3-16 accepted walks a pull and 1-7 re-sends after a `dup` (window 11: 2-12 and
  0-6). `flip.move.by:iron assembly tank assignment action` 1.05 against 0.67-2.20 before.
- The three wipes start elsewhere (below). The one moved metric is uld1's raid position flips
  (`spread <-> spread-rune`, 12.4/min against 1.2-5.3): two ranged, Uldxap and Uldxas, flipped ~25 times
  3:33-3:59 next to Molgeim's last rune, a ranged mover, not the tank walk (cause not read).
- Priests ran dry earlier in uld1 (3:48.8 / 4:01.5 against 4:19-4:43 in windows 10-11) and uld2 (3:14.5
  against 3:45-4:30), uld3 within range (3:52.5 / 3:53.2 against 3:48-4:28). Phase lengths match (Brundir
  1:41-1:47, Molgeim 3:30-3:37), and round 7 touches no healer or damage taken.

**Verdict on round 7 (re-send a stopped tank walk): works.** All ten rises in the window read "a healer
in 40 yd at once, stood 0.0s" (`--revived`; window 11 uld3: 7.5 / 4.8 s and 6.8 / 4.8 s); no risen tank
died to High Voltage at the Meltdown point.

**Deaths by mechanic.**

| Pull | Phase timeline | Deaths |
|---|---|---|
| uld1 | Brundir 1:41, Molgeim 3:30, alone 134 s, lowest 16.0% (4:51.5) | 23 High Voltage, 2 Meltdown (carriers at the point), 2 Fusion Punch (carrier Uldxaz 5:08.1, 15 s in, 3 charges, nothing castable; risen Uldxaa 5:27.1 at 63%, 4 charges, Divine Protection on), 2 Steelbreaker melee on tanks (carrier Uldxam 4 s in), 2 on non-tanks, 1 Static Disruption |
| uld2 | Brundir 1:47, Molgeim 3:36, alone 102 s, lowest 36.5% (4:49.0) | 27 High Voltage (the first two: priests Uldyaf 4:12.0 at Z 224 and Uldyad 4:21.0 at Z 340, under the floor), 2 Fusion Punch (risen Uldyaa 4:49.1 at 95%, 3 charges, Last Stand and Shield Wall spent at 2:42 / 3:37; Uldyab 5:05.4 at 32%, 5 charges), 1 Meltdown, 1 Steelbreaker melee on a tank, 1 on a non-tank |
| uld3 | Brundir 1:46, Molgeim 3:37, alone 143 s, lowest 1.9% (5:31.7) | Molgeim phase: 1 Rune of Death (Uldzaq 3:03.3). Alone: 25 High Voltage, 3 Meltdown (carriers at the point), 2 Fusion Punch (carrier Uldzab 5:36.2, 3 s in at 77%, 3 charges, Barkskin on; Uldzaz at 23), 1 Static Disruption, 1 Steelbreaker melee on a tank |

What started each wipe. uld1: healer mana (priests under 10% from 3:48.8 and 4:01.5), a ranged High
Voltage death at 4:57.0, then the third carrier's punch at 5:08.1. uld2: two priests walked through the
floor at 3:37.7 and 3:43.5, out of every heal's reach, and died there; the two tanks then died to punches
at 3 and 5 charges. uld3: 1.9% when the third carrier blew at 5:31.8 (+1 charge, +9.7%), four of five
healers under 10% by 5:21.8, then the fourth carrier's punch 3 s into its Overwhelming Power.

**Picked: members walking through a hole in the floor (bug).** New `--floor`: in uld2 Uldyad sank from
3:37.7 (walking from the stack point to (1552.7, 157.5), `spread`) and Uldyaf from 3:43.5 (to (1548.0,
150.4), `spread`, sinking 2.5 yd short of it) at the run speed, both by `iron assembly raid position
action`; window 9's uld3 lost Uldzae 3:45.5, Uldzac 3:54.3 and Uldzaq 3:55.5 the same way (to (1548.3,
89.0), (1553.8, 85.7), (1546.7, 90.5)), the two healers its first deaths bar a Meltdown. No other Iron
Assembly trace on disk (19 more) has a member under the floor. Cause:
- The spread ring centres on `TryGetIronAssemblyBossTankSpot` (`DeriveIronAssemblyRaidSpot`,
  `UldEncounter_IronAssembly.cpp:1229` before), which a Rune of Power near the designed spot moves
  (`:883`-`:929` before). The Meltdown circles show where Steelbreaker's tank stood: uld2 (1568.0, 140.9)
  for 24 s, window 9 uld3 (1562.2, 106.0); every fallen slot sits 22 yd from those spots (uld2's fit
  (1568.3, 142.0) exactly), 48-51 yd from the anchor at 133°-143° and 217°-227°, past the 40 yd where
  the doc's navprobe found the diagonals off the floor. Shifted rings without a fall: window 12 uld3 and
  window 11 uld2 (1572.0, 141.6), window 10 uld3 (1573.0, 140.3), ~10 yd off the designed spot.
- `PathGenerator::NormalizePath` runs `UpdateAllowedPositionZ` on every path point, which for a player is
  `GetMapWaterOrGroundLevel(x, y, z)`; over a hole it returns that hole's ground, so the end point drops
  hundreds of yards and the spline runs through the floor. Uldyad's line from (1577.1, 121.1, 427.3)
  moved 1.9 yd across in 5 s while dropping 37, the slope of its 44 yd walk to an end point ~860 yd down
  (the doc's hole reads Z −438); Uldyaf walked the floor to its last navmesh corner, then went straight
  down.

**Fix.** The ring samples its 64 bearings for floor with that same call (`IronAssemblyOnFloor`: ground
within `ULDUAR_IRON_ASSEMBLY_FLOOR_TOLERANCE` 2 yd of the spot's Z), cached per instance per ring centre
(`IronAssemblyRingOnFloor`, 64 queries when the centre moves). An off-floor bearing is
blocked like a rune-covered one in the squeeze (`SqueezeIronAssemblyRing`, label `spread-floor`); when
some bearing is off the floor the slot's own point is checked too and, still off, snaps to the nearest
clear sample (rune-free and on the floor), else the nearest on the floor, else the stack point
(`spread-nofloor`); that point check is the one query per bot per tick, and only while a bearing is off
the floor. A ring wholly on the floor reads as before, rune-only squeezes included. No size, mode or player input (the ring is phase 3 only, so normal mode never
reaches it). pb-check green (compile, pblint, codestyle, botobs).

**Also seen, not picked.**
- Fusion Punch: 6 deaths, 2 per pull, none from a strategy gap that is ours to fix this round. uld1
  Uldxaz's 5:08.1 punch (`under 59k`) found nothing castable: Shield Wall read `IMPOSSIBLE` at 4:50.4 and
  was never cast this pull (cast in window 11's uld1; cause not read), and its Last Stand ran to 5:08.5,
  so the shared `PickDefensive` would have read it `covered` anyway; it died at 100% under Last Stand. The
  rest are healing or buttons gone at 3+ charges. Shared `PickDefensive` questions: 0 deaths again.
- uld2 Uldyab's Overwhelming Power ran out at 4:55.4 with the carrier alive and no Meltdown death (cause
  not read).

**Numbers for the open questions (not built).**
1. *Innervate from balance/resto druids to priests:* 7 Innervates; 2 on a priest, both from cats (uld1
   Uldxba 3:28.3 at 15%, uld3 Uldzac 3:51.6 at 12%); 5 on the druid itself (balance 4:15.1 at 40%, 4:09.9
   at 41%; resto 5:22.6 at 39%, 2:47.7 at 58%, 3:56.7 at 61%). uld2 (no cat) put none on a priest, dry
   from 3:14.5. Priests under 10%: uld1 3:48.8 / 4:01.5, uld2 3:14.5, uld3 3:52.5 / 3:53.2.
2. *Rune heading keeping Steelbreaker's tank within 40 yd in Molgeim's phase:* 5 stack shifts in 3 pulls;
   2 left Steelbreaker's tank with no living healer within 40 yd, uld2 Uldyaa 2:26.5-2:52.5 (25.9 s,
   nearest healer up to 43.6 yd, down to 23%) and uld3 Uldzaa 2:28.3-2:52.7 (24.5 s, 48.0 yd, 23%); the
   other three kept a healer within 10.1 yd. No tank died in Molgeim's phase (window 11: 1, after 21.5 s
   out of reach); both took their punch in that gap under a button the punch node spent, Last Stand (uld2
   2:43.4 at 73%) and Divine Protection (uld3 2:45.7 at 50%).

**Next window should show.** `--floor`: "none" in every pull. `postmortem.py --notes ironassembly.spot`:
`spread-floor` rows only in a pull whose Steelbreaker spot moved (Meltdown circle off (1575.9, 132.3)),
and its ranged at slots 22 yd from that centre on the floor; `spread-nofloor` never. Pulls with the
designed spot read as before (`spread` / `spread-rune`). Rises still "at once" (round 7).

**Review.** Root cause holds: `--floor` over all 21 traces on disk finds only the five falls named,
each walking to a slot 22 yd from a shifted centre (uld2: Steelbreaker at (1573.1, 136.0) from 3:35
on, the Meltdown circles at (1568.0, 140.9)). The move targets bound the holes: round uld2's centre
the floor holds at 113° and 180° and not at 135° or 158°; round window 9 uld3's at 217° and 270° and
not at 225°-248°. One fix: the lane's squeeze cut only the widest covered run and snapped a slot
still over a hole to the nearest clear bearing. A ring cut by a rune and a hole at once (window 9
uld3, Uldzae's `spread-rune` slot at 3:45.5) has two runs: with the rune widest, the hole's ~3 slots
snap to its two edges, two bots on a point; with the hole widest, the rune stays unsqueezed and its
slots trade with the escape. Now, with any bearing off the floor, the slots spread by rank over every
sample on the floor and clear of runes (`SpreadIronAssemblyRingOver`), then the floor alone, then
the stack; a ring wholly on the floor keeps the squeeze. Not checked offline (navprobe needs its own
container): that the runtime query reads the designed ring 64/64 on the floor, as the doc's navprobe
does; the next window's `spot` rows show it. Left: Steelbreaker's shifted tank spot itself has no
floor check (up to 36 yd off the stack, the 135° hole 34); no tank fell in the 21 traces.

**User answers (2026-11-22):** Innervate from balance and restoration druids to priests: no, those druids
need their own mana. Rune heading toward Steelbreaker's tank: not built; the user asked whether it is
critical and it is not (no tank death in Molgeim's phase in windows 12-13, 25n polished in window 13).
Shared `PickDefensive` (`src/Ai/Raid/RaidTankDefensive.cpp`): yes, fix both bugs (skip a button whose
runes or runic power are not there; a running defensive covers only if it lasts past the hit).
