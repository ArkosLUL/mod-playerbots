#include "UldActions_YoggSaron.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

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
#include "RaidObs.h"
#include "UldEncounter_YoggSaron.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "RtiValue.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include <TankAssistStrategy.h>

using namespace EncounterHelpers;

namespace
{
// The check behind "nearest npcs", with the entry tested first.
struct AnyUnitOfEntriesInRangeCheck
{
    Acore::AnyUnitInObjectRangeCheck inRange;
    uint32 const* entries;
    size_t count;

    bool operator()(Unit* unit)
    {
        return std::find(entries, entries + count, unit->GetEntry()) != entries + count && inRange(unit);
    }
};

// Guardians the raid finishes one at a time instead of spreading over three. In phase 3 a Shadow
// Beacon heal outruns split damage; in phase 1 two brought down together detonate together, and a
// pair of Shadow Novas killed all eight melee inside 16 ms.
bool IsYoggSaronFocusedGuardian(Unit* unit)
{
    uint32 const entry = unit->GetEntry();

    return entry == NPC_GUARDIAN_OF_YS || entry == NPC_IMMORTAL_GUARDIAN || entry == NPC_MARKED_IMMORTAL_GUARDIAN;
}
}  // namespace

const Position ULDUAR_YOGG_SARON_BOSS_ROOM_RESTORE_POINT = Position(1928.8923f, -24.871964f, 324.88956f, 6.247805f);

bool YoggSaronGuardianPositioningAction::Execute(Event /*event*/)
{
    if (YoggSaronHandoverState(botAI).clearing)
    {
        // Straight out along the bearing the bot already holds, so nine melee fan around the ring
        // rather than stacking on one point, and each of them walks the shortest line out there is.
        float const angle = ULDUAR_YOGG_SARON_MIDDLE.GetAngle(bot->GetPositionX(), bot->GetPositionY());
        float const radius = ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS;

        return MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionX() + radius * std::cos(angle),
                      ULDUAR_YOGG_SARON_MIDDLE.GetPositionY() + radius * std::sin(angle),
                      ULDUAR_YOGG_SARON_MIDDLE.GetPositionZ(), false, false, false, true,
                      MovementPriority::MOVEMENT_FORCED, true, false);
    }

    return MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY(),
                  ULDUAR_YOGG_SARON_MIDDLE.GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_FORCED, true, false);
}

bool YoggSaronSanityAction::Execute(Event /*event*/)
{
    Creature* sanityWell = bot->FindNearestCreature(NPC_SANITY_WELL, 200.0f);
    if (!sanityWell)
        return false;

    if (!YoggSaronWalkMakingProgress(botAI, "sanity", sanityWell->GetPosition()))
        return false;

    return MoveTo(bot->GetMapId(), sanityWell->GetPositionX(), sanityWell->GetPositionY(), sanityWell->GetPositionZ(),
                  false, false, false, true, MovementPriority::MOVEMENT_FORCED,
                  true, false);
}

