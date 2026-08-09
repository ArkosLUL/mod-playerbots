/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXBOSSHELPER_H
#define PLAYERBOTS_NAXXBOSSHELPER_H

#include "AiObject.h"
#include "AiObjectContext.h"
#include "EventMap.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "NamedObjectContext.h"
#include "NaxxSpellIds.h"
#include "ObjectGuid.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "Timer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

const uint32 NAXX_MAP_ID = 533;

inline bool NaxxHasStrategyAnyState(Player* player, char const* name)
{
    if (!player || !name)
        return false;

    if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
        return ai->HasStrategy(name, BOT_STATE_NON_COMBAT) || ai->HasStrategy(name, BOT_STATE_COMBAT);

    return false;
}

// Slot assignment for ring formations. Every bot walks the group in the same order, so each one
// derives the same index for itself without any of them having to agree on anything.
struct NaxxRoleGroups
{
    std::vector<Player*> healers;
    std::vector<Player*> rangedDps;
    std::vector<Player*> meleeDps;
};

inline NaxxRoleGroups NaxxGetRoleGroups(PlayerbotAI* botAI, Player* bot)
{
    NaxxRoleGroups result;
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group)
    {
        return result;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        // Dead members keep their slot: dropping them would renumber everyone else mid-fight.
        if (!member || botAI->IsTank(member))
        {
            continue;
        }
        if (botAI->IsHeal(member))
        {
            result.healers.push_back(member);
        }
        else if (botAI->IsRanged(member))
        {
            result.rangedDps.push_back(member);
        }
        else
        {
            result.meleeDps.push_back(member);
        }
    }
    return result;
}

// {slot index, slots on this ring}. Count is never 0, so callers can divide by it.
inline std::pair<size_t, size_t> NaxxGetSlotIndexAndCount(PlayerbotAI* botAI, Player* bot,
                                                          NaxxRoleGroups const& groups)
{
    std::vector<Player*> const& slots =
        botAI->IsHeal(bot) ? groups.healers : (botAI->IsRanged(bot) ? groups.rangedDps : groups.meleeDps);

    auto it = std::find(slots.begin(), slots.end(), bot);
    if (it == slots.end())
    {
        return {0, 1};
    }
    return {static_cast<size_t>(std::distance(slots.begin(), it)), slots.size()};
}

template <class BossAiType>
class GenericBossHelper : public AiObject
{
public:
    GenericBossHelper(PlayerbotAI* botAI, std::string name) : AiObject(botAI), _name(name) {}
    virtual bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
        {
            _unit = nullptr;
        }
        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
        {
            _unit = nullptr;
        }
        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", _name);
            if (!_unit)
            {
                return false;
            }
            _target = _unit->ToCreature();
            if (!_target)
            {
                return false;
            }
            _ai = dynamic_cast<BossAiType*>(_target->GetAI());
            if (!_ai)
            {
                return false;
            }
            _event_map = &_ai->events;
            if (!_event_map)
            {
                return false;
            }
        }
        if (!_event_map)
        {
            return false;
        }
        _timer = getMSTime();
        return true;
    }
    virtual void Reset()
    {
        _unit = nullptr;
        _target = nullptr;
        _ai = nullptr;
        _event_map = nullptr;
        _timer = 0;
    }

protected:
    std::string _name;
    Unit* _unit = nullptr;
    Creature* _target = nullptr;
    BossAiType* _ai = nullptr;
    EventMap* _event_map = nullptr;
    uint32 _timer = 0;
};

