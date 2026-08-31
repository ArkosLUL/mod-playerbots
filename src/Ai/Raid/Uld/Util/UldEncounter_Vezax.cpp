/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Vezax.h"

#include "Creature.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <array>
#include <list>

using namespace EncounterHelpers;

std::unordered_map<uint32, VezaxEncounterState> vezaxEncounterStates;

namespace
{

// Slot 0 sits on the row centre and later slots alternate outwards, so an under-filled row stays
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

Position VezaxPositionAt(Position const& centre, float bearing, float radius)
{
    return Position(centre.GetPositionX() + std::cos(bearing) * radius,
                    centre.GetPositionY() + std::sin(bearing) * radius, centre.GetPositionZ());
}

// One row of the formation. The arc width is not stored because it falls out of the spacing: a row
// three slots wide at 3.7 yd covers the same ground whatever radius it sits at.
struct VezaxSlotRow
{
    uint8 firstSlot;
    uint8 slotCount;
    float centreOffset;  // from ULDUAR_VEZAX_ARC_ORIENTATION
    float radius;
    float spacing;
    bool bossRelative;
};

constexpr float VEZAX_GROUP_OFFSET = ULDUAR_VEZAX_RANGED_GROUP_OFFSET;
constexpr uint8 VEZAX_ROW = ULDUAR_VEZAX_BLOCK_ROW_SLOTS;

// Healers get one row of six; each ranged group gets a near row, a far row and an overflow row.
constexpr std::array<VezaxSlotRow, 7> VEZAX_SLOT_ROWS = {{
    {0, ULDUAR_VEZAX_HEALER_SLOTS, 0.0f, ULDUAR_VEZAX_HEALER_RADIUS, ULDUAR_VEZAX_HEALER_SPACING,
     true},
    {6, VEZAX_ROW, VEZAX_GROUP_OFFSET, ULDUAR_VEZAX_RANGED_NEAR_RADIUS, ULDUAR_VEZAX_RANGED_SPACING,
     false},
    {9, VEZAX_ROW, VEZAX_GROUP_OFFSET, ULDUAR_VEZAX_RANGED_FAR_RADIUS, ULDUAR_VEZAX_RANGED_SPACING,
     false},
    {12, VEZAX_ROW, -VEZAX_GROUP_OFFSET, ULDUAR_VEZAX_RANGED_NEAR_RADIUS,
     ULDUAR_VEZAX_RANGED_SPACING, false},
    {15, VEZAX_ROW, -VEZAX_GROUP_OFFSET, ULDUAR_VEZAX_RANGED_FAR_RADIUS, ULDUAR_VEZAX_RANGED_SPACING,
     false},
    {18, VEZAX_ROW, VEZAX_GROUP_OFFSET, ULDUAR_VEZAX_RANGED_OVERFLOW_RADIUS,
     ULDUAR_VEZAX_RANGED_SPACING, false},
    {21, VEZAX_ROW, -VEZAX_GROUP_OFFSET, ULDUAR_VEZAX_RANGED_OVERFLOW_RADIUS,
     ULDUAR_VEZAX_RANGED_SPACING, false},
}};

// Claim order, not index order. Taking the lowest free index would fill group L completely before
// group R started, and eight ranged would end up 6/2 across a split whose whole point is that a
// Shadow Crash on one side leaves the other working.
constexpr std::array<uint8, ULDUAR_VEZAX_TOTAL_SLOTS> VEZAX_SLOT_FILL_ORDER = {{
    0, 1, 2, 3, 4, 5,        // healers, already centred by the offset above
    6, 12, 7, 13, 8, 14,     // near rows, alternating groups
    9, 15, 10, 16, 11, 17,   // far rows
    18, 21, 19, 22, 20, 23,  // overflow, only once both groups are full
}};

bool TryGetVezaxSlotRow(uint8 slotIndex, VezaxSlotRow& row)
{
    for (VezaxSlotRow const& candidate : VEZAX_SLOT_ROWS)
    {
        if (slotIndex >= candidate.firstSlot && slotIndex < candidate.firstSlot + candidate.slotCount)
        {
            row = candidate;
            return true;
        }
    }

    return false;
}

bool VezaxTakesSlot(Player* member)
{
    return member && PlayerbotAI::IsRanged(member) && !PlayerbotAI::IsMainTank(member);
}

bool VezaxSlotIsHealerSlot(uint8 slotIndex) { return slotIndex < ULDUAR_VEZAX_HEALER_SLOTS; }

bool VezaxSlotSuitsBot(Player* bot, uint8 slotIndex)
{
    return VezaxSlotIsHealerSlot(slotIndex) == PlayerbotAI::IsHeal(bot);
}

float VezaxSlotToleranceFor(uint8 slotIndex)
{
    return VezaxSlotIsHealerSlot(slotIndex) ? ULDUAR_VEZAX_HEALER_SLOT_TOLERANCE
                                            : ULDUAR_VEZAX_SLOT_TOLERANCE;
}

// Which block a slot belongs to, for the trace. The index alone is readable only against the table
// above, and a postmortem is read without the source next to it.
char const* VezaxSlotBlockName(uint8 slotIndex)
{
    if (VezaxSlotIsHealerSlot(slotIndex))
        return "heal";
    if (slotIndex < 12)
        return "left";
    if (slotIndex < 18)
        return "right";

    return slotIndex < 21 ? "left-overflow" : "right-overflow";
}

uint8 VezaxManaPct(Player* bot)
{
    uint32 const maxMana = bot ? bot->GetMaxPower(POWER_MANA) : 0;
    if (!maxMana)
        return 0;

    return static_cast<uint8>(bot->GetPower(POWER_MANA) * 100 / maxMana);
}

// Split out of VezaxIsVaporHandler so the ranking loop can ask about other members without recursing
// back into the ranking itself.
bool VezaxIsVaporHandlerCandidate(Player* member)
{
    if (!member || member->GetMaxPower(POWER_MANA) == 0 || PlayerbotAI::IsTank(member))
        return false;

    return VezaxManaPct(member) < ULDUAR_VEZAX_VAPOR_HANDLER_MANA_PCT;
}

Position VezaxClampRadius(Position const& centre, Position const& spot, float minRadius,
                          float maxRadius)
{
    float const distance = centre.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY());
    if (distance >= minRadius && distance <= maxRadius)
        return spot;

