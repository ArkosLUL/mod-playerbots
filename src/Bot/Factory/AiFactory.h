/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AIFACTORY_H
#define PLAYERBOTS_AIFACTORY_H

#include "Common.h"
#include <map>

class AiObjectContext;
class Engine;
class Player;
class PlayerbotAI;

enum BotRoles : uint8;

class AiFactory
{
public:
    static AiObjectContext* createAiObjectContext(Player* player, PlayerbotAI* botAI);
    static Engine* createCombatEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* aiObjectContext);
    static Engine* createNonCombatEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* aiObjectContext);
    static Engine* createDeadEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* aibjectContext);
    static void AddDefaultNonCombatStrategies(Player* player, PlayerbotAI* const facade, Engine* nonCombatEngine);
    static void AddDefaultDeadStrategies(Player* player, PlayerbotAI* const facade, Engine* deadEngine);
    static void AddDefaultCombatStrategies(Player* player, PlayerbotAI* const facade, Engine* engine);

    // Cached per player. The talent hooks in Playerbots.cpp invalidate on both sides of every change
    // to the talent map; the active spec and level are checked on every read instead. Invalidate is
    // safe to call any number of times and in any order - nothing pairs up calls. Forget drops the
    // player's row for good, so the cache does not grow a bucket per guid the realm has ever seen.
    static uint8 GetPlayerSpecTab(Player* player);
    static std::map<uint8, uint32> GetPlayerSpecTabs(Player* player);
    static void InvalidatePlayerSpecTab(Player* player);
    static void ForgetPlayerSpecTab(Player* player);
    static BotRoles GetPlayerRoles(Player* player);
    static std::string GetPlayerSpecName(Player* player);
};

#endif
