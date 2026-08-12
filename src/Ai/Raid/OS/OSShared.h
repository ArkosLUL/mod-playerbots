/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSSHARED_H
#define PLAYERBOTS_OSSHARED_H

#include "OSTriggers.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "ThreatManager.h"

#include <vector>

// Inline helpers shared across the Obsidian Sanctum (Sartharion) components. Header-only, RS-style,
// so no extra .cpp/context wiring is needed.
namespace ObsidianSanctumHelpers
{
    // Kill order is landing order, which is also the order the drakes are called in: Tenebron at
    // 20s, Shadron at 60s, Vesperon at 120s. They arrive far enough apart that usually only one is
    // up at a time, and finishing the one already engaged beats swapping to the fresh arrival.
    inline constexpr uint32 killOrder[3] = {NPC_TENEBRON, NPC_SHADRON, NPC_VESPERON};

    inline bool IsDrakeEntry(uint32 entry)
    {
        return entry == NPC_TENEBRON || entry == NPC_SHADRON || entry == NPC_VESPERON;
    }

    // Every drake that joins gets killed. How much bonus loot the raid gets is fixed the moment
    // Sartharion is engaged - he counts the drakes still alive then and never recounts - so killing
    // them during the fight costs nothing, while leaving one up keeps its Power of ... aura on the
    // raid for the rest of the fight.
    inline Unit* FindDrakeToKill(PlayerbotAI* botAI)
    {
        for (uint32 const entry : killOrder)
            if (Unit* drake = GetFirstAliveUnitByEntry(botAI, entry))
                return drake;

        return nullptr;
    }

    // Whelps only. Tenebron's eggs sit in phase 16 where nothing on the ground can see or hit them,
    // and nobody is sent into her portal, so the whelps get killed once they cross into phase 1.
    inline Unit* FindTwilightAdd(PlayerbotAI* botAI)
    {
        return GetFirstAliveUnitByEntry(botAI, NPC_TWILIGHT_WHELP);
    }

    inline Unit* FindLavaBlaze(PlayerbotAI* botAI)
    {
        return GetFirstAliveUnitByEntry(botAI, NPC_LAVA_BLAZE);
    }

    // Adds the off-tank owns alongside the drakes. Whelps come first: they hatch on the boss
    // platform inside the raid stack and put stacking Fade Armor on whoever they reach, which is
    // usually a healer. Lava Blazes matter because one caught loose by a Flame Tsunami enrages.
    inline std::vector<Unit*> FindOffTankAdds(PlayerbotAI* botAI, Player* bot, float range)
    {
        std::vector<Unit*> whelps;
        std::vector<Unit*> blazes;

        auto const& targets =
            botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
        for (ObjectGuid const& guid : targets)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive() || bot->GetExactDist2d(unit) >= range)
                continue;

            if (unit->GetEntry() == NPC_TWILIGHT_WHELP)
                whelps.push_back(unit);
            else if (unit->GetEntry() == NPC_LAVA_BLAZE)
                blazes.push_back(unit);
        }

        whelps.insert(whelps.end(), blazes.begin(), blazes.end());
        return whelps;
    }

    // Acolytes live in phase 16, so a bot on the ground can never see one - detect them by the
    // auras their presence puts on phase-1 targets instead. Shadron's acolyte keeps Gift of Twilight
    // Fire on Sartharion (which zeroes all damage he takes); Vesperon's blankets the whole raid in
    // Twilight Torment. Both ids are the called-by-Sartharion variants, so they can't be confused
    // with a solo drake pull. Tenebron reopens the same shared portal every 60s but produces
    // neither, which is what keeps runners out of a realm with nothing to kill in it.
    inline bool TwilightRealmNeedsRunner(PlayerbotAI* botAI, Player* bot)
    {
        if (bot->HasAura(SPELL_TWILIGHT_TORMENT_SARTHARION))
            return true;

        Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_SARTHARION);
        return boss && boss->HasAura(SPELL_GIFT_OF_TWILIGHT_FIRE);
    }

    // The acolyte a twilight-realm runner should kill: Shadron's first, then Vesperon's. Perception-
    // limited ("possible targets no los"), so it only returns an acolyte the runner can actually
    // reach. Null means the runner's work in the realm is done.
    inline Unit* FindTwilightRealmAcolyte(PlayerbotAI* botAI)
    {
        if (Unit* acolyte = GetFirstAliveUnitByEntry(botAI, NPC_ACOLYTE_OF_SHADRON))
            return acolyte;
        return GetFirstAliveUnitByEntry(botAI, NPC_ACOLYTE_OF_VESPERON);
    }

    // Bots allowed to run the twilight realm. Capped and role-stable so the raid isn't emptied into
    // the portal every cycle: tanks never run (main tank holds the boss, off-tank holds the drakes),
    // and only the first assist ranged DPS runs (plus the second in 25-man).
    inline bool IsTwilightRealmRunner(PlayerbotAI* botAI, Player* bot)
    {
        if (botAI->IsTank(bot))
            return false;

        if (PlayerbotAI::IsAssistRangedDpsOfIndex(bot, 0))
            return true;

        Difficulty const diff = bot->GetRaidDifficulty();
        bool const is25Man = diff == RAID_DIFFICULTY_25MAN_NORMAL || diff == RAID_DIFFICULTY_25MAN_HEROIC;
        return is25Man && PlayerbotAI::IsAssistRangedDpsOfIndex(bot, 1);
    }

    // Lock a drake onto the off-tank so all landed drakes can be held at one spot. Mirrors the
    // RS force-threat pattern: inject overwhelming threat + fixate, guarded so it is not re-issued
    // once the drake is already on the bot.
    inline void ForceThreat(Unit* target, Player* bot)
    {
        if (!target || target->GetVictim() == bot)
            return;

        ThreatManager& mgr = target->GetThreatMgr();
        mgr.AddThreat(bot, 1000000.0f, nullptr, true, true);
        mgr.FixateTarget(bot);
    }
}

#endif
