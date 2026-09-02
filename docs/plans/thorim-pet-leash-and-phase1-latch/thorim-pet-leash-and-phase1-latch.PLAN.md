# Thorim: stop the phase-1 boss latch, and leash the gauntlet squad's pets

Trace: `env/dist/logs/botobs/603_1_thorim_1788377511.ndjson`, schema **v10**, 25-man, pull to wipe
command in 4:57. Read with `python modules/mod-playerbots/tools/botobs/postmortem.py <file>`.
Scratchpad helpers under
`C:\Users\boss2\AppData\Local\Temp\claude\...\f68e6a38-.../scratchpad\plib.py`.

On commit, copy this file to `docs/plans/thorim-pet-leash-and-phase1-latch/thorim-pet-leash-and-phase1-latch.PLAN.md`.

---

## Context

The ask was pet positioning and pet targeting. The headline is that **pets are mostly fine**, and
the one real pet defect is not in pet code at all: it is a bot-side target loop that pets faithfully
follow off a cliff.

v10 delivered what the last round was missing. Pets now have `snap.u` rows and an `own` field, so
every number below is read straight off the trace instead of inferred from spell lists.

### Pets are broadly working

| pet | owner | squad | in range of own target | orphan target | max dist from owner |
|---|---|---|---|---|---|
| Corpseravager | Deathsong (human) | Gauntlet | 93% | 4% | 43 yd |
| Wolf | Trueshot | Arena | 93% | 16% | 36 yd |
| Flaaghun | Fel | Gauntlet | 88% | 5% | 42 yd |
| Worm | Nightwarrior | Gauntlet | 87% | 6% | **128 yd** |
| Khiigrom | Agony | Gauntlet | 70% | 5% | 54 yd |
| Ruirin (imp) | Hellflame | Arena | 89% within 30 yd | 2% | 38 yd |
| Spirit Wolf ×2 | Totemist | Gauntlet | 32% / 46% | 0% | **164 / 90 yd** |

"orphan target" = the pet is on something no living raid member is attacking. 2-6% everywhere except
the Wolf's 16%. **Both arena pets stayed inside the arena box 100% of the pull** — the existing leash
works where it applies, and it only had to fire 12 times, all in the first 5.5 s.

No pet died. No pet clipped through geometry: the largest position steps are 8-13 yd over 0.2-0.3 s
(snapshot jitter on a normal run), and the one genuine outlier is the Shadowfiend's 26 yd hop, which
is its own Warp/Shadowcrawl. **The "through textures" complaint does not reproduce here** — the
runaway pets walked the real corridor, which is the problem: the real corridor is 300 yd long.

Pet Growl is a non-issue: 102 casts, but hostiles aimed at a pet on 52 of 55 171 rows (0%). Tanks
out-threat it. Not worth touching.

### The one real defect: pets chase a boss they cannot reach

Five pets spent time attacking **Thorim while he stood on his balcony at z 438**, untouchable in
phase 1. 91 of 1654 pet-seconds (6%) overall — but concentrated brutally on the short-lived
guardians: **55% and 40% of the two Feral Spirits' entire lifetimes.**

Because Thorim is unreachable from the arena floor, a melee pet ordered onto him takes the only
walkable route: out of the arena and down the gauntlet corridor. Spirit Wolf, once Totemist targeted
Thorim at 0:29:

```
0:29.772  Totemist (2149,-253) tgt=Thorim | wolf (2168,-261) tgt=Thorim  d=21
0:34.822  Totemist (2166,-252) tgt=Thorim | wolf (2218,-277) tgt=Thorim  d=58
0:40.064  Totemist (2167,-257) tgt=Thorim | wolf (2219,-356) tgt=Thorim  d=112
0:45.325  Totemist (2199,-264) tgt=-      | wolf (2212,-428) tgt=Thorim  d=164
```

The Worm did the same, 125 yd out and back over 22 s.

### Root cause: the arena empties, and the generic picker hands back the boss

Nothing in the module syncs a pet to its owner's target. `PetAttackAction` is **disabled globally**
(no strategy registers the node; three separate code comments state this as the design), so pets run
on core `PetAI` in defensive stance and simply assist whatever the owner is attacking. Thorim never
calls `CommandPetAttack`, unlike Freya, Razorscale and Mimiron.

So the pet is only ever as good as the owner's target. And the owner's target went bad:

