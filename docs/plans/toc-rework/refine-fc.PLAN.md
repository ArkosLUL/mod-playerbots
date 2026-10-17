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

## Verified facts

- **Death Grip lands on a heroic champion.** Heroic templates (`difficulty_entry_2/3`, e.g. 35706,
  35707) carry `CreatureImmunitiesId` -13: charm, effect 114 (attack me), aura 11 (taunt). The
  player's 49576 is one dummy; `spell_dk_death_grip` exempts the 28 champion entries from its boss
  check and casts 49560 (dummy, taunt aura, attack me). `DoSpellHitOnUnit` drops the two immune
  effects, so the dummy fires and the champion jumps (57604) in front of the DK; no aura is left,
  so no diminishing return either. The hit also interrupts the champion's cast (Kick's test).
  Normal templates (-12) take the taunt aura: `DIMINISHING_TAUNT`, shared with every class taunt,
  `OBEYS_TAUNT_DIMINISHING_RETURNS` on every champion, so the fifth inside the window misses whole.
  No rune cost (SpellRuneCost 683 is empty), 35 s cooldown, 30 yd.
- **Melee champions focus one victim together.** Every `RecalculateThreat` seeds each player at 10M
  x distance x health x armour mods, the same for every melee champion, so all four jump to one
  bot and stay until it dies or turns suppressed (`ThreatReference::ShouldBeSuppressed`: immune to
  their melee school, confused, or a breakable stun). B's first 70 s: 1-7 yd from the victim at
  each switch, the next one picked by health from 19-21 yd. 1.05M of melee damage in 60 s, ~0.31M
  of it crowd AoE (Arcane Explosion 114k, Bladestorm 90k, Fan of Knives 54k, Hellfire 37k). The
  kill target had 2+ other champions within 8 yd in 459/459 samples (B) and 456/456 (A).
- **Hand of Protection makes its holder suppressed** for melee champions, so they move on at once.
- **`blessing of protection on party` is dead**: the action names a spell the 3.3.5 client calls
  Hand of Protection, and `SpellIdValue` matches the DBC string exactly.
- **Hunter pet Growl is effect 63 (threat) only**, no taunt; pets keep earned threat while players
  get 10M x mod, so it never matters. It costs the pet 15 focus per cast.
- **Rebirth**: 2 s cast, 30 yd, category 26 (10 min), 68% base mana. Its target value lists the
  master first, then healers, tanks, the rest. 25H: every Rebirth fired within 1 s of a melee death.
- **Snare spells**: Hamstring 1715 and Wing Clip 2974 melee, Piercing Howl 12323 10 yd round the
  warrior, Chains of Ice 45524 20 yd (a frost rune), Concussive Shot 5116 35 yd, Frost Shock 25 yd.
  Frost Trap 13809 and Earthbind Totem 2484 sit at the caster's feet (no Trap Launcher in 3.3.5).

## Task list

1. **Grip duty** (Defence stem): `faction champions death grip` at `ACTION_RAID + 4.5`. While the
   kill target has 2+ other champions within 8 yd and Death Grip can hit it, the lowest-guid DK bot
   within 30 yd, in control, grip off cooldown, latches a stand 20 yd from the kill target, walks
   there and grips. Stand: 16 bearings, on the floor (44 yd of `ARENA_CENTER`), nearest living
   healer within 30 yd, every healer 15+ yd off, 10+ yd from every other champion; best is the one
   farthest from the nearest other champion, then nearest the healers' centre. Released on the
   cooldown, a new kill target, the pack gone, 8 s, or the kill target past 28 yd of the stand.
   `fc.grip` keyed on
   the DK. Rule `faction champions grip hold` blocks the generic movers for the holder.
2. **Snare duty**: `faction champions snare` at `ACTION_RAID + 3.2`. A melee champion whose victim
   is a healer or under 50%, not slowed, not CC'd, gets one non-healer snarer: nearest bot with a
   ready Hamstring, Piercing Howl, Chains of Ice, Concussive Shot, Wing Clip or Frost Shock in range.