    float const bearing =
        distance > 0.01f ? std::atan2(spot.GetPositionY() - centre.GetPositionY(),
                                      spot.GetPositionX() - centre.GetPositionX())
                         : ULDUAR_VEZAX_ARC_ORIENTATION;

    return VezaxPositionAt(centre, bearing, distance < minRadius ? minRadius : maxRadius);
}

}  // namespace

Unit* GetVezax(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    InstanceScript* instance = bot ? bot->GetInstanceScript() : nullptr;
    if (!instance)
        return nullptr;

    // The guid lookup has no liveness filter of its own, unlike the entry sweep it replaced, so a
    // dead boss has to be rejected here or every gate below stays open through the whole loot roll.
    Creature* vezax = instance->GetCreature(ULD_DATA_VEZAX);
    return vezax && vezax->IsAlive() ? vezax : nullptr;
}

bool VezaxEncounterActive(PlayerbotAI* botAI) { return GetVezax(botAI) != nullptr; }

bool VezaxFormationActive(PlayerbotAI* botAI)
{
    if (!botAI)
        return false;

    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    bool const inRoom =
        bot->GetExactDist2d(&ULDUAR_VEZAX_ANCHOR) <= ULDUAR_VEZAX_ARENA_RADIUS &&
        std::fabs(bot->GetPositionZ() - ULDUAR_VEZAX_ANCHOR.GetPositionZ()) <=
            ULDUAR_VEZAX_ARENA_HEIGHT;

    Unit* vezax = inRoom ? GetVezax(botAI) : nullptr;
    bool const active = vezax && vezax->IsInCombat();

    // Probed here rather than at the call sites: the trigger and the movement multiplier both route
    // through this, and a bot standing still with no slot is the symptom this answer explains. The
    // reason carries as much as the answer does - "outside" before the raid walks in is the design
    // working, while "noboss" means the instance lookup came back empty and every gate below it is
    // shut with nothing else in the trace to say so.
    char const* reason = "on";
    if (!inRoom)
        reason = "outside";
    else if (!vezax)
        reason = "noboss";
    else if (!active)
        reason = "idle";

    RaidObs::NoteDerived(bot, "vezax.formation", reason);

    return active;
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

        float const distance =
            bot->GetExactDist2d(candidate.position.GetPositionX(), candidate.position.GetPositionY());
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
    if (!bot || PlayerbotAI::IsHeal(bot) || !PlayerbotAI::IsRangedDps(bot))
        return false;

    return bot->GetMaxPower(POWER_MANA) > 0;
}

bool VezaxMustLeaveShadowCrashField(Player* bot) { return bot && PlayerbotAI::IsHeal(bot); }

