/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Algalon.h"

#include "Creature.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "Timer.h"
#include "Unit.h"

#include <array>
#include <cmath>
#include <limits>

std::unordered_map<uint32, AlgalonEncounterState> algalonEncounterStates;

namespace
{

// Every hostile in the encounter is read off "nearest npcs" rather than "possible targets": the
// holes, the Cosmic Smash markers and Algalon himself during the intro are all unselectable, and the
// attackable-target lists drop them.
std::vector<Unit*> CollectNpcs(PlayerbotAI* botAI, uint32 entry)
{
    std::vector<Unit*> found;
    if (!botAI)
        return found;

    auto const& npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get();
    for (auto const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && unit->GetEntry() == entry)
            found.push_back(unit);
    }

    return found;
}

Unit* FirstNpc(PlayerbotAI* botAI, uint32 entry)
{
    std::vector<Unit*> const found = CollectNpcs(botAI, entry);
    return found.empty() ? nullptr : found.front();
}

bool CastingBigBang(Unit* boss)
{
    if (!boss || !boss->HasUnitState(UNIT_STATE_CASTING))
        return false;

    return boss->FindCurrentSpellBySpellId(SPELL_ALGALON_BIG_BANG) ||
           boss->FindCurrentSpellBySpellId(SPELL_ALGALON_BIG_BANG_25);
}

Player* FindGroupMember(Group* group, ObjectGuid const& guid)
{
    if (!group || guid.IsEmpty())
        return nullptr;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->GetGUID() == guid)
            return member;
    }

    return nullptr;
}

// 62169 is the one "phased" aura in the encounter - the holes apply it, and so does the fifth Phase
// Punch stack. Either way the wearer is already out of Big Bang's reach.
bool IsPhasedOut(Player const* member)
{
    return member && member->HasAura(SPELL_ALGALON_BLACK_HOLE_DAMAGE);
}

// Lower is better. 0xFF means this bot must not be asked.
uint8 SoakTier(Player* member)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    if (!memberAI)
        return 0xFF;

    if (memberAI->CanCastSpell("dispersion", member))
        return 0;

    if (memberAI->CanCastSpell("guardian spirit", member))
        return 1;

    // No cooldown left anywhere in the raid. Someone still has to be a target when CheckTargets runs
    // or the spell finds nobody and Algalon evades outright, so a body it is - but never a tank, who
    // is holding the boss, and a healer only once no damage dealer is left.
    if (PlayerbotAI::IsTank(member))
        return 0xFF;

    return PlayerbotAI::IsHeal(member) ? 3 : 2;
}

// Out-param rather than two calls, because SoakTier walks CanCastSpell twice and this runs across the
// whole raid on every candidate.
bool CanBeSoaker(Player* member, uint8& tier)
{
    if (!member || !member->IsAlive() || member->GetMapId() != ULDUAR_MAP_ID || IsPhasedOut(member))
        return false;

    tier = SoakTier(member);
    return tier != 0xFF;
}

// Slot 0 sits on the arc centre and later slots alternate outwards, so an under-filled ring stays
// centred and keeps its spacing instead of bunching at one end.
float ArcSlotAngleOffset(uint8 slotIndex, uint8 slotCount, float arcWidth)
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

Position SlotPositionAt(float bearing, float radius)
{
    return Position(ULDUAR_ALGALON_TANK_SLOT.GetPositionX() + std::cos(bearing) * radius,
                    ULDUAR_ALGALON_TANK_SLOT.GetPositionY() + std::sin(bearing) * radius,
                    ULDUAR_ALGALON_TANK_SLOT.GetPositionZ());
}

bool SlotIsHealerSlot(uint8 slotIndex) { return slotIndex < ULDUAR_ALGALON_HEALER_SLOTS; }

}  // namespace

Unit* GetAlgalon(PlayerbotAI* botAI) { return FirstNpc(botAI, PB_NPC_ALGALON); }

bool AlgalonEncounterActive(PlayerbotAI* botAI)
{
    if (!GetAlgalon(botAI))
        return false;

    AlgalonTickEncounterState(botAI);
    return true;
}

