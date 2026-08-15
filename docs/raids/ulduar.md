# Ulduar (map 603)

Strategy key `ulduar`, one strategy for all 14 encounters. Cross-raid conventions are in
[README.md](README.md).

Files are split per boss: `Action/UldActions_<Boss>.{h,cpp}`, `Trigger/UldTriggers_<Boss>.{h,cpp}`,
with `Action/UldActions.h` and `Trigger/UldTriggers.h` kept as **thin umbrella headers** that include
the parts — so `UldActionContext.h`, `UldTriggerContext.h` and every registration map needed no edits
when the monolith was split.

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

Behaviour worth knowing: with the Hodir option on, bots run the DPS-race behaviour on **every** Hodir
kill — that is the intended semantics. Flame Leviathan's mask claims all four towers, but hazards are
found by NPC entry, so destroyed towers contribute nothing.

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

## Per-boss hard modes

### Vezax — the reference implementation

Hard mode = leave Saronite Vapors alive until the **Saronite Animus (33524)** spawns; Vezax gains an
invulnerable Saronite Barrier until it dies. Everyone switches target and kills it (no RTI mark —
each bot `Attack()`s directly); ranged and healers move out of its Profound Darkness (63420).
`ULDUAR_VEZAX_PROFOUND_DARKNESS_RADIUS = 15.0f` is a conservative guess — the radius is DBC, not in
the script.

### Assembly of Iron

Each survivor gains a phase whenever a council member dies — the dying boss casts Supercharge (61920)
and each `SpellHit` calls `UpdatePhase()`, so **the last one alive reaches `_phase == 3`**. The hard
mode is the kill order: Steelbreaker last.

**Steelbreaker has no `GetData` override and `_phase` is private**, so empowerment is inferred:
*Steelbreaker alive AND Molgeim dead AND Brundir dead*.

Empowered kit: Fusion Punch (61903, heavy Nature DoT on the tank), Static Disruption (61911, random
non-melee + ~5 yd splash), **Overwhelming Power (64637 — instakills the tank on expiry unless
dispelled)**, and Electrical Charge (61902, a stacking damage nova gained each time a player dies).

Bots skull-mark Brundir → Molgeim → Steelbreaker and the off-tank taunts Steelbreaker off a tank
carrying Fusion Punch or Overwhelming Power. **Electrical Charge is deliberately not handled
positionally** — it is a stacking damage buff, a healer/uptime problem rather than a movement one.
Meltdown is moot in the empowered phase, since Molgeim (who summons the elementals) is dead by then.

### Flame Leviathan

The whole fight is from vehicles, and nothing about it behaves like a normal encounter. Conclusions
here carry their derivation, because three of them were re-derived *wrongly* in one session: the tar
snare "working", Blaze hurting the raid, and pyrite crates being cosmetic.

**Spellbooks** are `creature_template_spell`, not guesswork, and **every one of these spells is
instant** (`CastingTimeIndex 1`). `CanCastVehicleSpell` only refuses cast-time spells while the base
is moving, so on this encounter driving and casting compose freely — there is no kite-or-DPS choice.

| Vehicle | Entry | Spells (energy cost) |
|---|---|---|
| Salvaged Siege Engine | 33060 | Ram 62345 (40) · Electroshock 62522 (20, 10s) · Steam Rush 62346 (40, 15s) |
| Salvaged Siege Turret | 33067 | Fire Cannon 62358 (20) · Anti-Air Rocket 62359 (10) · Shield Generator 64677 (0, 60s) |
| Salvaged Chopper | 33062 | Sonic Horn 62974 (20) · Tar 62286 (0, 15s) · Speed Boost 62299 (50) |
| Salvaged Demolisher | 33109 | Hurl Boulder 62306 (0) · Hurl Pyrite Barrel 62490 (5) · Ram 62308 (0) |
| Demolisher Mechanic Seat | 33167 | Mortar 62634 (0) · Anti-Air Rocket 64979 (0) · Grab Crate 62479 (0) · Increased Speed 62471 (25) |

#### Gunners can steer, which is why nothing gates on `IsInVehicle(true)`

Seat 0 of **both** the turret (33067) and the mechanic seat (33167) carries `CAN_CONTROL`, so
`MovementAction::MoveTo` passes its `seat->CanControl()` check for a *gunner* and calls `DoMovePoint`
on the turret creature bolted into seat 7. Movement is gated on `vehicleBase->GetEntry()` instead.
No seat a bot can occupy has `CAN_ATTACK` — that flag is on the accessory seats, which hold NPCs — so
`CastSpellAction::isUseful` already blocks class spells and no targeting-suppression multiplier is
needed. Siege engine seats 1 and 2 have **no flags at all**; the chopper rear seat and demolisher
seat 3 are `ENTER_EXIT` only.

