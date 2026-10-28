/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_FrostGiant.h"

#include <algorithm>
#include <unordered_map>

#include "Group.h"
#include "ICCHelpers_Shared.h"
#include "Playerbots.h"
#include "RaidEncounter.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "Timer.h"

namespace
{
constexpr uint32 RFG_UPDATE_MS = 250;

struct RfgState
{
    RaidObs::ObsGuidMap<ObjectGuid> partners{"rfg.partner"};
    // Carriers whose pair walks out to a spot, and where it stands.
    RaidObs::ObsGuidSet apart{"rfg.apart"};
    std::unordered_map<ObjectGuid, Position> spots;
    std::unordered_map<ObjectGuid, uint32> spotSince;
    uint32 updatedMs = 0;
};

RaidInstanceState<RfgState> sRfgState;

bool IsTankPlayer(Player* player) { return PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true); }

// A bot of its own: players and selfbots hold no duty.
bool IsDutyBot(Player* player) { return GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

bool Carries(Unit* unit) { return unit->HasAura(SPELL_DEATH_PLAGUE); }

bool Cured(Unit* unit) { return unit->HasAura(SPELL_RFG_RECENTLY_INFECTED); }

std::vector<Player*> MembersOnMap(Player* bot)
{
    std::vector<Player*> members;
    Group* group = bot->GetGroup();
    if (!group)
    {
        members.push_back(bot);
        return members;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && member->IsInMap(bot))
            members.push_back(member);
    }

    return members;
}

Player* Find(std::vector<Player*> const& members, ObjectGuid guid)
{
    auto const found = std::find_if(members.begin(), members.end(), [guid](Player* p) { return p->GetGUID() == guid; });
    return found == members.end() ? nullptr : *found;
}

bool IsEmpty(Position const& pos) { return !pos.GetPositionX() && !pos.GetPositionY(); }

// A spot at least RFG_PLAGUE_CLEAR from every other player and from the other pairs' spots, in sight of
// the giant and in a caster's range of it, so casters keep casting. A partner already standing on such
// ground, the cured one of the last tick, is the spot. Empty when nothing within the sweep is clear.
Position PairSpot(Player* carrier, Player* partner, std::vector<Player*> const& members, Creature* giant,
                  RfgState const& state)
{
    std::vector<EncounterHelpers::HazardCircle> circles;
    for (Player* member : members)
        if (member != carrier && member != partner)
            circles.push_back({member->GetPosition(), RFG_PLAGUE_CLEAR});

    for (auto const& [guid, spot] : state.spots)
        if (guid != carrier->GetGUID())
            circles.push_back({spot, RFG_PLAGUE_CLEAR});

    auto seesGiant = [giant, carrier](float x, float y)
    { return giant->GetExactDist2d(x, y) <= RFG_SPOT_GIANT_MAX && giant->IsWithinLOS(x, y, carrier->GetPositionZ()); };

    Position const here = partner->GetPosition();
    auto insideCircle = [&here](EncounterHelpers::HazardCircle const& circle)
    { return circle.first.GetExactDist2d(here) < circle.second; };
    bool const clear = std::none_of(circles.begin(), circles.end(), insideCircle);
    if (clear && seesGiant(here.GetPositionX(), here.GetPositionY()))
        return here;

    Position const prefer = giant->GetPosition();
    return EncounterHelpers::FindNearestPositionClearOfHazards(carrier, circles, RFG_SPOT_SWEEP, 2.0f,
                                                               static_cast<float>(M_PI) / 8.0f, &prefer, seesGiant);
}

struct CarrierMemo
{
    ObjectGuid bot;
    uint32 ms = 0;
    std::vector<EncounterHelpers::HazardCircle> hazards;
};

std::vector<EncounterHelpers::HazardCircle> BuildCarrierHazards(PlayerbotAI* botAI);
}  // namespace

Creature* RfgGiant(PlayerbotAI* botAI)
{
    return UnitGateLiveUnit<NPC_ROTTING_FROST_GIANT_10, NPC_ROTTING_FROST_GIANT_25>(botAI);
}

