/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_FROSTGIANT_H
#define PLAYERBOTS_ICCACTIONS_FROSTGIANT_H

#include "MovementActions.h"

// One of each pair walks to the other and stays; anyone else clean steps out of every carrier's reach.
class IccRottingFrostGiantPlagueAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc rotting frost giant plague";

    IccRottingFrostGiantPlagueAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
