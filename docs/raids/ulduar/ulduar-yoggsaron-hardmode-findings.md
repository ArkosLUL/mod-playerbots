# Ulduar Yogg-Saron Reduced-Keeper Hard-Mode — Server-Truth Findings

Phase 5 (final) of [ulduar-hard-mode-plan.md](ulduar-hard-mode-plan.md). Vezax, Iron
Assembly, Flame Leviathan, Thorim, Freya, Hodir, and Mimiron are done and config-gated.
This doc covers **Yogg-Saron**. Follower model — bots never free Keepers; a human sets up
the encounter and bots detect and adapt.

Source: `src/server/scripts/Northrend/Ulduar/Ulduar/boss_yoggsaron.cpp` + `ulduar.h`.

Config: `AiPlayerbot.UlduarYoggSaronHardMode` (default 0).

---

## Server truth

Yogg-Saron's hard mode is the **reduced-Keeper** achievement ladder: the raid frees fewer
than 4 Keepers before the pull, losing that Keeper's support. This is tuned for the hardest
single-Keeper case, **Thorim only**.

**Detection — the freed-Keeper bitmask.** `instance->GetPersistentData(PERSISTENT_DATA_WATCHERS_MASK)`
(`GetPersistentData(uint32)` is public const on `InstanceScript`). Bit set = that Keeper is
active. Bits (`ulduar.h`): `KEEPER_FREYA=0, KEEPER_HODIR=1, KEEPER_MIMIRON=2, KEEPER_THORIM=3`.
This is the exact source the boss script reads, and it is stable for the whole lockout — so it
is preferred over Sara's `GetData(DATA_GET_KEEPERS_COUNT)` (count only): reading the mask lets
us confirm **Thorim** specifically.

**What Thorim-only removes:**
- **Freya → no Sanity Wells.** Wells spawn only from `spell_keeper_freya_summon_sanity_well`.
  With no Freya, `SPELL_SANITY` (63050, 100 stacks) is a **one-way drain**: nothing restores it.
- **Hodir → no Protective Gaze absorb shield.**
- **Mimiron → no haste clouds → slower kill → more total sanity drain.**
- **Thorim stays → Titanic Storm still executes Weakened Immortal Guardians** (the one crutch we keep).

**Sanity drains** (`spell_yogg_saron_sanity_reduce`):

| Source | Spell | Loss | Avoidable? |
|---|---|---|---|
| Psychosis | 63795 / 65301 | −9 / −12 | **No** — single random target every 3.5s in P2 (the floor) |
| Malady of the Mind | 63830 / 63881 | −3 | Yes — existing move-away action |
| Brain Link | 63803 | −2 | Yes — only if the linked pair is >20yd apart; existing action |
| Lunatic Gaze (P2 skull) | 64168 | −2 | Yes — only players *facing* the caster (`HasInArc(M_PI)`) |
| Lunatic Gaze (P3 Yogg) | 64164 | −4 | Yes — face away; existing P3 look-away action |
| Induce Madness (brain) | 64059 | −100 | Yes — leave the brain room; existing exit-portal action |

After the existing avoidance actions run, the only surviving drains in the boss room are
Psychosis (irreducible) and the odd Lunatic Gaze / Malady tick. With no Wells there is no way
back up, so the strategy is minimise avoidable loss + race.

**Immortal Guardians (P3) + Thorim execute.**
- `NPC_IMMORTAL_GUARDIAN=33988`, `NPC_MARKED_IMMORTAL_GUARDIAN=36064` (Shadow Beacon mark).
- `spell_yogg_saron_empowered_aura`: a guardian holds `SPELL_EMPOWERED` at stacks = `min(HP%/10, 9)`
  (its damage reduction scales with its HP, so focus-fire matters). At 0 stacks (~<10% HP) it
  drops Empowered and gains `SPELL_WEAKENED` (64162).
- `spell_yogg_saron_titanic_storm`: with Thorim present, auto-kills any unit carrying
  `SPELL_WEAKENED`. **So melee only need to burn a guardian to Weakened; Thorim finishes it.**
  No Thorim → guardians are effectively unkillable without the cheat.

