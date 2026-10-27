/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_LichKing.h"
#include "Playerbots.h"
#include "RaidObs.h"

bool IccLichKingInFrostmourne(Unit const* unit)
{
    return unit->GetExactDistSq(495.708f, -2523.76f, 1049.95f) < 40.0f * 40.0f;
}

IccLichKingPhaseId IccLichKingPhase(Player* bot, Unit* boss)
{
    // The Lich King voice in Valithria's room shares his name
    if (!boss || boss->GetEntry() != NPC_THE_LICH_KING)
        return IccLichKingPhaseId::One;

    // At 70% and 40% his script turns him passive and walks him to the centre for the winter, then
    // turns him aggressive 5 s after the Quake that ends it. Heroic's Harvest Souls holds him passive
    // too, while the raid is inside Frostmourne.
    Creature const* const lichKing = boss->ToCreature();
    bool const passive = lichKing && lichKing->HasReactState(REACT_PASSIVE);
    uint8 const winter = IccRemorselessWinterOf(boss);

    IccLichKingPhaseId phase = IccLichKingPhaseId::Winter2;
    if (winter == 1)
        phase = IccLichKingPhaseId::Winter1;
    else if (winter == 2)
        phase = IccLichKingPhaseId::Winter2;
    else if (boss->HealthAbovePct(70))
        phase = IccLichKingPhaseId::One;
    else if (!passive)
        phase = boss->HealthAbovePct(40) ? IccLichKingPhaseId::Two : IccLichKingPhaseId::Three;
    else if (boss->HealthAbovePct(40))
        phase = IccLichKingPhaseId::Winter1;
    else if (boss->GetMap()->IsHeroic())
    {
        bool harvested = IccLichKingInFrostmourne(bot);
        for (uint32 const spellId : ICC_LK_HARVEST_SOULS)
            harvested = harvested || bot->HasAura(spellId);
        if (harvested)
            phase = IccLichKingPhaseId::Frostmourne;
    }

    if (RaidObs::Active())
    {
        static char const* const NAMES[] = {"one", "winter1", "two", "winter2", "three", "frostmourne"};
        RaidObs::NoteDerived(bot, "lk.phase", NAMES[static_cast<uint8>(phase)]);
    }

    return phase;
}

namespace
{
    // 1 or 2 by the winter's row, 0 when the boss casts neither
    uint8 IccRemorselessWinterCastOf(Unit* boss)
    {
        if (!boss || !boss->HasUnitState(UNIT_STATE_CASTING))
            return 0;

        for (uint32 const spellId : ICC_LK_FIRST_WINTER)
            if (boss->FindCurrentSpellBySpellId(spellId))
                return 1;

        for (uint32 const spellId : ICC_LK_SECOND_WINTER)
            if (boss->FindCurrentSpellBySpellId(spellId))
                return 2;

        return 0;
    }
}

bool IccBossCastingRemorselessWinter(Unit* boss) { return IccRemorselessWinterCastOf(boss) != 0; }

uint8 IccRemorselessWinterOf(Unit* boss)
{
    if (!boss)
        return 0;

    for (uint32 const spellId : ICC_LK_FIRST_WINTER)
        if (boss->HasAura(spellId))
            return 1;

    for (uint32 const spellId : ICC_LK_SECOND_WINTER)
        if (boss->HasAura(spellId))
            return 2;

    return IccRemorselessWinterCastOf(boss);
}

bool IccBossHasRemorselessWinter(Unit* boss) { return IccRemorselessWinterOf(boss) != 0; }
