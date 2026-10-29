# Valithria Dreamwalker

Boss id 10, trace slug `valithria-dreamwalker`, prefix `vd.` ([README](README.md)). Code: stem `Valithria`
(`src/Ai/Raid/ICC/*/ICC*_Valithria.*`); nodes `icc valithria` dodge, kite, dream, portal, taunt, snare,
void, target, heal; multiplier `icc valithria splash`. Core: `boss_valithria_dreamwalker.cpp`.

## Mechanics

Ids are 10N/25N/10H/25H where they differ; code remaps a difficulty row through
`GetSpellIdForDifficulty`. Every area is a creature's on players: centre to centre.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Win and loss | Friendly, at (4203.65, 2483.89), guid slot 10; starts at 50%, healed to 100% wins (credit spell 72706); at 0 the combat trigger 38752 (slot 35) sets `NOT_STARTED` and despawns the room 5 s later. `IN_PROGRESS` once a player pulls one of the archmages channelling on her. Immune to percent heals. Heroic's 7 min berserk is a yell only | Heal her; 5-10 healers in 25, 3-5 in 10, 1 tank | Bot healers heal her by name (`icc valithria heal`) |
| Adds | The Lich King controller summons at the doors, north corners on 10-man (x 4241), all four on 25-man, on decaying timers (floor 5 s), first at: Gluttonous Abomination 37886 5 s, Suppresser 37863 10 s (3 at each of 2 doors), Blistering Zombie 37934 15 s, Risen Archmage 37868 20 s, Blazing Skeleton 36791 30 s. Each attacks a random player within 200 yd; suppressers go for her. None obeys taunt DR | Skeletons and archmages, then worms and suppressers, then zombies and abominations | Resolver by tier, nearest her (`vd.target`) |
| Lay Waste 69325/71730 | Skeleton, 15-20 s, every 2 s for 12 s 69326 (row 2095) on the raid and her | Kill skeletons first | Tier 1; fire resistance aura |
| Frostbolt Volley 70759 (row 2112) | Archmage, 2 s cast, 40 yd, slows 50% for 8 s, magic | Interrupt | Tier 1; class interrupts on the target |
| Column of Frost 70704/71747 | Archmage, on a random player past 10 yd: a marker 37918 that 2 s later casts 70702 (row 2101) 11-20k and a knock-up within 3 yd | Move out | Forced dodge to 5.5 yd while the marker is under 2.5 s old (`vd.dodge column`) |
| Mana Void 71179/71741 | Archmage, on a mana user past 15 yd: a void 38068 burning mana within 6 yd every 0.5 s (71086, row 2100) for 36 s | Move out | Mana users step to 8.5 yd, finishing their cast first (`vd.void`) |
| Suppression 70588 | Each suppresser channelling on her: -10% healing taken | Burn them, they have no threat | Tier 2 |
| Acid Burst 70744 (row 2096) | A zombie at 1 health roots and casts it, 0.75 s: 9-20k within 15 yd plus a 20 s DoT, then turns unselectable 2 s before despawning | Nobody near a dying zombie | Everyone but its victim dodges forced to 17.5 yd from under 15% until it bursts (`vd.dodge burst`); kite legs keep out |
| Zombie melee, Corrosion 70751 | Huge swings stacking -10% armour, 5 stacks; runs 5 yd/s against a player's 7 | Kite, snare with Chains of Ice, ranged only | Victim kites the 25 yd ring (`vd.kite`); one snare per zombie (`vd.snare`); melee never target one; area damage held within 10 yd of one |
| Gut Spray 70633 (row 2002) | Abomination, 10-13 s, 8 yd cone at its victim: nature DoT, +25% physical taken, disease | One tank, faced away; cleanse | Tank roster (`vd.tank`); generic melee stand behind; class cleanses |
| Rot Worms 37907 | An abomination's death spawns them on random players: Flesh Rot 72963 stacks | AoE them, off the healers | Tier 2; tanks taunt them off healers |
| Portals | Every 45-48 s, first at 45-48 s: 3 (10-man) or 8 (25-man) pre-effects 38186 (heroic 38429) 20-30 yd from her, on her north half on 10-man; their visual 71304 (71986) shows 15 s, then a real portal 37945 (38430) opens on the spot for 10 s and the pre-effect lingers 6 s | 1 healer outside on 10-man, 1-2 on 25-man | Bot healer split; portal healers claim one each (`vd.healer`, `vd.portalclaim`) |
| Dream State 70766 | The portal's spell click: 20 s, phase 16, flight at +100%; phased from her, the adds and everyone outside. Dream Clouds 37985 (heroic Nightmare 38421) hover 17-24 yd up, 25-48 yd round her, wandering 10 yd, back 10 s (25 s) after use; a player within 5 yd bursts one 0.5-1.25 s later | Pop the nearest at once, then fly together; fly low before the end | Flight mover (`vd.dream`, `vd.cloud`) |
| Emerald Vigor 70873, heroic Twisted Nightmares 71941 (DB-only row) | On everyone within 10 yd of a burst cloud: +10% healing and damage, 200 mana every 3 s, 40 s, up to 100 stacks; heroic adds 71940, 200 nature a stack a second | Refresh before stacks lapse; heroic: heal each other, mind the stacks | A heroic healer skips portals at 25 stacks |
| Fall | Leaving Dream State in the air drops the player | Fly low | Descend 3.5 s before the end; a bot still airborne falls with real damage |

