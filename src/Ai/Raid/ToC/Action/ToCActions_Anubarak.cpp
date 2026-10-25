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
constexpr float ANUBARAK_RANGED_REACH_MARGIN = 2.0f;
constexpr float ANUBARAK_BOOKED_TOLERANCE = 0.5f;

// No Ice Block: it roots the mage, and a still target hands the spike on at its current speed
char const* const KITER_SELF_IMMUNITIES[] = {"divine shield", "hand of protection"};
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

// MoveTo refuses a repeat of its last point for up to 5 s even once the bot has stopped (a potion's
// StopMoving, a knockback), so drop the booking when the bot stands still past the issue.
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

// A walk waits behind any booking of equal or higher priority until it runs out
void DropForeignBooking(LastMovement& last, Position const& own, MovementPriority upTo)
{
    if (last.msTime && last.priority <= upTo && !BookedOn(last, own))
        last.clear();
}

// A point move issued mid channel never starts its spline, though MoveTo still reports it issued
void BreakChannelPinningTheFeet(Player* bot)
{
    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);
}

// True when the bot can cast spell on target from where it stands. Otherwise stop is how close to
// walk first, edge to edge as MoveTo(target, distance) measures it.
bool InSpellReach(PlayerbotAI* botAI, Player* bot, Unit* target, char const* spell, float& stop)
{
    uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get();
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);

    // Self range covers War Stomp and Shockwave, which only reach what stands next to the bot
    if (!info || !info->RangeEntry || info->RangeEntry->ID == 1 || (info->RangeEntry->Flags & SPELL_RANGE_MELEE))
    {
        stop = CONTACT_DISTANCE;
        return bot->IsWithinMeleeRange(target);
    }

    float const range = bot->GetSpellMaxRangeForTarget(target, info);
    stop = std::max(range - ANUBARAK_RANGED_REACH_MARGIN, CONTACT_DISTANCE);
    return bot->IsWithinCombatRange(target, range) && bot->IsWithinLOSInMap(target);
}

// CastSpell selects the target, and faces it for a spell that needs facing, before the cast can fail.
// Gated so a taunt on cooldown doesn't do that every tick to a tank holding something else.
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

// A loose burrower is on a healer or ranged more often than not: Righteous Defense off whoever most of
// them hit, a shout for two or more close by, the class taunt, then a charge or pull
bool TryBurrowerPickup(PlayerbotAI* botAI, Player* bot, AnubarakBurrowerPick const& pick)
{
    float stop = 0.0f;
    if (pick.rescue && pick.rescue != bot && botAI->CanCastSpell("righteous defense", pick.rescue) &&
        InSpellReach(botAI, bot, pick.rescue, "righteous defense", stop))
        return botAI->CastSpell("righteous defense", pick.rescue);

    if (pick.looseNear >= 2 && !pick.otherNear)
        for (char const* shout : BURROWER_SHOUTS)
            if (botAI->CanCastSpell(shout, bot))
                return botAI->CastSpell(shout, bot);

    if (TryClassTaunt(botAI, bot, pick.loose))
        return true;

    // A DK owed a grip keeps Death Grip for the side's tank patch
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

// Already walking there: every MoveTo clears the spline, so a walk in flight is left alone
bool WalkInFlight(PlayerbotAI* botAI, Player* bot, Position const& spot)
{
    ReleaseStalledWalk(botAI, bot);
    LastMovement& last = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    return bot->isMoving() && BookedOn(last, spot);
}

}

bool AnubarakKiterImmunityAction::Execute(Event /*event*/)
{
    for (char const* immunity : KITER_SELF_IMMUNITIES)
    {
        if (botAI->CanCastSpell(immunity, bot))
            return botAI->CastSpell(immunity, bot);
    }

    return false;
}

bool AnubarakProtectKiterAction::Execute(Event /*event*/)
{
    Player* kiter = GetAnubarakKiterToProtect(bot);
    return kiter && botAI->CastSpell("hand of protection", kiter);
}

