# Polish history: Algalon the Observer

## Round 1 (2026-11-22, build 61011f323, DC 5443dbc)

**Data.** Window 7, one valid pull per pool (`raidtest.py report --boss algalon --raid uld --head
61011f323 --pulls 10`); window 6's one pull (`603_2_algalon-the-observer_1795299361`, uld1 roster,
wipe 4:31 at 55%) read for comparison only.

| Pool | Run | Trace `603_*_algalon-the-observer_*` | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261122-104356-42` | `603_42_…_1795344339` | wipe 4:05, Ascend 3:58.7, boss 59% | 32: star explosion 12, Algalon loose 8, Constellation Barrage 4, Black Hole DoT 4, Algalon on tank 2, Cosmic Smash 1, Dark Matter in Big Bang 1 |
| uld2 | `tr-20261122-104717-43` | `603_43_…_1795344541` | wipe 3:54, Ascend 3:47.7, boss 64% | 32: star explosion 15, loose 5, on tank 3, Cosmic Smash 3, Barrage 2, Dark Matter in Big Bang 2, Black Hole DoT 1, Dark Matter stray 1 |
| uld3 | `tr-20261122-103352-39` | `603_39_…_1795343736` | wipe 4:26, Ascend 4:20.4, boss 53% | 33: star explosion 14, loose 11, on tank 4, Barrage 3, Cosmic Smash 1 |

Bots who died twice (soulstone, Reincarnation) count twice. No DC veto row inside a pull. What started
each wipe:

- uld1: two stars 1.8 s apart at Big Bang 2's cast start (3:24.9 killed, 3:26.7 drained), 11 dead
  with 3 healers; both swap tanks died 3:41 and 3:45, then he was loose.
- uld2: the holder died at 3:08.7 (126k taken in 6 s, both tank healers alive 18.6 yd off), the
  partner took him at 4 stacks and the 5th phased it, the bear handler died at 3:15; then two stars 3.0
  s apart at the cast start (3:23.7 drained, 3:26.7 killed), 10 dead.
- uld3: the holder died at 2:10.7 (103k in 6 s, no cutter), the bear at 2:37.1 (70k in 1.6 s; its
  second tank healer 52 yd off on a meteor dodge); one tank left, 5th stack at 3:57.4, loose 10 s, and a
  deadline kill at 4:03.7 went off on 16 under the line (11 dead).
- window 6: the holder died at 2:24 (143k in 6 s), four more tank deaths 3:40-4:03, loose 23 s.

**Stop rules.** Not polished (three wipes). First round.

**Picked: a paced star kill kept into another star's death at Big Bang 2** (21 of the 41 star
explosion deaths, uld1's wipe trigger). Bug.

- uld1: at 3:02.5 the team opened star 1653 at 81% (`starwindow open`). At 3:03.8 the under-star 1655
  was released early at 25% (`underdrop early`) and from 3:04.7 drained 1 %/s to its death at 3:26.7,
  0.1 s before the cast. The team kept 1653 for 22 s at 3.5 %/s (planned 5.5-9) and killed it at 3:24.9.
- uld2: at 2:57.0 the team opened 1642 at 86% as a deadline kill while 1606 drained unattended to its
  planned death at 3:23.7 ("1606 26% 25s"). The kill ran 2.9 %/s and landed at 3:26.7, 0.2 s into the
  cast and 3.0 s after 1606.
- Cause, `StarWindow` keep-or-drop (`UldEncounter_Algalon.cpp`, the `focus && !focus->under` block): a
  kill continues if its finish is clear or if dropping wouldn't help, and the dropped star was modelled
  dying anywhere from `StarDeathAt(min(slope, 5 %/s))` to the drain. For a kill slower than 5 %/s that
  span starts at the kill's own finish, so whenever the finish collided the dropped span collided too
  and the kill was always kept. The due star it collided with (1655, 1606) was never reached.
- A dropped star only carries its slope while casts in flight and DoTs land: 1655 left at ~37% lost
  ~15% in 2.2 s, then drained 1 %/s; the doc's own figure is 22% in 3.8 s.
- Replaying the rule on both traces (per second, 4 s slope, blackouts from the `starplan` notes): the
  old rule kept every tick; the new one drops 1653 from 3:06 (72%) and 1642 from 3:05-3:07 (68-70%),
  when their finish first meets the other star's death. Both drop spans then sit near a meteor
  blackout, which is why meteors are ignored for a kill that itself lands on an explosion or Big Bang.

**Fix** (Algalon files only): `DroppedDeathMs`, a dropped star keeps its slope (capped 5 %/s) for
`ULDUAR_ALGALON_STAR_DROP_CARRY_MS` 4 s, then drains; the keep test uses it as the dropped span's early
end, and `StarCollision` gets a `meteors` flag so a kill colliding with another death or Big Bang
drops when the dropped span only meets a meteor. Unchanged: a star with no more than 4 s of its slope
left (20% at most) still reads as dying within the carry, so low stars keep as before; 10 and 25-man
share it.

**Not picked.**

- Tank deaths (9, plus 24 loose deaths after them): the holder took 66-161k in the last 6 s, 40-88k in
  the last 2 s, with every healer alive and unphased in 9 of 14 tank deaths (window 6 included). A
  healing and gear limit, not a strategy bug (Q2).
- Paced kills run 2.2-5.0 %/s against the 5.5-9 %/s the planner assumes, so planned finishes slide;
  the slow end could follow the measured speed. Left for a later round, after this fix is judged.

**Damage (no deaths needed to wipe).** Before anyone died (uld1 to 2:16, uld3 to 2:01) Algalon lost
0.22-0.25 %/s; at that rate phase 1 alone takes 320-365 s of the 360 s enrage, so no kill is possible
at this gear even deathless. The uld3 roster dealt ~160k raid DPS on XT-002 and ~108k here before its
first death. The two DPS death knights per pool did 2.1-3.4k against 5-7k for the other melee, and
about half the other melee on XT-002 too (a shared class issue). Melee spent 21-95% of the 15 s before
each cast on `algalon.spot=nearhole`, 22 yd off a hole, not on him.

**Questions for the user.**

- Q1: Algalon's enrage is out of reach at ilvl 226 even with no deaths (above). Gear the Algalon pulls
  higher (pools regeared to ~239 for its window, or a fourth uld pool), or keep 226 and accept that
  rounds judge mechanics only and the stop rule can't be met? Recommendation: regear to ~239 for
  Algalon's pulls; it also eases the tank deaths (Q2).
- Q2: tanks die to his burst with their healers on them. A 5th healer at 25 (DC comp, as Iron Assembly
  got) costs a DPS the enrage can't spare. Recommendation: no 5th healer now; revisit after Q1.
- Q3 (shared code, another lane): DPS death knights run at about half the other melee on every boss
  read. Recommendation: a separate lane on the DK rotation.

**Next window should show** (`algalon.py --stars`, `--verdict`): no paced kill (`killed`, window
`open` or `deadline`) dying within 8 s of another star's death or in a Big Bang cast's first second
(window 7: uld1 3:24.9/3:26.7, uld2 3:23.7/3:26.7); star explosion deaths around Big Bang 2 near zero
(window 7: 11 and 10). Watch that a dropped star is not reopened and dropped again (`--stars` "over
one switch per star"), and that one left at 40-70% is later killed by a deadline rather than drained
into a meteor. A drop runs on into the due pass, whose no-clear fallback can take the same star back as
`deadline` when the due star it collided with has no clear finish yet (uld1 at 3:06: 1655 still inside
the 3:05.5 meteor's blackout): a `deadline` on the kill's own star right after its `open`, with the other
star going off within 8 s, means the drop changed nothing.

## Round 2 (2026-11-22, build 1b45df985 with round 1, DC 2ce3824)

**Data.** Window 9, one valid pull per pool (`raidtest.py report --boss algalon --raid uld --head
1b45df985 --pulls 10`). Window 7's three traces and window 6's `603_2` reread for the Big Bang
aftermath only. Roster death knights still had ~25 of 71 talent points.

| Pool | Run | Trace `603_*_algalon-the-observer_*` | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261122-140312-21` | `603_21_…_1795356295` | wipe 4:09, Ascend 4:06.3, boss 58% | 33: loose 17, star explosion 5, Constellation Barrage 5, on tank 3, Cosmic Smash 2, Black Hole DoT 1 |
| uld2 | `tr-20261122-140633-22` | `603_22_…_1795356498` | wipe 5:48, Ascend 5:45.2, boss 40% | 32: loose 16, star explosion 5, Cosmic Smash 4, Barrage 4, on tank 2, Black Hole DoT 1 |
| uld3 | `tr-20261122-141741-26` | `603_26_…_1795357166` | wipe 4:02, Ascend 3:59.0, boss 56% | 33: star explosion 16, loose 9, on tank 4, Barrage 2, Cosmic Smash 1, Black Hole DoT 1 |

What started each wipe:

- uld1: three explosions 10-11 s apart before Big Bang 1: the shelter kill (1:30.6), under-star 1584
  taken `urgent` at 0% after a stray hole ate the only hole (1:41.4), under-star 1585 drained (1:51.3,
  raid min 35%, 3 dead, 2 of them healers). Both had been over-cut: the bulk took 1584 from 38.8% to
  20% in 7 s with 5.5% splash (Explosive Shot, Haunt), and both were released `early`. The holder died at
  2:07.2 (40 k in 6 s, 9 k healed), the next at 2:15.8; the last tank ate the 5th stack at 3:26.1.
- uld2: in Big Bang 2's hide the tank Uldyaa walked 37.7 yd to its hole across a meteor and died to it
  (3:30.6); the other two died to swings at 4:49.9 (152 k in 6 s, 126 k healed) and 4:53.6 (the partner,
  3.7 s after taking him), both tank healers alive, then loose; Big Bang 3's under-star went off 0.3 s early on 7 runners (4 dead).
- uld3: the holder died at 2:26.5 (116 k in 6 s, 80 k healed). Uldzaa left Big Bang 2's void at ~37% and
  one swing killed it (3:37.6). A `deadline` kill opened at 3:34.8 landed 3:43.7, 9.2 s after the hit, on
  14 under the line (raid min 28%): 12 dead, and both remaining tanks took 8.6 k and 15.7 k from it and
  died to swings within 4.2 s.

**Stop rules.** Not polished (three wipes; a kill isn't expected on 226 rosters, user 2026-11-22). The
failure picked below is new.

**Verdict on round 1: held.** No `killed` star in window 9 died within 8 s of another star's death or
in a Big Bang cast's first second; every pair under 8 s was under-stars inside a cast. Star explosion
deaths 41 → 26, inside Big Bang 2's cast 11 and 10 → 1, 0, 0. Watch items: no `deadline` right after an
`open` on the same star. But uld2's 1636 was dropped and retaken 6 times from 3:06.8 to 3:20.5
(`deadline`/`prebang` alternating), reached the cast at 17% and was killed 5.3 s after the hit (1 dead):
a star not yet the focus is planned at 5.5-9 %/s, the focus at its own slope (~3.7 %/s), so each
retake lands clear on paper and on the Big Bang window once measured.

**Picked: a due kill landing in Big Bang's aftermath** (13 of window 9's 26 explosion deaths, uld3's
wipe trigger). Bug: the raid-health gate counts phased bots because "the gate opened 1.6 s after a hit
with the raid drained, 7 dead", and the blackout is meant to last "until the raid is back out of the
void", but the due branch opens "whatever the raid's health" and the blackout ends 3 s after the hit.