```
0:15  arena hostiles=9   bots targeting Thorim=0
0:25  arena hostiles=1   bots targeting Thorim=0
0:30  arena hostiles=1   bots targeting Thorim=11
0:35  arena hostiles=1   bots targeting Thorim=12
0:40  arena hostiles=0   bots targeting Thorim=8
0:50  arena hostiles=2   bots targeting Thorim=7
```

The opening trash dies at ~0:25 and the first Dark Rune wave does not land until 0:50. In that gap:

1. `GetThorimDpsTarget` has no candidate, so `ThorimHasDpsTarget` is false.
2. `ThorimDisableAutomaticTargetingMultiplier` (`UldMultipliers_Thorim.cpp:118`) therefore returns
   **1.0** — it deliberately releases the generic pickers rather than strand the bot.
3. The generic picker takes the only hostile in range: Thorim.
4. `ThorimDpsTargetAllowed` (`UldEncounter_Thorim.cpp:663`) refuses him, so
   `ThorimDpsPriorityAction` clears the target — `AttackStop` + `InterruptNonMeleeSpells(true)` +
   `SetTarget(Empty)` — and returns false.
5. Next tick, back to 3.

18 of 25 bots held Thorim at some point, all from z 419-420 (arena floor, below the 429.6 threshold),
Obliteration for 113 snapshot rows. `thorim.dpstarget` picked Thorim **zero** times — the encounter
picker is behaving perfectly; this is entirely the generic picker being let back in.

Each re-acquire re-points the pet, which is why the pets kept running.

### Not in scope, but it is what actually killed the raid

The gauntlet stalled at the **bottom of the ramp**. Runic Colossus died at 2:04; the Ancient Rune
Giant sat at (2135, -440, **438**) at 100% health for the whole pull. The seven survivors ended at
(2193-2210, -429..-434, z 415-423) — roughly 60 yd short and 15-23 yd below it — grinding Iron Honor
Guards that spawn every ~10 s (23 of them). **Thorim took zero damage in 4:57 and never left his
platform**, so the balcony route from the last round never got a chance to run: `thorim.balcony`
emits no note at all, correctly, because `ThorimBalconyOpen` waits on the Rune Giant's death.

This is the unwaypointed last leg already logged as out of scope in the balcony plan (corridor
waypoints stop at y -329). Separate job, by decision.

Fix 4 from the last round did work: the human Deathsong's `thorim.squad` flipped 1 → 2 at 0:40 on
entering the corridor. Final split: arena 14, gauntlet 11.

---

## Fix 1 — teach the encounter picker about the opening trash

**File:** `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp` (plus the `ThorimEncounterTargets` struct in
`UldEncounter_Thorim.h`)

**This is a hard prerequisite for Fix 2, not a nice-to-have.** Verified against the trace:

- Thorim is `IsInCombat()` from 0:00.3 (he targets Deathsong, then Elemena), so `ThorimSplitActive`
  is true from the pull.
- The whole raid is already within 200 yd of the arena centre at 0:00 (min 0-7 yd), so
  `NearThorimEncounter` is true too.
- The opening trash is **inside the arena box**, 5-21 yd from centre.
- `GatherThorimEncounterTargets` (`UldEncounter_Thorim.cpp:611-660`) has an entry whitelist that does
  **not** include it, so the encounter picker returns null and the generic picker is what kills it —
  1898 roster target-rows across the first 25 s.

Suppressing the generic pickers without this change would leave 25 bots with no target source for the
opening 25 seconds.

Add to the switch, into a new `std::vector<Unit*> trash;` member on `ThorimEncounterTargets`:

```
NPC_CAPTURED_MERCENARY_SOLDIER_ALLY  = 32885
NPC_CAPTURED_MERCENARY_SOLDIER_HORDE = 32883
NPC_CAPTURED_MERCENARY_CAPTAIN_ALLY  = 32908
NPC_CAPTURED_MERCENARY_CAPTAIN_HORDE = 32907
NPC_JORMUNGAR_BEHEMOT                = 32882
```

All five constants already exist at `UldEncounter_Thorim.h:36-40`; they are simply unused.

Append `&targets.trash` as the **last** tier in both branches of `GetThorimDpsTarget` — after
`commoners` in the arena `TierOrder` (widen the `std::array` to 6), and after `guards` in the
gauntlet tier list. Last on purpose: it must never outrank an Acolyte or a Champion once the real
waves start. The arena branch's existing `ThorimInArenaBox` filter passes here because the trash
fights inside the box.

## Fix 2 — stop the phase-1 boss latch

