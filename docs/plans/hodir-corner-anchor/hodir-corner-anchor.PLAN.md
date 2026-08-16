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
