/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PartyMemberToResurrect.h"
#include "Playerbots.h"

class IsTargetOfResurrectSpell : public SpellEntryPredicate
{
public:
    bool Check(SpellInfo const* spellInfo) override
    {
        for (uint8 i = 0; i < 3; ++i)
        {
            if (spellInfo->Effects[i].Effect == SPELL_EFFECT_RESURRECT ||
                spellInfo->Effects[i].Effect == SPELL_EFFECT_RESURRECT_NEW ||
                spellInfo->Effects[i].Effect == SPELL_EFFECT_SELF_RESURRECT)
                return true;
        }

        return false;
    }
};

class FindDeadPlayer : public FindPlayerPredicate
{
public:
    FindDeadPlayer(PartyMemberValue* value, bool battleRez) : value(value), battleRez(battleRez) {}

    bool Check(Unit* unit) override
    {
        Player* player = unit->ToPlayer();
        if (!player || player->isResurrectRequested() || player->getDeathState() != DeathState::Corpse)
            return false;

        // Soulstone/Reincarnation still pending, don't burn a battle rez on them.
        // Self-res spell stays set after a spirit release, so ghosts are fair game.
        if (battleRez && !player->HasPlayerFlag(PLAYER_FLAGS_GHOST) && player->GetUInt32Value(PLAYER_SELF_RES_SPELL))
            return false;

        return !value->IsTargetOfSpellCast(player, predicate);
    }

private:
    PartyMemberValue* value;
    bool battleRez;
    IsTargetOfResurrectSpell predicate;
};

Unit* PartyMemberToResurrect::Calculate()
{
    FindDeadPlayer finder(this, battleRez);
    return FindPartyMember(finder);
}
