# Raid observability (RaidObs)

Per-pull NDJSON traces of where every raid member stood, what its engine decided, and what killed it.
Answers what video cannot: position over time, and the action that produced it.

`mod-chronicle` already writes a full WotLK combat log per instance to `logs/chronicle_logs/`. RaidObs
does not replace it — it records what a combat log has no field for: **position and bot intent**. It
duplicates enough damage/heal/aura data that a pull file stands alone.

Code: [src/Bot/Obs/](../../src/Bot/Obs/). Reader: [tools/botobs/postmortem.py](../../tools/botobs/postmortem.py).

## Reading a trace

Traces land in `<LogsDir>/botobs/<map>_<instance>_<boss>_<epoch>.ndjson`, named from the creature that
engaged. `.playerbots debug obs` lists what is open.

```
postmortem.py <file>               summary + a block per death
postmortem.py <file> --death N     full rewind for one death
postmortem.py <file> --bot NAME    one bot's timeline
postmortem.py <file> --track NAME  position track + distance to each boss
postmortem.py <file> --notes [KEY] pull/note/hazard/end only; KEY narrows to one note-key prefix
```

NDJSON is one record per line with no enclosing array, so `grep '"e":"death"'` beats parsing 15 MB.

## Config

Knobs and their defaults are documented in `conf/playerbots.conf.dist` under RAID OBSERVABILITY; env
overrides are `AC_AI_PLAYERBOT_OBS_*`. Read effective values with
`docker exec ac-worldserver env | grep ^AC_` — never trust the `.conf`.

Three that decide file size: `SnapshotIntervalMs` (250 — two samples per oscillation cycle, the
minimum that distinguishes a bouncing bot from a stationary one), `DeathVerdictMs`, and
`MinDamageToLog`.

Only **raid** maps are tracked unless `Obs.Maps` names a map id — a tracked map is sampled for as
long as anyone stands on it, which no 5-man is worth.

## Lifecycle

A session opens on the first of: boss state → `IN_PROGRESS`, `RaidObs::MarkPull` from encounter code,
or a boss-flagged creature engaging a raid member. It closes on kill, wipe, reset, map destruction, or
`IdleCloseSeconds` out of combat.

**Boss state is observed, not requested.** `SetBossState` fires its hook *before* deciding: it drops
every change while a boss loads from the DB, and refuses `DONE` while a world-boss minion lives. The
hook only queues the boss id; the next map update reads the state that stuck. That delay also lets the
engage hook name the trace after the creature rather than the map — the core runs `SetBossState`
before `OnUnitEnterCombat`.

While a raid sits on a tracked map with no session, snapshots go to a `PreRollSeconds` ring that is
flushed into the file when a pull starts — so the trace opens *before* the engage, and pre-pull
records carry a **negative** `t`. That is what makes a bad squad latch visible.

`end.out` follows the roster at the close, not the instance script: more than half the raid dead files
a `reset` or `idle` as `wipe`. Hodir's script reports `NOT_STARTED` on release, which filed a 23-of-24
wipe as `reset`.

## Schema (`v: 7`)

`t` is milliseconds from the `hdr`. A guid is a type tag in the high 32 bits over
`ObjectGuid::GetCounter()` in the low 32 — the counter alone is a separate numbering space per type, so
a creature and a player both start at 1 and would collide. Players are tag `0`, so they read as the
bare counter; creatures are `1`, pets `2`, vehicles `3`, gameobjects `4`, dynamic objects `5`, anything
else `7`. `0` still means no unit.

