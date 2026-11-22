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
on a mana tank, Arcane Intellect, one bot each (dps before healers, then guid; never a bear); and each
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

**Watch.** Round 1 left the hold out because tanks dropped to 8-38% in Molgeim's phase, where those
buttons kept them up; between 25 and 38% they now wait for a dangerous punch.

**Next window should show.** `--revived`: within ~5 s of each rise the tank's Mark of the Wild,
Fortitude and blessings back, and at its next death or the end "every raid buff back" (Arcane
Intellect only on a paladin tank); `none:nobody` only with no buffer within 30 yd. `postmortem.py`
vetoes: `iron assembly tank defensive hold` on class defensives at 25%+ health, none cast there.
`--punches`: phase-3 punches on tanks find a button (`defensive <name>`), fewer `no defensive` than
window 8, and fewer tank deaths to Fusion Punch. Check tank deaths in Molgeim's phase against window 8
(2 in 3 pulls, both punches) for the risk above.