bool VezaxWantsVaporPuddleMana(Player* bot)
{
    if (!bot)
        return false;

    uint32 const maxMana = bot->GetMaxPower(POWER_MANA);
    return maxMana > 0 && bot->GetPower(POWER_MANA) < maxMana;
}

bool VezaxMayStandInVaporPuddle(Player* bot)
{
    if (!bot || bot->GetMaxPower(POWER_MANA) == 0)
        return false;

    return VezaxIsVaporHandler(bot) || VezaxManaPct(bot) < sPlayerbotAIConfig.lowMana;
}

bool VezaxShouldLeaveVaporPuddle(Player* bot)
{
    if (!bot)
        return false;

    Aura* puddle = bot->GetAura(SPELL_VEZAX_SARONITE_VAPORS_PUDDLE);
    if (!puddle)
        return false;

    // The damage lands either way, so anyone who is not entitled to the mana leaves at the first
    // stack rather than waiting for the curve below to say so.
    if (!VezaxMayStandInVaporPuddle(bot))
        return true;

    float const nextTick = 100.0f * std::pow(2.0f, float(puddle->GetStackAmount() + 1));
    return nextTick >= float(bot->GetHealth()) * ULDUAR_VEZAX_VAPOR_SOAK_MAX_TICK_HP_PCT;
}

void VezaxBuildAvoidPositions(Player* bot, std::vector<VezaxHazard> const& hazards,
                              std::vector<Position>& avoid)
{
    avoid.clear();

    bool const avoidFields = VezaxMustLeaveShadowCrashField(bot);
    bool const avoidPuddles = !VezaxMayStandInVaporPuddle(bot);

    for (VezaxHazard const& hazard : hazards)
        if (hazard.isShadowCrashField ? avoidFields : avoidPuddles)
            avoid.push_back(hazard.position);
}

bool TryGetVezaxSlotPosition(Player* bot, uint8 slotIndex, Position& position)
{
    VezaxSlotRow row;
    if (!TryGetVezaxSlotRow(slotIndex, row))
        return false;

    Position centre = ULDUAR_VEZAX_ANCHOR;
    if (row.bossRelative)
    {
        // The healer ring hangs off the boss because the Shadow Crash exclusion is measured from him,
        // and a ranged pull can settle him yards off his spawn. With him gone there is no ring.
        PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
        Unit* vezax = botAI ? GetVezax(botAI) : nullptr;
        if (!vezax)
            return false;

        centre = vezax->GetPosition();
    }

    float const arcWidth = row.slotCount > 1 ? static_cast<float>(row.slotCount - 1) * row.spacing /
                                                   row.radius
                                             : 0.0f;

    float const bearing = Position::NormalizeOrientation(
        ULDUAR_VEZAX_ARC_ORIENTATION + row.centreOffset +
        VezaxArcSlotAngleOffset(slotIndex - row.firstSlot, row.slotCount, arcWidth));

    position = VezaxPositionAt(centre, bearing, row.radius);
    return true;
}

