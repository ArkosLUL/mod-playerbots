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
#include "RaidInstanceState.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "Timer.h"
#include "UldHardMode.h"
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

static RaidInstanceState<VezaxEncounterState> vezaxEncounterStates;

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
    VezaxEncounterState const* state = bot ? vezaxEncounterStates.Find(bot->GetInstanceId()) : nullptr;
    if (!state)
        return false;

    auto const assignmentItr = state->slotAssignments.find(bot->GetGUID());
    if (assignmentItr == state->slotAssignments.end())
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

// He is the authority on his own pull, and combat is the only part of him that says so: the guid
// lookup above answers for the whole instance, so a bare "he is alive" test goes true the moment his
// grid loads. The guards hanging off this one zero the generic target pickers and hold caster damage,
// so an untested one shuts the raid down wherever it actually is - 2026-09-17 lost every `dps assist`
// on Razorscale to it.
bool VezaxEncounterActive(PlayerbotAI* botAI)
{
    Unit* vezax = GetVezax(botAI);
    return vezax && vezax->IsInCombat();
}

namespace
{
enum class VezaxFormation
{
    Outside,
    NoBoss,
    Idle,
    On
};

VezaxFormation ReadVezaxFormation(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot)
        return VezaxFormation::Outside;

    Unit* vezax = GetVezax(botAI);
    if (!vezax)
        return VezaxFormation::NoBoss;

    // Off him, not his spawn: every slot and reach spell's stop are measured from him, and this has to
    // hold all of them wherever he settles.
    bool const inRoom =
        bot->GetExactDist2d(vezax) <= ULDUAR_VEZAX_ARENA_RADIUS &&
        std::fabs(bot->GetPositionZ() - vezax->GetPositionZ()) <= ULDUAR_VEZAX_ARENA_HEIGHT;
    if (!inRoom)
        return VezaxFormation::Outside;

    return vezax->IsInCombat() ? VezaxFormation::On : VezaxFormation::Idle;
}
}  // namespace

bool VezaxFormationActive(PlayerbotAI* botAI) { return ReadVezaxFormation(botAI) == VezaxFormation::On; }

void TickVezax(PlayerbotAI* botAI)
{
    if (!RaidObs::Active())
        return;

    // A bot standing still with no slot is what this explains. "outside" before the raid walks in is
    // the design working, "noboss" means the instance lookup came back empty and every gate on the
    // formation is shut with nothing else in the trace to say so.
    char const* reason = "on";
    switch (ReadVezaxFormation(botAI))
    {
        case VezaxFormation::Outside:
            reason = "outside";
            break;
        case VezaxFormation::NoBoss:
            reason = "noboss";
            break;
        case VezaxFormation::Idle:
            reason = "idle";
            break;
        case VezaxFormation::On:
            break;
    }

    RaidObs::NoteDerived(botAI->GetBot(), "vezax.formation", reason);

    // Both flags come from spell hooks on the vapors, which nothing else in the trace would show firing.
    char const* hardMode = "off";
    if (IsVezaxHardModeActive(botAI))
    {
        VezaxEncounterState const* state = vezaxEncounterStates.Find(botAI->GetBot()->GetInstanceId());
        hardMode = !state                 ? "pending"
                   : state->vaporKilled    ? "lost"
                   : state->animusSummoned ? "animus"
                                           : "pending";
    }

    RaidObs::NoteDerived(botAI->GetBot(), "vezax.hardmode", hardMode);
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
    // The same test that hands out camp slots, so everyone the formation places also walks to a
    // field. Healers included: the -75% healing done is worth paying when nothing else on this boss
    // restores a point of mana.
    if (!bot || !PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsMainTank(bot))
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

    VezaxEncounterState& state = vezaxEncounterStates.For(bot->GetInstanceId());

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

    VezaxEncounterState const* state = vezaxEncounterStates.Find(bot->GetInstanceId());
    if (!state)
        return false;

    auto const assignmentItr = state->slotAssignments.find(bot->GetGUID());
    if (assignmentItr == state->slotAssignments.end())
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
        // Around the boss on its own group's side, never across him. An arc rather than a sideways
        // step because the offset then costs nothing in distance from the boss: the camp band holds
        // at both ends, and the dodge's own 15 yd strafe can no longer close the gap.
        float const side = VezaxSlotIsRightGroup(slotIndex) ? ULDUAR_VEZAX_MARK_SIDE_OFFSET
                                                            : -ULDUAR_VEZAX_MARK_SIDE_OFFSET;
        float const bearing = ULDUAR_VEZAX_ARC_ORIENTATION + side / ULDUAR_VEZAX_CAMP_RADIUS;

        RaidObs::NoteDerived(bot, "vezax.mark", "side");
        position = VezaxPositionAt(vezax->GetPosition(), Position::NormalizeOrientation(bearing),
                                   ULDUAR_VEZAX_CAMP_RADIUS);
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
    if (!GetVezax(botAI))
        return false;

    VezaxEncounterState const* state = vezaxEncounterStates.Find(botAI->GetBot()->GetInstanceId());
    if (!state || !state->crashWindowMs ||
        getMSTimeDiff(state->crashCastMs, getMSTime()) >= state->crashWindowMs)
    {
        return false;
    }

    impact = state->crashImpact;
    return true;
}

