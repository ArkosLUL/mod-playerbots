# r13-lich-king — The Lich King

Wave 3g, alone. Rules, conventions and the boss lane recipe: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md).
Layout and seams: `docs/raids/icecrown-citadel/README.md`, "Code layout". It splits: phase 1 and the
transitions, phase 2, phase 3 with Frostmourne are natural groups.

## Encounter

Slug `the-lich-king`, prefix `lk.`, boss id 12, stem `LichKing`. Core `boss_the_lich_king.cpp`. Doc
`the-lich-king.md`, reader `the_lich_king.py`. Guides in the cache: Warcraft Tavern 25 and 10,
instant-carry's raid overview, Wowhead if saved.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 4454 lines (`ICCActions_LK.cpp`).
- The phase is stateless, from his react state and the winter row (`IccLichKingPhase`,
  `IccRemorselessWinterOf`). It lags up to 2 s at thresholds.
- 19 icon writes and 14 reads (`LkSetIcon`). The Val'kyr bucket keeps its `rti`.
- A hand-rolled flee vector.
- `IccLichKingAddsMultiplier`:
  - in the Terenas room: melee and warlocks get only movement and the adds action;
  - cures until the plague is delivered;
  - always: Formation, Follow, Blink, Disengage and Flee;
  - phase 1: the AoE family and assist-tank targeting;
  - during a Winter cast: heals only for non-tanks;
  - a sphere victim: all MovementAction;
  - ranged movement while Defile is up.
- `IccLichKingSpiritBombMultiplier` vetoes everything but the bomb action and `SetFacing` under bomb
  threat.
- Cheats:
  - `lk.ragingspiritkill` (`IccLichKingWinterAction`) kills the fullest Raging Spirit past two;
  - `lk.wickedspiritkill` (`HandleSpiritMarkingAndTargeting`) kills the starred Wicked Spirit;
  - `lk.hoj` (`HandleAssistTankAddManagement`) casts Hammer of Justice on Shamblings from any class;
  - `lk.frostmourneescape` (`HandleTeleportationFixes`, normal) snaps a Harvest Soul victim out of
    Frostmourne on its first tick, skipping the Spirit Warden.
  - Buffs `lk.heroicbuff`, `lk.empoweredblood` and `lk.valkyrstun` stay.
- Borderline: in `IccLichKingAddsAction` the Val'kyr edge dive puts the carry aura on itself and
  suicides at the bottom.
- Unstucks through `NearTeleportTo` (6 sites). Among them, a 1 yd platform snap cancels knockbacks
  mid-flight and re-snaps fallen bots; it waits on a navprobe of the Frozen Throne and the cheat
  rule.
- Only the main tank dodges Shadow Traps. Heroic Harvest Souls are only partly handled.
- The Defile listener in `ICCScripts.cpp` notes `lk.defile` and a 6 yd hazard for the 2 s cast.
- 14 positions and 5 Z constants.

## Guide answers to build on

- **Phase 1:**
  - Dispel Necrotic Plague next to the adds so it kills them.
  - Shambling Horrors: the off-tank faces Shockwave away. Dispel Enrage with Tranquilizing Shot or
    Anesthetic Poison, and keep the horror stunned through Frenzy.
  - Shadow Trap is heroic-only (the script).
- **Transitions:**
  - A tank picks up each Raging Spirit at once and faces it away, and the raid burns it before the
    next phase. Its Soul Shriek silence is dispellable.
  - Ranged kill the Ice Spheres.
- **Phase 2:**
  - Val'kyr take a raider to the nearest edge, so the Lich King is tanked in the centre and the raid
    stacks tight. They can be stunned and slowed to 50%, not rooted.
  - Heroic: they drop the raider at 50%, cast Life Siphon, and must die before the next grab.
  - Keep Val'kyr out of Defile; Defile is run out of the raid. Soul Reaper is a tank swap.
- **Phase 3:**
  - Vile Spirits are set off by a tank, Dispersion or Divine Shield.
  - Harvest Soul on normal: the victim fights the Spirit Warden with Terenas, keeps Terenas alive, and
    interrupts or dispels Soul Rip. On failure Destroy Soul kills the victim and the Lich King gets
    +200% damage.
  - Heroic: the whole raid enters Frostmourne for about 40 s. Ranged kill the Wicked Spirits early,
    Spirit Bombs repeat at the same spot, and a death inside wipes the raid (Harvested Soul).
- **Setup:** 2 tanks, 4-7 healers.

## Focus

- Every mechanic above on every difficulty, with direct targeting.
- A stun rotation over the raid's kits and the Enrage dispel duty.
- A Raging Spirit tank pickup and burn.
- Val'kyr stun, slow and kill duties.
- The victim's Frostmourne fight and the heroic Frostmourne raid.
- The knockback question decided by navprobe and the cheat rule.
- Replacing the four cheats and the dive suicide.

## Owns

Stem `LichKing` and its definition, the doc, the reader and its test, this brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
