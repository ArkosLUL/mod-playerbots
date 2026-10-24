# refine-fc-13: ranged kite to the re-pick, escapes that land clear

Wave 4t, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4s `b02871a99`, which the
pulls below ran.

## Why

2026-10-23 late, two 25H wipes: `env/dist/logs/botobs/649_5_caiphus-the-stern_1792785686.ndjson` (5:20) and
`649_5_caiphus-the-stern_1792786170.ndjson` (3:49). 4s worked: all three healers died, a living healer off the
latch ≤ 2.5 s. But the raid died alongside: 3+ champions on one raider in 88% of samples, a death about every
10 s (the 9 s re-pick); 7 and 18 dead when the last healer fell.

- **No kite:** ranged DPS stood a median 7.6-14.5 yd from the nearest melee champion, within 8 yd 21-53%; with one
  on them within 8 yd they moved 0-12%.
- **Escapes given back:** teleports that kept 26-58 yd made the chasers drop the bot within 2-9 s; a warlock that
  walked back (`reach spell`) was caught in 7 s; Blink gained 11-21 yd, the chaser back within 2 s.
- **Frost Nova and Shadowfury never escaped:** the AoE gate counts unlanded CC duties and root peels.
- **Blink into the pack:** it fled only the champions on the bot.
- Champions run at our speed (p90 7-8 yd/s): running only holds a gap an escape, CC or snare opened.

## Scope (the user's calls)

1. **Kite:** a ranged DPS bot with a melee champion on it within 30 yd that is past 6 yd, CC'd, rooted or
   snared runs 15 yd hops away (floor, clear of AoE, off our healers, farthest from melee champions) until none
   is within 30 yd. `reach spell` held while a melee champion on it is within 45 yd. Notes `fc.kite`.
2. **Escapes:** the AoE gate blocks only on landed breakable fear, stun or confuse, the grip guard, and the kill
   target for fears and Dragon's Breath; Blink flees every melee champion within 15 yd and lands 12+ yd off all;
   hunter Disengage.
3. Healers keep the step-off; snares, peels and healthstones unchanged.
4. **Reader:** escapes and kites with chaser distance at +2 s and +6 s and `dropped`; ranged distance to melee
   in `--stands`. Tests, docs.

## Targets (next 25H)

Kites after most escapes, the chaser dropped by +9 s in over half; Frost Nova and Shadowfury escapes; ranged
moving share with melee on them well above 0-12%; fewer ranged deaths before the last healer than 4-5; 25N
still kills.
