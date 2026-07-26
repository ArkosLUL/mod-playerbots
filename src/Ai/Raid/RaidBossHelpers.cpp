/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidBossHelpers.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Playerbots.h"
#include "CreatureAI.h"
#include "RtiTargetValue.h"
#include <algorithm>
#include <cmath>

// Functions to mark targets with raid target icons
// Note that these functions do not allow the player to change the icon during the encounter
bool MarkTargetWithIcon(Player* bot, Unit* target, uint8 iconId)
{
    if (!target)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    ObjectGuid currentGuid = group->GetTargetIcon(iconId);
    if (currentGuid != target->GetGUID())
    {
        group->SetTargetIcon(iconId, bot->GetGUID(), target->GetGUID());
        return true;
    }

    return false;
}

bool MarkTargetWithSkull(Player* bot, Unit* target)
{
    return MarkTargetWithIcon(bot, target, RtiTargetValue::skullIndex);
}

bool MarkTargetWithSquare(Player* bot, Unit* target)
{
    return MarkTargetWithIcon(bot, target, RtiTargetValue::squareIndex);
}

bool MarkTargetWithStar(Player* bot, Unit* target)
{
    return MarkTargetWithIcon(bot, target, RtiTargetValue::starIndex);
}

bool MarkTargetWithCircle(Player* bot, Unit* target)
{
    return MarkTargetWithIcon(bot, target, RtiTargetValue::circleIndex);
}

bool MarkTargetWithDiamond(Player* bot, Unit* target)
{
    return MarkTargetWithIcon(bot, target, RtiTargetValue::diamondIndex);
}

bool MarkTargetWithTriangle(Player* bot, Unit* target)
{
    return MarkTargetWithIcon(bot, target, RtiTargetValue::triangleIndex);
}

bool MarkTargetWithCross(Player* bot, Unit* target)
{
    return MarkTargetWithIcon(bot, target, RtiTargetValue::crossIndex);
}

bool MarkTargetWithMoon(Player* bot, Unit* target)
{
    return MarkTargetWithIcon(bot, target, RtiTargetValue::moonIndex);
}

bool ClearTargetIcon(Player* bot, uint8 iconId)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    ObjectGuid currentGuid = group->GetTargetIcon(iconId);
    if (currentGuid != ObjectGuid::Empty)
    {
        group->SetTargetIcon(iconId, bot->GetGUID(), ObjectGuid::Empty);
        return true;
    }

    return false;
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
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != mapId ||
            !GET_PLAYERBOT_AI(member))
        {
            continue;
        }

        return member == bot;
    }

    return false;
}

bool IsMechanicTrackerBot(Player* bot, uint32 mapId)
{
    return IsMechanicTrackerBot(nullptr, bot, mapId, nullptr);
}

// Requires the main tank to be alive
Player* GetGroupMainTank(PlayerbotAI* botAI, Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    ObjectGuid const mainTankGuid = botAI->GetMainTankGuid(group);
    if (mainTankGuid.IsEmpty())
        return nullptr;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && member->GetGUID() == mainTankGuid)
            return member;
    }

    return nullptr;
}

// Returns the alive assist tank of the specified index (0 = first, 1 = second, etc.)
Player* GetGroupAssistTank(PlayerbotAI* botAI, Player* bot, uint8 index)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    ObjectGuid const mainTankGuid = botAI->GetMainTankGuid(group);
    if (mainTankGuid.IsEmpty())
        return nullptr;

    uint8 assistantCount = 0;
    std::vector<Player*> nonAssistantTanks;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !botAI->IsTank(member) ||
            member->GetGUID() == mainTankGuid)
        {
            continue;
        }

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

    uint8 nonAssistantIndex = index - assistantCount;
    if (nonAssistantIndex < nonAssistantTanks.size())
        return nonAssistantTanks[nonAssistantIndex];

    return nullptr;
}

// Return the first matching alive unit from PossibleTargetsValue within sightDistance from config
// Note that PossibleTargetsValue picks up only hostile units
Unit* GetFirstAliveUnitByEntry(PlayerbotAI* botAI, uint32 entry)
{
    auto const& units =
        botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    for (auto const& unitGuid : units)
    {
        Unit* unit = botAI->GetUnit(unitGuid);
        if (unit && unit->IsAlive() && unit->GetEntry() == entry)
            return unit;
    }

    return nullptr;
}