void RfgUpdatePartners(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    RfgState& state = sRfgState.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (state.updatedMs && getMSTimeDiff(state.updatedMs, now) < RFG_UPDATE_MS)
        return;
    state.updatedMs = now;

    std::vector<Player*> const members = MembersOnMap(bot);

    std::vector<ObjectGuid> released;
    for (auto const& [carrierGuid, partnerGuid] : state.partners)
    {
        Player* carrier = Find(members, carrierGuid);
        Player* partner = Find(members, partnerGuid);
        if (!carrier || !Carries(carrier) || !partner || Carries(partner))
            released.push_back(carrierGuid);
    }

    for (ObjectGuid const& carrier : released)
        state.partners.erase(carrier);

    // A spot lives with its pair. One the pair hasn't reached in time is dropped, and the pair forms where
    // it stands.
    std::vector<ObjectGuid> dropped;
    for (auto const& [carrierGuid, spot] : state.spots)
    {
        auto const pair = state.partners.find(carrierGuid);
        Player* carrier = Find(members, carrierGuid);
        Player* partner = pair == state.partners.end() ? nullptr : Find(members, pair->second);
        bool const late = getMSTimeDiff(state.spotSince[carrierGuid], now) > RFG_SPOT_BUDGET_MS;
        bool const placed = carrier && partner && carrier->GetExactDist2d(spot) <= RFG_SPOT_ARRIVE + 1.0f &&
                            partner->GetExactDist2d(spot) <= RFG_SPOT_ARRIVE + 1.0f;
        if (!partner || (late && !placed))
            dropped.push_back(carrierGuid);
    }

    for (ObjectGuid const& carrier : dropped)
    {
        state.spots.erase(carrier);
        state.spotSince.erase(carrier);
        state.apart.erase(carrier);
    }

    std::vector<Player*> carriers;
    for (Player* member : members)
        if (Carries(member) && !state.partners.count(member->GetGUID()))
            carriers.push_back(member);

    std::sort(carriers.begin(), carriers.end(), [](Player* a, Player* b) { return a->GetGUID() < b->GetGUID(); });

    Creature* giant = RfgGiant(botAI);
    for (Player* carrier : carriers)
    {
        // A cured partner ends the chain, a clean one carries it once more back onto the cured carrier.
        auto better = [carrier](Player* a, Player* b)
        {
            if (Cured(a) != Cured(b))
                return Cured(a);

            float const distA = carrier->GetExactDist2d(a);
            float const distB = carrier->GetExactDist2d(b);
            return distA != distB ? distA < distB : a->GetGUID() < b->GetGUID();
        };

        // A tank only when no one else is left: it holds the giant where it is, so the carrier walks to it.
        Player* best = nullptr;
        for (bool const tanks : {false, true})
        {
            for (Player* candidate : members)
            {
                if (candidate == carrier || Carries(candidate) || IsTankPlayer(candidate) != tanks ||
                    (IsTankPlayer(carrier) && !IsDutyBot(candidate)))
                    continue;

                bool taken = false;
                for (auto const& pair : state.partners)
                    taken = taken || pair.second == candidate->GetGUID();

                if (!taken && (!best || better(candidate, best)))
                    best = candidate;
            }

            if (best)
                break;
        }

        if (!best)
            continue;

        state.partners[carrier->GetGUID()] = best->GetGUID();
        // Both walk to a spot, so both must be bots; a tank stays at the giant and a player holds no duty.
        if (!giant || IsTankPlayer(carrier) || IsTankPlayer(best) || !IsDutyBot(carrier) || !IsDutyBot(best))
            continue;

        Position const spot = PairSpot(carrier, best, members, giant, state);
        if (!IsEmpty(spot))
        {
            state.spots[carrier->GetGUID()] = spot;
            state.spotSince[carrier->GetGUID()] = now;
            state.apart.insert(carrier->GetGUID());
        }
    }
}

