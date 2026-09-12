#include "UldActions_Freya.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <cmath>

#include "AiObjectContext.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Position.h"
#include "Spell.h"
#include "UldData.h"
#include "UldEncounter_Freya.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <TankAssistStrategy.h>

using namespace EncounterHelpers;

namespace
{
// A point move never launches its spline while the bot is casting: PointMovementGenerator::DoInitialize
// bails on IsMovementPreventedByCasting and DoUpdate calls StopMoving every tick until the cast ends,
// while MoveTo still reports success. A channeled Blizzard or Volley therefore pins a bot inside the
// blast it was just told to leave. Only bites when movement is genuinely blocked, so the rotation
// survives the ticks where the bot could have walked anyway.
void FreyaClearCastBlockingMove(Player* bot)
{
    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);
}

// Every other living raid member near the bot. Nature's Fury has no ground marker to route around - the
// raid itself is the hazard, and the carrier is the only one who can move away from it.
std::vector<Position> GetFreyaAlliesNear(PlayerbotAI* botAI, float radius)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return {};

    std::vector<Position> allies;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        if (bot->GetExactDist2d(member) < radius)
            allies.push_back(member->GetPosition());
    }

    return allies;
}

// Straight out of the blasts the bot is standing in, on the bearing away from their centre. Only runs
// once the ring sweep has failed on every clearance it has, so there is no clear spot left to find: the
// job here is just to leave the circles, and crossing a fifth bomb on the way out still beats holding
// still inside four.
Position StepOffNatureBombs(Player* bot, std::vector<Position> const& bombs)
{
    float sumX = 0.0f;
    float sumY = 0.0f;
    uint32 covering = 0;
    for (Position const& bomb : bombs)
    {
        if (bomb.GetExactDist2d(bot->GetPositionX(), bot->GetPositionY()) >= ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS)
            continue;

        sumX += bomb.GetPositionX();
        sumY += bomb.GetPositionY();
        ++covering;
    }

    if (!covering)
        return Position();

    float const centreX = sumX / covering;
    float const centreY = sumY / covering;

    // Standing dead on the centre leaves no bearing to run on, and one direction is as good as another.
    float const bearing = bot->GetExactDist2d(centreX, centreY) > CONTACT_DISTANCE
                              ? std::atan2(bot->GetPositionY() - centreY, bot->GetPositionX() - centreX)
                              : bot->GetOrientation();

    float x = bot->GetPositionX() + std::cos(bearing) * ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS;
    float y = bot->GetPositionY() + std::sin(bearing) * ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS;
    float z = bot->GetPositionZ();
    if (!bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                        bot->GetPositionZ(), x, y, z))
        return Position();

    return Position(x, y, z, 0.0f);
}
}  // namespace

bool FreyaMoveAwayNatureBombAction::isUseful()
{
    FreyaNearNatureBombTrigger trigger(botAI);
    if (!trigger.IsActive())
        return false;

    // The trigger reaches out to the whole hazard search radius so this node still runs once the bot has
    // stepped clear. Narrow it back to the blast here, plus the window in which the bot may still be
    // holding a spot, so everyone else falls through to their own ladder without leaving a verdict.
    if (bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS))
        return true;

    return bombSpotMs && getMSTimeDiff(bombSpotMs, getMSTime()) < ULDUAR_FREYA_NATURE_BOMB_LATCH_MS;
}

