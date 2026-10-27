/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_MARROWGAR_H
#define PLAYERBOTS_ICCTRIGGERS_MARROWGAR_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

class IccLmTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lm";

    IccLmTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
