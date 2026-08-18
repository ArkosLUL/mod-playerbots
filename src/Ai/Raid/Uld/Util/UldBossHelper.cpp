/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldBossHelper.h"
#include "AiFactory.h"
#include "GameObject.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "UldHardMode.h"
#include "Vehicle.h"
#include "DynamicObject.h"
#include "Map.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>

const Position ULDUAR_IGNIS_WATER_POOL_WEST = Position(526.771f, 277.796f, 360.802f);
const Position ULDUAR_IGNIS_WATER_POOL_EAST = Position(646.771f, 277.796f, 360.802f);
const Position ULDUAR_THORIM_NEAR_ARENA_CENTER = Position(2134.9854f, -263.11853f, 419.8465f);
const Position ULDUAR_THORIM_NEAR_ENTRANCE_POSITION = Position(2172.4355f, -258.27957f, 418.47162f);
const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_1 = Position(2237.6187f, -265.08844f, 412.17548f);
const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_2 = Position(2237.2498f, -275.81122f, 412.17548f);
const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_5_YARDS_1 = Position(2236.895f, -294.62448f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1 = Position(2242.1162f, -310.15308f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_2 = Position(2242.018f, -318.66003f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_3 = Position(2242.1904f, -329.0533f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1 = Position(2219.5417f, -264.77167f, 412.17548f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2 = Position(2217.446f, -275.85248f, 412.17548f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1 = Position(2217.8877f, -295.01193f, 412.13434f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1 = Position(2212.193f, -307.44992f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2 = Position(2212.1353f, -318.20795f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3 = Position(2212.1956f, -328.0144f, 412.1348f);
const Position ULDUAR_THORIM_JUMP_END_POINT = Position(2137.8818f, -278.18942f, 419.66653f);
const Position ULDUAR_THORIM_PHASE2_TANK_SPOT = Position(2134.8572f, -287.0291f, 419.4935f);
const Position ULDUAR_THORIM_PHASE2_RANGE1_SPOT = Position(2112.8752f, -267.69305f, 419.52814f);
const Position ULDUAR_THORIM_PHASE2_RANGE2_SPOT = Position(2134.1296f, -257.3316f, 419.8462f);
const Position ULDUAR_THORIM_PHASE2_RANGE3_SPOT = Position(2156.798f, -267.57434f, 419.52722f);
const Position ULDUAR_MIMIRON_ROOM_CENTER = Position(2744.65f, 2569.46f, 364.32f);
const Position ULDUAR_MIMIRON_PHASE3_STAGE = Position(2762.65f, 2569.46f, 364.31f);
const Position ULDUAR_MIMIRON_PHASE4_TANK_SPOT = Position(2744.5754f, 2570.8657f, 364.3138f);
const Position ULDUAR_IRON_ASSEMBLY_ANCHOR = Position(1587.18f, 121.02f, 427.27f);
// Vezax' own spawn point, and the point the Saronite Vapors charge to when they merge.
const Position ULDUAR_VEZAX_ANCHOR = Position(1852.78f, 81.3856f, 342.461f);
// Algalon's home position - he evades past 47 yd from it - and the tank slot on the -Y edge of the
// worm hole square. navprobe map 603: both on mesh at 0.04, settled Z 417.321.
const Position ULDUAR_ALGALON_ROOM_CENTER = Position(1632.668f, -302.7656f, 417.3211f);
const Position ULDUAR_ALGALON_TANK_SLOT = Position(1632.7f, -321.5f, 417.321f);
const Position ULDUAR_YOGG_SARON_MIDDLE = Position(1980.28f, -25.5868f, 329.397f);
const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE = Position(1927.1511f, 68.507256f, 242.37657f);
const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE = Position(1925.6553f, -121.59296f, 239.98965f);
const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE = Position(2104.5667f, -25.509348f, 242.64679f);
const Position ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE = Position(1980.1971f, -27.854689f, 236.06789f);
const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_ENTRANCE = Position(1954.06f, 21.66f, 239.71f);
const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_ENTRANCE = Position(1950.11f, -79.284f, 239.98982f);
const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_ENTRANCE = Position(2048.63f, -25.5f, 239.72f);
const Position ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT = Position(1998.5377f, -22.90317f, 324.8895f);
const Position ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT = Position(2018.7628f, -18.896868f, 327.07245f);

// XT-002 anchors. XT spawns at (886.28, -12.05) facing -x, so the tank spot sits just behind him and
// the boss settles roughly between the two lines. The ranged spot is pulled in from the value that
// was measured in-game because a 30yd caster clipped out of range there and walked in every tick.
const Position ULDUAR_XT002_MAINTANK_SPOT = Position(895.82f, -12.53954f, 409.68756f);
const Position ULDUAR_XT002_RANGED_SPOT = Position(866.0f, -12.5f, 409.8f);
// Far enough from both anchors to clear ULDUAR_XT002_DEBUFF_SPREAD_RADIUS without leaving the room.
const Position ULDUAR_XT002_SEARING_LIGHT_SPOT = Position(862.73724f, 12.77857f, 409.8322f);
// Two origins so melee and ranged carriers do not drop Void Zones on top of each other.
const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_MELEE = Position(871.5199f, -54.04216f, 409.80377f);
const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED = Position(837.0746f, -53.01061f, 409.80362f);

// Auriaya's lane. She spawns at (1956.2, 49.32, 411.36) facing (-0.955, 0.296), which points down
// the room and directly away from the corridor at +x, so the fight walks that bearing in 10 yd steps
// as her void zones pile up. Tank spots sit 5/15/25 yd out, the raid points 15 yd behind each. All
// six are inside the floor at x 1909-1956, y 43-82.
const Position ULDUAR_AURIAYA_MAINTANK_SPOTS[ULDUAR_AURIAYA_STATION_COUNT] = {
    Position(1951.42f, 50.79f, 411.36f), Position(1941.87f, 53.75f, 411.36f),
    Position(1932.32f, 56.71f, 411.36f)};
const Position ULDUAR_AURIAYA_NOMINAL_RAID_POINTS[ULDUAR_AURIAYA_STATION_COUNT] = {
    Position(1937.10f, 55.23f, 411.36f), Position(1927.54f, 58.19f, 411.36f),
    Position(1917.99f, 61.15f, 411.36f)};

// Hodir's corner. His room runs x 1965-2041 and y -170 to -298, and he evades the moment he leaves
// that y band. Tanking him in the south-west corner collapses the helper NPCs' 17-30 yd stand-off
// arc into one place, which is what makes the Starlight zone and the Toasty Fires land somewhere the
// raid can predict. That corner is chamfered, not square - the floor bevels away from about
// (1966, -274) to (1990, -298) - so the tank spot is the deepest point with 6 yd of floor all round
// rather than the visual corner, which has three yards of nothing behind it. The off-tank sits
// further into the corner rather than toward the raid, so a taunt never walks him at the stack. The
// raid anchor is only the fallback centre for the ranged ring; normally the ring rides Starlight.
const Position ULDUAR_HODIR_MAINTANK_SPOT = Position(1974.50f, -275.50f, 432.687f);
const Position ULDUAR_HODIR_OFFTANK_SPOT = Position(1980.00f, -277.00f, 432.687f);
const Position ULDUAR_HODIR_RAID_ANCHOR = Position(1986.56f, -257.11f, 432.687f);

// Prevent harpoon spam
std::unordered_map<ObjectGuid, time_t> RazorscaleBossHelper::_harpoonCooldowns;
// Prevent role assignment spam
std::unordered_map<ObjectGuid, std::time_t> RazorscaleBossHelper::_lastRoleSwapTime;
const std::time_t RazorscaleBossHelper::_roleSwapCooldown;

bool RazorscaleBossHelper::UpdateBossAI()
{
    _boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (_boss)
    {
        Group* group = bot->GetGroup();
        if (group && !AreRolesAssigned())
        {
            AssignRolesBasedOnHealth();
        }
        return true;
    }
    return false;
}

Unit* RazorscaleBossHelper::GetBoss() const
{
    return _boss;
}

bool RazorscaleBossHelper::IsGroundPhaseFor(Unit* boss)
{
    return boss && boss->IsAlive() &&
           (boss->GetPositionZ() <= RAZORSCALE_FLYING_Z_THRESHOLD) &&
           (boss->GetHealthPct() < 50.0f) &&
           !boss->HasAura(SPELL_STUN_AURA);
}

bool RazorscaleBossHelper::IsFlyingPhaseFor(Unit* boss)
{
    return boss && (!IsGroundPhaseFor(boss) || boss->GetPositionZ() >= RAZORSCALE_FLYING_Z_THRESHOLD);
}

bool RazorscaleBossHelper::IsGroundPhase() const
{
    return IsGroundPhaseFor(_boss);
}

bool RazorscaleBossHelper::IsFlyingPhase() const
{
    return IsFlyingPhaseFor(_boss);
}

Unit* RazorscaleBossHelper::FindDevouringFlameNear(PlayerbotAI* botAI, float radius)
{
    Player* bot = botAI->GetBot();

    Unit* nearest = nullptr;
    float best = std::numeric_limits<float>::max();

    GuidVector npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest hostile npcs")->Get();
    for (ObjectGuid const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != UNIT_DEVOURING_FLAME)
            continue;

        float const distance = bot->GetDistance2d(unit);
        if (distance > radius)
            continue;

        if (!nearest || distance < best)
        {
            nearest = unit;
            best = distance;
        }
    }

    return nearest;
}

void RazorscaleBossHelper::CollectDevouringFlames(Player* bot, float radius, std::vector<Position>& out)
{
    out.clear();
    if (!bot)
        return;

    // Grid search rather than "nearest hostile npcs": that value ranks by distance to the bot, and the
    // question here is about points he is not standing on yet.
    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, UNIT_DEVOURING_FLAME, radius);

    out.reserve(found.size());
    for (Creature* flame : found)
    {
        if (!flame || !flame->IsAlive())
            continue;

        out.push_back(flame->GetPosition());
    }
}

bool RazorscaleBossHelper::DevouringFlameBlocks(std::vector<Position> const& flames, float x, float y)
{
    for (Position const& flame : flames)
    {
        if (flame.GetExactDist2d(x, y) < DEVOURING_FLAME_CLEAR_RADIUS)
            return true;
    }

    return false;
}

bool RazorscaleBossHelper::DevouringFlameBlocks(Player* bot, float x, float y)
{
    if (!bot)
        return false;

    std::vector<Position> flames;
    CollectDevouringFlames(bot, DEVOURING_FLAME_CLEAR_RADIUS + bot->GetExactDist2d(x, y), flames);

    return DevouringFlameBlocks(flames, x, y);
}

bool RazorscaleBossHelper::IsHarpoonReady(GameObject* harpoonGO)
{
    if (!harpoonGO)
        return false;

    // A spent harpoon keeps standing there, flagged unselectable, until the controller rebuilds it.
    if (harpoonGO->HasGameObjectFlag(GO_FLAG_NOT_SELECTABLE))
        return false;

    auto it = _harpoonCooldowns.find(harpoonGO->GetGUID());
    if (it != _harpoonCooldowns.end())
    {
        time_t currentTime = std::time(nullptr);
        time_t elapsedTime = currentTime - it->second;
        if (elapsedTime < HARPOON_COOLDOWN_DURATION)
            return false;
    }

    return harpoonGO->GetGoState() == GO_STATE_READY;
}

void RazorscaleBossHelper::SetHarpoonOnCooldown(GameObject* harpoonGO)
{
    if (!harpoonGO)
        return;

    time_t currentTime = std::time(nullptr);
    _harpoonCooldowns[harpoonGO->GetGUID()] = currentTime;
}

GameObject* RazorscaleBossHelper::FindNearestHarpoon(float x, float y, float z) const
{
    GameObject* nearestHarpoon = nullptr;
    float minDistanceSq = std::numeric_limits<float>::max();

    for (auto const& harpoon : GetHarpoonData())
    {
        if (GameObject* harpoonGO = bot->FindNearestGameObject(harpoon.gameObjectEntry, 200.0f))
        {
            float dx = harpoonGO->GetPositionX() - x;
            float dy = harpoonGO->GetPositionY() - y;
            float dz = harpoonGO->GetPositionZ() - z;
            float distanceSq = dx * dx + dy * dy + dz * dz;

            if (distanceSq < minDistanceSq)
            {
                minDistanceSq = distanceSq;
                nearestHarpoon = harpoonGO;
            }
        }
    }

    return nearestHarpoon;
}

const std::vector<RazorscaleBossHelper::HarpoonData>& RazorscaleBossHelper::GetHarpoonData()
{
    // Only two of these exist in 10-man; the missing entries simply never resolve to a GameObject.
    static const std::vector<HarpoonData> harpoonData =
    {
        { GO_RAZORSCALE_HARPOON_1 },
        { GO_RAZORSCALE_HARPOON_2 },
        { GO_RAZORSCALE_HARPOON_3 },
        { GO_RAZORSCALE_HARPOON_4 },
    };
    return harpoonData;
}

bool RazorscaleBossHelper::AreRolesAssigned() const
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Retrieve the group member slot list (GUID + flags + other info)
    Group::MemberSlotList const& slots = group->GetMemberSlots();
    for (auto const& slot : slots)
    {
        // Check if this member has the MAINTANK flag
        if (slot.flags & MEMBER_FLAG_MAINTANK)
        {
            return true;
        }
    }

    return false;
}

bool RazorscaleBossHelper::CanSwapRoles() const
{
    // Identify the GUID of the current bot
    ObjectGuid botGuid = bot->GetGUID();
    if (!botGuid)
        return false;

    // If no entry exists yet for this bot, initialize it to 0
    auto it = _lastRoleSwapTime.find(botGuid);
    if (it == _lastRoleSwapTime.end())
    {
        _lastRoleSwapTime[botGuid] = 0;
        it = _lastRoleSwapTime.find(botGuid);
    }

    // Compare the current time against the stored time
    std::time_t currentTime = std::time(nullptr);
    std::time_t lastSwapTime = it->second;

    return (currentTime - lastSwapTime) >= _roleSwapCooldown;
}

void RazorscaleBossHelper::AssignRolesBasedOnHealth()
{
    // Check if enough time has passed since last swap
    if (!CanSwapRoles())
        return;

    Group* group = bot->GetGroup();
    if (!group)
        return;

    // Gather all tank-capable players (bots + real players), excluding those with too many Fuse Armor stacks
    std::vector<Player*> tankCandidates;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !botAI->IsTank(member, true) || !member->IsAlive())
            continue;

        Aura* fuseArmor = member->GetAura(SPELL_FUSE_ARMOR);
        if (fuseArmor && fuseArmor->GetStackAmount() >= FUSEARMOR_THRESHOLD)
            continue;

        tankCandidates.push_back(member);
    }

    // If there are no viable tanks, do nothing
    if (tankCandidates.empty())
        return;

    // Sort by highest max health first
    std::sort(tankCandidates.begin(), tankCandidates.end(),
        [](Player* a, Player* b)
        {
            return a->GetMaxHealth() > b->GetMaxHealth();
        }
    );

    // Pick the top candidate
    Player* newMainTank = tankCandidates[0];
    if (!newMainTank) // Safety check
        return;

    // Unflag everyone from main tank
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && botAI->IsMainTank(member))
            group->SetGroupMemberFlag(member->GetGUID(), false, MEMBER_FLAG_MAINTANK);
    }

    // Assign the single main tank
    group->SetGroupMemberFlag(newMainTank->GetGUID(), true, MEMBER_FLAG_MAINTANK);

    // Yell a message regardless of whether the new main tank is a bot or a real player
    const std::string playerName = newMainTank->GetName();
    const std::string text = playerName + " set as main tank!";
    bot->Yell(text, LANG_UNIVERSAL);

    ObjectGuid botGuid = bot->GetGUID();
    if (!botGuid)
        return;

    // Set current time in the cooldown map for this bot to start cooldown
    _lastRoleSwapTime[botGuid] = std::time(nullptr);
}

bool YoggSaronInPhase2(PlayerbotAI* botAI)
{
    Creature* yogg = botAI->GetBot()->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true);

    return yogg && yogg->IsAlive() && yogg->HasAura(SPELL_SHADOW_BARRIER);
}

bool YoggSaronInPhase3(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Creature* yogg = bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true);
    Creature* guardian = bot->FindNearestCreature(NPC_GUARDIAN_OF_YS, 200.0f, true);

    return yogg && yogg->IsAlive() && !yogg->HasAura(SPELL_SHADOW_BARRIER) && !guardian;
}

