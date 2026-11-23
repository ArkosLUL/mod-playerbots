/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Freya.h"

#include "AiObjectContext.h"
#include "AttackersValue.h"
#include "CellImpl.h"
#include "Creature.h"
#include "EncounterHelpers.h"
#include "GameObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Map.h"
#include "PetDefines.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "ThreatManager.h"
#include "Timer.h"
#include "UldScripts.h"
#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <list>
#include <vector>

using namespace EncounterHelpers;

namespace
{
bool FreshThisTick(uint32& atMs)
{
    uint32 const now = getMSTime();
    if (atMs && atMs == now)
        return true;

    atMs = now;
    return false;
}

struct FreyaStalkerInRangeCheck
{
    Acore::AnyUnitInObjectRangeCheck inRange;

    bool operator()(Unit* unit)
    {
        uint32 const entry = unit->GetEntry();

        return (entry == NPC_HEALTHY_SPORE || entry == NPC_FREYA_SUN_BEAM ||
                entry == NPC_FREYA_UNSTABLE_SUN_BEAM) &&
               inRange(unit);
    }
};

struct FreyaUnfriendlyOfEntryCheck
{
    Acore::AnyUnfriendlyUnitInObjectRangeCheck inRange;
    uint32 entry;

    bool operator()(Unit* unit) { return unit->GetEntry() == entry && inRange(unit); }
};
}

const Position ULDUAR_FREYA_TANK_ANCHOR = Position(2360.0847f, -43.1235f, 425.333f);

std::vector<Unit*> FreyaWaveState::LivingTrio() const
{
    std::vector<Unit*> living;
    for (Unit* member : {snaplasher, stormLasher, waterSpirit})
    {
        if (member && member->IsAlive())
            living.push_back(member);
    }

    return living;
}

bool FreyaWaveState::TrioLocked() const
{
    for (Unit* member : LivingTrio())
    {
        if (member->GetHealthPct() < ULDUAR_FREYA_TRIO_SYNC_WINDOW_PCT)
            return true;
    }

    return false;
}

bool FreyaWaveState::TrioReleased() const
{
    std::vector<Unit*> const living = LivingTrio();
    if (living.empty())
        return false;

    for (Unit* member : living)
    {
        if (member->GetHealthPct() > ULDUAR_FREYA_TRIO_FLOOR_RELEASE_PCT)
            return false;
    }

    return true;
}

void GatherFreyaWaveState(PlayerbotAI* botAI, FreyaWaveState& state)
{
    auto const keepMoreDamaged = [](Unit*& slot, Unit* candidate)
    {
        if (!slot || candidate->GetHealth() < slot->GetHealth())
            slot = candidate;
    };

    for (ObjectGuid const& guid : GetFreyaScan(botAI).PossibleTargets())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        switch (unit->GetEntry())
        {
            case NPC_EONARS_GIFT:
                keepMoreDamaged(state.eonarsGift, unit);
                break;
            case NPC_ANCIENT_CONSERVATOR:
                keepMoreDamaged(state.conservator, unit);
                break;
            case NPC_SNAPLASHER:
                keepMoreDamaged(state.snaplasher, unit);
                break;
            case NPC_STORM_LASHER:
                keepMoreDamaged(state.stormLasher, unit);
                break;
            case NPC_ANCIENT_WATER_SPIRIT:
                keepMoreDamaged(state.waterSpirit, unit);
                break;
            case NPC_DETONATING_LASHER:
                state.detonatingLashers.push_back(unit);
                break;
            default:
                break;
        }
    }
}

bool FreyaTrioSyncSuppress(FreyaWaveState const& state, Unit* target)
{
    if (!target)
        return false;

    if (target != state.snaplasher && target != state.stormLasher && target != state.waterSpirit)
        return false;

    std::vector<Unit*> const living = state.LivingTrio();

    if (living.size() < 3)
        return false;

    if (state.TrioReleased())
        return false;

    if (target->GetHealthPct() > ULDUAR_FREYA_TRIO_HARD_FLOOR_PCT)
        return false;

    for (Unit* member : living)
    {
        if (member != target && member->GetHealthPct() > ULDUAR_FREYA_TRIO_FLOOR_RELEASE_PCT)
            return true;
    }

    return false;
}