`instance_ulduar.cpp` spawns **2 of each vehicle in 10-man, 5 in 25-man** at `x = 119.8`, west of the
arena: exactly 10 / 25 usable slots, **zero slack**.

#### The demolisher runs on pyrite, not energy

`Creature::Regenerate` returns early without `UNIT_FLAG2_REGENERATE_POWER`. The siege engine, turret
and chopper have it (+20 per 2s); **the demolisher and its mechanic seat have `unit_flags2 = 0` and
never regenerate**. Max pool is 100 for all of them.

So a full demolisher is **20 barrels**, then free Hurl Boulder forever unless crates arrive. Crates:
80 `Mechanolift 304-A` (33214) spawns each carry a `33218 Pyrite Safety Container` in seat 1; shoot
one down and the container falls, `MovementInform` summons `33189 Liquid Pyrite`, and the demolisher
gunner's Grab Crate runs `62496 → 62473 Reload Ammo` = `SPELL_EFFECT_ENERGIZE` **+25 to the
demolisher**. Anti-Air Rocket reaches 1000 yd and lifts spawn far outside the arena, so targets are
restricted to the arena box — a crate that lands across the zone is a crate nobody drives to.

The mechanic seat's own 100 is **never** refilled (`62473` targets the demolisher, not the seat), so
its Increased Speed is capped at four casts for the fight. Ample: a given demolisher takes Pursued
once or twice.

`spell_vehicle_grab_pyrite`'s **chopper** branch loads a crate into the chopper's rear seat to ferry
to other vehicles. That is the retail supply line and no bot code uses it — see the gap list.

#### Tar cannot slow him, and Blaze cannot hurt us

`Pool of Tar` 33090 is **faction 1965, the same faction as the boss** (our vehicles are 2105).
`SummonProperties 64` is `Category 0 / Faction 0 / Flags 0` and the pool is not a vehicle, so
`TempSummon::InitStats` never swaps in the summoner's faction. Both effects target
`TARGET_UNIT_SRC_AREA_ALLY`, so they only ever hit *his* side:

| Spell | Effect | Hits |
|---|---|---|
| `62287` Tar snare, 50 yd | `MOD_DECREASE_SPEED -75%`, `Mechanic 11` | His faction — but **blocked on the boss**: `creature_immunities` row `-362` masks SNARE. Adds are not immune. |
| `62290` Burning Tar, 15 yd | 5400 fire per second, 45s | Him and his adds. **Never our vehicles.** |

`62286 Tar` drops the pool at `TARGET_DEST_CASTER_BACK`, 9 yd **behind** the chopper, so it only
lands in his path while he is behind us. Ignition needs a fire-school spell to hit the pool, which is
`NOT_SELECTABLE` and cannot be aimed at — so it happens incidentally, from **Mortar** (school 4) and
**Blue Pyrite** (school 68 = Arcane|Fire, 20 yd enemy-area). **Fire Cannon is school 1, physical,
despite the name**, and never ignites anything.

#### Boss mechanics

- **Flame Vents 62396 is a 10s channel** — `AttributesEx = 200` carries `0x40`
  (`SPELL_ATTR1_CHANNELED_2`) — ticking 63847 for 15000 in a 500 yd radius, so it is not dodgeable.
  `EVENT_VENT` repeats every 20s. `boss_flame_leviathan::SpellHit` maps Electroshock to
  `InterruptNonMeleeSpells`, which always cancels a channel, so **the interrupt is real**. He is
  `MECHANIC_INTERRUPT`-immune, but only Electroshock's effect 1 carries that mechanic; effect 2
  (8489 damage) lands, the spell hits, and the script hook fires.
- **Ram, Electroshock and Sonic Horn are `TARGET_UNIT_CONE_ENEMY_104`.** `Spell::CheckRange` returns
  OK immediately for `RangeEntry->ID == 1` and waves all three through, so the **effect radius is the
  real limit**: Ram 18 yd, Electroshock 25 yd, Sonic Horn 35 yd. The interrupt rotation gates on
  range for exactly this reason — a siege engine parked across the arena would otherwise win the
  ranking and land nothing.
- **Pursued 62374** picks a random vehicle every 31s and lasts 35s; he then drives at it and uses
  `Battering Ram 62376` inside 15 yd. The aura is a far better signal than `GetVictim()`.
- **He accelerates all fight.** `Gathering Speed 62375` is `MOD_SPEED_ALWAYS +5%`, **stacks to 20**,
  600s, re-applied every 15s and cleared only on reset. Against `speed_run`: he goes 5.0 → **10.0**
  yd/s, a siege engine or demolisher is a flat 7.0, a chopper 14.0. He out-runs a siege engine after
  roughly two minutes, which is why the kite path has to be efficient and why Steam Rush, Increased
  Speed and Speed Boost carry the late fight.
