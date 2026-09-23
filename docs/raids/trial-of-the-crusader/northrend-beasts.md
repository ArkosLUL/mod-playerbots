# Northrend Beasts

One continuous fight in three stages. Raid-wide facts and the code layout: [README.md](README.md).

- **Gormok**: Staggering Stomp is a 0.5 s PBAoE around Gormok (15 yd damage, 20 yd interrupt),
  Fire Bombs leave ground hazards, spread.
- **Acidmaw & Dreadscale**: submerge/emerge form swap, avoid slime pools and churning ground, focus
  the mobile worm, spread for sprays.
  - **Bite and spray carry no aura.** Burning Bite and Paralytic Bite trigger the debuff
    (`EffectTriggerSpell`); each Burning/Paralytic Spray id links it in `spell_linked_spell`. The
    debuffs are Burning Bile 66869 (24 s, no difficulty row) and Paralytic Toxin 66823 (remaps,
    README table). Burning Bile pulses 66870 every 2 s: damage within 10 yd, and a
    `spell_linked_spell` row strips Paralytic Toxin from everyone it hits, so Bile carriers cleanse
    Toxin carriers by standing next to them.
- **Icehowl**: on the jump-to-centre charge, dodge the Trample, never intercept it: any living
  player within 12 yd of Icehowl during or at the end of the charge gives him Frothing Rage
  (`boss_northrend_beasts.cpp` `DoTrampleIfValid`). A clean miss hits the wall for Staggered Daze
  and a 15 s pause, a free DPS window. Then move out of Whirl and Arctic Breath.

## Known gaps

- `northrend worms afflicted by burning` keys on Burning Bite/Spray auras, so it has never fired,
  and its keep-moving remedy is not the mechanic above.

## What a trace answers

No `nb.` notes or boss reader yet, only the raid-agnostic streams.
