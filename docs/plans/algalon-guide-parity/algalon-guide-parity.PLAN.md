# Algalon the Observer — guide-parity analysis and rework

## Context

The Algalon strategy was written in one pass (`410838672 Add Algalon strategy for Ulduar`) and never revisited against
the encounter script or a strategy guide. It is the shallowest Ulduar boss in the tree: 7 triggers, 7 actions, 1
multiplier, no encounter helper, no per-instance state, no raid formation, no phase model.

Checked against `boss_algalon_the_observer.cpp`, the DBC reference tables and the Warcraft Tavern / Icy Veins /
Warcraft Wiki guides, it has **six defects that make the fight fail outright** (one resets the boss deterministically)
and **seven unimplemented mechanics**. Scope: **full guide parity**, Vezax-level depth. Every design decision below was
settled with the user; the rationale is recorded inline so a fresh session does not relitigate them.

---

## Verified ground truth

Read from source, SQL or DBC. Do not re-derive.

### Server script — `src/server/scripts/Northrend/Ulduar/Ulduar/boss_algalon_the_observer.cpp`

| Fact | Value |
|---|---|
| Room | 47 yd disc around home `(1632.668, -302.7656, 417.3211)`, floor `z >= 410`; boss evades outside (`IsInRoom`, :629) |
| Entry | `GO_DOODAD_UL_ULDUAR_TRAPDOOR_03` (194253) — a trapdoor, no door on a wall. Planetarium console 194628 sits at `(1646.18, -174.69, 427.25)`, ~128 yd on the **+Y** side. Algalon lands facing **+Y**, toward the arriving raid |
| Intro delay | 26 s first pull, 8.5 s afterwards; **all combat timers are offset by it** (:503-533) |
| Quantum Strike | 3.5 s + intro, repeat 3–4.5 s. ~27 k main-hand / ~15 k off-hand on 25-man |
| Phase Punch | 15.5 s + intro, repeat **15.5 s**, on current victim |
| Collapsing Star | 16.5 s + intro, repeat **60 s**, tops up to 4 alive at `CollapsingStarPos` |
| Cosmic Smash | 26 s + intro, repeat **25.5 s**, `RAID_MODE(1, 3)` targets |
| Constellation activate | 60 s + intro, repeat **50 s**, **3 at a time** via `CallConstellations()` |
| Big Bang | 90 s + intro, repeat **90.5 s** |
| Ascend (berserk) | 360 s + intro → 3 Big Bangs fit in a first-pull kill window |
| Phase 2 | **20 % HP**; despawns constellations/stars/black holes, spawns 4× Worm Hole (34099) at `CollapsingStarPos` |
| Fight ends | 2 % HP, not death |
| Big Bang zero-target evade | `spell_algalon_big_bang::CheckTargets` → `ACTION_ASCEND` → `EnterEvadeMode` ~4 s later (:1300-1330) |
| Constellation activation | `REACT_AGGRESSIVE`, clears `UNIT_FLAG_NOT_SELECTABLE`, picks a player within 250 yd, `AddThreat(target, 100.0f)`, melee-chases that victim (:1015-1027) |
| Arcane Barrage | `SPELLVALUE_MAX_TARGETS = 1`, every 5 s, only while the constellation has a victim |
| Constellation death | Black Hole casts 65508 → 65509 on it; **both** despawn (:1039-1047) |
| Collapsing Star | `MoveRandom(25.0f)`; `SPELL_COLLAPSE` drains **1 % max HP per second**; on lethal damage summons the Black Hole (:964-987) |
| Black Hole on spawn | casts 62185 (phase field), 65508 (constellation eater), **64122 Black Hole Explosion**, void-zone visual (:555-560) |
| Cosmic Smash | 33104 = ground marker; 33105 teleports +35 z and fires the impact **exactly 4 s later** (:564-580) |
| Unleashed Dark Matter | **no AI, no spells** — `SelectTarget(Random, 0, 100.0f, playerOnly)` off Algalon's threat list, then `MoveChase`. Pure melee chaser (:581-585) |
| Worm Hole | spawns Dark Matter first at 6–8 s, then every **30 s**, *per hole* → 4 adds per 30 s in P2 (:1078-1104) |

### Geometry

