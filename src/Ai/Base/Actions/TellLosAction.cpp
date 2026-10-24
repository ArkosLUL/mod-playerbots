/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TellLosAction.h"
#include "BisDatasetMgr.h"
#include "BisListMgr.h"
#include "ChatHelper.h"
#include "Event.h"
#include "ItemTemplate.h"
#include "ItemUsageValue.h"
#include "ObjectMgr.h"
#include "Playerbots.h"
#include "StatsWeightCalculator.h"
#include "World.h"
#include <charconv>
#include <iomanip>
#include <iterator>
#include <sstream>

bool TellLosAction::Execute(Event event)
{
    std::string const param = event.getParam();

    if (param.empty() || param == "targets")
    {
        ListUnits("--- Targets ---", *context->GetValue<GuidVector>("possible targets"));
        ListUnits("--- Targets (All) ---", *context->GetValue<GuidVector>("all targets"));
    }

    if (param.empty() || param == "npcs")
    {
        ListUnits("--- NPCs ---", *context->GetValue<GuidVector>("nearest npcs"));
    }

    if (param.empty() || param == "corpses")
    {
        ListUnits("--- Corpses ---", *context->GetValue<GuidVector>("nearest corpses"));
    }

    if (param.empty() || param == "gos" || param == "game objects")
    {
        ListGameObjects("--- Game objects ---", *context->GetValue<GuidVector>("nearest game objects"));
    }

    if (param.empty() || param == "players")
    {
        ListUnits("--- Friendly players ---", *context->GetValue<GuidVector>("nearest friendly players"));
    }

    if (param.empty() || param == "triggers")
    {
        ListUnits("--- Triggers ---", *context->GetValue<GuidVector>("possible triggers"));
    }

    return true;
}

void TellLosAction::ListUnits(std::string const title, GuidVector units)
{
    botAI->TellMaster(title);

    for (ObjectGuid const guid : units)
    {
        if (Unit* unit = botAI->GetUnit(guid))
        {
            botAI->TellMaster(unit->GetNameForLocaleIdx(sWorld->GetDefaultDbcLocale()));
        }
    }
}
void TellLosAction::ListGameObjects(std::string const title, GuidVector gos)
{
    botAI->TellMaster(title);

    for (ObjectGuid const guid : gos)
    {
        if (GameObject* go = botAI->GetGameObject(guid))
            botAI->TellMaster(chat->FormatGameobject(go));
    }
}

bool TellAuraAction::Execute(Event /*event*/)
{
    botAI->TellMaster("--- Auras ---");
    sLog->outMessage("playerbot", LOG_LEVEL_DEBUG, "--- Auras ---");
    Unit::AuraApplicationMap& map = bot->GetAppliedAuras();
    for (Unit::AuraApplicationMap::iterator i = map.begin(); i != map.end(); ++i)
    {
        Aura* aura = i->second->GetBase();
        if (!aura)
            continue;
        const std::string auraName = aura->GetSpellInfo()->SpellName[0];
        sLog->outMessage("playerbot", LOG_LEVEL_DEBUG, "Info of Aura - name: " + auraName);
        AuraObjectType type = aura->GetType();
        WorldObject* owner = aura->GetOwner();
        std::string owner_name = owner ? owner->GetName() : "unknown";
        float distance = bot->GetDistance2d(owner);
        Unit* caster = aura->GetCaster();
        std::string caster_name = caster ? caster->GetName() : "unknown";
        bool is_area = aura->IsArea();
        int32 duration = aura->GetDuration();
        int32 spellId = aura->GetSpellInfo()->Id;
        bool isPositive = aura->GetSpellInfo()->IsPositive();
        sLog->outMessage("playerbot", LOG_LEVEL_DEBUG,
                         "Info of Aura - name: " + auraName + " caster: " + caster_name + " type: " +
                             std::to_string(type) + " owner: " + owner_name + " distance: " + std::to_string(distance) +
                             " isArea: " + std::to_string(is_area) + " duration: " + std::to_string(duration) +
                             " spellId: " + std::to_string(spellId) + " isPositive: " + std::to_string(isPositive));

        botAI->TellMaster("Info of Aura - name: " + auraName + " caster: " + caster_name + " type: " +
                          std::to_string(type) + " owner: " + owner_name + " distance: " + std::to_string(distance) +
                          " isArea: " + std::to_string(is_area) + " duration: " + std::to_string(duration) +
                          " spellId: " + std::to_string(spellId) + " isPositive: " + std::to_string(isPositive));

        if (type == DYNOBJ_AURA_TYPE)
        {
            DynamicObject* dyn_owner = aura->GetDynobjOwner();
            float radius = dyn_owner->GetRadius();
            int32 spellId = dyn_owner->GetSpellId();
            int32 duration = dyn_owner->GetDuration();
            sLog->outMessage("playerbot", LOG_LEVEL_DEBUG,
                             std::string("Info of DynamicObject -") + " name: " + dyn_owner->GetName() +
                                 " radius: " + std::to_string(radius) + " spell id: " + std::to_string(spellId) +
                                 " duration: " + std::to_string(duration));

            botAI->TellMaster(std::string("Info of DynamicObject -") + " name: " + dyn_owner->GetName() +
                              " radius: " + std::to_string(radius) + " spell id: " + std::to_string(spellId) +
                              " duration: " + std::to_string(duration));
        }
    }
    return true;
}

