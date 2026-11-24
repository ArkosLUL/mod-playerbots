#include "UldActions_Algalon.h"

#include <cmath>
#include <vector>

#include "AiObjectContext.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Position.h"
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "Timer.h"
#include "UldData.h"
#include "UldEncounter_Algalon.h"
#include "Unit.h"

using namespace EncounterHelpers;

namespace
{
void BreakCastPinningTheFeet(PlayerbotAI* botAI, Player* bot)
{
    if (bot->IsMovementPreventedByCasting())
        botAI->RequestSpellInterrupt();
}

LastMovement& Booking(PlayerbotAI* botAI)
{
    return botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
}

void ReleaseStalledWalk(PlayerbotAI* botAI, Player* bot)
{
    LastMovement& last = Booking(botAI);
    if (!bot->isMoving() && last.msTime && getMSTimeDiff(last.msTime, getMSTime()) > ULDUAR_ALGALON_STALL_MS)
        last.clear();
}

void DropForeignBooking(PlayerbotAI* botAI, Position const& own, MovementPriority upTo)
{
    LastMovement& last = Booking(botAI);
    if (last.msTime && last.priority <= upTo &&
        std::hypot(last.lastMoveToX - own.GetPositionX(), last.lastMoveToY - own.GetPositionY()) > 0.5f)
        last.clear();
}

enum class AlgalonPathRule
{
    Clear,
    Field,
    Any,
};

Position FindAlgalonClearSpot(Player* bot, float markerClearance, AlgalonPathRule rule, Position const* prefer,
                              HazardSweepCache& sweep)
{
    auto const reachable = [bot, rule](float x, float y)
    {
        if (!AlgalonSpotInRoom(x, y))
            return false;

        switch (rule)
        {
            case AlgalonPathRule::Clear:
                return !AlgalonPathCrossesHole(bot, x, y);
            case AlgalonPathRule::Field:
                return !AlgalonPathCrossesHole(bot, x, y, ULDUAR_ALGALON_FIELD_PATH_CLEARANCE);
            default:
                return true;
        }
    };
    return FindNearestPositionClearOfHazards(bot, GetAlgalonDodgeHazards(bot, markerClearance),
                                             ULDUAR_ALGALON_DODGE_SEARCH_RADIUS, ULDUAR_ALGALON_SWEEP_STEP,
                                             ULDUAR_ALGALON_SWEEP_ANGLE_STEP, prefer, reachable, &sweep);
}

bool UseExplosionCover(PlayerbotAI* botAI, Player* bot)
{
    char const* cover = GetAlgalonExplosionCover(botAI);
    if (!cover || !botAI->CastSpell(cover, bot))
        return false;

    ClaimAlgalonExplosionCover(bot, cover);
    return true;
}

bool UseExplosionPersonal(PlayerbotAI* botAI, Player* bot)
{
    AlgalonPersonalPick const pick = GetAlgalonExplosionPersonal(botAI);
    if (!pick)
        return false;

    bool const used =
        pick.consumable ? botAI->DoSpecificAction(pick.consumable, Event(), true) : botAI->CastSpell(pick.spellId, bot);
    if (!used)
        return false;

    ClaimAlgalonExplosionPersonal(bot, pick);
    return true;
}

bool CastBackupTaunt(PlayerbotAI* botAI, Player* bot, Unit* boss)
{
    Unit* victim = boss->GetVictim();
    switch (bot->getClass())
    {
        case CLASS_PALADIN:
            return victim && victim != bot && victim->IsPlayer() &&
                   botAI->CanCastSpell("righteous defense", victim) && botAI->CastSpell("righteous defense", victim);
        case CLASS_WARRIOR:
            return bot->IsWithinDistInMap(boss, ULDUAR_ALGALON_AREA_TAUNT_RANGE) &&
                   botAI->CanCastSpell("challenging shout", bot) && botAI->CastSpell("challenging shout", bot);
        case CLASS_DRUID:
            return bot->IsWithinDistInMap(boss, ULDUAR_ALGALON_AREA_TAUNT_RANGE) &&
                   botAI->CanCastSpell("challenging roar", bot) && botAI->CastSpell("challenging roar", bot);
        default:
            return false;
    }
}
}