`CollapsingStarPos` — the Collapsing Star spawn points in P1 and the Worm Hole spots in P2:
`(1649.438, -319.813)`, `(1647.005, -288.679)`, `(1622.451, -321.156)`, `(1615.060, -291.682)`, all z ≈ 417.4.
A **~32 yd square** centred on `(1633.5, -305.4)`; corners ~23 yd from room centre.

> **Corrected during implementation.** An earlier draft of this document said P1 Black Holes stand on
> `CollapsingStarPos`. They do not. `boss_algalon_the_observer.cpp:968` gives the star
> `MoveRandom(25.0f)` and `:984` has it cast `SPELL_SUMMON_BLACK_HOLE` **on itself** when Collapse
> finishes it, so a P1 hole lands wherever that star happened to be — anywhere within ~25 yd of a
> spawn point. Only the P2 Worm Holes (`:609`) use the fixed square. Consequences: the formation
> cannot be designed clear of P1 holes, so slots displace at runtime instead; and the ">6 yd from all
> four hole centres" constraint below binds in P2 only, though the chosen slots satisfy it anyway.

### DBC — `modules/mod-spell-tweaks/data/dbc-reference/`

| Spell | Value |
|---|---|
| Phase Punch **64412** | duration **45 000 ms**, max **5** stacks, refreshed on each 15.5 s application |
| Phase Punch 5th stack | applies alpha 64417 → `spell_linked_spell` → **62169**, tank phased for 10 s |
| Big Bang **64443 / 64584** | cast **8 000 ms**, radius 50 000 (whole map — position is irrelevant), damage **76 312** / **107 249** |
| Black Hole **62168** / Worm Hole **65250** | radius **6.0 yd**, aura 10 s, refreshed while inside; no target cap — one hole fits the raid |
| Black Hole Damage **62169** | **1 531** per tick — the single "phased / safe" aura, applied by 62168, 65250 **and** 64417 |
| Constellation Phase Effect **65509** | radius **6.0 yd**, `MaxAffectedTargets = 1` — **one hole eats exactly one constellation** |
| Black Hole Explosion **64122 / 65108** | **16 087** (25-man higher), radius **200 yd**, unavoidable, on star death |
| Cosmic Smash damage **62311 / 64596** | base **41 437**; `< 6 yd` full, `6–10 yd` = `dmg/dist*2`, `>= 10 yd` = `dmg/dist` (script :1278-1298) |
| Summon Black Hole 62189 | no despawn timer — holes persist until eaten or the 20 % transition |
| Dispersion **47585** | cooldown **120 000 ms** (75 s with Glyph of Dispersion) |
| Guardian Spirit **47788** | cooldown **180 000 ms** |
| Cauterize | **does not exist in WotLK** — the only DBC entries named Cauterize (43930, 60211) are not mage abilities |

Big Bang repeats every 90.5 s, so **no single soaker covers consecutive casts unglyphed**. This forces a rotation.

### Creature template — `data/sql/base/db_world/creature_template.sql`

| Entry | `HealthModifier` | `mechanic_immune_mask` | `flags_extra` |
|---|---|---|---|
| Living Constellation 33052 | **20** | −348 | 0 → **tauntable** |
| Collapsing Star 32955 | 7 | −361 | 0 |
| Unleashed Dark Matter 34097 | 3.5, `speed_run 1.42857`, rank 4 | −361 | 0 → **tauntable** |

A level-81 elite at 20× base health is not a kill target. The kite is the only removal mechanism.

### Effective config (`docker exec ac-worldserver env`; no `AC_*` override exists)

`AiPlayerbot.FleeDistance = 5.0`, `AiPlayerbot.SightDistance = 100.0`, `BotCheats = "food,taxi,raid"`.

### Positioning

From the guide's positioning screenshot, cross-checked against the coordinates: the four glows are the
`CollapsingStarPos` square in perspective. Boss and tank sit at the far edge of the square, melee behind the boss,
ranged and healers in a compact cluster on the near flank, **inside the square**. The raid does not spread across the
room — everyone stays a short sprint from a hole. The guide prose ("ranged spread as much as possible") obscures this.

---

## Findings

### Sev-1 — currently broken

