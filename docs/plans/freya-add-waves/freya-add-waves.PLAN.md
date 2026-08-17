# Freya (Ulduar) — fix the add-wave failures found in testing

## Context

The trio-sync change (commit `2dda9eb`) landed and was tested in-game. The trio now dies together, but
three failures showed up in the other two wave types:

1. The off-tank collects Detonating Lashers and walks them into the raid stack, where they detonate
   and kill bots.
2. Lashers also land on DPS bots, apparently at random.
3. The Ancient Conservator is never brought to a Healthy Spore, so melee and the add tank spend the
   whole wave pacified and do nothing.

Intended outcome: lasher waves stop wiping the raid, and the Conservator wave produces damage instead
of a pacified standstill.

## Ground truth (verified — do not re-derive)

`src/server/scripts/Northrend/Ulduar/Ulduar/boss_freya.cpp`:

- **Detonating Lashers cannot be tanked.** `EVENT_DETONATING_LASHER_FLAME_LASH` (`:1257-1272`) fires
  every 10 s: Flame Lash on the victim, then **`DoResetThreatList()`** and
  `AttackStart(SelectTargetFromPlayerList(80))` — a random player within 80 yd. Spawn does the same
  (`:1108-1113`: 5 s submerged, then a random player). Misdirection and Tricks are erased inside 10 s,
  and there are 10 lashers against one 30 s-cooldown redirect, so **threat redirection cannot hold
  lashers**. Report 2 is the encounter working as designed, not a bot defect.
- **Waves are one group type each** (`SpawnWave`, `:366-401`): `GROUP_TRIO`, `GROUP_CONSERVATOR`
  (1 add), or `GROUP_LASHERS` (**10** lashers). The 60 s wave clock still lets two waves overlap.
- **Conservator's Grip pacifies the whole raid.** Cast once at 6 s with no repeat (`:1205`, `:1236`).
  Spell 62532: effect 129 `APPLY_AREA_AURA_ENEMY`, aura 60 `MOD_PACIFY_SILENCE`, mechanic 9, radius
  index 28 = **50000 yd**. Pacify blocks melee swings as well as casts, so tanks are hit as hard as
  casters — the paladin/DK observation is the mechanic, not a class quirk.
- **Spores spawn 20 yd from the Conservator.** 62566 is *Healthy Spore Summon Periodic*, an 8 s
  periodic on the Conservator triggering three directional summons (62582 / 62591 / 62592), each
  `SPELL_EFFECT_SUMMON` of 33215 at radius index 9 = **20 yd**. Potent Pheromones 64321 is a **6 yd**
  ally area aura (aura 77 mechanic-immunity to silence, aura 79 +24 % damage). Spores despawn after
  22 s (`:1056-1065`).
  → Melee can never be sheltered and in range at once unless the tank parks the Conservator on a
  spore. This is report 3.
- **Nature Bomb runs the whole fight**, not just hard mode: `EVENT_FREYA_NATURE_BOMB` (`:645-660`)
  repeats every **18 s**, dropping 7-10 bombs in 25 m (3-4 in 10 m) at the feet of players within
  70 yd. Any deliberate stack will be bombed regularly.

DBC / `acore_world`:

| Fact | Value |
|---|---|
| Detonate 62598 | 4162 + 675 roll, radius index 18 = **15 yd**, **no `spelldifficulty` entry** — identical in 10 m and 25 m |
| Detonating Lasher HP | **71 682** (32918, 10 m) / **234 594** (33399, 25 m) → wave total 717 k / **2.35 M** |
| Ancient Conservator HP | 522 900 (33203) / 2 215 610 (33376) |