RfgWalk RfgPairWalk(PlayerbotAI* botAI)
{
    RfgWalk walk;
    Player* bot = botAI->GetBot();
    if (!RfgGiant(botAI))
        return walk;

    RfgState* state = sRfgState.Find(bot->GetInstanceId());
    if (!state)
        return walk;

    ObjectGuid carrierGuid;
    ObjectGuid other;
    if (Carries(bot))
    {
        auto const pair = state->partners.find(bot->GetGUID());
        if (pair == state->partners.end() || IsTankPlayer(bot))
            return walk;

        carrierGuid = bot->GetGUID();
        other = pair->second;
    }
    else
    {
        for (auto const& [carrier, partner] : state->partners)
        {
            if (partner == bot->GetGUID())
            {
                carrierGuid = carrier;
                other = carrier;
            }
        }
    }

    Player* mate = other.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, other);
    if (!mate || !mate->IsAlive() || !mate->IsInMap(bot))
        return walk;

    // Both of a pair with a spot walk to it.
    auto const spot = state->spots.find(carrierGuid);
    if (spot != state->spots.end())
    {
        walk.spot = spot->second;
        walk.toSpot = true;
        return walk;
    }

    // Otherwise only one of the pair walks: the carrier, unless it tanks or is a player.
    if (Carries(bot) || IsTankPlayer(mate) || !IsDutyBot(mate))
        walk.mate = mate;

    return walk;
}

std::vector<EncounterHelpers::HazardCircle> RfgCarrierHazards(PlayerbotAI* botAI)
{
    // Per bot and ms: the movement rules ask for every movement action the bot weighs.
    thread_local CarrierMemo memo;
    Player* bot = botAI->GetBot();
    uint32 const now = getMSTime();
    if (memo.bot != bot->GetGUID() || memo.ms != now)
    {
        memo.bot = bot->GetGUID();
        memo.ms = now;
        memo.hazards = BuildCarrierHazards(botAI);
    }

    return memo.hazards;
}

namespace
{
std::vector<EncounterHelpers::HazardCircle> BuildCarrierHazards(PlayerbotAI* botAI)
{
    std::vector<EncounterHelpers::HazardCircle> hazards;
    Player* bot = botAI->GetBot();
    // Recently Infected can't catch it, and a tank keeps the giant where it is.
    if (!RfgGiant(botAI) || Carries(bot) || Cured(bot) || IsTankPlayer(bot))
        return hazards;

    RfgState* state = sRfgState.Find(bot->GetInstanceId());
    for (Player* member : MembersOnMap(bot))
    {
        if (member == bot || !Carries(member))
            continue;

        Position at = member->GetPosition();
        if (state)
        {
            auto const pair = state->partners.find(member->GetGUID());
            if (pair != state->partners.end() && pair->second == bot->GetGUID())
                continue;

            // Where it ticks, once a pair walking out to a spot gets there.
            auto const spot = state->spots.find(member->GetGUID());
            if (spot != state->spots.end())
                at = spot->second;
        }

        hazards.push_back({at, RFG_PLAGUE_CLEAR});
    }

    return hazards;
}
}  // namespace

bool RfgPlagueDuty(PlayerbotAI* botAI)
{
    if (RfgPairWalk(botAI))
        return true;

    Player* bot = botAI->GetBot();
    for (EncounterHelpers::HazardCircle const& carrier : RfgCarrierHazards(botAI))
        if (bot->GetExactDist2d(carrier.first) < RFG_PLAGUE_REACH)
            return true;

    return false;
}

bool RfgNearCarrier(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    for (EncounterHelpers::HazardCircle const& carrier : RfgCarrierHazards(botAI))
        if (bot->GetExactDist2d(carrier.first) < carrier.second)
            return true;

    return false;
}

void RfgTick(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot->GetInstanceId())
        return;

    Creature* giant = RfgGiant(botAI);
    if (!giant)
    {
        if (sRfgState.Find(bot->GetInstanceId()))
            sRfgState.Reset(bot->GetInstanceId());
        return;
    }

    IccMarkPull(bot, giant, "rotting-frost-giant");
    RfgUpdatePartners(botAI);
}
