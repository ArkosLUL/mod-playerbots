/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Sindragosa.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "GroupReference.h"
#include "ICCData.h"
#include "ICCHelpers_Shared.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidDifficulty.h"
#include "RaidDirectTargeting.h"
#include "RaidHumanTankCall.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidResistAura.h"
#include "RaidTankDefensive.h"
#include "RtiTargetValue.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "Timer.h"

using namespace EncounterHelpers;

namespace
{
constexpr float GATE_RANGE = 120.0f;
constexpr float SCAN_RANGE = 150.0f;
constexpr uint32 SCAN_MS = 200;
constexpr uint32 DUTY_MS = 250;
constexpr uint32 HUMAN_CALL_CHECK_MS = 500;

// Within this of the far station she is over one of the two
constexpr float STATIONS_RADIUS = 50.0f;
constexpr size_t BOMBS_PER_AIR_PHASE = 4;

// A walk parks within PARK and walks back once pushed past UNPARK. Her live spot, which the stands hang off,
// re-latches once she has drifted ANCHOR_DRIFT off it.
constexpr float PARK = 1.0f;
constexpr float UNPARK = 2.5f;
constexpr float ANCHOR_DRIFT = 3.0f;
static_assert(PARK < UNPARK, "a walk has to end inside its own trigger");
// A floor check that pulls a spot back further than this turns it down
constexpr float FLOOR_TOLERANCE = 1.5f;

// Blistering Cold reaches 25 yd from her centre: a creature's area spell adds no target size
constexpr float COLD_RANGE = 29.5f;

constexpr uint32 UNCHAINED_HOLD_STACKS = 3;
// Backlash's 20 yd plus a margin
constexpr float BACKLASH_KEEP = 23.0f;
// A carrier's node keeps Backlash off the raid's spots and off every other carrier with both parked a yard or two
// out, and stays outside Blistering Cold
constexpr float UNCHAINED_RAID_KEEP = 24.0f;
constexpr float UNCHAINED_CARRIER_APART = 25.0f;
constexpr float UNCHAINED_HER_KEEP = 28.0f;
// A 30 yd spell reaches her from here: both combat reaches count
constexpr float UNCHAINED_CAST_REACH = 46.0f;
// Heroic Frost Breath is a 24 degree cone out to 60 yd, and Tail Smash hits 20 yd round a point 20 yd behind her
constexpr float BREATH_REACH = 62.0f;
constexpr float BREATH_KEEP_DEG = 22.0f;
constexpr float TAIL_KEEP = 23.0f;
// navprobe's default collision height, the eye of a sight ray
constexpr float SIGHT_EYE = 2.0f;
constexpr uint32 CHILLED_STOP_STACKS = 6;

constexpr float BRAKE_CAP = 0.9f;
constexpr float BRAKE_RELEASE = 0.8f;

// Ice Tomb hits everyone within 10 yd of the beacon
constexpr float SPLASH_TRIGGER = 12.0f;
constexpr float SPLASH_CLEAR = 13.5f;
constexpr float SPLASH_SWEEP = 25.0f;
constexpr uint32 SPLASH_RETRY_MS = 1000;

constexpr float BOMB_HIDE_BEHIND = 3.5f;
// The script drops a marker at least 3 yd off every ice block
constexpr float BOMB_TOMB_MIN = 3.0f;
constexpr float BUFFET_HIDE_BEHIND = 4.0f;
constexpr uint32 BUFFET_HIDE_STACKS = 3;
// The free tank resets from one stack, so it can take the swap at SWAP_BUFFET_MAX or under
constexpr uint32 FREE_TANK_HIDE_STACKS = 1;
// Heals pick targets out to 38.5 yd
constexpr float HEAL_REACH = 38.0f;
// A live tomb this close to a last phase tomb spot fouls it
constexpr float TOMB_SPOT_FOUL = 6.0f;
// A Blistering Cold run past a tomb this close paths round it
constexpr float ESCAPE_TOMB_KEEP = 8.0f;
constexpr float PRISONER_MATCH = 3.0f;

// 20% of a 25-man tomb is 90k, enough for damage still in flight. A 10-man tomb has 113k, so it stops at 45k.
constexpr float TOMB_FLOOR_PCT_25 = 20.0f;
constexpr float TOMB_FLOOR_PCT_10 = 40.0f;
constexpr float PRISONER_LOW_PCT = 40.0f;

constexpr uint32 SWAP_WALK_MS = 12000;
constexpr uint32 SWAP_TAUNT_MS = 16000;
// A taunt lands at once: still not her victim this long after it, the swapper missed
constexpr uint32 SWAP_MISS_MS = 1000;
constexpr uint32 SWAP_BUFFET_MAX = 1;
constexpr float TAUNT_RANGE = 30.0f;

// Her next breath comes 20-25 s after the last, and every external below lasts 8 s or more
constexpr uint32 COVER_FROM_MS = 18000;
constexpr uint32 COVER_BUFFET_STACKS = 5;
constexpr float COVER_RANGE = 40.0f;
constexpr uint32 SPELL_SG_PAIN_SUPPRESSION = 33206;
constexpr uint32 SPELL_SG_GUARDIAN_SPIRIT = 47788;
constexpr uint32 SPELL_SG_HAND_OF_SACRIFICE = 6940;

constexpr uint32 FREEDOM_AIR_MS = 6000;
constexpr uint32 FREEDOM_HOLDER_STACKS = 3;
constexpr float FREEDOM_RANGE = 30.0f;

constexpr uint32 DEFENSIVE_BUFFET_STACKS = 3;
constexpr float HOLDER_LOW_PCT = 50.0f;
constexpr float TANK_LOW_PCT = 40.0f;
constexpr float TANK_LAST_RESORT_PCT = 25.0f;

constexpr uint32 SPELL_WEAKENED_SOUL = 6788;

// Stands are polar from her live spot in the room frame: degrees from +x toward +y, then yards
struct Polar
{
    float deg;
    float r;
};

// The holder 20 yd west, the raid on her south flank toward the stairs, all clear of the 20 yd Tail Smash
// circle 20 yd behind her and of Frost Breath's cone toward the holder
constexpr Polar STAND_HOLDER{90.0f, 20.0f};
constexpr Polar COLD_HOLDER{90.0f, 30.0f};
constexpr Polar STAND_MELEE{170.0f, 14.0f};
constexpr Polar STAND_RANGED{180.0f, 30.0f};
constexpr Polar COLD_ESCAPES[] = {{150.0f, 30.0f}, {180.0f, 30.0f}, {210.0f, 30.0f}};
// Last phase tombs, alternating, outside her 21.6 yd melee and 25 yd Blistering Cold
constexpr Polar TOMB_SPOTS[] = {{140.0f, 31.0f}, {215.0f, 31.0f}};
constexpr uint8 BEACON_SPOT_A = 7;
constexpr Polar TAIL_SMASH{270.0f, 20.0f};

// Heroic Unchained Magic nodes: a 5 yd grid round her landing spot, row i at x + 5 (i + UNCHAINED_GRID_I0), bit j
// at y + 5 (j + UNCHAINED_GRID_J0). navprobe --nav 0x09 settles every set node within 0.01 of 203.38, with static
// sight to her at her landing spot and 15 and 30 yd west of it. A carrier picks its node live, since she drifts.
constexpr int UNCHAINED_GRID_I0 = -13;
constexpr int UNCHAINED_GRID_J0 = -11;
constexpr int UNCHAINED_GRID_COLS = 25;
constexpr float UNCHAINED_GRID_STEP = 5.0f;
constexpr float UNCHAINED_GRID_Z = 203.38f;
constexpr uint32 UNCHAINED_GRID[] = {
    0x01F80FC, 0x01FFFFC, 0x01FFFFC, 0x03FFFFE, 0x07FFFFF, 0x0FFFFFF, 0x1FFFFFF, 0x1FFFFFF, 0x1FFFFFF,
    0x0FFFFFF, 0x07FFFFF, 0x07FFFFF, 0x1FFFFFF, 0x1FFFFFF, 0x0FFFFFF, 0x07FFFFF, 0x003FFE0, 0x001FFC0,
    0x001FFE0, 0x001FFE0, 0x001FFE0, 0x001FFC0, 0x000FF80, 0x0007F00, 0x0001C00};
constexpr int UNCHAINED_GRID_ROWS = static_cast<int>(sizeof(UNCHAINED_GRID) / sizeof(UNCHAINED_GRID[0]));
constexpr uint8 HOLDER_SIGHT_TRIES = 8;

enum class TankRole : uint8
{
    None = 0,
    Holder = 1,
    Free = 2
};

enum class WalkKind : uint8
{
    None,
    Cold,
    Splash,
    BombHide,
    Beacon,
    BuffetHide,
    Unchained,
    Stand
};

enum class StandKind : uint8
{
    None,
    Tank,
    Melee,
    Ranged,
    Stage
};

struct TombInfo
{
    ObjectGuid guid;
    Position pos;
    float healthPct = 100.0f;
    // Prisoner: 0 tank, 1 healer, 2 anyone else
    uint8 tier = 2;
    // Asphyxiation or a prisoner under PRISONER_LOW_PCT. Under the bombs only Asphyxiation counts: the tomb
    // soaks 99% of the prisoner's frost damage, and its 16k hit alone puts casters under 40%.
    bool danger = false;
    bool asphyxiating = false;
    // A player's skull is on it, which overrides the hold
    bool skulled = false;
    // Formed in an air phase
    bool air = false;
    uint32 seenMs = 0;
};

struct MarkerInfo
{
    ObjectGuid guid;
    Position pos;
    uint32 seenMs = 0;
};

struct TombSeen
{
    uint32 ms = 0;
    bool air = false;
};

struct SgBotState
{
    WalkKind walk = WalkKind::None;
    Position walkSpot;
    bool walkParked = false;
    bool owned = false;
    // Its stand waits out a beacon, so a stand walk still in flight has to stop
    bool halt = false;

    bool cold = false;
    // Outside Blistering Cold while she casts it: nothing may walk the bot in
    bool coldHold = false;
    Position coldSpot;
    std::string coldNote = "none";

    bool splash = false;
    Position splashSpot;
    uint32 splashBlockedMs = 0;

    ObjectGuid hideMarker;
    ObjectGuid hideTomb;
    Position hideSpot;
    bool hidThisAir = false;
    // The tomb of its last bomb hide this air phase, which it damages between bombs too
    ObjectGuid coverTomb;

    ObjectGuid buffetTomb;
    Position buffetSpot;
    ObjectGuid buffetTried;

    // The unchained node this bot walks to
    uint16 unchainedFor = 0;
    Position unchainedSpot;
    bool unchainedValid = false;
    bool unchainedLatch = false;
    bool unchainedHeld = false;

    bool chilled = false;
};

// Asked by a trigger, its action and a rule for every action a bot weighs, and a bot never ticks twice in
// one ms
struct TargetMemo
{
    uint32 ms = 0;
    bool set = false;
    Unit* pick = nullptr;
};

struct SgState
{
    uint32 pullMs = 0;

