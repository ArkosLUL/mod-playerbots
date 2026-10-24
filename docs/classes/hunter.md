# Hunter — Beast Mastery, Marksmanship, Survival

Engine semantics are in [../engine/action-selection.md](../engine/action-selection.md).

## Current ladders

Shared (`GenericHunterStrategy`):

| Rel | Trigger | Action |
|---|---|---|
| 61 | tranquilizing shot enrage / magic | tranquilizing shot |
| 37 | enemy within melee | explosive trap *(see gap D2)* |
| 35 | low health / medium threat / enemy too close | deterrence / feign death / disengage *(D4)* |
| 34 | enemy too close for auto shot | flee |
| 30 / 29.5 | no ammo / hunter's mark | equip upgrades / hunter's mark |
| 29 | rapid fire off cooldown | rapid fire *(burst-gated)* |
| 28 | aspect of the viper | aspect of the viper |
| 27 | low tank threat / md on main tank and light aoe | misdirection on main tank |
| 22 / 21 | pet medium / low health, enemy within melee | mend pet, mongoose bite / wing clip |
| 20 | has aggro / concussive shot on snare target | concussive shot *(D1)* |
| 17 | trap launcher: explosive trap no cd | trap launcher: explosive trap |
| 16.5 | pet acid spit | pet acid spit |
| 5.25 / 5.24 | no pet / hunters pet dead | call pet / revive pet |

Per spec, on top:

| Rel | Beast Mastery | Marksmanship | Survival |
|---|---|---|---|
| 40 | intimidation *(not on stun-immune)* | silencing shot *(interrupt)* | — |
| 28.9 | — | readiness *(burst-gated)* | — |
| 28.5 | bestial wrath *(burst-gated)* | — | — |
| 28 / 27.9 | — | — | lock and load → explosive shot rank 4 / proc → rank 3 |
| 18.5 / 18 | kill command / kill shot | kill command / kill shot (18.4: silencing shot on immune target) | kill command / kill shot |
| 17.5 | low mana → viper sting | low mana → viper sting | explosive shot |
| 17.1 / 17 / 16.9 | no stings → serpent sting (17) | no stings → serpent sting / chimera shot | — |
| 16.5 | serpent sting on attacker | — | black arrow |
| 16 | — | serpent sting on attacker | low mana → viper sting |
| 15.5 / 15 | — | aimed shot | no stings → serpent sting, serpent sting on attacker |
| 5.9 → 5.1 | *defaults* | *defaults* | *defaults* |