- **`"attackers"` usually does not contain him.** `AttackersValue` walks the *bot player's*
  `GetThreatenedByMeList()`, and threat here belongs to the vehicle creature. He is resolved by entry
  instead; `"attackers"` is kept only for picking adds.
- **Steam Rush 62346** is `CHARGE_DEST` at `TARGET_DEST_CASTER_FRONT`: a forward dash along our own
  facing, so it only escapes when he is already behind us.
- **Shield Generator 64677** is `SCHOOL_ABSORB` **15** for 5s on a 60s cooldown. Wired because it is
  free, not because it matters.
- **Hurl Pyrite Barrel needs no ammo** — no `CasterAuraSpell`, 5 energy is the only gate. It triggers
  `62489` (54000 AoE) which applies stacking `68605 Blue Pyrite` (10s, 10 stacks). Each demolisher
  carries its **own** aura instance, so the refresh check reads the caster-scoped overload.

#### Vehicle spells that must be self-cast

Tar, Steam Rush, Speed Boost, Increased Speed and Shield Generator are aimed at the caster or its own
vehicle. They cannot go through `CanCastVehicleSpell` — a self-cast returns `SPELL_FAILED_BAD_TARGETS`,
which that helper does not tolerate — so they are gated on cooldown and power directly. They must
also be passed the vehicle base as their target: `CastVehicleSpell` turns the vehicle to face any
*other* target first, which would aim Steam Rush straight at him and drop the tar pool on the wrong
side of the chopper.

#### One action owns all movement

`FlameLeviathanDriveAction` is the only thing that steers a vehicle, with internal precedence:
kite when Pursued → clear a hard-mode hazard → detour to a crate below the pyrite reserve → run the
tar lead → hold station. Hazard clearance used to be a separate action at a higher priority; that is
the two-owner bounce with the priorities swapped, so it was folded in rather than re-ordered. The
action is wired to two trigger nodes, `drive urgent` at `ACTION_RAID + 3` and the routine one at
`+0.5` — the engine caches actions by name, so both nodes drive one instance and one latched
destination.

Positioning: siege engines and non-lead choppers hold his **rear arc**; demolishers hold a 50 yd band
and never close; one designated chopper (lowest guid, not currently Pursued) runs *ahead* of him,
back turned, so its tar pool lands in his path.

`FlameLeviathanVehicleMovementMultiplier` zeroes every other `MovementAction` while a bot is on an FL
vehicle, exempting only the drive action, boarding and `LeaveVehicleAction` — and it stays **inert
until he is engaged**, or the raid could never drive into the arena. The Void Reaver risk applies:
once it is live, a silently failing drive action means vehicles stand still all fight.

#### The kite is a wall loop, not a corner run

`FlameLeviathanKiteRing()` builds eight nodes from the four `NPC_FREYA_WARD_TARGET` spawn points —
`(159.4, 64.1)`, `(382.9, 74.0)`, `(374.0, -141.0)`, `(157.7, -140.3)` — inset 15 yd off the walls
with each corner chamfered into two nodes 35 yd out. The direction is latched per instance on the
first Pursued and **never reversed**; nodes within 50 yd of him are skipped so the ring never routes
through him; and the vehicle **advances on approach at 30 yd, never on arrival**.

That last rule is the whole point. The old kite drove corner to corner and only switched once within
5 yd, so the vehicle buried itself in the corner while he cut the diagonal — and its advance was also
conditioned on him being within 50 yd, which made progress depend on already being caught.

#### Hard mode

At pull, `ActivateTowers()` adds one empower aura per surviving tower and schedules that tower's
periodic ground event:

| Tower | Boss aura | Ground NPC | Hazard |
|---|---|---|---|
| Storm | 65076 | 33364 (8 spawn) | Static lightning strikes at 8 fixed marks, ~5s telegraph |
| Flame | 65075 | 33369 | Escort-path **moving** fire trail, drops fire every 2s |
| Frost | 65077 | 33108 (2 spawn) | **Chases** a random player, stuns, drops frost AoE at the catch point |
| Life | 64482 | 33367 | Spawns attacking adds — kill, not dodge |

Life tower is skipped: its adds are already covered by the vehicle's kill-nearest-attacker loop.

#### Known gaps

- **Boarding depends on the raid leader.** `FlameLeviathanVehicleNearTrigger` returns false unless
  `master->GetVehicle()` — the Oculus `GroupFlyingTrigger` defect, where one human who has not
  mounted freezes the whole raid.
- **No chopper pyrite ferry**, so crates only reach a demolisher that drives to them itself.
- **No seat-shortfall fallback.** With zero slack, a bot that loses a boarding race is left on foot.
- **`PlayerbotAI::CastVehicleSpell(uint32, float, float, float)` is declared and never defined**
  (`PlayerbotAI.h:558`), so there is no ground-targeting and tar cannot be ignited deliberately.

### Thorim