Module config: `possible targets no los` is a `NearestUnitsValue` bounded by
`sPlayerbotAIConfig.sightDistance`, **100 yd measured per bot**
([PlayerbotAIConfig.cpp:112](modules/mod-playerbots/src/PlayerbotAIConfig.cpp#L112)). So
`state.detonatingLashers` differs bot to bot, and raid-wide agreement on a lasher is not free the way
it is for the trio.

## Root cause of report 1

[UldMultipliers.cpp:174-180](modules/mod-playerbots/src/Ai/Raid/Uld/UldMultipliers.cpp#L174-L180)
silences generic `TankAssistAction` for assist tank 0 **only when `GetFreyaTankTarget` returns
non-null**, and
[UldBossHelper.cpp:767-780](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L767-L780)
only ever returns the Snaplasher or the Conservator. On a pure lasher wave it returns `nullptr`, so
generic tank assist takes over, the off-tank engages lashers, and generic positioning walks it back to
the raid with them in tow.

The same function returns `nullptr` for the **main tank** in every wave while the multiplier zeroes its
tank assist unconditionally — so the main tank has no targeting node at all today. It holds Freya only
because it pulled her. Latent; fixed here.

## Design decisions (settled with the user — do not relitigate)

- **Lashers**: ranged DPS focus-fire one lasher at a time; melee hit whatever lasher is already on top
  of them and never chase. No raid-wide split, no ranged-only rule.
- **Off-tank helps kill lashers**, using the same local rule as melee — but any other add outranks
  them. What made it dangerous was collecting and walking them, not damaging them.
- **Tanks never flee a Detonate.** They eat it.
- **Main tank stays on Freya during the Conservator wave**, pacified. Freya is never dragged to a
  spore.
- **Melee converge on the spore the Conservator is parked on**; ranged and healers use their own
  nearest spore.
- **Threat redirection is kept**, pointed at the assist tank while it holds the Snaplasher or the
  Conservator — the half of report 2 that threat can fix.
- Nature Bomb dodging keeps winning over everything.

## Patterns being reused

- **Tank drags a mob to a position** — `IgnisConstructTankAction::Execute`
  ([UldActions_Ignis.cpp:36-75](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Ignis.cpp#L36-L75)):
  attack → taunt if not the victim → `MoveTo(..., MovementPriority::MOVEMENT_FORCED)`, with a
  "hold still once it is in range" hysteresis. Exact shape for parking the Conservator.
- **Per-encounter threat redirect** — `XT002RedirectThreatAction` / `XT002RedirectThreatTrigger`
  ([UldActions_XT002.cpp:192-256](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_XT002.cpp#L192-L256),
  [UldTriggers_XT002.cpp:159-171](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_XT002.cpp#L159-L171)),
  stood down from the generic node via the list at
  [UldMultipliers.cpp:150](modules/mod-playerbots/src/Ai/Raid/Uld/UldMultipliers.cpp#L150).
- **Sticky selection with a switch margin** — the existing
  `FreyaSetDpsPriorityAction::SelectNearestLasher` already has the 10 yd margin; it gains a range cap
  and moves to the helper so the tank path can share it.
- **Latched position state on an action** — `GruulTheDragonkillerSpreadRangedAction::_initialPosition`
  (`src/Ai/Raid/Gruul/GruulActions.cpp:550-565`) is the precedent for holding a choice across ticks.

---

## 1. New constants — `UldBossHelper.h`

Beside `ULDUAR_FREYA_DETONATE_RADIUS`:

```cpp
// Melee never chase a lasher; past this they stay on what they were hitting. Deliberately tight -
// "nearby" has to mean the lasher came to the melee group, not that the group crosses the room for it.
constexpr float ULDUAR_FREYA_MELEE_LASHER_RANGE = 12.0f;

// Detonate (62598) rolls 4162-4837 and does not scale with difficulty. Ranged focus one lasher at a
// time, so a bot in range of two blasts at once is the exception and does not set this floor.
constexpr uint32 ULDUAR_FREYA_DETONATE_FLEE_HEALTH = 5500;

// Margin before the add tank moves between two near-equal trio members, or its own damage makes it
// swap every few ticks and lose swing timers to nothing.
constexpr float ULDUAR_FREYA_TANK_TRIO_SWITCH_PCT = 5.0f;
```

## 2. Lasher targeting — `UldBossHelper.*`, `UldActions_Freya.*`

Two free functions in `UldBossHelper.cpp` beside `GetFreyaTrioAssignment`. `SelectNearestLasher` moves
off `FreyaSetDpsPriorityAction` and becomes the second one, so the tank path shares it:

```cpp
// Lowest health, GUID breaking ties: raid-wide agreement with no shared state, and self-stabilising -
// the add being focused stays the lowest, so the pick does not flicker between bots.
Unit* GetFreyaRangedLasherFocus(FreyaWaveState const& state);

// Nearest living lasher inside ULDUAR_FREYA_MELEE_LASHER_RANGE, sticky on currentTarget with a 10 yd
// switch margin. Also the leash: a lasher that runs out of range is dropped, which is what stops a
// melee bot being towed across the room every time the add retargets.
Unit* GetFreyaMeleeLasherTarget(PlayerbotAI* botAI, FreyaWaveState const& state, Unit* currentTarget);
```

The sticky current target is kept only while it is *also* inside the cap — otherwise the leash never
releases.

In `ResolveFreyaDpsTarget`, the lasher slot becomes:

```cpp
Unit* lasher = PlayerbotAI::IsRangedDps(bot) ? GetFreyaRangedLasherFocus(state) : nullptr;

// The focus can be 60 yd out. Walking to it would put the bot inside the 15 yd blast, which is the one
// property that makes ranged safe here, so it takes its own nearest instead and the two converge on
// their own as lashers close on players.
if (lasher && bot->GetExactDist2d(lasher) > sPlayerbotAIConfig.spellDistance)
    lasher = nullptr;

if (!lasher)
    lasher = GetFreyaMeleeLasherTarget(botAI, state, currentTarget);
```

Rest of the priority chain unchanged. A melee bot with no lasher in range falls through to Freya, which
is the intent.

## 3. Tank targeting — `UldBossHelper.cpp`, `UldMultipliers.cpp`

`GetFreyaTankTarget` takes the bot's current target (for the trio stickiness) and gains a full ladder:

```
main tank      -> Freya
assist tank 0  -> Snaplasher
               -> Ancient Conservator
               -> highest-health non-suppressed living trio member, sticky:
                     keep the current one until another beats it by
                     ULDUAR_FREYA_TANK_TRIO_SWITCH_PCT of max health
               -> GetFreyaMeleeLasherTarget(...)      // same local rule and leash as melee DPS
               -> Freya
other tanks    -> nullptr
```

Aiming the tank at the **highest-health** trio member matters: `GetFreyaTrioAssignment` only counts
`PlayerbotAI::IsDps` members, so tank damage is invisible to the split. Putting it on the member
furthest from the floor makes that unaccounted damage help convergence instead of skewing it, and
reusing `FreyaTrioSyncSuppress` means it can never be what crosses the hard floor.

Detonating Lashers are reachable only through `GetFreyaMeleeLasherTarget`, so the tank can damage one
standing next to it but can never walk one anywhere — that is the whole fix for report 1. Storm Lasher
and Ancient Water Spirit are still never *owned*; the ladder only borrows them as a damage target.

`FreyaDisableAutomaticTargetingMultiplier` simplifies. With the Freya fallback, every tank that matters
now has a node, so `TankAssistAction` is zeroed for **any** tank while Freya is alive and the
conditional assist-tank-0 branch — the thing that let generic assist through on lasher waves — goes
away. It no longer needs to call `GetFreyaTankTarget` at all. The `DpsAssistAction` half is unchanged.
`FreyaTankAddsTrigger` likewise collapses to "Freya alive and this bot is main tank or assist tank 0".

`FreyaTankAddsAction` gates its taunt: `UldCastClassTaunt` only when the target's current victim is
**not a tank**. That recovers a dead or displaced tank without a bot taunting Freya off a live human
main tank whose raid roles are set differently.

## 4. Conservator parked on a spore — `UldBossHelper.*`, `UldActions_Freya.*`

One derivation shared by both sides, in `UldBossHelper.cpp`:

```cpp
// The spore the Conservator is being parked on. Keyed off the Conservator, never off the calling bot,
// so the tank doing the dragging and the melee walking to shelter resolve the same spore.
Unit* GetFreyaConservatorSpore(PlayerbotAI* botAI, Unit* conservator);   // nearest living 33215
```

**Tank side** — `FreyaTankAddsAction::Execute` keeps its attack/taunt head, then adds the Ignis drag,
only when the tank target is the Conservator:

```
if (!_parkedSpore alive)  _parkedSpore = GetFreyaConservatorSpore(...)   // latched ObjectGuid member
if (!_parkedSpore)                                       -> return false
if conservator within ULDUAR_FREYA_SPORE_RADIUS - 1      -> return false   // parked, hold still
MoveTo(spore, ..., MovementPriority::MOVEMENT_FORCED)
```

The latch is what makes the walk monotonic: fresh spores keep appearing 20 yd from wherever the
Conservator currently is, so recomputing every tick could flip the destination mid-walk and turn the
tank around. Cleared when the spore or the Conservator dies, then re-latched — which is also how the
22 s despawn is handled.

The latch and the melee's per-tick derivation agree without communicating: the latched spore is ≤20 yd
from the Conservator and closing, while every new spore spawns at 20 yd, so it stays the nearest for
the whole walk.

**Melee side** — `FreyaMoveToHealingSporeAction` splits by role:

- melee non-tanks → `GetFreyaConservatorSpore(...)`, for the whole wave, so there is no second trip
  and no idle window waiting for the park;
- ranged and healers → nearest living spore to themselves. They only need the aura, not melee range,
  and the spores they pick sit 20 yd from the Conservator, inside casting range of it and of the melee
  stack. Keeps the pile to the group that actually needs the boss in reach.

`FreyaMoveToHealingSporeTrigger` keeps its tank exclusion, but the rationale comment at
[UldTriggers_Freya.cpp:84](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.cpp#L84) is
now wrong and must be replaced: the assist tank is excluded because the drag already lands it inside
the aura, and letting the spore node move it as well would oscillate it between the spore nearest
itself and the spore it is dragging the boss to. The main tank is excluded because Freya is never
repositioned.

Nature Bomb keeps its `ACTION_RAID + 4` priority — a bomb hit is worse than a few pacified seconds —
and the melee rule above is what makes the recovery re-converge on one spore instead of smearing melee
across three. Re-parking the Conservator away from a bombed spore is deliberately **not** built until a
test shows how bad the churn is.

## 5. Detonate avoidance — `UldTriggers_Freya.cpp`

Replace the `Is25ManRaid() ? 7200 : 4900` split at
[UldTriggers_Freya.cpp:67](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.cpp#L67) —
Detonate has no heroic entry — with `ULDUAR_FREYA_DETONATE_FLEE_HEALTH`, plus two exclusions:

- **any tank** never flees; it eats the blast;
- the bot whose current target *is* that lasher never flees — it is inside 15 yd by definition and
  cannot do its job otherwise.

## 6. Threat redirect — new node + `UldMultipliers.cpp`

`FreyaRedirectThreatTrigger`: hunter or rogue, Freya alive.

`FreyaRedirectThreatAction`, copied from `XT002RedirectThreatAction`:

```
Snaplasher or Ancient Conservator alive -> GetGroupAssistTank(botAI, bot, 0)
else Freya's victim, if it is a tank    -> that tank      // survives a swap or a tank death
else                                    -> GetGroupMainTank(botAI, bot)
```

Rogue casts `tricks of the trade`; hunter casts `misdirection` and spends leftover charges on Freya
with `steady shot`, exactly as the XT-002 action does. Kept deliberately simple rather than "the tank
of my current target" — Storm Lasher and Water Spirit are unowned, so that variant resolves to nobody
for precisely the case it would be built for.

Add `NPC_FREYA` to the `noRedirectBosses` sweep in `UldThreatRedirectMultiplier`
([UldMultipliers.cpp:375-390](modules/mod-playerbots/src/Ai/Raid/Uld/UldMultipliers.cpp#L375-L390)) so
the class-generic main-tank redirect stands down, and whitelist `"freya redirect threat action"`
wherever the encounter's own retargets are exempted.

## 7. Registration — `UldTriggerContext.h`, `UldActionContext.h`, `UldStrategy.cpp`

One new pair; everything else modifies existing nodes.

| Node | Priority | Gate |
|---|---|---|
| `freya redirect threat` | `ACTION_RAID + 1` | hunter/rogue, Freya alive |

Same tier as `freya tank adds`; the two never both apply to one bot, and the action returns `false`
when it cannot cast so the chain continues. All other priorities stay as set in `2dda9eb`. No CMake
change — everything lands in existing files.

## 8. Docs — `docs/raids/ulduar.md`

Update the Freya section with: the 10 s threat reset that makes lashers untankable, the raid-wide
pacify (not a caster-only mechanic), the 20 yd spore geometry against the 6 yd aura and why the
Conservator gets parked, the ranged-focus / melee-local lasher rule, and the 18 s Nature Bomb cadence
as the reason the melee stack is expected to be disrupted. Fold the interrupt gap row into the same
pass if it is still open.

## Deliberately out of scope

- **Interrupts** for Storm Lasher (62649 / 62648) and Ancient Water Spirit (62653).
- **Owning Storm Lasher / Ancient Water Spirit** — owning them means owning Tidal Wave positioning.
- **Dragging Freya**, and **re-parking the Conservator away from a Nature Bomb**, per the decisions
  above.

## Verification

The module cannot be compiled headless here, so the build is a hand-off.

1. Static checks: each new trigger/action name appears in exactly three places (the `creators[...]`
   map, the factory method, the `TriggerNode`); no tank path can reach a lasher except through
   `GetFreyaMeleeLasherTarget`; the multiplier no longer calls `GetFreyaTankTarget`.
2. Build the worldserver with the module.
3. **Lasher wave, 25 m.** No tank ever walks a lasher anywhere — an off-tank swinging at one standing
   next to it is correct, an off-tank towing three toward the raid is the bug. Ranged bots all attack
   the same lasher and it dies before the next is touched. Melee stay put unless a lasher is inside
   ~12 yd. The failure signature is several bots dropping at once.
4. Confirm the wave still clears inside 60 s. A second lasher wave stacking on the first means the
   12 yd melee cap is starving the wave; widen `ULDUAR_FREYA_MELEE_LASHER_RANGE`.
5. **Conservator wave.** The assist tank walks the Conservator onto one spore and holds it; melee
   gather on that same spore and keep swinging; ranged and healers spread to other spores. Nobody
   stands pacified and idle except the main tank.
6. Watch a spore expire at 22 s — the tank should step the Conservator to a fresh one **once**, and
   melee should follow to the same one, not oscillate between two.
7. Confirm melee bots carry `Potent Pheromones` (64321) for most of the wave, and that a Nature Bomb
   landing on the stack scatters them and they re-converge on the parked spore rather than splitting up.
8. Confirm hunters/rogues redirect to the assist tank while the Snaplasher or Conservator is up, and to
   the Freya tank otherwise.
9. **Trio wave regression**: `EMOTE_TRIO_WITHERS` three times with no `EMOTE_TRIO_REGENERATES` between
   them, as before. Watch that the off-tank joining a trio member after the Snaplasher dies has not
   broken the sync.
10. Confirm `Attuned to Nature` (62519) still reaches 0 so the final phase starts.
11. Repeat in 10 m and with `UlduarFreyaHardMode = true`.
