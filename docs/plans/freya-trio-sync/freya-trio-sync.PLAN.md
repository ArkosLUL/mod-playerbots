# Freya (Ulduar) playerbot strategy — close the guide gaps

## Context

Bots cannot clear Freya because the **Ancient Water Spirit / Storm Lasher / Snaplasher** wave never
dies. Each member starts its own **11-second** revive timer on death and comes back unless all three
are down when that timer expires
(`src/server/scripts/Northrend/Ulduar/Ulduar/boss_freya.cpp:1155-1198`), so the wave loops forever
and Freya's `Attuned to Nature` stacks never drop.

Two defects cause it:

1. **The Snaplasher is deliberately excluded from targeting**
   (`src/Ai/Raid/Uld/Action/UldActions_Freya.cpp:105-114`). The comment justifying this — that
   funnelling the raid onto it "would make it invulnerable" — is **wrong**. Verified against the DBC
   reference: `Hardened Bark` (62663, 99 stacks) applies `SPELL_AURA_MOD_DAMAGE_PERCENT_DONE +10%`
   and a movement slow. It raises the Snaplasher's **outgoing** damage. It never reduces damage
   taken.
2. **The Snaplasher is the fattest member of the trio.** From `acore_world` (`difficulty_entry_1`
   templates, `basehp2 × HealthModifier`):

   | Add | Entry (10m / 25m tpl) | 10-man HP | 25-man HP |
   |---|---|---|---|
   | Snaplasher | 32916 / 33400 | 312 792 | **977 475** |
   | Storm Lasher | 32919 / 33401 | 234 594 | 781 980 |
   | Ancient Water Spirit | 33202 / 33398 | 188 748 | 524 300 |

The biggest add gets zero focus, the two smaller ones die, and both revive.

A third defect keeps the node permanently hot: the trigger picks the highest-health trio member
(`src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.cpp:113-138`) while the action refuses to ever mark the
Snaplasher, so trigger and action can never agree.