Sif is summoned every pull and normally channels, then despawns after the 150s dominion timer. If the
raid clears the gauntlet fast enough she joins instead and casts Frostbolt Valley (raid-wide,
unavoidable — healed through), Blizzard (62577 → moving `NPC_SIF_BLIZZARD` 32879, respawned every
~15s) and Frost Nova (62605, teleport then point-blank).

**Detector: Sif (33196) alive AND `GetPositionZ() < 429.6`** — she spawns at the throne and only
`NearTeleportTo`s onto the arena floor when she joins. This reuses the same floor threshold the
normal-mode Thorim strategy already uses.

### Hodir

**Hodir's hard mode is not a new hazard** — it is the "Rare Cache of Winter" 3-minute timed kill
(`EVENT_HARD_MODE_MISSED` at 180s). The fight is otherwise identical; the raid just needs more DPS
and fewer deaths. The way to beat it is exploiting the **friendly helper NPCs**, which spawn
flash-frozen and must be freed first:

- **Storm Cloud** (65123, difficulty-mapped at runtime) lands on a random raider every 30s; while
  that carrier stands near allies it triggers Storm Power, the fight's #1 DPS multiplier.
- **Toasty Fire** (`NPC_TOASTY_FIRE` 33342, aura 62821) prevents Biting Cold stacks and grants
  Flash-Freeze exemption — legitimate non-cheat mitigation for a mechanic that previously had only a
  cheat handler.
- **Starlight** (druid haste zone) was dropped by user decision — area-aura detection is fiddlier and
  the gain is lower.

**Priority interaction that matters**: the Flash-Freeze icicle move sits at `ACTION_RAID + 1` and
must strictly outrank helper-freeing at `ACTION_RAID`, so a bot never leaves line-of-sight cover to
chase a re-frozen helper mid-cast.

Storm Cloud spreading moves to the **nearest ally**, not the raid centroid, which could be an empty
midpoint between two groups. Both radii (5 yd Toasty Fire, 10 yd Storm Cloud stack) are DBC guesses
to confirm in-game.

### Freya

Hard mode = Elders left alive at pull (Brightleaf 32915 / Stonebark 32914 / Ironbranch 32913). Per
living Elder, Freya gains an extra ability: Iron Roots (62862), Unstable Sun Beam (62450), or Ground
Tremor (62437, raid-wide knockback — not handled, undodgeable).

**Critical: the empower events are scheduled once at pull and repeat unconditionally — they keep
firing for the whole fight even after the Elder dies.** So hard-mode reactions must key off the
**hazard world object**, never off an Elder still being alive. Killing Elders never removes the
empower and never costs the achievement, because the Elder count is locked at pull.

The two object types differ in a way that matters:

- **Root creatures 33088 / 33168 are selectable** (unit_flags 0), so a trapped bot targets and kills
  its own root to free itself.
- **Beam stalkers 33170 / 33050 are non-selectable** (`0x2000000`), so they never appear in
  attack-target lists — find them by scanning `"nearest npcs"`.

Breaking Iron Roots sits at `ACTION_RAID + 3`, above the Sun Beam dodge at `+2`, because **a rooted
bot cannot move**, so it must free itself before it can step out of anything. The beam dodge flees
the centroid of the in-range beam cluster, not the single nearest beam.
`ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS = 12.0f` is a DBC guess.

### Mimiron — Firefighter

A player presses the Big Red Button before the pull; `_hardmode` is set at activation and never
cleared. Detection uses `SPELL_EMERGENCY_MODE (64582)`, the empower aura applied to whichever mech is
currently active — those mechs *are* valid attack targets, unlike Mimiron himself.

Hazards: persistent ground fire (`NPC_FLAMES_INITIAL` 34363 → `NPC_FLAMES_SPREAD` 34121, aura 64561)
that creeps toward the nearest player all fight, and the VX-001 **Frost Bomb** (`NPC_FROST_BOMB`
34149). Both flame nodes are non-selectable trigger creatures, so they are found by scanning
`"nearest npcs"` — the same idiom as Freya's beams. Bots avoid the bomb *creature*, so no spell id is
needed and 10/25 are covered identically.

**Correction to the master plan**: Emergency Fire Bots (34147) are **friendly, non-combat fire
extinguishers**. They never enter combat with players, never heal or repair Mimiron, and only run to
flame nodes and cast Water Spray to put fires out. They are **not** kill targets.

### Yogg-Saron — reduced Keepers

The raid frees fewer than 4 Keepers, losing that Keeper's support. Tuned for the hardest single-Keeper
case, **Thorim only**. The bitmask lives in `PERSISTENT_DATA_WATCHERS_MASK` — preferred over Sara's
`GetData(DATA_GET_KEEPERS_COUNT)`, which gives a count only, because reading the mask confirms
**which** Keeper.

