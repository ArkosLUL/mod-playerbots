/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_FrostGiant.h"

#include <algorithm>

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

    std::vector<Player*> carriers;
    for (Player* member : members)
        if (Carries(member) && !state.partners.count(member->GetGUID()))
            carriers.push_back(member);

    std::sort(carriers.begin(), carriers.end(), [](Player* a, Player* b) { return a->GetGUID() < b->GetGUID(); });

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

        Player* best = nullptr;
        for (Player* candidate : members)
        {
            if (candidate == carrier || Carries(candidate) || IsTankPlayer(candidate) ||
                (IsTankPlayer(carrier) && !IsDutyBot(candidate)))
                continue;

            bool taken = false;
            for (auto const& pair : state.partners)
                taken = taken || pair.second == candidate->GetGUID();

            if (!taken && (!best || better(candidate, best)))
                best = candidate;
        }

        if (best)
            state.partners[carrier->GetGUID()] = best->GetGUID();
    }
}

Player* RfgPairWalk(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!RfgGiant(botAI))
        return nullptr;

    RfgState* state = sRfgState.Find(bot->GetInstanceId());
    if (!state)
        return nullptr;

    ObjectGuid other;
    if (Carries(bot))
    {
        auto const pair = state->partners.find(bot->GetGUID());
        if (pair == state->partners.end() || IsTankPlayer(bot))
            return nullptr;

        other = pair->second;
    }
    else
    {
        for (auto const& [carrier, partner] : state->partners)
            if (partner == bot->GetGUID())
                other = carrier;
    }

    Player* mate = other.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, other);
    if (!mate || !mate->IsAlive() || !mate->IsInMap(bot))
        return nullptr;

    // Only one of the pair walks: the carrier, unless it tanks or is a player.
    return Carries(bot) || IsTankPlayer(mate) || !IsDutyBot(mate) ? mate : nullptr;
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

        if (state)
        {
            auto const pair = state->partners.find(member->GetGUID());
            if (pair != state->partners.end() && pair->second == bot->GetGUID())
                continue;
        }

        hazards.push_back({member->GetPosition(), RFG_PLAGUE_CLEAR});
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
