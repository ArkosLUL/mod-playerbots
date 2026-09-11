/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EncounterHelpers.h"
#include "CellImpl.h"
#include "DKActions.h"
#include "DruidActions.h"
#include "DruidBearActions.h"
#include "DruidCatActions.h"
#include "GenericSpellActions.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "PaladinActions.h"
#include "Playerbots.h"
#include "CreatureAI.h"
#include "RogueActions.h"
#include "RtiTargetValue.h"
#include "ShamanActions.h"
#include "WarlockActions.h"
#include "WarriorActions.h"
#include <algorithm>
#include <cmath>
#include <list>

namespace EncounterHelpers
{

// Calculate incremental movement to a position. No ground or collision is validated. The
// Z position passed for the MoveTo() action using this helper should use the bot's Z, not the
// position's. Returns false once the bot is within arrivalDist.
bool GetStepToPosition(
    Player* bot, Position const& position, float arrivalDist, Unit* facing, float& stepX,
    float& stepY, bool& backwards)
{
    float const distToPosition = bot->GetExactDist2d(position);
    if (distToPosition <= arrivalDist)
        return false;

    float const botX = bot->GetPositionX();
    float const botY = bot->GetPositionY();
    float const toPosX = position.GetPositionX() - botX;
    float const toPosY = position.GetPositionY() - botY;

    // 'facing' is optional and is for tanks. Pass the mob being tanked to allow the step to be
    // walked backwards when (1) the bot has aggro on the mob it is tanking, (2) the bot is in
    // melee range of the mob, and (3) the destination is on the opposite side of the bot from the
    // mob. Generally, the entire movement would be gated on (1) and (2) anyway, but there are some
    // exceptions and thus the checks are made again here. Pass nullptr for a plain forward step.
    backwards = false;
    if (facing && facing->GetVictim() == bot && bot->IsWithinMeleeRange(facing))
    {
        float const toFacingX = facing->GetPositionX() - botX;
        float const toFacingY = facing->GetPositionY() - botY;
        backwards = (toPosX * toFacingX + toPosY * toFacingY) < 0.0f;
    }

    // Default time between AI ticks is 100ms, and base movement speed for players is 7y/s forwards
    // and 4.5y/s backwards (i.e., 0.7y/0.45y per tick). There is not really benefit to having the
    // step be farther than the distance that can be covered in a single tick. But this helper
    // uses 5x tick distance to account for possible speed boosts, latency, and longer configured
    // AI ticks. In my experience, this is plenty short enough to navigate poor terrain, but if you
    // are moving steeply uphill and find that movement is failing, it may be possible that the step
    // distances would need to be even shorter (in which case you couldn't use this helper).
    constexpr float backwardDistancePerStep = 2.25f;
    constexpr float forwardDistancePerStep = 3.5f;
    float const maxMoveDist = backwards ? backwardDistancePerStep : forwardDistancePerStep;
    float const ratio = std::min(maxMoveDist, distToPosition) / distToPosition;

    stepX = botX + toPosX * ratio;
    stepY = botY + toPosY * ratio;

    return true;
}

// Functions to mark targets with raid target icons.
// Note that these functions do not allow the player to change the icon during the encounter.
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

// For clearing marks outside of combat so bots don't Leeroy on sight. This is best used when gated
// behind an out-of-combat check (such as with IsInCombatValue).
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

// For bots to set their raid target icon to the specified icon
void SetRtiTarget(PlayerbotAI* botAI, std::string const& rtiName)
{
    Value<std::string>* rtiValue =
        botAI->GetAiObjectContext()->GetValue<std::string>("rti");

    if (rtiValue->Get() != rtiName)
        rtiValue->Set(rtiName);
}

// Points "rti target" at an explicit unit as well, for an encounter that has to keep the focus on
// one creature rather than on whatever the bot happens to be hitting.
void SetRtiTarget(PlayerbotAI* botAI, std::string const& rtiName, Unit* target)
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
void SetRtiCcTarget(PlayerbotAI* botAI, std::string const& rtiName, Unit* target)
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

// Return the first alive bot in the specified instance map for purposes of assigning
// a single bot to manage associative containers, mark targets, etc.
bool IsMechanicTrackerBot(Player* bot, uint32 mapId)
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

// Requires the main tank to be alive
Player* GetGroupMainTank(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    ObjectGuid const mainTankGuid = PlayerbotAI::GetMainTankGuid(group);
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
Player* GetGroupAssistTank(Player* bot, uint8 index)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    ObjectGuid const mainTankGuid = PlayerbotAI::GetMainTankGuid(group);
    if (mainTankGuid.IsEmpty())
        return nullptr;

    uint8 assistantCount = 0;
    std::vector<Player*> nonAssistantTanks;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !PlayerbotAI::IsTank(member) ||
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

// DO NOT USE. TO BE REMOVED HERE ONCE ALL CALL SITES ARE MODIFIED.
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

// Stalagg and Feugen survive their "death" at 1 HP in feign death, so treat unselectable or
// lying-down creatures as down instead of trusting IsAlive().
bool IsDownOrFeigning(Unit const* unit)
{
    if (!unit || !unit->IsAlive())
        return true;

    return unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) || unit->getStandState() == UNIT_STAND_STATE_DEAD;
}

// Return the nearest alive player (human or bot) within the specified radius. Distance is
// measured by GetExactDist2d(), which does not take into account either player's CombatReach
// (i.e., their hitboxes), which are 1.5y for all races (or 1.95y with Bloodlust/Heroism active).
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

// Grid search for dynamic objects for methods to avoid dynobj-based AoE hazards.
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

Position FindNearestPositionClearOfHazards(Player* bot, std::vector<HazardCircle> const& hazards, float maxRadius,
                                           float distanceStep, float angleStep, Position const* preferNear,
                                           std::function<bool(float, float)> const& accept)
{
    if (hazards.empty() || distanceStep <= 0.0f || angleStep <= 0.0f)
        return Position();

    auto const clearOf = [&hazards](float x, float y)
    {
        for (HazardCircle const& hazard : hazards)
            if (hazard.first.GetExactDist2d(x, y) < hazard.second)
                return false;

        return true;
    };

    // Rings outward, so the first hit is also the shortest walk. Nothing checks the path: a bot that
    // has to cross a hazard to leave one is still better off out the far side than standing still.
    for (float distance = distanceStep; distance <= maxRadius; distance += distanceStep)
    {
        Position best;
        float bestScore = 0.0f;
        bool found = false;

        for (float angle = 0.0f; angle < 2.0f * static_cast<float>(M_PI); angle += angleStep)
        {
            float x = bot->GetPositionX() + distance * std::cos(angle);
            float y = bot->GetPositionY() + distance * std::sin(angle);
            float z = bot->GetPositionZ();

            if (!clearOf(x, y))
                continue;

            if (!bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                               bot->GetPositionZ(), x, y, z))
                continue;

            // The collision check can pull the spot back short of the hazard it was clearing.
            if (!clearOf(x, y))
                continue;

            if (accept && !accept(x, y))
                continue;

            if (!preferNear)
                return Position(x, y, z, 0.0f);

            float const score = preferNear->GetExactDist2d(x, y);
            if (!found || score < bestScore)
            {
                best = Position(x, y, z, 0.0f);
                bestScore = score;
                found = true;
            }
        }

        // Ring by ring, so preferNear only ever reorders spots that are the same walk away and can
        // never talk the bot into a longer one.
        if (found)
            return best;
    }