bool FreyaMoveAwayNatureBombAction::Execute(Event /*event*/)
{
    // Not FleePosition: it clamps its travel to AiPlayerbot.FleeDistance (5 yd), which cannot clear a
    // 10 yd blast the bot is standing in the middle of, and it only ever reads one hazard. A volley
    // drops a bomb on every player, so the melee stack ends up under several overlapping ones.
    std::vector<Position> bombs = GetFreyaNatureBombPositions(bot, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);
    if (bombs.empty())
    {
        bombSpotMs = 0;
        return false;
    }

    // Beams too, because a spot that clears both is the only one worth walking to or holding. Both
    // escapes sit at the same relevance, so a bot that reads only its own hazard alternates between
    // them forever. Built from the bombs already in hand rather than collected a second time.
    std::vector<HazardCircle> const hazards =
        BuildFreyaEscapeHazards(bombs, GetFreyaSunBeamPositions(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS));

    auto const stillClear = [&hazards](Position const& spot)
    {
        for (HazardCircle const& hazard : hazards)
            if (hazard.first.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < hazard.second)
                return false;

        return true;
    };

    auto const covered = [&bombs](Position const& spot)
    {
        for (Position const& bomb : bombs)
            if (bomb.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS)
                return true;

        return false;
    };

    uint32 const now = getMSTime();
    bool const latched = bombSpotMs && getMSTimeDiff(bombSpotMs, now) < ULDUAR_FREYA_NATURE_BOMB_LATCH_MS;

    // Claim the tick without touching the motion master while the escape is already in flight. Returning
    // true is the point: the engine stops here, so combat movement never gets to pull the bot back onto
    // the bomb it just stepped off, and the spline carrying it out survives. A bot whose cast is
    // blocking movement is not in flight and never will be, so it falls through to the interrupt below.
    if (latched && stillClear(bombSpot) && !bot->IsMovementPreventedByCasting() &&
        bot->GetExactDist2d(bombSpot.GetPositionX(), bombSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    if (!covered(bot->GetPosition()))
    {
        // Arrived, clear, and the bomb it fled is still counting down. A bomb goes off 6s after it lands
        // and the walk out costs about two, so a melee bot that turns round the moment it arrives spends
        // the rest of the fuse back inside the blast - which is how bots at full health die to a volley.
        // Holding costs it nothing it has not already spent, because it is out of melee range at the
        // escape spot either way. Ranged and healers are not held: they can cast from where they stand,
        // and returning true here would silence them for the rest of the fuse.
        if (latched && PlayerbotAI::IsMelee(bot) && stillClear(bombSpot) && covered(bombOrigin))
            return true;

        bombSpotMs = 0;
        return false;
    }

    bombSpotMs = 0;

    // Every tier below walks the same ring of candidates with a looser clearance, so the collision
    // check only ever has to answer for each of them once.
    HazardSweepCache sweep;

    Position safe = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    // A volley drops a bomb on every player at once, so nothing may clear all of them by the full
    // margin. Somebody else's hazard beats your own, and barely outside beats standing in one.
    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, bombs, ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS,
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, bombs, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS + 1.0f,
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    // Down to the blast with no margin at all, and then straight out of it. Giving up instead leaves the
    // bot standing in the circle for the rest of the fuse: one pull did that fifteen times, and a
    // warlock died at full health with a bomb at its feet having issued no move at all.
    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, bombs, ULDUAR_FREYA_NATURE_BOMB_BLAST_RADIUS,
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position())
        safe = StepOffNatureBombs(bot, bombs);

    if (safe == Position())
        return false;

    FreyaClearCastBlockingMove(bot);

    if (!MoveTo(bot->GetMapId(), safe.GetPositionX(), safe.GetPositionY(), safe.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_FORCED, true, false))
        return false;

    bombOrigin = bot->GetPosition();
    bombSpot = safe;
    bombSpotMs = now;

    return true;
}

bool FreyaTankNatureBombAction::isUseful()
{
    FreyaTankNatureBombTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaTankNatureBombAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
    {
        kiteSpotMs = 0;
        return false;
    }

    std::vector<Position> bombs = GetFreyaNatureBombPositions(bot, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);
    if (bombs.empty())
    {
        kiteSpotMs = 0;
        return false;
    }

    auto const stillClear = [&bombs](Position const& spot)
    {
        for (Position const& bomb : bombs)
            if (bomb.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS)
                return false;

        return true;
    };

    uint32 const now = getMSTime();

    if (kiteSpotMs && getMSTimeDiff(kiteSpotMs, now) < ULDUAR_FREYA_TANK_BOMB_LATCH_MS && stillClear(kiteSpot) &&
        !bot->IsMovementPreventedByCasting() &&
        bot->GetExactDist2d(kiteSpot.GetPositionX(), kiteSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    kiteSpotMs = 0;

    // Freya walks after whoever holds her, so every yard the tank takes drags the boss and her melee
    // ring with it. Aim at the far side of her from the back line: FindNearestPositionClearOfHazards
    // only reorders spots that are the same walk away, so this picks a direction without ever choosing
    // a longer trip. No living ranged DPS means no back line to walk away from.
    //
    // Past the leash that preference flips to the anchor. Aiming away from the back line every volley
    // with nothing pulling the other way is a bias that compounds - eighteen seconds a volley, and one
    // pull ended with Freya 100 yd east of where she is tanked, six yards down the slope toward the
    // water - so the same tie-break that walked her out is what walks her home.
    Position preferred = ULDUAR_FREYA_TANK_ANCHOR;
    if (boss->GetExactDist2d(&ULDUAR_FREYA_TANK_ANCHOR) <= ULDUAR_FREYA_TANK_LEASH)
    {
        if (Player* anchor = GetFreyaRangedCampAnchor(botAI))
        {
            float const dx = boss->GetPositionX() - anchor->GetPositionX();
            float const dy = boss->GetPositionY() - anchor->GetPositionY();
            float const length = std::sqrt(dx * dx + dy * dy);
            if (length > 0.0f)
                preferred = Position(boss->GetPositionX() + dx / length * ULDUAR_FREYA_HAZARD_SEARCH_RADIUS,
                                     boss->GetPositionY() + dy / length * ULDUAR_FREYA_HAZARD_SEARCH_RADIUS,
                                     boss->GetPositionZ(), 0.0f);
        }
    }

    Position const* preferNear = &preferred;

    std::vector<EncounterHelpers::HazardCircle> hazards =
        BuildFreyaEscapeHazards(bombs, GetFreyaSunBeamPositions(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS));

    HazardSweepCache sweep;

    Position safe = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, 2.0f,
                                                     static_cast<float>(M_PI) / 8.0f, preferNear, {}, &sweep);

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, bombs, ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS,
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, bombs, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS + 1.0f,
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position())
        return false;

    FreyaClearCastBlockingMove(bot);

    if (!MoveTo(bot->GetMapId(), safe.GetPositionX(), safe.GetPositionY(), safe.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_FORCED, true, false))
        return false;

    kiteSpot = safe;
    kiteSpotMs = now;

    return true;
}

bool FreyaSetDpsPriorityAction::isUseful()
{
    FreyaSetDpsPriorityTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaSetDpsPriorityAction::Execute(Event /*event*/)
{
    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    Unit* target = ResolveFreyaDpsTarget(currentTarget);
    if (!target)
        return false;

    // Every tick, not only on a switch: the early return below is the common case, and a pet left on
    // a dead or floored add would never catch up. CommandPetAttack no-ops when it is already there.
    CommandPetAttack(botAI, target);

    bool needsAttack = currentTarget != target;
    if (PlayerbotAI::IsMelee(bot))
        needsAttack = needsAttack || !bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING);

    // Returning false once the bot is on the right target is what lets the lower-priority nodes run:
    // the engine ends the tick at the first action that succeeds.
    return needsAttack ? Attack(target) : false;
}

bool FreyaSetDpsPriorityAction::TakesEonarsGift(FreyaWaveState const& state)
{
    if (!state.eonarsGift || !state.eonarsGift->IsAlive())
    {
        giftGuid.Clear();
        return false;
    }

    bool packUp = false;
    for (Unit* lasher : state.detonatingLashers)
        packUp = packUp || (lasher && lasher->IsAlive());

    // Nothing is competing for the ranged, so the Gift takes all of them as it always did.
    if (!packUp)
        return true;

    uint32 const now = getMSTime();
    if (giftGuid != state.eonarsGift->GetGUID())
    {
        giftGuid = state.eonarsGift->GetGUID();
        giftSeenMs = now;
    }

    // A head start, not a hand-off: whatever the share has managed by now, the rest of the ranged join
    // it with time left on the 12s.
    if (getMSTimeDiff(giftSeenMs, now) >= ULDUAR_FREYA_GIFT_SHARE_MS)
        return true;

    return GetFreyaRangedDpsRank(botAI) < ULDUAR_FREYA_GIFT_SHARE;
}

Unit* FreyaSetDpsPriorityAction::ResolveFreyaDpsTarget(Unit* currentTarget)
{
    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    // Eonar's Gift heals Freya for 30-60% if it lives 12s. Ranged burn it from where they stand, so
    // melee never eat the travel time both ways - unless there is no ranged DPS left to do it.
    bool const takesGift =
        (PlayerbotAI::IsRangedDps(bot) || !FreyaHasLivingRangedDps(botAI)) && TakesEonarsGift(state);

    // Once the trio is low, leaving it costs the whole wave: a member abandoned above the floor
    // revives 11s later, and the 60s until the next wave spawns is gone.
    bool const trioLocked = state.TrioLocked();

    std::vector<Unit*> priority;

    if (takesGift && !state.TrioReleased())
        priority.push_back(state.eonarsGift);

    if (!trioLocked)
        priority.push_back(state.conservator);

    // One slot for all three members; which one this bot takes is the greedy split, not entry order.
    Unit* const trioMember = GetFreyaTrioAssignment(botAI, state);
    priority.push_back(trioMember);

    if (trioLocked)
    {
        priority.push_back(state.conservator);
    }
    else
    {
        bool const isRanged = PlayerbotAI::IsRangedDps(bot);

        // Two phases. While the pack is healthy every ranged bot points at the middle of it, because
        // AoeTrigger measures 8 yd around the *current target*, so that pick is what makes class AoE
        // fire at all. Once the pile is at the finish the lowest-health focus takes over and the
        // detonations stagger instead of chaining.
        Unit* lasher = nullptr;
        if (isRanged)
        {
            lasher = GetFreyaLasherPackFocus(state);
            if (lasher && IsFreyaLasherPackFinishing(state, lasher->GetPosition()))
                lasher = GetFreyaRangedLasherFocus(state);
        }

        // The raid-wide focus can be most of the room away. Walking to it would put the bot inside the
        // 15 yd blast, which is the one thing that keeps ranged safe here, so it shoots whatever it can
        // already reach instead - and the two converge on their own as lashers close on players.
        float const reach = isRanged ? sPlayerbotAIConfig.spellDistance : ULDUAR_FREYA_MELEE_LASHER_RANGE;
        if (lasher && bot->GetExactDist2d(lasher) > reach)
            lasher = nullptr;

        if (!lasher)
            lasher = GetFreyaLocalLasherTarget(botAI, state, currentTarget, reach);

        priority.push_back(lasher);
    }

    priority.push_back(AI_VALUE2(Unit*, "find target", "freya"));

    Unit* target = nullptr;
    for (Unit* candidate : priority)
    {
        if (candidate && candidate->IsAlive())
        {
            target = candidate;
            break;
        }
    }

    // Hold the current target unless something strictly more urgent is up, so a churn of adds cannot
    // keep resetting swing and cast timers. The trio slot is exempt: GetFreyaTrioAssignment is already
    // stable by construction, and stickiness on top would freeze each bot onto its first pick.
    auto const priorityIndex = [&priority](Unit* unit) -> size_t
    {
        if (!unit || !unit->IsAlive())
            return priority.size();

        for (size_t i = 0; i < priority.size(); ++i)
        {
            if (priority[i] == unit)
                return i;
        }

        return priority.size();
    };

    if (currentTarget && currentTarget != trioMember && target != trioMember &&
        priorityIndex(currentTarget) <= priorityIndex(target))
    {
        target = currentTarget;
    }

    return target ? target : AI_VALUE(Unit*, "dps target");
}

bool FreyaTankAddsAction::isUseful()
{
    FreyaTankAddsTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaTankAddsAction::Execute(Event /*event*/)
{
    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    Unit* target = GetFreyaTankTarget(botAI, state, currentTarget);
    if (!target)
        return false;

    // Taunt only what this encounter actually claims. The rest of the ladder is borrowed for damage:
    // taunting Freya would fight a human main tank whose raid roles are set differently, taunting a Storm
    // Lasher or Water Spirit would mean owning Tidal Wave positioning, and a lasher drops the taunt again
    // on its next 10s threat wipe anyway.
    bool const owned = target == state.snaplasher || target == state.conservator;
    if (owned && target->GetVictim() != bot && CastClassTaunt(botAI, target))
        return true;

    if (currentTarget != target)
        return Attack(target);

    return target == state.conservator ? ParkConservator(target) : false;
}

bool FreyaTankAddsAction::ParkConservator(Unit* conservator)
{
    Unit* spore = botAI->GetUnit(parkedSpore);
    if (!spore || !spore->IsAlive())
    {
        spore = GetFreyaConservatorSpore(botAI, conservator);
        parkedSpore = spore ? spore->GetGUID() : ObjectGuid::Empty;
    }

    if (!spore)
        return false;

    // Pheromones is a 6 yd aura on the spore and the Conservator stops at melee range of the tank, so
    // standing on the spore is what puts the boss in reach of everyone sheltering on it. Hold still once
    // it is there rather than nudging it back and forth.
    if (conservator->GetExactDist2d(spore) <= ULDUAR_FREYA_SPORE_RADIUS - 1.0f)
        return false;

    return MoveTo(bot->GetMapId(), spore->GetPositionX(), spore->GetPositionY(), spore->GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_FORCED, true, false);
}

bool FreyaMoveToHealingSporeAction::isUseful()
{
    FreyaMoveToHealingSporeTrigger freyaMoveToHealingSporeTrigger(botAI);
    return freyaMoveToHealingSporeTrigger.IsActive();
}

bool FreyaMoveToHealingSporeAction::Execute(Event /*event*/)
{
    Unit* target = GetFreyaTargetSpore(botAI);
    if (!target)
        return false;

    float const distance = bot->GetExactDist2d(target);
    if (distance <= ULDUAR_FREYA_SPORE_STAND_RANGE)
        return false;

    // Stop on the near edge of the aura rather than the spore's centre. The centre is inside the spore's
    // own collision, so the bot can never occupy it: MoveTo then re-issues the same unreachable point,
    // IsDuplicateMove rejects it from the second tick on, and the tick falls through to the DPS chase
    // which walks the bot straight back out of the aura.
    float const ratio = ULDUAR_FREYA_SPORE_STAND_RANGE / distance;
    float const x = target->GetPositionX() + (bot->GetPositionX() - target->GetPositionX()) * ratio;
    float const y = target->GetPositionY() + (bot->GetPositionY() - target->GetPositionY()) * ratio;

    return MoveTo(target->GetMapId(), x, y, target->GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool FreyaRedirectThreatAction::isUseful()
{
    return bot->getClass() == CLASS_HUNTER || bot->getClass() == CLASS_ROGUE;
}

Player* FreyaRedirectThreatAction::GetRedirectTank()
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    // One walk of the scan answers all three, and this runs every tick a hunter or rogue is up. The two
    // adds have a real threat table that someone else is holding; Detonating Lashers reset threat every
    // 10s, so no redirect can ever help there.
    Unit* snaplasher = nullptr;
    Unit* conservator = nullptr;
    Unit* freya = nullptr;
    for (ObjectGuid const& guid : GetFreyaScan(botAI).PossibleTargets())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        switch (unit->GetEntry())
        {
            case NPC_SNAPLASHER:
                if (!snaplasher)
                    snaplasher = unit;
                break;
            case NPC_ANCIENT_CONSERVATOR:
                if (!conservator)
                    conservator = unit;
                break;
            case NPC_FREYA:
                if (!freya)
                    freya = unit;
                break;
            default:
                break;
        }
    }

    if (snaplasher || conservator)
    {
        if (Player* assistTank = GetGroupAssistTank(bot, 0))
            return assistTank;
    }

    // Otherwise feed whoever is actually holding Freya, which survives a swap or a tank death.
    if (freya)
    {
        if (Unit* victim = freya->GetVictim())
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

bool FreyaRedirectThreatAction::Execute(Event /*event*/)
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

    // Misdirection only moves the threat of the next three shots. Spend them on Freya rather than
    // leaving them to whatever the rotation picks - never on an add the tank does not want.
    Unit* freya = GetFreyaScanUnitByEntry(botAI, NPC_FREYA);
    if (freya && bot->HasAura(SPELL_MISDIRECTION) && botAI->CanCastSpell("steady shot", freya))
        return botAI->CastSpell("steady shot", freya);

    return false;
}

bool FreyaBreakIronRootsAction::isUseful()
{
    FreyaBreakIronRootsTrigger freyaBreakIronRootsTrigger(botAI);
    return freyaBreakIronRootsTrigger.IsActive();
}

bool FreyaBreakIronRootsAction::Execute(Event /*event*/)
{
    // The root creature is summoned on the trapped bot; killing it removes the root DoT.
    Creature* root = bot->FindNearestCreature(NPC_FREYA_STRENGTHENED_IRON_ROOTS, 10.0f);
    if (!root)
        root = bot->FindNearestCreature(NPC_FREYA_IRON_ROOTS, 10.0f);

    if (!root || !root->IsAlive())
        return false;

    return Attack(root);
}

bool FreyaDodgeUnstableSunBeamAction::isUseful()
{
    FreyaDodgeUnstableSunBeamTrigger freyaDodgeUnstableSunBeamTrigger(botAI);
    return freyaDodgeUnstableSunBeamTrigger.IsActive();
}

bool FreyaDodgeUnstableSunBeamAction::Execute(Event /*event*/)
{
    // Every beam in the search area is routed around, not just the one the bot is standing in, or it
    // sidesteps one beam straight into another. FleePosition cannot do this: it clamps travel to
    // AiPlayerbot.FleeDistance (5 yd), which does not clear a 12 yd beam, and it only reads one hazard.
    std::vector<Position> beams = GetFreyaSunBeamPositions(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);

    if (beams.empty())
    {
        dodgeSpotMs = 0;
        return false;
    }

    auto const stillClear = [&beams](Position const& spot)
    {
        for (Position const& beam : beams)
            if (beam.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < ULDUAR_FREYA_SUN_BEAM_CLEARANCE)
                return false;

        return true;
    };

    uint32 const now = getMSTime();

    // Claim the tick without touching the motion master while the escape is already in flight. Returning
    // true is the point: the engine stops here, so no lower node gets to re-aim the bot mid-dodge, and
    // the spline that is actually carrying it out of the beam survives. A bot whose cast is blocking
    // movement is not in flight and never will be, so it falls through to the interrupt below instead
    // of sitting out the whole latch inside the beam.
    if (dodgeSpotMs && getMSTimeDiff(dodgeSpotMs, now) < ULDUAR_FREYA_SUN_BEAM_LATCH_MS && stillClear(dodgeSpot) &&
        !bot->IsMovementPreventedByCasting() &&
        bot->GetExactDist2d(dodgeSpot.GetPositionX(), dodgeSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    dodgeSpotMs = 0;

    HazardSweepCache sweep;

    // Bombs too, because a spot that clears both is the only one worth walking to. Both escapes sit at
    // the same relevance, so a bot that reads only its own hazard alternates between them forever. The
    // beams are the ones already collected above.
    Position safe = FindNearestPositionClearOfHazards(
        bot, BuildFreyaEscapeHazards(GetFreyaNatureBombPositions(bot, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS), beams),
        ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, beams, ULDUAR_FREYA_SUN_BEAM_CLEARANCE,
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    // Overlapping beams can leave nowhere that clears all of them by the full margin. Barely outside
    // beats standing in one, so fall back to the radius itself before giving up.
    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, beams, ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS + 1.0f,
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position())
        return false;

    FreyaClearCastBlockingMove(bot);

    if (!MoveTo(bot->GetMapId(), safe.GetPositionX(), safe.GetPositionY(), safe.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_FORCED, true, false))
        return false;

    dodgeSpot = safe;
    dodgeSpotMs = now;

    return true;
}

bool FreyaNaturesFuryBailAction::isUseful()
{
    FreyaNaturesFuryBailTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaNaturesFuryBailAction::Execute(Event /*event*/)
{
    auto const stillClear = [this](Position const& spot)
    { return CountFreyaRaidNear(botAI, spot, ULDUAR_FREYA_NATURES_FURY_RADIUS, bot) == 0; };

    uint32 const now = getMSTime();

    // Claim the tick without touching the motion master while the walk is in flight, the same way the
    // beam dodge does: a re-issued MoveTo kills the spline that is carrying the mark out of the raid.
    if (bailSpotMs && getMSTimeDiff(bailSpotMs, now) < ULDUAR_FREYA_NATURES_FURY_LATCH_MS && stillClear(bailSpot) &&
        !bot->IsMovementPreventedByCasting() &&
        bot->GetExactDist2d(bailSpot.GetPositionX(), bailSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    bailSpotMs = 0;

    Position safe;
    if (Unit* shelter = GetFreyaNaturesFuryShelter(botAI))
    {
        // Another spore beats open floor by the whole of Conservator's Grip, which has no range and no
        // duration and is only ever answered by Potent Pheromones. Stop on the near edge of the aura
        // rather than the spore's centre: the centre sits inside its collision, so MoveTo would re-issue
        // the same unreachable point until the mark expired.
        float const distance = bot->GetExactDist2d(shelter);
        if (distance > ULDUAR_FREYA_SPORE_STAND_RANGE)
        {
            float const ratio = ULDUAR_FREYA_SPORE_STAND_RANGE / distance;
            safe = Position(shelter->GetPositionX() + (bot->GetPositionX() - shelter->GetPositionX()) * ratio,
                            shelter->GetPositionY() + (bot->GetPositionY() - shelter->GetPositionY()) * ratio,
                            shelter->GetPositionZ(), 0.0f);
        }
    }

    // No free spore, or none up at all: open floor and ten seconds of being pacified is still cheaper
    // than five volleys into the raid.
    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, GetFreyaAlliesNear(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS),
                                                 ULDUAR_FREYA_NATURES_FURY_CLEAR, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);

    if (safe == Position())
        return false;

    FreyaClearCastBlockingMove(bot);

    if (!MoveTo(bot->GetMapId(), safe.GetPositionX(), safe.GetPositionY(), safe.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_FORCED, true, false))
        return false;

    bailSpot = safe;
    bailSpotMs = now;

    return true;
}

bool FreyaStepOutOfSunbeamAction::isUseful()
{
    FreyaStepOutOfSunbeamTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaStepOutOfSunbeamAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    Unit* target = boss ? GetFreyaSunbeamTarget(boss) : nullptr;
    if (!target)
    {
        stepSpotMs = 0;
        return false;
    }

    Position const beam = target->GetPosition();

    auto const stillClear = [&beam](Position const& spot)
    { return beam.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) >= ULDUAR_FREYA_SUNBEAM_CLEAR_RADIUS; };

    uint32 const now = getMSTime();

    if (stepSpotMs && getMSTimeDiff(stepSpotMs, now) < ULDUAR_FREYA_SUNBEAM_LATCH_MS && stillClear(stepSpot) &&
        !bot->IsMovementPreventedByCasting() &&
        bot->GetExactDist2d(stepSpot.GetPositionX(), stepSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    stepSpotMs = 0;

    // Bombs and beams belong in the same sweep: a spot that only clears the sunbeam is worth nothing if
    // it sits in one of those, and both escapes hold this relevance, so a bot reading one hazard walks
    // between them until something kills it.
    std::vector<EncounterHelpers::HazardCircle> hazards =
        GetFreyaEscapeHazards(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);
    hazards.emplace_back(beam, ULDUAR_FREYA_SUNBEAM_CLEAR_RADIUS);

    HazardSweepCache sweep;

    Position safe = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    // Barely outside the beam beats standing in it, which is what the raid did with every volley of a
    // pull that took 271k a minute off this one spell.
    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, std::vector<Position>{beam}, ULDUAR_FREYA_SUNBEAM_CLEAR_RADIUS,
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position())
        return false;

    FreyaClearCastBlockingMove(bot);

    if (!MoveTo(bot->GetMapId(), safe.GetPositionX(), safe.GetPositionY(), safe.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_FORCED, true, false))
        return false;

    stepSpot = safe;
    stepSpotMs = now;

    return true;
}

bool FreyaTankHoldFreyaAction::isUseful()
{
    FreyaTankHoldFreyaTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaTankHoldFreyaAction::Execute(Event /*event*/)
{
    uint32 const now = getMSTime();

    if (holdMs && getMSTimeDiff(holdMs, now) < ULDUAR_FREYA_TANK_HOLD_LATCH_MS &&
        !bot->IsMovementPreventedByCasting() && bot->GetExactDist2d(&ULDUAR_FREYA_TANK_ANCHOR) > CONTACT_DISTANCE)
        return true;

    holdMs = 0;

    // Onto the anchor itself, not near it: Freya stops in melee range of wherever the tank ends up, so
    // she settles a boss length short of this point either way. MOVEMENT_COMBAT, so a bomb volley still
    // outranks the walk home.
    if (!MoveTo(bot->GetMapId(), ULDUAR_FREYA_TANK_ANCHOR.GetPositionX(), ULDUAR_FREYA_TANK_ANCHOR.GetPositionY(),
                ULDUAR_FREYA_TANK_ANCHOR.GetPositionZ(), false, false, false, true,
                MovementPriority::MOVEMENT_COMBAT))
        return false;

    holdMs = now;

    return true;
}

bool FreyaLasherAboutToBlowAction::isUseful()
{
    FreyaLasherAboutToBlowTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaLasherAboutToBlowAction::Execute(Event /*event*/)
{
    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    std::vector<Position> blasts =
        GetFreyaLowLasherPositions(botAI, state, ULDUAR_FREYA_LASHER_BAIL_PCT, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);
    if (blasts.empty())
    {
        bailSpotMs = 0;
        return false;
    }

    auto const stillClear = [&blasts](Position const& spot)
    {
        for (Position const& blast : blasts)
            if (blast.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < ULDUAR_FREYA_LASHER_PACK_CLEAR)
                return false;

        return true;
    };

    uint32 const now = getMSTime();

    // Claim the tick without touching the motion master while the walk is already in flight, so no
    // lower node re-aims the bot back onto the pile mid-step. A bot whose cast is blocking movement is
    // not in flight and never will be, so it falls through to the interrupt below.
    if (bailSpotMs && getMSTimeDiff(bailSpotMs, now) < ULDUAR_FREYA_LASHER_BAIL_LATCH_MS && stillClear(bailSpot) &&
        !bot->IsMovementPreventedByCasting() &&
        bot->GetExactDist2d(bailSpot.GetPositionX(), bailSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    bailSpotMs = 0;

    HazardSweepCache sweep;

    Position safe = FindNearestPositionClearOfHazards(bot, blasts, ULDUAR_FREYA_LASHER_PACK_CLEAR,
                                                      ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    // At the finish half the pile is under the bail line at once, and nowhere in the room clears all of
    // them by the full margin. Barely outside the blast beats standing in the middle of it.
    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, blasts, ULDUAR_FREYA_DETONATE_RADIUS + 1.0f,
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position())
        return false;

    FreyaClearCastBlockingMove(bot);

    if (!MoveTo(bot->GetMapId(), safe.GetPositionX(), safe.GetPositionY(), safe.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_FORCED, true, false))
        return false;

    bailSpot = safe;
    bailSpotMs = now;

    return true;
}

bool FreyaRangedCampAction::isUseful()
{
    FreyaRangedCampTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaRangedCampAction::Execute(Event /*event*/)
{
    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    Position const camp = GetFreyaLasherCampSpot(botAI, state);
    if (camp == Position())
        return false;

    // Stop a spacing short of the camp, on the bearing the bot is already on, so the back line lands as
    // a ring rather than on one square. Every bot keeps the direction it arrived from, which spreads
    // them with no shared state; the alternative measured 0.6 yd between neighbours and handed every
    // 8 yd hit the whole raid at once.
    float const distance = bot->GetExactDist2d(camp.GetPositionX(), camp.GetPositionY());
    float const bearing =
        distance > CONTACT_DISTANCE ? camp.GetAngle(bot->GetPositionX(), bot->GetPositionY()) : bot->GetOrientation();
    float const x = camp.GetPositionX() + std::cos(bearing) * ULDUAR_FREYA_RANGED_CAMP_SPACING;
    float const y = camp.GetPositionY() + std::sin(bearing) * ULDUAR_FREYA_RANGED_CAMP_SPACING;

    // No arrival latch: the trigger standing down inside the tolerance is what stops the churn, and a
    // latch held across ticks would swallow the re-aim when the pack moves.
    //
    // MOVEMENT_COMBAT, not FORCED, so a Nature Bomb or a Sun Beam still outranks it - gathering is the
    // lowest-value thing a bot can be doing on this encounter.
    return MoveTo(bot->GetMapId(), x, y, camp.GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool FreyaFrostNovaLashersAction::isUseful()
{
    FreyaFrostNovaLashersTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaFrostNovaLashersAction::Execute(Event /*event*/)
{
    // Cast on self rather than through "frost nova": that action gates on the *current target* being
    // within 10 yd, and a ranged mage's current target is the focused lasher, usually across the room.
    // Frost Nova is a sphere on the caster, so the pack standing on the mage is what it actually needs.
    return botAI->CastSpell("frost nova", bot);
}

bool FreyaTrapLashersAction::isUseful()
{
    FreyaTrapLashersTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaTrapLashersAction::Execute(Event /*event*/)
{
    // No walk to a post. The trap drops at the hunter's feet, which is exactly where the lasher that
    // picked this bot is heading, so it arms in the one place something is guaranteed to walk over.
    return botAI->CastSpell("frost trap", bot);
}

bool FreyaSummonArmyAction::isUseful()
{
    FreyaSummonArmyTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaSummonArmyAction::Execute(Event /*event*/)
{
    // Cast by name rather than through the class node, which is what keeps the burst gates off it: the
    // Ulduar window multiplier holds anything IsBurstCooldownAction recognises until Attuned to Nature
    // is gone, and it matches on the action name.
    return botAI->CastSpell("army of the dead", bot);
}

bool FreyaGroundTremorHoldCastAction::isUseful()
{
    FreyaGroundTremorHoldCastTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaGroundTremorHoldCastAction::Execute(Event /*event*/)
{
    Unit* boss = GetFreyaBossByEntry(botAI);
    if (!boss)
        return false;

    Spell* tremor = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    Spell* own = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!tremor || !own)
        return false;

    // A cast that still lands first is worth finishing. Only the ones the tremor would cut are thrown
    // away, and those are the expensive ones anyway: being cut is what applies the 10s school lockout.
    if (own->GetCastTimeRemaining() < tremor->GetCastTimeRemaining())
        return false;

    bot->CastStop();

    return true;
}