class KelthuzadBossHelper : public AiObject
{
public:
    KelthuzadBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}

    static constexpr uint32 NPC_GUARDIAN_OF_ICECROWN = 16441;
    static constexpr uint32 NPC_SHADOW_FISSURE = 16129;

    bool IsGuardian(Unit* unit) const
    {
        if (!unit)
            return false;
        if (Creature* c = unit->ToCreature())
            if (c->GetEntry() == NPC_GUARDIAN_OF_ICECROWN)
                return true;
        return botAI->EqualLowercaseName(unit->GetName(), "guardian of icecrown");
    }

    bool IsShadowFissure(Unit* unit) const
    {
        if (!unit)
            return false;
        if (Creature* c = unit->ToCreature())
            if (c->GetEntry() == NPC_SHADOW_FISSURE)
                return true;
        return botAI->EqualLowercaseName(unit->GetName(), "shadow fissure");
    }

    // Activated adds are AttackStart()ed by the boss script; parked perimeter adds are
    // stationary decoration until their proximity aggro fires.
    bool IsAddActive(Unit* unit) const
    {
        return unit && (unit->IsInCombat() || unit->GetVictim() || unit->isMoving());
    }

    const std::pair<float, float> center = {3716.19f, -5106.58f};
    const std::pair<float, float> tank_pos = {3709.19f, -5104.86f};
    const std::pair<float, float> assist_tank_pos = {3746.05f, -5112.74f};

    static constexpr float ROOM_MIN_RADIUS = 6.0f;
    static constexpr float ROOM_MAX_RADIUS = 24.0f;
    static constexpr float DETONATE_MIN_RADIUS = 20.0f;
    static constexpr float DETONATE_MAX_RADIUS = 24.0f;
    static constexpr float TANK_HOLD_MAX_RADIUS = 20.0f;
    static constexpr float PHASE1_TANK_MAX_RADIUS = 16.0f;
    static constexpr float PHASE1_TANK_HOLD_RADIUS = 12.0f;
    // Void Blast (27812) is 10y, but radius checks add target combat reach - pad the escape.
    static constexpr float FISSURE_DANGER_RADIUS = 13.0f;
    // Frost Blast chains at 10y around its target.
    static constexpr float FROST_BLAST_SAFE_DIST = 12.0f;
    static constexpr float P1_ACTIVE_ADD_MAX_CENTER_DIST = 45.0f;

    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
        {
            Reset();
        }
        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
        {
            Reset();
        }
        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "kel'thuzad");
        }
        return _unit != nullptr;
    }
    bool IsPhaseOne() { return _unit && _unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE); }
    bool IsPhaseTwo() { return _unit && !_unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE); }

    Unit* GetBoss() const { return _unit; }

    bool IsBossCastingAny(std::initializer_list<uint32> spellIds) const
    {
        if (!_unit)
        {
            return false;
        }

        if (Spell* spell = _unit->GetCurrentSpell(CURRENT_GENERIC_SPELL))
        {
            return NaxxSpellIds::MatchesAnySpellId(spell->GetSpellInfo(), spellIds);
        }
        return false;
    }

    bool IsBossCasting(uint32 spellId) const { return IsBossCastingAny({spellId}); }

    uint32 GetRangedCount() const
    {
        Group* group = bot->GetGroup();
        if (!group)
        {
            return 0;
        }

        uint32 count = 0;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member)
            {
                continue;
            }

            if (botAI->IsRanged(member))
            {
                ++count;
            }
        }
        return count;
    }

    // GetMeleeIndex counts tanks too - melee spread needs a DPS-only index.
    uint32 GetMeleeDpsIndex(Player* player) const
    {
        Group* group = bot->GetGroup();
        if (!group)
        {
            return 0;
        }

        uint32 index = 0;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member)
            {
                continue;
            }

            if (botAI->IsRanged(member) || botAI->IsTank(member))
            {
                continue;
            }

            if (member == player)
            {
                return index;
            }
            ++index;
        }
        return 0;
    }

    uint32 GetMeleeDpsCount() const
    {
        Group* group = bot->GetGroup();
        if (!group)
        {
            return 0;
        }

        uint32 count = 0;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member)
            {
                continue;
            }

            if (!botAI->IsRanged(member) && !botAI->IsTank(member))
            {
                ++count;
            }
        }
        return count;
    }

    void ClampToRoom(float& x, float& y, float minRadius = ROOM_MIN_RADIUS, float maxRadius = ROOM_MAX_RADIUS) const
    {
        float dx = x - center.first;
        float dy = y - center.second;
        float r2 = dx * dx + dy * dy;
        if (r2 < 0.0001f)
        {
            x = center.first + minRadius;
            y = center.second;
            return;
        }

        float r = std::sqrt(r2);
        float clamped = std::clamp(r, minRadius, maxRadius);
        x = center.first + dx / r * clamped;
        y = center.second + dy / r * clamped;
    }

    bool IsWithinRoom(WorldObject const* obj, float maxRadius = ROOM_MAX_RADIUS) const
    {
        return obj && obj->GetDistance2d(center.first, center.second) <= maxRadius;
    }

    bool RecallControlledPetsToBot(float leashRadius = (ROOM_MAX_RADIUS + 2.0f), float followDist = 1.5f)
    {
        bool recalled = false;

        auto RecallUnit = [&](Unit* u)
        {
            if (!u)
                return;

            Creature* creature = u->ToCreature();
            if (!creature)
                return;

            if (creature->IsTotem())
                return;

            if (creature->GetDistance2d(center.first, center.second) <= leashRadius)
                return;

            creature->AttackStop();

            if (CharmInfo* charm = creature->GetCharmInfo())
            {
                charm->SetIsCommandAttack(false);
                charm->SetIsAtStay(false);
                charm->SetIsFollowing(true);
                charm->SetIsCommandFollow(true);
                charm->SetIsReturning(false);
            }

            creature->GetMotionMaster()->MoveFollow(bot, followDist, M_PI);
            recalled = true;
        };

        RecallUnit(bot->GetPet());

        for (Unit::ControlSet::const_iterator itr = bot->m_Controlled.begin(); itr != bot->m_Controlled.end(); ++itr)
        {
            RecallUnit(*itr);
        }

        return recalled;
    }

    std::pair<float, float> GetAssistTankHoldPosition() const
    {
        float x = assist_tank_pos.first;
        float y = assist_tank_pos.second;
        ClampToRoom(x, y, ROOM_MIN_RADIUS, TANK_HOLD_MAX_RADIUS);
        return {x, y};
    }

    std::pair<float, float> GetMainTankHoldPosition() const
    {
        float x = tank_pos.first;
        float y = tank_pos.second;
        ClampToRoom(x, y, ROOM_MIN_RADIUS, TANK_HOLD_MAX_RADIUS);
        return {x, y};
    }

    // Two-ring layout keeping every pair >= ~11y apart (Frost Blast chains at 10y).
    // Outer ring r=24 holds up to 12; overflow goes to an inner ring at r=14 whose slots
    // are staggered 15 degrees off the outer grid and ordered to stay away from the MT.
    void ComputeRangedSpreadPosition(uint32 index, uint32 total, float& outX, float& outY) const
    {
        if (total == 0)
        {
            outX = center.first;
            outY = center.second;
            return;
        }

        float tankAngle = std::atan2(tank_pos.second - center.second, tank_pos.first - center.first);
        uint32 nOuter = std::min<uint32>(total, 12);

        float angle;
        float radius;
        if (index < nOuter)
        {
            angle = tankAngle + float(M_PI) / 12.0f + 2.0f * float(M_PI) * float(index) / float(nOuter);
            radius = ROOM_MAX_RADIUS;
        }
        else
        {
            // +-90/+-150 first (far from MT); +-30 are last-resort slots ~8.6y from the MT.
            static constexpr float innerOffsetsDeg[6] = {90.0f, -90.0f, 150.0f, -150.0f, 30.0f, -30.0f};
            angle = tankAngle + innerOffsetsDeg[(index - nOuter) % 6] * float(M_PI) / 180.0f;
            radius = 14.0f;
        }

        outX = center.first + std::cos(angle) * radius;
        outY = center.second + std::sin(angle) * radius;
        ClampToRoom(outX, outY);
    }

    // Melee DPS rear arc around the boss, excluding a half-arc around the MT so a Frost
    // Blast on melee never chains to the tank. If the boss reach is too small for real
    // isolation, everyone stacks the single rear point and we accept the residual risk.
    bool ComputeMeleeSpreadPosition(uint32 index, uint32 total, float& outX, float& outY)
    {
        Unit* boss = GetBoss();
        if (!boss || total == 0)
        {
            return false;
        }

        Player* mainTank = nullptr;
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (member && member->IsAlive() && botAI->IsMainTank(member))
                {
                    mainTank = member;
                    break;
                }
            }
        }

        float refAngle = mainTank ? boss->GetAngle(mainTank) : boss->GetAngle(tank_pos.first, tank_pos.second);

        float maxR = std::max(5.0f, bot->GetMeleeRange(boss) - 0.5f);
        float r = std::clamp(boss->GetCombatReach() + 1.0f, 5.0f, maxR);

        float phi = std::max(100.0f * float(M_PI) / 180.0f, 2.0f * std::asin(std::min(1.0f, 5.5f / r)));
        float arcWidth = 2.0f * float(M_PI) - 2.0f * phi;

        float angle;
        if (total == 1 || arcWidth <= 0.0f)
        {
            angle = refAngle + float(M_PI);
        }
        else
        {
            angle = refAngle + phi + arcWidth * float(index % total) / float(total - 1);
        }

        outX = boss->GetPositionX() + std::cos(angle) * r;
        outY = boss->GetPositionY() + std::sin(angle) * r;
        ClampToRoom(outX, outY);
        return true;
    }

    // Clamp-aware escape from a hazard point: straight away first; if ClampToRoom would
    // project the destination back into danger (outer-ring bots), walk the ring instead.
    bool ComputeEscapeFromPoint(float hx, float hy, float safeDist, float& outX, float& outY)
    {
        float dirX = bot->GetPositionX() - hx;
        float dirY = bot->GetPositionY() - hy;
        float len = std::sqrt(dirX * dirX + dirY * dirY);
        if (len < 0.001f)
        {
            dirX = bot->GetPositionX() - center.first;
            dirY = bot->GetPositionY() - center.second;
            len = std::sqrt(dirX * dirX + dirY * dirY);
        }
        if (len < 0.001f)
        {
            dirX = 1.0f;
            dirY = 0.0f;
            len = 1.0f;
        }

        float dx = hx + dirX / len * (safeDist + 1.0f);
        float dy = hy + dirY / len * (safeDist + 1.0f);
        ClampToRoom(dx, dy);
        if (std::sqrt((dx - hx) * (dx - hx) + (dy - hy) * (dy - hy)) >= safeDist)
        {
            outX = dx;
            outY = dy;
            return true;
        }

        float botDx = bot->GetPositionX() - center.first;
        float botDy = bot->GetPositionY() - center.second;
        float r = std::clamp(std::sqrt(botDx * botDx + botDy * botDy), ROOM_MIN_RADIUS, ROOM_MAX_RADIUS);
        float theta = std::atan2(botDy, botDx);
        float hazardTheta = std::atan2(hy - center.second, hx - center.first);
        float diff = theta - hazardTheta;
        while (diff > float(M_PI))
        {
            diff -= 2.0f * float(M_PI);
        }
        while (diff < -float(M_PI))
        {
            diff += 2.0f * float(M_PI);
        }
        float awaySign = diff >= 0.0f ? 1.0f : -1.0f;

        for (uint32 k = 1; k <= 8; ++k)
        {
            for (float sign : {awaySign, -awaySign})
            {
                float candTheta = theta + sign * float(k) * float(M_PI) / 12.0f;
                float cx = center.first + std::cos(candTheta) * r;
                float cy = center.second + std::sin(candTheta) * r;
                if (std::sqrt((cx - hx) * (cx - hx) + (cy - hy) * (cy - hy)) >= safeDist)
                {
                    outX = cx;
                    outY = cy;
                    return true;
                }
            }
        }
        return false;
    }

    Player* GetPlayerWithAura(uint32 spellId)
    {
        Group* group = bot->GetGroup();
        if (!group)
        {
            return nullptr;
        }
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive())
            {
                continue;
            }
            if (member->HasAura(spellId))
            {
                return member;
            }
        }
        return nullptr;
    }
    bool HasAuraInGroup(uint32 spellId) { return GetPlayerWithAura(spellId) != nullptr; }
    bool HasDetonateMana(Player* player)
    {
        if (!player)
        {
            return false;
        }
        return player->HasAura(NaxxSpellIds::DetonateMana);
    }
    bool HasChains(Player* player)
    {
        if (!player)
        {
            return false;
        }
        return player->HasAura(NaxxSpellIds::ChainsOfKelthuzad);
    }

    std::vector<Unit*> GetGuardians() const
    {
        std::vector<Unit*> guardians;
        GuidVector targets = context->GetValue<GuidVector>("possible targets")->Get();
        for (ObjectGuid const& guid : targets)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit)
            {
                continue;
            }

            if (!IsGuardian(unit))
            {
                continue;
            }

            if (unit->GetDistance2d(center.first, center.second) > (ROOM_MAX_RADIUS + 4.0f))
            {
                continue;
            }

            guardians.push_back(unit);
        }

        return guardians;
    }

    bool AllGuardiansOnAssistTank(Player* assistTank) const
    {
        if (!assistTank)
        {
            return true;
        }

        std::vector<Unit*> guardians = GetGuardians();
        if (guardians.empty())
        {
            return true;
        }

        for (Unit* g : guardians)
        {
            if (!g)
            {
                continue;
            }

            if (g->GetVictim() != assistTank)
            {
                return false;
            }
        }
        return true;
    }

    Unit* GetGuardianToPickup(Player* assistTank) const
    {
        if (!assistTank)
        {
            return nullptr;
        }

        std::vector<Unit*> guardians = GetGuardians();
        if (guardians.empty())
        {
            return nullptr;
        }

        Unit* best = nullptr;
        float bestDist = std::numeric_limits<float>::max();
        for (Unit* g : guardians)
        {
            if (!g)
            {
                continue;
            }

            if (g->GetVictim() == assistTank)
            {
                continue;
            }

            float d = assistTank->GetDistance2d(g);
            if (!best || d < bestDist)
            {
                best = g;
                bestDist = d;
            }
        }
        if (!best)
        {
            for (Unit* g : guardians)
            {
                float d = assistTank->GetDistance2d(g);
                if (!best || d < bestDist)
                {
                    best = g;
                    bestDist = d;
                }
            }
        }

        return best;
    }

    Unit* GetGuardian()
    {
        GuidVector targets = context->GetValue<GuidVector>("possible targets")->Get();
        for (auto i = targets.begin(); i != targets.end(); ++i)
        {
            Unit* unit = botAI->GetUnit(*i);
            if (!unit)
            {
                continue;
            }
            if (IsGuardian(unit))
            {
                return unit;
            }
        }
        return nullptr;
    }

    Unit* GetGuardianForAssistTank(Player* assistTank)
    {
        if (!assistTank)
        {
            return nullptr;
        }

        GuidVector targets = context->GetValue<GuidVector>("possible targets")->Get();
        Unit* best = nullptr;
        float bestScore = std::numeric_limits<float>::max();

        for (ObjectGuid const& guid : targets)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit)
            {
                continue;
            }

            if (!IsGuardian(unit))
            {
                continue;
            }
            if (unit->GetDistance2d(center.first, center.second) > (ROOM_MAX_RADIUS + 4.0f))
            {
                continue;
            }

            Player* victimPlayer = unit->GetVictim() ? unit->GetVictim()->ToPlayer() : nullptr;
            bool victimIsAssistTank = victimPlayer && botAI->IsAssistTank(victimPlayer);

            float score = unit->GetDistance2d(assistTank);
            if (victimIsAssistTank)
            {
                score += 1000.0f;
            }

            if (!best || score < bestScore)
            {
                best = unit;
                bestScore = score;
            }
        }

        return best;
    }

    Unit* GetNearestShadowFissure()
    {
        Unit* nearest = nullptr;
        float bestDist = std::numeric_limits<float>::max();
        GuidVector units = *context->GetValue<GuidVector>("nearest triggers");
        for (ObjectGuid const& guid : units)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!IsShadowFissure(unit))
            {
                continue;
            }

            float dist = bot->GetDistance2d(unit);
            if (!nearest || dist < bestDist)
            {
                nearest = unit;
                bestDist = dist;
            }
        }
        return nearest;
    }

    bool IsNearShadowFissure(float x, float y, float radius = FISSURE_DANGER_RADIUS)
    {
        GuidVector units = *context->GetValue<GuidVector>("nearest triggers");
        for (ObjectGuid const& guid : units)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!IsShadowFissure(unit))
            {
                continue;
            }

            if (unit->GetDistance2d(x, y) < radius)
            {
                return true;
            }
        }
        return false;
    }

