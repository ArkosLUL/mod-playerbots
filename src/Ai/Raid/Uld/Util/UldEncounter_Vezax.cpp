/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Vezax.h"

#include "Creature.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "SpellAuras.h"
#include "Unit.h"

#include <cmath>
#include <limits>
#include <array>
#include <list>

std::unordered_map<uint32, VezaxEncounterState> vezaxEncounterStates;

namespace
{

// Slot 0 sits on the arc centre and later slots alternate outwards, so an under-filled arc stays
// centred and keeps its spacing instead of bunching at one end.
float VezaxArcSlotAngleOffset(uint8 slotIndex, uint8 slotCount, float arcWidth)
{
    if (slotCount <= 1)
        return 0.0f;

    float const angleStep = arcWidth / static_cast<float>(slotCount - 1);

    if (slotCount % 2 == 1)
    {
        if (slotIndex == 0)
            return 0.0f;

        float const angleOffset = angleStep * static_cast<float>((slotIndex + 1) / 2);
        return slotIndex % 2 == 0 ? -angleOffset : angleOffset;
    }

    float const angleOffset = angleStep / 2.0f + angleStep * static_cast<float>(slotIndex / 2);
    return slotIndex % 2 == 1 ? -angleOffset : angleOffset;
}

Position VezaxPositionAt(float bearing, float radius)
{
    return Position(ULDUAR_VEZAX_ANCHOR.GetPositionX() + std::cos(bearing) * radius,
                    ULDUAR_VEZAX_ANCHOR.GetPositionY() + std::sin(bearing) * radius,
                    ULDUAR_VEZAX_ANCHOR.GetPositionZ());
}

bool VezaxTakesSlot(Player* member)
{
    return member && PlayerbotAI::IsRanged(member) && !PlayerbotAI::IsMainTank(member);
}

// Healers own the inner ring, every other ranged bot owns the two outer ones.
bool VezaxSlotIsHealerSlot(uint8 slotIndex) { return slotIndex < ULDUAR_VEZAX_HEALER_SLOTS; }

bool VezaxSlotSuitsBot(Player* bot, uint8 slotIndex)
{
    return VezaxSlotIsHealerSlot(slotIndex) == PlayerbotAI::IsHeal(bot);
}

}  // namespace

Unit* GetVezax(PlayerbotAI* botAI) { return GetFirstAliveUnitByEntry(botAI, NPC_VEZAX); }

bool VezaxEncounterActive(PlayerbotAI* botAI) { return GetVezax(botAI) != nullptr; }

bool VezaxFormationActive(PlayerbotAI* botAI)
{
    if (!botAI)
        return false;

    // Room test first: it is two float compares against a fixed point, and it keeps every bot outside
    // the hall off the grid sweep GetVezax costs.
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    if (bot->GetExactDist2d(&ULDUAR_VEZAX_ANCHOR) > ULDUAR_VEZAX_ARENA_RADIUS)
        return false;

    if (std::fabs(bot->GetPositionZ() - ULDUAR_VEZAX_ANCHOR.GetPositionZ()) >
        ULDUAR_VEZAX_ARENA_HEIGHT)
        return false;

    Unit* vezax = GetVezax(botAI);
    return vezax && vezax->IsInCombat();
}

void GatherVezaxEncounterTargets(PlayerbotAI* botAI, VezaxEncounterTargets& targets)
{
    if (!botAI)
        return;

    targets.vezax = GetVezax(botAI);
    targets.animus = GetFirstAliveUnitByEntry(botAI, NPC_VEZAX_SARONITE_ANIMUS);

    Player* bot = botAI->GetBot();
    if (!bot)
        return;

    std::list<Creature*> vapors;
    bot->GetCreatureListWithEntryInGrid(vapors, NPC_VEZAX_SARONITE_VAPORS,
                                        ULDUAR_VEZAX_HAZARD_SEARCH_RADIUS);

    for (Creature* vapor : vapors)
        if (vapor && vapor->IsAlive())
            targets.liveVapors.push_back(vapor);
}

