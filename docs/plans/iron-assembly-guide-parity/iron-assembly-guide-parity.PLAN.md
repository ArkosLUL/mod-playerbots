# Iron Assembly (Assembly of Iron) — guide-parity rework

## Context

The Ulduar Assembly of Iron strategy in `src/Ai/Raid/Uld/` is one of the thinnest boss strategies in
the module: 5 triggers and 5 actions, of which 3 are Brundir movement dodges and 2 are
hard-mode-only. Since it was written the raid framework grew a documented convention set
(`docs/raids/README.md`, `docs/engine/raid-mechanics-lessons.md`) and several reference-quality
encounters (Vezax, Freya, Mimiron, EoE). This document audits the strategy against both the
conventions and the actual encounter, and specifies the rework.

**The headline problem: outside hard mode, bots do not focus fire.** Killing one council member
restores the other two to full health, so damage spread across members is thrown away — and the
kill-order node is gated behind a config flag that defaults to 0.

Ground truth, in order of authority:

1. `src/server/scripts/Northrend/Ulduar/Ulduar/boss_assembly_of_iron.cpp` — this core's script.
   **Verified stock upstream**: every commit touching it is an upstream PR (`#26470`, `#26449`,
   `#26262`, `#26200`, `#25029`, `#24714`). No fork-local patches.
2. 3.3.5 DBC dumps in `modules/mod-spell-tweaks/data/dbc-reference/` — radii, durations, dispel
   types, damage.
3. `acore_world.creature_immunities` — mechanic immunity masks.
4. `src/tools/navprobe` against map 603 — every coordinate below is probed, none invented.
5. Nine positioning diagrams supplied by the user (six normal, three hard mode).
6. Icy Veins and warcraft.wiki.gg. **warcrafttavern.com host-blocks the fetcher** — both the
   Ulduar-25 and Ulduar-10 URLs return 403 on repeat attempts, so it is not transient.

Where the guides and the core disagree, the core wins.

---

## Part 1 — Encounter ground truth

Three bosses, engaged together (`SetInCombatWithZone` + mutual `AttackStart`, so `"find target"`
resolves from the pull). When one dies the **other two are restored to full health** and gain a
Supercharge stack (61920: +25% damage, +15% scale). Each survivor's `_phase` increments on the
Supercharge `SpellHit`, so the last one alive reaches phase 3. Berserk (47008) at 15 min.
`SetLootMode(0)` on reset, `ResetLootMode()` only on entering phase 3 — **all loot comes from
whichever member dies last**.

### Steelbreaker — NPC 32867

| Phase | Ability | Detail |
|---|---|---|
| 1 | High Voltage 61890 | Self-aura, triggers 63525 every 3 s: **1500 nature, unlimited radius**. Unavoidable, healer problem. |
| 1 | Fusion Punch 61903 | Current victim, 15–20 s. **DispelType 1 = Magic**, 4 s duration, 1 s tick. Dispellable. |
| 2 | Static Disruption 61911 → 61912 | Random player **beyond 10 yd**, 20–40 s. **5000 nature in 6 yd** + **+75% nature damage taken in 5 yd** for 20 s. |
| 3 | Overwhelming Power 64637 | Current victim, repeat `RAID_MODE(61s, 36s)`. **DispelType 0 — NOT dispellable.** +200% damage, 60 s, then Meltdown 61889: **29,250 nature in 15 yd + instakill on the carrier**. |
| 3 | Electrical Charge 61902 | +25% damage per stack, gained on every player death and every Meltdown. |

### Runemaster Molgeim — NPC 32927

