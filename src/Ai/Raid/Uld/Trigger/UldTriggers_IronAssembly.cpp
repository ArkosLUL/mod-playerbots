#include "UldTriggers_IronAssembly.h"

#include "GameObject.h"
#include "Group.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "RaidTankDefensive.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "UldData.h"
#include "UldEncounter_IronAssembly.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"

using namespace EncounterHelpers;

bool IronAssemblyResetEncounterStateTrigger::IsActive()
{
    return IronAssemblyBotHasEncounterState(bot) && IronAssemblyEncounterStateIsStale(botAI);
}

bool IronAssemblyLightningTendrilsTrigger::IsActive()
{
    Unit* brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR);
    if (!brundir || !IronAssemblyTendrilsActive(brundir))
        return false;

    return bot->GetDistance2d(brundir) < ULDUAR_IRON_ASSEMBLY_TENDRILS_CLEARANCE;
}

bool IronAssemblyOverloadTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot))
        return false;

    Unit* brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR);
    if (!brundir || !IronAssemblyOverloadActive(brundir))
        return false;

    return bot->GetDistance2d(brundir) < ULDUAR_IRON_ASSEMBLY_OVERLOAD_CLEARANCE;
}

bool IronAssemblyRuneOfDeathTrigger::IsActive()
{
    if (!IronAssemblyEncounterActive(botAI))
        return false;

    std::vector<Position> const runes = GetIronAssemblyScan(botAI).RunesOfDeath();
    if (runes.empty())
        return false;

    return !IsIronAssemblyPositionClearOfRunes(bot->GetPosition(), runes,
                                               ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_DANGER_RADIUS);
}

bool IronAssemblyInterruptTrigger::IsActive()
{
    Unit* brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR);
    if (!brundir)
        return false;

    return IronAssemblyInterruptDuty(botAI, bot, brundir) != nullptr;
}

bool IronAssemblyTankAssignmentTrigger::IsActive()
{
    if (!IronAssemblyFormationActive(botAI))
        return false;

    Unit* boss = IronAssemblyAssignedBoss(botAI, bot);
    if (!boss)
        return false;

    if (boss->GetVictim() != bot)
        return true;

    Position spot;
    if (!TryGetIronAssemblyBossTankSpot(botAI, boss, spot))
        return false;

    return bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) >
           ULDUAR_IRON_ASSEMBLY_TANK_SPOT_TOLERANCE;
}

bool IronAssemblyShieldOfRunesTrigger::IsActive()
{
    Unit* molgeim = GetIronAssemblyMember(botAI, NPC_MOLGEIM);
    if (!molgeim || !IronAssemblyShieldOfRunesUp(molgeim))
        return false;

    static std::vector<std::string> const dispels = {"spellsteal", "purge", "dispel magic"};
    for (std::string const& dispel : dispels)
        if (botAI->CanCastSpell(dispel, molgeim))
            return true;

    return false;
}

bool IronAssemblyFusionPunchDispelTrigger::IsActive()
{
    if (!GetIronAssemblyMember(botAI, NPC_STEELBREAKER))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return IronAssemblyHasFusionPunch(bot);

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != ULDUAR_MAP_ID)
            continue;

        if (IronAssemblyHasFusionPunch(member))
            return true;
    }

    return false;
}

bool IronAssemblyRedirectThreatTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE)
        return false;

    return IronAssemblyFocusTarget(botAI) != nullptr;
}

bool IronAssemblyRuneOfPowerMeleeTrigger::IsActive()
{
    if (!IronAssemblyFormationActive(botAI))
        return false;

    Position spot;
    return TryGetIronAssemblyMeleeRuneSpot(botAI, bot, spot);
}

bool IronAssemblyMeltdownRunOutTrigger::IsActive()
{
    return IronAssemblyMeltdownRoleFor(botAI, bot) == IronAssemblyMeltdownRole::Run;
}

bool IronAssemblyMeltdownStepOutTrigger::IsActive()
{
    Position spot;
    return TryGetIronAssemblyMeltdownStepOut(botAI, bot, spot);
}

bool IronAssemblyTankBattleRezTrigger::IsActive()
{
    IronAssemblyBattleRez duty;
    return IronAssemblyBattleRezDuty(botAI, bot, duty);
}

bool IronAssemblyTankRebuffTrigger::IsActive()
{
    IronAssemblyTankRebuff duty;
    return IronAssemblyTankRebuffDuty(botAI, bot, duty);
}

bool IronAssemblyFusionPunchDefensiveTrigger::IsActive()
{
    uint32 const needMs = IronAssemblyFusionPunchIncomingMs(botAI, bot);
    return needMs && NextMagicDefensive(botAI, bot, nullptr, needMs);
}

bool IronAssemblyNatureTotemTrigger::IsActive()
{
    return IronAssemblyNatureTotemNext(botAI) != IronAssemblyNatureTotemStep::None;
}

bool IronAssemblyUnheldTauntTrigger::IsActive()
{
    return IronAssemblyUnheldMemberToTaunt(botAI, bot) != nullptr;
}

bool IronAssemblySetDpsPriorityTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot))
        return false;

    if (!IronAssemblyFormationActive(botAI))
        return false;

    return IronAssemblyFocusTarget(botAI) != nullptr;
}

bool IronAssemblyRaidPositionTrigger::IsActive()
{
    if (!IronAssemblyFormationActive(botAI))
        return false;

    Position spot;
    return TryGetIronAssemblyRaidSpot(botAI, bot, spot);
}
