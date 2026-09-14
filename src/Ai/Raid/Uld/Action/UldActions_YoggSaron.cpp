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

const Position yoggPortalLoc[] = {
    {1970.48f, -9.75f, 325.5f},  {1992.76f, -10.21f, 325.5f}, {1995.53f, -39.78f, 325.5f}, {1969.25f, -42.00f, 325.5f},
    {1960.62f, -32.00f, 325.5f}, {1981.98f, -5.69f, 325.5f},  {1982.78f, -45.73f, 325.5f}, {2000.66f, -29.68f, 325.5f},
    {1999.88f, -19.61f, 325.5f}, {1961.37f, -19.54f, 325.5f}};

bool YoggSaronGuardianPositioningAction::Execute(Event /*event*/)
{
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
        if (middle.GetExactDist2d(x, y) > ULDUAR_YOGG_SARON_SPACING_MAX_FROM_MIDDLE)
            return false;

        if (!RouteAcceptable(x, y))
            return false;

        return !set.clear || set.clear(x, y);
    };

    auto const capOnly = [this, &middle](float x, float y)
    {
        return middle.GetExactDist2d(x, y) <= ULDUAR_YOGG_SARON_SPACING_MAX_FROM_MIDDLE && RouteAcceptable(x, y);
    };

    // Every candidate in a ring is the same walk away, so preferNear is free and decides the whole
    // character of the dodge: biased at the middle it sidesteps along the orbit instead of running for
    // the rim.
    Position safe = FindNearestPositionClearOfHazards(bot, set.hazards, SearchRadius(), 2.0f,
                                                      static_cast<float>(M_PI) / 8.0f, &middle, accept, &sweep);

    // Nothing clears everything at once. Retry on the subset that kills, off the same cache.
    if (safe == Position() && !set.fallback.empty())
        safe = FindNearestPositionClearOfHazards(bot, set.fallback, SearchRadius(), 2.0f,
                                                 static_cast<float>(M_PI) / 8.0f, &middle, capOnly, &sweep);

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

    std::vector<Position> wedges = GetYoggSaronCrushWedges(botAI, SearchRadius() + ULDUAR_YOGG_SARON_CRUSH_RANGE);
    if (!wedges.empty())
    {
        set.clear = [wedges](float x, float y)
        { return !InYoggSaronCrushWedge(wedges, x, y, ULDUAR_YOGG_SARON_CRUSH_CLEAR_ARC); };

        // Rays only if nothing clears both: standing in one is certain death, while being in a wedge is
        // a coin-flip on the tentacle's swing timer.
        set.fallback = set.hazards;
    }

    return !set.hazards.empty() || !wedges.empty();
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

bool YoggSaronSetDpsPriorityAction::IsAllowedTarget(Unit* candidate, bool tentaclesCleared) const
{
    if (!candidate || !candidate->IsAlive() || !bot->IsWithinLOSInMap(candidate))
        return false;

    switch (candidate->GetEntry())
    {
        case NPC_BRAIN:
            return tentaclesCleared;
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
    bool const tentaclesCleared = brainLevel && YoggSaronInfluenceTentaclesCleared(botAI);

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
        if (tier >= tierCount || !IsAllowedTarget(unit, tentaclesCleared))
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
    if (currentTarget && IsAllowedTarget(currentTarget, tentaclesCleared))
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
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (player && player->IsAlive() && player->HasAura(SPELL_BRAIN_LINK) && player->GetGUID() != bot->GetGUID())
            return MoveNear(player, 10.0f, MovementPriority::MOVEMENT_FORCED);
    }

    return false;
}

bool YoggSaronMoveToEnterPortalAction::Execute(Event /*event*/)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    bool isInBrainRoomTeam = false;
    int portalNumber = 0;
    int brainRoomTeamCount = 10;
    if (bot->GetRaidDifficulty() == Difficulty::RAID_DIFFICULTY_10MAN_NORMAL)
        brainRoomTeamCount = 4;

    Player* master = botAI->GetMaster();
    if (master && !botAI->IsTank(master))
    {
        portalNumber++;
        brainRoomTeamCount--;
    }

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || !member->IsAlive() || botAI->IsTank(member) || botAI->GetMaster()->GetGUID() == member->GetGUID())
            continue;

        portalNumber++;
        if (member->GetGUID() == bot->GetGUID())
        {
            isInBrainRoomTeam = true;
            break;
        }

        brainRoomTeamCount--;
        if (brainRoomTeamCount == 0)
            break;
    }

    if (!isInBrainRoomTeam)
        return false;

    Position assignedPortalPosition = yoggPortalLoc[portalNumber - 1];

    botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("diamond");

    if (!YoggSaronWalkMakingProgress(botAI, "enter", assignedPortalPosition))
        return false;

    return MoveNear(bot->GetMapId(), assignedPortalPosition.GetPositionX(), assignedPortalPosition.GetPositionY(),
                    assignedPortalPosition.GetPositionZ(), sPlayerbotAIConfig.contactDistance,
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

    return assignedPortal->HandleSpellClick(bot);
}

bool YoggSaronIllusionRoomAction::Execute(Event /*event*/)
{
    YoggSaronTrigger yoggSaronTrigger(botAI);

    bool resultSetRtiMark = SetRtiMark(yoggSaronTrigger);
    bool resultGoToBrainRoom = GoToBrainRoom(yoggSaronTrigger);

    return resultSetRtiMark || resultGoToBrainRoom;
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
