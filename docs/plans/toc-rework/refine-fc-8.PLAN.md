# refine-fc-8: keep the kill target, stop the Cyclone ping-pong, defensives that save

Wave 4m, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4l `921f11ecf`.

## Why

25H, 2026-10-20, `env/dist/logs/botobs/649_2_caiphus-the-stern_1792501493.ndjson` (wipe 1:57, Thrakgar
killed 0:60) and `649_2_liandra-suncaller_1792501757.ndjson` (wipe 1:36, no kill). Grip chain, kicks and
purges worked.

- **Latch left a half-dead target.** `pulled` is geometry only (20+ yd off the pack, nobody within 12):
  Thrakgar at 40% lost it when the DK walked up, and a wandering Erin with no grip took the latch (pull 2
  0:36.5, 99 → 72% in 59 s); pull 1 1:12.4 the same off Caiphus. Druidica then Cycloned Thrakgar.
- **Cyclone ping-pong** (12/18, 10/15 own breaks): the 2 s release lead plus the healer exception sent
  Druidica between Erin and the rogue, each landing ending the other. 3 Cyclones into Bladestorm.
- **Cyclone stand** both in reach 28% and 50%: the spot farthest from the pack sits at the edge of reach.
- **Personal defensives** fired at 52-89% but 15/24 and 13/21 picks were a healthstone: `TANK_DEFENSIVES`
  has nothing physical for rogues, hunters, Disc priests; Avenging Wrath and an early Divine Protection
  blocked the paladins' bubble. Bots die from full in 1-2 s with 4-8 champions on them.
- **Rebirths** all on melee DPS, dead again in 13-32 s; healers then stayed dead.
- **Healers** cast 11-24%, moved 26-57%: every 6 yd pack drift re-placed all heal spokes (~25 yd walks).

## Scope

1. **Latch.** The `pulled` switch skips when the current target has been out (`outOnce`). `pulled` needs
   a grip: a healer champion that moved `GRIP_JUMP_YD` 10 within `GRIP_JUMP_MS` 1000 joins `gripped`,
   left when back within `PULLED_FROM_PACK` of the pack centre.
2. **CC lead per spell.** Tie and landed use cast time + `CC_RECAST_SLACK_MS` 400 instead of 2 s.
   Candidates under Bladestorm (65947) skipped.
3. **Cyclone stand.** Rings 20/26/30; rank within `reach − SIDE_REANCHOR`, then within reach, then
   nearest the healer; ties farthest from the pack, then nearest the druid.
4. **Personal defensives.** 4+ champions on a non-tank fire the hit pre-emptively at `health − floor + 1`.
   Physical hit with no tank row: Evasion, Deterrence, Pain Suppression on self. Paladin on a measured
   hit: Divine Shield before Divine Protection. Avenging Wrath blocked for tank, frontline and healer
   paladins.
5. **Rebirth** only on healers and tanks.
6. **Heal stands** kept while 20-34 yd from the pack centre and clear.
7. **Reader**: `--kill` grip or `no grip` before each pulled hold; `--stands` healer moving and casting
   share; Cyclone `SPELL_REACH` 33. Tests, docs.

## Targets (next 25H)

No `pulled` switch without a grip; Druidica ≤ 2 own breaks, Cyclone median hold 3 s+, no cast into
Bladestorm; Cyclone stand both in reach 70%+; healers cast 35%+, ≤ 15 stand moves each; class cooldowns
outnumber healthstones, a paladin bubble on a focus, no Avenging Wrath from tank, frontline or healer
paladins; Rebirths only on healers and tanks; first frontline DPS death after 0:20, a second kill; 25N
still kills.
