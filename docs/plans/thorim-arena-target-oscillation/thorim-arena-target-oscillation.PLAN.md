# Thorim arena: stop the target tug-of-war, leash the pets, and make a trace able to show both

## Context

The latest Thorim attempt is inside `env/dist/logs/botobs/603_1_ulduar_1788203223.ndjson` — a
whole-instance session (`boss: "ulduar"`, schema v7), not a per-boss file. Thorim was pulled at
1:54, the raid wiped at 5:24. Read it with
`python modules/mod-playerbots/tools/botobs/postmortem.py <file> [--death N|--bot NAME|--notes thorim]`.

The user reported two things. One is a real and large bug; the other is not the bots.

### Claim 1 — arena melee do almost no damage: CONFIRMED, root cause found

**The arena squad changes target about twice a second, all fight.** Measured from the `target`
column of `snap.u` at 250 ms sampling, after the split:

| bot | squad | target switches/min | in melee range of its own target | casts/min |
|---|---|---|---|---|
| Ecoterrorist (melee) | arena | 121 | 30% | 31 |
| Assasin (melee) | arena | 125 | 38% | 79 |
| Angry (melee) | arena | 122 | 40% | 105 |
| Obliteration (melee) | arena | 108 | 46% | 306 |
| Justice (melee) | gauntlet | **2.0** | 59% | 204 |
| Totemist (melee) | gauntlet | **2.6** | 41% | 234 |
| Mighty (melee) | gauntlet | **2.9** | 60% | 172 |

Same code, same tick rate, sixty times the churn on one side of the room.

Read the casts/min column with care and do not lean on it: that trace predates the upstream `tr`
proc flag, so those counts include passive procs. Obliteration's 306/min is 284 Blood Presence procs
and change. The switch rate and the melee-range column are the evidence; cast rate is a hint, and
the outgoing-damage records in Fix 3a are what will actually settle throughput next time.

**Mechanism.** `ThorimDpsPriorityTrigger::IsActive()`
(`src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.cpp:37`) ends with
`return target && target != currentTarget;`. The moment the bot already holds the pick, the trigger
goes false and the node is never queued. `Engine::DoNextAction`
(`src/Bot/Engine/Engine.cpp:195-295`) then walks on down the relevance queue and runs the next thing
that returns true — `dps assist` at relevance 50, from `DpsAssistStrategy`
(`src/Ai/Base/Strategy/DpsAssistStrategy.cpp:13`) — which retargets to the generic smart pick. Next
tick the raid pick differs again, `thorim dps priority action` (`ACTION_RAID` = 60) wins and
retargets back. Two-tick oscillation, 5 engine ticks a second.

The `act` verdicts show both actions reporting OK continuously, alternating: across the arena squad
`dps assist` OK 185, `thorim dps priority action` OK 155. In the gauntlet squad the two agree on
what to hit, so there is nothing to fight over — 92 and 24, and the 2/min switch rate above.

**Consequence.** The arena never clears a wave. Adds alive within 50 yd of the arena centre:

```
2:00   2      3:00  10      4:00  18      4:40  27      5:00  36      5:10  39
```

96 distinct adds spawned after the split. (RaidObs caps its creature sweep at `OBS_MAX_WATCHED` =
40, so the real pile past 4:40 is a floor, not a ceiling.) Everyone in the arena died between 3:42
and 5:24 under a pile that was never going to stop growing.

**Second-order problem, same picker.** `GetThorimDpsTarget`
(`src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp:632`) ranks the arena tiers Acolyte → Evoker →
Champion → Warbringer → Commoner and picks, within the top non-empty tier, whoever is closest to
**the room centre** (`SelectThorimTierTarget`, `:586`, 8 yd hysteresis). Every bot therefore picks
the same unit, and for melee that unit is usually a caster standing off: the picker chose an Evoker
55-59% of the time and a Champion ~20%, and *never once* a Warbringer or a Commoner. Median distance
from bot to its own pick was 9-12 yd for melee, and 43-65% of picks were beyond 10 yd. Melee spend
the fight running.

That ranking is also backwards against what actually kills the squad. Damage taken by the arena
squad after the split, 1 516 014 total:

