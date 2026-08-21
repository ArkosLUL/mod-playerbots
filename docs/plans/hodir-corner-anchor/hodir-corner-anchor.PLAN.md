# Hodir: corner-anchored positioning and a cheat-free fight

## Context

Bots kill Hodir, but nothing in the strategy positions anyone. There is no tank spot, no raid anchor,
no multiplier, no target priority — the four Hodir files are five reactive object-seeks. Three of the
five nodes are gated behind `AiPlayerbot.UlduarHodirHardMode` (default 0), so on a default server the
fight is: shelter from Flash Freeze, strip Biting Cold with the raid cheat, buff frost resistance.
Nothing else.

Every Hodir guide says to tank him **in a corner**, because it consolidates the friendly NPCs and
their buff zones into one place instead of scattering them across a 76 × 130 yd hall. That is the
change this plan makes, plus the survival gaps a fixed anchor would otherwise make *worse*.

Three mechanics force the shape of everything else:

- **Biting Cold stacks every 4 s on anyone standing still** (`62038` ticks 1 s, acts on `% 4`, skips
  `isMoving()` and anyone in a Toasty Fire), for `200 · 2^stacks`. Parking bots on dots with no
  answer to that is strictly worse than the status quo.
- **A small icicle lands every 2 s** on a random player and detonates for **14,000 Frost in 4 yd**
  after a 2 s telegraph. Unhandled today.
- **Starlight is +50% haste to casts *and* swings** — the largest single throughput lever in the
  fight, currently unused. It is an 8 yd zone at the druid helper's feet, and the druid only holds a
  predictable spot if Hodir does.

Shape follows the merged Auriaya work
(`docs/plans/auriaya-soak-and-anchor/auriaya-soak-and-anchor.PLAN.md`) plus four Sunwell patterns
that are shared code, not SWP-local.

## Established facts

Verified against the core script, the world DB, the DBC extract and `navprobe` — do not re-derive.

### Room and boss

| Fact | Source |
|---|---|
| Hodir spawns at **`(2000.85, -204.335, 432.758)`, `o = 4.71239`**, entry 32845, map 603 | `acore_world`.`creature` |
| Boss **evades outside `y ∈ (-297.793, -166.259)`** — a hard leash any anchor must respect | `boss_hodir.cpp:229-230,379-383` |
| **Room floor `x ∈ [1965, 2041]` at mid-length, `y` from ~-170 past -298; floor Z 432.687, mesh surface 432.827.** ~76 × 130 yd, long axis north-south | navprobe sweeps |
| **The SW corner is chamfered** — a 45° bevel from ~`(1966, -274)` to ~`(1990, -298)`. There is no square corner to back into | navprobe grid |
| Client data is **not** in `env/dist/data` on the host — it is in the `ac-client-data` Docker volume, and `navprobe` is prebuilt in `acore/ac-wotlk-build:master` | `docker-compose.override.yml:85-94` |
| Helper spawn grid `x 1976.41–2021.12`, `y -230.50…-243.46`; chests `(1967.15, -204.19)` and `(2035.95, -202.08)` | `boss_hodir.cpp:182-210`; `2026_08_08_09.sql:34-37` |
| Berserk at **8 min**; Hodir never dies — at **< 150,000 HP** the encounter is set DONE | `boss_hodir.cpp:268,333-374` |

### Mechanics

| Fact | Source |
|---|---|
| **Flash Freeze `61968` is a 9-second cast** (`CastingTimeIndex 204`), 200 yd, every **48–49 s** | `spellcasttimes.reference.csv:204` |
| It spares **only** units carrying `SPELL_SAFE_AREA_TRIGGERED (62464)`, plus pets | `boss_hodir.cpp:1339-1366` |
| **The shelter chain**: drift icicles force-cast on 2 (10m) / 3 (25m) players → NPC **33173** spawns → falls after 2 s → `62460` triggers `65370` (Ice Shards **7 yd**) **and** `62463 Snowdrift`, whose effects are `TRANS_DOOR` GO `194173` + `SUMMON` creature **33174**, duration **12 s**. 33174 carries `65705` → `62464`, **radius 9 yd** | `spell.reference.csv`; `creature_template_addon` 33174; `spellradius.reference.csv:40` |
| **Timeline per cycle**: T+0 icicle spawns, 9 s cast starts → T+2 it lands, shelter appears → T+9 freeze lands → T+14 shelter expires. **7 s of shelter before the freeze** | derived from the above |
| **Small icicles**: `62227` ticks every 2 s, `63545` picks 1 random non-frozen player, NPC **33169** spawns on them, falls after 2 s, `62236` triggers **`62457` = 14,000 Frost in 4 yd** + knockback. **No Snowdrift** — small icicles never become shelters | `npc_ulduar_icicle:594-624`; `spellradius.reference.csv:26` |
| Both icicle NPCs carry `UNIT_FLAG_NOT_SELECTABLE` — findable only through `"nearest npcs"` / `FindNearestCreature`, never `"possible targets"` | `creature_template` `unit_flags 33587200` |
| Ice Shards is **schoolMask 16 = Frost**, so the existing frost-resistance node mitigates it | `spell.reference.csv:226` |
| Small icicles are switched **off** at each Flash Freeze and back on after **12 s (25m) / 24 s (10m)** | `boss_hodir.cpp:420-422` |
| **Flash Freeze on a player lasts 300 s** (`61969`) and the **next** Flash Freeze instakills anyone still trapped (`62226`) | `spellduration.reference.csv:5`; `boss_hodir.cpp:1386-1397` |
| Trapped players are freed by killing **NPC 32926**, helpers by **32938**; both are `unit_flags 0x20004` — attackable and selectable | `creature_template` |
| **Frozen Blows** `62478`/`63512`, **20 s**, **15 s after each Flash Freeze**; +31,061 (10m) / +39,999 (25m) per swing plus a 3,999 raid-wide tick | `spellduration.reference.csv:18` |
| **Freeze `62469`**: random player within 50 yd every 17–20 s, 5,549 + root in 10 yd, **DispelType 1 (Magic)** | `boss_hodir.cpp:439-449` |
| **Starlight `62807`: 8 yd, 60 s**, recast every 15 s while `z < 433`, a persistent ground zone at the druid's feet. Aura **193 `SPELL_AURA_MELEE_SLOW`** → `HandleModCombatSpeedPct`, which calls `ApplyCastTimePercentMod` **and** all three attack times. Amount **50**, and `ApplyCastTimePercentMod` divides for positive values → **+50% haste, casting included** | `spellradius.reference.csv:14`; `SpellAuraEffects.cpp:4779-4801`; `Unit.cpp:13532-13542` |
| **Toasty Fire `62821` is 11 yd, 60 s**, at the mage's feet every 10 s. Blocks Biting Cold and procs Singed. It does **not** grant Flash Freeze exemption | `spellradius.reference.csv:42`; `boss_hodir.cpp:1209-1233` |
| **Flash Freeze wipes every Toasty Fire** (`SpellHitTarget` on `62148`) | `boss_hodir.cpp:322-329` |
| **Storm Power `63711/65134`: radius 3 yd**, +134% crit damage, 30 s | `spellradius.reference.csv:15` |
| **Storm Cloud: 4 stacks (10m `65123`) / 6 (25m `65133`)**, ticks 1 s, one stack per tick — the carrier is useful for **4–6 seconds**. Cast on a random player within 35 yd of the shaman every 30 s | `CumulativeAura`; `boss_hodir.cpp:999-1006` |
| Helper stand-off: priest **17**, druid **22**, shaman **25**, mage **30** yd. So Starlight and the fires sit **~8 yd apart radially** | `boss_hodir.cpp:710,827,944,1068` |
| **`MOVEMENTFLAG_FALLING` is inside `MOVEMENTFLAG_MASK_MOVING`** — a jump counts as moving for Biting Cold | `UnitDefines.h:405-408` |

### The packing arithmetic

Derived, and load-bearing for three decisions:

| Ring radius (16 ranged+healers) | Slot spacing | Bots inside a 4 yd splash | Raid damage / 2 s | Sustained HPS |
|---|---|---|---|---|
| `r = 5` (the largest that fits Starlight) | 1.95 yd | **5** | 70,000 | **35,000** |
| `r = 7` | 2.73 yd | 3 | 42,000 | 21,000 |
| target only | ≥ 4 yd apart | 1 | 14,000 | 7,000 |

**16 bots cannot be 4 yd apart inside an 8 yd circle** — that packing needs ~200 yd² and the circle
is 201 yd². So inside Starlight, splash is structural and heal-through is a wipe, not a trade.

For every slot to be inside *both* zones, with `|druid − fire| = 8`, radius `r`, tolerance `t`:
`8 + 2(r + t) ≤ 19` → `r + t ≤ 5.5` → at `t = 3`, `r ≤ 2.5`. A 2.5 yd ring is a stack. **Both auras
for everyone is unreachable**; the choice is which one anchors.

### Engine