// Auriaya
//
// Terrifying Screech repeats on a fixed 35s cycle from the pull, so the whole encounter is one long
// fear window - there is no narrower slice worth reserving Tremor Totem for.
bool AuriayaFearWindowActive(PlayerbotAI* botAI) { return AuriayaEncounterActive(botAI); }

Unit* GetAuriaya(PlayerbotAI* botAI) { return GetFirstAliveUnitByEntry(botAI, NPC_AURIAYA); }

bool AuriayaEncounterActive(PlayerbotAI* botAI) { return GetAuriaya(botAI) != nullptr; }

Unit* GetAuriayaFocusTarget(PlayerbotAI* botAI)
{
    if (Unit* sentry = GetFirstAliveUnitByEntry(botAI, NPC_AURIAYA_SANCTUM_SENTRY))
        return sentry;

    // Between lives the Defender lies feigned at 1 HP and unselectable, so IsAlive() alone would
    // keep the raid pointed at something it cannot hit.
    Unit* defender = GetFirstAliveUnitByEntry(botAI, NPC_AURIAYA_FERAL_DEFENDER);

    return IsDownOrFeigning(defender) ? nullptr : defender;
}

Unit* GetAuriayaLooseSentry(PlayerbotAI* botAI, Player* tank)
{
    auto const& units = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    for (auto const& guid : units)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_AURIAYA_SANCTUM_SENTRY)
            continue;

        if (unit->GetVictim() != tank)
            return unit;
    }

    return nullptr;
}

std::vector<Unit*> CollectAuriayaEssencePools(WorldObject* from, float radius)
{
    std::vector<Unit*> pools;
    if (!from)
        return pools;

    std::list<Creature*> found;
    from->GetCreatureListWithEntryInGrid(found, NPC_AURIAYA_SEEPING_FERAL_ESSENCE, radius);

    for (Creature* creature : found)
        if (creature && creature->IsAlive())
            pools.push_back(creature);

    return pools;
}

// A station is dead once a pool sits inside ULDUAR_AURIAYA_STATION_CLEAR_RADIUS of either of its two
// spots. In practice the raid point is what retires it, since the Defender dies wherever it aggroed.
static bool AuriayaStationClear(std::vector<Unit*> const& pools, int index)
{
    for (Unit* pool : pools)
    {
        if (pool->GetExactDist2d(&ULDUAR_AURIAYA_MAINTANK_SPOTS[index]) < ULDUAR_AURIAYA_STATION_CLEAR_RADIUS ||
            pool->GetExactDist2d(&ULDUAR_AURIAYA_NOMINAL_RAID_POINTS[index]) < ULDUAR_AURIAYA_STATION_CLEAR_RADIUS)
        {
            return false;
        }
    }

    return true;
}

int GetAuriayaStationIndex(PlayerbotAI* botAI)
{
    Unit* boss = GetAuriaya(botAI);
    if (!boss)
        return 0;

    std::vector<Unit*> const pools = CollectAuriayaEssencePools(boss, ULDUAR_AURIAYA_ROOM_SEARCH_RADIUS);

    for (int i = 0; i < ULDUAR_AURIAYA_STATION_COUNT; ++i)
        if (AuriayaStationClear(pools, i))
            return i;

    // Every station is polluted, so take the least bad one. Pools never expire, so this only changes
    // when a new one drops and cannot flip back and forth between ticks.
    int best = ULDUAR_AURIAYA_STATION_COUNT - 1;
    float bestClearance = -1.0f;

    for (int i = 0; i < ULDUAR_AURIAYA_STATION_COUNT; ++i)
    {
        float clearance = std::numeric_limits<float>::max();
        for (Unit* pool : pools)
            clearance = std::min(clearance, pool->GetExactDist2d(&ULDUAR_AURIAYA_NOMINAL_RAID_POINTS[i]));

        if (clearance > bestClearance)
        {
            bestClearance = clearance;
            best = i;
        }
    }

    return best;
}

bool GetAuriayaAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance)
{
    Unit* boss = GetAuriaya(botAI);
    if (!boss)
        return false;

    if (botAI->IsMainTank(bot))
    {
        out = ULDUAR_AURIAYA_MAINTANK_SPOTS[GetAuriayaStationIndex(botAI)];
        tolerance = ULDUAR_AURIAYA_MAINTANK_SPOT_TOLERANCE;
        return true;
    }

    if (!botAI->IsRanged(bot))
        return false;

    // The stack has to sit inside the cone, and the cone points at whoever she is chasing. Reading
    // her victim rather than a fixed bearing is what keeps this correct when a human holds her.
    Unit* victim = boss->GetVictim();
    float bearing = victim && victim != boss
                        ? boss->GetAngle(victim)
                        : boss->GetOrientation();

    // Round the bearing off so tank drift cannot shuffle the whole raid every tick.
    bearing = std::round(bearing / ULDUAR_AURIAYA_BEARING_QUANTUM) * ULDUAR_AURIAYA_BEARING_QUANTUM;

    out = Position(boss->GetPositionX() + std::cos(bearing) * ULDUAR_AURIAYA_RAID_STANDOFF,
                   boss->GetPositionY() + std::sin(bearing) * ULDUAR_AURIAYA_RAID_STANDOFF,
                   boss->GetPositionZ());
    tolerance = botAI->IsRangedDps(bot) ? ULDUAR_AURIAYA_RANGED_SPOT_TOLERANCE
                                        : ULDUAR_AURIAYA_HEALER_SPOT_TOLERANCE;
    return true;
}

Unit* GetHodir(PlayerbotAI* botAI) { return GetFirstAliveUnitByEntry(botAI, NPC_HODIR); }

// Which druid this instance got, so the four-entry sweep runs once rather than on every bot every
// tick. Guid, not a pointer, so a despawn drops out instead of dangling.
static thread_local std::unordered_map<uint32, ObjectGuid> _hodirDruids;