| source | share | of which |
|---|---|---|
| Dark Rune Champion | **41.2%** | Whirlwind 15578 25.8%, melee 8.5%, Mortal Strike 35054 5.9% |
| Dark Rune Warbringer | 21.2% | melee 14.0%, Runic Strike 62322 7.2% |
| Dark Rune Evoker | 16.5% | Runic Lightning 62445 |
| Lightning Orb (the wipe) | 6.4% | |
| Dark Rune Commoner | 3.6% | |

Champions are the top of the list, are melee, and are already standing on the bots. The picker sends
melee past them at an Evoker.

Worth knowing but not the main lever: Dark Rune Commoners stack **Low Blow 62326**, whose second
effect is `SPELL_AURA_MOD_DAMAGE_PERCENT_DONE` at −3% a stack (verified against
`modules/mod-spell-tweaks/data/dbc-reference/spell.reference.csv` and
`SpellAuraDefines.h:142`). Peaks in this pull: Dragon x25 (−75% damage done), Fel x21, Tree x11.
Only late, and only on a few bots, so it is a reason not to ignore Commoners forever rather than an
explanation for the whole fight.

### Claim 2 — pets running into the gauntlet: the trace cannot see it, and that is the finding

The user watched a wolf and a warlock pet run through the gauntlet and into the geometry, dragging
adds with them. **Nothing in a RaidObs trace can confirm or refute that**, and it is worth being
precise about why, because the first read of this data looked like an answer and was not one:

- Pets are absent from `snap.u` entirely — `BuildSnapshotPayload` walks the roster, the vehicles
  they ride and the hostile creature sweep, and a pet is none of those. **There is no pet position
  data at any point in the file.**
- `unit` records carry no owner, so ownership can only be reasoned out from spell lists.
- The only pet evidence in the file is `cast` records, and a pet running somewhere is not casting.
  Across the four arena-owned pets after the split there are 545 casts with **no target at all**
  (Flaaghun 210, Worm 204, Wolf 96, Ruirin 35) — self-buffs and procs that fire wherever the pet
  happens to be standing. Those windows are exactly when a pet would be off running, and they are
  blind.
- Snapshots are 250 ms apart while the engine ticks at ~200 ms, so a one-tick target flicker onto a
  corridor mob — which is all it takes to send a pet 140 yd — falls between samples.

What the file does say, for whatever it is worth: every *targeted* cast from the four arena pets
after the split was at something within 15 yd of the arena centre, and no arena bot was ever sampled
holding a target more than 40 yd from the centre. That is consistent with the pets behaving, and
equally consistent with them wandering off during the 545 untargeted casts. It is not evidence
either way.

One thing is settled: the ghoul that leapt onto the Runic Colossus (3:19.739) and onto the Iron
Honor Guards (4:07.635, the first action anything took against that pack) is Gravebreaker, and
Gravebreaker is the human's. Seven pets appear, identified by their spell lists:

| pet | what it is | owner |
|---|---|---|
| Gravebreaker | DK ghoul (Claw 47468, Leap 47482) | **Deathsong, human** |
| Khiigrom (two guids) | felhunter (Shadow Bite 54053, Improved Felhunter 54425) | Agony, gauntlet squad |
| Worm, Wolf | hunter pets (Bite, Growl, Furious Howl) | Trueshot, Nightwarrior, arena |
| Ruirin | imp (Firebolt 47964) | arena warlock |
| Flaaghun | felguard (Demonic Frenzy, Cleave, Intercept) | arena warlock |

Deathsong is flagged `h:1` in the roster, casts Ghoul Frenzy 63560 six times across the pull, and
was 37 yd and 25 yd ahead of the nearest bot at those two moments. Obliteration, the bot death
knight, is Frost, cast no ghoul ability at all and never left the arena. So the ghoul that opened
those two packs was a human running ahead of his group — but **the wolf and the warlock pet are
Wolf/Worm (Trueshot, Nightwarrior) and Ruirin/Flaaghun (Fel, Hellflame), and all four of those
owners are arena squad bots.** Any wolf or imp or felguard seen in the corridor is a bot's, because
the only pet the gauntlet squad owns is Agony's felhunter.