    return Position();
}

Position FindNearestPositionClearOfHazards(Player* bot, std::vector<Position> const& hazards, float clearRadius,
                                           float maxRadius, float distanceStep, float angleStep)
{
    std::vector<HazardCircle> circles;
    circles.reserve(hazards.size());
    for (Position const& hazard : hazards)
        circles.emplace_back(hazard, clearRadius);

    return FindNearestPositionClearOfHazards(bot, circles, maxRadius, distanceStep, angleStep);
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

// This function is primarily for use in multipliers during encounters where it is desirable
// for bots to save cooldowns for particular phases (or for a bit after the pull).
bool IsDpsCooldownAction(Player* bot, Action* action)
{
    if (bot->getClass() == CLASS_SHAMAN && // Before dps gate to capture Resto
        (dynamic_cast<CastBloodlustAction*>(action) || dynamic_cast<CastHeroismAction*>(action)))
    {
        return true;
    }

    if (!PlayerbotAI::IsDps(bot))
        return false;

    if (dynamic_cast<UseTrinketAction*>(action))
        return true;

    bool isClassCooldown = false;
    switch (bot->getClass())
    {
        case CLASS_DEATH_KNIGHT:
            isClassCooldown = dynamic_cast<CastSummonGargoyleAction*>(action) ||
                dynamic_cast<CastDeathchillAction*>(action) ||
                dynamic_cast<CastEmpowerRuneWeaponAction*>(action) ||
                dynamic_cast<CastArmyOfTheDeadAction*>(action);
            break;

        case CLASS_DRUID:
            isClassCooldown = dynamic_cast<CastStarfallAction*>(action) ||
                dynamic_cast<CastForceOfNatureAction*>(action) ||
                dynamic_cast<CastBerserkAction*>(action);
            break;

        case CLASS_HUNTER:
            isClassCooldown = dynamic_cast<CastKillCommandAction*>(action) ||
                dynamic_cast<CastRapidFireAction*>(action) ||
                dynamic_cast<CastReadinessAction*>(action) ||
                dynamic_cast<CastBestialWrathAction*>(action);
            break;

        case CLASS_MAGE:
            isClassCooldown = dynamic_cast<CastArcanePowerAction*>(action) ||
                dynamic_cast<CastCombustionAction*>(action) ||
                dynamic_cast<CastIcyVeinsAction*>(action) ||
                dynamic_cast<CastMirrorImageAction*>(action) ||
                dynamic_cast<CastColdSnapAction*>(action) ||
                dynamic_cast<CastPresenceOfMindAction*>(action);
            break;

        case CLASS_SHAMAN:
            isClassCooldown = dynamic_cast<CastElementalMasteryAction*>(action) ||
                dynamic_cast<CastFeralSpiritAction*>(action) ||
                dynamic_cast<CastFireElementalTotemAction*>(action) ||
                dynamic_cast<CastFireElementalTotemMeleeAction*>(action);
            break;

        case CLASS_PALADIN:
            isClassCooldown = dynamic_cast<CastAvengingWrathAction*>(action);
            break;

        case CLASS_ROGUE:
            isClassCooldown = dynamic_cast<CastKillingSpreeAction*>(action) ||
                dynamic_cast<CastBladeFlurryAction*>(action) ||
                dynamic_cast<CastAdrenalineRushAction*>(action) ||
                dynamic_cast<CastColdBloodAction*>(action);
            break;

        case CLASS_WARLOCK:
            isClassCooldown = dynamic_cast<CastMetamorphosisAction*>(action);
            break;

        case CLASS_WARRIOR:
            isClassCooldown = dynamic_cast<CastDeathWishAction*>(action) ||
                dynamic_cast<CastBladestormAction*>(action) ||
                dynamic_cast<CastRecklessnessAction*>(action);
            break;

        default:
            break; // Priest =(
    }

    if (isClassCooldown)
        return true;

    switch (bot->getRace())
    {
        case RACE_BLOODELF:
            return dynamic_cast<CastArcaneTorrentAction*>(action);

        case RACE_ORC:
            return dynamic_cast<CastBloodFuryAction*>(action);

        case RACE_TROLL:
            return dynamic_cast<CastBerserkingAction*>(action);

        default:
            return false;
    }
}

bool IsTauntAction(Player* bot, Action* action)
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    switch (bot->getClass())
    {
        case CLASS_DEATH_KNIGHT:
            return dynamic_cast<CastDarkCommandAction*>(action) ||
                dynamic_cast<CastDeathGripAction*>(action);

        case CLASS_DRUID:
            return dynamic_cast<CastGrowlAction*>(action) ||
                dynamic_cast<CastChallengingRoarAction*>(action);

        case CLASS_PALADIN:
            return dynamic_cast<CastHandOfReckoningAction*>(action) ||
                dynamic_cast<CastRighteousDefenseAction*>(action);

        case CLASS_WARRIOR:
            return dynamic_cast<CastTauntAction*>(action) ||
                dynamic_cast<CastChallengingShoutAction*>(action);

        default:
            return false;
    }
}

