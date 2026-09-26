#include "ToCActions_Anubarak.h"

#include <algorithm>
#include <string>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "Player.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "RtiTargetValue.h"
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

constexpr float ANUBARAK_DODGE_ARRIVE = 2.0f;
// A dodge that hasn't run for this long was out of danger in between, so its old spot is stale
constexpr uint32 ANUBARAK_DODGE_LATCH_GAP_MS = 1000;
constexpr uint32 ANUBARAK_STALL_MS = 500;
constexpr float ANUBARAK_RANGED_REACH_MARGIN = 2.0f;
// Short legs keep him in tow
constexpr float ANUBARAK_DRAG_STEP = 5.0f;

char const* const PENETRATING_COLD_HEALS[] = {
    "greater heal", "flash heal", "penance",
    "healing touch", "nourish", "regrowth",
    "holy light", "flash of light", "holy shock",
    "lesser healing wave", "healing wave", "riptide",
};

char const* const CLASS_TAUNTS[] = {"taunt", "hand of reckoning", "dark command", "growl"};

// MoveTo refuses a repeat of its last point for up to 5 s even once the bot has stopped (a potion's
// StopMoving, a knockback), so drop the booking when the bot stands still past the issue.
void ReleaseStalledWalk(PlayerbotAI* botAI, Player* bot)
{
    LastMovement& last = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    if (!bot->isMoving() && last.msTime && getMSTimeDiff(last.msTime, getMSTime()) > ANUBARAK_STALL_MS)
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

// Only a cross this node could have left: on a burrower off Permafrost, or on something dead or gone
void ClearStaleBurrowerCross(PlayerbotAI* botAI, Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return;

    ObjectGuid const guid = group->GetTargetIcon(RtiTargetValue::crossIndex);
    if (guid.IsEmpty())
        return;

    Unit* marked = botAI->GetUnit(guid);
    bool const stale = !marked || !marked->IsAlive() ||
                       (marked->GetEntry() == static_cast<uint32>(ToCNpcs::NPC_NERUBIAN_BURROWER) &&
                        !UnitOnPermafrost(marked));
    if (stale)
        ClearTargetIcon(bot, RtiTargetValue::crossIndex);
}

}

bool AnubarakKiteSpikeToPermafrostAction::Execute(Event /*event*/)
{
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
    uint32 const now = getMSTime();
    if (hasSpot && (getMSTimeDiff(spotMs, now) > ANUBARAK_DODGE_LATCH_GAP_MS ||
                    bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) <= ANUBARAK_DODGE_ARRIVE))
    {
        hasSpot = false;
    }

    if (!hasSpot)
    {
        Position found;
        if (!GetAnubarakSpikeDodgeSpot(bot, found))
            return false;

        spot = found;
        hasSpot = true;
    }

    spotMs = now;
    BreakChannelPinningTheFeet(bot);
    ReleaseStalledWalk(botAI, bot);
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_FORCED, true);
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
    char const* defensive = NextTankDefensive(botAI, bot, "anub.defensive");
    return defensive && botAI->CastSpell(defensive, bot);
}

bool AnubarakDestroyFrostSphereAction::Execute(Event /*event*/)
{
    Creature* sphere = GetAnubarakSphereToShoot(bot);
    return sphere && Attack(sphere);
}

bool AnubarakFocusBurrowerAction::Execute(Event /*event*/)
{
    Creature* burrower = GetAnubarakFocusBurrower(bot);
    if (!burrower)
    {
        SetRtiTarget(botAI, "skull");
        ClearStaleBurrowerCross(botAI, bot);
        return false;
    }

    MarkTargetWithCross(bot, burrower);
    SetRtiTarget(botAI, ANUBARAK_BURROWER_RTI, burrower);
    return Attack(burrower);
}

bool AnubarakAssistTankHoldBurrowerAction::Execute(Event /*event*/)
{
    AnubarakBurrowerPick const pick = GetAnubarakBurrowerPick(botAI);
    Creature* target = pick.held ? pick.held : pick.loose;
    if (!target)
    {
        // TankTargetValue reads rti first, so a leftover side mark has this tank chase that icon next fight
        if (IsAnubarakSideRti(AI_VALUE(std::string, "rti")))
            SetRtiTarget(botAI, "skull");

        return false;
    }

    // Taunts even with one already held: a wave brings more burrowers than there are side tanks
    if (pick.loose && TryClassTaunt(botAI, bot, pick.loose))
        return true;

    // Own rti name with no group icon behind it, so DPS never follow this pick
    SetRtiTarget(botAI, ANUBARAK_SIDE_RTI[pick.side], target);
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

    MarkTargetWithSkull(bot, boss);
    SetRtiTarget(botAI, "skull", boss);

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

    Position spot;
    bool const submerging = GetAnubarakSubmergeSpot(bot, spot);
    RaidObs::NoteDerived(bot, "anub.pickup", submerging ? "submerge" : "hold");
    return DragBossOnto(boss, submerging ? spot : GetAnubarakBossAnchor(bot));
}

bool AnubarakMainTankHoldBossAction::DragBossOnto(Unit* boss, Position const& point)
{
    if (boss->GetVictim() != bot)
        return false;

    float const bossDist = boss->GetExactDist2d(point.GetPositionX(), point.GetPositionY());
    if (bossDist <= ANUBARAK_DRAG_ARRIVE)
        return false;

    // He follows at the bot's current gap, so the bot aims that far past the point
    float const gap = bot->GetExactDist2d(boss);
    float const goalX = point.GetPositionX() + (point.GetPositionX() - boss->GetPositionX()) / bossDist * gap;
    float const goalY = point.GetPositionY() + (point.GetPositionY() - boss->GetPositionY()) / bossDist * gap;
    float const toGoal = bot->GetExactDist2d(goalX, goalY);
    // Bot already there, he's still catching up
    if (toGoal < ANUBARAK_DRAG_ARRIVE)
        return false;

    float const step = std::min(ANUBARAK_DRAG_STEP, toGoal);
    float const moveX = bot->GetPositionX() + (goalX - bot->GetPositionX()) / toGoal * step;
    float const moveY = bot->GetPositionY() + (goalY - bot->GetPositionY()) / toGoal * step;
    return MoveTo(bot->GetMapId(), moveX, moveY, point.GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true, true);
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
