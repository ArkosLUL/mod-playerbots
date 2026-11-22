/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_LichKingLate.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "ICCData.h"
#include "ICCHelpers_LichKing.h"
#include "ICCHelpers_Shared.h"
#include "ICCScripts.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidDifficulty.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

using namespace EncounterHelpers;

namespace
{
constexpr uint32 SCAN_MS = 200;
constexpr uint32 DUTY_MS = 250;
constexpr float PLATFORM_SCAN = 150.0f;
constexpr float PLATFORM_REACH = 120.0f;
constexpr float FM_SCAN = 90.0f;
constexpr float FM_ROOM = 40.0f;
constexpr float FM_SPOT_RANGE = 20.0f;
constexpr float FLOOR_MAX_R = 44.0f;
constexpr float ANCHOR_MAX_R = 40.0f;

constexpr float PARK = 1.0f;
constexpr float UNPARK = 2.5f;
constexpr float FOLLOW_UNPARK = 3.0f;
constexpr float SPOT_MOVED = 0.5f;
static_assert(PARK < UNPARK, "a walk has to end inside its own trigger");

constexpr float DEFILE_RADIUS_PER_SCALE = 10.0f;
constexpr float PUDDLE_TRIGGER = 2.0f;
constexpr float PUDDLE_SEGMENT = 1.5f;
constexpr float PUDDLE_STAND = PUDDLE_TRIGGER + PARK;
constexpr uint32 DEFILE_CAST_MS = 2000;
constexpr uint32 DEFILE_CAST_GRACE_MS = 300;
constexpr float DEFILE_RUN_MEMBERS = 14.0f;
constexpr float DEFILE_RUN_PUDDLES = 12.0f;
constexpr float DEFILE_RUN_SWEEP = 30.0f;
constexpr float DEFILE_RUN_FALLBACK[] = {16.0f, 12.0f, 8.0f};
constexpr float DEFILE_CLEAR = 13.0f;
constexpr float DEFILE_CLEAR_SWEEP = 20.0f;
constexpr float PUDDLE_SWEEP = 25.0f;
constexpr float SWEEP_STEP = 2.0f;
constexpr uint32 SWEEP_RETRY_MS = 1000;
constexpr float SIDE_UNDEFINED = 2.0f;

struct FrameOffset
{
    float deg;
    float r;
};

constexpr FrameOffset HOLDER_OFFSET{0.0f, 3.0f};
constexpr FrameOffset FREE_OFFSET{20.0f, 4.0f};
constexpr FrameOffset STACK_OFFSET{180.0f, 5.0f};
constexpr float LATTICE_STEP = 2.0f;
constexpr float LATTICE_RADIUS = 4.0f;
constexpr float SIDE_R = 20.0f;
constexpr float SIDE_EAST_DEG = 270.0f;
constexpr float SIDE_WEST_DEG = 90.0f;
constexpr uint8 SIDE_EAST = 1;
constexpr uint8 SIDE_WEST = 2;
constexpr float ANCHOR_SLIDE_MAX = 20.0f;
constexpr float ANCHOR_SLIDE_STEP = 1.0f;

constexpr float SIPHON_Z = 850.0f;
constexpr float SIPHON_RANGE = 20.0f;
constexpr float HEROIC_DROP_PCT = 50.0f;
constexpr uint32 CONTROL_LEAD_MS = 500;
constexpr uint32 CONTROL_FLIGHT_MS = 1000;
constexpr float RANGED_REACH = 30.0f;
constexpr float TAUNT_RANGE = 30.0f;
constexpr float SPIRIT_NEAR_HIM = 12.0f;

constexpr float SOAK_OFFSET = 10.0f;
constexpr float SOAK_REACH = 8.0f;
constexpr float SOAK_LOW = 5.0f;
constexpr float VILE_SHOOT_RANGE = 20.0f;
constexpr float TRAP_DROP = 2.0f;
constexpr uint32 VILE_WAVE_MIN_MS = 2000;

constexpr float BOMB_KEEP = 13.0f;
constexpr float BOMB_SOON = 14.0f;
constexpr float LEADER_RELATCH = 2.0f;

constexpr uint8 WARDEN_KICK_SCANS = 2;
constexpr float TERENAS_HEAL_PCT = 80.0f;

constexpr uint32 LATE_SPELL_VILE_SPIRITS = 70498;
constexpr uint32 LATE_SPELL_SOUL_RIP = 69397;

constexpr uint32 LATE_SPELL_DIVINE_SHIELD = 642;
constexpr uint32 LATE_SPELL_FORBEARANCE = 25771;
constexpr uint32 LATE_SPELL_DISPERSION = 47585;
constexpr uint32 LATE_SPELL_CLOAK = 31224;
constexpr uint32 LATE_SPELL_ICE_BLOCK = 45438;
constexpr uint32 LATE_SPELL_HYPOTHERMIA = 41425;

char const* const KICKS[] = {"kick", "pummel", "shield bash", "mind freeze", "counterspell", "wind shear"};

enum class WalkKind : uint8
{
    None,
    DefileRun,
    DefileClear,
    Puddle,
    Soak,
    Trap,
    FrostmourneLead,
    FrostmourneFollow,
    Stack,
    Round,
};

struct ValkyrInfo
{
    ObjectGuid guid;
    Position pos;
    bool carrier = false;
    bool siphoner = false;
    float healthPct = 100.0f;
};

struct PuddleInfo
{
    ObjectGuid guid;
    Position pos;
    float radius = 0.0f;
};

struct SpiritInfo
{
    ObjectGuid guid;
    Position pos;
    bool woken = false;
};

struct ControlCast
{
    ObjectGuid by;
    uint32 castMs = 0;
};

struct LateBot
{
    IccLichKingPhaseId phase = IccLichKingPhaseId::One;

    WalkKind walk = WalkKind::None;
    Position walkSpot;
    bool walkParked = false;
    bool owned = false;
    bool reachHeld = false;

    uint32 defileFor = 0;
    bool defileValid = false;
    Position defileSpot;
    uint32 defileBlockedMs = 0;

    ObjectGuid puddle;
    bool puddleValid = false;
    Position puddleSpot;
    uint32 puddleBlockedMs = 0;

    ObjectGuid chase;
    bool roundValid = false;
    Position roundStep;

    uint32 guard = 0;
    bool trapDue = false;

    ObjectGuid controlTarget;
    uint32 controlSpell = 0;
    bool controlStun = false;

    LkHarvestDuty harvest = LkHarvestDuty::None;
    ObjectGuid harvestTarget;
    std::string harvestSpell;
};

struct TargetMemo
{
    uint32 ms = 0;
    bool set = false;
    Unit* pick = nullptr;
    char const* rule = nullptr;
};

struct LateState
{
    uint32 platformScanMs = 0;
    bool platformScanned = false;
    std::vector<ValkyrInfo> valkyrs;
    std::vector<PuddleInfo> puddles;
    std::vector<SpiritInfo> viles;

    uint32 fmScanMs = 0;
    bool fmScanned = false;
    std::vector<SpiritInfo> bombs;
    std::vector<SpiritInfo> wicked;
    ObjectGuid warden;
    ObjectGuid terenas;
    uint8 ripScans = 0;

    uint32 platformDutyMs = 0;
    bool platformDutiesRan = false;
    uint32 fmDutyMs = 0;
    bool fmDutiesRan = false;

    ObjectGuid holder;
    ObjectGuid freeTank;

    uint8 frameFor = 0;
    float frame = 0.0f;
    Position anchor;
    Position holderSpot;
    Position freeSpot;
    Position stackCentre;
    bool stackValid = false;
    std::vector<Position> nodes;
    std::unordered_map<ObjectGuid, size_t> node;

    RaidObs::ObsValue<uint8> side{"lk.side"};
    uint8 oldSide = 0;
    bool vileCasting = false;

    uint32 defileCastMs = 0;
    ObjectGuid defileTarget;
    bool defileActive = false;
    bool defileRunValid = false;
    Position defileRunSpot;

    std::vector<ObjectGuid> carriers;
    std::unordered_map<ObjectGuid, ObjectGuid> bucket;
    std::unordered_map<ObjectGuid, ControlCast> stunCast;
    std::unordered_map<ObjectGuid, ControlCast> slowCast;
    std::unordered_map<ObjectGuid, ObjectGuid> stunner;
    std::unordered_map<ObjectGuid, ObjectGuid> slower;
    std::unordered_set<ObjectGuid> hammered;
    std::unordered_map<ObjectGuid, uint32> siphonTaunted;

    bool wave = false;
    uint32 waveMs = 0;
    ObjectGuid soaker;
    ObjectGuid trapper;
    bool trapDropped = false;
    bool soakValid = false;
    Position soakSpot;
    Position trapSpot;

    ObjectGuid leader;
    bool leaderLatched = false;
    Position leaderPos;
    bool dodging = false;
    bool dodgeSoonOnly = false;
    Position dodgeSpot;