What Thorim-only removes: no Freya means **no Sanity Wells, so Sanity (63050, 100 stacks) is a
one-way drain** — nothing restores it; no Hodir means no Protective Gaze absorb; no Mimiron means no
haste clouds and therefore a slower kill and more total drain.

Sanity drains, and whether anything can be done:

| Source | Spell | Loss | Avoidable |
|---|---|---|---|
| Psychosis | 63795 / 65301 | −9 / −12 | **No** — random target every 3.5s in P2 |
| Malady of the Mind | 63830 / 63881 | −3 | Yes |
| Brain Link | 63803 | −2 | Yes, if the pair stays within 20 yd |
| Lunatic Gaze (P2 skull) | 64168 | −2 | Yes — only players *facing* the caster |
| Lunatic Gaze (P3 Yogg) | 64164 | −4 | Yes |
| Induce Madness | 64059 | −100 | Yes — leave the brain room |

**Thorim's Titanic Storm auto-kills anything carrying `SPELL_WEAKENED` (64162)**, and a guardian
drops Empowered at ~<10% HP. So **melee only need to burn a guardian to Weakened; Thorim finishes
it.** With no Thorim, guardians are effectively unkillable without the cheat — which is why the P3
cheat instakill is suppressed **only when Thorim is a Keeper**, leaving other reduced-Keeper combos
winnable.

Crusher Tentacle (33966) is **stationary** and nobody tanks it — with no one in melee range its Crush
cannot land, so ranged nuke it in place. Its Diminish Power (64145) is a raid-wide DPS debuff, so it
must die fast.

Open risks: the removed P3 cheat existed because Lunatic Gaze "freezes" bots, so guardian DPS may
stall between gazes; a guardian spawns up to 48 yd out, so confirm taunt pickup is prompt; and the
sanity-conservation behaviour (stand behind Yogg facing away below 15 stacks) **nearly benches a bot**
— sanity never recovers Thorim-only, so a bot that drops to 15 stays there.

### XT-002

Ulduar is 10/25-man only, so **"hard mode" here is the Heartbreak split, not a difficulty flag**.
Killing the Heart during its 30s exposed window (63849) sets XT to full health, grants **Heartbreak
(65737) permanently**, and stops rescheduling the phase check — so there are no further Heart phases.
Heartbreak is a reliable runtime signal that hard mode is live, but there is no signal *before* the
kill, so the config declares intent: on, bots burn the Heart to zero on the first window; off, they
stop at 15% so the fight stays in normal mode.

**Fork quirk, deliberate:** `npc_xt_toy_pile::SummonDistance = 90.0f` with the check
`if (!xt002 || xt002->IsWithinDist(me, SummonDistance)) return;` — **adds only spawn when XT is more
than 90 yd from a pile**, so tanked in place they essentially never appear on this core. Add handling
is written to work whenever adds do spawn; tank positioning is deliberately untouched.

Boombots explode for 15-18k on reaching XT **or at 50% health**, so melee must never touch them.
Scrapbots walk to XT and heal him, so they must die en route.

`NPC_XT002` (33293), `NPC_XT_TOY_PILE` (33337), `NPC_XS013_SCRAPBOT` (33343) and
`NPC_HEART_OF_DECONSTRUCTOR` (33329) come from core `ulduar.h` via `UldScripts.h` — **do not
redeclare them.**

Use the `IsBurstCooldownAction` registry rather than hand-rolling a `dynamic_cast` list the way
BT and SWP do. Note `MoveAwayFromPlayerWithDebuffAction` takes a **single** spell id fixed at
construction, so it cannot cover both the 10 and 25-man ids of Searing Light or Gravity Bomb.

## Algalon

No separate heroic script — 10N and 25N share one AI with identical entries and ids, so the strategy
is difficulty-agnostic for free. Only the Cosmic Smash meteor count differs (1 vs 3), which is
irrelevant since bots dodge every asteroid-target NPC present.

| Mechanic | Ids | Handling |
|---|---|---|
| Cosmic Smash | selector 62301, impact 62304, targets 33104/33105 | Damage falls off past ~10 yd — flee each asteroid target |
| Big Bang | 64443, every 90.5s | Raid-wide lethal to anyone not phased; a Black Hole grants phase aura 62169 |
| Phase Punch | 64412, stacks 1→5 | At 5 the tank is fully phased out — swap before then |
| Black Hole chain | Collapsing Star 32955 → Black Hole 32953; P2 Worm Hole 34099 | The Big Bang shelter and the constellation sink |
| Living Constellation | 33052, phase effect 65509 | Must be **led onto a live Black Hole** — contact despawns both |
| Unleashed Dark Matter | 34097 | Chases a random player; focus-kill |
| Ascend / enrage | 64487 at 6 min | Also fires on the Big Bang evade |