private:
    void Reset() { _unit = nullptr; }

    Unit* _unit = nullptr;
};

class RazuviousBossHelper : public AiObject
{
public:
    RazuviousBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
        {
            Reset();
        }
        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
        {
            Reset();
        }
        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "instructor razuvious");
        }
        return _unit != nullptr;
    }

private:
    void Reset() { _unit = nullptr; }

    Unit* _unit = nullptr;
};

class SapphironBossHelper : public AiObject
{
public:
    const std::pair<float, float> mainTankPos = {3512.07f, -5274.06f};
    const std::pair<float, float> center = {3517.31f, -5253.74f};
    const float GENERIC_HEIGHT = 137.29f;
    SapphironBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
        {
            Reset();
        }
        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
        {
            Reset();
        }
        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "sapphiron");
            if (!_unit)
            {
                return false;
            }
        }
        bool now_flying = _unit->IsFlying();
        if (_was_flying && !now_flying)
        {
            _last_land_ms = getMSTime();
        }
        _was_flying = now_flying;
        UpdateIceboltState();
        return true;
    }
    bool IsPhaseGround() { return _unit && !_unit->IsFlying(); }
    bool IsPhaseFlight() { return _unit && _unit->IsFlying(); }
    bool JustLanded()
    {
        if (!_last_land_ms)
        {
            return false;
        }
        return getMSTime() - _last_land_ms <= POSITION_TIME_AFTER_LANDED;
    }
    bool WaitForExplosion()
    {
        if (!IsPhaseFlight())
        {
            return false;
        }
        return HasIceboltInGroup();
    }
    bool IsBreathWindow()
    {
        if (!IsPhaseFlight())
        {
            return false;
        }
        if (IsBreathCasting())
        {
            return true;
        }
        if (!_last_icebolt_ms)
        {
            return false;
        }
        uint32 elapsed = getMSTime() - _last_icebolt_ms;
        return elapsed >= BREATH_MIN_MS && elapsed <= BREATH_MAX_MS;
    }
    bool HasLifeDrainInGroup()
    {
        Group* group = bot->GetGroup();
        if (!group)
        {
            return false;
        }
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member)
            {
                continue;
            }
            if (NaxxSpellIds::HasAnyAura(member, {NaxxSpellIds::LifeDrain}) || botAI->HasAura("life drain", member))
            {
                return true;
            }
        }
        return false;
    }
    bool FindPosToAvoidChill(std::vector<float>& dest)
    {
        Aura* aura = NaxxSpellIds::GetAnyAura(bot, {NaxxSpellIds::Chill25});
        if (!aura)
        {
            aura = botAI->GetAura("chill", bot);
        }
        if (!aura)
        {
            return false;
        }
        /*DynamicObject* dyn_obj = aura->GetDynobjOwner();
        if (!dyn_obj)*/
        // Prefer the dynobject (classic ground effect), but keep a fallback for cases where
        // the aura is applied by a moving caster (e.g. Blizzard NPC) without a dynobject.
        WorldObject* source = aura->GetDynobjOwner();
        if (!source)
        {
            //return false;
            if (Unit* caster = ObjectAccessor::GetUnit(*bot, aura->GetCasterGUID()))
            {
                source = caster;
            }
        }
        if (!source)
        {
            return false;
        }
        Unit* currentTarget = AI_VALUE(Unit*, "current target");
        float angle = 0;
        uint32 index = botAI->GetGroupSlotIndex(bot);
        if (currentTarget)
        {
            if (botAI->IsRanged(bot))
            {
                if (bot->GetExactDist2d(currentTarget) <= 45.0f)
                {
                    angle = bot->GetAngle(source) - M_PI + (rand_norm() - 0.5) * M_PI / 2;
                }
                else
                {
                    if (index % 2 == 0)
                    {
                        angle = bot->GetAngle(currentTarget) + M_PI / 2;
                    }
                    else
                    {
                        angle = bot->GetAngle(currentTarget) - M_PI / 2;
                    }
                }
            }
            else
            {
                if (index % 3 == 0)
                {
                    angle = bot->GetAngle(currentTarget);
                }
                else if (index % 3 == 1)
                {
                    angle = bot->GetAngle(currentTarget) + M_PI / 2;
                }
                else
                {
                    angle = bot->GetAngle(currentTarget) - M_PI / 2;
                }
            }
        }
        else
        {
            angle = bot->GetAngle(source) - M_PI + (rand_norm() - 0.5) * M_PI / 2;
        }
        dest = {bot->GetPositionX() + cos(angle) * 5.0f, bot->GetPositionY() + sin(angle) * 5.0f, bot->GetPositionZ()};
        return true;
    }

private:
    void Reset()
    {
        _unit = nullptr;
        _was_flying = false;
        _last_land_ms = 0;
        _last_icebolt_ms = 0;
    }

    const uint32 POSITION_TIME_AFTER_LANDED = 5000;
    const uint32 BREATH_MIN_MS = 1000;
    const uint32 BREATH_MAX_MS = 12000;
    bool HasIceboltInGroup()
    {
        Group* group = bot->GetGroup();
        if (!group)
        {
            return false;
        }
        bool hasIcebolt = false;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member)
            {
                continue;
            }
            if (NaxxSpellIds::HasAnyAura(member, {NaxxSpellIds::Icebolt10, NaxxSpellIds::Icebolt25}) ||
                botAI->HasAura("icebolt", member, false, false, -1, true))
            {
                hasIcebolt = true;
                break;
            }
        }
        if (hasIcebolt)
        {
            _last_icebolt_ms = getMSTime();
        }
        return hasIcebolt;
    }
    void UpdateIceboltState()
    {
        if (!IsPhaseFlight())
        {
            _last_icebolt_ms = 0;
            return;
        }
        HasIceboltInGroup();
    }
    bool IsBreathCasting()
    {
        if (!_unit || !_unit->HasUnitState(UNIT_STATE_CASTING))
        {
            return false;
        }
        Spell* spell = _unit->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell)
        {
            spell = _unit->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        }
        if (!spell)
        {
            return false;
        }
        SpellInfo const* info = spell->GetSpellInfo();
        return NaxxSpellIds::MatchesAnySpellId(info, {NaxxSpellIds::FrostMissile, NaxxSpellIds::FrostExplosion});
    }

    Unit* _unit = nullptr;
    bool _was_flying = false;
    uint32 _last_land_ms = 0;
    uint32 _last_icebolt_ms = 0;
};

class GluthBossHelper : public AiObject
{
public:
    const std::pair<float, float> mainTankPos25 = {3331.48f, -3109.06f};
    const std::pair<float, float> mainTankPos10 = {3278.29f, -3162.06f};
    const std::pair<float, float> beforeDecimatePos = {3267.34f, -3175.68f};
    const std::pair<float, float> leftSlowDownPos = {3290.68f, -3141.65f};
    const std::pair<float, float> rightSlowDownPos = {3300.78f, -3151.98f};
    const std::pair<float, float> rangedPos = {3301.45f, -3139.29f};
    const std::pair<float, float> healPos = {3303.09f, -3135.24f};

    const float decimatedZombiePct = 10.0f;
    GluthBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
        {
            Reset();
        }
        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
        {
            Reset();
        }
        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "gluth");
            if (!_unit)
            {
                return false;
            }
        }
        if (_unit->IsInCombat())
        {
            if (_combat_start_ms == 0)
            {
                _combat_start_ms = getMSTime();
            }
        }
        else
        {
            _combat_start_ms = 0;
        }
        if (BeforeDecimate())
        {
            _decimate_cast_ms = getMSTime();
        }
        return true;
    }
    bool BeforeDecimate()
    {
        if (!_unit || !_unit->HasUnitState(UNIT_STATE_CASTING))
        {
            return false;
        }
        Spell* spell = _unit->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell)
        {
            spell = _unit->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        }
        if (!spell)
        {
            return false;
        }
        SpellInfo const* info = spell->GetSpellInfo();
        if (!info)
        {
            return false;
        }
        if (NaxxSpellIds::MatchesAnySpellId(
                info, {NaxxSpellIds::Decimate10, NaxxSpellIds::Decimate25, NaxxSpellIds::Decimate25Alt}))
        {
            return true;
        }

        return info->SpellName[LOCALE_enUS] && botAI->EqualLowercaseName(info->SpellName[LOCALE_enUS], "decimate");
    }
    // The Decimate cast itself is over in a second, but the zombie wave it releases only reaches the
    // off tank a good while later, so anything that has to react to the wave needs the wider window.
    bool InDecimateWindow() const
    {
        return _decimate_cast_ms != 0 && getMSTime() - _decimate_cast_ms < DecimateWindowMs;
    }
    bool JustStartCombat() const { return _combat_start_ms != 0 && getMSTime() - _combat_start_ms < 10000; }
    bool IsZombieChow(Unit* unit) const { return unit && botAI->EqualLowercaseName(unit->GetName(), "zombie chow"); }

private:
    static constexpr uint32 DecimateWindowMs = 15000;

    void Reset()
    {
        _unit = nullptr;
        _combat_start_ms = 0;
        _decimate_cast_ms = 0;
    }

    Unit* _unit = nullptr;
    uint32 _combat_start_ms = 0;
    uint32 _decimate_cast_ms = 0;
};

