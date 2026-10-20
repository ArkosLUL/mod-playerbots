#include "ToCActions_FactionChampionsBall.h"

#include <algorithm>

#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "ToCHelpers_FactionChampions.h"
#include "ToCHelpers_FactionChampionsBall.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
constexpr float PACK_TANK_ARRIVE = 2.0f;
constexpr float STAND_ARRIVE = 1.5f;
// a walk already headed here, so a reissue would only restart its spline
constexpr float SAME_SPOT = 0.5f;
// inside the AoE by this much, so a step by the champions doesn't leave them out
constexpr float OPENER_MARGIN = 1.0f;
constexpr uint32 OPENER_MIN_CHAMPIONS = 3;
// past the boost's own threshold, the run always boosts
constexpr float RUN_BOOST_DIST = 100.0f;

bool WalkingTo(Position const* walking, Position const& spot)
{
    return walking && walking->GetExactDist2d(&spot) < SAME_SPOT;
}

float SpellRadius(SpellInfo const* info, Unit* caster)
{
    float radius = 0.0f;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (info->Effects[i].IsEffect())
            radius = std::max(radius, info->Effects[i].CalcRadius(caster));

    return radius;
}

Unit* NearestPackChampion(PlayerbotAI* botAI, Player* bot)
{
    Unit* killTarget = FactionChampionsKillTarget(botAI);
    Unit* nearest = nullptr;
    for (Unit* champion : FactionChampionsInCombat(botAI))
        if (champion != killTarget && (!nearest || bot->GetExactDist2d(champion) < bot->GetExactDist2d(nearest)))
            nearest = champion;

    return nearest;
}
}  // namespace

bool FactionChampionsFrontlineReturnAction::Execute(Event /*event*/)
{
    Position centre;
    if (!FactionChampionsFrontlineHeld(botAI, centre) || bot->GetExactDist2d(&centre) <= PACK_TANK_ARRIVE)
    {
        DropWalk();
        return false;
    }

    if (WalkingTo(WalkInFlight(), centre))
        return true;

    FactionChampionsBoostWalk(bot, bot->GetExactDist2d(&centre));
    return WalkTo(centre);
}

bool FactionChampionsFrontlineAttackAction::Execute(Event /*event*/)
{
    Unit* champion = FactionChampionsFrontlineTarget(botAI);
    if (champion)
        FactionChampionsBoostWalk(bot, bot->GetExactDist2d(champion));

    return AttackDirect(champion);
}

bool FactionChampionsBallStandAction::Execute(Event /*event*/)
{
    Position spot;
    if (!FactionChampionsBallStand(botAI, spot) || bot->GetExactDist2d(&spot) <= STAND_ARRIVE)
    {
        DropWalk();
        return false;
    }

    if (WalkingTo(WalkInFlight(), spot))
        return true;

    // no Nitro Boosts here: ranged and healers keep them to run from a melee champion
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

    // a cast in progress pins the feet
    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    FactionChampionsBoostWalk(bot, RUN_BOOST_DIST);
    return WalkTo(spot, MovementPriority::MOVEMENT_FORCED);
}

bool FactionChampionsOpenerAction::Execute(Event /*event*/)
{
    uint32 spellId = 0;
    Position spot;
    SpellInfo const* info = FactionChampionsOpener(botAI, spellId, spot) ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info)
    {
        DropWalk();
        return false;
    }

    float const reach = SpellRadius(info, bot) - OPENER_MARGIN;
    if (FactionChampionsPackAround(botAI, reach) >= OPENER_MIN_CHAMPIONS)
    {
        DropWalk();
        // Intimidating Shout wants an enemy target, the rest go off round the caster
        Unit* target = info->NeedsExplicitUnitTarget() ? NearestPackChampion(botAI, bot) : bot;
        return target && botAI->CastSpell(spellId, target);
    }

    if (WalkingTo(WalkInFlight(), spot))
        return true;

    return WalkTo(spot);
}