1. **`UldMultipliers.cpp:37` looks up `"algalon observer"`; every other site uses `"algalon the observer"`.**
   `"find target"` is an exact full-name match, so `AlgalonMultiplier` never resolves the boss and never reserves
   Dispersion. The soaker spends it on the `low mana` / `critical health` nodes, has it down at Big Bang, falls
   through to hiding — every bot is then phased, `CheckTargets` sees zero targets, boss evades. Recorded in
   `docs/engine/pitfalls.md:23-29`; still live.

2. **All 7 triggers gate on `AI_VALUE2(Unit*, "find target", "algalon the observer")`.** That value walks only
   `bot->GetThreatMgr().GetThreatenedByMeList()`. Healers and anyone off Algalon's threat list never resolve him, so
   **they never hide from Big Bang** and eat 76 k / 107 k. Also inert through the 26 s intro, when Algalon is neutral
   and `UNIT_FLAG_NOT_SELECTABLE`.

3. **`ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET = 5.0f` is smaller than the hole's 6.0 yd radius.** The kiter parks
   *inside* the field it is meant to stand clear of, phases itself, takes 1 531/s and stops leading the constellation.
   The comment at `UldBossHelper.h:736` states the opposite of what the number does.

4. **`FleePosition(asteroid->GetPosition(), 12.0f)` is clamped to `FleeDistance = 5.0`.** A bot on the marker ends up
   ~5–6 yd out, inside the `dmg/dist*2` band, taking ~10 k instead of the ~3.7 k an 11 yd move costs.

5. **The kiter has no threat on the constellation.** It chases the player it `AddThreat`-ed at activation.
   `AlgalonConstellationKiteAction` never attacks or taunts, and the kiter is `IsMechanicTrackerBot` — the first alive
   bot on the map, almost never the victim. It walks to a hole and nothing follows. Three constellations activate
   every 50 s and only one bot ever kites.

6. **Phase Punch swap threshold of 3 leaves a 1.5 s margin.** 45 s aura refreshed every 15.5 s: the off-tank reaches
   3 stacks 46.5 s after taking the boss, while the previous tank's aura only expires 45 s after *its* third stack.
   One missed tick and the own-stacks guard blocks the taunt-back, the active tank rides to 5 and phases out with
   nobody on the boss. **Threshold 4 gives ~17 s of margin** and matches the guides.

### Sev-2 — missing mechanics

7. **No Collapsing Star kill serialisation, and ignoring them is fatal.** 64122 is 16–21 k unavoidable raid damage per
   star. At 1 % max HP per second a star self-destructs after ~100 s, and the 60 s event only tops up to 4 alive — so
   an ignoring raid takes **all four explosions within seconds of each other** at ~143 s. Deliberate, staggered kills
   are mandatory. Nothing currently suppresses AoE or paces the kills.

8. **Nothing guarantees a Black Hole exists at Big Bang.** Holes exist only if a star was killed, and each
   constellation eats one — 3 constellations per 50 s against 4 stars per 60 s consumes them faster than they appear.
   No hole = raid dead; all bots hidden = boss evades.

9. **Soaker is a single point of failure and cannot cover the cadence.** No Shadow Priest → guaranteed reset. Worse,
   Dispersion's 120 s cooldown exceeds the 90.5 s Big Bang interval, so even with a priest present one bot cannot soak
   consecutive casts unglyphed.

10. **Nothing stops bots attacking Living Constellations.** At `HealthModifier = 20` that is pure waste against a
    6-minute hard enrage.

11. **Phase 2 add handling is one skull mark.** Four Worm Holes each spawn a Dark Matter every 30 s — elite, 3.5×
    health, `speed_run 1.42857` (faster than players, so kiting is impossible), chasing a random player.

12. **No phase model.** P1-only nodes stay armed in P2; nothing distinguishes the intro from the fight.

13. **No raid formation.** The largest gap against the guide, the screenshot and every other reworked Ulduar boss.

> **Retracted, do not re-audit:** an earlier draft listed "post-Big-Bang threat recovery" as a finding. It is void.
> `CombatManager.cpp:53` gates only *entering* combat on `InSamePhase`, and `ThreatManager` never purges entries on
> phase change — threat survives the hide, the boss simply cannot select a phased target and re-picks the MT on
> unphase.

### Sev-3 — conventions and cost

