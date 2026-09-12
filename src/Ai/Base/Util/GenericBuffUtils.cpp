/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GenericBuffUtils.h"
#include "AiObjectContext.h"
#include "BossAuraTriggers.h"
#include "GameTime.h"
#include "Group.h"
#include "HunterTriggers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "Unit.h"
#include "Value.h"

namespace ai::buff
{
    namespace
    {
        // Prevents bots from immediately casting already-present buffs upon logging in
        constexpr uint32 POST_LOGIN_BUFF_GRACE_MS = 5 * IN_MILLISECONDS;

        bool IsWithinPostLoginBuffGrace(Player* player)
        {
            if (!player)
                return false;

            return getMSTimeDiff(
                player->GetInGameTime(), GameTime::GetGameTimeMS().count()) < POST_LOGIN_BUFF_GRACE_MS;
        }
    }

    bool BuffBelowRefreshTarget(PlayerbotAI* botAI, Aura* aura, uint32 baseBeforeDuration)
    {
        if (!aura)
            return true;

        return botAI->forceRebuff.BuffBelowRefreshTarget(
            aura->GetDuration(), aura->GetMaxDuration(), baseBeforeDuration);
    }

    static bool HasEnoughSameMapMissingPlayersForGroupVariant(
        Player* bot, PlayerbotAI* botAI, std::string const& baseName,
        std::string const& groupName, uint32 requiredCount = 3)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        uint32 missingCount = 0;
        for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* member = gref->GetSource();
            if (!member || !member->IsInWorld() || !member->IsAlive() ||
                member->GetMap() != bot->GetMap())
            {
                continue;
            }

            if (!BuffBelowRefreshTarget(botAI, botAI->GetAura(baseName, member), 0) ||
                !BuffBelowRefreshTarget(botAI, botAI->GetAura(groupName, member), 0))
                continue;

