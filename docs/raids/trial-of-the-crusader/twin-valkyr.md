# Twin Val'kyr

Raid-wide facts, the code layout and the essence, Vortex, Touch and Twin's Pact ids by difficulty:
[README.md](README.md). Mechanics: `boss_twin_valkyr.cpp`, the DBC CSVs and `SpellInfoCorrections.cpp`.
Figures run 10N / 25N / 10H / 25H.

Other ids, Light / Dark where paired:

| Spell | 10N | 25N | 10H | 25H |
|---|---|---|---|---|
| Surge damage | 65767 / 65769 | 67274 / 67265 | 67275 / 67266 | 67276 / 67267 |
| Vortex damage | 66048 / 66059 | 67203 / 67155 | 67204 / 67156 | 67205 / 67157 |
| Shield of Lights / Darkness | 65858 / 65874 | 67259 / 67256 | 67260 / 67257 | 67261 / 67258 |
| Unleashed Light / Dark | 65795 / 65808 | 67238 / 67172 | 67239 / 67173 | 67240 / 67174 |
| Empowered Light / Darkness | 65748 / 65724 | 67216 / 67213 | 67217 / 67214 | 67218 / 67215 |
| Powering Up | 67590 | 67602 | 67603 | 67604 |

Fjola Lightbane (34497, Light) runs the fight: the specials, the berserk, and the shared health pool
(each update she takes Eydis's health loss onto herself and copies the result back). Eydis Darkbane
(34496, Dark). Either death kills both. Both fly, CombatReach 9, immunity set -286: silence, stun,
fear, root, snare, knockback and more, but not interrupt or taunt. They turn aggressive at
(585.5, 170) and (545.5, 170), 3.25 s after `IsEncounterInProgress`. Their events advance only with
a victim, so every timer below counts from the pull.

## Colours

Light spells are fire (school 4), Dark ones shadow (32).

- **Essence**: absorbs 1,000,000 of its school, +20% run speed, and through its linked Effect 2
  (65811 Light / 65827 Dark) **+50% damage against the other-colour twin and −50% against her
  sister** (`MOD_DAMAGE_DONE_VERSUS_AURASTATE`; aura state 19 is Eydis, 22 Fjola). A Light bot on
  Fjola deals half. Death-persistent; taking one strips the other.
- **Surge**, each twin from the pull: every 2 s, map-wide, 1,500 / 2,500 / 2,500 / 4,500 to every
  player without her colour's essence (`ExcludeTargetAuraSpell`, set per difficulty in
  `SpellInfoCorrections`). Everyone takes exactly one; no essence takes both.
- **Vortex**: 8 s cast (6 s heroic), then a 5 s channel ticking every second map-wide (radius index
  28) on anyone without the caster's essence: 5,850-6,150 / 8,288-8,712 / 9,263-9,737 /
  15,600-16,400. Wrong colour is 30-80k in 5 s, lethal on 25H, tanks included. Not interruptible
  (`PreventionType` 0). Self-channelled, so `FindCurrentSpellBySpellId` sees it from cast start to
  channel end.
- **Powering Up** (67590 row): each 1,000 an essence absorbs is a stack; 100 become Empowered
  Light/Darkness for 20 s (+100% damage against the other-colour twin, 20% mana). Matching Vortex
  ticks give 5-16 stacks each, matching orbs and Touch ticks the rest. The stacks do nothing else.

## Essence portals

Light 34568 at (541.0, 117.3) and (586.2, 162.1), Dark 34567 at (586.1, 117.5) and (541.6, 161.9):
31.8 yd from `ARENA_CENTER`, opposite colours 45 yd apart along an edge. Friendly, gossip flag,
CombatReach 7, summoned and despawned with the twins. `npc_essence_of_twin::OnGossipHello` strips
the other essence and the Touch of the portal's colour, then casts the essence, remapped by the
`Spell` constructor, so a bot's `HandleGossipHelloOpcode` works on every difficulty.
`GetNPCIfCanInteractWith` allows `INTERACTION_DISTANCE` 5.5 **plus both combat reaches**: about
14 yd from a portal, 18 yd from the centre.

## Specials

Every 45 s from 45 s, one of four, none repeating until all four have run: Light Vortex (Fjola),
Dark Vortex (Eydis), Light Pact (Fjola), Dark Pact (Eydis).

