# Obsidian Sanctum (`wotlk-os`) — mechanic audit and fix plan

## Context

The Sartharion strategy under `modules/mod-playerbots/src/Ai/Raid/OS/` was audited against the
Warcraft Tavern Sartharion-25 guide **and** against the authoritative server script
(`src/server/scripts/Northrend/ChamberOfAspects/ObsidianSanctum/boss_sartharion.cpp` +
`instance_obsidian_sanctum.cpp`), the world DB, and the Spell.dbc reference export.

Three defects make the strategy fail outright, one whole mechanic is untanked, and the
`AiPlayerbot.SartharionDrakesAlive` config turns out to guard nothing: the achievement/loot count
is snapshotted at pull time and is unaffected by killing drakes mid-fight. The config is being
removed and the bots will always kill every drake that joins.

On completion the strategy should clear Sartharion at every drake count without any OS-specific
configuration, with the raid's chosen difficulty expressed the way a human raid expresses it —
by which drakes it kills before pulling.

---

## Verified reference data

Everything below was read out of the server script, the world DB, or the DBC export. Do not
re-derive it from memory; do not trust the numbers currently in `docs/raids/obsidian-sanctum.md`.

### Flame Tsunami geometry (`boss_sartharion.cpp:269-280, 633-685`)

Waves alternate randomly every 25s. Each segment is a `NPC_FLAME_TSUNAMI` (30616) creature.

| Wave | Spawn X | Orientation | Segment Y centres | Travels to |
|---|---|---|---|---|
| Left | 3211.0 | `0.0f` | 476, 484, 492, 524, 532, 540, 572, 580, 588 | X 3286 (eastward) |
| Right | 3286.0 | `3.14f` | 500, 508, 516, 548, 556, 564 | X 3211 (westward) |

Segments sit 8 apart and form continuous walls, so each covers roughly ±4 in Y:

- **Left wave occupies** Y `[472,496] [520,544] [568,592]` — safe lanes `(496,520)` and `(544,568)`
- **Right wave occupies** Y `[496,520] [544,568]` — safe lanes `(472,496)` `(520,544)` `(568,592)`

The two sets are **exact complements**. There is no Y that is safe from both directions, so
everyone — tanks included — must move for one of the two wave directions.

Safe-lane midpoints: **508** and **556** (left wave), **532** (right wave).

### What the guide's diagrams show

The guide carries 11 annotated top-down screenshots. They are schematic, not coordinate-accurate —
Tenebron is drawn at the far end of the platform when she actually lands 15 yd from Sartharion — so
read them for structure, not positions. Two are geometrically meaningful:

- `Sartharion-Tri-Lava-Positioning-Direction` — the **left** wave, drawn as **3 walls** with two
  interior gaps. Melee and the tank occupy one gap, healers and ranged the other.
- `Sartharion-Duo-Lava-Positioning-Arrows-1` — the **right** wave, drawn as **2 walls** with a
  single interior gap that the whole raid shares.

Wall counts match the server exactly (9 left segments in 3 groups of 3, 6 right segments in 2 groups
of 3), and the gap counts match the band arithmetic above. **This validates the module's existing
lane design**: two separate lanes for the left wave (`TSUNAMI_LEFT_SAFE_MELEE` /
`TSUNAMI_LEFT_SAFE_RANGED`) and one shared lane for the right (`TSUNAMI_RIGHT_SAFE_ALL`) is the
correct shape. Only the midpoints need adjusting.

The role diagrams establish the intended layout:

| Diagram | What it shows |
|---|---|
| `Sartharion-Tenebron-Landed` | Boss + MT at one end, drake + melee + **off-tank** at the other, healers and ranged in the middle |
| `Sartharion-Tenebron-Shadron-Positioning`, `Sartharion-Vesperon-Shadron-Positioning` | Both drakes **stacked on one off-tank**, melee on them, healers and ranged in the middle |
| `Sartharion-Lava-Blaze-No-Drake` | Lava Blaze pulled off to the side by the **off-tank**, away from the boss |
| `Sartharion-Tenebron-Whelp-Portal` | Twilight Whelps leaving the portal, picked up by the **off-tank** and stacked on Tenebron for melee cleave. Nobody enters Tenebron's portal |
| `Vesperon-Portal-Positioning` | Arrows into the portal from **melee, ranged and a healer**; a second healer and the off-tank stay outside with Vesperon |

