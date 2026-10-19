/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ShamanActions.h"
#include "Action.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "TotemsShamanStrategy.h"

bool CastTotemAction::isUseful()
{
    return CastBuffSpellAction::isUseful()
        && !AI_VALUE2(bool, "has totem", name)
        && !botAI->HasAura(buff, bot);
}

bool CastMagmaTotemAction::isUseful() {
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !bot->IsWithinMeleeRange(target))
        return false;

    return CastTotemAction::isUseful() && !AI_VALUE2(bool, "has totem", name);
}

bool CastFireNovaAction::isUseful() {
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    Creature* fireTotem = bot->GetMap()->GetCreature(bot->m_SummonSlot[1]);
    if (!fireTotem)
        return false;

    if (target->GetDistance(fireTotem) > 8.0f)
        return false;

    return CastMeleeSpellAction::isUseful();
}

bool CastCleansingTotemAction::isUseful()
{
    return CastTotemAction::isUseful() && !AI_VALUE2(bool, "has totem", "mana tide totem");
}

// Will only cast Stoneclaw Totem if low on health and not in a group
bool CastStoneclawTotemAction::isUseful()
{
    return !bot->GetGroup();
}

// Lava Burst's sure crit needs our Flame Shock up when the cast finishes, so with the shock about to drop
// Lightning Bolt goes first and Lava Burst waits for the refresh
bool CastLavaBurstAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    static const uint32 FLAME_SHOCK_SPELL_IDS[] =
        {8050, 8052, 8053, 10447, 10448, 29228, 25457, 49232, 49233};
    constexpr int32 MIN_FLAME_SHOCK_LEFT_MS = 2000;

    ObjectGuid botGuid = bot->GetGUID();
    for (uint32 spellId : FLAME_SHOCK_SPELL_IDS)
    {
        if (Aura const* flameShock = target->GetAura(spellId, botGuid))
            return flameShock->GetDuration() > MIN_FLAME_SHOCK_LEFT_MS;
    }

    return false;
}

bool CastTotemOfWrathAction::isUseful()
{
    // the glyph's 5 min self buff is also named Totem of Wrath, so check the totem's own auras by id or
    // the totem stays down after Fire Elemental until the glyph buff runs out
    static uint32 const TOTEM_OF_WRATH_AURA_IDS[] = {57658, 57660, 57662, 57663};

    if (!GetTarget() || !CastSpellAction::isUseful() || AI_VALUE2(bool, "has totem", name))
        return false;

    for (uint32 auraId : TOTEM_OF_WRATH_AURA_IDS)
    {
        if (bot->HasAura(auraId))
            return false;
    }

    return true;
}

// Logic for making a guardian (spirit wolf) use a spell (spirit walk)
// There is no existing code for guardians casting spells in the AC/Playerbots repo.
bool CastSpiritWalkAction::Execute(Event /*event*/)
{
    constexpr uint32 SPIRIT_WOLF = 29264;
    constexpr uint32 SPIRIT_WALK_SPELL = 58875;

    for (Unit* unit : bot->m_Controlled)
    {
        if (unit->GetEntry() == SPIRIT_WOLF && unit->HasSpell(SPIRIT_WALK_SPELL))
        {
            unit->CastSpell(unit, SPIRIT_WALK_SPELL, false);
            return true;
        }
    }

    return false;
}

// Set Strategy Assigned Totems (Actions) - First, it checks
// the highest-rank spell the bot knows for each totem type,
// then adds it to the Call of the Elements bar.
bool SetTotemAction::Execute(Event /*event*/)
{
    uint32 totemSpell = 0;
    for (size_t i = 0; i < totemSpellIdsCount; ++i)
    {
        if (bot->HasSpell(totemSpellIds[i]))
        {
            totemSpell = totemSpellIds[i];
            break;
        }
    }

    if (!totemSpell)
        return false;

    if (ActionButton const* button = bot->GetActionButton(actionButtonId);
        button && button->GetType() == ACTION_BUTTON_SPELL &&
        button->GetAction() == totemSpell)
    {
        return false;
    }

    bot->addActionButton(actionButtonId, totemSpell, ACTION_BUTTON_SPELL);
    return true;
}