bool AlgalonBigBangSoakAction::Execute(Event /*event*/)
{
    if (AlgalonCanDisperse(bot))
    {
        if (bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            bot->InterruptSpell(CURRENT_CHANNELED_SPELL);

        return botAI->CastSpell("dispersion", bot);
    }

    uint32 const defensive = NextTankDefensive(botAI, bot, "algalon.defensive", true);
    return defensive && botAI->CastSpell(defensive, bot);
}

bool AlgalonBigBangExternalAction::Execute(Event /*event*/)
{
    Player* soaker = GetAlgalonBigBangSoaker(botAI);
    char const* external = GetAlgalonSoakerExternal(botAI, soaker);
    return external && botAI->CastSpell(external, soaker);
}

bool AlgalonSoakerPrepAction::Execute(Event /*event*/)
{
    Player* soaker = GetAlgalonBigBangSoaker(botAI);
    char const* layer = GetAlgalonSoakerPrep(botAI, soaker);
    return layer && botAI->CastSpell(layer, soaker);
}

bool AlgalonSoakerRescueAction::Execute(Event /*event*/)
{
    Player* soaker = GetAlgalonRescueTarget(botAI);
    char const* heal = GetAlgalonRescueSpell(botAI, soaker);
    return heal && botAI->CastSpell(heal, soaker);
}

bool AlgalonTankHealAction::Execute(Event /*event*/)
{
    AlgalonTankHealPick const pick = GetAlgalonTankHealPick(botAI);
    char const* heal = GetAlgalonTankHealSpell(botAI, pick);
    return heal && botAI->CastSpell(heal, pick.target);
}

bool AlgalonTankBeaconAction::Execute(Event /*event*/)
{
    Player* target = GetAlgalonBeaconTarget(botAI);
    return target && botAI->CastSpell("beacon of light", target);
}

bool AlgalonTankDefensiveAction::Execute(Event /*event*/)
{
    AlgalonTankCut const cut = GetAlgalonTankCut(botAI);
    if (!cut.spellId)
        return false;

    if (bot->IsNonMeleeSpellCast(false, true))
        bot->InterruptNonMeleeSpells(false);

    if (!botAI->CastSpell(cut.spellId, cut.target))
        return false;

    ClaimAlgalonTankCut(bot, cut);
    return true;
}

bool AlgalonExplosionCoverAction::Execute(Event /*event*/) { return UseExplosionCover(botAI, bot); }

bool AlgalonExplosionPersonalAction::Execute(Event /*event*/)
{
    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    return UseExplosionPersonal(botAI, bot);
}

bool AlgalonShadowAuraAction::Execute(Event /*event*/) { return botAI->CastSpell("shadow resistance aura", bot); }

bool AlgalonFireTotemAction::Execute(Event /*event*/) { return botAI->CastSpell("fire resistance totem", bot); }

bool AlgalonBigBangHideAction::Execute(Event /*event*/)
{
    Position shelter;
    if (!GetAlgalonShelter(bot, shelter))
        return false;

    if (bot->GetExactDist2d(shelter) <= ULDUAR_ALGALON_SHELTER_IN_RADIUS)
        return false;

    for (uint32 const immunity : {SPELL_ALGALON_ICE_BLOCK, SPELL_ALGALON_DIVINE_SHIELD, SPELL_ALGALON_CLOAK_OF_SHADOWS,
                                  SPELL_ALGALON_ANTI_MAGIC_SHELL})
        if (bot->HasAura(immunity))
            bot->RemoveAurasDueToSpell(immunity);

    BreakCastPinningTheFeet(botAI, bot);
    ReleaseStalledWalk(botAI, bot);
    if (!UseExplosionCover(botAI, bot))
        UseExplosionPersonal(botAI, bot);

    Position next;
    bool const clear = AlgalonHideRoute(bot, shelter, next);
    AlgalonBoostHideRun(bot, bot->GetExactDist2d(next) + next.GetExactDist2d(shelter) -
                                 ULDUAR_ALGALON_SHELTER_IN_RADIUS);
    if (clear || !AlgalonCosmicSmashThreatens(bot))
        DropForeignBooking(botAI, _walk, MovementPriority::MOVEMENT_FORCED);
    _walk = next;

    if (MoveTo(bot->GetMapId(), next.GetPositionX(), next.GetPositionY(), next.GetPositionZ(), false, false, false,
               false, MovementPriority::MOVEMENT_FORCED, true))
        return true;

    return bot->isMoving();
}

bool AlgalonCosmicSmashAction::Execute(Event /*event*/)
{
    char const* branch = "clear";
    HazardSweepCache sweep;
    Position anchor;
    Position const* prefer = GetAlgalonDodgeAnchor(bot, anchor) ? &anchor : nullptr;
    Position clear =
        FindAlgalonClearSpot(bot, ULDUAR_ALGALON_COSMIC_SMASH_CLEARANCE, AlgalonPathRule::Clear, prefer, sweep);
    if (clear == Position())
    {
        branch = "min";
        clear =
            FindAlgalonClearSpot(bot, ULDUAR_ALGALON_COSMIC_SMASH_MIN_CLEARANCE, AlgalonPathRule::Clear, prefer, sweep);
    }

    if (clear == Position())
    {
        branch = "field";
        clear =
            FindAlgalonClearSpot(bot, ULDUAR_ALGALON_COSMIC_SMASH_MIN_CLEARANCE, AlgalonPathRule::Field, prefer, sweep);
    }

    if (clear == Position())
    {
        branch = "cross";
        clear =
            FindAlgalonClearSpot(bot, ULDUAR_ALGALON_COSMIC_SMASH_MIN_CLEARANCE, AlgalonPathRule::Any, prefer, sweep);
    }

    if (clear == Position())
        branch = "none";

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "algalon.dodge", branch);

    if (clear == Position())
        return false;

    BreakCastPinningTheFeet(botAI, bot);
    ReleaseStalledWalk(botAI, bot);
    return MoveTo(bot->GetMapId(), clear.GetPositionX(), clear.GetPositionY(), clear.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_FORCED, true);
}

