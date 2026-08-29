/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldBossHelper.h"
#include "AiFactory.h"
#include "BossAuraTriggers.h"
#include "GameObject.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "RaidBossHelpers.h"
#include "UldHardMode.h"
#include "Vehicle.h"
#include "DynamicObject.h"
#include "Map.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Timer.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <ctime>
#include <limits>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>

// Room centre, midway between the two water pools. The whole fight is fought here: it is the only
// spot from which the patch fan clears both pools by the 25 yd the core needs to light them.
const Position ULDUAR_IGNIS_BOSS_ANCHOR = Position(587.5f, 277.8f, 360.8f);
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
const Position ULDUAR_THORIM_PHASE2_MELEE1_SPOT = Position(2142.9f, -278.0f, 419.64f);
const Position ULDUAR_THORIM_PHASE2_MELEE2_SPOT = Position(2134.9f, -270.0f, 419.85f);
const Position ULDUAR_THORIM_PHASE2_MELEE3_SPOT = Position(2126.9f, -278.0f, 419.64f);
const Position ULDUAR_THORIM_PHASE2_OFFTANK_SPOT = Position(2137.9f, -287.0f, 419.48f);
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
// 41.7yd from the tank anchor and 25.5yd from the ranged anchor: well clear of the 12yd splash, and
// still inside spell range, so nobody has to move for the hard-mode Life Spark that spawns here.
const Position ULDUAR_XT002_SEARING_LIGHT_SPOT = Position(862.73724f, 12.77857f, 409.8322f);
// Two origins so melee and ranged carriers do not drop Void Zones on top of each other. Each is the
// corner of its grid nearest the raid - roughly 30yd from the carrier's usual spot, which is the run
// that has to fit inside the 9s the debuff lasts.
const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_MELEE = Position(871.5199f, -42.04216f, 409.80377f);
const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED = Position(837.0746f, -41.01061f, 409.80362f);

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

bool IsAuriayaEngaged(PlayerbotAI* botAI)
{
    Unit* boss = GetAuriaya(botAI);
    return boss && boss->IsInCombat();
}

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

bool IsHodirEngaged(PlayerbotAI* botAI)
{
    Unit* boss = GetHodir(botAI);
    return boss && boss->IsInCombat();
}

bool IsHodirFlashFreezeIncoming(PlayerbotAI* botAI)
{
    Unit* boss = GetHodir(botAI);

    // Every cast slot, not CURRENT_GENERIC_SPELL: which slot a scripted boss cast lands in is the
    // script's business, and guessing wrong here silently opens the window on nothing.
    return boss && boss->HasUnitState(UNIT_STATE_CASTING) &&
           boss->FindCurrentSpellBySpellId(SPELL_FLASH_FREEZE) != nullptr;
}

bool HodirFrozenBlowsActive(PlayerbotAI* botAI, Player* bot)
{
    Unit* boss = GetHodir(botAI);
    return boss && bot && boss->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_FROZEN_BLOWS, bot));
}

bool HodirTauntWouldBeSuicide(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || bot->GetHealthPct() >= ULDUAR_HODIR_TAUNT_HEALTH_FLOOR)
        return false;

    if (!HodirFrozenBlowsActive(botAI, bot))
        return false;

    // Somebody else has to already be holding him. With nobody on him the taunt is the rescue and has
    // to survive whatever shape the bot is in, and a taunt at a creature victim is how the boss comes
    // off a pet.
    Unit* boss = GetHodir(botAI);
    Unit* victim = boss ? boss->GetVictim() : nullptr;
    return victim && victim != bot && victim->IsPlayer() && victim->IsAlive();
}

Creature* GetHodirSharedShelter(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || !GetHodir(botAI))
        return nullptr;

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_SNOWPACKED_ICICLE, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);

    // Before the centre, not after: a shelter only exists for about 6s of every 49s cycle, and the
    // centre costs a second grid sweep to find the fire.
    if (found.empty())
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "hodir.shelter", "none");

        return nullptr;
    }

    // Nearest the ring centre rather than nearest the bot, so the whole raid converges on one drift
    // and re-forms cleanly instead of splitting across the two or three that spawn. Trigger and
    // action both call this: two derivations of "which shelter" would disagree and oscillate.
    //
    // The centre, not ULDUAR_HODIR_RAID_ANCHOR: the ring rides a Toasty Fire now and sits a median
    // 9.5 yd off that fixed point, p90 17.6. Measuring from a spot the raid is not standing on was
    // picking a drift 25 yd away with three other candidates on the floor.
    Position const centre = GetHodirRingCentre(botAI, bot);

    Creature* best = nullptr;
    float bestDist = 0.0f;
    for (Creature* shelter : found)
    {
        if (!shelter || !shelter->IsAlive())
            continue;

        float const dist = shelter->GetExactDist2d(&centre);
        if (!best || dist < bestDist)
        {
            best = shelter;
            bestDist = dist;
        }
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.shelter",
                             best ? RaidObs::DescribeAssignment(best->GetGUID()) : "none");

    return best;
}

Creature* GetHodirRaidFire(PlayerbotAI* botAI, Player* bot)
{
    Unit* hodir = bot ? GetHodir(botAI) : nullptr;
    if (!hodir)
        return nullptr;

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_TOASTY_FIRE, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);

    Creature* best = nullptr;
    float bestDist = 0.0f;
    for (Creature* fire : found)
    {
        if (!fire || !fire->IsAlive())
            continue;

        // Measured from Hodir, not from the fixed anchor: he drifts and the anchor does not, so a fire
        // picked off the anchor put the far side of the ring 45 yd from him and the casters walked a
        // reach spell back in. A fire the boss is standing on is no good either, however close.
        float const gap = fire->GetExactDist2d(hodir);
        if (gap < ULDUAR_HODIR_CENTRE_MIN_BOSS_GAP)
            continue;

        // The whole ring has to reach him from it, not just the centre.
        if (gap + ULDUAR_HODIR_RAID_RING_OUTER + ULDUAR_HODIR_RING_SPOT_TOLERANCE >
            ULDUAR_HODIR_CASTER_MAX_BOSS_GAP)
            continue;

        if (!best || gap < bestDist)
        {
            best = fire;
            bestDist = gap;
        }
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.fire",
                             best ? RaidObs::DescribeAssignment(best->GetGUID()) : "none");

    return best;
}