- The void's 62169 DoT leaves most of the raid under the line after every hit (scratch `raidhp.py`,
  8 traces, 18 hits). After Big Bang 1, raid near full strength: under the 21.5 k line median ~18 of 25
  at +0..+12 s, 14-22 at +14 s, back toward the usual 1-7 (median, 20+ alive, away from hits) by +18-20
  s; under 14 k (what a covered explosion lands) ≤ 3 by +16 s in every hit with no explosion inside it
  but uld1's (loose boss). Explosions within 15 s after a hit: 7 in 8 pulls, 20 dead; within 10 s, 4
  explosions, 18 dead, against about 1 per paced explosion elsewhere (round 1's collisions left out). The planned kills among them: uld3
  3:43.7 (+9.2 s, 12), uld2 3:39.9 (+5.3 s, 1), window 7 uld2 3:43.2 (+8.7 s, 4), `603_38` (uld1, before
  round 1) 3:45.4 (+11.2 s, 1).
- uld3 at 3:34.8 (`starplan`: `1640 31% 29s, 1639 48% 45s | smash+prebang -8..3 | smash 17..26 | smash
  42..51`): 1639's drain death sits in the 42..51 meteor blackout, and `SerialDue` (rank 2: 32 s + 15 s
  of blackouts ≥ 45 s) puts the serial line at 45 s, so 1640 (own death at +29 s, clear) is due too;
  its `FinishBand` 5.4..7.6 s clears the 3 s tail and the gap, so `StarWindow`'s due branch
  (`UldEncounter_Algalon.cpp`, the `if (!due.empty())` block) opened it with the raid drained.

**Fix** (Algalon files only): in the due branch, while anyone is under the explosion line
(`RaidAffordsExplosion` false) and within `ULDUAR_ALGALON_STAR_VOID_RECOVERY_MS` 16 s of the last hit, a
candidate whose finish band starts inside that span is blocked (`algalon.starwindow=recovery`) and is
never the no-clear fallback. A star's own drain death is not judged against the span, so it makes no
star due. Unchanged: open kills (already gated on raid health), the keep test, `urgent`, `SerialDue` and
the plan's blackouts. 10 and 25-man alike (62169 is not difficulty-mapped). Replayed on window 9 by
hand: only uld3's 3:34.8 kill changes (held until its finish clears 3:50.5; 1640 is then killed or
drains at 4:03.8, clear); uld2's 1636 is held too but drained 2 s after it would have died anyway; no
pull opened a kill in the 16 s after Big Bang 1.

**Not picked.**

- Loose deaths (42) follow the tank deaths (10): 3 to burst with healers alive (uld2 4:49.9, 4:53.6;
  uld3 2:26.5), 2 after a star killed healers (uld1), 1 to a meteor on the hide walk (uld2), 1 on the
  void exit (uld3 3:37.6), 2 after the 3:43.7 explosion (uld3), 1 alone at 5 stacks (uld1). The user's
  answer stands (no 5th healer).
