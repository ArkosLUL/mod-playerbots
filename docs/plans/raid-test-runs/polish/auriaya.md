# Polish log: Auriaya (25N)

Trace slug `auriaya`, raid `uld`, scenario `uld-auriaya` (drop up the corridor off the ring's
north-west corner at (1922, 80); Storm Tempered Keepers despawned; the harness sets her on the tank
wherever her patrol has her, ~185 yd off in all three pulls). No hard mode, so a kill counts as is.
No boss reader; `postmortem.py --threat 34014` gives Sentry time on target by role.

## Round 1 (2026-11-22, build 0c08a0922)

**Data.** Window 5, one pull per pool, all valid kills (`batch.py --strict --since 0c08a0922 --boss
auriaya`): uld1 `tr-20261121-213414-26` (`603_27_auriaya_1795296926`, 6:11, 10 deaths), uld2
`tr-20261121-214056-28` (`603_29_auriaya_1795297329`, 4:37, 2), uld3 `tr-20261121-213414-27`
(`603_26_auriaya_1795296931`, 4:21, 3).

**Stop rules.** Not polished: uld1 had 10 deaths. First round.

**Deaths by mechanic.** All 15 are Sanctum Sentry melee, between 0:18 and 0:47.

| Pull | Deaths |
|---|---|
| uld1 kill | 10 Sentry melee: cat 0:21 (all four Sentries on it), 2 ranged 0:23-0:25, off-tank (DK) 0:31 holding 2-3, then 6 after it (4 ranged, 2 melee, 0:35-0:46) |
| uld2 kill | 2 Sentry melee on melee DPS (0:24, 0:37) |
| uld3 kill | 3 Sentry melee on melee DPS: cat 0:19 (all four on it), 0:31, 0:34 |

Sentries spent only 52-59% of their time on a tank (`--threat 34014`); single Sentries sat 10-25 s
on a melee DPS (uld1 #202 25 s, uld2 #200 17 s, uld3 #200 12 s and #201 10 s).

**Picked: loose Sanctum Sentries (bug, three causes).**

1. *Bots open on the pack out of combat.* The harness engages her alone; her formation row (groupAI
   2) never makes the Sentries assist her, theirs (519) puts all four on whoever engages one. They
   trail her in out of combat, and the targeting node (gated only on her combat flag) hands DPS a
   Sentry: uld3 cat Faerie Fire on #201 0:18.38 → all four on it 0:18.6, dead 0:18.94; uld1 the
   same at 0:20.9 → dead 0:21.5; uld2 shaman wolves + Flame Shock on #201 0:21.3 → all four engage
   0:21.8. The off-tank stood at the drop 50-60 yd off: its taunt node fired at 0:16 and failed out
   of range, and nothing walked it over.
2. *Taunt pick ignores who the Sentry hits.* `GetAuriayaLooseSentry` returned the first Sentry in
   grid order not on the off-tank: uld2 Growl 0:31.4 went to #201 (just taken by the main tank's
   Thunder Clap) while #200 had sat on a warrior since 0:24.5; that warrior died 0:36.96.
3. *The tank leaves the taunted Sentry.* uld2 Growl #201 0:25.3 left the bear on #200, its old
   target. uld3 the bear was already on #201 when it growled it at 0:22.3, and generic tank assist
   (fires once the tank holds aggro, picks the nearest target it lacks aggro on) moved it to #200 by
   0:22.8; #201 went back to melee at 0:25.9 and killed two (0:30.6, 0:34.3).

The trigger fired 81-122 times a pull and the action ran 1-3 times: in reach it only cast the class
taunt, failing on cooldown.

Harness share: the remote aggro start is what brings the pack in unengaged; a body pull engages all
five on the puller. But any pull on her alone (a ranged pull, Misdirection) does the same, so the
strategy must handle it.

**Fix** (`UldEncounter_Auriaya.cpp`, `UldTriggers_Auriaya.cpp`, `UldActions_Auriaya.cpp`):
- DPS targeting waits while a Sentry within 30 yd of her is out of combat (`AuriayaSentriesUnengaged`,
  searched off her); generic assist stays zeroed, so the raid holds.
- The off-tank's pick: on a non-tank, then out of combat, then on another tank, nearest first; probe
  `auriaya.sentry`.