void GatherVezaxHazards(Player* bot, std::vector<VezaxHazard>& hazards, float searchRadius)
{
    hazards.clear();
    if (!bot)
        return;

    for (Position const& position :
         GetDynamicObjectPositions(bot, searchRadius, SPELL_VEZAX_SHADOW_CRASH_FIELD))
    {
        VezaxHazard hazard;
        hazard.position = position;
        hazard.isShadowCrashField = true;
        hazards.push_back(hazard);
    }

    // A living vapor has no auras and no AI - it only wanders. The hazard is the puddle its corpse
    // carries, which is why this looks for dead ones and MoveAwayFromCreatureAction never could.
    std::list<Creature*> vapors;
    bot->GetCreatureListWithEntryInGrid(vapors, NPC_VEZAX_SARONITE_VAPORS, searchRadius);

    for (Creature* vapor : vapors)
    {
        if (!vapor || vapor->IsAlive())
            continue;

        VezaxHazard hazard;
        hazard.position = vapor->GetPosition();
        hazards.push_back(hazard);
    }
}

bool TryGetVezaxNearestHazard(Player* bot, std::vector<VezaxHazard> const& hazards,
                              bool wantShadowCrashField, VezaxHazard& hazard)
{
    if (!bot)
        return false;

    bool found = false;
    float bestDistance = std::numeric_limits<float>::max();

    for (VezaxHazard const& candidate : hazards)
    {
        if (candidate.isShadowCrashField != wantShadowCrashField)
            continue;

        float const distance = bot->GetExactDist2d(candidate.position.GetPositionX(),
                                                   candidate.position.GetPositionY());
        if (found && distance >= bestDistance)
            continue;

        hazard = candidate;
        bestDistance = distance;
        found = true;
    }

    return found;
}

bool IsVezaxSpotSafe(Position const& spot, std::vector<Position> const& avoid, float clearance)
{
    for (Position const& hazard : avoid)
        if (spot.GetExactDist2d(hazard.GetPositionX(), hazard.GetPositionY()) <
            ULDUAR_VEZAX_HAZARD_RADIUS + clearance)
        {
            return false;
        }

    return true;
}

bool VezaxCanSoakShadowCrashField(Player* bot)
{
    if (!bot || !PlayerbotAI::IsRangedDps(bot))
        return false;

    // Hunters answer IsRangedDps but the field boosts magic damage and mana cost only.
    if (bot->getClass() == CLASS_HUNTER)
        return false;

    return bot->GetMaxPower(POWER_MANA) > 0;
}

bool VezaxMustLeaveShadowCrashField(Player* bot)
{
    return bot && PlayerbotAI::IsHeal(bot);
}

bool VezaxWantsVaporPuddleMana(Player* bot)
{
    if (!bot)
        return false;

    uint32 const maxMana = bot->GetMaxPower(POWER_MANA);
    return maxMana > 0 && bot->GetPower(POWER_MANA) < maxMana;
}

bool VezaxShouldLeaveVaporPuddle(Player* bot)
{
    if (!bot)
        return false;

    Aura* puddle = bot->GetAura(SPELL_VEZAX_SARONITE_VAPORS_PUDDLE);
    if (!puddle)
        return false;

    // The damage lands either way, so anything that cannot spend the mana leaves at the first stack.
    if (!VezaxWantsVaporPuddleMana(bot))
        return true;

    float const nextTick = 100.0f * std::pow(2.0f, float(puddle->GetStackAmount() + 1));
    return nextTick >= float(bot->GetHealth()) * ULDUAR_VEZAX_VAPOR_SOAK_MAX_TICK_HP_PCT;
}

void VezaxBuildAvoidPositions(Player* bot, std::vector<VezaxHazard> const& hazards,
                              std::vector<Position>& avoid)
{
    avoid.clear();

    bool const avoidFields = VezaxMustLeaveShadowCrashField(bot);
    for (VezaxHazard const& hazard : hazards)
        if (!hazard.isShadowCrashField || avoidFields)
            avoid.push_back(hazard.position);
}