bool YoggSaronSpacingAction::Execute(Event /*event*/)
{
    HazardSet set;
    if (!Collect(set))
        return false;

    auto const stillClear = [](std::vector<HazardCircle> const& circles,
                               std::function<bool(float, float)> const& clear, Position const& spot)
    {
        for (HazardCircle const& hazard : circles)
            if (hazard.first.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < hazard.second)
                return false;

        return !clear || clear(spot.GetPositionX(), spot.GetPositionY());
    };

    uint32 const now = getMSTime();
    bool const latched = heldSpotMs && getMSTimeDiff(heldSpotMs, now) < ULDUAR_YOGG_SARON_SPACING_HOLD_MS;

    // Claim the tick without touching the motion master while the step is in flight, so combat
    // movement cannot drag the bot back into what it just left and the spline survives.
    if (latched && stillClear(set.hazards, set.clear, heldSpot) && !bot->IsMovementPreventedByCasting() &&
        bot->GetExactDist2d(heldSpot.GetPositionX(), heldSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    heldSpotMs = 0;

    // Already outside everything, so this is what decides when a bot moves at all rather than where it
    // ends up. It has to agree with the trigger: a circle here that the trigger does not read leaves a
    // band where the bot sits in a hazard and the node is never asked.
    if (stillClear(set.hazards, set.clear, bot->GetPosition()))
        return false;

    HazardSweepCache sweep;
    Position const middle = Anchor();

    // The cap is the load-bearing half: a bot dodging outward otherwise walks out of spell range and
    // stops contributing for the rest of the phase.
    auto const accept = [this, &middle, &set](float x, float y)
    {
        if (middle.GetExactDist2d(x, y) > MaxFromMiddle())
            return false;

        if (!RouteAcceptable(x, y))
            return false;

        return !set.clear || set.clear(x, y);
    };

    auto const fallbackAccept = [this, &middle, &set](float x, float y)
    {
        if (middle.GetExactDist2d(x, y) > MaxFromMiddle())
            return false;

        if (!RouteAcceptable(x, y))
            return false;

        return !set.fallbackClear || set.fallbackClear(x, y);
    };

    // Somewhere the bot can still reach what it is killing, when such a spot exists. Without it the
    // hold lapses every 3 s, reach melee walks the bot back toward its target, the bot lands in a
    // hazard and this node walks it out again - 77 handovers between the two per melee bot over one
    // phase 2. Dropped rather than enforced: a bot in a hazard has to move whether or not it can shoot
    // from where it lands.
    Position safe;
    if (set.preferred)
    {
        auto const preferredAccept = [&accept, &set](float x, float y)
        { return accept(x, y) && set.preferred(x, y); };

        safe = FindNearestPositionClearOfHazards(bot, set.hazards, SearchRadius(), 2.0f,
                                                 static_cast<float>(M_PI) / 8.0f, &middle, preferredAccept,
                                                 &sweep);
    }

    // Every candidate in a ring is the same walk away, so preferNear is free and decides the whole
    // character of the dodge: biased at the middle it sidesteps along the orbit instead of running for
    // the rim.
    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, set.hazards, SearchRadius(), 2.0f,
                                                 static_cast<float>(M_PI) / 8.0f, &middle, accept, &sweep);

    // Nothing clears everything at once. Retry on the subset that kills, off the same cache.
    if (safe == Position() && !set.fallback.empty())
        safe = FindNearestPositionClearOfHazards(bot, set.fallback, SearchRadius(), 2.0f,
                                                 static_cast<float>(M_PI) / 8.0f, &middle, fallbackAccept, &sweep);

    if (safe == Position())
        return false;

    // MOVEMENT_FORCED: IsWaitingForLastMove yields only to a strictly higher priority, and at combat
    // priority a reach-spell walk already in flight wins the tick.
    if (!MoveTo(bot->GetMapId(), safe.GetPositionX(), safe.GetPositionY(), safe.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_FORCED, true, false))
        return false;

    heldSpot = safe;
    heldSpotMs = now;

    return true;
}

bool YoggSaronPhase1SpacingAction::Collect(HazardSet& set)
{
    clouds.clear();

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_OMINOUS_CLOUD,
                                        SearchRadius() + ULDUAR_YOGG_SARON_CLOUD_AVOID_RADIUS);
    for (Creature* cloud : found)
    {
        if (!cloud->IsAlive())
            continue;

        clouds.push_back(cloud->GetPosition());
        set.hazards.emplace_back(cloud->GetPosition(), ULDUAR_YOGG_SARON_CLOUD_AVOID_RADIUS);

        // And where it will be by the time the bot gets there. Without the lead a destination clear of
        // the orbit now is under it on arrival.
        set.hazards.emplace_back(YoggSaronCloudLead(cloud), ULDUAR_YOGG_SARON_CLOUD_AVOID_RADIUS);
    }

    std::vector<Unit*> const novas =
        GetYoggSaronNovaThreats(botAI, SearchRadius() + ULDUAR_YOGG_SARON_SHADOW_NOVA_CLEAR_RADIUS);
    for (Unit* guardian : novas)
        set.fallback.emplace_back(guardian->GetPosition(), ULDUAR_YOGG_SARON_SHADOW_NOVA_CLEAR_RADIUS);

    // Shadow Nova is the one that kills, so it is what the retry keeps when the clouds cannot also be
    // cleared.
    set.hazards.insert(set.hazards.end(), set.fallback.begin(), set.fallback.end());

    // With nothing about to detonate, a melee bot dodging a cloud otherwise steps out of its own swing
    // range and reach melee hauls it straight back - the two traded the tick 252 times in one pull.
    // Requiring the candidate to stay in reach ends the trade, and the retry drops it when nothing
    // satisfies both. Only toward a target on the stack: one further out is the tank's to fetch, and
    // chasing it put melee at a median 12-14 yd, on the inner orbit, in both pulls of 2026-09-16.
    if (novas.empty() && PlayerbotAI::IsMelee(bot))
    {
        Unit* target = AI_VALUE(Unit*, "current target");
        if (target && YoggSaronGuardianOnTheStack(target))
        {
            Position const at = target->GetPosition();
            float const reach = sPlayerbotAIConfig.meleeDistance;

            set.clear = [at, reach](float x, float y) { return at.GetExactDist2d(x, y) <= reach; };
            set.fallback = set.hazards;
        }
    }

    return !set.hazards.empty();
}

bool YoggSaronPhase1SpacingAction::RouteAcceptable(float x, float y) const
{
    return YoggSaronRouteClearOfClouds(bot, clouds, x, y);
}

bool YoggSaronPhase1StationAction::Execute(Event /*event*/)
{
    if (!YoggSaronWalkMakingProgress(botAI, "p1station", ULDUAR_YOGG_SARON_P1_RANGED_SPOT))
        return false;

    return MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_P1_RANGED_SPOT.GetPositionX(),
                  ULDUAR_YOGG_SARON_P1_RANGED_SPOT.GetPositionY(),
                  ULDUAR_YOGG_SARON_P1_RANGED_SPOT.GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_FORCED, true, false);
}

bool YoggSaronPhase2SpacingAction::Collect(HazardSet& set)
{
    std::list<Creature*> rays;
    bot->GetCreatureListWithEntryInGrid(rays, NPC_DEATH_RAY,
                                        SearchRadius() + ULDUAR_YOGG_SARON_DEATH_RAY_CLEAR_RADIUS);
    for (Creature* ray : rays)
        if (ray->IsAlive())
            set.hazards.emplace_back(ray->GetPosition(), ULDUAR_YOGG_SARON_DEATH_RAY_CLEAR_RADIUS);

    // Yogg's body, which is a hazard from the moment he emerges and stays one: 64022 triggers a 14 yd
    // knock back off him every second for the rest of the fight, and nothing in the world can be swept
    // for it. It costs melee nothing - his CombatReach is 30, so melee range on him is about 34 yd.
    set.hazards.emplace_back(ULDUAR_YOGG_SARON_MIDDLE, ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS);

    std::vector<Position> wedges = GetYoggSaronCrushWedges(botAI, SearchRadius() + ULDUAR_YOGG_SARON_CRUSH_RANGE);
    if (!wedges.empty())
    {
        set.clear = [wedges](float x, float y)
        { return !InYoggSaronCrushWedge(wedges, x, y, ULDUAR_YOGG_SARON_CRUSH_CLEAR_ARC); };

        // The wedge is what the retry keeps, and the rays are what it gives up. Across two pulls a
        // Death Ray killed once over 11 hits for 150,504; Crush killed four times over 19 hits for
        // 485,964, about 25k a hit against melee pools. Dropping the wedge to keep the rays put bots
        // back in the Crush line, which is where four of the five non-wipe deaths happened.
        set.fallbackClear = set.clear;
    }

    // The retry needs something to sweep against, and the body is the one circle that is always there.
    set.fallback.emplace_back(ULDUAR_YOGG_SARON_MIDDLE, ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS);

    // Ranged and healers only, and kept by the retry: it killed 3 of 3 times. Melee are already off the
    // Crusher, and a Constrictor spawned next to one would have this and reach melee trade them every
    // tick.
    if (!PlayerbotAI::IsMelee(bot))
    {
        for (Position const& reach :
             GetYoggSaronCrusherReaches(botAI, SearchRadius() + ULDUAR_YOGG_SARON_CRUSHER_REACH_CLEAR_RADIUS))
        {
            set.hazards.emplace_back(reach, ULDUAR_YOGG_SARON_CRUSHER_REACH_CLEAR_RADIUS);
            set.fallback.emplace_back(reach, ULDUAR_YOGG_SARON_CRUSHER_REACH_CLEAR_RADIUS);
        }
    }

    // Resolved once here rather than inside the sweep, which asks its filter hundreds of times.
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->IsAlive())
    {
        float const reach =
            botAI->IsMelee(bot) ? sPlayerbotAIConfig.meleeDistance : sPlayerbotAIConfig.spellDistance;
        float const targetX = target->GetPositionX();
        float const targetY = target->GetPositionY();

        set.preferred = [targetX, targetY, reach](float x, float y)
        { return std::hypot(targetX - x, targetY - y) <= reach; };
    }

    return true;
}

