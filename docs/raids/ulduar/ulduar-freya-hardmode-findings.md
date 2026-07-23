# Ulduar Freya Elders Hard-Mode — Server-Truth Findings

Phase 4 of [ulduar-hard-mode-plan.md](ulduar-hard-mode-plan.md) ("Mimiron Firefighter +
Freya Elders"); this doc covers **Freya**. Vezax (P1), Iron Assembly (P2), Flame
Leviathan + Thorim (P3) done and config-gated. Follower model — bots never trigger it.

Source: `src/server/scripts/Northrend/Ulduar/Ulduar/boss_freya.cpp`.

Config: `AiPlayerbot.UlduarFreyaHardMode` (default 0).

---

## Server truth

Freya hard mode = one or more Elders (Brightleaf 32915 / Stonebark 32914 / Ironbranch
32913) left alive when Freya (32906) is engaged. On engage,
`boss_freya::JustEngagedWith` checks each Elder via `instance->GetCreature(DATA_ELDER_*)`
and, per living Elder, casts `SPELL_DRAINED_OF_POWER` + that Elder's essence aura, pulls
the Elder into the fight, and schedules an extra Freya ability:

| Elder alive at pull | Freya gains (repeats all fight) | Bot-relevant hazard |
|---|---|---|
| Ironbranch | `EVENT_FREYA_IRON_ROOT` → `SPELL_IRON_ROOTS_FREYA` 62862, 1 tgt (10m) / 3 (25m) | traps players in a killable **Strengthened Iron Roots** (33168) |
| Brightleaf | `EVENT_FREYA_UNSTABLE_SUN_BEAM` → `SPELL_UNSTABLE_SUN_BEAM_FREYA` 62450 AoE | spawns **Sun Beam** stalker (33170), a damaging ground zone |
| Stonebark | `EVENT_FREYA_GROUND_TREMOR` → `SPELL_GROUND_TREMOR_FREYA` 62437 | raid-wide knockback — **not handled** |

**Critical:** the empower events are scheduled once at pull and repeat unconditionally —
they keep firing for the whole fight **even after the Elder dies**. So hard-mode
reactions must key off the *hazard world object*, not off an Elder still being alive.
Killing Elders never removes the empower, and never costs the achievement/chest (elder
count / `DATA_GET_ELDER_COUNT` is locked at pull) — which is why the bot doing nothing
about Elder targeting is safe.

The Elders' own kits (fought directly) spawn the same hazard types: Ironbranch's
`SPELL_IRON_ROOTS` 62275 → **Iron Roots** (33088); Brightleaf's beam → **Unstable Sun
Beam** (33050). Handling both entries covers both sources.

### Iron Roots
A trapped player gets a DoT aura — `SPELL_IRON_ROOTS_FREYA_DAMAGE` 62861 (Freya's) or
`SPELL_IRON_ROOTS_DAMAGE` 62283 (Ironbranch's) — and a root creature is summoned on them.
`boss_freya_iron_root::JustDied` removes the aura when the creature dies. Root creatures
33088/33168 are **selectable** (unit_flags 0), so the trapped bot targets and kills its
own root.

### Unstable Sun Beam
Beam stalkers 33170 (Freya-cast) / 33050 (Brightleaf-cast) are **non-selectable**
(unit_flags `0x2000000`), so they never appear in attack-target lists — find them by
scanning the `"nearest npcs"` GuidVector (same as `GetFlameLeviathanNearestTowerHazard`).
The zone detonates for `SPELL_UNSTABLE_SUN_DAMAGE` 62217 after ~15s; bots move out first.
Exact beam radius is DBC, not in the script — `ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS`
= 12.0f is a conservative default to confirm in-game.

---

## Scope implemented (mechanics only)

Per the user decision, bots do **not** touch Elder targeting or tanking — the raid
decides what to do with the Elders; bots only survive the added ground mechanics:

1. **Break Iron Roots** — a rooted bot (`SPELL_IRON_ROOTS_DAMAGE` / `_FREYA_DAMAGE`
   aura) attacks the nearest root creature (33168 Strengthened Iron Roots, else 33088 Iron
   Roots) to free itself. `ACTION_RAID + 3` — outranks the Sun Beam dodge, because a rooted
   bot can't move, so it must free itself before it can step out of anything.
2. **Dodge Unstable Sun Beam** — a bot within radius of a Sun Beam stalker (33170 / 33050)
   flees clear of the whole in-range beam cluster (centroid, not the single nearest beam).
   `ACTION_RAID + 2`.

**Skipped:** Ground Tremor (raid-wide knockback, no ground marker — undodgeable, healed
through) and the Elders' own casts (Solar Flare etc. — heal-through).

Coarse gate `IsFreyaHardModeActive` = config-enabled AND Freya (32906) alive and in
combat. The empowered hazard objects only ever spawn in hard mode, so this + the specific
hazard check is a sufficient gate and is robust to the Elder dying mid-fight.

## Files
`Util/UldBossHelper.h` (enums + radius), `Util/UldHardMode.{h,cpp}` (`IsFreyaHardModeActive`),
`Trigger/UldTriggers_Freya.{h,cpp}`, `Action/UldActions_Freya.{h,cpp}`, `UldTriggerContext.h`,
`UldActionContext.h`, `UldStrategy.cpp`, `src/PlayerbotAIConfig.{h,cpp}`,
`conf/playerbots.conf.dist`.