bool AnubarakKiteSpikeToPermafrostAction::Execute(Event /*event*/)
{
    // Heroic chains its speed boosts once the spike gains on it; each is instant and leaves the run going
    AnubarakKiteBoost boost;
    if (GetAnubarakKiteStage(bot, &boost) == AnubarakKiteStage::Boost)
    {
        if (boost.nitro)
        {
            if (UseNitroBoosts(bot))
                return true;
        }
        else if (boost.spellId)
        {
            if (boost.face)
                bot->SetOrientation(boost.facing);

            if (botAI->CastSpell(boost.spellId, bot))
                return true;
        }
    }

    Creature* spike = GetPursuingSpike(bot);
    ObjectGuid const spikeGuid = spike ? spike->GetGUID() : ObjectGuid::Empty;
    bool const keep = hasLatched && latchedSpike == spikeGuid;

    AnubarakKiteStand next;
    if (!GetAnubarakKiteStand(bot, keep ? &latched : nullptr, next))
    {
        hasLatched = false;
        return false;
    }

    latched = next;
    latchedSpike = spikeGuid;
    hasLatched = true;

    // Yields the tick on the stand: generic movers are vetoed for the marked bot, so it can cast here
    if (next.branch == AnubarakKiteBranch::Hold)
        return false;

    BreakChannelPinningTheFeet(bot);
    ReleaseStalledWalk(botAI, bot);

    // MoveTo refuses a point no higher than a FORCED leg still in flight, so a re-aimed stand, or the
    // first leg after this bot's own dodge, would wait out the stale one. Nothing outranks the kite.
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

        // Every MoveTo clears the spline, so a walk still carrying the bot there is left alone
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

    // A FORCED leg still in flight to some other spot refuses this one at the same priority
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
    uint32 const defensive = NextTankDefensive(botAI, bot, "anub.defensive");
    return defensive && botAI->CastSpell(defensive, bot);
}

bool AnubarakDestroyFrostSphereAction::Execute(Event /*event*/)
{
    Creature* sphere = GetAnubarakSphereToShoot(bot);
    if (!sphere)
        return false;

    if (Attack(sphere))
        return true;

    // On it already, and the rotation shoots it from here
    if (AnubarakSphereInReach(bot, sphere))
        return false;

    // Also covers Attack() refusing a sphere out of sight. A walk issued under a cast never starts
    // but still books its travel time. A bot parked on its dodge spot shoots only from there.
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

    // Picks up even with one already held: a wave brings more burrowers than there are side tanks
    if (pick.loose && TryBurrowerPickup(botAI, bot, pick))
    {
        RaidObs::NoteDerived(bot, "anub.sidetank", "pickup");
        return true;
    }

    RaidObs::NoteDerived(bot, "anub.sidetank", pick.held ? "hold" : "pickup");
    if (Attack(target))
        return true;

    // Walking off with one that isn't on this bot yet only leaves it on whoever it's hitting
    if (target != pick.held)
        return false;

    // The spike dodge would walk the tank straight back out
    Position const& hold = pick.hold;
    if (bot->GetExactDist2d(hold.GetPositionX(), hold.GetPositionY()) <= ANUBARAK_TANK_PATCH_ARRIVE ||
        AnubarakSpikeLaneCrosses(bot, hold))
        return false;

    return MoveTo(bot->GetMapId(), hold.GetPositionX(), hold.GetPositionY(), hold.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool AnubarakSideTankReadyAction::Execute(Event /*event*/)
{
    int8 const side = GetAnubarakBurrowerTankSide(bot);
    if (side < 0)
        return false;

    RaidObs::NoteDerived(bot, "anub.sidetank", "ready");

    // Scarabs that come to it get Permafrost's slow on the patch
    for (Unit* attacker : bot->getAttackers())
        if (attacker->GetEntry() == static_cast<uint32>(ToCNpcs::NPC_SWARM_SCARAB) && Attack(attacker))
            return true;

    Position const hold = GetAnubarakTankSpot(bot, static_cast<uint8>(side));
    if (bot->GetExactDist2d(hold.GetPositionX(), hold.GetPositionY()) <= ANUBARAK_TANK_PATCH_ARRIVE ||
        AnubarakSpikeLaneCrosses(bot, hold) || WalkInFlight(botAI, bot, hold))
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
                !botAI->CanCastSpell("death grip", sphere) || !botAI->CastSpell("death grip", sphere))
                return false;

            NoteAnubarakGripCast(bot, sphere);
            return true;
        }
        default:
            return false;
    }
}

