/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Vezax.h"

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "UldScripts.h"
#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <array>
#include <list>

using namespace EncounterHelpers;

// Vezax' own spawn point, and the point the Saronite Vapors charge to when they merge.
const Position ULDUAR_VEZAX_ANCHOR = Position(1852.78f, 81.3856f, 342.461f);

std::unordered_map<uint32, VezaxEncounterState> vezaxEncounterStates;

namespace
{

Position VezaxPositionAt(Position const& centre, float bearing, float radius)
{
    return Position(centre.GetPositionX() + std::cos(bearing) * radius,
                    centre.GetPositionY() + std::sin(bearing) * radius, centre.GetPositionZ());
}

// A point in the camp frame: radius outward from the boss along the camp bearing, tangential offset
// across it. One helper because the resting slot and the strafe target differ only in that offset -
// keeping them on the same axes is what makes the strafe a pure sideways step.
Position VezaxCampPosition(Position const& boss, float radius, float tangentialOffset)
{
    float const forward = ULDUAR_VEZAX_ARC_ORIENTATION;
    float const across = ULDUAR_VEZAX_ARC_ORIENTATION + static_cast<float>(M_PI) / 2.0f;

    return Position(
        boss.GetPositionX() + std::cos(forward) * radius + std::cos(across) * tangentialOffset,
        boss.GetPositionY() + std::sin(forward) * radius + std::sin(across) * tangentialOffset,
        boss.GetPositionZ());
}

// Which group a slot belongs to, and where inside it. Rows run outward from the boss, files across
// the camp; the index packs them so a group's slots stay contiguous and the trace reads L or R
// straight off the number.
bool VezaxSlotIsRightGroup(uint8 slotIndex) { return slotIndex >= ULDUAR_VEZAX_GROUP_SLOTS; }

uint8 VezaxSlotRow(uint8 slotIndex)
{
    return static_cast<uint8>((slotIndex % ULDUAR_VEZAX_GROUP_SLOTS) / ULDUAR_VEZAX_CAMP_FILES);
}

uint8 VezaxSlotFile(uint8 slotIndex)
{
    return static_cast<uint8>(slotIndex % ULDUAR_VEZAX_CAMP_FILES);
}

// Tangential offset of a slot from the camp centre line, signed: negative is group L. File 0 is the
// inner one of its group, so the two inner files end up 2 * GROUP_OFFSET - FILE_SPACING apart.
float VezaxSlotTangentialOffset(uint8 slotIndex)
{
    float const fromGroupCentre =
        (static_cast<float>(VezaxSlotFile(slotIndex)) - 0.5f) * ULDUAR_VEZAX_CAMP_FILE_SPACING;
    float const magnitude = ULDUAR_VEZAX_CAMP_GROUP_OFFSET + fromGroupCentre;

    return VezaxSlotIsRightGroup(slotIndex) ? magnitude : -magnitude;
}

// Radius of a slot from the boss. Row 0 is nearest, and the rows straddle the camp radius so the
// middle of the camp sits where CAMP_RADIUS says it does.
float VezaxSlotRadius(uint8 slotIndex)
{
    float const centred =
        static_cast<float>(VezaxSlotRow(slotIndex)) - (ULDUAR_VEZAX_CAMP_ROWS - 1) * 0.5f;

    return ULDUAR_VEZAX_CAMP_RADIUS + centred * ULDUAR_VEZAX_CAMP_ROW_SPACING;
}

// Claim order, not index order. Taking the lowest free index would fill group L completely before
// group R started, and eight bots would end up 10/0 across a split whose whole point is that a Shadow
// Crash on one side leaves the other working. Near rows first, so an under-filled camp is short at
// the back rather than missing its front rank.
constexpr std::array<uint8, ULDUAR_VEZAX_TOTAL_SLOTS> VEZAX_SLOT_FILL_ORDER = {{
    0, 10, 1, 11,  // row 0, alternating groups
    2, 12, 3, 13,  // row 1
    4, 14, 5, 15,  // row 2
    6, 16, 7, 17,  // row 3
    8, 18, 9, 19,  // row 4
}};

bool VezaxTakesSlot(Player* member)
{
    return member && PlayerbotAI::IsRanged(member) && !PlayerbotAI::IsMainTank(member);
}

// Which group a slot belongs to, for the trace. The index alone is readable only against the layout
// above, and a postmortem is read without the source next to it.
char const* VezaxSlotBlockName(uint8 slotIndex)
{
    return VezaxSlotIsRightGroup(slotIndex) ? "R" : "L";
}

uint8 VezaxManaPct(Player* bot)
{
    uint32 const maxMana = bot ? bot->GetMaxPower(POWER_MANA) : 0;
    if (!maxMana)
        return 0;

    return static_cast<uint8>(bot->GetPower(POWER_MANA) * 100 / maxMana);
}

// The slot a bot dodges from, and the one that decides which side it carries a mark to. Both read the
// assignment rather than where the bot currently stands, so a group keeps its shape.
bool TryGetVezaxDodgeSlot(Player* bot, uint8& slotIndex)
{
    auto const stateItr = bot ? vezaxEncounterStates.find(bot->GetInstanceId())
                              : vezaxEncounterStates.end();
    if (stateItr == vezaxEncounterStates.end())
        return false;

    auto const assignmentItr = stateItr->second.slotAssignments.find(bot->GetGUID());
    if (assignmentItr == stateItr->second.slotAssignments.end())
        return false;

    slotIndex = assignmentItr->second;
    return true;
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
        hazards.push_back(hazard);
    }
}