Unit* GetFreyaTrioAssignment(PlayerbotAI* botAI, FreyaWaveState const& state)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    std::vector<Unit*> candidates;
    for (Unit* member : state.LivingTrio())
    {
        if (!FreyaTrioSyncSuppress(state, member))
            candidates.push_back(member);
    }

    if (candidates.empty())
        candidates = state.LivingTrio();

    if (candidates.empty())
        return nullptr;

    std::vector<uint32> assigned(candidates.size(), 0);
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !PlayerbotAI::IsDps(member))
            continue;

        size_t pick = 0;
        float best = -1.0f;
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            float const share = float(candidates[i]->GetHealth()) / float(assigned[i] + 1);
            if (share > best)
            {
                best = share;
                pick = i;
            }
        }

        ++assigned[pick];

        if (member == bot)
            return candidates[pick];
    }

    return nullptr;
}

Unit* GetFreyaRangedLasherFocus(FreyaWaveState const& state)
{
    Unit* best = nullptr;
    for (Unit* lasher : state.detonatingLashers)
    {
        if (!lasher || !lasher->IsAlive())
            continue;

        if (!best || lasher->GetHealth() < best->GetHealth() ||
            (lasher->GetHealth() == best->GetHealth() && lasher->GetGUID() < best->GetGUID()))
            best = lasher;
    }

    return best;
}

Unit* GetFreyaLowLasherOnPlayer(Player* bot, FreyaWaveState const& state, float range, bool backLineOnly)
{
    Unit* best = nullptr;
    for (Unit* lasher : state.detonatingLashers)
    {
        if (!lasher || !lasher->IsAlive() || lasher->GetHealthPct() >= ULDUAR_FREYA_LASHER_BAIL_PCT)
            continue;

        Unit* victim = lasher->GetVictim();
        if (!victim || !victim->IsPlayer() || bot->GetExactDist2d(lasher) > range)
            continue;

        if (backLineOnly)
        {
            Player* const target = victim->ToPlayer();
            if (PlayerbotAI::IsTank(target) || !(PlayerbotAI::IsRangedDps(target) || PlayerbotAI::IsHeal(target)))
                continue;
        }

        if (!best || lasher->GetHealth() < best->GetHealth() ||
            (lasher->GetHealth() == best->GetHealth() && lasher->GetGUID() < best->GetGUID()))
            best = lasher;
    }

    return best;
}

bool IsFreyaLasherTail(FreyaWaveState const& state)
{
    uint64 summed = 0;
    uint32 oneLasher = 0;
    for (Unit* lasher : state.detonatingLashers)
    {
        if (!lasher || !lasher->IsAlive())
            continue;

        summed += lasher->GetHealth();
        oneLasher = std::max(oneLasher, lasher->GetMaxHealth());
    }

    return oneLasher && summed < oneLasher;
}

Player* GetFreyaStackedSnaplasherTank(FreyaWaveState const& state)
{
    Unit* snaplasher = state.snaplasher;
    Unit* victim = snaplasher ? snaplasher->GetVictim() : nullptr;
    Player* tank = victim ? victim->ToPlayer() : nullptr;
    if (!tank || !tank->IsAlive() || !PlayerbotAI::IsTank(tank))
        return nullptr;

    uint32 stacks = 0;
    for (uint32 spellId : {SPELL_HARDENED_BARK_STACK_10, SPELL_HARDENED_BARK_STACK_25})
        if (Aura const* bark = snaplasher->GetAura(spellId))
            stacks = std::max<uint32>(stacks, bark->GetStackAmount());

    return stacks >= ULDUAR_FREYA_ADD_TANK_DEFENSIVE_STACKS ? tank : nullptr;
}

namespace
{
Unit* GetFreyaForHold(PlayerbotAI* botAI)
{
    Unit* freya = GetFreyaScan(botAI).Boss();
    if (!freya)
        freya = GetFreyaBossByEntry(botAI);

    if (!freya || !freya->IsAlive() || !freya->IsInCombat() || !freya->HasAura(SPELL_ATTUNED_TO_NATURE))
        return nullptr;

    return freya;
}

struct FreyaTankExternal
{
    uint32 spellId;
    char const* castName;
};

constexpr FreyaTankExternal FREYA_TANK_EXTERNALS[] = {
    {SPELL_FREYA_PAIN_SUPPRESSION, "pain suppression"},
    {SPELL_FREYA_GUARDIAN_SPIRIT, "guardian spirit"},
    {SPELL_FREYA_HAND_OF_SACRIFICE, "hand of sacrifice"},
};
}  // namespace

bool IsFreyaAddTankDefensiveHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsAssistTankOfIndex(bot, 0, true) ||
        bot->GetHealthPct() < ULDUAR_FREYA_ADD_TANK_DEFENSIVE_LAST_RESORT_PCT)
        return false;

    Unit* freya = GetFreyaForHold(botAI);
    return freya && freya->GetVictim() != bot;
}

