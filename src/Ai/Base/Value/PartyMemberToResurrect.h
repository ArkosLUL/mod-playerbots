/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PARTYMEMBERTORESURRECT_H
#define PLAYERBOTS_PARTYMEMBERTORESURRECT_H

#include "PartyMemberValue.h"

class Player;
class PlayerbotAI;
class Unit;

// True when raising dead brings it straight back Insane: Yogg-Saron's Insane Periodic (64554) charms any
// player without Sanity (63050), Sanity survives death, and an Insane death has already lost it. Keyed on
// living carrying it, so it's false everywhere else.
bool ResurrectBringsBackInsane(Player* dead, Player* living);

class PartyMemberToResurrect : public PartyMemberValue
{
public:
    PartyMemberToResurrect(PlayerbotAI* botAI, std::string const name = "party member to resurrect",
                           bool battleRez = false)
        : PartyMemberValue(botAI, name), battleRez(battleRez)
    {
    }

protected:
    Unit* Calculate() override;

private:
    bool battleRez;
};

#endif