Creature* GetHodirDruidHelper(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return nullptr;

    uint32 const instanceId = bot->GetInstanceId();
    auto const cached = _hodirDruids.find(instanceId);
    if (cached != _hodirDruids.end())
    {
        Creature* druid = ObjectAccessor::GetCreature(*bot, cached->second);
        if (druid && druid->IsAlive())
            return druid;

        _hodirDruids.erase(instanceId);
    }

    // Faction and raid size decide which one spawned, and only one of the four is ever present.
    static uint32 const druids[] = {NPC_HODIR_DRUID_ALLIANCE_10, NPC_HODIR_DRUID_ALLIANCE_25,
                                    NPC_HODIR_DRUID_HORDE_10, NPC_HODIR_DRUID_HORDE_25};

    for (uint32 entry : druids)
    {
        Creature* druid = bot->FindNearestCreature(entry, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);
        if (druid && druid->IsAlive())
        {
            _hodirDruids[instanceId] = druid->GetGUID();
            return druid;
        }
    }

    return nullptr;
}

Creature* GetHodirSharedShelter(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || !GetHodir(botAI))
        return nullptr;

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_SNOWPACKED_ICICLE, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);

    // Nearest the raid anchor rather than nearest the bot, so the whole raid converges on one drift
    // and re-forms cleanly instead of splitting across the two or three that spawn. Trigger and
    // action both call this: two derivations of "which shelter" would disagree and oscillate.
    Creature* best = nullptr;
    float bestDist = 0.0f;
    for (Creature* shelter : found)
    {
        if (!shelter || !shelter->IsAlive())
            continue;

        float const dist = shelter->GetExactDist2d(&ULDUAR_HODIR_RAID_ANCHOR);
        if (!best || dist < bestDist)
        {
            best = shelter;
            bestDist = dist;
        }
    }

    return best;
}

// Latched per instance: up to four Starlight zones overlap and GetDynObject returns only the
// first, so re-picking every tick would swap the ring centre out from under the raid.
static thread_local std::unordered_map<uint32, Position> _hodirRingCentres;

Position GetHodirRingCentre(PlayerbotAI* botAI, Player* bot)
{
    if (!bot)
        return ULDUAR_HODIR_RAID_ANCHOR;

    uint32 const instanceId = bot->GetInstanceId();
    auto const latched = _hodirRingCentres.find(instanceId);

    // Keep the latched zone while this bot is still standing in Starlight. The aura going missing is
    // the exact moment the zone expired or the raid drifted off it, and it costs nothing to read -
    // far cheaper than tracking durations across the several zones alive at any time.
    if (latched != _hodirRingCentres.end() && bot->HasAura(SPELL_HODIR_STARLIGHT))
        return latched->second;

    Creature* druid = GetHodirDruidHelper(botAI);
    if (!druid)
    {
        _hodirRingCentres.erase(instanceId);
        return ULDUAR_HODIR_RAID_ANCHOR;
    }

    // Starlight is a persistent area aura, so there is no creature to find - the dynamic object the
    // druid owns is the only handle on where it actually landed.
    DynamicObject* zone = druid->GetDynObject(SPELL_HODIR_STARLIGHT);
    if (!zone || zone->GetExactDist2d(&ULDUAR_HODIR_RAID_ANCHOR) > ULDUAR_HODIR_ZONE_ADOPT_RADIUS)
    {
        _hodirRingCentres.erase(instanceId);
        return ULDUAR_HODIR_RAID_ANCHOR;
    }

    Position const centre = zone->GetPosition();
    _hodirRingCentres[instanceId] = centre;
    return centre;
}

bool GetHodirRingSlot(PlayerbotAI* botAI, Player* bot, Position const& centre, Position& out)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    std::vector<Player*> ringMembers;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || !memberAI->IsRanged(member) || memberAI->IsTank(member))
            continue;

        ringMembers.push_back(member);
    }

    if (ringMembers.empty())
        return false;

    // Guid order is identical on every bot, so nobody has to be told which slot is theirs.
    std::sort(ringMembers.begin(), ringMembers.end(),
              [](Player* left, Player* right) { return left->GetGUID() < right->GetGUID(); });

    size_t slot = ringMembers.size();
    for (size_t i = 0; i < ringMembers.size(); ++i)
        if (ringMembers[i] == bot)
            slot = i;

    if (slot >= ringMembers.size())
        return false;

    // Anchoring slot 0 on the bearing to the tank keeps the ring from rotating as the centre moves
    // between Starlight zones, so a re-latch does not shuffle everyone.
    float const anchorAngle = std::atan2(ULDUAR_HODIR_MAINTANK_SPOT.GetPositionY() - centre.GetPositionY(),
                                         ULDUAR_HODIR_MAINTANK_SPOT.GetPositionX() - centre.GetPositionX());
    float const step = 2.0f * static_cast<float>(M_PI) / static_cast<float>(ringMembers.size());
    float const angle = Position::NormalizeOrientation(anchorAngle + step * slot);

    float x = centre.GetPositionX() + std::cos(angle) * ULDUAR_HODIR_RAID_RING_RADIUS;
    float y = centre.GetPositionY() + std::sin(angle) * ULDUAR_HODIR_RAID_RING_RADIUS;

    // Raw ring geometry is exactly the shape that lands off the navmesh, and MoveTo would then fail
    // without telling anyone.
    float z = bot->GetMapWaterOrGroundLevel(x, y, centre.GetPositionZ());
    if (z <= INVALID_HEIGHT)
        z = centre.GetPositionZ();

    bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                   bot->GetPositionZ(), x, y, z, false);

    out = Position(x, y, z);
    return true;
}

bool GetHodirAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance)
{
    if (!GetHodir(botAI))
        return false;

    if (botAI->IsMainTank(bot))
    {
        out = ULDUAR_HODIR_MAINTANK_SPOT;
        tolerance = ULDUAR_HODIR_MAINTANK_SPOT_TOLERANCE;
        return true;
    }

    if (botAI->IsAssistTankOfIndex(bot, 0, true))
    {
        out = ULDUAR_HODIR_OFFTANK_SPOT;
        tolerance = ULDUAR_HODIR_MAINTANK_SPOT_TOLERANCE;
        return true;
    }

    // Melee ride the boss in the corner. Pinning them would cost uptime, and they are far outside
    // both buff zones there whatever we do.
    if (!botAI->IsRanged(bot))
        return false;

    if (!GetHodirRingSlot(botAI, bot, GetHodirRingCentre(botAI, bot), out))
        return false;

    tolerance = ULDUAR_HODIR_RING_SPOT_TOLERANCE;
    return true;
}

bool IsHodirTrappedAllyBreaker(PlayerbotAI* botAI, Player* bot, Unit* block)
{
    if (!block)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    // Healers keep healing: the raid is taking 14000 every two seconds from icicles while this block
    // is up, and it dies to a handful of hits anyway.
    std::vector<Player*> candidates;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || memberAI->IsHeal(member) || memberAI->IsTank(member))
            continue;

        if (member->GetExactDist2d(block) > ULDUAR_HODIR_TRAPPED_ALLY_RANGE)
            continue;

        candidates.push_back(member);
    }

    std::sort(candidates.begin(), candidates.end(), [block](Player* left, Player* right)
    {
        float const leftDist = left->GetExactDist2d(block);
        float const rightDist = right->GetExactDist2d(block);
        if (leftDist != rightDist)
            return leftDist < rightDist;
        return left->GetGUID() < right->GetGUID();
    });

    for (size_t i = 0; i < candidates.size() && i < ULDUAR_HODIR_TRAPPED_ALLY_BREAKERS; ++i)
        if (candidates[i] == bot)
            return true;

    return false;
}

bool UldCastClassTaunt(PlayerbotAI* botAI, Unit* target)
{
    if (!target)
        return false;

    switch (botAI->GetBot()->getClass())
    {
        case CLASS_WARRIOR:
            return botAI->CastSpell("taunt", target);
        case CLASS_PALADIN:
            return botAI->CastSpell("hand of reckoning", target);
        case CLASS_DEATH_KNIGHT:
            return botAI->CastSpell("dark command", target);
        case CLASS_DRUID:
            return botAI->CastSpell("growl", target);
        default:
            return false;
    }
}

bool YoggSaronFearWindowActive(PlayerbotAI* botAI) { return YoggSaronInPhase2(botAI) || YoggSaronInPhase3(botAI); }

// XT-002 Deconstructor
//
// XT and his Heart both spend part of the fight carrying UNIT_FLAG_NOT_SELECTABLE, which drops them
// out of "possible targets" entirely (AttackersValue::IsPossibleTarget rejects the flag). Scanning
// the raw nearby-npc list instead keeps the encounter visible right through the Heart phases.
static Unit* GetFirstAliveNpcByEntry(PlayerbotAI* botAI, uint32 entry)
{
    auto const& npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get();
    for (auto const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && unit->GetEntry() == entry)
            return unit;
    }

    return nullptr;
}

// Freya

std::vector<Unit*> FreyaWaveState::LivingTrio() const
{
    std::vector<Unit*> living;
    for (Unit* member : {snaplasher, stormLasher, waterSpirit})
    {
        if (member && member->IsAlive())
            living.push_back(member);
    }

    return living;
}

bool FreyaWaveState::TrioLocked() const
{
    for (Unit* member : LivingTrio())
    {
        if (member->GetHealthPct() < ULDUAR_FREYA_TRIO_SYNC_WINDOW_PCT)
            return true;
    }

    return false;
}

bool FreyaWaveState::TrioReleased() const
{
    std::vector<Unit*> const living = LivingTrio();
    if (living.empty())
        return false;

    for (Unit* member : living)
    {
        if (member->GetHealthPct() > ULDUAR_FREYA_TRIO_FLOOR_RELEASE_PCT)
            return false;
    }

    return true;
}

void GatherFreyaWaveState(PlayerbotAI* botAI, FreyaWaveState& state)
{
    // Waves spawn on a fixed 60s timer whether or not the last one died, so two sets of the same add
    // can be up at once. Keeping the more damaged one finishes the older wave first, and - because
    // the scan order is not stable - it is also what stops the split target flipping between two
    // identical adds from tick to tick.
    auto const keepMoreDamaged = [](Unit*& slot, Unit* candidate)
    {
        if (!slot || candidate->GetHealth() < slot->GetHealth())
            slot = candidate;
    };

    // "possible targets" enforces line of sight, which drops adds behind Freya's tree trunks out of
    // the scan and makes the split disagree between bots standing on opposite sides.
    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        switch (unit->GetEntry())
        {
            case NPC_EONARS_GIFT:
                keepMoreDamaged(state.eonarsGift, unit);
                break;
            case NPC_ANCIENT_CONSERVATOR:
                keepMoreDamaged(state.conservator, unit);
                break;
            case NPC_SNAPLASHER:
                keepMoreDamaged(state.snaplasher, unit);
                break;
            case NPC_STORM_LASHER:
                keepMoreDamaged(state.stormLasher, unit);
                break;
            case NPC_ANCIENT_WATER_SPIRIT:
                keepMoreDamaged(state.waterSpirit, unit);
                break;
            case NPC_DETONATING_LASHER:
                state.detonatingLashers.push_back(unit);
                break;
            default:
                break;
        }
    }
}

