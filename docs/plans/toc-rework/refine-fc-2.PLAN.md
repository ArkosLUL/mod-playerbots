# refine-fc-2: the death ball

Wave 4g, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4f `993dc208a`.

## Why

25H, 2026-10-18, `env/dist/logs/botobs/649_2_caiphus-the-stern_1792348889.ndjson`: wipe at 2:10, 31
deaths, one kill (Holy Paladin, 56 s).

- Every other champion stayed within 40 yd of the kill target; another champion healer 5.5-16 yd
  off. The grip moved it 17 yd and the pack followed. Disc Priest went 93% → 36% → 73%.
- A human DK gripped the Resto Druid out at 0.6 s; bots kept on the Holy Paladin in the pack. The
  bot tank hit the kill target all fight; champions targeted tanks 18% in the first 10 s, then 0%.
- Ranged walked from 70 yd to 17-24 yd of the pack. Champions then took healers (63% of targets at
  0:30-0:40) and ranged (44-79% at 0:40-1:10).

User decisions: Warcraft Tavern's death ball on every difficulty; the kill target is the healer
pulled out of the pack, by a bot or a human DK; kill order only picks which healer the bot DK grips.

## Core threat (`boss_faction_champions.cpp`)

Zone combat seeds every player at 0 threat; `RecalculateThreat` (engage, +2 s, then every ~9 s)
re-seeds only players with nonzero threat on that champion (damage on it, or any heal). Score
`mod_dist × mod_health × mod_armor`: health 2.0 above 40k, up to 6.0; melee distance `15/(10+d)`
past 5 yd and armour `1 − A/(A+16635)`; casters `25/(d−10)` past 35 yd, no armour. A tank scores
~0.72 (melee) and 2.0 (casters); a melee champion leaves him for plate DPS within ~22 yd,
leather/mail ~37, plate healer ~31, cloth ~57; casters for almost anyone within 50-57.

## Scope

1. **Geometry** (offence helper): P = landing centroid (`GetFactionChampionsLandingCentre`); pack
   centre = non-kill champions within 25 yd of P, else all; pulled healer = 20+ yd from the pack
   centre, no champion within 12; escapee = non-kill champion past 20 yd of P whose victim isn't a
   tank.
2. **Kill latch**: skull first; switch off an unpulled kill target above 30% to the pulled healer
   farthest from the pack (`pulled`); `first`/`dead` prefer pulled healers; a pulled one is kept.
3. **Grip chain**: duty while the kill target is under 45 yd from the pack centre or a champion
   healer is within 40 yd of it; and on the next in-pack healer once the kill target is pulled and
   at 20% or immune. Stand 27 yd off on the floor bearing farthest from the pack. Pack tanks never
   grip. Non-tank melee hold off a not-yet-pulled kill target while the duty holds.
4. **Pack tank** (stem `FactionChampionsBall`): bot tanks, staged as forced duties at P, hold P
   (back past 8 yd), attack the nearest non-kill champion within 10 yd, never focus. Dodge without
   the healer pull. Probe `fc.packtank`.
5. **Stands**: up to two tank healers by armour, 34 yd from P toward the kill target; other healers
   then ranged on slots 20/26 yd beyond the kill target, ±75° in 15° steps, farthest from the pack,
   off hazards. Reach, follow, formation and heal reach blocked on a stand.
6. **Escapees**: snare duty takes melee escapees; CC pool lists escapees first.
7. **Opener** (heroic): release +2.5-8 s, one non-tank, non-healer bot with Frost Nova > Psychic
   Scream > Intimidating Shout > Hungering Cold > instant Howl of Terror walks into the pack and
   casts to burn PvP trinkets. Probe `fc.opener`.
8. **Physical switch** only onto a pulled champion.
9. **Reader** `--ball`, tests, doc.

## Targets for the next 25H pull

Tank share of champion targets ≥ 50% for 30 s; kill target clear of other champion healers (40 yd)
in ≥ 60% of its hold after the first grip; ranged median ≥ 50 yd from the pack; no ranged or healer
death in 60 s; first kill under 40 s; 25N still kills.