**Big Bang evade caveat, documented and not worked around:** `spell_algalon_big_bang::CheckTargets`
calls `ACTION_ASCEND` — the boss evades and resets — when Big Bang hits **zero** targets. A pure-bot
raid where everyone hides therefore resets the boss. The fight requires at least one non-bot soaker,
or you accept the reset.

**Big Bang is unavoidable raid-wide damage, and full immunity does not prevent it** — Divine Shield
does not work. Only *mitigation* survives, so the soaker is a **Shadow Priest using Dispersion** (90%
reduction), not a Protection Paladin. That needed a second piece: `AlgalonMultiplier` **reserves the
cooldown** by returning `0.0f` for `CastDispersionAction` while the bot is the designated soaker,
Algalon is engaged and Big Bang is not channeling — otherwise the priest spends it on the normal
`low mana` / `critical health` nodes and it is down when it matters. Other shadow priests disperse
normally; with no living shadow priest, everyone hides and there is no soaker.

**Latent bug, still live:** the hide/soak triggers and `UldMultipliers.cpp:28` use
`"algalon observer"` while every other call site uses `"algalon the observer"`. `"find target"` is an
exact full-name match, so one of them never resolves. Not fixed.

Two as-built notes worth keeping: the constellation kite is **movement-only**, because
`MovementAction` exposes `MoveTo`/`FleePosition` but **not** `Attack` (that lives on the sibling
`AttackAction`) — the kiter stands on a Black Hole and drags the chasing add through the phase field.
And Phase Punch swap is an `AttackAction`, firing when the boss's victim reaches 3 stacks and the
first assist tank's own stacks have decayed below that.

## Ignis

The fight is a construct-disposal loop, not a damage race. Each Iron Construct (33121) Ignis
activates puts a stack of Strength of the Creator (64473) on him, and a construct **cannot be killed
by damage** — it only dies to the chain:

1. it stacks Heat (65667) while standing in a Scorched Ground patch (33123), and turns **Molten**
   (62373) at 10 stacks, which also wipes its threat table;
2. a Molten construct within **18 yd of a water trigger** turns **Brittle** (62382 10-man / 67114
   25-man) on the construct's own once-a-second poll;
3. any single hit of 5000 (10-man) / 3000 (25-man) then shatters it, killing it and removing a
   Strength stack.

Scorch only lights a patch when it lands more than 25 yd from water, so the fire and the pools are
always separate places and the walk between them is the mechanic. The two pools are at
`(526.771, 277.796, 360.802)` and `(646.771, 277.796, 360.802)` — hardcoded as
`ULDUAR_IGNIS_WATER_POOL_WEST` / `_EAST` rather than found by entry, because the water trigger is
22515, the generic Ulduar world trigger.

**Kiting is assist-tank only, on purpose.** `GetIgnisConstructTank` is a bare
`GetGroupAssistTank(botAI, bot, 0)` with no fallback: a main tank pulled off Ignis drags the boss
along behind the construct, and a DPS holding a Molten one dies. A raid without an assist tank
simply skips the loop and lets the Strength stacks climb.

The skull leaves Ignis only for the Brittle window and goes straight back afterwards — a Brittle
construct dies to one hit, so a raid-wide swap for a whole kill would cost more than it is worth.
Tanks are excluded from that swap for the same reason.

**Slag Pot** (62717 / 63477) is a vehicle ride: healers pour direct heals into the victim, and the
victim's movement actions are suppressed, since orders only fight the ride and leave it facing the
wrong way when it drops. **Flame Jets** (62680) is deliberately unhandled — raid-wide, no dodge and
no soak, so there is nothing a bot could do that generic healing does not already cover.

Ignis has no hard mode. Heroic is free: the paired spell ids above are both checked, and the only
other 25-man difference is the construct cadence (30s instead of 40s).

Encounter lookups go through `GetIgnis` (`GetFirstAliveNpcByEntry`), not `"find target"` — a bot
parked on a construct never has Ignis on its threat list, and a dormant construct carries
`UNIT_FLAG_NOT_SELECTABLE`, which drops it out of `"possible targets"` entirely.

## Auriaya

No hard mode, and no difficulty split — 10N and 25N share every entry and spell id.

| Mechanic | Ids | Handling |
|---|---|---|
| Sonic Screech | 64422 | **120°** frontal cone per `spell_cone`; non-tanks sidestep out of the arc |
| Terrifying Screech | 64386 | Fear every 35s from the pull, so the whole fight is one anti-fear window |
| Sentinel Blast | 64389 | Raid-wide, **not** a cone: no `spell_cone` row, and its SpellScript strips non-players. Healed through |
| Sanctum Sentry | 34014 | Assist tank 0 taunts each loose one; Strength of the Pack (64369) buffs the boss while they live |
| Feral Defender | 34035 | Random aggro (61906) makes it untankable — focus-killed, never tanked |
| Seeping Feral Essence | 34098 | Non-selectable stalker dropped per Defender life; found via `"nearest npcs"`, fled at 10 yd |
| Guardian Swarm | 64396 | Tank DoT, left to the generic dispel |
| Enrage | 47008 | 10 min, unhandled — no enrage awareness exists anywhere in the module |