    std::unordered_map<ObjectGuid, LateBot> bots;
    std::unordered_map<ObjectGuid, TargetMemo> targets;
};

RaidInstanceState<LateState> lateStates;

using Accept = std::function<bool(Position const&)>;

bool IsHeroicMap(Player* bot)
{
    Difficulty const difficulty = GetRaidMapDifficulty(bot);
    return difficulty == RAID_DIFFICULTY_10MAN_HEROIC || difficulty == RAID_DIFFICULTY_25MAN_HEROIC;
}

void NoteDerived(Player* bot, char const* key, std::string const& value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
}

std::string Counter(ObjectGuid const& guid) { return std::to_string(guid.GetCounter()); }

float FloorZ() { return LK_CENTER.GetPositionZ(); }
float FmZ() { return LK_FROSTMOURNE.GetPositionZ(); }

bool InFrostmourne(Player* player) { return IccLichKingInFrostmourne(player); }

bool OnPlatform(Player* player)
{
    return !InFrostmourne(player) && !player->GetVehicle() && player->GetExactDist2d(LK_CENTER) <= PLATFORM_REACH;
}

Position PointAt(Position const& centre, float deg, float r)
{
    float const angle = deg * static_cast<float>(M_PI) / 180.0f;
    return Position(centre.GetPositionX() + r * std::cos(angle), centre.GetPositionY() + r * std::sin(angle),
                    centre.GetPositionZ());
}

float SideDeg(uint8 side) { return side == SIDE_WEST ? SIDE_WEST_DEG : SIDE_EAST_DEG; }
char const* SideName(uint8 side) { return side == SIDE_WEST ? "west" : "east"; }

Position SideSpot(uint8 side)
{
    return PointAt(Position(LK_CENTER.GetPositionX(), LK_CENTER.GetPositionY(), FloorZ()), SideDeg(side), SIDE_R);
}

Player* PlayerOf(Player* bot, ObjectGuid const& guid)
{
    Player* player = guid ? ObjectAccessor::GetPlayer(*bot, guid) : nullptr;
    return player && player->IsAlive() && IccSameInstance(bot, player) ? player : nullptr;
}

Creature* CreatureOf(Player* bot, ObjectGuid const& guid)
{
    Creature* unit = guid ? ObjectAccessor::GetCreature(*bot, guid) : nullptr;
    return unit && unit->IsAlive() ? unit : nullptr;
}

Unit* Pickable(Player* bot, Creature* unit)
{
    return unit && unit->IsAlive() && unit->IsInCombat() && !unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) &&
                   bot->IsValidAttackTarget(unit)
               ? unit
               : nullptr;
}

bool HarvestStunned(Player* player, Unit* boss)
{
    return player->HasUnitState(UNIT_STATE_STUNNED) &&
           (player->HasAura(sSpellMgr->GetSpellIdForDifficulty(ICC_LK_HARVEST_SOUL[0], boss)) ||
            player->HasAura(sSpellMgr->GetSpellIdForDifficulty(ICC_LK_HARVEST_SOULS[0], boss)));
}

bool InReach(Player* bot, Unit* unit)
{
    return PlayerbotAI::IsRanged(bot) ? bot->IsWithinCombatRange(unit, RANGED_REACH) : bot->IsWithinMeleeRange(unit);
}

bool PlatformAccepts(Player* bot, Position const& p)
{
    return p.GetExactDist2d(LK_CENTER) <= FLOOR_MAX_R && LkFloorAccepts(bot, p.GetPositionX(), p.GetPositionY());
}

bool PuddleFouls(LateState const& st, Position const& p)
{
    return std::any_of(st.puddles.begin(), st.puddles.end(), [&](PuddleInfo const& puddle)
                       { return puddle.pos.GetExactDist2d(p) < puddle.radius + PUDDLE_STAND; });
}

bool PuddleTrips(LateState const& st, Position const& p)
{
    return std::any_of(st.puddles.begin(), st.puddles.end(), [&](PuddleInfo const& puddle)
                       { return puddle.pos.GetExactDist2d(p) < puddle.radius + PUDDLE_TRIGGER; });
}

bool SegmentClear(std::vector<PuddleInfo> const& puddles, Position const& from, Position const& to,
                  Position const* inside = nullptr)
{
    for (PuddleInfo const& puddle : puddles)
    {
        if (inside && puddle.pos.GetExactDist2d(*inside) < puddle.radius + PUDDLE_TRIGGER)
            continue;

        if (SegmentCrossesCircle(from, to, HazardCircle(puddle.pos, puddle.radius + PUDDLE_SEGMENT)))
            return false;
    }

    return true;
}

bool ClearOf(std::vector<HazardCircle> const& circles, Position const& p)
{
    for (HazardCircle const& circle : circles)
        if (circle.first.GetExactDist2d(p) < circle.second)
            return false;

    return true;
}

bool Sweep(Position const& from, std::vector<HazardCircle> const& circles, float maxRadius, float step,
           Accept const& accept, Position const* preferNear, float z, Position& out)
{
    constexpr int BEARINGS = 16;
    for (float r = step; r <= maxRadius + 0.01f; r += step)
    {
        bool found = false;
        float best = 0.0f;
        for (int i = 0; i < BEARINGS; ++i)
        {
            float const angle = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / BEARINGS;
            Position const p(from.GetPositionX() + r * std::cos(angle), from.GetPositionY() + r * std::sin(angle), z);
            if (!ClearOf(circles, p) || !accept(p))
                continue;

            if (!preferNear)
            {
                out = p;
                return true;
            }

            float const score = preferNear->GetExactDist2d(p);
            if (!found || score < best)
            {
                out = p;
                best = score;
                found = true;
            }
        }

        if (found)
            return true;
    }

    return false;
}

bool PlanStep(LateState const& st, Player* bot, Position const& dest, Position& out)
{
    Position const from = bot->GetPosition();
    PuddleInfo const* block = nullptr;
    float blockDistance = 0.0f;
    for (PuddleInfo const& puddle : st.puddles)
    {
        if (!SegmentCrossesCircle(from, dest, HazardCircle(puddle.pos, puddle.radius + PUDDLE_SEGMENT)))
            continue;

        float const distance = from.GetExactDist2d(puddle.pos);
        if (!block || distance < blockDistance)
        {
            block = &puddle;
            blockDistance = distance;
        }
    }

    if (!block)
    {
        out = dest;
        return true;
    }

    float const dx = dest.GetPositionX() - from.GetPositionX();
    float const dy = dest.GetPositionY() - from.GetPositionY();
    float const length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.01f)
    {
        out = dest;
        return true;
    }

    float const px = -dy / length;
    float const py = dx / length;
    for (float const extra : {PUDDLE_STAND + 1.0f, PUDDLE_STAND + 4.0f})
    {
        bool found = false;
        float best = 0.0f;
        for (float const side : {1.0f, -1.0f})
        {
            float const r = block->radius + extra;
            Position const waypoint(block->pos.GetPositionX() + px * side * r,
                                    block->pos.GetPositionY() + py * side * r, dest.GetPositionZ());
            if (!PlatformAccepts(bot, waypoint) || !SegmentClear(st.puddles, from, waypoint) ||
                !SegmentClear(st.puddles, waypoint, dest))
                continue;

            float const cost = from.GetExactDist2d(waypoint) + waypoint.GetExactDist2d(dest);
            if (!found || cost < best)
            {
                out = waypoint;
                best = cost;
                found = true;
            }
        }

        if (found)
            return true;
    }

    return false;
}

uint32 AuraLeft(Unit* unit, AuraType type)
{
    int32 left = 0;
    for (AuraEffect const* effect : unit->GetAuraEffectsByType(type))
    {
        Aura const* aura = effect->GetBase();
        if (aura && effect->GetCasterGUID() != unit->GetGUID())
            left = std::max(left, aura->GetDuration());
    }

    return left > 0 ? static_cast<uint32>(left) : 0;
}

char const* ClassTauntName(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
            return "taunt";
        case CLASS_PALADIN:
            return "hand of reckoning";
        case CLASS_DEATH_KNIGHT:
            return "dark command";
        case CLASS_DRUID:
            return "growl";
        default:
            return nullptr;
    }
}

bool TauntedBy(Unit* unit, Player* bot)
{
    for (AuraEffect const* effect : unit->GetAuraEffectsByType(SPELL_AURA_MOD_TAUNT))
        if (effect->GetCasterGUID() == bot->GetGUID())
            return true;

    return false;
}

bool HoldsRagingSpirit(Player* tank)
{
    for (Unit* attacker : tank->getAttackers())
        if (attacker && attacker->IsAlive() && attacker->GetEntry() == NPC_LK_RAGING_SPIRIT)
            return true;

    return false;
}

uint32 ImmunityReady(Player* member)
{
    auto const ready = [member](uint32 spellId)
    { return member->HasSpell(spellId) && !member->HasSpellCooldown(spellId); };
    if (ready(LATE_SPELL_DIVINE_SHIELD) && !member->HasAura(LATE_SPELL_FORBEARANCE))
        return LATE_SPELL_DIVINE_SHIELD;

    if (ready(LATE_SPELL_DISPERSION))
        return LATE_SPELL_DISPERSION;

    if (ready(LATE_SPELL_CLOAK))
        return LATE_SPELL_CLOAK;

    if (ready(LATE_SPELL_ICE_BLOCK) && !member->HasAura(LATE_SPELL_HYPOTHERMIA))
        return LATE_SPELL_ICE_BLOCK;

    return 0;
}

bool Guarded(Player* bot)
{
    return bot->HasAura(LATE_SPELL_DIVINE_SHIELD) || bot->HasAura(LATE_SPELL_DISPERSION) ||
           bot->HasAura(LATE_SPELL_CLOAK) || bot->HasAura(LATE_SPELL_ICE_BLOCK) ||
           (IsLkTankPlayer(bot) && RunningTankDefensive(bot, false, 0) != nullptr);
}

uint32 GuardSpell(PlayerbotAI* botAI, Player* bot)
{
    if (uint32 const immunity = ImmunityReady(bot))
        return immunity;

    return IsLkTankPlayer(bot) ? NextTankDefensive(botAI, bot, nullptr, false) : 0;
}

bool FrostTrapReady(Player* member)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    if (!memberAI || member->getClass() != CLASS_HUNTER)
        return false;

    uint32 const spellId = memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", "frost trap")->Get();
    return spellId && !member->HasSpellCooldown(spellId);
}