Intended outcome: bots kill the trio inside the revive window, and the remaining guide mechanics
(Conservator/spores for **all** roles, Detonating Lashers, Eonar's Gift, add tanking) are handled
rather than left to generic targeting.

## Ground truth (verified — do not re-derive)

`src/server/scripts/Northrend/Ulduar/Ulduar/boss_freya.cpp`:

- **Waves spawn every 60s unconditionally** (`EVENT_FREYA_ADDS_SPAM`, `events.Repeat(1min)`,
  `:612-623`), capped at 6. A wave not cleared in 60s gets a second wave stacked on top. In 25-man
  the trio is 2.28M HP, so the raid needs a ~38k DPS floor to stay ahead.
- **Lifebinder's Gift repeats every 45s** (`:625-629`), so Eonar's Gift *will* overlap a trio kill.
  The Gift is 65 165 HP in 25-man (33385), 19 550 in 10-man (33228).
- Trio revive: per-member `m_Events` at **11s** after death; `ReviveWithAllies()` aborts when
  `DATA_TRIO_DOWN >= 3` (`:1163-1198`). A revived member does **not** remove stacks again
  (`_hasDied`, `:1129`).
- `SetEntry(Entry) // normal entry always`
  (`src/server/game/Entities/Creature/Creature.cpp:509`) — `GetEntry()` returns the **base** entry
  in 25-man, so existing entry constants are correct. Only HP comes from the difficulty template.
- Eonar's Gift window is **12s**, not the 10s guides quote (`:1023-1028`). Healthy Spore despawns
  after 22s (`:1058-1063`).

DBC reference (`modules/mod-spell-tweaks/data/dbc-reference/spell.reference.csv`):

| Spell | ID | What it actually does |
|---|---|---|
| Conservator's Grip | 62532 | `APPLY_AREA_AURA_ENEMY` + `MOD_PACIFY_SILENCE`, radius index 28 = **50000 yd** → raid-wide, cannot be outranged |
| Potent Pheromones | 64321 (triggered by 62541) | ally area aura, radius index 29 = **6 yd**, mechanic immunity + damage bonus |
| Hardened Bark | 62663 | 99 stacks, +10% **damage done**, movement slow. Applied by proc 62664 |
| Detonate | 62598 | radius index 18 = **15 yd**, ~4162 base |
| Attuned to Nature | 62519 | 150 stacks, healing received |

## Design decisions (settled — do not relitigate)

- **Direct targeting, no raid icons.** Every `SetTargetIcon` / `GetTargetIcon` call in the Freya
  files is deleted. A human's own marks are *not* honoured during the encounter.
- **Convergence comes from a three-way split, not from throttling.** DPS bots are distributed
  across the three trio members by a per-tick greedy load balance. The floor suppression is a
  backstop for damage the targeting cannot steer, not the mechanism.
- **A floor-suppressed member sheds its bots** rather than idling them — no bot ever stands still.
- Helpers stay Freya-local in `UldBossHelper`; no shared "kill these N together" abstraction, and
  Thaddius (`NaxxBossHelper.h:2892-2934`) is left untouched.
- No config flag. The behaviour being replaced cannot clear the encounter, so there is nothing to
  fall back to.
- Interrupts are **out of scope** for this change.

## Patterns being reused

- **SWP M'uru direct targeting** — `MuruSetDpsPriorityAction::Execute` / `ResolveMuruDpsTarget` /
  `SelectMuruEncounterTarget` (`src/Ai/Raid/SWP/Action/SWPActions_Muru.cpp:178-380`), state gathered
  by `GatherMuruEncounterTargets` (`src/Ai/Raid/SWP/Util/SWPEncounter_Muru.h:24-52`), generic picker
  stood down by `MuruDisableDefaultTargetingMultiplier` (`SWPMultipliers.cpp:712-742`).
- **Thaddius sync shape** — `PetSyncSuppress` (`src/Ai/Raid/Naxx/NaxxBossHelper.h:2892-2934`),
  consumed at `src/Ai/Raid/Naxx/NaxxMultipliers.cpp:196-203`. Copied in shape only.
- **Kologarn** gives the Ulduar naming precedent for a targeting-override multiplier:
  `KologarnDisableAutomaticTargetingMultiplier` (`src/Ai/Raid/Uld/UldMultipliers.h:80-91`).

---

## 1. Helper block — `src/Ai/Raid/Uld/Util/UldBossHelper.h` / `.cpp`

Freya is currently the only Ulduar boss with no entries in `UldBossHelper.cpp`; every lookup is
inlined in the trigger/action files. Add a Freya section beside `GetAuriayaFocusTarget`
(`:423-433`) and `GetRazorscaleAddKillTarget` (`:565-574`).

Constants, near `ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS` (`UldBossHelper.h:375`):

```cpp
constexpr float ULDUAR_FREYA_TRIO_SYNC_WINDOW_PCT   = 30.0f;  // trio outranks other adds below this
constexpr float ULDUAR_FREYA_TRIO_FLOOR_RELEASE_PCT = 15.0f;  // all below -> free burn
constexpr float ULDUAR_FREYA_TRIO_HARD_FLOOR_PCT    = 10.0f;  // never cross while a sibling is high
constexpr float ULDUAR_FREYA_SPORE_RADIUS           = 6.0f;   // Potent Pheromones 64321
constexpr float ULDUAR_FREYA_DETONATE_RADIUS        = 15.0f;  // Detonate 62598
constexpr uint32 SPELL_POTENT_PHEROMONES            = 64321;
constexpr uint32 SPELL_CONSERVATORS_GRIP            = 62532;
```

The band is deliberately generous: it covers only the last ~228k of a 2.28M wave, roughly 6 seconds
of raid DPS out of the 60-second budget. Thaddius's tighter numbers exist because its window is 5s;
Freya's is 11s.

Wave state, mirroring `MuruEncounterTargets`:

```cpp
struct FreyaWaveState
{
    Unit* eonarsGift = nullptr;
    Unit* conservator = nullptr;
    Unit* snaplasher = nullptr;
    Unit* stormLasher = nullptr;
    Unit* waterSpirit = nullptr;
    std::vector<Unit*> detonatingLashers;

    std::vector<Unit*> LivingTrio() const;
    bool TrioActive() const;
    bool TrioLocked() const;   // any living member below SYNC_WINDOW_PCT
    bool TrioReleased() const; // every living member at or below FLOOR_RELEASE_PCT
};

void GatherFreyaWaveState(PlayerbotAI* botAI, FreyaWaveState& state);
bool FreyaTrioSyncSuppress(FreyaWaveState const& state, Unit* target);
Unit* GetFreyaTrioAssignment(PlayerbotAI* botAI, FreyaWaveState const& state);
Unit* GetFreyaTankTarget(PlayerbotAI* botAI, FreyaWaveState const& state);
```

`GatherFreyaWaveState` scans `AI_VALUE(GuidVector, "possible targets no los")` — **not** the
LOS-restricted `"possible targets"` the current code uses (`ValueContext.h:445-448`; rationale at
`UldBossHelper.h:523-526`).

### `FreyaTrioSyncSuppress` — hard floor only

```
not a trio member                                          -> false
fewer than 3 living members                                -> false   // window already open or over
every living member at or below FLOOR_RELEASE_PCT           -> false   // release, all die together
target at or below HARD_FLOOR_PCT and any sibling above
    FLOOR_RELEASE_PCT                                       -> true
otherwise                                                   -> false
```

No balance-margin hold. The split does the balancing; this only stops a stray execute crit from
ending a member 20% ahead of its siblings.

### `GetFreyaTrioAssignment` — greedy per-tick load balance

```
candidates = living trio members for which FreyaTrioSyncSuppress is false
if candidates empty -> candidates = living trio members     // safety, should not happen
if candidates empty -> return nullptr

bots = group members, alive, PlayerbotAI::IsDps, in stable group-slot order
assigned[member] = 0
for each b in bots:
    pick = argmax over candidates of (member->GetHealth() / (assigned[member] + 1))
    assigned[pick]++
    if b == bot: return pick
return nullptr
```

Every bot runs the same loop over the same inputs and reaches the same answer, so this needs no
shared state and no coordination. It is self-correcting: a member the raid over-kills sheds bots on
the next tick, deaths and latecomers fold in for free, and excluding floor-suppressed members is
what turns the floor into redistribution instead of idling. Weighting on **remaining** health rather
than max health converges on the quantity that actually has to reach zero together. Cost is
O(bots × 3) per bot per tick — under 2000 comparisons for a 25-man raid.

A raid with fewer than three DPS bots cannot split three ways; the floor carries it. No special case.

### `GetFreyaTankTarget`

- Main tank → Freya, always.
- Assist tank 0 → **Snaplasher** while the trio is up (the Hardened Bark sink), else the Ancient
  Conservator.
- Everything else → `nullptr`, left to generic tank assist. Storm Lasher and Ancient Water Spirit
  are deliberately not owned: owning them means owning Tidal Wave positioning, which this change
  does not solve.
- Detonating Lashers are never tanked — they fixate on random players.

## 2. Direct targeting — `FreyaSetDpsPriorityAction`

New trigger + action in `UldTriggers_Freya.*` / `UldActions_Freya.*`, built on
`MuruSetDpsPriorityAction`. **`PlayerbotAI::IsDps` only** — healers keep generic targeting, since
healer DPS is noise against a 2.28M/60s budget and owning their targets risks fighting the heal
rotation.

`Execute` mirrors `SWPActions_Muru.cpp:178-210`, plus the pet redirect:

```cpp
Unit* currentTarget = AI_VALUE(Unit*, "current target");
Unit* target = ResolveFreyaDpsTarget(currentTarget);
if (!target)
    return false;

// Every tick, including the early-return path below - otherwise a pet stranded on a dead or
// floored add never catches up. CommandPetAttack no-ops when the pet is already on target.
CommandPetAttack(botAI, target);

bool needsAttack = currentTarget != target;
if (PlayerbotAI::IsMelee(bot))
    needsAttack = needsAttack || !bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING);

return needsAttack ? Attack(target) : false;
```

Returning `false` when already on target is what lets lower-priority nodes run — the engine ends the
tick at the first action that succeeds.

### `ResolveFreyaDpsTarget`

Ordered list, first allowed entry wins. The Gift/trio ordering is conditional:

1. **`NPC_EONARS_GIFT` (33228)** — allowed when the bot is ranged DPS, **or** no ranged DPS is alive
   in the group (melee fallback, so the Gift never goes unkilled in a melee-only 10-man). Blocked
   when `state.TrioReleased()` — in the final seconds of a synced trio, nothing leaves.
2. **`NPC_ANCIENT_CONSERVATOR` (33203)** — blocked when `state.TrioLocked()`.
3. **Trio slot** → `GetFreyaTrioAssignment`. Abandoning a half-dead trio guarantees a revive and
   burns the whole 60s wave budget, which is why 2 and 4 yield to it once locked.
4. **Nearest `NPC_DETONATING_LASHER` (32918)** — blocked when `state.TrioLocked()`. Selected via a
   `SelectFreyaEncounterTarget` helper copied from `SelectMuruEncounterTarget`
   (`SWPActions_Muru.cpp:358-380`): sticky on the current target with a ~10 yd switch margin so two
   lashers do not trade the bot back and forth.
5. **`NPC_FREYA` (32906)**.

Then the M'uru sticky-target guard (`SWPActions_Muru.cpp:328-353`): compare priority index of
current vs desired target, keep the current one when `currentPriority <= desiredPriority`, and fall
back to `AI_VALUE(Unit*, "dps target")` when nothing is allowed. **The trio slot is exempt from the
sticky guard** — `GetFreyaTrioAssignment` is already stable by construction (a bot's assignment only
moves when the balance boundary crosses it), and applying stickiness on top would freeze bots onto
whichever member they first picked.

## 3. Remove the marking path entirely

Delete `FreyaMarkDpsTargetTrigger` (`UldTriggers_Freya.h:19-24`, `.cpp:33-165`) and
`FreyaMarkDpsTargetAction` (`UldActions_Freya.h:23-29`, `.cpp:56-170`), plus their entries in
`UldTriggerContext.h`, `UldActionContext.h` and `UldStrategy.cpp`. That removes in one go:

- the trigger/action disagreement over the Snaplasher,
- the `GetEntry()`-based mark comparisons (`UldTriggers_Freya.cpp:101-162`) that treat two adds of
  the same entry as interchangeable,
- the raw `Group::SetTargetIcon` calls that bypass `RaidBossHelpers`,
- the raid-wide Detonating Lasher gate (`UldTriggers_Freya.cpp:141-162`) that blanks the entire
  priority chain when any raid member is below detonate damage. Low-HP safety moves to §5.

Do not add an `attack rti target` node.

## 4. Healthy Spores for every role

`FreyaMoveToHealingSporeTrigger` currently gates on `botAI->IsRanged(bot)`
(`UldTriggers_Freya.cpp:174`). Conservator's Grip is a 50000 yd pacify-silence, so melee are hit too
and can do nothing until they reach a spore.

- Drop the ranged gate; exclude only tanks (a tank walking to a spore drags the Conservator into
  the raid).
- Trigger on the bot **lacking `SPELL_POTENT_PHEROMONES` (64321)** while a Conservator is alive —
  exact, and cheaper than the current distance scan.
- Action keeps `MoveTo` nearest `NPC_HEALTHY_SPORE`, stopping inside `ULDUAR_FREYA_SPORE_RADIUS`.

The spore node sits at `ACTION_RAID + 2` and the priority action at `ACTION_RAID`, so a pacified bot
already goes for a spore before it tries to attack. That ordering needs no change.

## 5. Detonating Lashers — `FreyaAvoidDetonatingLasherAction`

New trigger + action replacing the deleted mark gate: a bot below the detonate threshold
(`Is25ManRaid() ? 7200 : 4900`, the existing numbers) flees to `ULDUAR_FREYA_DETONATE_RADIUS + 1`
from the nearest Detonating Lasher. Reuse `FleePosition`, as `FreyaMoveAwayNatureBombAction` does
(`UldActions_Freya.cpp:53`).

No herding or stacking behaviour — lashers fixate on random players and cannot be tanked or moved,
and 234k each in 25-man dies to incidental cleave.

## 6. Add tanking — `FreyaTankAddsAction`

New trigger + action, tanks only, resolving through `GetFreyaTankTarget`. Uses
`PlayerbotAI::IsAssistTankOfIndex` (the M'uru tank split at `SWPMultipliers.cpp:729` and
`SWPStrategy.cpp:318` is the reference).

## 7. Multipliers — `src/Ai/Raid/Uld/UldMultipliers.h` / `.cpp`

There is currently no Freya multiplier at all. Add two, registered in
`RaidUlduarStrategy::InitMultipliers` (`UldStrategy.cpp:611-643`).

**`FreyaDisableAutomaticTargetingMultiplier`** — `MuruDisableDefaultTargetingMultiplier` shape
(`SWPMultipliers.cpp:712-742`). Without it the generic picker fights the priority action every tick
and direct targeting does nothing.

- Return `1.0f` immediately unless the action is a `DpsAssistAction` or `TankAssistAction`, and
  unless Freya is found.
- `DpsAssistAction` in `BOT_STATE_COMBAT` for a bot passing `PlayerbotAI::IsDps` → `0.0f`.
- `TankAssistAction` → `0.0f` for the **main tank** while Freya is alive, and for **assist tank 0**
  while the Snaplasher is alive. The main-tank guard is what stops generic tank assist from pulling
  the main tank off Freya onto a stray Storm Lasher.

**`FreyaTrioSyncMultiplier`** — Thaddius shape (`NaxxMultipliers.cpp:196-203`). A backstop only:
under normal operation `GetFreyaTrioAssignment` has already steered bots off suppressed members, so
this catches damage the targeting cannot steer (a swing mid-animation, a DoT already ticking).
Non-tank DPS only, when `FreyaTrioSyncSuppress(state, AI_VALUE(Unit*, "current target"))`:

- `0.0f` for `MeleeAction`
- `0.0f` for `CastSpellAction` that is not a `CastHealingSpellAction`. Confirm during implementation
  whether `CastDebuffSpellOnAttackerAction` derives from `CastSpellAction`; if it does not, add it
  explicitly so no new DoT lands below the floor.
- **Never** suppress movement or healing — a suppressed bot must still dodge Nature Bombs and reach
  a spore.

Leave the existing `UlduarBurstWindowMultiplier` Bloodlust gate (`UldMultipliers.cpp:437-442`)
untouched.

## 8. Registration — `UldStrategy.cpp:320-351`

Five existing Freya nodes tie at exactly `ACTION_RAID`, so their order is queue order rather than
intent. Re-tier the whole block:

| Node | Priority | Gate |
|---|---|---|
| `freya break iron roots` | `ACTION_RAID + 5` | hard mode; a rooted bot can do nothing else |
| `freya dodge unstable sun beam` | `ACTION_RAID + 4` | hard mode |
| `freya move away nature bomb` | `ACTION_RAID + 4` | any bot |
| `freya avoid detonating lasher` | `ACTION_RAID + 3` | low-HP bots |
| `freya move to healing spore` | `ACTION_RAID + 2` | non-tanks lacking 64321 |
| `freya tank adds` | `ACTION_RAID + 1` | tanks |
| `freya set dps priority` | `ACTION_RAID` | DPS |
| `freya nature/fire resistance` | `ACTION_RAID` | unchanged |

`freya mark dps target` is removed. Wire the new names into `UldTriggerContext.h:57-63` and
`UldActionContext.h:58-64` (both the `creators[...]` block and the factory methods), dropping the
two mark entries. No CMake change — all new code lands in existing files.

## 9. Docs — `docs/raids/ulduar.md`

- Rewrite the Freya section (`:283-304`): the trio revive rule, the 60s wave timer, the verified HP
  table, the real Hardened Bark effect, the split-and-floor design, and the note that Freya uses
  direct targeting rather than raid icons.
- Fix the gap table row at `:751` — it claims "Snaplasher is only skull-marked", but the Snaplasher
  was never marked at all. Replace it with the remaining interrupt gap (Storm Lasher, Ancient Water
  Spirit), which stays open.

## Deliberately out of scope

**Interrupts** for Storm Lasher (Stormbolt 62649, Lightning Lash 62648) and Ancient Water Spirit
(Tidal Wave 62653). Independent change with its own generic-interrupt plumbing risk; landing it in
the same diff as a targeting rewrite would make a failed in-game test ambiguous.

**Elder Brightleaf / Stonebark / Ironbranch pre-pull handling** (32915 / 32914 / 32913, zero code
references anywhere). Hard mode is selected by *leaving them alive*, and they are trash killed
before the pull, so there is no encounter hook to attach to. `IsFreyaHardModeActive` stays a pure
config read (`src/Ai/Raid/Uld/Util/UldHardMode.cpp:83`).

**Ground Tremor (62437)** stays unhandled — raid-wide and undodgeable, as `docs/raids/ulduar.md:287`
already records.

## Verification

The module cannot be compiled headless in this environment, so the build is a hand-off.

1. **Static checks before hand-off**:
   - every new trigger/action name appears in exactly three places — the `creators[...]` map, the
     factory method, and the `TriggerNode`;
   - `grep -riE "SetTargetIcon|MarkTargetWith|SetRtiTarget|GetTargetIcon" src/Ai/Raid/Uld/*Freya*`
     returns nothing;
   - `GetFreyaTrioAssignment` iterates group members in an order that does not depend on the calling
     bot, or the split will differ per bot and the balance will not hold.
2. **Build**: user compiles the worldserver with the module.
3. **10-man normal first** (`AiPlayerbot.UlduarFreyaHardMode = false`). Pull Freya, wait for the trio
   wave. Expected: DPS bots visibly divide across all three adds, and `EMOTE_TRIO_WITHERS` fires
   three times with **no `EMOTE_TRIO_REGENERATES` between them**. That emote in the log is the
   failure signature — a member revived, window missed. Widen `FLOOR_RELEASE_PCT` if it appears.
4. Confirm no bot idles during the wave. A stationary melee bot means suppressed members are not
   being excluded from the assignment.
5. Confirm no bot ping-pongs between two Detonating Lashers — that means the sticky guard in
   `SelectFreyaEncounterTarget` is not holding.
6. Confirm the wave clears inside 60s — a second wave spawning on top of the first is the
   deadline failure, distinct from the revive failure.
7. Confirm `Attuned to Nature` (62519) drops 10 stacks per trio member, 25 per Conservator, 2 per
   Detonating Lasher, and reaches 0 so the final phase starts.
8. Confirm melee bots pick up `Potent Pheromones` (64321) during the Conservator wave and are not
   left standing pacified.
9. Confirm no Eonar's Gift survives its 12s timer — Freya's health should never jump 30-60%.
10. Watch the assist tank 0 health as Hardened Bark ramps. Under a three-way split only about 43% of
    DPS bots ever touch the Snaplasher, so stacks should stay far below the 99 cap; if that tank
    still dies first, the split weighting is not taking effect.
11. **Repeat in 25-man**, where the Snaplasher/Water Spirit gap is widest (977k vs 524k) and the sync
    is hardest.
12. **Repeat with `UlduarFreyaHardMode = true`** to confirm the re-tiered priorities did not regress
    Iron Roots or Sun Beam handling.