| Phase | Ability | Detail |
|---|---|---|
| 1 | Shield of Runes 62274 | Self, 20–34 s. **DispelType 1 = Magic**, absorbs 20,000. If fully **absorbed**, grants 62277: **+50% damage for 15 s**. Dispelling while amount > 0 denies that. |
| 1 | Rune of Power 61973 → 63513 | Every 60 s on `DoSelectLowestHpFriendly(60)` — i.e. **on a council member**, not a player. Pulses 64320: **+50% damage, 5 yd, 0.8 s refresh**, rune lives 60 s. |
| 2 | Rune of Death 62269 | Random player, 30–40 s. **DynamicObject, 13 yd, 2750 shadow per 0.5 s = 5500 dps, 30 s.** |
| 3 | Rune of Summoning 62273 | Random player, 30–45 s. Spawns rune 33051 → Lightning Elementals 32958. |
| 3 | Lightning Blast 62054 | Elemental suicide: **9425 nature in 30 yd**, kills the caster. |

`npc_assembly_lightning` overrides `AttackStart`, `MoveInLineOfSight`, `UpdateAI` and
`EnterEvadeMode` to no-ops and follows a random player. **It has no threat table — taunting and
kiting both do nothing**, contradicting the guides. Only reachable in a Molgeim-last order, which
neither chosen order uses.

### Stormcaller Brundir — NPC 32857

| Phase | Ability | Detail |
|---|---|---|
| 1 | Chain Lightning 61879 | Random target, 9–17 s. `InterruptFlags = 15` — cast time, kickable. 5 chain targets. |
| 1 | Overload 61869 (25 m 63481) | Self, 25–40 s. **6 s channel** → 61878: **20,000 nature + knockdown, 20 yd**. Brundir is **invincible for the channel while another member lives**. |
| 2 | Lightning Whirl 61915 | Self, 10–25 s. **5 s channel** → 61916: **3770 per bolt, 100 yd** — raid-wide, no positional counter. |
| 3 | Stormshield 64187 | Permanent from phase 3. 250 damage shield vs melee. |
| 3 | Lightning Tendrils 61887 (25 m 63486) | Every 35 s, delays other events 18 s. Airborne 16 s, `REACT_PASSIVE`, `MovePoint` to a random player every 6 s. **3000 (10 m) / 5000 (25 m) per second in 18 yd.** On landing: **`DoResetThreatList()`**. |

**Brundir cannot overlap two channels** — `UpdateAI` returns early on
`HasUnitState(UNIT_STATE_CASTING)` (`boss_assembly_of_iron.cpp:750`), so Overload and Lightning
Whirl never run together.

### Mechanic immunities (`creature_immunities`)

| Creature | Stun | Interrupt | Silence | Knockback |
|---|---|---|---|---|
| **Brundir** (32857), mask `0x24CB375F` | **vulnerable** | **vulnerable** | immune | immune |
| Steelbreaker (32867) / Molgeim (32927), mask `0x26CB3F7F` | immune | immune | immune | immune |

**Brundir is the only interruptible member**, and he is **silence-immune** — so `silencing shot`
and `spell lock` do not work on him, while kicks and stuns both do. This is what makes the
"Can't Do That While Stunned" achievement possible on him specifically.

### Two guide corrections

- **Lightning Tendrils is 18 yd, not 10.** 61886/63485 both carry radius 18; the 10 yd effect
  (61884, from 61883) is a dummy. The module's existing `18 + 10` is right and the guides are wrong.
- **Overwhelming Power is not dispellable.** `DispelType = 0`. `docs/raids/ulduar.md:77` currently
  says otherwise. The only counter is the carrier leaving Meltdown's 15 yd.

### Room geometry — navprobe, map 603

Anchor `(1587.18, 121.02, 427.27)`, taken from Brundir's out-of-combat channel wander
(`boss_assembly_of_iron.cpp:742`) and confirmed on-mesh (mesh surface 427.493, vmap 427.267,
settled 427.269).

| Ring radius | Result |
|---|---|
| 20 yd | 8/8 clean |
| 30 yd | 8/8 clean |
| 40 yd | 8/8 on-poly, but **45° and 135° settle to −27.7 and −438** — mesh holes |
| 50 yd | 5/8; three headings off-mesh |

**Usable envelope is ~30 yd.** Everything below lives inside it.