**Twin's Pact.** The caster takes Shield of Lights/Darkness (16 s), then casts Twin's Pact (15 s,
interruptible). The shield absorbs 175k / 700k / 300k / 1.2M and carries `MECHANIC_IMMUNITY` to
interrupt (26), so every kick lands for nothing until the absorb is spent and the aura drops. Pact
heals 20% (normal) or 50% (heroic); `SpellInfoCorrections` strips its sister-heal effect and shared
health carries it over. The sister gets Power of the Twins and dual wield for 15 s, hitting her tank
harder. Interrupts that work: kick, pummel, shield bash, counterspell, wind shear, mind freeze,
spell lock; silences and stuns do nothing.

**Touch**, heroic only. Each twin, first at 10-25 s, then 45-50 s after one lands or 10 s after
finding no target; Fjola pushes hers to 15 s past each special. It targets a random player holding
the other colour's essence who is neither twin's victim. **In this core it hits the whole raid**:
`spell_valkyr_touch_aura` deals 2,925-3,075 (10H) or 5,850-6,150 (25H) every 2 s for 20 s to every
alive player on the map without `ExcludeTargetAuraSpell`, which is 0 for the Touch spells, and deals
the amount from before `CalcAbsorbResist`, so absorbs are spent (and grant Powering Up) without
reducing it. Only the touched player taking the Touch's colour ends it.

## Orbs

Concentrated Light 34630 and Darkness 34628: hostile, not selectable, reach 0. Waves at 10-15 s,
then +8, +8 and +15 s, repeating: 4, 4, 24 (normal) or 6, 6, 36 (heroic), half each colour. They
spawn on a 47 yd circle round `ARENA_CENTER`, 1.5 yd up, and fly chords to random points on it at
7-8 yd/s (flight or run speed, unmeasured), despawning at an endpoint two times in three. Every
85 ms (100101) an orb takes the nearest **player** within 2.75 yd 2D, never a pet, and explodes:
Unleashed Light/Dark, 6 yd, 7,800-8,200 / 7,800-8,200 / 8,775-9,225 / 14,625-15,375, absorbed by
the matching essence. An orb its own colour sets off still hits the other colour within 6 yd.

## Tanks, health, berserk

- Twin Spike every 7-10 s from 5-8 s: 100% weapon damage (125% heroic), physical, +20% damage taken
  for 15 s. Otherwise tank damage is light; Power of the Twins is the peak.
- Taunts diminish (`flags_extra` 0x80000 on all eight entries): 100 / 65 / 42 / 27% duration, then
  immune, threat effect included, until 15 s pass untaunted. Class taunts recharge in 8 s, so
  taunting on cooldown reaches the immune 5th.
- Berserk 64238 (+900% damage, +150% haste) at 10 min normal, 6 min heroic.
- HealthModifier 435 / 2000 / 600 / 2800, rescaled by `mod-dungeon-scale`: read `mhp` off a trace.

## Wipe and kill

An evade sets `TYPE_FAILED`, and `InstanceCleanup` despawns both twins with their portals and orbs
and strips every essence, Empowered and Touch (`DoAction(-1)`). Barrett's gossip summons fresh twins,
so each attempt restarts every timer. The kill strips the same auras.

## Floor