| Fact | Source |
|---|---|
| `MovementAction::JumpTo(mapId, x, y, z, priority)` exists and handles `IsMovingAllowed` / `IsWaitingForLastMove` / `last movement`. It rejects via `IsDuplicateMove`, so a repeated in-place jump is refused | `MovementActions.cpp:62-80` |
| `Unit::GetDynObject(spellId)` returns a persistent-area-aura object; module precedent at `DruidTriggers.cpp:101` | `Unit.h:1693` |
| `IsMechanicTrackerBot(Player*, mapId)` and `GetNearestPlayerInRadius(Player*, radius)` are **shared** raid helpers | `RaidBossHelpers.h:34,36` |
| `Engine::DoNextAction` breaks on the first action returning `true` — node priority starves, it does not merely order | `Engine.cpp:219-225` |
| `DpsTargetValue::Calculate` is **never null**, so `dps assist` retakes the target on alternating ticks unless a multiplier zeroes `DpsAssistAction` | `docs/raids/ulduar.md` (Kologarn) |
| `IsRanged()` includes healers; `IsRangedDps()` does not | `PlayerbotAI.cpp:1905`; `PlayerbotAI.h:460` |
| `AttackAction : MovementAction`; `ReachTargetAction` is one, `CastReachTargetSpellAction` is not | `docs/engine/pitfalls.md` |
| Auriaya's anchor is gated on `GetAuriaya() != nullptr`, **not** on combat; no Ulduar trigger uses a pull-window idiom | `UldBossHelper.cpp:429` |
| `GetCreatureListWithEntryInGrid` applies **no** alive/LOS filter | `Object.cpp:2614-2619` |

Difficulty-mapped Hodir spells are only three: `62478→63512`, `63711→65134`, `65123→65133`. The
heroic icicle force-cast is picked explicitly with `RAID_MODE(62476, 62477)`. Everything else shares
one id, so heroic is free provided those three go through `sSpellMgr->GetSpellIdForDifficulty`.

## Analysis: guide vs. what we ship

| Guide instruction | Current behaviour | Verdict |
|---|---|---|
| Tank in a corner; consolidate the helpers and their zones | No positioning of any kind | ❌ |
| Keep moving / jump to shed Biting Cold | `RemoveAurasDueToSpell` behind `HasCheat(raid)`; the real jump is commented out. Its trigger also demands a live `GetMaster()` **without** 2 stacks, so it is inert in an all-bot raid | ❌ |
| Move out from under a falling icicle | Nothing. 14,000 every 2 s with a 2 s telegraph, ignored | ❌ |
| Shelter behind a Snowpacked Icicle during Flash Freeze | Correct NPC (33174), and a 7 s window to use it | ✅ |
| Free flash-frozen **players** before the next Flash Freeze instakills them | Only helper blocks (32938); player blocks (32926) are invisible to the strategy | ❌ wipe-grade |
| Tank swap on Frozen Blows | Nothing | ❌ |
| **Stand in Starlight** | Not implemented — and it is +50% haste to everything | ❌ largest miss |
| Free the helpers and keep all 8 alive | Implemented, gated behind the hard-mode config | ⚠️ |
| Stand in a Toasty Fire | Gated behind config; radius set to 5 where the aura is 11 | ⚠️ |
| Storm Cloud carrier joins the pack | Implemented, but "the pack" is measured at 10 yd where Storm Power is 3 | ⚠️ wrong |
| Berserk 8 min; the Freeze root is dispellable | Nothing | ❌ |

Two documentation errors to fix while here: `docs/raids/ulduar.md:271-273` and the comment on
`ULDUAR_HODIR_TOASTY_FIRE_RADIUS` both claim Toasty Fire grants Flash-Freeze exemption. It does not.

## Design decisions

Settled with the user across five grilling rounds. Reasoning kept where it is not obvious.

### Anchoring

1. **Tank in the SW corner at `(1974.50, -275.50)`.** The user measured `(1971.995, -277.590)`;
   navprobe puts that on the bevel edge, with three of its eight 5 yd neighbours settling to terrain.
   The spot is nudged 3.9 yd inward to the deepest point with a full 6 yd of floor all round — still
   the corner, but it survives a knockback, and Ice Shards carries one.
2. **The off-tank sits deeper into the corner, not toward the raid**, so a taunt never walks Hodir at
   the stack.
3. **The raid anchor is a *fallback*; the live ring centre is the Starlight zone.** The fixed anchor
   is 22 yd out along the corner→room-centre bearing, which is also the druid's stand-off distance,
   so the two normally coincide within a few yards.
4. **The anchor is ungated on combat**, like Auriaya's. Hodir's `MoveInLineOfSight` is a no-op so he
   does not aggro on approach, which means bots pre-position in the corner and the main tank pulls
   *from* there instead of dragging him 75 yd. A combat gate would leave the strategy inert through
   the approach and the instant of the pull, and generic targeting would decide where he gets tanked.

### Which buff anchors the ring

5. **Starlight-primary. Toasty Fire is demoted to a tiebreak.** Both-for-everyone is unreachable (see
   the packing arithmetic). The fire's entire value is that it saves the jump, and the jump costs
   ~25% of cast uptime; Starlight is +50% haste to casts *and* swings. 1.5× throughput beats a ~25
   point uptime difference. **This inverts what an earlier draft of this plan assumed.**
6. **`r = 5`, tolerance 3.** `r` is capped at `8 − tolerance`. Tightening the tolerance to buy radius
   gains nothing measurable (still 5 bots per splash until `r ≥ 7`, which leaves the zone) and a tight
   tolerance is what makes bots slide in place instead of casting.
7. **No offset toward the fire.** Centred on Starlight, slots sit 3–13 yd from the fire, so roughly
   half the ring gets it incidentally. Shifting the centre 3 yd would bring the whole ring inside the
   fire but spend Starlight's entire margin — the wrong trade for a Biting Cold answer the jump
   already provides.
8. **Locate the zone with `GetHodirDruidHelper()->GetDynObject(62807)`, re-latch when the bot's own
   `HasAura(62807)` goes false.** Up to four zones overlap (60 s duration, 15 s recast) and
   `GetDynObject` returns only the first; nearest-to-the-fixed-anchor picks the right *place*, and the
   aura dropping is a free, exact expiry signal that needs no duration bookkeeping.
9. **Slot index is derived, never communicated.** Rank the live ranged+healer set by guid; every bot
   computes the same order. No shared state, self-corrects on death.

### Damage the raid takes

10. **Icicles are dodged, not out-spread** — 16 bots cannot be 4 yd apart inside an 8 yd circle, and
    heal-through is 21–35k sustained HPS. **But the dodge stays inside Starlight**: a bot at `r = 5`
    moving 6 yd tangentially traces a 74° chord and ends at `r = 5` again, still inside the zone, and
    moving inward through the centre also stays inside. Constrain the candidate set to in-zone points
    and prefer them; leave the zone only when nothing inside clears.
11. **Accept ~26% dodge churn.** Five bots move per icicle, one lands every 2 s, so each bot moves
    roughly every 6.4 s for ~1.7 s. This is the price of Starlight and it is written down so nobody
    later "optimises" the ring wider and quietly drops the buff. It is not pure loss — it doubles as
    the Biting Cold answer, so ringed bots barely need the jump.
12. **Biting Cold: Toasty Fire where it happens, jump everywhere else.** The jump keeps its own node
    even though ranged rarely need it, because small icicles are **off for 12–24 s after every Flash
    Freeze** — exactly when nothing else makes anyone move, and exactly when the fires have just been
    wiped. Tanks need it all fight. The raid cheat path is deleted.
13. **The jump is a 2 yd hop, alternating.** An in-place jump is refused by `JumpTo`'s
    `IsDuplicateMove`, and 2 yd stays inside every tolerance so the hop never triggers a re-anchor.
14. **Dodge scope: 33169 always; 33173 only until it lands.** The drift icicle you dodge and the
    shelter you run to are **different NPCs** — 33173 falls and summons 33174. So the 33173 dodge
    stands down once a 33174 exists within 9 yd of it: a bot steps off the falling drift, then walks
    back into what it leaves behind, with no clock and no conflicting nodes. This is the trap most
    likely to be "simplified" away by a later reader.
15. **The shelter run keys off 33174 existing, not off the falling icicle.** Starting when the icicle
    spawns puts the raid underneath a 14,000 / 7 yd detonation. Waiting costs 2 s and leaves 7 s to
    cross the room at 7 yd/s. One predicate instead of two, and it cannot arrive early by
    construction.
16. **The shelter run targets the drift nearest the *raid anchor*, not nearest the bot.** One drift
    holds 25 bots inside 9 yd; converging keeps the formation coherent for the re-form afterwards.
17. **The anchor is abandoned every 48 s and that is fine** — ~11 s of disruption per cycle. Do not
    try to hold the tank through Flash Freeze; he is encased and the raid loses its tank.
18. **Frozen Blows gets a real tank swap.** 40k per swing at 25-man is a tank death, not a healing
    problem, and with `UldCastClassTaunt` and two anchored spots the swap is nearly free.

### Buffs and targeting

