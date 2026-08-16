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
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <list>
#include <unordered_map>
#include <utility>

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
const Position ULDUAR_MIMIRON_PHASE4_TANK_SPOT = Position(2744.5754f, 2570.8657f, 364.3138f);
const Position ULDUAR_VEZAX_MARK_OF_THE_FACELESS_SPOT = Position(1913.6501f, 122.93989f, 342.38083f);
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

Player* GetAlgalonBigBangSoakerPriest(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != ULDUAR_MAP_ID)
            continue;

        if (member->getClass() != CLASS_PRIEST)
            continue;

        if (AiFactory::GetPlayerSpecTab(member) != PRIEST_TAB_SHADOW)
            continue;

        return member;
    }

    return nullptr;
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

Position GetAuriayaRaidCentroid(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return Position(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    float sumX = 0.0f;
    float sumY = 0.0f;
    uint32 count = 0;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !member->IsInWorld() || member->GetMapId() != ULDUAR_MAP_ID)
            continue;

        if (PlayerbotAI::IsMainTank(member) || PlayerbotAI::IsAssistTankOfIndex(member, 0, true))
            continue;

        sumX += member->GetPositionX();
        sumY += member->GetPositionY();
        ++count;
    }

    if (!count)
        return Position(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    return Position(sumX / count, sumY / count, bot->GetPositionZ());
}

bool GetAuriayaFacingError(PlayerbotAI* botAI, Player* bot, float& error)
{
    Unit* boss = GetAuriaya(botAI);
    if (!boss)
        return false;

    Position const centroid = GetAuriayaRaidCentroid(bot);
    if (boss->GetExactDist2d(centroid.GetPositionX(), centroid.GetPositionY()) <
        ULDUAR_AURIAYA_FACING_MIN_RAID_DIST)
    {
        return false;
    }

    // Auriaya faces her victim, so the tank steers her by standing on the bearing that points the
    // cone away from everyone else - that is the bearing running from the raid through the boss.
    float const desired = std::atan2(boss->GetPositionY() - centroid.GetPositionY(),
                                     boss->GetPositionX() - centroid.GetPositionX());

    float diff = Position::NormalizeOrientation(boss->GetOrientation() - desired);
    if (diff > M_PI)
        diff -= 2.0f * static_cast<float>(M_PI);

    error = diff;
    return true;
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

Unit* GetFreyaTankTarget(PlayerbotAI* botAI, FreyaWaveState const& state)
{
    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsAssistTankOfIndex(bot, 0, true))
        return nullptr;

    if (state.snaplasher && state.snaplasher->IsAlive())
        return state.snaplasher;

    if (state.conservator && state.conservator->IsAlive())
        return state.conservator;

    return nullptr;
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

float GetMimironBarrageAngle(Player* bot, Unit* vx001)
{
    if (!bot || !vx001)
        return 0.0f;

    if (Creature* dbTarget = bot->FindNearestCreature(NPC_MIMIRON_DB_TARGET, 250.0f))
        return vx001->GetAngle(dbTarget);

    return vx001->GetOrientation();
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

MimironBarrageArc const& GetMimironLatchedBarrageArc(Player* bot, Unit* vx001)
{
    static std::unordered_map<uint32, MimironBarrageArc> latched;
    static MimironBarrageArc const noArc;

    if (!bot || !vx001)
        return noArc;

    time_t const now = std::time(nullptr);
    MimironBarrageArc& arc = latched[bot->GetMap()->GetInstanceId()];

    bool const expired = arc.latchedAt == 0 || now - arc.latchedAt > ULDUAR_MIMIRON_BARRAGE_LATCH_TTL;
    bool const moved = arc.origin.GetExactDist2d(vx001->GetPositionX(), vx001->GetPositionY()) >=
                       ULDUAR_MIMIRON_BARRAGE_RELATCH_DIST;

    if (expired || moved)
    {
        arc.angle = GetMimironBarrageAngle(bot, vx001);
        arc.origin = vx001->GetPosition();
        arc.latchedAt = now;
    }

    return arc;
}

bool GetMimironSpreadSlot(PlayerbotAI* botAI, Player* bot, Position& out)
{
    if (!botAI || !bot)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // The main tank holds the chassis spot once all three mechs are up. Walking it anywhere else in
    // phase 4 drags VX-001 with it, and VX-001 is what the Laser Barrage cone radiates from.
    if (PlayerbotAI::IsMainTank(bot))
    {
        if (!GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) ||
            !GetFirstAliveUnitByEntry(botAI, NPC_VX001) ||
            !GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
            return false;

        out = ULDUAR_MIMIRON_PHASE4_TANK_SPOT;
        return true;
    }

    // Melee stand on whatever they are hitting, so only ranged and healers get a slot.
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

    float const angle = 2.0f * static_cast<float>(M_PI) * index / count;
    out = Position(ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX() + ULDUAR_MIMIRON_SPREAD_RADIUS * cos(angle),
                   ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY() + ULDUAR_MIMIRON_SPREAD_RADIUS * sin(angle),
                   ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());
    return true;
}
