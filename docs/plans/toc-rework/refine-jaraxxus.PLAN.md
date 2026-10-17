# refine-jaraxxus — Lord Jaraxxus 25H: heal the Incinerate, cover the Inferno, keep melee off the spawn

Wave 4f, a refine lane on the shipped Jaraxxus lanes (`w2-jaraxxus`, `w2-jaraxxus-rest`). Rules,
sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md). Mechanics and the
shipped design: [lord-jaraxxus.md](../../raids/trial-of-the-crusader/lord-jaraxxus.md). Line numbers
are as of `e692831de`.

Trace (25H kill at 4:20, 8 deaths, 3 Rebirths; build 2026-10-17 15:37 UTC carrying `352940a36`;
humans Dragon (paladin tank) and Deathsong (DK); roster 4 heal, 9 melee, 10 ranged, 2 tank), in
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/`: `649_2_lord-jaraxxus_1792259165.ndjson`.
25N baselines, same raid, 0 deaths, every Incinerate healed: `649_3_lord-jaraxxus_1792176436`,
`649_6_lord-jaraxxus_1792186303`. Read with `bosses/lord_jaraxxus.py <f>` (`--boss --nether
--incinerate --flame --adds`) and `postmortem.py <f>` (`--threat`). The trace's `heal` rows read
`a = 0` under a heal absorb; absorbed healing is in the Chronicle combat log
(`docker exec chronicle-app-1 sh -c 'zcat /root/.chronicle/storage/raidlogs/logs/64b30f2a-a71a-4d37-9376-4919cb394c73'`,
`SPELL_HEAL` fields after the spell are amount, overheal, absorbed, crit).

## Findings from the 2026-10-17 25H pull

1. **Burning Inferno: 6 of 8 deaths, 3.37M of the raid's 5.54M damage taken.** 6 of 7 Incinerate
   Flesh windows ran out. Incinerate (66237/67049/67050/67051) is aura 301
   `SPELL_AURA_SCHOOL_HEAL_ABSORB` (85k on 25H) plus an aura-23 tick at its 12 s end that casts
   Burning Inferno (fire, 7.8-8.2k/s on everyone for 5 s on 25H) if the aura is still up; the
   remaining absorb is `AuraEffect::GetAmount()` on effect 0. The target reads full health, so
   `JaraxxusHealIncinerateTargetAction` (`ToCActions_Jaraxxus.cpp:228-258`) skips every cast-time
   heal, because `PlayerbotAI::UpdateAI` (`PlayerbotAI.cpp:371-377`) cancels a single-target heal
   casting on a full-health target. Healing absorbed per failed window: 47k, 66k, 62k, 62k, 68k,
   68k. The cleared window (target at 91%, so cast-time heals ran) absorbed 81k in 9.6 s, 55k from
   2 Healing Waves and 4 Flashes of Light. The druid re-cast Rejuvenation 11 times a window on a
   target that had it (~1k each), the priest Renew ~4 times for one 2.4k heal.
2. **Heroic Mistress spawn kills melee on the Portal (2 deaths).** The heroic Portal leads the kill
   order, so melee stand on it; the Mistress spawns 6.0-6.3 s after it, hits within 10 yd (~10k) and
   swings at the nearest (27.6k, 43.9k).
3. **Legion Flame carrier out of heal range (1 death, 1 at 1.9%).** `DeriveCarrierSpot`
   (`ToCHelpers_Jaraxxus.cpp:334-385`) heads straight off the boss with no healer-distance bound;
   carriers ended 30-48 yd from the nearest healer, one starting its carry at 36% after an Inferno.
4. **Defensives only react.** Raid cooldowns came 2.2-3.1 s into an Inferno, in 4 of 6; Aura Mastery
   twice under Concentration Aura while Fire Resistance Aura sat on a ret paladin; Evasion and
   Deterrence against the fire AoE; never Tranquility or Anti-Magic Zone. Generic code fires
   personal buttons on health bands and raid cooldowns on "medium group heal setting"; Tranquility
   sits in an opt-in strategy (`DruidAiObjectContext.cpp:37`) no bot gets, Anti-Magic Zone
   (`DKAiObjectContext.cpp:178`) has no trigger, Aura Mastery never checks the aura.
5. **Threat at the pull.** A DPS warrior pulled the boss three times and held him 18.0-26.3 s; the
   two tanks taunted 8 times in 20 s and Jaraxxus obeys taunt diminishing returns (flags_extra
   0x80000), so the last four did nothing.
6. **Adds on non-tanks** (Mistresses 100% / 40%, Infernals 87% / 100% / 54%): the human held the
   boss 0:43-3:11 and 3:51-end, the bot main tank stayed on him.
7. **Fel Lightning**: 745k, no lethal hit. The doc's reopen condition fired 3 times, each a death
   taking 57-70% of its damage from Burning Inferno.

Worked, keep: Fel Fireball 18/18 kicked (median 43 ms); Nether Power at 1+ stacks 12.4 s of 4:20;
Legion Flame trails clear of the raid; Mistress' Kiss 0 punishes.

## Decided by the user

- Tank roles stay fixed: no bot cover for adds when another tank holds the boss (finding 6 goes to
  Known gaps).
- The pre-emptive defensives are a shared ToC helper, Jaraxxus its first user; later lanes plug
  other bosses into it.
- Composition stays (4 healers).

## Scope

1. **Heal the Incinerate target.**
   - `PlayerbotAI.cpp:372` (outside ToC, list it in the report): no full-health cancel while the
     target `HasAuraType(SPELL_AURA_SCHOOL_HEAL_ABSORB)`.
   - `JaraxxusHealIncinerateTargetAction`: drop the full-health skip; skip a HoT the bot already has
     on the target; order by throughput, high instants first, then cast-time heals, HoTs last. Keep
     the Kiss instant-only rule.
   - Probe `jaraxxus.absorb`: the target's remaining absorb, change-only.
2. **Shared pre-emptive defensive helper** in a new `Util/ToCHelpers_Defensive.{h,cpp}` (name it as
   fits), for a predicted raid-wide hit given school, time to impact, duration and damage per player.
   - Raid cover, one holder per cooldown, dealt and latched per hit: Divine Hymn, Tranquility and
     Anti-Magic Zone (cast directly), Divine Sacrifice, Aura Mastery only while its caster holds the
     resistance aura matching the school, each timed to cover the hit. Algalon's star cover is the
     pattern (`Uld/Util/UldEncounter_Algalon.cpp:3712-3770`; read only, Ulduar is never touched).
   - Personal: when the hit would take the bot under a floor, `NextMagicDefensive`
     (`RaidTankDefensive.h:33`, `needMs` = hit duration); on a lethal hit with no row, Dispersion,
     then `GetSelfImmunitySpell` (`EncounterHelpers.h:132`), then healthstone. Never Evasion or
     Deterrence for magic. Leave `RaidTankDefensive.cpp`'s table alone: Algalon and Yogg-Saron read it.
   - Hold the class raid-cooldown nodes only inside a predicted hit's lead time (an `e.Block` as at
     `UldDefinition_Algalon.cpp:146-156`), never fight-long.
   - Jaraxxus users: Burning Inferno (an Incinerate with ~2 s or less left and absorb remaining), and a
     Legion Flame carrier under 50% (personal only).
   - ResistAura stem (`ToCHelpers_ResistAura.h:24`): on Jaraxxus, Fire Resistance Aura goes to the
     paladin with Aura Mastery.
   - `refine-fc` runs alongside and wires its own single-target externals; folding them onto the
     helper is `w6-closeout`'s call.
3. **Melee off the heroic Portal**: melee DPS bots leave 10 yd of a heroic Portal (34825, selectable)
   from its age 5 s until the Mistress's victim is a tank or she is dead; ranged keep killing it.
4. **Carrier stays healable**: `DeriveCarrierSpot` rejects spots beyond 30 yd of the nearest living
   healer (the sweep's `fullLeg` predicate, `ToCHelpers_Jaraxxus.cpp:345`) and bends along that ring.
5. **DPS threat cap**: Anub'arak's latch (`AnubarakThreatCapped`, `ToCHelpers_Anubarak.cpp:1721`;
   `AnubarakThreatCapMultiplier`, `ToCMultipliers_Anubarak.cpp:98`) against whichever tank holds
   Jaraxxus; hoist it to Shared if it moves cleanly.
6. **Reader**: `--incinerate` reads `jaraxxus.absorb`; `--boss` reopen condition skips deaths with
   Burning Inferno in their last 5 s; new `--cover`: raid cooldowns and personal buttons against each
   Inferno's start.
7. **Docs**: 25H Incinerate numbers, the helper, the reopen condition; Known gaps gain finding 6 and
   the taunt-DR exchange.

Targets on the next 25H pull: at least 6 of 7 Incinerate windows healed, Burning Inferno under
0.5M, no death at a Mistress spawn, carriers within 30 yd of a healer, any Inferno covered within
its first second, 2 deaths or fewer; 25N no worse.

## Owns

- the `Jaraxxus` and `ResistAura` stems, `ToCDefinition_Jaraxxus.cpp`, the new defensive helper;
- `lord-jaraxxus.md`;
- `lord_jaraxxus.py` and its test.

Outside ToC: `src/Bot/PlayerbotAI.cpp` for scope 1 only, listed in the report.

## Investigation

- The trace build (15:37 UTC) carries the 2026-10-17 core merge: upstream #28111 marks every melee
  victim of him and his adds with Hittin' Ya 66326 (10 s), which Legion Flame, Incinerate Flesh, Fel
  Lightning and Spinning Pain Spike skip, and schedules Berserk 64238 at 10 min; #28117 moves ToC to
  boss states. Both are in the boss doc.
- Incinerate: effect 0 absorb (`AuraEffect::GetAmount`), effect 2 one aura-23 tick at its end; the
  heal that empties the absorb removes the aura (`Unit::CalcHealAbsorb`). Burning Inferno is cast by
  the target on every ally on his behalf (Nether Power counts), first tick 1 s after.
- Heroic portal aura 67104/67105 ticks every 6 s (normal 66263/67103 once at 8 s), each tick
  summoning a Mistress and hitting 10 yd round the portal (66264/68404-68406).
- `"spell id"` misses `Anti-Magic Zone` (the name compares the DBC string), so the DK class node and
  any name lookup are dead; the helper goes by id 51052.
- Guide read word for word: raid cooldowns are called for the Volcano (Divine Sacrifice, Aura Mastery
  with Fire Resistance Aura) and the Kiss (Aura Mastery with Concentration Aura); the Incinerate
  target may use a healthstone. No coordinate was proposed, so no navprobe.

## Task list

All done, one implementer (no split: the parts share the Jaraxxus stem and the helper).

1. `PlayerbotAI::UpdateAI`: no full-health heal cancel on a target with
   `SPELL_AURA_SCHOOL_HEAL_ABSORB`. `JaraxxusHealIncinerateTargetAction`: biggest heal a cast first,
   HoTs last and skipped when the bot's own is on the target, kiss rule kept. Probe `jaraxxus.absorb`
   in `IncinerateFleshTargetFor`.
2. `Util/ToCHelpers_Defensive.{h,cpp}`: `ToCPredictedHit`, `GetToCRaidCover` (deal, latch, window),
   `GetToCPersonalDefensive`, `FinishToCRaidCover`, `ToCRaidHitLeadIn`, `ToCRaidCoverClassNodes`;
   probes `toc.cover`, `toc.personal`. Jaraxxus: `GetJaraxxusBurningInferno`,
   `GetJaraxxusPersonalHit`, nodes `raid cover` / `personal defensive`, rules `jaraxxus raid cover
   hold`, `jaraxxus magic button guard`; cover holders mid-cast join the pinned-cast break. ResistAura:
   fire to a paladin with Aura Mastery on Jaraxxus.
3. `GetJaraxxusPortalToClear`, node `portal spawn` → `leave portal`, rule `jaraxxus portal hold`,
   probe `jaraxxus.portal`.
4. `DeriveCarrierSpot`: healer reach in the sweep's accept, `far` branch when nothing clear has it.
5. `JaraxxusThreatCapped`, `JaraxxusThreatCapMultiplier` (definition, Spell and Melee).
6. Reader: `--incinerate` absorb columns, `--boss` reopen line, `--adds` Mistress arrivals, new
   `--cover`; tests `HeroicCover`.
7. Boss doc.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Helper nodes live in the Jaraxxus stem, the logic in `ToCHelpers_Defensive` | Brief: Jaraxxus first user; no root registration edit while `refine-fc` runs | A raid-wide `Defensive` stem: edits `ToCStrategy.cpp`, both root contexts and `ToCDefinitions.h` |
| Covers dealt only once the hit leaves 3+ members under 25%, after average resistance and damage taken modifiers | Algalon's `RaidUnderExplosionLine` minimum; spares 8 min cooldowns on a 12k normal Inferno | Deal on every predicted Inferno |
| Every ready cover goes on one hit, one per bot | Brief: one holder per cooldown; a 25H Inferno is lethal to most casters | Stop dealing once the expected damage clears the floor: needs a model of each cover's cut |
| Window 1.5 s before the first tick to 1 s after for every cover | Aura Mastery and Divine Guardian last 6 s, so a cast in the window covers the 5 s hit; channels' first heal lands after the first tick | AMZ cast at the prediction (it lasts 10 s): wasted when the absorb is healed through in the last 2 s |
| Divine Sacrifice comes off once Divine Guardian is up | The class node's own continuer does the same; under a hit on everyone its 30% redirect drains the paladin | Keep it (Algalon's single hit) |
| Raid cover +8.6 and personal +8.4, above the flame walk; channel covers never to a kissed bot or one on a flame | A carrier's walk holds every tick; an instant goes out on the move | Under the flame walk: a carrier never presses a button |
| Personal floor 25% of max health; lethal fallbacks Dispersion, class immunity, healthstone | Brief; critical heal band is 25 | A flat damage threshold per difficulty |
| Portal hold: melee DPS targeting the portal or inside 12 yd, out to 14 yd, from 1 s before the next tick until each of its Mistresses is on a tank or dead | Core: 10 yd centre-to-centre arrival, nearest-player opener; tick read from the aura's periodic timer; 2 yd park margin | Fixed 5 s from the portal's spawn: misses the heroic portal's later spawns |
| Portal walk at `MOVEMENT_COMBAT` after dropping a booking of that priority | The hold blocks every other mover at that priority, and the flame dodge (`FORCED`) must still beat it | `FORCED`: two forced dodges deadlock (pitfalls) |
| Carrier reach 30 yd to a living healer other than itself, dropped only when nothing clear has it (`far`) | Brief; trace 30-48 yd | Hard reject: a carrier with no reachable spot stops and drops flames under itself |
| Threat cap copied, not hoisted | Anub'arak's latch sits in `AnubarakState` behind his phase gate; hoisting edits four Anub'arak files and Shared while `refine-fc` runs | Hoist now |
| Fire aura on Jaraxxus prefers an Aura Mastery paladin, live or staged, and a kept holder without it gives way | Brief | Only re-rank empty slots: a kept ret holder would keep fire |
| Reopen condition skips deaths with Burning Inferno in their last 5 s, heroic only | Brief; the 25H kill's 3 matches each took 57-70% from Inferno | Weigh Inferno's share instead |
| No healthstone for the Incinerate target, no Volcano or Kiss cover | Out of the brief's scope; logged as gaps | Plug the Volcano into the helper |

## Carried over

None. For the merge stage: README's ResistAura paragraph should say fire goes to an Aura Mastery
paladin on Jaraxxus first; README "Instance script" (no boss state, `IsEncounterInProgress` private)
and `ToCEncounterGate.h`'s `TOC_DATA_ANUBARAK = 13` predate #28117, which removed `TYPE_JARAXXUS`,
`TYPE_VALKYR` and `TYPE_NORTHREND_BEASTS_ALL`: Anub'arak's guid slot is now 11, so
`GetGuidData(13)` answers empty (Anub'arak lane / `w6-closeout`). w6: hoist the Jaraxxus and
Anub'arak threat caps and `IsAreaDamage` into Shared.

## Known gaps

In the boss doc: no Volcano or Kiss cover; class nodes can spend a cover before a predicted hit;
holder late or lost at its window; no healthstone on the Incinerate target; assist tank 0 not at the
portal, held melee idle; taunt diminishing returns between two tanks; adds on non-tanks under a human
assist tank (by decision); capped melee white-swing; `toc.*` probes silent on the other four bosses.

## Blocked

None.
