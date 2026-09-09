# Assembly of Iron

Killing one member restores the other two to full and hands them Supercharge (61920), so damage
spread across three health bars is thrown away and **focus fire is the encounter**. Each Supercharge
`SpellHit` calls `UpdatePhase()`, so the last one alive reaches `_phase == 3` — and `SetLootMode(0)`
is only undone there, so **all loot comes from whoever dies last**.

**Pets follow the focus too.** `PetAttackAction`'s node is commented out globally, so a pet keeps
whatever it first latched onto — two sat on Steelbreaker through the whole Molgeim phase, 124 of 958
pet casts on the wrong member, and Supercharge hands that damage straight back. `CommandPetAttack`
runs from the focus action every tick and **above its early return**, since already being on the
right member is the common case.

Kill order is the only thing `AiPlayerbot.UlduarIronAssemblyHardMode` changes: normally
**Steelbreaker → Molgeim → Brundir**, hard mode **Brundir → Molgeim → Steelbreaker**. Molgeim is
second either way, so Rune of Summoning is unreachable — deliberately, because
`npc_assembly_lightning` no-ops `AttackStart`, `MoveInLineOfSight` and `EnterEvadeMode`: the
Lightning Elementals have no threat table, so taunting and kiting both do nothing and only killing
them works. Guides calling them tauntable are wrong for this core.

`_phase` is private and Steelbreaker has no `GetData` override, so empowerment is inferred:
*Steelbreaker alive AND Molgeim dead AND Brundir dead*. It is **not** gated on the config flag — it
is a phase check, and gating it left a raid that reached the state without the option set with no
tank on the empowered boss.

His empowered kit is Fusion Punch (61903/63493, a dispellable Magic DoT on the tank; traced dispels
strip it inside 0.6s), Static Disruption (61911/63495, a random target beyond 10 yd, 6 yd blast plus
a 5 yd +75% nature vulnerability), Overwhelming Power, and **Electrical Charge (61902), which is
+25% damage *and* a heal** — `Effect_2 = 136`, measured at 9.7% of his 12.05M bar on every phase-3
death without exception. A corpse is worth ~1.17M to him, so the phase is decided by how few the
raid feeds him: traced, the raid takes him to **3.08% with all 25 alive** — only the tank dies, and
only to the guaranteed instakill — then loses 22 people in 20s as the corpses heal him back to full.

**Every ability is difficulty-mapped through `Unit::CastSpell`**, so each id is a 10/25 pair and
callers test both (`UldEncounter_IronAssembly.h`). Two corrections to the written guides: Lightning Tendrils is
**18 yd** (61886/63485), not the 10 of the 61884 dummy; and **Overwhelming Power (64637/61888) is
`DispelType 0`**, not dispellable.

**Overwhelming Power goes to `GetVictim()`**, never a random player: 8s into phase 3 and every 36s
after (25-man; 61s in 10), lasting **35s**, and its `EffectTriggerSpell_2` is Meltdown 61889 —
29,250 nature in 15 yd, plus an **INSTAKILL** on the carrier. So the raid loses whoever is tanking
every 36s and pays ~1.17M for it. **Only the carrier dies to Meltdown** — traced blasts caught 5–16
others for 3,600–26,800 after resists and every one lived, at 39–100% — and running from it costs far
more, because the carrier is the tank: a run-out towed Steelbreaker 180 yd, cut melee uptime to a 16%
median and left the raid doing **0.4% of a 12.05M bar in a 30s phase**. So **the carrier holds him
to the end and the off-tank inherits on threat**, already second on the table. Taunting early saves
nobody — the buff kills its target either way and the next cast lands a second later on whoever
inherited — so all it buys is a boss that moves. Both ranked tanks are assigned the empowered
Steelbreaker, so the tank node **taunts only when the current victim is not a group tank**: the tick
after a carrier dies, and the path that recovers him when a dps rips threat at the transition.

`creature_immunities`: **Brundir (`0x24CB375F`) is vulnerable to STUN and INTERRUPT but immune to
SILENCE** — kicks and stuns land, `silencing shot` and `spell lock` never do. Steelbreaker and
Molgeim (`0x26CB3F7F`) add STUN and INTERRUPT, so **Brundir is the only member worth an interrupt
node**. Lightning Whirl reaches 100 yd with no positional answer, so it takes the lowest-ranked
interrupter; Chain Lightning takes the second, and is deliberately allowed through when cooldowns are
thin.

**One encounter state per instance, shared behind a mutex — never `thread_local`.** Spread slots,
the shift heading and the alive mask must agree across the raid. `MapUpdate.Threads` is 6 and a map
is never pinned to a thread, so per-thread copies hand one pull six independent states: two bots
held slot 0 at once, 11 bots took 71 slot assignments in 4.5s, 10 of 16 slots were ever used, and
each ranged bot chased 4–6 destinations for 285–334 yd in a 50s Steelbreaker phase. `ObsValue` emits
only on change, so **six identical `ironassembly.alive` rows per transition is the signature**.
Mimiron and Flame Leviathan hold theirs this way; Thorim, Ignis, Hodir and the EoE caches are still
`thread_local` and unaudited.

Formation anchors on `(1587.18, 121.02, 427.27)`. navprobe: 8/8 headings clean at 20 and 30 yd, but
the 45° and 135° diagonals settle to Z −27.7 and −438 at 40, and three of eight leave the mesh at 50
— so **nothing anchor-centred sits outside 30 yd** and the slots use cardinals; a ring on another
centre earns its own probe. Brundir parks at 28 yd, 38 from the ranged stack: more than his Overload
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
escapes alone.