bool AnubarakBattleResSideTankAction::Execute(Event /*event*/)
{
    Player* tank = GetAnubarakSideTankToRes(botAI);
    if (!tank)
        return false;

    if (bot->GetExactDist(tank) > ANUBARAK_BATTLE_RES_REACH || !bot->IsWithinLOSInMap(tank))
        return !IsAnubarakDodgeHeld(botAI) && MoveTo(tank, ANUBARAK_BATTLE_RES_REACH - ANUBARAK_RANGED_REACH_MARGIN,
                                                     MovementPriority::MOVEMENT_COMBAT);

    return botAI->CastSpell("rebirth", tank);
}

bool AnubarakGatherAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetAnubarakGatherSpot(bot, spot))
        return false;

    if (bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) <= ANUBARAK_GATHER_ARRIVE)
    {
        RaidObs::NoteDerived(bot, "anub.gather", "hold");
        return false;
    }

    RaidObs::NoteDerived(bot, "anub.gather", "walk");
    Position const walk = ValidateFloorPoint(bot, spot);
    BreakChannelPinningTheFeet(bot);
    if (WalkInFlight(botAI, bot, walk))
        return true;

    return MoveTo(bot->GetMapId(), walk.GetPositionX(), walk.GetPositionY(), walk.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool AnubarakDemonicCircleAction::Execute(Event /*event*/)
{
    return AnubarakDemonicCircleDue(botAI) && botAI->CastSpell(SPELL_DEMONIC_CIRCLE_SUMMON, bot);
}

bool AnubarakTankPickUpScarabAction::Execute(Event /*event*/)
{
    Creature* scarab = GetAnubarakScarabToPickUp(bot);
    if (!scarab)
        return false;

    bool const taunted = TryClassTaunt(botAI, bot, scarab);
    return Attack(scarab) || taunted;
}

bool AnubarakMainTankHoldBossAction::Execute(Event /*event*/)
{
    Creature* boss = GetAnubarak(bot);
    if (!boss || boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return false;

    // He never drops threat, so a taunt is only owed when a non-tank ended up with him
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

    // Slowed 80% in a patch the tank would crawl through every leg
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
    std::vector<Position> const points = submerge ? std::vector<Position>{point} : GetAnubarakDragPoints(bot, boss);
    if (points.empty())
    {
        ClearAnubarakDrag(bot);
        RaidObs::NoteDerived(bot, "anub.pickup", "hold");
        return false;
    }

    // A latched leg comes back whatever the point; otherwise the first point with a clean leg wins
    AnubarakDragLeg leg;
    for (Position const& candidate : points)
    {
        leg = GetAnubarakDragLeg(bot, boss, candidate, submerge);
        if (leg.step != AnubarakDragStep::None)
            break;
    }

    if (leg.step == AnubarakDragStep::None)
    {
        RaidObs::NoteDerived(bot, "anub.pickup", "hold");
        return false;
    }

    RaidObs::NoteDerived(bot, "anub.pickup", submerge ? "submerge" : "drag");
    if (leg.step != AnubarakDragStep::Walk)
        return false;

    // True only on the issuing tick, so the tank's threat abilities get the ticks while he follows
    ReleaseStalledWalk(botAI, bot);
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    Position const& spot = leg.tankSpot;
    if (BookedOn(last, spot))
        return false;

    // Backwards keeps the tank's front to him, so his first swings on arrival can be parried and blocked
    DropForeignBooking(last, spot, MovementPriority::MOVEMENT_COMBAT);
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true, true);
}

bool AnubarakHealPenetratingColdAction::Execute(Event /*event*/)
{
    Player* target = GetAnubarakPenetratingColdHealTarget(botAI);
    if (!target)
        return false;

    for (char const* heal : PENETRATING_COLD_HEALS)
    {
        if (botAI->CanCastSpell(heal, target))
            return botAI->CastSpell(heal, target);
    }

    return false;
}