// The eruption schedule carries no RNG, so the safe zone is a pure function of the phase start.
// The boss never casts Eruption itself (the floor GameObjects do) and instance_naxxramas exposes no
// GetData, so a timer model anchored on the phase edge is the only thing the module can observe.
class HeiganBossHelper : public AiObject
{
public:
    const std::pair<float, float> platform = {2794.26f, -3706.67f};
    // Platform and arena sit on different Z-levels; both are explicit so nobody paths to an arena
    // waypoint while still holding the platform Z.
    const float platformZ = 276.54f;
    const float arenaZ = 264.00f;
    // Index i is the core's eruption section 3 - i. Index 0 is safe for the first eruption of every
    // phase; the walk from there is 0,1,2,3,2,1,0,...
    const std::vector<std::pair<float, float>> waypoints = {{2794.88f, -3668.12f},
                                                            {2775.49f, -3674.43f},
                                                            {2762.30f, -3684.59f},
                                                            {2755.99f, -3703.96f}};

    // Ranged hold the ledge through the slow dance, but the ramp down is a long walk and Plague
    // Cloud lands on the ledge one second after Heigan teleports up. Leave well before that.
    static constexpr uint32 LedgeDepartureMs = 12000;

    HeiganBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}

    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
        {
            Reset();
            return false;
        }
        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
        {
            Reset();
        }
        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "heigan the unclean");
            if (!_unit)
            {
                return false;
            }
        }

        Creature* creature = _unit->ToCreature();
        if (!creature)
        {
            return false;
        }

        _state = &PhaseStateFor(_unit);

        uint32 now = getMSTime();
        bool fast = creature->GetReactState() == REACT_PASSIVE || _unit->HasAura(NaxxSpellIds::PlagueCloud);

        if (!_state->phaseKnown || getMSTimeDiff(_state->lastSeenMs, now) > StaleStateMs)
        {
            // Either the first look at this Heigan, or nobody has watched him for a while - the raid
            // wiped or reset, so the old clock says nothing about the phase running now.
            _state->phaseKnown = true;
            _state->fastDance = fast;
            _state->phaseStartMs = now;
            // Only a pull gives a trustworthy anchor. Bots that show up later inherit the clock from
            // whoever was here first, and dance blind if nobody was.
            _state->synced = !fast && _unit->GetHealthPct() > 99.0f;
        }
        else if (fast != _state->fastDance)
        {
            _state->fastDance = fast;
            _state->phaseStartMs = now;
            _state->synced = true;
        }
        _state->lastSeenMs = now;
        return true;
    }

    Unit* GetBoss() const { return _unit; }
    bool IsFastDance() const { return _state && _state->fastDance; }
    bool IsSynced() const { return _state && _state->synced; }
    bool IsOnPlatform() const { return bot->IsWithinDist2d(platform.first, platform.second, PlatformTolerance); }

    // Nothing on the ledge erupts, so it also serves as the parking spot when the phase clock is
    // unknown - but only while Heigan is down in the arena, since he teleports up for the fast dance.
    bool ShouldHoldLedge() const
    {
        if (IsFastDance())
        {
            return false;
        }
        return !IsSynced() || MsUntilNextPhase() > LedgeDepartureMs;
    }

    // Safe waypoint for the *next* eruption, so bots step into the new zone the moment the previous
    // one has landed.
    uint32 SafeIndex() const
    {
        uint32 tick = NextEruptionTick();
        uint32 m = tick % 6;
        return m <= 3 ? m : 6 - m;
    }

    uint32 MsUntilNextEruption() const
    {
        if (!IsSynced())
        {
            return 0;
        }
        uint32 elapsed = getMSTime() - _state->phaseStartMs;
        uint32 first = FirstEruptionMs();
        if (elapsed < first)
        {
            return first - elapsed;
        }
        return PeriodMs() - ((elapsed - first) % PeriodMs());
    }

    uint32 MsUntilNextPhase() const
    {
        if (!IsSynced())
        {
            return 0;
        }
        uint32 length = IsFastDance() ? FastPhaseMs : SlowPhaseMs;
        uint32 elapsed = getMSTime() - _state->phaseStartMs;
        return elapsed >= length ? 0 : length - elapsed;
    }

private:
    static constexpr uint32 SlowFirstEruptionMs = 15000;
    static constexpr uint32 SlowPeriodMs = 10000;
    static constexpr uint32 SlowPhaseMs = 90000;
    static constexpr uint32 FastFirstEruptionMs = 7000;
    static constexpr uint32 FastPeriodMs = 4000;
    static constexpr uint32 FastPhaseMs = 45000;
    // Hold the old zone briefly after an eruption so its damage has landed before anyone steps.
    static constexpr uint32 StepDelayMs = 250;
    static constexpr float PlatformTolerance = 3.0f;
    // During a live fight the bots poll this several times a second, so a gap this long means combat
    // stopped and the clock has to be re-anchored.
    static constexpr uint32 StaleStateMs = 10000;

    struct PhaseState
    {
        ObjectGuid bossGuid;
        bool phaseKnown = false;
        bool fastDance = false;
        bool synced = false;
        uint32 phaseStartMs = 0;
        uint32 lastSeenMs = 0;
    };

    // One clock per Heigan, shared by every bot and by every trigger/action/multiplier holding a
    // helper. The eruption schedule is raid-wide, so a bot that battle-rezzed or arrived after the
    // pull can pick up an anchor somebody else already established instead of dancing blind. Keeping
    // the clock outside the helpers also stops a bot's own actions from disagreeing about the phase.
    static PhaseState& PhaseStateFor(Unit* boss)
    {
        // Instances update on parallel map threads, so the container lookup needs guarding. The state
        // itself is only ever touched by the map thread that owns the instance, and unordered_map
        // nodes keep their address across rehashes.
        static std::mutex mutex;
        static std::unordered_map<uint32, PhaseState> states;

        std::lock_guard<std::mutex> guard(mutex);
        PhaseState& state = states[boss->GetInstanceId()];
        if (state.bossGuid != boss->GetGUID())
        {
            state = PhaseState();
            state.bossGuid = boss->GetGUID();
        }
        return state;
    }

    uint32 FirstEruptionMs() const { return IsFastDance() ? FastFirstEruptionMs : SlowFirstEruptionMs; }
    uint32 PeriodMs() const { return IsFastDance() ? FastPeriodMs : SlowPeriodMs; }

    uint32 NextEruptionTick() const
    {
        if (!IsSynced())
        {
            return 0;
        }
        uint32 elapsed = getMSTime() - _state->phaseStartMs;
        uint32 base = FirstEruptionMs() + StepDelayMs;
        return elapsed < base ? 0 : 1 + (elapsed - base) / PeriodMs();
    }

    void Reset()
    {
        _unit = nullptr;
        _state = nullptr;
    }

    Unit* _unit = nullptr;
    PhaseState* _state = nullptr;
};

class LoathebBossHelper : public AiObject
{
public:
    const std::pair<float, float> mainTankPos = {2910.1597f, -4010.0f};
    const std::pair<float, float> rangePos = {2896.96f, -3980.61f};
    LoathebBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    Unit* GetBoss() const { return _unit; }
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
        {
            Reset();
        }
        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
        {
            Reset();
        }
        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "loatheb");
        }
        return _unit != nullptr;
    }

private:
    void Reset() { _unit = nullptr; }

    Unit* _unit = nullptr;
};

// The raid fights the whole encounter on the living side, so nothing here assigns sides - it only
// answers "may I hit the boss yet" and "which add comes next on my side". boss_gothikAI is declared
// inside boss_gothik.cpp, so GenericBossHelper cannot reach its EventMap and the phase is read off
// the unit flag the script sets instead.
class GothikBossHelper : public AiObject
{
public:
    // PosGroundLivingSide in boss_gothik.cpp.
    static constexpr float LivingHoldX = 2691.2f;
    static constexpr float LivingHoldY = -3387.0f;
    static constexpr float ArenaFloorZ = 267.68f;

    GothikBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}

    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
        {
            Reset();
        }
        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
        {
            Reset();
        }
        if (!_unit)
        {
            // JustEngagedWith calls SetInCombatWithZone(), so he is on everyone's threat list from the
            // pull onward even while he is REACT_PASSIVE up on the balcony.
            _unit = AI_VALUE2(Unit*, "find target", "gothik the harvester");
        }
        if (!_unit)
        {
            // A bot that battle-rezzed or arrived after the pull never made that threat list.
            _unit = GetFirstAliveUnitByEntry(botAI, NaxxSpellIds::GothikEntry);
        }
        return _unit != nullptr;
    }

    static bool IsLiveSide(WorldObject const* who) { return who && who->GetPositionY() < NaxxSpellIds::GothikGateY; }

    Unit* GetBoss() const { return _unit; }

    // Set on pull, removed when the 24-wave table runs out (boss_gothik.cpp:232, :481).
    bool IsPhaseTwo() const { return _unit && !_unit->HasUnitFlag(UNIT_FLAG_DISABLE_MOVE); }

    bool IsOnBalcony() const { return _unit && _unit->GetPositionZ() > NaxxSpellIds::GothikBalconyZ; }

    // He takes no damage before phase 2, and while the gate is shut anything on the other side is out
    // of reach anyway - in phase 2 he teleports between the sides every 20s.
    bool IsBossAttackable() const { return IsPhaseTwo() && !IsOnBalcony() && IsLiveSide(_unit) == IsLiveSide(bot); }

    static uint32 GetAddPriority(uint32 entry)
    {
        switch (entry)
        {
            // Drain Life heals it and Unholy Frenzy snowballs the rest of the wave.
            case NaxxSpellIds::GothikDeadRiderEntry:
                return 70;
            // Shadow Bolt Volley is the biggest raid damage in the wave phase.
            case NaxxSpellIds::GothikLivingRiderEntry:
                return 60;
            case NaxxSpellIds::GothikDeadKnightEntry:
                return 50;
            case NaxxSpellIds::GothikDeadHorseEntry:
                return 40;
            case NaxxSpellIds::GothikLivingKnightEntry:
                return 30;
            case NaxxSpellIds::GothikDeadTraineeEntry:
                return 20;
            case NaxxSpellIds::GothikLivingTraineeEntry:
                return 10;
            default:
                return 0;
        }
    }

    // Highest priority alive add on the bot's own side, nearest first on a tie. The no-los list is
    // deliberate: the gate wall hides adds the raid is about to inherit, and the side test is on
    // coordinates, so an add that walks over once the gate opens becomes a target on its own.
    Unit* GetBestAdd()
    {
        bool const myLiveSide = IsLiveSide(bot);
        GuidVector candidates = context->GetValue<GuidVector>("possible targets no los")->Get();

        Unit* best = nullptr;
        uint32 bestPriority = 0;
        float bestDistance = 0.0f;

        for (ObjectGuid const& guid : candidates)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive())
            {
                continue;
            }

            uint32 priority = GetAddPriority(unit->GetEntry());
            if (!priority || IsLiveSide(unit) != myLiveSide)
            {
                continue;
            }

            float distance = bot->GetDistance(unit);
            if (!best || priority > bestPriority || (priority == bestPriority && distance < bestDistance))
            {
                best = unit;
                bestPriority = priority;
                bestDistance = distance;
            }
        }
        return best;
    }

