/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BleedDebuff.h"

#include "Group.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "Unit.h"

namespace
{
bool IsBleed(Aura const* aura)
{
    SpellInfo const* spellInfo = aura ? aura->GetSpellInfo() : nullptr;
    if (!spellInfo || spellInfo->IsPositive())
        return false;

    return (spellInfo->GetAllEffectsMechanicMask() & (1ULL << MECHANIC_BLEED)) != 0;
}
}

bool TargetHasBleed(Unit* target)
{
    if (!target)
        return false;

    Unit::AuraApplicationMap const& auras = target->GetAppliedAuras();
    for (Unit::AuraApplicationMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
    {
        if (IsBleed(itr->second->GetBase()))
            return true;
    }

    return false;
}

bool GroupSuppliesBleedOn(Player* bot, Unit* target)
{
    if (!bot || !target)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    Unit::AuraApplicationMap const& auras = target->GetAppliedAuras();
    for (Unit::AuraApplicationMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
    {
        Aura* aura = itr->second->GetBase();
        if (!IsBleed(aura))
            continue;

        ObjectGuid casterGuid = aura->GetCasterGUID();
        if (casterGuid == bot->GetGUID())
            continue;

        if (group->IsMember(casterGuid))
            return true;
    }

    return false;
}
