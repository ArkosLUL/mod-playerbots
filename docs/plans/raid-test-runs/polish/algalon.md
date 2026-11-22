# Polish log: Algalon the Observer (25-man, no hard mode)

Trace slug `algalon` (pulls file under `algalon-the-observer`), raid `uld`, scenario `uld-algalon`
(Celestial Planetarium Access console with a lent key; he is passive until attacked). No hard mode.
Reader `tools/botobs/bosses/algalon.py <trace>`: `--verdict` first, then `--deaths`, `--stars`,
`--tankheal`, `--defensives`. Boss health is the snapshot `hp` column of entry 32871; per-bot damage
is the snapshot `dealt` column split by the bot's target (scratch scripts, not in the repo).

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
