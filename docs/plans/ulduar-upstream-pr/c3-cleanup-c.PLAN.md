# c3-cleanup-c

Read `ulduar-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the section named below bind this lane.

Behaviour-neutral cleanup on `Custom`, branch `uldpr/c3-cleanup-c`. Apply the **Cleanup lanes** list of `ulduar-upstream-pr.PLAN.md` to the files below. Measurements cut from comments go to the boss doc in `docs/raids/ulduar/`. RaidObs calls stay. Any change that alters behaviour beyond the listed fixes is out of scope: log it under Decisions for review instead.

Files: every `src/Ai/Raid/Uld/**` file whose stem ends in `_Algalon` or `_YoggSaron`. Includes the four
`TeleportTo` → `NearTeleportTo` switches in `Action/UldActions_YoggSaron.cpp`. Runs only after the user said their
own Algalon/Yogg-Saron work is paused; other sessions may have committed there since the plan was written, so
start from current `Custom`.

## Task list

## Decisions for review

## Blocked