- Under-stars over-cut before Big Bang 1 (uld1's chain): the bulk ran 1584 well past its band's top in
  7 s, with splash. Independent, not small; next candidate.
- The 1636 flip-flop above: paced kills ran 2.2-5.0 %/s in window 9 (median ~3.4) against the 5.5 %/s
  slow end. Letting the slow end follow the team's measured speed touches every keep, open and due
  decision, so not a small second fix; candidate after the over-cut.
- Death knights died to explosions with Anti-Magic Shell and Icebound Fortitude ready and unpressed
  (uld3 3:43.7 Uldzam, uld2 5:00.6 Uldyam; Uldyai with Anti-Magic Shell): 3 deaths, not traced yet.
- Damage: 0.24-0.27 %/s before the first death (window 7: 0.22-0.25).

**Questions for the user.** None.

**Next window should show** (`algalon.py --stars`, `--verdict`): no `killed` star explosion within 16 s
after a Big Bang hit (window 9: uld3 3:43.7 at +9.2 s, 12 dead; uld2 3:39.9 at +5.3 s); `recovery` in
the `algalon.starwindow` timeline after hits. Watch that the held kills don't bunch up: no two explosions
within 8 s, and no due star drained onto a meteor (`drained`, window `smash`), in the 16-45 s after a hit.

## Round 3 (2026-11-22, build 697052475 with round 2, DK rosters re-specced)

**Data.** Window 10, one valid pull per pool (`raidtest.py report --boss algalon --raid uld --head
697052475 --pulls 10`). All 13 Algalon traces on disk reread for the star explosions (scratch `explo.py`,
`carried.py`).

| Pool | Run | Trace `603_*_algalon-the-observer_*` | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261122-174354-31` | `603_33_…_1795369536` | wipe 4:07, Ascend 3:59.9, boss 55% | 32: loose 10, star explosion 9, Constellation Barrage 7, on tank 5, Cosmic Smash 1 |
| uld2 | `tr-20261122-180659-34` | `603_36_…_1795370922` | wipe 5:43, Ascend 5:36.8, boss 44% | 31: loose 10, star explosion 9, Barrage 5, on tank 4, Cosmic Smash 2, Black Hole DoT 1 |
| uld3 | `tr-20261122-175236-32` | `603_34_…_1795370059` | wipe 7:27, phase 2 at 6:18.6 (20.1%), Berserk 6:25.8, boss 13% | 33: phase 1 4 (star explosion 3, Barrage 1); after Berserk 29 (loose 21, on tank 6, Big Bang 1, Unleashed Dark Matter 1) |

What started each wipe:

- uld1: a stray hole ate the only hole, so `urgent` killed 1584 with a meteor live (1:47.2, 2 dead); Barrage
  killed 2 ranged and a healer by 2:14; the holder died at 2:21.7 (81 k in 6 s, 52 k in 2 s), a healer to a
  deadline kill with 5 under the line (2:57.5); four tank deaths 3:18.7-3:40.3, then loose. A constellation
  ate the raid's only hole 0.9 s before Big Bang 2's hit (3:33.7), `urgent` put the ranged on 1641 at 23%,
  and the kill went off at 3:41.2, 6.6 s after the hit, on 14 under the line: 6 dead.
- uld2: 3 deaths before 4:09. A `deadline` kill opened at 4:13.5 on 1636 at 8.4% to beat a meteor; the team
  took it +0.2 %/s beyond the drain in 4.1 s and it went off with the marker live and 12 under (4:17.7, 4
  dead); tank deaths 4:24.5-5:12.3, then loose.
- uld3: 4 deaths in phase 1, three to `deadline` kills landing 5.5-7.8 s after a meteor impact with 9-12
  under. The raid reached phase 2 7.2 s before Berserk: 0.227 %/s on him over phase 1.

**Stop rules.** Not polished (three wipes; the stop rule is waived on 226 rosters). Rounds 2 and 3 both
target the aftermath, two rounds so far.

**Verdict on round 2: held.** No due kill went off inside the 16 s after a hit but uld2's 2:19.8 at +15.9
s, whose band started just past the span (1582 at 4%, 0.8 s from its drain death, 0 dead). `recovery`
shows in uld2 at 2:11.6, 3:40.1 and 5:05.3-5:07.9. Watch items: no two explosions within 8 s outside the
casts' under-stars; no due star drained onto a meteor in the 16-45 s after a hit. The aftermath still cost
6 (window 9: 13), through a path round 2 left open: uld1's 3:41.2 kill was taken by `urgent` in the cast
and kept after it as `open`.

**Picked: a kill carried from the Big Bang cast into its aftermath** (6 of window 10's 21 explosion
deaths, the largest single explosion). Bug: round 2's rule is that no kill lands in the void's aftermath
while anyone is under the line, but only the due branch checked it.

- uld1: `algalon.holes` 0 and `algalon.urgent` 1 at 3:33.690, focus 1641 (23%) from then to its death at
  3:41.241; `urgent` 0 at 3:34.734 and the window `open`. The hit was at 3:34.630; 16-19 under the line
  and 10-15 under 14 k at +0..+6 s (`raidhp.py`).
- Cause, `StarWindow` (`UldEncounter_Algalon.cpp`): during the cast it returns `bigbang` and clears the
  focus, so only `urgent` (no hole, cast due or running, which overrides everything) can hold a kill
  across the hit. At the cast's end the keep test (the `focus && !focus->under` block) kept it on a clear
  finish; the recovery check sat in the due branch only, and the open branch's raid-health gate never
  ran. In 13 traces this is the only focus held across a hit (`carried.py`).

**Fix** (Algalon files only): `recovery` and `drained` move to the top of `StarWindow`; the keep test
drops a kill picked before the last cast ended (`focusSinceMs` before `lastBigBangEndMs`) while the raid
is drained and its finish starts inside the span. The due branch then holds it as `recovery` and opens it
once its finish lands past the span; kills opened after the hit keep the old keep test, so a reopened star
isn't dropped again. 10 and 25-man alike. Replay on uld1: dropped at 3:34.7 (forecast 3:39-3:41); the
drain alone ends it at 3:56.7, 1 s after a meteor, so it's due, and from about 3:47.6 (walk left out) its band fits
between the span's end (3:50.6) and that meteor's blackout (3:51.7): 4 under the line at +16 s against 16
at +6 s. Nothing else in the 13 traces changes.

**Not picked.**

- Tank deaths (9) and the loose deaths after them (20): uld1's five took a median 81 k in 6 s, 48 k in
  2 s. The user's answer stands (no 5th healer).
- Paced kills landing soon after a meteor: across the 13 traces, `deadline`/`open` kills within 9 s after
  an impact were 18, 29 dead (11 with deaths, median 8 under the line), against 17 elsewhere outside the
  aftermath, 24 dead (8 with deaths, 11 of them round 1's collision, median 3 under). The blackout ends
  5 s after the impact. Lengthening it moves every due and serial deadline; next candidate.
- uld2's 1636: a `deadline` kill the team barely touched (one member walking in from 57 yd, the others on
  Mind Flay and Drain Soul). Same family as the slow paced kills (2.2-5 %/s against 5.5-9 planned).
- Barrage 13: 5 before the first tank death, 8 in the collapse.

**Probe keys never emitted in window 10.** None is dead code:

- `algalon.call`: raid warnings to humans; `SendAlgalonCall` returns before the note with no real player
  in the group. Never in 13 traces.
- `algalon.handover`: the holder handing him over for want of a defensive applies only to a tank soak
  (`DefensiveHandoverDue` needs no disperser latched); every Big Bang here was a shadow priest's Dispersion
  soak. Never in 13 traces.
- `algalon.nitro`: needs a bot with Nitro Boosts on its boots at the cast start; no 54861 aura in any of
  the 13 traces, so no roster bot wears them.
- `algalon.speed`: Aspect of the Cheetah or Sprint on a hide run already late; fired in 3 older traces
  (`603_2`, `603_37`, `603_39_…_1795351067`); no hunter or rogue in window 10 ran late with it ready.

**Questions for the user.** None.

**Next window should show** (`algalon.py --stars`, `--verdict`): no `killed` explosion within 16 s after a
Big Bang hit (window 10: uld1 3:41.2 at +6.6 s, 6 dead). A kill `urgent` takes in a cast and still alive
at its end is dropped right after the hit while the raid is under the line (`algalon.focusstar` 0, window
`recovery`, or `raidhp` while the star isn't due), then killed once its finish clears the span or nobody is
under the line. Only 1 of 13 pulls had such a carry, so the window may
not exercise it; then judge by the aftermath alone. Watch that the dropped star doesn't drain onto a
meteor (`drained`, window `smash`).

## Round 4 (2026-11-22, build ebe746ad1 with round 3)

**Data.** Window 11, one valid pull per pool (`raidtest.py report --boss algalon --raid uld --head
ebe746ad1 --pulls 10`). All 16 Algalon traces on disk reread for hole losses and kite crosses (scratch
`stray.py`, `eatz.py`, `cross.py`, `taunthold.py`).

| Pool | Run | Trace `603_*_algalon-the-observer_*` | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261122-203346-21` | `603_21_…_1795379730` | wipe 5:09, Ascend 5:06.6, boss 43% | 34: star explosion 12, loose 8, Constellation Barrage 7, on tank 5, Big Bang 1, Cosmic Smash 1 |
| uld2 | `tr-20261122-202222-19` | `603_19_…_1795379045` | wipe 5:25, Ascend 5:22.3, boss 44% | 31: loose 12, star explosion 10, on tank 6, Barrage 2, Cosmic Smash 1 |
| uld3 | `tr-20261122-202824-20` | `603_20_…_1795379407` | wipe 4:34, Ascend 4:31.7, boss 55% | 32: star explosion 13, Big Bang 8, Barrage 4, on tank 4, loose 3 |

What started each wipe:

- uld1: a constellation diving on the healer Uldxad took the hole 11 yd from the tank slot at 3:16.5, 10 s
  before Big Bang 2's cast; both swap tanks were then 57-62 yd from any hole (`noshelter`), with no button
  left: Uldxaa died to the hit, Uldxaz 0.1 s later to the under-star going off at the strip. Tank deaths
  4:29-4:48, then loose.
- uld2: the holder died at 3:03.1 (146 k in 6 s, 63 k healed, both tank healers alive), the bear at 3:12.3; a
  `deadline` kill on 1633 at 11% (its drain death sat on a meteor) went off at 4:14.6, 10.9 s after the
  previous explosion with 13 under the line: 9 dead. Tank deaths 4:48-4:55, then loose.
- uld3: no hole at Big Bang 2. The kite took the stock from 3 to 1 at 2:45-2:55 (31-41 s before the cast,
  outside the reserve), then two constellations on the healer Uldzad closed the last two holes at 3:21.5 and
  3:25.4. `urgent` killed a star at 3:31.2 in the cast (2 dead); Uldzad's constellation followed it into the
  new hole at 3:32.2, `urgent` killed another 1.5 s after the first (9 dead), and the hit took 8.

**Stop rules.** Not polished (three wipes; waived on 226 rosters). The failure picked below is new.

**Verdict on round 3: held**, by the aftermath; the carried kill itself wasn't exercised. No `killed` star
went off within 16 s after a hit (window 10: uld1 +6.6 s, 6 dead). The only `urgent` spans (uld3
3:25.7-3:32.9) went off inside the cast, so no kill was carried across a hit. One explosion in an aftermath:
uld1 3:39.8 (+5.2 s, 1 dead, 18 under), under-star 1635 at 14.1% at the cast against its 4.0-9.6% band,
draining into it; round 2 and 3 leave drains alone.

**Failures by deaths** (97): tank burst 15 and loose 23 (the user's answer stands); star explosion 35 =
outside the casts on a drained raid 21 (uld2 4:14.6 9, uld1 4:20.7 4 and 4:50.8 3, five singles), Big Bang
2 without a usable hole 12, other casts 2; Big Bang 9 (all Big Bang 2 without a hole); Barrage 13 (mostly
in the collapse); Cosmic Smash 2.

**Picked: Big Bang 2 without a usable hole** (21: uld3 19, uld1 2; it started two of the three wipes).
Wave 3 wakes 20 s before Big Bang 2's cast; its constellations go to healers on healing threat and dive
across the room, and in every pull one took the hole Big Bang 2 needed: uld2 3:14.3 (1594, 7 yd from the
tank slot, on Uldyae), uld1 3:16.5 (1612, 11 yd from it, on Uldxad), uld3 3:21.5 and 3:25.4 (on Uldzad).
In the 13 older traces, constellations on someone other than the handler closed holes too, but at Big Bang
2 only inside the cast (a chased bot leading one into its hole). Three parts:

- Bug: the handler leaves a constellation over 5 yd up alone ("no hole takes one"), but the core takes one
  up to 7.5 yd up: `npc_living_constellation::SpellHit` ignores 65509 when `GetDistanceZ > 5`, which
  subtracts both combat reaches (model 28741 has none, so 1.5; the Black Hole's 28460 is 1.0 since
  `2026_11_06_01`), and 65509 (target 7, entries) picks non-players in 2D (`Spell.cpp`
  `WorldObjectSpellAreaTargetCheck`). Holes closed at +6.4 (uld3 2:30.9), +7.2 (uld1 3:16.5), +7.2 (uld3
  3:21.5; +7.7 a second earlier), each the eater's last snapshot before the close.
- Bug (the kite flag): uld1's handler stood still in `cross` 4 × 12.6 s (2:11.5, 2:26.2, 2:41.6, 2:57.0) on
  hole 1617. The cross spot (1660.3, -273.9), 40 yd out at bearing 46°, came back from the floor check
  within 2 yd of the handler (no `move` row, no `stuck` note: the only path through the action that fits);
  uld2's cross to (1657.5, -275.2), 37 yd out at 48°, walked. No roster bot stood past 37.1 yd at bearings
  40-50° in 16 traces. After the stall timeout the same hole was picked again, and kite-first kept the
  handler off loose ones meanwhile: C1537 sat low on the healer Uldxad from 3:10.3, taunted at 3:18.8.
- Plan: one class taunt every 8 s can't take three diving at once, and a taunted one goes back to its
  healer: 15 of 27 taunts on constellations in window 11 held under 15 s. Question Q4.

**Fix** (Algalon files only): `ULDUAR_ALGALON_CONSTELLATION_LOW_Z` 5 → 7.5; a stage or cross spot that
`ValidateFloorPoint` moves over 2 yd refuses its hole to the kite for the rest of the pull
(`kiteRefusedHoles`, read by `KiteableHole`), so the kite takes another hole or the handler taunts loose
ones (`algalon.kite` `stuck`, then `nohole` or `taunt`). No extra floor checks: the moved spot is the one
already validated. 10 and 25-man alike. Replay on window 11: no death changes. uld1's 3:16.5 constellation
was over 7.5 yd a second before it closed the hole; with 1617 refused at 2:11.5 the kite moves to 1615
(56 yd from the hold spot) and the handler takes C1537 at 3:10, but Big Bang 2 still finds the tanks
57-62 yd from a hole.

**Not picked.**

- Explosions outside the casts on a drained raid (21): uld2's 4:14.6 was a `deadline` opened 8.3 s after an
  explosion, its own death 1 s after a meteor; due kills skip the raid-health gate by design. Twin stars
  (1632 and 1633, 48% each at 3:34.7) left two explosions 11 s apart. Next candidate, with round 3's meteor
  blackout.
- uld1's under-star 1635 above its band at Big Bang 2 (1 dead after the hit).
- Tank deaths and loose: as before.

**Window flags.** Kite runs "3 of 11 ended with a constellation eaten": mostly the reader's count, since a
run is any unbroken `taunt`/`stage`/… stretch (3 s taunt-only stints, hand-offs while tanks died), and it
credits a run with holes closed by constellations on others; uld3's handler kited 3 into holes and uld2's 5.
The real bug was uld1's stalled crosses (fixed).
Melee on `algalon.spot=stage` for 97-100% of the intro: not a bug, the design: he is unselectable for the
26 s intro, the stage spots keep melee out of his reach when he picks a random victim at its end (0 inside
his reach in all three pulls), and a pair tank had him within 0.3 s.

**Questions for the user.**

- Q4: wave 3's constellations dive on healers 20 s before Big Bang 2 and take its holes. (a) Keep two holes
  out of the kite from 60 s before each Big Bang (now one from 30 s); (b) a second taunter: the swap tank
  not holding him taunts constellations off healers in the 30 s before a Big Bang; (c) both.
  Recommendation: (a) now: one constant, and uld3 would have kept a hole (the kite's eats at 2:45 and 2:55
  left 1); the cost is one more constellation's Barrage until the cast. (b) only if the hole by the tank
  slot keeps going (uld1, uld2).

**Next window should show** (`algalon.py --constellations`, `--holes`): no `cross` or `stage` stretch of
12 s with the handler standing still (window 11: uld1 4 × 12.6 s); a hole the floor check refuses reads
`stuck`, then the handler kites another hole or taunts. Constellations between 5 and 7.5 yd up count as
low: taunted and kited like the rest. Big Bang 2's hole losses won't change until Q4 is answered.

**User answer (2026-11-22) to Q4:** (a): keep two holes out of the constellation kite from 60 s before
each Big Bang.

## Round 5 (2026-11-23, Custom `b21cda8ee`; round 4 merged, not built)

**Data.** No new pulls. Round 4's "window 11" pulls are window 12's (build `ebe746ad1`; window 11 ran no
Algalon). Replay read windows 10 (uld1 `603_33`, uld2 `603_36`, uld3 `603_34`) and 12 (uld1 `603_21`, uld2
`603_19`, uld3 `603_20`); deaths by mechanic per pull are in rounds 3 and 4 (scratch `replay.py`,
`bb2deaths.py`). Stop rules, verdict and ranking skipped: no pulls on round 4.

**Built: the user's Q4 (a).** From 60 s before each Big Bang through its cast the two holes nearest the hold
spot are kept out of the kite (was one from 30 s): `ULDUAR_ALGALON_KITE_RESERVE_SECONDS` 60,
`ULDUAR_ALGALON_KITE_RESERVED_HOLES` 2. `ReservedHoles` returns 2 in that span, so the handler kites a held
one, or taunts from `behind`, only with a third hole up; `IsRaidShelterHole` (fewer than 2 holes nearer the
hold spot, ties by guid) replaces `RaidShelterHole` in `KiteableHole` and the gather's kite-hole keep.
`urgent` keeps its 30 s. Reader: `kite_blocked` counts a taunt only with more than 2 holes inside 60 s of a
cast (test added). 10 and 25-man alike, nothing keyed on a real player.

**Replay on Big Bang 2.** From the real stock 60 s before the cast: a hole a constellation on the handler
ate at its kite hole is kept while 2 or fewer stood, else the kite takes the spare nearest it; every other
loss (dives on healers, the handler's constellation eating a hole off its kite hole) happens to the same
hole if it is still there. Tanks reach about 43 yd in the cast (`603_36`: `noshelter` at 45-46 yd).

| Pull | Holes at the cast, now → rule (yd from the tank slot) | At the hit | Deaths, cast to hit + 16 s | What changes |
|---|---|---|---|---|
| w10 uld1 | 1 (59) → 2 (59, 69) | 0 → 1 (69) | 19: loose 7, star explosion 6, Barrage 4, on tank 2 | kite at 3:09.5 kept; a hole survives the 3:33.6 dive, so no `urgent` and no 3:41.2 kill (6 dead) |
| w10 uld2 | 2 (49, 52) → same | 2 → 2 | 1: Cosmic Smash | none |
| w10 uld3 | 3 (28, 37, 71) → 4 | 3 → 4 | 0 | kite at 2:29.6 kept; a tank hole either way |
| w12 uld1 | 3 (62-65) → same | 2 → 2 | 4: star explosion 2, Big Bang 1, on tank 1 | none: the hole 11 yd from the tank slot went to a dive on the healer Uldxad (3:16.5) |
| w12 uld2 | 1 (61) → 2 (51, 61) | 1 → 2 | 0 | kite at 2:33.5 kept |
| w12 uld3 | 0 → 1 (50) | 1 → 2 | 19: star explosion 11 (`urgent` kills in the cast), Big Bang 8 | kite at 2:45.7 takes the spare at 58 yd, at 2:55.7 kept; the raid has a hole, no `urgent` |

So the raid keeps a hole it lost in w12 uld3 (at the cast) and w10 uld1 (at the hit); no pull gains one in
the tanks' reach, since dives on healers took the near holes (w12 uld1, w12 uld3: Q4 (b)'s case). Big Bang 1:
w10 uld3 (1:50.0) and w12 uld2 (1:42.5) keep a kite hole, 2 holes at the cast instead of 1. Cost: 4 of 6 pulls
hold one more constellation into Big Bang 2 (one Barrage every 5 s; it follows the handler into its hole);
Barrage deaths in the minute before Big Bang 2: 1 in 6 pulls.

**Left alone.** The handler's held constellations ate a hole off its kite hole: w10 uld1 1:44.1 (the only hole,
12 s before Big Bang 1, round 3's `urgent` at 1:47.2), w10 uld2 3:10.0 (the reserved hole), w12 uld2 3:31.7 (in
the cast). The reserve doesn't cover these, and the handler now holds them twice as long.

**Questions for the user.** None.

**Next window should show** (`algalon.py --holes`, `--constellations`, `--bigbang`): no `algalon.holelost kite`
in the 60 s before a cast while 2 or fewer holes stood; holes at Big Bang 2's cast at least window 12's (3, 1,
0), unless dives on healers took them; `urgent` before Big Bang 2 near zero. Watch: `stray` losses eaten by a
constellation on the handler, Barrage deaths before Big Bang 2 (windows 10 and 12: 1), and round 4's checks
(no 12 s `cross`/`stage` stills; constellations 5-7.5 yd up kited).

## Round 6 (2026-11-23, build 1fa3c332c: round 5 and the shared PickDefensive fix)

**Data.** Window 14, one valid pull per pool (`raidtest.py report --boss algalon --raid uld --head 1fa3c332c
--pulls 10`). All 19 Algalon traces on disk reread for stars going off in a Big Bang cast (`--bigbang`,
`--stars`); scratch `hiders.py` (each hider's hide notes, run, phase and death per cast) and `replay.py`
(`AlgalonHideSlackMs`, `HideRoute` and `FindSmashDetour` re-run per hider per snapshot, each hole's real pulse
phase from its 62168 applications).

| Pool | Run | Trace `603_*_algalon-the-observer_*` | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261123-105934-29` | `603_29_…_1795431677` | wipe 4:43, Ascend 4:36.3, boss 47% | 33: star explosion 13, loose 9, on tank 6, Constellation Barrage 5 |
| uld2 | `tr-20261123-105813-28` | `603_28_…_1795431596` | wipe 4:31, Ascend 4:24.0, boss 54% | 32: loose 16, star explosion 6, on tank 6, Barrage 2, Dark Matter stray 1, Big Bang 1 |
| uld3 | `tr-20261123-110656-30` | `603_30_…_1795432119` | wipe 3:54, Ascend 3:48.1, boss 56% | 31: star explosion 13, loose 9, on tank 3, Cosmic Smash 3, Barrage 2, Dark Matter in Big Bang 1 |

What started each wipe:

- uld1: nobody died before Big Bang 2. Under-stars 1636 and 1637 (4.2% and 4.7% at the cast) went off on one tick
  at 3:30.64, the under window's first (hit − 4.0 s), 0.56 s after the cast's three meteors: 15 unphased bots
  hit, 12 dead, 3 of them healers. Tank deaths 3:52-4:29, then loose.
- uld2: the holder died at 2:53.2 (106 k in 6 s, 47 k healed, both tank healers alive); the partner, still on 4
  stacks from 2:38, got him back on threat and the 5th phased it at 2:54.9; the handler took him 3.9 s later from
  20 yd off its slot (2 dead) and died at 3:00.1, the third at 3:12.3. A constellation on the handler (3:15.8)
  and a stray (3:25.9) took the near holes: Big Bang 2's three stood 38-64 yd from the raid, 1 dead to the hit.
  Tank deaths 3:41-4:00, loose 22 s.
- uld3: the holder died at 2:36.4 (97 k in 6 s); a `deadline` kill went off with 10 under the line (3:10.6, 2
  dead); the next holder died at 3:15.2 and he was loose 16.5 s, the shadow priest among the dead. With no
  disperser both under-stars were released `nosoak` at 3:19.8 (11%, 20%): 1638 went off in Big Bang 2's cast
  (3:31.67, hit − 2.9 s) on 12 bots, 6 dead, and 1637 drained into the aftermath (3:40.7, +6.1 s, 9 under, 5
  dead).

**Stop rules.** Not polished (three wipes; waived on 226 rosters). The failure picked below is new.

**Verdict on round 5: held.** No `algalon.holelost kite` in the 60 s before a cast with 2 or fewer holes standing
(uld2's at 3:15.8 left 3); holes at Big Bang 2's cast 3, 3, 4 (window 12: 3, 1, 0); `urgent` never set. Watch
items: Barrage deaths before Big Bang 2 1 in 3 pulls (uld3 1:48.8; windows 10 and 12: 1 in 6); round 4's checks
hold (parked 3.0-5.8 s, `stuck` fired in uld2, no 12 s still). The holes didn't save Big Bang 2: uld1 had its
near hole and lost 12 to the under-stars; uld2's three stood far from the raid.

**Failures by deaths** (96): tank burst 15 and loose 34 (Q2 stands); star explosion 32 = in Big Bang 2's cast on
hiders 18 (uld1 12, uld3 6), a released under-star drained into the aftermath 5 (uld3), `deadline` kills on a
drained raid 3 (uld3 3:10.6 2, uld2 3:15.9 1), Big Bang 1's cast 1 (uld2), the collapse 5; Barrage 9; Cosmic
Smash 3 (uld3, in Big Bang 2's hide); Big Bang 1; Dark Matter 2.

**Picked: stars going off in Big Bang 2's cast on hiders** (18). Bug: the hide means every hider to be in 1.5 s
before a star dies in the cast (`UnderHideExtraMs`); two paths leave them out.

- uld1, the meteor wait. Big Bang 2's three meteors land 4.5 s before the hit, 0.4-0.6 s before the under window
  opens. A hider whose walk crosses one and can't be phased 0.5 s before it waits for the impact while that
  leaves its walk plus 1.5 s before the hit, "star under or not" (doc). Uldxah, Uldxai, Uldxal (17 yd from hole
  8914 after dodging to (1637.4, -340.6)) missed that early slot by 20-270 ms on the cast's first ticks, waited
  from 3:27.2, ran at 3:29.0 once the crossing test cleared by 0.1 yd, lost 34-53% of their health to the
  meteors 11.0-11.2 yd off, and were 5-6 yd from the hole when the stars went off; Uldxaf and Uldxar waited until
  3:29.4 and 3:29.8. Uldxab, Uldxag, Uldxas, Uldxav ran at the cast on the detour 12 yd round the meteors and
  died 14-21 yd short. Hole 8914 pulsed at cast +3.04 and +4.04; the stars went off at +4.04. The doc's case
  for waiting, "out under a star it still gets the cover or its button", failed: two stars on one tick (14-18 k
  each after resistance), and Anti-Magic Shell, Cloak and Ice Block are held through the cast (4 died with one
  ready).
- uld3, a released star. `UnderHideExtraMs` counted `under` stars only, so 1638 draining into the cast after its
  `nosoak` release added no margin: the near hiders set off on the base margin (cast +4.0..+5.2 s), and 7 were
  still running and 3 waiting when it went off at +5.07.

The replay reproduces 11 of uld1's 12 dead hiders' waits and detours (Uldxae ran 1.4 s late in the trace).

**Fix** (Algalon files only, `UldEncounter_Algalon.cpp`):

- `CastStarDeathMs` (`:2782`): the earliest drain death in the cast among under-stars and any star dying by the
  hit + 0.6 s; `UnderHideExtraMs` stretches from it, so a released star draining into the cast gets an
  under-star's +3 s (stretched, at most 4 s).
- `AlgalonHideSlackMs` (`:4118`): the meteor wait is skipped when it leaves the hider out at that star (wait,
  walk and 1.5 s past its death) and going round gets it in sooner, or, with no way round, straight gets it
  phased first (a pulse in hand) with no meteor landing on its walk before the hole; it then runs on its route's
  slack. A bot going last keeps the old rule. The review added the two last terms: without them any detour
  skipped the wait (a tank on one too long for the hit could drop to `noshelter`), and a straight walk could cross
  a landing meteor. The replay of all 35 casts with a star going off in them is unchanged by them.

10 and 25-man alike; with no star dying in the cast both rules are unchanged. Replay: in uld1, Uldxah, Uldxai
and Uldxal run straight at +0.91, in at +2.66, phased by the +3.04 pulse, before the impact (+3.48) and the
stars (+4.04): 3 saved. Uldxaf, Uldxar and Uldxad go round at +0.40, in at +3.05-3.42, phased by the +4.04
pulse, the stars' own tick (in the trace the explosion came 1 ms first): not counted. The east group (22.8-25.8
yd at the cast after the meteors landed on their spots) can't be in by +3.04 on any route. In uld3, 13 of 15
hiders are phased at +2.98..+4.98 against the star at +5.07 (now +5.98; Uldzaz on its detour isn't, Uldzag's
hole shows no pulse); of the dead, Uldzan, Uldzaf and Uldzaw make it. The other 10 Big Bang 2 casts on disk:
one hider changes (`603_2`'s Uldxaq, 0.3 s sooner on the same detour). Cost: when a released star drains into a
cast, hiders spend 3-4 s more in the void (62169 at 1.3-1.9 k/s, Dark Matter).

**Not picked.**

- The aftermath drain (uld3 3:40.7, 5 dead): drain deaths aren't made due (round 2), and the release came 7 s
  before the cast with him loose and no disperser.
- `deadline` kills on a drained raid (uld3 3:10.6 with 10 under, uld2 3:15.9 with 11): due kills skip the
  raid-health gate by design (round 4).
- uld1's east group: Big Bang 2's meteors landed on the `nearhole` spots east of the only near hole, and the
  dodge left them 24-26 yd out, too far for a star on the window's first tick. Under-stars on that tick are the
  bulk's overshoot (4.2-8.4% at the cast against the 4.0-9.6% band, 19 traces).
- uld2's Big Bang 2 holes far from the raid (1 dead, 9 bots 43-64 yd from any hole).
- Tank deaths and loose: Q2 stands.

**Questions for the user.** None.

**Next window should show** (`algalon.py --bigbang`, `--verdict`): with a star going off in Big Bang 2's cast,
no hider still `wait` past a meteor's impact in it, and the star's hit list mostly tanks on `hold`/`noshelter`
rather than `run`/`wait` hiders (window 14: uld1 15 hit, 12 dead; uld3 12, 6). A star released before a cast
and draining into it moves the hiders' runs 3-4 s earlier. Watch: Cosmic Smash deaths in Big Bang 2's hide
(window 14: uld3 3) and hiders "brought back inside 15 yd by the impact", and Dark Matter deaths while phased in
a Big Bang (window 14: 1).

## Round 7 (2026-11-23, build 523b2f60f with round 6)

**Data.** Window 15, one valid pull per pool (`raidtest.py report --boss algalon --raid uld --head 523b2f60f
--pulls 10`). All 25 Algalon traces on disk reread for star kills against the last meteor impact and for druid
tanks' forms (scratch `alg7/`: `smashhp2.py` raid under the line round each impact, `bins.py`/`opened.py` kills by
time after the impact and when the team was put on them, `bearform.py` Dire Bear Form spans against
`algalon.tankrole`, `power.py` mana per snapshot).

| Pool | Run | Trace `603_*_algalon-the-observer_*` | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261123-131238-22` | `603_22_…_1795439661` | wipe 4:53, Ascend 4:46.3, boss 45% | 33: loose 13, Constellation Barrage 8, star explosion 7, on tank 5 |
| uld2 | `tr-20261123-131258-23` | `603_23_…_1795439681` | wipe 5:09, Ascend 4:56.4, Big Bang 3 with no hole, boss 48% | 32: loose 15, star explosion 6, on tank 6, Barrage 3, Big Bang 1, Cosmic Smash 1 |
| uld3 | `tr-20261123-131700-24` | `603_24_…_1795439922` | wipe 4:57, Ascend 4:50.6, boss 45% | 31: loose 14, star explosion 8, on tank 4, Barrage 3, Cosmic Smash 2 |

What started each wipe:

- uld1: nobody died before a `deadline` kill landed 5.8 s after a meteor (3:10.6, 8 under the line, 1 dead). Uldxaz
  died at 3:55.9 (74 k in 6 s, 27 k healed, its tank healer 11 yd off); 1631's `deadline` kill landed 5.0 s after
  the next meteor (4:01.0, 15 under, 3 dead, a healer among them); tank deaths to 4:15.4, then loose 17.5 s, 15 dead.
- uld2: the holder died at 3:04.9 (122 k in 6 s, 67 k healed, its tank healer 4 yd off); Big Bang 2 killed Uldyaj
  (hide `none`, 51 k). Tank deaths 4:23.9-4:40.5 with the tank healer alive 16-26 yd off, loose 14 s, and Big Bang
  3 found no hole and no soaker.
- uld3: the holder died at 2:15.6 (110 k in 6 s). Its replacement, the bear druid Uldzab, couldn't take him at
  Uldzaa's 4th stack (3:09.4, see the second fix), so the 5th phased Uldzaa at 3:25.9; a released under-star went off
  3.05 s into Big Bang 2's cast on 6 runners (3 dead, Uldzal to it and a meteor), the other under-star killed Uldzav;
  Uldzab died at 3:37.7 (`noshelter`, 31 k from the hit), Uldzaa at 3:53.5 and to a kill landing 6.6 s after a
  meteor (4:02.7, 2 dead); loose 15.4 s from 4:33.6, 16 dead.

**Stop rules.** Not polished (three wipes; waived on 226 rosters). The failures picked below are new.

**Verdict on round 6: held.** In Big Bang 2's cast no hider was still waiting at an impact (all three pulls:
nobody). Stars going off in the cast hit nobody in uld1 (window 14: 15 hit, 12 dead), none went off in uld2's,
and in uld3 the released 1655 hit 6 runners (3 dead, plus Uldzal to the meteor on its walk; 1654 under killed
Uldzav): it died at cast +3.05 s (hit − 4.95) after 5.3% splash (Uldzak's Piercing Shots 2.6%, Wild Quiver 1.8%)
released it `early` at 13% 10.8 s before the cast. The stretch fired (every hider gone by cast +1.1 s), but from
the `nearhole` spots 22 yd out nobody is in before hit − 5. Star deaths on hiders in Big Bang 2's cast 18 → 4. Watch
items: Cosmic Smash deaths in the hide 1 (uld3 Uldzal; window 14: 3); hiders the hide brought back inside 15 yd of
an impact: uld3 Uldzal 10.2, Uldzab 12.2, Uldzaf 12.5, uld2 Uldyab 14.8; Dark Matter deaths while phased 0 (window
14: 1).

**Failures by deaths** (96): tank burst 15 and loose 42 (Q2 stands; uld3's late swap inside it, below); star
explosion 21 = kills landing 5-10 s after a meteor impact 8 (uld1 3:10.6 1 and 4:01.0 3; uld3 3:12.1 1, 4:02.7 2,
4:31.6 1), in a Big Bang cast on hiders 5 (uld3 Big Bang 2 4, uld2 Big Bang 1 1), the collapse and others 8 (uld1
4:14.7 3, uld2 4:17.7 2 and 4:36.7 2, uld2's `urgent` shelter kill 1); Barrage 14 (13 after the first tank death);
Cosmic Smash 3; Big Bang 1.

**Picked: star kills landing in a meteor's aftermath** (8, and in every pull). Bug: the strategy holds a kill
while anyone is under the explosion's line, but a due kill skips that gate (round 4) and the meteor blackout ends 5
s after the impact, so it lands on a raid the meteor just drained.

- The meteors' damage: 45 impacts away from Big Bang with no explosion from −12 to +20 s (25 traces) leave a
  median 15 of 25 under the line at +2 s, 11 at +4, 9 at +6, 6 at +8, 4 at +10, 2 at +12 (3 before the impact);
  under 14 k, a mean of 1 at most, so deaths come when an explosion lands on top of it.
- Kills outside casts and the void's aftermath, by when they landed after the impact: +5..+8 s 27 kills, 31
  dead (median 8 under the line); +8..+10 17, 8; +10..+12 12, 7; +12..+15 3, 1. Of those landing +4..+12 s, 32 were
  opened after the impact (36 dead) and 26 before it (13 dead), the deadly ones within 2 s of it.
- uld1 at 3:05.871 (`starplan`: `1603 17% 16s, … | smash -5..3 | prebang+smash 19..31`), 1.0 s after the impact:
  1603 is serial due (`SerialDue`: 16 s plus the 3 s blackout cover its death), and the due branch of `StarWindow`
  (`UldEncounter_Algalon.cpp`, the `if (!due.empty())` block) opens it on the first tick its `FinishBand` clears the
  blackout's end, impact + 5 s: it landed at +5.8 with 8 under. uld1 4:01.0, uld3 3:12.1 and 4:02.7 opened the same
  way 0.2-1.7 s after their impacts.

**Fix** (Algalon files only): `InSmashRecovery`, `ULDUAR_ALGALON_STAR_SMASH_RECOVERY_MS` 10 s. A due or open kill
whose finish band starts 5-10 s after an impact still to land, or within 10 s of the last impact while anyone is
under the line, is held (`algalon.starwindow=smashrecovery`), and a held due kill is never the no-clear fallback.
Unchanged: the keep test (a kill under way), due-ness and `SerialDue` (a drain death keeps the 5 s), `urgent` and
the Big Bang `recovery`. 10 and 25-man alike (one meteor at 10: the landed-impact hold lifts once nobody is under the
line; the upcoming one costs a kill 5 s of room). Replay by hand at the real kill speeds: uld1's 1603 opens about
3:10.5 and lands about 3:15 (its own death 3:21.9, Big Bang 2's blackout from 3:24.9); 1631 (11.8%, own death
4:07.9) goes off about 4:06-4:08 instead of 4:01.0, and 1630 (own death 4:19.9, inside the 4:16.9 meteor
blackout) then has 4:14-4:16.9 after the gap, tight (its real kill took 5.3 s); uld3's 1610 lands from 3:14.9 (own
death 3:20.9) and 1653 (27%, own death in the 4:17.3 blackout) in 4:06.1-4:17.3 instead of 4:02.7. Deaths saved
can't be counted (no trace has these raids at +10 s without the explosion on them); the curve says 4 under the line
at +10 against 8-15 at the real landings.

**Second fix: a tank druid stays in bear form** (independent, one rule). Bug: the swap partner can't take him.

- uld3: Uldzab, roster C's spare tank, went caster at 2:04.818 (`caster form` OK, ~86% mana) and healed (`rejuvenation
  on party`, `regrowth on party`) to 0.95% mana by 2:32.3, 0.4-9% to 3:17.6. In the pair from 2:15.8 (`yield`),
  `take` at 3:09.547 for Uldzaa's 4th stack: `dire bear form` PREREQ/FAILED every tick to 3:22.3, `algalon phase
  punch swap action` FAILED at 3:10.3 and 3:21.3 (Growl needs bear form); it shifted at 3:23.481 (11.6% mana) and
  Growled at 3:25.952, after the 5th stack phased Uldzaa (3:25.900), 0.6 s before Big Bang 2's cast.
- 14 pulls had a druid tank; it was out of bear form when `take` came in 3 (all uld3): window 9 `603_26` 8.9 s
  late, window 14 `603_30` 14.9 s late (he was loose 16.5 s, the shadow priest among the dead, so Big Bang 2 had no
  disperser: round 6's uld3), window 15 13.9 s late. Elsewhere it was already in form or 0.5-2.5 s late.
- Cause: the generic druid nodes (`caster form` is the prerequisite of the party heals; `CastCasterFormAction`
  shifts out while mana is above `MediumHealth`), shared class code.

Fix: `algalon tank form hold`, a `Block` on `caster form` for tanks while he is engaged (`UldDefinition_Algalon.cpp`).
Dire Bear Form's own `caster form` prerequisite is vetoed but the re-pushed shift skips prerequisites (`Engine.cpp`
`PushAgain`), so the shift still goes. A druid in caster form at the pull still heals until its first shift (1:27 in
every pull). 10 and 25-man alike; a human druid is untouched.

**Not picked.**

- Tank deaths and loose: Q2 stands (every tank death the reader checked had a tank healer alive and unphased 4-33 yd
  off).
- Stars in Big Bang casts on hiders (5): uld3's 1655 was released `early` after a hunter's Piercing Shots and Wild
  Quiver took 5.3% in 20 s, then couldn't be killed before the cast (`gap`, `prebang`, `under` flipping 3:18-3:24);
  Piercing Shots is a talent bleed the no-DoT guard doesn't see. Next candidate.
- Personal buttons ready and unpressed at 4 explosion deaths (Uldxaq Ice Block 4:01.0, Uldyah Ice Block 1:33.7, Uldyal
  Shadow Ward 4:19.4, Uldzag Cloak of Shadows 4:02.7), and uld2's `urgent` shelter kill (1:33.7) went off with no
  cover and three Aura Masteries ready: not traced.
- uld2's Uldyaj killed by Big Bang 2 on hide `none`: not traced.

**Questions for the user.**

- Q5 (shared code, another lane): a spare bear druid leaves bear form to heal and can run itself out of the mana to
  shift back; Algalon now blocks that, other bosses don't. Recommendation: a druid lane to keep tank druids in form
  in boss fights.

**Next window should show** (`algalon.py --stars`, scratch `alg3/explo.py`'s `after-smash` column): no `killed`
star (`deadline`/`open`) going off 5-10 s after a meteor impact with anyone under the line (window 15: uld1 +5.8 s 1
dead, +5.0 s 3; uld3 +7.2 s 1, +6.6 s 2, +9.9 s 1; uld2 +7.7 and +8.2 s, 0), and `smashrecovery` in the
`algalon.starwindow` timeline after impacts. Watch: no due star drained into the next meteor's blackout because its
kill was held (`drained`/`chewed` with "a meteor live"), no two explosions within 8 s, no due kill pushed into Big
Bang's `prebang`. With a druid tank: no `dire bear form` FAILED run while its `algalon.tankrole` is `take`, Dire Bear
Form (9634) up from its first shift on, and its swap within 2 s of the 4th stack (window 15 uld3: missed, 5th stack
at 3:25.9).

**Review.** The hold for an impact still to land lifted at the planned impact (marker + 4.8 s), but the hit comes
-0.06 to +0.41 s after it (median +0.16, 203 waves in 25 traces, `smash_waves`), and the 250 ms tick lands in that
gap about half the time: with nobody under the line yet, a held kill opened then and the keep test carried it to
impact + 5 s. Fixed: an impact counts as still to land for its marker's 5.5 s (`MARKER_LIFETIME_MS`). Left: the
serial deadline doesn't count the 5 s hold (round 3: lengthening the blackout moves every due and serial deadline); the
watch above covers it. No druid tank cast Rebirth or Innervate in any of the 14 pulls with one, so the form block
costs only its party heals.

## Round 8 (2026-11-23, build 8bc2a82d3 with round 7 and the shared ReadyTankDefensive fix d70101432)

**Data.** Window 17, one valid pull per pool (`raidtest.py report --boss algalon --raid uld --head 8bc2a82d3
--pulls 10`). All 25 Algalon traces on disk reread for the explosions after Big Bang 2 and the star team's time
(scratch `alg8/`: `postbb2.py`/`leftover.py`/`wave3.py` explosions, leftovers and wave fates after Big Bang 2,
`spacing.py` explosions against the previous one and the meteors, `drainsrc.py`/`hpcurve.py` raid under the line,
`healmana.py` healer mana, `teamtime.py`/`teammoves.py`/`movedist.py` the team's walking, `holdsim.py` the spot rule
replayed on the real star tracks).

| Pool | Run | Trace `603_*_algalon-the-observer_*` | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261123-150716-11` | `603_11_…_1795446539` | wipe 5:45, Ascend 5:42.4, boss 33% | 34: loose 12, Constellation Barrage 8, star explosion 6, on tank 4, Cosmic Smash 3, Big Bang 1 |
| uld2 | `tr-20261123-150455-10` | `603_10_…_1795446431` | wipe 5:39, Ascend 5:36.5, boss 41% | 32: loose 12, Barrage 8, star explosion 6, on tank 5, Dark Matter in Big Bang 1 |
| uld3 | `tr-20261123-151158-12` | `603_12_…_1795446821` | wipe 5:09, Ascend 5:06.5, boss 46% | 30: loose 14, on tank 7, star explosion 7, Black Hole DoT 1, Barrage 1 |

What started each wipe:

- uld1: one death (the holder, 1:45.1, 109 k in 6 s, its tank healers 29-33 yd off) before Big Bang 2. Of wave 3's two
  leftovers, 1645's `deadline` kill landed 4:05.2 (4 under the line, 0 dead) and 1644 drained 2.1 s after the 4:21.5
  meteor (4:23.6, 17 under, 5 dead, 2 healers); Barrage took 2 more, Big Bang 3 the holder on `noshelter` (36 yd from a
  hole), then tank deaths 5:13.7-5:27.4 and 23.5 s loose.
- uld2: one death before Big Bang 2. 1635's `deadline` kill landed 9.8 s after a meteor with 15 under (4:05.8, 2 dead,
  a healer), its twin 1636 drained 2.2 s after the next (4:23.8, 20 under, 4 dead); Barrage killed the tank Uldyaa
  (4:51.0), the holder died 4:53.4, loose 14.8 s.
- uld3: the holders died 2:13.2, 2:36.5, 3:01.5 (104-138 k in 6 s, both tank healers alive); a released under-star's
  kill (3:15.5, 7 under, 1 dead) landed 8.0 s before wave 2's 1601 drained (3:23.5, 11 under, 2 dead); the one
  leftover 1634's kill landed 4:09.2 with 13 under (3 dead); tank deaths to 4:47.5, loose 16.2 s.

**Stop rules.** Not polished (three wipes; waived on 226 rosters). The failure picked below is new.

**Verdict on round 7.**

- Meteor aftermath: held on the kills it targeted. No `deadline`/`open` kill landed 5-9 s after an impact (window
  15: 7, 8 dead); two landed at +9.3 (uld1, 4 under, 0 dead) and +9.8 s (uld2, 15 under, 2 dead), opened 3.4-4.3 s
  after the impact as the hold lifted (uld1 nobody under then, uld2's band starting 0.01 s past impact + 10 s).
  `smashrecovery` in the timeline: 25.0, 19.2, 2.8 s. The watch item hit: the hold moved the first of wave 3's two
  leftovers to 4:05, and the second, whose own death sits on the 4:21 meteor, drained onto it (uld1 5 dead, uld2 4).
  Explosion deaths between Big Bang 2 and 3: 6, 5, 3 (window 15: 6, 4, 3).
- Bear form: held. Uldyab and Uldzab stayed in Dire Bear Form from the 1:27 shift to their deaths (one re-shift after
  a rez), no `dire bear form` FAILED, `algalon tank form hold` vetoed caster form 8 and 5 times.
- Shared ReadyTankDefensive (d70101432): not exercised. No `algalon.handover` row in any pull: every Big Bang was a
  Dispersion soak but uld3's third (no disperser, a paladin soak with the pair dead), and `DefensiveHandoverDue` needs
  a tank soak. Nothing to go back and forth.

**Failures by deaths** (96): tank burst 16 and loose 38 (Q2 stands); star explosion 19 = wave 3's leftovers after Big
Bang 2 14 (uld1 5, uld2 6, uld3 3), uld3's pre-Big Bang 2 pair 3, Big Bang casts 2; Barrage 17 (5 within 30 s after the
leftovers' explosions, 12 in the collapse); Cosmic Smash 3; Big Bang 1; Dark Matter 1; Black Hole DoT 1.

**Picked: wave 3's leftovers after Big Bang 2** (14, and the start of the uld1 and uld2 wipes). Bug: the team is meant
to work a star from a spot of its own, but the spot followed the star and the team spent a third of its star time
walking.

- Structure: wave 3 spawns at 136.5 and drains to death at 236.5, 1.6 s after the meteor at 234.9 (T0 + 30.9 + 25.5 k),
  in every pull. After Big Bang 2's hit (188.5) the aftermath, the 209.4 meteor and its `smashrecovery` leave
  219.4-230.9 clear: one explosion fits, two only 8-11.5 s apart. 25 traces: one wave-3 star left after Big Bang 2 cost 1.5 dead
  a pull (8 pulls), two 6.2 (13), whether the second was killed 11-14 s after the first (9, 3, 1, 4, 2, 4 dead, before
  round 7) or drained onto the meteor (5, 4, 0).
- Two are left when the bulk lands only one of the two wave-3 under-stars it planned: the other is released `late` 8-14
  s before the cast at 38-70% (12 of 25 traces; window 17 uld1 1645 at 66%, uld2 1636 at 70%). Bulk median +0.4 to +3.0
  %/s beyond the drain over a pull (planned +5); both need ~+3.1 (~94% extra by cast − 5 s, less wave 2's kill). The
  two pulls that landed both ran +2.8 and +1.8.
- The team's star damage per second is far under its damage on him: uld1's mage 2.1 k against 7.1 k, warlock 4.1
  against 6.8, hunter 4.2 against 5.6 (snapshot `dealt` by target). Over its focus time a member walked 14-57%
  (median 37%) and cast at the star 8-60% (`teamtime.py`, 3 pulls). Moves: 80-111 a pull by `algalon raid position
  action` on `star` (median 8.6 yd, star 29-32 yd off) and 27-51 by `reach spell` (star 37-40 yd off).
- Cause, `StarTeamSpot` (`UldEncounter_Algalon.cpp`): a star wanders up to 25 yd round its spawn (`MoveRandom(25.0f)`)
  at a median 2.5 yd/s, and the spot sat 25 yd from it, so it moved with the star; the raid position action re-walked
  each time it drifted 4 yd (`ULDUAR_ALGALON_SLOT_TOLERANCE` × 2), about every 1.9 s a member, and `CanCastSpell`
  refuses a cast while moving. With the star back within 28 yd of the slot the spot was the slot, another walk.
- Replay (`holdsim.py`, the real star tracks of windows 15 and 17, 6 pulls, 1838 s of member focus time): the current
  rule gives 28% walking and 44% castable time (still and in range, less 1 s per broken cast), against the 42% cast at
  the star the traces show. Holding while the star is 12-28 yd off: 22% walking, 54% castable, walks 427 → 269 (8 yd:
  56%; 16 yd: 52%; the standoff past the hold barely matters).

**Fix** (Algalon files only): `StarTeamSpot` holds a ranged team member where it stands (`algalon.spot=starhold`)
while its focus star is `ULDUAR_ALGALON_STAR_TEAM_HOLD_MIN` 12 to 28 yd off, it is unphased, in the room and 13.5 yd
clear of him; otherwise as before. 12 yd keeps the hold off the kill's 7 yd clearance (5 yd for a kill under way).
Unchanged: the `nearhole` spot before Big Bang (checked first), dodges, the planner. 10 and 25-man alike; humans are
never on the team.

**Not picked.**

- Tank deaths and loose: Q2 stands (uld3's seven took 104-146 k in 6 s with both tank healers alive).
- Opening the second leftover inside the 8 s `gap`: it would land 8-11 s after the first, worse than the 11-14 s that
  killed 3.8 a pull before round 7; with two leftovers no timing wins.
- uld3's pair: at 3:10 the fallback took 1636 to land 5.8 s before 1601's drain death, since 1636's own death sat in
  Big Bang 2's prebang blackout. Design.
- Healer mana: one healer per pool was under 15% by 4:00 (Uldxae, Uldyaf, Uldzae), so the raid stayed under the line
  10-15 s after a meteor (uld2 15 at +9.8 s, uld3 13 at +13.3 s, against the round 7 curve's 4 at +10).
- The team's other casts on the focus (12-27%: buffs, Mana Shield, Hunter's Mark) and the bulk planned at min(measured,
  6) %/s: not traced.

**Questions for the user.**

- Q6: if wave 3 still leaves two stars after Big Bang 2 in most pulls with this fix, add a fourth ranged to the 25-man
  star team from wave 3's spawn to Big Bang 2's cast? It costs one damage dealer on him for ~45 s a cycle.
  Recommendation: judge this fix first; add the member only if two leftovers stay the rule.

**Next window should show** (`algalon.py --stars`, `--formation`, scratch `alg8/teamtime.py`, `leftover.py`): the
team walking about 30% of its focus time (window 17: median 37%), `algalon.spot=starhold` most of its star time, the
bulk beyond the drain above window 17's median +1.7 to +2.3 %/s, fewer `late` releases before Big Bang 2 (window 17:
2 of 3) and wave-3 stars left after it (2, 2, 1), and explosion deaths between Big Bang 2 and 3 (6, 5, 3). Watch: a kill
dropped or flipping because a holding member stood within 5 yd of its star, team members in his reach, and `reach
spell` walks on `starhold`.

**Review.** The cause holds on window 17: 80-111 raid position walks on `star` a pull, and stars move at 2.5 yd/s 80%
of the time (mean 2.0). The walking range above read 27-57% but `teamtime.py` gives 14-57%, median 37%; the replay's
current rule gives 28% where the traces show 37%, so its 6-point cut puts the hold near 30%, not 20%. `teamtime.py`
counts each proc or triggered spell as a 1 s cast at something else (34075 Viper, 15258 Shadow Weaving, 15290
Vampiric Embrace, 31818/57946 Life Tap), so its cast-at-star share undercounts and the 12-27% other casts are mostly
those. Watch too: a member holding far from its ring slot can stand farther from a hole than the old spot let it,
since `FarHoleSpot` measures from the slot (team members on `noshelter` at a Big Bang).

## Round 9 (2026-11-23, build 99188e0f1 with round 8)

**Data.** Window 19, one valid pull per pool (`raidtest.py report --boss algalon --raid uld --head 99188e0f1
--pulls 10`). All 28 Algalon traces on disk reread for under-star releases and what lands on a star after the team
leaves it (scratch `alg9/`: `teamspot.py` team walking and `starhold` share, `holdwatch.py` round 8's watch items,
`timeline.py` boss health, roles alive and healer mana every 20 s, `explokind.py`/`aftersmash.py` explosions by
release and by time since the meteor, `early.py`/`overcut.py`/`inflight.py`/`leave.py` what took a star under its
band, `offfocus.py` team damage on stars it wasn't on, `dotcasts.py` DoT casts on stars and the action behind each).

| Pool | Run | Trace `603_*_algalon-the-observer_*` | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261123-172938-18` | `603_18_…_1795455081` | wipe 4:18, Ascend 4:15.6, boss 54% | 34: loose 14, star explosion 12, on tank 5, Constellation Barrage 2, Dark Matter in Big Bang 1 |
| uld2 | `tr-20261123-172536-16` | `603_16_…_1795454839` | wipe 4:27, Ascend 4:25.0, boss 58% | 31: loose 13, on tank 6, star explosion 6, Cosmic Smash 3, Barrage 2, Black Hole DoT 1 |
| uld3 | `tr-20261123-172657-17` | `603_17_…_1795454920` | wipe 7:10, Berserk 6:26, Ascend 7:07.6, boss 32.5% | 33: loose 19, star explosion 5, on tank 5, Dark Matter in Big Bang 2, Black Hole DoT 1, Big Bang 1 |

What started each wipe:

- uld1: under-star 1582, released `early`, went off at 1:54.5 on 17 under the line, 5.0 s after under-star 1584 (4
  dead, healers Uldxad and Uldxae among them, both brought back). Before Big Bang 2 wave 2's 1603 was killed at
  3:15.6 (7 under, 1 dead), then the released wave-3 under-star 1645 went off 1.0 s before the cast (3:25.6, 14
  under, a meteor live, 4 dead with Uldxad and Uldxae again) and 1647 in the cast on the running healer Uldxab:
  one healer left, the three tanks died 3:40.0-3:48.1, loose 27 s, 17 dead.
- uld2: the holder Uldyaa died at 3:02.0 (92 k in 6 s, both tank healers alive 26-28 yd off), Uldyab at 3:09.9,
  Uldyaz at 3:15.6; `urgent` (no hole left) killed wave 2's 1602 at 3:15.1 (5 under, 2 dead); loose from 3:16, and
  the wave-3 leftover 1634 drained at 4:03.4 with him loose (4 dead).
- uld3: 10 dead by 3:33.5 (wave 2's 1604 drained 13.6 s after a meteor with 7 under, 3 dead; the holder Uldzaa at
  3:24.9, 88 k in 6 s), then nobody for 3 min; Berserk at 6:26.1 with healer mana at 16/3/1/1%, the pair died
  6:38.8-6:49.1 (Uldzab 230 k in 2 s), loose.

**What made uld3 last to 7:10.** It lost no healer and only one tank before Berserk. Both wave-3 under-stars went
off inside Big Bang 2's cast (no leftover), and every later star went off in Big Bang 3's or 4's cast or 0.5 s after
its hit, on 6 bots at most and 0 dead, so after 3:18.5 no explosion hit the raid outside a Big Bang. The bear
Uldzab and the blood DK Uldzaz then held him with all four healers up, the two tank healers putting 87-88% of their
healing on the pair (uld2: 75-83%). It still had no kill in reach: 57.9% at 3:26, 34.1% at Berserk, 32.5% at the
end. uld1 instead lost three healers to stars by 3:31.5 and its tanks within 17 s; uld2 lost its holder to burst at
3:02.

**Stop rules.** Not polished (three wipes; waived on 226 rosters). The failure picked below is new as a pick (rounds 2
and 7 named it the next candidate).

**Verdict on round 8: partly held.**

- Walking cut as forecast: the team walked 31, 27, 25% of its focus time (window 17: 36, 33, 37), `starhold` covered
  47, 60, 59%, cast at the star 38, 38, 44% (window 17: 41, 31, 34).
- Bulk not faster: +2.3, +1.9, +1.8 %/s beyond the drain (window 17: +1.7 to +2.3).
- `late` releases before Big Bang 2 1 of 3 (uld1 1646 at 70%; window 17: 2 of 3); wave-3 stars left after it 1, 2, 0
  (2, 2, 1); explosion deaths between Big Bang 2 and 3 2, 4, 0 (5, 6, 3), all in collapses with him loose. uld2's two
  were never under-stars: the planner put one of three under at the spawn, the team measured slow.
- Watch items: holding members within 5 yd of the focus star 4.5 s of 598 s on `starhold`, in his reach 2.1 s, 5
  `reach spell` walks on `starhold` (uld3), no team member on `noshelter`.
- Q6 not asked again: two wave-3 stars survived Big Bang 2 in one pull of three (uld2), and neither was an under-star;
  round 8 set "in most pulls".

**Failures by deaths** (98): tank burst 16 and loose 46 (Q2 stands); star explosion 23 = under-stars released `early`
going off before the cast 9 (uld1 1:54.5 4 and 3:25.6 4, uld3 1:56.5 1 in Big Bang 1's cast before anyone hid), wave
2's star in the 16-20 s before Big Bang 2's hit 6 (1, 2, 3), wave-3 leftovers in collapses 6 (uld2 4, uld1 2),
under-stars in casts 2 (a running healer, a tank on `hold`); Barrage 4; Cosmic Smash 3; Dark Matter 3; Black Hole
DoT 2; Big Bang 1 (after Berserk).

**Picked: under-stars over-cut below their band and released `early`** (9, and the start of uld1's wipe). Bug: the
plan means only the bulk's own casts to land on an under-star, stopped at its line ("a spell landing after the line is
1-2% of a 4-5 tick band"), but three kinds of damage the plan doesn't count land after the team leaves one.

- 28 traces: 31 `early` releases, 55 dead (26 went off outside a cast, 34 dead; 5 inside, 21). After the bulk finished
  a star (172 times) a median 1.3-2.2% beyond the drain landed in the next 3 s, p75 3.8-4.5%, up to 9-19%; 12 went
  `early`. The team put 27% of a star a pull on stars it wasn't on, 18.4% on under-stars: Shadow Bolt 4.9, Explosive
  Shot 2.7, Arcane Blast 2.4, Auto Shot 1.9, Piercing Shots 1.1, Corruption 0.5.
- uld1 1645: at 3:09.205 a `deadline` kill on 1603 took the team off it at 29%, 3.5% above its line, and 10% landed in
  1.6 s: three Explosive Shot ticks (cast 50 ms before the switch, 2.3-2.4% each), an Arcane Blast finishing 0.19 s
  after it (2.24%), a Shadow Bolt in flight (2.29%), an Auto Shot (0.78%). Released at 17% at 3:10.8 (it needed 18.8 s
  of drain), it went off 1.0 s before the cast. `StopCastsAtLine` (`UldEncounter_Algalon.cpp:2804`) interrupts a cast
  on an under-star only at the line, so one at a star the team left above it runs on. Window 7 uld1 `603_42` 1655: an
  Arcane Blast landed 1.15 s after an `open` kill took the team, 6.6%; `early` at 25%, 10 dead at 3:26.7.
- uld1 1582: Uldxak's Corruption (1:25.0, refreshed by Everlasting Affliction off the bulk's Shadow Bolts) ticked 5.2%
  after the bulk finished it at 1:33.8: `early` at 19% at 1:38.5. It came from `corruption on attacker`: the target
  guard (`UldMultipliers_Algalon.cpp:96`) judged a spell named `current target` on the bot's current target, but
  `CastDebuffSpellOnAttackerAction` keeps that name and overrides `GetTargetValue()` ("attacker without aura"). 6
  Corruption casts on stars in 28 traces, all from it (one an unlogged repeat of an OK row); window 12 uld1 `603_21`
  1675: 6.5%, `early` at 10%, 3 dead; window 19 uld3 one more (Uldzal).
- Explosive Shot is a periodic dummy (aura 226, 1 s) casting 53352 with the aura's amount, +16% ranged attack power
  from `spell_bonus_data`, so neither the DoT guard's `MostlyPeriodic` nor `PendingPeriodicHp` (`:360`) sees its two
  later ticks: 1.5% or more of what landed in 5 of the 14 releases within 3 s of a leave.

**Fix** (Algalon files only):

- `StopCastsAtLine`: a bot's cast, channel or wand on an under-star that isn't the focus is interrupted at once (it was
  only at the line), the group's skull excepted as in `AlgalonMayDamage`. The bulk's own finish is unchanged.
- `AlgalonTargetGuardMultiplier`: every damage spell is judged on `action->GetTarget()`, the same unit as before for a
  plain `current target` spell; positive spells on another target stay unjudged.
- `PendingPeriodicHp`: Explosive Shot's remaining ticks count, each its amount through `SpellDamageBonusDone` and
  `SpellDamageBonusTaken` for 53352, so the line and the bulk's work left net them out.

10 and 25-man alike (ticks valued by the core); humans' casts untouched; nothing keyed on a real player. Replay by hand:
1645 keeps the Arcane Blast (in progress at the switch) and stays under at ~19.2%; window 7's 1655 keeps 6.6% (31.6%);
1582 without the Corruption is ~24% at 1:38.5, above its ~21% release line. Not covered: projectiles a member
launched before starting its next cast (no longer its current spell), and splash on idle under-stars (below).

**Not picked.**

- Wave 2's star in the 16-20 s before Big Bang 2 (6, every pull): its kill or drain landed 10.3-13.6 s after the 3:05
  meteor with 5-7 under, where uld1 and uld2 had 0-4 under and at most one under 14 k in the 10 s before that meteor
  (uld3 9-10, after another star at 2:51). `SerialDue` leaves the 10 s `smashrecovery` hold out of its budget (round 7's
  review), so the star turns due too late to land before the meteor's blackout and is held to impact + 10 s. Before
  round 7 these landed 3:10-3:13 (4 dead in 6 pulls), since then 3:15-3:24 (8 in 6); the two landing before the
  meteor (2:54.6, 2:58.6) killed nobody. Next candidate; it moves every due and serial deadline and costs bulk time.
- Idle under-stars splashed under their band: uld1 1584 (4.9% in 21 s), uld3 1581 (6.4% in 32 s: Wandering Plague,
  Living Bomb, Fire Nova, Cleave, Consecration).
- Cosmic Smash in Big Bang 2's hide (uld2 3:30.3, 2 dead): the hide brought Uldyac back to 4.6 yd and Uldyau to 11.7.
- Tank deaths and loose: Q2 stands (uld2's six with a tank healer alive 17-26 yd off).

**Questions for the user.** None.

**Next window should show** (`algalon.py --stars`, `--collateral`, `--verdict`; scratch `alg9/early.py`,
`offfocus.py`): no `early` release within 3 s of a paced kill taking the team off that under-star (window 19: uld1
1645); no "DoT on a star" (window 19: uld1 and uld3, one each); team damage on under-stars it isn't on under window
19's 23.1, 15.7, 21.2% of a star; `early` releases under window 19's 4, 0, 2. Watch: bulk speed (interrupted casts are
lost time; window 19 +1.8 to +2.3 %/s), under-stars above their band at the cast (window 19: uld3 1631 at 12.5%),
`late` releases.

## Round 10 (2026-11-24, window 30 build 897faf0d8: round 9 and every shared change since)

**Data.** Window 30, one valid pull per pool (`window30-algalon.md`), plus window 21 (build 90400b39c, round 9;
`603_16/17/18_…_17954618xx`, unread by any lane) for the verdict. Scratch `alg10/`: `offfocus.py` (team damage on
idle under-stars, `early` releases against the team's last switch), `undersplash.py` (hits on idle under-stars in
the 25 s before each cast, by source), `castearly.py` (stars going off in a cast before hit − 4.6 s, all 37
traces), `starhits.py`, `starnear.py`.

| Pool | Run | Trace `603_*_algalon-the-observer_*` | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261124-171319-30` | `603_30_…_1795540502` | wipe 2:36, him 74% | 31: star 11, loose 7, on tank 6, Barrage 3, Dark Matter in Big Bang 2, Black Hole DoT 1, Big Bang 1 |
| uld4 | `tr-20261124-171207-28` | `603_28_…_1795540431` | wipe 5:25, 38% | 33: loose 16, Barrage 6, star 5, on tank 3, Big Bang 2, Cosmic Smash 1 |
| uld2 | `tr-20261124-171711-33` | `603_33_…_1795540734` | wipe 6:12, 26% | 32: star 11, loose 7, on tank 6, Barrage 5, Cosmic Smash 1, Dark Matter 1, Black Hole DoT 1 |
| uld5 | `tr-20261124-171249-29` | `603_29_…_1795540471` | wipe 4:54, 43% | 31: loose 13, star 9, on tank 6, Barrage 3 |
| uld3 | `tr-20261124-171339-31` | `603_31_…_1795540521` | wipe 6:03, 34% | 33: star 11, loose 10, Black Hole DoT 6, on tank 6 |
| uld6 | `tr-20261124-171339-32` | `603_32_…_1795540522` | wipe 5:36, 42% | 32: Big Bang 13, star 7, loose 7, on tank 4, Barrage 1 |

What started each wipe:

- uld1: two stars at 1:57.58, 1.5 s into Big Bang 1's cast, on 22 runners: 11 dead, 4 healers (the pick). Tanks
  died 2:07-2:23, then loose.
- uld4: Big Bang 2's hide, holes 30-41 yd from the pair: Ulduaa ran through a meteor (7.6 yd) into an under-star,
  Ulduaz on `noshelter` with no button died to the hit. 5th stack on the last tank at 4:28, loose 10.5 s.
- uld2: the pair died to burst 5:27-5:54 with a tank healer alive; a `deadline` kill on 13 under the line, 8 dead.
- uld5: under-star 1644 went off 0.8 s before its window with a meteor live (3 dead): the bulk's Arcane Missiles put
  8.6% on it in 1.1 s at its line. Tank deaths 4:06-4:25, tank healer alive 15-26 yd off.
- uld3: a `deadline` kill on 5 under (3:12.4, 4 dead); 1666 went off 0.6 s into Big Bang 3's cast (3 dead), 6 more to
  the void's DoT after it; tank deaths from 5:32.
- uld6: a constellation on the handler (4:40.5) and one diving on the healer Uldwad (4:56.7) ate the near holes;
  Big Bang 3's only hole stood 44-73 yd out: 13 to the hit, 7 to two under-stars.

**Stop rules.** Waived (user, 2026-11-22). The pick is round 9's family, under-stars cut below their band, a second
round.

**Verdict on round 9: held** (w21 and w30, 9 pulls). No `early` release within 3 s of a paced kill taking the team off
(w19: 1); the 5 within 1 s of a switch all followed the bulk's own. No DoT on a star (w19: 2). Team damage on
under-stars it wasn't on: 14.3, 13.1, 3.1 (w21); 12.6, 13.7, 12.8, 9.3, 10.8, 13.8% of a star (w30), against 23.1,
15.7, 21.2. `early` releases 1, 2, 0 and 2, 1, 1, 0, 1, 0 (w19: 4, 0, 2). Watch items: bulk +1.6 to +3.0 %/s beyond the
drain (w21 uld2 +0.4); w21 uld3 1642 at 12.1% at Big Bang 2's cast; `late` releases 6 in w30's 6 pulls.

**Picked: stars going off in a Big Bang cast's first seconds, before anyone hid** (18 dead, 4 of 6 pulls; uld1's
wipe). Since round 6, 8 such stars in 18 pulls killed 23, every one an under-star cut below its band, all but one
within 16.4 s of the cast. For Big Bang 1 and 3 the meteor 8-10 s before the cast, its blackout and `smashrecovery`
close the last ~14 s, so a late `early` release drains into the cast with the raid out. Bug: two splash sources
reach stars past the hold.

- uld1 1585, under, 14.6% at 1:48.1 (inside its band), then the holder Uldxaz's Revenge for 3775 and 3952 (1:50.8,
  1:55.9), Living Bomb 2765, Wandering Plague 2178, Magma Totem 772: 1.6% at the cast against its 4.0-9.6% band, gone
  on 1584's tick. Revenge (57823) is single-target in the DBC; Improved Revenge (12799) adds a melee chain jump, 10 yd
  from him (`Spell::SearchChainTargets`) plus the star's size, and `revenge` was in no hold set.
- Felguard Cleave (47994: weapon damage, chain 2) is a PetAI autocast, which no bot action or multiplier sees: 1.4%
  of the 2.9% that took uld3's 1666 below its band at 4:41.7 (the bulk's last Arcane Blast had left it 1.4% above the
  bottom), 0.7% of uld5's 1585.
- Off-team hits on idle under-stars in the 25 s before a cast, 9 pulls, % of a star: pet Cleave 4.8, Revenge 4.5,
  Living Bomb 4.1, Wandering Plague 3.5, Consecration 3.1, Maul 1.3.
- uld1 1584 (`early` 38 s out, the bulk 8% past its line) had no slot: due from 1:31.9, but the 1:31.8 shelter kill's
  gap, then the 1:48 meteor's blackout and `smashrecovery` ran into the prebang.

**Fix** (Algalon files only):

- `AlgalonSplashHoldMultiplier`: `revenge` joins the chains, held while a star is within 15 yd of the bot or 20 yd
  of him. Revenge went out 0-22 times a pull, half with a star that near.
- `HoldPetSplash` (tick): a bot's pet with a star within 15 yd of it, or the same reach of its victim (the star it
  hits aside), has its non-positive area and chain autocasts switched off (`Pet::ToggleAutocast`), remembered per
  owner and back on once none is near, outside phase 1, or before the wipe reset; `algalon.petsplash`
  `off|on <spell>` on the owner. Humans' pets are left alone.
- Reader: `--collateral` counts the holds per owner (test added).

10 and 25-man alike. Replay: uld1's 1585 keeps 3.0%, 4.6% at the cast, inside its band, so 1584 goes off alone at
cast + 1.5 s with 3 under the line; uld3's 1666 keeps 1.4%, landing on its band's bottom at 4:41.7. Cost: the
Felguard's Cleave on him while a star is near (one within 20 yd of him 37-44% of the time stars lived), a few
Revenges.

**Not picked.** The leads in State: the bulk's overshoot at its line and in-band casts, the `gap` return, the
remaining splash, w21 uld2's late under-star (14 dead), uld4's tanks far from holes, uld6's holes (Q4 (b)).

**Questions for the user.** Q4 (b), carried in State, not yet asked.

**Next window should show** (`algalon.py --collateral`, `--verdict`, `--stars`; scratch `undersplash.py`): no Revenge
and no pet Cleave or Thunderstomp in "damage on stars, off the star team" (w30: Revenge 7.5-27.2 k in 4 pulls, pet
Cleave 3.5-16.1 k in 6); `revenge` among the splash hold vetoes; a "pet splash autocast held" line for each Felguard
warlock, with `on` rows after each `off`; fewer "fell below its band" verdicts (w30: 2) and stars going off in a cast
before hit − 4.6 s (w30: 4, 18 dead). Watch: him loose by threat (w30: uld4 once), the Felguard's damage on him.
