# Algalon round 9: constellations into holes, stars under Big Bang, explosions survived

## Context

One 25N pull on the round-8 build (binary 2026-10-06 01:22, after `706ceb514`):
`env/dist/logs/botobs/603_7_algalon-the-observer_1791277329.ndjson` (paths from the core root). Read with
`python tools/botobs/bosses/algalon.py <trace> --verdict --deaths --stars --constellations --defensives --smash --bigbang`.
Scratch scripts behind the numbers below: `scratchpad/r9/{const,kite,kitetime,dmgtaken}.py`.

**Outcome.** Enrage: Ascend at 6:08.5 with Algalon at 28%, killing 13. The raid lost 0.43 %/s until 1:33, but
only 0.12 %/s after 3:11, once 10 damage dealers were dead. 29 deaths:

| When | Dead | Cause |
|---|---|---|
| 1:48.1 | 5 (Stormweaver, Druidica, Totemist, Trueshot, Justice) | under-star 1480 died 1.6 s after Big Bang 1's hit, raid just out of the void, 18 under the line |
| 1:56.3 | Druidica (back up after 1:48) | meteor splash at 17 yd |
| 2:55.3-2:58.2 | 3 (Stormweaver, Smartface, Agony) | paced star 1705 with **no cover**, though Holylight's Aura Mastery and Dragon's Divine Sacrifice were ready; the last two finished by Arcane Barrage |
| 3:12.9 | 2 (Power, Nightwarrior) | dodged the meteor, then `algalon big bang hide action` walked them back to 5.9 / 8.2 yd before the impact (44 k / 50 k) |
| 3:48.8 | 4 (Angry, Totemist, Elemena, Nightwarrior) | paced `deadline` kill with 7 under the line, covered by Divine Sacrifice. Totemist's Shamanistic Rage had been spent by its class rotation at 3:28.2 |
| 6:10.6 / 6:18.3 | 13 + Deathsong | enrage, then a star during Big Bang 4 |

**What held.** Every Big Bang soak was fine (Malediction in Dispersion, 5.8-6.5 k) and nobody was left out of a hole.
All 8 covers went on live stars. No button was pressed at a dead star or while phased. Prayer, Elemena and
Nightwarrior used a healthstone or potion. No swap tank died.

### Stars under Big Bang

**Under-stars:** 3 of 7 died while the raid hid (Big Bangs 1-3), each hitting only the soaker for 1.2-1.6 k. In the
last two pulls, 1 of ~6 did.

| Big Bang | Planned | Outcome |
|---|---|---|
| 1 | 1480, 1481, 1482 | 1482 ✓ (hit + 0.7 s); 1480 one tick late (hit + 1.6 s, killed 5); 1481 chewed by splash, released `early` |
| 2 | 1783, 1784 | 1784 ✓ (hit − 1.9 s); 1783 released `late` at 49%, paced, no deaths |
| 3 | 1816, 1817 | 1817 ✓ (hit + 0.6 s); 1816 chewed by Deathsong's (human DK) Death and Decay, 18.9% → 7% in 6 s, released `early`, paced |

- **1480 had no trimmer.** It stayed 40-66 yd from every wand user from 1:16 to 1:38, so `CanTrim`'s 28 yd range
  check never passed. It sat 1% (one tick) above its band from 1:18 to the cast with no wand on it.
  - `TrimWorkMs` assumes at least one wand, so the `late` release never fired.
  - Wands are the wrong tool anyway. A trim lands 0.2-0.3 %/s, and every wand second is a damage dealer off
    Algalon, against the enrage. The bulk stops 5% above the band only to leave that last stretch to the wands.
- **The bulk is a third of its planned speed.** Measured loss beyond the drain on under-star focus spans: 0.9-3.7 %/s,
  against the 6 %/s `BulkWorkMs` assumes.
  - `AlgalonTargetGuardMultiplier` holds every DoT and channel on an under-star.
  - Bulk on 1784 (2:25-2:47): the affliction warlock cast 6 Shadow Bolts and 2 Haunts (Corruption, UA and CoA
    vetoed). The shadow priest cast 9 wand shots, 3 Mind Blasts and 2 SW:D (Mind Flay vetoed). The mage cast 6
    Arcane Blasts.
  - So the second star of Big Bangs 2 and 4 was released `late` at 49%.
  - `StopCastsAtLine` already interrupts a channel at the line.