Position GetHodirRingCentre(PlayerbotAI* botAI, Player* bot, bool* onFire)
{
    Position centre = ULDUAR_HODIR_RAID_ANCHOR;

    // Unquantised. The fire does not move, so there is nothing for a quantum to smooth out, and
    // rounding would only push the centre off the one point the whole ring is sized around.
    Creature* fire = bot ? GetHodirRaidFire(botAI, bot) : nullptr;
    if (fire)
        centre = Position(fire->GetPositionX(), fire->GetPositionY(),
                          ULDUAR_HODIR_RAID_ANCHOR.GetPositionZ());

    // Reported rather than re-derived: the fire sweep is the expensive half of this call, and a
    // centre that happens to sit near a fire is not the same as a centre that is one.
    if (onFire)
        *onFire = fire != nullptr;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.centre", RaidObs::DescribeDerived(centre));

    return centre;
}

// The ring roster, in the order every bot derives identically. The dead keep their slots: indexing by
// the living instead shifts every bot after the corpse, so one death re-seats the whole formation and
// the raid walks the layout again mid-fight - exactly when it can least afford to. A vacant slot
// costs nothing; someone zoned out is a different case, because they are not coming back to it.
static bool BuildHodirRingMembers(Player* bot, std::vector<Player*>& out)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    out.clear();
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member->GetMapId() != bot->GetMapId())
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || !memberAI->IsRanged(member) || memberAI->IsTank(member))
            continue;

        out.push_back(member);
    }

    if (out.empty())
        return false;

    // Ranged dps ahead of healers, then guid. Both keys are identical on every bot, so nobody has to
    // be told which slot is theirs.
    std::sort(out.begin(), out.end(), [](Player* left, Player* right)
    {
        bool const leftDps = GET_PLAYERBOT_AI(left)->IsRangedDps(left);
        bool const rightDps = GET_PLAYERBOT_AI(right)->IsRangedDps(right);
        if (leftDps != rightDps)
            return leftDps;
        return left->GetGUID() < right->GetGUID();
    });

    return true;
}

// Raw ring geometry for one slot, no ground or collision pass. Cheap enough to run for the whole
// roster, which is what deciding who owns a Starlight zone needs.
static Position HodirRingSlotPoint(Position const& centre, size_t slot, size_t total)
{
    // Bearing between two fixed points, so the layout never rotates. Deriving it from the centre
    // instead would spin every slot each time the centre moved.
    float const baseAngle = std::atan2(ULDUAR_HODIR_MAINTANK_SPOT.GetPositionY() - ULDUAR_HODIR_RAID_ANCHOR.GetPositionY(),
                                       ULDUAR_HODIR_MAINTANK_SPOT.GetPositionX() - ULDUAR_HODIR_RAID_ANCHOR.GetPositionX());

    size_t const inner = std::min<size_t>(ULDUAR_HODIR_RAID_RING_INNER_SLOTS, total > 0 ? total - 1 : 0);

    float radius = 0.0f;
    float angle = baseAngle;

    if (!slot)
    {
        // Slot 0 stands on the centre itself - the safest spot in the fire and the only one that is
        // never within 4 yd of two neighbours at once.
        radius = 0.0f;
    }
    else if (slot <= inner)
    {
        radius = ULDUAR_HODIR_RAID_RING_INNER;
        angle = baseAngle + 2.0f * static_cast<float>(M_PI) * static_cast<float>(slot - 1) / static_cast<float>(inner);
    }
    else
    {
        size_t const outerCount = total - inner - 1;
        size_t const outerSlot = slot - inner - 1;
        radius = ULDUAR_HODIR_RAID_RING_OUTER;
        angle = baseAngle +
                2.0f * static_cast<float>(M_PI) * static_cast<float>(outerSlot) / static_cast<float>(outerCount);
    }

    angle = Position::NormalizeOrientation(angle);

    return Position(centre.GetPositionX() + std::cos(angle) * radius,
                    centre.GetPositionY() + std::sin(angle) * radius, centre.GetPositionZ());
}

// Raw formation geometry is exactly the shape that lands off the navmesh, and MoveTo would then fail
// without telling anyone.
static Position ValidateHodirFloorPoint(Player* bot, Position const& point)
{
    float x = point.GetPositionX();
    float y = point.GetPositionY();

    float z = bot->GetMapWaterOrGroundLevel(x, y, point.GetPositionZ());
    if (z <= INVALID_HEIGHT)
        z = point.GetPositionZ();

    bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                   bot->GetPositionZ(), x, y, z, false);

    return Position(x, y, z);
}

bool GetHodirRingSlot(PlayerbotAI* botAI, Player* bot, Position const& centre, Position& out)
{
    std::vector<Player*> ringMembers;
    if (!BuildHodirRingMembers(bot, ringMembers))
        return false;

    size_t slot = ringMembers.size();
    for (size_t i = 0; i < ringMembers.size(); ++i)
        if (ringMembers[i] == bot)
            slot = i;

    if (slot >= ringMembers.size())
        return false;

    size_t const total = ringMembers.size();
    out = ValidateHodirFloorPoint(bot, HodirRingSlotPoint(centre, slot, total));

    // The slot index and the size of the ring it was cut from, so a formation that re-seated is
    // readable without re-deriving the sort from the roster.
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.slot",
                             std::to_string(slot) + "/" + std::to_string(total) + " " +
                                 RaidObs::DescribeDerived(out));

    return true;
}