private:
    void Reset() { _unit = nullptr; }

    Unit* _unit = nullptr;
};

// Impale picks a uniformly random living player - the tank included - and hurts everything around
// the victim. Nobody can dodge that, so the raid just stands apart and eats it; the only thing worth
// tracking here is Locust Swarm, and its aura gives a clean edge to read the 90s cycle from.
class AnubrekhanBossHelper : public AiObject
{
public:
    static constexpr uint32 NPC_CRYPT_GUARD = 16573;
    static constexpr uint32 NPC_CORPSE_SCARAB = 16698;

    // Room geometry. The kite circle is centred on the room, not on the boss spawn point.
    static constexpr float RoomCenterX = 3272.49f;
    static constexpr float RoomCenterY = -3476.27f;
    // The floor is flat, and bots that inherit their own Z end up pathing into the walls.
    static constexpr float RoomFloorZ = 287.08f;
    // The raid stacks inside the kite circle during the swarm, so this is what sets boss-to-raid
    // distance for the whole window - drop it if healers start falling short of the tank.
    static constexpr float KiteRadius = 45.0f;
    // The swarm pile sits on the room centre itself. Anchoring it to the boss meant the pile trailed
    // a moving point and he swept through it; the centre is a fixed KiteRadius away from him for the
    // whole window, and the raid is standing on it before the aura goes up.
    // Bodies block each other on a single point. This is a de-clump ring, not a spread.
    static constexpr float SwarmStackRingRadius = 5.0f;
    // Locust Swarm reaches ~15 yd, so the ring starts outside that and ends inside cast/heal range.
    static constexpr float RangedBandMin = 20.0f;
    static constexpr float RangedBandMax = 28.0f;
    static constexpr float SlotTolerance = 3.0f;
    static constexpr float AddHoldDistance = 15.0f;
    // Has to clear KiteRadius, or the cap would drag the hold point back inside the boss and onto the
    // ranged. A Crypt Guard spawns at r=58.7, so there is floor out here, and the encounter's leash
    // circle is 77 yd.
    static constexpr float MaxHoldRadius = 52.0f;
    static constexpr uint32 RepositionIntervalMs = 1000;
    static constexpr uint32 SwarmWarningMs = 3000;

    struct SlotState
    {
        uint32 lastMoveMs = 0;
        float destX = 0.0f;
        float destY = 0.0f;
        bool hasDest = false;
        // The spread slot as an offset from the boss, latched on first use. Keeping the angle rather
        // than a world point is what stops a bot being thrown to the far side of the ring when the
        // boss-to-room-centre bearing swings.
        bool hasSpread = false;
        float spreadAngle = 0.0f;
        float spreadRadius = 0.0f;
    };

    AnubrekhanBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}

    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
        {
            Reset();
            return false;
        }
        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
        {
            Reset();
        }
        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "anub'rekhan");
            if (!_unit)
            {
                return false;
            }
        }

        _state = &EncounterStateFor(_unit);
        uint32 now = getMSTime();
        if (!_state->clockKnown || getMSTimeDiff(_state->lastSeenMs, now) > StaleStateMs)
        {
            // Either the first look at this Anub'Rekhan, or nobody has watched him for a while - the
            // raid wiped or reset, so the old anchor says nothing about the pull running now.
            _state->clockKnown = true;
            // The swarm anchor belongs to the attempt that set it, so it goes with the clock.
            _state->swarmSeen = false;
            _state->swarmActive = false;
            _state->lastSwarmStartMs = 0;
        }
        _state->lastSeenMs = now;

        // The core rolls the first swarm anywhere in 70-120s after the pull (boss_anubrekhan.cpp,
        // JustEngagedWith), so it cannot be predicted - but every later one is exactly 90s after the
        // last, and the aura gives a clean edge to anchor on.
        bool swarmActive = IsLocustSwarmActive();
        if (swarmActive && !_state->swarmActive)
        {
            _state->lastSwarmStartMs = now;
            _state->swarmSeen = true;
        }
        _state->swarmActive = swarmActive;
        return true;
    }

    Unit* GetBoss() const { return _unit; }

    // The one place the Locust auras get checked.
    bool IsLocustSwarmActive() const
    {
        if (!_unit)
        {
            return false;
        }
        return NaxxSpellIds::HasAnyAura(_unit, {NaxxSpellIds::LocustSwarm10, NaxxSpellIds::LocustSwarm25}) ||
               botAI->HasAura("locust swarm", _unit);
    }

    // 0 until a swarm has actually been seen - the opening cast is not predictable.
    uint32 MsUntilNextSwarm() const
    {
        if (!_state || !_state->swarmSeen)
        {
            return 0;
        }
        return SwarmPeriodMs - (getMSTimeDiff(_state->lastSwarmStartMs, getMSTime()) % SwarmPeriodMs);
    }

    bool IsSwarmImminent() const
    {
        return _state && _state->swarmSeen && !IsLocustSwarmActive() && MsUntilNextSwarm() <= SwarmWarningMs;
    }

    // The single gate the formation, the kite and the multiplier all read, so they cannot disagree
    // about which shape the raid is in.
    bool IsSwarmFormation() const { return IsLocustSwarmActive() || IsSwarmImminent(); }

    // Both lists are living units only, matched on entry id, sorted by GUID so every bot sees the
    // same order and they stop trading targets between ticks.
    std::vector<Unit*> GetCryptGuards() { return GetLivingAttackersByEntry(NPC_CRYPT_GUARD); }
    std::vector<Unit*> GetCorpseScarabs() { return GetLivingAttackersByEntry(NPC_CORPSE_SCARAB); }

    // Per-bot movement bookkeeping, shared by every action holding a helper so they cannot disagree
    // about where this bot was last sent.
    static SlotState& SlotStateFor(ObjectGuid guid)
    {
        static std::mutex mutex;
        static std::unordered_map<ObjectGuid, SlotState> states;

        std::lock_guard<std::mutex> guard(mutex);
        return states[guid];
    }

private:
    static constexpr uint32 SwarmPeriodMs = 90000;
    static constexpr uint32 StaleStateMs = 10000;

    struct EncounterState
    {
        ObjectGuid bossGuid;
        bool clockKnown = false;
        uint32 lastSeenMs = 0;
        // Anchored on the first swarm the raid actually sees, not on the pull.
        bool swarmSeen = false;
        bool swarmActive = false;
        uint32 lastSwarmStartMs = 0;
    };

    // One clock per Anub'Rekhan, shared by every bot in the instance.
    static EncounterState& EncounterStateFor(Unit* boss)
    {
        // Instances update on parallel map threads, so the container lookup needs guarding. The state
        // itself is only ever touched by the map thread that owns the instance, and unordered_map
        // nodes keep their address across rehashes.
        static std::mutex mutex;
        static std::unordered_map<uint32, EncounterState> states;

        std::lock_guard<std::mutex> guard(mutex);
        EncounterState& state = states[boss->GetInstanceId()];
        if (state.bossGuid != boss->GetGUID())
        {
            state = EncounterState();
            state.bossGuid = boss->GetGUID();
        }
        return state;
    }

    std::vector<Unit*> GetLivingAttackersByEntry(uint32 entry)
    {
        std::vector<Unit*> result;
        GuidVector attackers = AI_VALUE(GuidVector, "attackers");
        for (ObjectGuid const& guid : attackers)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive())
            {
                continue;
            }
            Creature* creature = unit->ToCreature();
            if (creature && creature->GetEntry() == entry)
            {
                result.push_back(unit);
            }
        }
        std::sort(result.begin(), result.end(), [](Unit* left, Unit* right)
                  { return left->GetGUID() < right->GetGUID(); });
        return result;
    }

    void Reset()
    {
        // The spread angle and the last destination are latched for a whole attempt, so they have to
        // die with it - kept across a wipe, they point at wherever the boss stood on the last pull.
        if (_unit || _state)
        {
            SlotStateFor(bot->GetGUID()) = SlotState();
        }

        _unit = nullptr;
        _state = nullptr;
    }

    Unit* _unit = nullptr;
    EncounterState* _state = nullptr;
};

// Curse of the Plaguebringer is the only curse in the encounter, and the two classes that can strip
// it are the two that can also be told apart cheaply here.
inline bool NaxxCanDispelCurse(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || !bot->IsAlive())
    {
        return false;
    }

    switch (bot->getClass())
    {
        case CLASS_MAGE:
        case CLASS_DRUID:
            return botAI->CanCastSpell("remove curse", bot);
        case CLASS_SHAMAN:
            return botAI->CanCastSpell("cleanse spirit", bot);
        default:
            return false;
    }
}