bool YoggSaronPhase2SpacingAction::RouteAcceptable(float x, float y) const
{
    return YoggSaronRouteClearOfBody(bot, x, y);
}

bool YoggSaronIllusionFacingAction::Collect(HazardSet& set)
{
    if (!YoggSaronRoomMiddle(bot, roomMiddle))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    std::vector<Position> const skulls = GetYoggSaronSkullsInRange(botAI);
    if (skulls.empty())
        return false;

    // FindNearestPositionClearOfHazards returns nothing for an empty hazard list, and the constraint
    // here is angular rather than radial, so the skulls go in as circles only to give the sweep
    // something to ring outward from. Standing under one is worth avoiding on its own account.
    for (Position const& skull : skulls)
        set.hazards.emplace_back(skull, ULDUAR_YOGG_SARON_SKULL_CLEAR_RADIUS);

    float const targetX = target->GetPositionX();
    float const targetY = target->GetPositionY();
    float const reach =
        botAI->IsMelee(bot) ? sPlayerbotAIConfig.meleeDistance : sPlayerbotAIConfig.spellDistance;

    // Staying in reach is a requirement, not a preference the sweep may drop. A spot out of reach puts
    // the bot back under reach melee, which walks it to the target and undoes this - one room spent
    // fifty seconds trading those two while the tentacle sat at 87%. No fallback either: every
    // candidate rejected here is one where the bot would be gazed anyway, so standing still and
    // fighting through it beats a walk that buys nothing.
    set.clear = [skulls, targetX, targetY, reach](float x, float y)
    {
        return std::hypot(targetX - x, targetY - y) <= reach &&
               YoggSaronFacingClearOfSkulls(skulls, x, y, targetX, targetY);
    };

    return true;
}

bool YoggSaronBodyDetourAction::Execute(Event /*event*/)
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    Position waypoint;
    if (!YoggSaronBodyDetour(bot, target->GetPosition(), waypoint))
        return false;

    // A blocked arc is worse than a straight walk, because this node claims the tick and reach cannot
    // run while it does. The latch hands the bot back after ULDUAR_YOGG_SARON_WALK_GIVE_UP_MS of it.
    if (!YoggSaronWalkMakingProgress(botAI, "detour", waypoint))
        return false;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.detour", "around");

    return MoveNear(bot->GetMapId(), waypoint.GetPositionX(), waypoint.GetPositionY(), waypoint.GetPositionZ(),
                    sPlayerbotAIConfig.contactDistance, MovementPriority::MOVEMENT_FORCED);
}

bool YoggSaronPetGuardAction::Execute(Event /*event*/)
{
    std::vector<Creature*> owned;
    for (Unit* controlled : bot->m_Controlled)
        if (Creature* creature = controlled->ToCreature())
            owned.push_back(creature);

    if (owned.empty())
        return false;

    std::list<Creature*> crushers;
    bot->GetCreatureListWithEntryInGrid(crushers, NPC_CRUSHER_TENTACLE,
                                        ULDUAR_YOGG_SARON_CRUSH_RANGE + ULDUAR_YOGG_SARON_SPACING_SEARCH_RADIUS);

    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && (!target->IsAlive() || target->GetEntry() == NPC_CRUSHER_TENTACLE))
        target = nullptr;

    bool pulled = false;
    for (Creature* pet : owned)
    {
        Creature* touching = nullptr;
        for (Creature* crusher : crushers)
        {
            // Either half is enough. Being the victim is what makes the tentacle swing, and standing
            // in reach is what lets it: SetInCombatWithZone gives it a threat list holding the whole
            // raid, so a pet that never attacked can still come up as the victim.
            if (crusher->IsAlive() && (crusher->GetVictim() == pet || crusher->IsWithinMeleeRange(pet)))
            {
                touching = crusher;
                break;
            }
        }

        if (!touching)
        {
            Unhush(pet);
            continue;
        }

        // Somewhere else to be. An idle pet is what PetAI reads as "pick your own", and its first two
        // picks are whoever is hitting the pet and whoever the owner is hitting - both this Crusher,
        // since the owner's target is a Crusher in every tick this node fires in. A pet that has a
        // living victim is never re-selected, so one good command is the whole fix.
        Unit* elsewhere = target ? target : YoggSaronPetFallbackTarget(botAI, pet);

        pet->AttackStop();

        if (elsewhere && pet->AI())
        {
            Unhush(pet);
            pet->AI()->AttackStart(elsewhere);
        }
        else
        {
            // Nothing else on the floor. Passive is the one state SelectNextTarget, AttackedBy and
            // OwnerAttacked all honour, so it is what keeps a pet out of the cone when there is no
            // other target to hold its attention.
            pet->SetReactState(REACT_PASSIVE);
            hushed.insert(pet->GetGUID());
            pet->GetMotionMaster()->MoveFollow(bot, PET_FOLLOW_DIST, pet->GetFollowAngle());
        }

        pulled = true;
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.petguard", pulled ? "pulled" : "clear");

    // Never claims the tick. Commanding a pet costs the bot nothing it was going to do with its own
    // body, and the node sits above the dps resolver only so it is asked before the pet swings again.
    return false;
}