The module never commands pets at all: the pet-attack trigger is commented out in
`src/Ai/Base/Strategy/CombatStrategy.cpp:52-55`, so pets acquire targets from their own stance and
nothing in this codebase ever calls them back. A pet whose owner is flipping target twice a second
(see claim 1) is being re-pointed by `PetAI` just as often, and a pet that ends up chasing something
across the room has nothing pulling it home. That is a plausible route from claim 1 to claim 2, and
one more reason to fix the churn — but it is a hypothesis, not a measurement.

So this plan does two things about claim 2: makes it visible (Fix 3b), and stops the consequence
regardless of cause (Fix 4).

---

## Fix 1 — stop the generic picker fighting the encounter

**Files:** `src/Ai/Raid/Uld/UldMultipliers.{h,cpp}`, `src/Ai/Raid/Uld/UldStrategy.cpp`,
`src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.cpp`,
`src/Ai/Raid/Uld/Util/UldEncounter_Thorim.{h,cpp}`

This is a solved problem in this codebase four times over — `IgnisDisableDefaultTargetingMultiplier`,
`KologarnDisableAutomaticTargetingMultiplier`, `IronAssemblyDisableAutomaticTargetingMultiplier`,
`FreyaDisableAutomaticTargetingMultiplier`. Thorim picks targets in code and has no such multiplier.
Add one, shaped like `IronAssemblyDisableAutomaticTargetingMultiplier`
(`UldMultipliers.cpp:279`).

`ThorimDisableAutomaticTargetingMultiplier::GetValue`:

- `BOT_STATE_COMBAT` only, and return 1.0 for anything that is not one of the generic pickers.
  Reuse the existing file-local `ThorimIsTargetSelectionAction` (`UldMultipliers.cpp:327`) —
  it already names the exact set — but **exclude `TankAssistAction` from the scope**: Thorim has no
  tank-targeting node to replace it with, and a tank chasing the ranged pick is worse than a tank on
  whatever is hitting the raid. So the scope is `DpsAssistAction`, `DpsAoeAction`,
  `AggressiveTargetAction`, `AttackAnythingAction`, `AttackLeastHpTargetAction`.
- Return 1.0 for healers and tanks, matching who the raid picker actually steers.
- Otherwise return 0.0 **only while the raid picker has an answer for this bot**. Zeroing
  unconditionally is a freeze: `GetThorimDpsTarget` returns null whenever no tier has an in-box
  candidate, and with every generic picker dead the bot would then have no target source at all.

For that last check, read the answer the picker already cached rather than recomputing it — the
multiplier chain runs once per queued action per tick. `NoteThorimDpsTarget`
(`UldEncounter_Thorim.cpp:621`) already writes `state.dpsTargets[bot]` on every call including the
null one, and `ThorimDpsPriorityTrigger` refreshes it every tick for every bot within 110 yd of the
arena centre. Add a small accessor beside it:

```cpp
// The last target the encounter picked for this bot, or false when it had nothing to offer. Read by
// the targeting guard, which runs per action per tick and must not re-walk the candidate list.
bool ThorimHasDpsTarget(PlayerbotAI* botAI, Player* bot);
```

Gate it on `NearThorimEncounter(bot)` and a living boss so a stale entry from an earlier pull cannot
strand a bot with no picker at all.

Then register it in `RaidUlduarStrategy::InitMultipliers` (`UldStrategy.cpp:847-851`) next to the
other four Thorim multipliers.

Also **exempt tanks in `ThorimDpsPriorityTrigger::IsActive()`**, the same way healers already are at
`UldTriggers_Thorim.cpp:48`. Without it a bot tank would keep oscillating against `tank assist`,
which the multiplier deliberately leaves alone. There was no bot tank in the arena this pull, so
this is prevention, not a repair.

Leave `ThorimDpsPriorityAction::Execute`'s `return false` when the pick is already held
(`UldActions_Thorim.cpp:77`) exactly as it is. With nothing left to steal the target, falling through
is what lets the class rotation run.

## Fix 2 — melee take what is standing on them