bool IsFreyaAddTankExternalHeld(PlayerbotAI* botAI)
{
    Player* addTank = GetGroupAssistTank(botAI->GetBot(), 0);
    if (!addTank)
        return false;

    Unit* freya = GetFreyaForHold(botAI);
    return freya && freya->GetVictim() != addTank;
}

bool HasFreyaTankExternal(Player* tank)
{
    for (FreyaTankExternal const& external : FREYA_TANK_EXTERNALS)
        if (tank->HasAura(external.spellId))
            return true;

    return false;
}

bool IsFreyaAddTankCovered(Player* tank)
{
    return HasFreyaTankExternal(tank) || RunningTankDefensive(tank, true, 0) || ReadyTankDefensive(tank, true, 0);
}

char const* GetFreyaTankExternal(PlayerbotAI* botAI, Player*& tank)
{
    Player* bot = botAI->GetBot();
    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    tank = GetFreyaStackedSnaplasherTank(state);
    if (!tank || tank == bot || IsFreyaAddTankCovered(tank))
        return nullptr;

    for (FreyaTankExternal const& external : FREYA_TANK_EXTERNALS)
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(external.spellId);
        if (info && bot->IsWithinDistInMap(tank, info->GetMaxRange(true, bot)) && bot->IsWithinLOSInMap(tank) &&
            botAI->CanCastSpell(external.spellId, tank))
            return external.castName;
    }

    return nullptr;
}

Unit* GetFreyaLocalLasherTarget(PlayerbotAI* botAI, FreyaWaveState const& state, Unit* currentTarget, float range)
{
    Player* bot = botAI->GetBot();

    Unit* selected = nullptr;
    if (currentTarget && currentTarget->IsAlive() && currentTarget->GetEntry() == NPC_DETONATING_LASHER &&
        bot->GetExactDist2d(currentTarget) <= range)
        selected = currentTarget;

    constexpr float switchMargin = 10.0f;
    for (Unit* candidate : state.detonatingLashers)
    {
        if (!candidate || !candidate->IsAlive() || candidate == selected)
            continue;

        if (bot->GetExactDist2d(candidate) > range)
            continue;

        if (!selected)
        {
            selected = candidate;
            continue;
        }

        if (candidate->GetExactDist2d(bot) + switchMargin < selected->GetExactDist2d(bot))
            selected = candidate;
    }

    return selected;
}

Unit* GetFreyaConservatorSpore(Unit* conservator)
{
    if (!conservator || !conservator->IsAlive())
        return nullptr;

    std::list<Creature*> found;
    conservator->GetCreatureListWithEntryInGrid(found, NPC_HEALTHY_SPORE, ULDUAR_FREYA_SPORE_SEARCH_RADIUS);

    Creature* best = nullptr;
    float bestDist = 0.0f;
    for (Creature* spore : found)
    {
        if (!spore || !spore->IsAlive())
            continue;

        float const dist = spore->GetExactDist2d(conservator);
        if (!best || dist < bestDist)
        {
            best = spore;
            bestDist = dist;
        }
    }

    return best;
}

std::vector<Unit*> GetFreyaSpores(PlayerbotAI* botAI)
{
    std::vector<Unit*> spores;
    for (ObjectGuid const& guid : GetFreyaScan(botAI).Stalkers())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && unit->GetEntry() == NPC_HEALTHY_SPORE)
            spores.push_back(unit);
    }

    return spores;
}

Unit* GetFreyaTargetSpore(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    if (PlayerbotAI::IsMelee(bot))
        if (Unit* parked = GetFreyaConservatorSpore(GetFreyaScanUnitByEntry(botAI, NPC_ANCIENT_CONSERVATOR)))
            return parked;

    std::vector<Position> const beams = GetFreyaSunBeamPositions(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);
    auto const pooled = [&beams](Unit* spore)
    {
        for (Position const& beam : beams)
            if (spore->GetExactDist2d(beam.GetPositionX(), beam.GetPositionY()) < ULDUAR_FREYA_SPORE_RADIUS)
                return true;

        return false;
    };

    Unit* nearest = nullptr;
    Unit* clean = nullptr;
    Unit* roomy = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    float cleanDistance = std::numeric_limits<float>::max();
    float roomyDistance = std::numeric_limits<float>::max();
    for (Unit* spore : GetFreyaSpores(botAI))
    {
        float const distance = bot->GetDistance2d(spore);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = spore;
        }

        if (pooled(spore))
            continue;

        if (distance < cleanDistance)
        {
            cleanDistance = distance;
            clean = spore;
        }

        if (distance < roomyDistance &&
            CountFreyaRaidNear(botAI, spore->GetPosition(), ULDUAR_FREYA_SPORE_RADIUS, bot) <
                ULDUAR_FREYA_SPORE_CROWD)
        {
            roomyDistance = distance;
            roomy = spore;
        }
    }

    if (roomy)
        return roomy;

    return clean ? clean : nearest;
}

