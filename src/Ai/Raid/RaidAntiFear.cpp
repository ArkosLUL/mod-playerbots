/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidAntiFear.h"

#include "Group.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "ShamanActions.h"
#include "ShamanTriggers.h"

namespace
{
// CanCastSpell treats SPELL_FAILED_OUT_OF_RANGE as castable, so a target on the far side of the
// instance (or on another map entirely) would otherwise be picked and never resolve.
bool CanWardTarget(PlayerbotAI* botAI, Player* bot, Player* target)
{
    return target && target->IsAlive() && !target->HasAura(SPELL_FEAR_WARD) &&
           bot->IsWithinDistInMap(target, sPlayerbotAIConfig.spellDistance) &&
           botAI->CanCastSpell("fear ward", target);
}
}  // namespace

Player* GetAntiFearWardTarget(PlayerbotAI* botAI, Player* bot)
{
    if (bot->getClass() != CLASS_PRIEST)
        return nullptr;

    // Cheap gate before the group walk: this runs from the trigger, isUseful and Execute on every
    // tick, and each CanCastSpell below builds a Spell and runs CheckCast.
    uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", "fear ward")->Get();
    if (!spellId || bot->HasSpellCooldown(spellId))
        return nullptr;

    Player* mainTank = GetGroupMainTank(botAI, bot);
    if (CanWardTarget(botAI, bot, mainTank))
        return mainTank;

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == mainTank || !botAI->IsHeal(member))
            continue;

        if (CanWardTarget(botAI, bot, member))
            return member;
    }

    return nullptr;
}

bool ShouldDropTremorTotem(PlayerbotAI* botAI, Player* bot)
{
    if (bot->getClass() != CLASS_SHAMAN || !bot->HasSpell(SPELL_TREMOR_TOTEM_RANK_1))
        return false;

    if (botAI->GetAiObjectContext()->GetValue<bool>("has totem", "tremor totem")->Get())
        return false;

    return botAI->CanCastSpell("tremor totem", bot);
}

bool RaidAntiFearReady(PlayerbotAI* botAI, Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_PRIEST:
            return GetAntiFearWardTarget(botAI, bot) != nullptr;
        case CLASS_SHAMAN:
            return ShouldDropTremorTotem(botAI, bot);
        default:
            return false;
    }
}

bool RaidAntiFearTrigger::IsActive()
{
    if (bot->getClass() != CLASS_PRIEST && bot->getClass() != CLASS_SHAMAN)
        return false;

    return FearWindowActive() && RaidAntiFearReady(botAI, bot);
}

bool RaidAntiFearAction::Execute(Event /*event*/)
{
    if (bot->getClass() == CLASS_PRIEST)
    {
        Player* target = GetAntiFearWardTarget(botAI, bot);
        return target && botAI->CastSpell("fear ward", target);
    }

    if (bot->getClass() == CLASS_SHAMAN)
        return ShouldDropTremorTotem(botAI, bot) && botAI->CastSpell("tremor totem", bot);

    return false;
}

bool RaidAntiFearAction::isUseful()
{
    if (bot->getClass() != CLASS_PRIEST && bot->getClass() != CLASS_SHAMAN)
        return false;

    return FearWindowActive() && RaidAntiFearReady(botAI, bot);
}

// The earth totem slot holds one totem, so Tremor only stays down if the shaman's own totem nodes
// are held for as long as the fear can land - otherwise Stoneskin replaces it on the next GCD.
float RaidAntiFearTotemGuardMultiplier::GetValue(Action* action)
{
    if (bot->getClass() != CLASS_SHAMAN)
        return 1.0f;

    // The encounter lookup only runs for the handful of actions that could take the slot.
    if (!dynamic_cast<CastStrengthOfEarthTotemAction*>(action) && !dynamic_cast<CastStoneskinTotemAction*>(action) &&
        !dynamic_cast<CastStoneclawTotemAction*>(action) && !dynamic_cast<CastEarthbindTotemAction*>(action))
    {
        return 1.0f;
    }

    return FearWindowActive() ? 0.0f : 1.0f;
}