### Living Constellations: not efficient

- **Removals.** 5 of 11 went into holes, all during a Big Bang cast. 5 died to AoE: Death and Decay from Deathsong
  and Obliteration, plus the handlers' Seal of Vengeance.
- **Lifetime.** They lived 95-181 s.
- **Holes.** 3-6 holes stood free from 1:48 on (`ReservedHoles` keeps 1 within 30 s of a cast), and no
  constellation was ever kited into one between casts.
- **The kite barely ran.** Counting only the current handler's notes, the branch was `taunt` for ~247 of 360 s, and
  `stage`/`wait`/`cross` for ~10 s: one attempt, 1:50-1:58.5, cut off when wave 2 woke.
- **Arcane Barrage cost.** 145 hits, 7.3 k each, 1.06 M on the raid; 133-171 k per 30 s from 2:00 to 4:30. That is
  the raid health missing at 2:55 and 3:48.

Causes:

1. **A stuck constellation counts as loose forever.**
   - 33052 guid `…712` spawned at (1685.6, −300.1, z 443.2), 53 yd from the centre. It never came lower than z 433
     (16 yd above the floor).
   - Each wave woke it. About 8.5 s later it lost its victim, flew home and evaded. `CreatureAI::_EnterEvadeMode`
     doesn't restore `UNIT_FLAG_NOT_SELECTABLE`, so the scan's `active` (`UldEncounter_Algalon.cpp:404`) stays true.
   - `GetAlgalonHandlerConstellation` (`:4305`) counts any active one whose victim isn't the handler as loose, so
     the stuck one keeps it in the `taunt` branch.
   - Example: at 2:11.7 every reachable one was on Dragon, 3 holes were spare, and the branch written was `taunt`.
   - The script's own flag (`npc_living_constellation::GetData` → `_isActive`) is cleared on evade.
   - The core only lets one within 5 yd of a hole's height close it (`SpellHit`).
2. **"Loose first" holds the kite through every 8 s taunt cooldown.**
   - The handler is the swap tank not holding Algalon, so it changes at every swap: 1:11, 2:22, 3:25, 4:29 and 5:38.
   - The new handler re-taunts each one with Hand of Reckoning, 8 s apart. At 3:25-4:02 it took 37 s to collect 5.
   - Deathsong pulled 5 off it between 4:03 and 4:49.
   - While any one is loose the held ones are never kited.

### Explosions

- **2:55.3: no cover.**
  - Paced kill of 1705: 6.1% at 2:54.03 and 3.2% at 2:54.26, then it died at 2:55.30.
  - The 4 s slope lagged the kill, so it came inside the 2 s cover lead (`EXPLOSION_COVER_LEAD_MS`) only about 1 s
    before it died.
  - By then Holylight, the Aura Mastery provider, was in a Holy Light (2:54.02, 1.33 s). A bot only thinks when its
    cast ends.
  - Aura Mastery is off the global cooldown, so `ExternalReady` still picked Holylight every tick. Dragon's Divine
    Sacrifice, last in line, was never reached.
- **1:48.1: no buttons pressed.**
  - Totemist and Stormweaver started Chain Heal and Justice started Holy Light (1.8-2.2 s) in the void at 1:47.0-1:47.2.
  - The strip came at 1:47.52 and they were still casting when the star went at 1:48.08. Totemist (Shamanistic Rage)
    and Justice (Divine Protection) had a button that saved them.

## Implementation

Code is in `src/Ai/Raid/Uld/{Util,Action,Trigger,Multiplier,Definition}/*Algalon*`. E is
`Util/UldEncounter_Algalon.cpp` and EH its header. The work lands as three commits, A, B and C, each with its own
docs and analyzer part.

### A. Constellations