uint32 CountFreyaRaidNear(PlayerbotAI* botAI, Position const& centre, float radius, Player* except)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return 0;

    uint32 count = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == except || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        if (member->GetExactDist2d(centre.GetPositionX(), centre.GetPositionY()) <= radius)
            ++count;
    }

    return count;
}

Unit* GetFreyaNaturesFuryShelter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    Unit* best = nullptr;
    float bestDistance = std::numeric_limits<float>::max();
    for (Unit* spore : GetFreyaSpores(botAI))
    {
        if (CountFreyaRaidNear(botAI, spore->GetPosition(), ULDUAR_FREYA_NATURES_FURY_RADIUS, bot))
            continue;

        float const distance = bot->GetDistance2d(spore);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = spore;
        }
    }

    return best;
}

static Unit* GetFreyaTankTrioTarget(FreyaWaveState const& state, Unit* currentTarget)
{
    Unit* best = nullptr;
    for (Unit* member : state.LivingTrio())
    {
        if (FreyaTrioSyncSuppress(state, member))
            continue;

        if (!best || member->GetHealthPct() > best->GetHealthPct())
            best = member;
    }

    if (!best || !currentTarget || currentTarget == best)
        return best;

    for (Unit* member : state.LivingTrio())
    {
        if (member != currentTarget || FreyaTrioSyncSuppress(state, member))
            continue;

        if (best->GetHealthPct() - member->GetHealthPct() < ULDUAR_FREYA_TANK_TRIO_SWITCH_PCT)
            return member;

        break;
    }

    return best;
}

Unit* GetFreyaTankTarget(PlayerbotAI* botAI, FreyaWaveState const& state, Unit* currentTarget)
{
    Player* bot = botAI->GetBot();
    Unit* freya = GetFreyaScanUnitByEntry(botAI, NPC_FREYA);

    if (PlayerbotAI::IsMainTank(bot))
    {
        Unit* storm = state.stormLasher;
        if (storm && storm->IsAlive() && !FreyaTrioSyncSuppress(state, storm) && bot->IsWithinMeleeRange(storm))
            return storm;

        return freya;
    }

    if (!PlayerbotAI::IsAssistTankOfIndex(bot, 0, true))
        return nullptr;

    if (state.snaplasher && state.snaplasher->IsAlive())
        return state.snaplasher;

    if (state.conservator && state.conservator->IsAlive())
        return state.conservator;

    if (Unit* member = GetFreyaTankTrioTarget(state, currentTarget))
        return member;

    if (Unit* lasher = GetFreyaLocalLasherTarget(botAI, state, currentTarget, ULDUAR_FREYA_MELEE_LASHER_RANGE))
        return lasher;

    return freya;
}

bool IsFreyaGiftFiller(PlayerbotAI* botAI, Unit* gift)
{
    Player* bot = botAI->GetBot();
    if (!gift || !PlayerbotAI::IsDps(bot) || PlayerbotAI::IsRangedDps(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    float const ownDistance = bot->GetExactDist2d(gift);
    uint32 ranged = 0;
    uint32 nearer = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        if (PlayerbotAI::IsRangedDps(member))
        {
            ++ranged;
            continue;
        }

        if (!GET_PLAYERBOT_AI(member) || !PlayerbotAI::IsDps(member))
            continue;

        float const distance = member->GetExactDist2d(gift);
        if (distance < ownDistance || (distance == ownDistance && member->GetGUID() < bot->GetGUID()))
            ++nearer;
    }

    return ranged < ULDUAR_FREYA_GIFT_SHARE && nearer < ULDUAR_FREYA_GIFT_SHARE - ranged;
}

std::vector<Position> GetFreyaNatureBombPositions(Player* bot, float searchRadius)
{
    std::list<GameObject*> bombs;
    bot->GetGameObjectListWithEntryInGrid(bombs, GOBJECT_NATURE_BOMB, searchRadius);

    std::vector<Position> positions;
    positions.reserve(bombs.size());
    for (GameObject* bomb : bombs)
    {
        if (bomb)
            positions.push_back(bomb->GetPosition());
    }

    return positions;
}

bool FreyaWalkCrossesNatureBomb(Position const& from, Position const& to, std::vector<Position> const& bombs,
                                float clearance)
{
    for (Position const& bomb : bombs)
        if (bomb.GetExactDist2d(from.GetPositionX(), from.GetPositionY()) >= clearance &&
            SegmentCrossesCircle(from, to, HazardCircle(bomb, clearance)))
            return true;

    return false;
}

std::vector<Position> GetFreyaSunBeamPositions(PlayerbotAI* botAI, float searchRadius)
{
    Player* bot = botAI->GetBot();

    std::vector<Position> beams;
    for (ObjectGuid const& guid : GetFreyaScan(botAI).Stalkers())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_FREYA_SUN_BEAM && unit->GetEntry() != NPC_FREYA_UNSTABLE_SUN_BEAM)
            continue;

        if (bot->GetExactDist2d(unit) < searchRadius)
            beams.push_back(unit->GetPosition());
    }

    return beams;
}

