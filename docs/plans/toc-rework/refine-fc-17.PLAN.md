# refine-fc-17: Frost Trap on a chased hunter

Wave 4z, run in session on the user's call (2026-10-29). Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4y `8936594fe`.

## Why

No bot cast Frost Trap: there's no Trap Launcher for it here, so it drops at the hunter's feet. With refine-fc-16 the
tactic is the kite, and a chased hunter's own feet are on the chaser's path.

## Facts

- Player Frost Trap 13809: category 411 with Freezing Trap and Freezing Arrow, 30 s, on the GCD.
- Its trap (gameobject 164639) arms 1 s after it's set in combat (`startDelay`) and goes off at 2.5 yd (diameter 5),
  casting 63487: Frost Trap Aura 13810, a 10 yd slick slowing 50% for 30 s (mechanic snare).
- Entrapment (19184/19387/19388) adds a 10 yd root (19185, 64803, 64804). A root pops the champion PvP trinket and
  ends a grip in the air.

## Scope

In `FactionChampionsEscape`, a hunter casts Frost Trap at its feet before Disengage, with no iced or grip-guarded
champion within 12 yd. The escape already wants a melee champion on the hunter within 8 yd. `fc.escape` names the
spell, so the reader needs no change.

## Targets (next 25H)

`fc.escape` Frost Trap on chased hunters, the chaser slowed through the kite; Freezing Arrow peels from that hunter
only while the trap's cooldown is up.
