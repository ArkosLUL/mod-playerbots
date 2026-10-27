# Icecrown Citadel (map 631)

Strategy key `icc`, one strategy for every encounter. Cross-raid conventions are in
[../README.md](../README.md). Mechanics source: `src/server/scripts/Northrend/IcecrownCitadel/`.
The code is upstream's: keep tactic changes inside the existing files, since splitting them conflicts
with every upstream merge.

## Code layout and registration

`src/Ai/Raid/ICC/`:

- `ICCStrategy.cpp`: 51 trigger nodes and 13 multipliers. ICC has **no encounter gate**: each
  trigger tests its boss itself, mostly through `"find target"` (the bot's threat list), Sindragosa's
  through a 200 yd `FindNearestCreature`, so every encounter's nodes are asked in every fight.
- `ICCTriggers.{h,cpp}`, `ICCMultipliers.{h,cpp}`, `ICCTriggerContext.h`, `ICCActionContext.h`.
- `Action/ICCActions.h` plus `Action/ICCActions_<stem>.cpp`: `LM`, `LDW`, `GSB`, `DBS`, `Dogs`, `FG`,
  `RF`, `PP`, `BPC`, `BQL`, `SS`, `VT`, `SG`, `LK`.
- `ICCShared.{h,cpp}`: cross-boss helpers (class taunt and CC, raid icons, heroic buffs, the cheat
  helpers).
- `ICCScripts.{h,cpp}`: per-instance state, the spell listeners, the state reset and the RaidObs name
  table, registered by `AddSC_IcecrownBotScripts` from `src/Script/Playerbots.cpp`.

The strategy is registered in `RaidStrategyContext.h`, `BuildSharedActionContexts.cpp`,
`BuildSharedTriggerContexts.cpp`, and `PlayerbotAI.cpp` (map 631 → `icc`, and the instance-strategy
name list).

**Triggers act inside `IsActive`.** Several strip Experienced, apply the heroic buffs or run a cheat
there, so the side effect runs whenever the node is asked, whatever wins the tick.

## Boss states and trace names

`instance_icecrown_citadel.cpp` calls `SetBossNumber(15)`; the ids are the core's `DATA_*` order in
`icecrown_citadel.h`. `AddSC_IcecrownBotScripts` registers each id's slug (`RaidObs::NameBossStates`).
A trace opened while that boss is `IN_PROGRESS` takes the slug outright, whether the boss state opens
it or an engage does: the core runs the engage hook after `JustEngagedWith` has queued the state. A
boss state renames a trace open under another name, except one `MarkPull` opened, which it closes
(`idle`) and replaces. Nothing renames a slug-named trace afterwards.

| Id | Encounter | `IN_PROGRESS` from (core) | Trace slug | Prefix |
|---|---|---|---|---|
| 0 | Lord Marrowgar | his `JustEngagedWith` | `lord-marrowgar` | `lm.` |
| 1 | Lady Deathwhisper | her `JustEngagedWith` | `lady-deathwhisper` | `ldw.` |
| 2 | Gunship Battle | the enemy captain's `DoAction(ACTION_ENEMY_GUNSHIP_TALK)` at the battle start | `icecrown-gunship-battle` | `gsb.` |
| 3 | Deathbringer Saurfang | his `JustEngagedWith` | `deathbringer-saurfang` | `dbs.` |
| 4 | Festergut | Putricide's `MovementInform` at his balcony point, seconds after the pull | `festergut` | `fg.` |
| 5 | Rotface | the same, at Rotface's point | `rotface` | `rf.` |
| 6 | Professor Putricide | his `JustEngagedWith` | `professor-putricide` | `pp.` |
| 7 | Blood Prince Council | Valanar's `JustEngagedWith`; Keleseth and Taldaram pull him in | `blood-prince-council` | `bpc.` |
| 8 | Blood-Queen Lana'thel | her `JustEngagedWith` | `blood-queen-lana-thel` | `bql.` |
| 9 | Sister Svalna | `BossAI::_JustEngagedWith` | `sister-svalna` | `ss.` |
| 10 | Valithria Dreamwalker | her `JustEnteredCombat`, as the adds reach her | `valithria-dreamwalker` | `vd.` |
| 11 | Sindragosa | her `JustEngagedWith` | `sindragosa` | `sg.` |
| 12 | The Lich King | his `JustEngagedWith` | `the-lich-king` | `lk.` |
| 13 | Sindragosa gauntlet | the gauntlet's `DoAction(ACTION_START_GAUNTLET)` | `sindragosa-gauntlet` | `sg.` |
| 14 | Blood Prince trash | never: `NOT_STARTED`, then `DONE` when the trash dies | `blood-prince-trash` | `bpc.` |
| — | Stinky and Precious | no boss state | `stinky-and-precious` | `dogs.` |
| — | Rotting Frost Giant | no boss state | `rotting-frost-giant` | `rfg.` |

`icc.` is raid-wide, matches every ICC slug and belongs to none: a slug owning it would have its keys
read silent on every other ICC pull. Festergut and Rotface engage before their state
moves, so their traces open on the engage, named by the DBC encounter credit, which matches the slug.
The last two rows open through `IccMarkPull` from `icc dogs` and `icc rotting frost giant tank
position` once the bot and the unit are both in combat. The dungeon bosses are the kill-credit
creatures: Marrowgar, Deathwhisper, Saurfang, Festergut, Rotface, Putricide, Valanar, the Blood-Queen,
Sindragosa, the Lich King. Gunship and Valithria credit by spell (72959, 72706), Svalna not at all.

## Cheats

Every cheat site runs through one of two `ICCShared` helpers and writes a `cheat` record
([observability.md](../../systems/observability.md)) keyed `<prefix>.<kind>`, with `tgt` the unit
acted on when it is not the bot:

- `IccRaidCheat(botAI, key, target)` returns `HasCheat(BotCheatMask::raid)` and records when true. It
  is asked last in each condition, so only a cheat that runs counts.
- `IccNoteCheat(bot, key, target)` only records. The `Playerbots.EnableICCBuffs` buffs keep that
  gate (shipped 1), act on heroic only, and reach human raiders too; the header carries it as
  `cfg.iccbuffs`.

Class **remove**: bots already have the legit answer (walking, a real class taunt, their own Nitro
Boosts), so the cheat is due to go. **Gap**: kept until a legit answer exists. **Buffs**: the
`EnableICCBuffs` operator option.

| Key | Class | Gate | Sites |
|---|---|---|---|
| `rf.nitro` | remove | raid | `ICCActions_RF.cpp`: `IccRotfaceTankPositionAction::HandleAssistTankPositioning`, `IccRotfaceGroupPositionAction::PositionHeroicGrid`, `IccRotfaceMoveAwayFromExplosionAction`, `IccRotfaceAvoidVileGasAction` (5) |
| `pp.nitro` | remove | raid | `IccPutricideGasCloudAction::HandleGaseousBloatMovement` |
| `vd.nitro` | remove | raid | `IccValithriaZombieKiteAction` |
| `sg.nitro` | remove | raid | `IccSindragosaGroupPositionTrigger`, `IccSindragosaFrostBeaconAction::HandleBeaconedPlayer`, `IccSindragosaBlisteringColdAction` |
| `lk.nitro` | remove | raid | `IccLichKingWinterAction`, `IccLichKingAddsAction::HandleDefileMechanics` (on the Defile target, humans too), `HandleVileSpiritMechanics` |
| `lk.painsuppression`, `lk.aegis` | remove | raid | `HandleVileSpiritMechanics`, beside its Nitro, no replacement |
| `rf.spitefulfury` | remove | raid | `IccRotfaceTankPositionAction`, Big Ooze pickup (3) |
| `vd.speed` | remove | raid | `IccValithriaDreamCloudAction`: run, walk and flight rate 2.0 in Dream State |
| `ldw.cyclone` | gap | raid | `IccAddsLadyDeathwhisperAction`: Cyclone on a Dominate Mind victim, bot or human |
| `ldw.insignificance` | gap | raid | same action: tank strips Touch of Insignificance |
| `rfg.deathplague` | gap | raid | `IccRottingFrostGiantTankPositionAction`: strips Death Plague |
| `gsb.battlefury` | gap | raid | `IccGunshipRocketJumpAction`: strips Battle Fury off the enemy captain |
| `pp.oozekill`, `pp.gascloudkill` | gap | raid | `IccPutricideVolatileOozeAction`, `IccPutricideGasCloudAction`: kill every extra add of the entry |
| `pp.unboundplague` | gap | raid | `IccPutricideAvoidMalleableGooAction::HandleUnboundPlague`: suicide with nobody near |
| `pp.freeooze` | gap | raid | `IccPutricideAbominationAction::TryRegurgitate`: Regurgitated Ooze with no energy cost; off, it casts at full cost |
| `bql.swarmingshadows` | gap | raid | `IccBqlGroupPositionAction`: tank strips Swarming Shadows |
| `vd.dreamteleport`, `vd.cloudteleport` | gap | raid | `IccValithriaDreamCloudAction`: onto a human in Dream State, onto the shared cloud |
| `sg.frostbeacon`, `sg.mysticbuffet`, `sg.magicbarrier`, `sg.nothreat` | gap | raid | `IccSindragosaGroupPositionTrigger`: main tank's P3 beacon strip, tanks' Mystic Buffet strip, Magic Barrier on tanks, 1% Threat on assist tanks |
| `sg.flares` | gap | raid | `IccSindragosaFrostBeaconAction::TryDropTombFlares`: smoke flares from `StoreNewItemInBestSlots` |
| `sg.tombstrip` | gap | raid | `IccSindragosaMysticBuffetAction`, `IccSindragosaFrostBombAction`: tomb aura strips |
| `lk.ragingspiritkill` | gap | raid | `IccLichKingWinterAction`: kills the fullest Raging Spirit past two |
| `lk.wickedspiritkill` | gap | raid | `HandleSpiritMarkingAndTargeting`: kills the star Wicked Spirit |
| `lk.hoj` | gap | raid | `HandleAssistTankAddManagement`: Hammer of Justice on Shamblings from any class |
| `ldw.nothreat`, `ldw.spitefulfury` | buffs | iccbuffs | `IccAddsLadyDeathwhisperAction` |
| `pp.heroicbuff`, `pp.spitefulfury` | buffs | iccbuffs | `IccPutricideMalleableGooTrigger`, on every member |
| `sg.heroicbuff`, `sg.spitefulfury` | buffs | iccbuffs | `IccSindragosaGroupPositionTrigger`, on every member |
| `lk.heroicbuff` | buffs | iccbuffs | `IccLichKingShadowTrapAction`, `IccLichKingWinterAction`, `IccLichKingAddsAction` (every member) |
| `lk.empoweredblood`, `lk.valkyrstun` | buffs | iccbuffs | `IccLichKingAddsAction`, `HandleValkyrAssignment` |

`*.heroicbuff` is `IccApplyHeroicBuffToMember` (Experienced, Aegis of Dalaran, Pain Suppression, 1%
Threat), recorded when it adds any, on members in the bot's own instance only. Experienced 71188 and
1% Threat 70115 never expire, so `IccStripExperienced`, run by the Marrowgar, giant, Saurfang,
Festergut, Rotface, council, Blood-Queen and Valithria triggers, takes both off the bot and off every
human in the instance. Every raid-gated site keeps the cheat's exact behaviour while the raid cheat is
on; off, the site skips the cheat. The cleanup halves (Cyclone, Magic Barrier, 1% Threat removal,
`SetSpeed` back to normal) stay ungated.

**The class taunt** (`IccCastClassTaunt`, ten bosses) casts the real taunt, at most once per 500 ms
per bot; a taunt on cooldown fails with no ranged pull, which would face the add. The 1M threat with
`FixateTarget` and the cooldown reset are gone, so:

- a caller taunting several units in a row reaches one per cooldown (8 s on every class): Saurfang's
  Blood Beasts, the gunship assist tank's boarders, Rotface's Big Ooze loop, the council's princes and
  Dark Nuclei, the Lich King's add passes and Raging Spirits, Stinky and Precious's adds;
- a taunted unit holds the taunter for the 3 s aura, then real threat decides;
- a tank swap (Festergut, Stinky and Precious, Putricide, Saurfang, Sindragosa) waits out a cooldown
  spent elsewhere, while the stacked tank has already stopped.

Each boss doc says what its callers risk.

Documented, not wrapped: Valithria's floor snaps (`IccValithriaPortalAction`,
`IccValithriaHealAction`) skip fall damage; `IccLichKingAddsAction`'s Val'kyr edge-dive puts the
carry aura on itself and suicides at the bottom. Not cheats: spell clicks and object use in range,
real item use, own-aura cancels, the spell listeners.