14. **No `AlgalonEncounterActive` gate** — 7 triggers run for all 25 bots for the entire raid.
15. **No throttling.** Every trigger uses `checkInterval = 1` while doing `FindNearestCreature` grid sweeps at 100 and
    200 yd. Vezax throttles its sweeping triggers to 2 s.
16. **200 yd search radii** exceed `SightDistance = 100`; the room is a 47 yd disc.
17. **No state across ticks** — the kite destination can flip mid-walk.
18. **No movement-suppression multiplier**, unlike Vezax and Freya.
19. **`IsAssistTankOfIndex(bot, 0)` without `ignoreDeadPlayers = true`** (`pitfalls.md:225-230`); should use
    `GetGroupMainTank` / `GetGroupAssistTank`.
20. **Cosmic Smash dodge at `ACTION_EMERGENCY` (90) with default movement priority** despite a hard 4 s window;
    `docs/raids/README.md:28-38` specifies `ACTION_EMERGENCY + 6` with `MOVEMENT_FORCED`.
21. **`SPELL_ALGALON_COSMIC_SMASH = 62301` is dead** — the damage spell is 62311 / 64596, the impact trigger 62304.
    Heroic ids (64584, 64592, 64596, 64598, 64607, 65108) are absent from the helper enum.

---

## Settled design decisions

| Decision | Choice | Why |
|---|---|---|
| Boss anchor | Held ~19 yd off room centre, on the **−Y** edge of the hole square | Raid arrives from +Y; boss ends up facing away from the raid, matching the screenshot |
| How he gets there | **No drag action** — the MT simply has a formation slot; the boss follows through normal chase | No new action class needed |
| Formation coverage | Ranged, healers **and the MT slot**; melee and off-tank use normal combat positioning | The MT slot *is* the anchor mechanism. Melee cannot be meaningfully spread inside a hitbox |
| Formation shape | Two rings at ~14 and ~22 yd, ~10 yd spacing, healers on the inner ring | 10 yd puts every neighbour in Cosmic Smash's cheap `dmg/dist` band; healers need ≤ 30 yd (`GetRange("heal")`) |
| Slot assignment | Vezax-style: latched per instance in a guid-keyed map, filled centre-out, released on death; **presence-gated** | Re-deriving renumbers everyone behind a corpse; the 26 s intro is the free window to position 25 bots |
| Big Bang clock | **Latch on the first observed Big Bang cast**, predict every later one at +90.5 s | Sidesteps the 26 s / 8.5 s introDelay branch and self-corrects. Big Bang #1 lands ~116 s in, well after holes exist naturally |
| Hole husbandry | Inside a 30 s pre-Big-Bang window: forbid the kite from consuming the last hole, and force a star kill if zero holes exist | Holes have no target cap, so exactly one suffices |
| Forcing that kill | Raise the star-focus priority **and** veto attacks on Algalon until a hole exists | A mark alone is advisory. This is the one moment losing boss DPS is unambiguously correct |
| Star pacing | Focus the **lowest-HP** star; require raid **minimum** HP ≥ 80 % and ≥ 8 s since the last explosion; override when any star drops below ~15 % | HP % *is* the remaining-lifetime clock, so lowest-first staggers deaths for free; the override stops natural deaths clustering |
| AoE suppression | Veto `DpsAoeAction` only, while ≥ 2 stars are alive | Narrow `dynamic_cast` per `action-selection.md:108-123`. P2 has no stars, so Dark Matter cleave is unaffected |
| Constellation targeting | **Veto attacking them outright** | `HealthModifier = 20` — killing is not a real option. Pets need no handling; `PetAttackAction` is globally disabled |
| Constellation kiter | Whoever it is already chasing; **off-tank taunts it off the Algalon MT** as the sole exception | Three constellations against one off-tank does not fit; the victim already has aggro free |
| Kiter's other actions | Kite action returns **`false`** once the constellation is following and the destination is latched | Lets instants and heals share the tick; cast-time spells were unavailable while moving anyway |
| Big Bang soaker | **Rotation**: lowest-guid bot whose soak cooldown is actually ready — Shadow Priest Dispersion, then Holy Priest Guardian Spirit, then a body who stays out and probably dies | Glyph-agnostic; mirrors `VezaxReadyInterrupt`. Target count is taken at select time, so a corpse still prevents the evade — better than a reset |
| Soaker latch | Chosen at cast start, latched for that cast; re-derived only if the holder dies or gets phased | Trigger and action must agree via one helper; a per-tick flip strands the exempted bot |
| Phase Punch threshold | **4** | Arithmetic above |
| Leave-hole node | Built, gated on **proximity to a hole creature within 6 yd**, not on aura 62169 | Excludes the Phase-Punch-phased tank for free and fires while walking in, before the aura lands |
| Dodge vs formation | Formation yields while a marker is within ~12 yd of the slot, **plus** Vezax's `_slotReached` latch with a `tolerance * 2.0f` re-arm | Widening tolerance alone would destroy the 10 yd spacing |
| P2 Dark Matter | Off-tank collects them **on the boss** for cleave (reuse the `FreyaTankAddsAction` shape); fall back to assist tank 1 where a third tank exists | Collecting on the boss means the off-tank never leaves its swap position, so the duties do not conflict |
| Single-tank raids | **Unsupported**; leave the swap trigger inert and record it | A DPS taunting Algalon dies in two swings; a fake second tank would hide the failure, not fix it |
| Burst window | **Unchanged** — Bloodlust on the pull, as `docs/raids/ulduar.md:1289` already says | P1 is 80 % of the health bar and where every mechanic steals DPS time; P2 is 18 % and short |
| Difficulty | Fully agnostic — no difficulty check anywhere | Centre-out slot fill handles smaller raids for free; 76 312 kills 10-man raiders too, so the soaker set does not widen |

