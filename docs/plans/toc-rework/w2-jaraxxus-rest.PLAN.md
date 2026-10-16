# w2-jaraxxus-rest — Lord Jaraxxus, Fel Lightning hit matching and spread

Wave 4d, a refine lane. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Mechanics, tactics and the node ladder:
`docs/raids/trial-of-the-crusader/lord-jaraxxus.md`.

Trace (25N kill 2:22, 0 deaths, raid DPS 177k, human tank "Dragon", build carrying waves 3-4c):
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/649_3_lord-jaraxxus_1792176436.ndjson`.

## Findings from the 2026-10-16 25N pull

Clean: all 11 Fel Fireballs kicked; every Nether Power purged or stolen within about 1 s; all 5
Incinerate Flesh windows healed through, no Burning Inferno; fire damage only on the human tank.
Fel Lightning hit the full chain of 5 on all 7 casts: 35 hits, about 5k each, 175k in total, 37% of
the raid's damage taken; nobody died of it. `bosses/lord_jaraxxus.py <f> --boss` matched only 5 of
those hits to their casts ("0.7 a cast", "30 hits matched no cast").

## Scope

1. **Fix the Fel Lightning cast-to-hit matching** in `lord_jaraxxus.py`, with a test built from this
   trace's shape.
2. **Decide the spread from the corrected numbers.** On 25N it cost about 25k a cast, no deaths.
   Recommended: no spread on normal; heroic decided by its own numbers. Log either way.
   - Fel Lightning: instant, every 10-15 s, on a random player (tanks included), chaining to
     3/5/3/5 targets (10N/25N/10H/25H, row 66528/67029/67030/67031). A magic chain jumps 10 yd
     (`Spell::SearchChainTargets`; no `spell_jump_distance` row), measured with `IsWithinDist`,
     which adds both combat reaches. The guide spreads ranged and healers evenly around the room;
     stacking them is its named alternative, harder on healers and on Legion Flame carriers.
   - Weigh the chain's damage against what else kills (pitfalls: "A gather node is a damage
     amplifier…", spacing is a step function) before building anything.
3. **If a spread is built:**
   - *Coordinates.* The arena floor is gameobject 195527, absent from the static navmesh and vmaps:
     navprobe verifies no floor point. Derive spots live (`ValidateFloorPoint`,
     `FindNearestPositionClearOfHazards`) or from positions bots stood on in traces; fix none from a
     model of the room.
   - *Coexist with the lane's nodes*: a spread slot must avoid Legion Flames (60 s each) and yield to
     `jaraxxus avoid legion flame` (`ACTION_EMERGENCY + 8`); healers stay within
     `sPlayerbotAIConfig.healDistance` of the tank and the Incinerate Flesh target; a positioning
     action returns false once parked, or it starves the interrupt and heal nodes below it.

**Wave 4e** (`w8-prepull-staging`) takes over `jaraxxus intro main tank`
(`ToCTriggers_Jaraxxus.cpp:28`, `ToCActions_Jaraxxus.cpp:158`): leave that node alone.

## Owns

Stem `Jaraxxus`; `docs/raids/trial-of-the-crusader/lord-jaraxxus.md`; `tools/botobs/bosses/lord_jaraxxus.py`
and its test.

## Investigation

- **Why the matching fails.** `Spell::prepare` runs `cast(true)` for a zero cast time *before*
  `sScriptMgr->OnSpellPrepare` (`Spell.cpp:3798-3804`), so a Speed 0 instant's `cast` row trails its
  own `dmg` rows by 0-3 ms (trace: hits at 11855-11857, cast row 11857). An instant with a travel
  speed (`Spell.cpp:4127`; Legion Flame 66197 Speed 30, Incinerate Flesh 66237 Speed 20) lands
  after its row instead, late by the flight time. `fel_lightning` takes
  `bisect_right(casts, hit) - 1`, the previous cast 10+ s back, so only a hit sharing the cast row's
  ms matched: 5 of 35.
- **Corrected numbers** (25N): 7 casts, all 5 hits, 35k a cast with absorbs (`a` 174,802 + `ab`
  70,996 = 245,798, 37.9% of the raid's 648,568 taken); 98% of all ranged and healer damage taken;
  every hit landed on 100% health, no lethal hit, no death, lowest ranged 59.7%; every cast at 0
  Nether Power stacks. Jumps 0-4.9 yd across all casts. Melee and tank picks (2) chained only melee
  and tanks; caster picks (5) the same five casters, hops 0.4-3.3 yd.
- **Chain mechanics** (`Spell::SearchChainTargets`, spell CSV): DmgClass 1, jump 10 yd from the last
  victim by `IsWithinDist` (both combat reaches, 1.5 each: 13 yd centre to centre, 3D), in LOS,
  nearest first. Base roll 7.9-9.1k/9.7-11.3k/9.7-11.3k/11.7-12.3k (BasePoints+1 to +DieSides).
  `EffectChainAmplitude` 0.8/0.9/1.1/1.1 multiplies each jump (`Spell.cpp:8684`): at the mean roll
  a full chain is about 21k/43k/35k/73k raw, 25H's fifth victim 17.6k against ~24-27k caster health.
  That assumes no Nether Power stack: 66228/67108 is aura 79 (MOD_DAMAGE_PERCENT_DONE, +20% a stack,
  school mask 126 covers Fel Lightning), so ×2 at a full 5, ×3 at a full 10 (25H ~220k a chain,
  52.7k on the fifth). `DelayEvents(5s)` gives removers 5 s; on the 25N trace stacks were gone in
  ~0.9 s.
- **Fel Fireball duty never fires.** Coverage: `jaraxxus fel fireball interruptible` asked 10,916x,
  never true; `interrupter` never `1`. Generic interrupt nodes kicked all 11 casts 7-103 ms in
  (median 50), one kick each, before the duty holder latched. `interrupter` is change-only, so the
  trace can't tell a duty node beaten to the kick from a dead one. Outcome perfect, so no C++
  change; the reader's "11 cast(s) with nobody latched" reads as a fault and is reworded (task 3).
- **`addtank` absent, Mistress 89% on non-tanks**: the only assist tank was the human Dragon
  (5105); the bot paladin Bulwark held the boss. Not a bot bug; logged as a Known gap.
- Coordinates: none proposed, so no navprobe run (the arena floor is GO 195527, unverifiable offline).

## Task list

One group: the lane has no C++ work. Checks: `python -m unittest discover -s tools/botobs/tests`
(no `.cpp` changed, so no syntax check or pblint run is needed).

### Group 1 — reader

Files: `tools/botobs/bosses/lord_jaraxxus.py`, `tools/botobs/tests/test_lord_jaraxxus.py`,
`docs/raids/trial-of-the-crusader/lord-jaraxxus.md` ("Known gaps" `--flame` line and "What a trace
answers" only; the rest of the doc already carries this lane's facts).

1. **Fel Lightning hit matching** (`fel_lightning`): match each hit to the *nearest* Fel Lightning
   cast within `CHAIN_MS` on either side (casts are 10+ s apart, so nearest is unambiguous); a hit
   with none is stray. Add a module-docstring bullet: a Speed 0 instant's `cast` row comes after its
   hits, because `Spell::prepare` casts it before the hook fires; one with a travel speed lands after
   the row. On the 2026-10-16 trace `--boss` must
   then read 7 casts, 35 hits, 5.0 a cast, `5 x7`, and no stray line.
2. **Spread-verdict figures** in `fel_lightning` rows and `show_boss`: per hit keep victim, `a`, `ab`,
   `hp` (before) and `ok`; per cast `taken` (a+ab), `absorbed`, first victim, and the largest jump:
   3D distance between consecutive victims from `Samples.row(guid, t, FRESH_MS)` (`row[1:4]`), `None`
   when either is unsampled. Constant `JUMP_YD = 13.0` with a comment (10 yd plus both 1.5 combat
   reaches). Summary: total taken and share of all roster `dmg` (a+ab); lowest `hp` a hit landed on
   and lowest health a hit left (`hp - a * 100 / trace.maxhp[victim]`, skipped without `maxhp`);
   lethal hits (`ok > 0`); deaths whose `rewind` rows (`[t, source, sp, amount]`) hold a Fel
   Lightning id within 5 s before the death (`death_records`). A per-cast table: at, first victim and
   role, hits, taken, max jump against `JUMP_YD`, `np` (his `jaraxxus.netherpower` stacks at the
   cast, `-` before the first note). Keep `hits per cast`.
3. **Fel Fireball kick delay** (`fel_fireballs`): add `delay`, first kick minus cast start, `None`
   without a kick, and a `delay` column (ms). Replace the "nobody latched" warning: kicked casts with
   nobody latched print as "N kicked by a bot with nobody latched as interrupter, median X ms in",
   first kicker in `trace.humans` apart as "N kicked by a human with nobody latched"; the "nobody
   latched" warning stays for casts that landed or had no hit. Trace: 11 by a bot, median 50 ms.
4. **Repeat tank window** (`carrier_windows`): `tank` = every `jaraxxus.flame` switch inside the
   window other than `none` is `tank`, and there is one; a window with no switch goes by the value in
   force at its start (`holder_spans`), so a stale `dodge` before a window that writes `tank` doesn't
   count. `branches` stays the switches inside. Delete the `--flame` line from the boss doc's Known
   gaps.
5. **Tests.**
   - Reshape the synthetic Fel Lightning rows to the real order: hits 0-2 ms *before* their cast row
     (hits 10000/10001/10001, cast 10002; hit 20000, cast 20001; stray 25000 stays): `[3, 1]`,
     stray 1, damage still summed. Give some hits `ab`, `hp`, `ok` and pin taken, absorbed, lowest
     health landed on and left, lethal count. Add a `death` with a Fel Lightning `rewind` row inside
     5 s (counted) and one outside (not), on roster members (hunter, warlock) that do nothing else.
   - A separate small 25N trace (own `setUpClass`, so the 25H fixture's other assertions don't move)
     shaped like the 2026-10-16 casts: 5 hits at t, t+1, t+1, t+1, t+2 with the cast row at t+2, and a
     second cast whose last hit is 1 ms before its row: 5 and 5, stray 0, with snapshot positions
     pinning the max jump (one pair 14 yd apart, printed `14.0!`). A `netherpower` note between the
     casts pins `np` as `None` then 10. Three unlatched kicked Fel Fireballs (two by bots at 50 and
     150 ms, one by a human) pin the bot line with median 100 and the human line.
   - Fireball delay: the kicked 17000 cast reads 800; landed casts `None`. The 25H pull prints
     `1 cast(s) with nobody latched` and no "kicked by" line.
   - Repeat tank window, in its own minimal trace: two windows on one tank with a single `tank` note
     before the first and none after both read `tank`; a tank holding `dodge` before its window that
     writes `tank` 100 ms in reads `tank`; a window seeded `none` then switching to `carrier` does not.
   - Keep `test_every_section_prints_on_the_pull` passing against the new `--boss` text.
6. **Boss doc, "What a trace answers"**: rewrite the `--boss` bullet for the new figures and kick
   delay (drop "which decides whether a spread is worth building"; the decision and its reopen
   condition are under Tactics); `--flame` "not judged when every value it switched to is `tank`, or
   with no switch the one latched at its start". No change list, per `docs/README.md`.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| No Fel Lightning spread on any difficulty | 25N trace: 0 deaths, every hit on full health; guide names the stack as its alternative (casters in range of Portal and Volcano, the heroic DPS check; stronger AoE heals), its stated risk is Legion Flame inside the stack, which bots walk out; conservative on heroic per the overview's Decisions, since no heroic trace exists | Heroic-only spread: 25H's chain grows ×1.1 a jump (73k raw, 17.6k on the fifth, at the mean roll and no Nether Power stack; each stack left adds 20%), but it needs 13+ yd between 14 ranged and healers and the melee ball on an unverifiable GO floor, against 60 s flames, heal range and adds, and melee or tank picks still chain 5 |
| Reopen condition: a heroic death with a Fel Lightning cast at 0 stacks in its last 5 s (`np` column) | Pitfalls: weigh the AoE against what kills, not against zero; a death under stacks is a remover failure, which a spread doesn't fix | A damage-share threshold: it is 98% of caster damage even on a clean kill, so it says nothing |
| Hits match the nearest cast either side within `CHAIN_MS` | Core: the `cast` row of a Speed 0 instant trails its hits | Match only the next cast row after a hit: breaks if the hook order ever changes |
| No C++ change for the Fel Fireball duty node | Trace: generic interrupts kicked 11/11 within 103 ms, one kick each | Veto generic interrupts on him so the duty node owns the kick: risks a missed kick for no measured gain |
| Fix the `--flame` repeat tank window here | Boss doc Known gaps names the fix; lane owns the file; small | Leave for `w6-closeout` |
| Lightning deaths read from `combat_deaths` | A `wipe`-command kill (`cause` `reset`) carries no blow, so Fel Lightning cannot cause it | Raw `death_records`: a reset kill whose rewind holds a hit would count |
| Fireball `delay` set only on a `kicked` outcome | Task 3: landed casts read `None` | First kick on any cast: a wasted kick inside a landed cast would read as a delay |
| A max jump past `JUMP_YD` prints with `!`, not dropped | A chain never hops past 13 yd, so a longer one is stale or moving samples (`FRESH_MS` 1 s) | Drop it: hides the sampling error |
| A window's latched `flame` value counts only when the window wrote no switch; a window whose only switch is `none` reads not-`tank` | Review: a bot tank keeps a stale `dodge` until its first carrier tick, and `none` is what the carrier wrote, replacing the seed | Fall back to the seed whenever the switches are all `none`: would judge a failed carrier search by a value it overwrote |
| Unlatched kicks split by first kicker: bot (with median delay) or human (`trace.humans`) | Review: a human kick says nothing about the bot interrupt nodes | One "generic interrupt" count: hides whether bots kicked at all |

## Engine lesson for the merge stage

An instant with no travel time (Speed 0), such as Fel Lightning, writes its hits 0-3 ms before its
`cast` row: `Spell::prepare` runs `cast(true)` for a zero cast time before `OnSpellPrepare` fires.
One with a travel speed (`Spell.cpp:4127` sends Speed > 0 to the delayed path; Legion Flame,
Incinerate Flesh) lands after the row, late by the flight time. Match a Speed 0 instant's effects to
the nearest cast, either side. Belongs in `docs/systems/observability.md` next to the `cast` row
("cast start" holds only for a cast time).

## Carried over

None.

## Review

Open issues after re-review: none. Deferred findings: none.

## Known gaps

- No heroic Jaraxxus trace: the no-spread decision is untested on 10H/25H.
- A human assist tank who leaves the Mistress gets no bot cover (in the boss doc).

## Blocked
