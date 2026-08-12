# Obsidian Sanctum (map 615)

Strategy key `wotlk-os`. Cross-raid conventions are in [README.md](README.md).

## Difficulty is fixed at the pull — so kill every drake

Sartharion counts the drakes alive in `JustEngagedWith`, adds a loot mode per drake, and **never
recounts**; the Twilight Assist/Duo/Zone criteria all read that snapshot. Killing drakes during the
fight costs no loot and no achievement, so bots kill every drake that joins. The raid picks its
difficulty by which drakes it kills *before* pulling.

Leaving one up is strictly worse: each keeps an aura on the raid all fight — Tenebron
61248 +100% shadow taken, Shadron 58105 +100% fire taken, Vesperon 61251 −25% max health — and
Shadron's acolyte re-applies **Gift of Twilight Fire 58766, which zeroes all damage Sartharion
takes**. Killing one costs a single Twilight Revenge (60639, +25% physical damage and attack speed).

Kill order is landing order — Tenebron (called at 20s), Shadron (60s), Vesperon (120s) — which arrive
far enough apart that finishing the engaged one beats swapping.

**Twilight Revenge fires on every drake death, unconditionally** (`DoAction(ACTION_DRAKE_DIED)`);
acolyte state is irrelevant to it. **Berserk is 30% and unconditional**, so keeping drakes alive
does not avoid it either.

## Flame Tsunami

Alternates direction randomly every 25s, as walls of 8-yard `NPC_FLAME_TSUNAMI` (30616) segments:

| Wave | Spawns | Orientation | Occupies Y | Leaves safe |
|---|---|---|---|---|
| Left | X 3211, eastbound | `0` | 472-496, 520-544, 568-592 | 496-520, 544-568 |
| Right | X 3286, westbound | `pi` | 496-520, 544-568 | 520-544, plus the outer edges |

The two sets are **exact complements** — no Y survives both directions, so everyone including the
tanks moves for one of them. The `TSUNAMI_*_SAFE_*` constants are the gap midpoints: 508 and 556 for
the left wave, which leaves two gaps and lets melee and ranged split; 532 for the right, one shared.

Identify the wave by **orientation**, never by a segment's Y — the right wave has six segments and a
Y-match recognised only two, sending bots into the fire on the other four.

## Cones — from `spell_cone`, which overrides the DBC defaults

| Spell | Shape | Angle | Radius |
|---|---|---|---|
| Flame Breath 56908 | front | 82° (±41°) | 60 yd |
| Tail Lash 56910 | **back** (`SpellVisual[0]==3879` → `CONE_BACK`) | 82° → hits 139°-221° | 30 yd |
| Shadow Breath 57570 | front | 60° (±30°) | 15 yd |
| Void Blast 57581 — the fissure detonation | area | — | 4 yd |

Generic `rear flank` (90°-120° off the target's facing) clears every one of them, which is why melee
need no OS-specific positioning action. Flame Breath reaching 60 yd is why the ranged stack is placed
off Sartharion's front axis rather than merely far from him.

## Twilight realm — acolytes are invisible from the ground

Acolytes (31218/31219) and Tenebron's eggs (30882) are created with `SetPhaseMask(16)`; players are
phase 1. Both bot perception paths filter phase (`IsPossibleTarget` → `CanSeeOrDetect`, and the grid
check's `IsWithinDistInMap`), so **no ground bot can ever see one** — any trigger looking for an
acolyte directly is dead code. Eggs are unkillable from outside; only whelps crossing into phase 1
get attacked.

Entry keys instead on the two auras an acolyte's presence puts on phase-1 targets:

| Acolyte | Signal |
|---|---|
| Shadron 31218 | Sartharion has Gift of Twilight Fire **58766** |
| Vesperon 31219 | The bot has Twilight Torment **58835** |

Both are the called-by-Sartharion ids — the solo-pull variants are 57835 and 57935 — so they double
as the encounter gate, and both clear server-side the instant the acolyte dies.

All three drakes share **one** refcounted instance portal at `(3247.29, 529.804, 58.9595)`. Tenebron
reopens it every 60s with no acolyte behind it, and the aura gate is what keeps runners out of a
realm with nothing to kill in. Shadron reschedules 30s after each acolyte death for as long as it
lives, so the portal logic stays stateless and simply re-fires each cycle.

Portal GO 193988 casts **57620** (aura 261 `SPELL_AURA_PHASE`, misc 16), whose third effect triggers
**57874** (−25% damage done, plus a DoT). `spell_linked_spell` carries `-57620 → -57874`, so
`HasAura(57874)` is a valid and self-clearing realm detector. Exit GO 193989 casts 61187, linked
`61187 → -57620`, and has a permanent spawn at the same coordinates.

## Roles

| Role | Position |
|---|---|
| Main tank | On Sartharion, apart from everything else |
| Off-tank | Holds **drakes, Twilight Whelps and Lava Blazes together**, away from the raid |
| Melee | `rear flank` on whatever they are killing |
| Ranged + healers | One stack, in range of *both* tanks |

Whelps and blazes are off-tank work, not just DPS targets: Tenebron's called eggs hatch at
`(3237-3258, 513-541)`, inside the raid stack, and whelps put stacking Fade Armor (60708) on whoever
they reach; a Lava Blaze caught loose by a tsunami enrages. Holding them drags them into the safe
lane with the off-tank.

`SetFacingToObject` on the off-tank does **not** turn the drake — a drake faces whoever it attacks,
so the off-tank's *position* is what keeps Shadow Breath off the raid.

`ForceThreat` does `AddThreat(1000000)` then `FixateTarget`, and fixate overrides threat outright, so
a misdirect cannot move a fixated drake and a wrong redirect costs only a wasted cooldown. Hence the
narrow veto in `SartharionMultiplier`: the two redirect actions only, never the shared
`BuffOnMainTankAction` base, which would also kill Beacon of Light and Earth Shield.

Target priority, first match wins: acolyte (reachable only inside the realm) → Twilight Whelps →
Lava Blaze → next drake in landing order → Sartharion.

## Open

- The ranged stack `(3240, 508)` is reasoned, not measured. Confirm in-game that it holds inside
  30 yd of Sartharion and out of the Flame Breath cone.
- `IsTwilightRealmRunner` sends one ranged DPS, two at 25-man, and never a melee or a healer; the
  encounter expects more. Widen only after measuring how long the Acolyte of Shadron survives.
- Both tank spots share a tsunami lane (Y 532.5 and 526), so Sartharion and the drake pack stay level
  in Y all fight. Survivable now that tanks dodge — noted, not redesigned.
