/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidCombatHelpers.h"

#include <algorithm>
#include <cstring>
#include <iterator>

#include "Action.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidSplash.h"
#include "Unit.h"

bool IsRaidDpsBot(Player* bot)
{
    return bot && !PlayerbotAI::IsTank(bot) && !PlayerbotAI::IsTank(bot, true) && !PlayerbotAI::IsHeal(bot);
}

bool IsRaidAreaDamage(PlayerbotAI* botAI, Action* action)
{
    return action->getThreatType() == Action::ActionThreatType::Aoe || MULTI_TARGET_ACTIONS.count(action->getName()) ||
           SpellSplashes(botAI, action);
}

bool CastRaidThroughputHeal(PlayerbotAI* botAI, Unit* target, std::function<bool(std::string const&)> const& skip)
{
    static char const* const heals[] = {
        "holy shock", "riptide", "swiftmend", "penance",
        "holy light", "greater heal", "healing wave", "nourish", "healing touch",
        "flash of light", "flash heal", "lesser healing wave", "regrowth",
        "renew", "rejuvenation", "lifebloom", "wild growth", "prayer of mending",
    };
    static char const* const hots[] = {"renew", "rejuvenation", "lifebloom", "wild growth", "prayer of mending"};

    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !target)
        return false;

    for (char const* heal : heals)
    {
        if (skip && skip(heal))
            continue;

        bool const hot = std::any_of(std::begin(hots), std::end(hots),
                                     [heal](char const* name) { return std::strcmp(heal, name) == 0; });
        if (hot)
        {
            uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", heal)->Get();
            if (spellId && target->HasAura(spellId, bot->GetGUID()))
                continue;
        }

        if (botAI->CanCastSpell(heal, target))
            return botAI->CastSpell(heal, target);
    }

    return false;
}