// Return the nearest alive player (human or bot) within the specified radius. Distance is
// measured by GetExactDist2d(), which does not take into account player hitboxes (1.5y).
Player* GetNearestPlayerInRadius(Player* bot, float radius)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Player* nearestPlayer = nullptr;
    float nearestDistance = radius;

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

    return nearestPlayer;
}

// Return true when bot sits inside source's frontal cone: within range and inside the half-angle arc
bool IsBotInFrontalCone(Player* bot, Unit* source, float coneAngle, float range)
{
    return bot && source && source->GetExactDist2d(bot) <= range && source->HasInArc(coneAngle, bot);
}

// Grid search for dynamic objects for methods to avoid dynobj-based AoE hazards
std::vector<Position> GetDynamicObjectPositions(Player* bot, float searchRadius, uint32 spellId)
{
    std::list<WorldObject*> objs;
    Acore::AllWorldObjectsInRange check(bot, searchRadius);
    Acore::WorldObjectListSearcher<Acore::AllWorldObjectsInRange> searcher(
        bot, objs, check, GRID_MAP_TYPE_MASK_DYNAMICOBJECT);
    Cell::VisitObjects(bot, searcher, searchRadius);

    std::vector<Position> dynObjs;
    for (WorldObject* obj : objs)
    {
        if (obj->GetTypeId() != TYPEID_DYNAMICOBJECT)
            continue;

        DynamicObject* dynObj = static_cast<DynamicObject*>(obj);
        if (dynObj->GetSpellId() == spellId)
        {
            dynObjs.emplace_back(
                dynObj->GetPositionX(), dynObj->GetPositionY(), dynObj->GetPositionZ());
        }
    }

    return dynObjs;
}

// Return the shortest-rotation spot just outside source's frontal cone, at the bot's current
// distance, so a bot caught in a cone attack sidesteps out of the arc instead of running the whole
// way behind the boss. coneAngle is the full arc width (matching IsBotInFrontalCone); margin is the
// extra clearance past the cone edge.
Position GetPositionOutsideFrontalCone(Player* bot, Unit* source, float coneAngle, float margin)
{
    float const distance = std::max(5.0f, source->GetExactDist2d(bot));
    float const facing = source->GetOrientation();

    // Signed bearing of the bot relative to where the boss is facing, in (-pi, pi]
    float diff = Position::NormalizeOrientation(source->GetAngle(bot) - facing);
    if (diff > M_PI)
        diff -= 2.0f * static_cast<float>(M_PI);

    // Rotate just past the cone edge on the side the bot is already on (shortest exit)
    float const edge = coneAngle / 2.0f + margin;
    float const targetAngle = Position::NormalizeOrientation(facing + (diff >= 0.0f ? edge : -edge));

    float const x = source->GetPositionX() + std::cos(targetAngle) * distance;
    float const y = source->GetPositionY() + std::sin(targetAngle) * distance;
    return Position(x, y, bot->GetPositionZ(), 0.0f);
}

// Command the bot's guardian pet onto target. Mirrors PetAttackAction, which is disabled
// globally, so scripted fights must redirect pets explicitly (e.g. off an immune boss).
void CommandPetAttack(PlayerbotAI* botAI, Unit* target)
{
    Player* bot = botAI->GetBot();
    Guardian* pet = bot->GetGuardianPet();
    if (!pet || !target)
        return;

    // Respect a passive pet stance and never attack an invalid target.
    if (pet->GetReactState() == REACT_PASSIVE)
        return;

    if (!bot->IsValidAttackTarget(target))
        return;

    // Already on target: avoid re-issuing the command every tick (would stutter the pet).
    if (pet->GetVictim() == target)
        return;

    pet->ClearUnitState(UNIT_STATE_FOLLOW);
    pet->AttackStop();
    pet->SetTarget(target->GetGUID());

    pet->GetCharmInfo()->SetIsCommandAttack(true);
    pet->GetCharmInfo()->SetIsAtStay(false);
    pet->GetCharmInfo()->SetIsFollowing(false);
    pet->GetCharmInfo()->SetIsCommandFollow(false);
    pet->GetCharmInfo()->SetIsReturning(false);

    pet->ToCreature()->AI()->AttackStart(target);
}

// Stop the bot's guardian pet and clear its target so it disengages the current victim.
void StopPet(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Guardian* pet = bot->GetGuardianPet();
    if (!pet)
        return;

    pet->AttackStop();
    pet->SetTarget(ObjectGuid::Empty);
    pet->GetCharmInfo()->SetIsCommandAttack(false);
}