    uint32 scanMs = 0;
    bool scanned = false;
    std::vector<TombInfo> tombs;
    std::vector<MarkerInfo> markers;
    std::unordered_map<ObjectGuid, TombSeen> tombSeen;
    std::unordered_map<ObjectGuid, uint32> markerSeen;

    uint32 dutyMs = 0;
    bool dutiesRan = false;
    IccSindragosaPhaseId phase = IccSindragosaPhaseId::Ground;
    // Her live spot, or her landing spot before the pull
    Position anchor = ICC_SG_LAND;

    // Her victim when a group tank, else the last holder while it lives, else the group's main tank
    ObjectGuid holder;
    // TankRole per tank bot
    RaidObs::ObsGuidMap<uint8> tank{"sg.tank"};
    RaidObs::ObsGuidSet brake{"sg.brake"};
    RaidObs::ObsValue<uint32> breath{"sg.breath"};
    bool breathCasting = false;
    uint32 lastBreathMs = 0;

    // The free tank taking her over before the next last phase breath
    ObjectGuid swapper;
    uint32 swapBreath = 0;
    bool swapTaunted = false;
    uint32 swapTauntMs = 0;
    // Its taunt went out and she stayed on the holder, who then takes the breath
    bool swapMissed = false;
    // The breath whose swap has gone out
    uint32 swappedBreath = 0;

    uint32 airStartMs = 0;
    bool stationReached = false;
    bool released = false;
    std::unordered_set<ObjectGuid> airMarkers;
    RaidObs::ObsValue<uint32> bombs{"sg.bombs"};
    RaidObs::ObsValue<bool> tombhold{"sg.tombhold"};

    // Spot 1-6 in the air, 7 or 8 in the last phase, per beaconed bot
    RaidObs::ObsGuidMap<uint8> beacon{"sg.beacon"};
    std::unordered_map<ObjectGuid, Position> beaconSpots;
    // Every beaconed member not yet in its tomb, humans too
    std::vector<ObjectGuid> beaconed;
    // Living members carrying neither Unchained Magic nor Instability, for heroic Backlash
    std::vector<Position> nonCarriers;

    // Heroic carrier -> its unchained node
    std::unordered_map<ObjectGuid, uint16> unchained;
    // Paladin -> the tank it frees
    std::unordered_map<ObjectGuid, ObjectGuid> freedom;
    // The healer spending an external on a holder no swap relieves
    ObjectGuid coverBy;
    // The newest tomb formed in the last phase, which the raid hides behind
    ObjectGuid newestTomb;
    // Air tombs some bot hides behind from a live bomb
    std::unordered_set<ObjectGuid> coverTombs;

    uint32 humanCallMs = 0;
    bool humanCallRan = false;

    std::unordered_map<ObjectGuid, SgBotState> bots;
    std::unordered_map<ObjectGuid, TargetMemo> targets;
};

RaidInstanceState<SgState> sgStates;
// Bots whose saved rti was checked this instance, so a player's later order sticks
RaidInstanceState<std::unordered_set<ObjectGuid>> sgRtiChecked;

bool IsTankPlayer(Player* player)
{
    return player && (PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true));
}

// Players hold no duty, and a selfbot counts as a player here.
bool IsDutyBot(Player* player) { return player && GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

bool IsHealer(Player* player) { return player && PlayerbotAI::IsHeal(player) && !IsTankPlayer(player); }

bool IsHeroic(Player* bot)
{
    Difficulty const difficulty = GetRaidMapDifficulty(bot);
    return difficulty == RAID_DIFFICULTY_10MAN_HEROIC || difficulty == RAID_DIFFICULTY_25MAN_HEROIC;
}

void NoteDerived(Player* bot, char const* key, std::string const& value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
}

// A literal builds its std::string only once a trace is open
void NoteDerived(Player* bot, char const* key, char const* value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
}

std::string Tag(char const* rule, Unit const* unit)
{
    return unit ? std::string(rule) + " " + std::to_string(unit->GetGUID().GetCounter()) : std::string(rule);
}

std::string Tag(char const* rule, ObjectGuid const& guid)
{
    return std::string(rule) + " " + std::to_string(guid.GetCounter());
}

Position PolarSpot(Position const& anchor, Polar const& polar)
{
    float const angle = polar.deg * static_cast<float>(M_PI) / 180.0f;
    return Position(anchor.GetPositionX() + polar.r * std::cos(angle),
                    anchor.GetPositionY() + polar.r * std::sin(angle), anchor.GetPositionZ());
}

// `behind` yd past `cover` on the line from `from`
Position Behind(Position const& from, Position const& cover, float behind)
{
    float const dx = cover.GetPositionX() - from.GetPositionX();
    float const dy = cover.GetPositionY() - from.GetPositionY();
    float const len = std::max(std::sqrt(dx * dx + dy * dy), 0.001f);
    return Position(cover.GetPositionX() + dx / len * behind, cover.GetPositionY() + dy / len * behind,
                    cover.GetPositionZ());
}

// The floor under the spot as the bot can walk to it, or false when the walk would stop short of it
bool FloorSpot(Player* bot, Position const& wanted, Position& out)
{
    Position const floor = ValidateStaticFloorPoint(bot, wanted);
    if (floor.GetExactDist2d(wanted) > FLOOR_TOLERANCE)
        return false;

    out = floor;
    return true;
}

// The straight walk from `from` to `to` keeps `keep` off every hazard, or at least never gets closer to one
// than `from` already is
bool SegmentClear(std::vector<Position> const& hazards, Position const& from, Position const& to, float keep)
{
    float const dx = to.GetPositionX() - from.GetPositionX();
    float const dy = to.GetPositionY() - from.GetPositionY();
    float const length2 = dx * dx + dy * dy;
    for (Position const& hazard : hazards)
    {
        float const hx = hazard.GetPositionX() - from.GetPositionX();
        float const hy = hazard.GetPositionY() - from.GetPositionY();
        float const along = length2 > 0.0f ? std::clamp((hx * dx + hy * dy) / length2, 0.0f, 1.0f) : 0.0f;
        float const closest = std::hypot(hx - along * dx, hy - along * dy);
        if (closest < std::min(keep, std::hypot(hx, hy) - 0.1f))
            return false;
    }

    return true;
}

bool StaticSight(Player* bot, Position const& from, Position const& to)
{
    return bot->GetMap()->isInLineOfSight(from.GetPositionX(), from.GetPositionY(), from.GetPositionZ() + SIGHT_EYE,
                                          to.GetPositionX(), to.GetPositionY(), to.GetPositionZ() + SIGHT_EYE,
                                          bot->GetPhaseMask(), LINEOFSIGHT_CHECK_VMAP,
                                          VMAP::ModelIgnoreFlags::Nothing);
}

bool UnchainedNode(uint16 index, Position& out)
{
    int const row = index / UNCHAINED_GRID_COLS;
    int const col = index % UNCHAINED_GRID_COLS;
    if (row >= UNCHAINED_GRID_ROWS || !((UNCHAINED_GRID[row] >> col) & 1u))
        return false;

    out = Position(ICC_SG_LAND.GetPositionX() + static_cast<float>(row + UNCHAINED_GRID_I0) * UNCHAINED_GRID_STEP,
                   ICC_SG_LAND.GetPositionY() + static_cast<float>(col + UNCHAINED_GRID_J0) * UNCHAINED_GRID_STEP,
                   UNCHAINED_GRID_Z);
    return true;
}

float TombFloorPct(Map const* map) { return map && map->Is25ManRaid() ? TOMB_FLOOR_PCT_25 : TOMB_FLOOR_PCT_10; }

Creature* BossUnit(Player* bot)
{
    InstanceScript* instance = bot->GetMapId() == ICC_MAP_ID ? bot->GetInstanceScript() : nullptr;
    return instance ? ObjectAccessor::GetCreature(*bot, instance->GetGuidData(SG_DATA_SINDRAGOSA)) : nullptr;
}

IccSindragosaPhaseId PhaseOf(Unit const* boss)
{
    // Her air phase sets her passive at takeoff and aggressive again at touchdown, and nothing else does
    Creature const* const creature = boss ? boss->ToCreature() : nullptr;
    if (!creature || !boss->IsInCombat())
        return IccSindragosaPhaseId::Ground;

    if (creature->GetReactState() == REACT_PASSIVE)
        return IccSindragosaPhaseId::Air;

    return boss->HealthAbovePct(35) ? IccSindragosaPhaseId::Ground : IccSindragosaPhaseId::Last;
}

std::vector<Player*> MembersOnMap(Player* bot)
{
    std::vector<Player*> members;
    Group* group = bot->GetGroup();
    if (!group)
    {
        members.push_back(bot);
        return members;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && IccSameInstance(bot, member))
            members.push_back(member);
    }

    return members;
}

bool IsTombed(Player* member) { return member->HasAura(SPELL_SG_ICE_TOMB); }
bool IsBeaconed(Player* member) { return member->HasAura(SPELL_SG_FROST_BEACON); }

bool CarriesUnchained(Player* member)
{
    return member->HasAura(SPELL_SG_UNCHAINED_MAGIC) || member->HasAura(SPELL_SG_INSTABILITY);
}

uint32 BuffetStacks(Player* member, Unit* boss)
{
    return member->GetAuraCount(sSpellMgr->GetSpellIdForDifficulty(SPELL_SG_MYSTIC_BUFFET, boss));
}

bool CastingBreath(Unit* boss)
{
    return boss->FindCurrentSpellBySpellId(sSpellMgr->GetSpellIdForDifficulty(SPELL_SG_FROST_BREATH, boss)) ||
           boss->FindCurrentSpellBySpellId(sSpellMgr->GetSpellIdForDifficulty(SPELL_SG_FROST_BREATH_LAST, boss));
}

bool CastingCold(Unit* boss)
{
    return boss->FindCurrentSpellBySpellId(sSpellMgr->GetSpellIdForDifficulty(SPELL_SG_BLISTERING_COLD, boss));
}

SgState* StateOf(PlayerbotAI* botAI, Creature** boss = nullptr)
{
    Creature* unit = GetSgBoss(botAI);
    if (boss)
        *boss = unit;

    return unit ? sgStates.Find(botAI->GetBot()->GetInstanceId()) : nullptr;
}

SgState* LiveStateOf(PlayerbotAI* botAI, Creature** boss = nullptr)
{
    Creature* unit = nullptr;
    SgState* st = StateOf(botAI, &unit);
    if (boss)
        *boss = unit;

    return st && unit->IsInCombat() ? st : nullptr;
}

SgBotState const* BotOf(SgState const& st, Player* bot)
{
    auto const itr = st.bots.find(bot->GetGUID());
    return itr != st.bots.end() ? &itr->second : nullptr;
}

TombInfo const* FindTomb(SgState const& st, ObjectGuid const& guid)
{
    for (TombInfo const& tomb : st.tombs)
        if (tomb.guid == guid)
            return &tomb;

    return nullptr;
}

TankRole RoleOf(SgState const& st, Player* bot)
{
    auto const itr = st.tank.find(bot->GetGUID());
    return itr != st.tank.end() ? static_cast<TankRole>(itr->second) : TankRole::None;
}