---

## Implementation

### 1. New encounter helper — `src/Ai/Raid/Uld/Util/UldEncounter_Algalon.{h,cpp}`

Mirror `UldEncounter_Vezax.{h,cpp}`. NPC entries, spell ids and tunables stay in `UldBossHelper.h`; logic and
per-instance state live here.

- `Unit* GetAlgalon(PlayerbotAI*)` — `GetFirstAliveUnitByEntry(botAI, PB_NPC_ALGALON)`, **never** `"find target"`.
  Replaces the name lookup in all 7 triggers, `AlgalonPhasePunchSwapAction` and `UldMultipliers.cpp:37`, killing
  findings #1 and #2 together.
- `bool AlgalonEncounterActive(PlayerbotAI*)`.
- `bool AlgalonInPhaseTwo(PlayerbotAI*)` — Worm Hole (34099) present; it only spawns at 20 %.
- `bool AlgalonBigBangCasting(PlayerbotAI*)` — boss casting 64443 **or** 64584.
- `bool AlgalonBigBangWithin(PlayerbotAI*, uint32 seconds)` — off the latched clock.
- `Creature* GetAlgalonNearestShelter(Player*)` — nearest 32953 or 34099, capped at 60 yd.
- `uint8 AlgalonShelterCount(PlayerbotAI*)` — drives husbandry and the target guard.
- `Player* GetAlgalonBigBangSoaker(PlayerbotAI*)` — the rotation, latched per Big Bang cast.
- `Unit* GetAlgalonKiteTarget(Player*)` — the active constellation whose `GetVictim() == bot`, excluding the Algalon
  MT (the off-tank taunts that one instead).
- `Creature* GetAlgalonFocusStar(PlayerbotAI*)` — lowest-HP Collapsing Star.
- `bool AlgalonStarKillWindowOpen(PlayerbotAI*)` — the pacing gate and its under-15 % override.
- `AlgalonEncounterState` + `extern std::unordered_map<uint32 /*instanceId*/, AlgalonEncounterState>` defined in
  exactly one `.cpp` (`pitfalls.md:211-223`): latched formation slots, latched kite/shelter guids, latched soaker guid,
  first-Big-Bang timestamp, last-explosion timestamp. Plus `ResetAlgalonEncounterState`, wired like Vezax's at
  `ACTION_EMERGENCY + 10`.

### 2. Formation — as built

Anchor latched at the pull, never tracking the boss (`raid-mechanics-lessons.md:55-91`). The MT slot is the anchor;
rings hang off it, not off the room centre, because the tank is who healers must stay in range of.

