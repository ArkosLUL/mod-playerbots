/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Svalna.h"

#include <list>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "GroupReference.h"
#include "ICCData.h"
#include "ICCHelpers_Shared.h"
#include "InstanceScript.h"
#include "Item.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidDirectTargeting.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "Timer.h"

using namespace EncounterHelpers;

namespace
{
constexpr float LIVE_RANGE = 100.0f;
constexpr float SCAN_RANGE = 100.0f;
constexpr uint32 SCAN_MS = 200;
constexpr float SHIELD_NOTE_RANGE = 30.0f;
// Hurl Spear reaches 25 yd
constexpr float HURL_RANGE = 24.0f;
constexpr float TAUNT_RANGE = 30.0f;

// Kill order among the raised captains: Arnath heals the others and, on 25-man, casts Dominate Mind
constexpr uint8 CAPTAIN_ORDER[SS_CAPTAINS] = {0, 3, 2, 1};

// Asked by a trigger, its action and a rule for every action a bot weighs, and a bot never ticks twice in
// one ms
struct TargetMemo
{
    uint32 ms = 0;
    bool set = false;
    Unit* pick = nullptr;
};

struct SsState
{
    uint32 scanMs = 0;
    bool scanned = false;
    std::vector<ObjectGuid> spears;
    // Bot -> the Impaling Spear it pulls out
    RaidObs::ObsGuidMap<ObjectGuid> spear{"ss.spear"};
    // Bot tank -> the raised captain it holds
    RaidObs::ObsGuidMap<ObjectGuid> tank{"ss.tank"};
    std::unordered_map<ObjectGuid, TargetMemo> targets;
};

RaidInstanceState<SsState> ssStates;

bool IsTankPlayer(Player* player) { return PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true); }

// Players hold no duty, and a selfbot counts as a player here.
bool IsDutyBot(Player* player) { return GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

void NoteDerived(Player* bot, char const* key, std::string const& value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
}

InstanceScript* IccInstance(Player* bot)
{
    return bot && bot->IsInWorld() && bot->GetMapId() == ICC_MAP_ID ? bot->GetInstanceScript() : nullptr;
}

Creature* SlotCreature(Player* bot, uint32 data)
{
    InstanceScript* instance = IccInstance(bot);
    return instance ? ObjectAccessor::GetCreature(*bot, instance->GetGuidData(data)) : nullptr;
}

bool IsRaisedCaptain(Unit* unit)
{
    switch (unit ? unit->GetEntry() : 0)
    {
        case NPC_SS_ARNATH_UNDEAD:
        case NPC_SS_BRANDON_UNDEAD:
        case NPC_SS_GRONDEL_UNDEAD:
        case NPC_SS_RUPERT_UNDEAD:
            return true;
        default:
            return false;
    }
}

bool IsFighting(Unit* unit) { return unit && unit->IsAlive() && unit->IsInCombat(); }

// Arnath, Brandon, Grondel, Rupert by slot; null where that captain is not a raised one in the fight
Creature* FightingCaptain(Player* bot, uint8 slot)
{
    Creature* captain = SlotCreature(bot, SS_DATA_CAPTAIN_ARNATH_GUID + slot);
    return IsRaisedCaptain(captain) && IsFighting(captain) ? captain : nullptr;
}

bool IsSsUnit(Unit* unit) { return IsRaisedCaptain(unit) || (unit && unit->GetEntry() == NPC_SS_SVALNA); }

bool AnyCaptainFighting(Player* bot)
{
    for (uint8 slot = 0; slot < SS_CAPTAINS; ++slot)
        if (FightingCaptain(bot, slot))
            return true;

    return false;
}

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
        if (member && member->IsAlive() && IccSameInstance(bot, member))
            members.push_back(member);
    }

    return members;
}

bool HoldsSomething(Player* bot, Player* member)
{
    if (Creature* svalna = SlotCreature(bot, SS_DATA_SVALNA_GUID))
        if (IsFighting(svalna) && svalna->GetVictim() == member)
            return true;

    for (uint8 slot = 0; slot < SS_CAPTAINS; ++slot)
        if (Creature* captain = FightingCaptain(bot, slot))
            if (captain->GetVictim() == member)
                return true;

    return false;
}

SsState* LiveState(PlayerbotAI* botAI)
{
    return IsSsLive(botAI) ? ssStates.Find(botAI->GetBot()->GetInstanceId()) : nullptr;
}

void Scan(SsState& st, Player* bot)
{
    st.spears.clear();
    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_SPEAR, SCAN_RANGE);
    for (Creature* spear : found)
        if (spear->IsAlive() && spear->GetVehicleBase())
            st.spears.push_back(spear->GetGUID());
}

Creature* LiveSpear(Player* bot, ObjectGuid const& guid)
{
    Creature* spear = bot->GetMap()->GetCreature(guid);
    return spear && spear->IsAlive() && spear->GetVehicleBase() ? spear : nullptr;
}

