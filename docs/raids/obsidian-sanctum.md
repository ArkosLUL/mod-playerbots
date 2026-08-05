# Obsidian Sanctum (map 615)

Strategy key `wotlk-os`. Cross-raid conventions are in [README.md](README.md).

## Encounter model

**"Leave N drakes alive" means bots *tank* those N drakes but never kill them**, and burn Sartharion.
Killing Sartharion ends the encounter and despawns the drakes, so surviving drakes cost nothing —
but they must be held and their mechanics serviced.

Locked decisions: keep-order is **Tenebron → Shadron → Vesperon**, so kill-order is the reverse.
Leave1 keeps Tenebron; Leave2 keeps Tenebron + Shadron. Config
`AiPlayerbot.SartharionDrakesAlive`, 0-3, default 0.

## Wipe vectors the overhaul targets

Each of these is a wipe at +1/+2/+3, and the strategy only reliably did Sarth+0 before:

| # | Gap |
|---|---|
| A | No difficulty config — killing all drakes was hardcoded |
| B | DPS attack-priority kills the drakes that are supposed to be kept |
| C | Portal entry hard-gated on "drakes still alive", so acolytes were never cleared when drakes are kept. Only **Shadron's** acolyte was handled, not Vesperon's, and the exit check only looked for `acolyte of shadron` |
| D | No Twilight Egg / Whelp handling (Tenebron) — the enum ids were unused |
| E | No Lava Blaze handling (Sartharion's Lava Strike adds) — enum id unused |
| F | Melee rear-flank fired only when **no** drake was alive, so melee ate Flame Breath / Cleave / Tail Lash in every leave-alive run |
| G | One off-tank spot and no facing control; the MT `TankFace` multiplier was a commented no-op, so Tail Lash / Cleave / drake breath pointed into the raid |
| H | `SARTHARION_RANGED_POSITION` was defined but never used |
| I | **Twilight Revenge**: killing a to-kill drake while its acolyte or portal is still open buffs Sartharion massively |

Gap I is why target selection gates on `DrakeAcolyteClear(drakeEntry)` before a to-kill drake becomes
a valid target. Tenebron has no acolyte, so it is always clear.

Target priority, first match wins: acolyte (if in the twilight realm) → twilight whelps/eggs (eggs
before they hatch) → Lava Blaze → to-kill drake whose acolyte is cleared → Sartharion. Kept drakes
are never selected. The same action serves both the realm and the ground because it is driven by
`find target` name lookups, which resolve to whatever the bot can currently see.

Because Shadron and Vesperon stay alive, their acolytes respawn — so the portal logic must stay
**stateless** and simply re-fire each cycle.

## `ObsidianSanctumHelpers`

An inline header (`OSShared.h`, RS-style — no new `.cpp` or context wiring) holding the keep-order
array and its derivations (`IsDrakeKept`, `IsDrakeToKill`, `FindDrakeToKill`), plus add predicates:
twilight egg 30882 / whelp 30890, Lava Blaze 30643, acolyte of Shadron 31218 / Vesperon 31219.

**`ForceThreat` does `AddThreat(bot, 1000000.0f, …)` then `FixateTarget(bot)`, and fixate overrides
threat outright — so a threat redirect cannot actually move a drake.** That makes a wrong redirect
here cost a wasted cooldown rather than a wipe, which is why the redirect veto is narrow.

Kept drakes are parked away from the boss stack so boss-centred AoE does not accidentally kill them
(that forfeits the achievement rather than wiping).

## To confirm against DBC during any further work

The aura→effect mapping — Gift of Twilight Fire vs Twilight Torment, Gift of Twilight Shadow 57835
vs Twilight Torment 57935. Several enum ids in `OSTriggers.h` are currently unused.
