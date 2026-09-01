# Ulduar (map 603)

Strategy key `ulduar`, one strategy for all 14 encounters. Cross-raid conventions are in
[../README.md](../README.md).

Files are split per boss: `Action/UldActions_<Boss>.{h,cpp}`, `Trigger/UldTriggers_<Boss>.{h,cpp}`,
with `Action/UldActions.h` and `Trigger/UldTriggers.h` kept as **thin umbrella headers** that include
the parts — so `UldActionContext.h`, `UldTriggerContext.h` and every registration map needed no edits
when the monolith was split.

| Boss | File |
|---|---|
| Flame Leviathan | [flame-leviathan.md](flame-leviathan.md) |
| Razorscale | [razorscale.md](razorscale.md) |
| Ignis | [ignis.md](ignis.md) |
| XT-002 | [xt002.md](xt002.md) |
| Assembly of Iron | [iron-assembly.md](iron-assembly.md) |
| Kologarn | [kologarn.md](kologarn.md) |
| Auriaya | [auriaya.md](auriaya.md) |
| Hodir | [hodir.md](hodir.md) |
| Thorim | [thorim.md](thorim.md) |
| Freya | [freya.md](freya.md) |
| Mimiron | [mimiron.md](mimiron.md) |
| Vezax | [vezax.md](vezax.md) |
| Yogg-Saron | [yogg-saron.md](yogg-saron.md) |
| Algalon | [algalon.md](algalon.md) |

## Hard modes are declared by config, not detected

**Ulduar hard modes are a raid choice, not the 10/25 heroic flag**, so the "heroic comes free via a
spell-id predicate" trick from other raids does not apply. Bots follow the **follower model**: they
never *trigger* a hard mode, they react once the raid has. In an all-bot raid nobody triggers one —
accepted.

Detection started as "config **and** a live server signal", and that second half turned out to be the
problem: it depended on this core's scripting details, it **silently disabled a whole boss's
hard-mode handling** when a signal was missing, and it was invisible to the operator who had
explicitly turned the option on. The eight `AiPlayerbot.Ulduar*HardMode` options (all default 0) are
now the **single source of truth** — each `Is*HardModeActive` is a plain config read. The per-boss
triggers keep their own mechanic checks (hazard nearby, debuff on the bot, add alive), and **those
mechanic checks are the real gate**; the detectors were a redundant second one.

**The config check lives in the detector, not scattered across triggers** — detectors are the only
coupling to server internals, and triggers stay thin. A Vezax trigger that bypassed the detector by
calling the raw lookup directly had to be fixed for exactly this reason.

Still dynamic, because they are *phase* or target selection rather than hard-mode detection:
`IsSteelbreakerEmpowered` (must not arm before phase 3), `GetIronAssemblyNextKillTarget`,
`GetFlameLeviathanNearestTowerHazard`. `YoggActiveKeeperMask` and the file-static
`GetBotInstanceScript` were deleted; **`YoggThorimKeeperActive` survives** and still reads
`PERSISTENT_DATA_WATCHERS_MASK` through `InstanceScript`, so that dependency is not fully gone.

Behaviour worth knowing: Flame Leviathan's mask claims all four towers, but hazards are found by NPC
entry, so destroyed towers contribute nothing.

### Why `GetData` is avoided

Three separate hard modes tried it and three found it wrong:

- **Vezax** `GetData(1)` returns `lootMode == 3`, set only in `DoAction(2)` — i.e. **after** the
  Saronite Animus dies. It is a completion flag, not a live signal. The real truth is simply "Animus
  (33524) alive".
- **Hodir** `GetData(3)` is the 3-minute timer flag; deliberately unused, because the buff
  optimisation is harmless past the window.
- **Mimiron** `GetData(1)` is authoritative but lives on Mimiron himself, who sits in his pod and
  **never becomes a bot attack target**, so he never enters the `"possible targets"` lists.
- **Thorim's** `SPELL_SIF_CHANNEL_HOLOGRAM` (64324) is **defined but never cast** in this core — the
  channel Sif actually casts is `SPELL_TOUCH_OF_DOMINION` (62507).