bool TryGetVezaxSlotPosition(uint8 slotIndex, Position& position)
{
    if (slotIndex >= ULDUAR_VEZAX_TOTAL_SLOTS)
        return false;

    float radius = ULDUAR_VEZAX_HEALER_RADIUS;
    uint8 localIndex = slotIndex;
    uint8 slotCount = ULDUAR_VEZAX_HEALER_SLOTS;

    if (slotIndex >= ULDUAR_VEZAX_HEALER_SLOTS + ULDUAR_VEZAX_RANGED_INNER_SLOTS)
    {
        radius = ULDUAR_VEZAX_RANGED_OUTER_RADIUS;
        slotCount = ULDUAR_VEZAX_RANGED_OUTER_SLOTS;
        localIndex = slotIndex - ULDUAR_VEZAX_HEALER_SLOTS - ULDUAR_VEZAX_RANGED_INNER_SLOTS;
    }
    else if (slotIndex >= ULDUAR_VEZAX_HEALER_SLOTS)
    {
        radius = ULDUAR_VEZAX_RANGED_INNER_RADIUS;
        slotCount = ULDUAR_VEZAX_RANGED_INNER_SLOTS;
        localIndex = slotIndex - ULDUAR_VEZAX_HEALER_SLOTS;
    }

    float const bearing = Position::NormalizeOrientation(
        ULDUAR_VEZAX_ARC_ORIENTATION +
        VezaxArcSlotAngleOffset(localIndex, slotCount, ULDUAR_VEZAX_ARC_WIDTH));

    position = VezaxPositionAt(bearing, radius);
    return true;
}

void EnsureVezaxSlotAssignments(Player* bot)
{
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group || bot->GetMapId() != ULDUAR_MAP_ID)
        return;

    VezaxEncounterState& state = vezaxEncounterStates[bot->GetInstanceId()];

    // Drop assignments whose holder left the instance or changed role.
    std::vector<ObjectGuid> stale;
    for (auto const& assignment : state.slotAssignments)
    {
        Player* holder = nullptr;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->GetGUID() == assignment.first)
            {
                holder = member;
                break;
            }
        }

        if (!holder || holder->GetMapId() != ULDUAR_MAP_ID || !VezaxTakesSlot(holder) ||
            !VezaxSlotSuitsBot(holder, assignment.second))
        {
            stale.push_back(assignment.first);
        }
    }

    for (ObjectGuid const& guid : stale)
    {
        state.slotAssignments.erase(guid);
        state.displacedAssignments.erase(guid);
    }

    std::array<bool, ULDUAR_VEZAX_TOTAL_SLOTS> used = {};
    for (auto const& assignment : state.slotAssignments)
        if (assignment.second < ULDUAR_VEZAX_TOTAL_SLOTS)
            used[assignment.second] = true;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member->GetMapId() != ULDUAR_MAP_ID || !VezaxTakesSlot(member))
            continue;

        if (state.slotAssignments.find(member->GetGUID()) != state.slotAssignments.end())
            continue;

        for (uint8 slotIndex = 0; slotIndex < ULDUAR_VEZAX_TOTAL_SLOTS; ++slotIndex)
        {
            if (used[slotIndex] || !VezaxSlotSuitsBot(member, slotIndex))
                continue;

            state.slotAssignments[member->GetGUID()] = slotIndex;
            used[slotIndex] = true;
            break;
        }
    }
}

bool TryGetVezaxSlot(Player* bot, std::vector<VezaxHazard> const& hazards, Position& position)
{
    if (!VezaxTakesSlot(bot))
        return false;

    EnsureVezaxSlotAssignments(bot);

    auto const stateItr = vezaxEncounterStates.find(bot->GetInstanceId());
    if (stateItr == vezaxEncounterStates.end())
        return false;

    VezaxEncounterState& state = stateItr->second;
    auto const assignmentItr = state.slotAssignments.find(bot->GetGUID());
    if (assignmentItr == state.slotAssignments.end())
        return false;

    std::vector<Position> avoid;
    VezaxBuildAvoidPositions(bot, hazards, avoid);

    Position ownSlot;
    if (!TryGetVezaxSlotPosition(assignmentItr->second, ownSlot))
        return false;

    if (avoid.empty() || IsVezaxSpotSafe(ownSlot, avoid, ULDUAR_VEZAX_SLOT_TOLERANCE))
    {
        state.displacedAssignments.erase(bot->GetGUID());
        position = ownSlot;
        return true;
    }

    // The slot is buried. Take the nearest slot of the same kind that is clear and unclaimed, rather
    // than fleeing to somewhere the formation does not know about.
    uint8 displaced = assignmentItr->second;
    float bestDistance = std::numeric_limits<float>::max();
    bool found = false;

    for (uint8 slotIndex = 0; slotIndex < ULDUAR_VEZAX_TOTAL_SLOTS; ++slotIndex)
    {
        if (slotIndex == assignmentItr->second || !VezaxSlotSuitsBot(bot, slotIndex))
            continue;

        bool claimed = false;
        for (auto const& assignment : state.slotAssignments)
        {
            if (assignment.second == slotIndex && assignment.first != bot->GetGUID())
            {
                claimed = true;
                break;
            }
        }

        if (claimed)
            continue;

        Position candidate;
        if (!TryGetVezaxSlotPosition(slotIndex, candidate))
            continue;

        if (!IsVezaxSpotSafe(candidate, avoid, ULDUAR_VEZAX_SLOT_TOLERANCE))
            continue;

        float const distance = bot->GetExactDist2d(candidate.GetPositionX(), candidate.GetPositionY());
        if (found && distance >= bestDistance)
            continue;

        displaced = slotIndex;
        bestDistance = distance;
        found = true;
    }

    if (!found)
    {
        // Every slot is covered. Step off the hazard rather than stand in it waiting for one.
        Position const clear = FindNearestPositionClearOfHazards(
            bot, avoid, ULDUAR_VEZAX_HAZARD_CLEARANCE, ULDUAR_VEZAX_HAZARD_LOCAL_SEARCH_RADIUS);
        if (clear == Position())
            return false;

        position = clear;
        return true;
    }

    state.displacedAssignments[bot->GetGUID()] = displaced;
    return TryGetVezaxSlotPosition(displaced, position);
}