**The spread ring centres on Steelbreaker's tank spot, at 22 yd.** Meltdown lands on whoever is
tanking him, so a ring built on the stack point runs its near arc 6.6 yd from the carrier: 5 of 16
slots sat inside the 15 yd blast and it caught the same five healers and ranged on every cast.
Boss-centred, every slot is 22 and they sit 8.6 yd apart rather than 7.0, which Static Disruption
wants anyway. navprobe on the bot filter: 21, 22 and 23 settle on the floor at all 16 headings from
every bearing the tank spot can rotate to, while **24 drops the 135° slot into a hole at Z −438**
(read `settledZ` — the trailing `16/16 on mesh` prints for 24 too), and the ring reaches 38 yd from
the anchor. It alone does not ride the stack displacement: Brundir and Molgeim are dead by this
phase, so a rune outliving Molgeim goes to the per-bot escape, which is all melee have ever had.

**Two radii, not one.** DBC says 13, but the searcher applying the aura adds object size at both
ends and traces measured applications to **15.4 yd**: bots run inside **16**, stand at **21**. One
radius put the escape spot exactly on the boundary — the search returns the nearest point that
clears — and the aura came straight back, 31 times on one bot. The gap buys the invariant: every
raid-spot candidate is ≥21 from every rune and 21 > 16, so **arriving at the raid spot never
re-fires the escape**. Without it the two actions cancelled every tick; nobody travelled, nobody
parked, and a permanently-moving bot holds no interrupt duty, so Lightning Whirl took 15 of 29
killing blows.

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

Rune of Power lands on `DoSelectLowestHpFriendly`, i.e. on a **member**, not a player. Ranged and
healers walk in, capped at 25 yd of travel, which admits every rune on Steelbreaker or Molgeim and
rejects every one on Brundir that would otherwise tow them into Overload. **The tank's answer is his
spot, not a second node.** A separate drag-out pushed the boss away at `MOVEMENT_COMBAT`, the same
priority as the tank spot, so the two traded the move slot and cancelled: 339 flip-flops in one
pull, five seconds of alternating `wait`, a drag target receding a yard a tick because it was
recomputed from the bot's live position — and Brundir never left the rune at all, so Overload fired
on the melee standing in it. Now the spot itself rotates off the designed bearing in 22.5° steps at
the boss's own radius until it is **12 yd** clear: the 5 yd rune, plus the 5 the boss stops short at
behind the tank, plus margin. The rune is swept as an object (**63513**, the only id in the chain
with a persistent area aura) — aiming at the carrier's feet makes the destination follow him and
never settle. navprobe: the 16 and 28 yd rings are both 16/16 on mesh at 22.5° steps. **Three steps,
not four** — the melee spots are 90° apart, so a fourth puts Steelbreaker's furthest candidate on
Molgeim's; three still clears 12 yd on either ring and keeps every Brundir candidate 33 yd off the
stack.

Deliberate non-behaviours: **tanks hold through Overload** (20,000 nature is survivable in plate and
lethal in cloth, and under the normal order Brundir dies last, so his channel invincibility never
applies); **ranged spread only in hard mode**, since Static Disruption needs Steelbreaker's phase 2;
**bots never set the skull**, so a human's mark wins; **melee stand in Meltdown**, being on the boss,
and every traced blast left them alive; and **the melee tank spots stay 11 yd off the stack**, which
keeps healers covering the tanks and the stack at once.

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

**The hold matches on action name as well as type, because neither predicate is complete.**
`IsDpsCooldownAction` is a `dynamic_cast` chain with no case for a potion, a tinker or any priest
cooldown, and its racial branch keys on `getRace()` while `RacialsStrategy` arms on `HasSpell` —
these bots carry Blood Fury and Berserking as Humans, Dwarves, Night Elves and Draenei, so it can
never match. One pull spent **17 potions, 69 tinkers and 14 Blood Furies** on the first two bosses.
`IsBurstCooldownAction`'s registry names all of those and misses nine the chain catches, so the hold
takes either, with `IsDps` on the name branch since a healer's Shadowfiend is mana, not burst.

**Reading a pull:** `postmortem.py <file> --notes ironassembly.` — `alive` (bit 0 Steelbreaker, 1
Molgeim, 2 Brundir; **one row per transition — more means the state is not shared**), `focus` (what
the raid is killing, and whether a human's skull beat the order), `tank` (the boss a bot tank owns,
or the branch that left it none — bot tanks only, so no row for a human one), `interrupt` (the duty
a bot holds for Brundir's current cast), `spot` (the formation branch — a `-rune` or `-overload`
suffix names the hazard that shifted the stack, and a bare `spread` is the boss-centred ring, which
never shifts), `slot` (its index on the spread ring, assigned
**once** per bot), `soak` (whether it reached Rune of Power, and what stopped it). Overload,
Lightning Tendrils and Meltdown have no world object, so they also write `haz` circles with the
spell radius and the clearance bots keep — equal for Meltdown, which nobody dodges. Rune of Death
and Rune of Power do, and are swept instead — only a swept hazard is tested against a death.

Core-version assumption: the strategy relies on recent upstream fixes — `#26470` (Rune of Death
restricted to players), `#26449` (Brundir surviving Tendrils), `#26200` (Static Disruption preferring
ranged) and `#25029` (Overload invincibility). An older AzerothCore behaves differently.