19. **The Storm Cloud carrier runs the ring arc.** The buff is worth more than one bot's damage, so
    the carrier tours the raid rather than stepping to one neighbour. At `r = 5` the circumference is
    31.4 yd ≈ 4.5 s at run speed, against a 4-tick (10m) / 6-tick (25m) budget — close to one full
    lap. Enter at the bot's own slot, run whichever direction has more un-buffed eligible allies
    ahead, stop when the aura drops. **Never greedy re-targeting** — that is the shape that produced
    the Auriaya corridor dance.
20. **Storm Power is for DPS and healers; tanks are skipped, and tanks never run the tour.** A tank
    leaving the corner mid-Frozen-Blows is worse than a lost buff. **Melee carriers do run** — ~3 s of
    a 4–6 s budget to reach the ring, to hand ~15 bots +134% crit damage.
21. **One priority action owns targeting, and a multiplier stands the generic picker down.** The two
    ice-block actions are raw `Attack()` calls today with nothing suppressing `dps assist` — the
    Kologarn defect. Fold them into `HodirSetDpsPriorityAction` with the Muru machinery. Order:
    **trapped player (32926) → helper block (32938) → Hodir**.
22. **Healers are excluded from the priority action entirely**, unlike Auriaya's tanks-only rule. A
    25-man raid eating 14,000 every 2 s while its healers DPS an ice block is a different wipe.
23. **Only the nearest 5 non-healers break a block**, by guid rank among those in range. Twenty bots
    on one low-HP block is a lot of lost boss damage for a one-second job. Derive-don't-communicate,
    same rule as the ring slots.
24. **`neglect threat` while the target is Hodir**, the way Muru sets it. No threat wipe in this
    fight, and it is a DPS race.
25. **No raid icons.** Bots set none; `attack rti target` stays registered so a human's mark wins.

### Scope

26. **The three hard-mode-gated nodes are ungated, and the config key is deleted.** The helpers *are*
    the raid's damage, the fire is a Biting Cold answer, and Storm Power is the biggest damage buff in
    the fight — all correct on every pull. `IsHodirHardModeActive` then has no callers. A key that
    gates nothing is worse than no key; the other seven `Ulduar*HardMode` options still gate real
    behaviour. Removing a key from `playerbots.conf.dist` is safe — an unknown key in an operator's
    own `.conf` is ignored.
27. **Melee get no anchor, no Toasty Fire and no Starlight.** They stand in Hodir's melee range at the
    corner, ~22 yd from the druid. This was re-examined once Starlight turned out to be +50% *melee*
    haste too and confirmed: the only fix is dragging Hodir toward the druid, which walks him into the
    raid and costs the corner the whole plan is built on. Record it as measured and accepted, not
    overlooked.
28. **`MOVEMENT_COMBAT`, no teleport branch.** All existing Hodir moves use `MOVEMENT_NORMAL`.
29. **Resolve the boss by entry, never `"find target"`** — a bot attacking an ice block otherwise
    silently loses its Flash Freeze shelter. Exactly what Auriaya was migrated away from.
30. **Berserk stays unhandled.** No enrage awareness exists anywhere in the module.
31. **No per-instance state, so no reset action.** Both latches (Starlight zone, adopted fire) are
    re-validated on every read, so they self-heal across pulls. `IsMechanicTrackerBot` is not needed.

### Sunwell patterns to reuse rather than re-derive

32. **Ring slots** — `GetMuruRangedSpreadPosition` (`SWPActions_Muru.cpp:118-176`): guid-ranked
    members, `anchorAngle + 2π · slot / count`, then `GetMapWaterOrGroundLevel` **and**
    `Map::CheckCollisionAndGetValidCoords`. Raw ring geometry is the off-mesh shape `MoveTo` rejects
    silently, so the validation is mandatory.
33. **Reach-then-hold hysteresis** — an `_anchorReached` latch set on arrival and cleared past
    `2 × tolerance` (`SWPActions_Muru.cpp:91-108`, `SWPActions.h:76-85`), paired with `MoveInside`.
    Without it, ringed bots slide in place and never cast — the Sapphiron air-phase bug.
34. **De-clump tail** — `GetNearestPlayerInRadius(bot, 4.0f)` → `FleePosition(..., 1000ms)`
    (`SWPActions_Muru.cpp:110-114`). Needs no formation, and gives melee some separation for free.
35. **Targeting** — the `MuruSetDpsPriorityAction` shape (`SWPActions_Muru.cpp:178-393`) in full.

## Geometry

Every value navmesh-verified with `navprobe`, not derived on paper.

| Constant | Value | Clearance |
|---|---|---|
| `ULDUAR_HODIR_MAINTANK_SPOT` | `(1974.50f, -275.50f, 432.687f)` | 8/8 floor at 6 yd; 2 of 8 off at 8 yd, toward the bevel |
| `ULDUAR_HODIR_OFFTANK_SPOT` | `(1980.00f, -277.00f, 432.687f)` | 8/8 floor at 6 yd; 5.70 yd from the tank spot |
| `ULDUAR_HODIR_RAID_ANCHOR` | `(1986.56f, -257.11f, 432.687f)` | 8/8 floor at 11 **and** 15 yd; 22.0 yd from the tank spot, `PATHFIND_NORMAL` between them |

## Files to change

### 1. `src/Ai/Raid/Uld/Util/UldBossHelper.h` / `.cpp`

**Add ids** beside the existing Hodir block (`.h:60-67`):

```cpp
NPC_HODIR_FLASH_FREEZE_PLAYER = 32926,   // ice block encasing a trapped raider
NPC_HODIR_ICICLE_SMALL        = 33169,   // 14,000 in 4 yd, lands every 2s, NOT_SELECTABLE
NPC_HODIR_ICICLE_DRIFT        = 33173,   // Flash Freeze drift; becomes 33174 when it lands
// Druid helpers - Starlight is an 8 yd zone at whichever one this raid got.
NPC_HODIR_DRUID_ALLIANCE_10 = 32901, NPC_HODIR_DRUID_ALLIANCE_25 = 33325,
NPC_HODIR_DRUID_HORDE_10    = 32941, NPC_HODIR_DRUID_HORDE_25    = 33333,
SPELL_HODIR_STARLIGHT            = 62807,
SPELL_HODIR_TOASTY_FIRE_AURA     = 62821,
SPELL_HODIR_FROZEN_BLOWS         = 62478,  // base id, difficulty-mapped at runtime
SPELL_HODIR_FLASH_FREEZE_TRAPPED = 61969,
```

`NPC_HODIR` (32845) comes from core `ulduar.h` through `UldScripts.h` — check before redeclaring,
the way `NPC_XT002` is handled.

**Replace the two existing tunables** and add the rest:

```cpp
constexpr float ULDUAR_HODIR_STARLIGHT_RADIUS        =  8.0f;  // 62807, EffectRadiusIndex 14
constexpr float ULDUAR_HODIR_TOASTY_FIRE_RADIUS      = 11.0f;  // 62821, EffectRadiusIndex 42
constexpr float ULDUAR_HODIR_STORM_CLOUD_STACK_RADIUS =  3.0f; // 63711/65134, EffectRadiusIndex 15
constexpr float ULDUAR_HODIR_SAFE_AREA_RADIUS        =  9.0f;  // 62464 off NPC 33174
constexpr float ULDUAR_HODIR_SAFE_AREA_TOLERANCE     =  6.0f;  // park inside the 9 yd with margin
constexpr float ULDUAR_HODIR_ICE_SHARDS_RADIUS       =  4.0f;  // 62457, EffectRadiusIndex 26
constexpr float ULDUAR_HODIR_ICE_SHARDS_CLEAR        =  6.0f;  // step past the edge, not onto it
constexpr float ULDUAR_HODIR_RAID_RING_RADIUS        =  5.0f;  // 8 - 3 tolerance; see the packing note
constexpr float ULDUAR_HODIR_ZONE_ADOPT_RADIUS       = 15.0f;  // from the fixed anchor
constexpr float ULDUAR_HODIR_MAINTANK_SPOT_TOLERANCE =  3.0f;
constexpr float ULDUAR_HODIR_RING_SPOT_TOLERANCE     =  3.0f;
constexpr float ULDUAR_HODIR_DODGE_LEASH             = 10.0f;
constexpr float ULDUAR_HODIR_DECLUMP_RADIUS          =  4.0f;
constexpr float ULDUAR_HODIR_JUMP_HOP                =  2.0f;
constexpr uint32 ULDUAR_HODIR_JUMP_IDLE_MS           = 3000;   // 62038 stacks on the 4th 1 s tick
constexpr float ULDUAR_HODIR_TRAPPED_ALLY_RANGE      = 45.0f;
constexpr int   ULDUAR_HODIR_TRAPPED_ALLY_BREAKERS   = 5;
```

Comment the ring radius with the packing arithmetic — it is the one number a later reader will want
to raise, and raising it silently drops Starlight.

**Add the three `Position` constants** (extern in the header, defined once in the `.cpp`), recording
that they are navprobe-verified and that the boss evade band is `y ∈ (-297.793, -166.259)`.