bool FreyaTrioSyncSuppress(FreyaWaveState const& state, Unit* target)
{
    if (!target)
        return false;

    if (target != state.snaplasher && target != state.stormLasher && target != state.waterSpirit)
        return false;

    std::vector<Unit*> const living = state.LivingTrio();

    // Below three the window is already open and counting down; holding damage back now only lets the
    // ones already dead come back.
    if (living.size() < 3)
        return false;

    if (state.TrioReleased())
        return false;

    if (target->GetHealthPct() > ULDUAR_FREYA_TRIO_HARD_FLOOR_PCT)
        return false;

    for (Unit* member : living)
    {
        if (member != target && member->GetHealthPct() > ULDUAR_FREYA_TRIO_FLOOR_RELEASE_PCT)
            return true;
    }

    return false;
}

Unit* GetFreyaTrioAssignment(PlayerbotAI* botAI, FreyaWaveState const& state)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    std::vector<Unit*> candidates;
    for (Unit* member : state.LivingTrio())
    {
        if (!FreyaTrioSyncSuppress(state, member))
            candidates.push_back(member);
    }

    // Everything floored at once should be impossible - the release check clears the floor as soon as
    // the last member joins the band - but a bot with nothing to hit would fall through to the boss.
    if (candidates.empty())
        candidates = state.LivingTrio();

    if (candidates.empty())
        return nullptr;

    std::vector<uint32> assigned(candidates.size(), 0);
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !PlayerbotAI::IsDps(member))
            continue;

        // Whichever member would carry the most remaining health per attacker if this bot joined it.
        // Over the whole group that lands a split proportional to remaining health, which is the
        // quantity that has to reach zero at the same time.
        size_t pick = 0;
        float best = -1.0f;
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            float const share = float(candidates[i]->GetHealth()) / float(assigned[i] + 1);
            if (share > best)
            {
                best = share;
                pick = i;
            }
        }

        ++assigned[pick];

        if (member == bot)
            return candidates[pick];
    }

    return nullptr;
}

Unit* GetFreyaRangedLasherFocus(FreyaWaveState const& state)
{
    Unit* best = nullptr;
    for (Unit* lasher : state.detonatingLashers)
    {
        if (!lasher || !lasher->IsAlive())
            continue;

        // GUID breaks the tie so a wave of untouched lashers does not resolve differently per bot.
        if (!best || lasher->GetHealth() < best->GetHealth() ||
            (lasher->GetHealth() == best->GetHealth() && lasher->GetGUID() < best->GetGUID()))
        {
            best = lasher;
        }
    }

    return best;
}

Unit* GetFreyaLocalLasherTarget(PlayerbotAI* botAI, FreyaWaveState const& state, Unit* currentTarget, float range)
{
    Player* bot = botAI->GetBot();

    Unit* selected = nullptr;
    if (currentTarget && currentTarget->IsAlive() && currentTarget->GetEntry() == NPC_DETONATING_LASHER &&
        bot->GetExactDist2d(currentTarget) <= range)
    {
        selected = currentTarget;
    }

    // The margin stops two lashers at similar range from trading the bot back and forth every tick.
    constexpr float switchMargin = 10.0f;
    for (Unit* candidate : state.detonatingLashers)
    {
        if (!candidate || !candidate->IsAlive() || candidate == selected)
            continue;

        if (bot->GetExactDist2d(candidate) > range)
            continue;

        if (!selected)
        {
            selected = candidate;
            continue;
        }

        if (candidate->GetExactDist2d(bot) + switchMargin < selected->GetExactDist2d(bot))
            selected = candidate;
    }

    return selected;
}

Unit* GetFreyaConservatorSpore(PlayerbotAI* botAI, Unit* conservator)
{
    if (!conservator || !conservator->IsAlive())
        return nullptr;

    std::list<Creature*> found;
    conservator->GetCreatureListWithEntryInGrid(found, NPC_HEALTHY_SPORE, ULDUAR_FREYA_SPORE_SEARCH_RADIUS);

    // Nearest the Conservator, not the caller: the tank drags the boss to this spore and the melee
    // shelter on it, and two derivations of "which spore" would disagree and oscillate. It is also
    // self-stabilising - once parked, the spore is at distance ~0 and stays nearest until it despawns,
    // while every new one spawns 20 yd out.
    Creature* best = nullptr;
    float bestDist = 0.0f;
    for (Creature* spore : found)
    {
        if (!spore || !spore->IsAlive())
            continue;

        float const dist = spore->GetExactDist2d(conservator);
        if (!best || dist < bestDist)
        {
            best = spore;
            bestDist = dist;
        }
    }

    return best;
}

// Highest health first, and never a suppressed member - see the header for why the tank goes to the
// member furthest from the floor rather than the nearest one.
//
// Percent, not absolute: the floor and release thresholds are percentages, and the three members differ
// by nearly 2x in max health, so absolute health would call the Snaplasher the furthest from dying even
// when it is the closest.
static Unit* GetFreyaTankTrioTarget(FreyaWaveState const& state, Unit* currentTarget)
{
    Unit* best = nullptr;
    for (Unit* member : state.LivingTrio())
    {
        if (FreyaTrioSyncSuppress(state, member))
            continue;

        if (!best || member->GetHealthPct() > best->GetHealthPct())
            best = member;
    }

    if (!best || !currentTarget || currentTarget == best)
        return best;

    // Hold the member the tank is already on until another is clear of it by the margin, or its own
    // damage closing the gap makes it swap every few ticks.
    for (Unit* member : state.LivingTrio())
    {
        if (member != currentTarget || FreyaTrioSyncSuppress(state, member))
            continue;

        if (best->GetHealthPct() - member->GetHealthPct() < ULDUAR_FREYA_TANK_TRIO_SWITCH_PCT)
            return member;

        break;
    }

    return best;
}

Unit* GetFreyaTankTarget(PlayerbotAI* botAI, FreyaWaveState const& state, Unit* currentTarget)
{
    Player* bot = botAI->GetBot();
    Unit* freya = GetFirstAliveUnitByEntry(botAI, NPC_FREYA);

    if (PlayerbotAI::IsMainTank(bot))
        return freya;

    if (!PlayerbotAI::IsAssistTankOfIndex(bot, 0, true))
        return nullptr;

    if (state.snaplasher && state.snaplasher->IsAlive())
        return state.snaplasher;

    if (state.conservator && state.conservator->IsAlive())
        return state.conservator;

    if (Unit* member = GetFreyaTankTrioTarget(state, currentTarget))
        return member;

    // Same local rule and leash as a melee DPS bot: hit the one standing next to it, never walk one
    // anywhere. Collecting lashers and towing them into the raid is what killed the raid before.
    if (Unit* lasher = GetFreyaLocalLasherTarget(botAI, state, currentTarget, ULDUAR_FREYA_MELEE_LASHER_RANGE))
        return lasher;

    return freya;
}

bool FreyaHasLivingRangedDps(PlayerbotAI* botAI)
{
    Group* group = botAI->GetBot()->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && PlayerbotAI::IsRangedDps(member))
            return true;
    }

    return false;
}

std::vector<Position> GetFreyaNatureBombPositions(Player* bot, float searchRadius)
{
    std::list<GameObject*> bombs;
    bot->GetGameObjectListWithEntryInGrid(bombs, GOBJECT_NATURE_BOMB, searchRadius);

    std::vector<Position> positions;
    positions.reserve(bombs.size());
    for (GameObject* bomb : bombs)
    {
        if (bomb)
            positions.push_back(bomb->GetPosition());
    }

    return positions;
}

// Lowest health first, so two Sentinels up do not split the raid's damage and the skull does not flip
// between them as the marking bot moves. Entry order alone is resolved per bot and is not stable.
static Unit* GetLowestHealthUnitByEntry(PlayerbotAI* botAI, uint32 entry)
{
    Unit* best = nullptr;
    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != entry)
            continue;

        if (!best || unit->GetHealth() < best->GetHealth())
            best = unit;
    }

    return best;
}

Unit* GetRazorscaleAddKillTarget(PlayerbotAI* botAI)
{
    if (Unit* sentinel = GetLowestHealthUnitByEntry(botAI, RazorscaleBossHelper::UNIT_DARK_RUNE_SENTINEL))
        return sentinel;

    if (Unit* watcher = GetFirstAliveUnitByEntry(botAI, RazorscaleBossHelper::UNIT_DARK_RUNE_WATCHER))
        return watcher;

    return GetFirstAliveUnitByEntry(botAI, RazorscaleBossHelper::UNIT_DARK_RUNE_GUARDIAN);
}

Unit* GetRazorscaleKillTarget(PlayerbotAI* botAI)
{
    Unit* boss = botAI->GetAiObjectContext()->GetValue<Unit*>("find target", "razorscale")->Get();
    if (!boss || !boss->IsAlive())
        return nullptr;

    if (boss->GetPositionZ() <= RazorscaleBossHelper::RAZORSCALE_FLYING_Z_THRESHOLD)
        return boss;

    return GetRazorscaleAddKillTarget(botAI);
}

Unit* GetXT002(PlayerbotAI* botAI) { return GetFirstAliveNpcByEntry(botAI, NPC_XT002); }

Unit* GetXT002ExposedHeart(PlayerbotAI* botAI)
{
    Unit* heart = GetFirstAliveNpcByEntry(botAI, NPC_HEART_OF_DECONSTRUCTOR);
    if (!heart || heart->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return nullptr;

    // The Heart is only worth hitting while it channels Exposed Heart - that aura is what transfers
    // its damage taken to XT.
    return heart->HasAura(SPELL_XT002_EXPOSED_HEART) ? heart : nullptr;
}

bool IsXT002Submerged(PlayerbotAI* botAI)
{
    Unit* xt002 = GetXT002(botAI);
    if (!xt002)
        return false;

    return xt002->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) || xt002->HasAura(SPELL_XT002_SUBMERGE);
}

uint32 GetXT002SearingLightSpellId(Player* bot)
{
    return bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? SPELL_XT002_SEARING_LIGHT_25
                                                                   : SPELL_XT002_SEARING_LIGHT_10;
}

uint32 GetXT002GravityBombSpellId(Player* bot)
{
    return bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? SPELL_XT002_GRAVITY_BOMB_25
                                                                   : SPELL_XT002_GRAVITY_BOMB_10;
}

// Ignis the Furnace Master

// Construct each assist tank has committed to, so one Ignis activates nearer to him mid-walk cannot
// steal the kite. Cleared once that construct turns Brittle or dies.
static std::unordered_map<ObjectGuid, ObjectGuid> ignisTankDrivenConstructGuid;

Unit* GetIgnis(PlayerbotAI* botAI)
{
    return botAI->GetBot()->FindNearestCreature(NPC_IGNIS, ULDUAR_IGNIS_ROOM_SEARCH_RADIUS, true);
}

bool IsIgnisConstructActivated(Unit const* construct)
{
    if (!construct || !construct->IsAlive() || construct->GetEntry() != NPC_IGNIS_IRON_CONSTRUCT)
        return false;

    return !construct->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) &&
           !construct->HasAura(SPELL_IGNIS_CONSTRUCT_INACTIVE);
}