void AlgalonTickEncounterState(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return;

    AlgalonEncounterState& state = algalonEncounterStates[bot->GetInstanceId()];
    if (state.lastTickMs && GetMSTimeDiffToNow(state.lastTickMs) < ULDUAR_ALGALON_STATE_TICK_MS)
        return;

    state.lastTickMs = getMSTime();

    uint8 const stars = static_cast<uint8>(CollectNpcs(botAI, PB_NPC_COLLAPSING_STAR).size());
    if (stars < state.starCount)
        state.lastStarDeathMs = state.lastTickMs;
    state.starCount = stars;

    bool const casting = CastingBigBang(GetAlgalon(botAI));
    if (casting && !state.bigBangCasting)
    {
        // Re-latched on every cast, not just the first, so the prediction self-corrects rather than
        // drifting away from an eight second cast over a six minute fight.
        state.lastBigBangMs = state.lastTickMs;
        if (!state.firstBigBangMs)
            state.firstBigBangMs = state.lastTickMs;

        state.bigBangSoaker = ObjectGuid::Empty;
    }

    state.bigBangCasting = casting;
}

bool AlgalonBigBangCasting(PlayerbotAI* botAI) { return CastingBigBang(GetAlgalon(botAI)); }

bool AlgalonBigBangWithin(PlayerbotAI* botAI, uint32 seconds)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot)
        return false;

    auto const stateItr = algalonEncounterStates.find(bot->GetInstanceId());
    if (stateItr == algalonEncounterStates.end() || !stateItr->second.firstBigBangMs)
        return false;

    uint32 const sinceLast = GetMSTimeDiffToNow(stateItr->second.lastBigBangMs);
    uint32 const intoCycle = sinceLast % ULDUAR_ALGALON_BIG_BANG_INTERVAL_MS;
    return ULDUAR_ALGALON_BIG_BANG_INTERVAL_MS - intoCycle <= seconds * IN_MILLISECONDS;
}

std::vector<Unit*> CollectAlgalonShelters(PlayerbotAI* botAI)
{
    std::vector<Unit*> shelters = CollectNpcs(botAI, PB_NPC_BLACK_HOLE);
    std::vector<Unit*> const wormHoles = CollectNpcs(botAI, PB_NPC_WORM_HOLE);
    shelters.insert(shelters.end(), wormHoles.begin(), wormHoles.end());
    return shelters;
}

uint8 AlgalonShelterCount(PlayerbotAI* botAI)
{
    return static_cast<uint8>(CollectAlgalonShelters(botAI).size());
}

Unit* GetAlgalonShelter(Player* bot)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI)
        return nullptr;

    AlgalonEncounterState& state = algalonEncounterStates[bot->GetInstanceId()];
    std::vector<Unit*> const shelters = CollectAlgalonShelters(botAI);

    // Hold the one already chosen. Holes land wherever a star died, so a fresh one appearing closer
    // mid-run would otherwise turn the bot around with the cast half over.
    auto const latched = state.shelterAssignments.find(bot->GetGUID());
    if (latched != state.shelterAssignments.end())
    {
        for (Unit* shelter : shelters)
            if (shelter->GetGUID() == latched->second)
                return shelter;

        state.shelterAssignments.erase(latched);
    }

    Unit* nearest = nullptr;
    float bestDistance = std::numeric_limits<float>::max();
    for (Unit* shelter : shelters)
    {
        float const distance = bot->GetExactDist2d(shelter);
        if (distance > ULDUAR_ALGALON_ROOM_SEARCH_RADIUS || (nearest && distance >= bestDistance))
            continue;

        nearest = shelter;
        bestDistance = distance;
    }

    if (nearest)
        state.shelterAssignments[bot->GetGUID()] = nearest->GetGUID();

    return nearest;
}

Unit* GetAlgalonShelterUnderfoot(Player* bot)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI)
        return nullptr;

    for (Unit* shelter : CollectAlgalonShelters(botAI))
        if (bot->GetExactDist2d(shelter) <= ULDUAR_ALGALON_SHELTER_RADIUS)
            return shelter;

    return nullptr;
}

bool AlgalonNeedsShelterUrgently(PlayerbotAI* botAI)
{
    if (AlgalonShelterCount(botAI) > 0)
        return false;

    return AlgalonBigBangCasting(botAI) || AlgalonBigBangWithin(botAI, ULDUAR_ALGALON_SHELTER_WINDOW_SECONDS);
}