### Positioning, from the diagrams

*Normal:* three tanks, one per member on its own floor lobe; one member pulled far out with a large
danger circle (Brundir — Overload and Chain Lightning are the only reasons to isolate). Melee on the
kill target; ranged and healers **stacked**, not spread. When Rune of Power appears, **melee and
ranged gather onto it**. Once only Brundir remains, melee enter his circle and ranged/healers hold
its edge. On Overload the raid runs clear; the tank's shield stays beside him.

*Hard mode:* Steelbreaker parked alone while the raid kills Molgeim, everyone clear of Rune of
Death. With only Steelbreaker up, **two tanks** on him and **ranged and healers spread out
individually**. When Overwhelming Power lands, the debuffed tank runs away while the partner holds.

Three things this settles that the text guides did not: **spread is hard-mode-only** (and includes
healers); **the Overwhelming Power carrier runs out**; **the raid stands in Rune of Power**.

### What each kill order costs

| Order | Faced |
|---|---|
| **Steelbreaker → Molgeim → Brundir** (chosen, normal) | Molgeim p2 (Rune of Death), Brundir p2+p3 (Whirl, Stormshield, Tendrils) |
| **Brundir → Molgeim → Steelbreaker** (chosen, hard) | Molgeim p2, Steelbreaker p2+p3 (Static Disruption, Overwhelming Power, Electrical Charge) |
| Brundir → Steelbreaker → Molgeim | Molgeim p3 — untauntable 30 yd suicide adds. Not used. |

---

## Part 2 — What the strategy does today

`Trigger/UldTriggers_IronAssembly.{h,cpp}` (127 lines), `Action/UldActions_IronAssembly.{h,cpp}`
(127 lines), wired at `UldStrategy.cpp:177-199`:

| Node | Priority | Roles | Behaviour |
|---|---|---|---|
| lightning tendrils | `ACTION_RAID` | all | `MoveAway(brundir, 28 − dist)` while 61887/63486 up and within 35 yd |
| overload | `ACTION_RAID` | non-tank | `MoveAway(brundir, 25 − dist)` while the Overload aura is up |
| rune of power | `ACTION_RAID` | tank | `MoveAway(currentTarget, 10, backwards)` when the bot's target has 64320 and is hitting the bot |
| kill order | `ACTION_RAID` | MT, **hard mode only** | skull-mark Brundir → Molgeim → Steelbreaker |
| fusion punch swap | `ACTION_RAID + 2` | MT ↔ assist-0, **hard mode only** | off-tank attacks + `"taunt spell"` when the active tank carries 64637 |

Supporting: `UldHardMode.cpp:16-38`, spell ids `UldBossHelper.h:27-37`, entries `UldScripts.h:17-20`,
threat-redirect veto `UldMultipliers.cpp:373`, Bloodlust hold `UldMultipliers.cpp:481-486`.

No values, no per-boss multiplier, no `Util/UldEncounter_IronAssembly.*`, no geometry constants, no
`InitMultipliers` entry.

---

## Part 3 — Findings

### Defects

**F1 — Two of the four "Overload" spell ids are Lightning Tendrils.** `UldBossHelper.h:32-33`
defines `SPELL_OVERLOAD_10_MAN_2 = 63485` and `SPELL_OVERLOAD_25_MAN_2 = 61886`; both are
**Lightning Tendrils damage triggers** (bp 4999 / 2999, radius 18) — direct-damage spells that never
sit as an aura on Brundir. The two real ids are also present so the dodge still fires, but the extra
checks are dead weight and misleading.

**F2 — Overload excludes tanks. Correct; keep it, but say why.**
`UldTriggers_IronAssembly.cpp:38` returns false for tanks with no comment. Keeping it is right: the
diagram holds the tank beside Brundir through the Overload frame, 20,000 nature is survivable for a
plate tank and lethal to a clothie, and under the chosen order Brundir dies last — he is the only
member alive during his own Overloads, so `OnSpellCast`'s invincibility never applies and a tank
leaving would cost real uptime and let him drift. **Not a defect**; add the comment.