| `e` | Fields |
|---|---|
| `hdr` | `v`, `ts` epoch ms, `map`, `inst`, `diff`, `boss`, `roster[]` of `{g,n,c,r,h}` — `r` role, `h` human |
| `pull` / `end` | `boss`,`src` / `out`: kill, wipe, reset, idle, mapgone, shutdown |
| `unit` | `g`,`en` entry,`n`,`lvl`,`mhp`,`b` is-boss, plus `c`,`r`,`h` for a player — once per guid |
| `spell` | `sp`,`n` — once per spell id |
| `snap` | `u[]` rows `[guid,x,y,z,o,hp%,mana%,target,moving,moveGen,castingSpell]`; `hz[]` swept dynamic objects `[spellId,x,y,z,radius,foe]` |
| `dmg` | `s`,`d`,`sp`,`a`,`ok` overkill,`sc` school,`ab`,`rs`,`hp` after |
| `heal` | `s`,`d`,`sp`,`a`,`oh` overheal,`hp` after |
| `abs` | `d`,`s` shield caster,`sp`,`a` |
| `aura` | `d`,`s` caster,`sp`,`r` 1=removed,`st` stacks,`dur` ms left,`p` 1=positive |
| `cast` | `s`,`sp`,`tgt`,`ct` cast time — cast **start**, the reaction window; roster, its pets and watched creatures only |
| `act` | `g`,`a`,`rel`,`vd`: OK, FAILED, IMPOSSIBLE, USELESS, PREREQ, UNKNOWN |
| `veto` | `g`,`m` multiplier,`a` action it zeroed |
| `move` | `g`,`k` generator,`x`,`y`,`z`,`tgt`,`ok`,`r` reason,`by` owning action,`pr` priority; on `wait` also `hpr`,`hms` — the walk that beat it |
| `note` | `g`,`k` kind,`txt` — assignments, latches, phases, derived state |
| `haz` | `sp`,`shape`,`x`,`y`,`z`,`ttl`, plus shape fields — hazards with no world object; timeline only, never tested against a death |
| `death` | `g`,`killer`,`x`,`y`,`z`,`dist{}`,`auras[]`,`rewind[]`,`blow[]`,`hplast[]`,`acts[]`,`lastmove{}` |
| `truncated` | file hit `MaxFileMB`; the `end` record is still written past it |

`death.auras` rows are `[sp,stacks,dur,caster,appliedT,removedT,positive]`, `removedT` −1 while held
and `appliedT` −1 when the recorder never saw the apply;
`death.rewind` rows are `[t,source,sp,amount]`; `death.blow` is `[source,amount]` and `death.hplast`
`[hp%,t]`; `death.acts` rows are `[firstT,lastT,action,rel,verdict,repeats]`, a veto's verdict reading
`VETO:<multiplier>`.

**Name every id in the file.** `unit` and `spell` are written once each, so later records carry a bare
number and the trace still reads standalone: `spell 63511` needs a DBC open beside it, `Frozen Blows
63511` does not. Call `EnsureUnit`/`EnsureSpell` from *every* field that emits an id, not just the
obvious one — `aura.s` and `cast.tgt` stayed unnamed for a release because only the caster was covered,
and a death block read `Flash Freeze from #1:1925`. A new *emitter* needs the sweep as much as a new
field: `NoteHazard` went unswept until Thorim became its first caller, then wrote 121 rows of bare
`62057`. A guid inside `note.txt` is the exception: that is free text the recorder cannot inspect, so
`postmortem.py` joins it on read.

**Emit per engine pass, not per verdict.** A pass walks several action nodes and reports a verdict for
each, so no single verdict is news on its own. `act` and `veto` buffer until `BeginTick` closes the
pass, and a pass whose ordered set matches the one before is not written at all — it becomes a
`repeats` count, replayed into `death.acts` with the interleaving intact. A bot losing the same tick
forty times *is* the oscillation signal; `repeats` is where it lives.

**Then latch each verdict per action.** The pass compare is far too coarse alone: relevance drifts
reorder the priority queue every tick, so barely 1 in 200 passes matches its predecessor. A verdict
surviving it is written only when that action's own verdict changed. Both grains re-emit after
`OBS_ACTION_REPEAT_MS` (10 s), so a bot stuck in one state leaves a trail, not a gap.

A verdict is written when its pass ends, so line order can trail `t` by a tick; sort by `t`
(`postmortem.py` does, on load).