No point on the arena floor can be verified offline ([README.md](README.md#arena-floor)); anchor
only on the script's own points, `ARENA_CENTER` and the portals.

## Strategy

Tactics follow the Warcraft Tavern 25-man guide wherever the script allows.

**Colours**, `GetWantedEssence`, first match:

1. **Touch** on the bot: its colour. Outranks the Vortex: a Touch hits the whole raid until its
   target swaps, so Vortex first costs up to 6 more ticks.
2. **Vortex** running: its colour, for everyone, tanks and healers included. The guide keeps tanks
   in place on cooldowns, lethal on 25H, and the tank that must swap usually holds the casting
   twin, who stands still.
3. **Shield**: while a shielded twin casts her Pact, a DPS bot without the other colour takes it,
   turning −50% on the shield into +50%; one with none goes straight there, not via Dark. A swap
   costs about 2.5 s each way (gossip reach is ~18 yd from the centre); the guide offers "switch to
   Light" when DPS is short.
4. **Base** (guide, "The Pull", "Tanks"): a tank takes the colour opposite its twin for +50% threat
   (Fjola's tank Dark, Eydis's Light, a tank holding both Dark), so tanks walk back after a Vortex;
   anyone else keeps what it holds, Dark when none.

**Essence walks** go to the nearest portal of the colour at `MOVEMENT_FORCED` and gossip in reach.
Touch, Vortex and Shield walks first clip a cast that pins the feet. A walk stopped 500 ms short of
range clears `last movement`, which otherwise refuses the repeat as a duplicate for 5 s; one still
walking holds the tick, or a lower node's charge leaps the bot back onto the twin. Touch and
Vortex walks and the orb dodge also clear a booking headed elsewhere: forced doesn't outrank forced.
Once the gossip lands the essence the bot stops rather than finish the walk under the lock. No
essence-swap multiplier: a blanket `MovementAction` veto also vetoes `AttackAction`
([pitfalls.md](../../engine/pitfalls.md)).

**Targets.** DPS hit the twin of the other colour than their essence (Light → Eydis, Dark or none
→ Fjola), the Pact twin overriding, each bot with its own RTI: skull on Fjola, cross on Eydis. A
skull on Fjola for everyone halves every Light bot. Under a lone tank all DPS stay on Fjola outside
a Pact: Light DPS would pull Eydis, and diminishing taunts can't bring her back.

**Tanks.** Alive tanks ranked main, then assists with assistants first, each passing `IsTank` or
`IsTank(bySpec)` (the strategy read flickers): Fjola to the first, Eydis to the second, both to a
lone one (the guide calls one tank possible). Each marks, holds, taunts back and drags its twin to
`ARENA_CENTER`, which keeps melee target swaps short and every portal equidistant. Multipliers:
`TwinValkyrTauntGuardMultiplier` zeroes a taunt on a twin another living tank owns,
`TwinValkyrControlTankMovementMultiplier` zeroes formation moves for a tank holding its twin.
Hunters and rogues redirect to their target twin's tank, casting outside the queue, so
`TwinValkyrRedirectGuardMultiplier` zeroes the class Misdirection and Tricks actions. Tank Vortex
damage and Twin Spike get no defensive node: the guide calls tank damage light, Power of the Twins
the peak.

**Twin's Pact.** Every DPS goes to the Pact twin. `TwinValkyrInterruptHoldMultiplier` zeroes the
seven working interrupts while any twin is shielded, and silencing shot, strangulate, silence,
hammer of justice and bash on a twin always. Arcane torrent stays: only the racial nodes cast it,
for its resource. Once the shield breaks, the duty falls to the lowest GUID among bots on the map
with a ready, in-range working interrupt that are not swapping for a Touch or Vortex, as on Vezax.
Ready also means off cooldown and affordable: `CanCastSpell` skips power, and a pet spell's
cooldown; the warlock casts Spell Lock itself, so its cooldown and range are the warlock's. The
burst row holds lust until a twin raises her shield, where the guide wants it, and allows every
other cooldown.

**Orbs.** A bot dodges orbs of another colour (every orb without an essence) and of its own colour
while a group member without it stands within 7 yd, testing points a yard apart along each orb's
next 7 yd (a second) of flight: trigger at 4 yd, search clear by 5 yd within 12 yd, clipping a
pinning cast only with a point within 3.25 yd. A found spot is kept while it stays 4 yd clear (so
it can't re-fire the trigger) and the dodge ran within 1 s (so no later wave reuses it). A wider
band or longer horizon dodges permanently: 6 yd over more path covers most of the 47 yd disk on a
heroic wave ([pitfalls.md](../../engine/pitfalls.md)). No stack, soakers or active collection,
against the guide: Empowered comes mostly from matching Vortex ticks. The dodge yields to a due
Touch or Vortex swap.

**Lookups.** One scan per instance per ms (`RaidInstanceState`) resolves the twins through
`GetGuidData`, reads the Vortex, the Pact, the shield and the tank list, and takes every orb and
portal in one 100 yd sweep from a twin; `GetFirstAliveUnitByEntry` is deprecated and sight-capped,
and per-bot sweeps and group walks multiply.

| Trigger | Action | Relevance |
|---|---|---|
| `twin valkyr pact interrupt duty` | `twin valkyr interrupt pact` | `ACTION_EMERGENCY + 7` |
| `twin valkyr touched requires essence` | `twin valkyr swap essence for touch` | `ACTION_EMERGENCY + 6` |
| `twin valkyr vortex requires essence` | `twin valkyr swap essence for vortex` | `ACTION_EMERGENCY + 5` |
| `twin valkyr orb incoming` | `twin valkyr dodge orb` | `ACTION_EMERGENCY + 4` |
| `twin valkyr shield requires essence` | `twin valkyr swap essence for shield` | `ACTION_RAID + 5` |
| `twin valkyr needs base essence` | `twin valkyr take base essence` | `ACTION_RAID + 4` |
| `twin valkyr engaged by main tank` | `twin valkyr main tank hold light twin` | `ACTION_RAID + 3` |
| `twin valkyr darkbane needs assist tank` | `twin valkyr assist tank hold dark twin` | `ACTION_RAID + 2` |
| `twin valkyr redirect threat` | `twin valkyr redirect threat` | `ACTION_RAID + 1` |
| `twin valkyr dps target` | `twin valkyr focus twin` | `ACTION_RAID` |

### Known gaps

- No soakers or active orb collection: Empowered comes from matching Vortex and Touch absorption and
  orbs that happen to pass.
- A bot touched during the other colour's Vortex swaps for the Touch and eats the Vortex unless it
  swaps back in time.
- Humans hold no interrupt duty: there is no `PlayerbotAI` to ask.
- A lone tank can't truly hold both (**Targets**): Light bots deal half on Fjola, and an Eydis
  Pact still pulls her off.
- Deferred cleanup: each essence and orb trigger re-derives `GetWantedEssence` every tick. A
  per-bot, per-ms memo in `TwinValkyrState` must match its `tv.essence` probe and the peer loop in
  `IsTwinPactInterrupter`, which reads other bots' wants.

## What a trace answers

Position, verdicts and movement come from the raid-agnostic streams. The Twins rows, under
`--notes tv.`, each per bot on change unless marked:

| Key | Says |
|---|---|
| `essence` | `<reason>:<colour>` wanted: `touch`, `vortex`, `shield`, `base`, or `none` with neither twin alive |
| `target` | `<fjola\|eydis>:<rule>`, the twin the DPS rule names and why: `pact`, `lone` (a lone tank), or the essence held (`light`, `dark`, `none`); `none` with no twin |
| `tank` | `fjola`, `eydis`, `both` or `none`, tanks only |
| `shield` | The shielded twin, `0` none, `1` Fjola, `2` Eydis. Per instance |
| `interrupt` | `duty`, `standby` or `none` (no ready interrupt), written by every living bot while a twin casts an unshielded Pact; a dead bot's goes stale, so the reader cuts it at the death |
| `orb` | The rule of the hazard point nearest the bot when it found a dodge spot, `wrong` or `splash`, or `none` when no spot was found |

`tools/botobs/bosses/val_kyr_twins.py` reads the rest: per bot, time without an essence and swaps
under the `tv.essence` reason held when the essence landed, and per Vortex, how many held its colour
and who took unabsorbed ticks, by role (`--essence`); each Touch's hold, what ended it and the raid
damage meanwhile (`--touch`); each Pact's shield up and down off `tv.shield`, cast length, outcome,
kicks wasted on the shield and after the break, duty and lust (`--pact`); Unleashed hits taken or
absorbed, peak Powering Up and Empowered per bot, and which bots dodged under each `tv.orb` rule
(`--orbs`); each twin's time on her `tv.tank` tank, taunts, distance from `ARENA_CENTER` and between
the twins (`--tanks`); each DPS bot's time on the twin the rule names, Fjola while a `tv.tank` reads
`both` (`--targets`).

Still invisible, and what stands in:

- **An essence taken before the trace opens** has no apply row. A bot's first removal names the
  colour it held; a bot that never swapped is placed by the Surges it took, since each skips holders
  of its colour's essence; one neither Surge touched reads `?`.
- **The shields and the Pact heal**: creature auras and heals are never recorded, so `tv.shield` is
  the shield and a jump in shared health the heal. A Pact the casting column drops more than 1 s
  early with no jump reads `kicked`.
- **Touch rows** carry the full amount; their `ab` is shield spent, not damage prevented.
- **A lone human tank** writes no `tv.tank`, so `--targets` scores its DPS by colour.
- A kill where no player hit Eydis carries no credit and files `reset` or `wipe`
  ([observability.md](../../systems/observability.md)).
