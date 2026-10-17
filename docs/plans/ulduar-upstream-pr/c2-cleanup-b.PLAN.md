# c2-cleanup-b

Read `ulduar-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the section named below bind this lane.

Behaviour-neutral cleanup on `Custom`, branch `uldpr/c2-cleanup-b`. Apply the **Cleanup lanes** list of `ulduar-upstream-pr.PLAN.md` to the files below. Measurements cut from comments go to the boss doc in `docs/raids/ulduar/`. RaidObs calls stay. Any change that alters behaviour beyond the listed fixes is out of scope: log it under Decisions for review instead.

Files: every `src/Ai/Raid/Uld/**` file whose stem ends in `_FlameLeviathan`, `_Freya`, `_Thorim`, `_Mimiron`.
Includes the `TeleportTo` → `NearTeleportTo` switch with the master-map guard at `Action/UldActions_Thorim.cpp`.

## Task list

## Decisions for review

## Blocked