**Add helpers:**

- `Unit* GetHodir(PlayerbotAI*)` — `GetFirstAliveUnitByEntry`, the shape of `GetAuriaya`
  (`UldBossHelper.cpp:427`).
- `Creature* GetHodirDruidHelper(PlayerbotAI*)` — first alive of the four druid entries.
- `bool GetHodirRingCentre(PlayerbotAI*, Position& out)` — the latched Starlight zone. Read
  `GetHodirDruidHelper()->GetDynObject(SPELL_HODIR_STARLIGHT)`; keep the latched position while the
  bot still has `62807` **and** the point is within `ZONE_ADOPT_RADIUS` of `ULDUAR_HODIR_RAID_ANCHOR`;
  otherwise re-latch to the qualifying zone nearest the anchor. No zone → the fixed anchor. Latch is
  `thread_local unordered_map<uint32 /*instanceId*/, Position>`.
- `bool GetHodirAnchor(PlayerbotAI*, Player*, Position& out, float& tolerance)` — **the single source
  of truth, called by trigger and action alike**:
  - main tank → `ULDUAR_HODIR_MAINTANK_SPOT`, MT tolerance;
  - assist tank 0 → `ULDUAR_HODIR_OFFTANK_SPOT`, MT tolerance;
  - `IsRanged` → ring slot at `RAID_RING_RADIUS` around `GetHodirRingCentre()`, ring tolerance;
  - melee → `false`.
- `bool GetHodirRingSlotPosition(PlayerbotAI*, Player*, Position const& centre, Position& out)` — a
  direct copy of `GetMuruRangedSpreadPosition` (`SWPActions_Muru.cpp:118-176`), including the
  `GetMapWaterOrGroundLevel` + `CheckCollisionAndGetValidCoords` validation. `anchorAngle` is the
  centre→tank-spot bearing, so the ring keeps a stable rotation as the centre drifts.
- `bool IsHodirTrappedAllyBreaker(PlayerbotAI*, Player*, Unit* block)` — true when the bot is among
  the nearest `TRAPPED_ALLY_BREAKERS` non-healers in range, ranked by distance then guid.

### 2. `src/Ai/Raid/Uld/Trigger/UldTriggers_Hodir.h` / `.cpp`

**Rewrite `HodirBitingColdTrigger`** — drop the `GetMaster()` gating entirely. Fires when Hodir is up,
the bot has no `SPELL_HODIR_TOASTY_FIRE_AURA`, is not trapped, is not casting, and either carries
`62039` or has not moved for `ULDUAR_HODIR_JUMP_IDLE_MS`. Hold the idle timestamp on the trigger.
Comment that ranged rarely reach it because dodge churn already moves them — the node exists for the
post-Flash-Freeze lull and for tanks.

**Rewrite `HodirNearSnowpackedIcicleTrigger`** — resolve the boss through `GetHodir`; require a live
**33174** (not merely that the boss is casting), and pick the one nearest `ULDUAR_HODIR_RAID_ANCHOR`;
compare against `ULDUAR_HODIR_SAFE_AREA_TOLERANCE`.

**Ungate `HodirSpreadStormCloudTrigger`.** Its `nearby < 2` test now runs against the corrected 3 yd
radius. Add: false for tanks.

**Delete** `HodirMoveToToastyFireTrigger` (folded into the anchor) and `HodirFreeFrozenHelperTrigger`
(folded into the priority action).

**Add:**
- `HodirIcicleDodgeTrigger` — a live **33169** within `ICE_SHARDS_CLEAR`, or a live **33173** within
  `ICE_SHARDS_CLEAR` that has **no 33174 within `SAFE_AREA_RADIUS`**. Found through `"nearest npcs"`.
- `HodirRaidPositionTrigger` — Hodir up; **false** when the shelter or dodge trigger would fire, or
  the bot is trapped; then `GetHodirAnchor` and a distance-vs-tolerance check.
- `HodirSetDpsPriorityTrigger` — Hodir up, `!botAI->IsTank(bot)` **and** `!botAI->IsHeal(bot)`.
- `HodirFrozenBlowsSwapTrigger` — Hodir carries `GetSpellIdForDifficulty(62478)`, the bot is assist
  tank 0, Hodir's victim is not the bot. The reverse case puts the main tank back on.

### 3. `src/Ai/Raid/Uld/Action/UldActions_Hodir.h` / `.cpp`

**Rewrite `HodirBitingColdJumpAction`** — delete the cheat and the commented-out block. Hop between
the anchor and `anchor + JUMP_HOP` along a per-bot bearing, whichever is further, via
`MovementAction::JumpTo(..., MOVEMENT_COMBAT)`. No `isUseful` cheat gate.

**Rewrite `HodirMoveSnowpackedIcicleAction`** — instantiate the trigger instead of duplicating its
body, and use `MOVEMENT_COMBAT`.

**Delete** `HodirMoveToToastyFireAction` and `HodirFreeFrozenHelperAction`.

**Add `HodirIcicleDodgeAction : MovementAction`** — `"hodir icicle dodge action"`. Sweep 8 directions
× 2 yd out to `DODGE_LEASH` from the bot's current position. A candidate is valid when it clears
`ICE_SHARDS_CLEAR` of every nearby icicle, is within the leash of the bot's anchor, and passes
`IsWithinLOS`. **Score in two tiers: candidates still inside `STARLIGHT_RADIUS` of the ring centre
first, then everything else; within a tier, smallest displacement.** A bot at `r = 5` always has an
in-zone escape — a 6 yd tangential step traces a 74° chord and lands back at `r = 5`. No valid
candidate → `FleePosition(nearestIcicle, ICE_SHARDS_CLEAR, 500ms)`.

**Add `HodirRaidPositionAction : MovementAction`** — `"hodir raid position action"`:
- Resolve `GetHodirAnchor`. No anchor (melee) → fall through to the de-clump tail.
- **Hysteresis latch** `_anchorReached`, the `MuruPositionRangedAction` shape: set on arrival,
  `return false` so the rotation runs, cleared only past `2 × tolerance` or when Hodir is gone.
- Not latched → `MoveInside(603, x, y, z, tolerance, MOVEMENT_COMBAT)`. Never with `distance = 0`.
- **De-clump tail**: `if (Player* near = GetNearestPlayerInRadius(bot, ULDUAR_HODIR_DECLUMP_RADIUS))
  return FleePosition(near->GetPosition(), ULDUAR_HODIR_DECLUMP_RADIUS, 1000ms);`

**Add `HodirSetDpsPriorityAction : AttackAction`** — `"hodir set dps priority action"`, the
`MuruSetDpsPriorityAction` shape exactly: one pass over `"nearest npcs"`, an `isAllowedPriorityTarget`
lambda, sticky-per-entry with a 10 yd switch margin measured **to the bot** (blocks appear wherever a
raider stood), sticky-across-entries on `<=` priority index, a `needsAttack` guard that also tests
`UNIT_STATE_MELEE_ATTACKING` for melee, and `AI_VALUE(Unit*, "dps target")` as fallback. Set
`neglect threat` when the resolved target is Hodir.

Priority **32926 → 32938 → Hodir**, with `isAllowedPriorityTarget`:
- **32926** — inside `TRAPPED_ALLY_RANGE` and `IsHodirTrappedAllyBreaker` is true for this bot;
- **32938** — inside `TRAPPED_ALLY_RANGE`, and never while a 32926 is up;
- **Hodir** — always; he is the tail.

**Rewrite `HodirSpreadStormCloudAction`** — replace the nearest-ally move with the **arc run**. Enter
at the bot's own ring slot; pick the direction with more un-buffed eligible allies ahead (DPS and
healers only — tanks are skipped); step to the next slot position each tick; stop when the Storm
Cloud aura drops. A melee or tank carrier resolves its entry slot as the ring point nearest itself;
**tanks return `false` immediately** rather than leaving the corner.

**Add `HodirFrozenBlowsSwapAction : AttackAction`** — `UldCastClassTaunt(botAI, GetHodir(botAI))`, the
call `AuriayaSentryTauntAction` already makes.

### 4. Wiring — `UldActionContext.h`, `UldTriggerContext.h`, `UldStrategy.cpp`

Register four triggers and four actions; delete the toasty-fire and helper-freeing pairs. Replace the
block at `UldStrategy.cpp:283-312`:

```cpp
"hodir near snowpacked icicle"    -> "hodir move snowpacked icicle"     ACTION_RAID + 5
"hodir icicle dodge"              -> "hodir icicle dodge action"        ACTION_RAID + 4
"hodir frozen blows swap"         -> "hodir frozen blows swap action"   ACTION_RAID + 3
"hodir spread storm cloud"        -> "hodir spread storm cloud"         ACTION_RAID + 2
"hodir biting cold"               -> "hodir biting cold jump"           ACTION_RAID + 1
"hodir set dps priority"          -> "hodir set dps priority action"    ACTION_RAID + 1
"hodir frost resistance trigger"  -> "hodir frost resistance action"    ACTION_RAID
"hodir raid position"             -> "hodir raid position action"       ACTION_RAID
```

