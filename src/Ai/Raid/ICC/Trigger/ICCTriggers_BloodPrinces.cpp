/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_BloodPrinces.h"
#include "ICCHelpers_BloodPrinces.h"
#include "ICCHelpers_Shared.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "Trigger.h"

bool IccBpcKelesethTankTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "prince keleseth");
    if (!boss)
        return false;

    IccStripExperienced(bot);

    if (!PlayerbotAI::IsAssistTank(bot))
        return false;

    Aura* aura = botAI->GetAura("Shadow Prison", bot, false, true);
    if (aura)
        if (aura->GetStackAmount() > 18)
            return false;

    return true;
}

bool IccBpcMainTankTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    if (!IccAnyBloodPrincePresent(botAI))
        return false;

    IccStripExperienced(bot);

    return true;
}

bool IccBpcEmpoweredVortexTrigger::IsActive()
{
    // Tanks should ignore this mechanic
    if (PlayerbotAI::IsMainTank(bot) || PlayerbotAI::IsAssistTank(bot))
        return false;

    Unit* valanar = AI_VALUE2(Unit*, "find target", "prince valanar");
    if (!valanar)
        return false;

    Aura* aura = botAI->GetAura("Shadow Prison", bot, false, true);
    if (aura)
        if (aura->GetStackAmount() > 12)
            return false;

    if (IccBpcEmpoweredPrince(botAI) != valanar)
    {
        // Ends the spread window, so the next one writes its slot even if it lands on the same one.
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "bpc.spread", "none");
        return false;
    }

    return true;
}

bool IccBpcKineticBombTrigger::IsActive()
{
    if (!IccAnyBloodPrincePresent(botAI))
        return false;

    if (!PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsHeal(bot))
        return false;

    // Allow up to 18 stacks for bomb-assigned bots (multiplier handles assignment)
    if (Aura* aura = botAI->GetAura("Shadow Prison", bot, false, true))
    {
        if (aura->GetStackAmount() > 18)
            return false;
    }

    static const std::array<uint32, 4> bombEntries = {NPC_KINETIC_BOMB1, NPC_KINETIC_BOMB2, NPC_KINETIC_BOMB3,
                                                      NPC_KINETIC_BOMB4};
    const GuidVector bombs = AI_VALUE(GuidVector, "possible targets no los");

    bool bombFound = false;

    for (const auto entry : bombEntries)
    {
        for (auto const& guid : bombs)
        {
            if (Unit* unit = botAI->GetUnit(guid))
            {
                if (unit->GetEntry() == entry)
                {
                    // Check if bomb is within valid Z-axis range
                    if (unit->GetPositionZ() - bot->GetPositionZ() < 35.0f)
                    {
                        bombFound = true;
                        break;
                    }
                }
            }
        }
        if (bombFound)
            break;
    }

    return PlayerbotAI::IsRangedDps(bot) && bombFound;
}

bool IccBpcBallOfFlameTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot))
        return false;

    bool present = false;
    Unit* empowered = IccBpcEmpoweredPrince(botAI, &present);
    if (!present)
        return false;

    if (empowered && empowered->GetEntry() == NPC_PRINCE_TALDARAM)
        return true;

    // A ball chases its victim whoever is empowered, and the multiplier leaves that bot only the ball action.
    for (uint32 entry : {NPC_BALL_OF_FLAME, NPC_BALL_OF_INFERNO_FLAME})
        if (Unit* ball = bot->FindNearestCreature(entry, 100.0f))
            if (ball->GetVictim() == bot)
                return true;

    return false;
}