// Where this bot stands if there is a Starlight zone it can use. Starlight is +50% to cast time and
// all three attack timers, the fight's biggest throughput lever, and it is worth stacking for: one
// Ice Shards hit is 41% of a health pool (p90 54%), so an icicle catching two bots in one zone costs
// two heals rather than two lives. Any number may share a zone; what each gets is a bearing of its
// own, taken from the slot it came from, so they spread around it instead of piling on a point.
//
// Usable means the resulting spot still reaches Hodir and is still out of his reach; the nearest zone
// to the slot wins among those, so the detour stays short.
static bool FindHodirStarlightStand(PlayerbotAI* botAI, Player* bot, Position const& centre,
                                    bool onFire, Position const& slot, Position& out)
{
    // Bounded rather than the room radius: this runs per bot per tick, and a zone further out than
    // this cannot be within reach of any slot the bot could be standing on anyway.
    std::vector<Position> const zones =
        GetDynamicObjectPositions(bot, ULDUAR_HODIR_STARLIGHT_SEARCH_RADIUS, SPELL_HODIR_STARLIGHT);
    if (zones.empty())
        return false;

    // The druid casts where it is standing, which is usually next to Hodir, so a good share of the
    // zones on the floor are ones no caster can use.
    Unit* hodir = GetHodir(botAI);

    // Staying inside the fire is not optional while there is one: a bot outside it starts shedding
    // Biting Cold, and that node outranks this one, so it would shuttle straight back out of the zone
    // it just walked to. Off a fire there is nothing to stay inside - the bot is shedding wherever it
    // stands - and applying the leash anyway drew a 10 yd box round the anchor that rejected 89% of
    // the zones on the floor, which is why only 1.40 dps of 18 were ever standing in one.
    float const fireLeash = ULDUAR_HODIR_TOASTY_FIRE_RADIUS - ULDUAR_HODIR_STARLIGHT_STAND_TOLERANCE;

    bool found = false;
    float bestWalk = 0.0f;

    for (Position const& zone : zones)
    {
        float const walk = slot.GetExactDist2d(&zone);
        if (found && walk >= bestWalk)
            continue;

        float const bearing = std::atan2(slot.GetPositionY() - zone.GetPositionY(),
                                         slot.GetPositionX() - zone.GetPositionX());

        Position const stand = ValidateHodirFloorPoint(
            bot, Position(zone.GetPositionX() + std::cos(bearing) * ULDUAR_HODIR_STARLIGHT_STAND_RADIUS,
                          zone.GetPositionY() + std::sin(bearing) * ULDUAR_HODIR_STARLIGHT_STAND_RADIUS,
                          zone.GetPositionZ()));

        if (onFire && centre.GetExactDist2d(&stand) > fireLeash)
            continue;

        // Both ends of the caster band. A zone the bot cannot shoot the boss from is not a throughput
        // lever, whatever haste it carries.
        if (hodir)
        {
            float const gap = stand.GetExactDist2d(hodir);
            if (gap < ULDUAR_HODIR_RANGED_MIN_BOSS_GAP || gap > ULDUAR_HODIR_CASTER_MAX_BOSS_GAP)
                continue;
        }

        out = stand;
        bestWalk = walk;
        found = true;
    }

    return found;
}

bool GetHodirStarlightZoneAt(PlayerbotAI* /*botAI*/, Player* bot, Position& out)
{
    if (!bot)
        return false;

    std::vector<Position> const zones =
        GetDynamicObjectPositions(bot, ULDUAR_HODIR_STARLIGHT_SEARCH_RADIUS, SPELL_HODIR_STARLIGHT);

    bool found = false;
    float bestDist = 0.0f;
    for (Position const& zone : zones)
    {
        float const dist = bot->GetExactDist2d(&zone);
        if (dist > ULDUAR_HODIR_STARLIGHT_RADIUS)
            continue;

        if (!found || dist < bestDist)
        {
            out = zone;
            bestDist = dist;
            found = true;
        }
    }

    return found;
}

bool IsHodirIcicleLethal(Creature* icicle)
{
    if (!icicle || !icicle->IsAlive())
        return false;

    TempSummon* summon = icicle->ToTempSummon();
    uint32 const remaining = summon ? summon->GetTimer() : 0;

    // Both icicles are TEMPSUMMON_TIMED_DESPAWN, so GetTimer counts the 7000ms down. Anything else
    // leaves it parked at its start value and every icicle reads live, which is the safe way to be
    // wrong.
    return !remaining || remaining > ULDUAR_HODIR_ICICLE_SPENT_MS;
}

static bool DeriveHodirShuttleLeg(PlayerbotAI* botAI, Player* bot, Position& out, char const*& how)
{
    if (!bot)
        return false;

    bool const mainTank = botAI->IsMainTank(bot);
    if (mainTank || botAI->IsAssistTankOfIndex(bot, 0, true))
    {
        // Tanks shuttle between two fixed points instead of wandering, because Hodir follows. The
        // axis runs parallel to the SW bevel, so neither end walks him toward the chamfer, and 6 yd
        // apart is both long enough to cover two aura ticks and short enough to keep him cornered.
        Position const& spot = mainTank ? ULDUAR_HODIR_MAINTANK_SPOT : ULDUAR_HODIR_OFFTANK_SPOT;
        float const dx = std::cos(ULDUAR_HODIR_SHUTTLE_BEARING) * ULDUAR_HODIR_SHUTTLE_HALF_LEG;
        float const dy = std::sin(ULDUAR_HODIR_SHUTTLE_BEARING) * ULDUAR_HODIR_SHUTTLE_HALF_LEG;

        Position const legA(spot.GetPositionX() + dx, spot.GetPositionY() + dy, spot.GetPositionZ());
        Position const legB(spot.GetPositionX() - dx, spot.GetPositionY() - dy, spot.GetPositionZ());

        // Whichever end is further away, so the leg is always the full 6 yd and IsDuplicateMove
        // cannot refuse it for repeating the last destination.
        out = bot->GetExactDist2d(&legA) > bot->GetExactDist2d(&legB) ? legA : legB;
        how = "tank";
        return true;
    }

    // A bot holding Starlight sheds across the zone rather than out of it. Same shape as the tank
    // shuttle and for the same reason: two opposite points through a centre, taking whichever end is
    // further so the leg is always the full length and IsDuplicateMove cannot refuse it for repeating
    // the last destination. Both ends sit inside the zone, so the aura survives the shuttle - which is
    // the whole point, because the shed outranks the anchor and would otherwise walk the bot out of
    // the biggest throughput buff in the fight to save 800 a tick.
    Position zone;
    if (GetHodirStarlightZoneAt(botAI, bot, zone))
    {
        float const bearing = std::atan2(bot->GetPositionY() - zone.GetPositionY(),
                                         bot->GetPositionX() - zone.GetPositionX());
        float const dx = std::cos(bearing) * ULDUAR_HODIR_STARLIGHT_SHED_RADIUS;
        float const dy = std::sin(bearing) * ULDUAR_HODIR_STARLIGHT_SHED_RADIUS;

        Position const legA = ValidateHodirFloorPoint(
            bot, Position(zone.GetPositionX() + dx, zone.GetPositionY() + dy, zone.GetPositionZ()));
        Position const legB = ValidateHodirFloorPoint(
            bot, Position(zone.GetPositionX() - dx, zone.GetPositionY() - dy, zone.GetPositionZ()));

        out = bot->GetExactDist2d(&legA) > bot->GetExactDist2d(&legB) ? legA : legB;
        how = "starlight";
        return true;
    }

    // Everyone else steps to the nearest point clear of the rest of the raid. distanceStep is the
    // leg length, not a probe granularity: the helper rings outward from it, so 6 yd is the shortest
    // move it can return and a shorter one would not span two aura ticks.
    std::vector<Position> crowd;
    for (auto const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest friendly players")->Get())
    {
        Unit* ally = botAI->GetUnit(guid);
        if (ally && ally->IsAlive() && ally != bot && bot->GetExactDist2d(ally) <= ULDUAR_HODIR_DODGE_LEASH)
            crowd.push_back(ally->GetPosition());
    }

    if (crowd.empty())
    {
        float const bearing = static_cast<float>(bot->GetGUID().GetCounter() % 8) * static_cast<float>(M_PI) / 4.0f;
        out = Position(bot->GetPositionX() + std::cos(bearing) * 2.0f * ULDUAR_HODIR_SHUTTLE_HALF_LEG,
                       bot->GetPositionY() + std::sin(bearing) * 2.0f * ULDUAR_HODIR_SHUTTLE_HALF_LEG,
                       bot->GetPositionZ());
        how = "solo";
        return true;
    }

    Position leg = FindNearestPositionClearOfHazards(bot, crowd, ULDUAR_HODIR_DECLUMP_RADIUS,
                                                     ULDUAR_HODIR_DODGE_LEASH,
                                                     2.0f * ULDUAR_HODIR_SHUTTLE_HALF_LEG);
    if (!leg.GetPositionX() && !leg.GetPositionY())
        leg = FindNearestPositionClearOfHazards(bot, crowd, ULDUAR_HODIR_SHUTTLE_HALF_LEG,
                                                ULDUAR_HODIR_DODGE_LEASH,
                                                2.0f * ULDUAR_HODIR_SHUTTLE_HALF_LEG);

    if (!leg.GetPositionX() && !leg.GetPositionY())
        return false;

    out = leg;
    how = "crowd";
    return true;
}

bool GetHodirShuttleLeg(PlayerbotAI* botAI, Player* bot, Position& out)
{
    char const* how = "none";
    bool const found = DeriveHodirShuttleLeg(botAI, bot, out, how);

    // The rule, not the leg. The leg is already a move record with this action's name on it, and a
    // crowd-derived one moves every tick, so latching the coordinate emitted on every tick too.
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.shuttle", how);

    return found;
}

static bool DeriveHodirAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance)
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

    // Derived once and passed down. The slot and the Starlight step both need it, and every call
    // sweeps the grid for the fire and writes a note.
    bool onFire = false;
    Position const centre = GetHodirRingCentre(botAI, bot, &onFire);
    if (!GetHodirRingSlot(botAI, bot, centre, out))
        return false;

    tolerance = ULDUAR_HODIR_RING_SPOT_TOLERANCE;

    // The step is here rather than in the position trigger so one place decides where the bot stands.
    // A trigger that fired on "no Starlight" while the action still walked to the slot would move the
    // bot forever without ever arriving, which is what the old raid-wide constraint did.
    //
    // Deliberately not gated on already holding the aura: that would hand the bot back its ring slot
    // the moment the buff landed, walk it out of the zone, and start the whole trip again. The anchor
    // stays on the zone until the zone expires.
    Position stand;
    if (FindHodirStarlightStand(botAI, bot, centre, onFire, out, stand))
    {
        out = stand;
        tolerance = ULDUAR_HODIR_STARLIGHT_STAND_TOLERANCE;
    }

    return true;
}

