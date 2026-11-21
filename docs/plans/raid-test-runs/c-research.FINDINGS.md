# Lane C findings: comps, pools, pilot start paths

Read-only research for [raid-test-runs.PLAN.md](raid-test-runs.PLAN.md). Paths without a repo prefix are
mod-playerbots (`docs/raids/...`, `src/Ai/Raid/...`); core paths start `src/server/...`, world SQL
`data/sql/...`. **(g)** marks a guess, anything else was read in the cited source.

## Open questions for the user

1. **Healer counts are guesses everywhere**: 5 at 25, 3 at 10. No strategy fixes a healer count; only
   Algalon (≥ 4 / ≥ 2, so its tank healers stay at half or less) and Thorim's gauntlet (takes 2 / 1)
   constrain it. Do any fights want 6 at 25 (Mimiron and Iron Assembly hard mode, Yogg, Algalon,
   heroic Anub'arak)? The pools carry an optional 6th healer for that.
2. **Tanks**: both pools use 2 protection paladins and a blood DK, the setup of the ULD1-3 test
   roster (`docs/raids/ulduar/README.md:190-205`). Keep it, or swap one paladin for a warrior or bear?
3. **The main tank**: who wears it per boss? A3 takes roles from `IsTank`; the main tank is the group's
   flag, else the first tank in group order (`PlayerbotAI.cpp:2606-2614`). Vezax wants the DK there (a
   paladin main tank runs out of mana, `vezax.md:379-382`). Should a scenario be able to name its main
   tank?
4. **Heroic Beasts, first pull**: Barrett's start option only shows to a clicker holding achievement
   3916 or 3917 (Call of the Crusade). Fresh characters and the driver have neither. Grant it to the
   driver or a pool member, or keep ToC test runs on normal?
5. **Pool size**: the union of every comp needs 26 members. I add two optional spares (6th healer,
   third DPS DK for Freya), so 28 for Ulduar and 27 for ToC, not 30-35. Cut the spares or keep them?
6. **Creation order is duty order.** Interrupt ranks, Freya's Army of the Dead, Algalon's star team,
   Eonar's Gift, Yogg's beacon claims and ToC's grips all rank by GUID, and GUIDs follow creation order.
   In template order the blood DK (#2) casts Freya's first Army while tanking. OK, or create the DPS
   DKs first?
7. **A third tank at 10-man** only for Anub'arak (the strategy latches two side tanks). Jaraxxus,
   Algalon and Iron Assembly at 10 get 2 (g). OK?
8. **Outside the brief, unverified**: `Playerbot.env` sets `AC_PLAYERBOTS_DISABLED_WITHOUT_REAL_PLAYER: "1"`,
   which logs out every non-self bot 300 s after the last real session, once any player has been seen
   since boot (`src/Bot/RandomPlayerbotMgr.cpp:385-410`, `src/Bot/PlayerbotMgr.cpp:369-395`). A baseline
   run on the live build after you log off could lose its raid mid-pull. Turn it off for test runs?

## 1. Comp table

T and H give tanks and healers as 25 / 10; DPS fill the rest. **Must have** lists the class or spec
minimums a scenario should declare. Interrupters are not a class, so they are not declared: the default
order puts a rogue, a mage, a DK and a shaman in every first 10. **Uses if present** lists duties the
strategy hands out when it finds the class but works around when it doesn't. Everywhere in Ulduar a
hunter or rogue redirects threat (Misdirection, Tricks) onto the tank the strategy names.

### Ulduar

| Boss | T | H | Must have | Uses if present | Source |
|---|---|---|---|---|---|
| Flame Leviathan | 2 / 2 (g); vehicle fight, no tank duty | 5 / 3 (g) | none: 2 / 5 of each vehicle give exactly 10 / 25 seats; melee drive siege engines, ranged drive demolishers | Boarding waits for the raid leader (master): wave 5 | `flame-leviathan.md:33-34`, `:488-491`; `Uld/Action/UldActions_FlameLeviathan.cpp:1291-1308` |
| Ignis | 2 / 2: main tank on Ignis, assist tank 0 walks constructs to the west pool | 5 / 3 (g) | none | Assist tank 1 (east pool) only matters when two constructs overlap, so a 3rd tank is optional (g). Ranged burst on a Brittle construct: Pyroblast (mage), Chimera/Aimed Shot, Chaos/Shadow Bolt, Mind Blast, Starfire, Lava Burst | `ignis.md:51-61`, `:89-93`; `Uld/Action/UldActions_Ignis.cpp:160-165` |
| Razorscale | 2 / 2: swap at 2 Fuse Armor stacks; main tank = the healthiest spec tank | 5 / 3 (g) | ranged DPS ≥ 4 / 2 (g): the harpoon crews are ranged non-healers, and 4 / 2 harpoons must hit to ground her | Dark Rune adds get no interrupt (open gap) | `razorscale.md:9-25`; `Uld/Util/UldEncounter_Razorscale.h:52`; `Uld/Util/UldEncounter_Razorscale.cpp:400-402`; `ulduar/README.md:178` |
| XT-002 | 2 / 2: the tank holding XT taunts the Pummeller only while a second tank lives | 5 / 3 (g) | none | Redirects go to the Pummeller's tank while one is out | `xt002.md:80-84`; `Uld/Action/UldActions_XT002.cpp:580-604` |
| Assembly of Iron | 3 / 2 (10 is g): bot tanks claim Brundir → Steelbreaker → Molgeim by GUID; two share the empowered Steelbreaker for the Meltdown swap, so 2 is the floor | 5 / 3 (g) | hunter ≥ 1 (Aspect of the Wild; everything is nature) | Interrupt duty on Brundir: rank 0 Lightning Whirl, rank 1 Chain Lightning (kick, pummel, shield bash, counterspell, wind shear, mind freeze, HoJ, bash, kidney shot, shockwave; he is silence-immune). A non-healer shaman drops Nature Resistance Totem | `iron-assembly.md:41-49`, `:63-76`, `:91-96`, `:119-129`; `Uld/Util/UldEncounter_IronAssembly.cpp:612-671`, `:1631-1637`, `:1696-1707` |
| Kologarn | 2 / 2: body tank, and off-tank on rubble / right arm; swap at 2 Crunch Armor | 5 / 3 (g) | hunter ≥ 1 (Aspect of the Wild; Shockwave and Petrifying Breath are nature) | | `kologarn.md:11-18`, `:51-53`, `:74-76`; `Uld/Trigger/UldTriggers_Kologarn.cpp:76-90`, `:165` |
| Auriaya | 2 / 2: assist tank 0 taunts every loose Sanctum Sentry | 5 / 3 (g) | priest or shaman ≥ 1 (Fear Ward / Tremor Totem; a fear every 35 s) | Ranged and healers soak Sonic Screech: more soakers, smaller shares | `auriaya.md:8-10`, `:25-38`, `:101-103`; `Uld/Trigger/UldTriggers_Auriaya.cpp:85`; `src/Ai/Raid/RaidAntiFear.cpp:25-102` |
| Hodir | 2 / 2: one hold point read by both tanks; off-tank stands square to it for the Frozen Blows taunt | 5 / 3 (g) | paladin ≥ 1; Frost Resistance Aura goes to a paladin tank first | | `hodir.md:69`, `:93-101`, `:254-258`; `Uld/Definition/UldDefinition_Hodir.cpp:73`; `Uld/Util/UldEncounter_Hodir.cpp:1007` |
| Thorim | 2 / 2: main tank holds the arena; assist tank 0 leads the gauntlet and is the phase 2 off-tank for the Unbalancing Strike swap | 5 / 3 (g); the gauntlet takes 2 / 1 healers and 9 / 4 DPS | bot paladin ≥ 1 (Frost Resistance Aura once Sif joins, which is hard mode); hunter ≥ 1 (Aspect of the Wild) | | `thorim.md:26-31`, `:239`, `:404-413`, `:684-698`, `:711-726`; `Uld/Util/UldEncounter_Thorim.cpp:422-466` |
| Freya | 2 / 2: main tank on Freya and the Storm Lasher; assist tank 0 works Snaplasher → Conservator → trio → lashers | 5 / 3 (g) | DK ≥ 2 (Army of the Dead, one per Detonating Lasher wave; a 10 min cooldown covers only one of the two); mage ≥ 1 (Frost Nova on lashers); hunter ≥ 1 (Frost Trap) | Paladin Hand of Freedom on rooted members; the first 5 ranged by GUID take Eonar's Gift; redirects to assist tank 0 | `freya.md:26-31`, `:240-243`, `:268-270`, `:285-290`, `:384-411`, `:503-505`; `Uld/Trigger/UldTriggers_Freya.cpp:217`, `:240`, `:260` |
| Mimiron | 2 / 2: phases 2-4 split two mechs across the main tank and assist tank 0 | 5 / 3 (g) | paladin ≥ 2 in hard mode (Fire Resistance Aura, and a second on Frost Resistance Aura in phase 3); non-tank melee ≥ 1 (Magnetic Core carrier); one of hunter, shaman or warlock (Bomb Bot snare) | Plasma Blast externals: Pain Suppression, Guardian Spirit, Hand of Sacrifice. Held raid cooldowns: Divine Sacrifice, Divine Hymn, Power Infusion | `mimiron.md:565-566`, `:664-671`, `:829`, `:973-985`, `:1071-1075`, `:1112`; `Uld/Definition/UldDefinition_Mimiron.cpp:130`; `Uld/Util/UldEncounter_Mimiron.cpp:908-910` |
| General Vezax | 2 / 2: in hard mode tanks switch to the Saronite Animus at once (normal mode needs 1) | 5 / 3 (g) | a main tank without mana (the DK): a paladin main tank ran dry at 1:12 | Searing Flames interrupt, GUID-ranked among ready bots; redirects saved for the Animus | `vezax.md:217-220`, `:314-321`, `:379-382`; `Uld/Util/UldEncounter_Vezax.cpp:831-832` |
| Yogg-Saron | 2 / 2: one Guardian pile; the second tank takes it when the holder drops under 50% | 5 / 3 (g) | priest or shaman ≥ 1 (anti-fear) | Interrupts on Dark Volley (P1) and Drain Life (P3). Brain team: melee first, exactly one healer (priest > druid > shaman > paladin), then ranged, never tanks. Shadow Beacon draggers (3 marks at 25): tanks while another lives, then non-healer paladins and DKs, hunters, a feral druid. Arms warrior Mortal Strike on healing Guardians. Warlock felhunter. Shaman lust in P3. 130 shadow resistance | `yogg-saron.md:60`, `:87-90`, `:992-997`, `:1519-1522`, `:1575`, `:1620-1625`, `:1647-1656`, `:1699-1704` |
| Algalon | 3 / 2 (10 is g): a swap pair of two bot tanks (swap at 4 Phase Punch) plus a handler, a third bot tank, else the pair tank not holding him. One tank is unsupported | 5 / 3 (g); at least 4 / 2 so the tank healers (2 / 1) stay at half or less | shadow priest ≥ 1 (Big Bang soaker; glyphed Dispersion 75 s against a 90.5 s cycle, premade glyph 45753); at 25 shadow priest ≥ 2 (the backup soaker) | Tank healers ranked holy paladin > disc > resto druid > resto shaman > holy priest; star team 3 / 2 damage dealers, ranged first; Hand of Sacrifice, Pain Suppression, Guardian Spirit, Lay on Hands as damage cutters | `algalon.md:49-56`, `:302-309`, `:403-405`, `:440-447`, `:474-500`, `:760-762`; `Uld/Util/UldEncounter_Algalon.h:280-281`; `Uld/Util/UldEncounter_Algalon.cpp:648-662`, `:1015-1048`, `:1205-1250`; `conf/playerbots.conf.dist:2291` |

### Trial of the Crusader

| Boss | T | H | Must have | Uses if present | Source |
|---|---|---|---|---|---|
| Northrend Beasts | 2 / 2 on normal: duties Gormok + Gormok swap, mobile + stationary worm, Icehowl, dealt over the living bot tanks. Heroic 3 / 3 (g): the timers overlap Gormok with the worms, 4 duties at once | 5 / 3 (g) | paladin ≥ 1 (Hand of Protection resets Impale: the stacked tank itself, then ret, prot, holy) | Two or more hunters split the worms for Misdirection (even index mobile, odd stationary); rogue Tricks on the mobile worm's tank; Tranquilizing Shot on Frothing Rage (normal only) | `northrend-beasts.md:37-46`, `:140-151`, `:321-326`, `:586-587`; `trial-of-the-crusader/README.md:316-327`; `ToC/Util/ToCHelpers_Gormok.cpp:326-335`; `ToC/Util/ToCHelpers_Jormungars.cpp:960-980` |
| Lord Jaraxxus | 3 / 2 (10 is g): main tank on him, assist tank 0 on Mistresses, assist tank 1 on Infernals; a lone assist tank takes Mistresses first | 5 / 3 (g) | one of: mage (Spellsteal), non-healer shaman (Purge), non-healer priest (Dispel Magic) to strip Nether Power | Fel Fireball interrupt (lowest GUID with one ready). Holy paladin with Aura Mastery on Fire Resistance Aura. Raid cooldowns on Burning Inferno: Aura Mastery, Divine Sacrifice, Anti-Magic Zone, Divine Hymn, Tranquility | `lord-jaraxxus.md:121-124`, `:138-139`, `:152-153`, `:178-181`, `:194-201`; `ToC/Util/ToCHelpers_Jaraxxus.cpp:421-424` |
| Faction Champions | 2 / 2 (g): bot tanks stand nearest and take every first pick; extras stand 3 yd aside | 5 / 3 (g) | non-tank DK ≥ 1 (Death Grip duty and grip post); mage ≥ 1 (Counterspell on the kill target); priest ≥ 1 (Mass Dispel); paladin ≥ 1 (Hand of Freedom, Fire Resistance Aura) | Ice duty: each DK keeps one melee champion in Chains of Ice. CC by dps mages, warlocks, ranged druids (Cyclone stand), dps shamans (Hex). Non-healer shaman or priest purges. Non-tank druid Rebirth. The grip chain needs a player's `pull` order: wave 4 | `faction-champions.md:277`, `:279`, `:283-287`, `:290-291`, `:296-297`, `:300-307`; `ToC/Util/ToCHelpers_Staging.cpp:584` |
| Twin Val'kyr | 2 / 2: Fjola's tank and Eydis's tank on door stands; a lone tank works | 5 / 3 (g) | shaman ≥ 1 (lust on heroic Eydis's Pact) | Twin's Pact interrupt: kick, pummel / shield bash, mind freeze, counterspell, wind shear, Spell Lock. A paladin on Fire Resistance Aura with Aura Mastery lets the raid hold through a normal Light Vortex | `twin-valkyr.md:120-125`, `:136`, `:153-154`; `ToC/Util/ToCHelpers_TwinValkyr.cpp:335-348`, `:915-930`, `:1363-1385` |
| Anub'arak | 3 / 3: main tank plus two side tanks latched per pull, bots first | 5 / 3 (g) | non-warlock ranged DPS ≥ 2 (g; sphere duty excludes warlocks) | Druid Rebirth on a dead side tank. DK grip duty on Frost Spheres (one per side). Paladins: Hand of Protection / Divine Shield for the spike kiter, Hand of Freedom, heroic escort, phase 3 Judgement of Light. Shaman lust and Healing Stream in phase 3. Hunter and rogue redirect duty. Shadow Strike interrupts (heroic). Warlock Demonic Circle (heroic) | `anubarak.md:168-172`, `:191-229`, `:685-695` |

## 2. Pool templates

**Assumptions about A3's picker**: per role, members in template order, required ones first; a comp
with fewer tanks or healers fills with DPS in order. The default 25 is 2 tanks, 5 healers, 18 DPS and
the default 10 is 2 / 3 / 5, so a 3-tank comp takes #26 and drops the last unrequired DPS (#25). The
rows above all pick from #1-26: 25-man from #1-25 plus #26, 10-man from #1-10 plus #26 (Anub'arak).

Race and gender are cosmetic except Draenei: Heroic Presence (+1% hit, party-wide) counts toward the
bots' hit caps, so each block of five carries one, assuming parties fill in order (g). Anub'arak heroic
phase 3 also depends on the party layout (Healing Stream per party), tuned for your own roster
(`anubarak.md:1384-1396`), so heroic Anub'arak on the pool needs its own grouping.

Spec names are `Playerbots.PremadeSpecName` values (`conf/playerbots.conf.dist:2135-2481`).

### `uld` (28)

| # | Class | Spec | Race | Gender | Role | Why here |
|---|---|---|---|---|---|---|
| 1 | Paladin | prot pve | Draenei | M | tank | Main tank; Hodir's paladin tank for Frost Resistance Aura; a Mimiron paladin |
| 2 | Death Knight | blood pve | Dwarf | F | tank | No mana bar (Vezax main tank); first Freya Army |
| 3 | Paladin | holy pve | Human | F | heal | Algalon tank healer #1; Aura Mastery |
| 4 | Priest | disc pve | Dwarf | M | heal | Algalon tank healer #2; Pain Suppression; Fear Ward |
| 5 | Shaman | resto pve | Draenei | F | heal | Lust (only shamans have it); Tremor Totem |
| 6 | Hunter | mm pve | Night Elf | M | ranged | Aspect of the Wild (Kologarn, Thorim, Iron Assembly); Frost Trap; Bomb Bot snare |
| 7 | Rogue | combat pve | Human | M | melee | Tricks; Kick; Magnetic Core carrier |
| 8 | Priest | shadow pve | Night Elf | F | ranged | Algalon soaker |
| 9 | Mage | arcane pve | Gnome | M | ranged | Counterspell; Freya Frost Nova |
| 10 | Death Knight | unholy pve | Draenei | M | melee | Second Freya Army; Mind Freeze; beacon dragger |
| 11 | Druid | resto pve | Night Elf | F | heal | Rebirth |
| 12 | Priest | holy pve | Draenei | F | heal | Guardian Spirit; Yogg brain-team healer |
| 13 | Priest | shadow pve | Human | M | ranged | Algalon backup soaker (25) |
| 14 | Shaman | enh pve | Draenei | M | melee | Iron Assembly's spare shaman (Nature Resistance Totem); second lust |
| 15 | Paladin | ret pve | Dwarf | M | melee | Hand of Freedom / Sacrifice; beacon dragger |
| 16 | Warrior | arms pve | Human | F | melee | Yogg Mortal Strike |
| 17 | Hunter | surv pve | Draenei | F | ranged | Second hunter (traps, redirect) |
| 18 | Rogue | as pve | Night Elf | M | melee | |
| 19 | Mage | fire pve | Human | F | ranged | Pyroblast on Brittle constructs |
| 20 | Druid | balance pve | Night Elf | M | ranged | Starfire on Brittle constructs |
| 21 | Shaman | ele pve | Draenei | M | ranged | Lava Burst; Bomb Bot Frost Shock |
| 22 | Warlock | affli pve | Gnome | F | ranged | Felhunter; Bomb Bot Curse of Exhaustion |
| 23 | Warlock | destro pve | Human | M | ranged | Chaos Bolt on Brittle constructs |
| 24 | Hunter | bm pve | Dwarf | F | ranged | |
| 25 | Warrior | fury pve | Dwarf | M | melee | First DPS dropped for a 3rd tank or 6th healer |
| 26 | Paladin | prot pve | Human | M | tank | **Only** Algalon 25 (handler) and Assembly of Iron 25 (third claim); Ignis's assist tank 1 if wanted |
| 27 | Paladin | holy pve | Dwarf | F | heal | **Optional**: 6th healer (question 1) |
| 28 | Death Knight | frost pve | Gnome | F | melee | **Optional**: Freya 25 with two DPS DKs, so the tank never channels Army (g) |

### `toc` (27)

| # | Class | Spec | Race | Gender | Role | Why here |
|---|---|---|---|---|---|---|
| 1 | Paladin | prot pve | Draenei | M | tank | Main tank; Hand of Protection on its own Impale stacks |
| 2 | Death Knight | blood pve | Dwarf | F | tank | |
| 3 | Paladin | holy pve | Human | F | heal | Aura Mastery on fire (Jaraxxus, Twins) |
| 4 | Priest | disc pve | Dwarf | M | heal | Pain Suppression; Fear Ward; Mass Dispel |
| 5 | Shaman | resto pve | Draenei | F | heal | Lust (Twins heroic Pact, Anub'arak P3); Healing Stream |
| 6 | Hunter | mm pve | Night Elf | M | ranged | Worm redirect index 0; Tranquilizing Shot |
| 7 | Rogue | combat pve | Human | M | melee | Tricks on the mobile worm; Kick; Vanish as spike kiter |
| 8 | Mage | arcane pve | Gnome | M | ranged | Spellsteal Nether Power; Counterspell / Polymorph in FC; Ice Block kiter |
| 9 | Death Knight | frost pve | Draenei | M | melee | Non-tank DK: FC Death Grip and ice duty; Anub'arak sphere grip |
| 10 | Priest | shadow pve | Night Elf | F | ranged | Non-healer Dispel Magic (Nether Power, FC purge) |
| 11 | Druid | resto pve | Night Elf | F | heal | Rebirth on a side tank |
| 12 | Priest | holy pve | Draenei | F | heal | Guardian Spirit |
| 13 | Hunter | surv pve | Dwarf | F | ranged | Worm redirect index 1 |
| 14 | Rogue | as pve | Night Elf | M | melee | |
| 15 | Mage | fire pve | Human | F | ranged | Second FC caster CC |
| 16 | Shaman | ele pve | Draenei | M | ranged | Non-healer Purge; Hex; second lust |
| 17 | Death Knight | unholy pve | Human | M | melee | Second ice duty; second sphere grip |
| 18 | Paladin | ret pve | Dwarf | M | melee | Hand of Protection / Freedom / Sacrifice; Anub'arak escort and Judgement of Light |
| 19 | Warlock | affli pve | Gnome | F | ranged | FC Fear; Spell Lock on Twin's Pact |
| 20 | Druid | balance pve | Night Elf | M | ranged | Cyclone stand; second Rebirth |
| 21 | Shaman | enh pve | Draenei | F | melee | |
| 22 | Warlock | destro pve | Human | M | ranged | |
| 23 | Warrior | arms pve | Human | F | melee | |
| 24 | Hunter | bm pve | Draenei | M | ranged | |
| 25 | Warrior | fury pve | Dwarf | M | melee | First DPS dropped for a 3rd tank or 6th healer |
| 26 | Paladin | prot pve | Human | M | tank | **Only** Jaraxxus 25, Anub'arak 25 and 10, heroic Beasts (g) |
| 27 | Paladin | holy pve | Dwarf | F | heal | **Optional**: 6th healer, second Aura Mastery |

## 3. Pilot start paths

### XT-002 (map 603, `BOSS_XT002`, entry 33293 / 25-man 33885)

- **Start: aggro.** No gossip; `Reset` leaves him `REACT_AGGRESSIVE` (`src/server/scripts/Northrend/Ulduar/Ulduar/boss_xt002.cpp:177-186`).
  Aggro radius is `detection_range` 20 (`data/sql/base/db_world/creature_template.sql:26638`) plus the
  level gap, about 23 yd against level 80 before the core's reach terms (`src/server/game/Entities/Creature/Creature.cpp:3477-3502`).
  Engaging calls `BossAI::_JustEngagedWith`: `DoZoneInCombat` with a 5 s combat pulse, so every player
  in the instance joins (`src/server/game/AI/ScriptedAI/ScriptedCreature.cpp:681-686`). A missing Heart
  is re-summoned on every engage (`boss_xt002.cpp:206-211`), so hard mode can be retried each pull.
- **He patrols before the pull**: `MovementType` 2, path 1360540 (`data/sql/base/db_world/creature.sql:128456`,
  `creature_addon.sql:33387`), 13 nodes over x 852-884, y -79..39, pausing 11-30 s
  (`data/sql/updates/db_world/2026_07_04_03.sql:40-53`). Each node reached becomes his home position
  (`src/server/game/Movement/MovementGenerators/WaypointMovementGenerator.cpp:113-132`, `:382`).
- **Requires no boss state.** `CheckRequiredBosses` gates only Yogg-Saron, on Vezax
  (`instance_ulduar.cpp:1324-1339`). His door (GO 194631) is `DOOR_TYPE_ROOM` (`instance_ulduar.cpp:36`),
  shut only while the encounter runs. A walk from the instance entrance is blocked by the Leviathan
  doors (GO 194630, spawned shut, `gameobject.sql:23801`) until Flame Leviathan is `DONE`
  (`instance_ulduar.cpp:592-601`, `:839-841`), so the drop has to be a teleport.
- **Wipe.** `EnterEvadeMode` despawns his summons (adds, Void Zones, Life Sparks), then the BossAI
  evade sets `BOSS_XT002` to `NOT_STARTED` (`boss_xt002.cpp:189-193`; `ScriptedCreature.cpp:706-714`);
  `Reset` restores Stand, selectable, aggressive and clears hard mode. No respawn: he doesn't die on a
  wipe. The door reopens. **He evades to where he was pulled, not to his spawn row, and resumes the
  patrol**, so A2's "boss back at spawn" check must read alive, out of combat and evade finished,
  never a distance to (886.3, -12.1).
- **Config**: `AC_PLAYERBOTS_ULDUAR_XT_002_HARD_MODE: "1"`, so bots burn the Heart and a kill is a
  Heartbreak kill.
- **Trash**: 11 Scrapyard spawns (Parts Recovery Technician 34267, XR-949 Salvagebot 34269, XD-175
  Compactobot 34271, XB-488 Disposalbot 34273) at x 785-818, |y| 50-92, 100-120 yd from him
  (`creature.sql:129527-129559`). With `ClearRoomBeforeBoss=0` they stay alive.

| Drop point | Source | Notes |
|---|---|---|
| (837.0746, 28.01061, 409.811) | `ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED_NORTH`, `Uld/Util/UldEncounter_XT002.cpp:39` | Preferred. 63 yd from the spawn row, **24.9 yd** from the nearest patrol segment (marginal against ~23 yd aggro), 40 yd from the nearest trash. Corner of the north ranged bomb lot, whose cells were navprobe-verified (`xt002.md:185-186`) |
| (763.883, -11.8925, 410.3) | XT-002 door, GO 194631, `gameobject.sql:23804` | Out of everyone's aggro: 122 yd from the spawn row, 91 yd from the patrol, 66 yd from trash. It is the room door's own position: check it lands inside the room. Ranged then walk ~100 yd after the pull |
| (866.0, -12.5, 409.8) | `ULDUAR_XT002_RANGED_SPOT`, `UldEncounter_XT002.cpp:31` | The fight's ranged anchor, but only 10.7 yd from the patrol: he pulls the raid as he walks past. Only with an uncontrolled opener |

The spawn row, (886.275, -12.0545, 409.602, o 3.129) (`creature.sql:128456`), is reference only: standing there pulls him.

### Northrend Beasts (map 649, boss 0)

- **Start: Barrett's gossip.** The fork's staging and the beasts' forced pulls key on it
  (`trial-of-the-crusader/README.md:251-301`).

  | Mode | NPC | First pull | Retry (Beasts state `FAIL`) |
  |---|---|---|---|
  | 10N / 25N | Barrett Ramsey **34816**, `gossip_menu_id` 10599 | menu 10599 option **0** → menu 10600 option **0** ("Bring forth the first challenge!") | 10599 option **1** → 10600 option **1** ("We're ready. This time things will be different.") |
  | 10H / 25H | Barrett Ramsey **35909**, `gossip_menu_id` 10704 | 10704 option **0** → 10706 option **0** → 10600 option **0**. Shown only if the clicker has achievement 3916 or 3917; without it only option 1 shows, leading to menu 10705, text only | 10704 option **2** → 10600 option **1**, no achievement needed |

  Sources: `data/sql/base/db_world/gossip_menu_option.sql:4078-4081`, `:4129-4132`;
  `data/sql/updates/db_world/2026_10_16_00.sql:6-7` (menus, SmartAI rows, conditions on boss 0
  `FAIL`); `2026_11_10_01.sql` (either achievement unlocks both heroic sizes). Both Barretts are the
  same entry at 10 and 25.
- **What the pick does.** Barrett's SmartAI closes the gossip, sets instance data 2
  (`TYPE_ANNOUNCER_GOSSIP_SELECT`) once, drops his gossip flag, walks to (563.54, 78.78) and despawns.
  At stage 0 the instance runs the 35 s intro (`EVENT_SCENE_001`); at stage 1 it goes straight to
  `EVENT_SCENE_004` (`instance_trial_of_the_crusader.cpp:223-234`, `:449-483`). `SCENE_004` sets
  Beasts `IN_PROGRESS`, closes the entrance portcullis (GO 195648 at y 78.2, `gameobject.sql:67107`)
  and summons a combat stalker that zone-combats every player each 5 s until the last beast dies
  (`:461-476`). Gormok attacks `SelectNearestTarget(200)` about 46 s after a first-pull gossip and
  about 11 s after a retry (`README.md:256`).
- **Who picks.** A player inside that instance within 5.5 yd (`INTERACTION_DISTANCE`) of Barrett at
  `Locs[LOC_BARRETT]` (559.1528, 90.55729, 395.2734) (`trial_of_the_crusader.h:343`). The heroic
  first-pull option checks that player's achievements (question 4).
- **Pick through the gossip, never `SetData(2)` directly, and only once the bots have seen him.** The
  bots' staging window opens only after a bot tick has seen this stage's Barrett with his gossip flag
  and then without it (`ToC/Util/ToCHelpers_Staging.cpp:985-1002`); the search runs every 500 ms
  (`:45-48`, `:917-945`). Picked before any bot saw him, no bot stages and Gormok opens on whoever is
  nearest. Give the raid about 1 s in the instance with Barrett up first.
- **Requires** a fresh instance at stage 0; no earlier boss. A wipe never lowers the stage
  (`README.md:26`), so every retry in the same instance is a stage 1 pick.
- **Wipe.** A beast's `EnterEvadeMode` sets `TYPE_FAILED` (`boss_northrend_beasts.cpp:528-534`,
  `:846-851`, `:1233-1238`); separately, every 5 s with no living non-GM-mode player in the instance,
  `InstanceCleanup` runs (`instance_trial_of_the_crusader.cpp:428-435`, `:1281-1294`). Cleanup
  despawns Gormok and his snobolds, both worms, Icehowl and the stalker, clears the beasts mask and
  the snobold vehicle kits, turns `IN_PROGRESS` into `FAIL`, reopens the entrance, and re-summons
  Barrett (normal: always; heroic: while attempts remain) (`:1296-1399`). No respawn delay.
- **Attempt counter, heroic only.** `MAX_ATTEMPTS` 50 (`:37`), minus one per wipe, either on
  `TYPE_FAILED` (`:213-219`) or on a cleanup with an encounter running (`:1382-1386`), never both
  (`_cleaned`). At 0 no Barrett is summoned and gossip picks are ignored (`:224`, `:1041`).

| Drop point | Source | Notes |
|---|---|---|
| (559.1528, 90.55729, 395.2734) | `Locs[LOC_BARRETT]`, `trial_of_the_crusader.h:343` | Barrett's own spot, so the clicker is in reach. 49 yd south of the arena centre, 12.4 yd inside the portcullis |
| (563.5, 88.2, 395.2) | Twin Val'kyr lone door stand, `ToC/Util/ToCHelpers_TwinValkyr.cpp:56`, `:67` | Where bots already park for the Twins; 4.9 yd from Barrett, 10 yd inside the portcullis |
| (563.672974, 139.571, 393.837006) | `ARENA_CENTER`, `ToC/Util/ToCData.cpp:10` (= `Locs[4]`) | The arena centre; stands are dealt outward from the arrivals once the gossip opens the window (`README.md:303-331`). The clicker still has to be at Barrett |

**navprobe cannot verify any of these.** The arena floor is GO 195527 with no static mesh under it
(`README.md:52-60`), and the entrance tunnel has none either (`twin-valkyr.md:192`). Read Z from a bot
standing there instead. Nothing may stand south of y ≈ 78 (the portcullis), or the gate locks it out.

### Traps for A2 and the orchestrator

- **A driver inside the instance must be in GM mode** (`IsGameMaster()`), not just hold GM security.
  Otherwise BossAI's zone combat (XT) and the Beasts' combat stalker pull it into the fight, and in ToC
  it counts as a living player, so the empty-instance cleanup never fires
  (`instance_trial_of_the_crusader.cpp:1281-1294`).
- **Revive only after the wipe has resolved**: ToC once Beasts is `FAIL` and Barrett is back with his
  gossip flag; XT once he is out of combat with his evade finished. Reviving earlier hands the
  still-fighting beasts or XT fresh targets.
