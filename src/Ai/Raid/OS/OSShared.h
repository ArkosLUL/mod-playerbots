/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSSHARED_H
#define PLAYERBOTS_OSSHARED_H

#include "OSTriggers.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "ThreatManager.h"

// Inline helpers shared across the Obsidian Sanctum (Sartharion) components. Header-only, RS-style,
// so no extra .cpp/context wiring is needed. All the difficulty logic funnels through here so the
// "leave N drakes alive" decision (AiPlayerbot.SartharionDrakesAlive) stays in one place.
namespace ObsidianSanctumHelpers
{
    // Keep-order: kept drakes are the first N of this list; the rest are killed, hardest-priority
    // (last of the list) first. Kill-order is therefore Vesperon -> Shadron -> Tenebron.
    inline constexpr uint32 keepOrder[3] = {NPC_TENEBRON, NPC_SHADRON, NPC_VESPERON};

    inline int32 DrakesToLeaveAlive() { return sPlayerbotAIConfig.sartharionDrakesAlive; }

    inline bool IsDrakeEntry(uint32 entry)
    {
        return entry == NPC_TENEBRON || entry == NPC_SHADRON || entry == NPC_VESPERON;
    }

    // A drake is kept when its position in keepOrder falls within the first N (N = drakes to leave).
    inline bool IsDrakeKept(uint32 entry)
    {
        int32 const leave = DrakesToLeaveAlive();
        for (int32 i = 0; i < leave && i < 3; ++i)
            if (keepOrder[i] == entry)
                return true;
        return false;
    }

    inline bool IsDrakeToKill(uint32 entry) { return IsDrakeEntry(entry) && !IsDrakeKept(entry); }

    // The acolyte NPC tied to a drake's portal, if any (Tenebron has none).
    inline uint32 AcolyteEntryFor(uint32 drakeEntry)
    {
        if (drakeEntry == NPC_SHADRON)
            return NPC_ACOLYTE_OF_SHADRON;
        if (drakeEntry == NPC_VESPERON)
            return NPC_ACOLYTE_OF_VESPERON;
        return 0;
    }

    inline bool AcolyteAliveFor(PlayerbotAI* botAI, uint32 drakeEntry)
    {
        uint32 const acolyte = AcolyteEntryFor(drakeEntry);
        return acolyte != 0 && GetFirstAliveUnitByEntry(botAI, acolyte) != nullptr;
    }

    // Gates Twilight Revenge (Gap I): killing a drake with its acolyte still up in the realm buffs
    // Sartharion massively. Tenebron has no acolyte so it is always clear.
    inline bool DrakeAcolyteClear(PlayerbotAI* botAI, uint32 drakeEntry)
    {
        return !AcolyteAliveFor(botAI, drakeEntry);
    }

    // Highest kill-priority living drake in the kill-set whose acolyte is already cleared.
    inline Unit* FindDrakeToKill(PlayerbotAI* botAI)
    {
        // Iterate kill-order (reverse of keepOrder): Vesperon, Shadron, Tenebron.
        for (int32 i = 2; i >= 0; --i)
        {
            uint32 const entry = keepOrder[i];
            if (!IsDrakeToKill(entry))
                continue;
            if (!DrakeAcolyteClear(botAI, entry))
                continue;
            if (Unit* drake = GetFirstAliveUnitByEntry(botAI, entry))
                return drake;
        }
        return nullptr;
    }

    inline Unit* FindTwilightAdd(PlayerbotAI* botAI)
    {
        // Kill eggs before they hatch, then whelps.
        if (Unit* egg = GetFirstAliveUnitByEntry(botAI, NPC_TWILIGHT_EGG))
            return egg;
        return GetFirstAliveUnitByEntry(botAI, NPC_TWILIGHT_WHELP);
    }

    inline Unit* FindLavaBlaze(PlayerbotAI* botAI)
    {
        return GetFirstAliveUnitByEntry(botAI, NPC_LAVA_BLAZE);
    }

    inline bool AnyTwilightPortalAcolyteAlive(PlayerbotAI* botAI)
    {
        return GetFirstAliveUnitByEntry(botAI, NPC_ACOLYTE_OF_SHADRON) != nullptr ||
               GetFirstAliveUnitByEntry(botAI, NPC_ACOLYTE_OF_VESPERON) != nullptr;
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