**F3 — Boss resolution by localized name off the bot's own threat list.**
`AI_VALUE2(Unit*, "find target", "stormcaller brundir")` in both triggers and both actions.
`FindTargetValue::Calculate` (`src/Ai/Base/Value/TargetValue.cpp:159-184`) walks
`GetThreatenedByMeList()` and string-matches `unit->GetName()`. It returns null for any bot not on
Brundir's threat list — and he calls `DoResetThreatList()` every Tendrils landing — and it breaks
entirely on a non-enUS world DB. `docs/raids/README.md:121-124` calls this out, and the same file
already uses `GetFirstAliveUnitByEntry` for Steelbreaker.

**F4 — Hazard dodges run at `MOVEMENT_COMBAT`.** `MovementAction::MoveAway`
(`src/Ai/Base/Actions/MovementActions.cpp:1603`) hardcodes it. `docs/raids/README.md:34` reserves
`ACTION_EMERGENCY + 6` with `MOVEMENT_FORCED` for lethal hazards.

**F5 — `IsSteelbreakerEmpowered` is gated on the hard-mode config flag** (`UldHardMode.cpp:20-21`).
`docs/raids/ulduar.md:30-31` lists this predicate as a *phase* check that must stay dynamic. As
written, a raid reaching Steelbreaker-last without the flag set gets no tank swap and chain-dies to
Meltdown.

**F6 — `IronAssemblyFusionPunchSwapTrigger` never reads Fusion Punch.** The in-code comment explains
the deliberate choice to swap on Overwhelming Power only. The reasoning is sound; the name is not.

**F7 — The 35 yd trigger gate will outlive its own correctness.** With the new layout, raid-to-
Brundir is ~38 yd, so the gate — not the hazard radius — is what stops bots reacting. Any later
tweak to the stack point silently changes hazard behaviour.

### Gaps

**G1 — No focus fire outside hard mode.** `IronAssemblyKillOrderTrigger` requires
`IsIronAssemblyHardModeActive`, default 0. `GeneralFindTargetSmartStrategy` ranks by attack range
then remaining lifetime with no current-target preference, so 25 bots spread damage across three
bosses whose health resets. XT-002, Auriaya, Kologarn and Freya all have a `set dps priority` plus a
targeting-suppression multiplier; Iron Assembly has neither. **Largest gap in the encounter.**

**G2 — Rune of Death unhandled.** 13 yd, 5500 dps, 30 s, from Molgeim phase 2 onward — which the
chosen order guarantees. `GetDynamicObjectPositions` (`RaidBossHelpers.h:37`) finds it and
`FindNearestPositionClearOfHazards` (`:42`) is the escape — the pair Vezax uses for Profound
Darkness.

**G3 — No interrupts.** Chain Lightning (every 9–17 s all fight) and Lightning Whirl (raid-wide,
must be stopped) have no node. Class interrupts sit at `ACTION_INTERRUPT` (40), below `ACTION_RAID`,
so they never fire for a boss cast on their own.

**G4 — No dispels.** Shield of Runes (letting it drain hands Molgeim +50% for 15 s) and Fusion
Punch. Class dispel nodes sit at 40–57, below `ACTION_RAID`.

**G5 — No positioning or tank layout.** No anchor, no arc, no spread. Every reworked Ulduar boss has
one.

**G6 — Lightning Elementals unhandled.** Unreachable under both chosen orders. Deliberate non-goal.

**G7 — The Overwhelming Power carrier never runs out.** The swap taunts the boss off them, but
nothing moves them clear of Meltdown's 15 yd, and every death it causes is another +25% Electrical
Charge.