`ULDUAR_ALGALON_TANK_SLOT = (1632.7, -321.5, 417.321)` — 18.73 yd from Algalon's home position, 10.25 yd from the
nearest worm hole spot, 16.8 yd from the next.

Three rings in the **+Y** half-plane, bearings measured from the tank slot. The arcs are trimmed asymmetrically
because the two −Y worm hole spots sit level with the tank slot: hole `(1649.438, -319.813)` is 16.8 yd out at
bearing 5.8°, hole `(1622.451, -321.156)` is 10.25 yd out at 178.1°. Untrimmed half circles would put the 14 and
20.5 yd rings inside their 6 yd fields.

| Ring | Radius | Arc | Slots | Spacing | Min clearance to any hole spot |
|---|---|---|---|---|---|
| Healers | 14.0 | 35°–145° (centre 90°, width 1.9199) | 4 | 8.96 | 7.78 |
| Ranged inner | 20.5 | 28°–178° (centre 103°, width 2.6180) | 6 | 10.7 | 8.05 |
| Ranged outer | 27.0 | 0°–180° (centre 90°, width 3.1416) | 8 | 12.1 | 8.75 |

18 slots, filled centre-out, latched per instance, healer ring preferred for healers with overflow into the ranged
rings so a raid with more healers than inner slots still places everyone. Reach-then-hold with hysteresis,
`return false` once parked, copying `VezaxRaidPositionAction` (`UldActions_Vezax.cpp:214-224`).

Spacing is 8.9–12.1 yd rather than a flat 10: Cosmic Smash's bands are `<6` full / `6–10` `dmg/dist*2` /
`>=10` `dmg/dist`, so every neighbour of a marked bot is at worst in the doubling band, and the marked bot moves
12 yd clear anyway.

**navprobe, map 603, all 19 points (tank slot + 18 ring slots):** every one on mesh, `distance to poly 0.040`,
`UpdateAllowedPositionZ 417.321`, mesh surface Z 417.360. The floor is a flat WMO — `terrain (.map)` reads
−438.107 there, which is why raw terrain height is meaningless in this room. Slot furthest from the room centre is
32.9 yd, well inside the 47 yd disc.

Probe command used (one `point` call per slot rather than `ring`, since the arcs are not evenly spaced round a full
circle):

```
MSYS_NO_PATHCONV=1 docker run --rm \
  -v azerothcore-wotlk-pb_ac-client-data:/azerothcore/env/dist/data:ro \
  --entrypoint /azerothcore/env/dist/bin/navprobe acore/ac-wotlk-build:master \
  --map 603 point <x> <y> 417.32
```

### 3. Triggers and actions

| Node | Change |
|---|---|
| `algalon big bang hide` | entry-based gate; shelter search capped at 60 yd; latch the shelter guid; skip when already phased |
| `algalon big bang soak` | driven by the latched rotation soaker |
| `algalon cosmic smash` | `FindNearestPositionClearOfHazards(bot, {marker}, 12.0f, 40.0f)`, `MOVEMENT_FORCED`; 4 s budget |
| `algalon leave black hole` | **new** — step out when within 6 yd of a hole creature and Big Bang is not casting |
| `algalon phase punch swap` | threshold **4**; `GetGroupMainTank` / `GetGroupAssistTank(…, 0)`; handle the partner already phased at 5 |
| `algalon constellation kite` | fires for the constellation's own victim; drags it through a hole; parks **> 6 yd** past it (`ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET = 9.0f`); latches the hole guid; returns `false` once it is following |
| `algalon constellation taunt` | **new** — off-tank pulls a constellation off the Algalon MT |
| `algalon collapsing star focus` | mark the lowest-HP star, gated by `AlgalonStarKillWindowOpen` |
| `algalon dark matter mark` | P2 only; skull the add the off-tank holds |
| `algalon dark matter tank` | **new** — off-tank collects every Dark Matter on the boss; no kiting, they outrun players |
| `algalon raid position` | **new**, per §2; yields while a Cosmic Smash marker is near the slot |
| `algalon reset encounter state` | **new**, Vezax pattern |

Throttle the grid-sweeping triggers to **2 s** via `Trigger(ai, name, interval)`; leave Big Bang and Cosmic Smash
reactions at 1. Never pass 2–99 by accident elsewhere — that means seconds.

### 4. Multipliers — `UldMultipliers.{h,cpp}`