bool GetHodirAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance)
{
    bool const found = DeriveHodirAnchor(botAI, bot, out, tolerance);

    if (RaidObs::Active())
    {
        // Melee have no anchor by design, and "none" is the answer that says so - without it a melee
        // bot standing in the corner is indistinguishable from one that never got told where to go.
        std::string value = "none";
        if (found)
        {
            char suffix[16];
            snprintf(suffix, sizeof(suffix), " ~%.1f", tolerance);
            value = RaidObs::DescribeDerived(out) + suffix;
        }

        RaidObs::NoteDerived(bot, "hodir.anchor", value);
    }

    return found;
}

Player* GetHodirResistancePaladin(PlayerbotAI* /*botAI*/, Player* bot)
{
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group)
        return nullptr;

    static uint32 const ranks[] = {SPELL_FROST_RESISTANCE_AURA_RANK_5, SPELL_FROST_RESISTANCE_AURA_RANK_4,
                                   SPELL_FROST_RESISTANCE_AURA_RANK_3, SPELL_FROST_RESISTANCE_AURA_RANK_2,
                                   SPELL_FROST_RESISTANCE_AURA_RANK_1};

    // A paladin tank first, then any non-healer, then whoever is left. The aura reaches 40 yd (48945,
    // radius index 23) and the two people it has to cover are the ones eating Frozen Blows melee -
    // 63511 is 39999 base and lands a median 23691 after resists, against a 34-45k tank pool. A tank
    // never leaves the corner, so it holds them at 100% against 82% for a retribution paladin who runs
    // the dodge and the shelter; every tank killing blow in 603_3_hodir_1787590072 landed with the aura
    // off and the retribution paladin 44-60 yd away, two of them resisting nothing at all.
    //
    // The raid loses about ten points of coverage for it, which is the right trade: a resist point on
    // the tank is ~6000 off a swing that kills, and on the raid ~700 off a tick the healers cover.
    Player* tank = nullptr;
    Player* dps = nullptr;
    Player* healer = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->getClass() != CLASS_PALADIN ||
            member->GetMapId() != bot->GetMapId())
            continue;

        bool knows = false;
        for (uint32 rank : ranks)
            if (member->HasActiveSpell(rank))
            {
                knows = true;
                break;
            }

        if (!knows)
            continue;

        if (PlayerbotAI::IsTank(member))
        {
            if (!tank)
                tank = member;
        }
        else if (PlayerbotAI::IsHeal(member))
        {
            if (!healer)
                healer = member;
        }
        else if (!dps)
        {
            dps = member;
        }
    }

    if (tank)
        return tank;

    return dps ? dps : healer;
}

bool IsHodirTrappedAllyBreaker(PlayerbotAI* botAI, Player* bot, Unit* block)
{
    if (!block)
        return false;

    bool breaker = false;
    Group* group = bot->GetGroup();
    if (!group)
        breaker = true;
    else
    {
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

        // Guid, not distance. Distances change every tick, so a distance rank re-shuffles the set
        // constantly and bots drop off the block and back onto the boss between one tick and the next.
        std::sort(candidates.begin(), candidates.end(),
                  [](Player* left, Player* right) { return left->GetGUID() < right->GetGUID(); });

        // Start the window at an offset derived from the block, so blocks that are up together draw
        // disjoint breaker sets. Taking the first five every time would put the same five bots on all
        // of them, and one Flash Freeze puts up a block per helper.
        size_t const total = candidates.size();
        size_t const offset = total ? block->GetGUID().GetCounter() % total : 0;

        for (size_t i = 0; i < total && i < ULDUAR_HODIR_TRAPPED_ALLY_BREAKERS; ++i)
            if (candidates[(offset + i) % total] == bot)
            {
                breaker = true;
                break;
            }
    }

    // Deliberately unprobed. Every block in range is asked about and a bot can come back a breaker for
    // several of them in one pass, so a note per true answer just flaps between blocks - and the one it
    // actually goes for is already in hodir.dpstarget.
    return breaker;
}

