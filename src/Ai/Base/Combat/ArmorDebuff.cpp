/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ArmorDebuff.h"

#include "Creature.h"
#include "Group.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "SharedDefines.h"
#include "Unit.h"

bool TargetHasMajorArmorDebuff(PlayerbotAI* botAI, Unit* target)
{
    if (!botAI || !target)
        return false;

    return botAI->HasAura("sunder armor", target) || TargetHasNonSunderMajorArmorDebuff(botAI, target);
}

bool TargetHasNonSunderMajorArmorDebuff(PlayerbotAI* botAI, Unit* target)
{
    if (!botAI || !target)
        return false;

    // On a hostile target IsRealAura hides another caster's aura until it reaches max stacks, so both
    // of these only report true at full value - 2/2 Acid Spit, and Expose Armor which never stacks.
    return botAI->HasAura("expose armor", target) || botAI->HasAura("acid spit", target);
}

bool GroupSuppliesMajorArmorDebuff(Player* bot)
{
    if (!bot)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || !member->IsInWorld() ||
            member->GetMapId() != bot->GetMapId())
        {
            continue;
        }

        if (member->getClass() == CLASS_WARRIOR)
            return true;

        if (member->getClass() == CLASS_HUNTER)
        {
            Pet* pet = member->GetPet();
            CreatureTemplate const* petTemplate = pet ? pet->GetCreatureTemplate() : nullptr;
            if (petTemplate && petTemplate->family == CREATURE_FAMILY_WORM)
                return true;
        }
    }

    return false;
}