// Whether a group member is one of the decursers at all. Deliberately asks what the member knows
// rather than what it could cast this instant: decursers split the cursed list by index, so a
// cooldown or an empty mana bar must not renumber everyone mid-fight.
inline bool NaxxIsCurseDispeller(Player* member)
{
    PlayerbotAI* memberAI = member && member->IsAlive() ? GET_PLAYERBOT_AI(member) : nullptr;
    if (!memberAI)
    {
        return false;
    }

    std::string spell;
    switch (member->getClass())
    {
        case CLASS_MAGE:
        case CLASS_DRUID:
            spell = "remove curse";
            break;
        case CLASS_SHAMAN:
            spell = "cleanse spirit";
            break;
        default:
            return false;
    }
    return memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get() != 0;
}

class NothBossHelper : public AiObject
{
public:
    const std::pair<float, float> center = {2684.94f, -3502.53f};

    // The encounter's RectangleBoundary, pulled in a few yards, plus a radius well short of the 80 yd
    // at which boss_noth.cpp IsInRoom() forces an evade.
    static constexpr float ROOM_MIN_X = 2623.0f;
    static constexpr float ROOM_MAX_X = 2749.0f;
    static constexpr float ROOM_MIN_Y = -3552.0f;
    static constexpr float ROOM_MAX_Y = -3455.0f;
    static constexpr float ROOM_MAX_RADIUS = 60.0f;

    NothBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}

    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
        {
            Reset();
        }
        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
        {
            Reset();
        }
        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "noth the plaguebringer");
        }
        if (!_unit)
        {
            _state = nullptr;
            return false;
        }

        _state = &PhaseStateFor(_unit);

        uint32 now = getMSTime();
        bool balcony = _unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE);

        if (!_state->phaseKnown || getMSTimeDiff(_state->lastSeenMs, now) > StaleStateMs)
        {
            // First look at this Noth, or nobody has watched him for long enough that the old clock
            // says nothing about the phase running now.
            _state->phaseKnown = true;
            _state->balcony = balcony;
            _state->phaseStartMs = now;
            _state->crippleMs = 0;
            // Only a pull anchors the ground clock. Bots arriving later inherit whatever anchor
            // somebody else established, and fall back to watching for Cripple if there is none.
            _state->synced = !balcony && _unit->GetHealthPct() > 99.0f;
        }
        else if (balcony != _state->balcony)
        {
            _state->balcony = balcony;
            _state->phaseStartMs = now;
            _state->crippleMs = 0;
            _state->synced = true;
        }
        _state->lastSeenMs = now;

        if (!balcony && IsCastingCripple())
        {
            _state->crippleMs = now;
        }
        return true;
    }

    Unit* GetBoss() const { return _unit; }

    bool IsBalconyPhase() const { return _unit && _unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE); }

    bool Is25Man() const { return _unit && _unit->GetMap() && _unit->GetMap()->Is25ManRaid(); }

    // EVENT_BLINK is 25-man only. It runs DoResetThreatList() on everyone, hard-casts Cripple and
    // then Blinks, so for a moment the boss belongs to whoever topped the (now empty) table.
    bool IsBlinkWindow() const
    {
        if (!_state || !Is25Man() || IsBalconyPhase())
        {
            return false;
        }

        uint32 now = getMSTime();
        if (_state->crippleMs && getMSTimeDiff(_state->crippleMs, now) < BlinkWindowMs)
        {
            return true;
        }

        // Blink itself is cast triggered and instant, so it is never observable - the timer is the
        // only thing that catches the reset before the raid has already pulled the boss off the tank.
        if (!_state->synced)
        {
            return false;
        }

        uint32 elapsed = getMSTimeDiff(_state->phaseStartMs, now) + BlinkLeadMs;
        if (elapsed < FirstBlinkMs)
        {
            return false;
        }
        return (elapsed - FirstBlinkMs) % BlinkPeriodMs < BlinkLeadMs + BlinkWindowMs;
    }

    bool IsWarrior(Unit* unit) const { return IsAddOfEntry(unit, NaxxSpellIds::NothPlaguedWarriorEntry, "plagued warrior"); }
    bool IsChampion(Unit* unit) const { return IsAddOfEntry(unit, NaxxSpellIds::NothPlaguedChampionEntry, "plagued champion"); }
    bool IsGuardian(Unit* unit) const { return IsAddOfEntry(unit, NaxxSpellIds::NothPlaguedGuardianEntry, "plagued guardian"); }

    bool IsAdd(Unit* unit) const { return IsWarrior(unit) || IsChampion(unit) || IsGuardian(unit); }

    bool HasCurse(Unit* member) const
    {
        if (!_unit || !member)
        {
            return false;
        }

        Unit::VisibleAuraMap const* auras = member->GetVisibleAuras();
        if (!auras)
        {
            return false;
        }

        for (auto const& slot : *auras)
        {
            AuraApplication* application = slot.second;
            if (!application)
            {
                continue;
            }
            Aura* aura = application->GetBase();
            if (!aura || aura->IsRemoved() || aura->GetCasterGUID() != _unit->GetGUID())
            {
                continue;
            }
            // Matching the dispel type rather than a spell id keeps this working in 25-man, where the
            // curse is a different DBC entry.
            SpellInfo const* info = aura->GetSpellInfo();
            if (info && info->Dispel == DISPEL_CURSE)
            {
                return true;
            }
        }
        return false;
    }

    // Ordered tanks -> healers -> everyone else, so decursers splitting the list by index all agree
    // on who comes first.
    std::vector<Player*> GetCursedMembers() const
    {
        std::vector<Player*> cursed;
        std::vector<Player*> healers;
        std::vector<Player*> others;

        Group* group = bot->GetGroup();
        if (!group)
        {
            return cursed;
        }

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || !HasCurse(member))
            {
                continue;
            }
            if (botAI->IsTank(member))
            {
                cursed.push_back(member);
            }
            else if (botAI->IsHeal(member))
            {
                healers.push_back(member);
            }
            else
            {
                others.push_back(member);
            }
        }

        cursed.insert(cursed.end(), healers.begin(), healers.end());
        cursed.insert(cursed.end(), others.begin(), others.end());
        return cursed;
    }

    Player* GetAliveAssistTank() const
    {
        Group* group = bot->GetGroup();
        if (!group)
        {
            return nullptr;
        }

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->IsAlive() && botAI->IsAssistTank(member))
            {
                return member;
            }
        }
        return nullptr;
    }

    // Who add duty falls to - the same rule "noth choose target" uses: the assist tank, or the main
    // tank when no assist tank is alive.
    Player* GetAliveAddTank() const
    {
        if (Player* assist = GetAliveAssistTank())
        {
            return assist;
        }

        Group* group = bot->GetGroup();
        if (!group)
        {
            return nullptr;
        }

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->IsAlive() && botAI->IsMainTank(member))
            {
                return member;
            }
        }
        return nullptr;
    }

    void ClampToRoom(float& x, float& y) const
    {
        x = std::clamp(x, ROOM_MIN_X, ROOM_MAX_X);
        y = std::clamp(y, ROOM_MIN_Y, ROOM_MAX_Y);

        float dx = x - center.first;
        float dy = y - center.second;
        float r = std::sqrt(dx * dx + dy * dy);
        if (r > ROOM_MAX_RADIUS)
        {
            x = center.first + dx / r * ROOM_MAX_RADIUS;
            y = center.second + dy / r * ROOM_MAX_RADIUS;
        }
    }

private:
    // Ground phase: EVENT_BLINK at 26s, repeating every 30s.
    static constexpr uint32 FirstBlinkMs = 26000;
    static constexpr uint32 BlinkPeriodMs = 30000;
    // How long the raid holds off after the reset while the tank re-establishes.
    static constexpr uint32 BlinkWindowMs = 4000;
    // EventMap only runs one event per boss tick, so the schedule drifts; start suppressing early.
    static constexpr uint32 BlinkLeadMs = 1500;
    static constexpr uint32 StaleStateMs = 10000;

    struct PhaseState
    {
        ObjectGuid bossGuid;
        bool phaseKnown = false;
        bool balcony = false;
        bool synced = false;
        uint32 phaseStartMs = 0;
        uint32 lastSeenMs = 0;
        uint32 crippleMs = 0;
    };

    // One clock per Noth, shared by every bot and by every trigger/action/multiplier holding a
    // helper, so a bot that battle-rezzed or arrived after the pull inherits an anchor instead of
    // running blind, and a bot's own actions cannot disagree about the phase.
    static PhaseState& PhaseStateFor(Unit* boss)
    {
        // Instances update on parallel map threads, so the container lookup needs guarding. The state
        // itself is only ever touched by the map thread that owns the instance, and unordered_map
        // nodes keep their address across rehashes.
        static std::mutex mutex;
        static std::unordered_map<uint32, PhaseState> states;

        std::lock_guard<std::mutex> guard(mutex);
        PhaseState& state = states[boss->GetInstanceId()];
        if (state.bossGuid != boss->GetGUID())
        {
            state = PhaseState();
            state.bossGuid = boss->GetGUID();
        }
        return state;
    }

    bool IsAddOfEntry(Unit* unit, uint32 entry, char const* name) const
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature)
        {
            return false;
        }

        switch (creature->GetEntry())
        {
            case NaxxSpellIds::NothPlaguedWarriorEntry:
            case NaxxSpellIds::NothPlaguedChampionEntry:
            case NaxxSpellIds::NothPlaguedGuardianEntry:
                return creature->GetEntry() == entry;
            default:
                // Reskinned spawns keep the name even when the entry does not. Only anything that is
                // not already a known add pays for the compare - up to a dozen of them are alive at
                // once on the third balcony.
                return botAI->EqualLowercaseName(creature->GetName(), name);
        }
    }

    // Cripple is the one part of the blink sequence that is hard-cast, so it is the only piece of it
    // bots can actually see.
    bool IsCastingCripple() const
    {
        Spell* spell = _unit->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell)
        {
            spell = _unit->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        }
        if (!spell)
        {
            return false;
        }

        SpellInfo const* info = spell->GetSpellInfo();
        if (NaxxSpellIds::MatchesAnySpellId(info, {NaxxSpellIds::Cripple}))
        {
            return true;
        }
        return info && info->SpellName[LOCALE_enUS] &&
               botAI->EqualLowercaseName(info->SpellName[LOCALE_enUS], "cripple");
    }

    void Reset()
    {
        _unit = nullptr;
        _state = nullptr;
    }

    Unit* _unit = nullptr;
    PhaseState* _state = nullptr;
};

