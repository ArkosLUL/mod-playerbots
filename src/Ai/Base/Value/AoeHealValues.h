/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AOEHEALVALUES_H
#define PLAYERBOTS_AOEHEALVALUES_H

#include "NamedObjectContext.h"
#include "Value.h"

class PlayerbotAI;
class Player;

class AoeHealValue : public Uint8CalculatedValue, public Qualified
{
public:
    AoeHealValue(PlayerbotAI* botAI, std::string const name = "aoe heal") : Uint8CalculatedValue(botAI, name) {}

    uint8 Calculate() override;
};

// Alive group members in heal range, the population "aoe heal" counts from. Anything comparing an
// "aoe heal" count against a group size has to use this, or the two sides measure different raids.
uint8 CountHealableGroupMembers(Player* bot);

#endif
