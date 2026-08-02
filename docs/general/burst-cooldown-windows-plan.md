# Hold Burst Cooldowns Until the Tank Has Hold, and for Boss DPS Checks

## Context

Playerbot DPS currently fire their burst cooldowns (Bloodlust/Heroism, Recklessness, Icy Veins,
Avenging Wrath, Army of the Dead, …) as soon as their class trigger says "boss target, balance
below threshold". That happens on the pull, before the main tank has threat, which:

- wastes the raid-wide Bloodlust/Heroism on the first seconds of a fight,
- pulls bots off the tank and gets them killed,
- burns cooldowns that a later phase actually needs.

Two changes are wanted:

1. **Default (all content):** hold burst until the main tank has hold of the boss, then let
   everything fire together so the burst stacks.
2. **Per-boss DPS checks:** on encounters with a real burn phase, keep holding until that phase
   and dump then.

## Decisions (confirmed with user)

- **"Tank has hold" =** the boss's victim is the group main tank, continuously, for a short dwell.
  A tank swap or tank death re-arms the gate.
- **Naxxramas bosses getting a dedicated burst window:** Kel'Thuzad, Sapphiron, Thaddius, Loatheb,
  Noth, Gothik.
- **Patchwerk and Four Horsemen get no boss-specific code** — they are pure enrage races, so the
  default "burst once the MT has hold" rule is already the correct behaviour.
- Anub'Rekhan, Faerlina, Maexxna, Grobbulus, Gluth, Heigan and Razuvious have no meaningful DPS
  check and are left to the default rule as well.

## Background

### Where burst cooldowns come from

`BoostTrigger` (`src/Ai/Base/Trigger/GenericTriggers.h:429`) is the shared base for most class
burst CDs; the `BOOST_TRIGGER` / `BOOST_TRIGGER_A` macros wrap it. `ShamanBoostTrigger`
(`src/Ai/Class/Shaman/ShamanTriggers.cpp:475`) overrides `IsActive()` and already restricts
level-60+ shamans to dungeon/world bosses only. A few bursts are plain `BuffTrigger` (bestial
wrath, blade flurry, summon gargoyle), so gating at the trigger layer would miss them.

### Gating happens at the Action layer via Multipliers

`Multiplier::GetValue(Action*)` returning `0.0f` suppresses an action, and the engine multiplies
all active multipliers — so several independent gates AND together naturally. Existing precedent
for exactly this problem, lust-only and per-boss:

- `src/Ai/Raid/Gruul/GruulMultipliers.cpp:13` — `HighKingMaulgarDelayBloodlustAndHeroismMultiplier`
- `src/Ai/Raid/ToC/ToCMultipliers.cpp:130` — `AnubarakDelayBloodlustUntilLeechingSwarmMultiplier`

Both use `dynamic_cast<CastBloodlustAction*>` / `<CastHeroismAction*>`. That does not scale to ~25
cooldowns across 10 classes — it would mean including every class action header.

### Action names are reliable identifiers

`CastSpellAction::CastSpellAction(botAI, spell)` calls `Action(botAI, spell)`
(`src/Ai/Base/Actions/GenericSpellActions.cpp:138`), so `action->getName()` returns the spell name
string used in the strategy tables ("bloodlust", "recklessness", "icy veins", …). Racials and
trinkets follow the same convention: `"berserking"`, `"blood fury"`, `"use trinket"`
(`src/Ai/Base/Strategy/RacialsStrategy.cpp:60-107`). Name matching is the right identification
mechanism here.

### Where an always-on strategy is registered

- the `creators[...]` table in `src/Ai/Base/StrategyContext.h`
- the default combat list at `src/Bot/Factory/AiFactory.cpp:289`:
  `engine->addStrategiesNoInit("racials", "chat", "default", "cast time", "potions", "duel", "boost", nullptr);`
  — deliberately skipped for battlegrounds, and the new gate should be skipped there too.

### Main tank lookup