**Files:** `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp`, `src/Ai/Raid/Uld/Util/UldBossHelper.h`

Only the arena branch of `GetThorimDpsTarget` (`:663-679`) changes. Three edits:

1. **Tier order by role.** Acolytes stay first for everyone — four of them all pull, and they heal
   the wave back up. After that, melee read `champions, warbringers, evokers, commoners` and ranged
   keep today's `evokers, champions, warbringers, commoners`. Use `PlayerbotAI::IsMelee(bot)`.

2. **Melee pivot on themselves.** Pass `*bot` to `SelectThorimTierTarget` for a melee bot instead of
   `ULDUAR_THORIM_NEAR_ARENA_CENTER`, which is exactly what the gauntlet branch already does at
   `:654`. Ranged keep the centre pivot, so ranged focus fire is unchanged.

3. **Melee do not cross the room.** For a melee bot, skip a tier whose nearest in-box candidate is
   further than a new constant:

   ```cpp
   // Arena adds land 19-24 yd from the centre and a Champion is on somebody within a second or two of
   // that, so anything past this is a target that will have moved twice before the bot arrives. Melee
   // skip a tier that has nothing this close and take the next one down; the walk is only worth it
   // when no tier has anything at all.
   constexpr float ULDUAR_THORIM_MELEE_TARGET_REACH = 15.0f;
   ```

   If no tier has a candidate in reach, fall through to one more pass with the reach test off, so a
   melee bot with nothing nearby still commits to the raid focus rather than standing idle. Keep the
   existing `ThorimInArenaBox` filter and the 8 yd `ULDUAR_THORIM_TARGET_SWITCH_MARGIN` hysteresis on
   every path — the hysteresis is what stops a moving pile re-picking every tick.

Ranged behaviour is deliberately untouched beyond the pivot staying put: Evokers are 16.5% of damage
taken, nothing else can reach them cheaply, and this pull's ranged were not the problem.

## Fix 3 — telemetry: deferred, because another plan is already doing most of it

**Do not touch `src/Bot/Obs/` in this round.** `docs/plans/raidobs-schema-v8/` is in flight with
uncommitted changes across `RaidObs.cpp`, `RaidObs.h`, `RaidObsScripts.cpp` and `postmortem.py`, and
it lands on the same functions this would.

**3a, outgoing damage, is superseded.** That plan's T2 already records damage dealt, by a cheaper
design than the one drafted here: a cumulative `uint64 damageDealt` on `BotTrace`, appended to each
roster row of `snap.u` as element 11, rather than a per-hit `odmg` record. Pet and totem damage is
folded into the owner's counter through the same owner resolution `NoteCast` uses. Cumulative
differences cleanly over any window and survives a dropped snapshot, and it costs ~0.25 MB a pull
against the ~10-15k extra records per-hit rows would have added. Take theirs. The only thing it does
not give is per-spell attribution, which nothing here needs.

That plan also independently found the proc problem behind the casts/min caveat above, and its T3
`tr` flag is the fix.

**3b, pet positions, is still missing and still wanted.** Their T1 reorders `SweepArea` to take the
nearest hostile creatures rather than the first 40 the grid happens to return; pets are friendly and
are not in that path, so they stay invisible. Ownership also stays a deduction from spell lists.

Do it as an additive v9 *after* their v8 lands, so it rebases instead of colliding:

- `BuildSnapshotPayload`: in the roster loop, after the player's own row, emit a `UnitRow` for each
  of `player->m_Controlled` that `IsPet() || IsGuardian()` and is not `IsTotem()`, alive, in world
  and on the same map. Follow the vehicle block above it, `ridden` set included, so nothing is
  emitted twice. Note that `UnitRow` will by then take the element-11 damage argument; a pet passes
  the default, since its damage is already counted against its owner.
- `unit` records gain `own`, the owner's guid key, wherever `EnsureUnit` writes a unit that has one.
  That is what turns ownership into a fact.
- `AiPlayerbot.Obs.LogPets` (default 1), wired like `Obs.LogHeals`.
- `postmortem.py`: pets in `--track` and in the death-block distance list, owner in parentheses.