void YoggSaronPetGuardAction::Unhush(Creature* pet)
{
    // Only pets this node silenced, so a stance the player set themselves is never overwritten.
    if (hushed.erase(pet->GetGUID()))
        pet->SetReactState(REACT_DEFENSIVE);
}

size_t YoggSaronSetDpsPriorityAction::TierOf(Unit* unit, bool brainLevel)
{
    constexpr size_t none = std::numeric_limits<size_t>::max();

    if (!unit)
        return none;

    uint32 const entry = unit->GetEntry();

    if (brainLevel)
    {
        // Influence Tentacles gate everything else: while one lives the Brain zeroes damage and kills
        // whoever dealt it, so nothing in the room can be made to count until they are down.
        if (entry == NPC_INFLUENCE_TENTACLE)
            return 0;

        if (std::find(ULDUAR_YOGG_SARON_ILLUSION_MOBS.begin(), ULDUAR_YOGG_SARON_ILLUSION_MOBS.end(), entry) !=
            ULDUAR_YOGG_SARON_ILLUSION_MOBS.end())
            return 1;

        return entry == NPC_BRAIN ? 2 : none;
    }

    // One boss-room ladder for both phases, led by whatever phase 1 left alive. Those carry over,
    // keep casting a 35 yd Dark Volley nothing can be walked out of, and regenerate to full the moment
    // the raid takes a portal. Left unordered they are picked off by nearest, which is how one pull
    // put two Guardians' worth of damage into five of them for no kill while taking 736,000 back.
    switch (entry)
    {
        case NPC_GUARDIAN_OF_YS:
            return 0;
        // Then the Crusher. Diminish Power is a 5-minute channel taking 21% off every point of damage
        // the raid does, multiplicative across tentacles, undispellable and unkickable - only a melee
        // hit (worth ~1.5 s) or the tentacle's death stops it.
        case NPC_CRUSHER_TENTACLE:
            return 1;
        case NPC_CONSTRICTOR_TENTACLE:
            return 2;
        case NPC_CORRUPTOR_TENTACLE:
            return 3;
        // Shadow Beacon swaps a guardian's entry to the marked one, then pours 750,000 of healing into
        // it over 20 s - 176% of its own max health. Focusing it is the only way it reaches Weakened
        // before that lands, and the entry swap makes it a free signal.
        case NPC_MARKED_IMMORTAL_GUARDIAN:
            return 4;
        case NPC_IMMORTAL_GUARDIAN:
            return 5;
        case NPC_YOGG_SARON:
            return 6;
        default:
            return none;
    }
}

bool YoggSaronSetDpsPriorityAction::IsAllowedTarget(Unit* candidate, bool brainApproachable) const
{
    if (!candidate || !candidate->IsAlive() || !bot->IsWithinLOSInMap(candidate))
        return false;

    switch (candidate->GetEntry())
    {
        case NPC_BRAIN:
            return brainApproachable;
        // Crush skips its own cone test inside 2 yd and re-aims onto whoever the tentacle is swinging
        // at, so a melee bot that was clear becomes collinear without moving. No angle answers that,
        // only not being there. Healers stay eligible: they are at range.
        case NPC_CRUSHER_TENTACLE:
            return !PlayerbotAI::IsMelee(bot);
        // Below 10% it is Weakened, and nothing but Thorim's Titanic Storm can finish one, so holding
        // there is a dead tick for the rest of the fight.
        case NPC_IMMORTAL_GUARDIAN:
        case NPC_MARKED_IMMORTAL_GUARDIAN:
            return candidate->GetHealthPct() > 10;
        // Shadow Barrier is what phase 2 is read off, and it makes him immune.
        case NPC_YOGG_SARON:
            return !candidate->HasAura(SPELL_SHADOW_BARRIER);
        default:
            return true;
    }
}

Unit* YoggSaronSetDpsPriorityAction::ResolveTarget(Unit* currentTarget)
{
    constexpr size_t none = std::numeric_limits<size_t>::max();
    constexpr float targetSwitchDistance = 10.0f;

    YoggSaronTrigger yoggSaronTrigger(botAI);
    bool const brainLevel = yoggSaronTrigger.IsInBrainLevel();

    std::vector<uint32> entries;
    size_t tierCount = 0;
    if (brainLevel)
    {
        entries = ULDUAR_YOGG_SARON_ILLUSION_MOBS;
        entries.push_back(NPC_BRAIN);
        tierCount = 3;
    }
    else
    {
        entries = {NPC_GUARDIAN_OF_YS,     NPC_CRUSHER_TENTACLE,  NPC_CONSTRICTOR_TENTACLE,
                   NPC_CORRUPTOR_TENTACLE, NPC_IMMORTAL_GUARDIAN, NPC_MARKED_IMMORTAL_GUARDIAN,
                   NPC_YOGG_SARON};
        tierCount = 7;
    }

    // The brain level needs the reach: its floor sits ~25 yd below the Brain and a portal drops the bot
    // 60-72 yd from it, so sight distance alone leaves bots with nothing to shoot. Attack() itself caps
    // at nothing but line of sight. The boss room is ~50 yd across and keeps the cheaper sweep.
    float const range = brainLevel ? 200.0f : sPlayerbotAIConfig.sightDistance;

    // The door as well as the tentacles. The Brain opens it in the same branch that fires when the
    // last Influence Tentacle in the room dies, so a bot reading the tentacles alone can start hitting
    // the Brain through a wall it has not been let through yet.
    bool const brainApproachable = brainLevel && YoggSaronBrainRoomApproachable(botAI);

    // "nearest npcs" cut down to these entries before its LOS test instead of after. Same units in the
    // same order, without a raycast for every pet and totem in the raid.
    std::vector<Unit*> candidates;
    AnyUnitOfEntriesInRangeCheck check{Acore::AnyUnitInObjectRangeCheck(bot, range), entries.data(), entries.size()};
    Acore::UnitListSearcher<AnyUnitOfEntriesInRangeCheck> searcher(bot, candidates, check);
    Cell::VisitObjects(bot, searcher, range);

    std::vector<Unit*> perTier(tierCount, nullptr);
    for (Unit* unit : candidates)
    {
        size_t const tier = TierOf(unit, brainLevel);
        if (tier >= tierCount || !IsAllowedTarget(unit, brainApproachable))
            continue;

        Unit*& selected = perTier[tier];
        if (!selected)
        {
            selected = unit;
            continue;
        }

        // Guardians go down lowest first so the raid's damage finishes one instead of spreading over
        // three; everything else is nearest, which is the shortest walk into range.
        bool better;
        if (IsYoggSaronFocusedGuardian(unit))
            better = unit->GetHealth() < selected->GetHealth();
        else
            better = unit->GetExactDist2d(bot) < selected->GetExactDist2d(bot);

        if (better)
            selected = unit;
    }

    Unit* target = nullptr;
    size_t desiredTier = none;
    for (size_t tier = 0; tier < tierCount; ++tier)
    {
        if (perTier[tier])
        {
            target = perTier[tier];
            desiredTier = tier;
            break;
        }
    }

    size_t currentTier = none;
    if (currentTarget && IsAllowedTarget(currentTarget, brainApproachable))
        currentTier = TierOf(currentTarget, brainLevel);

    if (currentTier != none && currentTier <= desiredTier)
    {
        // Never downgrade off something at least as urgent, and inside one tier only switch for
        // something meaningfully closer - otherwise two tentacles ping-pong the whole raid. Guardians
        // hold outright: that tier is ordered by health, and an order that flips mid-fight would reset
        // every swing and cast timer in the raid.
        if (currentTier < desiredTier || !target || IsYoggSaronFocusedGuardian(currentTarget) ||
            target->GetExactDist2d(bot) + targetSwitchDistance >= currentTarget->GetExactDist2d(bot))
        {
            target = currentTarget;
        }
    }

    // The illusion rooms hold adds the module does not enumerate, so a hard stop here would park a bot
    // with nothing to do rather than let it shoot what is in front of it.
    return target ? target : AI_VALUE(Unit*, "dps target");
}