Player* GetAlgalonBigBangSoaker(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group)
        return nullptr;

    AlgalonEncounterState& state = algalonEncounterStates[bot->GetInstanceId()];

    uint8 tier = 0xFF;
    Player* latched = FindGroupMember(group, state.bigBangSoaker);
    if (CanBeSoaker(latched, tier))
        return latched;

    state.bigBangSoaker = ObjectGuid::Empty;

    Player* best = nullptr;
    uint8 bestTier = 0xFF;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!CanBeSoaker(member, tier))
            continue;

        if (best && (tier > bestTier || (tier == bestTier && best->GetGUID() < member->GetGUID())))
            continue;

        best = member;
        bestTier = tier;
    }

    if (best)
        state.bigBangSoaker = best->GetGUID();

    return best;
}

Player* GetAlgalonBossTank(PlayerbotAI* botAI)
{
    Unit* boss = GetAlgalon(botAI);
    Unit* victim = boss ? boss->GetVictim() : nullptr;
    return victim ? victim->ToPlayer() : nullptr;
}

Player* GetAlgalonAddTank(PlayerbotAI* botAI, Player* bot)
{
    if (Player* thirdTank = GetGroupAssistTank(botAI, bot, 1))
        return thirdTank;

    return GetGroupAssistTank(botAI, bot, 0);
}

Unit* GetAlgalonKiteTarget(Player* bot)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI || bot == GetAlgalonBossTank(botAI))
        return nullptr;

    for (Unit* constellation : CollectNpcs(botAI, PB_NPC_LIVING_CONSTELLATION))
        if (!constellation->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) && constellation->GetVictim() == bot)
            return constellation;

    return nullptr;
}

Unit* GetAlgalonConstellationOnBossTank(PlayerbotAI* botAI)
{
    Player* bossTank = GetAlgalonBossTank(botAI);
    if (!bossTank)
        return nullptr;

    for (Unit* constellation : CollectNpcs(botAI, PB_NPC_LIVING_CONSTELLATION))
        if (!constellation->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) && constellation->GetVictim() == bossTank)
            return constellation;

    return nullptr;
}

Unit* GetAlgalonKiteHole(Player* bot, Unit* constellation)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI || !constellation)
        return nullptr;

    std::vector<Unit*> const shelters = CollectAlgalonShelters(botAI);
    if (shelters.empty())
        return nullptr;

    // A constellation eats the hole it walks into, and the raid needs one standing when Big Bang
    // lands. With a cast closing in, the last hole stops being a kite target.
    if (shelters.size() <= 1 && AlgalonBigBangWithin(botAI, ULDUAR_ALGALON_SHELTER_WINDOW_SECONDS))
        return nullptr;

    AlgalonEncounterState& state = algalonEncounterStates[bot->GetInstanceId()];

    auto const latched = state.kiteHoleAssignments.find(bot->GetGUID());
    if (latched != state.kiteHoleAssignments.end())
    {
        for (Unit* shelter : shelters)
            if (shelter->GetGUID() == latched->second)
                return shelter;

        state.kiteHoleAssignments.erase(latched);
    }

    // Nearest to the constellation, not to the kiter: the constellation is what has to arrive, and
    // it only ever walks as far as the bot leads it.
    Unit* nearest = nullptr;
    float bestDistance = std::numeric_limits<float>::max();
    for (Unit* shelter : shelters)
    {
        float const distance = constellation->GetExactDist2d(shelter);
        if (nearest && distance >= bestDistance)
            continue;

        nearest = shelter;
        bestDistance = distance;
    }

    if (nearest)
        state.kiteHoleAssignments[bot->GetGUID()] = nearest->GetGUID();

    return nearest;
}

uint8 AlgalonAliveStarCount(PlayerbotAI* botAI)
{
    return static_cast<uint8>(CollectNpcs(botAI, PB_NPC_COLLAPSING_STAR).size());
}

Unit* GetAlgalonFocusStar(PlayerbotAI* botAI)
{
    Unit* lowest = nullptr;
    for (Unit* star : CollectNpcs(botAI, PB_NPC_COLLAPSING_STAR))
        if (!lowest || star->GetHealthPct() < lowest->GetHealthPct())
            lowest = star;

    return lowest;
}

