# Icecrown Citadel (map 631)

Strategy key `icc`, one strategy for every encounter. Cross-raid conventions are in
[../README.md](../README.md). Mechanics source: `src/server/scripts/Northrend/IcecrownCitadel/`. A
code comment calling the core bugged is checked against it; the boss doc gives the verdict, with issue
text when it holds. During an upstream merge ICC keeps ours; the merge report lists upstream's ICC
changes so the user can pick which to port. Tactics come from the guides in
[guide-sources.md](../../systems/guide-sources.md), converted into `build-icc-wt/_guides/`
(`text/<slug>.<tag>.md`, listed in `INDEX.md`).

## Code layout and registration

`src/Ai/Raid/ICC/` splits per encounter stem, in dispatch order: `Marrowgar`, `Deathwhisper`,
`FrostGiant` (Rotting Frost Giants), `Gunship`, `Saurfang`, `Dogs` (Stinky and Precious), `Festergut`,
`Rotface`, `Putricide`, `BloodPrinces` (council and trash), `BloodQueen`, `Svalna` (two definitions:
`Svalna`, `SvalnaGauntlet` for the Ymirjar), `Valithria`, `SindragosaAdds` (two definitions:
`SindragosaDrakes` for Rimefang and Spinestalker, `SindragosaGauntlet`), `Sindragosa`, `LichKing`
(phases two and three and Frostmourne in `LichKingLate` files, under the one definition; its
`OwnMovement` rules are `icc lich king movement` (Attack only), `icc lich king late movement` (Attack
and Reach) and `icc lich king late reach hold` (Attack only, in heroic Frostmourne and on a Val'kyr
chase round a puddle)). A stem owns:

- `Action/ICCActions_<Stem>`, `Trigger/ICCTriggers_<Stem>`, `Multiplier/ICCMultipliers_<Stem>`;
- `Util/ICCHelpers_<Stem>`: its positions, constants, phase readers and other helpers;
- `Definition/ICCDefinition_<Stem>.cpp`: one `EncounterDefinition` with its rows and multipliers,
  the accessor declared in `Definition/ICCDefinitions.h`.

A file exists once it has content: `Marrowgar`, `FrostGiant`, `Svalna` and `LichKingLate` have no
multipliers.
Raid-wide parts:

- `Util/ICCHelpers_Shared`: helpers several encounters use (class taunt and CC, raid icons, heroic
  buffs, the cheat helpers).
- `Util/ICCData.h`: map id, the `CreatureIdsICC` and `SpellIdsICC` enums, and the Defile and
  Malleable Goo ids the spell listeners watch. A new encounter-specific id or position goes in the
  stem's helpers.