So the off-tank's job is drakes **plus Twilight Whelps plus Lava Blazes**, all held together away
from the raid, and the healer/ranged position is defined by being in range of *both* tanks.

### Cone and radius data (Spell.dbc + `acore_world.spell_cone`)

`spell_cone` overrides the DBC default, so these are the real values:

| Spell | Shape | Angle | Radius |
|---|---|---|---|
| Flame Breath 56908 | front cone | 82° (±41° from facing) | **60 yd** |
| Tail Lash 56910 | **back** cone (`SpellVisual[0]==3879` → `SPELL_ATTR0_CU_CONE_BACK`) | 82° → hit zone 139°–221° | 30 yd |
| Shadow Breath 57570 | front cone | 60° (±30°) | 15 yd |
| Cleave 56909 | melee | — | 5 yd |
| Void Blast 57581 (Twilight Fissure detonation) | area | — | **4 yd** |

**Consequence: the existing `rear flank` melee positioning is correct.** `RearFlankAction` picks
90°–120° off the target's facing, which clears Flame Breath (needs >41°) and stays outside Tail
Lash (needs <139°). Leave it alone. The 5 yd `AvoidTwilightFissureAction` radius is also correct
(Void Blast is 4 yd).

### Phase and aura facts

- Acolyte of Shadron 31218, Acolyte of Vesperon 31219 and Twilight Egg 30882 are created with
  `SetPhaseMask(16, true)`. Players are phase 1.
- Both bot perception paths filter on phase: `AttackersValue::IsPossibleTarget`
  (`src/Ai/Base/Value/AttackersValue.cpp:172`) calls `bot->CanSeeOrDetect` → `CanNeverSee` →
  `!InSamePhase`; and the grid check `AnyUnfriendlyUnitInObjectRangeCheck` → `IsWithinDistInMap` →
  `InSamePhase`. **A bot on the ground can never see an acolyte or an egg.**
- GO 193988 "Twilight Portal" is a type-10 goober whose `data10` is spell **57620** (aura 261
  `SPELL_AURA_PHASE`, misc 16). 57620 effect 3 triggers **57874** (−25% damage done + DoT), and
  `spell_linked_spell` has `-57620 → -57874`, so the module's `SPELL_TWILIGHT_SHIFT = 57874`
  realm detector **is valid and self-clearing**. Leave it.
- GO 193989 "Normal Portal" casts 61187, linked `61187 → -57620`. It has a permanent world spawn
  at `(3247.29, 529.804, 58.9595)` on map 615 — the same spot the fight's twilight portal appears.
  `ExitTwilightPortalAction` works.
- During the Sartharion fight there is **one** instance-managed portal at
  `(3247.29, 529.804, 58.9595)`, refcounted by `portalCount`, shared by all three drakes
  (`instance_obsidian_sanctum.cpp:119-152`). The single-portal assumption in the strategy holds.

### Encounter facts that differ from the guide

- **Berserk fires at 30% and is unconditional** (`boss_sartharion.cpp:483`), not 35% and not
  hard-mode-only. Leaving drakes alive does not avoid it.
- **Twilight Revenge (60639) fires on every drake death during the fight, unconditionally**
  (`DoAction(ACTION_DRAKE_DIED)`, `boss_sartharion.cpp:345-349`). It is +25% physical damage done
  and +25% melee/ranged haste. It has **nothing** to do with acolytes being alive.
- Drake call timers from `JustEngagedWith`: Tenebron 20s, Shadron 60s, Vesperon 120s, each plus
  ~4s of speech and flight. Landing spots: Tenebron `(3249.75, 566.95)`, Shadron `(3230.50, 533.00)`,
  Vesperon `(3269.71, 532.79)`.
