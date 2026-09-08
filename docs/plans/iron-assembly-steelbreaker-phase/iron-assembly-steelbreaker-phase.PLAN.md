# Iron Assembly — stop the Overwhelming Power carrier towing Steelbreaker

## Context

Two 25-man hard-mode pulls traced 2026-09-08, after commit `d4ff61347` was built and deployed. Both
wipes, same roster: **one bot tank** (Bulwark) plus two human death knights.

| | file | duration | deaths | Brundir | Molgeim | Steelbreaker alone |
|---|---|---|---|---|---|---|
| **E** | `603_1_stormcaller-brundir_1788897992.ndjson` | 3:57 | 31 | 1:34.9 | 3:27.3 | 29.7 s |
| **F** | `603_1_runemaster-molgeim_1788898372.ndjson` | 4:45 | 31 | 1:37.6 | 3:17.9 | 87.1 s |

`d4ff61347` is confirmed working: `ironassembly.alive` emits **3 rows, one per transition** (was six
per transition), spread slots are one write per bot, `iron assembly charge guard` vetoed 6 and 4
gap-closers, and the hard-mode cooldown hold fired 981 / 958 vetoes before phase 3 and **zero
during** — Heroism went out 4.3 s and 3.6 s into the Steelbreaker phase, exactly as designed.

The raid still dies in that phase, and the user's observation — Bulwark running across the arena —
is the cause of it.

## What the traces say

### Overwhelming Power always lands on the tank, and the bot runs the moment it does

`boss_assembly_of_iron.cpp:355` casts it on `me->GetVictim()`, never a random player, repeating
`RAID_MODE(61s, 36s)` from 8 s into phase 3. In 25-man that is **every 36 s on whoever is holding
Steelbreaker**. The aura (61888) lasts **35 s** (DurationIndex 125), is `DispelType 0`, and its
`EffectTriggerSpell_2` is Meltdown 61889, whose `Effect_2 = 1` is **INSTAKILL**. The carrier dies;
nothing prevents it.

`IronAssemblyOverwhelmingPowerRunOutTrigger` fires the instant the aura appears and stays true for
all 35 s (any raid member within `MELTDOWN_CLEARANCE`). The action searches for a spot 20 yd clear of
**all 24 other players**, which in a full room is the far side of the arena. Because the carrier is
the tank, **Steelbreaker follows him**:

```
pull F  206.0  Overwhelming Power on Bulwark, both parked at the Steelbreaker spot
        208.0  Bulwark bolts north; Steelbreaker turns and follows
        214.2  Bulwark (1607.2,158.1) 42 yd from the anchor, boss 36 yd behind him
        241.0  Bulwark dies to Meltdown having travelled 408 yd; boss travelled 180 yd
pull E  215.2  same, and he dies at 221.4 with the boss loose 3.7 yd from the ranged stack
```

**Melee uptime on Steelbreaker in phase 3 collapses to 10-20%** (eight melee, 1.7%-45%, median 16%).
That is the whole story of the damage:

| | Steelbreaker HP over the phase |
|---|---|
| **E** | 99.47% → **99.09%** in 29.7 s. The raid did 0.4% of a 12.05M bar. |
| **F** | 99.32% → 64.5% in 43 s (~105k dps), then deaths healed him back to 99.75%. |

### Every death heals Steelbreaker, which is why the spiral is unrecoverable

Electrical Charge (61902) is `Effect_1 = 6` (+25% damage, all schools) **and `Effect_2 = 136`, a
percent heal**. Measured on every phase-3 death in pull F, without exception:

```
241.0 Bulwark      63.51% -> 73.19%      254.4 Felesta   59.60% -> 69.40%
273.1 Bulwark      61.19% -> 70.96%      276.2 Nightwarrior 68.39% -> 78.21%
```

**+9.7% of max health per death, ~1.17M, plus a permanent +25% damage.** The tank dies every 36 s by
design, so the raid must beat ~32k HPS of returned health before it makes any progress at all, and
every extra corpse adds another 1.17M. High Voltage per hit tracked it: 2371 → 3322 → 4170 → 8947 in
E, and 2412 → 9622 → 36000 in F.

### Knock-on: burst is vetoed whenever the boss has no tank