Player* PlayerOf(Player* bot, ObjectGuid const& guid)
{
    Player* player = guid ? ObjectAccessor::GetPlayer(*bot, guid) : nullptr;
    return player && player->IsAlive() && IccSameInstance(bot, player) ? player : nullptr;
}

// Asphyxiation starts about as the 4th bomb lands, so a tomb covering a bot from a live bomb stays up
bool AirBreakable(SgState const& st, TombInfo const& tomb)
{
    return tomb.asphyxiating && !st.coverTombs.count(tomb.guid);
}

// An air tomb while she still bombs, or the newest last phase tomb, unless its prisoner can't wait
bool TombHeld(SgState const& st, TombInfo const& tomb)
{
    if (tomb.skulled)
        return false;

    if (tomb.air)
        return st.phase == IccSindragosaPhaseId::Air && !st.released && !AirBreakable(st, tomb);

    return !tomb.danger && st.phase == IccSindragosaPhaseId::Last && tomb.guid == st.newestTomb;
}

uint8 PrisonerTier(Player* prisoner)
{
    if (IsTankPlayer(prisoner))
        return 0;

    return PlayerbotAI::IsHeal(prisoner) ? 1 : 2;
}

void Scan(SgState& st, Player* bot, uint32 now)
{
    if (st.scanned && getMSTimeDiff(st.scanMs, now) < SCAN_MS)
        return;

    st.scanned = true;
    st.scanMs = now;
    st.tombs.clear();
    st.markers.clear();

    std::vector<Player*> prisoners;
    for (Player* member : MembersOnMap(bot))
        if (IsTombed(member))
            prisoners.push_back(member);

    Group* group = bot->GetGroup();
    ObjectGuid const skull = group ? group->GetTargetIcon(RtiTargetValue::skullIndex) : ObjectGuid::Empty;

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_SG_ICE_TOMB, SCAN_RANGE);
    std::unordered_set<ObjectGuid> alive;
    for (Creature* tomb : found)
    {
        if (!tomb->IsAlive() || tomb->GetExactDist2d(ICC_SG_LAND) > SCAN_RANGE)
            continue;

        alive.insert(tomb->GetGUID());
        auto const seen = st.tombSeen.try_emplace(tomb->GetGUID());
        if (seen.second)
        {
            seen.first->second.ms = now;
            seen.first->second.air = st.phase == IccSindragosaPhaseId::Air;
        }

        TombInfo info;
        info.guid = tomb->GetGUID();
        info.pos = tomb->GetPosition();
        info.healthPct = tomb->GetHealthPct();
        info.air = seen.first->second.air;
        info.seenMs = seen.first->second.ms;
        info.skulled = skull && skull == info.guid;

        Player* prisoner = nullptr;
        float best = PRISONER_MATCH;
        for (Player* member : prisoners)
        {
            float const distance = member->GetExactDist2d(tomb);
            if (distance <= best)
            {
                prisoner = member;
                best = distance;
            }
        }

        if (prisoner)
        {
            info.tier = PrisonerTier(prisoner);
            info.asphyxiating = prisoner->HasAura(SPELL_SG_ASPHYXIATION);
            info.danger = info.asphyxiating || prisoner->GetHealthPct() < PRISONER_LOW_PCT;
        }

        st.tombs.push_back(info);
    }

    for (auto itr = st.tombSeen.begin(); itr != st.tombSeen.end();)
        itr = alive.count(itr->first) ? std::next(itr) : st.tombSeen.erase(itr);

    found.clear();
    bot->GetCreatureListWithEntryInGrid(found, NPC_SG_FROST_BOMB, SCAN_RANGE);
    std::unordered_set<ObjectGuid> live;
    for (Creature* marker : found)
    {
        if (!marker->IsAlive() || !marker->HasAura(SPELL_SG_FROST_BOMB_VISUAL) ||
            marker->GetExactDist2d(ICC_SG_LAND) > SCAN_RANGE)
            continue;

        live.insert(marker->GetGUID());
        auto const seen = st.markerSeen.try_emplace(marker->GetGUID(), now);
        if (st.phase == IccSindragosaPhaseId::Air)
            st.airMarkers.insert(marker->GetGUID());

        st.markers.push_back({marker->GetGUID(), marker->GetPosition(), seen.first->second});
    }

    for (auto itr = st.markerSeen.begin(); itr != st.markerSeen.end();)
        itr = live.count(itr->first) ? std::next(itr) : st.markerSeen.erase(itr);
}

void UpdatePhase(SgState& st, Creature* boss, uint32 now)
{
    IccSindragosaPhaseId const phase = PhaseOf(boss);
    if (phase == IccSindragosaPhaseId::Air && st.phase != IccSindragosaPhaseId::Air)
    {
        st.airStartMs = now;
        st.stationReached = false;
        st.released = false;
        st.airMarkers.clear();
        st.bombs = 0;
        for (auto& entry : st.bots)
        {
            entry.second.hidThisAir = false;
            entry.second.coverTomb.Clear();
        }
    }

    st.phase = phase;
    if (phase == IccSindragosaPhaseId::Air)
    {
        bool const over = boss->GetExactDist2d(ICC_SG_FAR_STATION) <= STATIONS_RADIUS;
        if (over)
            st.stationReached = true;

        if (!st.released && st.markers.empty() &&
            (st.airMarkers.size() >= BOMBS_PER_AIR_PHASE || (st.stationReached && !over)))
            st.released = true;

        st.bombs = static_cast<uint32>(st.airMarkers.size());
    }

    st.tombhold = phase == IccSindragosaPhaseId::Air && !st.released;

    // Before the pull the stands hang off her landing spot; in the air the last one holds
    if (!boss->IsInCombat())
        st.anchor = ICC_SG_LAND;
    else if (phase != IccSindragosaPhaseId::Air && boss->GetExactDist2d(st.anchor) > ANCHOR_DRIFT)
        st.anchor = boss->GetPosition();
}

void UpdateBreath(SgState& st, Creature* boss, uint32 now)
{
    bool const casting = boss->IsInCombat() && CastingBreath(boss);
    if (casting && !st.breathCasting)
    {
        st.breath = st.breath.Get() + 1;
        st.lastBreathMs = now;
    }

    st.breathCasting = casting;
}

// Main tank, then the assist tanks in their order, then guid
std::vector<Player*> RankedTanks(std::vector<Player*> const& members)
{
    std::vector<std::pair<uint8, Player*>> ranked;
    for (Player* member : members)
    {
        if (!IsDutyBot(member) || !IsTankPlayer(member))
            continue;

        uint8 rank = PlayerbotAI::IsMainTank(member) ? 0 : 10;
        for (uint8 index = 0; rank == 10 && index < 4; ++index)
            if (PlayerbotAI::IsAssistTankOfIndex(member, index, true))
                rank = index + 1;

        ranked.emplace_back(rank, member);
    }

    std::sort(ranked.begin(), ranked.end(),
              [](auto const& a, auto const& b)
              { return a.first != b.first ? a.first < b.first : a.second->GetGUID() < b.second->GetGUID(); });

    std::vector<Player*> tanks;
    for (auto const& entry : ranked)
        tanks.push_back(entry.second);

    return tanks;
}

void AssignTanks(SgState& st, Player* bot, std::vector<Player*> const& members, std::vector<Player*> const& tanks,
                 Creature* boss)
{
    std::unordered_set<ObjectGuid> roster;
    for (Player* member : members)
        roster.insert(member->GetGUID());

    // An air tomb can take a tank, and a tombed holder could never taunt her back
    auto const holds = [&](Player* player)
    {
        return player && player->IsAlive() && roster.count(player->GetGUID()) && IsTankPlayer(player) &&
               !IsTombed(player);
    };

    Unit* victim = boss->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    if (!holds(holder))
        holder = PlayerOf(bot, st.holder);
    if (!holds(holder))
        holder = GetGroupMainTank(bot);
    for (auto itr = tanks.begin(); !holds(holder) && itr != tanks.end(); ++itr)
        holder = *itr;

    st.holder = holds(holder) ? holder->GetGUID() : ObjectGuid::Empty;

    std::unordered_set<ObjectGuid> current;
    for (Player* tank : tanks)
        current.insert(tank->GetGUID());

    std::vector<ObjectGuid> dropped;
    for (auto const& entry : st.tank)
        if (!current.count(entry.first))
            dropped.push_back(entry.first);

    for (ObjectGuid const& guid : dropped)
        st.tank.erase(guid);

    for (Player* tank : tanks)
        st.tank[tank->GetGUID()] =
            static_cast<uint8>(tank->GetGUID() == st.holder ? TankRole::Holder : TankRole::Free);
}

// From SWAP_WALK_MS after a last phase breath, the first free bot tank shed of Mystic Buffet walks to the
// holder's spot, then taunts at SWAP_TAUNT_MS. Her next breath lands 20-25 s after the last.
void UpdateSwap(SgState& st, Player* bot, std::vector<Player*> const& tanks, Creature* boss, uint32 now)
{
    if (st.swapper)
    {
        Player* swapper = PlayerOf(bot, st.swapper);
        bool const done = st.swapper == st.holder;
        bool const stale = !swapper || st.breath.Get() != st.swapBreath || st.phase != IccSindragosaPhaseId::Last;
        if (!done && !stale)
        {
            // It goes back to the melee stand rather than share the breath with the holder
            if (st.swapTaunted && getMSTimeDiff(st.swapTauntMs, now) >= SWAP_MISS_MS)
                st.swapMissed = true;
            return;
        }

        st.swapper.Clear();
        st.swapTaunted = false;
        st.swapMissed = false;
    }

    if (st.phase != IccSindragosaPhaseId::Last || !st.lastBreathMs || st.swappedBreath == st.breath.Get() ||
        getMSTimeDiff(st.lastBreathMs, now) < SWAP_WALK_MS || !st.holder)
        return;

    for (Player* tank : tanks)
    {
        if (RoleOf(st, tank) != TankRole::Free || IsTombed(tank) || IsBeaconed(tank) ||
            BuffetStacks(tank, boss) > SWAP_BUFFET_MAX)
            continue;

        PlayerbotAI* ai = GET_PLAYERBOT_AI(tank);
        if (!ai || !ClassTauntReady(ai))
            continue;

        st.swapper = tank->GetGUID();
        st.swapBreath = st.breath.Get();
        st.swapTaunted = false;
        st.swapMissed = false;
        return;
    }
}

uint8 AirBeaconSpotCount(Player* bot)
{
    return IsRaid25Man(bot) ? static_cast<uint8>(sizeof(ICC_SG_BEACON_SPOTS_25) / sizeof(ICC_SG_BEACON_SPOTS_25[0]))
                            : static_cast<uint8>(sizeof(ICC_SG_BEACON_SPOTS_10) / sizeof(ICC_SG_BEACON_SPOTS_10[0]));
}

Position const& AirBeaconSpot(Player* bot, uint8 id)
{
    return IsRaid25Man(bot) ? ICC_SG_BEACON_SPOTS_25[id - 1] : ICC_SG_BEACON_SPOTS_10[id - 1];
}

