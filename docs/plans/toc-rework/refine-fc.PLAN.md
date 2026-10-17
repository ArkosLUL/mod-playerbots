# refine-fc — Faction Champions 25H: out of the pack, peel, dodge to the healers

Wave 4f, a refine lane on the shipped FC lanes (`w3a-fc-offence`, `w3b-fc-defence`,
`w3b-fc-defence-rest`). Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Mechanics and the shipped design:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Line numbers are as
of `2f3470959`.

Traces (25H, both wipes, build 2026-10-17 15:37 UTC carrying `2f3470959`, humans Dragon (tank) and
Deathsong (DK); roster 4 heal, 9 melee, 10 ranged, 2 tank), in
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/`:

- `649_2_thrakgar_1792260282.ndjson` (A): wipe at 1:54, no champion killed;
- `649_2_caiphus-the-stern_1792260601.ndjson` (B): wipe at 2:06, Holy Paladin dead at ~54 s.

25N baselines, same raid, both kills: `649_3_thrakgar_1792176980` (4:05, 5 deaths),
`649_6_caiphus-the-stern_1792186694` (4:40, 6 deaths). Read with `bosses/faction_champions.py <f>`
(`--kill --cc --heals --aoe --hop --adds`) and `postmortem.py <f>` (`--threat`).

## Findings from the 2026-10-17 25H pulls

1. **Melee die inside the champion pack.** B's first 60 s: 1.05M of the raid's 1.31M damage taken
   on melee, 9 melee deaths; A: 7 by 0:31. Each melee had a mean 4.2 (A) / 4.9 (B) champions within
   8 yd, 4.1 on the 25N kill: the pack isn't new, heroic damage makes it lethal. Melee champions
   spent ~50% of their samples on our melee. Melee DPS bots dealt little (Assasin 151k in 114 s).
2. **The kill target stalls as the raid shrinks.** A: Holy Paladin 100 → 17% in 50 s with 15-20
   bots on her, then 15-23% for ~50 s while attackers fell 14 → 3. B: Disc Priest 94 → 36% by 90 s,
   then flat. Implied champion healing on the kill target ~50k/s. Champion health is 4.64M
   (non-cloth) / 3.71M (cloth): the core's 3.57M / 2.86M × 1.3 from `mod-dungeon-scale`
   (`AC_DUNGEON_SCALE_STAT_MODIFIER_RAID_HEROIC_BOSS_HEALTH`), every champion being boss-flagged.
3. **The AoE dodge carries melee out of heal range.** In A, 4 of the first 7 deaths were mid-walk on
   `faction champions avoid aoe`, nearest healer 40-47 yd off. The dodge takes the nearest clear
   spot (`ToCActions_FactionChampionsDefence.cpp:115`), unbiased toward the healers; melee champions
   (8 yd/s) follow anyway. Outside the dodge, melee stood a median 25-26 yd from a healer.
4. **Battle res revives melee into the pack.** B: all three Rebirths on melee, each dead again in
   17-25 s; A: Assasin revived at 24.0 s, dead at 27.5 s. One Rebirth of five went to a healer.
5. **Healing below damage taken**: B's first 60 s, 21.8k/s taken against 18.1k/s effective healing.
6. **No peel.** A+B: 1 Hamstring, 0 Frost Trap, 0 Chains of Ice, 0 Piercing Howl, 0 Earthbind; 1
   Pain Suppression and 0 Guardian Spirit per pull, 2-3 Hand of Sacrifice, no raid Hand of
   Protection.
7. **Taunts on taunt-immune heroic champions**: A had 13 Righteous Defense from Bulwark, 9 Hand of
   Reckoning from the human, 46 hunter-pet Growl.

Worked, keep: Heroism at +3 s with burst at the pull; Mass Dispel took Divine Shield 0.5 s in; all
38 purges on the kill target; CC on the raid dispelled at a 0.1-0.5 s median; totems dead in
1-4.5 s; ~60% of champion cast heals followed by an interrupt; Cyclone 8/8 landed.

**Guides** (warcraft.wiki.gg, Almar's, TenTonHammer): don't clump, avoid the middle of the
champions. Warcraft Tavern's death ball has a DK Death Grip one healer away from the pack while
ranged stay back: it separates the kill target, so `faction-champions.md`'s Spread row citing it
for the melee stack misreads it. Guides peel by kiting and snaring melee champions (Frost Trap,
Hamstring, Chains of Ice, Chilblains).

## Decided by the user

- Kill order stays: Holy Paladin first, `KILL_ORDER` (`ToCHelpers_FactionChampions.cpp:40`)
  untouched, though 25N data has Disc first dying faster (25.7-27.3 s against 36.6 s).
- The ×1.3 health stays: the bots must beat it.
- Composition stays (4 healers).

## Scope

1. **Pull the kill target out of the pack** (ranked first). While 2 or more other champions stand
   within 8 yd of the kill target, one DK bot Death Grips it toward the healers. Verify first on the
   core that the grip's pull lands on a heroic champion despite taunt immunity (`CreatureImmunitiesId`
   -13) and which DR category it shares; if it doesn't land, log the gap and stop this item. Decide
   from the traces where the raid's melee then stand so the pack follows as little as possible.
2. **Peel and external defensives**, in the `FactionChampionsDefence` stem:
   - snare a melee champion whose victim is a healer or a bot under 50% health: Frost Trap,
     Hamstring or Piercing Howl, Chains of Ice, Earthbind Totem; one snarer per champion;
   - Hand of Protection on a raid melee or healer hit by 2 or more physical champions (not a tank);
     Pain Suppression, Guardian Spirit and Hand of Sacrifice on a bot under focus. Check each class
     action exists before wiring.
3. **Dodge toward the healers**: the avoid AoE spot is the clear one nearest the healers' centre,
   never more than 30 yd from the nearest living healer.
4. **Battle res**: Rebirth takes a healer first and skips a corpse with 2 or more champions within
   8 yd. The action is generic (`CastRebirthAction`, `DruidActions.h:82`, nodes at
   `GenericDruidStrategy.cpp:68-76`): gate it from the FC stem with a rule or multiplier if the
   target pick allows, else change it outside ToC and list that in the report.
5. **Heroic taunt veto**: on heroic, a multiplier zeroes class taunt nodes and pet Growl aimed at a
   champion.
6. **Reader**: `faction_champions.py --pack`: per role, mean champions within 8 yd; melee deaths in
   the first 60 s; median melee-to-nearest-healer distance; time-to-first-kill; the longest kill
   target stall (health change under 3% over 20 s while 10+ bots attack it).
7. **Docs**: fix the Spread row's guide claim; add the ×1.3 heroic health; move what ships out of
   Known gaps.

Targets on the next 25H pulls: melee deaths in 60 s under B's 9, champions within 8 yd of a melee
under 4.2, first kill before 54 s, no 20 s stall; 25N kill time no worse.

## Owns

- the `FactionChampions` and `FactionChampionsDefence` stems and `ToCDefinition_FactionChampions.cpp`;
- `faction-champions.md`;
- `faction_champions.py` and its test.

Outside ToC only for scope 4 and only if the FC stem can't gate it, listed in the report.
