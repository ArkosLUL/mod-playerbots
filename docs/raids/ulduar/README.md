# Ulduar (map 603)

Strategy key `ulduar`, one strategy for all 14 encounters. Cross-raid conventions are in
[../README.md](../README.md).

Files are split per boss: `Action/UldActions_<Boss>.{h,cpp}`, `Trigger/UldTriggers_<Boss>.{h,cpp}`,
with `Action/UldActions.h` and `Trigger/UldTriggers.h` as **thin umbrella headers** including the
parts, so `UldActionContext.h`, `UldTriggerContext.h` and the registration maps include one header each.

Every boss is an [encounter definition](../README.md#file-layout-and-wiring) in
`Definition/UldDefinition_<Boss>.cpp`, listed in `Definition/UldDefinitions.h`; the two contexts
register only definitions, and `UldStrategy.cpp` adds each boss's nodes and multipliers in a fixed
per-boss order, which sets node and veto order. `UldEncounterGate.h` holds the boss ids.

Each boss doc records the upstream script and DBC facts its strategy keys off. Every one of them once
silently disabled something, so re-check them after any parent-repo sync.

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

The four `Playerbots.Ulduar*HardMode` options (Vezax, Iron Assembly, XT-002, Mimiron; all default 0)
are the **single source of truth**: each `Is*HardModeActive` is a plain config read. A live server
signal on top of the config was removed: it depended on this core's scripting details, **silently
disabled a whole boss's hard-mode handling** when a signal was missing, and was invisible to the
operator who had turned the option on. **A boss gets an option only when its hard mode changes the
plan before the mechanic shows** (vapors left alive, kill order, Heart burn, Firefighter camp). Flame
Leviathan, Freya, Thorim and Yogg-Saron have none, so bots react whenever the raid triggers those. The
per-boss triggers keep their own mechanic checks (hazard nearby, debuff on the bot, add alive), and
**those are the real gate**.

**The config check lives in the detector, not scattered across triggers**: detectors are the only
coupling to server internals, and triggers stay thin.

Still dynamic, because they are *phase* or target selection rather than hard-mode detection:
`IsSteelbreakerEmpowered` (must not arm before phase 3), `GetIronAssemblyNextKillTarget`,
`GetFlameLeviathanNearestTowerHazard`. `YoggActiveKeeperMask` and the file-static
`GetBotInstanceScript` were deleted; **`YoggThorimKeeperActive` survives** and still reads
`PERSISTENT_DATA_WATCHERS_MASK` through `InstanceScript`, so that dependency is not fully gone.

Flame Leviathan's hazard callers pass `FL_TOWER_ALL`, and hazards are found by NPC entry, so destroyed
towers contribute nothing. Thorim's Frost Resistance paladin waits for `sifJoined`, latched on Sif's
first Blizzard.

### Why `GetData` is avoided

Three hard modes tried it and found it wrong:

- **Vezax** `GetData(1)` returns `lootMode == 3`, set only in `DoAction(2)` — i.e. **after** the
  Saronite Animus dies. It is a completion flag, not a live signal; the truth is "Animus (33524) alive".
- **Hodir** `GetData(3)` is the 3-minute timer flag; deliberately unused, because the buff
  optimisation is harmless past the window.
- **Mimiron** `GetData(1)` is authoritative but lives on Mimiron himself, who sits in his pod and
  **never becomes a bot attack target**, so he never enters the `"possible targets"` lists.

**Thorim's** `SPELL_SIF_CHANNEL_HOLOGRAM` (64324) is **defined but never cast** in this core; the channel
Sif actually casts is `SPELL_TOUCH_OF_DOMINION` (62507). Prefer a boss's **empower aura** where one
exists: Flame Leviathan's tower auras tell you *which* towers are up, where
`GetData(DATA_GET_TOWER_COUNT)` gives only a count.

## Burst and Bloodlust windows

`UlduarBurstWindowMultiplier` is always active inside Ulduar — no config key, matching every other
raid. Two tiers are gated separately: `allowAll` covers every burst cooldown, `allowLust` covers
`bloodlust`/`heroism` only, because a 10-minute raid cooldown wants a later window than personal
cooldowns that come back within a phase.

The [base gate](../../systems/consumables-and-burst.md#behaviour-by-target) already zeroes burst on a
non-boss target, so **adds-only phases need no gate of their own**, and **no `allowAll` rule can open
burst on an add**: that veto is final. Freya's wave adds are all `flags_extra = 0`, `rank = 1`. The
shaman lust trigger skips a bot already Sated or Exhausted; there are no Drums or Time Warp, so lust is
shaman-only.

| Boss | `allowAll` | `allowLust` | Why |
|---|---|---|---|
| Razorscale | grounded (`Z <= 440`) | permanent ground phase (`IsGroundPhaseFor`) | Zero damage taken while airborne; every landing, harpoon knockdowns included, is a real burn window, but a ~30 s knockdown is too short for a 10-minute cooldown |
| Mimiron | always | P4: all three parts selectable, VX-001 riding, after `ULDUAR_MIMIRON_PHASE4_LUST_DELAY_MS` (15 s) | P1-P3 damage counts. A beaten or self-repairing mech stays alive but unselectable, so selectable is what counts as fighting; the raid spends the P4 opening running from the Frost Bomb |
| Yogg-Saron | P2 or P3 | never | `yogg-saron phase 3 lust` owns lust, waiting for the raid to reach its spots. P1 damage lands on Sara and is wasted. Opens only on the Brain (P2) and Yogg (P3), both in `BossTakesNoVictim`: before that the base gate held every bot cooldown for a whole 663 s pull. Read from the boss state, not sight: the brain level is 88-150 yd from Yogg, past `SightDistance`, and the fall-through lusted in P2 |
| Assembly of Iron | always | exactly one member alive | They resurrect each other; also covers the hard mode, since Steelbreaker-last means the survivor is empowered |
| Freya | same as `allowLust` | no `SPELL_ATTUNED_TO_NATURE` 62519, **or** HP ≤ 25% | 150 stacks of +8% healing received, so damage lands only in the final phase; the adds that strip it are not boss-flagged |
| Thorim | arena floor (`Z <= 429.6`) | same | Largely redundant, but cheap insurance against a stray lust while he is immune |
| Hodir | — | first Toasty Fire, or 30 s in | A definition rule (`hodir lust hold`), not this multiplier. Cast at the pull it ran on a no-fire opener and out before the freeze; see [hodir.md](hodir.md) |
| XT-002 | the exposed Heart | same | `XT002BurstWindowMultiplier`, a definition multiplier: bursting the Heart kills it and starts hard mode, so the config decides |
| Vezax | — | hard mode: the Animus summon or a vapor death | A definition rule (`vezax hard mode hold`, Starfall too) |
| Algalon, Ignis, Auriaya, Kologarn, Flame Leviathan | — | — | No change; the pull is the right window |

**Freya and Hodir are the only bosses with a fallback release**: `FREYA_LUST_FALLBACK_PCT = 25.0f`
keeps lust from being held forever if the aura read ever misses, and Hodir's 30 s covers a pull with no
fire. Every other window is on the mandatory path to the kill.

**Never call `RazorscaleBossHelper::UpdateBossAI()` from a multiplier**: it side-effects into
`AssignRolesBasedOnHealth()`, which reassigns the raid's main tank. Read Z straight off the target
sweep instead. Yogg is resolved by entry (`YoggSaronNearestCreature`), not `"find target"`, because he
is not reliably on a bot's threat list. **Sara is the worst case of that rule:** friendly all of P1 and
damaged only by Guardian Shadow Nova, so no bot ever holds threat on her, and the `"find target"`
lookup gating every Yogg trigger left all 21 of them dead through two wipes. See
[yogg-saron.md](yogg-saron.md).

**The `"possible targets no los"` sweep is capped at `Playerbots.SightDistance` (100 yd)**, and
Razorscale's second flight point `RazorFlightPos2` (619.1, -238.1, 475.2) sits past that from most of
the raid. `EvaluateWindow` therefore falls back to `"find target"` for her: `DoZoneInCombat()` on her
first flight point puts the whole raid on her threat list, so that read works at any range. Without it
the function reaches its `return {}`, and `BurstWindow`'s member initialisers are both `true`, so the
gate opens instead of closing. That fallback is ungated, so it runs in every other Ulduar fight too
([cost, open](../../engine/raid-mechanics-lessons.md#what-a-strategy-costs-per-raid)).

Open question that cannot be answered from source: whether the Mimiron mechs and the Assembly council
members are `IsDungeonBoss()`-flagged. If they are not, lust never fires on them and those two gates
are inert.

## Threat redirect

Without a veto the generic main-tank node fires unconditionally, including on the encounters where the
strategy deliberately puts a second tank, an add tank or a swap tank on what the DPS is hitting
([rubric](../README.md#threat-redirect--the-rubric)). Freya, Hodir, Iron Assembly, XT-002 and Mimiron
also have dedicated redirect actions; Mimiron's feeds the main tank through phase 1, the one phase a
redirect helps.

**Veto (`UldThreatRedirectMultiplier`) — the main tank is actively wrong:**

| Boss | Why | Gated on |
|---|---|---|
| Mimiron | Four phases, each a different creature with a fresh threat table; phase 3 has two tankable units split MT/AT0 | 33432 / 33651 / 33670 |
| Thorim | Raid splits into arena and gauntlet squads with a tank each, plus an Unbalancing Strike swap | boss present |
| Algalon | Phase Punch forces a swap on a stack timer between a swap pair: two bot tanks, else MT + AT0 | boss present |
| Razorscale — **outside the permanent ground phase** | The MT holds nothing while she flies; Dark Rune adds belong to assist tanks, and a harpoon knockdown above 50% puts her on the floor with nobody tanking her. Only the permanent ground phase is single-tank and the generic node *correct* there | `IsFlyingPhaseFor` |
| Freya | The add tank is the sink whenever the Snaplasher or Conservator is up, and `freya redirect threat` owns the choice | boss present |

**Do not veto** — the boss is main-tank-held all fight, so the generic node is right for the primary
target and the only gain would be redirecting *adds*: Auriaya, Kologarn, Yogg-Saron, Ignis.
**No** — Vezax (one tank, no swap, no reset), Flame Leviathan (vehicle combat, neither spell castable).

XT-002 is covered by `XT002TargetGuardMultiplier`, which vetoes for the whole encounter because the
main tank is the wrong sink while a Pummeller is out.

Iron Assembly leaves the veto off on purpose: its own redirect node at `ACTION_RAID + 2` outranks the
class-generic nodes (`ACTION_HIGH + 7`) and spends the cooldown on whoever tanks the kill target, so they
only fire with no kill target up.

**Razorscale's phase comes from the static `IsFlyingPhaseFor(Unit*)`**, which takes the unit the
multiplier already holds, so `UpdateBossAI()` (which reassigns the main tank) never runs. It holds
outside `IsGroundPhaseFor` (Z ≤ 440, under 50%, no stun aura), harpoon knockdowns included.

A pull-window redirect uses BT's stateless `boss->GetHealthPct() > 95.0f`, not a combat clock: it needs
no new state and doubles as the fresh-spawn / fresh-phase test. Use `GetUldUnitByEntry`, not
`"find target"`, for multi-tank detection: a bot parked on boss A never resolves boss B.

Hodir subclasses `RaidRedirectThreatAction` as `HodirRedirectThreatAction`, feeding whichever tank
currently holds him. The proc-aura id `35079` still sits in nine per-raid helper headers including
Ulduar's `UldData.h`; `SPELL_MISDIRECTION_PROC` on the shared header is the one to converge on.

## Normal-mode gaps still open

Sev-1 fails **even with the raid cheat on**:

| Boss | Gap |
|---|---|
| **Razorscale** | Dark Rune Watcher/Guardian adds have no interrupt (focus and the Flame Breath cone are handled) |
| **Freya** | Storm Lasher (Stormbolt, Lightning Lash) and Ancient Water Spirit (Tidal Wave) casts are not interrupted |

**Sev-2 CHEAT-ONLY** — works in the default config, breaks silently if `BotCheats` drops `raid`:
Yogg Crusher/Constrictor tentacles, illusion-room adds and P2 movement (cheat instakill / teleport).
Ominous Clouds are off this list: `yogg-saron phase 1 spacing` dodges them without the cheat.

**No boss reuses `RazorscaleBossHelper`'s role-swap machinery for a real tank swap**: each rolls its own
detector instead (Thorim on the Unbalancing Strike debuff, Kologarn on Crunch Armor stacks, Hodir on
Frozen Blows). There is no enrage-timer awareness anywhere, which blocks every hard-mode kill-timer
requirement.

## Test roster

A second 25-man roster exists only for strategy testing: accounts **ULD1/ULD2/ULD3** (password
`1234`, linked to ARKOS), cloned with `.pdump` from the core raid, so talents, glyphs, pets and
progression match. Tanks are two prot paladins and a blood DK, to pick the setup per fight.

| Role | Bots |
|---|---|
| Tank | Rampart, Bastion (prot paladin), Gravewarden (blood DK) |
| Heal | Dawnmender (holy paladin), Tidecaller (resto shaman), Wildbloom (resto druid), Penance (disc priest) |
| Melee | Verdict (ret), Stormfist (enh), Sunderer (arms), Bloodrage (fury), Venomblade (assassination), Cutthroat (combat), Rimefang (frost DK) |
| Ranged | Deadeye (survival), Beastcaller (BM), Spellsurge (arcane), Pyroclast (fire), Starfall (balance), Lavaburst (elemental), Mindflay (shadow), Demonbound (demo), Chaosbolt (destro), Soulrot (affliction) |

Gear is Ulduar drops and Conqueror's T8.5 only: ilvl 226, weapons 232 (Ulduar has no
226 weapons), **no gems or enchants**, nothing in bags. `maintenance` and `autogear` would add both
(`AltMaintenanceGemsEnchants = 1`), so don't run them on these bots.