`PlayerbotAI::GetMainTankGuid(Group*)` is the primitive; `GetGroupMainTank()`
(`src/Ai/Raid/RaidBossHelpers.cpp:138`) wraps it. The new base-layer helper must call
`GetMainTankGuid` directly so `src/Ai/Base` does not gain a dependency on `src/Ai/Raid`.

### Naxx phase detection already available

In `src/Ai/Raid/Naxx/NaxxBossHelper.h`:

| Helper | Method | Line |
|---|---|---|
| `KelthuzadBossHelper` | `IsPhaseOne()` / `IsPhaseTwo()` (`UNIT_FLAG_NON_ATTACKABLE`) | 172 |
| `SapphironBossHelper` | `IsPhaseGround()` / `IsPhaseFlight()` (`IsFlying()`) | 825 |
| `ThaddiusBossHelper` | `IsPhasePet()` / `IsPhaseTransition()` / `IsPhaseThaddius()` | 1413 |
| `LoathebBossHelper` | boss resolution only | 1116 |

`NothBossHelper` is **commented out** (lines 1191-1240) and there is **no** `GothikBossHelper` at
all — only a commented `GothikGenericMultiplier` (`src/Ai/Raid/Naxx/NaxxMultipliers.h:98`). Those
were disabled together with their positioning logic. **Do not revive them** — the burst gate only
needs a one-line phase check, verified against the core scripts:

- Noth: `boss_noth.cpp:99/115/174` sets and clears `UNIT_FLAG_NOT_SELECTABLE` for the balcony phase.
- Gothik: `boss_gothik.cpp:232` sets `UNIT_FLAG_DISABLE_MOVE` on engage (balcony / add waves) and
  `:481` removes it on the phase-2 landing.

### Loatheb caveat

The spore crit buff (Fungal Creep, 29232) is **not** referenced anywhere in
`src/server/scripts/Northrend/Naxxramas/boss_loatheb.cpp` — only `SPELL_SUMMON_SPORE = 29234`. The
aura may or may not land on this core, so the Loatheb window needs a time-based fallback or the
cooldowns would be held for the entire fight.

### Build note

The module has no `CMakeLists.txt`; AzerothCore globs `src/**`, so new files are picked up with no
build-file edit.

## Changes

### 1. Shared burst helper — new files

`src/Ai/Base/Combat/BurstCooldowns.h` / `.cpp`

```cpp
bool IsBurstCooldownAction(std::string const& actionName);

// Caller-owned dwell state, keyed on the boss guid so a different boss re-arms the gate.
struct BurstHoldState { ObjectGuid boss; uint32 sinceMs = 0; void Reset(); };

// True when the boss's victim has been the group main tank continuously for at least dwellMs.
// State is zeroed whenever the victim is not the main tank.
bool MainTankHasHeldBoss(Player* bot, Unit* boss, BurstHoldState& state, uint32 dwellMs);
```

It takes the name rather than the `Action*` because `Action::getName()` returns by value, and both
callers sit on a per-action-per-tick path.

`IsBurstCooldownAction` matches the name against a file-static
`std::unordered_set<std::string>`. **Offensive throughput cooldowns and lust only** — tank
survival CDs and healer mana CDs must not be gated:

| Source | Names |
|---|---|
| raid-wide / racial / item | `bloodlust`, `heroism`, `berserking`, `blood fury`, `use trinket` |
| Warrior | `recklessness`, `death wish` |
| Rogue | `adrenaline rush`, `blade flurry` |
| Mage | `arcane power`, `icy veins`, `combustion`, `mirror image`, `presence of mind` |
| Warlock | `metamorphosis` |
| Hunter | `rapid fire`, `bestial wrath`, `readiness` |
| Priest | `shadowfiend` |
| Druid | `berserk`, `force of nature` |
| Paladin | `avenging wrath` |
| Death Knight | `killing machine`, `army of the dead`, `summon gargoyle` |
| Shaman | `fire elemental totem`, `elemental mastery` |

Deliberately **excluded**: `unbreakable armor` and `dancing rune weapon` (tank mitigation),
`inner focus` (healer mana).