void YoggSaronSetDpsPriorityAction::DropTarget(Unit* target)
{
    bot->AttackStop();
    bot->InterruptNonMeleeSpells(true);
    bot->SetTarget(ObjectGuid::Empty);
    bot->SetSelection(ObjectGuid());
    context->GetValue<Unit*>("current target")->Set(nullptr);

    for (Unit* minion : bot->m_Controlled)
        if (minion && minion->GetVictim() == target)
            minion->AttackStop();
}

bool YoggSaronSetDpsPriorityAction::Execute(Event /*event*/)
{
    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    YoggSaronTrigger yoggSaronTrigger(botAI);
    bool const phaseOne = !yoggSaronTrigger.IsInBrainLevel() && YoggSaronInPhase1(botAI);

    // Phase 1 is one shared target and no "dps target" fallback. A parked Guardian stays parked even
    // when nothing else is killable: two killed out at the station on 2026-09-16 novaed the back line
    // 2.5 s apart and took five raiders.
    if (phaseOne && currentTarget && currentTarget->GetEntry() == NPC_GUARDIAN_OF_YS &&
        !YoggSaronPhase1GuardianKillable(botAI, currentTarget))
    {
        DropTarget(currentTarget);
        currentTarget = nullptr;
    }

    Unit* target = phaseOne ? YoggSaronPhase1Focus(botAI) : ResolveTarget(currentTarget);
    if (!target)
        return false;

    bool needsAttack = currentTarget != target;
    if (PlayerbotAI::IsMelee(bot))
        needsAttack = needsAttack || !bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING);

    // Returning false once the bot is on the right target is what lets the lower-priority nodes run:
    // the engine ends the tick at the first action that succeeds.
    return needsAttack ? Attack(target) : false;
}

int32 YoggSaronDarkVolleyInterruptAction::GetInterrupterIndex()
{
    Group* group = bot->GetGroup();
    if (!group)
        return 0;

    int32 index = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !YoggSaronCanInterrupt(member))
            continue;

        if (member == bot)
            return index;

        ++index;
    }

    return 0;
}

bool YoggSaronDarkVolleyInterruptAction::CastClassInterrupt(Unit* target)
{
    for (char const* spell : YoggSaronInterruptSpells(bot))
        if (botAI->CanCastSpell(spell, target) && botAI->CastSpell(spell, target))
            return true;

    return false;
}

bool YoggSaronDarkVolleyInterruptAction::Execute(Event /*event*/)
{
    std::vector<Unit*> casters = GetYoggSaronDarkVolleyCasters(botAI);
    if (casters.empty())
        return false;

    // Start each interrupter at its own slot. Several Guardians cast at once and the cooldowns run
    // 6-24s, so without the offset the whole raid spends its kicks on one volley.
    size_t const start = static_cast<size_t>(GetInterrupterIndex()) % casters.size();

    for (size_t i = 0; i < casters.size(); ++i)
        if (CastClassInterrupt(casters[(start + i) % casters.size()]))
            return true;

    return false;
}

bool YoggSaronDiminishPowerJudgementAction::Execute(Event /*event*/)
{
    std::vector<char const*> const spells = YoggSaronJudgementSpells(bot);

    // No walk and no target swap, only what is already in reach. No stagger either: the Judgement lands
    // inside Spell::cast, so the next paladin's read finds the channel already gone.
    for (Unit* crusher : GetYoggSaronChannellingCrushers(botAI))
    {
        for (char const* spell : spells)
        {
            if (!botAI->CanCastSpell(spell, crusher) || !botAI->CastSpell(spell, crusher))
                continue;

            if (RaidObs::Active())
                RaidObs::NoteDerived(bot, "yogg.judgement", "cast");

            return true;
        }
    }

    return false;
}