bool TryGetVezaxNearestHazard(Player* bot, std::vector<VezaxHazard> const& hazards,
                              VezaxHazard& hazard)
{
    if (!bot)
        return false;

    bool found = false;
    float bestDistance = std::numeric_limits<float>::max();

    for (VezaxHazard const& candidate : hazards)
    {
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

bool VezaxCanSoakShadowCrashField(Player* bot)
{
    if (!bot || PlayerbotAI::IsHeal(bot) || !PlayerbotAI::IsRangedDps(bot))
        return false;

    return bot->GetMaxPower(POWER_MANA) > 0;
}

bool TryGetVezaxSlotPosition(Player* bot, uint8 slotIndex, Position& position)
{
    if (slotIndex >= ULDUAR_VEZAX_TOTAL_SLOTS)
        return false;

    // Boss-relative, not anchor-relative: every distance the camp is built on - the flight time the
    // strafe has to beat, and the exclusion that keeps crashes off the melee - is measured from him,
    // and a ranged pull can settle him yards off his spawn. With him gone there is no camp.
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    Unit* vezax = botAI ? GetVezax(botAI) : nullptr;
    if (!vezax)
        return false;

    position = VezaxCampPosition(vezax->GetPosition(), VezaxSlotRadius(slotIndex),
                                 VezaxSlotTangentialOffset(slotIndex));
    return true;
}

float VezaxSlotTolerance(Player* bot)
{
    if (!bot)
        return ULDUAR_VEZAX_SLOT_TOLERANCE;

    return PlayerbotAI::IsMainTank(bot) ? ULDUAR_VEZAX_TANK_SLOT_TOLERANCE
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

        if (!holder || holder->GetMapId() != ULDUAR_MAP_ID || !VezaxTakesSlot(holder))
            stale.push_back(assignment.first);
    }

    for (ObjectGuid const& guid : stale)
    {
        state.slotAssignments.erase(guid);
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
            if (used[slotIndex])
                continue;

            state.slotAssignments[member->GetGUID()] = slotIndex;
            used[slotIndex] = true;
            break;
        }
    }
}

bool TryGetVezaxSlot(Player* bot, Position& position)
{
    if (!bot)
        return false;

    // vezax.block is written at each way out rather than once in the middle: NoteDerived emits on
    // change, so a probe in the middle followed by one at the return reads as a flap every tick.

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
        // The camp is full - 21 or more healers and ranged between them. From here the bot holds no
        // position at all and falls through to the melee de-clump, which walks it onto the boss.
        // Nothing else in the trace would show that.
        RaidObs::NoteDerived(bot, "vezax.block", "unslotted");
        return false;
    }

    RaidObs::NoteDerived(bot, "vezax.block", VezaxSlotBlockName(assignmentItr->second));
    return TryGetVezaxSlotPosition(bot, assignmentItr->second, position);
}

bool TryGetVezaxMarkSpot(Player* bot, Position& position)
{
    if (!bot)
        return false;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    Unit* vezax = botAI ? GetVezax(botAI) : nullptr;
    if (!vezax)
        return false;

    // Off the boss rather than the anchor, for two reasons: the camp these spots are measured
    // against is boss-relative, and a destination that never moves by so much as a hundredth of a
    // yard latches IsDuplicateMove, which strands a marked bot for five seconds of a ten second
    // debuff. One traced pull issued six moves here against thirty-three duplicate rejections.
    uint8 slotIndex = 0;
    if (TryGetVezaxDodgeSlot(bot, slotIndex))
    {
        // Sideways on its own group's side, never across the boss. The southern spots are a diameter
        // away and the walk there drags the leech straight through the melee ball.
        float const side = VezaxSlotIsRightGroup(slotIndex) ? ULDUAR_VEZAX_MARK_SIDE_OFFSET
                                                            : -ULDUAR_VEZAX_MARK_SIDE_OFFSET;

        RaidObs::NoteDerived(bot, "vezax.mark", "side");
        position = VezaxCampPosition(vezax->GetPosition(), ULDUAR_VEZAX_CAMP_RADIUS, side);
        return true;
    }

    // A marked melee has no camp side to step to. South of the boss is the one direction that is
    // away from both the ball and the camp.
    std::array<float, ULDUAR_VEZAX_MARK_SPOT_COUNT> const bearings = {
        ULDUAR_VEZAX_ARC_ORIENTATION + ULDUAR_VEZAX_MARK_SPOT_ARC_OFFSET,
        ULDUAR_VEZAX_ARC_ORIENTATION - ULDUAR_VEZAX_MARK_SPOT_ARC_OFFSET,
        ULDUAR_VEZAX_ARC_ORIENTATION + static_cast<float>(M_PI)};

    bool found = false;
    float bestDistance = std::numeric_limits<float>::max();

    for (float bearing : bearings)
    {
        Position const candidate = VezaxPositionAt(
            vezax->GetPosition(), Position::NormalizeOrientation(bearing),
            ULDUAR_VEZAX_MARK_SPOT_RADIUS);

        float const distance = bot->GetExactDist2d(candidate.GetPositionX(), candidate.GetPositionY());
        if (found && distance >= bestDistance)
            continue;

        position = candidate;
        bestDistance = distance;
        found = true;
    }

    RaidObs::NoteDerived(bot, "vezax.mark", found ? "south" : "none");
    return found;
}

