# Thorim phase 2: walk the camp home when the orb goes dark

## Context

Trace `env/dist/logs/botobs/603_1_thorim_1789146024.ndjson` (pull O, 11 Sep 20:06), the first pull on
`b4fc0d338`: the worldserver restarted 19:23, four minutes after that commit. It is compared against K
`1789071729`, L `1789129966`, M `1789135678` and N `1789136547`. Read the traces with
`tools/botobs/obstrace.py` `Trace` or `python tools/botobs/postmortem.py <file>`. **"+N s" is time since
Thorim is first seen below z 425.**

### Pull O in brief

- **Result:** 25-man hard mode, 24 in raid. The MT (Bulwark) dies at +156 s with Thorim at **30.5%**,
  the best pull so far (L 32.4%, M 50.6%, N 46.8%).
- **Deaths before the MT's, 7 in all:**
  - 5 to Chain Lightning: Tree at +87 s; Shadow, Assasin, Obliteration and Malediction at +102 s.
  - The off-tank twice. At +107 s it was the Unbalancing Strike at 25% hp, after a 17k Lightning Charge
    (orb 4's cone covers the tank spot) and 90k of melee. At +139 s, after a battle rez, it was melee.
- **Oscillation:** one A-B-A reversal in 2.6 min (M 5, N 10, L 107). Unbalancing Strike swaps land in
  79-204 ms.
- **The opening hold from `b4fc0d338` works:**
  - The first Chain Lightning hit one bot, for 995. K, M and N lost 2-3 bots each to that cast.
  - Gauntlet ranged: 7 bots on 6 platform spots, released at 12.4-12.8 s, landed at 15.4-18.9 s, all
    home by 25 s.
  - The rim release at 12.4 s walked three bots through orb 1's cone into the first Lightning Charge,
    for 6-15k each. Nobody died.

### Why Chain Lightning still killed five

1. **The cast lands while the camp is walking home.** This plan fixes this one.
   - In K-O, all 21 casts start between -0.13 and +0.80 s after an orb lights, and land 0.5 s later.
     Both run on 15 s cycles.
   - `ThorimRangedSpot` keeps a camp bot on its shelter until a *different* orb lights, so the walk
     home starts exactly then. Of 71 shelter exits in K-O, 68 were walks home at that moment. Only 3 went
     to another orb's shelter, all in K.
   - At +87 s, three bots stacked on orb 2's slot-0 shelter (2116.5, -283.4) chained through Power
     (walking home), Fel (on slot 2's shelter) and Tree (walking home). Tree took hop 6, 31.8k, and died.
   - Replayed with the walkers already home, the same cast stops at 3 hops.
2. **Thorim rested 7.4 yd off the tank spot.** The user chose to leave this.
   - A victim change at +10 s re-seated him at the chase near point, (2118.1, -254.0), and he never
     moved again (`TargetedMovementGenerator.cpp:362`).
   - That put RANGE4 14.9 yd from him, 6.9 yd outside the melee ring. In K-N he rested 4.1-5.9 yd off.
   - The +102 s chain ran: camp pair on RANGE6, the human (6.8 yd away), a stacked ring slot, Assasin in
     Killing Spree, Obliteration, then Malediction on RANGE4. Hops 5-8 all died.
   - Pushing camp spots out from the live boss was modelled. It brings camp pairs to 6.4-7.1 yd for
     orbs 1 and 3, so that isn't the fix.
3. **The melee pile.** Over K-O's post-opening casts, the model gives 0.53 expected lethal hops per
   cast, 65% of them on melee. Not in scope.

### Measured timing the change rests on (K-O)

| | |
|---|---|
| orb lit window (`thorim.lightningorb` note set to "0") | 4.55-5.29 s, no flicker |
| orb dark window | 9.70-10.56 s |
| shelter to home runs (`ULDUAR_THORIM_SHELTER_SPOTS` vs RANGE1-6) | 1.6-25.6 yd, at most 3.7 s |

## Change

### 1. Release the shelter latch at dark

In `ThorimRangedSpot` (`src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp:940-986`), right after `held` is
taken:

```cpp
if (!orb)
{
    held = {};
    return false;
}
```

- **Redundant guards:** the two later `orb &&` guards are now always true, so drop them.
- **Effect on the next orb:** it is always new to the latch, so its index is re-derived. The home is then
  cone-tested as before, and the shelter runs start from home, which is the case the table was solved for.
- **Blizzard:** the positioning trigger (`UldTriggers_Thorim.cpp:174`) still holds a walk home that
  crosses a Blizzard zone. Such a bot stays on its shelter until the zone clears or the next orb decides.
- **Unchanged:**
  - the shelter note (`thorim.shelter`, `UldEncounter_Thorim.cpp:2337`), which now shows "home" at dark;
  - the opening hold, which still runs before the latch;
  - the Blizzard escape's `preferNear` (home during the dark window);
  - the per-bot and raid-wide resets.
- **Other readers:** `rangedShelters` is only read here and in the resets (`:2706`, `:2737`).

### 2. Comments

Rewrite these to state the new rule and the Chain Lightning timing behind it:

| where | current text |
|---|---|
| `UldEncounter_Thorim.cpp:934-938` | "no snap home when it goes dark ... Same latch the melee ring's cone offset already runs on" (no longer true) |
| `UldEncounter_Thorim.h:375-378` | the `RangedShelter` comment: "Sticky for the life of the orb: no snapping home when it goes dark" |
| `UldEncounter_Thorim.cpp:860-862` | "starting from a previous orb's shelter has been measured at 60 yd" |

- The third one now only happens when a Blizzard zone holds the walk home.
- Invoke `use-conversational-language` fresh for the new comments: short, no dashes, no history.

### 3. Docs (one `/compact-docs-writer` invocation, up front, covering both)

- **Save this plan first** as
  `docs/plans/thorim-camp-home-at-dark/thorim-camp-home-at-dark.PLAN.md`.
- **Add to `docs/raids/ulduar/thorim.md`**, after "### The opening", a short subsection with:
  - Chain Lightning is cast within 0.8 s of each orb lighting;
  - the camp shelters for the lit window and walks home as soon as it goes dark;
  - why: the 68 of 71 exits, and the +87 s chain.
- **Stale text:** the Phase 2 paragraphs at `thorim.md:96-109` (old tank spot, "three fixed spots",
  rigid ring, "ranged and healers hold position and eat it") contradict this. Flag the contradicting
  sentence in the diff and let the user decide.

## Deliberately not changing

- **Melee ring cone offset** (`LightningChargeOffset`, `:623-663`).
  - 25 of the 44 ring moves inside the orb-to-Chain-Lightning window are dodges of the new cone, which
    have to happen then.
  - The 19 returns mostly sit inside the ring deadband.
- **The rim and balcony release timing, and the tank anchor.** The user chose to leave these.
- **Pets, Killing Spree, ring slot count** (the melee pile). A separate plan if wanted.

## Verification

**Static,** from `modules/mod-playerbots`:
- `python apps/codestyle/codestyle-cpp.py`;
- LF, ASCII, ≤ 120 columns;
- per-TU `-fsyntax-only` in `acore/ac-wotlk-build:master`, using `compile_commands.json`, the live tree
  mounted read-only and every module `src` dir on `-I`. Cover `UldEncounter_Thorim.cpp`,
  `UldTriggers_Thorim.cpp`, `UldActions_Thorim.cpp`, `UldMultipliers_Thorim.cpp` and `UldStrategy.cpp`.

**In game,** after one 25-man hard-mode pull, read the trace:
1. **Release at dark:** after every `thorim.lightningorb` note set to "0", each camp bot whose last
   `thorim.shelter` note said "orb N" gets a "home" note within about 1 s, unless a Blizzard zone lies on
   its walk.
2. **Nobody walking home at a landing:** at each Chain Lightning landing (`cast` record 64390 + 500 ms),
   no camp bot with snapshot `moving` = 1 has "home" as its latest shelter note. K-O had 0-11 such bots.
3. **Cones:** no Lightning Charge (62466) hit on a camp bot that stood at home when its orb lit.
4. **Blizzard:** camp Blizzard damage and escape orders are no higher than O's (11 orders, median
   2.8 yd).
5. **No regression:** the first Chain Lightning still kills no ranged. Ring latch notes, melee vetoes and
   A-B-A reversals stay at O's levels.