bool IsIgnisConstructMolten(Unit const* construct)
{
    return construct && construct->HasAura(SPELL_IGNIS_MOLTEN);
}

bool IsIgnisConstructBrittle(Unit const* construct)
{
    return construct && (construct->HasAura(SPELL_IGNIS_BRITTLE_10) || construct->HasAura(SPELL_IGNIS_BRITTLE_25));
}

// Constructs are dormant and unselectable until Ignis activates them, and the walk to the water
// takes the tank past SightDistance from the pack, so every lookup below searches the grid instead
// of the bot's cached, LOS-filtered "nearest npcs" list.
static Unit* GetNearestIgnisConstructMatching(WorldObject const* from, bool (*predicate)(Unit const*))
{
    if (!from)
        return nullptr;

    std::list<Creature*> constructs;
    from->GetCreatureListWithEntryInGrid(constructs, NPC_IGNIS_IRON_CONSTRUCT, ULDUAR_IGNIS_ROOM_SEARCH_RADIUS);

    Unit* best = nullptr;
    float bestDistance = ULDUAR_IGNIS_ROOM_SEARCH_RADIUS;

    for (Creature* construct : constructs)
    {
        if (!IsIgnisConstructActivated(construct) || !predicate(construct))
            continue;

        float const distance = from->GetExactDist2d(construct);
        if (distance > bestDistance)
            continue;

        best = construct;
        bestDistance = distance;
    }

    return best;
}

Unit* GetIgnisBrittleConstruct(PlayerbotAI* botAI)
{
    return GetNearestIgnisConstructMatching(botAI->GetBot(), &IsIgnisConstructBrittle);
}

Unit* GetIgnisNearestMoltenConstruct(PlayerbotAI* /*botAI*/, WorldObject const* from)
{
    return GetNearestIgnisConstructMatching(from, &IsIgnisConstructMolten);
}

Unit* GetIgnisDrivenConstruct(PlayerbotAI* botAI, Player* tank)
{
    if (!tank)
        return nullptr;

    ObjectGuid const tankGuid = tank->GetGUID();
    auto const held = ignisTankDrivenConstructGuid.find(tankGuid);
    if (held != ignisTankDrivenConstructGuid.end())
    {
        Unit* construct = botAI->GetUnit(held->second);

        // Molten wiped this construct's threat table, so handing it to a closer new one would release
        // it into the raid. Only Brittle (job done) or death lets the tank move on.
        if (IsIgnisConstructActivated(construct) && !IsIgnisConstructBrittle(construct))
            return construct;

        ignisTankDrivenConstructGuid.erase(held);
    }

    Unit* construct = GetNearestIgnisConstructMatching(
        tank, [](Unit const* candidate) { return !IsIgnisConstructBrittle(candidate); });
    if (!construct)
        return nullptr;

    ignisTankDrivenConstructGuid[tankGuid] = construct->GetGUID();

    return construct;
}

Unit* GetIgnisNearestScorchedGround(PlayerbotAI* /*botAI*/, WorldObject const* from)
{
    if (!from)
        return nullptr;

    std::list<Creature*> patches;
    from->GetCreatureListWithEntryInGrid(patches, NPC_IGNIS_SCORCHED_GROUND, ULDUAR_IGNIS_ROOM_SEARCH_RADIUS);

    Unit* best = nullptr;
    float bestDistance = ULDUAR_IGNIS_ROOM_SEARCH_RADIUS;

    for (Creature* patch : patches)
    {
        if (!patch->IsAlive())
            continue;

        // A patch that spawned next to the water never got lit, so it stacks no Heat on a construct
        // parked in it - the kite would sit there forever waiting for Molten.
        Position const& pool = GetIgnisNearestWaterPool(patch);
        if (patch->GetExactDist2d(&pool) <= ULDUAR_IGNIS_SCORCHED_GROUND_INERT_WATER_RADIUS)
            continue;

        float const distance = from->GetExactDist2d(patch);
        if (distance > bestDistance)
            continue;

        best = patch;
        bestDistance = distance;
    }

    return best;
}

Position const& GetIgnisNearestWaterPool(WorldObject const* from)
{
    if (!from)
        return ULDUAR_IGNIS_WATER_POOL_WEST;

    return from->GetExactDist2d(&ULDUAR_IGNIS_WATER_POOL_EAST) <
                   from->GetExactDist2d(&ULDUAR_IGNIS_WATER_POOL_WEST)
               ? ULDUAR_IGNIS_WATER_POOL_EAST
               : ULDUAR_IGNIS_WATER_POOL_WEST;
}

Player* GetIgnisConstructTank(PlayerbotAI* botAI, Player* bot) { return GetGroupAssistTank(botAI, bot, 0); }

bool IsIgnisSlagPotVictim(Player* bot)
{
    return bot && (bot->HasAura(SPELL_IGNIS_SLAG_POT_10) || bot->HasAura(SPELL_IGNIS_SLAG_POT_25));
}

Player* GetIgnisSlagPotVictim(PlayerbotAI* botAI)
{
    Group* group = botAI->GetBot()->GetGroup();
    if (!group)
        return nullptr;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && IsIgnisSlagPotVictim(member))
            return member;
    }

    return nullptr;
}

//
// Kologarn
//

Unit* GetKologarn(PlayerbotAI* botAI) { return GetFirstAliveUnitByEntry(botAI, NPC_KOLOGARN); }

Unit* GetKologarnRightArm(PlayerbotAI* botAI) { return GetFirstAliveUnitByEntry(botAI, NPC_RIGHT_ARM); }

bool KologarnEncounterActive(PlayerbotAI* botAI)
{
    // The body is the only authority: the arms and the rubble are its summons, and boss_kologarn
    // never calls SetInCombatWithZone, so this flips exactly when someone engages him.
    Unit* kologarn = GetKologarn(botAI);
    return kologarn && kologarn->IsInCombat();
}

Unit* GetKologarnNearestRubble(PlayerbotAI* botAI, WorldObject const* from)
{
    if (!from)
        return nullptr;

    Unit* best = nullptr;
    float bestDistance = ULDUAR_KOLOGARN_ROOM_SEARCH_RADIUS;

    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_RUBBLE)
            continue;

        float const distance = from->GetExactDist2d(unit);
        if (distance > bestDistance)
            continue;

        best = unit;
        bestDistance = distance;
    }

    return best;
}

bool KologarnHasRubble(PlayerbotAI* botAI)
{
    return GetFirstAliveUnitByEntry(botAI, NPC_RUBBLE) != nullptr;
}

Unit* GetKologarnLooseRubble(PlayerbotAI* botAI, Player* bot)
{
    if (!bot)
        return nullptr;

    Unit* nearest = nullptr;
    Unit* nearestLoose = nullptr;
    float nearestDistance = ULDUAR_KOLOGARN_ROOM_SEARCH_RADIUS;
    float nearestLooseDistance = ULDUAR_KOLOGARN_ROOM_SEARCH_RADIUS;

    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_RUBBLE)
            continue;

        float const distance = bot->GetExactDist2d(unit);
        if (distance < nearestDistance)
        {
            nearest = unit;
            nearestDistance = distance;
        }

        if (unit->GetVictim() != bot && distance < nearestLooseDistance)
        {
            nearestLoose = unit;
            nearestLooseDistance = distance;
        }
    }

    return nearestLoose ? nearestLoose : nearest;
}

uint8 GetKologarnCrunchArmorStacks(Unit const* unit)
{
    if (!unit)
        return 0;

    Aura* aura = unit->GetAura(SPELL_CRUNCH_ARMOR);
    if (!aura)
        aura = unit->GetAura(SPELL_CRUNCH_ARMOR_ALT);

    return aura ? aura->GetStackAmount() : 0;
}

bool IsKologarnStoneGripped(Unit const* unit)
{
    return unit && (unit->HasAura(SPELL_STONE_GRIP_10) || unit->HasAura(SPELL_STONE_GRIP_25));
}

Player* GetKologarnBodyTank(PlayerbotAI* botAI)
{
    Unit* kologarn = GetKologarn(botAI);
    if (!kologarn)
        return nullptr;

    Unit* victim = kologarn->GetVictim();
    return victim ? victim->ToPlayer() : nullptr;
}

bool IsKologarnBodyTank(PlayerbotAI* botAI, Player* bot) { return bot && GetKologarnBodyTank(botAI) == bot; }

bool IsKologarnOffTank(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || !bot->IsAlive())
        return false;

    if (!botAI->IsMainTank(bot) && !botAI->IsAssistTankOfIndex(bot, 0))
        return false;

    return !IsKologarnBodyTank(botAI, bot);
}

Unit* GetKologarnDpsTarget(PlayerbotAI* botAI, Player* bot)
{
    if (!bot)
        return nullptr;

    if (PlayerbotAI::IsRangedDps(bot))
    {
        if (Unit* rubble = GetKologarnNearestRubble(botAI, bot))
            return rubble;
    }

    if (Unit* rightArm = GetKologarnRightArm(botAI))
        return rightArm;

    return GetKologarn(botAI);
}

Unit* GetKologarnOffTankTarget(PlayerbotAI* botAI, Player* bot)
{
    Unit* kologarn = GetKologarn(botAI);
    if (!kologarn || !bot)
        return nullptr;

    Unit* rightArm = GetKologarnRightArm(botAI);
    if (rightArm && bot->GetExactDist2d(kologarn) <= ULDUAR_KOLOGARN_TAUNT_RANGE)
        return rightArm;

    return kologarn;
}

Unit* GetKologarnNearestEyebeam(PlayerbotAI* botAI, Player* bot, float radius)
{
    if (!bot)
        return nullptr;

    Unit* best = nullptr;
    float bestDistance = radius;

    for (uint32 entry : {static_cast<uint32>(NPC_KOLOGARN_EYEBEAM_LEFT), static_cast<uint32>(NPC_KOLOGARN_EYEBEAM_RIGHT)})
    {
        std::list<Creature*> eyes;
        bot->GetCreatureListWithEntryInGrid(eyes, entry, radius);

        for (Creature* eye : eyes)
        {
            if (!eye->IsAlive())
                continue;

            float const distance = bot->GetExactDist2d(eye);
            if (distance > bestDistance)
                continue;

            best = eye;
            bestDistance = distance;
        }
    }

    return best;
}

Unit* GetKologarnEyebeamChasing(PlayerbotAI* botAI, Player* bot)
{
    if (!bot)
        return nullptr;

    // The eye MoveChases whoever summoned it, so its victim is the one spell target - and it can be
    // well beyond the react radius while still closing, so this sweeps the whole room.
    Unit* eye = GetKologarnNearestEyebeam(botAI, bot, ULDUAR_KOLOGARN_ROOM_SEARCH_RADIUS);
    return eye && eye->GetVictim() == bot ? eye : nullptr;
}

