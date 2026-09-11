# Thorim

Two halves that share nothing: a corridor gauntlet past the Runic Colossus and the Ancient Rune Giant,
then a stationary fight on the arena floor once Thorim drops off his balcony. `GetPositionZ() < 429.6`
separates the two everywhere in the strategy.

## Phase 1 split

The raid fights phase 1 in two halves, and the arena half must never empty out.
`ThorimAI::GetArenaPlayer()` scans for one **living** player inside

```
x 2085..2185   y -305..-214   z < 425
```

**every 5 seconds** from the start of phase 1. The first scan that finds nobody is terminal: `SAY_WIPE`
and a `Lightning Orb` (33138) that kills the raid. There is no grace period and no recovery, so this
is a hard constraint rather than a preference. A separate 5 minute timer
(`EVENT_THORIM_NOT_REACH_IN_TIME`) fires the same orb regardless, which bounds the whole of phase 1.

The split is **latched once per pull** and held until Thorim drops to the floor. Recomputing it per
tick is what let a role predicate flipping mid-fight walk the arena squad into the corridor. Quotas
are 1 tank + 1 healer + 3 DPS (10 man) and 1 tank + 2 healers + 7 DPS (25 man); a roster that cannot
fill them shrinks the gauntlet rather than emptying the arena, and assignment stops entirely once the
arena would drop below 3. The main tank never leaves the arena.

Every gauntlet pick is **bot-only**. A human still occupies their role but nothing here can walk them
anywhere, so spending the single gauntlet tank slot on a human off-tank just leaves the corridor a body
short. **Set `MEMBER_FLAG_MAINTANK` in the raid frame**: without it `GetMainTankGuid` falls back to the
first tank in roster order, so a human tank ahead of the bot main tank silently takes the role — which
sends the bot main tank down the corridor and leaves the arena untanked.

Arena adds all land 19-24 yd from the centre and the nearest box edge is 42 yd out, so the leash is
**30 yd from `ULDUAR_THORIM_NEAR_ARENA_CENTER`**, 15.8 yd short of the corridor mouth at the lever gate.
Melee get a tighter **24 yd**, which is the furthest an add ever lands, so it costs no uptime. Two
guards back it. `ThorimArenaLeashMultiplier` holds the generic movers while a bot is outside its leash
— and, alone among the Ulduar guards, does **not** exempt `AttackAction` or `ReachTargetAction`,
because corridor mobs sit ~92 yd out, inside the 100 yd sight cap, and the chase is exactly what walks
a bot out of the box. `ThorimArenaTargetGuardMultiplier` drops a target outside the box, or the leash
and the chase would take turns at the gate forever.

**A fence is not a formation.** Left with only the leash, the squad diffuses outward until it is parked
against the east edge at x ≈ 2165, which is the gateway. East is the worst direction to drift:
`boss_thorim_arena_npcs::CanAIAttack` is `GetPositionX() < 2180 && GetPositionZ() < 425`, so a bot that
gets there stops being attackable at all and the add re-rolls — and `SelectT()` picks a **random**
arena-side player and gives it 500 threat, so a squad spread over 50 yd puts adds on people no healer
is in range of. Threat is chaotic here by design; being in one place is the only counter.