**`death.auras` comes from a tracked set, never from the unit.** `Unit::Kill` calls
`RemoveAllAurasOnDeath` well before `OnUnitDeath` fires, so at hook time only passives and
death-persistent auras are still applied — v3 death records listed 57 talents and no boss debuff.
`NoteAura` maintains the set itself, *ahead of* the `LogAuras` gate so switching the aura stream off
cannot also empty every death record, and stamps `removedT` rather than erasing, because the strip runs
through that same hook. Anything dropped within 2 s of the death is still reported, flagged. The strip
stamps the death's own timestamp, so a `removedT` that close means held to death, not worn off.

**An unknown timestamp gets a sentinel, never arithmetic.** `NoteAura` leaves `appliedMs` 0 when the
first event it sees for a spell is a removal — anything buffed before the pull — and stamping 0 yields
`−startMs`, one constant across every bot. `postmortem.py` renders the sentinel `?`: a fabricated
`held 1740.7s` in a 155 s fight is worse than no answer.

**Helps or hurts comes from the spell, not the caster.** `p` is `SpellInfo::IsPositive`. The caster is
wrong both ways — totems and pets buff from a creature guid, and Biting Cold is applied to the player
*by the player* through the zone aura's trigger — so a caster-tag heuristic listed Windfury Totem as a
debuff and hid the six stacks that killed the bot.

**`death.rewind` is trimmed on read as well as on push, and ordered by time.** The ring sheds its front
only when something new arrives, so a bot untouched for a minute would report that minute-old damage as
what killed it — and what a rewind answers is what landed last, not hardest.

Every damage hook is a `Send*Log` hook, so a fall or a script kill reaches none of them and leaves the
rewind empty. `NoteKillingBlow` fills `death.blow` from `Unit::DealDamage`, which all of them pass
through, taking only the blow that lands with the bot's health or more — emitting the rest from there
would double every hit the log hooks already see. `death.hplast` carries the last sampled health and
when, so the record states the drop even when nothing caught the blow. `blow` names no spell and cannot:
`OnDamage` carries no `SpellInfo`, and the hooks that do are modifiers rather than the funnel, missing
the environmental and script damage `blow` exists for. The debuff list answers that instead.

Same instance is not the same pull: gating `cast` on the session alone picked up 253 casts from a mob
two rooms away and none at all from the raid.

`snap.hz` radius falls back to the spell's own when the dynamic object reports zero, and rows still
come out at zero: Hodir's icicle dynobjects are `SPELL_ICICLE_VISUAL_*`, force-cast on the target with
no radius in the DBC to fall back to. A zero-radius row is a marker — where something landed, not an
area to test a position against — so `postmortem.py` reads it by proximity and keeps containment for
the rest. `foe` separates raid AoE from boss AoE: two thirds of a Hodir trace's rows were the
raid's own Death and Decay. What is lethal there is a creature, so hazard units carrying no dynamic
object that never enter combat are swept into `snap.u` instead, capped at `OBS_MAX_WATCHED` (40) so a
trash-heavy pull cannot blow the row count up.

`haz` is the other channel and the two never meet — `snap.hz` is what a sweep found, `haz` what nothing
can sweep for. Only `snap.hz` feeds the death block's containment test, so a `haz` mechanic reaches the
timeline and never a `STOOD IN` line. A `lane` carries no geometry to test anyway: an origin, no heading
or width, so only `side` separates Thorim's two.

**Derive a side from a guid, never from a sampled unit.** `foe` comes from the dynamic object's caster
guid, which outlives the caster. Asking a live roster member `IsHostileTo` flipped the same puddle
mid-fight, twice over: the member sampled was whichever the map iterated first, and a caster that had
gone fell into the hostile branch.

`move` covers every generator a bot drives: `k` is `point` (`MoveTo`), `jump`, `follow` or `chase`.
Follow and chase steer at a unit rather than a position, so they carry it in `tgt` and record where it
stood at the time; they emit only when the bot starts tracking somebody new, since the coordinates
change every tick by design.

