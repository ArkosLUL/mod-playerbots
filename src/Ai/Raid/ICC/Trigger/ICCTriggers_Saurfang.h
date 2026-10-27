/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_SAURFANG_H
#define PLAYERBOTS_ICCTRIGGERS_SAURFANG_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

class IccDbsTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dbs";

    IccDbsTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