`MainTankHasHeldBoss` reads `boss->GetVictim()`, compares its GUID against
`botAI->GetMainTankGuid(bot->GetGroup())`, and uses `state.sinceMs` to measure a continuous dwell —
reset to 0 whenever the victim is not the MT, so a tank swap or a tank death re-arms the gate. The
state also carries the boss GUID, and the caller must `Reset()` on every path that skips this call;
without that, the previous boss's timer satisfies the dwell instantly on the next pull.

### 2. Default rule — new always-on strategy

`src/Ai/Base/Strategy/BurstWindowStrategy.h` / `.cpp`

- `class BurstWindowStrategy : public Strategy` — `getName()` returns `"burst"`, **no triggers**,
  `InitMultipliers` pushes one `HoldBurstUntilTankEngagedMultiplier`.
- `HoldBurstUntilTankEngagedMultiplier::GetValue(Action*)`, in this order — the boss context is all
  cheap pointer work, the name lookup below it copies a `std::string`:
  1. `Unit* target = AI_VALUE(Unit*, "current target");` — then, in one combined check: no target,
     not a boss (`Creature* c = target->ToCreature(); c->IsDungeonBoss() || c->isWorldBoss()`, the
     same predicate `ShamanBoostTrigger::IsActive` uses), no group (solo bots are unaffected), or
     the bot is the MT (never gated on itself) → `holdState.Reset()` and return `1.0f`. The reset
     is what stops the previous boss's dwell from carrying into the next pull.
  2. `if (!IsBurstCooldownAction(name)) return 1.0f;`
  3. `if (name == "use trinket" && !PlayerbotAI::IsDps(bot)) return 1.0f;` — `use trinket` is the
     generic trinket action, so for a healer or an off-tank it also covers survival and mana
     trinkets, which have to stay available through the pull.
  4. Dwell: **3000 ms** for `bloodlust` / `heroism`, **4000 ms** for everything else. The 1 s
     stagger gets lust out first so the personal cooldowns land inside the haste window.
  5. Return `1.0f` once the dwell is met, `0.0f` otherwise.

Every bot's gate opens on the same tick, so the bursts stack without any cross-bot coordination.

**Wiring — 2 sites:**

- `creators["burst"] = &StrategyContext::burst;` plus the static factory, in
  `src/Ai/Base/StrategyContext.h`
- add `"burst"` to the `addStrategiesNoInit(...)` list at `src/Bot/Factory/AiFactory.cpp:289`
  (the non-battleground branch only)

Being a named strategy, it is switchable per bot with the usual `strategy -burst` chat command.

### 3. Naxxramas burst windows

One new multiplier, `NaxxBurstWindowMultiplier`, in `src/Ai/Raid/Naxx/NaxxMultipliers.h` / `.cpp`,
registered in `RaidNaxxStrategy::InitMultipliers` (`src/Ai/Raid/Naxx/NaxxStrategy.cpp:208`).

A single multiplier rather than six per-boss ones: the shared
`if (!IsBurstCooldownAction(action->getName())) return 1.0f;` early-out runs once instead of six
times, and six near-identical `UpdateBossAI()` sweeps per action per tick would be wasteful. It
holds `KelthuzadBossHelper`, `SapphironBossHelper`, `ThaddiusBossHelper` and `LoathebBossHelper` as
members, matching how `KelthuzadGenericMultiplier` holds its helper.

The boss sweep itself lives in a private `EvaluateWindow()` whose result is cached per millisecond
(`cachedAtMs` / `cachedValue`), so a tick with several queued cooldowns runs it once. Every helper
resolves through `"find target"`, which utf8-lowercases the bot's whole threat list on a 1 ms cache,
and Thaddius alone costs two lookups plus three RTI icon resolutions per call.

`EvaluateWindow()` calls `loatheb.UpdateBossAI()` **first** and clears `loathebFightStartMs` when it
is false, before the other bosses get a chance to take an early return — otherwise the Loatheb
fallback timer survives into a later attempt and is already expired at the pull.

After the early-out, resolve whichever boss is present and apply:

| Boss | Suppress burst while | Release when |
|---|---|---|
| Kel'Thuzad | `helper.IsPhaseOne()`, or phase 2 above 45 % HP | `IsPhaseTwo() && GetHealthPct() <= 45.0f` — matches `HealthBelowPct(45)` in `boss_kelthuzad.cpp`, where the Guardians of Icecrown start spawning |
| Sapphiron | `helper.IsPhaseFlight()` | ground phase — never burn burst while the boss is untargetable in the air |
| Thaddius | `helper.IsPhasePet()` or `helper.IsPhaseTransition()` | `helper.IsPhaseThaddius()` — saved for the 5-minute enrage race on the boss |
| Loatheb | no Fungal Creep on the bot **and** < 45 s since this bot entered the Loatheb fight | the bot has the spore buff (id `29232`, falling back to `botAI->HasAura("fungal creep", bot)`), **or** the 45 s fallback expires |
| Noth | `boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE)` (balcony) | ground phase |
| Gothik | `boss->HasUnitFlag(UNIT_FLAG_DISABLE_MOVE)` (balcony / add waves) | phase 2, boss has landed |

Noth and Gothik are resolved inline with
`AI_VALUE2(Unit*, "find target", "noth the plaguebringer")` and `"gothik the harvester"` plus the
flag check — **no** new helper class, and the commented-out `NothBossHelper` /
`GothikGenericMultiplier` blocks stay untouched.

Loatheb's 45 s fallback exists because Fungal Creep is not scripted in this core's
`boss_loatheb.cpp`; without it the cooldowns would never be released on that fight.

Add `Fungal Creep = 29232` to the Loatheb block of `src/Ai/Raid/Naxx/NaxxSpellIds.h:125`.

### Interaction with the existing per-boss lust gates

`HighKingMaulgarDelayBloodlustAndHeroismMultiplier`, `GruulTheDragonkillerDelayBloodlust...` and
`AnubarakDelayBloodlustUntilLeechingSwarmMultiplier` stay as they are. Multipliers multiply, so the
new default gate ANDs with them correctly — no behaviour change on those fights beyond the added
tank-hold delay.

## Verification

The module cannot be compiled headless in this environment, so verification is static review plus
an in-game pass on a running server.

**Static**

1. Every name in the `IsBurstCooldownAction` list resolves to a real `NextAction(...)` — grep each
   one under `src/Ai/Class/*/Strategy/` and `src/Ai/Base/Strategy/RacialsStrategy.cpp`.
2. No `#include` from `src/Ai/Base/**` into `src/Ai/Raid/**`.
3. `BurstWindowStrategy` is reachable: `"burst"` present in the `StrategyContext` creators **and**
   in the `AiFactory.cpp:289` default list.
4. Every early return in `HoldBurstUntilTankEngagedMultiplier::GetValue` that skips
   `MainTankHasHeldBoss` calls `holdState.Reset()` first, and `EvaluateWindow()` clears
   `loathebFightStartMs` on a path no other boss branch can bypass.

**In-game (server build required)**

1. Build the core with the module, start a server, spawn a raid group of bots including a shaman.
2. Naxxramas trash pull: the gate is a no-op on non-boss targets, so cooldowns fire on normal
   cooldown logic — confirm no regression.
3. Patchwerk: bots hold everything for ~3-4 s after the pull, then lust and personal cooldowns go
   off together once the MT has aggro.
4. Kel'Thuzad: nothing fires in phase 1; in phase 2 nothing fires until 45 %, then everything
   dumps as the Guardians spawn.
5. Sapphiron: no burst during an air phase; burst resumes on landing.
6. Thaddius: nothing during Feugen/Stalagg or the transition; everything on the boss.
7. Noth / Gothik: nothing while the boss is on the balcony; burst on the ground / phase-2 landing.
8. Loatheb: burst goes out either on the first spore buff or 45 s in, whichever comes first —
   confirm the cooldowns are not held for the whole fight.
9. A solo bot against a world boss, and a bot with no group: the gate must be inert.
10. `strategy -burst` on a bot restores the old behaviour — confirms the strategy is switchable.