1. **Only a reachable, script-active one counts.**
   - For constellations, the scan's `active` becomes "selectable and `creature->AI()->GetData(0)`".
   - In `GetAlgalonHandlerConstellation` and `GetAlgalonKiteSpot`'s held set, skip one more than 5 yd above the room
     floor.
   - So the stuck one (home, or its 8.5 s chase in the air) is neither loose nor held.
2. **Kite while the taunt cools.**
   - In `GetAlgalonHandlerConstellation`, add a case: held ones, not gathering, `holes.size() > ReservedHoles`, and
     the class taunt on cooldown. Then choose the held one, so the kite trigger runs.
   - Loose first only when the taunt is ready.
   - Add `ClassTauntReady(PlayerbotAI*)` beside `CastClassTaunt` (`src/Util/EncounterHelpers.{h,cpp}`): the class's
     taunt by name through `"spell id"`, and `!HasSpellCooldown`.
   - The kite's stage timers already restart a stalled walk.

### B. Stars under Big Bang

3. **No wands: the team takes each under-star into its band.**
   - **The stop line.** `BulkLeftHp`'s line becomes half a tick under the band top (`underHi − tickHp / 2`) instead
     of band top plus 5%.
   - **Why that line is safe.**
     - The band is 4-5 ticks wide (hit − 4 s to hit + 0.6 s), and one nuke landing after the line is 1-2%. So the
       star ends inside the band.
     - An overshoot below `underLo` gets the existing `early` release, so pacing kills it before the cast.
     - The star team works a far star from its own spot (`StarTeamSpot`), so distance no longer matters.
   - **Casts and channels.** `StopCastsAtLine` interrupts every cast and channel on an under-star at that same line.
     The team's focus is no longer excepted in the `under` window.
   - **Remove the wand trim:**
     - code: `AssignTrimmers`, `CanTrim`, `TrimsAny`, `IsTrimmer`, `TrimmerCount`, `TrimWorkMs`, `state.trimmers`,
       `AlgalonStarTrimAction` and its trigger, the `AlgalonTrimming` `OwnMovement` rule and `GetAlgalonTrimStar`;
     - constants: `UNDER_BULK_BUFFER_PCT` and the `UNDER_TRIM_*` set;
     - the `algalon.trim` trace.
   - **The plan's budget.** `UNDER_TRIM_ALLOWANCE_MS` (18 s, one wand's trim) gives way to a small overshoot
     allowance, `ULDUAR_ALGALON_UNDER_FINISH_MS` (5 s). The `late` rule tests the bulk's work alone.
   - The analyzer keeps reading `algalon.trim` in old traces.
4. **A faster, honest bulk.**
   - `AlgalonTargetGuardMultiplier`: on an under-star, channels are no longer held; `StopCastsAtLine` breaks them at
     the line.
   - DoTs are held only while the star is within `ULDUAR_ALGALON_UNDER_DOT_ROOM_PCT` (20) of its stop line
     (the new line from 3). A warlock's full set is ~12% of a 25-man star and a shadow priest's ~6%.
   - The guard reads that room through a new EH accessor.
   - `BulkWorkMs`: measure the bulk's own speed (the latched bulk star's slope after 4 s on it, `state.bulkPctPerSec`).
     Use `min(measured, 6)` like `teamKillPctPerSec`, so the plan stops promising stars the bulk can't reach.

### C. Explosions and the hide

5. **The tick breaks a cast that stands between a bot and its button.**
   - New `BreakCastsForExplosion`, called from `AlgalonTickEncounterState`.
   - It applies to every unphased bot outside the swap pair that is mid-cast or channelling, with an `ExplodingStar`
     due within the personal lead that it hasn't claimed, health at or under `StackedHit`, and a button off
     cooldown or a `ReadyConsumable`.
   - For such a bot it calls `InterruptNonMeleeSpells(false)` and traces `algalon.castbreak personal`.
   - Factor `GetAlgalonExplosionPersonal`'s test, so the tick can run it without the cast check.