bool CanPay(Player* bot, SpellInfo const* info)
{
    if (info->PowerType >= static_cast<uint32>(MAX_POWERS))
        return true;

    return static_cast<int32>(bot->GetPower(Powers(info->PowerType))) >=
           info->CalcPowerCost(bot, info->GetSchoolMask());
}

char const* ReadyKick(PlayerbotAI* botAI, Unit* target)
{
    Player* bot = botAI->GetBot();
    if (bot->HasUnitState(UNIT_STATE_CASTING))
        return nullptr;

    for (char const* kick : KICKS)
    {
        uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", kick)->Get();
        SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        if (!info || bot->HasSpellCooldown(spellId) || !CanPay(bot, info) ||
            info->CheckShapeshift(bot->GetShapeshiftForm()) != SPELL_CAST_OK)
            continue;

        if (botAI->CanCastSpell(spellId, target))
            return kick;
    }

    return nullptr;
}

char const* DispelName(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_PRIEST:
            return "dispel magic";
        case CLASS_PALADIN:
            return "cleanse";
        default:
            return nullptr;
    }
}

LateState* StateOf(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->GetInstanceId() || !GetLkBoss(botAI))
        return nullptr;

    return lateStates.Find(bot->GetInstanceId());
}

LateState* LiveStateOf(PlayerbotAI* botAI, Creature** boss = nullptr)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->GetInstanceId())
        return nullptr;

    Creature* unit = GetLkBoss(botAI);
    if (boss)
        *boss = unit;

    if (!unit || !IsLkLive(botAI))
        return nullptr;

    return lateStates.Find(bot->GetInstanceId());
}

LateBot* BotOf(LateState& st, Player* bot)
{
    auto const itr = st.bots.find(bot->GetGUID());
    return itr != st.bots.end() ? &itr->second : nullptr;
}

LateBot const* BotOf(LateState const& st, Player* bot)
{
    auto const itr = st.bots.find(bot->GetGUID());
    return itr != st.bots.end() ? &itr->second : nullptr;
}

Creature* CarrierOf(LateState const& st, Player* bot, ObjectGuid const& guid)
{
    bool const carrier = std::any_of(st.valkyrs.begin(), st.valkyrs.end(), [&](ValkyrInfo const& valkyr)
                                     { return valkyr.guid == guid && valkyr.carrier; });
    return carrier ? CreatureOf(bot, guid) : nullptr;
}

void ScanPlatform(LateState& st, Player* bot, uint32 now)
{
    if (st.platformScanned && getMSTimeDiff(st.platformScanMs, now) < SCAN_MS)
        return;

    st.platformScanned = true;
    st.platformScanMs = now;
    st.valkyrs.clear();
    st.puddles.clear();
    st.viles.clear();

    bool const heroic = IsHeroicMap(bot);
    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_LK_VALKYR, PLATFORM_SCAN);
    for (Creature* unit : found)
    {
        if (!unit->IsAlive() || unit->GetExactDist2d(LK_CENTER) > PLATFORM_REACH)
            continue;

        bool const carrying = unit->HasAura(SPELL_HARVEST_SOUL_VALKYR);
        ValkyrInfo info;
        info.guid = unit->GetGUID();
        info.pos = unit->GetPosition();
        info.healthPct = unit->GetHealthPct();
        info.carrier = carrying && (!heroic || unit->GetHealthPct() > HEROIC_DROP_PCT);
        info.siphoner = heroic && !carrying && !unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE) &&
                        unit->GetPositionZ() > SIPHON_Z && unit->GetExactDist2d(LK_CENTER) <= SIPHON_RANGE;
        st.valkyrs.push_back(info);
    }

    found.clear();
    bot->GetCreatureListWithEntryInGrid(found, DEFILE_NPC_ID, PLATFORM_SCAN);
    for (Creature* unit : found)
        if (unit->IsAlive() && unit->GetExactDist2d(LK_CENTER) <= PLATFORM_REACH)
            st.puddles.push_back(
                {unit->GetGUID(), unit->GetPosition(), DEFILE_RADIUS_PER_SCALE * unit->GetObjectScale()});

    found.clear();
    bot->GetCreatureListWithEntryInGrid(found, NPC_LK_VILE_SPIRIT, PLATFORM_SCAN);
    for (Creature* unit : found)
    {
        if (!unit->IsAlive() || unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) ||
            unit->GetExactDist2d(LK_CENTER) > PLATFORM_REACH)
            continue;

        st.viles.push_back({unit->GetGUID(), unit->GetPosition(), !unit->HasReactState(REACT_PASSIVE)});
    }
}

void ScanFrostmourne(LateState& st, Player* bot, uint32 now)
{
    if (st.fmScanned && getMSTimeDiff(st.fmScanMs, now) < SCAN_MS)
        return;

    st.fmScanned = true;
    st.fmScanMs = now;
    st.bombs.clear();
    st.wicked.clear();
    st.warden.Clear();
    st.terenas.Clear();

    auto const inside = [](Creature* unit)
    { return unit->IsAlive() && unit->GetExactDist2d(LK_FROSTMOURNE) <= FM_ROOM; };

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_SPIRIT_BOMB, FM_SCAN);
    for (Creature* unit : found)
        if (inside(unit))
            st.bombs.push_back({unit->GetGUID(), unit->GetPosition(), true});

    found.clear();
    bot->GetCreatureListWithEntryInGrid(found, NPC_LK_WICKED_SPIRIT, FM_SCAN);
    for (Creature* unit : found)
        if (inside(unit) && !unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            st.wicked.push_back({unit->GetGUID(), unit->GetPosition(), unit->HasReactState(REACT_AGGRESSIVE)});

    Creature* warden = nullptr;
    found.clear();
    bot->GetCreatureListWithEntryInGrid(found, NPC_LK_SPIRIT_WARDEN, FM_SCAN);
    for (Creature* unit : found)
        if (inside(unit))
            warden = unit;

    if (warden)
        st.warden = warden->GetGUID();

    bool const ripping = warden && warden->FindCurrentSpellBySpellId(LATE_SPELL_SOUL_RIP);
    st.ripScans = ripping ? static_cast<uint8>(std::min<uint32>(st.ripScans + 1u, 255u)) : 0;

    found.clear();
    bot->GetCreatureListWithEntryInGrid(found, NPC_TERENAS_MENETHIL, FM_SCAN);
    for (Creature* unit : found)
        if (inside(unit))
            st.terenas = unit->GetGUID();
}

bool PointFouled(LateState const& st, Player* bot, Position const& p)
{
    return !PlatformAccepts(bot, p) || PuddleFouls(st, p);
}

bool SetFouled(LateState const& st, Player* bot, Position const& anchor, float frame)
{
    return PointFouled(st, bot, anchor) ||
           PointFouled(st, bot, PointAt(anchor, frame + HOLDER_OFFSET.deg, HOLDER_OFFSET.r)) ||
           PointFouled(st, bot, PointAt(anchor, frame + STACK_OFFSET.deg, STACK_OFFSET.r));
}

Position SlideAnchor(LateState const& st, Player* bot, float frame, Position const& preferred)
{
    Accept const accept = [&](Position const& p)
    { return p.GetExactDist2d(LK_CENTER) <= ANCHOR_MAX_R && !SetFouled(st, bot, p, frame); };

    Position out;
    if (Sweep(st.anchor, std::vector<HazardCircle>(), ANCHOR_SLIDE_MAX, ANCHOR_SLIDE_STEP, accept, &preferred,
              FloorZ(), out))
        return out;

    return st.anchor;
}

void UpdateStack(LateState& st, Player* bot, std::vector<Player*> const& members, IccLichKingPhaseId phase)
{
    if (phase != IccLichKingPhaseId::Two && phase != IccLichKingPhaseId::Three)
    {
        st.stackValid = false;
        st.frameFor = 0;
        st.nodes.clear();
        st.node.clear();
        return;
    }

    bool const two = phase == IccLichKingPhaseId::Two;
    uint8 const frameFor = two ? 1 : (st.side.Get() == SIDE_WEST ? 3 : 2);
    float const frame = two ? 0.0f : SideDeg(st.side.Get());
    Position const centre(LK_CENTER.GetPositionX(), LK_CENTER.GetPositionY(), FloorZ());
    Position const preferred = two ? centre : PointAt(centre, frame, SIDE_R);
    if (frameFor != st.frameFor)
    {
        st.frameFor = frameFor;
        st.anchor = preferred;
    }

    if (!SetFouled(st, bot, preferred, frame))
        st.anchor = preferred;
    else if (SetFouled(st, bot, st.anchor, frame))
        st.anchor = SlideAnchor(st, bot, frame, preferred);

    st.frame = frame;
    st.holderSpot = PointAt(st.anchor, frame + HOLDER_OFFSET.deg, HOLDER_OFFSET.r);
    st.freeSpot = PointAt(st.anchor, frame + FREE_OFFSET.deg, FREE_OFFSET.r);
    st.stackCentre = PointAt(st.anchor, frame + STACK_OFFSET.deg, STACK_OFFSET.r);
    st.stackValid = true;

    std::vector<std::pair<float, Position>> nodes;
    float const rowHeight = LATTICE_STEP * std::sqrt(3.0f) / 2.0f;
    for (int j = -2; j <= 2; ++j)
        for (int i = -2; i <= 2; ++i)
        {
            float const ox = LATTICE_STEP * (static_cast<float>(i) + 0.5f * static_cast<float>(j));
            float const oy = rowHeight * static_cast<float>(j);
            if (std::sqrt(ox * ox + oy * oy) > LATTICE_RADIUS + 0.01f)
                continue;

            Position const p(st.stackCentre.GetPositionX() + ox, st.stackCentre.GetPositionY() + oy, FloorZ());
            if (!PointFouled(st, bot, p))
                nodes.emplace_back(p.GetExactDist2d(st.anchor), p);
        }

    std::stable_sort(nodes.begin(), nodes.end(), [](auto const& a, auto const& b) { return a.first < b.first; });
    st.nodes.clear();
    for (auto const& entry : nodes)
        st.nodes.push_back(entry.second);

    std::vector<Player*> melee;
    std::vector<Player*> others;
    for (Player* member : members)
    {
        if (!IsLkDutyBot(member) || !member->IsAlive() || member->GetGUID() == st.holder ||
            member->GetGUID() == st.freeTank)
            continue;

        (PlayerbotAI::IsRanged(member) || IsLkHealer(member) ? others : melee).push_back(member);
    }

    std::sort(melee.begin(), melee.end(), IccGuidLess());
    std::sort(others.begin(), others.end(), IccGuidLess());
    st.node.clear();
    if (st.nodes.empty())
        return;

    size_t rank = 0;
    for (Player* member : melee)
        st.node[member->GetGUID()] = rank++ % st.nodes.size();
    for (Player* member : others)
        st.node[member->GetGUID()] = rank++ % st.nodes.size();
}

