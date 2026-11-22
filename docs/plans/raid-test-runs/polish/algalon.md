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
