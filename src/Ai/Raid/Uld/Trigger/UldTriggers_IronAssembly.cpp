#include "UldTriggers_IronAssembly.h"

#include "GameObject.h"
#include "Group.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "UldBossHelper.h"
#include "UldEncounter_IronAssembly.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"

using namespace EncounterHelpers;

bool IronAssemblyResetEncounterStateTrigger::IsActive()
{
    return IronAssemblyBotHasEncounterState(bot) && IronAssemblyEncounterStateIsStale(botAI);
}

bool IronAssemblyOverwhelmingPowerRunOutTrigger::IsActive()
{
    if (!IronAssemblyHasOverwhelmingPower(bot))
        return false;

    // Nothing to run from once the blast would land on nobody.
    return GetNearestPlayerInRadius(bot, ULDUAR_IRON_ASSEMBLY_MELTDOWN_CLEARANCE) != nullptr;
}

bool IronAssemblyLightningTendrilsTrigger::IsActive()
{
    Unit* brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR);
    if (!brundir || !IronAssemblyTendrilsActive(brundir))
        return false;

    // Gate on the hazard itself rather than a wider round number, so the reaction distance and the
    // radius it is protecting against can never drift apart.
    return bot->GetDistance2d(brundir) < ULDUAR_IRON_ASSEMBLY_TENDRILS_CLEARANCE;
}

bool IronAssemblyOverloadTrigger::IsActive()
{
    // Tanks hold through it. 20,000 nature is survivable in plate and lethal in cloth, and under the
    // normal kill order Brundir dies last - so he is the only member alive during his own Overloads,
    // his channel invincibility never applies, and a tank leaving would cost real uptime and let him
    // drift toward the raid.
    if (botAI->IsTank(bot))
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

    std::vector<Position> runes;
    GatherIronAssemblyRunesOfDeath(bot, runes);
    if (runes.empty())
        return false;

    // DANGER, not CLEARANCE: this asks whether the bot is being ticked, and the escape action it
    // fires takes it the extra yards. Testing at the clearance would re-fire on every yard of drift.
    return !IsIronAssemblyPositionClearOfRunes(bot->GetPosition(), runes,
                                               ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_DANGER_RADIUS);
}

bool IronAssemblyInterruptTrigger::IsActive()
{
    Unit* brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR);
    if (!brundir)
        return false;

    // Which cast is up and who owes it are one decision, made in the encounter helper so the trigger
    // and the trace cannot end up describing different elections.
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
    if (!TryGetIronAssemblyTankSpot(botAI, bot, spot))
        return false;

    return bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) >
           ULDUAR_IRON_ASSEMBLY_TANK_SPOT_TOLERANCE;
}

bool IronAssemblyOverwhelmingPowerSwapTrigger::IsActive()
{
    if (!IsSteelbreakerEmpowered(botAI))
        return false;

    Unit* steelbreaker = GetIronAssemblyMember(botAI, NPC_STEELBREAKER);
    if (!steelbreaker)
        return false;

    // Only the two designated swap partners (main tank + first assist tank) trade the boss.
    bool const isMainTank = botAI->IsMainTank(bot);
    bool const isFirstAssistTank = botAI->IsAssistTankOfIndex(bot, 0);
    if (!isMainTank && !isFirstAssistTank)
        return false;

    // bot must be the off-tank (not the one currently holding the boss).
    Unit* activeTank = steelbreaker->GetVictim();
    if (!activeTank || activeTank == bot)
        return false;

    Player* activeTankPlayer = activeTank->ToPlayer();
    if (!activeTankPlayer)
        return false;

    bool const partnerIsSwapTank = isMainTank ? PlayerbotAI::IsAssistTankOfIndex(activeTankPlayer, 0)
                                              : PlayerbotAI::IsMainTank(activeTankPlayer);
    if (!partnerIsSwapTank)
        return false;

    if (IronAssemblyHasOverwhelmingPower(bot))
        return false;

    return IronAssemblyHasOverwhelmingPower(activeTank);
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

bool IronAssemblyRuneOfPowerTrigger::IsActive()
{
    Unit* boss = IronAssemblyAssignedBoss(botAI, bot);
    if (!boss || !boss->HasAura(SPELL_RUNE_OF_POWER))
        return false;

    // Only the tank actually holding him can walk him anywhere.
    return boss->GetVictim() == bot;
}

bool IronAssemblyRuneOfPowerSoakTrigger::IsActive()
{
    Position rune;
    if (!TryGetIronAssemblyRuneOfPowerSoakSpot(botAI, bot, rune))
        return false;

    return bot->GetExactDist2d(rune.GetPositionX(), rune.GetPositionY()) >
           ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_RADIUS;
}

bool IronAssemblySetDpsPriorityTrigger::IsActive()
{
    // Tanks have their own assignment node; this one owns every other target in the fight.
    if (botAI->IsTank(bot))
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