- Gift of Twilight Fire 58766 on Sartharion is −100% damage taken (the boss also hard-zeroes
  damage in `DamageTaken`). Only killing the Acolyte of Shadron or Shadron himself removes it.
  Shadron reschedules the portal 30s after each acolyte death, forever.
- Power auras: Tenebron 61248 = +100% shadow taken; Shadron 58105 = +100% fire taken;
  Vesperon 61251 = −25% max health. Will of Sartharion 61254 = +25% max health if any drake is up.

### Why the drake config is pointless

`dragonsCount` is counted once in `JustEngagedWith` and **never decremented**. Loot modes are added
there (`me->AddLootMode(1 << dragonsCount)`), and achievement criteria 7328/7331 (Twilight Assist),
7329/7332 (Duo) and 7330/7333 (Zone) all read that same snapshot
(`instance_obsidian_sanctum.cpp:85-108`).

Killing drakes during the fight therefore costs **zero** loot and zero achievement progress, while
keeping them alive costs a permanent Power aura each and, for Shadron, an endless damage-immunity
cycle. Killing them costs one Twilight Revenge stack. Always killing is strictly better.

---

## Findings

| # | Severity | Finding |
|---|---|---|
| F1 | Critical | `TwilightPortalEnterTrigger` can never fire |
| F2 | Critical | Flame Tsunami wave-side misclassification |
| F3 | Critical | Tanks never dodge tsunamis, and both tank spots sit in a left-wave band |
| F4 | High | Lava Blaze is never tanked |
| F5 | Medium | Ranged stack sits on the edge of Flame Breath's cone |
| F6 | Low | Safe-lane constants are 4 yd off their true midpoints |
| F7 | — | `AiPlayerbot.SartharionDrakesAlive` removal |
| F8 | — | Documentation is wrong about Twilight Revenge |
| F9 | High | Twilight Whelps are never tanked |
| F10 | Medium | Portal runner cap is too small and excludes melee and healers |
| F11 | Low | Both tanks share one tsunami lane |

**F1** — `OSTriggers.cpp:99` gates entry on `AnyTwilightPortalAcolyteAlive(botAI)`, which resolves
through `GetFirstAliveUnitByEntry` → `possible targets no los` → phase filter. Acolytes are phase
16, the bot is phase 1, so this is always false. No bot ever enters the realm, the Acolyte of
Shadron is never killed, and Sartharion stays damage-immune for the rest of the fight whenever
Shadron is up. This directly contradicts the design comment already written in
`EnterTwilightPortalAction` (`OSActions.cpp:236-238`), which says entry should key off the portal
alone.

**F2** — `OSActions.cpp:141-142` classifies the wave with
`int posY = (int)unit->GetPositionY(); if (posY == 500 || posY == 564)`. Right-wave segment centres
are {500, 508, 516, 548, 556, 564}, so **only 2 of 6 classify correctly**; the other four fall into
the left-wave branch. The left-wave "safe" targets are Y 552 and Y 504, both of which are *inside*
right-wave bands `[544,568]` and `[496,520]`. A misclassified right wave actively moves bots into
the fire. The loop acts on the first segment that needs movement and `nearest hostile npcs` is
unsorted, so which segment wins is effectively arbitrary.

**F3** — `OSTriggers.cpp:23` returns false for any tank, and both tank home spots
(MT Y 532.5, OT Y 526) lie inside the left-wave band `[520,544]`. Every left wave — half of all
waves, one every 25s — lands a heavy DoT plus a 12.5 yd knockback on both tanks, which also breaks
tank positioning. The guide expects the tank to shift for one of the two directions.

**F4** — Lava Blaze (30643) spawns on whoever Lava Strike hit, typically a ranged bot or healer.
The off-tank branch of `SartharionTankPositionAction` only handles the three drakes; Lava Blaze
appears solely in the DPS target priority. The guide makes this an explicit off-tank job.

