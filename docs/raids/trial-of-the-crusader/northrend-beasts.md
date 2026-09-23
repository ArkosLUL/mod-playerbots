# Northrend Beasts

One continuous fight in three stages. Raid-wide facts and the code layout: [README.md](README.md).

- **Gormok**: Staggering Stomp is a 0.5 s PBAoE around Gormok (15 yd damage, 20 yd interrupt),
  Fire Bombs leave ground hazards, spread.
- **Acidmaw & Dreadscale**: submerge/emerge form swap, avoid slime pools and churning ground, focus
  the mobile worm, spread for sprays.
- **Icehowl**: on the jump-to-centre charge, dodge the Trample, never intercept it: any living
  player within 12 yd of Icehowl during or at the end of the charge gives him Frothing Rage
  (`boss_northrend_beasts.cpp` `DoTrampleIfValid`). A clean miss hits the wall for Staggered Daze
  and a 15 s pause, a free DPS window. Then move out of Whirl and Arctic Breath.

## Known gaps

- `GetWormCastingSweep` checks only Sweep 66794 (10N) and 67646 (25H), not 67644 (25N) or 67645
  (10H), so the sweep dodge never fires in 25N/10H.

## What a trace answers

No `nb.` notes or boss reader yet, only the raid-agnostic streams.