So the squad is anchored. `GetThorimArenaAnchor` is the single answer trigger and action both read:
the tank holds `ULDUAR_THORIM_NEAR_ARENA_CENTER`, ranged and healers take a ring slot at 10 or 14 yd
(clear of the Champion's Whirlwind on the pile, close enough that anything in it is in range), and
melee get the centre only **out of combat** — in the fight they run free on the 24 yd leash. Slots come
from the latched squad in **roster order**, not from the survivors: bots die in here, and ranking by
who is still standing renumbers everyone behind the corpse and shuffles the formation mid-fight. Ring
points are computed and then validated with `GetMapWaterOrGroundLevel` and
`CheckCollisionAndGetValidCoords`, because raw ring geometry is the shape that lands off the navmesh
and `MoveTo` fails silently there. Slot 0 sits on the bearing from the lever gate to the centre, so the
formation opens away from the corridor. `ThorimArenaAnchorGuardMultiplier` holds the generic movers
once a bot is settled, exempting the chase — an add at 24 yd is up to 38 yd from an outer slot, and a
ranged bot that cannot step into range is silent.

## Runic Colossus (32872), spawned at (2227.5, -396.179, 412.176)

| Spell | Id | Detail |
|---|---|---|
| Runic Smash, left hand | 62057 | **5s cast**, lights the left-hand bunnies (33141) at x ~2235 / 2246 |
| Runic Smash, right hand | 62058 | **5s cast**, lights the right-hand bunnies (33140) at x ~2210 / 2221 |
| Runic Smash damage | 62465 | 10 yd per bunny; the wave starts 1s after the cast and marches y -385 → -257 at 16 yd / 500ms |
| Runic Barrier | 62338 | -51% damage taken **and a 2000 arcane damage shield per melee swing**; cast at t+10s, 20s duration, recast every 20s |

`EVENT_RC_RUNIC_SMASH` is scheduled in `Reset()` and **cancelled in `JustEngagedWith`**, so the corridor
smash only happens on the approach and stops the moment the Colossus is tanked. The Ancient Rune Giant
has no damage shield — its Runic Fortification (62942) is a friendly buff on its adds.

The two corridor lanes (left x ~2237-2242, right x ~2212-2219) are **index-matched by y**, so a dodge is
a straight index map: same waypoint number, other lane. Each lane sits 2-9 yd from its own hand's bunnies
and 15.5-22.9 yd from the other's. The bot side latches the hand it sees casting and holds the opposite
lane until the other hand goes up — the gauntlet formation follows that preference rather than the
master's own lane, or it would walk everyone straight back into the blast. The Colossus is 131 yd from the
top pair of waypoints, past the 100 yd `AiPlayerbot.SightDistance` cap, so the telegraph is read through a
targeted 150 yd creature lookup rather than the usual target values.

Runic Barrier is effectively permanent, so "stop attacking while it is up" would mean never attacking.
Non-tank melee instead back out to 14 yd below 55% health and return above 80%, keeping their target the
whole time so ranged and instant abilities keep landing.

## Phase 2

| Spell | Id | Detail |
|---|---|---|
| Chain Lightning | 62131 / 64390 | 8 targets at **×1.5 per hop** (`EffectChainAmplitude`): the eighth takes ~17× the first. Each hop goes to the unhit target nearest the last victim, within **8 yd** (`spell_jump_distance` 5.0, not the DBC 10, plus both combat reaches); pets count. First cast 13 s after the phase trigger, then every 15 s |
| Lightning Charge | 62466 | `spell_cone` **75 degrees**, 150 yd, 17343 base nature, **instant with no cast bar** |
| Lightning Orb Charged | 62186 | Lands on a Thunder Orb (33378). `SpellInfoCorrections` patches the amplitude to 5000ms, so it ticks once **5s before** the cone — the entire warning |
| Lightning Charge buff | 62279 | Permanent, one stack per cast: +15% damage and melee haste, **+10% nature damage per stack** |

Positioning has to clear 5 yd between stacks. The main tank drags Thorim to `(2134.857, -287.029)` — about
a yard from the hole in the floor south of y = -288, so nothing may be placed past it. Ranged and healers
round-robin three fixed spots. Melee take a **dynamic ring of radius 8 around Thorim's live position**,
three slots at the main tank's bearing +90 / +180 / +270 degrees, which leaves the whole tank side clear
and puts the stacks 11.3 yd apart. The off-tank sits at the tank bearing +20 degrees, inside taunt range
for the Unbalancing Strike swap. Slots are sticky per guid; the bearing is anchored on the tank so the ring
does not rotate as the boss shuffles, and falls back to the static tank spot's bearing when no tank is
alive. Arrival uses a 3 yd / 5 yd deadband, because a tight one against a ring recomputed from a moving
boss leaves the bot sliding in place — and a moving bot casts nothing.

When an orb lights, the **whole ring rotates rigidly** by the smallest angle that clears every occupied
slot out of the 75 degree cone (plus a 15 degree margin). Per-bot shortest paths would swing slots on
opposite edges toward each other and trade a Lightning Charge death for a Chain Lightning one. Both tanks,
ranged and healers hold position and eat it by design: moving a tank drags the boss and re-anchors the ring.

Every melee DPS carries the `behind` strategy from `AiFactory`, so `SetBehindTargetAction` would walk all
three stacks into one arc behind the boss the moment the ring node yields. `ThorimMovementGuardMultiplier`
holds the generic movers, scoped to a **settled** ring holder and exempting `AttackAction`,
`ReachTargetAction` and `AvoidAoeAction` — a permanent movement freeze is the Void Reaver failure.

### The opening

Thorim lands at (2134.68, -263.13), mid-camp, and over the first 13.5 s the melee pile chasing him to
the anchor passes within 8 yd of every camp spot. The first Chain Lightning lands **12.0-12.1 s** after
he drops below the floor line, the first cone at 15.9-16.1 s. That cast killed ranged walking into the
camp beside the pile, and the corridor squad's ranged dropping off the balcony stacked within a yard.

So ranged wait it out: the arena squad two to a spot on four north-east rim spots (12.8-15.6 yd clear
of the pile, in cast and heal range), the corridor squad on six platform spots 12 yd apart. The wait
ends, one way, at 12.5 s once Thorim is within 8 yd of the tank spot or the lit cone covers the wait
spot, and always by 25 s: Sif's bunny path runs 2.6-7.7 yd from the floor spots. No trace yet shows
whether Lightning Charge reaches players above z 430.

## The Charge Orb field is a 32.3 yd circle, not a 35 yd sphere

`EVENT_THORIM_CHARGE_ORB` fires 14 s into phase 1 and repeats every 16 s, casting **Charge Orb 62016**
at one of the seven **Thunder Orbs (33378)** (`conditions` row `(13,1,62016,…,33378)` restricts the
target). 62016 is a periodic trigger, period 1000 ms, **duration 15 000 ms**, trigger spell **62017**,
so the orb carries the aura for the whole window. **Lightning Shock 62017** is base 2830 + die 339 →
**~2831-3170 nature per tick** at `EffectRadiusIndex 21` = **35 yd**, not dispellable and with no cast
bar of its own.

**The radius test is 3D and the orbs float 13.5 yd above the floor**, so the 35 yd sphere cuts the
floor as a **32.3 yd circle** (`sqrt(35² − 13.5²)`) centred under the orb — the derivation behind
`ULDUAR_THORIM_CHARGED_ORB_RADIUS = 32.3f` and its 4.0 yd margin. Reading the DBC radius as a flat
distance puts the boundary 2.7 yd too far out. The trace shows the knife edge: at 1:14 a bot 34.9 yd
from the orb took every tick, while one 3 yd further east at 37.6 yd took none.

The seven Thunder Orb spawns are fixed, all at **z 433.3** and all **42.0 yd** from the arena centre
(`data/sql/base/db_world/creature.sql:148358-148370`): (2105.04, −292.56), (2092.95, −263.00),
(2104.94, −233.44), (2124.30, −222.60), (2145.50, −222.62), (2164.20, −233.47), (2164.55, −293.00).

Before anything read aura 62016, bots stood in the field for its full 15 s: Lightning Shock was
**50.3% of all damage taken** in one trace (503 097 over 340 hits).

## One Stormhammer debuffs the whole arena

**Stormhammer 62042** fires every 16 s at one random enemy within 100 yd (2451-2551 damage plus a 2 s
`MECHANIC_STUN`). `data/sql/base/db_world/spell_linked_spell.sql:562` —
`(62042, 62470, 1, 'Thorim - Stormhammer')`, type `SPELL_LINK_HIT` — makes the **hit unit** cast the
linked spell on itself with Thorim as original caster, so the blast is centred on the player the
hammer landed on, not on the boss. **Deafening Thunder 62470** is 4625-5376 nature at
`EffectRadiusIndex 14` = **8 yd**, plus `SPELL_AURA_HASTE_SPELLS` at **−75** at `EffectRadiusIndex 18`
= **15 yd**, duration **8 000 ms**. Four times the cast time, and not dispellable.

It cannot be dodged reactively: the target set is chosen inside `Spell::SelectSpellTargets` and is not
readable from a bot, and the aura is applied before anything can see it. **The only lever is how many
bots one 15 yd blast covers.** Every sample from 0:49 on used to put 11-14 of the 14 arena bots inside
a single 15 yd circle; the same clustering feeds Dark Rune Champion Whirlwind (15578, 8 yd).

## Hard mode

Sif is summoned every pull and normally channels, then despawns after the 150s dominion timer. If the
raid clears the gauntlet fast enough she joins instead and casts Frostbolt Valley (raid-wide,
unavoidable — healed through), Blizzard and Frost Nova (62605, teleport then point-blank).

**Blizzard is a trail, not a circle.** Every 36-41s a `NPC_SIF_BLIZZARD` 32879 spawns at
(2108.7, -280.04) and walks a fixed eight-waypoint loop for 30s. Its aura (62577/62603) drops a 10s, 8 yd
zone (62576 10-man, 62602 25-man) every 2s: up to six live, a median 26 yd behind it, hitting out to
10.8 yd. **Test the zones (`GetDynamicObjectPositions`), not the bunny** — the bunny alone missed 16 of
20 melee hits in one pull. Every camp home spot clears the loop by 12.9 yd or more.

**Detector: Sif (33196) alive AND `GetPositionZ() < 429.6`** — she spawns at the throne and only
`NearTeleportTo`s onto the arena floor when she joins. This reuses the same floor threshold the
normal-mode Thorim strategy already uses.

## The arena adds, and what to kill first

`GetThorimDpsTarget` buckets the encounter's creatures by entry in one sweep — acolytes, evokers,
champions, warbringers, commoners, and the corridor's Iron Ring and Iron Honor Guard — and hands each
bot a pick. It is deliberately **not** a raid icon: an icon is a sticky override that `RtiTargetValue`
hands back before the smart picker runs, so a wrong mark cannot be corrected until the bot leaves
combat (see [../../engine/pitfalls.md](../../engine/pitfalls.md)).

Measured share of arena damage taken: Dark Rune Warbringer 21.2% (melee 14.0%, Runic Strike **62322**
7.2%), Dark Rune Evoker 16.5% (Runic Lightning **62445**). Champions are melee, top of the list and
already standing on the bots — the generic picker used to send melee straight past them at an Evoker.

Dark Rune Commoners stack **Low Blow 62326**, whose second effect is
`SPELL_AURA_MOD_DAMAGE_PERCENT_DONE` at **−3% a stack**; one pull peaked at 25 stacks on a single bot,
i.e. −75% damage done. Only late and only on a few bots, so it is a reason not to ignore Commoners
forever rather than an explanation for a whole fight.

## Known gaps

Documented, not implemented: the in-combat `SPELL_SMASH` 62339 frontal cone (60 degrees, 3s cast — a
different spell from the corridor Runic Smash), Rune Detonation 62526, Stomp 62411,
Runic Fortification 62942, and arena tank pickup — nothing taunts an add off whoever it rolled.
Nothing recovers the fight once the arena squad is dead either; the 5 second scan leaves no room to
walk anyone back.

**The squad split counts human tanks.** `AssignThorimSquads` counts tanks with a bare
`PlayerbotAI::IsTank(member)` over the whole roster, so a human protection paladin makes `tankCount`
read 2 and the raid's only *bot* tank is sent down the corridor — leaving the arena tanked by the
human, which is also why a human shows up high on the arena damage meter. The fix is to count and
pick tanks bot-only (`IsBotPlayer(member) && PlayerbotAI::IsTank(member)`) and to skip tanks in the
DPS loop, since `IsDps` is also true for a protection paladin. **Excluded at the user's request,
twice — do not re-audit.**

**Burst cooldowns stay held for the whole of phase 1** — roughly 500 suppression episodes across one
arena squad: tinker 153, trinket 107, Blood Fury 58, Rapid Fire 28, Readiness 27, Blade Flurry 23,
Adrenaline Rush 18, Death Wish 16, Bestial Wrath 14, Army 13, Killing Spree 12, Recklessness 11,
Berserk 10. Recorded so it is not rediscovered.

**Pets deliberately do not dodge anything here** — they survive on their own resistances and damage
reduction, and a pet AI that steps out of hazards is a cost with no payoff. Gauntlet-squad pets are
likewise not leashed; only the arena squad's are.
