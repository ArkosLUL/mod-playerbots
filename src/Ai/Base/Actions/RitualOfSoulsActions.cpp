/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RitualOfSoulsActions.h"

#include "GameObject.h"
#include "Group.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Timer.h"

#include <mutex>
#include <vector>

namespace
{
    GameObject* FindGroupOwnedGO(PlayerbotAI* botAI, uint32 entry, bool excludeSelf, float range)
    {
        Player* bot = botAI->GetBot();
        Group* group = bot->GetGroup();
        if (!group)
            return nullptr;

        GameObject* go = bot->FindNearestGameObject(entry, range);
        if (!go)
            return nullptr;

        ObjectGuid ownerGuid = go->GetOwnerGUID();
        if (!ownerGuid)
            return nullptr;

        if (excludeSelf && ownerGuid == bot->GetGUID())
            return nullptr;

        if (!group->IsMember(ownerGuid))
            return nullptr;

        return go;
    }

    // Reservation slots (one per helper) so a whole raid doesn't stampede a single portal. Overshoot
    // is harmless server-side, but capping at the required participant count keeps things tidy and
    // frees a slot if a reserved bot never reaches the portal.
    struct RitualSlot
    {
        ObjectGuid go;
        ObjectGuid bot;
        uint32 expiry;
    };

    std::vector<RitualSlot> ritualReservations;
    std::mutex ritualReservationsMutex;

    void CleanupRitualReservations(uint32 now)
    {
        for (auto it = ritualReservations.begin(); it != ritualReservations.end();)
        {
            if (it->expiry <= now)
                it = ritualReservations.erase(it);
            else
                ++it;
        }
    }

    // Try to claim a helper slot on the given ritual. Returns false when the ritual is already full
    // and this bot isn't one of the existing holders.
    bool TryReserveRitualSlot(ObjectGuid goGuid, ObjectGuid botGuid, uint32 reqHelpers)
    {
        uint32 now = getMSTime();
        std::lock_guard<std::mutex> lock(ritualReservationsMutex);
        CleanupRitualReservations(now);

        uint32 active = 0;
        for (RitualSlot const& slot : ritualReservations)
        {
            if (slot.go != goGuid)
                continue;
            if (slot.bot == botGuid)
                return true;  // already holding a slot, keep going
            ++active;
        }

        if (active >= reqHelpers)
            return false;

        ritualReservations.push_back({goGuid, botGuid, now + 5000});
        return true;
    }
}  // namespace

GameObject* FindGroupRitualPortal(PlayerbotAI* botAI, bool excludeSelf, float range)
{
    if (GameObject* go = FindGroupOwnedGO(botAI, GO_SOUL_PORTAL_R1, excludeSelf, range))
        return go;
    return FindGroupOwnedGO(botAI, GO_SOUL_PORTAL_R2, excludeSelf, range);
}

GameObject* FindGroupSoulwell(PlayerbotAI* botAI, float range)
{
    if (GameObject* go = FindGroupOwnedGO(botAI, GO_SOULWELL_R1, false, range))
        return go;
    return FindGroupOwnedGO(botAI, GO_SOULWELL_R2, false, range);
}

bool JoinRitualOfSoulsAction::Execute(Event /*event*/)
{
    GameObject* portal = FindGroupRitualPortal(botAI);
    if (!portal)
        return false;

    uint32 reqParticipants = 0;
    if (GameObjectTemplate const* info = portal->GetGOInfo())
        reqParticipants = info->summoningRitual.reqParticipants;

    // reqParticipants counts the channelling caster too, so helpers needed is one fewer.
    uint32 reqHelpers = reqParticipants > 1 ? reqParticipants - 1 : 1;
    if (!TryReserveRitualSlot(portal->GetGUID(), bot->GetGUID(), reqHelpers))
        return false;

    if (!portal->IsAtInteractDistance(bot))
        return MoveTo(portal, fmaxf(portal->GetInteractionDistance() - 1.0f, 0.0f));

    WorldPacket data(CMSG_GAMEOBJ_USE);
    data << portal->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data);
    return true;
}

bool UseSoulwellAction::Execute(Event /*event*/)
{
    if (!AI_VALUE2(std::vector<Item*>, "inventory items", "healthstone").empty())
        return false;

    GameObject* soulwell = FindGroupSoulwell(botAI);
    if (!soulwell)
        return false;

    if (!soulwell->IsAtInteractDistance(bot))
        return MoveTo(soulwell, fmaxf(soulwell->GetInteractionDistance() - 1.0f, 0.0f));

    WorldPacket data(CMSG_GAMEOBJ_USE);
    data << soulwell->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data);
    return true;
}

bool RitualPortalNearbyTrigger::IsActive()
{
    if (bot->IsInCombat())
        return false;

    return FindGroupRitualPortal(botAI) != nullptr;
}

bool SoulwellNearbyTrigger::IsActive()
{
    if (bot->IsInCombat())
        return false;

    if (!AI_VALUE2(std::vector<Item*>, "inventory items", "healthstone").empty())
        return false;

    return FindGroupSoulwell(botAI) != nullptr;
}