- Fix `AlgalonMultiplier` to use `GetAlgalon`; rename `AlgalonDispersionReserveMultiplier`
  (`action-selection.md:73-81` naming). It reserves **Dispersion and Guardian Spirit**, and only for the bot currently
  designated soaker — other priests behave normally.
- **New** `AlgalonCollapsingStarAoeMultiplier` — veto `DpsAoeAction` while ≥ 2 stars are alive.
- **New** `AlgalonTargetGuardMultiplier` — veto attacks on Living Constellations always; veto attacks on Algalon while
  zero holes exist inside the 30 s pre-Big-Bang window. Copy the `XT002TargetGuardMultiplier` shape.
- **New** `AlgalonControlMovementMultiplier` — Vezax's shape (`UldMultipliers.cpp:553-576`): split on action family
  first, allowlist the Algalon movers, keep `ReachTargetAction` alive so healers can close range.
- `UldThreatRedirectMultiplier` already vetoes Misdirection/Tricks on Algalon — leave it.

### 5. Priority ladder — `UldStrategy.cpp:647-677`

| Node | Priority |
|---|---|
| `algalon reset encounter state` | `ACTION_EMERGENCY + 10` |
| `algalon big bang hide` / `algalon big bang soak` | `ACTION_EMERGENCY + 8` |
| `algalon cosmic smash` | `ACTION_EMERGENCY + 6` |
| `algalon leave black hole` | `ACTION_EMERGENCY + 4` |
| `algalon phase punch swap` | `ACTION_RAID + 7` |
| `algalon constellation taunt` | `ACTION_RAID + 6` |
| `algalon dark matter tank` | `ACTION_RAID + 5` |
| `algalon constellation kite` | `ACTION_RAID + 4` |
| `algalon collapsing star focus` | `ACTION_RAID + 3` |
| `algalon dark matter mark` | `ACTION_RAID + 2` |
| `algalon raid position` | `ACTION_RAID` |

### 6. Constants — `UldBossHelper.h:216-228, 733-743`

Add `NPC_ALGALON_VOID_ZONE_VISUAL_STALKER = 34100`; heroic ids 64584, 64592, 64596, 64598, 64607, 65108; 62311 (real
Cosmic Smash damage) and 62304; drop the unused 62301. Add `ULDUAR_ALGALON_ROOM_CENTER`,
`ULDUAR_ALGALON_ROOM_RADIUS = 47.0f`, `ULDUAR_ALGALON_SHELTER_RADIUS = 6.0f`,
`ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET = 9.0f`, `ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS = 4`,
`ULDUAR_ALGALON_BIG_BANG_INTERVAL_MS = 90500`, the star-pacing thresholds and the formation block.

### 7. Registration

New trigger/action names into `UldTriggerContext.h` and `UldActionContext.h` — names **and** factories; a mismatch is
silent (`pitfalls.md:11-29`).

### 8. Docs at ship time

Rewrite `docs/raids/ulduar.md:596-633` with the verified numbers, including the single-tank limitation and the
"threat survives the hide, do not re-audit" record; drop the stale Sev-1 line at `:1362`; remove the Algalon half of
the live-bug note in `docs/engine/pitfalls.md:23-29`. Then delete this plan directory (`docs/README.md:22-39`).

---

## Verification

1. `python apps/codestyle/codestyle-cpp.py` — **done**. The six Algalon files are clean. The run still fails overall on
   pre-existing double-blank-lines and tabs in `Aq40`, `OS`, `SWP`, `UldMultipliers.h:270`, `UldActions_XT002.h:136`
   and three spots in `UldBossHelper.h`; all of those reproduce on `HEAD` and none are in touched regions.