## Per-instance state

`IccInstanceState` (`ICCScripts.h`), one per instance id behind a mutex, `IccState(id)` creating it.
ICC keeps this store rather than `RaidInstanceState`. Assignments and latches are traced containers:
`lk.winterinbound`, `lk.mtinbound`, `lk.rangedinbound`, `lk.sharedslot` (-1 none), `sg.flaredblue`,
`sg.group`, `sg.tomb`, `sg.freedfallback`, `bql.curve`, `bql.groundphase`, `bql.postairlanding`,
`bql.airslot`, `bql.slot`, `vd.portalclaim`, `dbs.slot`, `rf.explosionslot`. They are unordered, and
no loop over one depends on its order. Timestamps, throttles, caches, edge detectors
(`sgLastPhase3`, `bqlBossWasAirborne`) and struct values stay bare, as does `sgFlaredRedThisPhase`,
an int set no traced container holds.

**Reset**: `IccBossStateResetScript` erases the whole state 5 s (`ICC_RESET_GRACE_MS`) after the last
boss state leaves `IN_PROGRESS`, kill or wipe, never mid-fight; `IccMapCleanupScript` erases it on
`OnDestroyMap`. It is keyed on boss state because Gunship, Valithria, Svalna, Keleseth and Taldaram are
not dungeon bosses, so a reset on "a dungeon boss left combat" let their latches outlive a wipe.