Rewrite the block comment: the shelter takes the top because it is the only node whose failure is an
outright death; the dodge is next at 14,000 every 2 s; the jump is listed before the targeting node so
a bot parked on an ice block still sheds Biting Cold; position is last because the engine breaks on
the first successful action.

### 5. `src/Ai/Raid/Uld/UldMultipliers.h` / `.cpp`

**Add `HodirGuardMultiplier`**, modelled on `AuriayaMovementGuardMultiplier`
(`UldMultipliers.cpp:521-550`) and `SWPMultipliers.cpp:712`, gated on `GetHodir(botAI)`:

- **Targeting** — for non-tank non-healers, zero `dynamic_cast<DpsAssistAction*>(action)` so the
  priority action owns `current target`. Without this it is pointless: `DpsTargetValue` is never null,
  so `dps assist` retakes the target every other tick and bots drift off the ice blocks. Comment that
  `attack rti target` is deliberately **not** zeroed.
- **Movement** — melee and anyone with no anchor keep every generic mover. Main tank, assist tank 0
  and `IsRanged` → zero `dynamic_cast<MovementAction*>(action)` unless it is also an `AttackAction` or
  a `ReachTargetAction`, or its name is in `{"hodir raid position action",
  "hodir move snowpacked icicle", "hodir icicle dodge action", "hodir biting cold jump",
  "hodir spread storm cloud"}`.
- Keep the `CastHealingSpellAction` escape hatch.
- Split on action family with one `dynamic_cast` each way up front — this runs once per queued action
  per bot per tick.

Register beside the Auriaya multiplier at `UldStrategy.cpp:642`.

### 6. Hard-mode removal

- `UldHardMode.h:58-62` / `UldHardMode.cpp:85` — delete `IsHodirHardModeActive`.
- `PlayerbotAIConfig.h:282`, `PlayerbotAIConfig.cpp:728` — delete `ulduarHodirHardMode`.
- `conf/playerbots.conf.dist:415-423` — delete the option block.

### 7. Docs

**Invoke `/compact-docs-writer` before editing any of these — they are governing docs.**

`modules/mod-playerbots/CLAUDE.md` — add a second bullet beside the `*.conf` warning, so it is seen
*before* coordinate work starts rather than only when someone opens `pitfalls.md`:

> **Never invent a raid coordinate:** verify every spot with `navprobe` before shipping it. Client
> data is in the `ac-client-data` Docker volume, not `env/dist/data`. See `docs/engine/pitfalls.md`.

`docs/engine/pitfalls.md:69-72` — **the stale line that caused this**. It currently says
`env/dist/data/mmaps` "is **not** in this checkout, so the tool has no tiles to read here until they
are generated or mounted", which is what made this session declare the room unboundable. Replace with:
the tiles are in the `ac-client-data` Docker volume; `navprobe` is prebuilt at
`/azerothcore/env/dist/bin/navprobe` inside `acore/ac-wotlk-build:master`, so nothing needs building;
Git Bash needs `MSYS_NO_PATHCONV=1` or the absolute entrypoint path is mangled; and **`settledZ` is
the answer, not the trailing "N/N on mesh" summary** — a point can report a nearest poly within 2 yd
and still settle to terrain `-27.706`, which is off the floor. Include the one-line invocation.

`docs/raids/ulduar.md:260-282` — rewrite the Hodir section around the facts above. Priorities: the
Toasty Fire Flash-Freeze claim is **wrong** and must go; `SPELL_AURA_MELEE_SLOW` being +50% haste to
casting is the least guessable fact in the fight; the packing arithmetic explains three decisions at
once; and the corrected radii (9 / 11 / 8 / 3 / 4 / 7) plus the 2 s icicle cadence are expensive to
re-derive. Also `:895` — Hodir comes off the Sev-2 CHEAT-ONLY list; and `:860` — the threat-redirect
table's "no meaningful tanking" is no longer true.

## Verification

The module cannot be compiled in this environment — static checks here, build and in-game by the user.

**Static:**
1. `grep -rn "find target\", \"hodir\"" src/` → nothing; every lookup goes through `GetHodir`.
2. `grep -rn "IsHodirHardModeActive\|ulduarHodirHardMode\|UlduarHodirHardMode" src/ conf/` → nothing.
3. `grep -rn "HasCheat\|HodirFreeFrozenHelper\|HodirMoveToToastyFire" src/` → nothing left behind.
4. Every `creators[...]` string in `UldActionContext.h` / `UldTriggerContext.h` matches a
   `NextAction(...)` / `TriggerNode(...)` string in `UldStrategy.cpp` character for character.
5. Every name in the multiplier allowlist matches a registered action name.
6. The ring slot helper calls **both** `GetMapWaterOrGroundLevel` and
   `CheckCollisionAndGetValidCoords`.
7. `SPELL_HODIR_FROZEN_BLOWS` and `SPELL_HODIR_STORM_CLOUD` go through
   `sSpellMgr->GetSpellIdForDifficulty` at every read site.
8. New `Position` externs declared once in the header, defined once in the `.cpp`.

**navprobe** — re-run before shipping any coordinate change:

```
MSYS_NO_PATHCONV=1 docker run --rm \
  -v azerothcore-wotlk-pb_ac-client-data:/azerothcore/env/dist/data:ro \
  --entrypoint /azerothcore/env/dist/bin/navprobe acore/ac-wotlk-build:master \
  --map 603 ring 1986.56 -257.11 432.69 5 16
```

Read the **`settledZ`** column, not the "N/N on mesh" line. Check the tank spot at r=6, the off-tank
at r=6, the raid anchor at r=11 and r=15, and `path` between tank spot and anchor → `PATHFIND_NORMAL`.

**Build:** no new warnings; several classes are new and `HodirBitingColdJumpAction` changes behaviour.

**In game** (10-man covers everything except the drift radius and the 6-stack Storm Cloud):
1. Approach without pulling. Bots should walk to the corner **before** anyone engages — the main tank
   to `(1974.5, -275.5)`, the off-tank to `(1980.0, -277.0)`. Then pull from there.
2. Once the druid is freed, ranged and healers should form a 5 yd ring **on the Starlight zone**, not
   on the fixed anchor. Confirm they carry `62807`. If they sit on `(1986.56, -257.11)` instead, the
   dynobject read is failing.
3. Confirm ring bots actually **cast** rather than sliding in place — that is the `_anchorReached`
   latch, and the Sapphiron air-phase bug is what happens without it.
4. Icicles: a targeted bot must step ~6 yd within the 2 s telegraph **and stay inside Starlight** —
   check it still has `62807` after the dodge. Expect ~5 bots moving per icicle; that churn is
   intended. Watch for dodge/hold ping-pong, which means the trigger stand-down is not firing.
5. Biting Cold: nobody above 1 stack. Ringed bots should rarely jump; tanks should hop every ~3 s.
6. Flash Freeze: bots hold position for the first ~2 s while the drift is airborne, **then** converge
   on the 33174 nearest the raid anchor — all on the same one. Nobody encased. Formation re-forms
   within a few seconds. Confirm nobody runs at the falling icicle and eats the 7 yd detonation.
7. Deliberately leave one bot out of the shelter. Exactly **5** non-healers should break its block,
   and stay on it rather than flicking back to Hodir — flicker means the `DpsAssistAction` half of the
   multiplier is not firing. **If the block is not broken, that bot dies to `62226`** 48 s later.
   Healers must keep healing throughout.
8. Frozen Blows 15 s after each Flash Freeze: the off-tank taunts, the main tank taunts back when it
   drops, and Hodir never moves more than ~6 yd.
9. Storm Cloud: the carrier should **lap the ring**, not step once, and Storm Power should land on
   most of the ring rather than one or two bots. A tank carrier must not move. A melee carrier should
   run to the ring and back.
10. **No raid icon is ever set by a bot.** Then mark a skull mid-fight and confirm bots honour it.
11. Healers whose target walks out of range still move — the `ReachTargetAction` carve-out.
12. Nothing should ever reach `y < -297.8` or `y > -166.2`.

## Follow-up

- Copy this plan to `docs/plans/hodir-corner-anchor/hodir-corner-anchor.PLAN.md`.
- **Do the `pitfalls.md` / `CLAUDE.md` navprobe corrections first, on their own.** They are not
  Hodir-specific and every future coordinate question in this repo is cheaper once they exist.
- **Not implemented, deliberately:** Berserk awareness; dispelling the Freeze root (generic dispel
  already covers Magic — confirm in game before adding a node); a real Ice Shards spread (impossible
  inside Starlight, see the packing arithmetic); Starlight for melee (decision 27).
- Cheese the Freeze and Getting Cold in Here fall out of this work for free but are not chased.

---

# Repair round: the three in-game failures

## Context

The corner-anchor rework shipped and compiles. First live pull produced three failures:

1. **Main tank died to Biting Cold stacks.**
2. **Lots of bots died to Ice Shards**, which overlap and are handled ungracefully.
3. **Bots oscillate constantly** instead of doing damage or healing.