2. **Name sweep** — **done**. Every Algalon `TriggerNode` / `NextAction` string in `UldStrategy.cpp` resolves to a
   `creators[...]` key, and no Algalon registration is orphaned. The sweep also turned up two **pre-existing** silent
   wiring bugs elsewhere in Ulduar, both since **fixed** and recorded in `docs/engine/pitfalls.md`:
   `UldTriggerContext.h` registered `"yogg-saron shadow resistance trigge**rr**"`, and
   `"thorim fall from floor action"` had a class and a trigger but no `creators[...]` entry at all. Enabling the
   Thorim node is safe — its threshold is Z 410, below the gauntlet floor at 412.13 and the arena at 419.8, so it
   only fires on a bot that actually fell through the world.

   Three more of the same shape live in **Zul'Aman** and are **not** fixed: `ZAStrategy.cpp` asks for
   `"<boss> boss engaged by main tank"` / `"<boss> main tank position boss"` for Akil'zon, Jan'alai and Zul'jin,
   while `ZATriggerContext.h` / `ZAActionContext.h` register those three as `"... boss engaged by tanks"` /
   `"... tanks position boss"`. Halazzi and Nalorakk are correctly wired. Left alone deliberately: fixing it turns on
   three dead tank-positioning nodes in a raid this work never looked at.
3. **navprobe** — **done**, results in §2. All 19 points on mesh, settled Z 417.321.
4. **Build is a hand-off** — the module cannot be compiled headless in this environment, and `AGENTS.md:7` says not to
   build unless asked. **Outstanding.**
5. **In-game, 10N**, stock `BotCheats`, checking in order: raid reaches formation during the intro; stars die one at a
   time lowest-first with raid HP recovering between each and no AoE splashing them; the first Big Bang has a hole,
   everyone phases except the rotation soaker, **and the boss does not evade**; the second Big Bang picks a *different*
   soaker; a constellation is dragged into a hole and both despawn; no bot attacks a constellation; the swap happens
   at 4 stacks and swaps back cleanly; Cosmic Smash markers are vacated inside 4 s **without** bots oscillating back
   onto them; at 20 % the raid switches to worm holes and the off-tank collects Dark Matter on the boss.
   **Outstanding.**
6. Wipe and re-pull: instance state map is empty, nobody walks to a stale slot, and the Big Bang clock re-latches on
   the first observed cast rather than carrying over. **Outstanding.**

---

## Build status

Code complete, **not compiled and not play-tested**. §1–§8 are implemented as described, with the deviations below.

### Files

New: `src/Ai/Raid/Uld/Util/UldEncounter_Algalon.{h,cpp}`.
Rewritten: `Trigger/UldTriggers_Algalon.{h,cpp}`, `Action/UldActions_Algalon.{h,cpp}`.
Edited: `Util/UldBossHelper.{h,cpp}`, `UldMultipliers.{h,cpp}`, `UldStrategy.cpp`, `UldTriggerContext.h`,
`UldActionContext.h`, `docs/raids/ulduar.md`, `docs/engine/pitfalls.md`.

The module has no `CMakeLists.txt` — the core globs module sources — so the two new files need no build registration.

### Deviations from the plan as written

| Plan said | Built | Why |
|---|---|---|
| Rings at ~14 and ~22 yd, two rings | Three rings at 14 / 20.5 / 27, asymmetric arcs | Two rings hold 9–14 slots; a 25-man needs ~16. 22 yd also sits on top of the +Y hole spots |
| Every slot > 6 yd from all four hole centres, by design | True for the P2 square; P1 holes displace at runtime | P1 holes land where the star died, not on `CollapsingStarPos` — see the correction in *Geometry* |
| `AlgalonInPhaseTwo` helper | Not written | Nothing needed it: Dark Matter's presence *is* the phase read, and stars/constellations despawn at 20 % anyway |
| Add `NPC_ALGALON_VOID_ZONE_VISUAL_STALKER`, heroic ids 64592/64596/64598/64607/65108 | Only `SPELL_ALGALON_BIG_BANG_25 = 64584` added | `spelldifficulty_dbc.sql` maps only 64443 and 64122; nothing in the strategy reads the others, and unused constants are noise |
| Separate `.FINDINGS.md` | Folded into this file | `docs/README.md:22-39` — durable content goes to `docs/raids/ulduar.md` at ship time; two files would duplicate every fact |
| Actions mirror their trigger in `isUseful()` | Dropped | The old code did this and paid for every grid sweep twice. These actions are only reachable through their own trigger node, which is the Vezax convention |

### Not yet done

`docs/raids/ulduar.md` has been rewritten and the stale Sev-1 row dropped, but **this plan directory stays** until the
build and the in-game pass above are green — the work is still in flight in the sense `docs/README.md:22-39` means.
Delete it then.