6. **Cover in time.**
   - `ULDUAR_ALGALON_EXPLOSION_COVER_LEAD_MS` goes from 2000 to 3500. Aura Mastery and Divine Guardian last 6 s, and
     the slope ran about 1 s late on a paced kill.
   - The tick interrupts the chosen provider's cast (`ExplosionCoverProvider` result, a bot) and traces
     `algalon.castbreak cover`.
7. **The hide waits out a meteor.**
   - In `AlgalonBigBangHideAction` and its trigger, while a live marker's impact is still ahead and the shelter or
     the straight walk to it lies within `ULDUAR_ALGALON_COSMIC_SMASH_CLEARANCE` (15 yd) of it, the bot doesn't run.
   - Exception: if the time from the impact to the hit is less than its walk plus `ULDUAR_ALGALON_HIDE_MARGIN_MS`, it
     runs anyway (Big Bang is 107 k, a late under-star 13 k).
8. **Buttons kept for explosions.**
   - Widen the Definition's "algalon phased button hold" from "phased" to all of phase 1, for bots outside the swap
     pair above 35%.
   - It covers barkskin, shadow ward, icebound fortitude, shamanistic rage and divine protection. The explosion node
     and the tank nodes cast by id, so they are unaffected.

### Analyzer (`tools/botobs/bosses/algalon.py`, tests in `tools/botobs/tests/test_algalon.py`)

- `--constellations`:
  - Branch time counted from the current handler's notes only (`algalon.handler`); today each tank's last note
    runs on, giving 686 s in a 370 s pull.
  - Flag a constellation that never came within 5 yd of the floor.
  - Count the seconds the kite was blocked (`taunt` with every reachable one held and a spare hole).
- `--stars`: per under-star, its health at the cast against its band, and the bulk rate per under-star span against
  `BulkWorkMs`'s 6.
- `--defensives`: every `algalon.castbreak`, with the bot, its broken spell and the star's death beside it.
- Verdicts:
  - a cover provider casting when its star died;
  - a bot dying to an explosion mid-cast with a button ready;
  - a hide run inside 15 yd of an impact it walked into.
- A fixture test for each.

## Docs, checks, commit

- On approval, save this plan as `docs/plans/algalon-round-9/algalon-round-9.PLAN.md`, and delete it in the last
  commit.
- After each commit's code:
  - Syntax-check the changed `.cpp` files plus the 7 includers of `UldEncounter_Algalon.h`: Util, Action, Definition,
    Trigger, Multiplier Algalon, Multiplier Shared, UldStrategy. For A, also the includers of `EncounterHelpers.h`.
    Use `scratchpad/pbchk/pb-syntax-check-core.sh`, which mounts the current core src.
  - Run `tools/pblint/pblint.py` on the changed paths, `python apps/codestyle/codestyle-cpp.py`, and
    `python -m unittest discover -s tools/botobs/tests`.
  - Review with a fresh agent (brief through prepare-prompt) and fix the confirmed findings.
- Update `docs/raids/ulduar/algalon.md` through `/compact-docs-writer`:
  - Constellations: stuck one, kite during cooldown.
  - Stars: the bulk into the band and the wand trim gone, DoT room, measured bulk.
  - Big Bang: hide vs meteor.
  - Cooldowns: lead, cast breaks, button hold.
  - The trace.
- Commit A, then B, then C, each with only its own files. No amend, push or branch, and no AI trailer.

## Verification (user, in game)

Pull 25N, then run `algalon.py <trace> --verdict --constellations --stars --defensives --smash`. Expect:

- **Kites between casts.** `stage`/`wait`/`cross` between Big Bangs, constellations eaten while holes are spare, and
  none flagged blocked.
- **The stuck one, if it spawns.** Flagged, and never the handler's taunt target.
- **Under-stars.** At most one release per Big Bang, every under-star inside its band at the cast with no wand
  shot, and a bulk of 4 %/s or more beyond the drain.
- **Explosions.** `algalon.castbreak` before covers and buttons, every paced explosion covered, and no explosion
  death mid-cast with a button ready.
- **Meteors.** Nobody brought back inside 15 yd by `algalon big bang hide action`.

Not in scope: Deathsong's Death and Decay chewing stars and pulling constellations is a player's choice. The 4 healers
are the roster.
