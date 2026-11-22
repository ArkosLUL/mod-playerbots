#include "ToCActions_Anubarak.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "LastMovementValue.h"
#include "Player.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCData.h"
#include "ToCHelpers_Anubarak.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

namespace
{

constexpr uint32 ANUBARAK_STALL_MS = 500;
constexpr uint32 SPELL_CAT_FORM = 768;
constexpr float ANUBARAK_RANGED_REACH_MARGIN = 2.0f;
constexpr float ANUBARAK_BOOKED_TOLERANCE = 0.5f;
constexpr float ANUBARAK_ESCORT_REAIM = 3.0f;
constexpr float ANUBARAK_SIDE_SHOUT_REACH = 10.0f;
constexpr uint32 ANUBARAK_COLD_BUBBLE_CANCEL_MS = 3000;

constexpr uint32 SPELL_DEMONIC_CIRCLE_SUMMON = 48018;

char const* const PENETRATING_COLD_HEALS[] = {
    "greater heal", "flash heal", "penance",
    "healing touch", "nourish", "regrowth",
    "holy light", "flash of light", "holy shock",
    "lesser healing wave", "healing wave", "riptide",
};

char const* const CLASS_TAUNTS[] = {"taunt", "hand of reckoning", "dark command", "growl"};
char const* const BURROWER_SHOUTS[] = {"challenging roar", "challenging shout"};
char const* const BURROWER_CLOSERS[] = {"feral charge - bear", "intercept", "avenger's shield", "death grip"};

void ReleaseStalledWalk(PlayerbotAI* botAI, Player* bot)
{
    LastMovement& last = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    if (!bot->isMoving() && last.msTime && getMSTimeDiff(last.msTime, getMSTime()) > ANUBARAK_STALL_MS)
        last.clear();
}

bool BookedOn(LastMovement const& last, Position const& spot)
{
    return last.msTime && std::fabs(last.lastMoveToX - spot.GetPositionX()) <= ANUBARAK_BOOKED_TOLERANCE &&
           std::fabs(last.lastMoveToY - spot.GetPositionY()) <= ANUBARAK_BOOKED_TOLERANCE;
}

void DropForeignBooking(LastMovement& last, Position const& own, MovementPriority upTo)
{
    if (last.msTime && last.priority <= upTo && !BookedOn(last, own))
        last.clear();
}

void BreakChannelPinningTheFeet(Player* bot)
{
    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);
}

bool InSpellReach(PlayerbotAI* botAI, Player* bot, Unit* target, char const* spell, float& stop)
{
    uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get();
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);

    if (!info || !info->RangeEntry || info->RangeEntry->ID == 1 || (info->RangeEntry->Flags & SPELL_RANGE_MELEE))
    {
        stop = CONTACT_DISTANCE;
        return bot->IsWithinMeleeRange(target);
    }

    float const range = bot->GetSpellMaxRangeForTarget(target, info);
    stop = std::max(range - ANUBARAK_RANGED_REACH_MARGIN, CONTACT_DISTANCE);
    return bot->IsWithinCombatRange(target, range) && bot->IsWithinLOSInMap(target);
}

bool TryClassTaunt(PlayerbotAI* botAI, Player* bot, Unit* target)
{
    for (char const* taunt : CLASS_TAUNTS)
    {
        float stop = 0.0f;
        if (botAI->CanCastSpell(taunt, target) && InSpellReach(botAI, bot, target, taunt, stop))
            return CastClassTaunt(botAI, target);
    }

    return false;
}

bool TauntLands(Unit* victim, float dist, bool swarm)
{
    return dist <= ANUBARAK_TAUNT_ARRIVE ||
           (!swarm && victim && victim->GetHealthPct() < ANUBARAK_TAUNT_RESCUE_PCT);
}