`ok: 0` is not automatically a refusal — read `r`. Only `blocked` (`IsMovingAllowed` said no) and
`nopath` are; `dup` and `wait` are the ordinary state while a bot walks to a destination its action
re-offers every tick, and `there` means it already stands on it. A non-issued destination is a
*candidate*, not a command — `MoveNear` sweeps eight angles a tick — so these are throttled to one a
second and never latched into `death.lastmove`, whose `arrived` would otherwise be measured against a
position no MotionMaster saw. `follow` and `chase` are left out of that latch for the same reason:
neither has a destination to arrive at.

**A `wait` is a contest, so record both sides.** `IsWaitingForLastMove` yields only to a *strictly*
higher priority, so an equal-priority command waits out the whole walk already in flight. `pr` is what
this command was issued at, `hpr`/`hms` what beat it and for how long. Without them a trace reports two
thirds of moves refused and cannot say what won: Hodir's shelter move lands 12.7% of the time purely
because it sits at `combat` alongside `reach melee`. `follow` and `chase` never reach the gate and carry
`pr` empty — worth seeing: they steer without outranking anything. Names, not ordinals:
`RaidObs::MovePriority` mirrors `MovementPriority` so `Bot/Obs` stays off `Ai`, and
`MovementActions.cpp` static_asserts the two in step.

Bump `SCHEMA_VERSION` in `RaidObs.h` and `SUPPORTED_SCHEMA` in `postmortem.py` on any field *or value*
change; an additive one keeps the old version in `READABLE_SCHEMAS` so traces already on disk still
read. A new sentinel in an existing column is not additive — a reader that does not know it computes a
wrong number.

## Adding a probe

Store assignment state in the traced containers and it records itself — instrumentation follows the
data, not the call sites, so a new boss is covered without further work:

```cpp
RaidObs::ObsGuidMap<uint8> meleeSlots{"thorim.slot"};      // per-bot assignment
RaidObs::ObsGuidSet ringArrived{"thorim.ringarrived"};     // membership latch
RaidObs::ObsValue<uint32> runicSmashSide{"thorim.smashside"};  // per-instance scalar
```

They emit only on real change. Non-const iteration and iterator-`erase` exist for prune loops and
deliberately emit nothing — dropping a departed member is not an assignment. `erase(guid)` does emit,
on both containers: clearing one named bot is a wipe reset or a boss that is gone, which is real news.
Keep scan timestamps and cached guid lookups in bare types; tracing bookkeeping is noise.

State **derived** fresh on every call rather than stored — Hodir derives every position that way on
purpose — has nothing for a container to watch. Probe inside the helper that derives it, never at the
call sites: trigger and action both route through the helper, so one probe covers both and two cannot
disagree about what was decided.

```cpp
RaidObs::NoteDerived(bot, "hodir.anchor", RaidObs::DescribeDerived(slot));  // emits only on a change
```

**Probe the rule, not the coordinate.** A derived destination is already a `move` record naming the
action that issued it, and one derived from the crowd or the bot's own position moves every tick, so
latching it emits every tick — `hodir.shuttle` records which branch produced the leg (`tank`, `solo`,
`crowd`, `none`). Where the value is the news, `DescribeDerived` rounds it to the yard;
`DescribeAssignment`'s full precision is for stored assignments, which do not drift.

**Do not probe what another stream already says.** An action that succeeds is an `OK` in the act stream
and one that fails a `FAILED` — a note per attempt re-adds what that stream latches. Hodir's tank swap
retries at tick rate whenever the 8 s taunt cooldown is not up, so it is unprobed; so is
`IsHodirTrappedAllyBreaker`, which is asked about every block in range and comes back true for several
in one pass, while the one the bot goes for is already in `hodir.dpstarget`.

Prefix every note key with the encounter that owns it — that is what `--notes KEY` filters on, and Iron
Assembly legitimately spans three bosses under one slug.

A pull no instance script reports — a gauntlet, trash, a mid-phase re-engage — needs `MarkPull(map,
source)`, where `source` names the file. Thorim's corridor uses it.