std::vector<EncounterHelpers::HazardCircle> BuildFreyaEscapeHazards(std::vector<Position> const& bombs,
                                                                    std::vector<Position> const& beams,
                                                                    std::vector<Position> const& lashers)
{
    std::vector<EncounterHelpers::HazardCircle> hazards;
    hazards.reserve(bombs.size() + beams.size() + lashers.size());
    for (Position const& bomb : bombs)
        hazards.emplace_back(bomb, ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS);

    for (Position const& beam : beams)
        hazards.emplace_back(beam, ULDUAR_FREYA_SUN_BEAM_CLEARANCE);

    for (Position const& lasher : lashers)
        hazards.emplace_back(lasher, ULDUAR_FREYA_DETONATE_RADIUS);

    return hazards;
}

std::vector<Position> GetFreyaEscapeLasherPositions(PlayerbotAI* botAI, float searchRadius)
{
    Player* bot = botAI->GetBot();

    if (PlayerbotAI::IsTank(bot) || !(PlayerbotAI::IsRangedDps(bot) || PlayerbotAI::IsHeal(bot)))
        return {};

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    return GetFreyaLowLasherPositions(botAI, state, ULDUAR_FREYA_LASHER_BAIL_PCT, searchRadius);
}

std::vector<EncounterHelpers::HazardCircle> GetFreyaEscapeHazards(PlayerbotAI* botAI, float searchRadius)
{
    return BuildFreyaEscapeHazards(GetFreyaNatureBombPositions(botAI->GetBot(), searchRadius),
                                   GetFreyaSunBeamPositions(botAI, searchRadius),
                                   GetFreyaEscapeLasherPositions(botAI, searchRadius));
}

Player* GetFreyaRangedCampAnchor(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return PlayerbotAI::IsRangedDps(bot) ? bot : nullptr;

    Player* anchor = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        if (!GET_PLAYERBOT_AI(member) || !PlayerbotAI::IsRangedDps(member))
            continue;

        if (!anchor || member->GetGUID() < anchor->GetGUID())
            anchor = member;
    }

    return anchor;
}

uint32 GetFreyaRangedDpsRank(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsRangedDps(bot))
        return std::numeric_limits<uint32>::max();

    Group* group = bot->GetGroup();
    if (!group)
        return 0;

    uint32 rank = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        if (!GET_PLAYERBOT_AI(member) || !PlayerbotAI::IsRangedDps(member))
            continue;

        if (member->GetGUID() < bot->GetGUID())
            ++rank;
    }

    return rank;
}

namespace
{
struct FreyaArmyState
{
    uint32 ghoulMark = 0;
};

RaidInstanceState<FreyaArmyState> freyaArmyStates;
}

bool IsFreyaArmyChanneling(Player* player)
{
    Spell const* channel = player->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    return channel && channel->GetSpellInfo()->Id == SPELL_ARMY_OF_THE_DEAD;
}