bool TryBurrowerPickup(PlayerbotAI* botAI, Player* bot, AnubarakBurrowerPick const& pick, bool& tooFar)
{
    tooFar = false;
    float stop = 0.0f;
    bool const swarm = GetAnubarakPhase(botAI) == AnubarakPhase::Swarm;
    Creature* boss = GetAnubarak(bot);
    bool const bossOnRescue = boss && pick.rescue && boss->GetVictim() == pick.rescue;
    if (pick.rescue && pick.rescue != bot && !bossOnRescue && botAI->CanCastSpell("righteous defense", pick.rescue) &&
        InSpellReach(botAI, bot, pick.rescue, "righteous defense", stop))
    {
        if (TauntLands(pick.rescue, pick.rescueFar, swarm))
            return botAI->CastSpell("righteous defense", pick.rescue);

        tooFar = true;
    }

    bool const bossClear =
        !boss || bot->GetExactDist2d(boss) > ANUBARAK_SIDE_SHOUT_REACH + boss->GetCombatReach();
    if (pick.looseNear >= 2 && !pick.otherNear && bossClear)
        for (char const* shout : BURROWER_SHOUTS)
            if (botAI->CanCastSpell(shout, bot))
                return botAI->CastSpell(shout, bot);

    bool const lands = TauntLands(pick.loose->GetVictim(), bot->GetExactDist2d(pick.loose), swarm);
    if (lands && TryClassTaunt(botAI, bot, pick.loose))
        return true;

    for (char const* taunt : CLASS_TAUNTS)
        if (!lands && botAI->CanCastSpell(taunt, pick.loose))
            tooFar = true;

    AnubarakGripDuty grip;
    bool const gripOwed = GetAnubarakGripDuty(bot, grip);
    for (char const* closer : BURROWER_CLOSERS)
    {
        if (gripOwed && std::strcmp(closer, "death grip") == 0)
            continue;

        if (botAI->CanCastSpell(closer, pick.loose) && InSpellReach(botAI, bot, pick.loose, closer, stop))
            return botAI->CastSpell(closer, pick.loose);
    }

    return false;
}

bool WalkInFlight(PlayerbotAI* botAI, Player* bot, Position const& spot)
{
    ReleaseStalledWalk(botAI, bot);
    LastMovement& last = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    return bot->isMoving() && BookedOn(last, spot);
}

}

bool AnubarakKiterImmunityAction::Execute(Event /*event*/)
{
    std::vector<char const*> immunities;
    switch (bot->getClass())
    {
        case CLASS_PALADIN:
            if (AnubarakKiterKeepsMark(bot))
                immunities = {"hand of protection", "divine shield"};
            else
                immunities = {"divine shield", "hand of protection"};
            break;
        case CLASS_MAGE:
            immunities = {"ice block"};
            break;
        case CLASS_ROGUE:
            immunities = {"vanish"};
            break;
        case CLASS_HUNTER:
            immunities = {"feign death"};
            break;
        default:
            break;
    }

    for (char const* immunity : immunities)
    {
        if (!botAI->CanCastSpell(immunity, bot))
            continue;

        if (bot->getClass() == CLASS_HUNTER && bot->isMoving())
            bot->StopMoving();

        return botAI->CastSpell(immunity, bot);
    }

    return false;
}

bool AnubarakProtectKiterAction::Execute(Event /*event*/)
{
    Player* kiter = GetAnubarakKiterToProtect(bot);
    return kiter && botAI->CastSpell("hand of protection", kiter);
}

bool AnubarakFreeKiterAction::Execute(Event /*event*/)
{
    Player* kiter = GetAnubarakKiterToFree(bot);
    return kiter && botAI->CastSpell("hand of freedom", kiter);
}

bool AnubarakHopEscortAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetAnubarakEscortSpot(bot, spot))
        return false;

    Position const walk = GetAnubarakPatchSafeLeg(bot, spot);
    ReleaseStalledWalk(botAI, bot);
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    if (bot->isMoving() && last.msTime &&
        walk.GetExactDist2d(last.lastMoveToX, last.lastMoveToY) <= ANUBARAK_ESCORT_REAIM)
        return true;

    BreakChannelPinningTheFeet(bot);
    return MoveTo(bot->GetMapId(), walk.GetPositionX(), walk.GetPositionY(), walk.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true);
}

Player* AnubarakRedirectThreatAction::GetRedirectTank()
{
    AnubarakRedirect redirect;
    return GetAnubarakRedirect(bot, redirect) ? redirect.tank : nullptr;
}

Unit* AnubarakRedirectThreatAction::GetThreatDumpTarget()
{
    AnubarakRedirect redirect;
    return GetAnubarakRedirect(bot, redirect) ? redirect.dump : nullptr;
}