- `Util/ICCEncounterGate.h`: the boss state ids (`IccEncounterId`, mirroring the core's `DataTypes`)
  and `IccRaidWideGate`.
- `Util/ICCSeams.{h,cpp}`: the hooks table and the configs for the raid-wide helpers in
  `src/Ai/Raid/` (seams below).
- Raid-wide definitions, on `IccRaidWideGate` (open, never live), `ICC_BOSS_NONE` and no trace name:
  `RaidWide` (`Trigger/ICCTriggers_RaidWide`, `Action/ICCActions_RaidWide`,
  `Definition/ICCDefinition_RaidWide.cpp`: the opener and resistance aura rows, their holds, and the
  direct-targeting bases) and `Definition/ICCDefinition_HeroicBuffs.cpp` (the strip, [Cheats](#cheats)).
- `Util/ICCScripts.{h,cpp}`: per-instance state, the spell listeners, the state reset, the Blood
  Prince trash hook and the RaidObs name table, registered by `AddSC_IcecrownBotScripts` from
  `src/Script/Playerbots.cpp`.
- Umbrellas `Action/ICCActions.h`, `Trigger/ICCTriggers.h`, `Multiplier/ICCMultipliers.h` include
  every stem; `RS/RSMultipliers.cpp` includes `ICCActions.h`.

**Gates.** An encounter's definition carries its slug from the table below as trace name. A boss
definition takes `BossStateGate` with its `IccEncounterId` (Svalna 9): open unless that boss is done
or another is in progress, so pre-pull steps run; live while `IN_PROGRESS`. Valithria's `VdGate` also
stays open past `DONE` for a bot still flagged in the air within 100 yd of her spawn, so it lands with
fall damage. `Dogs`, `FrostGiant`, `SvalnaGauntlet` and `SindragosaDrakes` (`ICC_BOSS_NONE`, 255) take
`UnitGate<entries>` (`RaidEncounter.h`) on Stinky 37025 and Precious 37217, on the giant's 38490 (10)
and 38494 (25), on the Ymirjar 37132, 38125, 37127, 37134, 37133, 38184 and 38154, and on Rimefang
37533 and Spinestalker 37534 (trace `sindragosa`): open while one is alive on the bot's
map, live while it is in combat with a victim in the bot's group (a pet counts as its owner) or with
the bot. A per-instance guid cache, rescanned at most once a second, finds it, so one in an unloaded
grid leaves the gate closed. Their ticks open the trace through `IccMarkPull` while live. Each trigger
still tests its boss itself, mostly through `"find target"` (the bot's threat list), Marrowgar's by
entry through a per-instance guid cache, the giant's through its gate's live unit, the Gunship's
through the instance's guid slots (2 raid ship, 40 enemy ship) and both ships' passenger sets,
Sindragosa's through slot 11, the drakes' through slots 25 (Rimefang) and 24, the gauntlet's through
its controller 37503's slot; one firing while its gate is live names the trace
(`RaidObs::NamePull`). The strip, the heroic buffs and `IccMarkPull` run in definition ticks, which
run while the gate is open.

Rows, rules, `e.Multiplier` and ticks run while open, `e.LiveMultiplier` only while live. Every
encounter multiplier is live. Festergut's and Rotface's state moves when Putricide reaches his balcony
point, about 7 s into the pull (~102 and ~104 yd at 15 yd/s). What acts before live:

| Encounter | Before live |
|---|---|
| Festergut | every row, rule and the tick from his engage, keyed on his combat; only the multiplier `icc festergut brake` waits for live |
| Rotface | every row and rule, keyed on him being in combat; the multipliers `icc rotface ooze splash` and `icc rotface self cure` wait for live |
| Gunship | for bots aboard the raid's ship: the definition tick, `icc gunship rocket pack setup` (no pack equipped, Zafod within 150 yd), `icc gunship cannon near` (a cannoneer with its pack), `icc in cannon`; `icc gunship tank defensive` is asked now but returns false until live; `icc gunship rocket jump`, every rule and the multiplier wait for live |
| Valithria | `icc valithria dream`'s landing (she is within 100 yd, and past `DONE` through `VdGate`) and the tick's once-per-instance `cross` to `skull` `rti` reset; every other node waits for `IN_PROGRESS`, which comes once the first archmage pulls the combat trigger |
| Sindragosa | through her fly-in (unattackable, out of combat): the tick, with its once-per-instance `cross` or `star` to `skull` `rti` reset, `icc sindragosa stand` round her landing spot and the `icc sindragosa tank form` caster-form block; every other node and rule waits for her combat |
| Lich King | the tick, with the same `rti` reset, and `icc lich king stand`'s opening stands while he walks the intro |

Nothing else: Marrowgar's nodes key on his boss state, the giant's and the dogs' on their gate being
live, any prince's engage pulls Valanar into the state in the same update, and the rest key on fight
auras, units or `"find target"`.

**Seams.** An encounter gains a trigger, action, multiplier, rule or tick in its stem files and
definition alone:

- trigger and action: classes carrying their name as `static constexpr char const* Name`, joined by
  an `e.Node<Trigger, Action>(priority)` row. Rows naming one trigger back to back share a node.
  pblint resolves a row through both `Name`s, so keep each on one line.
- multiplier: `e.LiveMultiplier<M>(families)`, `e.Multiplier<M>` only for one that acts before live;
  rules (`OwnMovement`, `OwnTargeting`, `Block`, `Exclusive`) and `Tick` likewise, rules after the
  stem's multipliers.
- direct targeting: a resolver `Unit* (PlayerbotAI*)` in the stem's helpers, `GetRaidHumanSkull` its
  first branch, every branch an in-combat unit, its pick probed there. A trigger class on
  `IccDirectTargetTrigger` and an action on `IccDirectAttackAction` pass their `Name` and the resolver;
  one row joins them, and `e.OwnTargeting(name, roles, &IccResolverPicks<&Resolver>, pickers)`
  follows the stem's multipliers. The action sends the pet first, the trigger also fires while the
  pet is off the pick. Marrowgar, Deathwhisper, the Gunship (`GsbTarget`), Saurfang (`GetDbsTarget`),
  Stinky and Precious (`GetDogsTarget`), Festergut (`GetFgTarget`), Rotface (`GetRfTarget`),
  Putricide (`GetPpTarget`), the Blood Prince Council (`GetBpcTarget`), the Blood-Queen
  (`GetBqlTarget`), Svalna (`GetSsTarget`), her gauntlet (`GetSsGauntletTarget`), Valithria
  (`GetVdTarget`), Sindragosa (`GetSgTarget`), her
  drakes (`GetSgDrakeTarget`), gauntlet (`GetSgGauntletTarget`) and the Lich King (`GetLkTarget`)
  use it, with no icon writes or `rti` use. `RaidRestoreLegacyRti` stays unwired
  until no ICC code writes `rti`, and it never resets `star`. The Marrowgar, Sindragosa and Lich King ticks reset
  a bot's `cross` or `star` `rti` to `skull` once per instance, the Valithria tick a `cross`, and the
  Gunship tick a `star` while its gate is open; a bot that skips those rooms keeps what the old code set.
- hooks: a row in `HookRows()` (`Util/ICCSeams.cpp`), `{&Icc<Stem>Definition, burstWindow,
  openerBosses, resistSchools, fireWantsMastery}`, unused hooks null, the functions in the stem's
  helpers. Only the first row whose definition is live is asked, so none answers before live (the
  table above). `burstWindow` feeds `icc burst window` (`RaidBurstWindow.h`), which with no row lets
  every burst through. `openerBosses` feeds the opener (`RaidOpener.h`, `icc.opener`): a DPS bot holds
  its damage on a listed boss until a group tank has held it 2 s, or 5 s after it came into combat.
  `resistSchools` and `fireWantsMastery` feed the aura election (`RaidResistAura.h`, `icc.aura`): one
  bot paladin per wanted fire, frost or shadow school, the nature hunter's Aspect of the Wild, and the
  hold rules keep each holder's own auras or aspects off. With no hook the opener and aura triggers neither fire nor note.
  Rows: Marrowgar's opener (boss 36612), Deathwhisper's burst window (lust waits for phase 2),
  Saurfang's opener (boss 37813) and burst window (lust waits for Frenzy), Festergut's resist
  schools (shadow: Gaseous Blight, Blighted Spores, Pungent Blight), Putricide's opener (boss 36678),
  burst window (lust waits for phase 3) and resist schools (shadow), the council's opener (Valanar
  37970), burst window (lust in Keleseth's empowered phase) and resist schools (shadow and fire), the
  Blood-Queen's opener (boss 37955), burst window (lust at the vampire peak, 8 on 10-man and 16 on
  25-man, or at 280 s) and resist schools (shadow), Valithria's resist schools (fire, frost and nature;
  her lust is the `icc valithria lust` node, which the burst gate never sees),
  the drakes' resist schools (frost: Rimefang's Frost Aura), Sindragosa's opener (boss 36853), burst
  window (lust in the last phase) and resist schools (frost), the Lich King's opener (boss 36597),
  burst window (lust in phase one) and resist schools (shadow and frost).
- defensives: `GetRaidCover` and `GetRaidPersonalDefensive` (`RaidDefensive.h`) with
  `ICC_DEFENSIVE_SCOPE` (`icc.cover`, `icc.personal`) and a `RaidPredictedHit` the stem builds.
  Tank buttons go through `NextTankDefensive` (`RaidTankDefensive.h`) with a `Block` over
  `HeldTankDefensiveNames()` keeping the class nodes off them until a low-health valve, and
  `CallHumanTankDefensive` tells a human tank. Users:
  - Marrowgar: `icc lm tank defensive` (each Bone Slice window and under 40%, class buttons held to
    25%, `lm.defensive`, `lm.humancall`) and `icc lm storm cover` (`GetRaidCover` per 6 s of Bone
    Storm ticks, class cover nodes held over the lead);
  - Deathwhisper: `icc lady deathwhisper defensive` (Deformed Fanatic, unkicked Frostbolt, under 35%),
    plus `icc lady deathwhisper brake`, a `LiveMultiplier` holding damage dealers at 90% of the top
    tank's threat (frees at 80%);
  - Gunship: `icc gunship tank defensive` (`gsb.defensive`, `gsb.furywindow`, `gsb.humancall`);
  - Saurfang: `icc dbs tank defensive` (holder or Mark carrier at Blood Power peaks and through
    Frenzy, `dbs.humancall`) and `icc dbs mark defensive` (`GetRaidPersonalDefensive` on
    `icc.personal`);
  - Stinky and Precious: `icc dogs tank defensive` (a button in each Decimate cast, `dogs.defensive`,
    `dogs.humancall`) and `icc dogs decimate cover` (raid cover during the dog's 3 s self-stun after
    Decimate);
  - Festergut: `icc festergut tank defensive` (ladder at 2 and 3 Inhaled Blight, `fg.defensive`,
    `fg.humancall`), `icc festergut pungent cover` and `icc festergut pungent defensive` (raid cover
    and personal buttons on the Pungent Blight cast);
  - Rotface: `icc rotface tank defensive` (a tank kiter under a Big Ooze of 4+ stacks within 6 yd, a
    damage dealer kiter under any Big Ooze within 6 yd through `NextPhysicalDefensive`, the kiter
    gassed with its ooze within 20 yd, any tank under 40%, class buttons held to 25%,
    `rf.defensive`, `rf.humancall`);
  - Putricide: `icc putricide tank defensive` (first 4 s of each phase 3 hold, under 40%, class
    buttons held to 25%, `pp.defensive`, `pp.humancall`), plus `icc putricide brake`, a
    `LiveMultiplier` holding non-holders at 90% of the holder's threat (frees at 80%);
  - Blood Prince Council: `icc bpc tank defensive` (Keleseth's tank through his empowered phase, any
    tank under 40%, class buttons held to 25%, `bpc.defensive`, `bpc.humancall`), `icc bpc lance
    defensive` (a Keleseth stand-in's `GetRaidPersonalDefensive` on `icc.personal` per Shadow Lance),
    `icc bpc flame defensive` (the Inferno victim's magic row, never Cloak of Shadows, or Dispersion;
    never an immunity; `bpc.flame`), plus `icc bpc brake`, a `LiveMultiplier` holding non-owners at
    90% of the empowered prince's owner's threat (frees at 80%);
  - Blood-Queen: `icc bql tank defensive` (any tank or damage-dealer mirror under 40%, her holder and
    the mirror in the last minute before Berserk, a damage dealer through `NextPhysicalDefensive`,
    class buttons held to 25%, `bql.defensive`, `bql.humancall`), `icc bql whirl cover` and `icc bql
    whirl defensive` (raid cover and personal buttons on Bloodbolt Whirl), plus `icc bql brake`, a
    `LiveMultiplier` holding non-holders at 90% of the holder's threat (frees at 80%), and `icc bql
    splash`, holding area damage within 15 yd of a mind-controlled vampire kept under control (not one
    marked `bql.kill`);
  - Svalna: `icc sister svalna tank defensive` (a captain's tank under 40%, `ss.defensive`,
    `ss.humancall`) and, in her gauntlet, `icc sister svalna gauntlet tank defensive` (a Ymirjar's
    tank under 40%, `ss.gauntlet.defensive`, `ss.gauntlet.humancall`), class buttons held to 25%;
  - Valithria: `icc valithria tank defensive` (an abomination's tank under 40%, 50% under Gut Spray,
    class buttons held to 25%, `vd.defensive`, `vd.humancall`);
  - Sindragosa: `icc sindragosa tank defensive` (the holder on each breath cast at 3 Mystic Buffet
    stacks in the last phase or under 50%, any tank under 40%, class buttons held to 25%,
    `sg.defensive`, `sg.humancall`), `icc sindragosa holder cover` (a healer bot's Pain Suppression,
    Guardian Spirit or Hand of Sacrifice on a holder no swap relieves, from 5 Mystic Buffet stacks,
    `sg.cover`), plus `icc sindragosa brake`, a `LiveMultiplier` holding non-holders but healers at
    90% of the holder's threat (frees at 80%);
  - her drakes: `icc sindragosa drake tank defensive` (a drake's tank under 40%, physical only;
    Rimefang's on each Frost Breath under 60%; `sg.drake.defensive`, `sg.drake.humancall`), plus `icc
    sindragosa drake brake` (free tanks all fight, everyone but healers while Rimefang flies);
  - the Lich King: `icc lich king tank defensive` (the holder at 15 Plague Siphon stacks and each Soul
    Reaper, Anti-Magic Shell only on Soul Reaper; the free tank on an unanswered Enrage; any tank under
    40%; class buttons held to 25%; `lk.defensive`, `lk.humancall`), plus `icc lich king brake`
    (non-holders at 90% of the holder's threat, frees at 80%).
- opening positions: a `Mover` row in the definition whose trigger holds the stand while the gate is
  open and not live, its positions navprobed in the stem's helpers. No raid-wide staging.

**The root only dispatches.** `ICCStrategy.cpp` adds nodes and runs `OnTick` in
`IccEncounterDefinitions()` order, and adds multipliers in its own: Marrowgar, Deathwhisper,
FrostGiant, Saurfang, Dogs, Festergut, Rotface, Putricide, BloodPrinces, BloodQueen, Svalna,
SvalnaGauntlet, Valithria, Sindragosa, SindragosaDrakes, SindragosaGauntlet, LichKing, Gunship,
RaidWide, then `icc burst window`. **Both orders are behaviour**: equal relevances pop in insertion order, and the engine stops at the first multiplier
that zeroes an action, crediting it with the veto, while several ICC multipliers act inside
`GetValue`. `ICCActionContext.h` and `ICCTriggerContext.h` register every definition's rows. The
registration sites name only these root files: `RaidStrategyContext.h`,
`BuildSharedActionContexts.cpp`, `BuildSharedTriggerContexts.cpp`, and `PlayerbotAI.cpp` (map 631 →
`icc`, and the instance-strategy name list).

## Boss states and trace names

`instance_icecrown_citadel.cpp` calls `SetBossNumber(15)`; the ids are the core's `DATA_*` order in
`icecrown_citadel.h`. `AddSC_IcecrownBotScripts` registers each id's slug (`RaidObs::NameBossStates`).
A trace opened while that boss is `IN_PROGRESS` takes the slug outright, whether the boss state opens
it or an engage does: the core runs the engage hook after `JustEngagedWith` has queued the state. A
boss state renames a trace open under another name, except one `MarkPull` opened, which it closes
(`idle`) and replaces: a dogs trace in its 30 s idle window would otherwise absorb the Festergut
pull. Nothing renames a slug-named trace afterwards.

| Id | Encounter | `IN_PROGRESS` from (core) | Trace slug | Prefix |
|---|---|---|---|---|
| 0 | [Lord Marrowgar](lord-marrowgar.md) | his `JustEngagedWith` | `lord-marrowgar` | `lm.` |
| 1 | [Lady Deathwhisper](lady-deathwhisper.md) | her `JustEngagedWith` | `lady-deathwhisper` | `ldw.` |
| 2 | [Gunship Battle](icecrown-gunship-battle.md) | the enemy captain's `DoAction(ACTION_ENEMY_GUNSHIP_TALK)` at the battle start | `icecrown-gunship-battle` | `gsb.` |
| 3 | [Deathbringer Saurfang](deathbringer-saurfang.md) | his `JustEngagedWith` | `deathbringer-saurfang` | `dbs.` |
| 4 | [Festergut](festergut.md) | Putricide's `MovementInform` at his balcony point, seconds after the pull | `festergut` | `fg.` |
| 5 | [Rotface](rotface.md) | the same, at Rotface's point | `rotface` | `rf.` |
| 6 | [Professor Putricide](professor-putricide.md) | his `JustEngagedWith` | `professor-putricide` | `pp.` |
| 7 | [Blood Prince Council](blood-prince-council.md) | Valanar's `JustEngagedWith`; Keleseth and Taldaram pull him in | `blood-prince-council` | `bpc.` |
| 8 | [Blood-Queen Lana'thel](blood-queen-lana-thel.md) | her `JustEngagedWith` | `blood-queen-lana-thel` | `bql.` |
| 9 | [Sister Svalna](sister-svalna.md) | `BossAI::_JustEngagedWith` | `sister-svalna` | `ss.` |
| 10 | [Valithria Dreamwalker](valithria-dreamwalker.md) | her `JustEnteredCombat`, as the adds reach her | `valithria-dreamwalker` | `vd.` |
| 11 | [Sindragosa](sindragosa.md) | her `JustEngagedWith` | `sindragosa` | `sg.` |
| 12 | [The Lich King](the-lich-king.md) | his `JustEngagedWith` | `the-lich-king` | `lk.` |
| 13 | [Sindragosa gauntlet](sindragosa.md#sindragosa-gauntlet) | the gauntlet's `DoAction(ACTION_START_GAUNTLET)` | `sindragosa-gauntlet` | `sg.` |
| 14 | [Blood Prince trash](blood-prince-council.md#blood-prince-trash) | never: `NOT_STARTED`, then `DONE` once its four door guards die | `blood-prince-trash` | `bpc.` |
| — | [Stinky and Precious](stinky-and-precious.md) | no boss state | `stinky-and-precious` | `dogs.` |
| — | [Rotting Frost Giant](rotting-frost-giant.md) | no boss state | `rotting-frost-giant` | `rfg.` |
| — | [Svalna gauntlet](sister-svalna.md#svalna-gauntlet) | no boss state | `svalna-gauntlet` | `ss.` |

`icc.` is raid-wide, matches every ICC slug and belongs to none: a slug owning it would have its keys
read silent on every other ICC pull. An encounter with a phase publishes it as `<prefix>.phase`;
`tools/botobs/bosses/icecrown_citadel.py --phases` reads it as one raid-wide timeline, each step from
the first bot to note it. Festergut and Rotface engage before their state moves, so their traces open
on the engage, named by the DBC encounter credit, which matches the slug. The last three rows open
through `IccMarkPull` from their definitions' ticks while the gate is live, the Blood Prince trash
through a hook in `Util/ICCScripts.cpp` ([its section](blood-prince-council.md#blood-prince-trash)).
Rimefang and Spinestalker's tick opens a `sindragosa` trace the same way.
The dungeon bosses are the kill-credit creatures: Marrowgar, Deathwhisper, Saurfang, Festergut,
Rotface, Putricide, Valanar, the Blood-Queen, Sindragosa, the Lich King. Gunship and Valithria credit
by spell (72959, 72706), Svalna not at all.

## Cheats

Every cheat site runs through one of two `ICCHelpers_Shared` helpers and writes a `cheat` record
([observability.md](../../systems/observability.md)) keyed `<prefix>.<kind>`, with `tgt` the unit
acted on when it is not the bot:

- `IccRaidCheat(botAI, key, target)` returns `HasCheat(BotCheatMask::raid)` and records when true. It
  is asked last in each condition, so only a cheat that runs counts.
- `IccNoteCheat(bot, key, target)` only records. The `Playerbots.EnableICCBuffs` buffs keep that
  gate (shipped 1), act on heroic only, and reach human raiders too; the header carries it as
  `cfg.iccbuffs`.

`tools/botobs/bosses/icecrown_citadel.py --cheats` is the only view of `cheat` rows: `--bot` and
`--notes` skip them.

Class **gap**: kept until the encounter carries the guide's answer; one survives only where bots
can't carry that answer out at all, its reason in the boss doc. **Buffs**: the `EnableICCBuffs`
operator option, kept. A removed cheat leaves the table; its boss doc's Cheats section says what
replaced it.

| Key | Class | Gate | Sites |
|---|---|---|---|
| `lk.floorsnap` | gap | raid | `FloorSnap` (`ICCHelpers_LichKing.cpp`): a bot sunk through the platform or Frostmourne floor is put back on it |
| `ldw.nothreat`, `ldw.spitefulfury` | buffs | iccbuffs | the Deathwhisper definition's tick while live |
| `pp.heroicbuff`, `pp.spitefulfury` | buffs | iccbuffs | the Putricide definition's tick while live, on every member |
| `sg.heroicbuff`, `sg.spitefulfury` | buffs | iccbuffs | the Sindragosa definition's tick while live, on every member |
| `lk.heroicbuff`, `lk.empoweredblood` | buffs | iccbuffs | the Lich King definition's tick while he is in combat, on every member (`lk.empoweredblood` 60-40%) |
| `lk.valkyrstun` | buffs | iccbuffs | the late tick: Hammer of Justice once per carrying Val'kyr |

`*.heroicbuff` is `IccApplyHeroicBuffToMember` (Experienced, Aegis of Dalaran, Pain Suppression, 1%
Threat), recorded when it adds any, on members in the bot's own instance only. Experienced 71188 and
1% Threat 70115 never expire, so `IccHeroicBuffsDefinition`'s tick takes both off whenever none of the
fights handing them out (Deathwhisper, Putricide, Sindragosa, the Lich King) is `IN_PROGRESS`: each bot
off itself, and the first bot past a 1 s per-instance throttle off every human on the map. Every
raid-gated site keeps the cheat's exact behaviour while the raid cheat is
on; off, the site skips the cheat. The cleanup halves (Cyclone, 1% Threat removal) stay ungated.

**The class taunt** (`IccCastClassTaunt`, every encounter but Marrowgar and the giant) casts the real taunt, at most once per 500 ms
per bot; a taunt on cooldown fails with no ranged pull, which would face the add. The 1M threat with
`FixateTarget` and the cooldown reset are gone, so:

- a caller taunting several units in a row reaches one per cooldown (8 s on every class): the gunship
  home tank's boarders, the council's princes and Dark Nuclei, the Lich King's add passes and Raging
  Spirits;
- a taunted unit holds the taunter against threat for the 3 s aura, then real threat decides; a
  later taunt takes it at once (`ThreatManager::TauntUpdate`: the latest taunt wins);
- a tank swap (Festergut, Stinky and Precious, Putricide, Saurfang, Sindragosa) waits out a cooldown
  spent elsewhere, while the stacked tank has already stopped.

Saurfang, Festergut, Rotface's Big Ooze, Putricide, the princes, the Blood-Queen and the gunship captain carry taunt
diminishing returns (`flags_extra` 0x80000). Saurfang's swap is one elected taunt per Rune of Blood with generic taunts
blocked, and his free tank taunts one Blood Beast only while the next swap stays covered. Festergut
and the dogs swap by one elected taunt, generic taunts blocked; with fewer than two tanks alive the
dogs' zombie guard (a Death Knight, else a paladin damage dealer) taunts zombies off healers, and a
free tank never taunts zombies. Rotface's kiter taunts a Big Ooze through one path, skipped at the
immune level. Putricide's swap is one elected taunt, generic taunts blocked, held for two Mutated
Plague applications, or to 3 stacks after one while taunt DR is under level 3; with no tank alive a
Death Knight or paladin damage dealer picks him up. The council's tanks own their princes by roster
and taunt one back only below DR level 3, generic taunts blocked; with one tank in the raid a damage
dealer stands in for Keleseth (a Death Knight or paladin by its class taunt, others by direct hits
on nuclei), its threat dumps (Soulshatter, Fade, Feign Death, Invisibility, Misdirection on the
main tank) blocked by `icc bpc stand-in threat`. So does the Blood-Queen's main
tank, with Death Grip blocked too; with one tank player in the raid her mirror is a damage dealer
(plate melee first). Svalna's captains go to bot tanks by roster, taunted back when lost; her
gauntlet elects one rescue taunt per Warlord, Battle-Maiden or Corrupted Ymirjar on a non-tank,
generic taunts free. Valithria's tanks taunt worms and zombies off healers, generic taunts blocked.
Sindragosa's holder taunts her back off a non-tank and her last-phase swap is one elected taunt,
generic taunts blocked; a swapper she isn't on 1 s after its taunt goes back to the melee stand. The
drakes' free tanks have generic taunts blocked; the Sindragosa gauntlet elects one rescue taunt per
Champion or Warrior on a non-tank, generic taunts free. With one tank player the Lich King's phase one
ghouls go to a Death Knight, else a paladin damage dealer (`lk.tank` 3), and a Raging Spirit a human
tank holds is never taunted off it. The gunship's board tank taunts the captain.

Each boss doc says what its callers risk.

Not cheats: spell clicks and object use in range, real item use, own-aura cancels, the spell
listeners.

## Per-instance state

`IccInstanceState` (`Util/ICCScripts.h`) sits in a `RaidInstanceState` store: `IccState(id)` creates
it, and a has-state read (`ActiveGooPositions`, the reset) uses `Find`. Assignments and latches are
traced containers, unordered, and no loop over one depends on its order. Saurfang keeps
`dbs.slot`, `dbs.kill`, `dbs.cc`, `dbs.markheal` and `dbs.protect` in its stem's own
`RaidInstanceState`, Stinky and Precious `dogs.brake` and `dogs.guard` (`ObsGuidSet`), Festergut
`fg.slot`, `fg.carrier`, `fg.leg`, `fg.sporespot`, `fg.brake`, `fg.closeshift`, `fg.farshift` and
`fg.drag` (`ObsValue`), Rotface `rf.slot`, `rf.dispel`, `rf.kiter`, `rf.flood`, `rf.oozespot` and
`rf.freedom` (`ObsValue`), Putricide `pp.spot`, `pp.slot`, `pp.rings` (`ObsValue`), `pp.driver`,
`pp.receiver` and `pp.brake`, the council `bpc.tank`, `bpc.slot`, `bpc.bomb`, `bpc.petbomb`,
`bpc.dispel` (`ObsGuidMap`), `bpc.soak` and `bpc.brake` (`ObsGuidSet`), the Blood-Queen `bql.tank`, `bql.slot`, `bql.airslot`,
`bql.group`, `bql.bite`, `bql.cc`, `bql.curve`, `bql.brake`, `bql.kill` (`ObsGuidSet`) and
`bql.massdispel`, Svalna `ss.spear` and `ss.tank`,
Valithria `vd.healer`, `vd.portalclaim` and `vd.tank`, Sindragosa `sg.tank`, `sg.brake`, `sg.breath`,
`sg.bombs`, `sg.tombhold` and `sg.beacon`, her drakes `sg.drake.tank`, `sg.drake.brake` and
`sg.drake.air`; Deathwhisper's stem latches `ldw.brake`
(`ObsGuidSet`) and the frost giant's `rfg.apart` (`ObsGuidSet`) the same way. Unused: the gunship's
`gsbLastStarPos` and `gsbLastRocketPackUse`, Festergut's `fgSlotState`, `festergutGooWaitUntil` and
`SpreadSlotState`, Rotface's `rotfaceVileGasWaitUntil` and `rfExplosionSlotMemory`
(`rf.explosionslot`, silent), Putricide's `ppBloatLastDir` and `BloatDir`, the council's
`bpcSpreadLockTimers`, the Blood-Queen's `bqlBotCurrentCurve`, `bqlAirSlotMemory` and
`bqlBotSlotMemory`, Valithria's `vtCloudSync` and `vtPortalClaim` (the stems reuse their keys),
Sindragosa's `sgFlaredRedThisPhase`, `sgLastPhase3` and the traced `sg.flaredblue`, `sg.group`,
`sg.tomb` and `sg.freedfallback`, the Lich King's traced `lk.winterinbound`, `lk.mtinbound`,
`lk.rangedinbound` and `lk.sharedslot` (-1 none). Timestamps, throttles, caches, edge detectors and struct values stay
bare.

**Reset**: `IccBossStateResetScript` stamps the state while any boss is `IN_PROGRESS` and erases it 5 s
(`ICC_RESET_GRACE_MS`) after the last one leaves it, kill or wipe, never mid-fight;
`RaidInstanceStateMapScript` drops it with the map. It is keyed on boss state because Gunship, Valithria, Svalna, Keleseth and Taldaram are
not dungeon bosses, so a reset on "a dungeon boss left combat" let their latches outlive a wipe.

## Spell listeners

`Util/ICCScripts.cpp`, on the caster's map thread, so their probes satisfy RaidObs's threading rule.
Unpaced: each fires once per cast, and 25H goo hits two players at once. A note's `txt` is the spell
id.

- **Malleable Goo**: Putricide's 70852 and the balcony's 72296, neither with a spelldifficulty row.
  The missiles and impacts they trigger (72297 row 2197; 72295/72615/74280/74281 row 2211) carry
  Putricide as their unit target and never pass the player test. Each impact is noted as a 12 yd, 8
  s hazard circle; consumers size their own. The 70852 missile flies 6 yd/s and its impact is 5 yd:
  Putricide times each landing from where he stood at its first scan after the cast and keeps it
  16 s (the listener evicts at 9 s only on the next cast), Festergut uses 5 yd, 8.5 s.
- **Vile Gas**: Putricide's balcony cast on Rotface heroic, 69240 remapped by the `Spell` constructor to
  73019 (10H) or 73020 (25H), so it is matched through `GetSpellIdForDifficulty`. Notes `rf.vilegas`
  on the victim; no Rotface code reads `rotfaceVileGas` any more.
- **Defile** 72762, a 2 s cast with no difficulty row, read at `OnSpellPrepare`. Notes `lk.defile` on
  the target and a 10 yd hazard circle (the script's 10 × scale, spawn scale 1) for the cast time;
  the puddle that follows is a sampled creature.

## Trigger names

Every trigger name starts `icc `, mostly followed by its boss: `lm`, `lady deathwhisper`, `dbs`,
`festergut`, `rotface`, `putricide`, `bpc`, `bql`, `valithria`, `sindragosa`, `lich king`. Deathwhisper's are
`icc lady deathwhisper` dodge, defensive, frostbolt, dominate mind, taunt, redirect and target;
Marrowgar's include `icc lm tank defensive` and `icc lm storm cover`, Saurfang's `icc dbs tank
defensive` and `icc dbs mark defensive`. Festergut's are `icc festergut` goo, pungent defensive,
pungent cover, tank defensive, bloat swap, bloat clear, brake, target and stand; Rotface's `icc
rotface` dodge, freedom, totem, tank defensive, dispel, ooze taunt, kite, carrier, cure walk, target
and stand; Putricide's `icc putricide` dodge, swap, tank defensive, kite, plague, stack, abomination,
brake, target and stand; the council's `icc bpc` dodge, tank defensive, ball, prince taunt, nucleus,
soak, lance defensive, flame defensive, dispel, brake (action `icc bpc brake stop`), target and
stand; the
Blood-Queen's `icc bql` dodge, vampiric bite, swarming shadows, pact of darkfallen, whirl defensive,
whirl cover, tank defensive, anti fear, mass dispel, frenzy cc, taunt, brake, target and stand;
Valithria's `icc valithria` dodge, kite, dream, portal, taunt, snare, void, target, heal, interrupt,
freedom, tank defensive, lust, guardian spirit and beacon; Sindragosa's `icc sindragosa` blistering
cold, splash, bomb hide, frost beacon, tank defensive, taunt, buffet hide, unchained magic, hand of
freedom, holder cover, beacon heal, hold, target and stand, her drakes' `icc sindragosa drake` dodge,
tank defensive, anti fear, brake, target and stand, her gauntlet's `icc sindragosa gauntlet` dodge,
taunt, kick, free and target (prefix row ahead of `icc sindragosa` in `ICC_PREFIXES`); the Lich
King's `icc lich king` quake, shadow trap, defile, vile spirits, frostmourne, tank defensive, taunt,
siphon taunt, plague cure, plague walk, tranquilize, stun, valkyr control, silence dispel, totem,
harvest soul, target, stack, stand and brake. The others:
`icc dark reckoning` (Deathwhisper), `icc in cannon` and the `icc gunship` ones (Gunship), `icc
rotting frost giant plague`, `icc dogs` tank defensive, decimate cover, swap, zombie taunt, yield and target
(Stinky and Precious), `icc valkyre spear` and `icc sister svalna` hurl, interrupt, tank defensive,
taunt and target (Svalna), and her gauntlet's `icc sister svalna gauntlet` dodge, interrupt (a stun
on Summon Undead and Frozen Orb channels, a kick on Empowered Shadow Bolt), tank defensive, taunt and
target (prefix row ahead of `icc sister svalna`). The raid-wide `icc opener swing`, `icc opener
redirect` and `icc resistance aura` belong to no encounter. Rows register classes under their `Name`, so two classes sharing one leave
one unbuilt.

## Known gaps

- Every gap cheat above: a mechanic it makes survivable is open, and a server with the raid cheat off
  loses it.
- The hooks answer only while a definition is live, so Festergut's and Valithria's aura hooks, and
  any for Rotface, sit out the seconds before their state moves.
