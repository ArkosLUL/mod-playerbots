#include "UldActions_IronAssembly.h"
#include "UldActions_Shared.h"

#include <cmath>

#include "AiObjectContext.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Position.h"
#include "EncounterHelpers.h"
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "ServerFacade.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldData.h"
#include "UldEncounter_IronAssembly.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"

using namespace EncounterHelpers;

bool IronAssemblyResetEncounterStateAction::Execute(Event /*event*/)
{
    ResetIronAssemblyEncounterState(bot, false);

    return false;
}

namespace
{

bool TryGetIronAssemblyStation(PlayerbotAI* botAI, Player* bot, Position& station)
{
    if (TryGetIronAssemblyRaidSpot(botAI, bot, station))
        return true;

    if (TryGetIronAssemblyTankSpot(botAI, bot, station))
        return true;

    if (Unit* focus = IronAssemblyFocusTarget(botAI))
    {
        station = focus->GetPosition();
        return true;
    }

    return false;
}

bool TryGetIronAssemblyEscapeSpot(PlayerbotAI* botAI, Player* bot, std::vector<HazardCircle> const& hazards,
                                  Position& spot)
{
    Position station;
    bool const hasStation = TryGetIronAssemblyStation(botAI, bot, station);

    Position const clear = FindNearestPositionClearOfHazards(
        bot, hazards, ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_SEARCH_RADIUS, ULDUAR_IRON_ASSEMBLY_ESCAPE_SWEEP_STEP,
        ULDUAR_IRON_ASSEMBLY_ESCAPE_SWEEP_ANGLE_STEP, hasStation ? &station : nullptr);
    if (clear == Position())
        return false;

    spot = clear;
    return true;
}

void AddIronAssemblyHazards(std::vector<Position> const& centres, float clearance,
                            std::vector<HazardCircle>& hazards)
{
    for (Position const& centre : centres)
        hazards.emplace_back(centre, clearance);
}

}

bool IronAssemblyLightningTendrilsAction::Execute(Event /*event*/)
{
    Unit* brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR);
    if (!brundir)
        return false;

    std::vector<HazardCircle> hazards = {{brundir->GetPosition(), ULDUAR_IRON_ASSEMBLY_TENDRILS_CLEARANCE}};

    Position spot;
    if (!TryGetIronAssemblyEscapeSpot(botAI, bot, hazards, spot))
        return false;

    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool IronAssemblyOverloadAction::Execute(Event /*event*/)
{
    Unit* brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR);
    if (!brundir)
        return false;

    std::vector<HazardCircle> hazards = {{brundir->GetPosition(), ULDUAR_IRON_ASSEMBLY_OVERLOAD_CLEARANCE}};

    std::vector<Position> const runes = GetIronAssemblyScan(botAI).RunesOfDeath();
    AddIronAssemblyHazards(runes, ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_CLEARANCE, hazards);

    Position spot;
    if (!TryGetIronAssemblyEscapeSpot(botAI, bot, hazards, spot))
        return false;

    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool IronAssemblyRuneOfDeathAction::Execute(Event /*event*/)
{
    std::vector<Position> const runes = GetIronAssemblyScan(botAI).RunesOfDeath();
    if (runes.empty())
        return false;

    std::vector<HazardCircle> hazards;
    AddIronAssemblyHazards(runes, ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_CLEARANCE, hazards);

    Position spot;
    if (!TryGetIronAssemblyEscapeSpot(botAI, bot, hazards, spot))
        return false;

    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool IronAssemblyInterruptAction::Execute(Event /*event*/)
{
    Unit* brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR);
    if (!brundir)
        return false;

    char const* interrupt = IronAssemblyReadyInterrupt(bot, brundir);
    if (!interrupt)
        return false;

    return botAI->CastSpell(interrupt, brundir);
}