float VezaxSlotTolerance(Player* bot)
{
    if (!bot)
        return ULDUAR_VEZAX_SLOT_TOLERANCE;

    if (PlayerbotAI::IsMainTank(bot))
        return ULDUAR_VEZAX_TANK_SLOT_TOLERANCE;

    return PlayerbotAI::IsHeal(bot) ? ULDUAR_VEZAX_HEALER_SLOT_TOLERANCE
                                    : ULDUAR_VEZAX_SLOT_TOLERANCE;
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

        for (uint8 slotIndex : VEZAX_SLOT_FILL_ORDER)
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
    if (!bot)
        return false;

    // vezax.block is written at each way out rather than once in the middle, because a displaced bot
    // resolves twice in one call: probing early would emit its assigned block and then the fallback,
    // and NoteDerived's emit-on-change would read that as a flap every tick.

    // The main tank holds the anchor, which is Vezax's own spawn: he is already in melee range of a
    // tank standing there, so he never walks, and every radius measured from that point stays true.
    if (PlayerbotAI::IsMainTank(bot))
    {
        RaidObs::NoteDerived(bot, "vezax.block", "tank");
        position = ULDUAR_VEZAX_ANCHOR;
        return true;
    }

    if (!VezaxTakesSlot(bot))
        return false;

    EnsureVezaxSlotAssignments(bot);

    auto const stateItr = vezaxEncounterStates.find(bot->GetInstanceId());
    if (stateItr == vezaxEncounterStates.end())
        return false;

    VezaxEncounterState& state = stateItr->second;
    auto const assignmentItr = state.slotAssignments.find(bot->GetGUID());
    if (assignmentItr == state.slotAssignments.end())
    {
        // Every slot of this bot's kind is taken - 7 or more healers, or 19 or more other ranged. It
        // holds no position at all from here on, which nothing else in the trace would show.
        RaidObs::NoteDerived(bot, "vezax.block", "unslotted");
        return false;
    }

    std::vector<Position> avoid;
    VezaxBuildAvoidPositions(bot, hazards, avoid);

    Position ownSlot;
    if (!TryGetVezaxSlotPosition(bot, assignmentItr->second, ownSlot))
        return false;

    float const tolerance = VezaxSlotToleranceFor(assignmentItr->second);

    if (avoid.empty() || IsVezaxSpotSafe(ownSlot, avoid, tolerance))
    {
        RaidObs::NoteDerived(bot, "vezax.block", VezaxSlotBlockName(assignmentItr->second));
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
        if (!TryGetVezaxSlotPosition(bot, slotIndex, candidate))
            continue;

        if (!IsVezaxSpotSafe(candidate, avoid, VezaxSlotToleranceFor(slotIndex)))
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
        {
            RaidObs::NoteDerived(bot, "vezax.block", "stuck");
            return false;
        }

        // Off the formation entirely, on a spot no slot table knows about.
        RaidObs::NoteDerived(bot, "vezax.block", "loose");
        position = clear;
        return true;
    }

    // The displaced slot, not the assigned one: that is where the bot is actually walking.
    RaidObs::NoteDerived(bot, "vezax.block", VezaxSlotBlockName(displaced));
    state.displacedAssignments[bot->GetGUID()] = displaced;
    return TryGetVezaxSlotPosition(bot, displaced, position);
}

bool TryGetVezaxMarkSpot(Player* bot, Position& position)
{
    if (!bot)
        return false;

    float const ownRadius =
        ULDUAR_VEZAX_ANCHOR.GetExactDist2d(bot->GetPositionX(), bot->GetPositionY());

    // Ranged walk straight outward along the bearing they already hold. That clears the 15 yd drain
    // in 18 yd of travel instead of the 40-odd it takes to reach the far side of the room, and the
    // debuff only lasts 10s. The cap keeps them inside the arena bubble - past it the formation gate
    // goes false and the movement multiplier hands their generic movers back mid-run.
    if (VezaxTakesSlot(bot) && !PlayerbotAI::IsHeal(bot) && ownRadius > 1.0f)
    {
        float const bearing = std::atan2(bot->GetPositionY() - ULDUAR_VEZAX_ANCHOR.GetPositionY(),
                                         bot->GetPositionX() - ULDUAR_VEZAX_ANCHOR.GetPositionX());
        float const radius = std::min(ownRadius + ULDUAR_VEZAX_MARK_SEPARATION,
                                      ULDUAR_VEZAX_MARK_MAX_RADIUS);

        position = VezaxPositionAt(ULDUAR_VEZAX_ANCHOR, Position::NormalizeOrientation(bearing),
                                   radius);
        return true;
    }

    // Everyone else takes the nearest of the three spots behind the boss. The core prefers a target
    // further than 15 yd out whenever enough players are there, and the ranged always are, so this is
    // the rare branch rather than the common one.
    std::array<float, ULDUAR_VEZAX_MARK_SPOT_COUNT> const bearings = {
        ULDUAR_VEZAX_ARC_ORIENTATION + ULDUAR_VEZAX_MARK_SPOT_ARC_OFFSET,
        ULDUAR_VEZAX_ARC_ORIENTATION - ULDUAR_VEZAX_MARK_SPOT_ARC_OFFSET,
        ULDUAR_VEZAX_ARC_ORIENTATION + static_cast<float>(M_PI)};

    bool found = false;
    float bestDistance = std::numeric_limits<float>::max();

    for (float bearing : bearings)
    {
        Position const candidate = VezaxPositionAt(
            ULDUAR_VEZAX_ANCHOR, Position::NormalizeOrientation(bearing),
            ULDUAR_VEZAX_MARK_SPOT_RADIUS);

        float const distance = bot->GetExactDist2d(candidate.GetPositionX(), candidate.GetPositionY());
        if (found && distance >= bestDistance)
            continue;

        position = candidate;
        bestDistance = distance;
        found = true;
    }

    return found;
}