bool TombSpotFree(SgState const& st, Position const& spot)
{
    return std::none_of(st.tombs.begin(), st.tombs.end(),
                        [&](TombInfo const& tomb) { return tomb.pos.GetExactDist2d(spot) < TOMB_SPOT_FOUL; });
}

// Air beacons take their spots in guid order. A last phase beacon takes A, or B while a tomb sits on A.
void AssignBeacons(SgState& st, Player* bot, std::vector<Player*> const& members)
{
    st.beaconed.clear();
    std::vector<Player*> beacons;
    for (Player* member : members)
    {
        if (!IsBeaconed(member) || IsTombed(member))
            continue;

        st.beaconed.push_back(member->GetGUID());
        if (IsDutyBot(member))
            beacons.push_back(member);
    }

    std::sort(beacons.begin(), beacons.end(), IccGuidLess());
    std::unordered_set<ObjectGuid> roster;
    for (Player* member : beacons)
        roster.insert(member->GetGUID());

    // A beacon ending is news: its tomb has formed or it was cleansed
    std::vector<ObjectGuid> ended;
    for (auto const& entry : st.beacon)
        if (!roster.count(entry.first))
            ended.push_back(entry.first);

    for (ObjectGuid const& guid : ended)
        st.beacon.erase(guid);

    for (auto itr = st.beaconSpots.begin(); itr != st.beaconSpots.end();)
        itr = roster.count(itr->first) ? std::next(itr) : st.beaconSpots.erase(itr);

    bool const air = st.phase == IccSindragosaPhaseId::Air;
    uint8 const count = AirBeaconSpotCount(bot);
    bool used[BEACON_SPOT_A + 2] = {};
    for (auto const& entry : st.beacon)
        if (entry.second < BEACON_SPOT_A + 2)
            used[entry.second] = true;

    for (Player* member : beacons)
    {
        if (st.beacon.count(member->GetGUID()))
            continue;

        uint8 spot = 0;
        Position pos;
        if (air)
        {
            for (uint8 id = 1; id <= count && !spot; ++id)
                if (!used[id])
                    spot = id;

            if (spot)
                pos = AirBeaconSpot(bot, spot);
        }
        else
        {
            Position const a = PolarSpot(st.anchor, TOMB_SPOTS[0]);
            Position const b = PolarSpot(st.anchor, TOMB_SPOTS[1]);
            bool const freeA = !used[BEACON_SPOT_A] && TombSpotFree(st, a);
            bool const freeB = !used[BEACON_SPOT_A + 1] && TombSpotFree(st, b);
            spot = freeA || !freeB ? BEACON_SPOT_A : BEACON_SPOT_A + 1;
            pos = spot == BEACON_SPOT_A ? a : b;
        }

        if (!spot)
            continue;

        used[spot] = true;
        st.beacon[member->GetGUID()] = spot;
        st.beaconSpots[member->GetGUID()] = pos;
    }
}

// Where the raid stands or runs, which a carrier's Backlash must not reach: the stands, the Blistering Cold
// runs and, in the last phase, both tomb spots and the hides behind them
std::vector<Position> UnchainedRaidSpots(SgState const& st)
{
    std::vector<Position> spots = {PolarSpot(st.anchor, STAND_HOLDER), PolarSpot(st.anchor, COLD_HOLDER),
                                   PolarSpot(st.anchor, STAND_MELEE), PolarSpot(st.anchor, STAND_RANGED)};
    for (Polar const& escape : COLD_ESCAPES)
        spots.push_back(PolarSpot(st.anchor, escape));

    if (st.phase == IccSindragosaPhaseId::Last)
        for (Polar const& tomb : TOMB_SPOTS)
        {
            spots.push_back(PolarSpot(st.anchor, tomb));
            spots.push_back(PolarSpot(st.anchor, {tomb.deg, tomb.r + BUFFET_HIDE_BEHIND}));
        }

    return spots;
}

// Outside Blistering Cold, her breath toward the holder, Tail Smash and the raid
bool UnchainedNodeClear(Position const& anchor, std::vector<Position> const& raid, Position const& node)
{
    float const her = node.GetExactDist2d(anchor);
    if (her < UNCHAINED_HER_KEEP)
        return false;

    float const dx = node.GetPositionX() - anchor.GetPositionX();
    float const dy = node.GetPositionY() - anchor.GetPositionY();
    float const deg = std::atan2(dy, dx) * 180.0f / static_cast<float>(M_PI);
    if (her <= BREATH_REACH && std::fabs(std::remainder(deg - STAND_HOLDER.deg, 360.0f)) < BREATH_KEEP_DEG)
        return false;

    if (node.GetExactDist2d(PolarSpot(anchor, TAIL_SMASH)) < TAIL_KEEP)
        return false;

    return std::none_of(raid.begin(), raid.end(),
                        [&](Position const& spot) { return spot.GetExactDist2d(node) < UNCHAINED_RAID_KEEP; });
}

// A node clear of her, the raid and the other carriers' nodes, or -1. Others need her within spell reach and
// take the node nearest the ranged stand. Healers want the holder in heal reach and sight, else the ranged
// stand in reach, else the node nearest it.
int PickUnchainedNode(SgState const& st, Player* bot, std::vector<Position> const& raid, bool healer)
{
    std::vector<Position> taken;
    for (auto const& entry : st.unchained)
        if (Position node; UnchainedNode(entry.second, node))
            taken.push_back(node);

    auto const crowded = [&](Position const& node)
    {
        return std::any_of(taken.begin(), taken.end(),
                           [&](Position const& other) { return other.GetExactDist2d(node) < UNCHAINED_CARRIER_APART; });
    };

    Position const holder = PolarSpot(st.anchor, STAND_HOLDER);
    Position const ranged = PolarSpot(st.anchor, STAND_RANGED);
    // reach tier, distance to the ranged stand, node
    std::vector<std::tuple<uint8, float, uint16>> ranked;
    for (int row = 0; row < UNCHAINED_GRID_ROWS; ++row)
        for (int col = 0; col < UNCHAINED_GRID_COLS; ++col)
        {
            uint16 const index = static_cast<uint16>(row * UNCHAINED_GRID_COLS + col);
            Position node;
            if (!UnchainedNode(index, node) || !UnchainedNodeClear(st.anchor, raid, node) || crowded(node))
                continue;

            float const toRanged = node.GetExactDist2d(ranged);
            if (!healer)
            {
                if (node.GetExactDist2d(st.anchor) <= UNCHAINED_CAST_REACH)
                    ranked.emplace_back(0, toRanged, index);
                continue;
            }

            uint8 const tier = node.GetExactDist2d(holder) <= HEAL_REACH ? 0 : toRanged <= HEAL_REACH ? 1 : 2;
            ranked.emplace_back(tier, toRanged, index);
        }

    std::sort(ranked.begin(), ranked.end());
    // The holder counts only where sight reaches it: the north wall hides the holder once she has drifted west
    uint8 tries = 0;
    for (auto& entry : ranked)
    {
        if (std::get<0>(entry) != 0)
            break;

        Position node;
        UnchainedNode(std::get<2>(entry), node);
        if (tries++ < HOLDER_SIGHT_TRIES && StaticSight(bot, node, holder))
            return std::get<2>(entry);

        std::get<0>(entry) = std::get<1>(entry) <= HEAL_REACH ? 1 : 2;
    }

    std::sort(ranked.begin(), ranked.end());
    return ranked.empty() ? -1 : std::get<2>(ranked.front());
}

void AssignUnchained(SgState& st, Player* bot, std::vector<Player*> const& members)
{
    st.nonCarriers.clear();
    for (Player* member : members)
        if (!CarriesUnchained(member))
            st.nonCarriers.push_back(member->GetPosition());

    if (!IsHeroic(bot) || st.phase == IccSindragosaPhaseId::Air)
    {
        st.unchained.clear();
        return;
    }

    std::vector<Player*> healers;
    std::vector<Player*> others;
    for (Player* member : members)
    {
        if (!IsDutyBot(member) || !CarriesUnchained(member) || IsTombed(member))
            continue;

        (IsHealer(member) ? healers : others).push_back(member);
    }

    std::sort(healers.begin(), healers.end(), IccGuidLess());
    std::sort(others.begin(), others.end(), IccGuidLess());
    std::unordered_set<ObjectGuid> roster;
    for (Player* member : healers)
        roster.insert(member->GetGUID());
    for (Player* member : others)
        roster.insert(member->GetGUID());

    // Nodes never move, so two kept nodes stay apart: a node goes only when her drift fouls it
    std::vector<Position> const raid = UnchainedRaidSpots(st);
    for (auto itr = st.unchained.begin(); itr != st.unchained.end();)
    {
        Position node;
        bool const keep = roster.count(itr->first) && UnchainedNode(itr->second, node) &&
                          UnchainedNodeClear(st.anchor, raid, node);
        itr = keep ? std::next(itr) : st.unchained.erase(itr);
    }

    // Others first: they need her within spell reach, which leaves them fewer nodes
    for (Player* member : others)
        if (!st.unchained.count(member->GetGUID()))
            if (int const node = PickUnchainedNode(st, bot, raid, false); node >= 0)
                st.unchained[member->GetGUID()] = static_cast<uint16>(node);

    for (Player* member : healers)
        if (!st.unchained.count(member->GetGUID()))
            if (int const node = PickUnchainedNode(st, bot, raid, true); node >= 0)
                st.unchained[member->GetGUID()] = static_cast<uint16>(node);
}

// One paladin per tank: each tank slowed by Frost Breath in the first seconds of an air phase, and the
// holder at FREEDOM_HOLDER_STACKS breaths on the ground
void AssignFreedom(SgState& st, Player* bot, std::vector<Player*> const& members, Creature* boss, uint32 now)
{
    st.freedom.clear();
    if (!boss->IsInCombat())
        return;

    uint32 const breathId = sSpellMgr->GetSpellIdForDifficulty(SPELL_SG_FROST_BREATH, boss);
    std::vector<Player*> wanted;
    if (st.phase == IccSindragosaPhaseId::Air)
    {
        if (getMSTimeDiff(st.airStartMs, now) <= FREEDOM_AIR_MS)
            for (Player* member : members)
                if (IsTankPlayer(member) && member->HasAura(breathId) && !IsTombed(member))
                    wanted.push_back(member);
    }
    else if (Player* holder = PlayerOf(bot, st.holder))
    {
        if (holder->GetAuraCount(breathId) >= FREEDOM_HOLDER_STACKS)
            wanted.push_back(holder);
    }

    std::unordered_set<ObjectGuid> busy;
    for (Player* target : wanted)
    {
        if (target->HasAura(SPELL_SG_HAND_OF_FREEDOM))
            continue;

        Player* best = nullptr;
        float bestDistance = 0.0f;
        for (Player* member : members)
        {
            if (!IsDutyBot(member) || member->getClass() != CLASS_PALADIN || busy.count(member->GetGUID()) ||
                IsTombed(member) || !member->HasSpell(SPELL_SG_HAND_OF_FREEDOM) ||
                member->HasSpellCooldown(SPELL_SG_HAND_OF_FREEDOM))
                continue;

            float const distance = member->GetExactDist(target);
            if (distance <= FREEDOM_RANGE && (!best || distance < bestDistance))
            {
                best = member;
                bestDistance = distance;
            }
        }

        if (best)
        {
            busy.insert(best->GetGUID());
            st.freedom[best->GetGUID()] = target->GetGUID();
        }
    }
}