Unit* GetHodirAssignedHelperBlock(PlayerbotAI* botAI, Player* bot)
{
    Unit* boss = GetHodir(botAI);
    if (!bot || !boss)
        return nullptr;

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    // Ranged carry this. A melee that leaves the boss for a block makes a 21 yd round trip and gives up
    // its whole uptime for it, while a ranged bot can shoot one from nearer where it already stands.
    // Falls back to the rest when the raid has no ranged at all, or nobody would be freed.
    std::vector<Player*> ranged;
    std::vector<Player*> rest;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || memberAI->IsHeal(member) || memberAI->IsTank(member))
            continue;

        if (PlayerbotAI::IsRanged(member))
            ranged.push_back(member);
        else
            rest.push_back(member);
    }

    std::vector<Player*>& candidates = ranged.empty() ? rest : ranged;

    // Before the sweep, not after: the group walk above is free next to a grid pass, and on a raid with
    // ranged this drops every melee out before it costs one.
    if (std::find(candidates.begin(), candidates.end(), bot) == candidates.end())
        return nullptr;

    // Swept from the boss rather than from the bot, and only as far as the room the fight happens in.
    // A bot-centred sweep hands every bot a different block set and so a different assignment, and the
    // raid then disagrees about who owns what.
    std::list<Creature*> found;
    boss->GetCreatureListWithEntryInGrid(found, NPC_HODIR_FLASH_FREEZE_BLOCK, ULDUAR_HODIR_TRAPPED_ALLY_RANGE);

    std::vector<Creature*> blocks;
    for (Creature* candidate : found)
        if (candidate && candidate->IsAlive())
            blocks.push_back(candidate);

    if (blocks.empty())
        return nullptr;

    // Guid, not live distance, on both lists. Distances to the bot change every tick, so ranking on
    // them re-shuffles the assignment constantly and bots drop off a block and back onto the boss
    // between one tick and the next. Sorted, both lists read the same to every bot, which is what lets
    // the greedy pass below agree across the raid without any shared state.
    std::sort(candidates.begin(), candidates.end(),
              [](Player* left, Player* right) { return left->GetGUID() < right->GetGUID(); });
    std::sort(blocks.begin(), blocks.end(),
              [](Creature* left, Creature* right) { return left->GetGUID() < right->GetGUID(); });

    size_t const total = candidates.size();
    size_t budget = std::min<size_t>(ULDUAR_HODIR_HELPER_BLOCK_BREAKERS, blocks.size());
    if (total > ULDUAR_HODIR_HELPER_BLOCK_MIN_FREE)
        budget = std::min(budget, total - ULDUAR_HODIR_HELPER_BLOCK_MIN_FREE);
    else
        budget = std::min<size_t>(budget, 1);

    // Rank on the formation slot, not on where the bot happens to be. The slot comes out of the same
    // guid-sorted roster and a fixed centre, so it reads identically on every bot and holds still
    // between ticks - the stability the guid rotation was buying - while still handing each block to
    // the bot that stands nearest it. Bots were spending 61.6% of their time on ice walking to it.
    //
    // The anchor rather than the adopted centre: this is a ranking key, not a spot anyone walks to,
    // and asking for the real centre would cost a second fire sweep every tick for nothing.
    std::vector<Player*> ringMembers;
    BuildHodirRingMembers(bot, ringMembers);

    std::vector<Position> spots(total);
    for (size_t i = 0; i < total; ++i)
    {
        size_t slot = ringMembers.size();
        for (size_t r = 0; r < ringMembers.size(); ++r)
            if (ringMembers[r] == candidates[i])
                slot = r;

        // Only the melee fallback lands here, and it has no slot to rank from. Its own position is
        // still the same number on every bot that reads it, so the raid keeps agreeing.
        spots[i] = slot < ringMembers.size()
                       ? HodirRingSlotPoint(ULDUAR_HODIR_RAID_ANCHOR, slot, ringMembers.size())
                       : candidates[i]->GetPosition();
    }

    // One breaker each, in block-guid order, every block taking the nearest slot still free.
    std::vector<bool> taken(total, false);
    for (size_t i = 0; i < budget; ++i)
    {
        size_t best = total;
        float bestDist = 0.0f;
        for (size_t j = 0; j < total; ++j)
        {
            if (taken[j])
                continue;

            float const dist = blocks[i]->GetExactDist2d(&spots[j]);
            if (best == total || dist < bestDist)
            {
                best = j;
                bestDist = dist;
            }
        }

        if (best == total)
            break;

        taken[best] = true;
        if (candidates[best] == bot)
            return blocks[i];
    }

    return nullptr;
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

Position GetFreyaLasherCorral(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Unit* freya = GetFirstAliveUnitByEntry(botAI, NPC_FREYA);
    Creature* creature = freya ? freya->ToCreature() : nullptr;
    if (!creature)
        return Position();

    // Home position, not the live one. Freya never walks, but she pivots to face her tank, and reading
    // GetOrientation() would swing the corral around the room every time the tank stepped.
    Position const& home = creature->GetHomePosition();
    float const angle = home.GetOrientation() + static_cast<float>(M_PI);

    float x = home.GetPositionX() + ULDUAR_FREYA_LASHER_CORRAL_DISTANCE * std::cos(angle);
    float y = home.GetPositionY() + ULDUAR_FREYA_LASHER_CORRAL_DISTANCE * std::sin(angle);
    float z = home.GetPositionZ();

    // Height only, no collision raycast. Several triggers read this every tick for every bot, and the
    // Conservatory floor is open: all 16 headings of the 35 yd ring around her spawn probe on-mesh, so
    // the raycast would cost a path generation per bot per tick to confirm what the terrain already is.
    bot->UpdateAllowedPositionZ(x, y, z);

    return Position(x, y, z);
}

Position GetFreyaLasherTrapPost(PlayerbotAI* botAI)
{
    Position const corral = GetFreyaLasherCorral(botAI);
    if (corral == Position())
        return Position();

    Unit* freya = GetFirstAliveUnitByEntry(botAI, NPC_FREYA);
    Creature* creature = freya ? freya->ToCreature() : nullptr;
    if (!creature)
        return Position();

    Player* bot = botAI->GetBot();
    Position const& home = creature->GetHomePosition();
    float const angle = corral.GetAngle(&home);

    float x = corral.GetPositionX() + ULDUAR_FREYA_LASHER_TRAP_OFFSET * std::cos(angle);
    float y = corral.GetPositionY() + ULDUAR_FREYA_LASHER_TRAP_OFFSET * std::sin(angle);
    float z = corral.GetPositionZ();
    bot->UpdateAllowedPositionZ(x, y, z);

    return Position(x, y, z);
}