`HoldBurstUntilTankEngagedMultiplier` → `TankHasHeldBoss` needs `boss->GetVictim()` to be a grouped
tank continuously for 5 s (3 s for lust) and resets on any gap. Vetoes cluster exactly at the phase
transition and at each tank death — F: 44 and 36 at 195-205, **none from 210 to 235**, then 51 at
235, 35 at 250, 60 at 270. Per the user's decision this is left alone and re-measured: keeping the
tank on the boss removes most of it by itself.

### Checked and not a defect

- **Fusion Punch dispels work.** Every application is removed within 0.1-0.6 s, 11-12 successful
  dispels per pull. The 34-58k in a death rewind is the direct hit summed over the 15 s window.
- **Meltdown is survivable.** Pull C caught six melee at 275.1 for 16,299-26,810 with 2.9-12.7k
  resisted; all six lived, at 39-66% health. Nobody needs to dodge it.
- **Runes of Death outlive Molgeim** — E's last rune tick was 21.3 s into the Steelbreaker phase, so
  the raid spent most of a 29.7 s phase dodging correctly. Not a bug, but it explains why E did 0.4%
  where F did 35%.

## How this is actually played

Overwhelming Power goes to the current tank, kills them, and the raid plans around losing one player
every 36 s; guides call the phase a soft enrage that wipes the raid after about four casts. Two
things follow, and both match the user's instruction:

1. **The carrier keeps the boss to the end.** Taunting it off early saves nobody — the buff still
   kills the carrier, and the next cast lands 1 s later on whoever inherited. All an early swap buys
   is a boss that moves.
2. **The off-tank inherits on threat.** Second on the threat table picks the boss up where it
   stands, so the boss never travels and the melee never lose uptime.

Which tank holds Steelbreaker, and when: **the highest-ranked living bot tank, from the moment he is
empowered until Meltdown kills him — then the next one, in place.**