bool Covered(Player* holder)
{
    return holder->HasAura(SPELL_SG_PAIN_SUPPRESSION) || holder->HasAura(SPELL_SG_GUARDIAN_SPIRIT) ||
           holder->HasAura(SPELL_SG_HAND_OF_SACRIFICE);
}

// The external this healer can put on the holder from where it stands, or null
char const* CoverSpell(Player* healer, Unit* holder)
{
    PlayerbotAI* ai = GET_PLAYERBOT_AI(healer);
    if (!ai)
        return nullptr;

    std::vector<char const*> names;
    if (healer->getClass() == CLASS_PRIEST)
        names = {"pain suppression", "guardian spirit"};
    else if (healer->getClass() == CLASS_PALADIN)
        names = {"hand of sacrifice"};

    // CanCastSpell passes an out of range target, and Hand of Sacrifice's 30 yd doesn't reach the holder from
    // the ranged stand
    for (char const* name : names)
    {
        uint32 const spellId = ai->GetAiObjectContext()->GetValue<uint32>("spell id", name)->Get();
        SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        if (info && healer->IsWithinCombatRange(holder, healer->GetSpellMaxRangeForTarget(holder, info)) &&
            ai->CanCastSpell(spellId, holder))
            return name;
    }

    return nullptr;
}

// A holder no swap relieves takes the next breath on all its Mystic Buffet stacks, so from COVER_FROM_MS after
// the last breath, or through one, the nearest healer puts an external on it from COVER_BUFFET_STACKS
void AssignCover(SgState& st, Player* bot, std::vector<Player*> const& members, Creature* boss, uint32 now)
{
    st.coverBy.Clear();
    Player* holder = PlayerOf(bot, st.holder);
    bool const swapDue = st.swapper && !st.swapMissed;
    bool const window =
        CastingBreath(boss) || (st.lastBreathMs && getMSTimeDiff(st.lastBreathMs, now) >= COVER_FROM_MS);
    if (st.phase != IccSindragosaPhaseId::Last || !holder || swapDue || !window ||
        BuffetStacks(holder, boss) < COVER_BUFFET_STACKS || Covered(holder))
        return;

    Player* best = nullptr;
    float bestDistance = 0.0f;
    for (Player* member : members)
    {
        auto const bs = st.bots.find(member->GetGUID());
        if (!IsDutyBot(member) || !IsHealer(member) || IsTombed(member) ||
            (bs != st.bots.end() && bs->second.unchainedHeld))
            continue;

        float const distance = member->GetExactDist(holder);
        if (distance > COVER_RANGE || (best && distance >= bestDistance) || !member->IsWithinLOSInMap(holder) ||
            !CoverSpell(member, holder))
            continue;

        best = member;
        bestDistance = distance;
    }

    if (best)
        st.coverBy = best->GetGUID();
}

void UpdateNewestTomb(SgState& st)
{
    st.newestTomb.Clear();
    uint32 newest = 0;
    for (TombInfo const& tomb : st.tombs)
        if (!tomb.air && (!st.newestTomb || tomb.seenMs >= newest))
        {
            st.newestTomb = tomb.guid;
            newest = tomb.seenMs;
        }
}

void RefreshDuties(SgState& st, Player* bot, Creature* boss, uint32 now)
{
    if (st.dutiesRan && getMSTimeDiff(st.dutyMs, now) < DUTY_MS)
        return;

    st.dutiesRan = true;
    st.dutyMs = now;
    std::vector<Player*> const members = MembersOnMap(bot);
    std::vector<Player*> const tanks = RankedTanks(members);

    UpdatePhase(st, boss, now);
    AssignTanks(st, bot, members, tanks, boss);
    UpdateSwap(st, bot, tanks, boss, now);
    AssignBeacons(st, bot, members);
    AssignUnchained(st, bot, members);
    AssignFreedom(st, bot, members, boss, now);
    AssignCover(st, bot, members, boss, now);
    UpdateNewestTomb(st);

    st.coverTombs.clear();
    for (auto const& entry : st.bots)
        if (entry.second.hideMarker && entry.second.hideTomb)
            st.coverTombs.insert(entry.second.hideTomb);
}

// Past BRAKE_CAP of the holder's threat until under BRAKE_RELEASE, read from her threat list
void UpdateBrake(SgState& st, Player* bot, Creature* boss)
{
    bool hold = false;
    Player* holder = PlayerOf(bot, st.holder);
    bool const swapping = st.swapper == bot->GetGUID() && st.swapTaunted && !st.swapMissed;
    if (boss->IsInCombat() && st.phase != IccSindragosaPhaseId::Air && !PlayerbotAI::IsHeal(bot) && holder &&
        holder != bot && !swapping)
    {
        float const top = boss->GetThreatMgr().GetThreat(holder);
        float const own = boss->GetThreatMgr().GetThreat(bot);
        float const share = st.brake.count(bot->GetGUID()) ? BRAKE_RELEASE : BRAKE_CAP;
        hold = top > 0.0f && own > 0.0f && own >= share * top;
    }

    if (hold)
        st.brake.insert(bot->GetGUID());
    else
        st.brake.erase(bot->GetGUID());
}

void UpdateHolds(SgState& st, SgBotState& bs, Player* bot, Creature* boss)
{
    bool const live = boss->IsInCombat();
    bool const carrier = bot->HasAura(SPELL_SG_UNCHAINED_MAGIC);
    Aura const* instability = bot->GetAura(SPELL_SG_INSTABILITY);
    if (!instability)
        bs.unchainedLatch = false;
    else if (carrier && instability->GetStackAmount() >= UNCHAINED_HOLD_STACKS)
        bs.unchainedLatch = true;

    bool crowded = false;
    if (IsHeroic(bot) && (carrier || instability))
        crowded = std::any_of(st.nonCarriers.begin(), st.nonCarriers.end(),
                              [&](Position const& pos) { return bot->GetExactDist2d(pos) <= BACKLASH_KEEP; });

    bs.unchainedHeld = live && (bs.unchainedLatch || crowded);

    uint32 const chill = bot->GetAuraCount(SPELL_SG_CHILLED_TO_THE_BONE);
    if (!chill || st.holder == bot->GetGUID())
        bs.chilled = false;
    else if (chill >= CHILLED_STOP_STACKS)
        bs.chilled = true;

    bs.chilled = bs.chilled && live;
}

bool IsHolderOrSwapper(SgState const& st, Player* bot)
{
    return st.holder == bot->GetGUID() || (st.swapper == bot->GetGUID() && !st.swapMissed);
}

std::vector<Position> BeaconHazards(SgState const& st, Player* bot)
{
    std::vector<Position> hazards;
    for (ObjectGuid const& guid : st.beaconed)
    {
        if (guid == bot->GetGUID())
            continue;

        if (Player* member = PlayerOf(bot, guid))
            hazards.push_back(member->GetPosition());

        auto const spot = st.beaconSpots.find(guid);
        if (spot != st.beaconSpots.end())
            hazards.push_back(spot->second);
    }

    return hazards;
}

float NearestHazard(std::vector<Position> const& hazards, Position const& pos)
{
    float nearest = 1000.0f;
    for (Position const& hazard : hazards)
        nearest = std::min(nearest, hazard.GetExactDist2d(pos));

    return nearest;
}

// A run past a tomb paths round it, and one into a beacon's splash ends in a tomb of its own
bool EscapeFouled(SgState const& st, std::vector<Position> const& hazards, Position const& spot)
{
    return NearestHazard(hazards, spot) < SPLASH_TRIGGER ||
           std::any_of(st.tombs.begin(), st.tombs.end(),
                       [&](TombInfo const& tomb) { return tomb.pos.GetExactDist2d(spot) < ESCAPE_TOMB_KEEP; });
}

void UpdateCold(SgState& st, SgBotState& bs, Player* bot, Creature* boss)
{
    bool const casting = boss->IsInCombat() && st.phase != IccSindragosaPhaseId::Air && CastingCold(boss);
    if (!casting || IsTombed(bot))
    {
        bs.cold = false;
        bs.coldHold = false;
        bs.coldNote = "none";
        return;
    }

    bs.coldHold = !bs.cold && bot->GetExactDist2d(boss) > COLD_RANGE;
    if (bs.cold || bs.coldHold)
        return;

    bs.cold = true;
    Position const her = boss->GetPosition();
    if (st.holder == bot->GetGUID())
    {
        bs.coldSpot = PolarSpot(her, COLD_HOLDER);
        bs.coldNote = "tout";
        return;
    }

    if (bs.unchainedValid && CarriesUnchained(bot))
    {
        bs.coldSpot = bs.unchainedSpot;
        bs.coldNote = "unchained " + std::to_string(bs.unchainedFor);
        return;
    }

    // The nearest clear escape, else the nearest
    std::vector<Position> const hazards = BeaconHazards(st, bot);
    size_t best = 0;
    std::pair<bool, float> bestKey;
    for (size_t i = 0; i < sizeof(COLD_ESCAPES) / sizeof(COLD_ESCAPES[0]); ++i)
    {
        Position const spot = PolarSpot(her, COLD_ESCAPES[i]);
        std::pair<bool, float> const key(EscapeFouled(st, hazards, spot), bot->GetExactDist2d(spot));
        if (!i || key < bestKey)
        {
            best = i;
            bestKey = key;
        }
    }

    bs.coldSpot = PolarSpot(her, COLD_ESCAPES[best]);
    bs.coldNote = "escape " + std::to_string(best + 1);
}

// Out of every beacon's 10 yd: to where the bot is going when that is clear, else the nearest clear spot
void UpdateSplash(SgBotState& bs, Player* bot, std::vector<Position> const& hazards, Position const* destination)
{
    if (hazards.empty() || IsBeaconed(bot) || IsTombed(bot))
    {
        bs.splash = false;
        NoteDerived(bot, "sg.splash", "none");
        return;
    }

    float const nearest = NearestHazard(hazards, bot->GetPosition());
    if (nearest >= (bs.splash ? SPLASH_CLEAR : SPLASH_TRIGGER))
    {
        bs.splash = false;
        NoteDerived(bot, "sg.splash", "none");
        return;
    }

    if (bs.splash && NearestHazard(hazards, bs.splashSpot) >= SPLASH_CLEAR)
        return;

    // Only where the straight walk there doesn't cut past another beacon
    if (destination && NearestHazard(hazards, *destination) >= SPLASH_CLEAR &&
        SegmentClear(hazards, bot->GetPosition(), *destination, SPLASH_TRIGGER))
    {
        bs.splash = true;
        bs.splashSpot = *destination;
        NoteDerived(bot, "sg.splash", "dodge");
        return;
    }

    // A sweep asks the navmesh up to 240 times, so a blocked one waits before the next
    uint32 const now = getMSTime();
    if (bs.splashBlockedMs && getMSTimeDiff(bs.splashBlockedMs, now) < SPLASH_RETRY_MS)
        return;

    std::vector<HazardCircle> circles;
    for (Position const& hazard : hazards)
        circles.emplace_back(hazard, SPLASH_CLEAR + PARK);

    Position const found = FindNearestPositionClearOfHazards(bot, circles, SPLASH_SWEEP, 2.0f,
                                                             static_cast<float>(M_PI) / 8.0f, destination);
    if (found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f)
    {
        bs.splash = false;
        bs.splashBlockedMs = now;
        NoteDerived(bot, "sg.splash", "blocked");
        return;
    }

    bs.splashBlockedMs = 0;

    bs.splash = true;
    bs.splashSpot = found;
    NoteDerived(bot, "sg.splash", "dodge");
}