uint32 CountFreyaLashersNear(Position const& centre, FreyaWaveState const& state, float radius)
{
    uint32 count = 0;
    for (Unit* lasher : state.detonatingLashers)
    {
        if (lasher && lasher->IsAlive() && centre.GetExactDist2d(lasher->GetPosition()) <= radius)
            ++count;
    }

    return count;
}

Unit* GetFreyaLasherChasing(Player* bot, FreyaWaveState const& state)
{
    for (Unit* lasher : state.detonatingLashers)
    {
        if (lasher && lasher->IsAlive() && lasher->GetVictim() == bot)
            return lasher;
    }

    return nullptr;
}

bool IsFreyaLasherTrapHunter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_HUNTER)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->getClass() != CLASS_HUNTER)
            continue;

        if (!GET_PLAYERBOT_AI(member) || member->GetMapId() != bot->GetMapId())
            continue;

        if (member->GetGUID() < bot->GetGUID())
            return false;
    }

    return true;
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

bool IsXT002PummellerTank(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI->IsTank(bot))
        return false;

    // Whoever holds XT keeps holding him, so the add belongs to the first assist tank and only falls
    // to the main tank when there is no second tank left.
    if (Player* assistTank = GetGroupAssistTank(botAI, bot, 0))
        return assistTank == bot;

    if (Player* mainTank = GetGroupMainTank(botAI, bot))
        return mainTank == bot;

    // Neither resolves only when this bot is the last tank standing, so it owns the add by default.
    return true;
}

bool IsXT002AddEngageable(PlayerbotAI* botAI, Unit* unit)
{
    if (!unit)
        return false;

    Unit* xt002 = GetXT002(botAI);
    if (!xt002)
        return true;

    return unit->GetExactDist2d(xt002) <= ULDUAR_XT002_ADD_LEASH_RADIUS;
}

Unit* GetXT002EngageableAdd(PlayerbotAI* botAI, Player* bot, uint32 entry, float botReach)
{
    Unit* nearest = nullptr;
    float nearestDistance = 0.0f;

    GuidVector const& npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get();
    for (ObjectGuid const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != entry)
            continue;

        if (!IsXT002AddEngageable(botAI, unit))
            continue;

        float const distance = unit->GetExactDist2d(bot);
        if (distance > botReach)
            continue;

        if (!nearest || distance < nearestDistance)
        {
            nearest = unit;
            nearestDistance = distance;
        }
    }

    return nearest;
}

// Ignis the Furnace Master

// Construct each assist tank has committed to, so one Ignis activates nearer to him mid-walk cannot
// steal the kite. Cleared once that construct turns Brittle or dies. Keyed by instance first: the
// same tank GUID comes back on a re-pull and in a second raid running the fight concurrently.
static std::unordered_map<uint32, std::unordered_map<ObjectGuid, ObjectGuid>> ignisTankDrivenConstructGuid;

Unit* GetIgnis(PlayerbotAI* botAI)
{
    return botAI->GetBot()->FindNearestCreature(NPC_IGNIS, ULDUAR_IGNIS_ROOM_SEARCH_RADIUS, true);
}

bool IsIgnisEngaged(PlayerbotAI* botAI)
{
    Unit* boss = GetIgnis(botAI);

    return boss && boss->IsAlive() && boss->IsInCombat();
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
static Unit* GetNearestIgnisConstructMatching(WorldObject const* from,
                                              std::function<bool(Unit const*)> const& predicate)
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
    std::list<Creature*> constructs;
    botAI->GetBot()->GetCreatureListWithEntryInGrid(constructs, NPC_IGNIS_IRON_CONSTRUCT,
                                                    ULDUAR_IGNIS_ROOM_SEARCH_RADIUS);

    // Lowest GUID rather than nearest. Two constructs can be Brittle at once, and a raid split
    // between them wastes the 15 s window on both - GUID order is the same everywhere, so every bot
    // lands on the same one with nothing to coordinate through.
    Unit* best = nullptr;
    for (Creature* construct : constructs)
    {
        if (!IsIgnisConstructActivated(construct) || !IsIgnisConstructBrittle(construct))
            continue;

        if (!best || construct->GetGUID() < best->GetGUID())
            best = construct;
    }

    return best;
}

Unit* GetIgnisNearestMoltenConstruct(PlayerbotAI* /*botAI*/, WorldObject const* from)
{
    return GetNearestIgnisConstructMatching(from, &IsIgnisConstructMolten);
}

