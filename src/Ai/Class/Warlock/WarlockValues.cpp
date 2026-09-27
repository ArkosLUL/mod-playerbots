/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "WarlockValues.h"

#include <ctime>

#include "LastSpellCastValue.h"
#include "Playerbots.h"
#include "Strategy.h"
#include "TargetValue.h"

// Seed cast time plus its flight, rounded up: "last spell cast" only keeps whole seconds.
static constexpr time_t SEED_IN_FLIGHT_SECONDS = 3;

static bool IsBoss(Unit* unit)
{
    Creature* creature = unit->ToCreature();
    return creature && (creature->IsDungeonBoss() || creature->isWorldBoss());
}

bool IsSeedOfCorruptionTarget(PlayerbotAI* botAI, Unit* target)
{
    return target && !IsBoss(target) && !botAI->HasAura("corruption", target, false, true);
}

bool SeedOfCorruptionPending(PlayerbotAI* botAI, Unit* target)
{
    if (!target)
        return false;

    if (botAI->HasAura("seed of corruption", target, false, true))
        return true;

    AiObjectContext* context = botAI->GetAiObjectContext();
    uint32 const seedId = context->GetValue<uint32>("spell id", "seed of corruption")->Get();
    LastSpellCast const& lastSpell = context->GetValue<LastSpellCast&>("last spell cast")->Get();

    return seedId && lastSpell.id == seedId && lastSpell.target == target->GetGUID() &&
           time(nullptr) - lastSpell.timer <= SEED_IN_FLIGHT_SECONDS;
}

Unit* SeedOfCorruptionTargetValue::Calculate()
{
    GuidVector const attackers = AI_VALUE(GuidVector, "attackers");
    GuidSet const dynamicExclusions = GatherStrategyTargetExclusions(botAI, TargetValueExclusionType::Attacker);
    float const range = botAI->GetRange("spell");

    Unit* clean = nullptr;
    // Trash already carrying our Corruption, only used when nothing else is left
    Unit* corrupted = nullptr;
    for (ObjectGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || dynamicExclusions.find(guid) != dynamicExclusions.end())
            continue;

        if (IsBoss(unit) || !bot->IsWithinCombatRange(unit, range) || SeedOfCorruptionPending(botAI, unit))
            continue;

        Unit*& best = botAI->HasAura("corruption", unit, false, true) ? corrupted : clean;
        if (!best || unit->GetHealth() > best->GetHealth())
            best = unit;
    }

    return clean ? clean : corrupted;
}