bool TryGetVezaxShadowCrashImpact(PlayerbotAI* botAI, Position& impact)
{
    Unit* boss = GetVezax(botAI);
    if (!boss)
        return false;

    Spell* spell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!spell || !spell->m_spellInfo || spell->m_spellInfo->Id != SPELL_VEZAX_SHADOW_CRASH_CAST)
        return false;

    // TARGET_DEST_TARGET_ENEMY, so the destination was resolved when the missile went out and does
    // not follow whoever it was aimed at. The unit fallback only matters if that ever changes.
    if (WorldLocation const* dst = spell->m_targets.GetDstPos())
    {
        impact.Relocate(dst->GetPositionX(), dst->GetPositionY(), dst->GetPositionZ());
        return true;
    }

    Unit* target = spell->m_targets.GetUnitTarget();
    if (!target)
        return false;

    impact = target->GetPosition();
    return true;
}

bool TryGetVezaxDodgeSpot(Player* bot, Position const& impact, Position& spot)
{
    if (!bot)
        return false;

    std::vector<Position> const avoid = {impact};
    Position const candidate = FindNearestPositionClearOfHazards(
        bot, avoid, ULDUAR_VEZAX_SHADOW_CRASH_DODGE_CLEARANCE,
        ULDUAR_VEZAX_SHADOW_CRASH_DODGE_SEARCH_RADIUS);

    // vezax.dodge is which rule produced the destination: the move record carries the coordinate and
    // the action that issued it, but nothing that separates these four. "none" is the search coming
    // up empty, "blast" the band being given up because the clamp landed back inside the impact.
    if (candidate == Position())
    {
        RaidObs::NoteDerived(bot, "vezax.dodge", "none");
        return false;
    }

    char const* branch = "band";

    // The helper answers with the nearest clear spot in any direction, which for a bot standing on
    // the impact is as likely to point into the melee ball as out of it. Healers get pulled back
    // inside the boss's target-exclusion radius instead: leaving it is what puts them in the pool.
    if (PlayerbotAI::IsHeal(bot))
    {
        branch = "heal";

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        Unit* vezax = botAI ? GetVezax(botAI) : nullptr;
        spot = vezax ? VezaxClampRadius(vezax->GetPosition(), candidate, 0.0f,
                                        ULDUAR_VEZAX_HEALER_DODGE_BAND_MAX)
                     : candidate;
    }
    else
    {
        spot = VezaxClampRadius(ULDUAR_VEZAX_ANCHOR, candidate, ULDUAR_VEZAX_DODGE_BAND_MIN,
                                ULDUAR_VEZAX_DODGE_BAND_MAX);
    }

    // Clamping moves the spot, so it can land back inside the blast. When it does, the unclamped
    // answer is the safer of the two - the band is a preference, the impact is 11310 and a knockback.
    if (spot.GetExactDist2d(impact.GetPositionX(), impact.GetPositionY()) <
        ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS)
    {
        branch = "blast";
        spot = candidate;
    }

    RaidObs::NoteDerived(bot, "vezax.dodge", branch);
    return true;
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

    bool interrupter = true;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMapId() != ULDUAR_MAP_ID)
            continue;

        if (member->GetGUID() < bot->GetGUID() && VezaxReadyInterrupt(member, boss))
        {
            interrupter = false;
            break;
        }
    }

    RaidObs::NoteDerived(bot, "vezax.interrupter", interrupter ? "1" : "0");
    return interrupter;
}

bool VezaxIsVaporHandler(Player* bot)
{
    if (!VezaxIsVaporHandlerCandidate(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    bool const healer = PlayerbotAI::IsHeal(bot);
    uint8 const mana = VezaxManaPct(bot);

    uint8 ahead = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMapId() != ULDUAR_MAP_ID)
            continue;

        if (!VezaxIsVaporHandlerCandidate(member))
            continue;

        // Healers first, then whoever is emptiest, and guid only to break an exact tie. Ranking on
        // the mana bars means the duty moves as they drain instead of sitting on the same two bots.
        bool const memberHealer = PlayerbotAI::IsHeal(member);
        if (memberHealer != healer)
        {
            if (memberHealer)
                ++ahead;

            continue;
        }

        uint8 const memberMana = VezaxManaPct(member);
        if (memberMana < mana || (memberMana == mana && member->GetGUID() < bot->GetGUID()))
            ++ahead;
    }

    bool const handler = ahead < ULDUAR_VEZAX_VAPOR_HANDLERS;
    RaidObs::NoteDerived(bot, "vezax.handler", handler ? "1" : "0");
    return handler;
}