**F5** — With MT at `(3258.5, 532.5)` the boss settles roughly NW of the tank facing SE, and the
ranged stack at `(3248, 507)` sits about 41° off that bearing. Flame Breath's half-angle is exactly
41° at 60 yd range, so the ranged stack is on the cone boundary and small variations in where the
boss comes to rest decide whether it clips them.

Note the spot is otherwise **well chosen** and must not simply be shoved aside: it is 27.6 yd from
the main-tank spot and 26.2 yd from the off-tank spot, which is exactly the "in range of both tanks"
property the guide's diagrams call for. Preserve that when correcting the bearing.

**F9** — Twilight Whelps (30890) are a DPS target only. Tenebron's called-egg positions are
`(3237–3258, 513–541)` (`boss_sartharion.cpp:205-212`) — they hatch *inside* the raid stack, and
whelps apply stacking Fade Armor (60708) to whoever they reach, which will be a healer or a ranged
bot. `Sartharion-Tenebron-Whelp-Portal` makes picking them up an off-tank job. Same class of gap as
F4 and fixed in the same place.

**F10** — `IsTwilightRealmRunner` (`OSShared.h:117-128`) allows one assist ranged DPS in 10-man and
two in 25-man, and never a melee or a healer. `Vesperon-Portal-Positioning` sends melee, ranged and
a healer in, and the guide text says "all the DPS as well as some Healers". With the cap this small
the acolyte dies slowly, which matters most during a Shadron cycle when Sartharion is taking zero
damage anyway — sending more bots in costs nothing there. Widen after F1 is fixed and confirmed
working, not before; this is a tuning change and needs an in-game measurement.

**F11** — The guide separates the two tanks along the platform's **long** axis (Y, spanning 476–588).
The module separates them along the **short** axis: MT `(3258.5, 532.5)`, OT `(3230.0, 526.0)` — 28.5
yd apart in X but only 6.5 yd apart in Y. Since tsunami walls span the full X and are separated in Y,
both tanks are always in the same lane and always dodge together. Survivable once F3 lands, but it
means Sartharion and the drake pack stay level in Y for the whole fight. Not worth redesigning now;
record it.

**F6** — `TSUNAMI_LEFT_SAFE_RANGED` 504 (midpoint 508), `TSUNAMI_LEFT_SAFE_MELEE` 552 (midpoint 556),
`TSUNAMI_RIGHT_SAFE_ALL` 529 (midpoint 532). All three are inside their lanes, so this is not a bug —
but combined with `looseDistance = 4.0f` a bot can end up 4 yd from a band edge instead of 8.

**F8** — `docs/raids/obsidian-sanctum.md:29-32` claims Gap I is "killing a to-kill drake while its
acolyte or portal is still open buffs Sartharion massively". False, per `DoAction(ACTION_DRAKE_DIED)`.
Separately, the `DrakeAcolyteClear` gate it justifies is a no-op anyway, since a ground bot can
never see the phase-16 acolyte it checks for.

---

## Changes

### 1. Portal entry (F1) — `src/Ai/Raid/OS/OSTriggers.cpp`

The acolyte-alive guard has to go, but **do not replace it with "a portal exists"**. All three drakes
share the same refcounted instance portal, and Tenebron opens one every 60s with no acolyte behind it
(`boss_sartharion.cpp:1058-1074`). Keying on the portal alone would send runners in for Tenebron's
portal, where `FindTwilightRealmAcolyte` immediately returns null and the exit trigger fires — an
enter/exit ping-pong every cycle. The guide is explicit that nobody enters Tenebron's portal.

Gate instead on the two **ground-observable, phase-1** signals that an acolyte actually exists:

| Acolyte | Signal | Verified |
|---|---|---|
| Acolyte of Shadron 31218 | Sartharion has **Gift of Twilight Fire 58766** | `DoCastAOE` from Shadron, implicit target 38 (nearby entry); the boss's own `DamageTaken` reads `me->HasAura(58766)` |
| Acolyte of Vesperon 31219 | The bot has **Twilight Torment 58835** | Cast by Vesperon with implicit targets 22/15 (src → area enemy) at 50000 yd radius, so every player in the instance carries it |