bool AlgalonStarKillWindowOpen(PlayerbotAI* botAI)
{
    Unit* star = GetAlgalonFocusStar(botAI);
    if (!star)
        return false;

    // No hole and a Big Bang coming: the raid needs the one this star leaves behind more than it
    // needs the health it is about to cost.
    if (AlgalonNeedsShelterUrgently(botAI))
        return true;

    // Collapse is about to finish the job anyway, and a death nobody chose is a death that lands on
    // top of the next one.
    if (star->GetHealthPct() <= ULDUAR_ALGALON_STAR_FINISH_HP_PCT)
        return true;

    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    auto const stateItr = algalonEncounterStates.find(bot->GetInstanceId());
    if (stateItr != algalonEncounterStates.end() && stateItr->second.lastStarDeathMs &&
        GetMSTimeDiffToNow(stateItr->second.lastStarDeathMs) < ULDUAR_ALGALON_STAR_PACING_GAP_MS)
    {
        return false;
    }

    // The explosion is unavoidable and hits everyone, so the raid's weakest member is what decides
    // whether it can be paid for.
    Group* group = bot->GetGroup();
    if (!group)
        return true;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != ULDUAR_MAP_ID)
            continue;

        if (member->GetHealthPct() < ULDUAR_ALGALON_STAR_PACING_RAID_HP_PCT)
            return false;
    }

    return true;
}

Unit* GetAlgalonCosmicSmashMarker(Player* bot)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI)
        return nullptr;

    for (uint32 entry : {NPC_ALGALON_ASTEROID_TARGET_1, NPC_ALGALON_ASTEROID_TARGET_2})
        for (Unit* marker : CollectNpcs(botAI, entry))
            if (bot->GetExactDist2d(marker) <= ULDUAR_ALGALON_COSMIC_SMASH_MARKER_RADIUS)
                return marker;

    return nullptr;
}

bool AlgalonCosmicSmashMarkerNear(PlayerbotAI* botAI, Position const& spot, float radius)
{
    for (uint32 entry : {NPC_ALGALON_ASTEROID_TARGET_1, NPC_ALGALON_ASTEROID_TARGET_2})
        for (Unit* marker : CollectNpcs(botAI, entry))
            if (spot.GetExactDist2d(marker->GetPositionX(), marker->GetPositionY()) <= radius)
                return true;

    return false;
}

bool AlgalonTakesRingSlot(Player* bot)
{
    return bot && PlayerbotAI::IsRanged(bot) && !PlayerbotAI::IsMainTank(bot) &&
           !PlayerbotAI::IsAssistTankOfIndex(bot, 0, true);
}

bool TryGetAlgalonSlotPosition(uint8 slotIndex, Position& position)
{
    if (slotIndex >= ULDUAR_ALGALON_TOTAL_SLOTS)
        return false;

    float radius = ULDUAR_ALGALON_HEALER_RADIUS;
    float arcCenter = ULDUAR_ALGALON_HEALER_ARC_CENTER;
    float arcWidth = ULDUAR_ALGALON_HEALER_ARC_WIDTH;
    uint8 slotCount = ULDUAR_ALGALON_HEALER_SLOTS;
    uint8 localIndex = slotIndex;

    if (slotIndex >= ULDUAR_ALGALON_HEALER_SLOTS + ULDUAR_ALGALON_RANGED_INNER_SLOTS)
    {
        radius = ULDUAR_ALGALON_RANGED_OUTER_RADIUS;
        arcCenter = ULDUAR_ALGALON_RANGED_OUTER_ARC_CENTER;
        arcWidth = ULDUAR_ALGALON_RANGED_OUTER_ARC_WIDTH;
        slotCount = ULDUAR_ALGALON_RANGED_OUTER_SLOTS;
        localIndex = slotIndex - ULDUAR_ALGALON_HEALER_SLOTS - ULDUAR_ALGALON_RANGED_INNER_SLOTS;
    }
    else if (slotIndex >= ULDUAR_ALGALON_HEALER_SLOTS)
    {
        radius = ULDUAR_ALGALON_RANGED_INNER_RADIUS;
        arcCenter = ULDUAR_ALGALON_RANGED_INNER_ARC_CENTER;
        arcWidth = ULDUAR_ALGALON_RANGED_INNER_ARC_WIDTH;
        slotCount = ULDUAR_ALGALON_RANGED_INNER_SLOTS;
        localIndex = slotIndex - ULDUAR_ALGALON_HEALER_SLOTS;
    }

    float const bearing =
        Position::NormalizeOrientation(arcCenter + ArcSlotAngleOffset(localIndex, slotCount, arcWidth));

    position = SlotPositionAt(bearing, radius);
    return true;
}

