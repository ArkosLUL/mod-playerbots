/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_BloodPrinces.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <string>

Unit* IccBpcEmpoweredPrince(PlayerbotAI* botAI, bool* present)
{
    struct Prince
    {
        char const* name;
        uint32 invocation;
        char const* phase;
    };
    static constexpr Prince PRINCES[] = {
        {"prince valanar", SPELL_INVOCATION_OF_BLOOD_VALANAR, "valanar"},
        {"prince keleseth", SPELL_INVOCATION_OF_BLOOD_KELESETH, "keleseth"},
        {"prince taldaram", SPELL_INVOCATION_OF_BLOOD_TALDARAM, "taldaram"},
    };

    auto* context = botAI->GetAiObjectContext();
    bool seen = false;
    Unit* empowered = nullptr;
    char const* phase = "none";
    for (Prince const& prince : PRINCES)
    {
        Unit* unit = AI_VALUE2(Unit*, "find target", prince.name);
        if (!unit)
            continue;

        seen = true;
        if (unit->HasAura(prince.invocation))
        {
            empowered = unit;
            phase = prince.phase;
            break;
        }
    }

    if (present)
        *present = seen;
    if (seen && RaidObs::Active())
        RaidObs::NoteDerived(botAI->GetBot(), "bpc.phase", phase);

    return empowered;
}

Unit* IccBpcAssignedKineticBomb(PlayerbotAI* botAI)
{
    static constexpr std::array<uint32, 4> BOMB_ENTRIES = {NPC_KINETIC_BOMB1, NPC_KINETIC_BOMB2, NPC_KINETIC_BOMB3,
                                                           NPC_KINETIC_BOMB4};
    static float const MAX_HEIGHT_DIFF = 35.0f;
    static float const MAX_ASSIGN_RANGE = 80.0f;

    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsRangedDps(bot))
        return nullptr;

    auto* context = botAI->GetAiObjectContext();
    std::vector<Unit*> kineticBombs;
    for (auto const& guid : AI_VALUE(GuidVector, "possible targets no los"))
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (std::find(BOMB_ENTRIES.begin(), BOMB_ENTRIES.end(), unit->GetEntry()) == BOMB_ENTRIES.end())
            continue;

        if (unit->GetPositionZ() - BPC_FLOOR_Z > MAX_HEIGHT_DIFF)
            continue;

        kineticBombs.push_back(unit);
    }

    if (kineticBombs.empty())
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "bpc.bomb", "none");
        return nullptr;
    }

    // lowest first, the next to land
    std::sort(kineticBombs.begin(), kineticBombs.end(),
              [](Unit* unitA, Unit* unitB) { return unitA->GetPositionZ() < unitB->GetPositionZ(); });

    // bots only, a human ranged never gets a bomb
    std::vector<Player*> rangedDps;
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member->IsAlive() && GET_PLAYERBOT_AI(member) && PlayerbotAI::IsRangedDps(member))
                rangedDps.push_back(member);
        }
    }
    else
    {
        rangedDps.push_back(bot);
    }

    static constexpr std::array<uint8, 3> CLASS_PRIORITY = {CLASS_HUNTER, CLASS_DRUID, 0};
    static char const* const TIER_NAMES[] = {"hunter", "druid", "any"};
    std::set<Player*> assigned;
    for (Unit* bomb : kineticBombs)
    {
        for (size_t tier = 0; tier < CLASS_PRIORITY.size(); ++tier)
        {
            Player* nearest = nullptr;
            float nearestDist = std::numeric_limits<float>::max();
            for (Player* dps : rangedDps)
            {
                if (assigned.count(dps))
                    continue;

                if (CLASS_PRIORITY[tier] != 0 && dps->getClass() != CLASS_PRIORITY[tier])
                    continue;

                float dist = dps->GetDistance(bomb);
                if (dist < nearestDist && dist < MAX_ASSIGN_RANGE)
                {
                    nearestDist = dist;
                    nearest = dps;
                }
            }

            if (!nearest)
                continue;

            assigned.insert(nearest);
            if (nearest == bot)
            {
                if (RaidObs::Active())
                    RaidObs::NoteDerived(bot, "bpc.bomb",
                                         std::string(TIER_NAMES[tier]) + " " +
                                             RaidObs::DescribeAssignment(bomb->GetGUID()));
                return bomb;
            }
            break;
        }
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "bpc.bomb", "spare");
    return nullptr;
}