bool TellEstimatedDpsAction::Execute(Event /*event*/)
{
    float dps = AI_VALUE(float, "estimated group dps");
    botAI->TellMaster("Estimated Group DPS: " + std::to_string(dps));
    return true;
}

namespace
{
char const* UsageText(ItemUsage usage)
{
    switch (usage)
    {
        case ITEM_USAGE_EQUIP:
            return "EQUIP";
        case ITEM_USAGE_REPLACE:
            return "REPLACE";
        case ITEM_USAGE_BAD_EQUIP:
            return "BAD_EQUIP";
        case ITEM_USAGE_BROKEN_EQUIP:
            return "BROKEN_EQUIP";
        default:
            return "NONE";
    }
}
}  // namespace

bool TellCalculateItemAction::Execute(Event event)
{
    std::string const text = event.getParam();
    ItemWithRandomProperty item = chat->parseItemWithRandomProperty(text);
    if (!item.itemId && text.find("Hitem:") == std::string::npos)
    {
        // Accept only a complete bare item id; atol() would accept a numeric prefix.
        char const* begin = text.c_str();
        while (*begin == ' ' || *begin == '\t')
            ++begin;

        char const* digitsEnd = begin;
        while (*digitsEnd >= '0' && *digitsEnd <= '9')
            ++digitsEnd;
        char const* tail = digitsEnd;
        while (*tail == ' ' || *tail == '\t')
            ++tail;

        uint32 parsedId = 0;
        auto const [ptr, ec] = std::from_chars(begin, digitsEnd, parsedId);
        if (begin == digitsEnd || *tail != '\0' || ec != std::errc() || ptr != digitsEnd || parsedId == 0)
            return false;

        item.itemId = parsedId;
    }

    StatsWeightCalculator calculator(bot);

    if (!item.itemId)
        return false;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(item.itemId);
    if (!proto)
        return false;

    // Match what EvaluateGear scores with, or the printed number is not the one the loot and equip
    // decisions actually saw.
    calculator.SetItemSetBonus(sPlayerbotAIConfig.ItemSetUseForUpgrades);
    calculator.SetOverflowPenalty(false);
    calculator.SetBisBonus(true);
    // nothing to compare against here, so the rank bonus prints undecayed
    calculator.SetBisContest(item.itemId, item.itemId);
    if (sRandomPlayerbotMgr.IsSpecPvp(bot->GetGUID().GetCounter(), bot->getClass()))
        calculator.SetPvpSpec(true);

    float score = calculator.CalculateItem(item.itemId, item.randomPropertyId);

    std::ostringstream out;
    out << "Calculated score of " << chat->FormatItem(proto) << " : " << score;

    BisProgress const cap = BisListMgr::ProgressForBot(bot);
    BisSource const source = sBisListMgr->ResolveSource(bot, cap);
    if (source.kind == BisSource::Kind::None)
    {
        out << " | BiS: no spec key (role mismatch or pvp spec)";
    }
    else
    {
        uint8 listedPhase = 0;
        uint8 const rank = sBisListMgr->GetBisRankFor(proto->ItemId, source, &listedPhase);

        BisSource anyPhase = source;
        if (source.kind == BisSource::Kind::Dataset)
        {
            anyPhase.progress.phase = BIS_PHASE_MAX;

            BisDatasetSubject const& subject = *source.subject;
            out << " | BiS: dataset " << source.dataset->version << ", ";
            if (subject.kind == BisDatasetSubject::KIND_ROSTER)
                out << "roster " << subject.name << " (" << subject.specName << ")";
            else
                out << "spec " << subject.specName;
            if (subject.variant)
                out << " variant " << uint32(subject.variant);
            out << ", phase " << uint32(source.progress.phase) << " (cap " << uint32(cap.phase) << ")";
        }
        else
        {
            anyPhase.progress.phase = BIS_MAX_PHASE[source.progress.expansion];

            out << " | BiS: class " << uint32(source.cls) << " tab " << uint32(source.tab)
                << ", expansion " << uint32(source.progress.expansion)
                << ", phase cap " << uint32(source.progress.phase);
        }

        out << ", rank " << uint32(rank);
        if (rank)
            out << " listed at phase " << uint32(listedPhase);
        out << " (any phase " << uint32(sBisListMgr->GetBisRankFor(proto->ItemId, anyPhase)) << ")";
        if (proto->Class == ITEM_CLASS_WEAPON)
            out << ", main hand " << uint32(calculator.BisRank(proto->ItemId, BisRankOrder::Hand::Main))
                << ", off hand " << uint32(calculator.BisRank(proto->ItemId, BisRankOrder::Hand::Off));
    }

    botAI->TellMasterNoFacing(out.str());
    botAI->TellMasterNoFacing(calculator.DescribeWeights());

    GearChoice const choice = EvaluateGear(botAI, proto, item.randomPropertyId);
    std::ostringstream decision;
    decision << "decision: " << UsageText(choice.usage);
    if (choice.slot != NULL_SLOT)
    {
        decision << " slot " << uint32(choice.slot);
        if (choice.incumbentTo != NULL_SLOT)
            decision << " (worn one to slot " << uint32(choice.incumbentTo) << ")";
        decision << ", " << choice.newScore << " vs " << choice.oldScore;
    }
    decision << " (" << choice.reason << ")";

    std::string const param = ItemUsageValue::BuildItemUsageParam(item.itemId, item.randomPropertyId);
    ItemUsage const usage = AI_VALUE2(ItemUsage, "item usage", param);
    decision << " | roll: "
             << RollVoteText(CalculateLootRollVote(bot, proto, item.randomPropertyId, usage, bot->GetGroup()));
    botAI->TellMasterNoFacing(decision.str());

    // Per-stat breakdown (value * weight = contribution) so score tuning is verifiable.
    // Keep in sync with enum StatsType in StatsCollector.h.
    // clang-format off
    static char const* statNames[] = {
        "agility",   "strength",   "intellect",     "spirit",          "stamina",
        "hit",       "crit",       "haste",         "armor",           "defense",
        "dodge",     "parry",      "block_value",   "block_rating",    "resilience",
        "health_regen", "spell_power", "spell_penetration", "heal_power", "mana_regen",
        "attack_power", "armor_penetration", "expertise", "melee_dps", "ranged_dps",
        "bonus"
    };
    // clang-format on
    static_assert(std::size(statNames) == STATS_TYPE_MAX, "statNames must stay in sync with StatsType");

    StatsCollector const* collector = calculator.GetCollector();
    float const* weights = calculator.GetStatWeights();
    float sum = 0.0f;
    std::vector<std::string> lines;
    std::string line;
    for (uint32 i = 0; i < STATS_TYPE_MAX; ++i)
    {
        if (collector->stats[i] == 0.0f)
            continue;

        float const contribution = collector->stats[i] * weights[i];
        sum += contribution;

        std::ostringstream term;
        term << std::fixed << std::setprecision(2) << statNames[i] << " " << collector->stats[i] << " * " << weights[i]
             << " = " << contribution;

        // Split into several whispers so no line exceeds the client limit.
        if (!line.empty() && line.size() + 2 + term.str().size() > PlayerbotAI::MAX_CHAT_MESSAGE_BYTES)
        {
            lines.push_back(line);
            line.clear();
        }

        if (!line.empty())
            line += ", ";
        line += term.str();
    }

    if (line.empty())
        return true;

    lines.push_back(line);

    std::ostringstream sumText;
    std::ostringstream multText;
    sumText << std::fixed << std::setprecision(2) << sum;
    multText << std::fixed << std::setprecision(2) << (sum != 0.0f ? score / sum : 0.0f);

    std::map<std::string, std::string> placeholders;
    placeholders["%sum"] = sumText.str();
    placeholders["%mult"] = multText.str();
    botAI->TellMasterNoFacing(PlayerbotTextMgr::instance().GetBotTextOrDefault(
        "calc_item_breakdown", "Breakdown: weighted sum %sum, multipliers x%mult", placeholders));

    for (std::string const& chunk : lines)
        botAI->TellMasterNoFacing(chunk);

    return true;
}