MarkerInfo const* OldestMarker(SgState const& st)
{
    MarkerInfo const* oldest = nullptr;
    for (MarkerInfo const& marker : st.markers)
        if (!oldest || marker.seenMs < oldest->seenMs)
            oldest = &marker;

    return oldest;
}

// Behind a tomb from the marker that goes off first, the hide nearest the bot, latched per marker
void UpdateBombHide(SgState& st, SgBotState& bs, Player* bot, Creature* boss)
{
    MarkerInfo const* marker = OldestMarker(st);
    if (!boss->IsInCombat() || st.phase != IccSindragosaPhaseId::Air || !marker || st.tombs.empty() ||
        IsBeaconed(bot) || IsTombed(bot))
    {
        bs.hideMarker.Clear();
        bs.hideTomb.Clear();
        return;
    }

    if (bs.hideMarker == marker->guid && (!bs.hideTomb || FindTomb(st, bs.hideTomb)))
        return;

    bs.hideMarker = marker->guid;
    bs.hideTomb.Clear();

    std::vector<std::pair<float, TombInfo const*>> candidates;
    for (TombInfo const& tomb : st.tombs)
        if (tomb.pos.GetExactDist2d(marker->pos) >= BOMB_TOMB_MIN)
            candidates.emplace_back(bot->GetExactDist2d(Behind(marker->pos, tomb.pos, BOMB_HIDE_BEHIND)), &tomb);

    std::sort(candidates.begin(), candidates.end(),
              [](auto const& a, auto const& b) { return a.first < b.first; });

    for (auto const& candidate : candidates)
    {
        Position spot;
        if (!FloorSpot(bot, Behind(marker->pos, candidate.second->pos, BOMB_HIDE_BEHIND), spot))
            continue;

        bs.hideTomb = candidate.second->guid;
        bs.coverTomb = candidate.second->guid;
        bs.hideSpot = spot;
        bs.hidThisAir = true;
        return;
    }
}

// Behind the newest tomb from her once Mystic Buffet reaches BUFFET_HIDE_STACKS (the free tank
// FREE_TANK_HIDE_STACKS), until it drops or the tomb goes. A healer only where the holder stays in heal
// reach, the holder and swapper never, a heroic carrier only without Instability.
void UpdateBuffetHide(SgState& st, SgBotState& bs, Player* bot, Creature* boss)
{
    uint32 const stacks = BuffetStacks(bot, boss);
    if (!stacks)
        bs.buffetTried.Clear();

    if (!stacks || !boss->IsInCombat() || IsTombed(bot) || IsHolderOrSwapper(st, bot) ||
        (bs.buffetTomb && !FindTomb(st, bs.buffetTomb)))
        bs.buffetTomb.Clear();

    uint32 const hideAt = RoleOf(st, bot) == TankRole::Free ? FREE_TANK_HIDE_STACKS : BUFFET_HIDE_STACKS;
    if (bs.buffetTomb || stacks < hideAt || st.phase != IccSindragosaPhaseId::Last ||
        bs.buffetTried == st.newestTomb || IsHolderOrSwapper(st, bot) || IsBeaconed(bot) || IsTombed(bot) ||
        (IsHeroic(bot) && bot->HasAura(SPELL_SG_INSTABILITY)))
        return;

    TombInfo const* tomb = FindTomb(st, st.newestTomb);
    if (!tomb)
        return;

    // One try per tomb: the reach and the floor only change once a newer tomb forms
    bs.buffetTried = tomb->guid;
    Position const wanted = Behind(boss->GetPosition(), tomb->pos, BUFFET_HIDE_BEHIND);
    if (IsHealer(bot))
    {
        Player* holder = PlayerOf(bot, st.holder);
        if (!holder || holder->GetExactDist2d(wanted) > HEAL_REACH)
            return;
    }

    Position spot;
    if (!FloorSpot(bot, wanted, spot))
        return;

    bs.buffetTomb = tomb->guid;
    bs.buffetSpot = spot;
}

void UpdateUnchainedSpot(SgState& st, SgBotState& bs, Player* bot)
{
    auto const itr = st.unchained.find(bot->GetGUID());
    bs.unchainedValid = itr != st.unchained.end() && !IsTombed(bot) && UnchainedNode(itr->second, bs.unchainedSpot);
    bs.unchainedFor = bs.unchainedValid ? itr->second : 0;
}