bool TryGetVezaxMarkSpot(Player* bot, Position& position)
{
    if (!bot)
        return false;

    std::array<float, ULDUAR_VEZAX_MARK_SPOT_COUNT> const bearings = {
        ULDUAR_VEZAX_ARC_ORIENTATION + ULDUAR_VEZAX_MARK_SPOT_ARC_OFFSET,
        ULDUAR_VEZAX_ARC_ORIENTATION - ULDUAR_VEZAX_MARK_SPOT_ARC_OFFSET,
        ULDUAR_VEZAX_ARC_ORIENTATION + static_cast<float>(M_PI)};

    bool found = false;
    float bestDistance = std::numeric_limits<float>::max();

    for (float bearing : bearings)
    {
        Position const candidate =
            VezaxPositionAt(Position::NormalizeOrientation(bearing), ULDUAR_VEZAX_MARK_SPOT_RADIUS);

        float const distance = bot->GetExactDist2d(candidate.GetPositionX(), candidate.GetPositionY());
        if (found && distance >= bestDistance)
            continue;

        position = candidate;
        bestDistance = distance;
        found = true;
    }

    return found;
}

void ResetVezaxEncounterState(Player* bot, bool clearInstance)
{
    if (!bot)
        return;

    if (clearInstance)
    {
        vezaxEncounterStates.erase(bot->GetInstanceId());
        return;
    }

    auto const stateItr = vezaxEncounterStates.find(bot->GetInstanceId());
    if (stateItr == vezaxEncounterStates.end())
        return;

    stateItr->second.slotAssignments.erase(bot->GetGUID());
    stateItr->second.displacedAssignments.erase(bot->GetGUID());
}

char const* VezaxReadyInterrupt(Player* bot, Unit* target)
{
    if (!bot || !target)
        return nullptr;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return nullptr;

    static char const* const interrupts[] = {"kick",        "pummel",         "shield bash",
                                             "counterspell", "wind shear",    "mind freeze",
                                             "silencing shot", "spell lock"};

    for (char const* interrupt : interrupts)
        if (botAI->CanCastSpell(interrupt, target))
            return interrupt;

    return nullptr;
}

bool VezaxIsSearingFlamesInterrupter(Player* bot, Unit* boss)
{
    if (!bot || !boss || !VezaxReadyInterrupt(bot, boss))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMapId() != ULDUAR_MAP_ID)
            continue;

        if (member->GetGUID() < bot->GetGUID() && VezaxReadyInterrupt(member, boss))
            return false;
    }

    return true;
}

bool VezaxIsVaporKiller(Player* bot)
{
    if (!bot || !PlayerbotAI::IsRangedDps(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    uint8 ahead = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMapId() != ULDUAR_MAP_ID)
            continue;

        if (PlayerbotAI::IsRangedDps(member) && member->GetGUID() < bot->GetGUID())
            ++ahead;
    }

    return ahead < ULDUAR_VEZAX_VAPOR_KILLERS;
}
