/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PARTYMEMBERTOHEAL_H
#define PLAYERBOTS_PARTYMEMBERTOHEAL_H

#include "PartyMemberValue.h"

class Pet;
class PlayerbotAI;
class Unit;

class PartyMemberToHeal : public PartyMemberValue
{
public:
    PartyMemberToHeal(PlayerbotAI* botAI, std::string const name = "party member to heal")
        : PartyMemberValue(botAI, name)
    {
    }

protected:
    Unit* Calculate() override;
    bool Check(Unit* player) override;
};

class PartyMemberToProtect : public PartyMemberValue
{
public:
    // excludeTanks skips tanks entirely, for spells that would drop their threat (Blessing of Protection).
    PartyMemberToProtect(PlayerbotAI* botAI, std::string const name = "party member to protect",
                         bool excludeTanks = false)
        : PartyMemberValue(botAI, name), excludeTanks(excludeTanks)
    {
    }

protected:
    Unit* Calculate() override;

    bool excludeTanks;
};

class HealerLowMana : public PartyMemberValue
{
public:
    HealerLowMana(PlayerbotAI* botAI) : PartyMemberValue(botAI, "healer low mana") {}

protected:
    Unit* Calculate() override;
};

#endif