## Layout

navprobe `--nav 0x09` settles every point within 30 yd of her at 364.86-364.88, 11 rings of up to 48
headings, each in sight of her; 427 sight lines from the 25 yd ring to every cloud spawn and between
every pair of clouds are clear. Every derived stand stays in that disc: dodge spots, the kite ring at
25 yd (24 points), the heal walk to 25 yd, the landing point (the bot's bearing from her, at most 25
yd out, 1 yd above her).

## Duties

- **Live** while her state is `IN_PROGRESS` and the bot is within 100 yd of her.
- **Healer split** (`vd.healer`): bot healers only, non-paladins first, by guid; one raid healer on
  10-man, two on 25-man, never the last bot healer. The rest take portals.
- **Portal healers**: each claims the nearest unclaimed pre-effect, walks to it and parks within 1 yd
  (back past 2.5); a Mana Void on the spot waits for the portal to open, an open portal is worth a
  dropped cast. It clicks the real portal within 5 yd. A claim ends in Dream State or when nothing
  stands on the spot; a kited healer claims none, and one holding a claim kites.
- **Healing her**: portal healers unless someone within 40 yd is under 40%, raid healers while
  everyone within 40 yd is above 85%; inside 36 yd and in sight, else walk to 25 yd from her. By
  class, renewing a heal over time with 2 s left: druid Lifebloom (3), Rejuvenation, Regrowth, Wild
  Growth, Nourish, Healing Touch; priest Renew, Penance, Greater Heal, Flash Heal; shaman Riptide,
  Healing Wave, Lesser Healing Wave; paladin Holy Shock, Holy Light, Flash of Light. Earth Shield,
  Beacon of Light and Sacred Shield stay on the tank.
- **Dream flight**: straight 3D splines with the fly flags set by hand (no client answers the aura,
  and the generic movers fly only with a flying master). First the cloud nearest the bot until a
  stack is newer than the dream; then the group of refreshed bot dreamers flies onto its lowest-guid
  leader when over 6 yd off, else to the shared cloud nearest the leader, left once the leader has
  waited 1.3 s within 3 yd. A player in Dream State leads instead. Descent in the last 3.5 s; once
  out, a bot still flagged drops through `MoveFall` and `HandleFall`. `PlayerbotAI::CastSpell`
  exempts Dream State 70766 from its in-flight refusal, as it does Gravity Lapse, so heroic dreamers
  heal each other.
- **Targets**: tanks and damage dealers; a player's skull, a tank's own abomination, then the tiers,
  nearest her within 50 yd, the current pick kept inside its tier. Zombies are ranged only; damage
  dealers take an abomination only once a tank holds it.
- **Tanks** (`vd.tank`): each abomination to the nearest free bot tank. A tank taunts its own back,
  else a worm off a healer, else, holding none, a zombie off a healer; generic taunts are blocked.
- **Kite**: a bot a zombie chases within 45 yd, unless it is a tank holding its abomination. It drops
  its cast, uses Nitro Boosts and walks forced along the ring, two points (30°) at a time away from
  the zombie, else three, one, or back the other way; legs stay 6 yd off the zombie and clear of
  live columns and other zombies' bursts. It does nothing else meanwhile.
- **Snare**: per chasing unslowed zombie, the nearest non-healer bot with Chains of Ice (20 yd),
  Concussive Shot (30) or Frost Shock (20) ready and in sight.

## Decisions