// Who may pull a spear: no impaled bot (stunned), no bot under Arnath's Dominate Mind, no tank holding an
// add, no bot locked to one ability (Bladestorm turns the click's cast into a silent no-op)
bool CanPull(Player* bot, Player* member)
{
    return IsDutyBot(member) && !member->HasAura(SPELL_SS_IMPALING_SPEAR) && !member->GetVehicle() &&
           !member->IsCharmed() && !member->HasAuraType(SPELL_AURA_ALLOW_ONLY_ABILITY) &&
           !(IsTankPlayer(member) && HoldsSomething(bot, member));
}

void AssignSpears(SsState& st, Player* bot, std::vector<Player*> const& members)
{
    // A puller that can no longer pull hands its spear on: the victim loses 10% a second meanwhile
    std::vector<ObjectGuid> lapsed;
    for (auto const& [puller, spear] : st.spear)
    {
        Player* member = ObjectAccessor::GetPlayer(*bot, puller);
        if (!LiveSpear(bot, spear) || !member || !member->IsAlive() || !IccSameInstance(bot, member) ||
            !CanPull(bot, member))
            lapsed.push_back(puller);
    }
    for (ObjectGuid const& puller : lapsed)
        st.spear.erase(puller);

    for (ObjectGuid const& guid : st.spears)
    {
        bool claimed = false;
        for (auto const& [puller, spear] : st.spear)
            claimed = claimed || spear == guid;
        Creature* spear = LiveSpear(bot, guid);
        if (claimed || !spear)
            continue;

        // Damage dealers first, then healers, then a tank holding nothing
        Player* best = nullptr;
        std::tuple<uint8, float, ObjectGuid> bestKey;
        for (Player* member : members)
        {
            if (!CanPull(bot, member) || st.spear.count(member->GetGUID()))
                continue;

            uint8 const tier = IsRaidDpsBot(member) ? 0 : PlayerbotAI::IsHeal(member) ? 1 : 2;
            auto const key = std::make_tuple(tier, member->GetExactDist2d(spear), member->GetGUID());
            if (!best || key < bestKey)
            {
                best = member;
                bestKey = key;
            }
        }

        if (best)
            st.spear[best->GetGUID()] = guid;
    }
}

void AssignCaptainTanks(SsState& st, Player* bot, std::vector<Player*> const& members)
{
    std::vector<ObjectGuid> lapsed;
    for (auto const& [holder, captainGuid] : st.tank)
    {
        Creature* captain = bot->GetMap()->GetCreature(captainGuid);
        Player* member = ObjectAccessor::GetPlayer(*bot, holder);
        if (!IsRaisedCaptain(captain) || !IsFighting(captain) || !member || !member->IsAlive() ||
            !IccSameInstance(bot, member))
            lapsed.push_back(holder);
    }
    for (ObjectGuid const& holder : lapsed)
        st.tank.erase(holder);

    for (uint8 slot : CAPTAIN_ORDER)
    {
        Creature* captain = FightingCaptain(bot, slot);
        if (!captain)
            continue;

        bool owned = false;
        for (auto const& [holder, captainGuid] : st.tank)
            owned = owned || captainGuid == captain->GetGUID();
        if (owned)
            continue;

        Player* best = nullptr;
        float bestDistance = 0.0f;
        for (Player* member : members)
        {
            if (!IsDutyBot(member) || !IsTankPlayer(member) || st.tank.count(member->GetGUID()))
                continue;

            float const distance = member->GetExactDist2d(captain);
            if (!best || distance < bestDistance ||
                (distance == bestDistance && member->GetGUID() < best->GetGUID()))
            {
                best = member;
                bestDistance = distance;
            }
        }

        if (best)
            st.tank[best->GetGUID()] = captain->GetGUID();
    }
}

Creature* OwnedCaptain(SsState const& st, Player* bot)
{
    auto const itr = st.tank.find(bot->GetGUID());
    Creature* captain = itr != st.tank.end() ? bot->GetMap()->GetCreature(itr->second) : nullptr;
    return IsRaisedCaptain(captain) && IsFighting(captain) ? captain : nullptr;
}

bool Shielded(Unit* svalna) { return svalna && svalna->HasAura(SPELL_AETHER_SHIELD); }

Unit* ResolveTarget(PlayerbotAI* botAI, SsState const& st)
{
    Player* bot = botAI->GetBot();
    bool const tank = IsTankPlayer(bot);
    if (!tank && !IsRaidDpsBot(bot))
        return nullptr;

    if (Unit* skull = GetRaidHumanSkull(botAI, &IsSsUnit))
    {
        NoteDerived(bot, "ss.target", "skull");
        return skull;
    }

    if (tank)
        if (Creature* captain = OwnedCaptain(st, bot))
        {
            NoteDerived(bot, "ss.target", "owned");
            return captain;
        }

    for (uint8 slot : CAPTAIN_ORDER)
    {
        Creature* captain = FightingCaptain(bot, slot);
        if (!captain || captain->HasAura(SPELL_SS_CAPTAIN_DIVINE_SHIELD))
            continue;

        // A tank leaves another tank's captain alone
        Player* victim = captain->GetVictim() ? captain->GetVictim()->ToPlayer() : nullptr;
        if (tank && victim && victim != bot && IsTankPlayer(victim))
            continue;

        NoteDerived(bot, "ss.target", "captain");
        return captain;
    }

    Creature* svalna = GetSsSvalna(botAI);
    if (IsFighting(svalna) && !Shielded(svalna))
    {
        NoteDerived(bot, "ss.target", "svalna");
        return svalna;
    }

    NoteDerived(bot, "ss.target", "none");
    return nullptr;
}