class FourhorsemanBossHelper : public AiObject
{
public:
    const float posZ = 241.27f;
    const std::pair<float, float> attractPos[2] = {{2502.03f, -2910.90f},
                                                   {2484.61f, -2947.07f}};  // left (sir zeliek), right (lady blaumeux)
    FourhorsemanBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
        {
            Reset();
        }
        else if (_combat_start_ms == 0)
        {
            _combat_start_ms = getMSTime();
        }
        if (_sir && (!_sir->IsInWorld() || !_sir->IsAlive()))
        {
            Reset();
        }
        if (!_sir)
        {
            _sir = AI_VALUE2(Unit*, "find target", "sir zeliek");
            if (!_sir)
            {
                return false;
            }
        }
        _lady = AI_VALUE2(Unit*, "find target", "lady blaumeux");
        return true;
    }
    void Reset()
    {
        _sir = nullptr;
        _lady = nullptr;
        _combat_start_ms = 0;
        posToGo = 0;
    }
    // UpdateBossAI needs Zeliek, and "find target" only sees creatures that already have this bot on
    // their threat list - a melee bot parked on Thane never resolves him. Anything that only needs
    // "the encounter is running" takes any of the four instead.
    bool IsEncounterUp()
    {
        if (UpdateBossAI())
        {
            return true;
        }
        for (char const* name : {"lady blaumeux", "thane korth'azz", "baron rivendare", "highlord mograine"})
        {
            if (AI_VALUE2(Unit*, "find target", name))
            {
                return true;
            }
        }
        return false;
    }
    bool JustStartCombat() const { return _combat_start_ms != 0 && getMSTime() - _combat_start_ms < PullWindowMs; }
    bool IsAttracter(Player* bot)
    {
        Difficulty diff = bot->GetRaidDifficulty();
        if (diff == RAID_DIFFICULTY_25MAN_NORMAL)
        {
            return botAI->IsAssistRangedDpsOfIndex(bot, 0) || botAI->IsAssistHealOfIndex(bot, 0) ||
                   botAI->IsAssistHealOfIndex(bot, 1) || botAI->IsAssistHealOfIndex(bot, 2);
        }
        return botAI->IsAssistRangedDpsOfIndex(bot, 0) || botAI->IsAssistHealOfIndex(bot, 0);
    }
    void CalculatePosToGo(Player* bot)
    {
        bool raid25 = bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL;
        Unit* lady = _lady;
        if (!lady)
        {
            posToGo = 0;
        }
        else
        {
            uint32 elapsed_ms = _combat_start_ms ? getMSTime() - _combat_start_ms : 0;
            // Interval: 24s - 15s - 15s - ...
            posToGo = !(elapsed_ms <= 9000 || ((elapsed_ms - 9000) / 67500) % 2 == 0);
            if (botAI->IsAssistRangedDpsOfIndex(bot, 0) || (raid25 && botAI->IsAssistHealOfIndex(bot, 1)))
            {
                posToGo = 1 - posToGo;
            }
        }
    }
    std::pair<float, float> CurrentAttractPos()
    {
        bool raid25 = bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL;
        float posX = attractPos[posToGo].first, posY = attractPos[posToGo].second;
        if (posToGo == 1)
        {
            float offset_x = 0.0f;
            float offset_y = 0.0f;
            float bias = 4.5f;
            if (raid25)
            {
                offset_x = -bias;
                offset_y = bias;
            }
            posX += offset_x;
            posY += offset_y;
        }
        return {posX, posY};
    }
    Unit* CurrentAttackTarget()
    {
        if (posToGo == 0)
        {
            return _sir;
        }
        return _lady;
    }

protected:
    // How long a pull lasts for anything that only makes sense before the attractor rotation
    // starts moving the horsemen around.
    static constexpr uint32 PullWindowMs = 10000;

    Unit* _sir = nullptr;
    Unit* _lady = nullptr;
    uint32 _combat_start_ms = 0;
    int posToGo = 0;
};
class ThaddiusBossHelper : public AiObject
{
public:
    const std::pair<float, float> tankPosFeugen = {3522.94f, -3002.60f};
    const std::pair<float, float> tankPosStalagg = {3436.14f, -2919.98f};
    const std::pair<float, float> rangedPosFeugen = {3500.45f, -2997.92f};
    const std::pair<float, float> rangedPosStalagg = {3441.01f, -2942.04f};
    const float tankPosZ = 312.61f;

    // RTI indices in WotLK: 0 star, 1 circle, 2 diamond, 3 triangle, 4 moon, 5 square, 6 cross, 7 skull
    static constexpr uint8 RAID_ICON_SQUARE = 5;
    static constexpr uint8 RAID_ICON_CROSS = 6;
    static constexpr uint8 RAID_ICON_SKULL = 7;

    static constexpr uint32 NPC_STALAGG = 15929;
    static constexpr uint32 NPC_FEUGEN  = 15930;

    ThaddiusBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        // Phase-1 logic relies on stable pet pointers (Feugen/Stalagg).
        // Keep them updated even if the boss ("thaddius") is not yet a valid target.