void DealBuckets(LateState& st, std::vector<Player*> const& pool, std::vector<ValkyrInfo const*> const& carriers)
{
    st.bucket.clear();
    std::vector<size_t> left = IccBalancedGroupSizes(pool.size(), carriers.size());
    std::vector<Player*> melee;
    std::vector<Player*> ranged;
    for (Player* member : pool)
        (PlayerbotAI::IsRanged(member) ? ranged : melee).push_back(member);

    std::sort(melee.begin(), melee.end(), IccGuidLess());
    std::sort(ranged.begin(), ranged.end(), IccGuidLess());

    for (Player* member : melee)
    {
        size_t best = carriers.size();
        float bestDistance = 0.0f;
        for (size_t i = 0; i < carriers.size(); ++i)
        {
            float const distance = member->GetExactDist2d(carriers[i]->pos);
            if (left[i] && (best == carriers.size() || distance < bestDistance))
            {
                best = i;
                bestDistance = distance;
            }
        }

        if (best == carriers.size())
            continue;

        --left[best];
        st.bucket[member->GetGUID()] = carriers[best]->guid;
    }

    size_t index = 0;
    for (Player* member : ranged)
    {
        while (index < carriers.size() && !left[index])
            ++index;

        if (index == carriers.size())
            break;

        --left[index];
        st.bucket[member->GetGUID()] = carriers[index]->guid;
    }
}

void Elect(LateState& st, bool stun, ObjectGuid const& guid, Creature* valkyr, std::vector<Player*> const& duty,
           std::unordered_set<ObjectGuid>& busy, uint32 now)
{
    ControlCast const& last = (stun ? st.stunCast : st.slowCast)[guid];
    uint32 const left = AuraLeft(valkyr, stun ? SPELL_AURA_MOD_STUN : SPELL_AURA_MOD_DECREASE_SPEED);
    if (left > CONTROL_LEAD_MS || (last.castMs && getMSTimeDiff(last.castMs, now) < CONTROL_FLIGHT_MS))
        return;

    for (Player* member : duty)
    {
        if (busy.count(member->GetGUID()) || member->GetGUID() == st.holder)
            continue;

        uint32 const spellId = stun ? LkReadyStun(member, valkyr) : LkReadySlow(member, valkyr);
        if (!spellId)
            continue;

        (stun ? st.stunner : st.slower)[member->GetGUID()] = guid;
        busy.insert(member->GetGUID());
        return;
    }
}

void UpdateValkyrs(LateState& st, Player* bot, std::vector<Player*> const& members, IccLichKingPhaseId phase,
                   uint32 now)
{
    st.stunner.clear();
    st.slower.clear();

    std::unordered_set<ObjectGuid> present;
    for (ValkyrInfo const& info : st.valkyrs)
        present.insert(info.guid);

    for (auto* casts : {&st.stunCast, &st.slowCast})
        for (auto itr = casts->begin(); itr != casts->end();)
            itr = present.count(itr->first) ? std::next(itr) : casts->erase(itr);

    for (auto itr = st.siphonTaunted.begin(); itr != st.siphonTaunted.end();)
        itr = present.count(itr->first) ? std::next(itr) : st.siphonTaunted.erase(itr);

    std::vector<ValkyrInfo const*> carriers;
    if (phase == IccLichKingPhaseId::Two)
        for (ValkyrInfo const& info : st.valkyrs)
            if (info.carrier)
                carriers.push_back(&info);

    std::sort(carriers.begin(), carriers.end(),
              [](ValkyrInfo const* a, ValkyrInfo const* b) { return a->guid < b->guid; });

    if (carriers.empty())
    {
        st.carriers.clear();
        st.bucket.clear();
        return;
    }

    std::vector<Player*> duty;
    for (Player* member : members)
        if (IsLkDutyBot(member) && member->IsAlive() && OnPlatform(member))
            duty.push_back(member);

    std::sort(duty.begin(), duty.end(), IccGuidLess());

    std::vector<ObjectGuid> set;
    for (ValkyrInfo const* info : carriers)
        set.push_back(info->guid);

    if (set != st.carriers)
    {
        st.carriers = set;
        std::vector<Player*> pool;
        for (Player* member : duty)
        {
            if (member->GetGUID() == st.holder)
                continue;

            bool const freeTank = member->GetGUID() == st.freeTank && !HoldsRagingSpirit(member);
            if (IsRaidDpsBot(member) || freeTank)
                pool.push_back(member);
        }

        DealBuckets(st, pool, carriers);
    }

    std::unordered_set<ObjectGuid> busy;
    for (bool const stun : {true, false})
        for (ValkyrInfo const* info : carriers)
            if (Creature* valkyr = CreatureOf(bot, info->guid))
                Elect(st, stun, info->guid, valkyr, duty, busy, now);

    if (!sPlayerbotAIConfig.EnableICCBuffs || !IsHeroicMap(bot))
        return;

    for (ValkyrInfo const* info : carriers)
    {
        Creature* valkyr = CreatureOf(bot, info->guid);
        if (!valkyr || st.hammered.count(info->guid) || valkyr->HasAura(SPELL_HAMMER_OF_JUSTICE))
            continue;

        bot->AddAura(SPELL_HAMMER_OF_JUSTICE, valkyr);
        IccNoteCheat(bot, "lk.valkyrstun", valkyr);
        st.hammered.insert(info->guid);
    }
}

ObjectGuid ElectSoaker(LateState const& st, std::vector<Player*> const& members)
{
    Player* best = nullptr;
    bool bestHealer = false;
    for (Player* member : members)
    {
        if (!IsLkDutyBot(member) || !member->IsAlive() || !OnPlatform(member) || member->GetGUID() == st.holder ||
            !ImmunityReady(member))
            continue;

        bool const healer = IsLkHealer(member);
        if (!best || (healer != bestHealer ? !healer : member->GetGUID() < best->GetGUID()))
        {
            best = member;
            bestHealer = healer;
        }
    }

    return best ? best->GetGUID() : st.freeTank;
}

ObjectGuid ElectTrapper(LateState const& st, std::vector<Player*> const& members)
{
    Player* best = nullptr;
    for (Player* member : members)
        if (IsLkDutyBot(member) && member->IsAlive() && OnPlatform(member) && member->GetGUID() != st.soaker &&
            FrostTrapReady(member) && (!best || member->GetGUID() < best->GetGUID()))
            best = member;

    return best ? best->GetGUID() : ObjectGuid::Empty;
}

void UpdateVileWave(LateState& st, Player* bot, std::vector<Player*> const& members, IccLichKingPhaseId phase,
                    uint32 now)
{
    if (!st.wave)
        return;

    if (getMSTimeDiff(st.waveMs, now) > VILE_WAVE_MIN_MS && st.viles.empty())
    {
        st.wave = false;
        st.soaker.Clear();
        st.trapper.Clear();
        st.soakValid = false;
        return;
    }

    if (phase != IccLichKingPhaseId::Three || !st.stackValid)
    {
        st.soakValid = false;
        return;
    }

    Position const old = SideSpot(st.oldSide);
    float const dx = old.GetPositionX() - st.stackCentre.GetPositionX();
    float const dy = old.GetPositionY() - st.stackCentre.GetPositionY();
    float const length = std::sqrt(dx * dx + dy * dy);
    st.soakValid = false;
    if (length > 0.1f)
    {
        st.soakSpot = Position(st.stackCentre.GetPositionX() + dx / length * SOAK_OFFSET,
                               st.stackCentre.GetPositionY() + dy / length * SOAK_OFFSET, FloorZ());
        st.trapSpot = Position((st.stackCentre.GetPositionX() + st.soakSpot.GetPositionX()) / 2.0f,
                               (st.stackCentre.GetPositionY() + st.soakSpot.GetPositionY()) / 2.0f, FloorZ());
        st.soakValid = PlatformAccepts(bot, st.soakSpot) && !PuddleFouls(st, st.soakSpot);
    }

    Player* soaker = PlayerOf(bot, st.soaker);
    if (!soaker || !OnPlatform(soaker))
        st.soaker = ElectSoaker(st, members);

    Player* trapper = PlayerOf(bot, st.trapper);
    if (!st.trapDropped && (!trapper || !OnPlatform(trapper) || !FrostTrapReady(trapper)))
        st.trapper = ElectTrapper(st, members);
}