Both are removed server-side the moment the matching acolyte dies, so the gate is self-clearing and
stays stateless across the repeating cycles.

Replace `AnyTwilightPortalAcolyteAlive` in `OSShared.h` with a helper along these lines, and keep the
runner cap, the boss lookup and the `FindNearestGameObject(GO_TWILIGHT_PORTAL, 100.0f)` check:

```cpp
// Acolytes live in phase 16 and are invisible from the ground, so detect them by the auras their
// presence puts on phase-1 targets instead. Tenebron's portal produces neither, which is what keeps
// runners out of it.
inline bool TwilightRealmNeedsRunner(PlayerbotAI* botAI, Player* bot)
{
    if (bot->HasAura(SPELL_TWILIGHT_TORMENT_SARTHARION))
        return true;

    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    return boss && boss->HasAura(SPELL_GIFT_OF_TWILIGHT_FIRE);
}
```

`SPELL_TWILIGHT_TORMENT_SARTHARION = 58835` is not currently in the `ObsidianSanctumIDs` enum in
`OSTriggers.h`; add it. `SPELL_GIFT_OF_TWILIGHT_FIRE = 58766` is already there and unused.

Leave `TwilightPortalExitTrigger` alone — inside the realm the bot is phase 16 and can see the
acolyte, so `FindTwilightRealmAcolyte(botAI) == nullptr` works correctly.

### 2. Tsunami wave classification (F2) — `src/Ai/Raid/OS/OSActions.cpp`

Replace the `posY == 500 || posY == 564` test in `AvoidFlameTsunamiAction::Execute` with an
orientation test, which is what the server's own tsunami spell script uses
(`boss_sartharion.cpp:1453`):

```cpp
bool isRightWave = std::fabs(unit->GetOrientation() - M_PI) < M_PI / 4;
```

Left waves are summoned with orientation `0.0f`, right waves with `3.14f`, and orientation does not
change in flight. This classifies all 15 segments correctly instead of 2 of 6. Replace the
existing comment block about int-casting with a one-liner naming the two spawn orientations.

### 3. Tanks dodge tsunamis (F3) — `src/Ai/Raid/OS/OSTriggers.cpp`

Delete `if (botAI->IsTank(bot)) { return false; }` from `FlameTsunamiTrigger::IsActive`.

No action change should be needed: tanks satisfy `botAI->IsMelee(bot)`, so they take the melee
branch and get Y 556 for left waves and Y 532 for right waves, both correct. **Confirm
`botAI->IsMelee(bot)` is true for every tank spec** (Prot warrior/paladin, Blood DK, Feral bear)
before relying on this; if any tank spec returns false, add an explicit
`botAI->IsTank(bot) || botAI->IsMelee(bot)` branch instead.

Priority already resolves correctly: `avoid flame tsunami` is `ACTION_RAID + 1` (61) and
`sartharion tank position` is `ACTION_MOVE` (30), so dodging wins while a tsunami is alive and the
tank walks back once it despawns after 13.5s.

### 4. Off-tank picks up Lava Blaze and Twilight Whelps (F4, F9) — `OSActions.cpp`, `OSShared.h`

In the off-tank branch of `SartharionTankPositionAction::Execute`, after the drake loop, sweep adds
the same way: for each `NPC_LAVA_BLAZE` (30643) and `NPC_TWILIGHT_WHELP` (30890) within ~40 yd, call
`ObsidianSanctumHelpers::ForceThreat(add, bot)` and `return Attack(add)` if not already engaged —
the same one-per-tick pattern the drake loop already uses. Reuse `FindLavaBlaze` / `FindTwilightAdd`
or add a small collect helper in `OSShared.h`; do not add a new value class.

Order matters: **drakes first, then whelps, then blazes.** Whelps beat blazes because they hatch
inside the raid stack and stack Fade Armor on healers.