All three are now diagnosed to specific defects. Two of them come from DBC/script facts the original
plan got wrong, so the fix changes design, not just code. The shipped work stays; this is a repair
round on top of it.

The original plan lives at `docs/plans/hodir-corner-anchor/hodir-corner-anchor.PLAN.md`. Fold this
round into it at implementation time rather than creating a second directory.

## Corrected facts

Read from the core script and the DBC extract this session. **These override the original plan.**

| Fact | Source | What the old plan said |
|---|---|---|
| **Biting Cold `62039` ticks every 1 s and damages `200·2^stacks` every tick** (`EffectAuraPeriod_1 = 1000`) | `spell.reference.csv` | "stacks every 4 s" — the *gain* is every 4 s, the *damage* is every 1 s |
| **A stack comes off only on the second moving tick**: a moving tick sets `_prev = true`, the next moving tick calls `ModStackAmount(-1)`; **any stationary tick resets `_prev = false`** | `boss_hodir.cpp:1259-1273` | not modelled |
| **A Toasty Fire aura is treated exactly like moving**, every tick, so it sheds a stack every 2 s with no movement at all | `boss_hodir.cpp:1259` | "blocks Biting Cold" — it also *removes* stacks |
| **Ice Shards `62457` = 13,999 base points, radius index 26 = 4 yd** | `spell.reference.csv` | correct |
| **Both icicle NPCs live exactly 7 s** (`62234`/`62462` DurationIndex 165 = 7000) | `spellduration.reference.csv` | not known |
| **Detonation is at t = 3.7 s**, not 2 s: the AI casts the fall effect at t = 2.0 (`timer1`), and `62236`/`62460` is a 1700 ms aura whose single tick triggers the blast | `boss_hodir.cpp:605-614`; DurationIndex 487 = 1700 | "2 s telegraph" |
| **So an icicle is inert for its last 3.3 s** and roughly half the icicles alive at any moment have already blown | derived | treated every live icicle as lethal |
| `TempSummon::GetTimer()` is public and counts the 7000 ms down | `TemporarySummon.h:68` | — |
| Shelter window is **t=3.7 → 15.7** against a Flash Freeze landing at t=9, i.e. **5.3 s of shelter**, not 7 | 62463 DurationIndex 29 = 12000 | "7 s of shelter" |
| `FindNearestPositionClearOfHazards(bot, hazards, clearRadius, maxRadius, distanceStep, angleStep)` rings outward, collision-validates each candidate, and is the sanctioned replacement for `FleePosition` | `RaidBossHelpers.h:38-44`, `.cpp:308-358` | plan used a hand-rolled sweep + `FleePosition` |
| `MOVEMENT_FORCED > MOVEMENT_COMBAT`, and `IsWaitingForLastMove` returns false only when `priority > lastMove.priority` | `MovementActions.cpp:955`; `LastMovementValue.h:18-25` | dodge and position both used `MOVEMENT_COMBAT` |
| `Trigger` default `checkInterval = 1` → polled every engine tick | `Trigger.cpp:35-37` | — |

## Diagnosis

### 1. Main tank died to Biting Cold

