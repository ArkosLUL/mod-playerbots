#ifndef PLAYERBOTS_ULDHARDMODE_H
#define PLAYERBOTS_ULDHARDMODE_H

#include "Define.h"

class PlayerbotAI;
class Unit;

// Ulduar hard modes.
//
// Hard modes are optional harder versions of an encounter, activated by a raid choice
// (leave towers up, keep Saronite Vapors alive, press Mimiron's button, ...). They are
// NOT the 10/25 heroic difficulty flag, so the spell-id predicate trick does not apply.
//
// These are declared by config, not detected: the per-boss switch (e.g.
// AiPlayerbot.UlduarVezaxHardMode, opt-in and default off) is the single source of truth, and while
// it is on that boss' hard-mode handling is armed for the whole encounter. Nothing fires when there
// is nothing to react to, because every per-boss trigger still checks its own mechanic (hazard
// nearby, debuff on the bot, add alive). So the server owner must set the option to match what the
// raid actually does.

// General Vezax: the raid keeps six Saronite Vapors alive, which spawns the Saronite Animus (33524)
// and gives Vezax the invulnerable Saronite Barrier until the Animus dies.
bool IsVezaxHardModeActive(PlayerbotAI* botAI);

// Assembly of Iron: the hard mode is a kill-order choice - leave Steelbreaker (32867) for last so
// he reaches his empowered phase 3.
bool IsIronAssemblyHardModeActive(PlayerbotAI* botAI);

// IsSteelbreakerEmpowered is a phase check and deliberately NOT gated on the option above: he is
// empowered whenever he is alive with both Molgeim (32927) and Brundir (32857) dead, meaning two
// Supercharges advanced him to phase 3 (Static Disruption, then Overwhelming Power on the tank). A
// raid that reaches that state without the option set still needs the tank swap.
bool IsSteelbreakerEmpowered(PlayerbotAI* botAI);
// Overload for callers that already resolved the three members, so a per-tick path does not repeat
// three grid lookups.
bool IsSteelbreakerEmpowered(PlayerbotAI* botAI, Unit* steelbreaker, Unit* molgeim, Unit* brundir);

// The council member the raid should kill next. Hard mode saves Steelbreaker for last; the normal
// order kills him first. Returns nullptr if none are alive.
Unit* GetIronAssemblyNextKillTarget(PlayerbotAI* botAI);

// Flame Leviathan: hard mode = raid leaves towers standing. Each surviving tower empowers the boss
// and spawns that tower's periodic ground hazard. Returns FL_TOWER_ALL when the option is on, 0
// otherwise. Claiming all four towers costs nothing: hazards are found by NPC entry, so a tower the
// raid did destroy simply contributes none.
uint32 FlameLeviathanActiveTowerMask(PlayerbotAI* botAI);

// Nearest active-tower ground hazard (Storm strike / Flame trail / Frost chase) within
// radius of `from`, restricted to the towers set in `towerMask`. Life tower is adds, not a
// ground hazard, so it is never considered. Returns nullptr if none are in range.
Unit* GetFlameLeviathanNearestTowerHazard(PlayerbotAI* botAI, Unit* from, uint32 towerMask, float radius);

// Thorim: hard mode = the arena gauntlet is cleared fast enough that Sif (33196) interrupts her
// channel and joins the fight instead of despawning.
bool IsThorimHardModeActive(PlayerbotAI* botAI);

// Freya: hard mode = an Elder (Brightleaf/Stonebark/Ironbranch) was left alive when Freya was engaged,
// permanently empowering her with an extra ability. The empower keeps firing for the rest of the fight
// even after the Elder dies. The per-mechanic triggers carry the specific hazard check (rooted bot /
// nearby Unstable Sun Beam), and those objects only ever exist in hard mode.
bool IsFreyaHardModeActive(PlayerbotAI* botAI);

// Mimiron: hard mode ("Firefighter") = a player pressed the Big Red Button before the pull, which
// empowers the mechs for the whole fight and adds two hazards - a persistent, spreading ground fire
// and VX-001's Frost Bomb. Note: the Emergency Fire Bots (34147) that also spawn are friendly fire
// extinguishers, not kill targets, so bots leave them alone.
bool IsMimironHardModeActive(PlayerbotAI* botAI);

// XT-002 Deconstructor: hard mode = the raid kills the exposed Heart during one of the 75/50/25%
// windows, which full-heals XT and gives him Heartbreak for the rest of the fight.
//
// Unlike the other Ulduar hard modes there is no server signal before the kill - the raid's intent
// is the only input - so IsXT002HardModeActive is the config alone. It decides whether bots burn
// the Heart down or stop at ULDUAR_XT002_HEART_SAFE_HP_PCT.
bool IsXT002HardModeActive(PlayerbotAI* botAI);

// Hard mode is already live: XT carries Heartbreak (65737), so there will be no further Heart phase
// and Life Sparks / Void Zones now spawn. Deliberately config-independent, so a Heartbreak the human
// players triggered is handled too.
bool IsXT002HeartbreakActive(PlayerbotAI* botAI);

// Yogg-Saron: hard mode is the reduced-Keeper achievement ladder - the raid frees fewer than 4 Keepers
// before the pull, losing that Keeper's support. The handling is tuned for the hardest single-Keeper
// case, Thorim only: with Freya absent there are no Sanity Wells, so sanity is a one-way drain.
bool IsYoggSaronHardModeActive(PlayerbotAI* botAI);

// Whether Thorim is among the Keepers the raid freed, read from the instance's persistent watchers
// mask. Which Keepers are up is chosen in-instance, so unlike the hard-mode switch this cannot come
// from config. It decides whether Immortal Guardians can be brought to Weakened for Thorim's Titanic
// Storm to execute, or have to fall back to the cheat instakill.
bool YoggThorimKeeperActive(PlayerbotAI* botAI);

#endif