Note in a comment that adds held at the off-tank spot follow the off-tank into the tsunami safe lane,
which is what stops Lava Blazes being hit and enraging.

### 5. Positioning constants (F5, F6) — `src/Ai/Raid/OS/OSActions.h`

```cpp
const float TSUNAMI_LEFT_SAFE_MELEE  = 556.0f;  // midpoint of left-wave gap (544,568)
const float TSUNAMI_LEFT_SAFE_RANGED = 508.0f;  // midpoint of left-wave gap (496,520)
const float TSUNAMI_RIGHT_SAFE_ALL   = 532.0f;  // midpoint of right-wave gap (520,544)
```

For F5, move the ranged stack west so it is not collinear with the main tank as seen from the boss,
while staying in the Y≈508 lane and keeping the both-tanks-in-range property from F5's note:

```cpp
const std::pair<float, float> SARTHARION_RANGED_POSITION = {3240.0f, 508.0f};
```

That gives 26.4 yd to the main-tank spot and 20.6 yd to the off-tank spot — still in range of both,
with the ranged stack clearly off the boss's front axis. This is a recommended starting value, not a
measured one; the boss's resting position depends on chase geometry. **Verify in-game** that ranged
stay inside 30 yd of Sartharion and clear of the Flame Breath cone, and adjust X before merging.

Leave `SARTHARION_MAINTANK_POSITION` and `SARTHARION_OFFTANK_POSITION` unchanged. Both are
right-wave safe and, with change 3, the tanks now step out of left waves. Moving the MT north to
the Y≈544 band boundary would halve the dodge distance but pushes the boss out of ranged range;
record that as a tuning option rather than doing it.

### 6. Remove the drake config (F7)

| File | Change |
|---|---|
| `conf/playerbots.conf.dist:446-455` | Delete the `AiPlayerbot.SartharionDrakesAlive` block |
| `src/PlayerbotAIConfig.h:286-288` | Delete the comment and `int32 sartharionDrakesAlive;` |
| `src/PlayerbotAIConfig.cpp:732` | Delete the `std::clamp(...)` load line |
| `src/Ai/Raid/OS/OSShared.h` | Delete `keepOrder`, `DrakesToLeaveAlive`, `IsDrakeKept`, `IsDrakeToKill`, `AcolyteEntryFor`, `AcolyteAliveFor`, `DrakeAcolyteClear`; rewrite `FindDrakeToKill` |

`IsDrakeEntry` stays — `SartharionMultiplier` uses it.

New `FindDrakeToKill`: return the first alive drake in **landing order** — Tenebron 30452, then
Shadron 30451, then Vesperon 30449 — with no keep-set and no acolyte gate. Landing order is what
the guide recommends ("prioritize killing Tenebron before swapping to Shadron"), and because the
drakes land 40s apart there is usually only one up at a time. No `PlayerbotAIConfig.h` include is
needed in `OSShared.h` afterwards; drop it if nothing else uses it.

### 7. Dead code — `src/Ai/Raid/OS/OSMultipliers.cpp`

Delete the empty `if (botAI->IsMainTank(bot) && dynamic_cast<TankFaceAction*>(action)) { }` block at
lines 52-55. Drop the now-unused `MovementActions.h` include if nothing else in the file needs it.

Also fix the misleading comment at `OSActions.cpp:89-90`: `bot->SetFacingToObject(held)` sets the
*bot's* facing, not the drake's. What actually keeps Shadow Breath off the raid is where the
off-tank stands, since a drake faces its victim. State that instead.

### 8. Documentation (F8) — `docs/raids/obsidian-sanctum.md`

Rewrite. Run `/compact-docs-writer` first, per the governing-docs rule.

- Drop the config section and the whole "leave N drakes alive" model; replace with the
  pull-time-snapshot explanation and "always kill every drake that joins".
- Correct Gap I: Twilight Revenge is +25% physical damage and +25% attack speed per drake death,
  unconditional. The real gate that matters is Gift of Twilight Fire zeroing all damage to
  Sartharion until the Acolyte of Shadron dies.
