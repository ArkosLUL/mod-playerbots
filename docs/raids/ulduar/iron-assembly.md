# Assembly of Iron

Killing one member restores the other two to full and hands them Supercharge (61920), so damage
spread across three health bars is thrown away and **focus fire is the encounter**. Each Supercharge
`SpellHit` calls `UpdatePhase()`, so the last one alive reaches `_phase == 3` — and `SetLootMode(0)`
is only undone there, so **all loot comes from whoever dies last**.

**Pets follow the focus too** ([pet sender](../../engine/action-selection.md#multiplier-semantics)).
Uncommanded, two pets sat on Steelbreaker through the whole Molgeim phase, 124 of 958 pet casts on the
wrong member, and Supercharge hands that damage straight back. `CommandPetAttack` runs from the focus
action every tick and **above its early return**, since already being on the right member is the
common case. **Known-open:** `IronAssemblySetDpsPriorityTrigger::IsActive` bails
for tanks, so a *tank's* ghoul is never redirected. Closing it means six wiring sites and no trace
has shown it costing anything, so it is left open on purpose.

Kill order is the only thing `Playerbots.UlduarIronAssemblyHardMode` changes: normally
**Steelbreaker → Molgeim → Brundir**, which takes Fusion Punch off the tank earliest and never lets his
phase 3 kit come up, hard mode **Brundir → Molgeim → Steelbreaker**. Molgeim is
second either way, so Rune of Summoning is unreachable — deliberately, because
`npc_assembly_lightning` no-ops `AttackStart`, `MoveInLineOfSight` and `EnterEvadeMode`: the
Lightning Elementals have no threat table, so taunting and kiting both do nothing and only killing
them works. Guides calling them tauntable are wrong for this core.

`_phase` is private and Steelbreaker has no `GetData` override, so empowerment is inferred:
*Steelbreaker alive AND Molgeim dead AND Brundir dead*. It is **not** gated on the config flag — it
is a phase check, and gating it left a raid that reached the state without the option set with no
tank on the empowered boss.

His empowered kit is Fusion Punch (61903/63493, a dispellable Magic DoT on the tank; traced dispels
strip it inside 0.6s), Static Disruption (61911/63495, a random target beyond 10 yd, triggering
61912/63494: a 6 yd blast of 5,000 / 7,000 nature plus a 5 yd +75% nature vulnerability), Overwhelming Power, and **Electrical Charge (61902), which is
+25% damage *and* a heal** — `Effect_2 = 136`, measured at 9.7% of his 12.05M bar on every phase-3
death without exception. A corpse is worth ~1.17M to him, so the phase is decided by how few the
raid feeds him: traced, the raid takes him to **3.08% with all 25 alive** — only the tank dies, and
only to the guaranteed instakill — then loses 22 people in 20s as the corpses heal him back to full.

**Every ability is difficulty-mapped through `Unit::CastSpell`**, so each id is a 10/25 pair and
callers test both (`UldEncounter_IronAssembly.h`). Two corrections to the written guides: Lightning Tendrils is
**18 yd** (61886/63485), not the 10 of the 61884 dummy; and **Overwhelming Power (64637/61888) is
`DispelType 0`**, not dispellable.

**Overwhelming Power goes to `GetVictim()`**, never a random player: 8s into phase 3 and every 36s
after, lasting **35s** (25-man 61888; 10-man 64637 recasts at 61s, lasts 60s). Its
`EffectTriggerSpell_2` is Meltdown 61889, **cast by the carrier**: 29,250 nature to its allies within
15 yd centre to centre, plus an **INSTAKILL** on the carrier. So the raid loses whoever is tanking
every cycle and pays ~1.17M for it either way; held in place, both 17 Oct Meltdowns also hit the
other tank, 8 melee for 7.9–11.4k and 16 pets and totems.

**So the pair swaps and the carrier dies alone.** Both ranked tanks share the empowered Steelbreaker
and stand on the same spot, so the swap moves no boss. With **≤ 10 s** of the buff left on a grouped
tank holding him, the other assigned bot tank taunts (`ironassembly.meltdown` `taunt`); once he is off
the carrier, the carrier walks to the **Meltdown point**, anchor + 30 yd @ 315° = (1608.39, 99.81):
46 yd from his spot, 24 off the spread ring, empty in phase 3, navprobe on the floor with a straight
path (`run` → `parked`). A swapped carrier drops out of the tank ranking, so the next bot tank becomes
the partner, and counts as a hazard mover, so the movement guard keeps attack and reach from pulling it
back. The run claims every tick at `MOVEMENT_FORCED`: after a taunt both threats match, and a carrier
that swings takes him back. Fallbacks hold where the tank stands: a swap landing with less than walk
time + 1 s left (`late`; dying halfway puts the blast on the ranged ring instead of the melee, and a
started walk is latched in `ironassembly.meltdownrun` so a detour can't stop it), and no second bot tank
(`nopartner`; a human partner never taunts). Running **without** a swap is the trap: it towed
Steelbreaker 180 yd, cut melee uptime to a 16% median and left the raid doing **0.4% of a 12.05M bar
in a 30s phase**.

Outside the swap the tank node **taunts only when the current victim is not a group tank**: the tick
after a carrier dies, and the path that recovers him when a dps rips threat at the transition. The
class nodes (`hand of reckoning`, `righteous defense`) bypass that node, so **`iron assembly taunt
guard` zeroes any `IsTauntAction` while another grouped tank holds him**, the swap taunt excepted — no
current-target gate, since Righteous Defense targets the ally. Unguarded, a soulstoned bot tank taunted
him off a human carrier, died 9s later and left nobody to inherit.

`creature_immunities`: **Brundir (`0x24CB375F`) is vulnerable to STUN and INTERRUPT but immune to
SILENCE** — kicks and stuns land, `silencing shot` and `spell lock` never do. Steelbreaker and
Molgeim (`0x26CB3F7F`) add STUN and INTERRUPT, so **Brundir is the only member worth an interrupt
node**. Lightning Whirl reaches 100 yd with no positional answer, so it takes the lowest-ranked
interrupter; Chain Lightning takes the second, and is deliberately allowed through when cooldowns are
thin.

**One encounter state per instance, in a `RaidInstanceState` — never `thread_local`**
([../README.md](../README.md#per-instance-state); the signature is in
[coordinating a raid](../../engine/raid-mechanics-lessons.md#coordinating-a-raid-with-no-shared-state)). Spread slots, the
shift heading and the alive mask must agree across the raid, and per-thread copies handed one pull
six independent states: two bots held slot 0 at once, 11 bots took 71 slot assignments in 4.5s, 10
of 16 slots were ever used, and each ranged bot chased 4–6 destinations for 285–334 yd in a 50s
Steelbreaker phase.

Formation anchors on `(1587.18, 121.02, 427.27)`. navprobe: 8/8 headings clean at 20 and 30 yd, but
the 45° and 135° diagonals settle to Z −27.7 and −438 at 40, and three of eight leave the mesh at 50
— so **nothing anchor-centred sits outside 30 yd**, where every heading is clean; a ring on another
centre earns its own probe. `ULDUAR_IRON_ASSEMBLY_ARENA_RADIUS` 78 stops short of both hall doors,
(1671.31, 120.70) and (1501.49, 119.70), 84 and 86 yd out. Brundir parks at 28 yd, 38 from the ranged stack: more than his Overload
needs, so the stack never reacts to it and someone is always parked and free to kick. **Tanks stand
rather than drag**, so a boss follows its tank to the spot instead of being towed through the raid —
and **`tank face` is vetoed for a tank holding an assignment**, being a second mover at the same
`MOVEMENT_COMBAT` priority as the spot: the two alternated every second between the designed spot and
a point 6.8 yd nearer the raid, 45–57 destination flips and 385–515 yd of wandering destination in
one phase, which also dragged the Meltdown centre in over the ranged. `set facing` is a separate
node, so facing is untouched.

**Tanks rank among bots only**, by guid, and claim from the front of **Brundir → Steelbreaker →
Molgeim**. Group role flags count humans, and a human holding a slot left its boss untowed: bots
cannot know a human is tanking, and a slot they cannot fill is worse than none. A lone bot tank still
takes one, and it is **Brundir** — it cannot split three bosses but need not, since the unheld two
drift onto the only real threat on the floor anyway. So the assignment really decides **where the
council parks**: Brundir's spot is 38 yd off the stack, the melee spots 11. Two traced pulls differed
in nothing else — with the lone tank on Steelbreaker, 5 of 5 Overloads covered the stack and ranged
stood within 10 yd of a boss 77% of the time; on Brundir, 0 of 4 and 11%. Guid ranking renumbers on a
tank death, which is wanted: that is when someone must pick up Brundir.

**Hazards land on the stack point.** `DoCastRandomTarget` puts Rune of Death on a random member, so
on a stacked raid it covers the stack — four of four in one traced pull, one exactly on it — and it
repeats every 30–40s for ~30s from Molgeim's phase 2. Overload rides Brundir, so a loose one covers
the stack too: five of five in another pull, 22–24 of 25 members inside the circle at cast. So the
**raid spot answers both**: a covered stack shifts to the nearest clear of the eight anchor headings
at 25 yd (navprobe 8/8 on mesh), each hazard tested at its own clearance — 21 for a rune, 25 for
Overload. Derived from the stack and the hazards alone, never the caller — every bot computes it, and
reading the caller's position would scatter the raid instead of moving it. **The heading latches**
until it stops clearing: a rune holds still, but Overload walks with Brundir, and re-picking every
tick would flip the winner mid-cast. With no heading clear the raid holds formation and each bot
escapes alone. **Known-open:** on the exact stack point a rune lands 0 yd from every caster, and one
killed four casters in one pull and a healer in another at 2:47 — at 50–65% they started moving
0.2–2.2s late (cause unverified) and died walking out.

**The spread ring centres on Steelbreaker's tank spot, at 22 yd.** Meltdown lands on whoever is
tanking him, so a ring built on the stack point runs its near arc 6.6 yd from the carrier: 5 of 16
slots sat inside the 15 yd blast and it caught the same five healers and ranged on every cast.
Boss-centred, every slot is 22 and they sit 8.6 yd apart rather than 7.0, which Static Disruption
wants anyway. navprobe on the bot filter: 21, 22 and 23 settle on the floor at all 16 headings from
every bearing the tank spot can rotate to, while **24 drops the 135° slot into a hole at Z −438**
(read `settledZ` — the trailing `16/16 on mesh` prints for 24 too), and the ring reaches 38 yd from
the anchor. It alone does not ride the stack displacement, Brundir and Molgeim being dead, but
**Molgeim's last rune outlives him** on the stack point, 11.4 yd off the tank spot and so inside the
ring: 4 of 4 traced phase-3 pulls, alive 2.9–20.6s into it. Left to the per-bot escape, escape and
raid spot traded the ranged every 1.5s (89 flips against 13 in the kill) and Steelbreaker stood at
79–84% at +20s against the kill's 77%. So **the slots squeeze evenly into the arc the rune leaves
clear at 21** (`spread-rune`): rotating only the covered ones to the nearest clear bearing lands a
side's worth on one point, which is what Static Disruption's 6 yd blast wants. Melee still have
only the escape. navprobe: 64/64 headings at 22 round the designed spot settle on the floor.

**Two radii, not one.** DBC says 13, but the searcher applying the aura adds object size at both
ends and traces measured applications to **15.4 yd**: bots run inside **16**, stand at **21**. One
radius put the escape spot exactly on the boundary — the search returns the nearest point that
clears — and the aura came straight back, 31 times on one bot. The gap buys the invariant: every
raid-spot candidate is ≥21 from every rune and 21 > 16, so **arriving at the raid spot never
re-fires the escape**. Without it the two actions cancelled every tick; nobody travelled, nobody
parked, and a permanently-moving bot holds no interrupt duty, so Lightning Whirl took 15 of 29
killing blows.

**The invariant covers paths too.** A straight walk to a clear spot can still cut a rune's trigger
radius, and the escape turns the bot back at its edge — 10–12 of 14 ranged paths did at the phase-3
transition. So both formation movers, raid spot and tank spot, step through
`TryGetIronAssemblyRouteStep`: a line or goal inside **18.5** of a rune goes round it via points on
its 21 yd circle at fixed 45° bearings, the short way, each chord staying 19.4 out. 18.5 sits halfway
because a line that only clears 16 grazes it and walks are not straight; goals share it, or one
between the radii never clears and the detour circles forever. A walk whose nearest point is the bot
itself only moves away and never counts as blocked — counting it spirals a bot inside 18.5 inward. A
covered goal, or a detour point inside a second rune, holds. **The tank holds at his escape spot**
while a rune covers his: the ring stays on his spot, leaving its near arc ~12 yd from him inside
Meltdown, safe only because a rune lasts 30s and the first Meltdown lands 43s into the phase.
navprobe: 8/8 on the 21 yd circle round the stack point.

**Escapes carry a clearance per hazard and break ties toward the bot's station** — its raid spot,
its tank spot, or whatever the raid is killing. The sweep rings outward from the bot and takes the
first angle that clears, a fixed compass direction from wherever it stands, so the answer slides as
it walks: 21–26% of consecutive escape destinations jumped over 5 yd, one bot chasing 25. A station
holds still while the bot moves, which is what lets one answer keep winning. The per-hazard
clearance matters in the same call: an Overload escape folds in the runes, and one clearance for the
whole vector cleared them at 25 instead of 21 — four wasted yards each, walked inside a 5.5s cast.

**Gap-closers need their own guard.** Charge, Intercept and both Feral Charges are
`CastReachTargetSpellAction` — a `CastSpellAction`, *not* a `MovementAction` — so the movement guard
cannot see them and the server's spell effect translocates the bot with no `MovementPriority`
involved. One `dynamic_cast` to that base catches all four; the ICC and Ruby Sanctum enumerations of
concrete classes each miss one. It holds for the **whole** Overload or Tendrils channel, not just
while the bot is inside the circle: the escape parks it at 16–19 yd and both spells reach 25, so
releasing on "out of the blast" hands the ability back at exactly the range that undoes the dodge.
Traced: the three melee that cast one during an Overload were the only non-tanks that took Overload
damage, one more than the tank; the five without took none.

**Rune of Power is a creature, not a DynamicObject.** Molgeim casts 61973 on `DoSelectLowestHpFriendly`
(a **member**, usually the focus), which force-casts 63513, a summon of NPC **33705** at the target's
feet. The NPC carries 61974 (`creature_template_addon`) for 60s, pulsing 64320 (+50% damage, 0.8s)
every 0.5s at **5 yd, 2D, centre to centre**: `TARGET_UNIT_DEST_AREA_ENTRY` from a non-player caster
adds no combat reach, and `conditions` admit every unit but the rune. Sweep it by entry and require
61974, since the NPC can outlive its aura; a `GetDynamicObjectPositions` sweep for 63513 finds nothing.
That sweep shipped once, so the tank shift never ran: on 17 Oct Brundir stood in his own rune for
112–115 of 120 pulses, and each soak aimed at the buffed boss's feet, then lost him 0.8s after he
stepped out and walked back to the raid spot (ranged 0–7 pulses on most runes).

Combat reach decides who fits: Molgeim 5, Steelbreaker 8, Brundir 1.5, players 1.5, so melee stand
outside 5 yd of a big boss's centre and a rune under him misses them (2–26 pulses on Molgeim's
third). Melee range is both reaches + 4/3 (7.83 / 10.83 / 5.0); a chasing boss stays put until his
tank leaves that range, then stops at both reaches + 0.5.

- **A boss's rune** is the live one nearest his designed tank spot within 15 yd (spots ≥ 22.6 apart).
  Keyed on the spot, never his buff: the buff drops the moment he steps out, and gating on it walks him
  back in.
- **The tank's answer is his spot, not a second node.** A separate drag-out at `MOVEMENT_COMBAT`
  traded the move slot with the tank spot: 339 flip-flops in one pull, a drag target receding a yard a
  tick because it was recomputed from the bot's live position. The spot moves to `rune + u · (6.5 +
  boss reach + 1.5 + 0.5)`, `u` from the rune toward the designed spot: fixed inputs, so it holds
  still, and the boss ends **6.5 yd** off the rune facing the tank, rune behind him. navprobe: every
  spot this yields (up to 16.5 yd round each designed spot) is on the floor.
- **Melee** (non-tank, not `IsRanged`) step to `rune + 3 yd toward the focus`: 2 yd inside the edge,
  3.5 from a parked boss, behind him so nothing parries. Only while that spot is in melee range − 1,
  ≥ 21 from every Rune of Death, and Brundir isn't in Overload or Tendrils; held while within 4.5 of
  the rune and in melee range.
- **Ranged and healers**: the rune *is* the raid spot (`rune-power`), one mover rather than a soak
  node, when within 25 yd of the **opening** stack point (every rune on Steelbreaker or Molgeim, none
  on Brundir, 38 yd off in Overload range), ≥ 21 from every Rune of Death and outside a live Overload.
  Deadband 2 / 4, both inside the rune. Never in the spread phase: one Static Disruption on a stack is
  +75% nature taken for 20s, and High Voltage is nature.

**Known-open:** Static Disruption starts at Steelbreaker's phase 2, which in hard mode is Molgeim's
phase, while the raid stacks: 14 and 10 hits per cast in a 17 Oct wipe, 1 once spread.

Deliberate non-behaviours: **tanks hold through Overload** (20,000 nature is survivable in plate and
lethal in cloth, and under the normal order Brundir dies last, so his channel invincibility never
applies); **ranged spread only in hard mode**, since Static Disruption needs Steelbreaker's phase 2;
**bots never set the skull**, so a human's mark wins; **melee never dodge Meltdown**, which a swap
takes off the boss and a failed one leaves on them (every traced blast but one left them alive); and
**the melee tank spots stay 11 yd off the
stack**, which keeps healers covering the tanks and the stack at once.

**The 11 yd is forced, and that is why `MELEE_BOSS_RADIUS = 16` and `SPREAD_RING_RADIUS = 18` must not
be widened.** For a melee spot `D` yd from the stack, the nearest spread slot is `|18 − D|` away and
the furthest `D + 18`. Keeping a slot clear of the boss needs `D ≤ 10` **or** `D ≥ 26`; keeping every
slot inside caster range needs `D ≤ 12`. Only the narrow band around 11 satisfies both, so nudging
either radius breaks one of the two silently. `SPREAD_RING_RADIUS = 18` is still live in its own
right: it is the **stack-centred** ring used when nobody is empowered, and the fallback whenever the
boss-centred spot cannot be derived.

The points the rest of this geometry is stated against, derived off the anchor: the stack is
`anchor + 10 yd @ 180°` = **(1577.18, 121.02)**, Steelbreaker's tank spot is `anchor + 16 yd @ 135°`
= **(1575.87, 132.33)**, and Brundir's is **(1615.18, 121.02)**. The navprobe rings and the 5-of-16
Meltdown overlap were both measured at those.

**Melee uptime measured by an 8 yd proximity proxy is a lie here — do not re-audit it on that.** It
read a 2% median while those melee were parked 9.3 yd out, which is inside melee range of a boss this
size. Cast rate on the boss is the honest measure, and it is flat across pulls: melee **2.8-3.0/s**,
ranged **0.93-1.07/s**.

Nothing positional answers what actually kills raids here. **High Voltage** (61890 → 63525/63526) is
`EffectRadiusIndex 28` = **50,000 yd**, a whole-instance pulse every 3s on every member — 58–65% of
all damage taken in four traced pulls, and **72–86% with 63 of 90 killing blows** once the phase
runs its full length. It scales with Supercharge and then with Electrical Charge, so a phase-3
Steelbreaker compounds: per-hit went 1.7k → 2.9k on Supercharge, then 3.8k → 6.1k → 8.8k → **28.7k**
as corpses fed charges. Neither is handled positionally — a stacking raid-wide buff is a healer and
kill-speed problem, not a movement one. The one lever bots have is time in that phase, so **in hard
mode every DPS cooldown, trinket, racial, potion, tinker and Bloodlust is held until Steelbreaker is
the last one standing**. Held burst costs nothing when three bars are one pool, and two traced pulls
lost 21 of 31 and 20 of 27 deaths inside it. Traced release: Heroism goes out 3.6–4.3s into the
phase. Hard mode only — the normal order kills him first, so it would never release.

**One round goes at the pull**, by the user's call, to speed the fight up: while all three are up and
within 20 s of the engage stamp (`engagedAtMs`, cleared once the council is back at full health), trinkets,
tinkers and any burst spell whose talented cooldown (`SpellCooldownMs`) is **≤ 3 min**. Every allowed
cooldown is ≥ 60 s, so the window makes it once; a 3 min one spent at ~8 s is back ~18 s after
Steelbreaker is alone (~170 s in). Potions, Heroism/Bloodlust (Sated) and longer cooldowns (Army of the
Dead, Fire Elemental, 5 min ones) stay held.

**A guaranteed tank death re-vetoes the release, which is a known and accepted interaction.**
`TankHasHeldBoss` needs `boss->GetVictim()` to be a grouped tank *continuously* for 5 s — 3 s for lust
— and any gap resets the dwell (the generic rule is in
[../../systems/consumables-and-burst.md](../../systems/consumables-and-burst.md)). Here a tank dies
about every 36 s, so the dwell re-arms and burst is vetoed again mid-phase: 44 and 36 vetoes at the
transition, **none from 210 to 235 s**, then 51 / 35 / 60. Left alone until the tank stops travelling,
at which point it is worth re-measuring rather than tuning the dwell.

**A priest's `shadowfiend` is exempt from this hold** — it is a mana return, and shadow carries no
mana potions, so holding it for the phase starved the priest into drinking one and losing its
offensive potion for the Steelbreaker phase (traced: 19 vetoes, one cast). The generic rule is in
[../../systems/consumables-and-burst.md](../../systems/consumables-and-burst.md).

**The hold matches on action name as well as type, because neither predicate is complete.**
`IsDpsCooldownAction` is a `dynamic_cast` chain with no case for a potion, a tinker or any priest
cooldown, and its racial branch keys on `getRace()` while `RacialsStrategy` arms on `HasSpell` —
these bots carry Blood Fury and Berserking as Humans, Dwarves, Night Elves and Draenei, so it can
never match. One pull spent **17 potions, 69 tinkers and 14 Blood Furies**, and both priest
cooldowns, on the first two bosses.
`IsBurstCooldownAction`'s registry names all of those and misses nine the chain catches, so the hold
takes either, with `IsDps` on the name branch since a healer's Shadowfiend is mana, not burst.

**Reading a pull:** `postmortem.py <file> --notes ironassembly.` — `alive` (bit 0 Steelbreaker, 1
Molgeim, 2 Brundir; **one row per transition — more means the state is not shared**; the snapshot
stream shows a death only as a boss row that stops appearing), `focus` (what
the raid is killing, and whether a human's skull beat the order), `tank` (the boss a bot tank owns,
or the branch that left it none — bot tanks only, so no row for a human one), `interrupt` (the duty
a bot holds for Brundir's current cast), `spot` (the formation branch — a `-rune` or `-overload`
suffix names the hazard that shifted the stack, a bare `spread` is the boss-centred ring and
`spread-rune` that ring squeezed off a rune, `rune-power` standing in a Rune of Power), `slot` (its
index on the spread ring, assigned **once** per bot), `meleerune` (`in` / `walk`, or `none:` what
stopped it), `meltdown` (a tank's swap role: `hold` / `nopartner` / `taunt` / `run` / `parked` /
`late`), `meltdownrun` (the latched walkers). Overload,
Lightning Tendrils and Meltdown have no world object, so they also write `haz` circles with the
spell radius and the clearance bots keep — equal for Meltdown, which nobody dodges. Rune of Death
and Rune of Power do, and are swept instead — only a swept hazard is tested against a death.

`IronAssemblyScan` ([per-bot scan](../../engine/raid-mechanics-lessons.md#what-a-strategy-costs-per-raid))
caches the three members and the Rune of Death sweep per bot per tick: a ranged bot asks for a member
about a hundred times a tick, each `possible targets no los` read a 100 yd sweep plus a vector copy.

Core-version assumption: the strategy relies on recent upstream fixes — `#26470` (Rune of Death
restricted to players), `#26449` (Brundir surviving Tendrils), `#26200` (Static Disruption preferring
ranged) and `#25029` (Overload invincibility). An older AzerothCore behaves differently.