bool AnubarakKiteSpikeToPermafrostAction::Execute(Event /*event*/)
{
    AnubarakKiteBoost boost;
    GetAnubarakKiteStage(bot, &boost);
    if (boost.nitro)
    {
        if (UseNitroBoosts(bot))
            return true;
    }
    else if (boost.spellId)
    {
        if (boost.face)
            bot->SetOrientation(boost.facing);

        bool shifted = false;
        if (boost.catForm && botAI->CanCastSpell(SPELL_CAT_FORM, bot))
        {
            bot->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
            shifted = botAI->CastSpell(SPELL_CAT_FORM, bot);
        }

        Unit* target = boost.target.IsEmpty() ? bot : botAI->GetUnit(boost.target);
        if ((!boost.catForm || shifted) && target && botAI->CastSpell(boost.spellId, target))
            return true;

        if (shifted)
            return true;
    }

    AnubarakKiteStand next;
    if (!GetAnubarakKiteStand(bot, next))
        return false;

    if (next.branch == AnubarakKiteBranch::Hold)
        return false;

    BreakChannelPinningTheFeet(bot);
    ReleaseStalledWalk(botAI, bot);

    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    MovementPriority const held = last.priority;
    bool const lowered = held == MovementPriority::MOVEMENT_FORCED &&
                         last.lastMoveShort.GetExactDist(next.stand.GetPositionX(), next.stand.GetPositionY(),
                                                         next.stand.GetPositionZ()) > 0.01f;
    if (lowered)
        last.priority = MovementPriority::MOVEMENT_COMBAT;

    if (MoveTo(bot->GetMapId(), next.stand.GetPositionX(), next.stand.GetPositionY(), next.stand.GetPositionZ(),
               false, false, false, false, MovementPriority::MOVEMENT_FORCED, true))
        return true;

    if (lowered)
        last.priority = held;

    return false;
}

bool AnubarakAvoidSpikeAction::Execute(Event /*event*/)
{
    Position spot;
    if (GetAnubarakDodgeHold(bot, spot))
    {
        if (bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) <= ANUBARAK_DODGE_ARRIVE)
            return false;

        if (bot->isMoving() && BookedOn(AI_VALUE(LastMovement&, "last movement"), spot))
            return true;

        return WalkToDodge(spot);
    }

    if (!GetAnubarakSpikeDodgeSpot(bot, spot))
        return false;

    SetAnubarakDodgeHold(bot, spot);
    return WalkToDodge(spot);
}

bool AnubarakAvoidSpikeAction::WalkToDodge(Position const& spot)
{
    BreakChannelPinningTheFeet(bot);
    ReleaseStalledWalk(botAI, bot);

    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    MovementPriority const held = last.priority;
    bool const lowered = held == MovementPriority::MOVEMENT_FORCED && !BookedOn(last, spot);
    if (lowered)
        last.priority = MovementPriority::MOVEMENT_COMBAT;

    bool const issued = MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false,
                               false, false, false, MovementPriority::MOVEMENT_FORCED, true);
    if (!issued && lowered)
        last.priority = held;

    return issued || bot->isMoving();
}

bool AnubarakInterruptShadowStrikeAction::Execute(Event /*event*/)
{
    Creature* caster = GetAnubarakShadowStrikeDuty(bot);
    if (!caster)
        return false;

    char const* interrupt = AnubarakReadyInterrupt(bot, caster);
    if (!interrupt)
        return false;

    float stop = 0.0f;
    if (!InSpellReach(botAI, bot, caster, interrupt, stop))
        return MoveTo(caster, stop, MovementPriority::MOVEMENT_COMBAT);

    return botAI->CastSpell(interrupt, caster);
}

bool AnubarakTankDefensiveAction::Execute(Event /*event*/)
{
    bool const physical = _physicalOnly && GetAnubarakPhase(botAI) != AnubarakPhase::Swarm;
    uint32 const defensive = NextTankDefensive(botAI, bot, "anub.defensive", physical);
    return defensive && botAI->CastSpell(defensive, bot);
}

bool AnubarakColdSelfSaveAction::Execute(Event /*event*/)
{
    if (_bubbleSpell && bot->HasAura(_bubbleSpell) &&
        getMSTimeDiff(_bubbleMs, getMSTime()) < ANUBARAK_COLD_BUBBLE_CANCEL_MS)
    {
        bot->RemoveAurasDueToSpell(_bubbleSpell);
        _bubbleSpell = 0;
        return true;
    }

    uint32 const save = GetAnubarakColdSelfSave(botAI);
    if (!save || !botAI->CastSpell(save, bot))
        return false;

    _bubbleSpell = IsAnubarakColdBubble(save) ? save : 0;
    _bubbleMs = getMSTime();
    return true;
}

