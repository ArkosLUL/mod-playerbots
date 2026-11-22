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
else walks straight at the body to 26 (caster form first). Holds while the walk's end is within 17 yd
of a carrier inside its 10 s swap lead, or a cast would still run within 16 yd of a carrier's last 3 s.
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
