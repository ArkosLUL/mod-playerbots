# refine-fc-12: kill every healer, heal from the start, let casters escape

Wave 4s, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4r `3c99c4394`, which the
pulls below ran.

## Why

2026-10-23 evening, two 25H wipes: `env/dist/logs/botobs/649_5_caiphus-the-stern_1792776040.ndjson` (3:45,
Thrakgar, Caiphus and Ruj'kah killed) and `649_5_caiphus-the-stern_1792776441.ndjson` (3:07, Thrakgar and
Caiphus). 4r worked: healers moved 4-29% (was 56-84%), raid healing 11.1k/s (was 5.8k/s).

- **Last healer never killed:** at each kill switch Erin Misthoof (Resto Druid, an elemental) sat under our
  own Banish or Cyclone, so the pick took the next touchable champion (Hunter, Mage) and nothing brought it
  back; she then stayed the first CC candidate for 96-138 s. Our Banish on her ran 3-6.1 s a cast.
- **Healers start 40-53 yd behind the tanks:** staging deals healers and ranged the slots farthest from the
  landing (69-83 yd out on the landing → `ARENA_CENTER` line; tanks 30, melee 33-42). Healers walked 25-31
  yd in the first 10 s, first heal 4-9 s in. The pulled healer lands among them (grip posts 54 and 78 out).
- **Casters die 3-8 s after two melee champions reach them** (DK, Warrior, Ret, Rogue; holders hold
  14-32%). Blink is zeroed by `faction champions stay put`, Nitro needs 3 champions, Frost Nova, Dragon's
  Breath and Shadowfury are vetoed by the AoE ban, Psychic Scream and Howl of Terror never tried, Demonic
  Circle unimplemented. Warlocks fought a median 16-34 yd from their pre-pull spot.

## Scope (the user's calls)

1. **Healers first:** a healer the pick skips for being immune is suspended (no CC lands on it) and takes
   the latch back from a non-healer once touchable, whatever the health; a touchable healer takes the
   latch from a non-healer (`healer`). The skull still wins.
2. **Stands:** healers off the grip lane (10 yd either side, landing centroid to the player's post), then
   nearest 12 yd past the front tank stand; ranged off the lane, then farthest; melee unchanged.
3. **Escapes** for a ranged or healer bot with a melee champion on it within 8 yd, CC before movement:
   mage Frost Nova, Blink away (floor, clear of Bladestorm and Hellfire), Dragon's Breath; warlock
   Shadowfury, Demonic Circle: Teleport (12+ yd gain), instant Howl of Terror, Death Coil; priest Psychic
   Scream. An AoE only with no champion inside under our breakable CC or chain-guarded, the fears and
   Dragon's Breath not on the kill target. Notes `fc.escape`.
4. **Demonic Circle** cast on the parked Champions staging stand when missing or under 60 s left.
5. **Nitro** also at 2 melee champions on the bot.
6. **Reader:** `--kill` living-healer-off-latch seconds; `--personal` escapes; `--stands` healers' first
   heal and opening moving share. Tests, docs.

## Targets (next 25H)

All three healers killed, a living healer off the latch only in spans ≤ 7 s; healers' first heal within 2 s
of the pull, moving ≤ 15% in the first 10 s; escapes cast with melee on the bot and caster deaths slower
than 3-8 s; no escape AoE breaking our Polymorph or Fear; 25N still kills.