Position GetKologarnRubbleHoldSpot(PlayerbotAI* botAI, Unit* rubble)
{
    Player* bot = botAI->GetBot();
    Unit* kologarn = GetKologarn(botAI);

    // Rubble drop at the dead arm's side, so their own Y says which way to drag them out of the raid.
    float const raidY = kologarn ? kologarn->GetPositionY() : bot->GetPositionY();
    float const side = rubble && rubble->GetPositionY() >= raidY ? 1.0f : -1.0f;

    float const y = std::clamp(raidY + side * ULDUAR_KOLOGARN_RUBBLE_HOLD_OFFSET,
                               ULDUAR_KOLOGARN_WALKWAY_Y_MIN, ULDUAR_KOLOGARN_WALKWAY_Y_MAX);
    float const x = std::clamp(bot->GetPositionX(), ULDUAR_KOLOGARN_WALKWAY_X_MIN, ULDUAR_KOLOGARN_WALKWAY_X_MAX);

    return Position(x, y, ULDUAR_KOLOGARN_WALKWAY_Z, bot->GetOrientation());
}

Position GetKologarnEyebeamEscapeStep(Player* bot, Unit* eye)
{
    float const step = ULDUAR_KOLOGARN_EYEBEAM_RUN_STEP;
    float const botX = bot->GetPositionX();
    float const botY = bot->GetPositionY();

    // Toward the entrance first, then along the walkway, and only then back toward the boss. X never
    // reaches the broken span, so even the last resort stays out of the pit.
    float const lateralSign = botY >= (ULDUAR_KOLOGARN_WALKWAY_Y_MIN + ULDUAR_KOLOGARN_WALKWAY_Y_MAX) / 2.0f
                                  ? 1.0f
                                  : -1.0f;
    std::pair<float, float> const candidates[] = {
        {-step, 0.0f}, {0.0f, lateralSign * step}, {0.0f, -lateralSign * step}, {step, 0.0f}};

    Position best(botX, botY, ULDUAR_KOLOGARN_WALKWAY_Z, bot->GetOrientation());
    float bestGain = 0.0f;

    for (auto const& [dx, dy] : candidates)
    {
        float const x = std::clamp(botX + dx, ULDUAR_KOLOGARN_WALKWAY_X_MIN, ULDUAR_KOLOGARN_WALKWAY_X_MAX);
        float const y = std::clamp(botY + dy, ULDUAR_KOLOGARN_WALKWAY_Y_MIN, ULDUAR_KOLOGARN_WALKWAY_Y_MAX);

        Position candidate(x, y, ULDUAR_KOLOGARN_WALKWAY_Z, bot->GetOrientation());

        // Clamped flat against a wall - no ground gained, so it buys no distance from the beam.
        float const moved = candidate.GetExactDist2d(botX, botY);
        if (moved < ULDUAR_KOLOGARN_EYEBEAM_RADIUS)
            continue;

        float const gain = eye ? candidate.GetExactDist2d(eye) - bot->GetExactDist2d(eye) : moved;
        if (gain <= bestGain)
            continue;

        best = candidate;
        bestGain = gain;
    }

    return best;
}

//
// Flame Leviathan
//

// The four NPC_FREYA_WARD_TARGET spawn points from boss_flame_leviathan.cpp, in ring order.
std::vector<Position> const ULDUAR_FL_ARENA_CORNERS = {
    Position(159.4f, 64.1f, 409.8f),
    Position(382.9f, 74.0f, 411.6f),
    Position(374.0f, -141.0f, 411.0f),
    Position(157.7f, -140.3f, 409.8f)
};

Unit* FlameLeviathanBoss(PlayerbotAI* botAI) { return GetFirstAliveUnitByEntry(botAI, NPC_FLAME_LEVIATHAN); }

bool FlameLeviathanEngaged(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || !bot->IsInCombat())
        return false;

    Unit* boss = FlameLeviathanBoss(botAI);
    return boss && !boss->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
}

Unit* FlameLeviathanRiddenVehicle(Player* bot)
{
    Unit* base = bot ? bot->GetVehicleBase() : nullptr;
    if (!base)
        return nullptr;

    // A gunner rides seat 0 of a turret creature that is itself a passenger of the real vehicle.
    if (Unit* parent = base->GetVehicleBase())
        return parent;

    return base;
}

bool FlameLeviathanIsDriver(Player* bot)
{
    Unit* base = bot ? bot->GetVehicleBase() : nullptr;
    if (!base)
        return false;

    uint32 const entry = base->GetEntry();
    return entry == NPC_SALVAGED_SIEGE_ENGINE || entry == NPC_VEHICLE_CHOPPER ||
           entry == NPC_SALVAGED_DEMOLISHER;
}

bool FlameLeviathanIsPursued(Player* bot)
{
    Unit* base = bot ? bot->GetVehicleBase() : nullptr;
    if (!base)
        return false;

    if (base->HasAura(SPELL_FL_PURSUED))
        return true;

    Unit* parent = base->GetVehicleBase();
    return parent && parent->HasAura(SPELL_FL_PURSUED);
}

bool FlameLeviathanIsVentChanneling(Unit* boss)
{
    if (!boss)
        return false;

    Spell* channel = boss->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    return channel && channel->m_spellInfo && channel->m_spellInfo->Id == SPELL_FL_FLAME_VENTS;
}

// Electroshock is a 25 yd frontal cone, so a siege engine parked across the arena would win the
// ranking and then land nothing. Range is part of eligibility, not an afterthought.
static bool FlameLeviathanCanElectroshock(Unit* siegeEngine, Unit* boss)
{
    return siegeEngine && boss && !siegeEngine->HasSpellCooldown(SPELL_FL_ELECTROSHOCK) &&
           siegeEngine->GetPower(POWER_ENERGY) >= ULDUAR_FL_ELECTROSHOCK_COST &&
           siegeEngine->IsWithinCombatRange(boss, ULDUAR_FL_ELECTROSHOCK_CONE_RADIUS);
}

bool FlameLeviathanIsVentInterrupter(PlayerbotAI* botAI, Player* bot)
{
    Unit* base = bot ? bot->GetVehicleBase() : nullptr;
    if (!base || base->GetEntry() != NPC_SALVAGED_SIEGE_ENGINE)
        return false;

    Unit* boss = FlameLeviathanBoss(botAI);
    if (!FlameLeviathanCanElectroshock(base, boss))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    uint32 const myEnergy = base->GetPower(POWER_ENERGY);
    ObjectGuid const myGuid = bot->GetGUID();

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        Unit* memberBase = member->GetVehicleBase();
        if (!memberBase || memberBase->GetEntry() != NPC_SALVAGED_SIEGE_ENGINE)
            continue;

        if (!FlameLeviathanCanElectroshock(memberBase, boss))
            continue;

        uint32 const energy = memberBase->GetPower(POWER_ENERGY);

        // Highest energy wins, guid breaks ties. Casting spends 20, which drops the caster to the
        // back of its own queue, so the duty rotates with nobody having to be told.
        if (energy > myEnergy || (energy == myEnergy && member->GetGUID() < myGuid))
            return false;
    }

    return true;
}

bool FlameLeviathanIsTarLead(PlayerbotAI* /*botAI*/, Player* bot)
{
    Unit* base = bot ? bot->GetVehicleBase() : nullptr;
    if (!base || base->GetEntry() != NPC_VEHICLE_CHOPPER)
        return false;

    // A pursued chopper is kiting away from him with its back turned, which drops tar in his path
    // for free - it does not need the lead slot, and taking it would strand the role.
    if (FlameLeviathanIsPursued(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    ObjectGuid const myGuid = bot->GetGUID();
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        Unit* memberBase = member->GetVehicleBase();
        if (!memberBase || memberBase->GetEntry() != NPC_VEHICLE_CHOPPER)
            continue;

        if (FlameLeviathanIsPursued(member))
            continue;

        if (member->GetGUID() < myGuid)
            return false;
    }

    return true;
}

bool FlameLeviathanInArena(Position const& pos, float margin)
{
    float minX = ULDUAR_FL_ARENA_CORNERS[0].GetPositionX();
    float maxX = minX;
    float minY = ULDUAR_FL_ARENA_CORNERS[0].GetPositionY();
    float maxY = minY;

    for (Position const& corner : ULDUAR_FL_ARENA_CORNERS)
    {
        minX = std::min(minX, corner.GetPositionX());
        maxX = std::max(maxX, corner.GetPositionX());
        minY = std::min(minY, corner.GetPositionY());
        maxY = std::max(maxY, corner.GetPositionY());
    }

    return pos.GetPositionX() >= minX - margin && pos.GetPositionX() <= maxX + margin &&
           pos.GetPositionY() >= minY - margin && pos.GetPositionY() <= maxY + margin;
}

static Position FlameLeviathanOffsetPoint(Unit* boss, float bearing, float standDist)
{
    float const dist = boss->GetCombatReach() + standDist;
    return Position(boss->GetPositionX() + std::cos(bearing) * dist,
                    boss->GetPositionY() + std::sin(bearing) * dist, boss->GetPositionZ());
}

Position FlameLeviathanRearPoint(Unit* boss, float standDist)
{
    return FlameLeviathanOffsetPoint(boss, boss->GetOrientation() + M_PI, standDist);
}

Position FlameLeviathanLeadPoint(Unit* boss)
{
    return FlameLeviathanOffsetPoint(boss, boss->GetOrientation(), ULDUAR_FL_TAR_LEAD_DIST);
}

std::vector<Position> const& FlameLeviathanKiteRing()
{
    static std::vector<Position> const ring = []
    {
        std::vector<Position> nodes;
        size_t const count = ULDUAR_FL_ARENA_CORNERS.size();

        float centreX = 0.0f;
        float centreY = 0.0f;
        for (Position const& corner : ULDUAR_FL_ARENA_CORNERS)
        {
            centreX += corner.GetPositionX();
            centreY += corner.GetPositionY();
        }
        centreX /= static_cast<float>(count);
        centreY /= static_cast<float>(count);

        // Pull each corner off the wall first, so the chamfer is cut from a point the vehicles can
        // actually path to rather than from the wall itself.
        std::vector<Position> inset;
        inset.reserve(count);
        for (Position const& corner : ULDUAR_FL_ARENA_CORNERS)
        {
            float dx = centreX - corner.GetPositionX();
            float dy = centreY - corner.GetPositionY();
            float const len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.0f)
            {
                dx /= len;
                dy /= len;
            }
            inset.emplace_back(corner.GetPositionX() + dx * ULDUAR_FL_KITE_WALL_INSET,
                               corner.GetPositionY() + dy * ULDUAR_FL_KITE_WALL_INSET,
                               corner.GetPositionZ());
        }

        auto towards = [](Position const& from, Position const& to)
        {
            float dx = to.GetPositionX() - from.GetPositionX();
            float dy = to.GetPositionY() - from.GetPositionY();
            float const len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.0f)
            {
                dx /= len;
                dy /= len;
            }
            return Position(from.GetPositionX() + dx * ULDUAR_FL_KITE_CORNER_CHAMFER,
                            from.GetPositionY() + dy * ULDUAR_FL_KITE_CORNER_CHAMFER,
                            from.GetPositionZ());
        };

        // Two nodes per corner, one on each adjoining edge, so a kiting vehicle rounds the turn
        // instead of driving into the corner while the boss cuts the diagonal.
        for (size_t i = 0; i < count; ++i)
        {
            Position const& prev = inset[(i + count - 1) % count];
            Position const& next = inset[(i + 1) % count];
            nodes.push_back(towards(inset[i], prev));
            nodes.push_back(towards(inset[i], next));
        }

        return nodes;
    }();

    return ring;
}

