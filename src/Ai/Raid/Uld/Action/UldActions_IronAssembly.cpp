#include "UldActions_IronAssembly.h"
#include "UldActions_Shared.h"

#include <cmath>

#include "AiObjectContext.h"
#include "Group.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Position.h"
#include "EncounterHelpers.h"
#include "ServerFacade.h"
#include "UldData.h"
#include "UldEncounter_IronAssembly.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"

using namespace EncounterHelpers;

bool IronAssemblyResetEncounterStateAction::Execute(Event /*event*/)
{
    ResetIronAssemblyEncounterState(bot, false);

    // Never claims the tick: clearing state is bookkeeping, and the pull still needs every node below
    // this one to run on the same tick.
    return false;
}

namespace
{

// Where a bot stands when nothing is chasing it: its formation slot, its tank spot, or the boss the
// raid is killing.
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

// Shared shape for the hazard exits. FleePosition is deliberately not used: it clamps travel to
// AiPlayerbot.FleeDistance and cannot clear a 20 yd blast, and it only reads one hazard.
//
// Ties inside the first clear ring break toward the bot's station. Without that the sweep takes the
// first angle that passes, which is a fixed compass direction measured from wherever the bot happens
// to be standing - so the answer slides as the bot walks and a raid leaving a rune picks a new escape
// every tick instead of finishing the one it started. The station holds still while the bot moves,
// which is what makes the same answer keep winning.
bool TryGetIronAssemblyEscapeSpot(PlayerbotAI* botAI, Player* bot, std::vector<HazardCircle> const& hazards,
                                  Position& spot)
{
    Position station;
    bool const hasStation = TryGetIronAssemblyStation(botAI, bot, station);

    Position const clear = FindNearestPositionClearOfHazards(
        bot, hazards, ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_SEARCH_RADIUS, 2.0f,
        static_cast<float>(M_PI) / 8.0f, hasStation ? &station : nullptr);
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

}  // namespace

bool IronAssemblyOverwhelmingPowerRunOutAction::Execute(Event /*event*/)
{
    // Everyone else is the hazard here: Meltdown is centred on this bot, so the spot to find is one
    // far enough from the raid that the blast lands on nobody.
    std::vector<Position> crowd;

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || !member->IsAlive() || member->GetMapId() != ULDUAR_MAP_ID)
                continue;

            crowd.push_back(member->GetPosition());
        }
    }

    if (crowd.empty())
        return false;

    std::vector<HazardCircle> hazards;
    AddIronAssemblyHazards(crowd, ULDUAR_IRON_ASSEMBLY_MELTDOWN_CLEARANCE, hazards);

    Position spot;
    if (!TryGetIronAssemblyEscapeSpot(botAI, bot, hazards, spot))
        return false;

    // MOVEMENT_FORCED: these are the kill-you hazards, so nothing generic gets to argue about it.
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool IronAssemblyLightningTendrilsAction::Execute(Event /*event*/)
{
    Unit* brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR);
    if (!brundir)
        return false;

    // 2d on purpose: he is hovering while the tendrils tick, and the damage is projected on the floor
    // under him rather than measured to where he is floating.
    std::vector<HazardCircle> hazards = {{brundir->GetPosition(), ULDUAR_IRON_ASSEMBLY_TENDRILS_CLEARANCE}};

    Position spot;
    if (!TryGetIronAssemblyEscapeSpot(botAI, bot, hazards, spot))
        return false;

    // MOVEMENT_FORCED: these are the kill-you hazards, so nothing generic gets to argue about it.
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool IronAssemblyOverloadAction::Execute(Event /*event*/)
{
    Unit* brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR);
    if (!brundir)
        return false;

    std::vector<HazardCircle> hazards = {{brundir->GetPosition(), ULDUAR_IRON_ASSEMBLY_OVERLOAD_CLEARANCE}};

    // Runes of Death count too, or the escape from one hazard walks straight into the other. Each at
    // its own clearance: folding them in at Overload's would give up four more yards per rune than
    // they need, and every yard of it is walked in a 5.5s cast window.
    std::vector<Position> runes;
    GatherIronAssemblyRunesOfDeath(bot, runes);
    AddIronAssemblyHazards(runes, ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_CLEARANCE, hazards);

    Position spot;
    if (!TryGetIronAssemblyEscapeSpot(botAI, bot, hazards, spot))
        return false;

    // MOVEMENT_FORCED: these are the kill-you hazards, so nothing generic gets to argue about it.
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool IronAssemblyRuneOfDeathAction::Execute(Event /*event*/)
{
    std::vector<Position> runes;
    GatherIronAssemblyRunesOfDeath(bot, runes);
    if (runes.empty())
        return false;

    std::vector<HazardCircle> hazards;
    AddIronAssemblyHazards(runes, ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_CLEARANCE, hazards);

    Position spot;
    if (!TryGetIronAssemblyEscapeSpot(botAI, bot, hazards, spot))
        return false;

    // MOVEMENT_FORCED: these are the kill-you hazards, so nothing generic gets to argue about it.
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

    // Stand, do not drag. The tank walks to its spot and the boss follows it there, which never
    // routes a boss through the ranged stack the way a dragged pull does.
    if (boss->GetVictim() != bot && CastClassTaunt(botAI, boss))
        return true;

    Position spot;
    if (!TryGetIronAssemblyTankSpot(botAI, bot, spot))
        return false;

    float const distance = bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY());

    if (_spotReached && distance > ULDUAR_IRON_ASSEMBLY_TANK_SPOT_TOLERANCE * 2.0f)
        _spotReached = false;

    if (_spotReached || distance <= ULDUAR_IRON_ASSEMBLY_TANK_SPOT_TOLERANCE)
    {
        _spotReached = true;
        return false;
    }

    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

