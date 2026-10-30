# g12-sindragosa — close Sindragosa's, her drakes' and her gauntlet's gaps

Gap wave `3efg-gaps`, after `3cd-gaps`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap
lanes" section included. Builds on the merged `r12-sindragosa` lane and its review: read
[r12-sindragosa.PLAN.md](r12-sindragosa.PLAN.md) first. Its decisions stand unless closing a gap
needs a change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/sindragosa.md` and in r12's brief. The
drakes' and the gauntlet's gaps are in that brief too.

- 25H Unchained Magic: each set's three spots need a new navprobed layout. The doc's bullet gives the
  constraints.
- One bot tank: no last-phase swap. The Gap lanes' one-bot-tank rule applies.

## Owns

Stems `Sindragosa` and `SindragosaAdds` and their three definitions (`Sindragosa`, `SindragosaDrakes`,
`SindragosaGauntlet`), the doc, the readers `sindragosa.py` and `sindragosa_gauntlet.py` and their
tests, this brief.

## Task list

One lead, no split: the changes sit in the two stems' helpers with small rows each.

1. 25H/10H Unchained Magic: carriers pick a node of a navprobed 5 yd floor grid live (`sg.unchained`
   `spot N`), 25 yd apart and 24 yd off the raid's spots; the fixed polar sets are gone.
2. One bot tank: no stand-in (doc gives the numbers); `icc sindragosa holder cover`, a healer's
   external on a holder no swap relieves from 5 Buffet stacks (`sg.cover`).
3. A missed swap taunt sends the swapper back to M after 1 s (`sg.swap` `missed`).
4. 10-man tomb floor 40%; a tomb the bot can't attack yields the next.
5. Splash: the destination shortcut needs a clear straight walk; a stand whose walk cuts a beacon's
   splash stops and waits (`sg.stand` `wait`).
6. Blistering Cold escapes skip one near a beacon or a tomb.
7. Tank caster form blocked through her fly-in.
8. Drakes: healers split on a double pull; melee on Spinestalker while Rimefang flies; a fouled ranged
   stand moves to an alternate (`... alt`).
9. Gauntlet: `icc sindragosa gauntlet kick` on the Sorceress's Ice Tomb (`sg.gauntlet.kick`), `icc
   sindragosa gauntlet free` on a Frost Nova or Web root inside a whirl (`sg.gauntlet.free`).
10. Readers and tests: `--tanks` covers and `missed`, gauntlet `--kicks` and `--freedom`; the doc.

All done.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| One bot tank: no damage dealer takes the swap; the lone holder keeps buttons from 3 stacks and gets a healer's Pain Suppression, Guardian Spirit or Hand of Sacrifice from 5 | her swing ~29-40k (10N) / 56-77k (25N) raw every 1.5 s (`creature_template` 81.6/158.7 × level-83 base × 1.5 s attack time); WH: 1 tank with cooldowns, externals per breath | a DK or paladin DPS stand-in |
| A swap taunt still off her 1 s later counts as missed: the swapper goes back to M, no retry | 8 s taunt cooldown against 4-9 s left before the breath | retry with another tank |
| Heroic carriers on a live-picked grid node; others first within 46 yd of her, nearest S; healers holder in reach and sight, else S, else nearest S | Backlash 20 yd; the r12 sets 9-17 yd apart; simulation over the probed grid (0 short at westward drift, 2/532 with 6 yd sideways) | a new fixed polar layout, which runs off the floor as she drifts |
| 10-man held tombs stop at 40% (45k) | 20% of 113k left 22.6k for damage in flight | WH's 20% on both sizes |
| A stand walk crossing a beacon's 12 yd stops and waits instead of routing round | the stair foot spots cover every straight line onto the stage | waypoint routing round the cluster |
| Escapes skip one within 12 yd of a beacon or 8 yd of a tomb while another is clear | r12 gap: 150°/210° within 6 yd of A/B | fixed nearest |
| Drakes: healers split by guid on a double pull | tanks ~78 yd apart | all on Rimefang (r12) |
| Melee take Spinestalker while Rimefang flies | nothing else in reach | idle |
| A fouled ranged drake stand moves 5 or 10 yd, floor-, sight- and reach-checked at runtime (40 yd to the drake's landing spot, a healer's 38 yd to its tank spot); melee and tank stands still drop | ranged stand still; melee follow their target | fixed alternates per stand |
| 10-man Tail Sweep: melee stay on his flank | the only safe 82° is his front, under Cleave; the sweep 3.75-6.25k every 22-25 s | a front-quarter spot |
| Spinestalker's tank and melee fear left to class anti-fear | the roar's 35 yd covers all of Tremor's or Fear Ward's 30 yd round his tank but his front | move a shaman or priest into the roar |
| Gauntlet: Ice Tomb kicked by the lowest-guid bot with a reaching interrupt; Frostbolt unkicked | blog silent; the stun hits her victim, mostly a tank; she is immune to fear and horror only | kick everything |
| Gauntlet: roots broken only inside a Frostblade whirl, Hand of Freedom or a priest's Dispel Magic on Frost Nova | blog silent; 3.5k a second for 6 s in a whirl, harmless elsewhere | dispel every root |

Coordinates: the grid's 450 nodes navprobed (`point`, `--nav 0x09`, settled Z 203.373-203.389) and
sight-checked to her at three drifts (`los`, 7 nodes out); sight to T and S at three drifts checked too
(44 blocked rays, the north corridor to a far-west T, handled by the runtime sight try). Drake
alternates are runtime-checked. Every other coordinate is r12's, unchanged.

## Carried over

- Merge stage, README "Trigger names": Sindragosa's list gains `holder cover`, the gauntlet's `kick` and
  `free`.
- Merge stage, README defensives bullet for Sindragosa: `icc sindragosa holder cover` (a healer's Pain
  Suppression, Guardian Spirit or Hand of Sacrifice on a holder no swap relieves, from 5 Mystic Buffet
  stacks, `sg.cover`).
- Merge stage, README "Before live" row for Sindragosa: the rule `icc sindragosa tank form` also holds
  through her fly-in.
- Merge stage, status row: r12's "one bot tank: no last-phase swap", "25H unchained spots", "10-man
  tombs", "splash dodge", "drakes double pull", "10-man Tail Sweep" and "no gauntlet Frost Nova, Ice
  Tomb or Web answer" are closed or decided here.
- r14: as r12's, plus `StaticSight` in both stems and a sixth `CanPayFor` copy.

## Known gaps

- Those in the boss doc.
- No 631 trace; the user built `623638b99` clean, this change needs a rebuild (no new `.cpp`, no CMake
  re-run); no `-Werror` pass beyond `pb-check`.

## Blocked