For a hazard with no world object — a cone, a rolling wave, a rotating sweep — nothing can sweep for
it, so the helper that derives the geometry must say so (Thorim's Runic Smash lanes do):

```cpp
RaidObs::NoteHazardCircle(map, spellId, pos, radius, ttlMs);
RaidObs::NoteHazard(map, spellId, origin, "sweep", "\"lead\":1.2,\"rate\":0.5", ttlMs);
```

It lands on the timeline only, so give the shape enough fields to reconstruct it by hand.

Anything else: `RaidObs::Note(bot, kind, text)`, which emits every call rather than on change. Reserve
it for something that happens once and is in no other stream; nothing uses it today.

Everything above this section is raid-agnostic and already covers every instance map — only the `note`
stream needs per-raid wiring, because only the encounter code knows what an assignment is. Converted:
Ulduar (Thorim, Vezax, Algalon, Iron Assembly, Hodir), Black Temple, Hyjal, SSC, Tempest Keep. Still
bare: ICC's `IccInstanceState` (`std::map`, needs an ordered container variant), SWP's instance-keyed
nested maps (the inner map must be default-constructible, which a kind-carrying container is not), and
Naxx's function-local statics. Timestamps, thresholds and caches are left bare on purpose.

## Constraints

**Map thread only.** Per-session state is unlocked, safe solely because every probe runs on the map
thread owning that instance — which the damage, aura, death and `OnMapUpdate` hooks all do. A probe
driven from the world thread (`WorldScript::OnUpdate`) breaks it.

Two callers are not, and each earns it: `Status()` reads only `path` — fixed before the session is
published — and the two atomics that exist for it; `Shutdown()` runs from `Main.cpp` after
`WorldUpdateLoop` returns and the map thread pool is gone. Any new field either reads must be atomic.

**Gate hot paths on `RaidObs::Active()`** — a single bool, false whenever no trace is open. Put the
gate *inside* the probe and pass the object, never the name: `Action::getName()` returns
`std::string` **by value**, so a name passed as an argument is built before any gate can skip it, on
every bot every tick. A thousand idle open-world bots pay that bill.

**Filter on the map before touching the registry.** The damage, heal and aura hooks fire for the whole
server, not just instances. `SessionFor` tests `IsDungeon()` — a DBC flag read, no lock — and
`FindSession` answers from a per-map-thread memo a generation counter clears on every open and close.
Neither open-world combat nor an idle instance touches the registry mutex.

**Publish the bot for the whole engine pass** with `RaidObs::BotContext`, as `Engine::DoNextAction`
and `ExecuteAction` do. An `ObsValue` latch carries no guid and resolves its trace through it, so
covering only action execution silently drops every latch a trigger or value flips.

**Open the pass with `RaidObs::BeginTick`** at the top of `DoNextAction`, beside `BotContext`. It closes
the previous pass; without it the verdict buffer flushes only on a death or at the trace's close.

**Publish the action around every `ListenAndExecute`** with `RaidObs::ActionScope`, which is what
`move.by` reads. It restores the previous action instead of clearing, because a nested
`DoSpecificAction` runs a second action inside the first — clearing would blame the rest of the outer
action's movement on nobody. Construct it only under `Active()`: it copies the name.

**This module no longer builds against upstream AzerothCore.** The `Send*Log` and
`OnAuraApplicationClientUpdate` hooks are local core additions (commit `208764946`), absent from
`mod-playerbots/azerothcore-wotlk`, so module CI cannot compile it. Deliberate.

Probes in `Engine.cpp` (`BeginTick` plus seven verdict sites) and in `MovementActions.cpp` (the
`MoveTo` wrapper plus `JumpTo`, `Follow` and `ChaseTo`) will conflict on upstream merges. They sit
beside the existing `LogAction` calls and the `MotionMaster` calls respectively, so re-applying is
mechanical. `MoveToImpl` returning `RaidObs::MoveOutcome` rather than `bool` is the one part that is
not.