- **Kill order**: Warcraft Tavern 25's tiers (Wowhead ranks suppressers second); nearest her within a
  tier, so every bot agrees.
- **Melee and zombies**: never, per Wowhead's one-shot warning; area damage near one is held too,
  since its threat would turn the zombie.
- **Raid healers** 1 and 2 (Warcraft Tavern 10/25); paladins last for it, Beacon and Holy Light being
  the guides' healer on her; they heal her too while the raid is topped (instant-carry).
- **Dream group**: Wowhead's refresh-then-gather; the lowest guid leads so every bot agrees.
- **Fall**: a bot still airborne takes the fall a player would, not a snap.
- **Ring kite**: on a ring a pursuer settles inside it and behind, about 17 yd at these speeds, and
  the kiter stays in heal range; a tank holding an abomination face-tanks rather than drag it.
- **Snares** are ranged only: `IccTryClassCC`'s melee stuns and fears put bots in reach or scatter adds.
- **No lust hook**: the base gate already holds lust here (Known gaps); a per-boss window can only hold.
- **Resist hook**: fire, frost and nature.

## Cheats

Removed:

- `vd.dreamteleport` and `vd.cloudteleport`, same-map teleports onto a human dreamer and the shared
  cloud: replaced by the flight mover.
- The floor snaps (`NearTeleportTo` to z 365 above z 367.961): replaced by the descent and a real fall.
- `vd.nitro` and `vd.speed` earlier: the kite uses the bot's own boots; Dream State's speed is the aura's.

## Known gaps

- Lust and healer cooldowns: the base burst gate holds lust off non-boss adds and a victimless
  friendly boss, and no node puts Guardian Spirit on her.
- The kite has no answer to Frostbolt Volley's slow on the kiter.
- A dreamer at the win keeps its fly flags until its next move glides it down, no fall: the gate
  closes at `DONE` before Dream State ends.
- No tank defensive ladder: no boss hits a bot tank.
- No opening positions: the archmages start the fight wherever the raid pulls them.
- Humans hold no duty; a bot may claim the portal a human takes, and claims again if one is left.
- No ICC trace yet carries these probes, so nothing here is measured.

## Targets for the next pulls

25N first: `--heal` shows two raid healers and every other bot healer on `portal`; `--waves` has a
claim per portal healer and each of them in Dream State inside its wave; `--dream` shows `refresh`
then `cloud` or `gather`, `descend` before every window ends and no death inside one; `--adds` has
skeletons and archmages first for every damage dealer and `owned` on each abomination's tank;
`--zombies` shows kites ending without a death to Acid Burst or a zombie swing, and a snare per zombie.
25H: the same, plus no portal healer past 25 Twisted Nightmares.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `vd.wave` | per bot healer | `pre` while pre-effects show their visual, `open` while a real portal stands, `none` |
| `vd.healer` | traced map, healer → duty | `1` raid, `2` portal, `0` released |
| `vd.portalclaim` | traced map, healer → pre-effect | The claimed portal; `0` once it lapses |
| `vd.dream` | per bot | The flight rule: `refresh`, `gather`, `cloud`, `human`, `hover`, `descend`, `land`, `none` |
| `vd.cloud` | the bot that moves the shared cloud | `cloud` and the unit, `none` |
| `vd.target` | per tank and damage dealer | `skull`, `owned`, the tier, `none` |
| `vd.tank` | traced map, tank → abomination | The one it holds; `0` once it lapses |
| `vd.kite` | per bot | `ring ccw`, `ring cw`, `reverse`, `blocked`, `none` |
| `vd.snare` | per non-healer bot | The spell, `none` |
| `vd.dodge` | per bot | `column`, `burst`, `blocked`, `none` |
| `vd.void` | per mana user | `step`, `blocked`, `none` |

Dream State itself is the `aura` stream (70766), so no probe repeats it; each Column of Frost is a
3 yd hazard circle for 2.5 s. Notes are change-only per bot.

`tools/botobs/bosses/valithria_dreamwalker.py`; its banner names declared keys the pull never wrote:

- `--waves`: per wave, pre and open times, the claiming healers and who entered Dream State;
- `--dream`: each Dream State window, `vd.dream` per bot and the `vd.cloud` counts;
- `--heal`: `vd.healer` per healer;
- `--adds`: `vd.target` per bot and `vd.tank` per tank;
- `--zombies`: kites, snares, dodges and void steps per bot;
- `--cheats`: uses in the pull, none of them hers.