`IsBotInFrontalCone` forwards to `HasInArc`, which takes the **full** arc, so the original
`M_PI / 2` left every bot between 45° and 60° off-centre standing in a cone it believed it had
cleared.

Kill order is **Sentries → Feral Defender → boss**: sentries stay dead and drop the boss's buff,
where each Defender kill costs a void zone and buys 35s. The Defender feigns at 1 HP wearing
`UNIT_FLAG_NOT_SELECTABLE`, so it resolves through `GetFirstLiveUnitByEntry`, never
`GetFirstAliveUnitByEntry`. Savage Pounce (64666) fires only at 8–25 yd from the sentry's own
victim, so a tank holding it in melee is the whole counter — the taunt needs no positioning code
behind it.

**Every Auriaya trigger used to resolve the boss through `"find target"`**, which walks only the
bot's own threat list: any bot fighting a sentry or the Defender silently lost its cone dodge, its
void-zone dodge and its anti-fear. All of them go through `GetAuriaya`, by entry, now.

The main tank arc-steps around Auriaya until she faces away from the raid centroid — tanks excluded
from that centroid, or they drag the bearing they are steering by. It holds still inside a 0.15 rad
tolerance, because a tank that steps every tick never lands a cast.

**Crazy Cat Lady requires no sentry killed, so it is incompatible with the kill order.** Bots
optimise for the kill and, per the follower model, never chase achievements.

## Core behaviours the strategies key off

Upstream script and DBC facts our code now depends on, with the behaviour each one drives. Every one
of them silently disabled something before it was accounted for, so re-check them after any
parent-repo sync.

### Razorscale — harpoons are GameObject state, not auras

`SPELL_CHAIN_1..4` (49679, 49682, 49683, 49684) **no longer exist anywhere in the core**. Harpoons
fire `SPELL_HARPOON_SHOT_1..4` cast by `NPC_RAZORSCALE_CONTROLLER` out of
`go_razorscale_harpoon::OnGossipHello`; the GameObjects are summoned per ground phase by the
controller (two in 10-man, four in 25-man) and a spent one carries `GO_FLAG_NOT_SELECTABLE` until it
is rebuilt. Readiness is that flag — `IsHarpoonFired()` and `HarpoonData::chainSpellId` are gone,
they had been testing an aura that could never be present.

The tank debuff also moved: **Fuse Armor is 64821**, not 64771 (64774 is still the 5-stack `Fused
Armor`). 64771 is gone from the core, so the tank-swap check never fired. Threshold stays at 2 stacks.

### Razorscale — Devouring Flame is 5 yd, and the skull has one owner

The patch is NPC 34188 carrying 64709, a 2 s periodic trigger of 64704 (64733 in 25-man), **whose
damage radius index is 8 = 5.0 yd**. Bots clear `DEVOURING_FLAME_CLEAR_RADIUS` (7 yd) so a step
actually leaves the patch instead of stopping on its edge, and `DevouringFlameBlocks()` rejects any
destination covered by another one — she drops these every 6–12 s and they stack up.

Clearing is only half of it. `razorscale avoid devouring flames` also **holds the tick without moving**
while the spot a melee bot would walk back to is on fire; `RazorscaleMultiplier` zeroes the generic
movers and `avoid aoe` for the same window. Both release the moment the tank drags her clear, so
nobody stands out the patch's full life. A permanent veto here is the freeze bug.

`razorscale kill target action` is the **only** thing that sets the skull: the boss whenever she is
on the floor, otherwise Sentinel → Watcher → Guardian. `DpsTargetValue` prefers the RTI target, so a
skull left on an add is the whole raid left on an add. Moon stays on the boss while she is airborne —
it is excluded from every DPS target scan — and is cleared on landing.

The generic pet-attack node is commented out engine-wide (`CombatStrategy.cpp`), so pets keep whatever
they last hit unless a script re-orders them: `razorscale pet control action` puts them on the adds
while she flies and on her when she lands, and always reports failure so the tick falls through.

### Vezax — Shadow Crash can land in the melee stack