- Add the verified Flame Tsunami band table and safe-lane midpoints.
- Add the cone/radius table and note that `rear flank` at 90°–120° is validated against it.
- Add the phase-16 constraint on acolytes and eggs, and note that this is why portal entry keys off
  the portal GameObject rather than the acolyte.
- Note that Berserk is 30% and unconditional on AzerothCore.
- Record the role layout the guide's diagrams establish (MT on the boss at one end; off-tank holding
  drakes **plus whelps plus Lava Blazes** at the other; melee on the off-tank's pack; healers and
  ranged in the middle, in range of both tanks; nobody enters Tenebron's portal) and note that the
  diagrams are schematic, so only the wave-wall counts should be read as geometry.
- Record the two ground-observable acolyte signals (58766 on the boss, 58835 on the raid) and why
  they replace a direct acolyte lookup.
- Resolve the open DBC question at the end of the current doc: 57620 is the phase shift, 57874 is
  the triggered −25% damage/DoT, and the two are linked. Remove that "to confirm" section.

---

## Verification

The module cannot be compiled headless in this environment, so verification is static plus an
in-game pass handed to the user.

**Static, before hand-off:**

1. `grep -rn "sartharionDrakesAlive\|DrakesToLeaveAlive\|IsDrakeKept\|IsDrakeToKill\|DrakeAcolyteClear\|AnyTwilightPortalAcolyteAlive\|AcolyteEntryFor\|AcolyteAliveFor" modules/mod-playerbots/`
   must return nothing.
2. `grep -rn "keepOrder" modules/mod-playerbots/` must return nothing.
3. Confirm `OSShared.h` still compiles conceptually: every remaining helper's includes are still
   present, and `IsDrakeEntry` is still exported for `OSMultipliers.cpp`.
4. Re-read `AvoidFlameTsunamiAction` and check each branch's target Y against the band table above.
5. Confirm `botAI->IsMelee(bot)` returns true for all four tank specs (grep the `IsMelee`
   implementation in `PlayerbotAI.cpp` and check it against tank talent specs).

**In-game, on map 615 with a bot raid:**

1. Pull Sartharion with all three drakes alive. Confirm all three get picked up by the off-tank and
   dragged to `(3230, 526)`, and that bots kill Tenebron, then Shadron, then Vesperon.
2. Watch a left wave and a right wave. Confirm **both tanks** move, that melee end up near Y 556
   (left) / Y 532 (right) and ranged near Y 508 (left) / Y 532 (right), and that nobody takes
   Flame Tsunami damage. Repeat until at least three of each direction have been observed — the
   old bug is intermittent by nature.
3. When Shadron is called, confirm a designated runner enters the portal within a few seconds,
   kills the Acolyte of Shadron, and that Gift of Twilight Fire drops off Sartharion so his health
   starts moving again. Confirm the runner leaves the realm afterwards.
4. **Confirm no runner enters on Tenebron's portal.** Tenebron opens the shared portal every 60s
   with no acolyte behind it; a runner entering and immediately leaving means the aura gate from
   change 1 is not working. Watch at least two Tenebron portal cycles with no drake but Tenebron up.
5. Trigger Lava Strike and confirm the off-tank picks up the Lava Blaze rather than leaving it on
   the ranged bot it spawned on. Let Tenebron's eggs hatch and confirm the off-tank collects the
   Twilight Whelps instead of letting them chew on the healers — check no bot accumulates Fade
   Armor (60708) stacks.
6. Confirm the kill awards the Twilight Zone achievement (three drakes alive at pull, all killed
   during the fight) — this is the direct test of the config-removal premise.
7. Repeat the pull with drakes pre-killed to confirm Sarth+0 still works with no config present.

Once 1–7 pass, measure how long the Acolyte of Shadron survives with the current runner cap. If
Sartharion spends more than a few seconds immune per cycle, widen `IsTwilightRealmRunner` per F10
(admit melee DPS and a healer) and re-measure. Do not change the cap before the rest is verified —
it would confound the F1 result.
