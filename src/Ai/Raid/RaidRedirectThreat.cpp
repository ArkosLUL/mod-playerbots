/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidRedirectThreat.h"

#include "Group.h"
#include "Playerbots.h"

bool RaidRedirectThreatAction::isUseful()
{
    return bot->getClass() == CLASS_HUNTER || bot->getClass() == CLASS_ROGUE;
}

bool RaidRedirectThreatAction::Execute(Event /*event*/)
{
    Player* tank = GetRedirectTank();
    if (!tank || tank == bot)
    {
        return false;
    }

    if (bot->getClass() == CLASS_ROGUE)
    {
        // Tricks redirects everything the rogue does for the next 6s, so there is no dump shot.
        return botAI->CanCastSpell("tricks of the trade", tank) && botAI->CastSpell("tricks of the trade", tank);
    }

    if (botAI->CanCastSpell("misdirection", tank))
    {
        return botAI->CastSpell("misdirection", tank);
    }

    // Misdirection only moves the threat of the next three shots, so spend them where they are
    // wanted instead of leaving them to whatever the rotation picks.
    Unit* dumpTarget = GetThreatDumpTarget();
    if (dumpTarget && bot->HasAura(SPELL_MISDIRECTION_PROC) && botAI->CanCastSpell("steady shot", dumpTarget))
    {
        return botAI->CastSpell("steady shot", dumpTarget);
    }

    return false;
}

Player* RaidRedirectThreatAction::GetTankHolding(Unit* target)
{
    if (!target)
    {
        return nullptr;
    }

    Group* group = bot->GetGroup();
    if (!group)
    {
        return nullptr;
    }

    Unit* victim = target->GetVictim();
    if (!victim)
    {
        return nullptr;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member == bot)
        {
            continue;
        }
        if (botAI->IsTank(member) && member == victim)
        {
            return member;
        }
    }
    return nullptr;
}

int32 RaidRedirectThreatAction::GetRedirecterIndex()
{
    Group* group = bot->GetGroup();
    if (!group)
    {
        return -1;
    }

    int32 index = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive())
        {
            continue;
        }
        if (member->getClass() != CLASS_HUNTER && member->getClass() != CLASS_ROGUE)
        {
            continue;
        }
        if (member == bot)
        {
            return index;
        }
        ++index;
    }
    return -1;
}
