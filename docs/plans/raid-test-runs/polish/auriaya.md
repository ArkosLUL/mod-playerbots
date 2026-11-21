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

**Next window should show.** No DPS attack on a Sentry before one is in combat; the first Sentry
engage on the off-tank (`auriaya.sentry` going `free` → `none`/`dps`), not on a cat or pet; no
Sentry-melee death on a non-tank in the first 0:30; `--threat 34014` "on a tank" well above 59%.
Watch the off-tank's health while it holds the pack.