namespace
{
// Where NPC 33576 will be `seconds` from now: it laps the room clockwise on a fixed waypoint path, so
// rotating its current position about the room centre by (speed / radius) * time predicts it. Radius
// is measured live rather than hardcoded, which absorbs the polygon's 110-116 yd wobble.
Position MimironOrbitAhead(Position const& now, float seconds)
{
    float const dx = now.GetPositionX() - ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX();
    float const dy = now.GetPositionY() - ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY();
    float const radius = std::sqrt(dx * dx + dy * dy);
    if (radius < 1.0f)
        return now;

    float const turned = -ULDUAR_MIMIRON_DB_TARGET_SPEED * seconds / radius;
    float const c = std::cos(turned);
    float const sn = std::sin(turned);

    return Position(ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX() + dx * c - dy * sn,
                    ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY() + dx * sn + dy * c,
                    now.GetPositionZ());
}
}  // namespace

MimironBarrageWindow GetMimironBarrageWindow(Player* bot, Unit* vx001)
{
    MimironBarrageWindow window;
    if (!bot || !vx001)
        return window;

    // Spinning Up is a 4 s aura whose single tick starts the barrage, and the barrage aura then runs
    // 10 s. Reading both durations rather than assuming them is what makes the model survive a bot
    // joining the fight mid-cast.
    float fire = ULDUAR_MIMIRON_BARRAGE_FIRE_SECONDS;
    if (Aura* spinningUp = vx001->GetAura(SPELL_SPINNING_UP))
        window.untilLive = static_cast<float>(spinningUp->GetDuration()) / 1000.0f;
    else if (Aura* barrage = vx001->GetAura(SPELL_P3WX2_LASER_BARRAGE_AURA_1))
        fire = static_cast<float>(barrage->GetDuration()) / 1000.0f;
    else
        return window;

    window.valid = true;

    Creature* dbTarget = bot->FindNearestCreature(NPC_MIMIRON_DB_TARGET, 250.0f);
    if (!dbTarget)
    {
        // FaceBarrageArc returns early without 33576, so the core never re-aims and the cone stays
        // frozen wherever it is pointing. A static wedge is the honest read of that; running a sweep
        // that is not happening would walk the raid straight through the beams.
        window.lead = vx001->GetOrientation();
        return window;
    }

    Position const dbNow = dbTarget->GetPosition();
    Position const ignition = MimironOrbitAhead(dbNow, window.untilLive);
    Position const finish = MimironOrbitAhead(dbNow, window.untilLive + fire);

    window.lead = vx001->GetAngle(ignition.GetPositionX(), ignition.GetPositionY());

    float const tail = vx001->GetAngle(finish.GetPositionX(), finish.GetPositionY());
    window.sweep = Position::NormalizeOrientation(window.lead - tail);
    window.rate = fire > 0.0f ? window.sweep / fire : 0.0f;

    return window;
}

bool IsMimironSpotMineSafe(Player* bot, Position const& dest, float clearance)
{
    if (!bot)
        return true;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return true;

    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_PROXIMITY_MINE)
            continue;

        if (dest.GetExactDist2d(unit->GetPositionX(), unit->GetPositionY()) < clearance)
            return false;
    }

    return true;
}

bool IsMimironSpotSafe(Player* bot, Position const& dest)
{
    if (!bot)
        return true;

    if (!IsMimironSpotMineSafe(bot, dest))
        return false;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return true;

    // Firefighter spreads ground fire across the floor, so a standing spot can end up inside it. Without
    // this the flames node at ACTION_RAID + 4 pushes the bot out and the formation at ACTION_RAID pulls
    // it straight back, and it paces on the edge until it burns down.
    bool const hardMode = IsMimironHardModeActive(botAI);

    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        uint32 const entry = unit->GetEntry();
        float clearance = 0.0f;

        if (entry == NPC_ROCKET_STRIKE_N)
            clearance = ULDUAR_MIMIRON_ROCKET_CLEARANCE;
        else if (hardMode && (entry == NPC_FLAMES_SPREAD || entry == NPC_FLAMES_INITIAL))
            clearance = ULDUAR_MIMIRON_FLAMES_RADIUS;
        else
            continue;

        if (dest.GetExactDist2d(unit->GetPositionX(), unit->GetPositionY()) < clearance)
            return false;
    }

    return true;
}

bool IsMimironSpotBarrageSafe(Unit* vx001, MimironBarrageWindow const& window, Position const& dest,
                              float travelSeconds)
{
    if (!vx001 || !window.valid)
        return true;

    float const clearance = ULDUAR_MIMIRON_BARRAGE_HALF_ANGLE + ULDUAR_MIMIRON_BARRAGE_MARGIN;
    float const twoPi = 2.0f * static_cast<float>(M_PI);

    // Extend the band by the sweep the leg will not be able to react to. The band only grows on the
    // trailing side - that is the edge coming toward a bot standing still.
    float const grown =
        std::min(window.sweep + window.rate * std::max(travelSeconds, 0.0f), twoPi - 2.0f * clearance);

    float const cw = Position::NormalizeOrientation(
        window.lead - vx001->GetAngle(dest.GetPositionX(), dest.GetPositionY()));

    return cw > grown + clearance && cw < twoPi - clearance;
}

std::string GetMimironBombBotSnare(Player* bot)
{
    if (!bot)
        return "";

    switch (bot->getClass())
    {
        case CLASS_HUNTER:  return "concussive shot";
        case CLASS_SHAMAN:  return "frost shock";
        case CLASS_WARLOCK: return "curse of exhaustion";
        default:            return "";
    }
}

float GetMimironBombBotApproach(Player* bot, Unit* bombBot)
{
    if (!bot || !bombBot)
        return 0.0f;

    if (Unit* victim = ServerFacade::instance().GetChaseTarget(bombBot))
        return bombBot->GetExactDist2d(victim);

    return bombBot->GetExactDist2d(bot);
}

bool IsMimironAcuGrounded(PlayerbotAI* botAI)
{
    if (!botAI)
        return false;

    Unit* aerialCommandUnit = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
    return aerialCommandUnit && aerialCommandUnit->HasAura(SPELL_MIMIRON_MAGNETIC_CORE_AURA);
}

Unit* GetMimironRingFocus(PlayerbotAI* botAI)
{
    if (!botAI)
        return nullptr;

    if (Unit* vx001 = GetFirstAliveUnitByEntry(botAI, NPC_VX001))
        return vx001;

    if (Unit* leviathanMkII = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII))
        return leviathanMkII;

    return GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
}

Unit* GetMimironStagingFocus(Player* bot)
{
    if (!bot)
        return nullptr;

    Creature* vx001 = bot->FindNearestCreature(NPC_VX001, ULDUAR_MIMIRON_STAGING_SEARCH_RANGE);
    Creature* aerialCommandUnit =
        bot->FindNearestCreature(NPC_AERIAL_COMMAND_UNIT, ULDUAR_MIMIRON_STAGING_SEARCH_RANGE);

    // VX-001 only ever rides anything in phase 4, so this is the moment the assembly is far enough
    // along to be worth forming a ring for. It boards 18.8 s into a 31.8 s handover, which still leaves
    // more than twice the walk from a phase 3 wedge slot.
    if (vx001 && vx001->GetVehicleBase())
        return vx001;

    if (aerialCommandUnit)
        return aerialCommandUnit;

    // Nothing before the pull or after a wipe: the MK II is NOT_SELECTABLE until it is pulled, and
    // evade despawns VX-001 and the Aerial Command Unit outright.
    return vx001;
}

bool IsMimironPhase4(Player* bot)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI)
        return false;

    // Cached lookups only. This is asked several times per bot per tick, from the target list, the
    // tank node and the pet node, so a grid sweep here would cost the whole raid every phase.

    // VX-001 and the Aerial Command Unit both ride something from phase 4 on, and neither does before
    // it, so either one answers on its own while it is still attackable.
    if (Unit* vx001 = GetFirstAliveUnitByEntry(botAI, NPC_VX001))
        return vx001->GetVehicleBase() != nullptr;

    if (Unit* aerialCommandUnit = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        return aerialCommandUnit->GetVehicleBase() != nullptr;

    // Both pushed under 15000 and gone NON_ATTACKABLE, so only the chassis is left. It never rides
    // anything itself, but seat 3 holds the cannon in phase 1 and VX-001 from phase 4 on, and who is
    // sitting there is readable whatever flags the passengers carry.
    if (Unit* leviathanMkII = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII))
        if (Vehicle* kit = leviathanMkII->GetVehicleKit())
            if (Unit* seated = kit->GetPassenger(3))
                return seated->GetEntry() == NPC_VX001;

    return false;
}

Unit* GetMimironPhase4Focus(PlayerbotAI* botAI, Player* bot, bool melee)
{
    if (!botAI || !bot || !IsMimironPhase4(bot))
        return nullptr;

    std::vector<Unit*> parts;
    for (uint32 entry : {NPC_LEVIATHAN_MKII, NPC_VX001, NPC_AERIAL_COMMAND_UNIT})
        if (Unit* part = GetFirstAliveUnitByEntry(botAI, entry))
            parts.push_back(part);

    if (parts.empty())
        return nullptr;

    auto const highest = [](std::vector<Unit*> const& candidates) -> Unit*
    {
        Unit* best = nullptr;
        for (Unit* candidate : candidates)
            if (!best || candidate->GetHealthPct() > best->GetHealthPct())
                best = candidate;

        return best;
    };

    // Fewer than three attackable means one is already channelling Self Repair and the 15 s clock is
    // running. The rendezvous is over, so every restriction comes off - including melee on the Aerial
    // Command Unit, who would otherwise have nothing to hit through the stretch that decides whether
    // the kill lands or the whole phase resets.
    if (parts.size() < 3)
        return highest(parts);

    std::vector<Unit*> allowed;
    for (Unit* part : parts)
    {
        // Ranged DPS own the Aerial Command Unit. IsRangedDps rather than IsRanged so a healer is never
        // steered onto it, nor into the hold below, where it would stop healing.
        if (part->GetEntry() == NPC_AERIAL_COMMAND_UNIT && (melee || !PlayerbotAI::IsRangedDps(bot)))
            continue;

        allowed.push_back(part);
    }

    if (allowed.empty())
        return nullptr;

    // All three levelled out, so stop holding and push them under together.
    bool levelled = true;
    for (Unit* part : parts)
        if (part->GetHealthPct() > ULDUAR_MIMIRON_PHASE4_HOLD_PCT)
            levelled = false;

    if (levelled)
        return highest(allowed);

    std::vector<Unit*> aboveFloor;
    for (Unit* part : allowed)
        if (part->GetHealthPct() > ULDUAR_MIMIRON_PHASE4_HOLD_PCT)
            aboveFloor.push_back(part);

    // Nothing left this bot may touch that is not already at the floor. Hold: all three sit on one
    // point server-side, so cleave splashes every part, and 10 % is the margin that keeps incidental
    // damage from pushing one under while the others are still high.
    return aboveFloor.empty() ? nullptr : highest(aboveFloor);
}

bool IsMimironTankAnchorSlot(PlayerbotAI* botAI, Player* bot)
{
    // Phases 1 and 4 both park the MK II, and both are the phases it lays mines in.
    return botAI && bot && PlayerbotAI::IsMainTank(bot) &&
           GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) != nullptr;
}