Unit* GetIgnisDrivenConstruct(PlayerbotAI* botAI, Player* tank)
{
    if (!tank)
        return nullptr;

    auto& driven = ignisTankDrivenConstructGuid[tank->GetInstanceId()];

    ObjectGuid const tankGuid = tank->GetGUID();
    auto const held = driven.find(tankGuid);
    if (held != driven.end())
    {
        Unit* construct = botAI->GetUnit(held->second);

        // Molten wiped this construct's threat table, so handing it to a closer new one would release
        // it into the raid. Only Brittle (job done) or death lets the tank move on.
        if (IsIgnisConstructActivated(construct) && !IsIgnisConstructBrittle(construct))
            return construct;

        driven.erase(held);
    }

    Unit* construct = GetNearestIgnisConstructMatching(tank, [&driven, &tankGuid](Unit const* candidate)
    {
        if (IsIgnisConstructBrittle(candidate))
            return false;

        // Whatever the other tank already holds is off limits for the same reason: its threat table
        // is gone, so a tank swapping onto it hands it to the raid rather than to a tank.
        for (auto const& entry : driven)
            if (entry.first != tankGuid && entry.second == candidate->GetGUID())
                return false;

        return true;
    });

    if (!construct)
        return nullptr;

    driven[tankGuid] = construct->GetGUID();

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

Unit* GetIgnisAssignedScorchedGround(PlayerbotAI* /*botAI*/, WorldObject const* from, int8 tankIndex)
{
    if (!from)
        return nullptr;

    std::list<Creature*> patches;
    from->GetCreatureListWithEntryInGrid(patches, NPC_IGNIS_SCORCHED_GROUND, ULDUAR_IGNIS_ROOM_SEARCH_RADIUS);

    Position const& assigned = GetIgnisAssignedWaterPool(tankIndex);

    Unit* best = nullptr;
    float bestDistance = 0.0f;

    for (Creature* patch : patches)
    {
        if (!patch->IsAlive())
            continue;

        Position const& pool = GetIgnisNearestWaterPool(patch);
        if (patch->GetExactDist2d(&pool) <= ULDUAR_IGNIS_SCORCHED_GROUND_INERT_WATER_RADIUS)
            continue;

        // Sorted towards this tank's own pool rather than towards the tank, so the two of them work
        // opposite ends of the patch fan and their constructs never end up in the same water.
        float const distance = patch->GetExactDist2d(&assigned);
        if (best && distance >= bestDistance)
            continue;

        best = patch;
        bestDistance = distance;
    }

    return best;
}

int8 GetIgnisConstructTankIndex(PlayerbotAI* botAI, Player* bot)
{
    if (GetGroupAssistTank(botAI, bot, 0) == bot)
        return 0;

    if (GetGroupAssistTank(botAI, bot, 1) == bot)
        return 1;

    return -1;
}

Position const& GetIgnisAssignedWaterPool(int8 tankIndex)
{
    return tankIndex == 1 ? ULDUAR_IGNIS_WATER_POOL_EAST : ULDUAR_IGNIS_WATER_POOL_WEST;
}

// Which of the three arc slots the main tank holds, and whether Scorch was already up last time we
// looked. Latched per instance so a wipe or a second raid does not inherit a stale rotation.
struct IgnisTankArcState
{
    uint8 slot = 0;
    bool scorchUp = false;
};

static thread_local std::unordered_map<uint32, IgnisTankArcState> _ignisTankArcStates;

Position GetIgnisMainTankPosition(PlayerbotAI* botAI, Player* bot)
{
    IgnisTankArcState& state = _ignisTankArcStates[bot->GetInstanceId()];

    // Rising edge, not "while up": the slot advances once per Scorch, at the start of the 3 s root.
    // Ignis cannot turn or follow during those seconds and the patch spawns from the orientation he
    // was frozen with, so the tank crosses to the next slot for free and is 17.3 yd clear when it
    // lands. Only the main tank may consume the edge - anyone else asking would eat the transition.
    if (botAI->IsMainTank(bot))
    {
        bool const scorchUp = IsIgnisScorchWindow(GetIgnis(botAI));
        if (scorchUp && !state.scorchUp)
            state.slot = (state.slot + 1) % ULDUAR_IGNIS_TANK_ARC_SLOTS;

        state.scorchUp = scorchUp;
    }

    float const angle = Position::NormalizeOrientation(
        ULDUAR_IGNIS_TANK_BEARING + (static_cast<float>(state.slot) - 1.0f) * ULDUAR_IGNIS_TANK_ARC_STEP);

    float x = ULDUAR_IGNIS_BOSS_ANCHOR.GetPositionX() + std::cos(angle) * ULDUAR_IGNIS_TANK_RADIUS;
    float y = ULDUAR_IGNIS_BOSS_ANCHOR.GetPositionY() + std::sin(angle) * ULDUAR_IGNIS_TANK_RADIUS;

    float z = bot->GetMapWaterOrGroundLevel(x, y, ULDUAR_IGNIS_BOSS_ANCHOR.GetPositionZ());
    if (z <= INVALID_HEIGHT)
        z = ULDUAR_IGNIS_BOSS_ANCHOR.GetPositionZ();

    bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                   bot->GetPositionZ(), x, y, z, false);

    return Position(x, y, z);
}

bool IsIgnisScorchWindow(Unit* boss) { return boss && boss->HasAura(SPELL_IGNIS_SCORCH); }

bool IsIgnisFlameJetsCasting(Unit* boss)
{
    if (!boss || !boss->HasUnitState(UNIT_STATE_CASTING))
        return false;

    Spell* spell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);

    return spell && spell->m_spellInfo->Id == SPELL_IGNIS_FLAME_JETS;
}

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

namespace
{
// Raid-wide answers, folded once per instance per tick rather than once per bot - and sharing the
// result is what stops two vehicles disagreeing about whose channel it is.
struct FlameLeviathanState
{
    // boss_flame_leviathan never sets IN_PROGRESS (only SPECIAL / NOT_STARTED / DONE) and the unit
    // it engages is a vehicle rather than a roster player, so neither RaidObs opener fires and this
    // fight has never left a trace.
    bool pullTraced = false;

    // Who has already fired into the channel now running. Cleared the moment he stops channelling,
    // so the next channel starts unclaimed - the 10 s gap between channels guarantees we see one.
    RaidObs::ObsValue<ObjectGuid> ventClaimedBy{"fl.interrupter"};

    // The vehicle currently wearing Pursued, and when it last changed. EVENT_PURSUE repeats on 31s,
    // so the switch is predictable and the fleet can be out of his front before he turns.
    RaidObs::ObsValue<ObjectGuid> pursuedVehicle{"fl.pursued"};
    uint32 pursueSeenMs = 0;

    uint32 scanMs = 0;
};

thread_local std::unordered_map<uint32 /*instanceId*/, FlameLeviathanState> flStates;

// Long enough that the scan is cheap, short enough that a 31s Pursued cycle is never missed.
constexpr uint32 ULDUAR_FL_SCAN_INTERVAL_MS = 200;

FlameLeviathanState& FlameLeviathanStateFor(Player* bot) { return flStates[bot->GetInstanceId()]; }

// Everything that has to be true once per instance per tick rather than once per bot: open the
// trace, expire a vent claim, and notice a Pursued switch.
void TickFlameLeviathan(PlayerbotAI* botAI, Player* bot, Unit* boss)
{
    FlameLeviathanState& state = FlameLeviathanStateFor(bot);
    if (state.scanMs && GetMSTimeDiffToNow(state.scanMs) < ULDUAR_FL_SCAN_INTERVAL_MS)
        return;

    state.scanMs = getMSTime();

    // Off the boss, never off the calling bot: one bot dropping combat is not the pull ending, and
    // without this reset a wipe would leave the latch set and the re-pull would open no trace.
    if (!boss || !boss->IsInCombat())
    {
        state.pullTraced = false;
        state.ventClaimedBy = ObjectGuid::Empty;
        state.pursuedVehicle = ObjectGuid::Empty;
        state.pursueSeenMs = 0;
        return;
    }

    if (!state.pullTraced)
    {
        state.pullTraced = true;
        RaidObs::MarkPull(bot->GetMap(), boss);
    }

    if (!FlameLeviathanIsVentChanneling(boss))
        state.ventClaimedBy = ObjectGuid::Empty;

    // Pursued is read off the vehicles rather than the players: the aura lands on whichever unit the
    // boss's spell picked, and a gunner's own guid never carries it.
    ObjectGuid pursued;
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* member = gref->GetSource();
            if (!member || !member->IsAlive())
                continue;

            if (Unit* base = FlameLeviathanRiddenVehicle(member))
                if (base->HasAura(SPELL_FL_PURSUED))
                {
                    pursued = base->GetGUID();
                    break;
                }
        }
    }

    if (pursued && pursued != state.pursuedVehicle.Get())
        state.pursueSeenMs = getMSTime();

    state.pursuedVehicle = pursued;
}
}  // namespace

