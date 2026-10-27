/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Sindragosa.h"
#include "Playerbots.h"
#include "RaidObs.h"

IccSindragosaPhaseId IccSindragosaPhase(Player* bot, Unit const* boss)
{
    // Her air phase sets her passive at takeoff and aggressive again at touchdown, and nothing else does.
    Creature const* const creature = boss->ToCreature();
    IccSindragosaPhaseId phase = IccSindragosaPhaseId::Ground;
    if (boss->IsInCombat() && creature && creature->GetReactState() == REACT_PASSIVE)
        phase = IccSindragosaPhaseId::Air;
    else if (!boss->HealthAbovePct(35))
        phase = IccSindragosaPhaseId::Last;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "sg.phase",
                             phase == IccSindragosaPhaseId::Air ? "air"
                             : phase == IccSindragosaPhaseId::Last ? "last" : "ground");

    return phase;
}

bool IccSindragosaOverAirStation(Player* bot, Unit const* boss, float radius)
{
    return IccSindragosaPhase(bot, boss) == IccSindragosaPhaseId::Air &&
           boss->GetExactDist2d(ICC_SINDRAGOSA_FLYING_POSITION.GetPositionX(),
                                ICC_SINDRAGOSA_FLYING_POSITION.GetPositionY()) < radius;
}

bool IccBossCastingBlisteringCold(Unit* boss)
{
    return boss && (boss->FindCurrentSpellBySpellId(SPELL_BLISTERING_COLD1) ||
                    boss->FindCurrentSpellBySpellId(SPELL_BLISTERING_COLD2) ||
                    boss->FindCurrentSpellBySpellId(SPELL_BLISTERING_COLD3) ||
                    boss->FindCurrentSpellBySpellId(SPELL_BLISTERING_COLD4));
}
