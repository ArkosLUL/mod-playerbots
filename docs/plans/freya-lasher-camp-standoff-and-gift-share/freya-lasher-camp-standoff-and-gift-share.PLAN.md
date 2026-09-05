# Freya: keep the ranged camp out of Detonate range, and stop Eonar's Gift emptying the pack

## Context

Three Freya 25 hard-mode pulls were recorded on 2026-09-05 (traces
`env/dist/logs/botobs/603_1_elder-stonebark_{1788608477,1788609171,1788609636}.ndjson`, referred to
below as P1/P2/P3). P1 survived to 5:18 with 31 deaths, P2 wiped at 3:08, P3 at 1:49.

**None of them tested the current code.** The running worldserver was built 2026-09-04 23:54 local,
one minute after `e865b00bf` and roughly two hours before `3fbddfce3` ("get bots out of Sun Beam and
off lashers that are about to blow"). Three independent confirmations from the traces themselves:
`freya lasher about to blow` never appears in any of them; `avoid aoe` executed 38 times at relevance
90 with no `freya avoid aoe hold` veto; and 138 of 139 `freya ranged camp` destinations landed within
0.5 yd of a live bot, which is the anchor-bot aim `3fbddfce3` replaced. So today's three pulls are
repeats of last night's `1788559467`, and the committed Sun Beam / lasher-bail work is still
unmeasured.

Across those three pulls there were **four** Detonating Lasher waves, one clean and three lethal.
Comparing them is what this plan is built on.

| wave | pack kill rate | melee distance at spawn | 7 melee in contact | lashers reaching within 8 yd of the back line | raid inside a 15 yd blast | deaths |
|---|---|---|---|---|---|---|
| P1 0:10 **clean** | 39.1 %hp/s | 3.7-5.2 yd | 2.5 s | **0 of 10** | 14/25 | **0** |
| P1 2:38 | 22.3 | 3.0-11.5 | 6.1 s | 6 of 10 | 25/25 | 8 |
| P2 1:54 | 24.1 | median 27.3 | 6.4 s | 8 of 10 | 22/25 | 9 |
| P3 0:10 | 36.4 | 6.5-10.1 | 6.0 s | 1 of 10 (8 within 15) | 15/25 | 5 |

**15 of the 22 wave deaths were ranged or healers.** The committed `freya lasher about to blow` bail
is melee-only, so it would not have saved them.

The measurement that sizes this plan is the distance from each ranged/healer to the **nearest** living
lasher, sampled every snapshot of each wave:

| wave | p10 | median | p90 | share of the wave spent inside the 15 yd Detonate radius |
|---|---|---|---|---|
| P1 clean | 13.1 | 18.2 | 21.2 | **18.5%** |
| P1 2:38 | 2.3 | 7.0 | 20.5 | **76.9%** |
| P2 | 1.5 | 5.5 | 16.4 | **85.0%** |
| P3 | 11.6 | 15.5 | 19.1 | **45.0%** |
| `1788559467` (last night, 4:52) | 11.5 | 15.9 | 18.2 | **43.1%** |

The camp is not far enough out, and the reason is that `ULDUAR_FREYA_LASHER_CAMP_STANDOFF` (20 yd) is
measured from the pack **centroid** while the pack's own radius is a median 16.8-21.4 yd. A bot
standing exactly on the camp spot is therefore a few yards from the nearest lasher, and the whole
back line sits inside the blast when the pack finishes. Then five low lashers detonate together at
7-9k each and the raid dies in about two seconds: 9 deaths in 2.3 s in P2, 8 in 2.3 s in P1.

Secondary: `FreyaSetDpsPriorityAction::ResolveFreyaDpsTarget` gives Eonar's Gift to **every** ranged
DPS whenever one is up (`takesGift = IsRangedDps(bot) || !FreyaHasLivingRangedDps(botAI)`,
`UldActions_Freya.cpp:96-105`). Measured: 10-11 ranged leave the pack for 4.7-6.5 s. In the clean wave
that landed at 10% pack health and cost nothing; in P1's second wave it landed at 84% and in P2 at
86%, taking roughly 15% of the wave's ranged damage out at the worst moment.

Intended outcome: the back line spends the wave outside Detonate range as it did in the clean wave,
and the pack dies closer to the clean wave's 39 %hp/s than to the lethal waves' 22-24.

## Approach

Two changes, both on top of `3fbddfce3`. Neither touches `ReachTargetAction` or any movement
suppression — that is what broke `45d9a3e36`.

### A. Measure the camp standoff from the nearest lasher, and floor it at the blast radius

`GetFreyaLasherCampSpot` (`src/Ai/Raid/Uld/Util/UldEncounter_Freya.cpp:460-520`) currently puts the
camp `ULDUAR_FREYA_LASHER_CAMP_STANDOFF` yards from the centroid along the bearing from the pile
toward the anchor bot, sweeping ±π/8 when collision blocks a bearing, and rejecting any candidate
pulled back inside `ULDUAR_FREYA_DETONATE_RADIUS` **of the centroid**.

Change the acceptance test from centroid distance to **nearest living detonating lasher**: a candidate
is valid only when every living detonating lasher is at least `ULDUAR_FREYA_LASHER_CAMP_STANDOFF` away
from it. The bearing sweep and the collision check stay exactly as they are. Where the current code
reads `centre.GetExactDist2d(x, y) < ULDUAR_FREYA_DETONATE_RADIUS`, the new test walks
`state.detonatingLashers`.

Push the candidate out along the bearing until it clears, rather than only rejecting: start at
`STANDOFF` from the centroid and step outward in 2 yd increments to a cap of
`AiPlayerbot.SpellDistance` (verified 28.5 from `env/dist/etc/modules/*.conf`, no `AC_` override)
before giving up on that bearing. Without the outward walk a spread pack rejects every bearing and the
helper falls back to standing on the anchor bot, which is the behaviour being replaced.

- **Constant**: `ULDUAR_FREYA_LASHER_CAMP_STANDOFF` 20.0 -> **18.0**, and its comment changes from
  "relative to the pack" to nearest-lasher clearance. 18 is the clean wave's measured median (18.2)
  and the number that held the back line inside the blast only 18.5% of the time. It is deliberately
  not larger: the far side of a 17 yd pack then sits past SpellDistance and ranged stop hitting it.

- **Blast floor on the trigger.** `FreyaRangedCampTrigger::IsActive`
  (`src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.cpp:148-174`) stands down whenever the bot is within
  `ULDUAR_FREYA_RANGED_CAMP_TOLERANCE` (10) or `_HEALER_CAMP_TOLERANCE` (15) of the spot. With an 18 yd
  spot a healer may legally sit 3 yd from a lasher, which is exactly what the p10 numbers show. Add a
  floor that fires regardless of tolerance: if any living detonating lasher is within
  `ULDUAR_FREYA_DETONATE_RADIUS`, the trigger is active. Tolerance keeps its job of stopping churn
  everywhere else.

### B. Share Eonar's Gift instead of sending every ranged bot

In `ResolveFreyaDpsTarget`, keep the Gift at the top of the priority list, but while a Detonating
Lasher pack is alive give it to only the first few ranged DPS.

- **Subset**, deterministic and needing no shared state, following `GetFreyaRangedCampAnchor`'s idiom
  (`UldEncounter_Freya.cpp:404`): rank the living ranged DPS by GUID and take it when the bot's rank is
  under `ULDUAR_FREYA_GIFT_SHARE` (**5**). New helper alongside the anchor, e.g.
  `uint32 GetFreyaRangedDpsRank(PlayerbotAI*)`.
- **Deadline, not just a share.** The Gift heals Freya 30-60% if it lives 12 s, so the subset must not
  be allowed to be too slow. Latch the first tick this bot saw the current Gift guid on the action
  (`ObjectGuid giftGuid; uint32 giftSeenMs;`, the same member idiom as the Sun Beam and bail latches),
  and once `ULDUAR_FREYA_GIFT_SHARE_MS` (**5000**) has passed, every ranged bot takes it again.
- **Only while the pack is up.** When `state.detonatingLashers` has no living member, behaviour is
  unchanged: everybody takes the Gift, as today.

Measured basis: 10 bots take a Gift from 100% to 2% in 4.2-6.3 s, about 16 %hp/s, so roughly
1.6 %hp/s per bot. Five bots for 5 s puts it near 60%, and the full group then finishes it in about
3.75 s — under 9 s total, with ~3 s of margin on the 12 s deadline. The Gifts that failed in these
traces (14.4 s at 7-8 bots, still at 32-40%) were all post-wipe with a crippled raid, not evidence
against a healthy subset.

## Files

- `src/Ai/Raid/Uld/Util/UldEncounter_Freya.h` — `ULDUAR_FREYA_LASHER_CAMP_STANDOFF` 20 -> 18 with a
  reworded comment; new `ULDUAR_FREYA_GIFT_SHARE` and `ULDUAR_FREYA_GIFT_SHARE_MS`; declaration for
  `GetFreyaRangedDpsRank`.
- `src/Ai/Raid/Uld/Util/UldEncounter_Freya.cpp` — nearest-lasher acceptance plus the outward walk in
  `GetFreyaLasherCampSpot`; `GetFreyaRangedDpsRank` next to `GetFreyaRangedCampAnchor`.
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.cpp` — blast floor in `FreyaRangedCampTrigger::IsActive`.
- `src/Ai/Raid/Uld/Action/UldActions_Freya.{h,cpp}` — the Gift share and its latch members in
  `FreyaSetDpsPriorityAction`.
- `docs/raids/ulduar/freya.md` — record the four-wave comparison, the nearest-lasher distance table,
  and the Gift's cost. `docs/engine/pitfalls.md` needs a line that a trace proves which build ran:
  check a multiplier veto label or an action name before drawing any conclusion from a pull.
  Both docs are governed, so run `/compact-docs-writer` before editing either.
- Plan copied to `docs/plans/freya-lasher-camp-standoff-and-gift-share/freya-lasher-camp-standoff-and-gift-share.PLAN.md`.

## What this deliberately does not fix

The single strongest signal in the data is not addressed here, and a fresh session should not mistake
that for an oversight. In the clean wave the pack spawned on top of the melee, 7 of 8 were in contact
2.5 s later, and the pack died at the melee stack without one lasher ever reaching the back line. In
the three lethal waves the melee were 19-27 yd away and needed 6.0-6.4 s. They cannot win that race:
in P2 the melee needed to cover a median of 29 yd and covered 11.4 in six seconds — an effective
1.9 yd/s against a 7 yd/s run speed, stuttering through `reach melee` and mid-cast 19% of frames —
while the pack crosses at 8.0 yd/s. A melee-intercept node was considered and deliberately left out of
this round to keep the number of untested changes down.

Also out of scope: the Snaplasher that one-shot Shadow for 43,801 at full health at P2 0:57;
`ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS` (12.0 in code, 5.0 as recorded, unresolved and needing a DBC
check); everything in `3fbddfce3` itself, which stays as committed.

## Verification

The module cannot be compiled headless here, so everything past step 2 is a hand-off.

1. **Static**: `python apps/codestyle/codestyle-cpp.py`; every new symbol resolves in
   `UldTriggerContext.h` / `UldActionContext.h`.
2. **Build** the worldserver in Docker. This is the step that was skipped last cycle — confirm the
   binary's mtime moves past the commit before pulling anything.
3. **Confirm the build actually shipped** before reading any trace as evidence: `freya lasher about to
   blow` must appear at least once, and `avoid aoe` must show `freya avoid aoe hold` vetoes rather than
   executions at relevance 90.
4. **Re-pull** Freya 25 hard mode and measure against the numbers above:
   - Ranged/healer distance to the nearest lasher during a Detonating Lasher wave: median **18+ yd**,
     and time spent inside 15 yd down from 43-85% toward the clean wave's **18.5%**.
   - Pack kill rate up from 22-24 %hp/s toward **39**.
   - Lashers reaching within 8 yd of a ranged or healer: **6-8 of 10** today, expect 0-2.
   - Wave deaths among ranged and healers: **15 of 22** today, expect a clear drop.
   - Eonar's Gift must still die: no Gift may live past **12 s** or end above 10% health while the raid
     is healthy. This is the regression that matters most — a Gift that lives heals Freya 30-60%.
   - **Guard against the lattice failure**: melee damage on the wave must stay in the 40k+ band and the
     median bot-to-nearest-lasher for melee must stay under 10 yd. If melee output falls, the camp
     standoff went too far — pull it back toward 16, do not widen it further.
5. `python tools/botobs/postmortem.py <trace>`, plus `--clump` and `--stalls`.
