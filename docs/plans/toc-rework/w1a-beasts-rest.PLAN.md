# w1a-beasts-rest — Arctic Breath spread, Fire Bomb impact

Carried over from `w1a-beasts` under the oversized-lane rule; runs once that lane has merged. Rules,
sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md). Mechanics, ids and the
stage model: `docs/raids/trial-of-the-crusader/northrend-beasts.md`. Guide:
`beasts-of-northrend-master-strategy-guide-toc-25/`.

## Scope

1. **Arctic Breath spread.**
   - The cone is 60° on 10N and 24° on the other three, 100 yd, aimed at a random target within 90 yd;
     5 s freeze plus damage.
   - Guide: ranged and melee in a half circle behind Icehowl, healers far apart.
   - Spread by bearing around him, sized past the cone's width at the stand radius: a spread tighter
     than the AoE buys nothing (`docs/engine/pitfalls.md`, gather/spread).
   - He is held where each charge leaves him, so the stands are boss-relative: clamp, don't chase
     (`docs/engine/raid-mechanics-lessons.md`). Stand down while `icehowl charge guard` is latched.
2. **Fire Bomb impact.** NPC 34854 appears at its target; 66317 hits 8 yd around it after a 1 s cast
   and a 14 yd/s missile from Gormok. Dodge a bomb younger than the impact, read from
   `TempSummon::GetTimer()` against its 60 s lifetime, to a clearance past 8 yd. The 2 yd pulse stays
   with the generic `avoid aoe`.
3. **Observability.** `nb.` probes for both, and the matching sections in
   `tools/botobs/bosses/northrend_beasts.py` and its test.

## Owns

The Arctic Breath part of the `Icehowl` stem, the Fire Bomb part of the `Gormok` stem, their sections
of `northrend-beasts.md`, `northrend_beasts.py` and its test.

## Task list

Verified mechanics (Fire Bomb timing, Arctic Breath cone and targeting) are in
`docs/raids/trial-of-the-crusader/northrend-beasts.md`; read it first. Paths are under
`src/Ai/Raid/ToC/` unless they start with `docs/` or `tools/`. No new files, so no CMake re-run, and no
registration seam outside the owned stems: each group registers its trigger, action and multiplier in
its own stem's context and `AddToC<Stem>…` list. Trigger names keep the `icehowl` / `gormok` prefix so
`ToCGatedTrigger` gates them; new action names keep it too, so `GormokSnoboldCarrierMultiplier` spares
them. Remap a spell with a difficulty row at the call; new ids go in the stem's helper header.

Contracts the groups share (group C documents all of them and reads the keys):

| Key | Kind | Writer | Value |
|---|---|---|---|
| `nb.spread` | `NoteDerived`, per non-tank bot | A | its bearing step from directly behind Icehowl (`0`, `1`, `-1`, …), or `none` |
| `nb.arc` | `ObsValue<uint32>`, per instance | A | bearings the layout kept, 0 while it is off |
| `nb.bomb` | `NoteDerived`, per bot | B | the dodge's branch: `move <yd>`, `tight`, `hold`, `pinned`, `stunned`, `none`, `locked`; `clear` from `IsInFireBombImpact` while it is false for a living bot |
| haz `circle` | `NoteHazardCircle(map, 66317, bomb, FIRE_BOMB_IMPACT_RADIUS, ttl)`, once per bomb | B | `ttl` = modelled impact age minus the bomb's age at first sight |

The reader takes `FIRE_BOMB_IMPACT_RADIUS` from the source through `raidobs.geometry.radius`, so it
stays a `constexpr float` in `Util/ToCHelpers_Gormok.h`.

### Group A — Arctic Breath spread