Until that lands, Fix 4 below is unverifiable from a trace — which is an argument for shipping it on
the reported sighting rather than waiting for proof.

## Fix 4 — keep the arena squad's pets in the arena

**This is a boundary and nothing else.** A pet does not dodge, and no part of this makes one dodge:
pets carry enough resistance and damage reduction to eat anything in this fight, and the module has
no business steering them around Charge Orb, Deafening Thunder or Sif's Blizzard. The only question
this node ever asks is "is the pet still in the room", and the only thing it ever does is send it
back. If a recalled pet walks through a hazard on the way home, that is fine and is not a bug to fix
later.


**Files:** `src/Ai/Raid/Uld/Action/UldActions_Thorim.{h,cpp}`,
`src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.{h,cpp}`,
`src/Ai/Raid/Uld/Util/UldEncounter_Thorim.{h,cpp}`, `src/Ai/Raid/Uld/UldStrategy.cpp`,
`src/Ai/Raid/Uld/Uld{Trigger,Action}Context.h`

No new constant: `ThorimInArenaBox` and `ULDUAR_THORIM_ARENA_LEASH_RADIUS` already draw the line.

Thorim already leashes bots — `ThorimArenaLeashMultiplier` and `ULDUAR_THORIM_ARENA_LEASH_RADIUS`
exist because a bot outside the arena box summons the Lightning Orb. Their pets are under no such
rule and nothing recalls them. Give the pets the same boundary their owners have.

New node `thorim pet leash trigger` / `thorim pet leash action`, at `ACTION_RAID` (it competes with
nothing; it moves the pet, not the bot).

- Enumerate `bot->m_Controlled`, keeping units where `IsPet() || IsGuardian()` and `!IsTotem()`.
  `bot->GetPet()` alone is not enough — it misses guardians, which is what a Frost death knight's
  Raise Dead, Feral Spirits and Army all are. Skip anything dead or on another map.
- **Arena squad owners only.** Out of bounds is `!ThorimInArenaBox(pet)` or further than
  `ULDUAR_THORIM_ARENA_LEASH_RADIUS` (30) from the arena centre — the same boundary the owner is
  held to. A pet chasing an add that landed 24 yd out is doing its job; this only catches the ones
  that have left the room. Gauntlet-squad pets are left alone: that squad is walking a corridor with
  no box to test against, a pet 30 yd up the lane there is often legitimately on the next pack, and
  nothing has been reported or measured going wrong with them.
- Recall with the idiom already proven in `IccLichKingWinterAction::HandlePetManagement`
  (`src/Ai/Raid/ICC/Action/ICCActions_LK.cpp:1714-1739`): `AttackStop()`, `CastStop()`,
  `MoveFollow(bot, PET_FOLLOW_DIST, pet->GetFollowAngle())`, then the `CharmInfo` flags —
  `SetCommandState(COMMAND_FOLLOW)`, `SetIsCommandAttack(false)`, `SetIsAtStay(false)`,
  `SetIsReturning(true)`, `SetIsCommandFollow(true)`, `SetIsFollowing(false)`, `RemoveStayPosition()`.
  Guard the whole block on `GetCharmInfo()` being non-null: a guardian may not have one, in which
  case the `AttackStop` and `MoveFollow` still stand on their own.
- Only issue the recall on the transition, not every tick, or the pet is re-commanded five times a
  second and never gets anywhere. Latch per pet guid in `ThorimEncounterState` and clear the latch
  once the pet is back inside, the same reach-then-hold shape the arena anchor uses.
- Emit `thorim.petrecall` (an `ObsGuidMap<ObjectGuid>` keyed by owner, holding the pet) so the next
  trace says this fired and how often. Clear it in `ResetThorimEncounterState` with the rest.

The `CombatStrategy.cpp` comment warns that a previous pet-attack trigger was removed for overriding
stay and follow commands. This is the opposite direction — it only ever recalls, never commands an
attack — and it is scoped to one encounter, so it cannot leak into normal play.

## Verification

Static, before any pull:

- `grep -rn "ThorimDisableAutomaticTargetingMultiplier" src/Ai/Raid/Uld/` — three hits: header,
  definition, and the `InitMultipliers` registration. A multiplier that is never pushed is silently
  inert.
- `grep -rn "thorim pet leash" src/Ai/Raid/Uld/` — the trigger and the action each appear in their
  own header, their own cpp, the matching context, and `UldStrategy.cpp`. An unregistered name is
  skipped in silence.
- `git status --short -- src/Bot/Obs tools/botobs` — this round must leave all four of those files
  untouched. `docs/plans/raidobs-schema-v8/` owns them.
- `python apps/codestyle/codestyle-cpp.py` from `modules/mod-playerbots`, clean.
- The module cannot be compiled headless here. Hand the branch off for a build rather than claiming
  one.

In-game, one 25-man pull, then re-read the fresh trace:

1. **Churn collapses.** Arena melee target switches drop from 108-125/min to the gauntlet's order of
   magnitude (single digits). This is the primary number; everything else follows from it.
2. **`dps assist` stops winning ticks in the arena.** `act` verdicts show `thorim dps priority
   action` OK and no `dps assist` OK for arena DPS, with
   `thorim disable automatic targeting killed dps assist` vetoes in the `veto` stream.
3. **Melee connect.** "in melee range of its own target" rises from 30-46% toward the gauntlet's
   ~60%. Read cast rate only if the v8 `tr` flag has landed by then, and filter the procs out with
   it; without that filter the number measures Blood Presence.
4. **Melee pick what is on them.** The `thorim.dpstarget` notes for melee bots name Champions and
   Warbringers, and the median bot-to-pick distance drops from 9-12 yd to under 8. Ranged picks are
   unchanged.
5. **The wave gets cleared.** Adds alive stops climbing monotonically — today 2 → 18 → 39. Anything
   that plateaus and comes back down is the fix working. Note that v8's T1 reorders the creature
   sweep nearest-first, so this count gets *more* accurate and may read higher for the same pile;
   compare shape, not absolute numbers, across the schema boundary.
6. **Nobody freezes.** No arena bot sits with no target for more than a second or two; if one does,
   the multiplier is zeroing a picker while `ThorimHasDpsTarget` says false, which is the one failure
   mode this design has.
7. **The pet leash fired, or had nothing to do.** `thorim.petrecall` notes are the only evidence
   available until pet snapshot rows land, so read them directly: a burst of them says pets were
   leaving and are now being pulled back; none at all says either the problem did not recur or the
   bounds are too loose to catch it. Watch the fight to tell those apart — the trace cannot.

If pets turn out to be clean, keep the leash anyway — it costs nothing when it never fires, and the
sighting is more reliable than a file that cannot see pets at all.

## Explicitly not in this plan

- **The bot-tank split.** `AssignThorimSquads` (`UldEncounter_Thorim.cpp:231-238`) counts tanks with
  a bare `PlayerbotAI::IsTank(member)` over the whole roster, so the human protection paladin Dragon
  counts, `tankCount` reads 2, and Bulwark — the raid's only *bot* tank — is sent down the corridor
  again, exactly as in the previous round. The arena is therefore tanked by a human, which is the
  real reason the human shows up high on the arena damage meter. The fix is to count and pick tanks
  bot-only (`IsBotPlayer(member) && PlayerbotAI::IsTank(member)`) and to skip tanks in the DPS loop,
  since `IsDps` is also true for a protection paladin. Excluded at the user's request, twice now.
- **Pets dodging anything.** Not now, not later. Pets survive this fight on their own resistances
  and damage reduction, and a pet AI that steps out of hazards is a cost with no payoff.
- **Leashing gauntlet-squad pets**, for the reasons in Fix 4. If the next trace shows a gauntlet pet
  opening a pack ahead of its group — which the new pet snapshot rows would make obvious — it is a
  small extension of the same node.
- Writing Thorim's arena mechanics into `docs/raids/ulduar.md`. Still owed from two rounds back:
  Charge Orb geometry and the seven orb spawns, Deafening Thunder, burst-cooldown suppression,
  `DpsAoeStrategy` dead wiring, and now the Low Blow damage-done stacking.
