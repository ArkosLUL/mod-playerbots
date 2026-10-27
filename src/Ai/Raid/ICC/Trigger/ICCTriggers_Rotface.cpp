/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Rotface.h"
#include "ICCHelpers_Rotface.h"
#include "ICCHelpers_Shared.h"
#include "ICCScripts.h"
#include "Playerbots.h"
#include "Trigger.h"

bool IccRotfaceTankPositionTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "rotface");
    return boss && (PlayerbotAI::IsTank(bot) || PlayerbotAI::IsMelee(bot));
}

bool IccRotfaceGroupPositionTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "rotface");
    if (!boss)
        return false;

    return true;
}

bool IccRotfaceMoveAwayFromExplosionTrigger::IsActive()
{
    Creature* boss = bot->FindNearestCreature(NPC_BIG_OOZE, 100.0f);
    bool castingNow = boss && boss->IsAlive() &&
        boss->HasUnitState(UNIT_STATE_CASTING) && boss->FindCurrentSpellBySpellId(SPELL_UNSTABLE_OOZE_EXPLOSION);

    if (castingNow)
    {
        _wasCasting = true;
        _castEnded = false;
        return true;
    }

    // Cast just ended — record the time
    if (_wasCasting)
    {
        _wasCasting = false;
        _castEnded = true;
        _castEndMs = getMSTime();
    }

    // Stay active for 6 seconds after cast ended (2s wait + return movement)
    if (_castEnded && getMSTimeDiff(_castEndMs, getMSTime()) < 6000)
        return true;

    _castEnded = false;
    return false;
}

bool IccRotfaceAvoidVileGasTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "rotface");
    if (!boss)
        return false;

    uint32 const now = getMSTime();

    IcecrownHelpers::IccInstanceState& st = IcecrownHelpers::IccState(bot->GetMap()->GetInstanceId());
    bool const isVictim =
        st.rotfaceVileGas.victimGuid == bot->GetGUID() &&
        getMSTimeDiff(st.rotfaceVileGas.castTime, now) < 8000;
    if (isVictim)
        return true;

    if (botAI->HasAura("Vile Gas", bot))
        return true;

    auto const& waitMap = st.rotfaceVileGasWaitUntil;
    auto it = waitMap.find(bot->GetGUID());
    if (it != waitMap.end() && now < it->second)
        return true;

    return false;
}