The event used to only consider players beyond 15 yd; it now picks a random target outside combat
reach and falls back to any target, so the puddle can drop on the tank. Detection was never the
problem (`VezaxShadowCrashTrigger` keys off the puddle's area aura 63277) — the dodge orbited at a
hard-coded 15 yd and dragged every melee out of range and held them there. It strafes at the bot's
own radius now: melee and tanks 4–8 yd, ranged 13–17 yd, constant 5 yd of arc per step so a tight
radius still clears the puddle quickly. Tunables sit next to the other Vezax ones in `UldBossHelper.h`.

### Mimiron — Laser Barrage arcs are deterministic

VX-001 does not aim at a random player. Arcs start at 6.17 rad, advance +60° counterclockwise per
cast, reset at the start of phases 2 and 4, and the boss faces the arc during Spinning Up; the aura
then sweeps that facing clockwise at π/60 per 250 ms (~12°/s) with the beams following unit facing.
Cadence is 60 s, not 45. So the dodge reads the boss facing and moves to `orientation + delta_angle`
(π/8) at the bot's own radius clamped to 10–24 yd, re-issued each tick so bots trail the beam —
the old code teleported everyone onto the master, which is now only the fallback when VX-001 cannot
be resolved.

### Yogg-Saron — Squeeze breaks on immunity

Removing the Squeeze aura (64125 / 64126) kills the Constrictor Tentacle and drops the passenger, so
`yogg-saron squeeze escape` at `ACTION_RAID + 1` has a grabbed mage cast Ice Block and a paladin cast
Divine Shield. Hunter Feign Death and rogue Vanish are deliberately not used — neither removes a
periodic damage aura.

## Burst and Bloodlust windows

`UlduarBurstWindowMultiplier` is always active inside Ulduar — no config key, matching every other
raid. Two tiers are gated separately: `allowAll` covers every burst cooldown, `allowLust` covers
`bloodlust`/`heroism` only, because a 10-minute raid cooldown wants a later window than personal
cooldowns that come back within a phase.

Because burst only ever fires while the bot's current target is boss-flagged, **adds-only phases need
no gate of their own**. There is no Sated/Exhaustion check anywhere, and no Drums or Time Warp — lust
is shaman-only.

| Boss | `allowAll` | `allowLust` | Why |
|---|---|---|---|
| Razorscale | grounded (`Z <= 440`) | same | Zero damage taken while airborne; every landing, harpoon knockdowns included, is a real burn window |
| Mimiron | always | all three mechs alive | P1-P3 damage counts; all three up is P4, the enrage burn |
| Yogg-Saron | P2 or P3 | P3 | P1 damage lands on Sara and is wasted |
| Assembly of Iron | always | exactly one member alive | They resurrect each other; also covers the hard mode, since Steelbreaker-last means the survivor is empowered |
| Freya | always | no `SPELL_ATTUNED_TO_NATURE` 62519, **or** HP ≤ 25% | The aura reduces damage taken for the whole wave phase |
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

**Do not veto** — the boss is main-tank-held all fight, so the generic node is right for the primary
target and the only gain would be redirecting *adds*: Auriaya, Freya, Kologarn, Yogg-Saron, Ignis.
**No** — Vezax (one tank, no swap, no reset), Hodir (no meaningful tanking), Flame Leviathan (vehicle
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

Per decision, `NaxxRedirectThreatAction` stays where it is rather than being promoted to a shared
raid-level base. Known cost: the Misdirection proc-aura id `35079` is already duplicated across nine
per-raid helper headers, and Ulduar would make ten.

## Normal-mode gaps still open

From the Sev-1/Sev-2 audit. Sev-1 fails **even with the raid cheat on**:

| Boss | Gap |
|---|---|
| **Mimiron** | No ground-fire avoidance in normal mode either — only fire *resistance* |
| **Thorim** | Unbalancing Strike had no real tank swap, only a cheat debuff strip |
| **Vezax** | Saronite Vapor puddles never dodged |
| **Razorscale** | Dark Rune Watcher/Guardian adds have no interrupt (focus and the Flame Breath cone are handled) |
| **Freya** | Snaplasher is only skull-marked, so bots multi-DPS it and it hardens; Storm Lasher cast not interrupted |
| **Algalon** | Collapsing Star (32955) unhandled — both `big bang hide` and `constellation kite` search only for *existing* Black Holes and silently fail when none exist |

**Sev-2 CHEAT-ONLY** — works in the default config, breaks silently if `BotCheats` drops `raid`:
Hodir Biting Cold (real movement commented out, cheat strips the aura); Thorim Unbalancing Strike;
Mimiron Proximity Mines and Bomb Bots (cheat-kill only — the signature Firefighter killers have no
avoidance); Kologarn Crunch Armor and Focused Eyebeam (cheat aura-remove / teleport, no real swap or
kiting); Yogg Ominous Clouds, Crusher/Constrictor tentacles, illusion-room adds and P2 movement
(cheat instakill / teleport); Vezax no-mana-regen (cheat mana refill).

Structural notes: **no boss reuses `RazorscaleBossHelper`'s role-swap machinery for a real tank
swap** — Thorim, Hodir and Kologarn all fall back to cheats. There is no enrage-timer awareness
anywhere, which blocks every hard-mode kill-timer requirement.
