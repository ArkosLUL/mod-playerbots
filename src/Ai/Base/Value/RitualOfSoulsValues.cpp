/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RitualOfSoulsValues.h"

#include "CellImpl.h"
#include "Creature.h"
#include "GameObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Playerbots.h"
#include "Timer.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace
{
    // The core keeps the portal on its caster's GO list but not the soulwell, so a soulwell
    // takes a grid search. Only done for a while after the group's last portal sighting.
    constexpr uint32 SOULWELL_SEARCH_WINDOW_MS = 190000;  // 3 min soulwell + slack

    std::unordered_map<ObjectGuid, uint32> lastPortalSeen;
    std::mutex lastPortalSeenMutex;

    void StampPortalSeen(ObjectGuid groupGuid)
    {
        uint32 now = getMSTime();
        std::lock_guard<std::mutex> lock(lastPortalSeenMutex);
        for (auto it = lastPortalSeen.begin(); it != lastPortalSeen.end();)
        {
            if (getMSTimeDiff(it->second, now) >= SOULWELL_SEARCH_WINDOW_MS)
                it = lastPortalSeen.erase(it);
            else
                ++it;
        }
        lastPortalSeen[groupGuid] = now;
    }

    bool PortalSeenRecently(ObjectGuid groupGuid)
    {
        std::lock_guard<std::mutex> lock(lastPortalSeenMutex);
        auto it = lastPortalSeen.find(groupGuid);
        return it != lastPortalSeen.end() && getMSTimeDiff(it->second, getMSTime()) < SOULWELL_SEARCH_WINDOW_MS;
    }

    class IdleHostileCheck
    {
    public:
        IdleHostileCheck(WorldObject const* centre, float range) : _centre(centre), _range(range) {}

        bool operator()(Creature* creature) const
        {
            return _centre->IsWithinDist(creature, _range, false) && creature->IsAlive() && !creature->IsInCombat() &&
                   creature->HasReactState(REACT_AGGRESSIVE) && !creature->IsImmuneToPC() && !creature->IsCivilian();
        }

    private:
        WorldObject const* _centre;
        float _range;
    };

    float SegmentDistance2d(Position const& from, Position const& to, Position const& point)
    {
        float const dx = to.GetPositionX() - from.GetPositionX();
        float const dy = to.GetPositionY() - from.GetPositionY();
        float const lengthSq = dx * dx + dy * dy;
        float along = 0.0f;
        if (lengthSq > 0.0f)
            along = std::clamp(((point.GetPositionX() - from.GetPositionX()) * dx +
                                (point.GetPositionY() - from.GetPositionY()) * dy) / lengthSq,
                               0.0f, 1.0f);

        return point.GetExactDist2d(from.GetPositionX() + along * dx, from.GetPositionY() + along * dy);
    }
}  // namespace

std::vector<Creature*> FindIdleHostiles(WorldObject const* centre, float reach)
{
    float const range = reach + MAX_AGGRO_RADIUS + RITUAL_WALK_AGGRO_MARGIN;
    std::vector<Creature*> idle;
    IdleHostileCheck check(centre, range);
    Acore::CreatureListSearcher<IdleHostileCheck> searcher(centre, idle, check);
    Cell::VisitObjects(centre, searcher, range);
    return idle;
}

bool WalkWakesIdleHostile(std::vector<Creature*> const& idle, Player* walker, Position const& to)
{
    Position const from = walker->GetPosition();
    for (Creature* creature : idle)
    {
        if (!creature->IsHostileTo(walker))
            continue;

        float const aggro = creature->GetAggroRange(walker);
        if (aggro > 0.0f && SegmentDistance2d(from, to, creature->GetPosition()) < aggro + RITUAL_WALK_AGGRO_MARGIN)
            return true;
    }

    return false;
}

ObjectGuid RitualOfSoulsPortalValue::Calculate()
{
    Group* group = bot->GetGroup();
    if (!group)
        return ObjectGuid::Empty;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        // other maps update on other threads, so their GO lists are off limits
        if (!member || member->getClass() != CLASS_WARLOCK || !member->IsInMap(bot))
            continue;

        GameObject* portal = member->GetGameObject(SPELL_RITUAL_OF_SOULS_R2);
        if (!portal)
            portal = member->GetGameObject(SPELL_RITUAL_OF_SOULS_R1);
        if (!portal)
            continue;

        StampPortalSeen(group->GetGUID());

        if (member != bot && bot->IsWithinDistInMap(portal, RITUAL_OF_SOULS_GO_RANGE))
            return portal->GetGUID();
    }

    return ObjectGuid::Empty;
}

ObjectGuid RitualOfSoulsSoulwellValue::Calculate()
{
    Group* group = bot->GetGroup();
    if (!group || !PortalSeenRecently(group->GetGUID()))
        return ObjectGuid::Empty;

    for (uint32 entry : {GO_SOULWELL_R1, GO_SOULWELL_R2})
    {
        GameObject* soulwell = bot->FindNearestGameObject(entry, RITUAL_OF_SOULS_GO_RANGE);
        if (soulwell && group->IsMember(soulwell->GetOwnerGUID()))
            return soulwell->GetGUID();
    }

    return ObjectGuid::Empty;
}

bool RitualWalkWakesHostileValue::Calculate()
{
    GameObject* go = botAI->GetGameObject(AI_VALUE(ObjectGuid, qualifier));
    if (!go || go->IsAtInteractDistance(bot))
        return false;

    return WalkWakesIdleHostile(FindIdleHostiles(bot, bot->GetExactDist2d(go)), bot, go->GetPosition());
}