bool FindRunSpot(LateState const& st, Player* finder, Player* target, Position& out)
{
    std::vector<HazardCircle> circles;
    for (Player* member : LkMembersOnMap(finder))
        if (member != target && member->IsAlive() && OnPlatform(member))
            circles.emplace_back(member->GetPosition(), DEFILE_RUN_MEMBERS);

    for (PuddleInfo const& puddle : st.puddles)
        circles.emplace_back(puddle.pos, puddle.radius + DEFILE_RUN_PUDDLES);

    Position const from = target->GetPosition();
    Position const centre = st.stackValid ? st.stackCentre : LK_CENTER;
    float const sx = from.GetPositionX() - centre.GetPositionX();
    float const sy = from.GetPositionY() - centre.GetPositionY();
    bool const sided = std::sqrt(sx * sx + sy * sy) >= SIDE_UNDEFINED;

    Accept const accept = [&](Position const& p)
    {
        if (p.GetExactDist2d(LK_CENTER) > ANCHOR_MAX_R)
            return false;

        float const along =
            (p.GetPositionX() - centre.GetPositionX()) * sx + (p.GetPositionY() - centre.GetPositionY()) * sy;
        if (sided && along < 0.0f)
            return false;

        return SegmentClear(st.puddles, from, p, &from) && PlatformAccepts(finder, p);
    };

    if (Sweep(from, circles, DEFILE_RUN_SWEEP, SWEEP_STEP, accept, nullptr, FloorZ(), out))
        return true;

    float dx = sided ? sx : from.GetPositionX() - LK_CENTER.GetPositionX();
    float dy = sided ? sy : from.GetPositionY() - LK_CENTER.GetPositionY();
    float const length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.1f)
    {
        dx = 1.0f;
        dy = 0.0f;
    }
    else
    {
        dx /= length;
        dy /= length;
    }

    for (float const distance : DEFILE_RUN_FALLBACK)
    {
        Position const p(from.GetPositionX() + dx * distance, from.GetPositionY() + dy * distance, FloorZ());
        if (accept(p) && !PuddleFouls(st, p))
        {
            out = p;
            return true;
        }
    }

    return false;
}

bool FindClearSpot(LateState const& st, Player* bot, Player* target, Position const* prefer, Position& out)
{
    std::vector<HazardCircle> circles;
    circles.emplace_back(target->GetPosition(), DEFILE_CLEAR + PARK);
    if (st.defileRunValid)
        circles.emplace_back(st.defileRunSpot, DEFILE_CLEAR + PARK);

    for (PuddleInfo const& puddle : st.puddles)
        circles.emplace_back(puddle.pos, puddle.radius + PUDDLE_STAND);

    Position const from = bot->GetPosition();
    Accept const accept = [&](Position const& p)
    { return SegmentClear(st.puddles, from, p, &from) && PlatformAccepts(bot, p); };
    return Sweep(from, circles, DEFILE_CLEAR_SWEEP, SWEEP_STEP, accept, prefer, FloorZ(), out);
}

bool FindPuddleExit(LateState const& st, Player* bot, Position const* prefer, Position& out)
{
    std::vector<HazardCircle> circles;
    for (PuddleInfo const& puddle : st.puddles)
        circles.emplace_back(puddle.pos, puddle.radius + PUDDLE_STAND);

    Position const from = bot->GetPosition();
    Accept const accept = [&](Position const& p)
    { return SegmentClear(st.puddles, from, p, &from) && PlatformAccepts(bot, p); };
    return Sweep(from, circles, PUDDLE_SWEEP, SWEEP_STEP, accept, prefer, FloorZ(), out);
}

void DetectDefileCast(LateState& st, Player* bot, uint32 now)
{
    IcecrownHelpers::DefileCastInfo const& cast = IcecrownHelpers::IccState(bot->GetInstanceId()).defileCast;
    uint32 const window = DEFILE_CAST_MS + DEFILE_CAST_GRACE_MS;
    if (cast.targetGuid && cast.castTime != st.defileCastMs && getMSTimeDiff(cast.castTime, now) <= window)
    {
        st.defileCastMs = cast.castTime;
        st.defileTarget = cast.targetGuid;
        st.defileRunValid = false;
        Player* target = PlayerOf(bot, cast.targetGuid);
        if (target && IsLkDutyBot(target) && OnPlatform(target))
            st.defileRunValid = FindRunSpot(st, bot, target, st.defileRunSpot);
    }

    st.defileActive = st.defileTarget && getMSTimeDiff(st.defileCastMs, now) <= window;
}

void DetectVileCast(LateState& st, Creature* boss, IccLichKingPhaseId phase, uint32 now)
{
    bool const casting = boss->FindCurrentSpellBySpellId(LATE_SPELL_VILE_SPIRITS) != nullptr;
    if (casting && !st.vileCasting && phase == IccLichKingPhaseId::Three && st.side.Get())
    {
        st.oldSide = st.side.Get();
        st.side = st.side.Get() == SIDE_EAST ? SIDE_WEST : SIDE_EAST;
        st.wave = true;
        st.waveMs = now;
        st.soaker.Clear();
        st.trapper.Clear();
        st.trapDropped = false;
        st.soakValid = false;
        st.platformDutiesRan = false;
    }

    st.vileCasting = casting;
}

void RefreshPlatformDuties(LateState& st, PlayerbotAI* botAI, IccLichKingPhaseId phase, uint32 now)
{
    if (st.platformDutiesRan && getMSTimeDiff(st.platformDutyMs, now) < DUTY_MS)
        return;

    st.platformDutiesRan = true;
    st.platformDutyMs = now;

    Player* bot = botAI->GetBot();
    std::vector<Player*> const members = LkMembersOnMap(bot);
    Player* holder = GetLkHolder(botAI);
    Player* freeTank = GetLkFreeTank(botAI);
    st.holder = holder ? holder->GetGUID() : ObjectGuid::Empty;
    st.freeTank = freeTank && freeTank != holder ? freeTank->GetGUID() : ObjectGuid::Empty;

    if (phase == IccLichKingPhaseId::Two || phase == IccLichKingPhaseId::Three)
    {
        st.leader.Clear();
        st.leaderLatched = false;
        st.dodging = false;
    }

    if (phase == IccLichKingPhaseId::Three && !st.side.Get())
        st.side = SIDE_EAST;

    UpdateStack(st, bot, members, phase);
    UpdateValkyrs(st, bot, members, phase, now);
    UpdateVileWave(st, bot, members, phase, now);
}

void RefreshFrostmourneDuties(LateState& st, PlayerbotAI* botAI, uint32 now)
{
    if (st.fmDutiesRan && getMSTimeDiff(st.fmDutyMs, now) < DUTY_MS)
        return;

    st.fmDutiesRan = true;
    st.fmDutyMs = now;

    Player* bot = botAI->GetBot();
    Player* tank = nullptr;
    Player* any = nullptr;
    if (IsHeroicMap(bot))
        for (Player* member : LkMembersOnMap(bot))
        {
            if (!IsLkDutyBot(member) || !member->IsAlive() || !InFrostmourne(member))
                continue;

            if (!any || member->GetGUID() < any->GetGUID())
                any = member;

            if (IsLkTankPlayer(member) && (!tank || member->GetGUID() < tank->GetGUID()))
                tank = member;
        }

    Player* leader = tank ? tank : any;
    ObjectGuid const guid = leader ? leader->GetGUID() : ObjectGuid::Empty;
    if (guid != st.leader)
    {
        st.leader = guid;
        st.leaderLatched = false;
        st.dodging = false;
    }
}

bool BombNear(LateState const& st, Position const& p, bool soonOnly)
{
    for (SpiritInfo const& bomb : st.bombs)
        if ((!soonOnly || bomb.pos.GetPositionZ() - FmZ() < BOMB_SOON) && bomb.pos.GetExactDist2d(p) < BOMB_KEEP)
            return true;

    return false;
}

bool FindBombExit(LateState const& st, Player* leader, bool soonOnly, Position& out)
{
    std::vector<HazardCircle> circles;
    for (SpiritInfo const& bomb : st.bombs)
        if (!soonOnly || bomb.pos.GetPositionZ() - FmZ() < BOMB_SOON)
            circles.emplace_back(bomb.pos, BOMB_KEEP + PARK);

    if (circles.empty())
        return false;

    Accept const accept = [leader](Position const& p)
    {
        return p.GetExactDist2d(LK_FROSTMOURNE) <= FM_SPOT_RANGE &&
               LkFloorAccepts(leader, p.GetPositionX(), p.GetPositionY());
    };
    return Sweep(leader->GetPosition(), circles, 2.0f * FM_SPOT_RANGE, SWEEP_STEP, accept, nullptr, FmZ(), out);
}

void LeaderDodge(LateState& st, Player* leader, std::string& note)
{
    if (st.dodging)
    {
        if (BombNear(st, st.dodgeSpot, st.dodgeSoonOnly))
            st.dodging = false;
        else if (leader->GetExactDist2d(st.dodgeSpot) <= PARK && !BombNear(st, leader->GetPosition(), false))
        {
            st.dodging = false;
            note = "safe";
            return;
        }
        else
        {
            note = "dodge";
            return;
        }
    }

    if (!BombNear(st, leader->GetPosition(), false))
    {
        note = "safe";
        return;
    }

    for (bool const soonOnly : {false, true})
        if (FindBombExit(st, leader, soonOnly, st.dodgeSpot))
        {
            st.dodging = true;
            st.dodgeSoonOnly = soonOnly;
            note = "dodge";
            return;
        }

    note = "stuck";
}