bool AnubarakDestroyFrostSphereAction::Execute(Event /*event*/)
{
    Creature* sphere = GetAnubarakSphereToShoot(bot);
    if (!sphere)
        return false;

    if (Attack(sphere))
        return true;

    if (AnubarakSphereInReach(bot, sphere))
        return false;

    Position stand;
    if (bot->IsMovementPreventedByCasting() || IsAnubarakDodgeHeld(botAI) ||
        !GetAnubarakSphereStand(bot, sphere, stand))
        return false;

    ReleaseStalledWalk(botAI, bot);
    return MoveTo(bot->GetMapId(), stand.GetPositionX(), stand.GetPositionY(), stand.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool AnubarakDpsTargetAction::Execute(Event /*event*/) { return AttackDirect(GetAnubarakDpsTarget(botAI)); }

bool AnubarakAssistTankHoldBurrowerAction::Execute(Event /*event*/)
{
    AnubarakBurrowerPick const pick = GetAnubarakBurrowerPick(botAI);
    Creature* target = pick.held ? pick.held : pick.loose;
    if (!target)
        return false;

    bool tooFar = false;
    if (pick.loose && TryBurrowerPickup(botAI, bot, pick, tooFar))
    {
        RaidObs::NoteDerived(bot, "anub.sidetank", "pickup");
        return true;
    }

    RaidObs::NoteDerived(bot, "anub.sidetank", tooFar ? "far" : pick.held ? "hold" : "pickup");
    if (Attack(target))
        return true;

    if (target != pick.held)
        return false;

    if (pick.heldTaunted && !bot->IsWithinMeleeRange(pick.held))
        return false;

    Position const& hold = pick.hold;
    if (bot->GetExactDist2d(hold.GetPositionX(), hold.GetPositionY()) <= ANUBARAK_TANK_PATCH_ARRIVE ||
        AnubarakSpikeReachesFirst(bot, hold))
        return false;

    return MoveTo(bot->GetMapId(), hold.GetPositionX(), hold.GetPositionY(), hold.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool AnubarakSideTankAreaThreatAction::Execute(Event /*event*/)
{
    AnubarakSideTankAoe aoe;
    if (!GetAnubarakSideTankAoe(botAI, aoe))
        return false;

    if (!aoe.point)
        return botAI->CastSpell(aoe.spell, aoe.target);

    uint32 const spellId = AI_VALUE2(uint32, "spell id", aoe.spell);
    return spellId && botAI->CastSpell(spellId, aoe.pos.GetPositionX(), aoe.pos.GetPositionY(), aoe.pos.GetPositionZ());
}

bool AnubarakDropCleaveBuffAction::Execute(Event /*event*/)
{
    char const* buff = GetAnubarakCleaveBuffToDrop(botAI);
    if (!buff)
        return false;

    botAI->RemoveAura(buff);
    return true;
}

bool AnubarakSideTankReadyAction::Execute(Event /*event*/)
{
    int8 const side = GetAnubarakBurrowerTankSide(bot);
    if (side < 0)
        return false;

    RaidObs::NoteDerived(bot, "anub.sidetank", "ready");

    for (Unit* attacker : bot->getAttackers())
        if (attacker->GetEntry() == static_cast<uint32>(ToCNpcs::NPC_SWARM_SCARAB) && Attack(attacker))
            return true;

    Position const hold = GetAnubarakSideTankHold(bot, static_cast<uint8>(side));
    if (bot->GetExactDist2d(hold.GetPositionX(), hold.GetPositionY()) <= ANUBARAK_TANK_PATCH_ARRIVE ||
        AnubarakSpikeReachesFirst(bot, hold) || WalkInFlight(botAI, bot, hold))
        return false;

    return MoveTo(bot->GetMapId(), hold.GetPositionX(), hold.GetPositionY(), hold.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool AnubarakGripFrostSphereAction::Execute(Event /*event*/)
{
    AnubarakGripDuty duty;
    if (!GetAnubarakGripDuty(bot, duty))
        return false;

    switch (duty.step)
    {
        case AnubarakGripStep::Kill:
            return duty.sphere && Attack(duty.sphere);
        case AnubarakGripStep::Walk:
        {
            Position const& spot = duty.spot;
            if (WalkInFlight(botAI, bot, spot))
                return true;

            return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false,
                          false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
        }
        case AnubarakGripStep::Cast:
        {
            Creature* sphere = duty.sphere;
            if (bot->GetExactDist(sphere) > ANUBARAK_GRIP_RANGE || !bot->IsWithinLOSInMap(sphere) ||
                !botAI->CanCastSpell("death grip", sphere))
                return false;

            if (bot->isMoving())
                bot->StopMoving();

            if (!botAI->CastSpell("death grip", sphere))
                return false;

            TrackAnubarakGripCast(bot, sphere, duty.side);
            return true;
        }
        default:
            return false;
    }
}

bool AnubarakBattleResSideTankAction::Execute(Event /*event*/)
{
    Player* dead = GetAnubarakBattleResTarget(botAI);
    if (!dead)
        return false;

    if (bot->GetExactDist(dead) > ANUBARAK_BATTLE_RES_REACH || !bot->IsWithinLOSInMap(dead))
        return !IsAnubarakDodgeHeld(botAI) && MoveTo(dead, ANUBARAK_BATTLE_RES_REACH - ANUBARAK_RANGED_REACH_MARGIN,
                                                     MovementPriority::MOVEMENT_COMBAT);

    return botAI->CastSpell("rebirth", dead);
}

bool AnubarakGatherAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetAnubarakGatherSpot(bot, spot))
        return false;

    if (GetAnubarakBurrowerTankSide(bot) >= 0)
        RaidObs::NoteDerived(bot, "anub.sidetank", "gather");

    if (bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) <= ANUBARAK_GATHER_ARRIVE)
    {
        RaidObs::NoteDerived(bot, "anub.gather", "hold");
        return false;
    }

    Position const walk = GetAnubarakPatchSafeLeg(bot, ValidateFloorPoint(bot, spot));
    if (!AnubarakWalkClearOfSpike(bot, spot))
    {
        RaidObs::NoteDerived(bot, "anub.gather", "wait");
        if (WalkInFlight(botAI, bot, walk) || WalkInFlight(botAI, bot, spot))
        {
            bot->StopMoving();
            AI_VALUE(LastMovement&, "last movement").clear();
        }

        return false;
    }

    RaidObs::NoteDerived(bot, "anub.gather", "walk");
    BreakChannelPinningTheFeet(bot);
    if (WalkInFlight(botAI, bot, walk))
        return true;

    return MoveTo(bot->GetMapId(), walk.GetPositionX(), walk.GetPositionY(), walk.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool AnubarakHealerStandAction::Execute(Event /*event*/)
{
    Position slot;
    if (!GetAnubarakHealerStand(bot, slot))
        return false;

    Position const walk = GetAnubarakPatchSafeLeg(bot, ValidateFloorPoint(bot, slot));
    if (AnubarakHealerStandWaits(bot))
    {
        if (WalkInFlight(botAI, bot, walk) || WalkInFlight(botAI, bot, slot))
        {
            bot->StopMoving();
            AI_VALUE(LastMovement&, "last movement").clear();
        }

        return false;
    }

    if (WalkInFlight(botAI, bot, walk))
        return true;

    return MoveTo(bot->GetMapId(), walk.GetPositionX(), walk.GetPositionY(), walk.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool AnubarakRangedStandAction::Execute(Event /*event*/)
{
    Position stand;
    if (!GetAnubarakRangedStand(bot, stand))
        return false;

    Position const walk = GetAnubarakPatchSafeLeg(bot, stand);
    if (WalkInFlight(botAI, bot, walk))
        return true;

    return MoveTo(bot->GetMapId(), walk.GetPositionX(), walk.GetPositionY(), walk.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool AnubarakLeavePermafrostAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetAnubarakPermafrostStepOut(bot, spot))
    {
        RaidObs::NoteDerived(bot, "anub.leave", "none");
        return false;
    }

    RaidObs::NoteDerived(bot, "anub.leave", "walk");
    BreakChannelPinningTheFeet(bot);
    if (WalkInFlight(botAI, bot, spot))
        return true;

    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool AnubarakMeleeBurrowerStandAction::Execute(Event /*event*/)
{
    Unit* target = AI_VALUE(Unit*, "current target");
    Creature* burrower = target ? target->ToCreature() : nullptr;
    Position stand;
    if (!burrower || !GetAnubarakMeleeBurrowerStand(bot, burrower, stand))
    {
        RaidObs::NoteDerived(bot, "anub.mstand", "none");
        return false;
    }

    RaidObs::NoteDerived(bot, "anub.mstand", "walk");
    Position const walk = GetAnubarakPatchSafeLeg(bot, stand);
    if (WalkInFlight(botAI, bot, walk))
        return true;

    return MoveTo(bot->GetMapId(), walk.GetPositionX(), walk.GetPositionY(), walk.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool AnubarakBurrowerLeadAction::Execute(Event /*event*/)
{
    AnubarakBurrowerLead lead;
    if (!GetAnubarakBurrowerLead(botAI, lead) || !lead.walk)
        return false;

    Position const to = ValidateFloorPoint(bot, lead.to);
    if (!AnubarakWalkClearOfSpike(bot, to))
        return false;

    Position const walk = GetAnubarakPatchSafeLeg(bot, to);
    ReleaseStalledWalk(botAI, bot);
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    if (bot->isMoving() && last.msTime &&
        walk.GetExactDist2d(last.lastMoveToX, last.lastMoveToY) <= ANUBARAK_LEAD_REAIM)
        return true;

    BreakChannelPinningTheFeet(bot);
    return MoveTo(bot->GetMapId(), walk.GetPositionX(), walk.GetPositionY(), walk.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool AnubarakKiteScarabAction::Execute(Event /*event*/)
{
    Position step;
    if (!GetAnubarakScarabKite(botAI, step))
        return false;

    ReleaseStalledWalk(botAI, bot);
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    if (bot->isMoving() && last.msTime &&
        step.GetExactDist2d(last.lastMoveToX, last.lastMoveToY) <= ANUBARAK_LEAD_REAIM)
        return true;

    BreakChannelPinningTheFeet(bot);
    return MoveTo(bot->GetMapId(), step.GetPositionX(), step.GetPositionY(), step.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool AnubarakTopUpManaAction::Execute(Event /*event*/)
{
    char const* spell = GetAnubarakManaTopUp(botAI);
    return spell && botAI->CastSpell(spell, bot);
}

bool AnubarakJudgeLightAction::Execute(Event /*event*/)
{
    Creature* boss = AnubarakLightJudgeDue(botAI) ? GetAnubarak(bot) : nullptr;
    return boss && botAI->CastSpell("judgement of light", boss);
}

bool AnubarakKiteCatDropAction::Execute(Event /*event*/)
{
    if (!AnubarakKiteCatDropDue(botAI))
        return false;

    botAI->RemoveShapeshift();
    return true;
}

bool AnubarakTankRescueAction::Execute(Event /*event*/)
{
    AnubarakTankRescue rescue;
    return GetAnubarakTankRescue(botAI, rescue) && botAI->CastSpell(rescue.spell, rescue.tank);
}

bool AnubarakLustAction::Execute(Event /*event*/)
{
    char const* lust = GetAnubarakLust(botAI);
    return lust && botAI->CastSpell(lust, bot);
}

bool AnubarakDemonicCircleAction::Execute(Event /*event*/)
{
    if (!AnubarakDemonicCircleDue(botAI))
        return false;

    Position spot;
    if (GetAnubarakCircleSpot(botAI, spot) &&
        bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) > ANUBARAK_CIRCLE_CENTRE_SLACK / 2.0f)
        return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                      false, true, MovementPriority::MOVEMENT_NORMAL, true);

    if (bot->isMoving())
        bot->StopMoving();

    return botAI->CastSpell(SPELL_DEMONIC_CIRCLE_SUMMON, bot);
}

bool AnubarakTankPickUpScarabAction::Execute(Event /*event*/)
{
    Creature* scarab = GetAnubarakScarabToPickUp(bot);
    if (!scarab)
        return false;

    bool const taunted = scarab->GetVictim() != bot && TryClassTaunt(botAI, bot, scarab);
    return Attack(scarab) || taunted;
}

bool AnubarakMainTankHoldBossAction::Execute(Event /*event*/)
{
    Creature* boss = GetAnubarak(bot);
    if (!boss || boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return false;

    Unit* victim = boss->GetVictim();
    Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
    bool const onNonTank = victim && victim != bot && !IsAnubarakTankPlayer(victimPlayer);
    if (onNonTank && TryClassTaunt(botAI, bot, boss))
    {
        RaidObs::NoteDerived(bot, "anub.pickup", "taunt");
        return true;
    }

    if (Attack(boss))
    {
        RaidObs::NoteDerived(bot, "anub.pickup", "attack");
        return true;
    }

    Position stepOut;
    if (GetAnubarakTankStepOut(bot, boss, stepOut))
    {
        ClearAnubarakDrag(bot);
        RaidObs::NoteDerived(bot, "anub.pickup", "stepout");
        if (WalkInFlight(botAI, bot, stepOut))
            return false;

        DropForeignBooking(AI_VALUE(LastMovement&, "last movement"), stepOut, MovementPriority::MOVEMENT_COMBAT);
        return MoveTo(bot->GetMapId(), stepOut.GetPositionX(), stepOut.GetPositionY(), stepOut.GetPositionZ(), false,
                      false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
    }

    Position point;
    bool const submerge = GetAnubarakSubmergeSpot(bot, point);
    std::vector<Position> const points =
        submerge ? GetAnubarakSubmergeDragPoints(bot, boss, point) : GetAnubarakDragPoints(bot, boss);
    if (points.empty())
    {
        ClearAnubarakDrag(bot);
        RaidObs::NoteDerived(bot, "anub.pickup", "hold");
        return false;
    }

    AnubarakDragLeg leg;
    for (Position const& candidate : points)
    {
        leg = GetAnubarakDragLeg(bot, boss, candidate, submerge);
        if (leg.step != AnubarakDragStep::None)
            break;
    }

    if (leg.step == AnubarakDragStep::None)
    {
        RaidObs::NoteDerived(bot, "anub.pickup", submerge ? "subhold" : "hold");
        return false;
    }

    RaidObs::NoteDerived(bot, "anub.pickup", leg.preStep ? "prestep" : submerge ? "submerge" : "drag");
    if (leg.step != AnubarakDragStep::Walk)
        return false;

    ReleaseStalledWalk(botAI, bot);
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    Position const& spot = leg.tankSpot;
    if (BookedOn(last, spot))
        return false;

    DropForeignBooking(last, spot, MovementPriority::MOVEMENT_COMBAT);
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true, true);
}

bool AnubarakHealPenetratingColdAction::Execute(Event /*event*/)
{
    Player* target = GetAnubarakPenetratingColdHealTarget(botAI);
    if (!target)
        return false;

    if (CastAnubarakColdCarrierCover(botAI, target))
        return true;

    if (target->GetHealthPct() >= ANUBARAK_PENETRATING_COLD_HEAL_PCT)
        return false;

    for (char const* heal : PENETRATING_COLD_HEALS)
    {
        if (botAI->CanCastSpell(heal, target))
            return botAI->CastSpell(heal, target);
    }

    return false;
}

bool AnubarakSwarmHealAction::Execute(Event /*event*/)
{
    AnubarakSwarmHealPick const pick = GetAnubarakSwarmHealPick(botAI);
    if (!pick.target)
        return false;

    if (pick.floor)
        return CastAnubarakSwarmFloor(botAI);

    if (pick.rescue && CastAnubarakColdRescue(botAI, pick.target))
        return true;

    if (pick.hot)
        return CastAnubarakSwarmHot(botAI, pick.target);

    if (AnubarakSwarmShieldsTank(bot, pick.target) && botAI->CanCastSpell("power word: shield", pick.target))
        return botAI->CastSpell("power word: shield", pick.target);

    if (CastAnubarakColdCarrierCover(botAI, pick.target))
        return true;

    return CastRaidThroughputHeal(botAI, pick.target,
                                  [](std::string const& heal) { return heal == "wild growth"; });
}

bool AnubarakAuraMasteryAction::Execute(Event /*event*/)
{
    return botAI->CastSpell("aura mastery", bot);
}

bool AnubarakAuraMasteryAction::isUseful() { return AnubarakAuraMasteryDue(botAI); }

bool AnubarakInnervateHealerAction::Execute(Event /*event*/)
{
    Player* healer = GetAnubarakInnervateTarget(botAI);
    return healer && botAI->CastSpell("innervate", healer);
}

bool AnubarakInnervateHealerAction::isUseful() { return AnubarakInnervateHealerDue(botAI); }

std::vector<NextAction> AnubarakInnervateHealerAction::getPrerequisites() { return {NextAction("caster form")}; }