void EnsureAlgalonSlotAssignments(Player* bot)
{
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group || bot->GetMapId() != ULDUAR_MAP_ID)
        return;

    AlgalonEncounterState& state = algalonEncounterStates[bot->GetInstanceId()];

    std::vector<ObjectGuid> stale;
    for (auto const& assignment : state.slotAssignments)
    {
        Player* holder = FindGroupMember(group, assignment.first);
        if (!holder || holder->GetMapId() != ULDUAR_MAP_ID || !AlgalonTakesRingSlot(holder))
            stale.push_back(assignment.first);
    }

    for (ObjectGuid const& guid : stale)
        state.slotAssignments.erase(guid);

    std::array<bool, ULDUAR_ALGALON_TOTAL_SLOTS> used = {};
    for (auto const& assignment : state.slotAssignments)
        if (assignment.second < ULDUAR_ALGALON_TOTAL_SLOTS)
            used[assignment.second] = true;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member->GetMapId() != ULDUAR_MAP_ID || !AlgalonTakesRingSlot(member))
            continue;

        if (state.slotAssignments.find(member->GetGUID()) != state.slotAssignments.end())
            continue;

        // Healers want the inner ring - they measure range to the tank at GetRange("heal") = 30 yd -
        // but a raid with more healers than inner slots must not leave anyone unplaced, so the
        // preference is a first pass rather than a rule.
        bool const wantsHealerSlot = PlayerbotAI::IsHeal(member);
        bool placed = false;
        for (uint8 pass = 0; pass < 2 && !placed; ++pass)
        {
            for (uint8 slotIndex = 0; slotIndex < ULDUAR_ALGALON_TOTAL_SLOTS; ++slotIndex)
            {
                if (used[slotIndex] || (pass == 0 && SlotIsHealerSlot(slotIndex) != wantsHealerSlot))
                    continue;

                state.slotAssignments[member->GetGUID()] = slotIndex;
                used[slotIndex] = true;
                placed = true;
                break;
            }
        }
    }
}

bool TryGetAlgalonSlot(Player* bot, Position& position)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI)
        return false;

    Position slot;
    if (PlayerbotAI::IsMainTank(bot))
    {
        // The tank slot is the anchor for everything else: Algalon follows whoever holds him, so
        // parking the tank here is what puts the boss on the -Y edge facing away from the raid.
        slot = ULDUAR_ALGALON_TANK_SLOT;
    }
    else
    {
        if (!AlgalonTakesRingSlot(bot))
            return false;

        EnsureAlgalonSlotAssignments(bot);

        auto const stateItr = algalonEncounterStates.find(bot->GetInstanceId());
        if (stateItr == algalonEncounterStates.end())
            return false;

        auto const assignmentItr = stateItr->second.slotAssignments.find(bot->GetGUID());
        if (assignmentItr == stateItr->second.slotAssignments.end())
            return false;

        if (!TryGetAlgalonSlotPosition(assignmentItr->second, slot))
            return false;
    }

    // Phase 1 holes appear wherever a star happened to die - the star wanders 25 yd from its spawn
    // point before Collapse finishes it - so any slot can end up buried under one. The phase field
    // is only 6 yd across, so stepping off it beats abandoning the formation.
    std::vector<Position> holes;
    for (Unit* shelter : CollectAlgalonShelters(botAI))
        if (slot.GetExactDist2d(shelter) < ULDUAR_ALGALON_SHELTER_RADIUS + ULDUAR_ALGALON_SLOT_TOLERANCE)
            holes.push_back(shelter->GetPosition());

    if (holes.empty())
    {
        position = slot;
        return true;
    }

    Position const clear = FindNearestPositionClearOfHazards(
        bot, holes, ULDUAR_ALGALON_SHELTER_RADIUS + ULDUAR_ALGALON_SLOT_TOLERANCE,
        ULDUAR_ALGALON_SLOT_DISPLACE_RADIUS);
    if (clear == Position())
        return false;

    position = clear;
    return true;
}

void ResetAlgalonEncounterState(Player* bot, bool clearInstance)
{
    if (!bot)
        return;

    if (clearInstance)
    {
        algalonEncounterStates.erase(bot->GetInstanceId());
        return;
    }

    auto const stateItr = algalonEncounterStates.find(bot->GetInstanceId());
    if (stateItr == algalonEncounterStates.end())
        return;

    stateItr->second.slotAssignments.erase(bot->GetGUID());
    stateItr->second.shelterAssignments.erase(bot->GetGUID());
    stateItr->second.kiteHoleAssignments.erase(bot->GetGUID());
}
