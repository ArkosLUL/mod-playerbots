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
#include "UldHardMode.h"
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

bool IsYoggSaronImmortalGuardian(Unit* unit)
{
    return unit->GetEntry() == NPC_IMMORTAL_GUARDIAN || unit->GetEntry() == NPC_MARKED_IMMORTAL_GUARDIAN;
}
}  // namespace

const Position ULDUAR_YOGG_SARON_BOSS_ROOM_RESTORE_POINT = Position(1928.8923f, -24.871964f, 324.88956f, 6.247805f);

const Position yoggPortalLoc[] = {
    {1970.48f, -9.75f, 325.5f},  {1992.76f, -10.21f, 325.5f}, {1995.53f, -39.78f, 325.5f}, {1969.25f, -42.00f, 325.5f},
    {1960.62f, -32.00f, 325.5f}, {1981.98f, -5.69f, 325.5f},  {1982.78f, -45.73f, 325.5f}, {2000.66f, -29.68f, 325.5f},
    {1999.88f, -19.61f, 325.5f}, {1961.37f, -19.54f, 325.5f}};

bool YoggSaronOminousCloudCheatAction::Execute(Event /*event*/)
{
    YoggSaronTrigger yoggSaronTrigger(botAI);

    Unit* boss = yoggSaronTrigger.GetSaraIfAlive();
    if (!boss)
        return false;

    Creature* target = boss->FindNearestCreature(NPC_OMINOUS_CLOUD, 25.0f);
    if (!target || !target->IsAlive())
        return false;

    target->Kill(bot, target);
    return true;
}

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

    // Already outside everything. The trigger fires at a tighter radius and a tighter arc than this on
    // purpose: move at 10 yd from a cloud, rest at 14, so a hazard drifting a yard closer cannot
    // restart the dance.
    if (stillClear(set.hazards, set.clear, bot->GetPosition()))
        return false;

    HazardSweepCache sweep;
    Position const middle = ULDUAR_YOGG_SARON_MIDDLE;

    // The cap is the load-bearing half: a bot dodging outward otherwise walks out of spell range and
    // stops contributing for the rest of the phase.
    auto const accept = [&middle, &set](float x, float y)
    {
        if (middle.GetExactDist2d(x, y) > ULDUAR_YOGG_SARON_SPACING_MAX_FROM_MIDDLE)
            return false;

        return !set.clear || set.clear(x, y);
    };

    auto const capOnly = [&middle](float x, float y)
    { return middle.GetExactDist2d(x, y) <= ULDUAR_YOGG_SARON_SPACING_MAX_FROM_MIDDLE; };

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
    // Melee and tanks only dodge clouds. Standing in Shadow Nova is the price of killing a Guardian,
    // and a Guardian dying next to Sara is the only way she takes damage at all.
    bool const avoidNova = PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsHeal(bot);

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_OMINOUS_CLOUD,
                                        SearchRadius() + ULDUAR_YOGG_SARON_CLOUD_CLEAR_RADIUS);
    for (Creature* cloud : found)
        if (cloud->IsAlive())
            set.hazards.emplace_back(cloud->GetPosition(), ULDUAR_YOGG_SARON_CLOUD_CLEAR_RADIUS);

    if (avoidNova)
    {
        found.clear();
        bot->GetCreatureListWithEntryInGrid(found, NPC_GUARDIAN_OF_YS,
                                            SearchRadius() + ULDUAR_YOGG_SARON_SHADOW_NOVA_CLEAR_RADIUS);
        for (Creature* guardian : found)
            if (guardian->IsAlive())
                set.fallback.emplace_back(guardian->GetPosition(), ULDUAR_YOGG_SARON_SHADOW_NOVA_CLEAR_RADIUS);

        // Shadow Nova is the one that kills, so it is what the retry keeps when the clouds cannot also
        // be cleared.
        set.hazards.insert(set.hazards.end(), set.fallback.begin(), set.fallback.end());
    }

    return !set.hazards.empty();
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

    // One boss-room ladder for both phases: the tentacles are gone by the time a Guardian exists, so
    // the tail never competes with the head.
    switch (entry)
    {
        // The Crusher leads it. Diminish Power is a 5-minute channel taking 21% off every point of
        // damage the raid does, multiplicative across tentacles, undispellable and unkickable - only a
        // melee hit (worth ~1.5 s) or the tentacle's death stops it.
        case NPC_CRUSHER_TENTACLE:
            return 0;
        case NPC_CONSTRICTOR_TENTACLE:
            return 1;
        case NPC_CORRUPTOR_TENTACLE:
            return 2;
        case NPC_IMMORTAL_GUARDIAN:
        case NPC_MARKED_IMMORTAL_GUARDIAN:
            return 3;
        case NPC_YOGG_SARON:
            return 4;
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
        entries = {NPC_CRUSHER_TENTACLE,  NPC_CONSTRICTOR_TENTACLE,     NPC_CORRUPTOR_TENTACLE,
                   NPC_IMMORTAL_GUARDIAN, NPC_MARKED_IMMORTAL_GUARDIAN, NPC_YOGG_SARON};
        tierCount = 5;
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
        size_t const tier = TierOf(unit, brainLevel);
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
        bool const better = IsYoggSaronImmortalGuardian(unit)
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
        currentTier = TierOf(currentTarget, brainLevel);

    if (currentTier != none && currentTier <= desiredTier)
    {
        // Never downgrade off something at least as urgent, and inside one tier only switch for
        // something meaningfully closer - otherwise two tentacles ping-pong the whole raid. Guardians
        // hold outright: that tier is ordered by health, and an order that flips mid-fight would reset
        // every swing and cast timer in the raid.
        if (currentTier < desiredTier || !target || IsYoggSaronImmortalGuardian(currentTarget) ||
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
    auto const cast = [&](char const* spell)
    { return botAI->CanCastSpell(spell, target) && botAI->CastSpell(spell, target); };

    switch (bot->getClass())
    {
        case CLASS_DEATH_KNIGHT:
            return cast("mind freeze") || cast("strangulate");
        case CLASS_HUNTER:
            return cast("silencing shot");
        case CLASS_MAGE:
            return cast("counterspell");
        case CLASS_ROGUE:
            return cast("kick");
        case CLASS_SHAMAN:
            return cast("wind shear");
        case CLASS_WARRIOR:
            return cast("pummel") || cast("shield bash");
        default:
            return bot->getRace() == RACE_BLOODELF && cast("arcane torrent");
    }
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

    // Added because lunatic gaze freeze all bots and they can't attack
    // If someone fix it then this cheat can be removed.
    // With Thorim as a Keeper the raid plays it for real instead: the tank brings the guardian to the
    // melee stack, they cleave it to Weakened, and Titanic Storm executes it. Nothing else can kill a
    // Weakened guardian, so anywhere else the cheat is the only way one ever dies.
    if (!botAI->HasCheat(BotCheatMask::raid) || (IsYoggSaronHardModeActive(botAI) && YoggThorimKeeperActive(botAI)))
        return acted;

    GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");

    Unit* lowestHealthUnit = nullptr;
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if ((unit->GetEntry() != NPC_IMMORTAL_GUARDIAN && unit->GetEntry() != NPC_MARKED_IMMORTAL_GUARDIAN) ||
            unit->GetHealthPct() <= 10)
            continue;

        if (!lowestHealthUnit || unit->GetHealth() < lowestHealthUnit->GetHealth())
            lowestHealthUnit = unit;
    }

    if (!lowestHealthUnit)
        return acted;

    lowestHealthUnit->Kill(bot, lowestHealthUnit);
    return true;
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

    if (botAI->HasCheat(BotCheatMask::raid))
    {
        return bot->TeleportTo(bot->GetMapId(), assignedPortalPosition.GetPositionX(),
                                      assignedPortalPosition.GetPositionY(),
                        assignedPortalPosition.GetPositionZ(), bot->GetOrientation());
    }
    else
    {
        return MoveNear(bot->GetMapId(), assignedPortalPosition.GetPositionX(),
                               assignedPortalPosition.GetPositionY(),
                 assignedPortalPosition.GetPositionZ(), sPlayerbotAIConfig.contactDistance,
                 MovementPriority::MOVEMENT_FORCED);
    }
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

bool YoggSaronBossRoomMovementCheatAction::Execute(Event /*event*/)
{
    FollowMasterStrategy followMasterStrategy(botAI);
    if (botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
        botAI->ChangeStrategy(REMOVE_STRATEGY_CHAR + followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT);

    if (!botAI->HasCheat(BotCheatMask::raid))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    // Land at the bot's own range along the bearing it already had, not on top of the target: that can
    // now be a Crusher Tentacle, and dropping a ranged bot inside its ~10.8 yd reach makes it a Crush
    // candidate with no angle left to dodge.
    float const reach = botAI->IsMelee(bot) ? sPlayerbotAIConfig.meleeDistance : sPlayerbotAIConfig.spellDistance;
    float const bearing = target->GetAngle(bot);

    return bot->TeleportTo(bot->GetMapId(), target->GetPositionX() + reach * cos(bearing),
                           target->GetPositionY() + reach * sin(bearing), target->GetPositionZ(),
                           bot->GetOrientation());
}

bool YoggSaronUsePortalAction::Execute(Event /*event*/)
{
    Creature* assignedPortal = bot->FindNearestCreature(NPC_DESCEND_INTO_MADNESS, 2.0f, true);
    if (!assignedPortal)
        return false;

    FollowMasterStrategy followMasterStrategy(botAI);
    if (botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
        botAI->ChangeStrategy(ADD_STRATEGY_CHAR + followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT);

    return assignedPortal->HandleSpellClick(bot);
}

bool YoggSaronIllusionRoomAction::Execute(Event /*event*/)
{
    YoggSaronTrigger yoggSaronTrigger(botAI);

    bool resultSetRtiMark = SetRtiMark(yoggSaronTrigger);
    bool resultKillIllusionAdd = KillIllusionAdd(yoggSaronTrigger);
    bool resultGoToBrainRoom = GoToBrainRoom(yoggSaronTrigger);

    return resultSetRtiMark || resultKillIllusionAdd || resultGoToBrainRoom;
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

// If proper adds handling in illusion room will be implemented, then this can be removed. Without the
// cheat a bot reaches the room's adds through yogg-saron set dps priority like anything else.
bool YoggSaronIllusionRoomAction::KillIllusionAdd(YoggSaronTrigger yoggSaronTrigger)
{
    if (!botAI->HasCheat(BotCheatMask::raid))
        return false;

    Unit* add = yoggSaronTrigger.GetNextIllusionRoomRtiTarget();
    if (!add)
        return false;

    bot->TeleportTo(bot->GetMapId(), add->GetPositionX(), add->GetPositionY(), add->GetPositionZ(),
                    bot->GetOrientation());

    Unit::DealDamage(bot->GetSession()->GetPlayer(), add, add->GetHealth(), nullptr, DIRECT_DAMAGE,
                     SPELL_SCHOOL_MASK_NORMAL, nullptr, false, true);

    return true;
}

bool YoggSaronIllusionRoomAction::GoToBrainRoom(YoggSaronTrigger yoggSaronTrigger)
{
    if (AI_VALUE(std::string, "rti") == "square" || !yoggSaronTrigger.IsBrainRoomApproachable())
        return false;

    botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("square");

    // The room's middle, not its entrance: a bot parked at the doorway healed from there for 40 s while
    // the Brain sat untouched. The dps priority resolver picks the Brain up once the bot is inside.
    if (botAI->HasCheat(BotCheatMask::raid))
    {
        bot->TeleportTo(bot->GetMapId(), ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE.GetPositionX(),
                        ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE.GetPositionY(),
                        ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE.GetPositionZ(), bot->GetOrientation());
    }
    else
    {
        MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE.GetPositionX(),
               ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE.GetPositionY(),
               ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE.GetPositionZ(), false, false, false, true,
               MovementPriority::MOVEMENT_FORCED, true, false);
    }

    return true;
}

bool YoggSaronMoveToExitPortalAction::Execute(Event /*event*/)
{
    // Three of these are permanently spawned around the brain level, and a window can end with the bot
    // ~120 yd from the nearest one.
    GameObject* portal = bot->FindNearestGameObject(GO_FLEE_TO_THE_SURFACE_PORTAL, 200.0f);
    if (!portal)
        return false;

    if (botAI->HasCheat(BotCheatMask::raid))
        bot->TeleportTo(bot->GetMapId(), portal->GetPositionX(), portal->GetPositionY(), portal->GetPositionZ(),
                               bot->GetOrientation());
    else
        MoveTo(bot->GetMapId(), portal->GetPositionX(), portal->GetPositionY(), portal->GetPositionZ(), false,
                      false, false, true, MovementPriority::MOVEMENT_FORCED,
                      true, false);

    if (bot->GetDistance2d(portal) > 2.0f)
        return false;

    portal->Use(bot);

    botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("skull");
    return true;
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
    if (botAI->IsRanged(bot))
    {
        if (botAI->HasCheat(BotCheatMask::raid))
        {
            return bot->TeleportTo(bot->GetMapId(), ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT.GetPositionX(),
                            ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT.GetPositionY(),
                            ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT.GetPositionZ(),
                            bot->GetOrientation());
        }
        else
        {
            return MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT.GetPositionX(),
                   ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT.GetPositionY(),
                   ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT.GetPositionZ(), false,
                   false, false, true, MovementPriority::MOVEMENT_FORCED, true, false);
        }
    }

    if (botAI->IsMelee(bot) && !botAI->IsTank(bot))
    {
        if (botAI->HasCheat(BotCheatMask::raid))
        {
            return bot->TeleportTo(bot->GetMapId(), ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionX(),
                            ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionY(),
                            ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionZ(), bot->GetOrientation());
        }
        else
        {
            return MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionX(),
                   ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionY(),
                   ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionZ(), false, false, false, true,
                   MovementPriority::MOVEMENT_FORCED, true, false);
        }
    }

    if (botAI->IsTank(bot))
    {
        if (bot->GetDistance(ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT) > 30.0f)
        {
            if (botAI->HasCheat(BotCheatMask::raid))
            {
                return bot->TeleportTo(bot->GetMapId(), ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionX(),
                                       ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionY(),
                                       ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionZ(), bot->GetOrientation());
            }
        }

        return MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionX(),
                      ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionY(),
                      ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionZ(), false, false, false, true,
                      MovementPriority::MOVEMENT_FORCED, true, false);
    }

    return false;
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