// The holder nearest Svalna throws; the rest keep theirs for the next shield
Player* ElectedThrower(Player* bot, Creature* svalna)
{
    Player* best = nullptr;
    float bestDistance = 0.0f;
    for (Player* member : MembersOnMap(bot))
    {
        if (!IsDutyBot(member) || !member->HasItemCount(ITEM_SPEAR, 1) || member->HasAura(SPELL_SS_IMPALING_SPEAR) ||
            member->IsCharmed())
            continue;

        float const distance = member->GetExactDist(svalna);
        if (!best || distance < bestDistance || (distance == bestDistance && member->GetGUID() < best->GetGUID()))
        {
            best = member;
            bestDistance = distance;
        }
    }

    return best;
}
}  // namespace

Creature* GetSsSvalna(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Creature* svalna = SlotCreature(bot, SS_DATA_SVALNA_GUID);
    return svalna && svalna->IsAlive() && bot->GetExactDist2d(svalna) <= LIVE_RANGE ? svalna : nullptr;
}

bool IsSsLive(PlayerbotAI* botAI)
{
    Creature* svalna = GetSsSvalna(botAI);
    if (!svalna)
        return false;

    return svalna->IsInCombat() || AnyCaptainFighting(botAI->GetBot());
}

bool IsSsShielded(PlayerbotAI* botAI, Unit* svalna)
{
    bool const shielded = Shielded(svalna);
    NoteDerived(botAI->GetBot(), "ss.shield", shielded ? "up" : "down");
    return shielded;
}

Unit* GetSsTarget(PlayerbotAI* botAI)
{
    SsState* st = LiveState(botAI);
    if (!st)
        return nullptr;

    TargetMemo& memo = st->targets[botAI->GetBot()->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.pick = ResolveTarget(botAI, *st);
    }

    return memo.pick;
}

Unit* GetSsCaptainToTaunt(PlayerbotAI* botAI)
{
    SsState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    Creature* captain = st ? OwnedCaptain(*st, bot) : nullptr;
    if (!captain || captain->GetVictim() == bot || !ClassTauntReady(botAI))
        return nullptr;

    return bot->GetExactDist(captain) <= TAUNT_RANGE && bot->IsWithinLOSInMap(captain) ? captain : nullptr;
}

Creature* GetSsSpearToPull(PlayerbotAI* botAI)
{
    SsState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st)
        return nullptr;

    auto const itr = st->spear.find(bot->GetGUID());
    return itr != st->spear.end() ? LiveSpear(bot, itr->second) : nullptr;
}

Item* GetSsSpearToThrow(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Creature* svalna = IsSsLive(botAI) ? GetSsSvalna(botAI) : nullptr;
    if (!svalna || !IsDutyBot(bot) || !bot->HasItemCount(ITEM_SPEAR, 1))
        return nullptr;

    if (!IsFighting(svalna) || !Shielded(svalna))
    {
        NoteDerived(bot, "ss.hurl", "hold");
        return nullptr;
    }

    if (ElectedThrower(bot, svalna) != bot)
    {
        NoteDerived(bot, "ss.hurl", "wait");
        return nullptr;
    }

    bool const inRange = bot->GetExactDist(svalna) <= HURL_RANGE && bot->IsWithinLOSInMap(svalna);
    NoteDerived(bot, "ss.hurl", inRange ? "throw" : "walk");
    for (Item* item : botAI->GetInventoryItems())
        if (item->GetEntry() == ITEM_SPEAR)
            return item;

    return nullptr;
}

void TickSsEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    // A wipe or a kill ends the fight: the next one rebuilds every duty. Her death kills the raised captains.
    Creature* slot = SlotCreature(bot, SS_DATA_SVALNA_GUID);
    if (!slot || !slot->IsAlive() || (!slot->IsInCombat() && !AnyCaptainFighting(bot)))
    {
        if (ssStates.Find(bot->GetInstanceId()))
            ssStates.Reset(bot->GetInstanceId());
        return;
    }

    if (!IsSsLive(botAI))
        return;

    SsState& st = ssStates.For(bot->GetInstanceId());
    if (Creature* svalna = GetSsSvalna(botAI))
        if (bot->GetExactDist2d(svalna) <= SHIELD_NOTE_RANGE)
            IsSsShielded(botAI, svalna);

    uint32 const now = getMSTime();
    if (st.scanned && getMSTimeDiff(st.scanMs, now) < SCAN_MS)
        return;

    st.scanned = true;
    st.scanMs = now;
    Scan(st, bot);
    std::vector<Player*> const members = MembersOnMap(bot);
    AssignSpears(st, bot, members);
    AssignCaptainTanks(st, bot, members);
}
