# Ulduar Boss Strategy — Gap & Hard-Mode Analysis

## Context

Playerbot Ulduar raid AI lives in `src/Ai/Raid/Uld/` (`UldStrategy.cpp` wires
trigger→action nodes; `UldTriggers.cpp/.h`, `UldActions.cpp/.h`,
`Util/UldBossHelper.cpp/.h` implement them). Goal: find gaps that wipe raids,
and confirm strategies can clear Hard Modes. This doc is the analysis; the
implementation scope is chosen with the user before any code is written.

**Key config fact:** stock `AiPlayerbot.BotCheats = "food,taxi,raid"` — raid
cheat is ON by default. Many mechanics are "handled" only by a cheat
(instakill add / strip debuff / refill mana / teleport). Those function in the
default config but (a) are crutches, not real play, and (b) silently no-op if a
user disables the raid cheat. They are flagged below as **CHEAT-ONLY**.

---

## Severity 1 — Normal-mode wipe risks (fail even WITH raid cheat on)

| Boss | Gap | Effect |
|---|---|---|
| **Ignis** | Entire fight unimplemented. Only a fire-resistance aura buff wired (`UldStrategy.cpp:54`). No Slag Pot (grab→pot), Flame Jets (raid knock-up), Scorch ground fire, Iron Construct tanking/kiting/shatter-in-water. | Bots stand in fire, ignore constructs → wipe. |
| **Auriaya** | Entire fight unimplemented except fall-recovery teleport (`UldActions.cpp:1401`). No Sonic Screech facing, Terrifying Screech fear, Feral Defender (9-lives add + void zones), Sanctum Sentry adds. | Screech + Defender kill the raid. |
| **Mimiron (Firefighter)** | No ground-fire avoidance anywhere — only fire *resistance* aura. Firefighter fills the floor with fire. Frost Bomb NPC not even enum'd. | Bots stand in persistent fire → wipe. |
| **Thorim** | Unbalancing Strike has **no real tank swap** — only a CHEAT-ONLY debuff strip (`UldActions.cpp:1689`). No off-tank taunt. | Non-cheat: tank dies. |
| **Vezax** | Saronite Vapor puddles (dropped by Shadow Crash) never dodged — only the self-debuff mover exists (`UldActions.cpp:2489`). | Bots stand in puddles, gain Saronite buff / die. |
| **Razorscale** | Dark Rune Watcher / Guardian adds have no interrupt or focus (only Sentinel handled). Flame Breath frontal cone not dodged. | Unchecked add casts + breath damage. |
| **Freya** | Snaplasher (hardening under multi-attacker) is only skull-marked — bots multi-DPS it → it hardens and mishandles. Storm Lasher cast not interrupted. | Add scaling → prolonged fight / wipe. |
| **Algalon** | Collapsing Star (32955) unhandled — `big bang hide` and `constellation kite` both search only for *existing* Black Holes; if none exist they silently fail. | Big Bang with no hole = raid death. |

## Severity 2 — CHEAT-ONLY crutches (work default, break if raid cheat off; not "solid")

- Hodir Biting Cold: real movement commented out; cheat strips aura (`UldActions.cpp:1485`). No fallback if no icicle near for Flash Freeze.
- Thorim Unbalancing Strike swap (above).
- Mimiron Proximity Mines + Bomb Bots: cheat-kill only (`UldTriggers.cpp:1488`). Signature Firefighter killers have no avoidance.
- Kologarn Crunch Armor + Focused Eyebeam: cheat aura-remove / teleport (`UldActions.cpp:1392,1365`) — no real tank swap / kiting.
- Yogg Ominous Clouds, Crusher/Constrictor tentacles, illusion-room adds, P2 movement: cheat instakill / teleport (`UldActions.cpp:2537,2580,2870`).
- Vezax no-mana-regen: cheat mana refill only (`UldActions.cpp:2479`).

## Severity 3 — Hard Mode NOT implemented (no code path exists for any)

| Boss | Hard-mode trigger | Status |
|---|---|---|
| Flame Leviathan | Tower control (leave Storm/Flame/Frost/Life up) | **Absent** — zero tower logic; no hard-mode at all. |
| Assembly of Iron | Kill order (Steelbreaker last) + Steelbreaker/Molgeim abilities | **Absent** — triggers cover Brundir only; no kill-order enforcement. |
| Hodir | Beat rare-cache / enrage timer (adds, NPC buffs, Toasty Fire) | **Absent** — Toasty Fire enum'd but unused; no timer awareness. |
| Freya | Leave 1–3 Elders alive (Brightleaf/Ironbranch/Stonebark) | **Structurally impossible** — the 3 Elders have no NPC entries in `UldBossHelper.h`. |
| Thorim | Kill boss within arena time limit | **Absent** — gauntlet followed, no kill-speed / timer logic. |
| Mimiron | Firefighter (no extinguish; fires + frost bombs persist) | **Absent** — no fire-cell avoidance, no frost bomb. |
| Vezax | Let Saronite Vapors live → kill Saronite Animus | **Absent** — no Animus/Vapor NPC entries, no "leave vapors" toggle. |
| Yogg-Saron | Reduce active Keepers (down to Alone in the Darkness) | **Absent** — no keeper selection/beam logic; no buff checks. |

## Structural observations

- No boss reuses `RazorscaleBossHelper`'s role-swap machinery (`UldBossHelper.cpp:198`) for real tank swaps — Thorim/Hodir/Kologarn all fall back to cheats instead.
- No EventMap/enrage-timer reading for Hodir/Freya/Thorim/Mimiron — no soft-enrage awareness (blocks every hard-mode kill-timer requirement).
- Ignis + Auriaya are the only two bosses with essentially no mechanical AI.
- Hard-mode enablement generally requires new NPC entries (Freya Elders, Vezax Animus/Vapor, Flame Leviathan towers) that do not exist yet.