        if (!bot->IsInCombat())
        {
            Reset();
        }
        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
        {
            Reset();
        }
        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "thaddius");
        }

        // Try to resolve pets by name first (normal case).
        feugen = AI_VALUE2(Unit*, "find target", "feugen");
        stalagg = AI_VALUE2(Unit*, "find target", "stalagg");

        // Fallback: resolve pets from RTI icons (works even when "find target" is not yet available at pull).
        auto ResolveFromIcon = [&](uint8 icon)
        {
            Unit* u = GetMarkedUnitRaw(icon);
            if (!u)
                return;

            if (Creature const* c = u->ToCreature())
            {
                if (!feugen && c->GetEntry() == NPC_FEUGEN)
                    feugen = u;
                if (!stalagg && c->GetEntry() == NPC_STALAGG)
                    stalagg = u;
            }

            // Safety fallback (should not be needed in retail data, but keeps it resilient).
            if (!feugen && botAI->EqualLowercaseName(u->GetName(), "feugen"))
                feugen = u;
            if (!stalagg && botAI->EqualLowercaseName(u->GetName(), "stalagg"))
                stalagg = u;
        };

        ResolveFromIcon(RAID_ICON_SKULL);
        ResolveFromIcon(RAID_ICON_CROSS);
        ResolveFromIcon(RAID_ICON_SQUARE);

        // Consider the helper "available" as soon as we have the boss OR at least one pet.
        return _unit != nullptr || feugen != nullptr || stalagg != nullptr;
    }
    // Both pets feign death on their "kill" and only really die 12s later, when Thaddius'
    // overload finishes them off - so the pet phase has to end on the feign, not on IsAlive().
    bool IsPhasePet() { return !IsDownOrFeigning(feugen) || !IsDownOrFeigning(stalagg); }
    bool IsPhaseTransition()
    {
        if (IsPhasePet())
        {
            return false;
        }
        return _unit && _unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
    }
    bool IsPhaseThaddius() { return !IsPhasePet() && !IsPhaseTransition(); }

    Unit* GetBoss() const { return _unit; }
    Unit* GetFeugen() const { return feugen; }
    Unit* GetStalagg() const { return stalagg; }

    // Determine which platform the bot is currently on during phase 1.
    // Tanks are teleported by Magnetic Pull; non-tanks must NOT "follow" them across.
    bool IsOnFeugenSide(Unit const* unit) const
    {
        if (!unit)
            return false;

        float dFeugen = unit->GetDistance2d(tankPosFeugen.first, tankPosFeugen.second);
        float dStalagg = unit->GetDistance2d(tankPosStalagg.first, tankPosStalagg.second);
        return dFeugen < dStalagg;
    }

    Unit* GetNearestPet()
    {
        Unit* unit = nullptr;
        if (!IsDownOrFeigning(feugen))
        {
            unit = feugen;
        }
        if (!IsDownOrFeigning(stalagg) && (!feugen || bot->GetDistance(stalagg) < bot->GetDistance(feugen)))
        {
            unit = stalagg;
        }
        return unit;
    }

    Unit* GetMarkedUnitRaw(uint8 iconIndex)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return nullptr;

        ObjectGuid guid = group->GetTargetIcon(iconIndex);
        if (guid.IsEmpty())
            return nullptr;

        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            return nullptr;

        return unit;
    }

    bool IsPet(Unit const* unit) const
    {
        if (!unit)
            return false;
        if (Creature const* c = unit->ToCreature())
        {
            uint32 entry = c->GetEntry();
            return entry == NPC_FEUGEN || entry == NPC_STALAGG;
        }
        return false;
    }

    // Return the pet marked with the given RTI icon, if it is Stalagg/Feugen and still up.
    Unit* GetMarkedPet(uint8 iconIndex)
    {
        Unit* unit = GetMarkedUnitRaw(iconIndex);
        return IsPet(unit) && !IsDownOrFeigning(unit) ? unit : nullptr;
    }

    // Decide which RTI pair is used for phase 1.
    // Supported setups:
    //  - skull + cross
    //  - cross + square (recommended to avoid skull bias)
    //  - skull + square
    bool GetPetIconPair(uint8& primaryIcon, uint8& secondaryIcon)
    {
        if (GetMarkedPet(RAID_ICON_SKULL) && GetMarkedPet(RAID_ICON_CROSS))
        {
            primaryIcon = RAID_ICON_SKULL;
            secondaryIcon = RAID_ICON_CROSS;
            return true;
        }

        if (GetMarkedPet(RAID_ICON_CROSS) && GetMarkedPet(RAID_ICON_SQUARE))
        {
            primaryIcon = RAID_ICON_CROSS;
            secondaryIcon = RAID_ICON_SQUARE;
            return true;
        }

        if (GetMarkedPet(RAID_ICON_SKULL) && GetMarkedPet(RAID_ICON_SQUARE))
        {
            primaryIcon = RAID_ICON_SKULL;
            secondaryIcon = RAID_ICON_SQUARE;
            return true;
        }

        return false;
    }

    bool HasPetIconPair()
    {
        uint8 primaryIcon = RAID_ICON_SKULL;
        uint8 secondaryIcon = RAID_ICON_CROSS;
        return GetPetIconPair(primaryIcon, secondaryIcon);
    }

    bool IsMainTankEngagedOnPets() const
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive())
                continue;
            if (!botAI->IsTank(member) || !botAI->IsMainTank(member))
                continue;

            Unit* victim = member->GetVictim();
            if (IsPet(victim))
                return true;
        }
        return false;
    }

    bool IsOffTankEngagedOnPets() const
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive())
                continue;
            if (!botAI->IsTank(member) || botAI->IsMainTank(member))
                continue;

            Unit* victim = member->GetVictim();
            if (IsPet(victim))
                return true;
        }
        return false;
    }

    // Called several times per AI tick on a hot combat path, and each call rescans the
    // whole group. Memoize the self result per tick; getMSTime granularity is fine here
    // (a rare ms-boundary miss just recomputes).
    bool IsAssignedToPrimarySide(Player* player)
    {
        if (player != bot)
            return ComputeAssignedToPrimarySide(player);

        uint32 now = getMSTime();
        if (_sideCacheValid && _sideCacheTime == now)
            return _sideCacheValue;

        _sideCacheValue = ComputeAssignedToPrimarySide(player);
        _sideCacheTime = now;
        _sideCacheValid = true;
        return _sideCacheValue;
    }

    bool ComputeAssignedToPrimarySide(Player* player)
    {
        if (!player)
            return true;

        Group* group = bot->GetGroup();

        if (botAI->IsTank(player))
        {
            if (NaxxHasStrategyAnyState(player, "tank face"))
                return true;

            if (NaxxHasStrategyAnyState(player, "tank assist") && !NaxxHasStrategyAnyState(player, "tank face"))
                return false;

            if (botAI->IsMainTank(player))
                return true;

            // Split remaining tanks evenly: first non-main tank -> secondary, then alternate.
            uint32 index = 0;
            if (group)
            {
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                {
                    Player* member = ref->GetSource();
                    if (!member || !member->IsAlive())
                        continue;
                    if (!botAI->IsTank(member) || botAI->IsMainTank(member))
                        continue;
                    if (member == player)
                        return (index % 2) == 1;
                    ++index;
                }
            }

            // Lone/unmatched tank (no group, or own entry transiently skipped): use the
            // same stable parity fallback the heal/DPS branches use instead of pinning
            // to one pet.
            int32 slotIndex = botAI->GetGroupSlotIndex(player);
            if (slotIndex >= 0)
                return (slotIndex % 2) == 1;
            return (player->GetGUID().GetCounter() % 2) == 1;
        }

        // Even 50/50 split per role by group-order index parity, independent of raid size.
        if (botAI->IsHeal(player))
        {
            uint32 index = 0;
            if (group)
            {
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                {
                    Player* member = ref->GetSource();
                    if (!member || !member->IsAlive())
                        continue;

                    if (botAI->IsTank(member) || !botAI->IsHeal(member))
                        continue;

                    if (member == player)
                        return (index % 2) == 0;

                    ++index;
                }
            }
        }

        uint32 index = 0;
        if (group)
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || !member->IsAlive())
                    continue;

                if (botAI->IsTank(member) || botAI->IsHeal(member))
                    continue;

                if (member == player)
                    return (index % 2) == 0;

                ++index;
            }
        }

        int32 slotIndex = botAI->GetGroupSlotIndex(player);
        if (slotIndex >= 0)
            return (slotIndex % 2) == 0;

        return (player->GetGUID().GetCounter() % 2) == 0;
    }

    // Single source of truth for side->pet. When a valid RTI pair is set the marks win
    // (primary = primary-icon pet), so marked and mark-free code paths never disagree
    // within a tick. Mark-free fallback convention: primary = Stalagg, secondary = Feugen.
    // Falls back to the live sibling if the chosen pet is dead.
    Unit* GetPetForSide(bool primary)
    {
        uint8 primaryIcon = RAID_ICON_SKULL;
        uint8 secondaryIcon = RAID_ICON_CROSS;
        if (GetPetIconPair(primaryIcon, secondaryIcon))
        {
            if (Unit* marked = GetMarkedPet(primary ? primaryIcon : secondaryIcon))
                return marked;
        }

        Unit* preferred = primary ? stalagg : feugen;
        Unit* sibling   = primary ? feugen : stalagg;
        if (!IsDownOrFeigning(preferred))
            return preferred;
        if (!IsDownOrFeigning(sibling))
            return sibling;
        return nullptr;
    }

    // Sync window tuning (percent). Balancing only kicks in once a pet enters the low
    // window; above that both burn freely. Inside it, keep both pets dropping through
    // the last few % together so both die inside the ~5s revive window.
    static constexpr float SYNC_WINDOW_PCT = 30.0f;
    static constexpr float SYNC_BALANCE_MARGIN = 5.0f;
    static constexpr float SYNC_HARD_FLOOR_PCT = 5.0f;
    static constexpr float SYNC_FLOOR_RELEASE_PCT = 8.0f;

    // Whether damage on `target` (one of the two pets) should be suppressed to keep
    // both pets converging to death together. Symmetric, margin-based, hard-floored.
    bool PetSyncSuppress(Unit* target)
    {
        if (!target || !feugen || !stalagg)
            return false;
        if (target != feugen && target != stalagg)
            return false;
        // Once one pet is down the window is over: it sits at 1 HP feigning and would drag the
        // sibling's damage to a halt.
        if (IsDownOrFeigning(feugen) || IsDownOrFeigning(stalagg))
            return false;

        float targetPct = target->GetHealthPct();
        Unit* other = (target == feugen) ? stalagg : feugen;
        float otherPct = other->GetHealthPct();

        // Only manage the sync near death: while both pets are healthy, burn freely.
        if (targetPct > SYNC_WINDOW_PCT && otherPct > SYNC_WINDOW_PCT)
            return false;

        // Both within the floor band -> free-burn both to death simultaneously.
        if (targetPct <= SYNC_FLOOR_RELEASE_PCT && otherPct <= SYNC_FLOOR_RELEASE_PCT)
            return false;

        // Hard floor: don't push this pet to 0 while the sibling is still above the band.
        if (targetPct <= SYNC_HARD_FLOOR_PCT)
            return true;

        // Balance hold: this pet is already the lower one -> let the sibling catch up.
        if (targetPct < otherPct - SYNC_BALANCE_MARGIN)
            return true;

        return false;
    }

    Unit* GetAssignedPetForBot()
    {
        bool hasPair = HasPetIconPair();

        if (botAI->IsTank(bot))
        {
            if (hasPair && (!bot->IsInCombat() || (!IsMainTankEngagedOnPets() && !IsOffTankEngagedOnPets())))
            {
                if (Unit* pet = GetPetForSide(IsAssignedToPrimarySide(bot)))
                    return pet;
            }

            Unit* feugenAlive = !IsDownOrFeigning(feugen) ? feugen : nullptr;
            Unit* stalaggAlive = !IsDownOrFeigning(stalagg) ? stalagg : nullptr;

            if (feugenAlive || stalaggAlive)
            {
                float dFeugen = bot->GetDistance2d(tankPosFeugen.first, tankPosFeugen.second);
                float dStalagg = bot->GetDistance2d(tankPosStalagg.first, tankPosStalagg.second);
                if (hasPair && (dFeugen > 45.0f && dStalagg > 45.0f))
                {
                    if (Unit* pet = GetPetForSide(IsAssignedToPrimarySide(bot)))
                        return pet;
                }
                bool onFeugenSide = IsOnFeugenSide(bot);
                Unit* sidePet = onFeugenSide ? feugenAlive : stalaggAlive;
                Unit* otherPet = onFeugenSide ? stalaggAlive : feugenAlive;
                if (sidePet)
                    return sidePet;
                if (otherPet)
                    return otherPet;
            }

            return GetNearestPet();
         }

        // Non-tanks: GetPetForSide resolves marks (when set) or the fixed side mapping,
        // so marked and mark-free assignments always agree within a tick.
        if (Unit* sidePet = GetPetForSide(IsAssignedToPrimarySide(bot)))
            return sidePet;

        return GetNearestPet();
    }

    std::pair<float, float> PetPhaseGetPosForTank(Unit* pet)
    {
        if (pet == feugen)
            return tankPosFeugen;
        return tankPosStalagg;
    }
    std::pair<float, float> PetPhaseGetPosForRanged(Unit* pet)
    {
        if (pet == feugen)
            return rangedPosFeugen;
        return rangedPosStalagg;
    }

protected:
    void Reset()
    {
        _unit = nullptr;
        feugen = nullptr;
        stalagg = nullptr;
        _sideCacheValid = false;
    }

    Unit* _unit = nullptr;
    Unit* feugen = nullptr;
    Unit* stalagg = nullptr;

    // Per-tick memo of IsAssignedToPrimarySide(bot); see that method.
    uint32 _sideCacheTime = 0;
    bool _sideCacheValue = false;
    bool _sideCacheValid = false;
};

#endif