**File:** `src/Ai/Raid/Uld/Multiplier/UldMultipliers_Thorim.cpp`

One line, at the end of `ThorimDisableAutomaticTargetingMultiplier::GetValue` (currently `:118`):

```cpp
// Only while the encounter actually has an answer...
return ThorimHasDpsTarget(botAI, bot) ? 0.0f : 1.0f;
```

becomes

```cpp
return (ThorimHasDpsTarget(botAI, bot) || ThorimSplitActive(botAI)) ? 0.0f : 1.0f;
```

and the comment above it gets rewritten: with Fix 1 in place, "the encounter picker has nothing" now
genuinely means "there is nothing legal in reach", so idling is the correct answer and the generic
picker can no longer hand back an untouchable boss.

`ThorimSplitActive` (`UldEncounter_Thorim.cpp:1085`) is the right gate and needs no change — it
already requires `NearThorimEncounter`, a hostile boss, `IsInCombat()`, and balcony height, so it is
false outside the encounter and false the instant Thorim drops for phase 2. Healers and tanks are
already exempted earlier in the function and stay exempt.

Known limitation to accept: a bot pulled by something unrelated and un-whitelisted inside the
encounter would now idle rather than fight back. The arena is a closed room and the whitelist covers
everything that spawns in it, so this is theoretical.

## Fix 3 — leash the gauntlet squad's pets to their owner

**Files:** `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.{h,cpp}`,
`src/Ai/Raid/Uld/Action/UldActions_Thorim.cpp`

`ThorimStrayArenaPets` (`UldEncounter_Thorim.cpp:1156`) bails on
`GetThorimSquad(botAI, bot) != ThorimSquad::Arena`, so a gauntlet bot's pet is never bounded by
anything. Every runaway this pull belonged to a gauntlet-squad owner, and during the 0:27-0:50 window
those owners were still standing in the arena — so the squad label said Gauntlet while the body was
in the arena, and no leash applied.

Rename to `ThorimStrayPets` (it is no longer arena-only; the registered node names
`thorim pet leash trigger` / `thorim pet leash action` stay as they are — they are wired in three
places) and replace the squad bail with a per-squad boundary:

- **Arena squad** — unchanged: inside `ThorimInArenaBox` **and** within
  `ULDUAR_THORIM_ARENA_LEASH_RADIUS` (30 yd) of `ULDUAR_THORIM_NEAR_ARENA_CENTER`.
- **Gauntlet squad** — within a new `ULDUAR_THORIM_PET_OWNER_LEASH_RADIUS` of the owner, set to
  `ULDUAR_THORIM_DPS_TARGET_RANGE` (50 yd, `UldEncounter_Thorim.h:135`). Principled rather than
  arbitrary: that is how far the encounter picker looks for a target, so a pet beyond it is further
  out than its owner can even see something to kill. Measured headroom — every legitimate excursion
  this pull peaked at 42-54 yd, and the runaways hit 90-164 yd.
- **`ThorimSquad::None`** — unchanged, no leash.

Everything else in the helper stays: the totem/pet/guardian filter, the same-map check, and the 2 s
`petRecallMs` throttle. `ThorimRecallPet` needs no change — it iterates nothing, it is called per
stray pet, and the arena pets prove it works.

Also change `ThorimPetLeashAction::Execute` (`UldActions_Thorim.cpp:115`) to `return false`. It moves
a pet, not the bot, so it should not consume the bot's tick — `RazorscalePetControlAction` and
`MimironPetControlAction` both return false for exactly this reason. `UldStrategy.cpp:474` already
claims "competes with nothing", which is only true once this returns false.

### Deliberately not doing

- **No `CommandPetAttack` node for Thorim.** Decided. Worth recording why it would have been awkward
  anyway: `CommandPetAttack` (`EncounterHelpers.cpp:427`) drives `bot->GetGuardianPet()`, which reads
  the single `SUMMON_SLOT_PET` slot, so it reaches at most one guardian — it would have missed both
  Feral Spirits, the two pets that ran furthest. It also dereferences `GetCharmInfo()` unchecked,
  which `ThorimRecallPet` explicitly guards against. A Thorim pet-targeting node would need a
  multi-guardian variant walking `m_Controlled`.
- **The `SetIsFollowing` / `SetIsReturning` pairing** in `ThorimRecallPet` is inverted relative to
  Naxx's `RecallControlledPetsToBot`. Left alone: the arena pets stayed in the box all pull, so the
  recall demonstrably works as written.