**G8 — Nobody moves *into* Rune of Power, and the one node that touches it moves the raid away.**
The diagrams show melee and ranged converging on the rune. The module's node does the opposite half
— a tank backs off 10 yd, dragging the boss out. Both halves are legitimate (Icy Veins: "bosses
directly next to Rune of Power, not inside of it") but they conflict for melee, because the core
places the rune on the kill target itself.

**G9 — No per-boss tank assignment.** Every diagram shows one tank per living member.
`UldThreatRedirectMultiplier` (`UldMultipliers.cpp:373`) already *asserts* "one tank per council
member" to justify vetoing the generic redirect, but nothing makes it true. Precedent for the fix is
strong: Gruul/Maulgar maps first and second assist tanks to named adds, and ZA Halazzi plus four ToC
bosses do the same.

### Convention gaps

| # | Convention | Status |
|---|---|---|
| 1 | Logic in a shared `Util/*Encounter_*` read by trigger and action | ✗ actions re-instantiate the trigger on the stack in `isUseful()`, then duplicate the lookup |
| 2 | Priority stack commented as a survival ranking | ✗ four nodes flat at `ACTION_RAID` |
| 3 | Positioning returns false once parked | ~ distance test, no hysteresis |
| 4 | Hazard escape via `FindNearestPositionClearOfHazards` | ✗ `MoveAway` clears one hazard, can step into another |
| 5 | Movement-suppression multiplier | ✗ none — generic mover competes every tick |
| 6 | Whole-fight targeting suppression with a terminal fallback | ✗ none |
| 7 | Boss resolution by entry | ✗ for Brundir, ✓ for the hard-mode paths |
| 9 | Expensive derivations memoised, triggers throttled | ✗ all five run per tick |
| 10 | Taunt scoped to what the encounter owns | ✓ |
| 11 | Hard mode = config read in one detector | ~ detector is clean but leaks into the phase check |
| 12 | Geometry constants in the header with derivation | ✗ inline literals |

### What is already right

Tendrils radius (18 + 10) matches DBC and beats the guides. Overload radius (20 + 5) matches 61878.
The 10/25 aura pairs for Tendrils and Overload are correct and OR-ing both is the documented
no-heroic-branch idiom. Swapping on Overwhelming Power rather than Fusion Punch, with the reasoning
in a comment. `GetDistance2d` against a hovering boss. Not handling Electrical Charge positionally.

---

## Part 4 — Decisions

All settled. Numbered for reference from the implementation section.

**Scope and delivery**
1. Full parity rework, all phases.
2. **One PR, no slices.**
3. **Ungated** — no rework opt-out config. The eight `Ulduar*HardMode` options declare raid intent,
   not rework toggles.
4. **Local-only.** Record the core-version assumption in `docs/raids/ulduar.md` as a note rather
   than defensive code — the strategy depends on recent upstream fixes (`#26470` Rune of Death
   restricted to players, `#26449` Brundir surviving Tendrils, `#26200` Static Disruption preferring
   ranged, `#25029` Overload invincibility).

**Kill order and targeting**
5. **Normal order: Steelbreaker → Molgeim → Brundir.** Hard mode keeps Brundir → Molgeim →
   Steelbreaker. `GetIronAssemblyNextKillTarget` branches on the config flag.
6. **Focus fire via `set dps priority` + unconditional targeting suppression**, not marker-driven.
7. **A human's skull mark wins.** If a human has marked a living council member, follow that;
   otherwise use our order.
8. **Suppression covers every role, whole fight, keyed on action type only** — no role test, so
   healers cannot be swept in by `IsRanged()` returning true for them
   (`docs/engine/pitfalls.md:225-247`). Terminal fallback is always a living council member.

**Interrupts**
9. **Stuns in, silences out.** Usable list is `{kick, pummel, shield bash, counterspell, wind shear,
   mind freeze}` plus stuns. **Not** `silencing shot` or `spell lock` — Brundir is silence-immune.
10. **Lowest ready interrupter by guid takes Lightning Whirl; the second takes Chain Lightning;
    nobody else acts.** Reproduces "let some through" with no threshold to tune.
11. **The election skips any bot currently executing a dodge or a return**, so the kick falls to
    someone already parked. At 25 bots there is always one.

**Tanks**
12. **Full three-way assignment**, main tank → assist 0 → assist 1 mapped onto the kill order.
13. **Graceful collapse below three tanks**, with **Brundir's slot filled first** at every roster
    size — his is the only position that prevents damage.
14. **Surplus tanks DPS the kill target.** No taunt reserve, no three-way swap rotation.
15. **Combat-only positioning, but Brundir's tank gets a high-priority walk.** No out-of-combat
    pre-positioning — that would need a non-combat node fighting `FollowMasterStrategy`, which
    nothing in Ulduar does.
16. **Narrow the threat-redirect veto**: remove the three council entries and add a boss-scoped
    redirect targeting the *assigned* tank, on the Freya shape (`UldActions_Freya.cpp:294-326`) over
    the `NaxxRedirectThreatAction` base that covers hunters and rogues. `docs/raids/README.md:50` —
    the MT is not *reliably* wrong here, merely wrong for two of three bosses, which is the exact
    case the corollary says not to veto.

**Geometry** — all points probed on-mesh, anchor `(1587.18, 121.02, 427.27)`

| Slot | Bearing / radius | Coordinate |
|---|---|---|
| Brundir tank | 0° / 28 yd | `(1615.18, 121.02, 427.27)` |
| Steelbreaker tank | 135° / 16 yd | `(1575.87, 132.33, 427.27)` |
| Molgeim tank | 225° / 16 yd | `(1575.87, 109.71, 427.27)` |
| Ranged + healer stack | 180° / 10 yd | `(1577.18, 121.02, 427.27)` |
| Ranged + healer stack, **Brundir-last** | 180° / −3 yd | `(1590.18, 121.02, 427.27)` |

17. Brundir-to-stack is 38 yd — deliberately more than the 25 the dodge needs, so bots in the stack
    never react to Overload at all, which is what makes decision 11 reliable. Steelbreaker and
    Molgeim sit 22.6 yd apart and 11.4 yd from the stack.
18. **Stand, don't drag.** Tanks walk to fixed spots and bosses follow.
19. **Fixed stack point, not one that tracks the kill target** — two stack points across the fight,
    not a moving one. The Brundir-last point sits 25 yd from him, toward the centre.
20. **Ranged spread is hard-mode-only**, gated on `IsSteelbreakerEmpowered`, and includes healers.
    Under the normal order Static Disruption never fires.

**Hazards and buffs**
21. **Overload: tanks stay** (decision confirms F2). **Rune of Death: purely reactive** — the stack
    scatters and re-forms; no permanent loosening, since Rune of Death is 30 s on a 30–40 s cadence.
22. **Rune of Power: both halves.** Keep the tank drag-out; add a soak that walks ranged and healers
    into the rune. Melee lose it and that is accepted. The soak **outranks the layout** but sits
    below every hazard node, and takes a **25 yd cap** — which admits every rune on Steelbreaker or
    Molgeim and rejects every rune on Brundir, so it can never drag the ranged group into Overload.
23. **Movement guard is scoped to the hazard window**, not the whole fight — a whole-fight guard is
    the Void Reaver failure at `docs/raids/README.md:83-86`.
24. **Bloodlust stays gated on exactly one member alive, in both modes — no change to
    `UlduarBurstWindowMultiplier` (`UldMultipliers.cpp:481-486`).** Worth recording why, since the
    normal order makes the survivor Brundir with Stormshield and 16 s of every 35 spent airborne:
    only the last member's health bar actually counts, because every earlier kill restores the
    others to full, so lust spent before then is partly refunded to the boss. Melee downtime during
    Lightning Tendrils is the smaller loss. Ranged keep hitting him throughout — he is airborne and
    `REACT_PASSIVE`, not immune, and `SetRegeneratingHealth(false)` means nothing heals back.
25. **Latched state resets on all three alive at full health**, with a staleness backstop —
    **never on "not in combat"**, which Brundir's 16 s `AttackStop` would trip every Tendrils. The
    core's own comment at `boss_assembly_of_iron.cpp:138-140` records the same trap.
26. **Trigger gates use hazard radius + tolerance, not a flat 35 yd** (fixes F7).

---

## Part 5 — Implementation

Node priorities, as a survival ranking. Comment it in `InitTriggers` the way
`UldStrategy.cpp:496-506` does for Vezax.

| Relevance | Node |
|---|---|
| `ACTION_EMERGENCY + 10` | reset encounter state |
| `+ 7` | overwhelming power carrier runs out (≥ 20 yd from nearest raid member) |
| `+ 6` | overload dodge · lightning tendrils dodge — both `MOVEMENT_FORCED` |
| `+ 5` | rune of death dodge — `MOVEMENT_FORCED`, trigger throttled to `200` ms |
| `+ 4` | brundir interrupt |
| `ACTION_RAID + 6` | tank assignment (Brundir's slot first) |
| `+ 5` | overwhelming power tank swap (existing, renamed) |
| `+ 4` | shield of runes dispel |
| `+ 3` | fusion punch dispel |
| `+ 2` | threat redirect (new, boss-scoped) · rune of power tank drag-out (existing) |
| `+ 1` | rune of power soak (ranged + healers, 25 yd cap) · set dps priority |
| `ACTION_RAID` | raid position — one action picking stacked or spread on `IsSteelbreakerEmpowered` |

Positioning actions return false once parked, with a deadband, so lower nodes still get a tick.

### Work items

1. `Util/UldEncounter_IronAssembly.{h,cpp}` — boss resolution by entry, phase inference, kill order,
   hazard lookups, interrupter election, tank-slot mapping, latched per-instance state. Triggers and
   actions call it; retires the `isUseful()`-constructs-a-trigger idiom in all five actions.
2. Geometry constants into `UldBossHelper.h` beside the Vezax block (`:399-460`), each with its
   derivation and the navprobe envelope in the comment.
3. **F3** — replace all four `"find target"` lookups with `GetFirstAliveUnitByEntry(botAI,
   NPC_BRUNDIR)`.
4. **F1** — delete `SPELL_OVERLOAD_10_MAN_2` / `SPELL_OVERLOAD_25_MAN_2`.
5. **F5** — drop the hard-mode gate from `IsSteelbreakerEmpowered`; update `UldHardMode.h:26-35`.
6. **F2** — keep the tank exclusion, add the comment.
7. **F6** — rename the swap trigger/action to `IronAssemblyOverwhelmingPowerSwap*`.
8. **F7 / 26** — replace the 35 yd gates with hazard radius + tolerance.
9. **G1** — `iron assembly set dps priority` (order branches on the flag, human skull wins) +
   `IronAssemblyDisableAutomaticTargetingMultiplier` on the Freya shape (`UldMultipliers.cpp:158-177`).
10. **G9 / 12–16** — tank assignment, graceful collapse, Brundir priority walk, boss-scoped redirect,
    veto entries removed from `UldMultipliers.cpp:373`.
11. **G5 / 17–20** — raid position action, two stack points, hard-mode spread.
12. **G3 / 9–11** — interrupt node on the Vezax election shape
    (`Util/UldEncounter_Vezax.h:113-119`, `.cpp:448-488`), matching spell ids 61915 and 61879.
13. **G2** — Rune of Death dodge via `GetDynamicObjectPositions(bot, 40.0f, 62269)` +
    `FindNearestPositionClearOfHazards`.
14. **G4** — Shield of Runes and Fusion Punch dispels; reuse the offensive-dispel list at
    `ToCActions.cpp:452-453` (`{spellsteal, purge, dispel magic}`).
15. **G7** — Overwhelming Power carrier runs clear, driven off the phase check not the config flag.
16. **G8 / 22** — Rune of Power soak.
17. **F4 / 23** — dodges to `MOVEMENT_FORCED`; `IronAssemblyMovementGuardMultiplier` on the Vezax
    model (`UldMultipliers.cpp:553-577`), hazard-window scoped, exempting `AttackAction` and
    `ReachTargetAction`.
18. **25** — reset node.
19. Rewrite `InitTriggers` as the commented survival ranking above.
20. Docs: fix the Overwhelming Power dispel claim at `docs/raids/ulduar.md:77`; rewrite the Assembly
    section; record G6 as a deliberate non-goal with the `npc_assembly_lightning` reasoning; record
    the core-version assumption from decision 4.

### Critical files

| Path | Change |
|---|---|
| `src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.{h,cpp}` | new — all encounter logic |
| `src/Ai/Raid/Uld/Trigger/UldTriggers_IronAssembly.{h,cpp}` | F1, F3, F6, F7; new triggers |
| `src/Ai/Raid/Uld/Action/UldActions_IronAssembly.{h,cpp}` | F4; new actions |
| `src/Ai/Raid/Uld/Util/UldHardMode.{h,cpp}` | F5, kill-order branch |
| `src/Ai/Raid/Uld/Util/UldBossHelper.h` | spell ids, geometry constants |
| `src/Ai/Raid/Uld/UldMultipliers.{h,cpp}` | two new multipliers, veto narrowed (lust window unchanged) |
| `src/Ai/Raid/Uld/UldStrategy.cpp` | trigger nodes + `InitMultipliers` |
| `src/Ai/Raid/Uld/Uld{Trigger,Action}Context.h` | register new names |
| `docs/raids/ulduar.md` | rewritten Assembly section |
| `docs/plans/iron-assembly-guide-parity/…PLAN.md` | this document, copied in as step 0; folded into `ulduar.md` and deleted on ship (`docs/raids/README.md:187-191`) |

No `CMakeLists.txt` edit — `.cpp` files are globbed.

### Verification

The module cannot be compiled in this environment, so verification is static here plus an in-game
pass by you.

Static:
- `python apps/codestyle/codestyle-cpp.py`
- Every new trigger/action name in exactly one `creators[...]` and one `TriggerNode`/`NextAction`.
- Every spell id cross-checked against `modules/mod-spell-tweaks/data/dbc-reference/`.
- Every coordinate already navprobed above — re-probe anything that changes.

In-game, 10-man and 25-man, both flag states:
1. Flag 0 — skull tracks **Steelbreaker** first; Molgeim's and Brundir's bars stay untouched until
   he dies, then Molgeim, then Brundir.
2. Tanks reach their spots; Brundir ends up ~38 yd from the ranged stack.
3. Overload — non-tanks clear 25 yd, the tank holds, Brundir does not drift toward the raid.
4. Molgeim phase 2 — bots vacate Rune of Death within ~1 s and do not step back in; the stack
   re-forms between runes.
5. Rune of Power — ranged and healers enter it while the tank walks the boss out; the soak yields
   the moment a hazard trigger fires; no soak attempted on a rune more than 25 yd away.
6. Brundir phase 2 — **Lightning Whirl interrupted on every cast**, duty rotating between bots. This
   is the make-or-break test for the chosen order.
7. Brundir phase 3 — Tendrils out-run at 28 yd; bots re-acquire him after the landing
   `DoResetThreatList()`; the raid relocates to the second stack point.
8. Bloodlust is held until exactly one member is alive, in both modes — confirm it does not fire on
   Steelbreaker or Molgeim in a normal clear.
9. Flag 1 — order flips to Brundir-first; ranged and healers spread once Steelbreaker is alone; the
   swap fires and the carrier walks clear before Meltdown.
10. Wipe and re-pull — latched state resets, no stale layout or phase flag.
11. Watch for oscillation at every hazard boundary
    (`docs/engine/raid-mechanics-lessons.md:56-92`).