---

## Scope (confirmed with user)

Implement **all Severity-1 normal-mode wipe fixes across every boss**. Hard Mode
(Sev-3) deferred to a later phase. Cheat crutches (Sev-2) replaced with real
mechanics **per-boss** only where the real fix is the same work as the wipe fix
(Thorim tank swap, Mimiron mines); otherwise leave the working cheat handler in
place and only add the mechanic that fails even with cheat on.

### Framework recap (each new behavior = same 5 edit sites)

Per the existing pattern, one new trigger→action pair touches:
1. `UldTriggers.h` / `UldTriggers.cpp` — new `Trigger` subclass (detection).
2. `UldActions.h` / `UldActions.cpp` — new `Action` subclass (behavior).
3. `UldTriggerContext.h` + `UldActionContext.h` — register creator strings.
4. `UldStrategy.cpp` `InitTriggers` — `TriggerNode` wiring + `ACTION_*` priority.
5. `Util/UldBossHelper.h` — add missing NPC/spell/GO enums + positions.

**Reuse (do NOT rebuild):**
- Cone/facing: `IsBotInFrontalCone(bot, source, angle, range)` (`RaidBossHelpers.h:24`).
- Ground/puddle avoidance: existing `MoveAwayFromCreature` / `FleePosition` patterns already used by Vezax shadow-crash & Mimiron rocket-strike actions.
- Add lookup: `GetFirstAliveUnitByEntry` (`RaidBossHelpers.h:21`).
- Real tank swap: `RazorscaleBossHelper::AssignRolesBasedOnHealth` role-swap machinery (`UldBossHelper.cpp:198`) + `GetGroupAssistTank` taunt — reuse for Thorim.
- Marking: `MarkTargetWithSkull` etc. (`RaidBossHelpers.h:7-15`).
- Interrupts: bots already kick via class rotations; raid layer only needs to focus/priority the caster (mark), not new interrupt code.

### Per-boss work items

**Ignis** (new — no prior mechanics):
- Add enums: Iron Construct, Slag Pot spells, Scorch GO/NPC (`UldBossHelper.h`).
- Ground-fire (Scorch) avoidance action — reuse `MoveAwayFromCreature`/`FleePosition`.
- Slag Pot victim: skull-mark grabbed player for heal focus (marking helper).
- Iron Construct: tank marks + assigns; DPS focus. (Water-shatter = Sev-3, skip.)

**Auriaya** (new — only fall-recovery existed):
- Add enums: Sanctum Sentry, Feral Defender, Sonic/Terrifying Screech spells.
- Sonic Screech: if `IsBotInFrontalCone(bot, Auriaya, …)` → turn/step out of cone.
- Feral Defender: skull-mark + focus; avoid its death void zone.
- Sentinel adds: mark + assign at pull.

**Mimiron (Firefighter fires)** — per-boss: keep cheat mine-kill, ADD real avoidance:
- Ground-fire cell avoidance action (persistent Firefighter fire) — reuse flee/move-away primitives; wire high priority.
- Proximity Mine real avoidance action as non-cheat fallback alongside existing cheat-kill.

**Vezax** (new avoidance):
- Add enum: Saronite Vapor NPC + Shadow Crash puddle.
- Puddle avoidance action (dodge existing Saronite/Shadow Crash ground pools), separate from the self-debuff mover.

**Thorim** — per-boss: replace cheat with real swap:
- `ThorimUnbalancingStrikeAction`: off-tank taunt swap via `GetGroupAssistTank` + role-swap helper instead of cheat aura-strip (keep cheat gated as fallback).

**Hodir**:
- Flash Freeze fallback when no icicle within range (move to nearest icicle always, or safe spot).
- Real Biting Cold movement (jump/reposition) replacing the commented-out/cheat path.

**Razorscale**:
- Focus/mark Dark Rune Watcher + Guardian casters (add NPC enums; mark for interrupt priority).
- Flame Breath frontal-cone dodge via `IsBotInFrontalCone`.

**Freya**:
- Snaplasher single-target discipline: keep only assigned attacker(s) on it (avoid multi-attacker Hardening); adjust `FreyaMarkDpsTargetAction` assignment.
- Storm Lasher: mark for interrupt focus.

**Algalon**:
- `AlgalonCollapsingStarTrigger`/`Action`: mark + focus Collapsing Star (32955, already enum'd) so a Black Hole exists.
- Guard `big bang hide` / `constellation kite`: if no Black Hole found, fall back to a safe-spread position instead of silently no-op'ing.

### Suggested implementation order (by wipe impact)

1. Ignis + Auriaya (unimplemented — biggest correctness win).
2. Mimiron fires + Vezax puddles (stand-in-bad avoidance).
3. Thorim swap + Hodir Flash Freeze fallback.
4. Razorscale casters/cone + Freya Snaplasher + Algalon Collapsing Star.

## Verification

Builds are slow — do NOT build unless asked. To validate end-to-end:
- Enable `RaidUlduarStrategy`, spawn a bot raid, and pull each fixed boss on a
  local worldserver; confirm bots dodge the specific mechanic (fire/puddle/cone),
  swap tanks on Thorim, hide under icicle on Hodir with no nearby icicle, and
  focus the new marked adds.
- Test each boss twice: raid cheat ON (default) and OFF, to confirm the non-cheat
  paths (Thorim swap, Mimiron mine avoidance) actually fire.
- Run `python apps/codestyle/codestyle-cpp.py` before claiming done.
- Confirm no regressions in already-handled mechanics on each touched boss.