- **Pet Growl / autocast.** 0% aggro held. Not worth a change.
- **The ramp stall and the Ancient Rune Giant.** Separate job, by decision.

---

## Fix 4 — found while implementing: the arena target guard deadlocks against the picker

**File:** `src/Ai/Raid/Uld/Multiplier/UldMultipliers_Thorim.cpp`

This is why the latched bots *stayed* on Thorim instead of merely oscillating, and it was not in the
plan above.

`ThorimArenaTargetGuardMultiplier` zeroes every `AttackAction` and `ReachTargetAction` for an
arena-squad bot whose current target is outside the arena box. Thorim on his balcony is outside the
box (z 438 > the box's 425 ceiling), so a bot holding him trips the guard. The guard has an escape
hatch, `ThorimIsTargetSelectionAction`, and its comment states the intent exactly:

> The escape hatch. Zeroing these too is a deadlock: the guard fires because the target is wrong,
> and the action it kills is the one that would pick a different one.

But that helper only lists the generic pickers (`DpsAssistAction`, `DpsAoeAction`, `TankAssistAction`,
`AggressiveTargetAction`, `AttackAnythingAction`, `AttackLeastHpTargetAction`).
`ThorimDpsPriorityAction` derives from `AttackAction` directly, so it is a sibling of all of them and
no `dynamic_cast` in the list matches it. The encounter's own picker — the only node that clears a
forbidden target — was never exempt.

The trace confirms it outright. `thorim arena target guard` vetoes `thorim dps priority action` 42
times, and the first eight land at 0:27.3-0:28.9 on Tree, Smartface, Obliteration, Ecoterrorist,
Justice, Assasin, Stormweaver and Holylight — the same bots that then hold Thorim for 89-113
snapshot rows each:

```
0:27.324 Tree         {"m": "thorim arena target guard", "a": "thorim dps priority action"}
0:27.563 Obliteration {"m": "thorim arena target guard", "a": "thorim dps priority action"}
0:27.587 Justice      {"m": "thorim arena target guard", "a": "thorim dps priority action"}
```

Fix: add `dynamic_cast<ThorimDpsPriorityAction*>(action)` to `ThorimIsTargetSelectionAction`.
`UldMultipliers_Thorim.cpp` already includes `UldActions.h`, which reaches `UldActions_Thorim.h`, so
the type is in scope.

Worth keeping even though Fix 2 stops the bot acquiring Thorim in the first place: it is the general
guard against any other route to a forbidden target, and it restores the behaviour the helper's own
comment already claims. It also exempts the picker from `ThorimArenaLeashMultiplier`, which uses the
same helper for the same reason — a bot outside the leash still has to be able to choose a target
while the leash walks it home.

---

## Verification

Static, before any pull:

- `grep -n "trash" src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp` shows the new tier appended last in
  both branches, and the arena `TierOrder` array is widened to 6.
- `grep -rn "ThorimStrayArenaPets" src/` returns nothing; `ThorimStrayPets` appears in the helper,
  the header, the trigger and the action.
- `python tools/botobs/postmortem.py env/dist/logs/botobs/603_1_thorim_1788377511.ndjson` still runs
  clean against this v10 trace.
- The module cannot be compiled headless here. Hand the branch off for a build rather than claiming
  one.

In-game, one 25-man pull, then re-read the fresh trace:

1. **The opening trash still dies.** All 25 bots hold a target through 0:00-0:25 and the Mercenaries
   and the Behemoth die on roughly the old clock. This is the regression risk of Fix 1/2; check it
   first.
2. **No bot targets Thorim in phase 1.** Zero snapshot rows where a roster member below z 429.6 has
   Thorim as `snap.u[7]`, against 18 bots and 100+ rows each this pull.
3. **No pet targets Thorim in phase 1.** Was 91 pet-seconds across five pets.
4. **Bots idle rather than flip-flop** through the 0:25-0:50 add gap: `thorim dps priority action`
   should stop logging repeated FAILED verdicts in that window.
5. **No pet exceeds 50 yd from its owner** for more than the 2 s throttle. Was 128 yd (Worm) and
   164/90 yd (Feral Spirits).
6. **Feral Spirit in-range rises** from 32%/46%; the melee pets should stay at their current 87-93%
   rather than regress.
7. **`thorim.petrecall` fires for gauntlet owners**, not just the 12 arena recalls in the first 5.5 s.
8. **Arena pets do not regress**: still ~100% inside the arena box, and the leash still fires for
   them.
