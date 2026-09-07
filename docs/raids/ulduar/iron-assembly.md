# Assembly of Iron

Killing one member restores the other two to full and hands them Supercharge (61920), so damage
spread across three health bars is thrown away and **focus fire is the encounter**. Each Supercharge
`SpellHit` calls `UpdatePhase()`, so the last one alive reaches `_phase == 3` — and `SetLootMode(0)`
is only undone there, so **all loot comes from whoever dies last**.

Kill order is the only thing `AiPlayerbot.UlduarIronAssemblyHardMode` changes: normally
**Steelbreaker → Molgeim → Brundir**, hard mode **Brundir → Molgeim → Steelbreaker**. Molgeim is
second either way, so Rune of Summoning is unreachable — deliberately, because
`npc_assembly_lightning` no-ops `AttackStart`, `MoveInLineOfSight` and `EnterEvadeMode`: the
Lightning Elementals have no threat table, so taunting and kiting both do nothing and only killing
them works. Guides calling them tauntable are wrong for this core.

`_phase` is private and Steelbreaker has no `GetData` override, so empowerment is inferred:
*Steelbreaker alive AND Molgeim dead AND Brundir dead*. It is **not** gated on the config flag — it
is a phase check, and gating it left a raid that reached the state without the option set with no
tank swap.

His empowered kit is Fusion Punch (61903/63493, a dispellable Magic DoT on the tank), Static
Disruption (61911/63495, a random target beyond 10 yd, 6 yd blast plus a 5 yd +75% nature
vulnerability), Overwhelming Power, and Electrical Charge (61902), +25% per player death. The first
two bot tanks trade him on **Overwhelming Power only** — Fusion Punch recurs far too fast, and
swapping on it would ping-pong the boss between them.

**Every ability is difficulty-mapped through `Unit::CastSpell`**, so each id is a 10/25 pair and
callers test both (`UldEncounter_IronAssembly.h`). Two corrections to the written guides: Lightning Tendrils is
**18 yd** (61886/63485), not the 10 of the 61884 dummy; and **Overwhelming Power (64637/61888) is
`DispelType 0`**, not dispellable. Its carrier dies to Meltdown (61889, 29,250 in 15 yd) regardless,
so the node walks them clear of the raid instead — every death it causes is another permanent +25%
Electrical Charge.

`creature_immunities`: **Brundir (`0x24CB375F`) is vulnerable to STUN and INTERRUPT but immune to
SILENCE** — kicks and stuns land, `silencing shot` and `spell lock` never do. Steelbreaker and
Molgeim (`0x26CB3F7F`) add STUN and INTERRUPT, so **Brundir is the only member worth an interrupt
node**. Lightning Whirl reaches 100 yd with no positional answer, so it takes the lowest-ranked
interrupter; Chain Lightning takes the second, and is deliberately allowed through when cooldowns are
thin.

Formation anchors on `(1587.18, 121.02, 427.27)`. navprobe: 8/8 headings clean at 20 and 30 yd, but
the 45° and 135° diagonals settle to Z −27.7 and −438 at 40, and three of eight leave the mesh at 50
— so **nothing sits outside 30 yd** and the slots use cardinals. Brundir parks at 28 yd, 38 from the
ranged stack: more than his Overload needs, so the stack never reacts to it and someone is always
parked and free to kick. **Tanks stand rather than drag**, so a boss follows its tank to the spot
instead of being towed through the raid.

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

**Two radii, not one.** DBC says 13, but the searcher applying the aura adds object size at both
ends and traces measured applications to **15.4 yd**: bots run inside **16**, stand at **21**. One
radius put the escape spot exactly on the boundary — the search returns the nearest point that
clears — and the aura came straight back, 31 times on one bot. The gap buys the invariant: every
raid-spot candidate is ≥21 from every rune and 21 > 16, so **arriving at the raid spot never
re-fires the escape**. Without it the two actions cancelled every tick; nobody travelled, nobody
parked, and a permanently-moving bot holds no interrupt duty, so Lightning Whirl took 15 of 29
killing blows.

Rune of Power lands on `DoSelectLowestHpFriendly`, i.e. on a **member**, not a player — so both
halves apply at once: the tank walks his boss off it while ranged and healers walk in. The soak is
capped at 25 yd, which admits every rune on Steelbreaker or Molgeim and rejects every one on Brundir
that would otherwise tow the ranged group into Overload.

Deliberate non-behaviours: **tanks hold through Overload** (20,000 nature is survivable in plate and
lethal in cloth, and under the normal order Brundir dies last, so his channel invincibility never
applies); **ranged spread only in hard mode**, since Static Disruption needs Steelbreaker's phase 2;
**bots never set the skull**, so a human's mark wins; and **the melee tank spots stay at 11 yd** —
against the 18 yd spread ring a spot `D` yd out has its nearest slot at `|18 − D|` and its furthest
at `D + 18`, so a slot clears the boss only at `D ≤ 10` or `D ≥ 26` while every slot stays in caster
range only at `D ≤ 12`; widening breaks the ring, and ranking the tanks fixes the same harm.

Nothing positional answers what actually kills raids here. **High Voltage** (61890 → 63525/63526) is
`EffectRadiusIndex 28` = **50,000 yd**, a whole-instance pulse every 3s on every member — 58–65% of
all damage taken in two traced pulls. It scales with Supercharge and then with Electrical Charge, so
a phase-3 Steelbreaker compounds: per-hit went 1.7k → 2.9k → 13.2k as the other two died and deaths
fed charges, taking 24 of 32 killing blows in 23 seconds. Neither is handled positionally — a
stacking raid-wide buff is a healer and kill-speed problem, not a movement one.

**Reading a pull:** `postmortem.py <file> --notes ironassembly.` — `alive` (bit 0 Steelbreaker, 1
Molgeim, 2 Brundir), `focus` (what the raid is killing, and whether a human's skull beat the order),
`tank` (the boss a bot tank owns, or the branch that left it none — bot tanks only, so no row for a
human one), `interrupt` (the duty a bot holds for Brundir's current cast), `spot` (the formation
branch — a `-rune` or `-overload` suffix names the hazard that shifted the stack), `slot` (its index
on the spread ring), `soak` (whether it reached Rune of Power, and what stopped it). Overload,
Lightning Tendrils and Meltdown have no world object, so they also write `haz` circles with the spell
radius and the clearance; Rune of Death and Rune of Power do, and are swept instead — only a swept
hazard is tested against a death.

Core-version assumption: the strategy relies on recent upstream fixes — `#26470` (Rune of Death
restricted to players), `#26449` (Brundir surviving Tendrils), `#26200` (Static Disruption preferring
ranged) and `#25029` (Overload invincibility). An older AzerothCore behaves differently.