bool YoggSaronPhase3ControlAction::Execute(Event /*event*/)
{
    bool acted = false;

    TankFaceStrategy tankFaceStrategy(botAI);
    if (botAI->HasStrategy(tankFaceStrategy.getName(), BotState::BOT_STATE_COMBAT))
    {
        botAI->ChangeStrategy(REMOVE_STRATEGY_CHAR + tankFaceStrategy.getName(), BotState::BOT_STATE_COMBAT);
        acted = true;
    }

    TankAssistStrategy tankAssistStrategy(botAI);
    if (!botAI->HasStrategy(tankAssistStrategy.getName(), BotState::BOT_STATE_COMBAT))
    {
        botAI->ChangeStrategy(ADD_STRATEGY_CHAR + tankAssistStrategy.getName(), BotState::BOT_STATE_COMBAT);
        acted = true;
    }

    return acted;
}

bool YoggSaronBrainLinkAction::Execute(Event /*event*/)
{
    Player* partner = YoggSaronBrainLinkTarget(botAI);
    if (!partner)
        return false;

    // Both ends run this, so both walk and the gap closes at twice the rate. Aiming at the partner
    // instead is a chase, and a chase after somebody moving away at the same speed never arrives -
    // one link ran 17 walks while the gap grew from 45.8 to 78.5 yd.
    float x = (bot->GetPositionX() + partner->GetPositionX()) / 2.0f;
    float y = (bot->GetPositionY() + partner->GetPositionY()) / 2.0f;

    // Two bots on opposite sides of Yogg have the body itself as their midpoint, which knocks both of
    // them back once a second. Pushed straight out along the same bearing, so the two still meet.
    float const middleX = ULDUAR_YOGG_SARON_MIDDLE.GetPositionX();
    float const middleY = ULDUAR_YOGG_SARON_MIDDLE.GetPositionY();
    float const fromMiddle = std::hypot(x - middleX, y - middleY);
    if (fromMiddle < ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS)
    {
        float const heading = fromMiddle > 0.1f ? std::atan2(y - middleY, x - middleX) : bot->GetOrientation();
        x = middleX + std::cos(heading) * ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS;
        y = middleY + std::sin(heading) * ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS;
    }

    return MoveNear(bot->GetMapId(), x, y, bot->GetPositionZ(), sPlayerbotAIConfig.contactDistance,
                    MovementPriority::MOVEMENT_FORCED);
}

bool YoggSaronMoveToEnterPortalAction::Execute(Event /*event*/)
{
    Position spot;
    YoggSaronPortalIntent const intent = YoggSaronPortalPlan(botAI, spot);
    if (intent != YOGG_SARON_PORTAL_SPREADING && intent != YOGG_SARON_PORTAL_LATE)
        return false;

    botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("diamond");

    // Progress is measured against the spot, not the waypoint: the detour's bisector moves as the bot
    // walks, and a latch keyed on a moving destination is a new latch every tick.
    if (!YoggSaronWalkMakingProgress(botAI, "enter", spot))
        return false;

    // Straight across the room is straight through the body, which throws the bot back once a second
    // for as long as it is inside the ring. One waypoint turns the crossing into an arc, and each leg
    // halves the turn the next one has to make.
    Position destination = spot;
    Position waypoint;
    if (YoggSaronBodyDetour(bot, spot, waypoint))
        destination = waypoint;

    return MoveNear(bot->GetMapId(), destination.GetPositionX(), destination.GetPositionY(),
                    destination.GetPositionZ(), sPlayerbotAIConfig.contactDistance,
                    MovementPriority::MOVEMENT_FORCED);
}

bool YoggSaronFallFromFloorAction::Execute(Event /*event*/)
{
    std::string rtiMark = AI_VALUE(std::string, "rti");
    if (rtiMark == "skull")
    {
        return bot->TeleportTo(bot->GetMapId(), ULDUAR_YOGG_SARON_BOSS_ROOM_RESTORE_POINT.GetPositionX(),
                               ULDUAR_YOGG_SARON_BOSS_ROOM_RESTORE_POINT.GetPositionY(),
                               ULDUAR_YOGG_SARON_BOSS_ROOM_RESTORE_POINT.GetPositionZ(),
                               ULDUAR_YOGG_SARON_BOSS_ROOM_RESTORE_POINT.GetOrientation());
    }
    if (rtiMark == "cross")
    {
        return bot->TeleportTo(bot->GetMapId(), ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE.GetPositionX(),
                               ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE.GetPositionY(),
                               ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE.GetPositionZ(),
                               bot->GetOrientation());
    }
    if (rtiMark == "circle")
    {
        return bot->TeleportTo(bot->GetMapId(), ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE.GetPositionX(),
                               ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE.GetPositionY(),
                               ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE.GetPositionZ(), bot->GetOrientation());
    }
    if (rtiMark == "star")
    {
        return bot->TeleportTo(bot->GetMapId(), ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE.GetPositionX(),
                               ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE.GetPositionY(),
                               ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE.GetPositionZ(), bot->GetOrientation());
    }
    return false;
}