Files: `Util/ToCHelpers_Icehowl.h`, `Util/ToCHelpers_Icehowl.cpp`, `Trigger/ToCTriggers_Icehowl.h`,
`Trigger/ToCTriggers_Icehowl.cpp`, `Action/ToCActions_Icehowl.h`, `Action/ToCActions_Icehowl.cpp`,
`Multiplier/ToCMultipliers_Icehowl.h`, `Multiplier/ToCMultipliers_Icehowl.cpp`.

1. **Constants** in `ToCHelpers_Icehowl.h`: `SPELL_ARCTIC_BREATH = 66689` (remapped at the call);
   floats `ICEHOWL_BREATH_DEFAULT_CONE_DEG = 24`, `ICEHOWL_SPREAD_MARGIN_DEG = 6`,
   `ICEHOWL_SPREAD_HALF_ARC_DEG = 90`, `ICEHOWL_SPREAD_HEALER_BEARING_DEG = 60`,
   `ICEHOWL_SPREAD_MELEE_MIN = 8`, `ICEHOWL_SPREAD_MELEE_MAX = 13`, `ICEHOWL_SPREAD_HEALER_MIN = 16`,
   `ICEHOWL_SPREAD_HEALER_MAX = 18`, `ICEHOWL_SPREAD_RANGED_MIN = 20`, `ICEHOWL_SPREAD_RANGED_MAX = 25`,
   `ICEHOWL_SPREAD_MIN_ROOM = 21`, `ICEHOWL_SPREAD_TRIGGER = 1.25`, `ICEHOWL_SPREAD_ARRIVE = 0.75`,
   `ICEHOWL_SPREAD_RELATCH_MOVE = 8`, `ICEHOWL_SPREAD_RELATCH_TURN_DEG = 30`.
