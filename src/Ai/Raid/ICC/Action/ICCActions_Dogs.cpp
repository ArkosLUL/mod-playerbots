/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_Dogs.h"

#include "Creature.h"
#include "ICCHelpers_Shared.h"
#include "Playerbots.h"
#include "RaidDefensive.h"

bool IccDogsCastTankDefensiveAction::Execute(Event /*event*/)
{
    uint32 const spellId = GetDogsTankDefensive(botAI, "dogs.defensive");
    return spellId && botAI->CastSpell(spellId, bot);
}

bool IccDogsCastDecimateCoverAction::Execute(Event /*event*/)
{
    RaidCover const cover = GetDogsDecimateCover(botAI);
    uint32 const spellId = RaidCoverSpellId(botAI, cover);
    if (!spellId)
        return false;

    // A cast in progress fails this one's check, and a moving bot breaks its own channel
    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    if (IsRaidCoverChannel(cover) && bot->isMoving())
        bot->StopMoving();

    if (!botAI->CastSpell(spellId, bot))
        return false;

    FinishRaidCover(botAI, cover);
    return true;
}

bool IccDogsSwapAction::Execute(Event /*event*/)
{
    Creature* dog = GetDogsBoss(botAI);
    if (!dog || !IsDogsSwapTaunter(botAI) || !IccCastClassTaunt(bot, botAI, dog))
        return false;

    AttackDirect(dog);
    return true;
}

bool IccDogsYieldAction::Execute(Event /*event*/)
{
    Creature* dog = GetDogsBoss(botAI);
    if (!dog)
        return false;

    if (bot->GetVictim() == dog)
        bot->AttackStop();

    if (AI_VALUE(Unit*, "current target") == dog)
        context->GetValue<Unit*>("current target")->Set(nullptr);

    return true;
}
