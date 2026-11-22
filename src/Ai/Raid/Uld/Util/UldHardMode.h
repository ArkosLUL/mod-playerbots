#ifndef PLAYERBOTS_ULDHARDMODE_H
#define PLAYERBOTS_ULDHARDMODE_H

#include "Define.h"

#include <vector>

class Player;
class PlayerbotAI;
class Unit;

bool IsVezaxHardModeActive(PlayerbotAI* botAI);

bool IsIronAssemblyHardModeActive(PlayerbotAI* botAI);

bool IsSteelbreakerEmpowered(PlayerbotAI* botAI);
bool IsSteelbreakerEmpowered(PlayerbotAI* botAI, Unit* steelbreaker, Unit* molgeim, Unit* brundir);

Unit* GetIronAssemblyNextKillTarget(PlayerbotAI* botAI);

bool IsMimironHardModeActive(PlayerbotAI* botAI);

bool IsXT002HardModeActive(PlayerbotAI* botAI);

bool IsXT002HeartbreakActive(Player* bot, Unit* xt002);

bool YoggThorimKeeperActive(PlayerbotAI* botAI);

uint32 YoggKeepersFreedCount(PlayerbotAI* botAI);

#endif