            if (++missingCount >= requiredCount)
                return true;
        }

        return false;
    }

    static bool IsEligibleGroupForPartyBuffs(Group const* group)
    {
        if (!group)
            return false;

        switch (sPlayerbotAIConfig.autoPartyBuffs)
        {
            case AutoPartyBuffMode::RAID_ONLY:
                return group->isRaidGroup();
            case AutoPartyBuffMode::GROUP_OR_RAID:
                return true;
            case AutoPartyBuffMode::DISABLED:
                return false;
        }

        return false;
    }

    bool IsGroupVariantEnabled(Player* bot, std::string const& name)
    {
        if (!IsEligibleGroupForPartyBuffs(bot->GetGroup()))
            return false;

        return !GroupVariantFor(name).empty();
    }

    std::string MakeAuraQualifierForBuff(std::string const& name)
    {
        // Paladin
        if (name == "blessing of kings")        return "blessing of kings,greater blessing of kings";
        if (name == "blessing of might")        return "blessing of might,greater blessing of might";
        if (name == "blessing of wisdom")       return "blessing of wisdom,greater blessing of wisdom";
        if (name == "blessing of sanctuary")    return "blessing of sanctuary,greater blessing of sanctuary";
        // Druid
        if (name == "mark of the wild")         return "mark of the wild,gift of the wild";
        // Mage
        if (name == "arcane intellect")         return "arcane intellect,arcane brilliance";
        // Priest
        if (name == "power word: fortitude")    return "power word: fortitude,prayer of fortitude";
        if (name == "divine spirit")            return "divine spirit,prayer of spirit";
        if (name == "shadow protection")        return "shadow protection,prayer of shadow protection";

        return name;
    }

    std::string GroupVariantFor(std::string const& name)
    {
        // Druid
        if (name == "mark of the wild")         return "gift of the wild";
        // Mage
        if (name == "arcane intellect")         return "arcane brilliance";
        // Priest
        if (name == "power word: fortitude")    return "prayer of fortitude";
        if (name == "divine spirit")            return "prayer of spirit";
        if (name == "shadow protection")        return "prayer of shadow protection";

        // Paladin blessings are intentionally not included here because they are
        // coordinated by the auto greater blessing system instead.
        return std::string();
    }

    bool NeedsPostLoginBuffGrace(std::string const& name)
    {
        static char const* const trackedBuffs[] = {
            "mark of the wild",
            "arcane intellect",
            "power word: fortitude",
            "prayer of fortitude",
            "divine spirit",
            "prayer of spirit",
            "shadow protection",
            "prayer of shadow protection",
            "blessing of kings",
            "blessing of might",
            "blessing of wisdom",
            "blessing of sanctuary"
        };

        for (char const* trackedBuff : trackedBuffs)
        {
            if (name.find(trackedBuff) != std::string::npos)
                return true;
        }

        return false;
    }

    bool ShouldDeferPartyBuffEvaluationForRecentLogin(
        Player* bot, Unit* target, std::string const& spell)
    {
        if (!NeedsPostLoginBuffGrace(spell))
            return false;

        if (IsWithinPostLoginBuffGrace(bot))
            return true;

        Player* playerTarget = target ? target->ToPlayer() : nullptr;
        return IsWithinPostLoginBuffGrace(playerTarget);
    }

    bool ShouldDeferGreaterBlessingAssignmentForRecentLogin(Player* bot)
    {
        if (IsWithinPostLoginBuffGrace(bot))
            return true;

        Group* group = bot->GetGroup();
        if (!group)
            return false;

        for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* member = gref->GetSource();
            if (!member || !member->IsInWorld())
                continue;

            if (IsWithinPostLoginBuffGrace(member))
                return true;
        }

        return false;
    }

    bool HasRequiredReagents(Player* bot, uint32 spellId)
    {
        if (!spellId)
            return false;

        if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId))
        {
            for (int i = 0; i < MAX_SPELL_REAGENTS; ++i)
            {
                if (info->Reagent[i] > 0)
                {
                    uint32 const itemId = info->Reagent[i];
                    int32 const need = info->ReagentCount[i];
                    if ((int32)bot->GetItemCount(itemId, false) < need)
                        return false;
                }
            }
            return true;
        }
        return false;
    }

    void ClearMissingBuffReagentNotice(PlayerbotAI* botAI, std::string const& groupName)
    {
        if (!botAI || groupName.empty())
            return;

        botAI->GetAiObjectContext()
            ->GetValue<MissingBuffReagentNoticeMap&>("missing buff reagent notice")->Get()
            .erase(groupName);
    }

    bool TryAnnounceMissingBuffReagents(
        PlayerbotAI* botAI, std::string const& baseName, std::string const& groupName)
    {
        if (!sPlayerbotAIConfig.tellWhenMissingBuffReagents)
            return false;

        Player* bot = botAI->GetBot();
        if (bot->InBattleground())
            return false;

        auto const cooldownMs = sPlayerbotAIConfig.missingBuffReagentMessageCooldown * IN_MILLISECONDS;
        auto const now = GameTime::GetGameTimeMS().count();
        auto& noticeTimes = botAI->GetAiObjectContext()
            ->GetValue<MissingBuffReagentNoticeMap&>("missing buff reagent notice")->Get();
        auto const noticeIt = noticeTimes.find(groupName);

        if (cooldownMs && noticeIt != noticeTimes.end() &&
            getMSTimeDiff(noticeIt->second, now) < cooldownMs)
        {
            return false;
        }

        std::map<std::string, std::string> placeholders = {
            {"%base_spell", baseName},
            {"%group_spell", groupName}
        };

        std::string const message = PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "missing_group_buff_reagent",
            "I am out of reagents for %group_spell and am casting %base_spell instead.",
            placeholders);

        Group* group = bot->GetGroup();
        if (!group)
            return false;

        const bool announced =
            group->isRaidGroup() ? botAI->SayToRaid(message) : botAI->SayToParty(message);

        if (announced)
            noticeTimes[groupName] = now;

        return announced;
    }

    std::string UpgradeToGroupIfAppropriate(
        Player* bot,
        PlayerbotAI* botAI,
        std::string const& baseName,
        std::string* outMissingReagentGroupName)
    {
        if (outMissingReagentGroupName)
            outMissingReagentGroupName->clear();

        if (!IsGroupVariantEnabled(bot, baseName))
            return baseName;

        std::string const groupName = GroupVariantFor(baseName);
        if (groupName.empty())
            return baseName;

        // Prefer singles until at least three living, in-world group members on the bot's map
        // are missing both the single-target buff and its group variant.
        if (!HasEnoughSameMapMissingPlayersForGroupVariant(bot, botAI, baseName, groupName))
            return baseName;

        uint32 const groupSpellId = botAI->GetAiObjectContext()
            ->GetValue<uint32>("spell id", groupName)->Get();

        if (groupSpellId && HasRequiredReagents(bot, groupSpellId))
        {
            ClearMissingBuffReagentNotice(botAI, groupName);
            return groupName;
        }

        if (groupSpellId && outMissingReagentGroupName)
            *outMissingReagentGroupName = groupName;

        return baseName;
    }

    Unit* FindBossByName(PlayerbotAI* botAI, std::string const& bossName)
    {
        if (bossName.empty())
            return nullptr;

        for (ObjectGuid const& guid :
             botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get())
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit)
                continue;

            std::wstring wnamepart;
            if (!Utf8toWStr(unit->GetName(), wnamepart))
                continue;

            wstrToLower(wnamepart);
            if (bossName.length() == wnamepart.length() && Utf8FitTo(bossName, wnamepart))
                return unit;
        }

        return nullptr;
    }

    Player* GetNatureResistanceHunter(PlayerbotAI* botAI, Player* bot)
    {
        Group* group = bot ? bot->GetGroup() : nullptr;
        if (!group || !group->isRaidGroup())
            return nullptr;

        static uint32 const ranks[] = {SPELL_ASPECT_OF_THE_WILD_RANK_4, SPELL_ASPECT_OF_THE_WILD_RANK_3,
                                       SPELL_ASPECT_OF_THE_WILD_RANK_2, SPELL_ASPECT_OF_THE_WILD_RANK_1};

        // Eligibility reuses the Viper band, so the role only moves on a real mana swing rather than
        // tick by tick: a hunter that hands off at 30% is not eligible again until Viper has carried it
        // back to 60%. Over a long fight two healthy hunters trade the role every minute or so, which is
        // fine - each one gets its turn on Viper.
        //
        // The last fallback ignores eligibility on purpose - the raid-wide nature resistance is worth
        // more than one hunter's mana, so the aura must never lapse. Two consequences that look like
        // bugs but are not:
        //   - a raid with exactly one hunter pins that hunter into Wild for the whole encounter, and it
        //     will run low;
        //   - during a raid-wide mana trough the first hunter is held in Wild while the others refill on
        //     Viper. The first one back over 60% takes the role, which frees the pinned one.
        Player* auraHolder = nullptr;
        Player* staleHolder = nullptr;
        Player* firstEligible = nullptr;
        Player* firstCandidate = nullptr;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || member->getClass() != CLASS_HUNTER ||
                member->GetMapId() != bot->GetMapId())
                continue;

            bool knows = false;
            for (uint32 rank : ranks)
                if (member->HasActiveSpell(rank))
                {
                    knows = true;
                    break;
                }

            if (!knows)
                continue;

            if (!firstCandidate)
                firstCandidate = member;

            bool const hasWild = botAI->HasAura("aspect of the wild", member);

            if (botAI->HasAura("aspect of the viper", member) ||
                member->GetPowerPct(POWER_MANA) < HUNTER_VIPER_ENTER_MANA_PCT)
            {
                if (!staleHolder && hasWild)
                    staleHolder = member;

                continue;
            }

            if (!firstEligible)
                firstEligible = member;

            if (!auraHolder && hasWild)
                auraHolder = member;
        }

        if (auraHolder)
            return auraHolder;

        // Somebody too low to keep the role is still wearing the aura. Nobody is drafted and nobody is
        // pinned: Aspect of the Wild costs nothing to hold, so the raid keeps its resistance, and the
        // outgoing holder drops it by itself as soon as it swaps to Viper - aspects are exclusive.
        // Drafting a replacement before that happens stacks a second copy of an exclusive area aura,
        // which buys the raid nothing and costs that hunter its own aspect for as long as it holds.
        //
        // Locality here is only "same map", so a stale holder that wanders out of the aura radius keeps
        // the draft suppressed while the raid has no buff. Tighten with a distance check if a trace
        // ever shows that.
        if (staleHolder)
            return nullptr;

        return firstEligible ? firstEligible : firstCandidate;
    }
}

namespace ai::spell
{
    bool HasSpellOrCategoryCooldown(Player* bot, uint32 spellId)
    {
        if (bot->HasSpellCooldown(spellId))
            return true;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            return false;

        uint32 category = spellInfo->GetCategory();
        if (!category)
            return false;

        for (auto const& [cooldownSpellId, cooldown] : bot->GetSpellCooldownMap())
        {
            if (cooldown.category == category && bot->GetSpellCooldownDelay(cooldownSpellId) > 0)
                return true;
        }

        return false;
    }
}