Unit* GetVezaxMarkedAlly(Player* bot)
{
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group)
        return nullptr;

    Unit* nearest = nullptr;
    float bestDistance = std::numeric_limits<float>::max();

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMapId() != ULDUAR_MAP_ID)
            continue;

        if (!member->HasAura(SPELL_MARK_OF_THE_FACELESS))
            continue;

        float const distance = bot->GetExactDist2d(member);
        if (distance > ULDUAR_VEZAX_MARK_BREAK_DISTANCE || (nearest && distance >= bestDistance))
            continue;

        nearest = member;
        bestDistance = distance;
    }

    return nearest;
}

bool TryGetVezaxMarkBreakSpot(Player* bot, Unit* marked, Position& spot)
{
    if (!bot || !marked)
        return false;

    // The marked ally is the hazard, and the clearance is the leech radius plus a yard and a half -
    // the DBC says 15 but the area test is IsWithinDist3d, which adds the victim's own combat reach.
    std::vector<Position> const avoid = {marked->GetPosition()};
    Position const candidate = FindNearestPositionClearOfHazards(
        bot, avoid, ULDUAR_VEZAX_MARK_BREAK_DISTANCE, ULDUAR_VEZAX_HAZARD_LOCAL_SEARCH_RADIUS);

    if (candidate == Position())
    {
        RaidObs::NoteDerived(bot, "vezax.mark", "none");
        return false;
    }

    RaidObs::NoteDerived(bot, "vezax.mark", "break");
    spot = candidate;
    return true;
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

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    Unit* vezax = botAI ? GetVezax(botAI) : nullptr;

    // The whole group steps the same way, so the impact keeps its bearing relative to everyone and
    // the camp arrives intact on the other side. A per-bot search for the nearest clear spot cannot
    // do that: it points each bot wherever its own footprint happens to be emptiest, which scatters
    // the group and lands several of them back under the next crash.
    uint8 slotIndex = 0;
    if (vezax && TryGetVezaxDodgeSlot(bot, slotIndex))
    {
        float const strafe = VezaxSlotIsRightGroup(slotIndex) ? ULDUAR_VEZAX_CAMP_STRAFE
                                                              : -ULDUAR_VEZAX_CAMP_STRAFE;
        Position const strafed =
            VezaxCampPosition(vezax->GetPosition(), VezaxSlotRadius(slotIndex),
                              VezaxSlotTangentialOffset(slotIndex) + strafe);

        // Worth checking rather than assuming: the strafe clears the impact by design, but the impact
        // is only where the crash went - a puddle already sitting on the far lane is not.
        if (strafed.GetExactDist2d(impact.GetPositionX(), impact.GetPositionY()) >=
            ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS)
        {
            RaidObs::NoteDerived(bot, "vezax.dodge", "strafe");
            spot = strafed;
            return true;
        }
    }

    // No slot, no boss, or the lane is buried. Anything out of the blast beats standing in it.
    std::vector<Position> const avoid = {impact};
    Position const candidate = FindNearestPositionClearOfHazards(
        bot, avoid, ULDUAR_VEZAX_SHADOW_CRASH_DODGE_CLEARANCE,
        ULDUAR_VEZAX_SHADOW_CRASH_DODGE_SEARCH_RADIUS);

    // vezax.dodge is which rule produced the destination: the move record carries the coordinate and
    // the action that issued it, but nothing that separates the three.
    if (candidate == Position())
    {
        RaidObs::NoteDerived(bot, "vezax.dodge", "none");
        return false;
    }

    RaidObs::NoteDerived(bot, "vezax.dodge", "search");
    spot = candidate;
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