bool IronAssemblyOverwhelmingPowerSwapAction::Execute(Event event)
{
    Unit* steelbreaker = GetIronAssemblyMember(botAI, NPC_STEELBREAKER);
    if (!steelbreaker)
        return false;

    if (AI_VALUE(Unit*, "current target") != steelbreaker)
        return Attack(steelbreaker);

    if (steelbreaker->GetVictim() != bot)
        return botAI->DoSpecificAction("taunt spell", event, true);

    return false;
}

bool IronAssemblyShieldOfRunesAction::Execute(Event /*event*/)
{
    Unit* molgeim = GetIronAssemblyMember(botAI, NPC_MOLGEIM);
    if (!molgeim || !IronAssemblyShieldOfRunesUp(molgeim))
        return false;

    // Offensive magic dispel: spellsteal (mage), purge (shaman), dispel magic (priest). Stripping the
    // shield denies the +50% damage buff it pays out when it is drained instead.
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

    // Feed whoever actually holds the kill target. With one tank per member the group main tank is
    // wrong for two of the three, which is exactly the case the redirect rubric says to narrow rather
    // than veto - the old blanket veto left every hunter and rogue here with nothing to cast.
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

                if (botAI->IsTank(member) && member == victim)
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
        // Tricks redirects everything the rogue does for the next 6s, so there is no dump shot.
        return botAI->CanCastSpell("tricks of the trade", tank) && botAI->CastSpell("tricks of the trade", tank);
    }

    if (botAI->CanCastSpell("misdirection", tank))
        return botAI->CastSpell("misdirection", tank);

    // Misdirection only moves the threat of the next three shots. Spend them on the kill target
    // rather than leaving them to whatever the rotation picks.
    Unit* focus = IronAssemblyFocusTarget(botAI);
    if (focus && bot->HasAura(SPELL_MISDIRECTION) && botAI->CanCastSpell("steady shot", focus))
        return botAI->CastSpell("steady shot", focus);

    return false;
}

bool IronAssemblyRuneOfPowerSoakAction::Execute(Event /*event*/)
{
    Position rune;
    if (!TryGetIronAssemblyRuneOfPowerSoakSpot(botAI, bot, rune))
        return false;

    return MoveTo(bot->GetMapId(), rune.GetPositionX(), rune.GetPositionY(), rune.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

bool IronAssemblySetDpsPriorityAction::Execute(Event /*event*/)
{
    Unit* focus = IronAssemblyFocusTarget(botAI);
    if (!focus)
        return false;

    // Returning false once the bot is already on the right member is what lets the lower nodes run:
    // the engine ends the tick at the first action that succeeds.
    if (AI_VALUE(Unit*, "current target") == focus)
        return false;

    return Attack(focus);
}

bool IronAssemblyRaidPositionAction::Execute(Event /*event*/)
{
    Position spot;
    if (!TryGetIronAssemblyRaidSpot(botAI, bot, spot))
    {
        _spotReached = false;
        return false;
    }

    float const distance = bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY());

    // Reach then hold, with a deadband. Re-issuing a move on every yard of drift restarts the spline
    // and a moving bot cannot start a cast, so it slides on the spot and never casts. Yielding once
    // parked is also what keeps a bot available to interrupt Lightning Whirl.
    if (_spotReached && distance > ULDUAR_IRON_ASSEMBLY_SLOT_TOLERANCE * 2.0f)
        _spotReached = false;

    if (_spotReached || distance <= ULDUAR_IRON_ASSEMBLY_SLOT_TOLERANCE)
    {
        _spotReached = true;
        return false;
    }

    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_COMBAT, true);
}
