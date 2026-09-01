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
vulnerability), Overwhelming Power, and Electrical Charge (61902), +25% per player death. Main tank
and assist 0 trade him on **Overwhelming Power only** — Fusion Punch recurs far too fast, and
swapping on it would ping-pong the boss between them.

**Every ability is difficulty-mapped through `Unit::CastSpell`**, so each id is a 10/25 pair and
callers test both (`UldBossHelper.h`). Two corrections to the written guides: Lightning Tendrils is
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

**Rune of Death lands on the raid.** `DoCastRandomTarget` puts it on a random member, so on a
stacked raid it covers the stack point — four of four did in one traced pull, one exactly on it —
and it repeats every 30–40s for ~30s from Molgeim's phase 2. So the **raid spot answers it**: a
covered stack shifts to the nearest clear of the eight anchor headings at 25 yd (navprobe 8/8 on
mesh). Derived from the stack and the runes alone, never the caller — every bot computes it, and
reading the caller's position would scatter the raid instead of moving it.

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
applies); **Electrical Charge is not handled positionally** — a stacking damage buff is a healer
problem, not a movement one; **ranged spread only in hard mode**, since Static Disruption needs
Steelbreaker's phase 2; and **bots never set the skull**, so a human's mark wins.

**Reading a pull:** `postmortem.py <file> --notes ironassembly.` — `alive` (bit 0 Steelbreaker, 1
Molgeim, 2 Brundir), `focus` (what the raid is killing, and whether a human's skull beat the order),
`tank` (the boss a tank owns, or the branch that left it none), `interrupt` (the duty a bot holds
for Brundir's current cast), `spot` (the formation branch — a `-rune` suffix means the stack shifted
off a Rune of Death), `slot` (its index on the spread ring), `soak` (whether it reached Rune of
Power, and what stopped it). Overload, Lightning Tendrils and Meltdown have no world object, so they
also write `haz` circles with the spell radius and the clearance; Rune of Death and Rune of Power
do, and are swept instead — only a swept hazard is tested against a death.

Core-version assumption: the strategy relies on recent upstream fixes — `#26470` (Rune of Death
restricted to players), `#26449` (Brundir surviving Tendrils), `#26200` (Static Disruption preferring
ranged) and `#25029` (Overload invincibility). An older AzerothCore behaves differently.