bool IronAssemblyTankAssignmentAction::Execute(Event /*event*/)
{
    Unit* boss = IronAssemblyAssignedBoss(botAI, bot);
    if (!boss)
    {
        _spotReached = false;
        return false;
    }

    if (AI_VALUE(Unit*, "current target") != boss)
        return Attack(boss);

    Unit* const victim = boss->GetVictim();
    bool const shared = IsSteelbreakerEmpowered(botAI) && boss->GetEntry() == NPC_STEELBREAKER;
    bool const swap = shared && IronAssemblyMeltdownRoleFor(botAI, bot) == IronAssemblyMeltdownRole::Taunt;

    if (victim != bot && (swap || !(shared && IronAssemblyHeldByOtherTank(bot, boss))) && CastClassTaunt(botAI, boss))
        return true;

    Position spot;
    if (!TryGetIronAssemblyBossTankSpot(botAI, boss, spot))
        return false;

    float const distance = bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY());

    if (_spotReached && distance > ULDUAR_IRON_ASSEMBLY_TANK_SPOT_TOLERANCE * 2.0f)
        _spotReached = false;

    if (_spotReached || distance <= ULDUAR_IRON_ASSEMBLY_TANK_SPOT_TOLERANCE)
    {
        _spotReached = true;
        return false;
    }

    Position step;
    if (!TryGetIronAssemblyRouteStep(botAI, bot, spot, step))
        return false;

    return MoveTo(bot->GetMapId(), step.GetPositionX(), step.GetPositionY(), step.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

bool IronAssemblyShieldOfRunesAction::Execute(Event /*event*/)
{
    Unit* molgeim = GetIronAssemblyMember(botAI, NPC_MOLGEIM);
    if (!molgeim || !IronAssemblyShieldOfRunesUp(molgeim))
        return false;

    static std::vector<std::string> const dispels = {"spellsteal", "purge", "dispel magic"};
    for (std::string const& dispel : dispels)
        if (botAI->CanCastSpell(dispel, molgeim))
            return botAI->CastSpell(dispel, molgeim);

    return false;
}

bool IronAssemblyFusionPunchDispelAction::Execute(Event /*event*/)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    static std::vector<std::string> const dispels = {"dispel magic", "cleanse", "mass dispel"};

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != ULDUAR_MAP_ID)
            continue;

        if (!IronAssemblyHasFusionPunch(member))
            continue;

        for (std::string const& dispel : dispels)
            if (botAI->CanCastSpell(dispel, member))
                return botAI->CastSpell(dispel, member);
    }

    return false;
}

bool IronAssemblyRedirectThreatAction::isUseful()
{
    return bot->getClass() == CLASS_HUNTER || bot->getClass() == CLASS_ROGUE;
}

Player* IronAssemblyRedirectThreatAction::GetRedirectTank()
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Unit* focus = IronAssemblyFocusTarget(botAI);
    if (focus)
    {
        if (Unit* victim = focus->GetVictim())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || !member->IsAlive() || member == bot)
                    continue;

                if (PlayerbotAI::IsTank(member) && member == victim)
                    return member;
            }
        }
    }

    return GetGroupMainTank(bot);
}

bool IronAssemblyRedirectThreatAction::Execute(Event /*event*/)
{
    Player* tank = GetRedirectTank();
    if (!tank || tank == bot)
        return false;

    if (bot->getClass() == CLASS_ROGUE)
    {
        return botAI->CanCastSpell("tricks of the trade", tank) && botAI->CastSpell("tricks of the trade", tank);
    }

    if (botAI->CanCastSpell("misdirection", tank))
        return botAI->CastSpell("misdirection", tank);

    Unit* focus = IronAssemblyFocusTarget(botAI);
    if (focus && bot->HasAura(SPELL_MISDIRECTION) && botAI->CanCastSpell("steady shot", focus))
        return botAI->CastSpell("steady shot", focus);

    return false;
}