void FrostmourneWalk(LateState& st, Player* bot, WalkKind& kind, Position& spot, float& unpark, std::string& bombNote)
{
    if (bot->GetGUID() == st.leader)
    {
        LeaderDodge(st, bot, bombNote);
        if (st.dodging)
        {
            kind = WalkKind::FrostmourneLead;
            spot = st.dodgeSpot;
        }

        if (!st.leaderLatched || bot->GetExactDist2d(st.leaderPos) > LEADER_RELATCH)
        {
            st.leaderPos = Position(bot->GetPositionX(), bot->GetPositionY(), FmZ());
            st.leaderLatched = true;
        }

        return;
    }

    if (!st.leader || !st.leaderLatched)
        return;

    kind = WalkKind::FrostmourneFollow;
    spot = st.dodging ? st.dodgeSpot : st.leaderPos;
    unpark = FOLLOW_UNPARK;
}

void HarvestDuty(LateState const& st, LateBot& bs, PlayerbotAI* botAI, std::string& note)
{
    Player* bot = botAI->GetBot();
    Creature* warden = CreatureOf(bot, st.warden);
    if (!IsLkHealer(bot))
    {
        if (warden && st.ripScans >= WARDEN_KICK_SCANS && warden->FindCurrentSpellBySpellId(LATE_SPELL_SOUL_RIP))
            if (char const* kick = ReadyKick(botAI, warden))
            {
                bs.harvest = LkHarvestDuty::Kick;
                bs.harvestTarget = warden->GetGUID();
                bs.harvestSpell = kick;
                note = "kick";
                return;
            }

        if (warden)
        {
            bs.harvest = LkHarvestDuty::Warden;
            note = "warden";
        }

        return;
    }

    Creature* terenas = CreatureOf(bot, st.terenas);
    Unit* ripped = nullptr;
    if (terenas && terenas->HasAura(LATE_SPELL_SOUL_RIP))
        ripped = terenas;
    else if (bot->HasAura(LATE_SPELL_SOUL_RIP))
        ripped = bot;

    if (ripped)
        if (char const* dispel = DispelName(bot))
            if (botAI->CanCastSpell(dispel, ripped))
            {
                bs.harvest = LkHarvestDuty::Dispel;
                bs.harvestTarget = ripped->GetGUID();
                bs.harvestSpell = dispel;
                note = "dispel";
                return;
            }

    if (terenas && terenas->GetHealthPct() < TERENAS_HEAL_PCT)
    {
        bs.harvest = LkHarvestDuty::Heal;
        bs.harvestTarget = terenas->GetGUID();
        note = "heal";
    }
}

bool StackSpotOf(LateState const& st, Player* bot, IccLichKingPhaseId phase, Position& want, std::string& note)
{
    if (!st.stackValid || (phase != IccLichKingPhaseId::Two && phase != IccLichKingPhaseId::Three))
        return false;

    std::string tag;
    if (bot->GetGUID() == st.holder)
    {
        want = st.holderSpot;
        tag = "holder";
    }
    else if (bot->GetGUID() == st.freeTank)
    {
        want = st.freeSpot;
        tag = "free";
    }
    else
    {
        auto const itr = st.node.find(bot->GetGUID());
        if (itr == st.node.end() || itr->second >= st.nodes.size())
            return false;

        want = st.nodes[itr->second];
        tag = std::to_string(itr->second + 1);
    }

    note = phase == IccLichKingPhaseId::Two ? "p2 " + tag : std::string("p3 ") + SideName(st.side.Get()) + " " + tag;
    return true;
}

bool DefileWalk(LateState& st, LateBot& bs, Player* bot, Position const* prefer, uint32 now, WalkKind& kind,
                Position& spot, std::string& note)
{
    if (st.defileActive)
    {
        if (st.defileTarget == bot->GetGUID())
        {
            if (st.defileRunValid)
            {
                kind = WalkKind::DefileRun;
                spot = st.defileRunSpot;
                note = "run";
                return true;
            }
        }
        else if (Player* target = PlayerOf(bot, st.defileTarget))
        {
            if (bs.defileFor == st.defileCastMs && bs.defileValid)
            {
                kind = WalkKind::DefileClear;
                spot = bs.defileSpot;
                note = "clear";
                return true;
            }

            bool const atDefile = bot->GetExactDist2d(target) < DEFILE_CLEAR ||
                              (st.defileRunValid && bot->GetExactDist2d(st.defileRunSpot) < DEFILE_CLEAR);
            bool const retry = bs.defileFor != st.defileCastMs || !bs.defileBlockedMs ||
                               getMSTimeDiff(bs.defileBlockedMs, now) >= SWEEP_RETRY_MS;
            if (atDefile && retry)
            {
                bs.defileFor = st.defileCastMs;
                bs.defileValid = FindClearSpot(st, bot, target, prefer, bs.defileSpot);
                bs.defileBlockedMs = bs.defileValid ? 0 : now;
                if (bs.defileValid)
                {
                    kind = WalkKind::DefileClear;
                    spot = bs.defileSpot;
                    note = "clear";
                    return true;
                }
            }
        }
    }

    if (bs.puddleValid)
    {
        if (!PuddleTrips(st, bs.puddleSpot) && bot->GetExactDist2d(bs.puddleSpot) > PARK)
        {
            kind = WalkKind::Puddle;
            spot = bs.puddleSpot;
            note = "puddle " + Counter(bs.puddle);
            return true;
        }

        bs.puddleValid = false;
    }

    PuddleInfo const* in = nullptr;
    float nearest = 0.0f;
    for (PuddleInfo const& puddle : st.puddles)
    {
        float const distance = bot->GetExactDist2d(puddle.pos);
        if (distance < puddle.radius + PUDDLE_TRIGGER && (!in || distance < nearest))
        {
            in = &puddle;
            nearest = distance;
        }
    }

    if (!in || (bs.puddleBlockedMs && getMSTimeDiff(bs.puddleBlockedMs, now) < SWEEP_RETRY_MS))
        return false;

    bs.puddle = in->guid;
    bs.puddleValid = FindPuddleExit(st, bot, prefer, bs.puddleSpot);
    bs.puddleBlockedMs = bs.puddleValid ? 0 : now;
    if (!bs.puddleValid)
        return false;

    kind = WalkKind::Puddle;
    spot = bs.puddleSpot;
    note = "puddle " + Counter(bs.puddle);
    return true;
}

bool VileWalk(LateState const& st, LateBot& bs, PlayerbotAI* botAI, WalkKind& kind, Position& spot)
{
    Player* bot = botAI->GetBot();
    if (!st.wave)
        return false;

    if (bot->GetGUID() == st.soaker)
    {
        SpiritInfo const* into = nullptr;
        float best = 0.0f;
        for (SpiritInfo const& vile : st.viles)
        {
            if (!vile.woken || vile.pos.GetPositionZ() - FloorZ() > SOAK_LOW)
                continue;

            float const distance = bot->GetExactDist2d(vile.pos);
            if (distance <= SOAK_REACH && (!into || distance < best))
            {
                into = &vile;
                best = distance;
            }
        }

        if (into && !Guarded(bot))
        {
            bs.guard = GuardSpell(botAI, bot);
            if (!bs.guard && !IsLkTankPlayer(bot))
                into = nullptr;
        }

        Position want;
        if (into)
            want = Position(into->pos.GetPositionX(), into->pos.GetPositionY(), FloorZ());
        else if (st.soakValid)
            want = st.soakSpot;
        else
            return false;

        Position step;
        if (!PlanStep(st, bot, want, step))
            return false;

        kind = WalkKind::Soak;
        spot = step;
        return true;
    }

    if (bot->GetGUID() != st.trapper || st.trapDropped ||
        std::none_of(st.viles.begin(), st.viles.end(), [](SpiritInfo const& vile) { return vile.woken; }))
        return false;

    if (bot->GetExactDist2d(st.trapSpot) <= TRAP_DROP)
        bs.trapDue = FrostTrapReady(bot);

    kind = WalkKind::Trap;
    spot = st.trapSpot;
    return true;
}

void UpdateChase(LateState const& st, LateBot& bs, Player* bot, IccLichKingPhaseId phase)
{
    Creature* valkyr = nullptr;
    auto const bucket = st.bucket.find(bot->GetGUID());
    if (phase == IccLichKingPhaseId::Two && bucket != st.bucket.end())
        valkyr = CarrierOf(st, bot, bucket->second);

    Position const from = bot->GetPosition();
    if (!valkyr || PlayerbotAI::IsRanged(bot) || SegmentClear(st.puddles, from, valkyr->GetPosition()))
        bs.roundValid = false;

    if (!valkyr)
    {
        bs.chase.Clear();
        return;
    }

    if (SegmentClear(st.puddles, from, valkyr->GetPosition()))
    {
        if (bs.chase != valkyr->GetGUID() && !InReach(bot, valkyr))
            bs.chase = valkyr->GetGUID();

        return;
    }

    bool const kept = bs.roundValid && bot->GetExactDist2d(bs.roundStep) > PARK &&
                      SegmentClear(st.puddles, from, bs.roundStep) && !PuddleTrips(st, bs.roundStep);
    if (!kept && !PlayerbotAI::IsRanged(bot))
    {
        Position const dest(valkyr->GetPositionX(), valkyr->GetPositionY(), FloorZ());
        Position step;
        bs.roundValid = PlanStep(st, bot, dest, step) && step.GetExactDist2d(dest) > SPOT_MOVED;
        if (bs.roundValid)
            bs.roundStep = step;
    }

    if (bs.roundValid)
        bs.chase = valkyr->GetGUID();
    else
        bs.chase.Clear();
}