3. **External duty**: `faction champions protect` at `ACTION_EMERGENCY + 4`. A member that is the
   victim of 2+ physical champions (melee and hunter, not CC'd) and holds no external gets one:
   Hand of Protection (not a tank, no Forbearance), Pain Suppression, Guardian Spirit, Hand of
   Sacrifice, in that order, healers first then lowest health, one caster each, non-healers first.
4. **Dodge toward the healers**: the avoid AoE ring search prefers the spot nearest the healers'
   centre and accepts only spots within 30 yd of the nearest other living healer whose path crosses
   no other hazard; without one, the plain nearest clear spot.
5. **Battle res duty**: `faction champions battle res` at `ACTION_RAID + 4.2`, rule `faction
   champions battle res guard` blocks the generic `rebirth` while live. Corpses with fewer than 2
   champions within 8 yd, healers first; one druid bot each, not a tank, Rebirth ready, within
   30 yd and in sight; skips a corpse another res is already casting on.
6. **Heroic taunt guard**: rule `faction champions heroic taunt guard` blocks the `Taunt` family on
   heroic while live; `faction champions pet taunt` turns a pet's taunt or threat autocast off on
   heroic, which the non-combat `toggle pet spell` turns back on.
7. **Reader**: `--pack` (per role champions within 8 yd, melee deaths in 60 s, melee to nearest
   healer, first kill, longest stall, grips) and `--peel` (snares, externals, battle res, taunts);
   synthetic-pull tests.
8. **Docs**: `faction-champions.md` decisions, Spread row's guide claim, the x1.3 heroic health,
   Known gaps.

All eight done, one implementer, no split: every task lands in the same Defence helper file.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Grip stand on a 20 yd ring, nearest living healer 15-30 yd off, every other champion 10+ yd off, the clearest bearing then nearest the healers' centre, rather than straight at the healers | brief ("toward the healers"); guide ("as far away from the rest of the Champions as possible"); traces: the kill target sat 26-34 yd off the healers' centre, so a straight pull lands the pack, which follows the raid's melee, on the healers; a stand passed in 97% of 25H pack samples, 88% on 25N | straight toward the healers' centre |
| The raid's melee follow the kill target wherever it lands | traces: the melee champions jump together to one victim picked by health and distance (1-7 yd off at each switch, a hurt bot from 19-21 yd) and follow it, so no melee stand keeps them off | hold melee short of the landing |
| Grip on every difficulty | brief names none; the grip interrupts the kill target's cast either way. Cost on 25N: the DK about 3 s out of melee and the melee about 2.5 s walking each 35 s | heroic only |
| Concussive Shot and Wing Clip for Frost Trap, Frost Shock for Earthbind Totem | DBC: Frost Trap and Earthbind sit at the caster's feet (no Trap Launcher in 3.3.5); Earthbind would take Tremor's earth slot from `RaidAntiFear` | Earthbind when the champion is within 10 yd of a shaman |
| Snare victims include humans; snarers are non-healer bots, nearest first | brief ("a healer or a bot under 50%"); a healer's GCD is a heal | bot victims only |
| Hand of Protection on any non-tank, not only melee and healers | HoP leaves its holder suppressed for melee champions, and on a caster it costs nothing | brief's melee and healers |
| External order HoP > Pain Suppression > Guardian Spirit > Hand of Sacrifice, one per member, casters non-healers first | HoP ends the hits, Pain Suppression takes 40% for 8 s, Guardian Spirit one death, Hand of Sacrifice 30% | Guardian Spirit before Pain Suppression |
| Physical champions: the ten melee entries and the two hunters; pets and CC'd champions don't count | brief ("2 or more physical champions") | count the cat and felhunter |
| Battle res as an FC duty plus a `Block` on the class `rebirth`, no change outside ToC; a corpse is safe with at most one champion within 8 yd; healers first, then guid; no bear tank shifts | brief scope 4; the class target value can't be steered from a multiplier, so a veto alone would block a dead master's res for good | change `PartyMemberToResurrect` outside ToC |
| Heroic taunt guard zeroes the whole `Taunt` family, Mocking Blow and the generic Death Grip included | brief; every heroic taunt target is a champion, pet, totem or treant, none tauntable to any purpose | aim-checked multiplier |
| Pet taunt off on heroic only | brief; pet Growl is threat only (effect 63) and useless on every difficulty, but normal kills stay untouched | every difficulty |
| A dodge with no spot within 30 yd of a healer takes the nearest clear spot | escaping the Bladestorm beats heal range; the brief's "never more than 30 yd" would leave a bot already far out standing in it | no dodge |
| Grip stand not floor-checked, bound to 44 yd of `ARENA_CENTER` | the arena floor is a gameobject navprobe can't read; the Beasts spots are known to reach 44 yd | `ValidateFloorPoint` from the DK |
| Only `fc.grip` probed | the other duties are instant casts already in `cast`, like the dispel and purge duties | per-bot snare, external and res keys |
| The grip walk reuses `NorthrendBeastsWalkAction` from the Beasts seam header, included unchanged | it already holds a latched walk in flight | a second copy in the FC stem |

## Carried over

- w6: hoist `NorthrendBeastsWalkAction` (`Action/ToCActions_NorthrendBeasts.h`) into Shared, now
  that FC's grip derives from it.
- Outside ToC: `CastBlessingOfProtectionProtectAction` (`PaladinActions.h:382`) names "blessing of
  protection", which `SpellIdValue` never resolves on 3.3.5, so the class Hand of Protection never
  casts anywhere.
- Outside ToC: the class Rebirth target (`PartyMemberToResurrect`) takes the master first and any
  corpse, however many enemies stand on it.

## Known gaps

In `faction-champions.md` Known gaps: the grip holds the pack off only until it follows the melee,
humans hold no grip duty, chained grips, kiting, the externals' cooldowns against a new focus
victim every 1-2 s, a melee under HoP deals no physical damage, `grip` keeps its last value after a
kill, `--pack` stalls need 10+ bots on the kill target. The 25H targets (melee deaths in 60 s under
9, champions within 8 yd of a melee under 4.2, first kill before 54 s, no 20 s stall, 25N kill time
no worse) wait on the next pulls; `--pack` and `--peel` read each.

## Blocked

None.