**Crusher Tentacle** `NPC_CRUSHER_TENTACLE=33966` (P2): erupts at a fixed spot and is
**stationary**. Its Diminish Power (64145) is a raid-wide DPS debuff, so it must die fast.
Ranged nuke it in place; nobody tanks it (with no one in melee range its Crush can't land).

**Phase detection** (already in `YoggSaronTrigger`): P2 = `NPC_YOGG_SARON`(33288) alive with
`SPELL_SHADOW_BARRIER`(63894); P3 = it alive, no Shadow Barrier, no Guardian of Yogg-Saron.

---

## Scope implemented

The existing Yogg-Saron strategy is comprehensive and heavily `BotCheatMask::raid`-driven.
The reduced-Keeper work keeps every other cheat and only changes the two adds the user asked
to play for real, plus adds sanity discipline. Everything is gated on
`IsYoggSaronHardModeActive` (config + Yogg encounter IN_PROGRESS + fewer than 4 Keepers).

1. **Crusher Tentacle — ranged only.** The P2 cheat instakill in `YoggSaronMarkTargetAction` is
   suppressed in hard mode. A new `yogg-saron crusher tentacle` trigger/action makes each
   **ranged DPS** bot directly `Attack()` the nearest Crusher Tentacle (no group marker, so
   melee keep their target and healers keep healing). `ACTION_RAID + 2`.
2. **Immortal Guardian — tank pickup + Thorim execute.** The P3 cheat instakill is suppressed
   in hard mode **only when Thorim is a Keeper** (else it falls back to the cheat, so other
   reduced-Keeper combos stay winnable). Mark-target keeps the lowest-HP guardian on skull for
   focus. A new `yogg-saron guardian control` trigger/action makes the **tank** hold the melee
   stack (`ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT`) and class-taunt loose guardians (not already
   on a tank) to it, so melee cleave them to Weakened and Thorim's Titanic Storm executes each.
   `ACTION_RAID + 3`.
3. **Sanity conservation.** New `yogg-saron sanity conservation` trigger/action: a non-tank bot
   whose `SPELL_SANITY` is at or below `ULDUAR_YOGG_SARON_SANITY_CONSERVE_THRESHOLD` (15), with no
   Sanity Well reachable and no Brain Link, in the boss room, pulls to the back of Yogg (the spot
   behind him, opposite his facing) and faces directly away. It then only heals/DPSes a target
   already in its front hemisphere, so it never turns back toward Yogg and eats a gaze - riding out
   the unavoidable Psychosis. Yields to the death-orb dodge when an orb is near. `ACTION_RAID + 5`.
   Also hardened `YoggSaronSanityAction` with a null-check (the well can be absent).

**Detection infra:** `IsYoggSaronHardModeActive`, `YoggActiveKeeperMask`, `YoggThorimKeeperActive`
in `Util/UldHardMode.{h,cpp}` read the instance mask via `((InstanceMap*)map)->GetInstanceScript()`.

## Files
`Util/UldBossHelper.h` (SPELL_WEAKENED + sanity threshold), `Util/UldHardMode.{h,cpp}`,
`Trigger/UldTriggers_YoggSaron.{h,cpp}`, `Action/UldActions_YoggSaron.{h,cpp}`,
`UldTriggerContext.h`, `UldActionContext.h`, `UldStrategy.cpp`, `src/PlayerbotAIConfig.{h,cpp}`,
`conf/playerbots.conf.dist`.

## Open risks (verify in-game)
- **Lunatic-gaze freeze vs. real guardian DPS.** The removed P3 cheat existed because Lunatic
  Gaze "freezes" bots so they can't attack; without it, guardian DPS may stall between gazes
  (every 12s). The look-away action mitigates but may not fully unfreeze — this is a separate
  bot-engine issue and may need a follow-up if the kill stalls.
- **Taunt reach.** A guardian spawns up to 48yd out; the tank only taunts once it is within
  taunt range of the stack. Guardians drain-life a random raider so they approach anyway, but
  confirm the pickup is prompt enough.
- **Conservation nearly benches a bot.** Behind Yogg facing away, most heal/DPS targets are on the
  far side (behind the bot), so it contributes little while conserving. Sanity never recovers
  Thorim-only, so a bot at/below 15 stays there. Threshold is low so this should be rare; confirm
  Psychosis doesn't drive several bots that low at once and stall the kill. Also confirm the
  behind-Yogg spot is on the floor (uses Yogg's Z) and reachable.