bool OnLeftoverSpirit(PlayerbotAI* botAI, LateState const& st)
{
    Player* bot = botAI->GetBot();
    if (!st.stackValid || !IsRaidDpsBot(bot) || PlayerbotAI::IsRanged(bot))
        return false;

    Unit* pick = GetLkTarget(botAI);
    return pick && pick->GetEntry() == NPC_LK_RAGING_SPIRIT && pick->GetExactDist2d(st.anchor) <= SPIRIT_NEAR_HIM;
}

void ValkyrDuty(LateState const& st, LateBot& bs, Player* bot, IccLichKingPhaseId phase, std::string& note)
{
    if (phase != IccLichKingPhaseId::Two)
        return;

    for (bool const stun : {true, false})
    {
        auto const& elected = stun ? st.stunner : st.slower;
        auto const itr = elected.find(bot->GetGUID());
        if (itr == elected.end())
            continue;

        note = std::string(stun ? "stun " : "slow ") + Counter(itr->second);
        if (Creature* valkyr = CreatureOf(bot, itr->second))
            if (uint32 const spellId = stun ? LkReadyStun(bot, valkyr) : LkReadySlow(bot, valkyr))
            {
                bs.controlSpell = spellId;
                bs.controlTarget = itr->second;
                bs.controlStun = stun;
            }

        return;
    }

    auto const bucket = st.bucket.find(bot->GetGUID());
    if (bucket != st.bucket.end())
        note = std::string(bs.roundValid ? "round " : "dps ") + Counter(bucket->second);
}

void SetWalk(LateBot& bs, Player* bot, WalkKind kind, Position const& spot, float unpark)
{
    if (kind != bs.walk || (kind != WalkKind::None && bs.walkSpot.GetExactDist2d(spot) > SPOT_MOVED))
    {
        bs.walk = kind;
        bs.walkSpot = spot;
        bs.walkParked = false;
    }

    if (kind == WalkKind::None)
        return;

    float const distance = bot->GetExactDist2d(bs.walkSpot);
    bs.walkParked = bs.walkParked ? distance <= unpark : distance <= PARK;
}

void WriteNotes(Player* bot, std::string const& stack, std::string const& defile, std::string const& valkyr,
                char const* vile, std::string const& harvest, std::string const& bomb, bool siphonEnded)
{
    if (!RaidObs::Active())
        return;

    NoteDerived(bot, "lk.stack", stack);
    NoteDerived(bot, "lk.defilerun", defile);
    NoteDerived(bot, "lk.valkyr", valkyr);
    NoteDerived(bot, "lk.vile", vile);
    NoteDerived(bot, "lk.harvest", harvest);
    NoteDerived(bot, "lk.bombdodge", bomb);
    if (siphonEnded)
        NoteDerived(bot, "lk.siphon", "none");
}

void Leave(LateState& st, Player* bot)
{
    auto const itr = st.bots.find(bot->GetGUID());
    if (itr == st.bots.end())
        return;

    st.bots.erase(itr);
    WriteNotes(bot, "none", "none", "none", "none", "none", "none", true);
}

void UpdateBot(LateState& st, LateBot& bs, PlayerbotAI* botAI, Creature* boss, IccLichKingPhaseId phase, bool inFm,
               uint32 now)
{
    Player* bot = botAI->GetBot();
    bs.phase = phase;
    bs.guard = 0;
    bs.trapDue = false;
    bs.controlTarget.Clear();
    bs.controlSpell = 0;
    bs.controlStun = false;
    bs.harvest = LkHarvestDuty::None;
    bs.harvestTarget.Clear();
    bs.harvestSpell.clear();

    std::string stackNote = "none";
    std::string defileNote = "none";
    std::string valkyrNote = "none";
    std::string harvestNote = "none";
    std::string bombNote = "none";
    char const* vileNote = "none";

    WalkKind kind = WalkKind::None;
    Position spot;
    float unpark = UNPARK;
    bool hold = false;

    bool const platformPhase = phase == IccLichKingPhaseId::Two || phase == IccLichKingPhaseId::Three;
    bool const stuck = !bot->IsAlive() || bot->GetVehicle() || HarvestStunned(bot, boss);
    if (inFm && bot->IsAlive())
    {
        hold = true;
        if (IsHeroicMap(bot))
        {
            stackNote = "fm";
            FrostmourneWalk(st, bot, kind, spot, unpark, bombNote);
        }
        else
            HarvestDuty(st, bs, botAI, harvestNote);
    }
    else if (!inFm && !stuck && platformPhase)
    {
        Position want;
        bool const assigned = StackSpotOf(st, bot, phase, want, stackNote);
        Position const* prefer = assigned ? &want : nullptr;

        if (bot->GetGUID() == st.soaker && st.wave)
            vileNote = "soak";
        else if (bot->GetGUID() == st.trapper && st.wave && !st.trapDropped)
            vileNote = "trap";

        if (!DefileWalk(st, bs, bot, prefer, now, kind, spot, defileNote) &&
            !(phase == IccLichKingPhaseId::Three && VileWalk(st, bs, botAI, kind, spot)))
        {
            UpdateChase(st, bs, bot, phase);
            if (bs.roundValid && IsLkMovementOwned(botAI))
                bs.roundValid = false;

            if (bs.roundValid)
            {
                kind = WalkKind::Round;
                spot = bs.roundStep;
            }
            else if (assigned && OnLeftoverSpirit(botAI, st))
                stackNote = phase == IccLichKingPhaseId::Two
                                ? std::string("p2 spirit")
                                : std::string("p3 ") + SideName(st.side.Get()) + " spirit";
            else if (!bs.chase && assigned && !IsLkMovementOwned(botAI))
            {
                Position step;
                if (PlanStep(st, bot, want, step))
                {
                    kind = WalkKind::Stack;
                    spot = step;
                }
                else
                    hold = true;
            }
        }
        else
        {
            bs.roundValid = false;
        }

        ValkyrDuty(st, bs, bot, phase, valkyrNote);
    }

    if (stuck || !platformPhase || inFm)
    {
        bs.chase.Clear();
        bs.roundValid = false;
    }

    SetWalk(bs, bot, kind, spot, unpark);
    bs.owned = kind != WalkKind::None || hold;
    bs.reachHeld = kind == WalkKind::Round || (inFm && bot->IsAlive() && IsHeroicMap(bot));

    bool const siphoning = phase == IccLichKingPhaseId::Two && bot->GetGUID() == st.freeTank &&
                           std::any_of(st.valkyrs.begin(), st.valkyrs.end(),
                                       [](ValkyrInfo const& valkyr) { return valkyr.siphoner; });
    WriteNotes(bot, stackNote, defileNote, valkyrNote, vileNote, harvestNote, bombNote, !siphoning);
}

Unit* Resolve(PlayerbotAI* botAI, LateState const& st, Creature* boss, char const*& rule)
{
    Player* bot = botAI->GetBot();
    LateBot const* bs = BotOf(st, bot);
    bool const dps = IsRaidDpsBot(bot);
    if (!bs || (!dps && !IsLkTankPlayer(bot)) || !bot->IsAlive() || bot->GetVehicle() || HarvestStunned(bot, boss))
        return nullptr;

    bool const ranged = PlayerbotAI::IsRanged(bot);
    if (InFrostmourne(bot))
    {
        if (!IsHeroicMap(bot))
        {
            Unit* warden = Pickable(bot, CreatureOf(bot, st.warden));
            if (warden)
                rule = "warden";

            return warden;
        }

        if (!dps || !ranged)
            return nullptr;

        Player* leader = PlayerOf(bot, st.leader);
        Position const from = leader ? leader->GetPosition() : bot->GetPosition();
        Unit* best = nullptr;
        float bestDistance = 0.0f;
        for (SpiritInfo const& spirit : st.wicked)
        {
            if (!spirit.woken)
                continue;

            float const distance = from.GetExactDist2d(spirit.pos);
            if (best && distance >= bestDistance)
                continue;

            Unit* unit = Pickable(bot, CreatureOf(bot, spirit.guid));
            if (unit && bot->IsWithinCombatRange(unit, RANGED_REACH))
            {
                best = unit;
                bestDistance = distance;
            }
        }

        if (best)
            rule = "wicked";

        return best;
    }

    if (bs->phase == IccLichKingPhaseId::Two)
    {
        auto const bucket = st.bucket.find(bot->GetGUID());
        if (bucket != st.bucket.end())
            if (Unit* valkyr = Pickable(bot, CarrierOf(st, bot, bucket->second)))
                if (bs->chase == valkyr->GetGUID() || InReach(bot, valkyr))
                {
                    rule = "valkyr";
                    return valkyr;
                }

        if (!dps || !ranged || !IsHeroicMap(bot))
            return nullptr;

        Unit* best = nullptr;
        for (ValkyrInfo const& info : st.valkyrs)
        {
            if (!info.siphoner || (best && info.healthPct >= best->GetHealthPct()))
                continue;

            if (Unit* unit = Pickable(bot, CreatureOf(bot, info.guid)))
                best = unit;
        }

        if (best)
            rule = "siphon";

        return best;
    }

    if (bs->phase != IccLichKingPhaseId::Three || !dps || !ranged || !st.stackValid)
        return nullptr;

    Unit* best = nullptr;
    float bestDistance = 0.0f;
    for (SpiritInfo const& vile : st.viles)
    {
        float const distance = st.stackCentre.GetExactDist2d(vile.pos);
        if (!vile.woken || distance > VILE_SHOOT_RANGE || (best && distance >= bestDistance))
            continue;

        if (Unit* unit = Pickable(bot, CreatureOf(bot, vile.guid)))
        {
            best = unit;
            bestDistance = distance;
        }
    }

    if (best)
        rule = "vile";

    return best;
}