Sources: [Icy Veins](https://www.icy-veins.com/wotlk-classic/assembly-of-iron-encounter-guide-strategy-abilities-loot),
[Warcraft Tavern (25)](https://www.warcrafttavern.com/wotlk/guides/the-assembly-of-iron-strategy-guide-ulduar-25/)

## Approach

### 1. Delete the Overwhelming Power run-out

The carrier is always the boss's victim, so there is no case where running helps: it costs the raid
the boss's position and buys a blast everyone survives anyway. Remove the trigger, the action, both
context entries, both `creators[...]` map entries, the `TriggerNode` at `UldStrategy.cpp:196`, the
`"iron assembly overwhelming power run out action"` entry in the `encounterMovers` set
(`UldMultipliers_IronAssembly.cpp:65`), and `ULDUAR_IRON_ASSEMBLY_MELTDOWN_CLEARANCE`. Keep
`MELTDOWN_RADIUS` and the `NoteIronAssemblyCircle(SPELL_MELTDOWN, ...)` obs note — reading a trace
still needs the circle.

Same pattern as last session's Rune of Power drag-out removal, and for the same reason: a second
mover fighting the tank spot.

In `IronAssemblyChargeGuardMultiplier::GetValue`, drop the trailing
`return IronAssemblyHasOverwhelmingPower(bot) ? 0.0f : 1.0f;` and its comment. It exists only to stop
a carrier charging back into the raid it just ran from; with no run-out, a carrier closing on the
boss he is tanking is what we want.

### 2. Delete the Overwhelming Power taunt swap

`IronAssemblyOverwhelmingPowerSwapTrigger` makes the off-tank taunt the moment the active tank is
buffed. That is the swap the user ruled out, and it could never fire here anyway: it gates on
`IsMainTank` / `IsAssistTankOfIndex(0)`, group role flags that count humans, while boss assignment
moved onto the bot-only ranking in `68572b75c`. Remove the trigger, the action, both context entries,
both map entries, and the node at `UldStrategy.cpp:220`.

### 3. Let the off-tank inherit instead of contest

`DeriveIronAssemblyAssignedBoss` (`UldEncounter_IronAssembly.cpp:430-440`) already puts both ranked
tanks on Steelbreaker in the empowered phase, and `IronAssemblyTankAssignmentAction`
(`UldActions_IronAssembly.cpp:209`) taunts whenever `boss->GetVictim() != bot`. With the swap gone
that is now the only taunt path, and with two bot tanks it makes them trade the boss on every taunt
cooldown.

Gate the taunt: **taunt only when the boss's current victim is not a group tank.** A living carrier
holds to the end, and the instant Meltdown kills him the boss is on a dps, so the off-tank taunts it
back to the spot on the next tick. It also recovers the boss when a dps rips threat at the phase
transition, which is what left Steelbreaker loose in the stack in pull E.

Keep the `how = "swap"` note string — it names the branch, and `--notes ironassembly.tank` is how the
handoff gets read back.

### 4. Doc

Fold into `docs/raids/ulduar/iron-assembly.md`: the phase-3 arithmetic (12.05M restored bar, +25%
damage **and ~9.7% heal per death**, a guaranteed tank death every 36 s), that Overwhelming Power is
`GetVictim()`-targeted, 35 s, undispellable and an instakill, that Meltdown is survivable at
16-27k so nothing dodges it, and the rule that the carrier holds while the off-tank inherits on
threat. Replace the "first two bot tanks trade him on Overwhelming Power only" paragraph, which is
now wrong.

The doc is reachable from the module `CLAUDE.md`, so **invoke `/compact-docs-writer` up front**, not
as cleanup.

## Files

| Path | Change |
|---|---|
| `src/Ai/Raid/Uld/Trigger/UldTriggers_IronAssembly.{h,cpp}` | Delete both triggers (1, 2) |
| `src/Ai/Raid/Uld/Action/UldActions_IronAssembly.{h,cpp}` | Delete both actions (1, 2); gate the taunt in `IronAssemblyTankAssignmentAction` (3) |
| `src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.h` | Drop `MELTDOWN_CLEARANCE`; document the shared-Steelbreaker taunt rule |
| `src/Ai/Raid/Uld/Multiplier/UldMultipliers_IronAssembly.cpp` | Drop the run-out entry from `encounterMovers`; drop the carrier clause from the charge guard |
| `src/Ai/Raid/Uld/UldActionContext.h`, `UldTriggerContext.h`, `UldStrategy.cpp` | Remove four creators, four map entries, two `TriggerNode`s |
| `docs/raids/ulduar/iron-assembly.md` | Findings above, via `/compact-docs-writer` |

Do not touch `MovementActions.cpp`, `BurstWindowStrategy`, `TankHasHeldBoss`, the tank-spot bearings,
`MELEE_BOSS_RADIUS`, `SPREAD_RING_RADIUS`, or any other encounter's `thread_local` state.

## Verification

Static, here: `python apps/codestyle/codestyle-cpp.py`, and confirm no dangling references to the
four deleted symbols. **The module cannot be compiled in this environment (no `compile_commands.json`,
both build volumes empty) — hand the build off rather than claiming one.**

In-game, one 25-man hard-mode pull, then `tools/botobs/postmortem.py`:

1. `iron assembly overwhelming power run out action` and `... swap action` no longer appear in the
   move or act stream at all.
2. Bulwark's travel during the Steelbreaker-only phase falls from 408 yd (F) toward the ~20 yd a
   tank standing on his spot needs; Steelbreaker's travel falls from 180 yd toward zero, and his
   distance from the anchor stays near the 16 yd tank-spot radius instead of reaching 36.
3. Melee uptime within 8 yd of Steelbreaker rises from a 16% median toward 80%+.
4. Steelbreaker's health actually falls: better than F's 35% in 43 s, and E's 0.4% in 29.7 s does not
   recur.
5. `--notes ironassembly.tank` shows the `swap` branch on both ranked tanks, and the boss changes
   victim only at a carrier's death, not on a taunt cooldown.
6. Deaths in the phase drop toward the one-per-36 s floor that Overwhelming Power guarantees. Each
   one still heals ~9.7%, so this is the metric that decides whether the phase is winnable.
7. `hold burst until tank engaged` vetoes fall from 127 / 271, concentrated at the transition rather
   than spread across the phase. If they do not, that is the engine-wide `TankHasHeldBoss` reset and
   a separate decision.

If the raid holds the boss still, keeps 80% melee uptime and still cannot kill him, the phase is a
throughput problem — healing and gear — not a positioning one, and the remaining lever is the first
avoidable death.

## Step 0

Copy this document to
`docs/plans/iron-assembly-steelbreaker-phase/iron-assembly-steelbreaker-phase.PLAN.md` before
starting.