bool AlgalonLeaveBlackHoleAction::Execute(Event /*event*/)
{
    Position clear;
    HazardSweepCache sweep;
    Position anchor;
    Position const* prefer = GetAlgalonSlotAnchor(bot, anchor) ? &anchor : nullptr;
    for (AlgalonPathRule rule : {AlgalonPathRule::Clear, AlgalonPathRule::Field, AlgalonPathRule::Any})
    {
        clear = FindAlgalonClearSpot(bot, ULDUAR_ALGALON_COSMIC_SMASH_TRIGGER_RADIUS, rule, prefer, sweep);
        if (clear != Position())
            break;
    }

    if (clear == Position())
        return false;

    BreakCastPinningTheFeet(botAI, bot);
    ReleaseStalledWalk(botAI, bot);
    DropForeignBooking(botAI, _walk, MovementPriority::MOVEMENT_FORCED);
    if (MoveTo(bot->GetMapId(), clear.GetPositionX(), clear.GetPositionY(), clear.GetPositionZ(), false, false,
               false, false, MovementPriority::MOVEMENT_FORCED, true))
    {
        _walk = clear;
        return true;
    }

    return bot->isMoving();
}

bool AlgalonYieldBossAction::Execute(Event /*event*/)
{
    Unit* boss = GetAlgalon(botAI);
    if (!boss)
        return false;

    if (bot->GetVictim() == boss)
        bot->AttackStop();

    if (AI_VALUE(Unit*, "current target") == boss)
        context->GetValue<Unit*>("current target")->Set(nullptr);

    return true;
}

Player* AlgalonThreatRedirectAction::GetRedirectTank() { return GetAlgalonOpenerTank(botAI); }

Unit* AlgalonThreatRedirectAction::GetThreatDumpTarget()
{
    Unit* boss = GetAlgalon(botAI);
    return boss && !boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) ? boss : nullptr;
}

bool AlgalonTakeBossAction::Execute(Event /*event*/)
{
    Unit* boss = GetAlgalon(botAI);
    if (!boss)
        return false;

    bool const soaking = AlgalonBigBangLatched(bot) && GetAlgalonBigBangSoaker(botAI) == bot;
    LastMovement& last = Booking(botAI);
    if (soaking && last.msTime && AlgalonWalkEntersField(bot, last.lastMoveToX, last.lastMoveToY,
                                                         ULDUAR_ALGALON_SHELTER_RADIUS))
    {
        bot->StopMoving();
        last.clear();
    }

    if (boss->GetVictim() != bot && CastClassTaunt(botAI, boss))
    {
        // Attack before clearing the booking: it stops a walk booked below combat, and a dodge under way is forced.
        Attack(boss);
        last.clear();
        return true;
    }

    Unit* victim = boss->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    if (victim != bot && (!holder || !IsAlgalonSwapTank(holder) || soaking) && CastBackupTaunt(botAI, bot, boss))
    {
        Attack(boss);
        last.clear();
        return true;
    }

    if (AI_VALUE(Unit*, "current target") != boss)
        return Attack(boss);

    return false;
}