2. **Layout**, per instance in a `RaidInstanceState`, refreshed at most once per `getMSTime()` ms, only
   from a living bot on map 649:
   - **Active** while Icehowl is engaged, not `REACT_PASSIVE`, the only engaged beast
     (`GetBeastsStageMask == BEASTS_STAGE_ICEHOWL`), no charge is latched, and his victim is a player
     inside his melee range (`IsWithinMeleeRange`). Otherwise off: keep the assignments, drop the latch,
     `nb.arc` 0. Icehowl not engaged at all also clears the assignments (wipe).
   - **Cone width** `W` = `sSpellMgr->GetSpellCone(GetSpellIdForDifficulty(SPELL_ARCTIC_BREATH, icehowl))`
     degrees, else `ICEHOWL_BREATH_DEFAULT_CONE_DEG`. Step `s = W / 2 + ICEHOWL_SPREAD_MARGIN_DEG`
     (18° on 25N/10H/25H, 36° on 10N).
   - **Latch** when it turns active, when Icehowl is more than `ICEHOWL_SPREAD_RELATCH_MOVE` from the
     latched spot, or when his victim's bearing is more than `ICEHOWL_SPREAD_RELATCH_TURN_DEG` off the
     latched front. It stores his spot and the anchor = bearing to his victim + 180°.
   - **Bearings** `anchor + k·s`, candidates in the order `k = 0, 1, -1, 2, -2, …` while
     `|k·s| <= 180° - s` (keeps the victim's own cone clear). Probe each at `ICEHOWL_SPREAD_RANGED_MAX`
     with `map->CheckCollisionAndGetValidCoords(bot, icehowl x, y, z, x, y, z)` (the bot as source, the
     player path this floor needs); room = 2D distance from him to the clipped point, 0 on a false
     return. Keep a candidate with room ≥ `ICEHOWL_SPREAD_MIN_ROOM`, until
     `2·floor(ICEHOWL_SPREAD_HALF_ARC_DEG / s) + 1` are kept (11, or 5 on 10N). None kept = off.
   - **Roster**, rebuilt at most once a second: living bots (`GET_PLAYERBOT_AI`) of the bot's group on
     the same instance, not `IsBeastsTank`; role healer (`IsHeal`), ranged (`IsRanged` and not healer),
     else melee, computed at the rebuild.
   - **Deal**, sticky: a bot keeps its `k` while that bearing is kept. An unassigned bot (first deal, a
     new or revived bot, a dropped bearing), taken healers → ranged → melee, each by guid, picks the kept
     bearing minimising, for a healer, (healers on it, `| |k·s| - ICEHOWL_SPREAD_HEALER_BEARING_DEG |`,
     occupants, `k`); for anyone else (occupants, `|k|`, `k`). Dead or departed bots drop out.
   - Writes `nb.arc` = bearings kept.
3. **Stand**, one pure read for trigger, action and multiplier:
   `bool GetIcehowlBreathStand(PlayerbotAI*, Position& stand)` and `bool HasIcehowlBreathSlot(PlayerbotAI*)`.
   Radius band by role (melee 8-13, healer 16-18, ranged 20-25), its top capped at the bearing's room − 1;
   stand = Icehowl's current spot + `clamp(bot's 2D distance to him, band)` along the slot's bearing.
   `GetIcehowlBreathStand` writes `nb.spread` (`k`, or `none` while off or unassigned).
4. **Trigger** `icehowl breath spread` → `icehowl move to breath stand`, `ACTION_RAID + 2`: the bot has a
   stand, `botAI->CanMove()`, and stands more than `ICEHOWL_SPREAD_TRIGGER` from it. A healer yields
   while its `"party member to heal"` is out of heal reach, the test
   `PartyMemberToHealOutOfSpellRangeTrigger` makes: `ServerFacade` 2D distance > `GetRange("heal") + 1 +
   contactDistance`, or no line of sight.
5. **Action** `IcehowlMoveToBreathStandAction : MovementAction`, `MOVEMENT_COMBAT`, the latch shape of
   `GormokWalkAction`: false within `ICEHOWL_SPREAD_ARRIVE` of the stand; true without re-issuing while
   its own walk is in flight (`isMoving` and `last movement` on the latched spot) and the spot is still
   within `ICEHOWL_SPREAD_TRIGGER` of the current stand; nothing issued while a cast pins the feet (no
   interrupt); own booking cleared when the walk stood still 500 ms past its issue.
6. **`IcehowlBreathSpreadGuardMultiplier`** ("icehowl breath spread guard"): zero
   `CombatFormationMoveAction` (and so `set behind`) for a bot with `HasIcehowlBreathSlot`, while the
   encounter is live. Test the action first. Register after the charge guard.

### Group B — Fire Bomb impact

Files: `Util/ToCHelpers_Gormok.h`, `Util/ToCHelpers_Gormok.cpp`, `Trigger/ToCTriggers_Gormok.h`,
`Trigger/ToCTriggers_Gormok.cpp`, `Action/ToCActions_Gormok.h`, `Action/ToCActions_Gormok.cpp`.

7. **Constants** in `ToCHelpers_Gormok.h`: `SPELL_FIRE_BOMB_IMPACT = 66317` (no difficulty row);
   uint32 `FIRE_BOMB_LIFETIME_MS = 60000`, `FIRE_BOMB_CAST_MS = 1000`, `FIRE_BOMB_IMPACT_SLACK_MS = 500`,
   `FIRE_BOMB_FALLBACK_IMPACT_MS = 5000`, `FIRE_BOMB_SCAN_MS = 200`; floats
   `FIRE_BOMB_MISSILE_SPEED = 14`, `FIRE_BOMB_IMPACT_RADIUS = 8`, `FIRE_BOMB_TRIGGER = 9`,
   `FIRE_BOMB_TIGHT_CLEARANCE = 9.5`, `FIRE_BOMB_CLEARANCE = 11`, `FIRE_BOMB_PULSE_CLEARANCE = 4`,
   `FIRE_BOMB_SWEEP_RADIUS = 20`; `static_assert` the order impact < trigger < tight < clearance.
8. **Bomb cache**, per instance in a `RaidInstanceState`: guid, impact age, noted. Rescan with
   `GetCreatureListWithEntryInGrid(NPC_FIRE_BOMB, 200)` at most every `FIRE_BOMB_SCAN_MS`, only while
   Gormok is engaged or a cached bomb is still young; drop guids that stop resolving. On first sight:
   impact age = `FIRE_BOMB_CAST_MS + 1000 · dist2d(source, bomb) / FIRE_BOMB_MISSILE_SPEED +
   FIRE_BOMB_IMPACT_SLACK_MS`, source = the summoner (`ToTempSummon()->GetSummonerGUID()`) when it
   resolves, else Gormok's guid slot, else `FIRE_BOMB_FALLBACK_IMPACT_MS`; write the haz row. Age =
   `FIRE_BOMB_LIFETIME_MS - GetTimer()`; a bomb that isn't a `TempSummon` ages from first sight. Young =
   age below its impact age.
   - `void GetFireBombs(PlayerbotAI*, std::vector<Position>& young, std::vector<Position>& old)`.
   - `bool IsInFireBombImpact(PlayerbotAI*)`: alive, not the victim of any engaged beast, within
     `FIRE_BOMB_TRIGGER` (2D) of a young bomb.
9. **Trigger** `gormok fire bomb incoming` → `gormok dodge fire bomb`, `ACTION_EMERGENCY + 2`, on
   `IsInFireBombImpact`.
10. `GormokWalkAction::WalkTo` gains `MovementPriority priority = MovementPriority::MOVEMENT_COMBAT`.
11. **Action** `GormokDodgeFireBombAction : GormokWalkAction`, walking at `MOVEMENT_FORCED`:
    - not `IsInFireBombImpact` → `DropWalk`, false; `!botAI->CanMove()` → `stunned`, false;
    - a walk in flight whose spot clears every young bomb by `FIRE_BOMB_TIGHT_CLEARANCE` → `hold`, true;
    - sweep `FindNearestPositionClearOfHazards` over `FIRE_BOMB_SWEEP_RADIUS`, `preferNear` Gormok for
      melee DPS and snobold carriers, else his victim. Circles: young bombs at `FIRE_BOMB_CLEARANCE`,
      old bombs at `FIRE_BOMB_PULSE_CLEARANCE`, and Gormok at `GORMOK_STOMP_CLEARANCE` for a ranged
      DPS or healer carrying nothing. With no spot drop Gormok's circle, then the old bombs, then take
      young bombs alone at `FIRE_BOMB_TIGHT_CLEARANCE` (`tight`); still none → `none`, false;
    - `WalkTo` false under a pinning cast → `pinned` (no interrupt); refused otherwise → `locked`;
      issued → `move <yd>` (or `tight`), true.
12. **Stomp walk** (`GormokLeaveStompRangeAction`): add young bombs at `FIRE_BOMB_CLEARANCE` to its
    circles; with no spot, fall back to Gormok's circle alone.

### Group C — reader, test, doc

Files: `tools/botobs/bosses/northrend_beasts.py`, `tools/botobs/tests/test_northrend_beasts.py`,
`docs/raids/trial-of-the-crusader/northrend-beasts.md`.

13. **Reader `--breath`**: one row per `cast` with `sp` in `ARCTIC_BREATH = (66689, 67650, 67651, 67652)`
    from an Icehowl guid: time, target (`tgt`, name and role); victims = `aura` applies of those ids on
    roster members within 1000 ms; predicted = living roster members whose bearing from Icehowl in the
    snapshot nearest the cast lies within the half-width of the target's (30° when `hdr.diff` is 0, else
    12°), target included; each victim's latest `nb.spread` at the cast; `nb.arc` at the cast. Summary:
    breaths, mean and max victims, times frozen per bot.
14. **Reader `--bomb`**: one row per `haz` with `sp` 66317 and shape `circle`: time, spot, target = the
    roster member nearest it in the snapshot nearest its time; hits = 66317 `dmg` rows in
    `[t, t + ttl + 1500]`, each given to the bomb nearest the victim (snapshot nearest the hit) among
    bombs whose window holds it; inside = living roster within `FIRE_BOMB_IMPACT_RADIUS` in the
    snapshot nearest `t + ttl`; `nb.bomb` first words per bot inside `[t, t + ttl]`. Summary: bombs,
    hits, mean hits per bomb. Add both to `SECTIONS` and the docstring.
15. **Test**: a second synthetic pull in its own class (leave `beasts_pull` alone): two breaths, one
    freezing its target only and one catching a co-bearing neighbour, with `nb.spread`/`nb.arc` notes;
    one bomb with one bot dodging (`nb.bomb` `move` then `hold`), one bot hit by 66317 and inside at the
    impact. A separate `EveryView` test that both sections read empty on `fixtures/full-v12.ndjson`.
16. **Doc** `northrend-beasts.md`, after groups A and B land, folded with `compact-docs-writer`: "What
    the bots do" bullets for the spread (layout, deal, bands, healer yield, guard) and the bomb dodge
    (model, clearances, who stays, fallback chain, stomp walk), the decisions below with their
    rationale, Known gaps (drop "Arctic Breath has no spread and Fire Bomb's 8 yd impact no dodge", add
    the lane's), and "What a trace answers" rows for `spread`, `arc`, `bomb`, the 66317 circle, and
    `--breath` / `--bomb` usage. Leave the Jormungars section and its gaps to `w1b-jormungars`.

### Integrate

Syntax-check every changed `.cpp` plus the two helper headers, run pblint over `src/Ai/Raid/ToC` (with
`--spell-difficulty`), and the Python suite. Check `radius("FIRE_BOMB_IMPACT_RADIUS")` resolves and
that the doc's constants match the code.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Breath spread by bearing, not distance: only the angle off the target's bearing decides a hit | Spell.cpp cone check (`HasInArc` off his facing, set to the target at `_cast`), 100 yd reach | radius rings |
| Bearing step = half the cone + 6° (18°, 36° on 10N), not the full width | the bisector always runs through the target, so a neighbour is hit only within the half-width; the 6° covers a 3° stand tolerance each side | full width, halving the bearings |
| Cone width read from `spell_cone` for the remapped id, 24° default | SpellMgr / `SelectImplicitConeTargets` | hard-code 60/24 |
| Bearings outward from directly behind him (opposite his victim), half circle's worth (11, or 5 on 10N); walled bearings dropped and replaced further round, never within a step plus the 30° re-latch turn of his front (±126°, ±108° on 10N) | guide (half circle behind, healers far apart); arena wall; he faces his victim, who can turn 30° before a re-latch (review mechanics-1) | a step short of his front (a breath on a drifted tank catches the far bearings) |
| Bots share bearings (inner melee, outer ranged), about two per bearing on 25-man | 20+ non-tanks against 11 bearings; victims per breath is a step function of spacing (pitfalls) | even spacing below the half-width, three per breath |
| Healers first, one per bearing, nearest 60° off his back; the rest to the least-loaded bearing nearest the back; sticky per `k` | guide (healers apart); heal reach: a healer at 60°/17 yd is ~27 yd from the tank, one straight behind ~30 | healers straight behind |
| Radius bands melee 8-13, healer 16.5-18.5, ranged 21.5-25, clamped from the bot's own distance; `static_assert` each floor minus the 1 yd trigger past Whirl 15 / hunter minimum 19.8, melee top plus it inside 14.8 | melee range 14.8; Whirl 15; hunter minimum 5 + 14.8; heal reach ~31 to the tank and flank ranged; raid-mechanics-lessons (clamp, don't chase; park tolerance in the clearance) | 16-18 / 20-25, which a bot 1.25 yd short of its floor never left (review conformance-3) |
| Stand judged in his frame: walk once 3° off the bearing or 1 yd outside the band, arrive at 1.5° and 0.5; in-flight walk kept on the same test | the 6° margin holds only while two neighbours' bearing errors sum under 6°; a 2D 1.25 yd tolerance is up to 9° for melee (review mechanics-2) | 2D distance with a 21° step |
| Stand top = min(band top, bearing room less his drift along it since the probe − 1); probe 34 yd (25 + 8 re-latch drift + 1) so an open bearing never reads short; re-latch when drift cuts a kept bearing's room under 22.5; no stand if the top still falls under the floor | a stand laid past the probe's clip point walks into the wall and re-issues every 500 ms (review conformance-2); re-latching keeps every kept bearing fitting all three bands | report no stand; force the floor |
| Melee seated only on bearings within 57° of his back (90 − 30° re-latch turn − 3° stand error): \|k\| ≤ 3 at 18°, ≤ 1 at 36°; sticky slots re-checked; none kept → no slot, `set behind` places it | Icehowl has neither `NO_PARRY` bit (flags_extra 0x80001), `Unit.cpp` parries from his front half and parry-hastes; guide wants melee behind (reviews mechanics-3, conformance-5). Stricter than the reviews' 72° on 18° steps, which his 30° turn before a re-latch still takes past 90° | \|k\|·s ≤ 90° − s or − 6°; fall back to every bearing |
| A healer's spread yields while its heal target is out of heal reach of its stand (`PartyMemberToHealOutOfSpellRangeTrigger`'s measure and LOS, from the stand) | `PartyMemberToHeal` picks up to healDistance 38.5 and `reach party member to heal` walks past heal range + 1, so a stand that fights it keeps the healer from healing at all; measured from the bot, the reach walk's arrival ends the yield and the spread walks it back (review conformance-1) | measured from the bot |
| Layout only while Icehowl is the only engaged beast, not passive, no charge latched, his victim in his melee range | brief (stand down under the charge guard); heroic worms overlap left to the worm rules; a victim out of reach means he is walking (Whirl, after a charge) | spread through the overlap |
| Re-latch on an 8 yd move or a 30° victim turn, `k` kept | Whirl throws the tank ~31 yd every 15-20 s and he follows; sticky steps keep the raid's layout | re-deal on every latch |
| Humans and tanks hold no bearing | a human can't be moved; tanks hold him | slot humans as fixed occupants |
| Spread walk `MOVEMENT_COMBAT`, `ACTION_RAID + 2`, no interrupt | 15-30k over 5 s is healable; the first breath comes 5-8 s after a rage, 20-23 s after a daze | `MOVEMENT_FORCED` |
| `icehowl breath spread guard` zeroes `CombatFormationMoveAction` for slot holders | `set behind` (relevance 37) moves melee in his front half to ±108° off his facing, which is off a wrapped slot, and his facing turns to each breath target for 5 s | restrict melee to the back half |
| Fire Bomb impact at `1 s + distance / 14 yd/s + 0.5 s` after the spawn, age from `TempSummon::GetTimer` | script (NPC `TEMPSUMMON_TIMED_DESPAWN` 60 s, 66313 1 s cast), DBC speed 14, `SpellInfoCorrections` 66318 speed | a fixed window |
| Doc corrected: the aura lands a second before the impact, not after | 66318 is a triggered 14 yd/s missile cast with the bomb | — |
| Trigger 9, clearance 11, tight 9.5, sweep 20; landed bombs cleared by 6, `static_assert` past 2 + 1 + 1.5 | 66317 is a creature's 8 yd area, centre to centre (pitfalls); a dodge clears past its own trigger, and past `avoid aoe`'s 4.5 on a landed bomb, whose flee can step back into a young bomb's trigger (review conformance-4) | 8/10; pulse 4 |
| Everyone dodges except a bot some engaged beast is hitting | moving Gormok drags the melee and Stomp around; a 5-6k hit on a tank is healable | tanks too |
| Dodge `MOVEMENT_FORCED`, `ACTION_EMERGENCY + 2`, no interrupt | must beat a COMBAT walk in flight (stomp, carrier, reach, `avoid aoe`) inside 1.7-3 s; non-lethal, so a pinned caster waits (w1a's Gormok walks); the charge guard already zeroes it under a latched charge | COMBAT; interrupt |
| Dodge spot also clears old bombs' pulse and, for casters, Stomp range, each dropped in turn | don't step into the next hazard; the stomp walk would pull the caster back through the bomb | young bombs only |
| Stomp walk avoids young bombs, and re-plans a walk in flight once a young bomb lands within 11 of a spot picked clear of them (a stomp-only fallback spot is held as before) | stops stomp and dodge trading a caster until the impact; latch and re-validate (review conformance-6) | leave it; re-plan a fallback spot every tick |
| Bomb scan every 200 ms while Gormok is engaged or a bomb is young | a grid sweep per instance; ≥ 1.7 s reaction budget | every ms |
| One haz circle per bomb with the modelled impact as `ttl` | nothing sweeps the impact | no probe |
| No coordinates proposed | the arena floor is GO 195527: a navprobe ring at 40-60 yd round `ARENA_CENTER` reads only the stands (mesh Z 417-453) and settled Z 349-395; every stand here is boss-relative and clipped live | navprobe-validated stands |
| Every bearing walled: spread off (`nb.arc` 0, no stands) but the latch kept; probes again on the 8 yd / 30° re-latch or when the on-conditions drop and return | implementer (A): dropping the latch re-probes up to 19 raycasts every ms until he moves | drop the latch, as when the conditions go off |
| Roster rebuild and deal run only while the spread is on; a bot that dies while it is off keeps its slot until the first rebuild after it turns on | implementer (A): nothing reads a slot while off | rebuild while off |
| A `spell_cone` row with width ≤ 0 falls back to 24° | implementer (A), conservative | trust the row |
| Probe Z is his latched Z; stand Z his current Z (`MoveTo` clamps it) | implementer (A) | probe at his current Z |
| Healer yield and roster role both test `PlayerbotAI::IsHeal(bot)` | implementer (A): one role test for both | a separate test for the yield |
| Spread walk derives from the Gormok walks' latch, hoisted into `NorthrendBeastsWalkAction` (`Action/ToCActions_NorthrendBeasts.h`, header-only, no new file): a re-issue before the new spline shows in `isMoving` gets Duplicate (false); a stand drifting past the trigger mid walk is refused by the movement gate until the old booking's travel time runs out, then corrected | review conformance-7 (one latch, two readers) | a second copy in the Icehowl stem |
| Assignments cleared and roster marked stale whenever Icehowl isn't engaged (wipe, before he's up, after his kill) | implementer (A), brief (wipe) | clear on a wipe only |
| Bomb dodge checks `pinned` before the sweep, not after `WalkTo` fails; a pinned bot with no clear spot notes `pinned`, not `none` | implementer (B): `WalkTo` refuses under a pinning cast anyway; skips up to 160 collision probes a tick | after `WalkTo` |
| Re-aiming its own walk, the dodge lowers its `MOVEMENT_FORCED` booking to `MOVEMENT_COMBAT`, restored if nothing is issued | implementer (B): a booking only yields to a higher priority, so the re-aim would come back `locked`; the Jaraxxus and Anub'arak walks do the same | leave the re-aim `locked` |
| Haz row written once per bomb, at the first read with `RaidObs::Active()` while it is young, `ttl` = modelled impact age minus its age then; a bomb already past its impact is never written | implementer (B): equals the contract's `ttl` when a trace runs at first sight | write at first sight only |
| `move <yd>` is the 2D travel to the spot, rounded to the yard | implementer (B), matches the charge dodge's `move` | 3D path length |
| `nb.bomb` `clear` written by `IsInFireBombImpact` for a living bot outside every young bomb's trigger or under a beast, so a repeated branch on the next bomb shows | review observability-1: `--bomb` counts rows per bomb window | leave the change-only gap |

## Carried over

- Merge stage, `docs/raids/trial-of-the-crusader/README.md`: spell table row Arctic Breath
  66689 / 67650 / 67651 / 67652 (cone row on 10N only); node table row `icehowl breath spread` keys on
  the remapped Arctic Breath id's cone width.
- Merge stage, engine lessons:
  - `raid-mechanics-lessons.md` "Reading a boss script": a spell's `Speed` delays even a triggered
    instant, so an aura sent with a telegraphed cast can land first (Fire Bomb's 66318 a second before
    66317); read `Speed` and `SpellInfoCorrections` before timing a hazard off its aura.
  - `raid-mechanics-lessons.md` "Range and reach": `PartyMemberToHeal` picks up to healDistance 38.5
    and `reach party member to heal` walks past heal range + 1, so a healer stand that doesn't keep
    every heal target in range has to yield to it, or the healer stops healing.
- Merge stage, `w1b-jormungars` runs alongside and edits `northrend_beasts.py` (`SECTIONS`, docstring),
  its test and `northrend-beasts.md` (Known gaps, trace table): keep both sides. If it adds a
  `MOVEMENT_FORCED` worm dodge, the Fire Bomb dodge meets it in a heroic Gormok + worms overlap:
  settle precedence in a multiplier.
- Seam-file edit outside the owned stems: `Action/ToCActions_NorthrendBeasts.h` now holds
  `NorthrendBeastsWalkAction` (was `GormokWalkAction` in the Gormok stem), the base of the three
  Gormok walks and the spread walk. Header-only, so no CMake re-run.

## Known gaps

- No spread against Fire Bomb in Gormok's stage (guide: stay spread); the impact dodge is the only
  answer.
- No raid cooldown on Arctic Breath (Divine Sacrifice, Aura Mastery with Frost Resistance Aura).
- Humans and tanks hold no bearing, so a breath on one can catch the bots on that bearing, and with
  about two to a bearing on 25-man a breath still freezes two.
- No spread in a heroic worms + Icehowl overlap.
- Melee with every bearing within 57° of his back walled hold none, and `set behind` stacks them
  behind him, so a breath on one freezes them all.
- A caster pinned by its own cast waits it out and can eat the impact.
- The bomb dodge ignores worm hazards in a heroic Gormok + worms overlap.
- With no wall tanking, Whirl moves him every 15-20 s and the whole spread walks after him.
- Unverified live: if every bearing probe fails against the GO floor, `nb.arc` stays 0 and the
  spread never runs.
- The stomp and carrier walks (`MOVEMENT_COMBAT`) can't re-aim while their own walk is in flight
  until the booking runs out or the stall clear fires; a dodge spot `MoveTo` refuses (no path) while
  an earlier dodge walk is in flight notes `locked` until that booking expires.

Open issues after re-review: none. Deferred findings: none.

Integrate: syntax check (9 TUs plus `ToCStrategy.cpp`, `BuildSharedActionContexts.cpp`,
`BuildSharedTriggerContexts.cpp`), pblint `--spell-difficulty` over the lane scope (0 errors; the two
`SPELL_BURNING_*` warnings are w1b's), Python suite (394 tests) green; `radius("FIRE_BOMB_IMPACT_RADIUS")`
= 8.0; doc constants match the code.

Review fixes: every bug and minor applied (mechanics-1/2/3, conformance-1 to 5, observability-1),
and the cleanups conformance-6/7 and conventions-1/2; decisions above carry the review ids. Same
checks green after them, Python suite at 395 tests.

## Blocked