## Spell listeners

`ICCScripts.cpp`, on the caster's map thread, so their probes satisfy RaidObs's threading rule:

- **Malleable Goo**: Putricide's 70852 and the balcony's 72296, neither with a spelldifficulty row.
  The missiles and impacts they trigger (72297 row 2197; 72295/72615/74280/74281 row 2211) carry
  Putricide as their unit target and never pass the player test. Each impact is a 12 yd, 8 s hazard
  circle, Festergut's avoid model and the widest any consumer uses (Putricide's is 8 yd, 6 s).
- **Vile Gas**: Putricide's balcony cast on Rotface heroic, 69240 remapped by the `Spell` constructor to
  73019 (10H) or 73020 (25H), so it is matched through `GetSpellIdForDifficulty`. Notes `rf.vilegas`
  on the victim.
- **Defile** 72762, a 2 s cast with no difficulty row, read at `OnSpellPrepare`. Notes `lk.defile` on
  the target and a 6 yd hazard circle for the cast time; the puddle that follows is a sampled creature.

## Trigger names

Every creator starts `icc `, mostly followed by its boss: `lm`, `lady deathwhisper`, `dbs`,
`festergut`, `rotface`, `putricide`, `bpc`, `bql`, `valithria`, `sindragosa`, `lich king`. The others:
`icc dark reckoning` (Deathwhisper), `icc in cannon` and the `icc gunship` ones (Gunship), `icc
rotting frost giant tank position`, `icc dogs` (Stinky and Precious), `icc valkyre spear` and `icc
sister svalna` (Svalna). A trigger class's own name must equal its node's: `IccGunshipCannonNearTrigger`
called itself `icc in cannon` and merged its coverage into the real one.

## Known gaps

- Targeting is by raid icon throughout. Converting to direct targeting is a tactic rework; boss
  lanes probe every icon pick.
- Every gap cheat above: a mechanic it makes survivable is open, and a server with the raid cheat off
  loses it.