Player* GetMimironCoreCarrier(PlayerbotAI* botAI)
{
    if (!botAI)
        return nullptr;

    Group* group = botAI->GetBot()->GetGroup();
    if (!group)
        return nullptr;

    Player* fallback = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !GET_PLAYERBOT_AI(member))
            continue;

        if (!fallback)
            fallback = member;

        if (!PlayerbotAI::IsRanged(member) && !PlayerbotAI::IsTank(member))
            return member;
    }

    return fallback;
}

namespace
{
// Keep a formation anchor on the floor. Moves the anchor and never a single slot: clamping slots one
// at a time deforms the formation into a lopsided blob leaning at the boss, which hands Rapid Burst
// and the Bomb Bots exactly the clumps the spread exists to prevent.
//
// This used to also slide the anchor toward the focus until the outermost slot was inside casting
// range. That measured `extent` in every direction while the phase 3 wedge only occupies 120 degrees
// of it, so with spellDistance 28.5 and a two-row wedge the slide always landed within half a yard of
// the boss - and could overshoot past it, because the excess was never clamped to the distance. The
// wedge then tracked the Aerial Command Unit exactly while the unit held 30 yd from a bot inside that
// wedge, and raid and boss circled the room together.
Position ClampMimironAnchorToRoom(Position anchor)
{
    float const z = ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ();

    float const fromCentre =
        ULDUAR_MIMIRON_ROOM_CENTER.GetExactDist2d(anchor.GetPositionX(), anchor.GetPositionY());
    if (fromCentre > ULDUAR_MIMIRON_ROOM_RADIUS)
    {
        float const bearing =
            ULDUAR_MIMIRON_ROOM_CENTER.GetAngle(anchor.GetPositionX(), anchor.GetPositionY());
        anchor = Position(
            ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX() + ULDUAR_MIMIRON_ROOM_RADIUS * cos(bearing),
            ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY() + ULDUAR_MIMIRON_ROOM_RADIUS * sin(bearing), z);
    }

    return anchor;
}

// How many rows `count` bots need, capped at `maxRows`. Once the band is full the remaining rows just
// pack tighter: spacing is the thing to give up, not range, because a Bomb Bot catching two bots is
// cheaper than half the raid unable to reach the boss at all.
uint32 MimironWedgeRows(float firstRow, uint32 maxRows, uint32 count)
{
    uint32 held = 0;
    for (uint32 rows = 1; rows <= maxRows; ++rows)
    {
        float const radius = firstRow + (rows - 1) * ULDUAR_MIMIRON_PHASE3_SPACING;
        held += static_cast<uint32>(2.0f * ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE * radius /
                                    ULDUAR_MIMIRON_PHASE3_SPACING);
        if (held >= count)
            return rows;
    }

    return maxRows;
}

// Slot `index` of `count`, dealt row by row from the inside out and then spread edge to edge along
// whichever row it landed in.
void MimironWedgeSlot(float firstRow, uint32 rows, uint32 index, uint32 count, float& outRadius,
                      float& outOffset)
{
    uint32 const base = count / rows;
    uint32 const extra = count % rows;  // the first `extra` rows carry one more

    uint32 row = 0;
    uint32 filled = 0;
    for (; row + 1 < rows; ++row)
    {
        uint32 const size = base + (row < extra ? 1 : 0);
        if (index < filled + size)
            break;

        filled += size;
    }

    uint32 const size = base + (row < extra ? 1 : 0);
    uint32 const slot = index - filled;

    outRadius = firstRow + row * ULDUAR_MIMIRON_PHASE3_SPACING;
    outOffset = size <= 1 ? 0.0f
                          : -ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE +
                                2.0f * ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE * slot / (size - 1);
}

// Where melee and tanks wait out a handover: a small ring on the room centre, which is where all three
// handovers converge - VX-001 is summoned there, the Aerial Command Unit spawns and is walked back
// there, and the chassis ends there. Never on the focus itself: it is mid-script for most of the
// window, so a ring pinned to it drags the raid along the chassis charge waypoints.
bool GetMimironStagingMeleeSlot(Player* bot, Group* group, Unit* focus, Position& out)
{
    uint32 index = 0;
    uint32 count = 0;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        // Main tanks are counted here too. Only a phase 4 main tank has a spot of its own, and that
        // one is handed out before this is ever reached.
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || PlayerbotAI::IsRanged(member))
            continue;

        if (member == bot)
            index = count;

        ++count;
    }

    if (count == 0)
        return false;

    // Outside the mech's own model. The chassis has the largest reach of the three at 8, so a flat
    // 8 yd ring would stage half the melee inside it.
    float const radius = focus ? std::max(ULDUAR_MIMIRON_STAGING_MELEE_RADIUS,
                                          focus->GetCombatReach() + 1.0f)
                              : ULDUAR_MIMIRON_STAGING_MELEE_RADIUS;

    float const bearing = 2.0f * static_cast<float>(M_PI) * index / count;
    out = Position(ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX() + radius * std::cos(bearing),
                   ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY() + radius * std::sin(bearing),
                   ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());
    return true;
}

// Phase 3. The raid groups in the east wedge instead of ringing the room: the summon pads sit on three
// arms - west, north-east and south-east, each carrying pads at roughly 17, 29 and 40 yd - so a ring
// drops lone ranged bots straight into an add's path.
bool GetMimironPhase3Slot(Player* bot, Group* group, Position& out)
{
    // Melee stand on whatever they are hitting. Every add walks in from a pad well outside the wedge,
    // so any fixed melee slot is a spot the target is not in - and this formation runs at ACTION_RAID,
    // above the chase at ACTION_HIGH, so it wins the tick and the bot never lands a swing.
    if (!PlayerbotAI::IsRanged(bot))
        return false;

    uint32 index = 0;
    uint32 count = 0;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !PlayerbotAI::IsRanged(member))
            continue;

        if (member == bot)
            index = count;

        ++count;
    }

    if (count == 0)
        return false;

    // The band stops at casting range, so the wedge is built to fit rather than grown until it does.
    // Past that the rows pack tighter instead: a Bomb Bot catching two bots is cheaper than half the
    // raid unable to reach the boss.
    float const rangedDepth = std::max(sPlayerbotAIConfig.spellDistance -
                                           ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN -
                                           ULDUAR_MIMIRON_PHASE3_MIN_RADIUS,
                                       0.0f);
    uint32 const rows = MimironWedgeRows(
        ULDUAR_MIMIRON_PHASE3_MIN_RADIUS,
        1u + static_cast<uint32>(rangedDepth / ULDUAR_MIMIRON_PHASE3_SPACING), count);

    float radius = 0.0f;
    float offset = 0.0f;
    MimironWedgeSlot(ULDUAR_MIMIRON_PHASE3_MIN_RADIUS, rows, index, count, radius, offset);

    // The room centre, and nothing else. The Aerial Command Unit has no attack in this phase - its
    // whole event list is add summons - so there is nothing range on it buys, and holding still is
    // what leaves a Bomb Bot spawning on it roughly 30 yd of open floor to cross at 8.0 yd/s.
    Position const& anchor = ULDUAR_MIMIRON_ROOM_CENTER;

    // The centreline is the bearing to the staging point: the middle of the gap between the two east
    // arms, and the one direction nothing walks in from.
    float const centreline = ULDUAR_MIMIRON_ROOM_CENTER.GetAngle(
        ULDUAR_MIMIRON_PHASE3_STAGE.GetPositionX(), ULDUAR_MIMIRON_PHASE3_STAGE.GetPositionY());
    float const bearing = Position::NormalizeOrientation(centreline + offset);

    out = Position(anchor.GetPositionX() + radius * cos(bearing),
                   anchor.GetPositionY() + radius * sin(bearing),
                   ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());
    return true;
}
}  // namespace

bool GetMimironSpreadSlot(PlayerbotAI* botAI, Player* bot, Position& out)
{
    if (!botAI || !bot)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Nothing attackable means a phase handover, which runs anywhere from 24 to 48 seconds. The mechs
    // are all NOT_SELECTABLE for the whole of it, so without this the raid falls through to follow and
    // walks into the next phase from wherever its master happened to be standing.
    Unit* focus = GetMimironRingFocus(botAI);
    bool const staging = focus == nullptr;
    if (staging)
        focus = GetMimironStagingFocus(bot);

    if (!focus)
        return false;

    // The main tank holds the chassis spot once all three mechs are up. Walking it anywhere else in
    // phase 4 drags VX-001 with it, and VX-001 is what the Laser Barrage cone radiates from.
    bool const phase4 = staging ? focus->GetEntry() == NPC_VX001 && focus->GetVehicleBase() != nullptr
                                : GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) &&
                                      GetFirstAliveUnitByEntry(botAI, NPC_VX001) &&
                                      GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);

    if (PlayerbotAI::IsMainTank(bot) && phase4)
    {
        out = ULDUAR_MIMIRON_PHASE4_TANK_SPOT;
        return true;
    }

    // Melee only get a spot while staging, where there is no chase for it to fight and arriving in
    // melee range before the boss goes live is the whole point.
    if (staging && !PlayerbotAI::IsRanged(bot))
        return GetMimironStagingMeleeSlot(bot, group, focus, out);

    if (focus->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
        return GetMimironPhase3Slot(bot, group, out);

    // Phase 1 tank spot. Nothing else brings the MK II back: the tank is melee, so it flees Shock
    // Blast every 30 s and the boss follows, and over a five minute phase that walks the fight round
    // the room until half the raid is out of casting range. This is the point the encounter script
    // itself charges the MK II to.
    if (PlayerbotAI::IsMainTank(bot) && focus->GetEntry() == NPC_LEVIATHAN_MKII)
    {
        out = ULDUAR_MIMIRON_ROOM_CENTER;
        return true;
    }

    // Melee stand on whatever they are hitting, so only ranged and healers get a ring slot.
    if (!PlayerbotAI::IsRanged(bot))
        return false;

    uint32 index = 0;
    uint32 count = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !PlayerbotAI::IsRanged(member) ||
            PlayerbotAI::IsMainTank(member))
            continue;

        if (member == bot)
            index = count;

        ++count;
    }

    if (count == 0)
        return false;

    // Centred on the mech while a phase is live: both ground mechs get dragged about by their tanks,
    // and a ring pinned to the room centre puts the far half of the raid past casting range after only
    // six yards of drift - which then deadlocks rather than self-correcting, because "reach spell" is
    // ACTION_HIGH and this ring is ACTION_RAID. During a handover it is the room centre instead, for
    // the same reason the melee staging ring is: the focus is mid-script and walking.
    Position const anchor = ClampMimironAnchorToRoom(
        staging ? ULDUAR_MIMIRON_ROOM_CENTER
                : Position(focus->GetPositionX(), focus->GetPositionY(),
                           ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ()));

    float const angle = 2.0f * static_cast<float>(M_PI) * index / count;
    out = Position(anchor.GetPositionX() + ULDUAR_MIMIRON_SPREAD_RADIUS * cos(angle),
                   anchor.GetPositionY() + ULDUAR_MIMIRON_SPREAD_RADIUS * sin(angle),
                   ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());
    return true;
}