bool WalkOf(PlayerbotAI* botAI, std::initializer_list<WalkKind> kinds, Position& spot)
{
    LateState* st = StateOf(botAI);
    LateBot const* bs = st ? BotOf(*st, botAI->GetBot()) : nullptr;
    if (!bs || bs->walkParked || std::find(kinds.begin(), kinds.end(), bs->walk) == kinds.end())
        return false;

    spot = bs->walkSpot;
    return true;
}
}

void TickLkLate(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsInWorld() || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    Creature* boss = GetLkBoss(botAI);
    if (!boss)
        return;

    uint32 const instanceId = bot->GetInstanceId();
    LateState* found = lateStates.Find(instanceId);
    if (!IsLkLive(botAI))
    {
        if (found)
            lateStates.Reset(instanceId);

        return;
    }

    IccLichKingPhaseId const phase = IccLichKingPhase(bot, boss);
    bool const inFm = InFrostmourne(bot);
    bool const mine = inFm || phase == IccLichKingPhaseId::Two || phase == IccLichKingPhaseId::Three ||
                      phase == IccLichKingPhaseId::Frostmourne;
    if (!mine)
    {
        if (found)
            Leave(*found, bot);

        return;
    }

    LateState& st = lateStates.For(instanceId);
    uint32 const now = getMSTime();
    if (inFm)
    {
        ScanFrostmourne(st, bot, now);
        RefreshFrostmourneDuties(st, botAI, now);
    }
    else
    {
        ScanPlatform(st, bot, now);
        DetectDefileCast(st, bot, now);
    }

    DetectVileCast(st, boss, phase, now);
    if (!inFm)
        RefreshPlatformDuties(st, botAI, phase, now);

    if (!IsLkDutyBot(bot))
        return;

    UpdateBot(st, st.bots[bot->GetGUID()], botAI, boss, phase, inFm, now);
}

Unit* GetLkLateTarget(PlayerbotAI* botAI, char const*& rule)
{
    rule = nullptr;
    Creature* boss = nullptr;
    LateState* st = LiveStateOf(botAI, &boss);
    if (!st)
        return nullptr;

    TargetMemo& memo = st->targets[botAI->GetBot()->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.rule = nullptr;
        memo.pick = Resolve(botAI, *st, boss, memo.rule);
    }

    rule = memo.rule;
    return memo.pick;
}

bool IsLkLateTargetHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if ((!IsLkTankPlayer(bot) && !IsRaidDpsBot(bot)) || !InFrostmourne(bot) || !LiveStateOf(botAI))
        return false;

    char const* rule = nullptr;
    return !GetLkLateTarget(botAI, rule);
}

bool IsLkLateMovementOwned(PlayerbotAI* botAI)
{
    LateState* st = StateOf(botAI);
    LateBot const* bs = st ? BotOf(*st, botAI->GetBot()) : nullptr;
    return bs && bs->owned;
}

bool GetLkDefileWalk(PlayerbotAI* botAI, Position& spot)
{
    return WalkOf(botAI, {WalkKind::DefileRun, WalkKind::DefileClear, WalkKind::Puddle}, spot);
}

bool GetLkVileWalk(PlayerbotAI* botAI, Position& spot) { return WalkOf(botAI, {WalkKind::Soak, WalkKind::Trap}, spot); }

bool GetLkFrostmourneWalk(PlayerbotAI* botAI, Position& spot)
{
    return WalkOf(botAI, {WalkKind::FrostmourneLead, WalkKind::FrostmourneFollow}, spot);
}

bool IsLkLateReachHeld(PlayerbotAI* botAI)
{
    LateState* st = StateOf(botAI);
    LateBot const* bs = st ? BotOf(*st, botAI->GetBot()) : nullptr;
    return bs && bs->reachHeld;
}

bool GetLkStackWalk(PlayerbotAI* botAI, Position& spot)
{
    return WalkOf(botAI, {WalkKind::Stack, WalkKind::Round}, spot);
}

bool IsLkDefileRun(PlayerbotAI* botAI)
{
    LateState* st = StateOf(botAI);
    LateBot const* bs = st ? BotOf(*st, botAI->GetBot()) : nullptr;
    return bs && bs->walk == WalkKind::DefileRun;
}

bool IsLkFrostmourneUrgent(PlayerbotAI* botAI)
{
    LateState* st = StateOf(botAI);
    LateBot const* bs = st ? BotOf(*st, botAI->GetBot()) : nullptr;
    return bs && st->dodging &&
           (bs->walk == WalkKind::FrostmourneLead || bs->walk == WalkKind::FrostmourneFollow);
}

uint32 GetLkSoakGuard(PlayerbotAI* botAI)
{
    LateState* st = LiveStateOf(botAI);
    LateBot const* bs = st ? BotOf(*st, botAI->GetBot()) : nullptr;
    return bs ? bs->guard : 0;
}

bool IsLkTrapDue(PlayerbotAI* botAI)
{
    LateState* st = LiveStateOf(botAI);
    LateBot const* bs = st ? BotOf(*st, botAI->GetBot()) : nullptr;
    return bs && bs->trapDue && !st->trapDropped;
}

void NoteLkTrapDropped(PlayerbotAI* botAI)
{
    LateState* st = LiveStateOf(botAI);
    if (!st || st->trapper != botAI->GetBot()->GetGUID())
        return;

    st->trapDropped = true;
    if (LateBot* bs = BotOf(*st, botAI->GetBot()))
        bs->trapDue = false;
}

Unit* GetLkSiphonTauntTarget(PlayerbotAI* botAI)
{
    LateState* st = LiveStateOf(botAI);
    Player* bot = botAI->GetBot();
    LateBot const* bs = st ? BotOf(*st, bot) : nullptr;
    if (!bs || bs->phase != IccLichKingPhaseId::Two || st->freeTank != bot->GetGUID() || !IsLkDutyBot(bot) ||
        !IsHeroicMap(bot) || bot->GetVehicle() || !ClassTauntReady(botAI))
        return nullptr;

    char const* taunt = ClassTauntName(bot);
    uint32 const tauntId = taunt ? botAI->GetAiObjectContext()->GetValue<uint32>("spell id", taunt)->Get() : 0;
    SpellInfo const* tauntInfo = tauntId ? sSpellMgr->GetSpellInfo(tauntId) : nullptr;
    if (!tauntInfo)
        return nullptr;

    uint32 const now = getMSTime();
    Creature* best = nullptr;
    bool bestFresh = false;
    uint32 bestAge = 0;
    for (ValkyrInfo const& info : st->valkyrs)
    {
        if (!info.siphoner)
            continue;

        Creature* valkyr = CreatureOf(bot, info.guid);
        if (!valkyr || !valkyr->IsInCombat() || !bot->IsValidAttackTarget(valkyr) ||
            valkyr->IsImmunedToSpell(tauntInfo) || TauntedBy(valkyr, bot) || bot->GetExactDist(valkyr) > TAUNT_RANGE ||
            !bot->IsWithinLOSInMap(valkyr))
            continue;

        auto const last = st->siphonTaunted.find(info.guid);
        bool const fresh = last == st->siphonTaunted.end();
        uint32 const age = fresh ? 0 : getMSTimeDiff(last->second, now);
        bool better = !best;
        if (!better)
            better = fresh != bestFresh ? fresh : (fresh ? valkyr->GetGUID() < best->GetGUID() : age > bestAge);

        if (better)
        {
            best = valkyr;
            bestFresh = fresh;
            bestAge = age;
        }
    }

    return best;
}

void NoteLkSiphonTaunt(PlayerbotAI* botAI, Unit* valkyr)
{
    LateState* st = LiveStateOf(botAI);
    if (!st || !valkyr)
        return;

    st->siphonTaunted[valkyr->GetGUID()] = getMSTime();
    NoteDerived(botAI->GetBot(), "lk.siphon", Counter(valkyr->GetGUID()));
}

Unit* GetLkValkyrControl(PlayerbotAI* botAI, uint32& spellId, bool& stun)
{
    spellId = 0;
    stun = false;
    LateState* st = LiveStateOf(botAI);
    Player* bot = botAI->GetBot();
    LateBot const* bs = st ? BotOf(*st, bot) : nullptr;
    if (!bs || !bs->controlSpell)
        return nullptr;

    Creature* valkyr = CarrierOf(*st, bot, bs->controlTarget);
    if (!valkyr)
        return nullptr;

    spellId = bs->controlSpell;
    stun = bs->controlStun;
    return valkyr;
}

void NoteLkValkyrControl(PlayerbotAI* botAI, Unit* valkyr, bool stun)
{
    LateState* st = LiveStateOf(botAI);
    if (!st || !valkyr)
        return;

    Player* bot = botAI->GetBot();
    ControlCast& cast = (stun ? st->stunCast : st->slowCast)[valkyr->GetGUID()];
    cast.by = bot->GetGUID();
    cast.castMs = getMSTime();
    (stun ? st->stunner : st->slower).erase(bot->GetGUID());
    if (LateBot* bs = BotOf(*st, bot))
        bs->controlSpell = 0;
}

LkHarvestDuty GetLkHarvestDuty(PlayerbotAI* botAI, Unit*& target, std::string& spell)
{
    target = nullptr;
    spell.clear();
    LateState* st = LiveStateOf(botAI);
    Player* bot = botAI->GetBot();
    LateBot const* bs = st ? BotOf(*st, bot) : nullptr;
    if (!bs || bs->harvest == LkHarvestDuty::None)
        return LkHarvestDuty::None;

    if (bs->harvestTarget == bot->GetGUID())
        target = bot;
    else
        target = CreatureOf(bot, bs->harvestTarget);

    spell = bs->harvestSpell;
    return bs->harvest;
}