bool IsFreyaArmyTurn(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return true;

    // No early return: every DK's ghouls go into the mark, this bot's own included.
    bool turn = true;
    uint32 ghoulMark = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId() ||
            member->getClass() != CLASS_DEATH_KNIGHT)
            continue;

        for (Unit* minion : member->m_Controlled)
        {
            if (minion && minion->IsAlive() && minion->GetEntry() == NPC_ARMY_OF_THE_DEAD)
            {
                turn = false;
                ghoulMark = std::max(ghoulMark, minion->GetGUID().GetCounter());
            }
        }

        if (member == bot)
            continue;

        if (IsFreyaArmyChanneling(member))
            turn = false;

        if (GET_PLAYERBOT_AI(member) && member->IsInCombat() && member->GetGUID() < bot->GetGUID() &&
            member->HasSpell(SPELL_ARMY_OF_THE_DEAD) && !member->HasSpellCooldown(SPELL_ARMY_OF_THE_DEAD))
            turn = false;
    }

    if (ghoulMark && bot->GetInstanceId())
    {
        FreyaArmyState& army = freyaArmyStates.For(bot->GetInstanceId());
        army.ghoulMark = std::max(army.ghoulMark, ghoulMark);
    }

    return turn;
}

bool HasUncoveredFreyaLasher(PlayerbotAI* botAI, FreyaWaveState const& state)
{
    uint32 const instanceId = botAI->GetBot()->GetInstanceId();
    FreyaArmyState const* army = instanceId ? freyaArmyStates.Find(instanceId) : nullptr;
    uint32 const ghoulMark = army ? army->ghoulMark : 0;

    for (Unit* lasher : state.detonatingLashers)
    {
        if (lasher && lasher->IsAlive() && lasher->GetGUID().GetCounter() > ghoulMark)
            return true;
    }

    return false;
}

Unit* GetFreyaSnaplasherToKite(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (PlayerbotAI::IsTank(bot))
        return nullptr;

    Unit* chaser = nullptr;
    for (ObjectGuid const& guid : GetFreyaScan(botAI).PossibleTargets())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && unit->GetEntry() == NPC_SNAPLASHER && unit->GetVictim() == bot)
        {
            chaser = unit;
            break;
        }
    }

    float const chaserSpeed = chaser ? chaser->GetSpeed(MOVE_RUN) : 0.0f;
    float const ownSpeed = bot->GetSpeed(MOVE_RUN);

    if (RaidObs::Active())
    {
        char speeds[32] = "";
        if (chaser)
            std::snprintf(speeds, sizeof(speeds), "%.2f vs %.2f", chaserSpeed, ownSpeed);

        RaidObs::NoteDerived(bot, "freya.kite", speeds);
    }

    return chaser && chaserSpeed < ownSpeed ? chaser : nullptr;
}

float GetFreyaSnaplasherKiteReach(Unit* snaplasher, Player* bot, float margin)
{
    return snaplasher->GetMeleeRange(bot) + margin;
}

Player* GetFreyaLooseLasherVictim(Unit* lasher)
{
    if (!lasher || !lasher->IsAlive() || lasher->GetEntry() != NPC_DETONATING_LASHER)
        return nullptr;

    Unit* victim = lasher->GetVictim();
    Player* player = victim ? victim->ToPlayer() : nullptr;
    if (!player || PlayerbotAI::IsTank(player))
        return nullptr;

    return player;
}

bool IsFreyaRooted(Unit* unit)
{
    return unit->HasAura(SPELL_IRON_ROOTS_DAMAGE_10) || unit->HasAura(SPELL_IRON_ROOTS_DAMAGE_25) ||
           unit->HasAura(SPELL_IRON_ROOTS_FREYA_DAMAGE_10) || unit->HasAura(SPELL_IRON_ROOTS_FREYA_DAMAGE_25);
}

uint32 GetFreyaHazardsUnder(PlayerbotAI* botAI, Unit* unit, Position* centre)
{
    uint32 count = 0;
    float sumX = 0.0f;
    float sumY = 0.0f;
    auto const add = [&](WorldObject const* hazard)
    {
        ++count;
        sumX += hazard->GetPositionX();
        sumY += hazard->GetPositionY();
    };

    std::list<GameObject*> bombs;
    unit->GetGameObjectListWithEntryInGrid(bombs, GOBJECT_NATURE_BOMB, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS);
    for (GameObject* bomb : bombs)
        if (bomb)
            add(bomb);

    for (ObjectGuid const& guid : GetFreyaScan(botAI).Stalkers())
    {
        Unit* beam = botAI->GetUnit(guid);
        if (beam && beam->IsAlive() &&
            (beam->GetEntry() == NPC_FREYA_SUN_BEAM || beam->GetEntry() == NPC_FREYA_UNSTABLE_SUN_BEAM) &&
            unit->GetExactDist2d(beam) < ULDUAR_FREYA_ROOTED_BEAM_RADIUS)
            add(beam);
    }

    if (count && centre)
        *centre = Position(sumX / count, sumY / count, unit->GetPositionZ(), 0.0f);

    return count;
}

