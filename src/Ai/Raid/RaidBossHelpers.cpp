#include "RaidBossHelpers.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"

// Functions to mark targets with raid target icons
// Note that these functions do not allow the player to change the icon during the encounter
void MarkTargetWithIcon(Player* bot, Unit* target, uint8 iconId)
{
    if (!target)
        return;

    if (Group* group = bot->GetGroup())
    {
        ObjectGuid currentGuid = group->GetTargetIcon(iconId);
        if (currentGuid != target->GetGUID())
            group->SetTargetIcon(iconId, bot->GetGUID(), target->GetGUID());
    }
}

void MarkTargetWithSkull(Player* bot, Unit* target)
{
    MarkTargetWithIcon(bot, target, RtiTargetValue::skullIndex);
}

void MarkTargetWithSquare(Player* bot, Unit* target)
{
    MarkTargetWithIcon(bot, target, RtiTargetValue::squareIndex);
}

void MarkTargetWithStar(Player* bot, Unit* target)
{
    MarkTargetWithIcon(bot, target, RtiTargetValue::starIndex);
}

void MarkTargetWithCircle(Player* bot, Unit* target)
{
    MarkTargetWithIcon(bot, target, RtiTargetValue::circleIndex);
}

void MarkTargetWithDiamond(Player* bot, Unit* target)
{
    MarkTargetWithIcon(bot, target, RtiTargetValue::diamondIndex);
}

void MarkTargetWithTriangle(Player* bot, Unit* target)
{
    MarkTargetWithIcon(bot, target, RtiTargetValue::triangleIndex);
}

void MarkTargetWithCross(Player* bot, Unit* target)
{
    MarkTargetWithIcon(bot, target, RtiTargetValue::crossIndex);
}

void MarkTargetWithMoon(Player* bot, Unit* target)
{
    MarkTargetWithIcon(bot, target, RtiTargetValue::moonIndex);
}

// For bots to set their raid target icon to the specified icon on the specified target
void SetRtiTarget(PlayerbotAI* botAI, const std::string& rtiName, Unit* target)
{
    if (!target)
        return;

    std::string currentRti = botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Get();
    Unit* currentTarget = botAI->GetAiObjectContext()->GetValue<Unit*>("rti target")->Get();

    if (currentRti != rtiName || currentTarget != target)
    {
        botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set(rtiName);
        botAI->GetAiObjectContext()->GetValue<Unit*>("rti target")->Set(target);
    }
}

// For bots to assign the crowd-control raid icon to a target. Unlike SetRtiTarget (which drives the
// main "rti"/focus mark), this drives the parallel "rti cc" value the per-class "cc" strategy reads,
// so the two never compete: a skull-marked kill target and a moon-marked CC target coexist. The
// icon is placed on the unit and the "rti cc" value is pointed at that icon; RtiCcTargetValue then
// resolves the marked unit for the CC actions.
void SetRtiCcTarget(PlayerbotAI* botAI, const std::string& rtiName, Unit* target)
{
    if (!target)
        return;

    int32 const iconIndex = RtiTargetValue::GetRtiIndex(rtiName);
    if (iconIndex < 0)
        return;

    MarkTargetWithIcon(botAI->GetBot(), target, static_cast<uint8>(iconIndex));

    if (botAI->GetAiObjectContext()->GetValue<std::string>("rti cc")->Get() != rtiName)
        botAI->GetAiObjectContext()->GetValue<std::string>("rti cc")->Set(rtiName);
}

// Return the first alive DPS bot in the specified instance map, excluding any specified bot
// Intended for purposes of storing and erasing timers and trackers in associative containers
bool IsMechanicTrackerBot(PlayerbotAI* botAI, Player* bot, uint32 mapId, Player* exclude)
{
    if (!botAI->IsDps(bot) || !bot->IsAlive() || bot->GetMapId() != mapId)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != mapId || member == exclude)
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || !memberAI->IsDps(member))
            continue;

        return member == bot;
    }

    return false;
}

// Requires the main tank to be alive
// Note that IsMainTank() will return the player with the main tank flag, even if dead
Player* GetGroupMainTank(PlayerbotAI* botAI, Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive())
            continue;

        if (botAI->IsMainTank(member))
            return member;
    }

    return nullptr;
}

// Returns the alive assist tank of the specified index (0 = first, 1 = second, etc.)
// Priority: Assistants first, then Non-Assistants.
Player* GetGroupAssistTank(PlayerbotAI* botAI, Player* bot, uint8 index)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    uint8 assistantCount = 0;
    std::vector<Player*> nonAssistantTanks;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive())
            continue;

        if (botAI->IsAssistTank(member))
        {
            if (group->IsAssistant(member->GetGUID()))
            {
                if (assistantCount == index)
                    return member;

                assistantCount++;
            }
            else
            {
                nonAssistantTanks.push_back(member);
            }
        }
    }

    // If the index wasn't found among assistants, check the non-assistants that were saved
    uint8 nonAssistantIndex = index - assistantCount;
    if (nonAssistantIndex < nonAssistantTanks.size())
        return nonAssistantTanks[nonAssistantIndex];

    return nullptr;
}

// Return the first matching alive unit from PossibleTargetsValue within sightDistance from config
Unit* GetFirstAliveUnitByEntry(PlayerbotAI* botAI, uint32 entry)
{
    auto const& npcs =
        botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    for (auto const& npcGuid : npcs)
    {
        Unit* unit = botAI->GetUnit(npcGuid);
        if (unit && unit->IsAlive() && unit->GetEntry() == entry)
            return unit;
    }

    return nullptr;
}

// Return the nearest alive player (human or bot) within the specified radius
Unit* GetNearestPlayerInRadius(Player* bot, float radius)
{
    Unit* nearestPlayer = nullptr;
    float nearestDistance = radius;

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref != nullptr; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || member == bot)
                continue;

            float distance = bot->GetExactDist2d(member);
            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearestPlayer = member;
            }
        }
    }

    return nearestPlayer;
}

// Return true when bot sits inside source's frontal cone: within range and inside the half-angle arc
bool IsBotInFrontalCone(Player* bot, Unit* source, float coneAngle, float range)
{
    return bot && source && source->GetExactDist2d(bot) <= range && source->HasInArc(coneAngle, bot);
}
