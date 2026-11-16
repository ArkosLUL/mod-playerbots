# refine-fc-19: healer target guard, Rebirth landing, chased cast break

Run in session on the user's call (2026-11-16). Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: fc-18 `8beaedbad`.

## Why

25H kill 2026-11-16 (`649_1_erin-misthoof_1794828810`), 7 deaths of 5 bots: Agony 0:59 (hunter burst, then
the free DK), Elemena 1:08 and Hellflame 1:08 (healers looped through a Feared champion, near no healing
1:04-1:10), Elemena 1:45 (burst from full), Elemena 1:48 (Rebirth landed it by the warlock and mage), Power
1:53 (mid Frostbolt as the DK and Ret turned on it), Smartface 2:05 (hunter burst).

## Changes

- **Healer target guard** (bug): `faction champions healer target guard multiplier` zeroes `DpsAssist` for
  healers while live. Assist picked a Feared champion that `invalid target` dropped every next tick.
- **Rebirth landing** (bug): `UpdateBattleRes` counts champions round the druid (`PackCrowded`), since a res
  lands at the caster; the self-res check stays on the corpse.
- **Chased cast break**: `BreakChasedCasts` in the ball `Refresh` requests an interrupt for a bot mid hard
  cast once a kite chaser (`KiteChasers`, `KiteFree`) would reach melee before it lands; `fc.castbreak`.

## Targets (next 25H)

No `raid.healerengine` flips during Fears; no Rebirth from a druid with 2+ champions within 8 yd;
`fc.castbreak` notes followed by `fc.kite` within a tick, fewer melee hits on casters mid hard cast.