- Out of reach (25 yd, LOS) it walks to 20 yd; in reach the trigger fires only with the taunt ready,
  and after the taunt the tank targets that Sentry (`AttackDirect`). A taunt on an out-of-combat
  Sentry engages the whole pack on the off-tank. `AttackDirect` covers the uld2 case only: tank
  assist can still move the tank off on its next tick (uld3); the opener should make that rare.
- Review: the DPS hold applies only while an off-tank lives (`GetGroupAssistTank(bot, 0)`), so a
  raid without one isn't stuck idle until the enrage.

Same entries and room at 10 and 25 (2 Sentries at 10); no hard mode or heroic split.

**Questions (user).**
- *If the off-tank dies holding the whole pack* (uld1's DK died holding two at 0:31): should the main
  tank keep up to two Sentries (taunt only those on non-tanks), or a third tank join at 25?
  *Recommended:* the main tank keeps two; she and two Sentries held fine in all three pulls.
- *DPS take the Sentry the off-tank is attacking, not the nearest?* That is a target-order change.
  *Recommended: yes* if Sentries still end up on DPS after the opener next window.
- *Harness (orchestrator):* engaging a Sentry rather than her (the DC row's engage entry) gives a
  real body pull. *Recommended:* keep her as is while this fix is judged, since it covers the
  ranged-pull case the strategy must survive anyway.

**User answers (2026-11-22), for round 2 after window 7 judges round 1.**
- If the off-tank dies, the main tank takes up to two loose Sentries (taunting only those on
  non-tanks) and keeps Auriaya. No third tank.
- DPS attack the Sentry the off-tank is attacking, not the nearest.
- Harness: the start keeps engaging her (orchestrator's call).

**Next window should show.** No DPS attack on a Sentry before one is in combat; the first Sentry
engage on the off-tank (`auriaya.sentry` going `free` → `none`/`dps`), not on a cat or pet; no
Sentry-melee death on a non-tank in the first 0:30; `--threat 34014` "on a tank" well above 59%.
Watch the off-tank's health while it holds the pack.

## Round 2 (2026-11-22, build 61011f323)

**Data.** Window 7, one pull per pool, all valid kills (`raidtest.py report --boss auriaya --raid uld
--head 61011f323 --pulls 10`): uld1 `tr-20261122-093418-14` (`603_15_auriaya_1795340123`, 4:32, 4
deaths), uld2 `tr-20261122-093418-15` (`603_14_auriaya_1795340143`, 4:28, 1), uld3
`tr-20261122-093337-13` (`603_13_auriaya_1795340074`, 3:49, 0).

**Stop rules.** Not polished: uld1 had 4 deaths. Sentries picked again, but a different cause.

**Deaths by mechanic.**

| Pull | Deaths |
|---|---|
| uld1 kill | 3 Sentry melee: mage 0:36.4 (pulled #200 with Arcane Blast 0:35.8), rogue 0:45.7, enh shaman 0:47.1; 1 Seeping Feral Essence (rogue 2:01.7, after a Feral Rush) |
| uld2 kill | 1 Sentry melee: warlock 0:50.7 (#200 on it 0:39.7 and 0:50.5, Chaos Bolt / Conflagrate) |
| uld3 kill | none |

**Verdict on round 1: worked.** Sentry-melee deaths 15 → 4, none before 0:36 (window 5: cat dead
0:19-0:21 with all four on it). No DPS hit a Sentry before one was in combat (first DPS casts 0:25.3,
0:23.0, 0:27.3 after engages at 0:25.2, 0:22.2, 0:23.5). The first engage went to the off-tank on uld1
(Dark Command 0:24.9) and uld3 (Growl 0:23.4), to the main tank on uld2 (all four on it at 0:22.24,
before the Growl at 0:22.65), never a DPS or pet. Sentry time on a tank 56.5 / 73.4 / 66.9% (52-59%
before; 68 / 90 / 85% of in-combat time). No tank died.

**Picked: Sentries pulled off the off-tank mid-fight (bug: threat on the pack).** 22 Sentry turns
onto non-tanks between 0:25 and 0:51 (14 / 5 / 3), about half by a single-target hit on that Sentry
(warriors' Heroic Strike / Bloodthirst, mage Arcane Blast, shaman Stormstrike, warlock Chaos Bolt /
Conflagrate), the rest by AoE or splash (uld1 warlock Shadowfury 0:26.85 → #200 and #201 on it at
0:27.18, a DPS DK's Death and Decay → 0:28.15, warrior Cleave / Whirlwind, rogue Blade Flurry).
1. *DPS spread over the pack.* Each took the nearest Sentry (`UldActions_Auriaya.cpp:332` at HEAD),
   kept unless another was 10 yd closer, so the off-tank's threat had to hold four targets against
   19 DPS split over them. The pack died almost together (uld1 0:49.9 / 50.1 / 50.4 / 55.3), so
   every Sentry lived its full ~25-30 s.
2. *The off-tank flickers.* Generic tank assist ranks attackers by aggro, then melee, then **lowest
   threat** (`TankTargetValue.cpp:94`), so the off-tank re-targeted about once a second: uld1 DK 19
   times 0:25-0:50, uld3 bear 19 times 0:23-0:44, uld2 bear 17 times 0:37-0:54 (Sentry ↔ Auriaya).
   Its single-target threat never built on one Sentry, and "DPS follow the off-tank" needs a steady
   target to follow.

The first user answer comes from round 1 uld1 (window 5): after the DK off-tank died at 0:31.5, the
main tank's target went Sentry to Sentry from 0:31.8 to 0:53.8 and it walked 9 yd off its station
(`TankTargetValue.cpp:74`: an explicit main tank sticks to its target only while more than one tank
lives).

**Fix** (both user answers; `UldEncounter_Auriaya.cpp`, `UldTriggers_Auriaya.cpp`,
`UldActions_Auriaya.cpp`, `UldDefinition_Auriaya.cpp`):
- DPS take the Sentry the living off-tank is hitting (`GetAuriayaOffTankSentry`, its `GetVictim()`),
  with no 10 yd rule; nearest only with the off-tank dead or on no Sentry. Probe `auriaya.focus`
  (`offtank`, `nearest`, `none`).
- The off-tank keeps one Sentry until it dies: `TankAssist` zeroed for it while a Sentry is in sight
  (rule `auriaya off tank target guard`), and `auriaya off tank target` (`ACTION_RAID + 1`) picks the
  next, nearest on itself first, else nearest in combat. The taunt action still retargets after a
  taunt off a non-tank or out of combat, no longer after one off the main tank (safe there, and the
  switch would drag the DPS off a half-dead Sentry).
- No off-tank alive: `auriaya main tank sentry` (`ACTION_RAID + 3`) keeps the main tank on her and
  taunts a Sentry on a non-tank within 30 yd while it holds fewer than two, never walking;
  `TankAssist` zeroed for it then (rule `auriaya main tank target guard`). Probe `auriaya.sentry` on
  the main tank: `dps`, `full`, `none`. A third tank in the raid becomes the off-tank instead.
- Review: between Sentries the DPS take a bot off-tank's next pick (`GetAuriayaOffTankHoldSentry` on
  it), not their nearest: in window 7 the off-tank went 0.2-0.8 s without a live Sentry after a kill,
  time enough for every DPS ticking then to open on a different one.

Same entries at 10 (2 Sentries) and 25; no hard mode. No shared code touched.

**Not picked.** The uld1 Seeping Feral Essence death (rogue at 5.6% after a Feral Rush, walking out
of a pool): one death, no pattern yet.

**Next window should show.** DPS `auriaya.focus` mostly `offtank` through the Sentry phase; the
off-tank's target changing only on a Sentry death or a taunt off a non-tank (a handful per pull, not
17-19); staggered Sentry deaths and the pack dead no later than ~30 s after its engage; fewer Sentry
turns onto non-tanks and Sentry-melee deaths below 3 / 1 / 0; `--threat 34014` on a tank above 56.5 /
73.4 / 66.9%. If an off-tank dies: the main tank's target stays Auriaya, `auriaya.station` holds, at
most two Sentries on it. If AoE (Shadowfury, Death and Decay, Starfall) is then what still pulls
them, that is a plan question: hold DPS AoE while the off-tank holds more than one Sentry.