Player* GetFreyaRootedAllyToFree(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Player* best = nullptr;
    bool bestInHazard = false;
    float bestDist = 0.0f;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId() || !IsFreyaRooted(member))
            continue;

        float const dist = bot->GetExactDist2d(member);
        if (dist > ULDUAR_FREYA_HAND_OF_FREEDOM_RANGE || !bot->IsWithinLOSInMap(member))
            continue;

        bool const inHazard = GetFreyaHazardsUnder(botAI, member) > 0;
        if (!best || (inHazard && !bestInHazard) || (inHazard == bestInHazard && dist < bestDist))
        {
            best = member;
            bestInHazard = inHazard;
            bestDist = dist;
        }
    }

    return best;
}

uint32 CountFreyaLashersNear(Position const& centre, FreyaWaveState const& state, float radius)
{
    uint32 count = 0;
    for (Unit* lasher : state.detonatingLashers)
    {
        if (lasher && lasher->IsAlive() && centre.GetExactDist2d(lasher->GetPosition()) <= radius)
            ++count;
    }

    return count;
}

std::vector<Position> GetFreyaLowLasherPositions(PlayerbotAI* botAI, FreyaWaveState const& state, float maxPct,
                                                 float radius)
{
    Player* bot = botAI->GetBot();
    std::vector<Position> low;

    for (Unit* lasher : state.detonatingLashers)
    {
        if (!lasher || !lasher->IsAlive() || lasher->GetHealthPct() >= maxPct)
            continue;

        if (bot->GetExactDist2d(lasher) <= radius)
            low.push_back(lasher->GetPosition());
    }

    return low;
}

Position GetFreyaLasherCampSpot(PlayerbotAI* botAI)
{
    return GetFreyaScan(botAI).LasherCamp();
}

bool FreyaShedsSnaplasherThreat(PlayerbotAI* botAI, Unit* snaplasher, uint32 seenForMs, bool shedding)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    if (seenForMs < ULDUAR_FREYA_SNAPLASHER_TANK_LEAD_MS)
        return true;

    Player* addTank = GetGroupAssistTank(bot, 0);
    float const tankThreat = addTank ? snaplasher->GetThreatMgr().GetThreat(addTank) : 0.0f;
    if (tankThreat <= 0.0f)
        return false;

    float const pct = snaplasher->GetThreatMgr().GetThreat(bot) * 100.0f / tankThreat;

    return pct >=
           (shedding ? ULDUAR_FREYA_SNAPLASHER_THREAT_RETURN_PCT : ULDUAR_FREYA_SNAPLASHER_THREAT_SHED_PCT);
}

Unit* GetFreyaLasherPackFocus(FreyaWaveState const& state)
{
    Unit* best = nullptr;
    uint32 bestCount = 0;

    for (Unit* lasher : state.detonatingLashers)
    {
        if (!lasher || !lasher->IsAlive())
            continue;

        uint32 const count = CountFreyaLashersNear(lasher->GetPosition(), state, ULDUAR_FREYA_LASHER_PACK_RADIUS);

        if (!best || count > bestCount || (count == bestCount && lasher->GetGUID() < best->GetGUID()))
        {
            best = lasher;
            bestCount = count;
        }
    }

    return best;
}

bool IsFreyaLasherPackFinishing(FreyaWaveState const& state, Position const& centre)
{
    uint32 count = 0;
    for (Unit* lasher : state.detonatingLashers)
    {
        if (!lasher || !lasher->IsAlive())
            continue;

        if (centre.GetExactDist2d(lasher->GetPosition()) > ULDUAR_FREYA_LASHER_PACK_RADIUS)
            continue;

        if (lasher->GetHealthPct() > ULDUAR_FREYA_LASHER_FINISH_PCT)
            return false;

        ++count;
    }

    return count >= ULDUAR_FREYA_LASHER_PACK_MIN_COUNT;
}

Unit* GetFreyaFinishingPackNear(PlayerbotAI* botAI, FreyaWaveState const& state, float radius)
{
    Unit* focus = GetFreyaLasherPackFocus(state);
    if (!focus || botAI->GetBot()->GetExactDist2d(focus) > radius)
        return nullptr;

    return IsFreyaLasherPackFinishing(state, focus->GetPosition()) ? focus : nullptr;
}

Unit* GetFreyaRangedLasherAim(FreyaWaveState const& state)
{
    Unit* focus = GetFreyaLasherPackFocus(state);
    if (focus && IsFreyaLasherPackFinishing(state, focus->GetPosition()))
        return GetFreyaRangedLasherFocus(state);

    return focus;
}

