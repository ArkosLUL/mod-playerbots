/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "WarlockValues.h"

#include <ctime>

#include "Bag.h"
#include "Group.h"
#include "Item.h"
#include "LastSpellCastValue.h"
#include "Playerbots.h"
#include "RitualOfSoulsValues.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
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

bool ImmolateNeedsRefresh(PlayerbotAI* botAI, Unit* target)
{
    if (!target)
        return false;

    Aura* aura = botAI->GetAura("immolate", target, true);
    if (!aura)
        return true;

    return aura->GetDuration() < static_cast<int32>(aura->GetSpellInfo()->CalcCastTime(botAI->GetBot()));
}

bool HasOwnCurseOfDoom(PlayerbotAI* botAI, Unit* target)
{
    return target && botAI->HasAura("curse of doom", target, false, true);
}

Unit* SeedOfCorruptionTargetValue::Calculate()
{
    GuidVector const attackers = AI_VALUE(GuidVector, "attackers");
    GuidSet const dynamicExclusions =
        GatherStrategyTargetExclusions(botAI, TargetValueExclusionType::DebuffTarget, attackers);
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

char* strstri(char const* haystack, char const* needle);

static constexpr uint32 ITEM_SOUL_SHARD = 6265;
static constexpr uint32 SPELL_IMPROVED_HEALTHSTONE_R1 = 18692;
static constexpr uint32 SPELL_IMPROVED_HEALTHSTONE_R2 = 18693;
static constexpr float RITUAL_NEARBY_RANGE = 30.0f;
static constexpr uint32 RITUAL_MIN_PARTICIPANTS = 3;  // caster + 2 helpers
static constexpr uint32 RITUAL_MIN_MISSING = 3;

uint32 ReadyRitualOfSoulsRank(Player* warlock)
{
    uint32 rank = 0;
    if (warlock->HasSpell(SPELL_RITUAL_OF_SOULS_R2))
        rank = warlock->HasSpellCooldown(SPELL_RITUAL_OF_SOULS_R2) ? 0 : 2;
    else if (warlock->HasSpell(SPELL_RITUAL_OF_SOULS_R1))
        rank = warlock->HasSpellCooldown(SPELL_RITUAL_OF_SOULS_R1) ? 0 : 1;

    if (!rank || !warlock->HasItemCount(ITEM_SOUL_SHARD))
        return 0;

    return rank;
}

// Same name match as the "healthstone" inventory value, so every rank and talent variant counts
static bool ItemNameHasHealthstone(Item* item)
{
    ItemTemplate const* proto = item ? item->GetTemplate() : nullptr;
    return proto && strstri(proto->Name1.c_str(), "Healthstone");
}

static bool HasAnyHealthstone(Player* player)
{
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (ItemNameHasHealthstone(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot)))
            return true;

    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        if (Bag* pBag = player->GetBagByPos(bag))
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                if (ItemNameHasHealthstone(pBag->GetItemByPos(j)))
                    return true;

    return false;
}

// The soulwell hands out improved stones only when its owner has Improved Healthstone, so the
// talented warlock casts. A higher ritual rank still wins over the talent.
static bool IsPreferredRitualCaster(Player* bot, Group* group)
{
    Player* best = nullptr;
    uint32 bestPreference = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member->getClass() != CLASS_WARLOCK || !member->IsAlive() || member->IsInCombat())
            continue;
        if (member != bot && !bot->IsWithinDistInMap(member, RITUAL_NEARBY_RANGE))
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || !memberAI->HasStrategy("ritualofsouls", BOT_STATE_NON_COMBAT))
            continue;

        uint32 ritualRank = ReadyRitualOfSoulsRank(member);
        if (!ritualRank)
            continue;

        uint32 talentRank = member->HasAura(SPELL_IMPROVED_HEALTHSTONE_R2) ? 2
                            : (member->HasAura(SPELL_IMPROVED_HEALTHSTONE_R1) ? 1 : 0);
        uint32 preference = ritualRank * 3 + talentRank;  // talent rank is 0-2, ritual rank decides first
        if (best && (preference < bestPreference ||
                     (preference == bestPreference && best->GetGUID() < member->GetGUID())))
            continue;

        best = member;
        bestPreference = preference;
    }

    return best == bot;
}

bool ShouldCastRitualOfSoulsValue::Calculate()
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    if (AI_VALUE(ObjectGuid, "ritual of souls portal") || AI_VALUE(ObjectGuid, "ritual of souls soulwell"))
        return false;

    if (!IsPreferredRitualCaster(bot, group))
        return false;

    uint32 nearbyMembers = 0;
    uint32 missing = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->IsInCombat())
            continue;
        if (member != bot && !bot->IsWithinDistInMap(member, RITUAL_NEARBY_RANGE))
            continue;

        ++nearbyMembers;
        if (!HasAnyHealthstone(member))
            ++missing;
    }

    return nearbyMembers >= RITUAL_MIN_PARTICIPANTS && missing >= RITUAL_MIN_MISSING;
}