bool YoggSaronStopFollowingAction::Execute(Event /*event*/)
{
    FollowMasterStrategy followMasterStrategy(botAI);
    if (!botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
        return false;

    botAI->ChangeStrategy(REMOVE_STRATEGY_CHAR + followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT);
    return true;
}

bool YoggSaronUsePortalAction::Execute(Event /*event*/)
{
    Creature* assignedPortal =
        bot->FindNearestCreature(NPC_DESCEND_INTO_MADNESS, ULDUAR_YOGG_SARON_PORTAL_CLICK_RADIUS, true);
    if (!assignedPortal)
        return false;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.portal", "clicking");

    return assignedPortal->HandleSpellClick(bot);
}

bool YoggSaronIllusionRoomAction::Execute(Event /*event*/)
{
    YoggSaronTrigger yoggSaronTrigger(botAI);

    bool resultSetRtiMark = SetRtiMark(yoggSaronTrigger);
    bool resultGoToBrainRoom = GoToBrainRoom(yoggSaronTrigger);

    return resultSetRtiMark || resultGoToBrainRoom || WalkIntoRoom();
}

bool YoggSaronIllusionRoomAction::WalkIntoRoom()
{
    if (YoggSaronRoomStateOf(botAI) != YOGG_SARON_ROOM_STATE_WALKING_IN)
        return false;

    // The room's middle is the centroid of its Influence Tentacle summon group, so walking there is
    // what puts the tentacles in line of sight - which is all the dps resolver was ever waiting for.
    Position middle;
    if (!YoggSaronRoomMiddle(bot, middle))
        return false;

    if (!YoggSaronWalkMakingProgress(botAI, "illusion", middle))
        return false;

    return MoveTo(bot->GetMapId(), middle.GetPositionX(), middle.GetPositionY(), middle.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_FORCED, true, false);
}

bool YoggSaronIllusionRoomAction::SetRtiMark(YoggSaronTrigger yoggSaronTrigger)
{
    if (AI_VALUE(std::string, "rti") == "diamond")
    {
        if (yoggSaronTrigger.IsInStormwindKeeperIllusion())
        {
            botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("cross");
            return true;
        }
        else if (yoggSaronTrigger.IsInIcecrownKeeperIllusion())
        {
            botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("circle");
            return true;
        }
        else if (yoggSaronTrigger.IsInChamberOfTheAspectsIllusion())
        {
            botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("star");
            return true;
        }
    }
    return false;
}

bool YoggSaronIllusionRoomAction::GoToBrainRoom(YoggSaronTrigger yoggSaronTrigger)
{
    if (AI_VALUE(std::string, "rti") == "square" || !yoggSaronTrigger.IsBrainRoomApproachable())
        return false;

    botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("square");

    // The room's middle, not its entrance: a bot parked at the doorway healed from there for 40 s while
    // the Brain sat untouched. The dps priority resolver picks the Brain up once the bot is inside.
    MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE.GetPositionX(),
           ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE.GetPositionY(), ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE.GetPositionZ(),
           false, false, false, true, MovementPriority::MOVEMENT_FORCED, true, false);

    return true;
}

bool YoggSaronMoveToExitPortalAction::Execute(Event /*event*/)
{
    // Three of these are permanently spawned around the brain level, and a window can end with the bot
    // ~120 yd from the nearest one.
    std::list<GameObject*> found;
    bot->GetGameObjectListWithEntryInGrid(found, GO_FLEE_TO_THE_SURFACE_PORTAL, 200.0f);

    std::vector<GameObject*> portals(found.begin(), found.end());
    std::sort(portals.begin(), portals.end(), [this](GameObject* left, GameObject* right)
              { return bot->GetDistance2d(left) < bot->GetDistance2d(right); });

    for (GameObject* portal : portals)
    {
        // Standing down is fatal here - Induce Madness lands, and a mind control is always a death - so
        // a route going nowhere moves on to the next portal instead of giving up on the lot. They sit
        // far enough apart that a blocked route to one says nothing about the others.
        if (!YoggSaronWalkMakingProgress(botAI, "exit", portal->GetPosition()))
            continue;

        MoveTo(bot->GetMapId(), portal->GetPositionX(), portal->GetPositionY(), portal->GetPositionZ(), false, false,
               false, true, MovementPriority::MOVEMENT_FORCED, true, false);

        if (bot->GetDistance2d(portal) > 2.0f)
            return false;

        portal->Use(bot);

        botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("skull");
        return true;
    }

    return false;
}

bool YoggSaronLaughingSkullAction::Execute(Event /*event*/)
{
    std::vector<Unit*> const skulls = GetYoggSaronSkullsInArc(botAI);
    if (skulls.empty())
        return false;

    // Never while there is something to kill. An illusion room is a 60 s race for eight Influence
    // Tentacles, there is no way out until they are dead, and everyone still inside when Induce
    // Madness lands loses all 100 Sanity - against 1750 damage and 2 Sanity a second for looking at a
    // skull. A bot cannot face away from what it is attacking in any case: set facing, AttackAction
    // and CastSpell each turn it back within the same tick. Turning anyway cost one pull six melee,
    // who stood on one spot flipping between two headings for 48 s apiece with a tentacle 78 yd away.
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->IsAlive())
        return false;

    float x = 0.0f;
    float y = 0.0f;
    for (Unit* skull : skulls)
    {
        x += skull->GetPositionX();
        y += skull->GetPositionY();
    }

    x /= skulls.size();
    y /= skulls.size();

    // Away from the centroid. Four skulls to a room can be spread wide enough that no heading clears
    // all of them, and the middle of the ones that are in arc is the heading that clears the most.
    float const away = Position::NormalizeOrientation(bot->GetAngle(x, y) + static_cast<float>(M_PI));

    float drift = std::fabs(Position::NormalizeOrientation(bot->GetOrientation() - away));
    if (drift > static_cast<float>(M_PI))
        drift = 2.0f * static_cast<float>(M_PI) - drift;

    if (drift <= ULDUAR_YOGG_SARON_FACING_TOLERANCE)
        return false;

    bot->SetFacingTo(away);

    // The tick is never claimed. Walking into the room and leaving it both sit below this node and
    // both matter more than a heading, and a heading needs no tick of its own to hold once nothing
    // is turning the bot back.
    return false;
}

bool YoggSaronLunaticGazeAction::Execute(Event /*event*/)
{
    Creature* boss = bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true);
    if (!boss || !boss->IsAlive())
        return false;

    float angle = bot->GetAngle(boss);
    float newAngle = Position::NormalizeOrientation(angle + M_PI);  // Add 180 degrees (PI radians)
    bot->SetFacingTo(newAngle);

    return true;
}

bool YoggSaronPhase3PositioningAction::Execute(Event /*event*/)
{
    // A tank is only ever walked back on the leash: wherever it drifted to, a guardian took it there.
    if (botAI->IsTank(bot))
    {
        return MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionX(),
                      ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionY(),
                      ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionZ(), false, false, false, true,
                      MovementPriority::MOVEMENT_FORCED, true, false);
    }

    Position const& spot =
        botAI->IsRanged(bot) ? ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT : ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT;

    YoggSaronTrigger yoggSaronTrigger(botAI);
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->IsAlive() && !yoggSaronTrigger.PhaseThreeStationReaches(target))
        return false;

    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  true, MovementPriority::MOVEMENT_FORCED, true, false);
}