bool IsFreyaLasherHeldByGhoul(PlayerbotAI* botAI, Unit* lasher)
{
    if (!lasher || !lasher->IsAlive())
        return false;

    Unit* victim = lasher->GetVictim();
    if (victim && victim->GetEntry() == NPC_ARMY_OF_THE_DEAD)
        return true;

    Group* group = botAI->GetBot()->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member->getClass() != CLASS_DEATH_KNIGHT || member->GetMapId() != lasher->GetMapId())
            continue;

        for (Unit* minion : member->m_Controlled)
        {
            if (minion && minion->IsAlive() && minion->GetEntry() == NPC_ARMY_OF_THE_DEAD &&
                minion->GetVictim() == lasher)
                return true;
        }
    }

    return false;
}

Unit* FreyaScan::Boss()
{
    if (!FreshThisTick(bossAtMs))
    {
        Unit* found = botAI->GetAiObjectContext()->GetValue<Unit*>("find target", "freya")->Get();
        boss = found ? found->GetGUID() : ObjectGuid::Empty;
    }

    return boss.IsEmpty() ? nullptr : botAI->GetUnit(boss);
}

GuidVector const& FreyaScan::PossibleTargets()
{
    if (!FreshThisTick(targetsAtMs))
        targets = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();

    return targets;
}

GuidVector const& FreyaScan::Stalkers()
{
    if (FreshThisTick(stalkersAtMs))
        return stalkers;

    stalkers.clear();

    Player* bot = botAI->GetBot();
    if (!bot)
        return stalkers;

    float const range = sPlayerbotAIConfig.SightDistance;

    std::vector<Unit*> matches;
    FreyaStalkerInRangeCheck check{Acore::AnyUnitInObjectRangeCheck(bot, range)};
    Acore::UnitListSearcher<FreyaStalkerInRangeCheck> searcher(bot, matches, check);
    Cell::VisitObjects(bot, searcher, range);

    for (Unit* unit : matches)
        if (!unit->IsPlayer() && bot->IsWithinLOSInMap(unit))
            stalkers.push_back(unit->GetGUID());

    return stalkers;
}

Position const& FreyaScan::LasherCamp()
{
    if (!FreshThisTick(campAtMs))
    {
        Player* anchor = GetFreyaRangedCampAnchor(botAI);
        camp = anchor ? anchor->GetPosition() : Position();
    }

    return camp;
}

FreyaScan& GetFreyaScan(PlayerbotAI* botAI)
{
    return *botAI->GetAiObjectContext()->GetValue<FreyaScan*>("freya scan")->Get();
}

Unit* GetFreyaScanUnitByEntry(PlayerbotAI* botAI, uint32 entry)
{
    for (ObjectGuid const& guid : GetFreyaScan(botAI).PossibleTargets())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && unit->GetEntry() == entry)
            return unit;
    }

    return nullptr;
}

Unit* GetFreyaBossByEntry(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return nullptr;

    float const range = sPlayerbotAIConfig.SightDistance;

    std::vector<Unit*> matches;
    FreyaUnfriendlyOfEntryCheck check{Acore::AnyUnfriendlyUnitInObjectRangeCheck(bot, bot, range), NPC_FREYA};
    Acore::UnitListSearcher<FreyaUnfriendlyOfEntryCheck> searcher(bot, matches, check);
    Cell::VisitObjects(bot, searcher, range);

    for (Unit* unit : matches)
        if (unit->IsAlive() && AttackersValue::IsPossibleTarget(unit, bot, range))
            return unit;

    return nullptr;
}

bool IsFreyaGroundTremorCasting(Unit* boss)
{
    if (!boss || !boss->HasUnitState(UNIT_STATE_CASTING))
        return false;

    Spell* spell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!spell)
        return false;

    return spell->m_spellInfo->Id == SPELL_FREYA_GROUND_TREMOR_10 ||
           spell->m_spellInfo->Id == SPELL_FREYA_GROUND_TREMOR_25;
}

Unit* GetFreyaSunbeamTarget(Unit* boss)
{
    if (!boss || !boss->HasUnitState(UNIT_STATE_CASTING))
        return nullptr;

    Spell* spell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!spell)
        return nullptr;

    if (spell->m_spellInfo->Id != SPELL_FREYA_SUNBEAM_10 && spell->m_spellInfo->Id != SPELL_FREYA_SUNBEAM_25)
        return nullptr;

    return spell->m_targets.GetUnitTarget();
}
