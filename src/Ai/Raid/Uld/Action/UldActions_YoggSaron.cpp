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
    Position const middle = ULDUAR_YOGG_SARON_MIDDLE;

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
    // satisfies both.
    if (novas.empty() && PlayerbotAI::IsMelee(bot))
    {
        if (Unit* target = AI_VALUE(Unit*, "current target"))
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

size_t YoggSaronSetDpsPriorityAction::TierOf(Unit* unit, bool brainLevel, bool phaseOne)
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

    // Phase 1 has one tier because one Guardian at a time is the whole point: splitting damage let
    // two come down together once, and the double nova killed all eight melee inside 16 ms.
    if (phaseOne)
        return entry == NPC_GUARDIAN_OF_YS ? 0 : none;

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
    bool const phaseOne = !brainLevel && YoggSaronInPhase1(botAI);

    std::vector<uint32> entries;
    size_t tierCount = 0;
    if (brainLevel)
    {
        entries = ULDUAR_YOGG_SARON_ILLUSION_MOBS;
        entries.push_back(NPC_BRAIN);
        tierCount = 3;
    }
    else if (phaseOne)
    {
        entries = {NPC_GUARDIAN_OF_YS};
        tierCount = 1;
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
        size_t const tier = TierOf(unit, brainLevel, phaseOne);
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
        bool const better = IsYoggSaronFocusedGuardian(unit)
                                ? unit->GetHealth() < selected->GetHealth()
                                : unit->GetExactDist2d(bot) < selected->GetExactDist2d(bot);
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
        currentTier = TierOf(currentTarget, brainLevel, phaseOne);

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

bool YoggSaronSetDpsPriorityAction::Execute(Event /*event*/)
{
    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    Unit* target = ResolveTarget(currentTarget);
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
    // The nearest raider, not the first group member carrying the aura: Brain Link puts 63802 on one
    // end only, so iterating for a second holder finds nobody and the old walk went to whoever the
    // group happened to list first.
    Player* partner = YoggSaronBrainLinkTarget(botAI);
    if (!partner)
        return false;

    return MoveNear(partner, ULDUAR_YOGG_SARON_BRAIN_LINK_CLOSE, MovementPriority::MOVEMENT_FORCED);
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
    Creature* assignedPortal = bot->FindNearestCreature(NPC_DESCEND_INTO_MADNESS, 2.0f, true);
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

    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
            return botAI->CastSpell("taunt", looseGuardian);
        case CLASS_PALADIN:
            return botAI->CastSpell("hand of reckoning", looseGuardian);
        case CLASS_DEATH_KNIGHT:
            return botAI->CastSpell("dark command", looseGuardian);
        case CLASS_DRUID:
            return botAI->CastSpell("growl", looseGuardian);
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