void VezaxNoteShadowCrash(Unit* vezax, Position const& impact, uint32 flightMs)
{
    if (!vezax)
        return;

    VezaxEncounterState& state = vezaxEncounterStates.For(vezax->GetInstanceId());
    state.crashImpact = impact;
    state.crashCastMs = getMSTime();
    state.crashWindowMs = flightMs + ULDUAR_VEZAX_SHADOW_CRASH_LAND_SLACK_MS;
}

bool VezaxHardModePending(PlayerbotAI* botAI)
{
    if (!IsVezaxHardModeActive(botAI) || !VezaxEncounterActive(botAI))
        return false;

    // No state yet just means nothing has happened in this pull.
    VezaxEncounterState const* state = vezaxEncounterStates.Find(botAI->GetBot()->GetInstanceId());
    return !state || (!state->vaporKilled && !state->animusSummoned);
}

void VezaxNoteVaporKilled(Unit* vapor)
{
    if (vapor)
        vezaxEncounterStates.For(vapor->GetInstanceId()).vaporKilled = true;
}

void VezaxNoteAnimusSummoned(Unit* vapor)
{
    if (!vapor)
        return;

    VezaxEncounterState& state = vezaxEncounterStates.For(vapor->GetInstanceId());
    state.animusSummoned = true;
    state.animusSummonMs = getMSTime();
}

void VezaxNoteVaporSummon(Unit* vezax)
{
    if (vezax)
        ++vezaxEncounterStates.For(vezax->GetInstanceId()).vaporSummons;
}

bool VezaxAnimusDue(PlayerbotAI* botAI)
{
    if (!VezaxHardModePending(botAI))
        return false;

    VezaxEncounterState const* state = vezaxEncounterStates.Find(botAI->GetBot()->GetInstanceId());
    return state && state->vaporSummons >= ULDUAR_VEZAX_REDIRECT_SAVE_SUMMON;
}

bool TryGetVezaxAnimusAge(PlayerbotAI* botAI, uint32& ageMs)
{
    VezaxEncounterState const* state = vezaxEncounterStates.Find(botAI->GetBot()->GetInstanceId());
    if (!state || !state->animusSummoned)
        return false;

    ageMs = getMSTimeDiff(state->animusSummonMs, getMSTime());
    return true;
}

char const* VezaxAnimusRedirectSpell(Player* bot)
{
    if (!bot)
        return nullptr;

    switch (bot->getClass())
    {
        case CLASS_HUNTER:
            return "misdirection";
        case CLASS_ROGUE:
            return "tricks of the trade";
        default:
            return nullptr;
    }
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
        vezaxEncounterStates.Reset(bot->GetInstanceId());
        return;
    }

    VezaxEncounterState* state = vezaxEncounterStates.Find(bot->GetInstanceId());
    if (!state)
        return;

    state->slotAssignments.erase(bot->GetGUID());
}

bool VezaxHasEncounterState(Player* bot)
{
    return bot && vezaxEncounterStates.Find(bot->GetInstanceId());
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