// These abilities can be particularly problematic on the pull for a council-type boss.
bool IsAoeThreatAction(Player* bot, Action* action)
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    switch (bot->getClass())
    {
        case CLASS_DEATH_KNIGHT:
            return dynamic_cast<CastDeathAndDecayAction*>(action) ||
                dynamic_cast<CastPestilenceAction*>(action) ||
                dynamic_cast<CastBloodBoilAction*>(action);

        case CLASS_DRUID:
            return dynamic_cast<CastSwipeBearAction*>(action);

        case CLASS_PALADIN:
            return dynamic_cast<CastAvengersShieldAction*>(action) ||
                dynamic_cast<CastConsecrationAction*>(action);

        case CLASS_WARRIOR:
            return dynamic_cast<CastThunderClapAction*>(action) ||
                dynamic_cast<CastShockwaveAction*>(action) ||
                dynamic_cast<CastCleaveAction*>(action);

        default:
            return false;
    }
}

Position ValidateFloorPoint(Player* bot, Position const& point)
{
    float x = point.GetPositionX();
    float y = point.GetPositionY();

    float z = bot->GetMapWaterOrGroundLevel(x, y, point.GetPositionZ());
    if (z <= INVALID_HEIGHT)
        z = point.GetPositionZ();

    bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                   bot->GetPositionZ(), x, y, z, false);

    return Position(x, y, z);
}

bool CastClassTaunt(PlayerbotAI* botAI, Unit* target)
{
    if (!target)
        return false;

    switch (botAI->GetBot()->getClass())
    {
        case CLASS_WARRIOR:
            return botAI->CastSpell("taunt", target);
        case CLASS_PALADIN:
            return botAI->CastSpell("hand of reckoning", target);
        case CLASS_DEATH_KNIGHT:
            return botAI->CastSpell("dark command", target);
        case CLASS_DRUID:
            return botAI->CastSpell("growl", target);
        default:
            return false;
    }
}

}