bool AlgalonConstellationTauntAction::Execute(Event /*event*/)
{
    Unit* constellation = GetAlgalonHandlerConstellation(bot);
    if (!constellation)
        return false;

    Position behind;
    bool const fromBehind = AlgalonKiteBehind(bot, constellation, behind);
    if (fromBehind && bot->GetExactDist2d(behind) > ULDUAR_ALGALON_SLOT_TOLERANCE)
    {
        Position next;
        if (!AlgalonWalkAround(bot, behind, next))
            return false;

        ReleaseStalledWalk(botAI, bot);
        return MoveTo(bot->GetMapId(), next.GetPositionX(), next.GetPositionY(), next.GetPositionZ(), false, false,
                      false, false, MovementPriority::MOVEMENT_COMBAT, true);
    }

    if (CastClassTaunt(botAI, constellation))
        return true;

    if (fromBehind)
        return false;

    if (AI_VALUE(Unit*, "current target") != constellation)
        return Attack(constellation);

    return false;
}

bool AlgalonOffTankTauntAction::Execute(Event /*event*/)
{
    Unit* constellation = GetAlgalonOffTankConstellation(bot);
    Unit* victim = constellation ? constellation->GetVictim() : nullptr;
    if (!victim || !CastClassTaunt(botAI, constellation))
        return false;

    if (RaidObs::Active())
        RaidObs::Note(bot, "algalon.offtaunt", RaidObs::DescribeAssignment(victim->GetGUID()));

    return true;
}

bool AlgalonDarkMatterTankAction::Execute(Event /*event*/)
{
    Unit* darkMatter = GetAlgalonLooseDarkMatter(bot);
    if (!darkMatter)
        return false;

    if (CastClassTaunt(botAI, darkMatter))
        return true;

    if (AI_VALUE(Unit*, "current target") != darkMatter)
        return Attack(darkMatter);

    return false;
}

bool AlgalonConstellationKiteAction::Execute(Event /*event*/)
{
    Unit* constellation = GetAlgalonHandlerConstellation(bot);
    Position spot;
    char const* stage = nullptr;
    if (!constellation)
        return false;

    if (!GetAlgalonKiteSpot(bot, constellation, spot, stage))
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "algalon.kite", "nohole");

        return false;
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "algalon.kite", stage);

    if (bot->GetExactDist2d(spot) <= ULDUAR_ALGALON_SLOT_TOLERANCE)
        return false;

    Position next;
    if (!AlgalonWalkAround(bot, spot, next))
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "algalon.kite", "stuck");

        return false;
    }

    ReleaseStalledWalk(botAI, bot);
    DropForeignBooking(botAI, _walk, MovementPriority::MOVEMENT_COMBAT);
    if (!MoveTo(bot->GetMapId(), next.GetPositionX(), next.GetPositionY(), next.GetPositionZ(), false, false, false,
                false, MovementPriority::MOVEMENT_COMBAT, true))
    {
        if (!bot->isMoving() && RaidObs::Active())
            RaidObs::NoteDerived(bot, "algalon.kite", "stuck");

        return false;
    }

    _walk = next;
    return true;
}

bool AlgalonStarTeamAction::Execute(Event /*event*/)
{
    Unit* star = GetAlgalonStarTarget(botAI);
    return star && Attack(star);
}

bool AlgalonStarMarkAction::Execute(Event /*event*/)
{
    Unit* star = GetAlgalonFocusStar(botAI);
    return star && MarkTargetWithStar(bot, star);
}

bool AlgalonRaidPositionAction::Execute(Event /*event*/)
{
    Position slot;
    bool waypoint = false;
    if (!TryGetAlgalonSlot(bot, slot, &waypoint))
    {
        _slotReached = false;
        return false;
    }

    float const distance = bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY());

    if (waypoint)
    {
        _slotReached = false;
        if (distance <= ULDUAR_ALGALON_SLOT_TOLERANCE)
            return false;

        return MoveTo(bot->GetMapId(), slot.GetPositionX(), slot.GetPositionY(), slot.GetPositionZ(), false, false,
                      false, true, MovementPriority::MOVEMENT_COMBAT, true);
    }

    if (_slotReached && distance > ULDUAR_ALGALON_SLOT_TOLERANCE * 2.0f)
        _slotReached = false;

    if (_slotReached || distance <= ULDUAR_ALGALON_SLOT_TOLERANCE)
    {
        _slotReached = true;
        return false;
    }

    return MoveTo(bot->GetMapId(), slot.GetPositionX(), slot.GetPositionY(), slot.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_COMBAT, true);
}
