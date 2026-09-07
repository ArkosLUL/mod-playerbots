/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "WipeAction.h"
#include "PlayerbotAI.h"
#include "RaidObs.h"

bool WipeAction::Execute(Event event)
{
    Player* const owner = event.getOwner();
    Player* const master = this->botAI->GetMaster();

    if (owner != nullptr && master != nullptr && master->GetGUID() != owner->GetGUID())
        return false;

    if (!bot->IsAlive())
        return false;

    // Before the kill: Unit::Kill runs the death hook inline, so the flag has to be set by the time
    // it does or the record is already written.
    RaidObs::NoteScriptedWipe(bot);

    bot->Kill(bot, bot);
    return true;
}
