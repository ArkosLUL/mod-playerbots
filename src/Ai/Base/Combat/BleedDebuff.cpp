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
    // The core keeps a bleeding flag in UNIT_FIELD_AURASTATE on every aura apply and remove, so this
    // costs one bit test instead of a walk over the aura map. AURA_STATE_BLEEDING is not per-caster,
    // hence no caster argument.
    return target && target->HasAuraState(AURA_STATE_BLEEDING);
}

bool GroupSuppliesBleedOn(Player* bot, Unit* target)
{
    if (!bot || !target)
        return false;

    if (!target->HasAuraState(AURA_STATE_BLEEDING))
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
        // Group::IsMember walks a member list, so drop creature and pet casters before paying for it.
        if (!casterGuid.IsPlayer() || casterGuid == bot->GetGUID())
            continue;

        if (group->IsMember(casterGuid))
            return true;
    }

    return false;
}