StandKind StandFor(SgState const& st, Player* bot, Creature* boss, uint32 now)
{
    if (IsTombed(bot))
        return StandKind::None;

    bool const live = boss->IsInCombat();
    // Before the pull, only through her fly-in: she stands idle at her landing spot after a wipe
    if (!live && !boss->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
        return StandKind::None;

    if (live && st.phase == IccSindragosaPhaseId::Air)
    {
        auto const itr = st.bots.find(bot->GetGUID());
        bool const hid = itr != st.bots.end() && itr->second.hidThisAir;
        return IsBeaconed(bot) || hid ? StandKind::None : StandKind::Stage;
    }

    bool const swapWalk = st.swapper == bot->GetGUID() && !st.swapMissed && st.lastBreathMs &&
                          getMSTimeDiff(st.lastBreathMs, now) >= SWAP_WALK_MS;
    if (st.holder == bot->GetGUID() || swapWalk)
        return StandKind::Tank;

    if (IsTankPlayer(bot))
        return StandKind::Melee;

    return PlayerbotAI::IsHeal(bot) || PlayerbotAI::IsRanged(bot) ? StandKind::Ranged : StandKind::Melee;
}

Position StandSpot(SgState const& st, StandKind kind)
{
    switch (kind)
    {
        case StandKind::Tank:
            return PolarSpot(st.anchor, STAND_HOLDER);
        case StandKind::Melee:
            return PolarSpot(st.anchor, STAND_MELEE);
        case StandKind::Ranged:
            return PolarSpot(st.anchor, STAND_RANGED);
        case StandKind::Stage:
            return ICC_SG_STAGE;
        default:
            return st.anchor;
    }
}

char const* StandName(StandKind kind)
{
    switch (kind)
    {
        case StandKind::Tank:
            return "T";
        case StandKind::Melee:
            return "M";
        case StandKind::Ranged:
            return "S";
        case StandKind::Stage:
            return "stage";
        default:
            return "none";
    }
}

void SetWalk(SgBotState& bs, Player* bot, WalkKind kind, Position const& spot)
{
    if (kind != bs.walk || (kind != WalkKind::None && bs.walkSpot.GetExactDist2d(spot) > 0.5f))
    {
        bs.walk = kind;
        bs.walkSpot = spot;
        bs.walkParked = false;
    }

    if (kind == WalkKind::None)
        return;

    float const distance = bot->GetExactDist2d(bs.walkSpot);
    bs.walkParked = bs.walkParked ? distance <= UNPARK : distance <= PARK;
}

void NoteUnchained(SgState const& st, SgBotState const& bs, Player* bot)
{
    if (!RaidObs::Active())
        return;

    if (!CarriesUnchained(bot))
    {
        NoteDerived(bot, "sg.unchained", "none");
        return;
    }

    std::string value = st.unchained.count(bot->GetGUID()) && bs.unchainedValid
                            ? "spot " + std::to_string(bs.unchainedFor)
                            : std::string("carrier");
    if (bs.unchainedHeld)
        value += " hold";

    NoteDerived(bot, "sg.unchained", value);
}

void NoteSwap(SgState const& st, Player* bot)
{
    if (!RaidObs::Active() || !IsTankPlayer(bot))
        return;

    char const* value = "none";
    if (st.swapper == bot->GetGUID())
        value = st.swapMissed ? "missed" : st.swapTaunted ? "taunt" : "walk";

    NoteDerived(bot, "sg.swap", value);
}

// The walk the bot owes now, highest first, and whether anything of this fight owns its feet
void UpdateWalks(SgState& st, SgBotState& bs, Player* bot, Creature* boss, uint32 now)
{
    if (!bot->IsAlive())
    {
        bs = SgBotState();
        return;
    }

    UpdateUnchainedSpot(st, bs, bot);
    UpdateCold(st, bs, bot, boss);
    UpdateBombHide(st, bs, bot, boss);
    UpdateBuffetHide(st, bs, bot, boss);

    StandKind const stand = StandFor(st, bot, boss, now);
    Position const standSpot = StandSpot(st, stand);
    bool const live = boss->IsInCombat();
    bool const unchainedWalk = live && bs.unchainedValid && CarriesUnchained(bot);

    // Where the bot heads once out of the splash: the walk it would take with no beacon out
    Position const* destination = nullptr;
    if (bs.hideTomb && bs.hideMarker)
        destination = &bs.hideSpot;
    else if (bs.buffetTomb)
        destination = &bs.buffetSpot;
    else if (unchainedWalk)
        destination = &bs.unchainedSpot;
    else if (stand != StandKind::None)
        destination = &standSpot;

    std::vector<Position> const hazards = live ? BeaconHazards(st, bot) : std::vector<Position>();
    UpdateSplash(bs, bot, hazards, destination);

    auto const beaconSpot = st.beaconSpots.find(bot->GetGUID());
    bool const beaconWalk = live && IsBeaconed(bot) && !IsTombed(bot) && beaconSpot != st.beaconSpots.end();
    // A stand inside a beacon's splash, or one whose straight walk cuts through it (the stair foot spots sit on the
    // way to the stage), waits where the bot is or where the dodge left it
    bool const standYields = stand != StandKind::None && !hazards.empty() &&
                             (NearestHazard(hazards, standSpot) < SPLASH_TRIGGER ||
                              !SegmentClear(hazards, bot->GetPosition(), standSpot, SPLASH_TRIGGER));

    if (bs.cold)
        SetWalk(bs, bot, WalkKind::Cold, bs.coldSpot);
    else if (bs.coldHold)
        SetWalk(bs, bot, WalkKind::None, Position());
    else if (bs.splash)
        SetWalk(bs, bot, WalkKind::Splash, bs.splashSpot);
    else if (bs.hideTomb && bs.hideMarker)
        SetWalk(bs, bot, WalkKind::BombHide, bs.hideSpot);
    else if (beaconWalk)
        SetWalk(bs, bot, WalkKind::Beacon, beaconSpot->second);
    else if (bs.buffetTomb)
        SetWalk(bs, bot, WalkKind::BuffetHide, bs.buffetSpot);
    else if (unchainedWalk)
        SetWalk(bs, bot, WalkKind::Unchained, bs.unchainedSpot);
    else if (stand != StandKind::None && !standYields)
        SetWalk(bs, bot, WalkKind::Stand, standSpot);
    else
        SetWalk(bs, bot, WalkKind::None, Position());

    // Between bombs the raid waits behind the tombs; once the tombs are released it closes on them
    bool const airHold = live && st.phase == IccSindragosaPhaseId::Air && !st.released && bs.hidThisAir &&
                         !IsTombed(bot);
    bs.owned = bs.walk != WalkKind::None || bs.coldHold || standYields || airHold;
    bs.halt = bs.walk == WalkKind::None && standYields;

    NoteDerived(bot, "sg.stand", bs.walk == WalkKind::Stand ? StandName(stand) : bs.halt ? "wait" : "none");
    NoteDerived(bot, "sg.cold", bs.coldNote);
    if (bs.hideTomb && bs.hideMarker)
        NoteDerived(bot, "sg.hide", Tag("bomb", bs.hideTomb));
    else if (bs.buffetTomb)
        NoteDerived(bot, "sg.hide", Tag("buffet", bs.buffetTomb));
    else
        NoteDerived(bot, "sg.hide", "none");
}

bool IsSkullUnit(Unit* unit)
{
    return unit && (unit->GetEntry() == NPC_SG_SINDRAGOSA || unit->GetEntry() == NPC_SG_ICE_TOMB);
}

Unit* TombUnit(Player* bot, TombInfo const& tomb)
{
    Creature* unit = ObjectAccessor::GetCreature(*bot, tomb.guid);
    return unit && unit->IsAlive() && bot->IsValidAttackTarget(unit) ? unit : nullptr;
}

// The first tomb `wanted` lets through that the bot can attack: prisoners in danger first, then tank prisoners,
// healers, anyone else, the lowest tomb first in each
Unit* PickTomb(SgState const& st, Player* bot, bool (*wanted)(SgState const&, TombInfo const&))
{
    std::vector<TombInfo const*> ranked;
    for (TombInfo const& tomb : st.tombs)
        if (wanted(st, tomb))
            ranked.push_back(&tomb);

    auto const key = [](TombInfo const* tomb)
    { return std::make_tuple(tomb->danger ? 0 : 1, tomb->tier, tomb->healthPct); };
    std::sort(ranked.begin(), ranked.end(), [&](TombInfo const* a, TombInfo const* b) { return key(a) < key(b); });
    for (TombInfo const* tomb : ranked)
        if (Unit* unit = TombUnit(bot, *tomb))
            return unit;

    return nullptr;
}

bool AnyTomb(SgState const&, TombInfo const&) { return true; }

// On the ground: air leftovers, last phase tombs other than the newest, and any whose prisoner can't wait
bool Breakable(SgState const& st, TombInfo const& tomb)
{
    return tomb.danger || tomb.air || tomb.guid != st.newestTomb;
}

Unit* AirTombPick(PlayerbotAI* botAI, SgState const& st, SgBotState const* bs, std::string& rule)
{
    Player* bot = botAI->GetBot();
    bool const ranged = IsRaidDpsBot(bot) && PlayerbotAI::IsRanged(bot);
    if (st.released)
    {
        rule = "release";
        return PickTomb(st, bot, &AnyTomb);
    }

    if (ranged)
        if (Unit* unit = PickTomb(st, bot, &AirBreakable))
        {
            rule = "break";
            return unit;
        }

    float const floorPct = TombFloorPct(bot->GetMap());
    if (bs && bs->coverTomb)
        if (TombInfo const* tomb = FindTomb(st, bs->coverTomb))
            if (TombHeld(st, *tomb) && tomb->healthPct > floorPct)
                if (Unit* unit = TombUnit(bot, *tomb))
                {
                    rule = "hide";
                    return unit;
                }

    if (!ranged)
        return nullptr;

    // The highest held tomb the bot can attack
    std::vector<TombInfo const*> held;
    for (TombInfo const& tomb : st.tombs)
        if (TombHeld(st, tomb) && tomb.healthPct > floorPct)
            held.push_back(&tomb);

    std::sort(held.begin(), held.end(),
              [](TombInfo const* a, TombInfo const* b) { return a->healthPct > b->healthPct; });
    rule = "tomb";
    for (TombInfo const* tomb : held)
        if (Unit* unit = TombUnit(bot, *tomb))
            return unit;

    return nullptr;
}

Unit* ResolveTarget(PlayerbotAI* botAI, SgState const& st, Creature* boss)
{
    Player* bot = botAI->GetBot();
    if (!IsTankPlayer(bot) && !IsRaidDpsBot(bot))
        return nullptr;

    SgBotState const* bs = BotOf(st, bot);
    if (IsTombed(bot) || (bs && (bs->unchainedHeld || bs->buffetTomb)))
    {
        NoteDerived(bot, "sg.target", IsTombed(bot) ? "tombed" : bs->buffetTomb ? "hide" : "held");
        return nullptr;
    }

    bool const braked = st.brake.count(bot->GetGUID()) != 0;
    bool const restrained = braked || (bs && bs->chilled);
    if (Unit* skull = GetRaidHumanSkull(botAI, &IsSkullUnit))
        if (!restrained || skull != boss)
        {
            NoteDerived(bot, "sg.target", Tag("skull", skull));
            return skull;
        }

    // Permeating Chill procs and threat build only on her, so tombs stay open to a braked or chilled bot
    std::string rule;
    if (st.phase == IccSindragosaPhaseId::Air)
    {
        Unit* tomb = AirTombPick(botAI, st, bs, rule);
        NoteDerived(bot, "sg.target", tomb ? Tag(rule.c_str(), tomb) : std::string("air"));
        return tomb;
    }

    if (IsRaidDpsBot(bot) && PlayerbotAI::IsRanged(bot))
        if (Unit* unit = PickTomb(st, bot, &Breakable))
        {
            NoteDerived(bot, "sg.target", Tag("break", unit));
            return unit;
        }

    if (restrained)
    {
        NoteDerived(bot, "sg.target", braked ? "brake" : "chilled");
        return nullptr;
    }

    if (boss->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE) || !bot->IsValidAttackTarget(boss))
    {
        NoteDerived(bot, "sg.target", "none");
        return nullptr;
    }

    NoteDerived(bot, "sg.target", "boss");
    return boss;
}

// A human tank has no trigger, so it gets told: on the holder's breath windows and when low
void CallHumanTank(SgState& st, Player* sender, Creature* boss, uint32 now)
{
    if (st.humanCallRan && getMSTimeDiff(st.humanCallMs, now) < HUMAN_CALL_CHECK_MS)
        return;

    st.humanCallRan = true;
    st.humanCallMs = now;
    if (!boss->IsInCombat())
        return;

    bool const breath = CastingBreath(boss);
    for (Player* member : MembersOnMap(sender))
    {
        if (IsDutyBot(member) || !IsTankPlayer(member))
            continue;

        bool const low = member->GetHealthPct() < TANK_LOW_PCT;
        bool const window = member->GetGUID() == st.holder && breath &&
                            ((st.phase == IccSindragosaPhaseId::Last &&
                              BuffetStacks(member, boss) >= DEFENSIVE_BUFFET_STACKS) ||
                             member->GetHealthPct() < HOLDER_LOW_PCT);
        if (!low && !window)
            continue;

        if (uint32 const spellId = ReadyTankDefensiveSpell(member, false, 0, 0))
            CallHumanTankDefensive(sender, member, spellId, 0, low ? "you are low" : "Frost Breath is coming",
                                   "sg.humancall");
    }
}
}  // namespace

IccSindragosaPhaseId IccSindragosaPhase(Player* bot, Unit const* boss)
{
    IccSindragosaPhaseId const phase = PhaseOf(boss);
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "sg.phase",
                             phase == IccSindragosaPhaseId::Air    ? "air"
                             : phase == IccSindragosaPhaseId::Last ? "last"
                                                                   : "ground");

    return phase;
}

Creature* GetSgBoss(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsInWorld() || bot->GetMapId() != ICC_MAP_ID || bot->GetExactDist2d(ICC_SG_LAND) > GATE_RANGE)
        return nullptr;

    Creature* boss = BossUnit(bot);
    return boss && boss->IsAlive() ? boss : nullptr;
}

bool IsSgLive(PlayerbotAI* botAI)
{
    Creature* boss = GetSgBoss(botAI);
    return boss && boss->IsInCombat();
}

bool IsSgPresent(PlayerbotAI* botAI)
{
    Creature* boss = GetSgBoss(botAI);
    return boss && (boss->IsInCombat() || boss->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE));
}

Unit* GetSgTarget(PlayerbotAI* botAI)
{
    Creature* boss = nullptr;
    SgState* st = LiveStateOf(botAI, &boss);
    if (!st)
        return nullptr;

    TargetMemo& memo = st->targets[botAI->GetBot()->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.pick = ResolveTarget(botAI, *st, boss);
    }

    return memo.pick;
}

bool IsSgTargetHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return (IsTankPlayer(bot) || IsRaidDpsBot(bot)) && IsSgLive(botAI) && !GetSgTarget(botAI);
}

namespace
{
bool WalkOf(PlayerbotAI* botAI, WalkKind kind, Position& spot)
{
    SgState* st = StateOf(botAI);
    SgBotState const* bs = st ? BotOf(*st, botAI->GetBot()) : nullptr;
    if (!bs || bs->walk != kind || bs->walkParked)
        return false;

    spot = bs->walkSpot;
    return true;
}
}  // namespace

bool GetSgColdWalk(PlayerbotAI* botAI, Position& spot) { return WalkOf(botAI, WalkKind::Cold, spot); }
bool GetSgSplashWalk(PlayerbotAI* botAI, Position& spot) { return WalkOf(botAI, WalkKind::Splash, spot); }
bool GetSgBombHideWalk(PlayerbotAI* botAI, Position& spot) { return WalkOf(botAI, WalkKind::BombHide, spot); }
bool GetSgBeaconWalk(PlayerbotAI* botAI, Position& spot) { return WalkOf(botAI, WalkKind::Beacon, spot); }
bool GetSgBuffetHideWalk(PlayerbotAI* botAI, Position& spot) { return WalkOf(botAI, WalkKind::BuffetHide, spot); }
bool GetSgUnchainedWalk(PlayerbotAI* botAI, Position& spot) { return WalkOf(botAI, WalkKind::Unchained, spot); }
bool GetSgStandWalk(PlayerbotAI* botAI, Position& spot) { return WalkOf(botAI, WalkKind::Stand, spot); }

bool IsSgMovementOwned(PlayerbotAI* botAI)
{
    SgState* st = StateOf(botAI);
    SgBotState const* bs = st ? BotOf(*st, botAI->GetBot()) : nullptr;
    return bs && bs->owned;
}

bool IsSgStandHalted(PlayerbotAI* botAI)
{
    SgState* st = StateOf(botAI);
    SgBotState const* bs = st ? BotOf(*st, botAI->GetBot()) : nullptr;
    return bs && bs->halt;
}