bool FlameLeviathanEngaged(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return false;

    Unit* boss = FlameLeviathanBoss(botAI);

    // Ahead of the combat test, because the housekeeping it drives includes the wipe reset.
    TickFlameLeviathan(botAI, bot, boss);

    if (!bot->IsInCombat())
        return false;

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
//
// IsWithinCombatRange adds *both* combat reaches, and his is 15, so asking it for 25 yd answered yes
// out to 47.7 - roughly twice what the cone covers. The cone check adds only the target's reach
// (WorldObject::GetObjectSize), so that is what this mirrors.
static bool FlameLeviathanInConeRange(Unit* caster, Unit* target, float radius)
{
    return caster && target && caster->GetExactDist(target) <= radius + target->GetObjectSize();
}

static bool FlameLeviathanCanElectroshock(Unit* siegeEngine, Unit* boss)
{
    return siegeEngine && boss && !siegeEngine->HasSpellCooldown(SPELL_FL_ELECTROSHOCK) &&
           siegeEngine->GetPower(POWER_ENERGY) >= ULDUAR_FL_ELECTROSHOCK_COST &&
           FlameLeviathanInConeRange(siegeEngine, boss, ULDUAR_FL_ELECTROSHOCK_CONE_RADIUS);
}

bool FlameLeviathanFaceForCone(Unit* vehicleBase, Unit* target, float halfAngle, float radius)
{
    if (!vehicleBase || !target)
        return false;

    if (!FlameLeviathanInConeRange(vehicleBase, target, radius))
        return false;

    // HasInArc splits what it is handed, so the full cone width goes in.
    if (vehicleBase->HasInArc(halfAngle * 2.0f, target))
        return true;

    // Outside the cone but inside CAST_ANGLE_IN_FRONT, so CastVehicleSpell would not have turned and
    // the shot would have gone nowhere. Spend the tick turning and let a later one fire.
    vehicleBase->SetFacingToObject(target);
    return false;
}

bool FlameLeviathanIsVentInterrupter(PlayerbotAI* botAI, Player* bot)
{
    Unit* base = bot ? bot->GetVehicleBase() : nullptr;
    if (!base || base->GetEntry() != NPC_SALVAGED_SIEGE_ENGINE)
        return false;

    Unit* boss = FlameLeviathanBoss(botAI);
    if (!FlameLeviathanCanElectroshock(base, boss))
        return false;

    // One shot per channel. Without this the ranking below re-elects on every tick of the channel:
    // the winner spends 20 energy casting, which promotes whoever is now highest, and the whole line
    // of siege engines empties into a single channel milliseconds apart.
    ObjectGuid const claimed = FlameLeviathanStateFor(bot).ventClaimedBy.Get();
    if (claimed)
        return claimed == bot->GetGUID();

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
        // back of its own queue, so the duty rotates across channels with nobody having to be told.
        if (energy > myEnergy || (energy == myEnergy && member->GetGUID() < myGuid))
            return false;
    }

    return true;
}

void FlameLeviathanClaimVentChannel(Player* bot)
{
    if (bot)
        FlameLeviathanStateFor(bot).ventClaimedBy = bot->GetGUID();
}

uint32 FlameLeviathanMsSincePursue(Player* bot)
{
    if (!bot)
        return 0;

    uint32 const seen = FlameLeviathanStateFor(bot).pursueSeenMs;
    return seen ? GetMSTimeDiffToNow(seen) : 0;
}

bool FlameLeviathanPursueSwitchImminent(Player* bot)
{
    uint32 const since = FlameLeviathanMsSincePursue(bot);
    if (!since)
        return false;

    // Nothing seen for longer than a full cycle means the timing is lost - a re-pull, or a switch
    // the scan missed. Treat that as imminent rather than safe: being wrong the cautious way costs
    // a few yards, being wrong the other way costs the fleet a Battering Ram.
    if (since >= ULDUAR_FL_PURSUE_PERIOD_MS)
        return true;

    return since >= ULDUAR_FL_PURSUE_PERIOD_MS - ULDUAR_FL_PURSUE_CLEAR_LEAD_MS;
}

bool FlameLeviathanInBatteringRamArc(Unit* vehicleBase, Unit* boss)
{
    if (!vehicleBase || !boss)
        return false;

    if (vehicleBase->GetExactDist2d(boss) > ULDUAR_FL_BATTERING_RAM_RADIUS + vehicleBase->GetObjectSize())
        return false;

    // Measured from him outwards: the blast lands on a point in front of him, so what matters is
    // whether this vehicle is the thing he is facing.
    return boss->HasInArc(float(M_PI), vehicleBase);
}

bool FlameLeviathanShouldClearBatteringRam(PlayerbotAI* botAI, Player* bot)
{
    // Being in front of him is the pursued vehicle's whole job, and it is already kiting.
    if (!bot || FlameLeviathanIsPursued(bot))
        return false;

    // The ridden vehicle, not the seat: a gunner's GetVehicleBase is the bolted-on turret, whose
    // position is the parent's anyway but whose object size is not.
    Unit* vehicleBase = FlameLeviathanRiddenVehicle(bot);
    Unit* boss = FlameLeviathanBoss(botAI);
    if (!vehicleBase || !boss)
        return false;

    if (vehicleBase->GetExactDist2d(boss) > ULDUAR_FL_BATTERING_RAM_RADIUS + vehicleBase->GetObjectSize())
        return false;

    return FlameLeviathanInBatteringRamArc(vehicleBase, boss) || FlameLeviathanPursueSwitchImminent(bot);
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

bool IsMimironEngaged(PlayerbotAI* botAI)
{
    // Any construct, because each phase hands over to the next: the outgoing one goes passive and
    // unselectable while the incoming one calls SetInCombatWithZone, so between the two there is
    // nothing worth targeting anyway.
    for (uint32 entry : {NPC_LEVIATHAN_MKII, NPC_VX001, NPC_AERIAL_COMMAND_UNIT})
    {
        Unit* construct = GetFirstAliveUnitByEntry(botAI, entry);
        if (construct && construct->IsInCombat())
            return true;
    }

    return false;
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
