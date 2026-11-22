#include "ToCActions_FactionChampionsBall.h"

#include <string>

#include "EncounterHelpers.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "ToCHelpers_FactionChampions.h"
#include "ToCHelpers_FactionChampionsBall.h"
#include "ToCHelpers_FactionChampionsDefence.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
constexpr float STAND_ARRIVE = 1.5f;
constexpr float SAME_SPOT = 0.5f;
constexpr float ICE_REPLAN = 3.0f;

bool WalkingTo(Position const* walking, Position const& spot)
{
    return walking && walking->GetExactDist2d(&spot) < SAME_SPOT;
}
}

bool FactionChampionsIceTargetAction::Execute(Event /*event*/)
{
    Unit* champion = FactionChampionsIceTarget(botAI);
    if (!champion)
        return false;

    if (FactionChampionsCcProtected(botAI, champion))
    {
        bot->AttackStop();
        bot->SetSelection(ObjectGuid::Empty);
        if (Pet* pet = bot->GetPet())
            pet->AttackStop();

        context->GetValue<Unit*>("current target")->Set(nullptr);
        return true;
    }

    return AttackDirect(champion);
}

bool FactionChampionsChainsOfIceAction::Execute(Event /*event*/)
{
    uint32 spellId = 0;
    Unit* champion = FactionChampionsIceChains(botAI, spellId);
    return champion && botAI->CastSpell(spellId, champion);
}

bool FactionChampionsIceReachAction::Execute(Event /*event*/)
{
    Position stand;
    if (!FactionChampionsIceReach(botAI, stand))
    {
        DropWalk();
        return false;
    }

    if (WalkingTo(WalkInFlight(), stand))
        return true;

    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    if (bot->isMoving() && stand.GetExactDist2d(last.lastMoveToX, last.lastMoveToY) < ICE_REPLAN)
        return true;

    MovementPriority const held = last.priority;
    if (held > MovementPriority::MOVEMENT_NORMAL)
        last.priority = MovementPriority::MOVEMENT_NORMAL;

    if (WalkTo(stand))
        return true;

    last.priority = held;
    return false;
}

bool FactionChampionsCycloneStandAction::Execute(Event /*event*/)
{
    Position spot;
    if (!FactionChampionsCycloneStand(botAI, spot) || bot->GetExactDist2d(&spot) <= STAND_ARRIVE)
    {
        DropWalk();
        return false;
    }

    if (WalkingTo(WalkInFlight(), spot))
        return true;

    return WalkTo(spot);
}

bool FactionChampionsStepOffAction::Execute(Event /*event*/)
{
    Position spot;
    if (!FactionChampionsStepOff(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (WalkingTo(WalkInFlight(), spot))
        return true;

    return WalkTo(spot);
}

bool FactionChampionsChasedAction::Execute(Event /*event*/)
{
    Position spot;
    if (!FactionChampionsChased(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (WalkingTo(WalkInFlight(), spot))
        return true;

    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    if (!bot->HasAura(EncounterHelpers::NITRO_BOOSTS_SPEED_SPELL))
        EncounterHelpers::UseNitroBoosts(bot);

    return WalkTo(spot, MovementPriority::MOVEMENT_FORCED);
}

bool FactionChampionsKiteAction::Execute(Event /*event*/)
{
    Position spot;
    if (!FactionChampionsKite(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (WalkingTo(WalkInFlight(), spot))
        return true;

    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    return WalkTo(spot, MovementPriority::MOVEMENT_FORCED);
}

bool FactionChampionsSpeedBoostAction::Execute(Event /*event*/)
{
    uint32 const spellId = FactionChampionsSpeedBoost(botAI);
    return spellId && botAI->CastSpell(spellId, bot);
}

bool FactionChampionsOpenerAction::Execute(Event /*event*/)
{
    uint32 spellId = 0;
    Unit* target = FactionChampionsOpener(botAI, spellId);
    return target && botAI->CastSpell(spellId, target);
}

bool FactionChampionsEscapeAction::Execute(Event /*event*/)
{
    FactionChampionsEscapeCast cast;
    if (!FactionChampionsEscape(botAI, cast))
        return false;

    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    if (cast.face)
        bot->SetOrientation(cast.facing);

    if (!botAI->CastSpell(cast.spellId, cast.target))
        return false;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(cast.spellId);
    RaidObs::Note(bot, "fc.escape", info ? std::string(info->SpellName[0]) : std::to_string(cast.spellId));
    return true;
}

bool FactionChampionsDemonicCircleAction::Execute(Event /*event*/)
{
    return FactionChampionsCircleDue(botAI) && botAI->CastSpell(SPELL_DEMONIC_CIRCLE_SUMMON, bot);
}