Unit* GetSgTauntTarget(PlayerbotAI* botAI, bool& swap)
{
    swap = false;
    Creature* boss = nullptr;
    SgState* st = LiveStateOf(botAI, &boss);
    Player* bot = botAI->GetBot();
    if (!st || st->phase == IccSindragosaPhaseId::Air || !IsDutyBot(bot) || IsTombed(bot))
        return nullptr;

    Unit* victim = boss->GetVictim();
    if (!victim || victim == bot || !ClassTauntReady(botAI) || bot->GetExactDist(boss) > TAUNT_RANGE ||
        !bot->IsWithinLOSInMap(boss))
        return nullptr;

    if (st->swapper == bot->GetGUID() && !st->swapTaunted && st->lastBreathMs &&
        getMSTimeDiff(st->lastBreathMs, getMSTime()) >= SWAP_TAUNT_MS)
    {
        swap = true;
        return boss;
    }

    // She has no taunt diminishing returns. On another tank she is that tank's: the roles follow her.
    Player* held = victim->ToPlayer();
    if (st->holder == bot->GetGUID() && !(held && IsTankPlayer(held)))
        return boss;

    return nullptr;
}

void NoteSgSwapTaunt(PlayerbotAI* botAI)
{
    SgState* st = LiveStateOf(botAI);
    if (!st || st->swapper != botAI->GetBot()->GetGUID())
        return;

    st->swapTaunted = true;
    st->swapTauntMs = getMSTime();
    st->swappedBreath = st->swapBreath;
}

Unit* GetSgCoverTarget(PlayerbotAI* botAI, std::string& spell)
{
    Creature* boss = nullptr;
    SgState* st = LiveStateOf(botAI, &boss);
    Player* bot = botAI->GetBot();
    if (!st || !IsHealer(bot) || !IsDutyBot(bot))
        return nullptr;

    // Held since the last duty pass: another cast stacks Instability
    SgBotState const* bs = BotOf(*st, bot);
    bool const held = bs && bs->unchainedHeld;
    Player* holder = st->coverBy == bot->GetGUID() && !held ? PlayerOf(bot, st->holder) : nullptr;
    char const* name = holder && !Covered(holder) ? CoverSpell(bot, holder) : nullptr;
    NoteDerived(bot, "sg.cover", name ? name : "none");
    if (!name)
        return nullptr;

    spell = name;
    return holder;
}

bool IsSgUnchainedHeld(PlayerbotAI* botAI)
{
    SgState* st = LiveStateOf(botAI);
    SgBotState const* bs = st ? BotOf(*st, botAI->GetBot()) : nullptr;
    return bs && bs->unchainedHeld;
}

bool IsSgRestrained(PlayerbotAI* botAI)
{
    SgState* st = LiveStateOf(botAI);
    if (!st)
        return false;

    SgBotState const* bs = BotOf(*st, botAI->GetBot());
    return st->brake.count(botAI->GetBot()->GetGUID()) || (bs && bs->chilled);
}

uint8 GetSgHoldDue(PlayerbotAI* botAI)
{
    Creature* boss = nullptr;
    SgState* st = LiveStateOf(botAI, &boss);
    Player* bot = botAI->GetBot();
    SgBotState const* bs = st ? BotOf(*st, bot) : nullptr;
    if (!bs || !bot->IsAlive())
        return 0;

    Unit* victim = bot->GetVictim();
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    Spell* autoRepeat = bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL);
    Unit* shot = autoRepeat ? autoRepeat->m_targets.GetUnitTarget() : nullptr;

    uint8 due = 0;
    if (bs->buffetTomb && (victim || target || autoRepeat))
        due |= SG_HOLD_ALL;

    bool const restrained = st->brake.count(bot->GetGUID()) || bs->chilled;
    if (restrained && (victim == boss || target == boss || shot == boss))
        due |= SG_HOLD_HER;

    if (bs->unchainedHeld && bot->IsNonMeleeSpellCast(false, false, true))
        due |= SG_HOLD_CAST;

    auto const atFloor = [&](Unit* unit) { return unit && IsSgAtTombFloor(unit) && IsSgHeldTomb(botAI, unit); };
    if (atFloor(victim) || atFloor(target) || atFloor(shot))
        due |= SG_HOLD_TOMB;

    return due;
}

bool IsSgHeldTomb(PlayerbotAI* botAI, Unit* unit)
{
    if (!unit || unit->GetEntry() != NPC_SG_ICE_TOMB)
        return false;

    SgState* st = LiveStateOf(botAI);
    TombInfo const* tomb = st ? FindTomb(*st, unit->GetGUID()) : nullptr;
    return tomb && TombHeld(*st, *tomb);
}

bool IsSgAtTombFloor(Unit* tomb) { return tomb && tomb->GetHealthPct() <= TombFloorPct(tomb->GetMap()); }

void GetSgHeldTombs(PlayerbotAI* botAI, std::vector<SgHeldTomb>& out)
{
    out.clear();
    SgState* st = LiveStateOf(botAI);
    if (!st)
        return;

    for (TombInfo const& tomb : st->tombs)
        if (TombHeld(*st, tomb))
            out.push_back({tomb.guid, tomb.pos});
}

uint32 GetSgTankDefensive(PlayerbotAI* botAI, char const* noteKind)
{
    Creature* boss = nullptr;
    SgState* st = LiveStateOf(botAI, &boss);
    Player* bot = botAI->GetBot();
    if (!st || !bot->IsAlive() || !IsDutyBot(bot) || !IsTankPlayer(bot) || IsTombed(bot))
        return 0;

    bool const low = bot->GetHealthPct() < TANK_LOW_PCT;
    bool const window = st->holder == bot->GetGUID() && CastingBreath(boss) &&
                        ((st->phase == IccSindragosaPhaseId::Last &&
                          BuffetStacks(bot, boss) >= DEFENSIVE_BUFFET_STACKS) ||
                         bot->GetHealthPct() < HOLDER_LOW_PCT);
    if (!low && !window)
    {
        // Change-latched, so the way out of a window is written too
        if (noteKind)
            NoteDerived(bot, noteKind, "idle");

        return 0;
    }

    // Frost Breath is frost, so Anti-Magic Shell counts
    return NextTankDefensive(botAI, bot, noteKind, false);
}

bool IsSgTankDefensiveHeld(PlayerbotAI* botAI)
{
    return IsSgLive(botAI) && botAI->GetBot()->GetHealthPct() >= TANK_LAST_RESORT_PCT;
}

Unit* GetSgFreedomTarget(PlayerbotAI* botAI)
{
    SgState* st = LiveStateOf(botAI);
    Player* bot = botAI->GetBot();
    SgBotState const* bs = st ? BotOf(*st, bot) : nullptr;
    if (!st || (bs && bs->unchainedHeld) || IsTombed(bot))
        return nullptr;

    auto const itr = st->freedom.find(bot->GetGUID());
    Player* target = itr != st->freedom.end() ? PlayerOf(bot, itr->second) : nullptr;
    if (!target || target->HasAura(SPELL_SG_HAND_OF_FREEDOM) || bot->GetExactDist(target) > FREEDOM_RANGE ||
        !bot->IsWithinLOSInMap(target))
        return nullptr;

    return botAI->CanCastSpell("hand of freedom", target) ? target : nullptr;
}

Unit* GetSgBeaconHealTarget(PlayerbotAI* botAI, std::string& spell)
{
    SgState* st = LiveStateOf(botAI);
    Player* bot = botAI->GetBot();
    if (!st || st->beaconed.empty() || !IsDutyBot(bot) || !IsHealer(bot) || IsTombed(bot))
        return nullptr;

    SgBotState const* bs = BotOf(*st, bot);
    if (bs && bs->unchainedHeld)
        return nullptr;

    std::vector<char const*> names;
    switch (bot->getClass())
    {
        case CLASS_PRIEST:
            names = {"renew", "power word: shield"};
            break;
        case CLASS_DRUID:
            names = {"rejuvenation"};
            break;
        case CLASS_SHAMAN:
            names = {"riptide"};
            break;
        default:
            return nullptr;
    }

    for (ObjectGuid const& guid : st->beaconed)
    {
        Player* member = PlayerOf(bot, guid);
        if (!member || IsTombed(member) || bot->GetExactDist(member) > HEAL_REACH || !bot->IsWithinLOSInMap(member))
            continue;

        for (char const* name : names)
        {
            bool const shield = std::string(name) == "power word: shield";
            if (shield && (member->HasAura(SPELL_WEAKENED_SOUL) || botAI->HasAura(name, member)))
                continue;

            if (!shield && botAI->HasAura(name, member, false, true))
                continue;

            if (botAI->CanCastSpell(name, member))
            {
                spell = name;
                return member;
            }
        }
    }

    return nullptr;
}

void TickSgEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId() ||
        bot->GetExactDist2d(ICC_SG_LAND) > GATE_RANGE)
        return;

    // A wipe or a kill ends the pull: the next one rebuilds every duty
    Creature* unit = BossUnit(bot);
    SgState* found = sgStates.Find(bot->GetInstanceId());
    if (!unit || !unit->IsAlive() || (found && found->pullMs && !unit->IsInCombat()))
    {
        if (found)
            sgStates.Reset(bot->GetInstanceId());
        return;
    }

    bool const live = unit->IsInCombat();
    if (!live && !unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
        return;

    // A saved rti of cross or star kept bots on the old air groups' tombs
    if (sgRtiChecked.For(bot->GetInstanceId()).insert(bot->GetGUID()).second)
    {
        Value<std::string>* rti = botAI->GetAiObjectContext()->GetValue<std::string>("rti");
        if (rti->Get() == "cross" || rti->Get() == "star")
            rti->Set("skull");
    }

    SgState& st = sgStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (live && !st.pullMs)
        st.pullMs = now;

    UpdateBreath(st, unit, now);
    Scan(st, bot, now);
    RefreshDuties(st, bot, unit, now);
    if (IsDutyBot(bot))
        CallHumanTank(st, bot, unit, now);

    if (live)
        IccSindragosaPhase(bot, unit);

    if (!IsDutyBot(bot))
        return;

    SgBotState& bs = st.bots[bot->GetGUID()];
    UpdateBrake(st, bot, unit);
    UpdateHolds(st, bs, bot, unit);
    UpdateWalks(st, bs, bot, unit, now);
    NoteUnchained(st, bs, bot);
    NoteDerived(bot, "sg.chilled", bs.chilled ? "stop" : "none");
    NoteSwap(st, bot);
}

RaidBurstWindow IccSindragosaBurstWindow(PlayerbotAI* botAI)
{
    RaidBurstWindow window;
    Creature* boss = GetSgBoss(botAI);
    window.allowLust = boss && PhaseOf(boss) == IccSindragosaPhaseId::Last;
    return window;
}

std::vector<uint32> const& GetSgOpenerBosses(PlayerbotAI* /*botAI*/)
{
    static std::vector<uint32> const bosses = {NPC_SG_SINDRAGOSA};
    return bosses;
}

uint8 IccSindragosaResistSchools(PlayerbotAI* /*botAI*/, uint8 /*previous*/) { return RAID_RESIST_FROST; }