**Defect A — the jump action can never run.** `HodirBitingColdJumpAction::isUseful()`
([UldActions_Hodir.cpp:164-168](src/Ai/Raid/Uld/Action/UldActions_Hodir.cpp#L164-L168)) constructs a
**fresh** `HodirBitingColdTrigger` on the stack. Its `_stillSince` starts at 0, so `IsActive()` takes
the `if (!_stillSince) { _stillSince = now; return false; }` branch and returns false **before** it
ever reaches the aura check. `Engine` calls `isUseful()` on that fresh trigger twice per tick
(`Engine.cpp:185`, `:336`). The node has therefore never executed once, for anybody.

**Defect B — even fixed, a 2 yd jump cannot shed a stack.** `JumpTo` →
`MoveJump(..., runSpeed, runSpeed, 1)` over 2 yd is ~0.3 s of motion, then `last movement` locks for
1000 ms. Tick instants are 1 s apart, so two consecutive moving ticks essentially never happen and
`_prev` is reset by every stationary tick in between. Net effect is only to slow the *gain* from one
stack per 4 s to roughly one per 10 s. At 8 stacks the tank eats 51,200/s.

**Two moving ticks with no stationary tick between them requires > 1 s of continuous motion**, i.e.
legs of at least ~7 yd, or short legs chained back to back with no idle gap.

### 2. Ice Shards deaths

**Defect C — spent icicles are still treated as lethal.** `NearestHodirIcicle` and
`CollectHodirIcicles` filter on `IsAlive()` only. An icicle blows at t=3.7 and lingers to t=7, so at
any moment ~1.7 of the ~3.5 live icicles are harmless. This inflates the hazard set, and it is why
bots refuse to return to a slot that is already safe.

**Defect D — the dodge is all-or-nothing.** [UldActions_Hodir.cpp:95-158](src/Ai/Raid/Uld/Action/UldActions_Hodir.cpp#L95-L158)
requires a candidate ≥ 6 yd from **every** icicle within 100 yd, inside a 10 yd leash. With phantom
hazards from defect C plus real overlap, the union of exclusion discs covers the whole leash and
`found` stays false. It then falls through to `FleePosition(nearest, 6.0f, 500)`, which
`pitfalls.md:100-110` documents as clamping travel to `AiPlayerbot.FleeDistance` (5.0), reading one
hazard only, and blacklisting reverse angles for 5 s — so the second hazard of a volley usually
returns `false` and the bot stands in the blast.

**Defect E — the dodge cannot preempt its own movement.** Both the dodge and the position action use
`MOVEMENT_COMBAT`, so `IsWaitingForLastMove` blocks a re-dodge while a move is in flight, and the
dodge cannot interrupt the position action walking a bot back into an icicle.

**Defect F — the ring is the hazard.** 16 ranged on one r=5 ring sit 1.95 yd apart, so one 4 yd
splash covers ~5 of them. The original plan documented this and bet on the dodge; the dodge does not
work, so the bet lost. **User decision: fix the layout, not just the dodge.**

### 3. Oscillation

**Defect G (primary) — the ring-centre latch is shared per instance but validated per bot.**
[UldBossHelper.cpp:653-690](src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L653-L690):

```cpp
if (latched != _hodirRingCentres.end() && bot->HasAura(SPELL_HODIR_STARLIGHT))
    return latched->second;
```

`_hodirRingCentres` is keyed by **instanceId** — one value for the whole raid — but the keep-it test
is **this bot's** aura. Any bot momentarily outside Starlight (mid-dodge, walking in, an outer slot at
the zone edge) falls through and **rewrites the shared centre** to the druid's current dynobject.
Every other bot then reads a different centre, its slot moves, `_anchorReached` clears past 6 yd, and
it runs. With 16 bots and constant dodging this rewrites many times per second and the whole ring
thrashes. Compounded by `GetDynObject` returning only the first of up to four overlapping zones while
the druid recasts every 15 s from wherever it has walked to.

**Defect H — `anchorAngle` is derived from the moving centre**
([UldBossHelper.cpp:729-730](src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L729-L730)), so a centre
translation rotates *and* translates every slot. The comment claims it prevents rotation; it does not.

**Defect I — `IsHodirTrappedAllyBreaker` ranks by distance**, not guid
([UldBossHelper.cpp:809-816](src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L809-L816)), contrary to the
original plan's decision 23. Distances change every tick, so membership of the "nearest 5" churns and
bots flick between the ice block and Hodir.

**Defect J — dead code.** `HodirRaidPositionTrigger` returns false whenever `GetHodirAnchor` returns
false, and `GetHodirAnchor` returns false for melee, so the melee de-clump tail in
[UldActions_Hodir.cpp:196-203](src/Ai/Raid/Uld/Action/UldActions_Hodir.cpp#L196-L203) is unreachable.

**Defect K — `MoveInside` parks bots 3 yd off their slot.** It delegates to
`MoveNear(mapId, x, y, z, distance, …)` = `MoveTo(x + cos(GetFollowAngle())·distance, …)`
(`MovementActions.cpp:82-86`, `:1687-1694`). `GetFollowAngle()` is group-index based, so it is stable
but unrelated to the ring — every bot rests 3 yd off-slot in an arbitrary direction, eating the
spacing budget.

## Decisions

Settled with the user this round.

1. **Spacing-first layout with a Starlight core.** Replace the single r=5 ring with concentric slots
   whose minimum separation exceeds the 4 yd Ice Shards radius. One bot per icicle instead of five.
2. **Biting Cold: shed at ≥ 2 stacks**, keep going until the aura is gone. ~33 % movement duty,
   ~600/s average damage, casting uptime stays high. Same rule for tanks and ranged.
3. **Melee get nothing.** Delete the unreachable de-clump; melee keep every generic mover, which the
   multiplier already allows.

Carried forward from the original plan and still correct: corner tanking, the shelter run, Starlight
as the buff worth anchoring on, tank swap on Frozen Blows, the Storm Cloud arc, derive-don't-
communicate, no raid icons, Berserk unhandled.

## Geometry

Every value navmesh-verified with `navprobe` this session, map 603. `settledZ` is **432.687** at every
probed point; all rings 100 % on mesh.

| Probe | Result |
|---|---|
| Ring r=4.5, 6 headings, around the raid anchor | 6/6 on mesh |
| Ring r=11 (covered by r=15 probe), 12 headings | 12/12 on mesh |
| Ring r=15, 12 headings | 12/12 on mesh |
| Ring r=21, 12 headings | 12/12 on mesh |
| Ring r=24, 12 headings | 12/12 on mesh — the room is wider than the old "x ≥ 1965" estimate |
| Ring r=3, 8 headings, around **both** tank spots | 8/8 on mesh each |
| Path MT shuttle `(1976.621, -277.621)` → `(1972.379, -273.379)` | 6.00 yd, direct, on mesh |
| Path OT shuttle `(1982.121, -279.121)` → `(1977.879, -274.879)` | 6.00 yd, direct, on mesh |

Both tank shuttle axes run at **315°/135°**, parallel to the SW bevel, so the shuttle never walks a
tank toward the chamfer. Both stay well inside the boss evade band `y ∈ (-297.793, -166.259)`.

Invocation, for re-verification:

```
MSYS_NO_PATHCONV=1 docker run --rm \
  -v azerothcore-wotlk-pb_ac-client-data:/azerothcore/env/dist/data:ro \
  --entrypoint /azerothcore/env/dist/bin/navprobe acore/ac-wotlk-build:master \
  --map 603 ring 1986.56 -257.11 432.69 11 12
```

## The new ranged layout

Slot index comes from the guid-stable roster, sorted **`(IsRangedDps ? 0 : 1, guid)`** so ranged DPS
fill the Starlight slots first and healers take the outer ring. `n` = live ranged + healers,
excluding tanks.

```
slot 0        r =  0.0                              (centre)
slots 1..6    r =  4.5   step 2π/min(6, n-1)        4.50 yd apart at 6 slots
slots 7..     r = 11.0   step 2π/outerCount         7.50 yd apart at 9 slots; 5.69 at 12
radial gap    6.5 yd
```

- **Minimum separation 4.50 yd > the 4 yd splash** → one bot per icicle. The original plan's own
  table puts that at **7,000 sustained HPS** instead of 35,000.
- **Starlight (8 yd) covers slots 0-6 — seven slots.** A 10-man's whole ranged group fits; a 25-man
  gives it to the seven highest-priority DPS. This is the deliberate reversal of the original
  decision 5: 16 bots cannot be 4 yd apart inside an 8 yd circle, so Starlight-for-everyone and
  icicle safety are mutually exclusive.
- **The outer ring is at 11, not 9,** so an inner-ring bot shedding Biting Cold (see below) can move
  3 yd outward and still be 3.5 yd clear of the outer ring, and still inside Starlight at r=7.5.
- `base` angle = bearing from **`ULDUAR_HODIR_RAID_ANCHOR` to `ULDUAR_HODIR_MAINTANK_SPOT`**, both
  compile-time constants, so the layout never rotates. This fixes defect H.

Write the packing arithmetic into the constant's comment. It is the one number a later reader will
want to shrink, and shrinking it re-creates defect F.

## Changes

### 1. `Util/UldBossHelper.h` / `.cpp`

**Delete `_hodirRingCentres` entirely** and rewrite `GetHodirRingCentre` with no shared mutable state
(fixes defect G):

```cpp
Position GetHodirRingCentre(PlayerbotAI* botAI, Player* bot)
{
    Creature* druid = GetHodirDruidHelper(botAI);
    if (!druid)
        return ULDUAR_HODIR_RAID_ANCHOR;

    // Quantised so the druid shuffling a yard does not walk the whole raid. 3 yd keeps an inner
    // slot within 6.6 yd of the druid, inside Starlight's 8.
    float const q = ULDUAR_HODIR_CENTRE_QUANTUM;
    Position centre(std::round(druid->GetPositionX() / q) * q,
                    std::round(druid->GetPositionY() / q) * q,
                    ULDUAR_HODIR_RAID_ANCHOR.GetPositionZ());

    if (centre.GetExactDist2d(&ULDUAR_HODIR_RAID_ANCHOR) > ULDUAR_HODIR_ZONE_ADOPT_RADIUS ||
        centre.GetExactDist2d(&ULDUAR_HODIR_MAINTANK_SPOT) < ULDUAR_HODIR_CENTRE_MIN_TANK_GAP)
        return ULDUAR_HODIR_RAID_ANCHOR;

    return centre;
}
```

Every bot computes the same answer from the same inputs, it self-heals across pulls, and the
`_anchorReached` latch (which clears at `2 × tolerance` = 4 yd) absorbs a 3 yd quantum step without
moving anyone. No dynobject read at all — Starlight sits at the druid's feet, so the druid *is* the
zone.

**Rewrite `GetHodirRingSlot`** for the concentric layout above. Keep the existing
`GetMapWaterOrGroundLevel` + `Map::CheckCollisionAndGetValidCoords` validation — that part is right.
Change the roster sort to `(IsRangedDps ? 0 : 1, guid)`. Compute `base` from the two fixed constants.

**Add `bool IsHodirIcicleLethal(Creature* icicle)`** — the fix for defect C:

```cpp
// An icicle summon lives 7 s but detonates at 3.7 s, so its last 3.3 s are inert. Dodging a spent
// one is what keeps bots off their slots and walking back and forth.
TempSummon* summon = icicle->ToTempSummon();
uint32 const remaining = summon ? summon->GetTimer() : 0;
return !remaining || remaining > ULDUAR_HODIR_ICICLE_SPENT_MS;   // 3300
```

`remaining == 0` means the summon type carries no timer; treat it as lethal, which is the safe
default. Use this in **both** `NearestHodirIcicle` (trigger) and `CollectHodirIcicles` (action) so the
two cannot disagree.

**Fix `IsHodirTrappedAllyBreaker`** (defect I): filter by `TRAPPED_ALLY_RANGE`, then rank by **guid
only**. Delete the distance comparator.

**Add `bool GetHodirShuttleLeg(PlayerbotAI*, Player*, Position& out)`** — where a bot goes to shed
Biting Cold:

- **Main tank / assist tank 0**: alternate between the two navprobe-verified endpoints,
  `spot ± 3 yd` along bearing `−π/4`. Pick whichever is further from the bot, so the leg is always a
  full 6 yd and `IsDuplicateMove` never refuses it. Hodir oscillates 6 yd along the wall; that is the
  price and it is bounded.
- **Everyone else**: `FindNearestPositionClearOfHazards(bot, <every other live raider within 12 yd>,
  ULDUAR_HODIR_DECLUMP_RADIUS, 12.0f, 6.0f, M_PI/8)`. Passing `distanceStep = 6.0f` makes the helper
  probe rings at 6 and 12 yd only, so the leg is always long enough to cover two tick instants. The
  helper collision-validates, so the destination is never off-mesh. Empty result → retry at
  `clearRadius = 3.0f`; still empty → return false.

**Constants** — replace and add:

```cpp
constexpr float  ULDUAR_HODIR_RAID_RING_INNER      =  4.5f;  // 6 slots, 4.50 yd apart, inside Starlight
constexpr float  ULDUAR_HODIR_RAID_RING_OUTER      = 11.0f;  // leaves 6.5 yd radial gap for a shed leg
constexpr uint32 ULDUAR_HODIR_RAID_RING_INNER_SLOTS = 6;
constexpr float  ULDUAR_HODIR_RING_SPOT_TOLERANCE  =  2.0f;  // re-anchor at 4 yd > the 3 yd centre quantum
constexpr float  ULDUAR_HODIR_CENTRE_QUANTUM       =  3.0f;
constexpr float  ULDUAR_HODIR_ZONE_ADOPT_RADIUS    = 10.0f;  // was 15
constexpr float  ULDUAR_HODIR_CENTRE_MIN_TANK_GAP  = 18.0f;  // never centre the ring in Hodir's melee
constexpr float  ULDUAR_HODIR_DECLUMP_RADIUS       =  4.5f;  // ICE_SHARDS_RADIUS + margin
constexpr float  ULDUAR_HODIR_SHUTTLE_HALF_LEG     =  3.0f;
constexpr float  ULDUAR_HODIR_SHUTTLE_BEARING      = -0.785398f;  // -pi/4, parallel to the SW bevel
constexpr uint32 ULDUAR_HODIR_ICICLE_SPENT_MS      = 3300;   // 7000 lifespan - 3700 to detonation
constexpr uint32 ULDUAR_HODIR_BITING_COLD_SHED_STACKS = 2;
constexpr float  ULDUAR_HODIR_DODGE_LEASH          = 12.0f;
```

Delete `ULDUAR_HODIR_RAID_RING_RADIUS` and `ULDUAR_HODIR_JUMP_HOP` / `ULDUAR_HODIR_JUMP_IDLE_MS`.

### 2. `Trigger/UldTriggers_Hodir.h` / `.cpp`

**Rewrite `HodirBitingColdTrigger` stateless** — delete `_stillSince` (fixes defect A at the root):

```cpp
if (!IsHodirEngaged(botAI)) return false;
if (bot->HasAura(SPELL_HODIR_FLASH_FREEZE_TRAPPED)) return false;
if (bot->HasAura(SPELL_HODIR_TOASTY_FIRE_AURA)) return false;   // the fire sheds a stack every 2 s for free
return bot->HasAura(SPELL_BITING_COLD_PLAYER_AURA);
```

The ≥ 2 stack arm and the shed-until-clear hysteresis live on the **action**, which is a cached
instance, so a stack-allocated copy can never lose them.

**`HodirIcicleDodgeTrigger`** — route both entry lookups through `IsHodirIcicleLethal`.

**`HodirRaidPositionTrigger`** — add `HodirBitingColdTrigger` to the stand-down list beside the
shelter and dodge triggers, so the position node stops dragging a shedding bot back to its slot.

Also fire the dodge stand-down on the **slot**, not just the bot: return false when a lethal icicle is
within `ICE_SHARDS_CLEAR` of the anchor position. Otherwise a bot that has correctly dodged is walked
straight back onto the icicle that is still counting down.

### 3. `Action/UldActions_Hodir.h` / `.cpp`

**Delete every `isUseful()` override in this file.** All three construct a trigger on the stack; one
of them is defect A and the other two are the same footgun waiting. The trigger node is the gate.

**Rename `HodirBitingColdJumpAction` → `HodirBitingColdShedAction`** (`"hodir biting cold shed"`) and
rewrite:

```cpp
bool HodirBitingColdShedAction::Execute(Event)
{
    Aura* cold = bot->GetAura(SPELL_BITING_COLD_PLAYER_AURA);
    if (!cold)
    {
        _shedding = false;
        return false;
    }

    if (!_shedding && cold->GetStackAmount() < ULDUAR_HODIR_BITING_COLD_SHED_STACKS)
        return false;

    // Once started, keep moving until the aura is gone: a stack only comes off on the second moving
    // tick, and any stationary tick in between resets that progress.
    _shedding = true;

    Position leg;
    if (!GetHodirShuttleLeg(botAI, bot, leg))
        return false;

    return MoveTo(bot->GetMapId(), leg.GetPositionX(), leg.GetPositionY(), leg.GetPositionZ(),
                  false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
}
```

Legs chain: `MoveTo` sets `lastdelayTime` to travel time, and the action re-fires on the next bot tick
after it expires, so motion is near-continuous for the 2-4 s it takes to clear the aura.

**Rewrite `HodirIcicleDodgeAction::Execute`** on the shared helper (fixes defects D and E):

```cpp
std::vector<Position> hazards;   // lethal icicles only, both entries, via IsHodirIcicleLethal
...
Position dest = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_HODIR_ICE_SHARDS_CLEAR,
                                                  ULDUAR_HODIR_DODGE_LEASH);
if (!dest.GetPositionX() && !dest.GetPositionY())
    // Graceful degradation: 4 yd is the actual kill radius, 6 was only margin.
    dest = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_HODIR_ICE_SHARDS_RADIUS + 0.5f,
                                             ULDUAR_HODIR_DODGE_LEASH * 2.0f);
if (!dest.GetPositionX() && !dest.GetPositionY())
    return false;

return MoveTo(..., MovementPriority::MOVEMENT_FORCED);
```

Delete the hand-rolled 8-direction sweep, the Starlight tiering and the `FleePosition` fallback. The
helper rings outward, so the first hit is already the shortest walk, which keeps a bot near its slot
without a tier rule. `MOVEMENT_FORCED` lets the dodge preempt the position action.

Note in a comment that a **second** icicle arriving mid-dodge still cannot preempt, because
`IsWaitingForLastMove` only yields to a strictly higher priority. The ≥ 4.5 yd layout is what makes
that rare; do not "fix" it by escalating priority further.

**`HodirRaidPositionAction`** — delete the unreachable melee de-clump tail (decision 3, defect J).
Replace `MoveInside` with `MoveTo` to the **exact** slot (fixes defect K); the `_anchorReached` latch
already provides the stop condition, so the 3 yd `MoveNear` offset buys nothing and costs spacing.

**`HodirSpreadStormCloudAction`** — step to the next slot on the bot's own ring, not a fixed π/4 arc,
now that the ring radius depends on the slot.

### 4. Wiring — `UldStrategy.cpp`, `UldActionContext.h`, `UldTriggerContext.h`

Rename `"hodir biting cold jump"` → `"hodir biting cold shed"` in **all three** places plus the
multiplier allowlist. A mismatch fails silently at runtime (`pitfalls.md:11-29`).

Re-rank so the shed cannot starve targeting — the engine breaks on the first action that returns
true, and the shed returns true ~33 % of ticks:

```
"hodir near snowpacked icicle"    -> "hodir move snowpacked icicle"     ACTION_RAID + 6
"hodir icicle dodge"              -> "hodir icicle dodge action"        ACTION_RAID + 5
"hodir frozen blows swap"         -> "hodir frozen blows swap action"   ACTION_RAID + 4
"hodir set dps priority"          -> "hodir set dps priority action"    ACTION_RAID + 3
"hodir spread storm cloud"        -> "hodir spread storm cloud"         ACTION_RAID + 2
"hodir biting cold"               -> "hodir biting cold shed"           ACTION_RAID + 1
"hodir frost resistance trigger"  -> "hodir frost resistance action"    ACTION_RAID
"hodir raid position"             -> "hodir raid position action"       ACTION_RAID
```

Targeting is safe above the shed because `HodirSetDpsPriorityAction::Execute` returns false whenever
the current target is already correct, so it starves nothing.

### 5. `UldMultipliers.cpp`

Update the `encounterMovers` allowlist name. No other change — the movement and `DpsAssistAction`
halves are correct.

### 6. Docs

**Invoke `/compact-docs-writer` before editing any of these.**

- `docs/raids/ulduar.md` Hodir section — the 1 s Biting Cold tick, the two-moving-ticks shed rule,
  Toasty Fire removing stacks, the 7 s icicle lifespan with detonation at 3.7 s, the 5.3 s shelter
  window, and the layout arithmetic that replaces the single-ring table.
- `docs/engine/pitfalls.md` — **new entry**: *never construct a Trigger inside `Action::isUseful()`*.
  A stack-allocated trigger loses any per-bot state the registered instance holds, and the node then
  silently never runs. Cite `HodirBitingColdJumpAction` as the case that shipped.
- `docs/plans/hodir-corner-anchor/hodir-corner-anchor.PLAN.md` — fold this round in and mark decision
  5 reversed with the reason.

## Verification

**Static**

1. `grep -rn "_stillSince\|_hodirRingCentres\|hodir biting cold jump\|ULDUAR_HODIR_RAID_RING_RADIUS" src/ conf/` → nothing.
2. `grep -rn "Trigger [a-z]*(botAI);" src/Ai/Raid/Uld/Action/UldActions_Hodir.cpp` → nothing.
3. Every `creators[...]` string matches a `NextAction(...)` / `TriggerNode(...)` string character for
   character, and every multiplier allowlist name matches a registered action.
4. `IsHodirIcicleLethal` is the only path by which either icicle entry becomes a hazard.
5. `IsHodirTrappedAllyBreaker` has no distance comparator left.
6. `GetHodirRingSlot` still calls both `GetMapWaterOrGroundLevel` and `CheckCollisionAndGetValidCoords`.

**Build** — the user builds via Docker; paste errors back. Several classes are renamed, so expect
link-level fallout if any wiring site is missed.

**In game** (10-man exercises everything except the 12-slot outer ring)

1. Ranged form the concentric layout, not a ring: one bot at the centre, up to six at 4.5 yd, the rest
   at 11 yd. On 10-man everyone should carry `62807`; on 25-man exactly seven should.
2. Bots **hold still and cast**. Watch specifically for the centre drifting — if slots move without
   the druid relocating, the quantum or the `_anchorReached` threshold is wrong.
3. Icicles: one bot moves per icicle, not five. It should step ~6 yd and **not** be walked back until
   the icicle is spent. No bot should dodge an icicle that has already blown.
4. Biting Cold: nobody above 2 stacks, including the main tank. The tank shuttles 6 yd along the wall
   and Hodir never leaves the corner. Bots stop moving the moment the aura clears.
5. Deliberately leave one bot out of a shelter: exactly **5** non-healers break its block and stay on
   it — no flicker back to Hodir. Healers keep healing.
6. Frozen Blows 15 s after each Flash Freeze: off-tank taunts, main tank takes him back, Hodir moves
   ≤ 6 yd.
7. Storm Cloud carrier laps its own ring; tanks never leave the corner to carry it.
8. Nothing reaches `y < -297.8` or `y > -166.2`.

## Out of scope

Unchanged from the original plan and still deliberate: Berserk awareness, dispelling the Freeze root,
Starlight for melee, raid icons. Melee positioning is now explicitly out too (decision 3).