bool YoggSaronGuardianControlAction::Execute(Event /*event*/)
{
    // Tank only: melee/ranged already focus the skull guardian and the phase-3 positioning stacks them.
    if (!botAI->IsTank(bot))
        return false;

    if (YoggSaronInPhase1(botAI))
        return ControlPhaseOne();

    // Hold the melee stack so taunted guardians pile onto the melee bots to be cleaved down.
    if (bot->GetDistance(ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT) > 5.0f)
    {
        return MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionX(),
                      ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionY(),
                      ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionZ(), false, false, false, true,
                      MovementPriority::MOVEMENT_FORCED, true, false);
    }

    // Taunt the nearest loose guardian (not already coming to a tank) so it comes to the stack.
    GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
    Unit* looseGuardian = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_IMMORTAL_GUARDIAN && unit->GetEntry() != NPC_MARKED_IMMORTAL_GUARDIAN)
            continue;

        Player* targetedPlayer = botAI->GetPlayer(unit->GetTarget());
        if (targetedPlayer && botAI->IsTank(targetedPlayer))
            continue;

        float distance = bot->GetDistance(unit);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            looseGuardian = unit;
        }
    }

    if (!looseGuardian)
        return false;

    return Taunt(looseGuardian);
}

bool YoggSaronGuardianControlAction::ControlPhaseOne()
{
    // Inside the cloud-free circle, not the 5 yd the phase 3 stack uses. A taunted Guardian walks to
    // the tank, so wherever the tank stands is where it dies, and outside 2.89 yd the innermost orbit
    // sweeps the pile - one pull fed it 8 of its 23 Guardians that way. Taunt reaches 30 yd from here,
    // which covers the whole 21.5 yd back line, so this is never a walk out after one.
    if (bot->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY()) >
        ULDUAR_YOGG_SARON_P1_CLOUD_FREE_RADIUS)
    {
        return MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(),
                      ULDUAR_YOGG_SARON_MIDDLE.GetPositionY(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionZ(), false,
                      false, false, true, MovementPriority::MOVEMENT_FORCED, true, false);
    }

    Unit* guardian = YoggSaronPhase1TauntTarget(botAI);
    if (!guardian)
        return false;

    // Whether the Guardian being fetched is still somewhere its death would pay for the phase, which
    // is the one thing worth knowing about a taunt here after the fact.
    if (RaidObs::Active())
    {
        RaidObs::NoteDerived(bot, "yogg.p1taunt",
                             YoggSaronGuardianCountsForSara(guardian) ? "counts" : "beyond");
    }

    return Taunt(guardian);
}

bool YoggSaronGuardianControlAction::Taunt(Unit* guardian)
{
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
            return botAI->CastSpell("taunt", guardian);
        case CLASS_PALADIN:
            return botAI->CastSpell("hand of reckoning", guardian);
        case CLASS_DEATH_KNIGHT:
            return botAI->CastSpell("dark command", guardian);
        case CLASS_DRUID:
            return botAI->CastSpell("growl", guardian);
        default:
            return false;
    }
}

bool YoggSaronSanityConservationAction::Execute(Event /*event*/)
{
    Unit* yogg = AI_VALUE2(Unit*, "find target", "yogg-saron");
    if (!yogg || !yogg->IsAlive())
        return false;

    // Pull to the back of Yogg-Saron - the spot behind him, opposite his facing.
    float const behindDistance = 15.0f;
    float behindAngle = Position::NormalizeOrientation(yogg->GetOrientation() + M_PI);
    float behindX = yogg->GetPositionX() + behindDistance * cos(behindAngle);
    float behindY = yogg->GetPositionY() + behindDistance * sin(behindAngle);
    float behindZ = yogg->GetPositionZ();

    if (bot->GetDistance2d(behindX, behindY) > 5.0f)
    {
        return MoveTo(bot->GetMapId(), behindX, behindY, behindZ, false, false, false, true,
                      MovementPriority::MOVEMENT_FORCED, true, false);
    }

    // Face directly away from Yogg: Lunatic Gaze only hits units with him in their front arc.
    float awayAngle = Position::NormalizeOrientation(bot->GetAngle(yogg) + M_PI);
    bot->SetFacingTo(awayAngle);

    // Only heal/DPS a target already in the front hemisphere (away from Yogg), so the bot never
    // turns back toward Yogg and eats a gaze. Otherwise hold, facing away.
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->IsAlive())
    {
        float diff = Position::NormalizeOrientation(bot->GetAngle(target) - awayAngle);
        if (diff > M_PI)
            diff = 2 * M_PI - diff;
        if (diff <= M_PI / 2)
            return false;
    }

    return true;
}

bool YoggSaronSqueezeRescueAction::Execute(Event /*event*/)
{
    Player* victim = YoggSaronSqueezeVictim(botAI);
    if (!victim)
        return false;

    // Hand of Protection's own debuff, matched by id because PlayerbotAI::HasAura compares the DBC
    // string exactly. A second Hand inside two minutes is refused outright, so this is not a nicety.
    constexpr uint32 SPELL_FORBEARANCE = 25771;
    if (victim->HasAura(SPELL_FORBEARANCE))
        return false;

    // The 3.3.5a name. It was Blessing of Protection in 2.x, and SpellIdValue matches the DBC string
    // exactly - the old spelling resolves to no spell at all.
    if (!botAI->CanCastSpell("hand of protection", victim))
        return false;

    if (!ClaimYoggSaronSqueezeRescue(botAI, victim))
        return false;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.squeeze", "rescue");

    return botAI->CastSpell("hand of protection", victim);
}

bool YoggSaronSqueezeEscapeAction::Execute(Event /*event*/)
{
    switch (bot->getClass())
    {
        case CLASS_MAGE:
            return botAI->CanCastSpell("ice block", bot) && botAI->CastSpell("ice block", bot);

        case CLASS_PALADIN:
            return botAI->CanCastSpell("divine shield", bot) && botAI->CastSpell("divine shield", bot);

        default:
            return false;
    }
}
