# p3-ulduar

Read `ulduar-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the section named below bind this lane.

New branch `ulduar-rework` from `raid-shared-helpers`, per the **`p3-ulduar`** paragraph, the
"PR-branch-only adaptations" paragraph and the Verification section of `ulduar-upstream-pr.PLAN.md`. The export
base is `Custom`'s HEAD when the lane starts; write it into the overview's Status line. Big lane: split by boss
group into at most 3 implementers with disjoint files, the shared files and registration stay with the finisher.

RaidObs strip: `ObsValue<T>` → `T`, `ObsGuidMap<V>` → `std::unordered_map<ObjectGuid, V>`, `ObsGuidSet` →
`GuidSet`, and `.Get()/.Set()` call sites to plain access; delete every `RaidObs::` call, `#include "RaidObs.h"`,
and trace-only code (e.g. `TickVezax` and Mimiron's tick except `ResetMimironFightState`). Commit layout: (1)
remove the old Ulduar, (2) shared Ulduar files + registration, (3) one commit per boss, (4) config options, titled
`feat(Raid/Ulduar): <summary>`; no push.

From earlier lanes:
- `raid-shared-helpers` carries `src/Util/RaidDifficulty.h` (`e82ec26c8`; `IsRaid25Man`, called by Flame
  Leviathan, Thorim, XT-002 and Yogg-Saron), so branch from its tip.
- `raid-shared-helpers` dropped the trace-name argument of `NextTankDefensive`, `NextMagicDefensive`,
  `NextPhysicalDefensive` and `CallHumanTankDefensive`: remove it at every Ulduar call. `UldActions_Mimiron.cpp`
  `NextTankDefensive(botAI, bot, "mimiron.plasma")` still compiles with it (the string becomes `physicalOnly =
  true`), so fix that one by hand.
- Add the GPLv2 header to every Ulduar file that lacks one.

## Task list

## Decisions for review

## Blocked