Survival's default list has no `arcane shot` (unreachable there). MM and Survival match the sim's
order for Explosive Trap (shares Black Arrow's 30 s category, so Black Arrow is never cast in either).

## Decisions and traps

- **Arcane Shot** (`CastArcaneShotAction::isUseful`) is skipped once Explosive Shot is known
  (Survival's slot) and above 435 armor-penetration rating, where the physical fillers out-damage it.
  Read the rating as `PLAYER_FIELD_COMBAT_RATING_1 + CR_ARMOR_PENETRATION`: `CR_ARMOR_PENETRATION`
  (24) is an index into the rating block, and update field 24 is `UNIT_FIELD_HEALTH`.
- **Aspect of the Viper** enters below `HUNTER_VIPER_ENTER_MANA_PCT` 30 and leaves at
  `HUNTER_VIPER_LEAVE_MANA_PCT` 60, kept together in `HunterTriggers.h` because entry and exit sit in
  different triggers. The old `lowMana / 2` entry (integer 7%) could not sustain; with the band, an
  XT-002 pull held mean 53.6% mana, 72.5% of the fight inside it, never below 15%. Hunters skip mana
  potions so the one potion stays free for Potion of Speed
  ([consumables](../systems/consumables-and-burst.md)).
- **`HunterAspectOfTheViperTrigger` hard-returns false while `rnature` is set**, so never set
  `rnature` loosely ([../raids/README.md](../raids/README.md)). Thorim's nature resistance casts
  Aspect of the Wild directly under `BossNatureAspectHoldMultiplier` for one designated hunter,
  self-reverting ([../raids/ulduar/thorim.md](../raids/ulduar/thorim.md)).
- **Rapid Fire and Bestial Wrath** are `SpellNoCooldownTrigger`s placed by `BurstWindowStrategy`. As a
  `BoostTrigger` (`balance <= 50`) Rapid Fire went unspent for whole encounters; Bestial Wrath's
  trigger was never registered.
- **Call and revive pet sit in the combat set**: `HunterPetStrategy` is non-combat only, and a pet lost
  mid-fight takes Kill Command and Bestial Wrath with it (both resolve `"pet target"`), ~35-40% of BM's
  throughput. At 5.25/5.24 they sit under every triggered shot and above the Steady/Auto Shot filler.
- **`maintenance` keeps an unslotted pet.** A pet whose corpse despawned or fell out of range sits in
  `UnslottedPets`, not `CurrentPet`; `InitPet` once tamed a random creature over it, and
  `SavePetToDB(PET_SAVE_AS_CURRENT)` deleted the old one. `pet dead` is true whenever no pet is out
  but a `character_pet` row exists, so the bot revives or calls it.
- **Pets above 20 talent points**: premade links (`Playerbots.PremadeHunterPetLink.<spec>.<points>`)
  stop at 20 (`ParsedHunterPetLinkOrder[3][21]`), and mod-spell-tweaks here raises Beast Mastery
  (53270) from +4 to +6 pet points, a 22-point cap at 80. `InitPetTalents` applies the 20-point link
  and spends the rest on random picks, which only reach the last row once 20 are spent.
- **A new pet's passives get the owner's pet talents.** `CreateTamedPetFrom` casts them before
  `SetMinion` gives the pet an owner, so Ferocity, Unleashed Fury, Serpent's Swiftness, Bestial
  Discipline, Endurance Training and Thick Hide stay off until the next DB load (test Wolf: crit 18%
  against the sim's 29%). `PlayerbotFactory::InitPet`, which `tame` also runs, recalculates them; core
  Tame Beast still has it for players.
- **Kill Command is a plain `SpellNoCooldownTrigger`**: its aura lands on the hunter, while the action
  targets the pet, so an aura check never worked; the cooldown paces it.
- **Chimera Shot and Aimed Shot have triggers**: as `5.5` / `5.4` defaults, `no stings` at 17.0
  hard-recast Serpent Sting instead of Chimera rolling it forward. The sting still goes first when
  none is up: Chimera on an unstung target refreshes nothing and loses its Serpent hit.
- **Intimidation skips template stun immunity**: its value is the pet's stun (24394), and every raid
  boss's `creature_immunities` row carries stun and silence.
- **Silencing Shot on a silence-immune target** is free off-GCD damage, so `silencing shot on immune
  target` fires it on cooldown there; the interrupt node is untouched elsewhere.
- **Readiness** fires with Rapid Fire's buff down and over 10 s of its cooldown left (the sim's rule):
  as only `rapid fire`'s alternative it never fired, since that trigger needs Rapid Fire ready.
- **Lock and Load**: with rank 4's dot still ticking, the rank 4 action is USELESS, not IMPOSSIBLE, so a
  rank 3 alternative never runs; `lock and load proc` (buff 56453) pushes rank 3 itself. Trap:
  `lock and load` matches the talent passive's name (56344), so it is true all fight and is what drives
  rank 4 Explosive Shot at 28.
- **Pet Acid Spit**: the pet pays the focus. A direct `pet->CastSpell` registers no creature cooldown
  ([why](../engine/raid-mechanics-lessons.md#vehicles-and-flight)), so the action stamps the spell's own
  cooldown on the pet, or the trigger refires every tick.
- **Items in combat pass `skipAutorepeat`**: Auto Shot holds `CURRENT_AUTOREPEAT_SPELL` all fight, so
  item, trinket and tinker use refusing on `IsNonMeleeSpellCast(...)` meant no Potion of Speed,
  Hyperspeed, on-use trinket or healthstone.
- **Pet taunts stay off in groups**: `TogglePetSpellAutoCastAction` leaves Growl, Torment and Suffering
  off while the group has a main tank other than the bot. Autocast Growl pulled the boss and burned
  focus (36-48 per 180 s; Bite fell to 94 against the sim's 116).
- **Viper band 30/60 against the sim's 10/30** costs short fights ~1 extra Viper stint per 180 s,
  roughly 1-3% DPS, shrinking with fight length. Kept for mana-starved encounters.

## Open gaps

| # | Gap |
|---|---|
| D1 | **Concussive Shot on aggro wastes a GCD on bosses.** `has aggro` → concussive shot @20; bosses are snare-immune, so the cast burns a GCD every tick the hunter holds aggro. Fix is gating the trigger to non-boss targets. |
| D2 | **Melee weave outranks the whole shot list.** `enemy within melee` pushes explosive trap @37, mongoose bite @22, wing clip @21. At 37 the trap beats everything except dispels, and melee weaving is a straight DPS loss for a ranged spec in WotLK. |
| D4 | **Disengage is raid-hostile.** Fixed-vector backwards leap at @35; on Thorim / Mimiron / Gunship-style encounters it lands bots in fire, off platforms, or out of healer range. Should be suppressed in raid instances in favour of the existing `flee` alternative. |
| D5 | `trap launcher: explosive trap` @17 sits mid-rotation, between Survival's explosive shot (17.5) and black arrow (16.5). Left alone because Survival's trap usage is what the user wanted preserved. |
| D6 | `ExplosiveShotTrigger` overrides `DebuffTrigger` to `BuffTrigger::IsActive()`, so it re-pushes on debuff-absence while the 6s cooldown still has the spell unavailable. Harmless — the action is IMPOSSIBLE — but it costs a queue iteration. |
| D7 | `freezing trap on cc` has no action creator ([pitfalls](../engine/pitfalls.md#names-fail-silently-at-runtime)): hunter bots never trap a CC target. |

Against the wowtbc.gg guides, also open: BM and Survival default lists both carry `aimed shot`, a
Marksmanship talent neither spec has (dead entries); neither BM nor Survival has Multi-Shot in its
single-target priority (only `AoEHunterStrategy` on `light aoe`); Survival's Sniper Training (6s
stationary) is not modelled.

## Confirmed correct — do not re-audit

`medium threat` → feign death @35 and `low health` → deterrence @35 keep their current relevance and
triggers by explicit decision. Aspect of the Dragonhawk via `bdps`, Hunter's Mark, Kill Shot at 20%,
Chimera Shot refreshing a live Serpent Sting instead of a hard recast, trap-launcher Explosive Trap,
Misdirection on the main tank, and Volley/Multi-Shot on AoE are all correct as written. Misdirection
fires on cooldown in the bot test only because the test's tank deals no threat (`low tank threat`).
