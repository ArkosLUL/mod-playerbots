#include "ToCHelpers_Shared.h"

#include <algorithm>
#include <cstring>
#include <iterator>

#include "Playerbots.h"
#include "Creature.h"
#include "Group.h"
#include "Pet.h"
#include "RaidSplash.h"
#include "RtiTargetValue.h"
#include "Unit.h"

namespace TrialOfTheCrusaderHelpers
{

Unit* GetNearestCreatureByEntry(Player* bot, uint32 entry, float radius)
{
    if (!bot)
        return nullptr;

    std::list<Creature*> creatures;
    bot->GetCreatureListWithEntryInGrid(creatures, entry, radius);

    Unit* nearest = nullptr;
    float nearestDist = radius;
    for (Creature* creature : creatures)
    {
        if (!creature->IsAlive())
            continue;

        float const dist = bot->GetExactDist2d(creature);
        if (!nearest || dist < nearestDist)
        {
            nearest = creature;
            nearestDist = dist;
        }
    }

    return nearest;
}

bool GetCreatureClusterCenter(Player* bot, uint32 entry, float radius, Position& center)
{
    if (!bot)
        return false;

    std::list<Creature*> creatures;
    bot->GetCreatureListWithEntryInGrid(creatures, entry, radius);

    float sumX = 0.0f;
    float sumY = 0.0f;
    float sumZ = 0.0f;
    uint32 count = 0;
    for (Creature* creature : creatures)
    {
        if (!creature->IsAlive())
            continue;

        sumX += creature->GetPositionX();
        sumY += creature->GetPositionY();
        sumZ += creature->GetPositionZ();
        ++count;
    }

    if (!count)
        return false;

    center.Relocate(sumX / count, sumY / count, sumZ / count);
    return true;
}

bool IsToCDpsBot(Player* bot)
{
    return bot && !PlayerbotAI::IsTank(bot) && !PlayerbotAI::IsTank(bot, true) && !PlayerbotAI::IsHeal(bot);
}

bool IsToCAreaDamage(PlayerbotAI* botAI, Action* action)
{
    return action->getThreatType() == Action::ActionThreatType::Aoe || MULTI_TARGET_ACTIONS.count(action->getName()) ||
           SpellSplashes(botAI, action);
}

Unit* GetToCHumanSkull(PlayerbotAI* botAI, bool (*belongs)(Unit*))
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group || !belongs)
        return nullptr;

    ObjectGuid const skull = group->GetTargetIcon(RtiTargetValue::skullIndex);
    if (skull.IsEmpty())
        return nullptr;

    // In combat is the pre-pull guard: Attack() has no out-of-combat check of its own.
    Unit* unit = botAI->GetUnit(skull);
    if (!unit || !unit->IsAlive() || !unit->IsInCombat() || unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) ||
        !belongs(unit) || !bot->IsValidAttackTarget(unit))
        return nullptr;

    return unit;
}

bool ToCPetOffPick(PlayerbotAI* botAI, Unit* pick)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Guardian* pet = bot ? bot->GetGuardianPet() : nullptr;
    return pet && pick && pet->IsAlive() && pet->GetReactState() != REACT_PASSIVE &&
           bot->IsValidAttackTarget(pick) && pet->GetVictim() != pick;
}

void ToCRestoreLegacyRti(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || bot->GetMapId() != TRIAL_OF_THE_CRUSADER_MAP_ID)
        return;

    Value<std::string>* rti = botAI->GetAiObjectContext()->GetValue<std::string>("rti");
    std::string const mark = rti->Get();
    if (mark == "cross" || mark == "square" || mark == "triangle" || mark == "diamond")
        rti->Set("skull");
}

bool CastToCThroughputHeal(PlayerbotAI* botAI, Unit* target, std::function<bool(std::string const&)> const& skip)
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

}