Prefer a boss's **empower aura** where one exists: Flame Leviathan's tower auras tell you *which*
towers are up, where `GetData(DATA_GET_TOWER_COUNT)` gives only a count.


## Core behaviours the strategies key off

Upstream script and DBC facts our code now depends on, with the behaviour each one drives. Every one
of them silently disabled something before it was accounted for, so re-check them after any
parent-repo sync.
They are recorded in the per-boss files above.

## Burst and Bloodlust windows

`UlduarBurstWindowMultiplier` is always active inside Ulduar — no config key, matching every other
raid. Two tiers are gated separately: `allowAll` covers every burst cooldown, `allowLust` covers
`bloodlust`/`heroism` only, because a 10-minute raid cooldown wants a later window than personal
cooldowns that come back within a phase.

Burst only ever fires while the bot's current target is boss-flagged — `IsDungeonBoss() ||
isWorldBoss()` in `HoldBurstUntilTankEngagedMultiplier`, under `AiPlayerbot.BurstOnBossOnly` (default
on). So **adds-only phases need no gate of their own**, and conversely **no `allowAll` rule can open
burst on an add**: that veto is final. Freya's wave adds are all `flags_extra = 0`, `rank = 1`. There
is no Sated/Exhaustion check anywhere, and no Drums or Time Warp — lust is shaman-only.

| Boss | `allowAll` | `allowLust` | Why |
|---|---|---|---|
| Razorscale | grounded (`Z <= 440`) | same | Zero damage taken while airborne; every landing, harpoon knockdowns included, is a real burn window |
| Mimiron | always | all three mechs alive | P1-P3 damage counts; all three up is P4, the enrage burn |
| Yogg-Saron | P2 or P3 | P3 | P1 damage lands on Sara and is wasted |
| Assembly of Iron | always | exactly one member alive | They resurrect each other; also covers the hard mode, since Steelbreaker-last means the survivor is empowered |
| Freya | same as `allowLust` | no `SPELL_ATTUNED_TO_NATURE` 62519, **or** HP ≤ 25% | 150 stacks of +8% healing received, so damage lands only in the final phase; the adds that strip it are not boss-flagged |
| Thorim | arena floor (`Z <= 429.6`) | same | Largely redundant, but cheap insurance against a stray lust while he is immune |
| Hodir, Vezax, Algalon, Ignis, Auriaya, Kologarn, Flame Leviathan | — | — | No change; the pull is the right window |

**Freya is the only boss with a fallback release** — `FREYA_LUST_FALLBACK_PCT = 25.0f` keeps lust
from being held forever if the aura read ever misses. Every other window is on the mandatory path to
the kill.

**Never call `RazorscaleBossHelper::UpdateBossAI()` from a multiplier** — it side-effects into
`AssignRolesBasedOnHealth()`, which reassigns the raid's main tank. Read Z straight off the target
sweep instead. Yogg is resolved with `FindNearestCreature`, not `"find target"`, because he is not
reliably on a bot's threat list.

**The `"possible targets no los"` sweep is capped at `AiPlayerbot.SightDistance` (100 yd)**, and
Razorscale's second flight point `RazorFlightPos2` (619.1, -238.1, 475.2) sits past that from most of
the raid. `EvaluateWindow` therefore falls back to `"find target"` for her — `DoZoneInCombat()` on her
first flight point puts the whole raid on her threat list, so that read works at any range. Without
it the function reaches its `return {}`, and `BurstWindow`'s member initialisers are both `true`, so
the gate opens instead of closing.

Open question that cannot be answered from source: whether the Mimiron mechs and the Assembly council
members are `IsDungeonBoss()`-flagged. If they are not, lust never fires on them and those two gates
are inert.

## Threat redirect

Before this work **no WotLK raid had either a redirect action or a veto**, so the generic main-tank
node fired unconditionally — including on the encounters where the strategy deliberately puts a
second tank, an add tank or a swap tank on what the DPS is hitting. This is the veto slice only; no
Ulduar boss has a dedicated redirect action yet.

**Veto — the main tank is actively wrong:**

| Boss | Why | Gated on |
|---|---|---|
| Iron Assembly | Three bosses, one tank each, plus a Fusion Punch swap | boss entries |
| Mimiron | Four phases, each a different creature with a fresh threat table; phase 3 has two tankable units split MT/AT0 | 33432 / 33651 / 33670 |
| Thorim | Raid splits into arena and gauntlet squads with a tank each, plus an Unbalancing Strike swap | boss present |
| Algalon | Phase Punch forces an MT ↔ AT0 swap on a stack timer | boss present |
| Razorscale — **airborne only** | The MT holds nothing while she flies; Dark Rune adds belong to assist tanks. Ground phases are single-tank and the generic node is *correct*, so this is phase-gated, not blanket | boss Z vs 440 |
| Freya | The add tank is the sink whenever the Snaplasher or Conservator is up, and `freya redirect threat` owns the choice | boss present |

**Do not veto** — the boss is main-tank-held all fight, so the generic node is right for the primary
target and the only gain would be redirecting *adds*: Auriaya, Kologarn, Yogg-Saron, Ignis.
**No** — Vezax (one tank, no swap, no reset), Flame Leviathan (vehicle
combat, neither spell castable).

XT-002 was already covered by `XT002TargetGuardMultiplier`, which vetoes for the whole encounter
because the main tank is the wrong sink while a Pummeller is out.

Two load-bearing details: the multiplier `dynamic_cast`s to the two **concrete** redirect actions
only, never the shared `BuffOnMainTankAction` base; and **Razorscale's phase is read straight off the
unit's Z** rather than through `IsFlyingPhase()`, because the helper needs `UpdateBossAI()` first and
that reassigns the main tank. The temporary harpoon knockdowns count as grounded, which is intended —
she is tankable then, and a redirect during the knockdown is what puts her back on the MT.

**No pull-window helper exists outside Naxx, RS and SWP.** If dedicated actions follow, use BT's
stateless `boss->GetHealthPct() > 95.0f` idiom rather than building a combat clock — it needs no new
state and doubles as the fresh-spawn / fresh-phase test. And use `GetFirstAliveUnitByEntry`, not
`"find target"`, for multi-tank detection: a bot parked on boss A never resolves boss B.

`RaidRedirectThreatAction` lives in `Raid/RaidRedirectThreat.{h,cpp}`; Hodir subclasses it as
`HodirRedirectThreatAction`, feeding whichever tank currently holds him. The proc-aura id `35079`
still sits in nine per-raid helper headers including `UldBossHelper.h`; `SPELL_MISDIRECTION_PROC` on
the shared header is the one to converge on.

## Normal-mode gaps still open

From the Sev-1/Sev-2 audit. Sev-1 fails **even with the raid cheat on**:

| Boss | Gap |
|---|---|
| **Razorscale** | Dark Rune Watcher/Guardian adds have no interrupt (focus and the Flame Breath cone are handled) |
| **Freya** | Storm Lasher (Stormbolt, Lightning Lash) and Ancient Water Spirit (Tidal Wave) casts are not interrupted |
| **Auriaya** | `AuriayaEncounterActive` is presence-only (`GetAuriaya(botAI) != nullptr`), so bots pull her on sight from up to 100 yd. Kologarn had the same defect and now gates on `IsInCombat()` |

**Sev-2 CHEAT-ONLY** — works in the default config, breaks silently if `BotCheats` drops `raid`:
Yogg Ominous Clouds, Crusher/Constrictor tentacles, illusion-room adds and P2 movement
(cheat instakill / teleport).

Structural notes: **no boss reuses `RazorscaleBossHelper`'s role-swap machinery for a real tank
swap** — each rolls its own detector instead: Thorim on the Unbalancing Strike debuff, Kologarn on
Crunch Armor stacks, Hodir on Frozen Blows. There is no enrage-timer awareness anywhere, which
blocks every hard-mode kill-timer requirement.