bool IronAssemblyRuneOfPowerMeleeAction::Execute(Event /*event*/)
{
    Position spot;
    if (!TryGetIronAssemblyMeleeRuneSpot(botAI, bot, spot))
        return false;

    Position step;
    if (!TryGetIronAssemblyRouteStep(botAI, bot, spot, step))
        return false;

    return MoveTo(bot->GetMapId(), step.GetPositionX(), step.GetPositionY(), step.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

bool IronAssemblyMeltdownRunOutAction::Execute(Event /*event*/)
{
    Position const point = IronAssemblyMeltdownPoint();

    Position step;
    if (!TryGetIronAssemblyRouteStep(botAI, bot, point, step))
        return false;

    IronAssemblyUseNitroBoosts(bot, bot->GetExactDist2d(point.GetPositionX(), point.GetPositionY()), "meltdown");

    return MoveTo(bot->GetMapId(), step.GetPositionX(), step.GetPositionY(), step.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool IronAssemblyMeltdownStepOutAction::Execute(Event /*event*/)
{
    Position spot;
    if (!TryGetIronAssemblyMeltdownStepOut(botAI, bot, spot))
        return false;

    Position step;
    if (!TryGetIronAssemblyRouteStep(botAI, bot, spot, step))
        return false;

    return MoveTo(bot->GetMapId(), step.GetPositionX(), step.GetPositionY(), step.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool IronAssemblyTankBattleRezAction::Execute(Event /*event*/)
{
    IronAssemblyBattleRez duty;
    if (!IronAssemblyBattleRezDuty(botAI, bot, duty))
        return false;

    if (duty.step == IronAssemblyBattleRezStep::Walk)
    {
        Position step;
        if (!TryGetIronAssemblyRouteStep(botAI, bot, duty.walkTo, step))
            return false;

        auto const walk = [&]()
        {
            return TryMoveTo(bot->GetMapId(), step.GetPositionX(), step.GetPositionY(), step.GetPositionZ(), false,
                             false, false, true, MovementPriority::MOVEMENT_COMBAT, true);
        };

        MoveOutcome outcome = walk();
        if (outcome == MoveOutcome::Duplicate && !bot->isMoving())
        {
            AI_VALUE(LastMovement&, "last movement").clear();
            outcome = walk();
        }

        return outcome == MoveOutcome::Issued;
    }

    if (bot->isMoving())
    {
        LastMovement& last = AI_VALUE(LastMovement&, "last movement");
        if (last.priority == MovementPriority::MOVEMENT_FORCED && last.lastdelayTime + last.msTime > getMSTime())
            return false;

        bot->StopMoving();
        last.clear();
    }

    botAI->RemoveShapeshift();
    return duty.spellId && botAI->CastSpell(duty.spellId, duty.tank);
}

bool IronAssemblyTankRebuffAction::Execute(Event /*event*/)
{
    IronAssemblyTankRebuff duty;
    if (!IronAssemblyTankRebuffDuty(botAI, bot, duty))
        return false;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(duty.spellId);
    if (info && info->CheckShapeshift(bot->GetShapeshiftForm()) != SPELL_CAST_OK)
        botAI->RemoveShapeshift();

    return botAI->CastSpell(duty.spellId, duty.tank);
}

bool IronAssemblyFusionPunchDefensiveAction::Execute(Event /*event*/)
{
    uint32 const needMs = IronAssemblyFusionPunchIncomingMs(botAI, bot);
    if (!needMs)
        return false;

    uint32 const defensive = NextMagicDefensive(botAI, bot, "ironassembly.punchdef", needMs);
    return defensive && botAI->CastSpell(defensive, bot);
}

bool IronAssemblyNatureTotemAction::Execute(Event /*event*/)
{
    switch (IronAssemblyNatureTotemNext(botAI))
    {
        case IronAssemblyNatureTotemStep::SetButton:
            return botAI->DoSpecificAction("set nature resistance totem", Event(), true);
        case IronAssemblyNatureTotemStep::Drop:
        {
            uint32 const totem = IronAssemblyNatureTotemSpell(bot);
            return totem && botAI->CastSpell(totem, bot);
        }
        default:
            return false;
    }
}

bool IronAssemblyUnheldTauntAction::Execute(Event /*event*/)
{
    Unit* member = IronAssemblyUnheldMemberToTaunt(botAI, bot);
    if (!member || !CastClassTaunt(botAI, member))
        return false;

    if (RaidObs::Active())
        RaidObs::Note(bot, "ironassembly.unheld", RaidObs::DescribeAssignment(member->GetGUID()));

    return true;
}

bool IronAssemblySetDpsPriorityAction::Execute(Event /*event*/)
{
    Unit* focus = IronAssemblyFocusTarget(botAI);
    if (!focus)
        return false;

    CommandPetAttack(botAI, focus);

    if (AI_VALUE(Unit*, "current target") == focus)
        return false;

    return Attack(focus);
}

bool IronAssemblyRaidPositionAction::Execute(Event /*event*/)
{
    Position spot;
    float tolerance = ULDUAR_IRON_ASSEMBLY_SLOT_TOLERANCE;
    if (!TryGetIronAssemblyRaidSpot(botAI, bot, spot, &tolerance))
    {
        _spotReached = false;
        return false;
    }

    float const distance = bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY());

    if (_spotReached && distance > tolerance * 2.0f)
        _spotReached = false;

    if (_spotReached || distance <= tolerance)
    {
        _spotReached = true;
        return false;
    }

    Position step;
    if (!TryGetIronAssemblyRouteStep(botAI, bot, spot, step))
        return false;

    if (IsSteelbreakerEmpowered(botAI))
        IronAssemblyUseNitroBoosts(bot, distance, "spread");

    return MoveTo(bot->GetMapId(), step.GetPositionX(), step.GetPositionY(), step.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_COMBAT, true);
}
