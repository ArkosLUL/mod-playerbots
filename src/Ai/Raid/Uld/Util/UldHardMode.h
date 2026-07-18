#ifndef PLAYERBOTS_ULDHARDMODE_H
#define PLAYERBOTS_ULDHARDMODE_H

#include "Define.h"

class PlayerbotAI;
class Unit;

// Ulduar hard-mode detectors.
//
// Hard modes are optional harder versions of an encounter, activated by a raid choice
// (leave towers up, keep Saronite Vapors alive, press Mimiron's button, ...). They are
// NOT the 10/25 heroic difficulty flag, so the spell-id predicate trick does not apply.
//
// Bots follow the raid: they never trigger hard mode themselves, they only react once the
// server marks it active. Each detector returns config-enabled AND current server truth
// (extra add alive, empowering aura present, boss GetData flag set) so the per-boss triggers
// stay thin. The per-boss config switch (e.g. AiPlayerbot.UlduarVezaxHardMode) is opt-in
// and defaults off, so a raid must explicitly enable a boss' hard-mode handling.

// General Vezax: the Saronite Animus (33524) is alive. It spawns once six Saronite Vapors
// are reached without any being killed; Vezax gains the invulnerable Saronite Barrier until
// the Animus dies. Note: the boss' GetData(1) reports loot mode 3, which is only set AFTER
// the Animus dies (hard mode completed), so it is not a live "active" signal - the Animus
// being alive is.
bool IsVezaxHardModeActive(PlayerbotAI* botAI);

// Assembly of Iron: the hard mode is a kill-order choice - leave Steelbreaker (32867) for last so
// he reaches his empowered phase 3. There is no boss GetData flag exposed for this, so it is
// inferred from server truth. IsIronAssemblyHardModeActive returns config-enabled AND at least one
// council member still alive (the window during which a kill order matters). IsSteelbreakerEmpowered
// additionally requires Steelbreaker alive with both Molgeim (32927) and Brundir (32857) dead: two
// Supercharges have then advanced him to phase 3 (Fusion Punch DoT + Overwhelming Power on the tank).
bool IsIronAssemblyHardModeActive(PlayerbotAI* botAI);
bool IsSteelbreakerEmpowered(PlayerbotAI* botAI);

// The council member that should die next to keep Steelbreaker for last: Brundir, then Molgeim,
// then Steelbreaker himself. Returns nullptr if none are alive.
Unit* GetIronAssemblyNextKillTarget(PlayerbotAI* botAI);

// Flame Leviathan: hard mode = raid leaves towers standing. Each surviving tower adds an
// empower aura to the boss (33113) at pull and spawns that tower's periodic ground hazard.
// Returns config-enabled AND the bitmask (FlameLeviathanTowerFlags) of towers still up,
// read straight off the boss's auras. 0 when off, boss absent, or no towers left.
uint32 FlameLeviathanActiveTowerMask(PlayerbotAI* botAI);

// Nearest active-tower ground hazard (Storm strike / Flame trail / Frost chase) within
// radius of `from`, restricted to the towers set in `towerMask`. Life tower is adds, not a
// ground hazard, so it is never considered. Returns nullptr if none are in range.
Unit* GetFlameLeviathanNearestTowerHazard(PlayerbotAI* botAI, Unit* from, uint32 towerMask, float radius);

// Thorim: hard mode = the arena gauntlet is cleared fast enough that Sif interrupts her channel
// and joins the fight instead of despawning. Sif (33196) spawns at Thorim's throne (above the
// arena floor) and only drops onto the arena floor when she joins, so being alive AND below the
// floor threshold is the live signal. Note: the plan's proposed "not channeling hologram" proxy
// does not work - SPELL_SIF_CHANNEL_HOLOGRAM (64324) is never cast in this core. Returns
// config-enabled AND Sif fighting in the arena.
bool IsThorimHardModeActive(PlayerbotAI* botAI);

// Freya: hard mode = an Elder (Brightleaf/Stonebark/Ironbranch) was left alive when Freya was engaged,
// permanently empowering her with an extra ability. The empower keeps firing for the rest of the fight
// even after the Elder dies, so this is only the coarse config + encounter gate; the per-mechanic
// triggers add the specific hazard check (rooted bot / nearby Unstable Sun Beam). Those hazard objects
// only ever exist in hard mode, so config-enabled AND Freya in combat is a sufficient gate.
bool IsFreyaHardModeActive(PlayerbotAI* botAI);

#endif
