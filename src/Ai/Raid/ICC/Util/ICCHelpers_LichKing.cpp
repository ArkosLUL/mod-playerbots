/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_LichKing.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <list>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "GameObject.h"
#include "Group.h"
#include "GroupReference.h"
#include "ICCHelpers_LichKingLate.h"
#include "ICCHelpers_Shared.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidDifficulty.h"
#include "RaidDirectTargeting.h"
#include "RaidHumanTankCall.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidResistAura.h"
#include "RaidTankDefensive.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Timer.h"

using namespace EncounterHelpers;

namespace
{
constexpr float GATE_RANGE = 120.0f;
constexpr float FM_GATE_RANGE = 50.0f;
// Searched from him, who never strays far from C, then filtered to GATE_RANGE of C
constexpr float SCAN_RANGE = 140.0f;
constexpr uint32 SCAN_MS = 200;
constexpr uint32 DUTY_MS = 250;
constexpr uint32 HUMAN_CALL_CHECK_MS = 500;
// A sweep asks the collision check up to 240 times, so a blocked one waits before the next
constexpr uint32 SWEEP_RETRY_MS = 1000;

constexpr float FLOOR_Z = 840.857f;
constexpr float FM_FLOOR_Z = 1049.95f;
constexpr float FLOOR_Z_TOLERANCE = 2.0f;
constexpr float SOLID_RADIUS = 44.0f;
// The Quake's crack triggers stand 47-51 yd out
constexpr float RING_INNER = 49.0f;
constexpr float RING_OUTER = 62.0f;
// The heroic teleport spreads the raid 2-20 yd off F, the only floor known there
constexpr float FM_SPOT_RADIUS = 20.0f;

// A walk parks within PARK and walks back once pushed past UNPARK
constexpr float PARK = 1.0f;
constexpr float UNPARK = 2.5f;
static_assert(PARK < UNPARK, "a walk has to end inside its own trigger");
// A stand that moves this far with what it hangs off is latched again
constexpr float RELATCH = 2.0f;
// A stand walk that has not parked by then lets go for STAND_REST_MS, so a spot it can't reach doesn't
// pin the bot for the whole phase
constexpr uint32 STAND_CAP_MS = 15000;
constexpr uint32 STAND_REST_MS = 3000;

struct Polar
{
    float deg;
    float r;
};

// Phase one: him on the holder south of C, the adds west with their tank past them so Shockwave points
// west off everyone, ranged and healers north
constexpr Polar HOLDER_SPOT{180.0f, 12.0f};
// In taunt range of where his intro walk stops, (465.07, -2123.47)
constexpr Polar OPENING_SPOT{180.0f, 14.0f};
constexpr Polar ADD_PILE{90.0f, 17.0f};
constexpr Polar ADD_TANK_SPOT{90.0f, 22.0f};
constexpr Polar RANGED_ROUND{0.0f, 8.0f};
constexpr float LATTICE_SPACING = 6.0f;
constexpr float LATTICE_REACH = 12.0f;
constexpr float LATTICE_PILE_MIN = 12.0f;
constexpr float LATTICE_HOLDER_MAX = 28.0f;

// Winters: two clusters on the ring, slots 6 yd apart on two arcs. The tank cluster grows east from 0°
// and the ranged one west from its edge, so Pain and Suffering's cone at one never reaches the other.
constexpr float WINTER_INNER = 51.0f;
constexpr float WINTER_OUTER = 57.0f;
constexpr float WINTER_SPACING = 6.0f;
// Pain and Suffering's half-width plus 8°: its spell_cone row names only the 10N id (25°), the others
// fall back to 104°
constexpr float RANGED_CLUSTER_10N = 20.5f;
constexpr float RANGED_CLUSTER = 60.0f;
// Each spirit tank's spot, the spirit inward of it so Soul Shriek's cone points off the platform
constexpr Polar SPIRIT_SPOTS[] = {{-7.0f, 62.0f}, {7.0f, 62.0f}};
constexpr size_t SPIRIT_TANKS = 2;
// While the ring is shattered, short of the crack
constexpr float DAMAGED_HOLD = 42.0f;
constexpr float SPIRIT_HELD_NEAR = 8.0f;
constexpr float SPIRIT_INNER = 3.0f;
// Taunts reach 30 yd: a spirit tank further than PICKUP_FROM off its spirit walks to PICKUP_REACH of it
constexpr float PICKUP_FROM = 28.0f;
constexpr float PICKUP_REACH = 22.0f;
// A ranged pick in a winter has to be in spell range from the bot's slot: reach is held there, since it
// would walk the bot into Remorseless Winter, and 30 yd is every caster's base nuke range
constexpr float RANGED_REACH = 30.0f;
// Off 10N the ranged cluster sits 44 yd or more off a sphere's path to the tank cluster, so one ranged DPS
// bot in this many, hunters first, guards the tank cluster from its middle
constexpr size_t SPHERE_GUARD_SHARE = 3;

constexpr float QUAKE_FROM = 40.0f;
constexpr float QUAKE_TO = 38.0f;

// A Shadow Trap fires on any unit within 3 yd once armed, a pet or a totem included, and throws everyone
// within 10 yd off the platform, so bots keep out of the throw, not just off the trap
constexpr float TRAP_TRIGGER = 10.5f;
constexpr float TRAP_CLEAR = 11.0f;
constexpr float TRAP_SEGMENT = 3.5f;
constexpr float TRAP_SWEEP = 25.0f;
constexpr float RELATCH_SWEEP_STEP = 1.5f;
constexpr float RELATCH_SWEEP_MAX = 18.0f;
constexpr int RELATCH_SWEEP_ANGLES = 16;

// Necrotic Plague jumps to the nearest unit within 10 yd when it is dispelled, expires or kills
constexpr float PLAGUE_JUMP = 10.0f;
constexpr float PLAGUE_SIDE = 2.0f;
constexpr float PLAGUE_ISOLATE = 12.0f;
constexpr float PLAGUE_SWEEP = 30.0f;
constexpr int32 PLAGUE_TICK_MARGIN_MS = 1000;
constexpr float PLAGUE_LOW_PCT = 35.0f;
constexpr float CURE_RANGE = 30.0f;
constexpr float DISPEL_RANGE = 30.0f;
// Water totems, the first one known replaces a Cleansing Totem: it strips a disease every 3 s
constexpr uint32 SPELL_CLEANSING_TOTEM = 8170;
constexpr char const* WATER_TOTEM_SWAPS[] = {"mana spring totem", "healing stream totem"};

constexpr float BRAKE_CAP = 0.9f;
constexpr float BRAKE_RELEASE = 0.8f;

constexpr float TAUNT_RANGE = 30.0f;
constexpr float AOE_TAUNT_RADIUS = 10.0f;
constexpr size_t AOE_TAUNT_MIN = 2;
constexpr uint32 SPELL_CHALLENGING_SHOUT = 1161;
constexpr uint32 SPELL_CHALLENGING_ROAR = 5209;

constexpr uint32 SIPHON_DEFENSIVE_STACKS = 15;
constexpr float TANK_LOW_PCT = 40.0f;
constexpr float TANK_LAST_RESORT_PCT = 25.0f;

constexpr int32 STUN_NEXT_MS = 500;
constexpr uint32 STUN_STALE_MS = 1500;
constexpr uint32 STUN_RETRY_MS = 2000;

constexpr uint32 SPELL_GNAW = 47481;
constexpr uint32 SPELL_INTIMIDATION = 19577;
// What Intimidation's pet hit applies, which is what diminishing returns read
constexpr uint32 SPELL_INTIMIDATION_STUN = 24394;
constexpr float MELEE_SPELL_RANGE = 5.0f;

// A bot this far under the floor is inside it, where only a collision fault puts one
constexpr float FLOOR_SNAP_Z = 838.0f;
constexpr float FM_FLOOR_SNAP_Z = 1046.0f;
constexpr float FLOOR_SNAP_RANGE = 44.0f;
constexpr float FM_FLOOR_SNAP_RANGE = 38.0f;

constexpr int32 EMPOWERED_BLOOD_FROM = 60;
constexpr int32 EMPOWERED_BLOOD_TO = 40;

// The winters a pull has seen, as bits: the first, the second. Without a pull state the phase reads as
// if both had run.
constexpr uint8 WINTER_FIRST = 1;
constexpr uint8 WINTER_SECOND = 2;
constexpr uint8 WINTERS_UNKNOWN = WINTER_FIRST | WINTER_SECOND;

struct ControlSpell
{
    // 0 for every class
    uint8 cls;
    char const* name;
    // A frontal cone from the caster
    bool frontal;
};

// All of his adds are undead, so Holy Wrath stuns them
constexpr ControlSpell STUN_SPELLS[] = {
    {CLASS_PALADIN, "hammer of justice", false}, {CLASS_WARRIOR, "concussion blow", false},
    {CLASS_DRUID, "bash", false},                {CLASS_ROGUE, "kidney shot", false},
    {CLASS_WARRIOR, "shockwave", true},          {CLASS_PALADIN, "holy wrath", false},
    {CLASS_WARLOCK, "shadowfury", false},        {0, "war stomp", false},
};

constexpr ControlSpell SLOW_SPELLS[] = {
    {CLASS_DEATH_KNIGHT, "chains of ice", false}, {CLASS_MAGE, "slow", false},
    {CLASS_HUNTER, "concussive shot", false},     {CLASS_SHAMAN, "frost shock", false},
    {CLASS_WARLOCK, "curse of exhaustion", false}, {CLASS_WARRIOR, "hamstring", false},
    {CLASS_HUNTER, "wing clip", false},           {CLASS_MAGE, "frostbolt", false},
    {CLASS_WARRIOR, "piercing howl", false},
};

enum class WalkKind : uint8
{
    None,
    Quake,
    Trap,
    Plague,
    // A melee walk round a trap to its pick, not a dodge: walked on the stand row
    Reach,
    Stand
};

enum class StandKind : uint8
{
    None,
    Opening,
    Holder,
    AddTank,
    Ranged,
    Winter,
    Spirit
};

enum class PlagueMode : uint8
{
    None,
    Walk,
    Delivered,
    Isolate
};

enum class CureRule : uint8
{
    None,
    Delivered,
    AddTank,
    Isolated,
    Emergency
};

struct AddInfo
{
    ObjectGuid guid;
    uint32 entry = 0;
    Position pos;
    float healthPct = 100.0f;
    // Its 5 s emerge is over
    bool emerged = false;
    bool inCombat = false;
    ObjectGuid victim;
    bool enraged = false;
    bool frenzied = false;
};

struct SpiritInfo
{
    ObjectGuid guid;
    Position pos;
    float healthPct = 100.0f;
    bool rooted = false;
    bool passive = false;
    bool inCombat = false;
    ObjectGuid victim;
};

struct SphereInfo
{
    ObjectGuid guid;
    Position pos;
    bool inCombat = false;
    // Off the player it follows
    float toVictim = 1000.0f;
};

struct TrapInfo
{
    ObjectGuid guid;
    Position pos;
};

struct TauntDuty
{
    char const* rule = nullptr;
    ObjectGuid unit;
    bool aoe = false;
};

struct CureDuty
{
    ObjectGuid carrier;
    CureRule rule = CureRule::None;
};

struct WinterSlot
{
    bool ranged = false;
    uint8 index = 0;
    // A ranged DPS bot in the tank cluster for its spheres
    bool guard = false;
};

struct FloorContext
{
    bool frostmourne = false;
    // The bot's own spot reads the floor through the gameobject collision, so other spots can be asked
    bool heightKnown = false;
    bool ringStandable = true;
};

struct FloorMemo
{
    uint32 ms = 0;
    bool set = false;
    FloorContext ctx;
};

struct LkBotState
{
    WalkKind walk = WalkKind::None;
    Position walkSpot;
    bool walkParked = false;
    bool owned = false;

    StandKind stand = StandKind::None;
    // The stand as laid out, and where it latched once a trap moved it
    Position standWanted;
    Position standSpot;
    // Fouled with no clear spot: the bot waits where it is
    bool standHeld = false;
    uint32 standSinceMs = 0;
    bool standResting = false;
    uint32 standRestMs = 0;
    uint32 standBlockedMs = 0;
    std::string standNote = "none";

    bool trap = false;
    ObjectGuid trapGuid;
    Position trapSpot;
    uint32 trapBlockedMs = 0;

    // A melee walk to its pick round a trap, or a hold when no spot round the pick clears the traps
    bool reach = false;
    bool reachValid = false;
    ObjectGuid reachTrap;
    ObjectGuid reachPick;
    // Where the pick stood when the spot was found
    Position reachBasis;
    Position reachSpot;
    uint32 reachBlockedMs = 0;

    PlagueMode plague = PlagueMode::None;
    ObjectGuid plagueAdd;
    // The add the plague would jump to now
    ObjectGuid plagueJump;
    // The add's spot the walk hangs off
    Position plagueBasis;
    Position plagueSpot;
    bool plagueValid = false;
    uint32 plagueBlockedMs = 0;

    bool quake = false;
    Position quakeSpot;
};

// Asked by a trigger, its action and a rule for every action a bot weighs, and a bot never ticks twice in
// one ms
struct TargetMemo
{
    uint32 ms = 0;
    bool set = false;
    Unit* pick = nullptr;
};

struct LkState
{
    uint32 pullMs = 0;

    uint32 scanMs = 0;
    bool scanned = false;
    std::vector<AddInfo> adds;
    std::vector<SpiritInfo> spirits;
    std::vector<SphereInfo> spheres;
    std::vector<TrapInfo> traps;

    uint32 dutyMs = 0;
    bool dutiesRan = false;
    IccLichKingPhaseId phase = IccLichKingPhaseId::One;
    bool reachedThree = false;
    uint8 winters = 0;
    bool heroic = false;
    bool tenNormal = false;
    bool ringStandable = true;
    // From the Quake's cast until he turns aggressive
    bool quake = false;

    ObjectGuid holder;
    ObjectGuid freeTank;
    // Phase one with no other tank player: the damage dealer that holds the ghouls
    ObjectGuid standIn;
    // LkTankRole per tank: the holder, players included, the free bot tank and the stand-in
    RaidObs::ObsGuidMap<uint8> tank{"lk.tank"};
    RaidObs::ObsGuidSet brake{"lk.brake"};
    std::unordered_map<ObjectGuid, TauntDuty> taunts;
    // The bot tanks at SPIRIT_SPOTS in this order, the holder first
    std::vector<ObjectGuid> spiritTanks;
    // Raging Spirit -> the bot tank that holds it
    std::unordered_map<ObjectGuid, ObjectGuid> spiritTank;

    std::unordered_set<ObjectGuid> carriers;
    // Curer -> its carrier and why it dispels now
    std::unordered_map<ObjectGuid, CureDuty> cures;
    // Hunter -> the enraged Shambling Horror it Tranquilizes
    std::unordered_map<ObjectGuid, ObjectGuid> tranq;
    // Bot -> the Frenzied Shambling Horror its stun goes on
    RaidObs::ObsGuidMap<ObjectGuid> stun{"lk.stun"};
    std::unordered_map<ObjectGuid, uint32> stunSince;
    std::unordered_map<ObjectGuid, uint32> stunTried;
    // Priest or paladin -> the member it frees from Soul Shriek's silence
    std::unordered_map<ObjectGuid, ObjectGuid> silence;

    std::unordered_map<ObjectGuid, uint8> rangedNode;
    // The winter the slots are laid out for, One for none
    IccLichKingPhaseId winterOf = IccLichKingPhaseId::One;
    std::unordered_map<ObjectGuid, WinterSlot> winterSlots;
    uint8 winterNext[2] = {0, 0};

    uint32 humanCallMs = 0;
    bool humanCallRan = false;

    std::unordered_map<ObjectGuid, LkBotState> bots;
    std::unordered_map<ObjectGuid, TargetMemo> targets;
    std::unordered_map<ObjectGuid, FloorMemo> floors;
};

RaidInstanceState<LkState> lkStates;
// Bots whose saved rti was checked this instance, so a player's later order sticks
RaidInstanceState<std::unordered_set<ObjectGuid>> lkRtiChecked;

void NoteDerived(Player* bot, char const* key, std::string const& value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
}

std::string Tag(char const* rule, ObjectGuid const& guid)
{
    return guid ? std::string(rule) + " " + std::to_string(guid.GetCounter()) : std::string(rule);
}

std::string Tag(char const* rule, Unit const* unit)
{
    return unit ? Tag(rule, unit->GetGUID()) : std::string(rule);
}

Position PolarSpot(Polar const& polar)
{
    float const angle = polar.deg * static_cast<float>(M_PI) / 180.0f;
    return Position(LK_CENTER.GetPositionX() + polar.r * std::cos(angle),
                    LK_CENTER.GetPositionY() + polar.r * std::sin(angle), FLOOR_Z);
}

// The same bearing from C at another radius
Position AtRadius(Position const& pos, float r)
{
    float const angle = LK_CENTER.GetAngle(pos.GetPositionX(), pos.GetPositionY());
    return Position(LK_CENTER.GetPositionX() + r * std::cos(angle), LK_CENTER.GetPositionY() + r * std::sin(angle),
                    FLOOR_Z);
}

// `distance` yd from `from` toward `to`
Position Toward(Position const& from, Position const& to, float distance)
{
    float const dx = to.GetPositionX() - from.GetPositionX();
    float const dy = to.GetPositionY() - from.GetPositionY();
    float const len = std::max(std::sqrt(dx * dx + dy * dy), 0.001f);
    return Position(from.GetPositionX() + dx / len * distance, from.GetPositionY() + dy / len * distance,
                    from.GetPositionZ());
}

bool IsHeroic(Player* bot)
{
    Difficulty const difficulty = GetRaidMapDifficulty(bot);
    return difficulty == RAID_DIFFICULTY_10MAN_HEROIC || difficulty == RAID_DIFFICULTY_25MAN_HEROIC;
}

bool InRoom(Player* bot)
{
    return bot->GetExactDist2d(LK_CENTER) <= GATE_RANGE || bot->GetExactDist2d(LK_FROSTMOURNE) <= FM_GATE_RANGE;
}

Creature* BossUnit(Player* bot)
{
    InstanceScript* instance = bot->GetMapId() == ICC_MAP_ID ? bot->GetInstanceScript() : nullptr;
    return instance ? ObjectAccessor::GetCreature(*bot, instance->GetGuidData(LK_DATA_THE_LICH_KING)) : nullptr;
}

bool IsLive(Creature const* boss) { return boss && (boss->IsInCombat() || boss->IsCombatDisallowed()); }

// Tirion's intro has stood him up and he walks to his stop, still immune until it ends
bool IsIntro(Creature const* boss)
{
    return boss && !boss->IsInCombat() && boss->IsImmuneToPC() && !boss->HasAura(SPELL_LK_SIT);
}

bool IsWinter(IccLichKingPhaseId phase)
{
    return phase == IccLichKingPhaseId::Winter1 || phase == IccLichKingPhaseId::Winter2;
}

bool IsFighting(IccLichKingPhaseId phase)
{
    return phase == IccLichKingPhaseId::One || phase == IccLichKingPhaseId::Two ||
           phase == IccLichKingPhaseId::Three;
}

bool Harvested(Player* bot)
{
    if (IccLichKingInFrostmourne(bot))
        return true;

    for (uint32 const spellId : ICC_LK_HARVEST_SOULS)
        if (bot->HasAura(spellId))
            return true;

    return false;
}

// Stunned on the platform by Harvest Soul, before its 6.25 s teleport. Heroic's Harvest Souls has no
// duration and ends only with his channel, so the stun itself is what counts.
bool HarvestStunned(Player* bot)
{
    if (IccLichKingInFrostmourne(bot) || !bot->HasUnitState(UNIT_STATE_STUNNED))
        return false;

    for (uint32 const spellId : ICC_LK_HARVEST_SOUL)
        if (bot->HasAura(spellId))
            return true;

    for (uint32 const spellId : ICC_LK_HARVEST_SOULS)
        if (bot->HasAura(spellId))
            return true;

    return false;
}

// `pastThree`: phase three has run this pull, so a passive stretch with no winter is the heroic Frostmourne
// return, never the second winter's walk. `winters`: the WINTER_* bits this pull has seen.
IccLichKingPhaseId PhaseOf(Unit* boss, bool harvested, bool pastThree, uint8 winters)
{
    // The Lich King voice in Valithria's room shares his name
    if (!boss || boss->GetEntry() != NPC_THE_LICH_KING)
        return IccLichKingPhaseId::One;

    if (boss->IsCombatDisallowed())
        return IccLichKingPhaseId::Outro;

    // At 70% and 40% his script turns him passive and walks him to C for the winter, then turns him
    // aggressive 5.25 s after the Quake that ends it. Heroic's Harvest Souls holds him passive too.
    Creature const* const lichKing = boss->ToCreature();
    bool const passive = lichKing && lichKing->HasReactState(REACT_PASSIVE);
    uint8 const winter = IccRemorselessWinterOf(boss);
    if (winter == 1)
        return IccLichKingPhaseId::Winter1;

    if (winter == 2)
        return IccLichKingPhaseId::Winter2;

    if (boss->HealthAbovePct(70))
        return IccLichKingPhaseId::One;

    // Under a threshold he turns passive on the first hit he takes while not casting, so a cast in flight
    // keeps the phase before it for up to 2 s
    if (!passive)
    {
        if (!(winters & WINTER_FIRST))
            return IccLichKingPhaseId::One;

        return boss->HealthAbovePct(40) || !(winters & WINTER_SECOND) ? IccLichKingPhaseId::Two
                                                                       : IccLichKingPhaseId::Three;
    }

    if (boss->HealthAbovePct(40))
        return IccLichKingPhaseId::Winter1;

    // He stays passive until 1 s after the return
    if (boss->GetMap()->IsHeroic() && (harvested || pastThree))
        return IccLichKingPhaseId::Frostmourne;

    return IccLichKingPhaseId::Winter2;
}

LkState* StateOf(PlayerbotAI* botAI, Creature** boss = nullptr)
{
    Creature* unit = GetLkBoss(botAI);
    if (boss)
        *boss = unit;

    return unit ? lkStates.Find(botAI->GetBot()->GetInstanceId()) : nullptr;
}

LkState* LiveStateOf(PlayerbotAI* botAI, Creature** boss = nullptr)
{
    Creature* unit = nullptr;
    LkState* st = StateOf(botAI, &unit);
    if (boss)
        *boss = unit;

    return st && IsLive(unit) ? st : nullptr;
}

LkBotState const* BotOf(LkState const& st, Player* bot)
{
    auto const itr = st.bots.find(bot->GetGUID());
    return itr != st.bots.end() ? &itr->second : nullptr;
}

Player* PlayerOf(Player* bot, ObjectGuid const& guid)
{
    Player* player = guid ? ObjectAccessor::GetPlayer(*bot, guid) : nullptr;
    return player && player->IsAlive() && IccSameInstance(bot, player) ? player : nullptr;
}

Unit* FoeOf(Player* bot, ObjectGuid const& guid)
{
    Creature* unit = guid ? ObjectAccessor::GetCreature(*bot, guid) : nullptr;
    return unit && unit->IsAlive() && unit->IsInCombat() && bot->IsValidAttackTarget(unit) ? unit : nullptr;
}

bool CanPayFor(Player* bot, SpellInfo const* info)
{
    // POWER_HEALTH is -2 and Powers is signed, hence the cast.
    if (info->PowerType >= static_cast<uint32>(MAX_POWERS))
        return true;

    return static_cast<int32>(bot->GetPower(Powers(info->PowerType))) >=
           info->CalcPowerCost(bot, info->GetSchoolMask());
}

uint32 SpellIdOf(PlayerbotAI* ai, char const* name)
{
    return ai->GetAiObjectContext()->GetValue<uint32>("spell id", name)->Get();
}

SpellInfo const* KnownReady(Player* member, uint32 spellId)
{
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info || !member->HasSpell(spellId) || member->HasSpellCooldown(spellId) || !CanPayFor(member, info) ||
        info->CheckShapeshift(member->GetShapeshiftForm()) != SPELL_CAST_OK)
        return nullptr;

    return info;
}

bool InReach(Player* member, Unit* target, SpellInfo const* info, bool frontal)
{
    float const maxRange = member->GetSpellMaxRangeForTarget(target, info);
    // Centred on the caster: Holy Wrath, War Stomp, Shockwave, Piercing Howl
    if (maxRange <= 0.1f)
    {
        float const radius = info->Effects[EFFECT_0].CalcRadius(member);
        return radius > 0.0f && member->IsWithinDist(target, radius) &&
               (!frontal || member->HasInArc(static_cast<float>(M_PI) / 2.0f, target));
    }

    float const minRange = member->GetSpellMinRangeForTarget(target, info);
    bool const inReach = maxRange <= MELEE_SPELL_RANGE ? member->IsWithinMeleeRange(target)
                                                       : member->IsWithinCombatRange(target, maxRange);
    return inReach && !(minRange > 0.0f && member->IsWithinCombatRange(target, minRange));
}

bool StunLands(Unit* target, SpellInfo const* info)
{
    return !target->IsImmunedToSpell(info) &&
           target->GetDiminishing(GetDiminishingReturnsGroupForSpell(info, false)) < DIMINISHING_LEVEL_IMMUNE;
}

uint32 ReadyControl(PlayerbotAI* ai, Player* member, Unit* target, ControlSpell const& entry, bool stun)
{
    if (entry.cls && entry.cls != member->getClass())
        return 0;

    uint32 const spellId = SpellIdOf(ai, entry.name);
    SpellInfo const* info = KnownReady(member, spellId);
    if (!info || (stun && !StunLands(target, info)) || (!stun && target->IsImmunedToSpell(info)) ||
        (info->NeedsComboPoints() && !member->GetComboPoints(target)) ||
        !InReach(member, target, info, entry.frontal) || !member->IsWithinLOSInMap(target) ||
        !ai->CanCastSpell(spellId, target))
        return 0;

    return spellId;
}

uint32 ReadyIntimidation(PlayerbotAI* ai, Player* member, Unit* target)
{
    Pet* pet = member->GetPet();
    SpellInfo const* info = KnownReady(member, SPELL_INTIMIDATION);
    SpellInfo const* stun = sSpellMgr->GetSpellInfo(SPELL_INTIMIDATION_STUN);
    if (!pet || !pet->IsAlive() || !info || !stun || !pet->IsWithinMeleeRange(target) || !StunLands(target, stun) ||
        !ai->CanCastSpell(SPELL_INTIMIDATION, member))
        return 0;

    return SPELL_INTIMIDATION;
}

uint32 ReadyGnaw(Player* member, Unit* target)
{
    Guardian* ghoul = member->GetGuardianPet();
    SpellInfo const* info = sSpellMgr->GetSpellInfo(SPELL_GNAW);
    if (!ghoul || !ghoul->IsAlive() || !info || !ghoul->HasSpell(SPELL_GNAW) || ghoul->HasSpellCooldown(SPELL_GNAW) ||
        ghoul->HasUnitState(UNIT_STATE_CASTING) || !ghoul->IsWithinMeleeRange(target) || !StunLands(target, info))
        return 0;

    return SPELL_GNAW;
}

int32 StunRemainingMs(Unit* unit)
{
    int32 longest = 0;
    for (auto const& entry : unit->GetAppliedAuras())
    {
        Aura const* aura = entry.second->GetBase();
        if (aura->GetSpellInfo()->GetAllEffectsMechanicMask() & (1ull << MECHANIC_STUN))
            longest = std::max(longest, aura->GetDuration());
    }

    return longest;
}

Aura* PlagueAura(Unit* unit)
{
    if (Aura* aura = unit->GetAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LK_NECROTIC_PLAGUE, unit)))
        return aura;

    return unit->GetAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LK_NECROTIC_PLAGUE_JUMP, unit));
}

bool HasPlague(Unit* unit) { return PlagueAura(unit) != nullptr; }

bool HasSoulShriek(Unit* unit)
{
    return unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LK_SOUL_SHRIEK, unit));
}

uint32 CureSpellFor(Player* curer, Player* carrier)
{
    PlayerbotAI* ai = GET_PLAYERBOT_AI(curer);
    if (!ai)
        return 0;

    std::vector<char const*> names;
    switch (curer->getClass())
    {
        case CLASS_PRIEST:
            names = {"cure disease", "abolish disease"};
            break;
        case CLASS_PALADIN:
            names = {"cleanse"};
            break;
        case CLASS_SHAMAN:
            names = {"cleanse spirit", "cure toxins"};
            break;
        default:
            return 0;
    }

    if (carrier != curer && (curer->GetExactDist(carrier) > CURE_RANGE || !curer->IsWithinLOSInMap(carrier)))
        return 0;

    for (char const* name : names)
    {
        uint32 const spellId = SpellIdOf(ai, name);
        if (KnownReady(curer, spellId) && ai->CanCastSpell(spellId, carrier))
            return spellId;
    }

    return 0;
}

uint32 SilenceDispelFor(Player* dispeller, Player* member)
{
    PlayerbotAI* ai = GET_PLAYERBOT_AI(dispeller);
    if (!ai)
        return 0;

    char const* name = dispeller->getClass() == CLASS_PRIEST    ? "dispel magic"
                       : dispeller->getClass() == CLASS_PALADIN ? "cleanse"
                                                                : nullptr;
    // Cleanse would take Necrotic Plague with the silence, and it jumps from wherever the carrier stands
    if (dispeller->getClass() == CLASS_PALADIN && HasPlague(member))
        return 0;

    if (!name || (member != dispeller &&
                  (dispeller->GetExactDist(member) > DISPEL_RANGE || !dispeller->IsWithinLOSInMap(member))))
        return 0;

    uint32 const spellId = SpellIdOf(ai, name);
    return KnownReady(dispeller, spellId) && ai->CanCastSpell(spellId, member) ? spellId : 0;
}

uint32 TranquilizeFor(Player* hunter, Unit* add)
{
    PlayerbotAI* ai = GET_PLAYERBOT_AI(hunter);
    if (!ai || hunter->getClass() != CLASS_HUNTER)
        return 0;

    uint32 const spellId = SpellIdOf(ai, "tranquilizing shot");
    SpellInfo const* info = KnownReady(hunter, spellId);
    if (!info || !InReach(hunter, add, info, false) || !hunter->IsWithinLOSInMap(add) ||
        !ai->CanCastSpell(spellId, add))
        return 0;

    return spellId;
}

uint32 AoeTauntSpell(Player* tank)
{
    uint32 const spellId = tank->getClass() == CLASS_WARRIOR ? SPELL_CHALLENGING_SHOUT
                           : tank->getClass() == CLASS_DRUID ? SPELL_CHALLENGING_ROAR
                                                             : 0;
    return spellId && KnownReady(tank, spellId) ? spellId : 0;
}

bool UnableToAct(Player* member)
{
    return !member->IsAlive() || member->GetVehicle() || IccLichKingInFrostmourne(member) || HarvestStunned(member);
}

void Scan(LkState& st, Creature* boss, uint32 now)
{
    if (st.scanned && getMSTimeDiff(st.scanMs, now) < SCAN_MS)
        return;

    st.scanned = true;
    st.scanMs = now;
    st.adds.clear();
    st.spirits.clear();
    st.spheres.clear();
    st.traps.clear();

    auto const found = [&](uint32 entry)
    {
        std::list<Creature*> list;
        boss->GetCreatureListWithEntryInGrid(list, entry, SCAN_RANGE);
        std::vector<Creature*> units;
        for (Creature* unit : list)
            if (unit->IsAlive() && unit->GetExactDist2d(LK_CENTER) <= GATE_RANGE)
                units.push_back(unit);

        std::sort(units.begin(), units.end(), IccGuidLess());
        return units;
    };

    for (uint32 const entry : {NPC_LK_DRUDGE_GHOUL, NPC_LK_SHAMBLING_HORROR})
        for (Creature* add : found(entry))
        {
            AddInfo info;
            info.guid = add->GetGUID();
            info.entry = entry;
            info.pos = add->GetPosition();
            info.healthPct = add->GetHealthPct();
            info.emerged = !add->HasReactState(REACT_PASSIVE);
            info.inCombat = add->IsInCombat();
            if (Unit* victim = add->GetVictim())
                info.victim = victim->GetGUID();
            info.enraged = add->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LK_SHAMBLING_ENRAGE, add));
            info.frenzied = add->HasAura(SPELL_LK_SHAMBLING_FRENZY);
            st.adds.push_back(info);
        }

    for (Creature* spirit : found(NPC_LK_RAGING_SPIRIT))
    {
        SpiritInfo info;
        info.guid = spirit->GetGUID();
        info.pos = spirit->GetPosition();
        info.healthPct = spirit->GetHealthPct();
        info.rooted = spirit->HasUnitState(UNIT_STATE_ROOT);
        info.passive = spirit->HasReactState(REACT_PASSIVE);
        info.inCombat = spirit->IsInCombat();
        if (Unit* victim = spirit->GetVictim())
            info.victim = victim->GetGUID();
        st.spirits.push_back(info);
    }

    for (Creature* sphere : found(NPC_LK_ICE_SPHERE))
    {
        SphereInfo info;
        info.guid = sphere->GetGUID();
        info.pos = sphere->GetPosition();
        info.inCombat = sphere->IsInCombat();
        if (Unit* victim = sphere->GetVictim())
            info.toVictim = sphere->GetExactDist2d(victim);
        st.spheres.push_back(info);
    }

    for (Creature* trap : found(NPC_LK_SHADOW_TRAP))
        st.traps.push_back({trap->GetGUID(), trap->GetPosition()});
}

float NearestTrap(LkState const& st, Position const& pos)
{
    float nearest = 1000.0f;
    for (TrapInfo const& trap : st.traps)
        nearest = std::min(nearest, trap.pos.GetExactDist2d(pos));

    return nearest;
}

TrapInfo const* NearestTrapInfo(LkState const& st, Position const& pos)
{
    TrapInfo const* nearest = nullptr;
    for (TrapInfo const& trap : st.traps)
        if (!nearest || trap.pos.GetExactDist2d(pos) < nearest->pos.GetExactDist2d(pos))
            nearest = &trap;

    return nearest;
}

// No straight walk from `from` passes within TRAP_SEGMENT of a trap, past the one it may be leaving
bool SegmentClearOfTraps(LkState const& st, Position const& from, Position const& to)
{
    for (TrapInfo const& trap : st.traps)
    {
        if (trap.pos.GetExactDist2d(from) < TRAP_SEGMENT)
            continue;

        if (SegmentCrossesCircle(from, to, HazardCircle(trap.pos, TRAP_SEGMENT)))
            return false;
    }

    return true;
}

bool FloorReads(Player* bot, float x, float y, float floorZ)
{
    return std::fabs(bot->GetMapHeight(x, y, floorZ) - floorZ) <= FLOOR_Z_TOLERANCE;
}

bool RingStandable(Player* bot)
{
    InstanceScript* instance = bot->GetMapId() == ICC_MAP_ID ? bot->GetInstanceScript() : nullptr;
    GameObject* platform =
        instance ? ObjectAccessor::GetGameObject(*bot, instance->GetGuidData(LK_DATA_ARTHAS_PLATFORM)) : nullptr;
    // Only the Quake's shatter drops the ring, and it needs the platform it shatters
    return !platform || platform->GetDestructibleState() != GO_DESTRUCTIBLE_DAMAGED;
}

FloorContext MakeFloorContext(Player* bot)
{
    FloorContext ctx;
    ctx.frostmourne = bot->GetExactDist2d(LK_FROSTMOURNE) <= FM_GATE_RANGE;
    ctx.heightKnown =
        FloorReads(bot, bot->GetPositionX(), bot->GetPositionY(), ctx.frostmourne ? FM_FLOOR_Z : FLOOR_Z);
    ctx.ringStandable = RingStandable(bot);
    return ctx;
}

FloorContext FloorContextOf(Player* bot)
{
    LkState* st = bot->GetInstanceId() ? lkStates.Find(bot->GetInstanceId()) : nullptr;
    if (!st)
        return MakeFloorContext(bot);

    // A sweep asks for every point it tries, and the answer only moves with the bot
    FloorMemo& memo = st->floors[bot->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.ctx = MakeFloorContext(bot);
    }

    return memo.ctx;
}

bool FloorAccepts(Player* bot, float x, float y, FloorContext const& ctx)
{
    if (ctx.frostmourne)
        return LK_FROSTMOURNE.GetExactDist2d(x, y) <= FM_SPOT_RADIUS &&
               (!ctx.heightKnown || FloorReads(bot, x, y, FM_FLOOR_Z));

    float const r = LK_CENTER.GetExactDist2d(x, y);
    if (r > SOLID_RADIUS && (r < RING_INNER || r > RING_OUTER || !ctx.ringStandable))
        return false;

    return !ctx.heightKnown || FloorReads(bot, x, y, FLOOR_Z);
}

// The nearest spot round `center` clear of every trap, on the floor, walked to without crossing a trap
bool SweepAround(LkState const& st, Player* bot, Position const& center, Position& out)
{
    FloorContext const ctx = FloorContextOf(bot);
    Position const from = bot->GetPosition();
    for (float r = RELATCH_SWEEP_STEP; r <= RELATCH_SWEEP_MAX; r += RELATCH_SWEEP_STEP)
    {
        bool found = false;
        float best = 0.0f;
        for (int k = 0; k < RELATCH_SWEEP_ANGLES; ++k)
        {
            float const angle = static_cast<float>(k) * 2.0f * static_cast<float>(M_PI) / RELATCH_SWEEP_ANGLES;
            Position const spot(center.GetPositionX() + r * std::cos(angle),
                                center.GetPositionY() + r * std::sin(angle), center.GetPositionZ());
            if (NearestTrap(st, spot) < TRAP_CLEAR + PARK ||
                !FloorAccepts(bot, spot.GetPositionX(), spot.GetPositionY(), ctx) ||
                !SegmentClearOfTraps(st, from, spot))
                continue;

            float const distance = from.GetExactDist2d(spot);
            if (!found || distance < best)
            {
                found = true;
                best = distance;
                out = spot;
            }
        }

        if (found)
            return true;
    }

    return false;
}

// The phase one ranged and healer nodes, nearest R1 first
std::vector<Position> const& Lattice()
{
    static std::vector<Position> const nodes = []
    {
        Position const round = PolarSpot(RANGED_ROUND);
        Position const pile = PolarSpot(ADD_PILE);
        Position const holder = PolarSpot(HOLDER_SPOT);
        std::vector<std::tuple<int, float, float>> found;
        for (int i = -3; i <= 3; ++i)
            for (int j = -3; j <= 3; ++j)
            {
                float const x = round.GetPositionX() + LATTICE_SPACING * (static_cast<float>(i) + 0.5f * j);
                float const y = round.GetPositionY() + LATTICE_SPACING * 0.8660254f * static_cast<float>(j);
                Position const node(x, y, FLOOR_Z);
                float const distance = node.GetExactDist2d(round);
                if (distance <= LATTICE_REACH + 0.01f && node.GetExactDist2d(pile) >= LATTICE_PILE_MIN &&
                    node.GetExactDist2d(holder) <= LATTICE_HOLDER_MAX)
                    found.emplace_back(static_cast<int>(std::lround(distance * 10.0f)), x, y);
            }

        std::sort(found.begin(), found.end());
        std::vector<Position> out;
        for (auto const& node : found)
            out.emplace_back(std::get<1>(node), std::get<2>(node), FLOOR_Z);

        return out;
    }();

    return nodes;
}

Position SpiritSpot(size_t index, bool standable)
{
    Position const spot = PolarSpot(SPIRIT_SPOTS[index < SPIRIT_TANKS ? index : 0]);
    return standable ? spot : AtRadius(spot, DAMAGED_HOLD);
}

Position WinterSlotSpot(WinterSlot const& slot, bool tenNormal, bool standable)
{
    uint8 const column = slot.index / 2;
    float const r = slot.index % 2 ? WINTER_OUTER : WINTER_INNER;
    float const step = WINTER_SPACING / r * 180.0f / static_cast<float>(M_PI);
    float const base = slot.ranged ? (tenNormal ? RANGED_CLUSTER_10N : RANGED_CLUSTER) : 0.0f;
    float const deg = slot.ranged ? base + column * step : base - column * step;
    return PolarSpot({deg, standable ? r : DAMAGED_HOLD});
}

int SpiritTankIndex(LkState const& st, ObjectGuid const& guid)
{
    for (size_t i = 0; i < st.spiritTanks.size() && i < SPIRIT_TANKS; ++i)
        if (st.spiritTanks[i] == guid)
            return static_cast<int>(i);

    return -1;
}

// A spirit on its tank and at its tank's spot, the lowest first
SpiritInfo const* HeldSpirit(LkState const& st)
{
    SpiritInfo const* best = nullptr;
    for (SpiritInfo const& spirit : st.spirits)
    {
        auto const itr = st.spiritTank.find(spirit.guid);
        if (!spirit.inCombat || spirit.passive || itr == st.spiritTank.end() || spirit.victim != itr->second)
            continue;

        int const index = SpiritTankIndex(st, itr->second);
        if (index < 0 ||
            spirit.pos.GetExactDist2d(SpiritSpot(static_cast<size_t>(index), st.ringStandable)) > SPIRIT_HELD_NEAR)
            continue;

        if (!best || spirit.healthPct < best->healthPct)
            best = &spirit;
    }

    return best;
}

// A phase one add the free tank brought to its spirit spot, the lowest first
AddInfo const* HeldLeftover(LkState const& st)
{
    int const index = SpiritTankIndex(st, st.freeTank);
    if (index < 0)
        return nullptr;

    Position const spot = SpiritSpot(static_cast<size_t>(index), st.ringStandable);
    AddInfo const* best = nullptr;
    for (AddInfo const& add : st.adds)
        if (add.inCombat && add.victim == st.freeTank && add.pos.GetExactDist2d(spot) <= SPIRIT_HELD_NEAR &&
            (!best || add.healthPct < best->healthPct))
            best = &add;

    return best;
}

struct JumpPick
{
    bool found = false;
    bool add = false;
    ObjectGuid guid;
};

// Where Necrotic Plague would jump off the carrier now: the nearest member or add within 10 yd, not him
JumpPick NearestJumpUnit(LkState const& st, Player* carrier, std::vector<Player*> const& members)
{
    JumpPick pick;
    float best = PLAGUE_JUMP;
    for (Player* member : members)
    {
        if (member == carrier || !member->IsAlive())
            continue;

        float const distance = carrier->GetExactDist(member);
        if (distance <= best)
        {
            best = distance;
            pick = {true, false, member->GetGUID()};
        }
    }

    for (AddInfo const& add : st.adds)
    {
        float const distance = carrier->GetExactDist(add.pos);
        if (distance <= best)
        {
            best = distance;
            pick = {true, true, add.guid};
        }
    }

    return pick;
}

// The carrier is to stand clear of everyone rather than at the pile
bool IsolateMode(LkState const& st)
{
    return st.phase != IccLichKingPhaseId::One || st.adds.empty();
}

IccLichKingPhaseId RaidPhaseOf(Creature* boss, std::vector<Player*> const& members, bool pastThree, uint8 winters)
{
    bool harvested = false;
    for (Player* member : members)
        harvested = harvested || Harvested(member);

    return PhaseOf(boss, harvested, pastThree, winters);
}

void UpdateQuake(LkState& st, Creature* boss)
{
    bool const passive = boss->HasReactState(REACT_PASSIVE);
    if (!IsWinter(st.phase) || !passive)
        st.quake = false;
    else if (boss->FindCurrentSpellBySpellId(SPELL_QUAKE))
        st.quake = true;
}

// Phase one with no tank player but the holder: a DK, then a paladin damage dealer holds the ghouls by its
// class taunt and plate. Taunt needs Defensive Stance and Growl bear form, and a Shambling's swings kill a
// non-tank.
void AssignStandIn(LkState& st, std::vector<Player*> const& members)
{
    ObjectGuid const previous = st.standIn;
    st.standIn.Clear();
    if (st.freeTank || st.phase != IccLichKingPhaseId::One)
        return;

    for (Player* member : members)
        if (IsLkTankPlayer(member) && member->GetGUID() != st.holder)
            return;

    Player* best = nullptr;
    for (Player* member : members)
    {
        bool const dk = member->getClass() == CLASS_DEATH_KNIGHT;
        if (!IsLkDutyBot(member) || !IsRaidDpsBot(member) || UnableToAct(member) ||
            (!dk && member->getClass() != CLASS_PALADIN))
            continue;

        if (member->GetGUID() == previous)
        {
            best = member;
            break;
        }

        bool const bestDk = best && best->getClass() == CLASS_DEATH_KNIGHT;
        if (!best || (dk != bestDk ? dk : member->GetGUID() < best->GetGUID()))
            best = member;
    }

    if (best)
        st.standIn = best->GetGUID();
}

void AssignTanks(LkState& st, Player* bot, std::vector<Player*> const& members, Creature* boss)
{
    std::unordered_set<ObjectGuid> roster;
    for (Player* member : members)
        roster.insert(member->GetGUID());

    auto const holds = [&](Player* player)
    { return player && roster.count(player->GetGUID()) && IsLkTankPlayer(player) && !UnableToAct(player); };

    std::vector<Player*> botTanks;
    for (Player* member : members)
        if (IsLkDutyBot(member) && IsLkTankPlayer(member))
            botTanks.push_back(member);

    std::sort(botTanks.begin(), botTanks.end(), IccGuidLess());

    Unit* victim = boss->IsInCombat() ? boss->GetVictim() : nullptr;
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    if (!holds(holder))
        holder = PlayerOf(bot, st.holder);
    if (!holds(holder))
        holder = GetGroupMainTank(bot);
    for (auto itr = botTanks.begin(); !holds(holder) && itr != botTanks.end(); ++itr)
        holder = *itr;

    st.holder = holds(holder) ? holder->GetGUID() : ObjectGuid::Empty;
    st.freeTank.Clear();
    for (Player* tank : botTanks)
        if (tank->GetGUID() != st.holder && holds(tank))
        {
            st.freeTank = tank->GetGUID();
            break;
        }

    AssignStandIn(st, members);

    std::vector<ObjectGuid> dropped;
    for (auto const& entry : st.tank)
        if (entry.first != st.holder && entry.first != st.freeTank && entry.first != st.standIn)
            dropped.push_back(entry.first);

    for (ObjectGuid const& guid : dropped)
        st.tank.erase(guid);

    if (st.holder)
        st.tank[st.holder] = static_cast<uint8>(LkTankRole::Holder);
    if (st.freeTank)
        st.tank[st.freeTank] = static_cast<uint8>(LkTankRole::Free);
    if (st.standIn)
        st.tank[st.standIn] = static_cast<uint8>(LkTankRole::StandIn);
}

// The holder bot and the free tank hold the Raging Spirits. A winter's new spirit goes to the one holding
// fewer, then the lower guid. Later phases leave every spirit to the free tank.
void AssignSpirits(LkState& st, Player* bot)
{
    st.spiritTanks.clear();
    if (Player* holder = PlayerOf(bot, st.holder))
        if (IsLkDutyBot(holder))
            st.spiritTanks.push_back(st.holder);
    if (st.freeTank)
        st.spiritTanks.push_back(st.freeTank);

    std::unordered_set<ObjectGuid> alive;
    for (SpiritInfo const& spirit : st.spirits)
        alive.insert(spirit.guid);

    bool const winter = IsWinter(st.phase);
    for (auto itr = st.spiritTank.begin(); itr != st.spiritTank.end();)
    {
        bool const keep = alive.count(itr->first) && SpiritTankIndex(st, itr->second) >= 0 &&
                          (winter || itr->second == st.freeTank);
        itr = keep ? std::next(itr) : st.spiritTank.erase(itr);
    }

    for (SpiritInfo const& spirit : st.spirits)
    {
        if (st.spiritTank.count(spirit.guid))
            continue;

        ObjectGuid tank;
        if (winter)
        {
            size_t fewest = 0;
            for (ObjectGuid const& candidate : st.spiritTanks)
            {
                size_t held = 0;
                for (auto const& entry : st.spiritTank)
                    held += entry.second == candidate ? 1 : 0;

                if (!tank || held < fewest || (held == fewest && candidate < tank))
                {
                    tank = candidate;
                    fewest = held;
                }
            }
        }
        else
            tank = st.freeTank;

        if (tank)
            st.spiritTank[spirit.guid] = tank;
    }
}

// Soul Reaper's hit lands 5 s after the aura: covered when a button already up lasts that long or one is
// off cooldown for it
bool ReaperUncovered(Player* holder, Creature* boss)
{
    Aura const* aura = holder->GetAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LK_SOUL_REAPER, boss));
    if (!aura)
        return false;

    uint32 const needMs = aura->GetDuration() > 0 ? static_cast<uint32>(aura->GetDuration()) : 0;
    return !RunningTankDefensive(holder, false, needMs) && !ReadyTankDefensiveSpell(holder, false, 0, needMs);
}

void AssignTaunts(LkState& st, Player* bot, Creature* boss)
{
    st.taunts.clear();
    if (!boss->IsInCombat() || st.phase == IccLichKingPhaseId::Outro ||
        st.phase == IccLichKingPhaseId::Frostmourne)
        return;

    Player* holder = PlayerOf(bot, st.holder);
    Player* freeTank = PlayerOf(bot, st.freeTank);
    auto const elect = [&](Player* tank, char const* rule, ObjectGuid const& unit, bool aoe)
    {
        if (tank && IsLkDutyBot(tank) && !st.taunts.count(tank->GetGUID()))
            st.taunts[tank->GetGUID()] = {rule, unit, aoe};
    };

    Unit* victim = boss->GetVictim();
    bool const aggressive = !boss->HasReactState(REACT_PASSIVE) && IsFighting(st.phase);

    // Soul Reaper on the holder with nothing to cover it: the free tank takes him at once, and the roles
    // follow his victim on the next pass
    if (aggressive && st.phase != IccLichKingPhaseId::One && holder && freeTank && victim == holder &&
        ReaperUncovered(holder, boss))
        elect(freeTank, "reaper", boss->GetGUID(), false);

    if (aggressive && holder && victim && victim != holder)
    {
        Player* held = victim->ToPlayer();
        if (!(held && IsLkTankPlayer(held)))
            elect(holder, "back", boss->GetGUID(), false);
    }

    for (SpiritInfo const& spirit : st.spirits)
    {
        auto const itr = st.spiritTank.find(spirit.guid);
        if (spirit.rooted || spirit.passive || itr == st.spiritTank.end() || spirit.victim == itr->second)
            continue;

        // A human tank holding one plays its own part
        Player* held = PlayerOf(bot, spirit.victim);
        if (held && IsLkTankPlayer(held) && !IsLkDutyBot(held))
            continue;

        elect(PlayerOf(bot, itr->second), "spirit", spirit.guid, false);
    }

    // The stand-in takes the ghouls alone, by its class taunt
    if (Player* standIn = PlayerOf(bot, st.standIn))
    {
        AddInfo const* nearest = nullptr;
        for (AddInfo const& add : st.adds)
            if (add.entry == NPC_LK_DRUDGE_GHOUL && add.emerged && add.inCombat && add.victim != st.standIn &&
                (!nearest || standIn->GetExactDist2d(add.pos) < standIn->GetExactDist2d(nearest->pos)))
                nearest = &add;

        if (nearest)
            elect(standIn, "add", nearest->guid, false);
    }

    // The adds end their emerge on whoever has threat or a random non-tank
    if (freeTank && (st.phase == IccLichKingPhaseId::One || IsWinter(st.phase)))
    {
        AddInfo const* nearest = nullptr;
        size_t close = 0;
        for (AddInfo const& add : st.adds)
        {
            if (!add.emerged || !add.inCombat || add.victim == st.freeTank)
                continue;

            if (freeTank->GetExactDist2d(add.pos) <= AOE_TAUNT_RADIUS)
                ++close;

            if (!nearest || freeTank->GetExactDist2d(add.pos) < freeTank->GetExactDist2d(nearest->pos))
                nearest = &add;
        }

        if (nearest)
            elect(freeTank, "add", nearest->guid, close >= AOE_TAUNT_MIN && AoeTauntSpell(freeTank));
    }
}

std::vector<Player*> CurerOrder(std::vector<Player*> const& members)
{
    std::vector<Player*> curers;
    for (Player* member : members)
    {
        uint8 const cls = member->getClass();
        if (IsLkDutyBot(member) && !UnableToAct(member) && !HasSoulShriek(member) &&
            (cls == CLASS_PRIEST || cls == CLASS_PALADIN || cls == CLASS_SHAMAN))
            curers.push_back(member);
    }

    std::sort(curers.begin(), curers.end(),
              [](Player* a, Player* b)
              {
                  bool const ha = IsLkHealer(a);
                  bool const hb = IsLkHealer(b);
                  return ha != hb ? ha : a->GetGUID() < b->GetGUID();
              });
    return curers;
}

CureRule DueRule(LkState const& st, Player* carrier, std::vector<Player*> const& members)
{
    JumpPick const jump = NearestJumpUnit(st, carrier, members);
    if (jump.found && jump.add)
        return carrier->GetGUID() == st.freeTank || carrier->GetGUID() == st.standIn ? CureRule::AddTank
                                                                                      : CureRule::Delivered;

    if (!jump.found && IsolateMode(st))
        return CureRule::Isolated;

    Aura* aura = PlagueAura(carrier);
    AuraEffect const* effect = aura ? aura->GetEffect(EFFECT_0) : nullptr;
    if ((effect && effect->GetPeriodicTimer() < PLAGUE_TICK_MARGIN_MS) || carrier->GetHealthPct() < PLAGUE_LOW_PCT)
        return CureRule::Emergency;

    return CureRule::None;
}

void AssignCures(LkState& st, std::vector<Player*> const& members)
{
    st.carriers.clear();
    std::vector<Player*> carriers;
    for (Player* member : members)
        if (HasPlague(member))
        {
            st.carriers.insert(member->GetGUID());
            carriers.push_back(member);
        }

    std::sort(carriers.begin(), carriers.end(), IccGuidLess());

    std::unordered_map<ObjectGuid, ObjectGuid> previous;
    for (auto const& entry : st.cures)
        previous[entry.second.carrier] = entry.first;

    st.cures.clear();
    if (carriers.empty())
        return;

    std::vector<Player*> const curers = CurerOrder(members);
    std::unordered_set<ObjectGuid> busy;
    for (Player* carrier : carriers)
    {
        Player* curer = nullptr;
        auto const kept = previous.find(carrier->GetGUID());
        if (kept != previous.end())
            for (Player* candidate : curers)
                if (candidate->GetGUID() == kept->second && CureSpellFor(candidate, carrier))
                    curer = candidate;

        for (auto itr = curers.begin(); !curer && itr != curers.end(); ++itr)
            if (!busy.count((*itr)->GetGUID()) && CureSpellFor(*itr, carrier))
                curer = *itr;

        if (!curer)
            continue;

        busy.insert(curer->GetGUID());
        st.cures[curer->GetGUID()] = {carrier->GetGUID(), DueRule(st, carrier, members)};
    }
}

void AssignTranquilize(LkState& st, std::vector<Player*> const& members)
{
    st.tranq.clear();
    std::vector<Player*> hunters;
    for (Player* member : members)
        if (IsLkDutyBot(member) && member->getClass() == CLASS_HUNTER && !UnableToAct(member))
            hunters.push_back(member);

    std::sort(hunters.begin(), hunters.end(), IccGuidLess());
    std::unordered_set<ObjectGuid> busy;
    for (AddInfo const& add : st.adds)
    {
        if (!add.enraged || !add.inCombat)
            continue;

        Creature* unit = hunters.empty() ? nullptr : ObjectAccessor::GetCreature(*hunters.front(), add.guid);
        if (!unit)
            continue;

        Player* best = nullptr;
        for (Player* hunter : hunters)
            if (!busy.count(hunter->GetGUID()) && TranquilizeFor(hunter, unit) &&
                (!best || hunter->GetExactDist(unit) < best->GetExactDist(unit)))
                best = hunter;

        if (best)
        {
            busy.insert(best->GetGUID());
            st.tranq[best->GetGUID()] = add.guid;
        }
    }
}

// Heroic: one stun in flight per Frenzied Shambling Horror, the next stunner once it has under 0.5 s left
void AssignStuns(LkState& st, Player* bot, std::vector<Player*> const& members, uint32 now)
{
    std::unordered_set<ObjectGuid> frenzied;
    if (st.heroic && st.phase == IccLichKingPhaseId::One)
        for (AddInfo const& add : st.adds)
            if (add.entry == NPC_LK_SHAMBLING_HORROR && add.frenzied && add.inCombat)
                frenzied.insert(add.guid);

    std::vector<ObjectGuid> dropped;
    for (auto const& [stunner, guid] : st.stun)
    {
        Player* member = PlayerOf(bot, stunner);
        Creature* unit = frenzied.count(guid) ? ObjectAccessor::GetCreature(*bot, guid) : nullptr;
        auto const since = st.stunSince.find(stunner);
        bool const stale = since == st.stunSince.end() || getMSTimeDiff(since->second, now) > STUN_STALE_MS;
        if (!member || !unit || StunRemainingMs(unit) >= STUN_NEXT_MS || stale || !LkReadyStun(member, unit))
        {
            dropped.push_back(stunner);
            if (stale)
                st.stunTried[stunner] = now;
        }
    }

    for (ObjectGuid const& guid : dropped)
    {
        st.stun.erase(guid);
        st.stunSince.erase(guid);
    }

    if (frenzied.empty())
        return;

    std::vector<Player*> stunners;
    for (Player* member : members)
        if (IsLkDutyBot(member) && !UnableToAct(member))
            stunners.push_back(member);

    std::sort(stunners.begin(), stunners.end(), IccGuidLess());
    std::vector<ObjectGuid> order(frenzied.begin(), frenzied.end());
    std::sort(order.begin(), order.end());
    for (ObjectGuid const& guid : order)
    {
        bool covered = false;
        for (auto const& entry : st.stun)
            covered = covered || entry.second == guid;

        Creature* unit = ObjectAccessor::GetCreature(*bot, guid);
        if (covered || !unit || StunRemainingMs(unit) >= STUN_NEXT_MS)
            continue;

        for (Player* member : stunners)
        {
            auto const tried = st.stunTried.find(member->GetGUID());
            if (st.stun.count(member->GetGUID()) ||
                (tried != st.stunTried.end() && getMSTimeDiff(tried->second, now) < STUN_RETRY_MS) ||
                !LkReadyStun(member, unit))
                continue;

            st.stun[member->GetGUID()] = guid;
            st.stunSince[member->GetGUID()] = now;
            break;
        }
    }
}

// Soul Shriek's silenced healers first, each to a priest or paladin, healers first
void AssignSilence(LkState& st, std::vector<Player*> const& members)
{
    st.silence.clear();
    std::vector<Player*> silenced;
    for (Player* member : members)
        if (HasSoulShriek(member))
            silenced.push_back(member);

    if (silenced.empty())
        return;

    std::sort(silenced.begin(), silenced.end(),
              [](Player* a, Player* b)
              {
                  bool const ha = IsLkHealer(a);
                  bool const hb = IsLkHealer(b);
                  return ha != hb ? ha : a->GetGUID() < b->GetGUID();
              });

    std::vector<Player*> dispellers;
    for (Player* member : CurerOrder(members))
        if (member->getClass() != CLASS_SHAMAN)
            dispellers.push_back(member);

    std::unordered_set<ObjectGuid> busy;
    for (Player* member : silenced)
        for (Player* dispeller : dispellers)
            if (!busy.count(dispeller->GetGUID()) && SilenceDispelFor(dispeller, member))
            {
                busy.insert(dispeller->GetGUID());
                st.silence[dispeller->GetGUID()] = member->GetGUID();
                break;
            }
}

bool StandsOnLattice(Player* member)
{
    return IsLkDutyBot(member) && !IsLkTankPlayer(member) &&
           (IsLkHealer(member) || (IsRaidDpsBot(member) && !PlayerbotAI::IsMelee(member)));
}

// Latched per bot: a newcomer takes the first free node, and past the last node they share in order
void AssignRangedNodes(LkState& st, std::vector<Player*> const& members)
{
    std::vector<Player*> ranged;
    std::unordered_set<ObjectGuid> roster;
    for (Player* member : members)
        if (StandsOnLattice(member))
        {
            ranged.push_back(member);
            roster.insert(member->GetGUID());
        }

    for (auto itr = st.rangedNode.begin(); itr != st.rangedNode.end();)
        itr = roster.count(itr->first) ? std::next(itr) : st.rangedNode.erase(itr);

    std::sort(ranged.begin(), ranged.end(), IccGuidLess());
    size_t const count = Lattice().size();
    if (!count)
        return;

    std::vector<uint8> used(count, 0);
    for (auto const& entry : st.rangedNode)
        if (entry.second < count)
            ++used[entry.second];

    for (Player* member : ranged)
    {
        if (st.rangedNode.count(member->GetGUID()))
            continue;

        size_t pick = 0;
        for (size_t node = 1; node < count; ++node)
            if (used[node] < used[pick])
                pick = node;

        ++used[pick];
        st.rangedNode[member->GetGUID()] = static_cast<uint8>(pick);
    }
}

// Laid out once per winter: the tank cluster takes the tanks off the spirits, melee, half the healers (all
// of them on 10N) and the sphere guards, the ranged cluster the rest. A bot joining later takes the next
// slot.
void AssignWinterSlots(LkState& st, std::vector<Player*> const& members)
{
    if (!IsWinter(st.phase))
    {
        st.winterOf = IccLichKingPhaseId::One;
        st.winterSlots.clear();
        return;
    }

    if (st.winterOf != st.phase)
    {
        st.winterOf = st.phase;
        st.winterSlots.clear();
        st.winterNext[0] = 0;
        st.winterNext[1] = 0;
    }

    std::vector<Player*> tanks;
    std::vector<Player*> melee;
    std::vector<Player*> healers;
    std::vector<Player*> ranged;
    for (Player* member : members)
    {
        if (!IsLkDutyBot(member) || SpiritTankIndex(st, member->GetGUID()) >= 0)
            continue;

        if (IsLkTankPlayer(member))
            tanks.push_back(member);
        else if (IsLkHealer(member))
            healers.push_back(member);
        else if (PlayerbotAI::IsMelee(member))
            melee.push_back(member);
        else
            ranged.push_back(member);
    }

    for (auto* list : {&tanks, &melee, &healers, &ranged})
        std::sort(list->begin(), list->end(), IccGuidLess());

    std::vector<Player*> guards;
    if (!st.tenNormal)
    {
        std::stable_sort(ranged.begin(), ranged.end(), [](Player* a, Player* b)
                         { return a->getClass() == CLASS_HUNTER && b->getClass() != CLASS_HUNTER; });
        size_t const count = (ranged.size() + SPHERE_GUARD_SHARE - 1) / SPHERE_GUARD_SHARE;
        guards.assign(ranged.begin(), ranged.begin() + count);
        ranged.erase(ranged.begin(), ranged.begin() + count);
    }

    // The tank cluster in slot order, the guards in its middle columns so every path to it passes in reach
    size_t const tankHealers = st.tenNormal ? healers.size() : (healers.size() + 1) / 2;
    std::vector<Player*> cluster(tanks);
    cluster.insert(cluster.end(), melee.begin(), melee.end());
    cluster.insert(cluster.end(), healers.begin(), healers.begin() + tankHealers);
    size_t const middle = (cluster.size() / 2) & ~size_t{1};
    cluster.insert(cluster.begin() + middle, guards.begin(), guards.end());

    auto const place = [&](Player* member, bool rangedCluster, bool guard)
    {
        if (st.winterSlots.count(member->GetGUID()))
            return;

        uint8& next = st.winterNext[rangedCluster ? 1 : 0];
        st.winterSlots[member->GetGUID()] = {rangedCluster, next, guard};
        ++next;
    };

    std::unordered_set<ObjectGuid> guardOf;
    for (Player* member : guards)
        guardOf.insert(member->GetGUID());

    for (Player* member : cluster)
        place(member, false, guardOf.count(member->GetGUID()) != 0);
    for (size_t i = tankHealers; i < healers.size(); ++i)
        place(healers[i], true, false);
    for (Player* member : ranged)
        place(member, true, false);
}

void ApplyHeroicBuffs(LkState const& st, Player* bot, Creature* boss, std::vector<Player*> const& members)
{
    if (!sPlayerbotAIConfig.EnableICCBuffs || !st.heroic || !boss->IsInCombat())
        return;

    bool const empowered = boss->HealthBelowPct(EMPOWERED_BLOOD_FROM) && boss->HealthAbovePct(EMPOWERED_BLOOD_TO);
    for (Player* member : members)
    {
        IccApplyHeroicBuffToMember(bot, member, true, true, "lk.heroicbuff");
        if (empowered && !member->HasAura(SPELL_EMPOWERED_BLOOD))
        {
            member->AddAura(SPELL_EMPOWERED_BLOOD, member);
            IccNoteCheat(bot, "lk.empoweredblood", member);
        }
    }
}

void RefreshDuties(LkState& st, Player* bot, Creature* boss, uint32 now)
{
    if (st.dutiesRan && getMSTimeDiff(st.dutyMs, now) < DUTY_MS)
        return;

    st.dutiesRan = true;
    st.dutyMs = now;
    std::vector<Player*> const members = LkMembersOnMap(bot);
    Difficulty const difficulty = GetRaidMapDifficulty(bot);
    st.heroic = IsHeroic(bot);
    st.tenNormal = difficulty == RAID_DIFFICULTY_10MAN_NORMAL;
    st.phase = RaidPhaseOf(boss, members, st.reachedThree, st.winters);
    st.reachedThree = st.reachedThree || st.phase == IccLichKingPhaseId::Three;
    if (st.phase == IccLichKingPhaseId::Winter1)
        st.winters |= WINTER_FIRST;
    else if (st.phase == IccLichKingPhaseId::Winter2)
        st.winters |= WINTER_FIRST | WINTER_SECOND;
    st.ringStandable = RingStandable(bot);

    UpdateQuake(st, boss);
    AssignTanks(st, bot, members, boss);
    AssignSpirits(st, bot);
    AssignTaunts(st, bot, boss);
    AssignCures(st, members);
    AssignTranquilize(st, members);
    AssignStuns(st, bot, members, now);
    AssignSilence(st, members);
    AssignRangedNodes(st, members);
    AssignWinterSlots(st, members);
    ApplyHeroicBuffs(st, bot, boss, members);
}

// Past BRAKE_CAP of the holder's threat until under BRAKE_RELEASE, read from his threat list
void UpdateBrake(LkState& st, Player* bot, Creature* boss)
{
    bool hold = false;
    Player* holder = PlayerOf(bot, st.holder);
    if (boss->IsInCombat() && IsFighting(st.phase) && !boss->HasReactState(REACT_PASSIVE) && holder &&
        holder != bot)
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

void UpdateQuakeWalk(LkState const& st, LkBotState& bs, Player* bot)
{
    if (!st.quake)
    {
        bs.quake = false;
        return;
    }

    if (bs.quake || bot->GetExactDist2d(LK_CENTER) <= QUAKE_FROM)
        return;

    bs.quake = true;
    bs.quakeSpot = AtRadius(bot->GetPosition(), QUAKE_TO);
}

void UpdateTrap(LkState const& st, LkBotState& bs, Player* bot, Creature* boss, uint32 now)
{
    if (st.phase != IccLichKingPhaseId::One || st.traps.empty())
    {
        bs.trap = false;
        return;
    }

    float const nearest = NearestTrap(st, bot->GetPosition());
    if (bs.trap)
    {
        if (nearest >= TRAP_CLEAR)
        {
            bs.trap = false;
            return;
        }

        if (NearestTrap(st, bs.trapSpot) >= TRAP_CLEAR)
            return;
    }
    else if (nearest >= TRAP_TRIGGER)
        return;

    if (bs.trapBlockedMs && getMSTimeDiff(bs.trapBlockedMs, now) < SWEEP_RETRY_MS)
        return;

    std::vector<HazardCircle> circles;
    for (TrapInfo const& trap : st.traps)
        circles.emplace_back(trap.pos, TRAP_CLEAR + PARK);

    FloorContext const ctx = FloorContextOf(bot);
    Position const from = bot->GetPosition();
    auto const accept = [&](float x, float y)
    { return FloorAccepts(bot, x, y, ctx) && SegmentClearOfTraps(st, from, Position(x, y, FLOOR_Z)); };

    // Melee have no stand: the spot nearest him keeps their reach back to him out of the throw
    Position const bossPos = boss->GetPosition();
    Position const* prefer = bs.stand != StandKind::None ? &bs.standWanted : &bossPos;
    Position const found = FindNearestPositionClearOfHazards(bot, circles, TRAP_SWEEP, 2.0f,
                                                             static_cast<float>(M_PI) / 8.0f, prefer, accept);
    if (found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f)
    {
        bs.trap = false;
        bs.trapBlockedMs = now;
        return;
    }

    TrapInfo const* trap = NearestTrapInfo(st, from);
    bs.trap = true;
    bs.trapGuid = trap ? trap->guid : ObjectGuid::Empty;
    bs.trapSpot = found;
    bs.trapBlockedMs = 0;
}

// The trap whose dodge trigger a straight walk from `from` to `to` passes through, if any
TrapInfo const* TrapOnApproach(LkState const& st, Position const& from, Position const& to)
{
    for (TrapInfo const& trap : st.traps)
        if (trap.pos.GetExactDist2d(to) < TRAP_TRIGGER ||
            SegmentCrossesCircle(from, to, HazardCircle(trap.pos, TRAP_TRIGGER)))
            return &trap;

    return nullptr;
}

// Generic reach walks a melee bot straight at its pick, and a line through a trap's dodge trigger bounces
// it off the dodge: it walks to the clear spot in melee reach of the pick nearest it instead, else holds
void UpdateTrapReach(LkState const& st, LkBotState& bs, Player* bot, Unit* pick, uint32 now)
{
    if (bs.trap || !pick || bs.stand != StandKind::None || st.traps.empty() || bot->IsWithinMeleeRange(pick))
    {
        bs.reach = false;
        bs.reachValid = false;
        bs.reachBlockedMs = 0;
        return;
    }

    float const range = bot->GetMeleeRange(pick);
    // Short of the edge, so a bot parked within PARK of the spot still swings
    float const ring = std::max(range - PARK - 0.5f, 1.0f);
    Position const from = bot->GetPosition();
    Position const at = pick->GetPosition();
    TrapInfo const* fouling = TrapOnApproach(st, from, Toward(at, from, ring));
    if (!fouling)
    {
        bs.reach = false;
        bs.reachValid = false;
        bs.reachBlockedMs = 0;
        return;
    }

    bs.reach = true;
    bs.reachTrap = fouling->guid;
    bool const kept = bs.reachValid && bs.reachPick == pick->GetGUID() && at.GetExactDist2d(bs.reachBasis) <= RELATCH &&
                      NearestTrap(st, bs.reachSpot) >= TRAP_CLEAR + PARK;
    if (kept || (!bs.reachValid && bs.reachBlockedMs && getMSTimeDiff(bs.reachBlockedMs, now) < SWEEP_RETRY_MS))
        return;

    FloorContext const ctx = FloorContextOf(bot);
    bs.reachValid = false;
    float best = 0.0f;
    for (int k = 0; k < RELATCH_SWEEP_ANGLES; ++k)
    {
        float const angle = static_cast<float>(k) * 2.0f * static_cast<float>(M_PI) / RELATCH_SWEEP_ANGLES;
        Position const spot(at.GetPositionX() + ring * std::cos(angle), at.GetPositionY() + ring * std::sin(angle),
                            FLOOR_Z);
        if (NearestTrap(st, spot) < TRAP_CLEAR + PARK || TrapOnApproach(st, from, spot) ||
            !FloorAccepts(bot, spot.GetPositionX(), spot.GetPositionY(), ctx))
            continue;

        float const distance = from.GetExactDist2d(spot);
        if (!bs.reachValid || distance < best)
        {
            bs.reachValid = true;
            bs.reachSpot = spot;
            best = distance;
        }
    }

    bs.reachPick = pick->GetGUID();
    bs.reachBasis = at;
    bs.reachBlockedMs = bs.reachValid ? 0 : now;
}

// The pile add nearest the carrier whose far side from the add tank is clear to walk to
bool PlagueWalkSpot(LkState const& st, Player* bot, ObjectGuid& add, Position& basis, Position& spot)
{
    FloorContext const ctx = FloorContextOf(bot);
    Position const from = bot->GetPosition();
    Position const addTank = PolarSpot(ADD_TANK_SPOT);
    std::vector<AddInfo const*> adds;
    for (AddInfo const& info : st.adds)
        adds.push_back(&info);

    std::sort(adds.begin(), adds.end(), [&](AddInfo const* a, AddInfo const* b)
              { return bot->GetExactDist2d(a->pos) < bot->GetExactDist2d(b->pos); });

    for (AddInfo const* info : adds)
    {
        // Straight on from the add tank past the add, or toward C with the add on top of it
        Position const away = info->pos.GetExactDist2d(addTank) > 0.5f
                                  ? Toward(info->pos, addTank, -PLAGUE_SIDE)
                                  : Toward(info->pos, LK_CENTER, PLAGUE_SIDE);
        if (!FloorAccepts(bot, away.GetPositionX(), away.GetPositionY(), ctx) || NearestTrap(st, away) < TRAP_CLEAR ||
            !SegmentClearOfTraps(st, from, away))
            continue;

        add = info->guid;
        basis = info->pos;
        spot = Position(away.GetPositionX(), away.GetPositionY(), FLOOR_Z);
        return true;
    }

    return false;
}

void UpdatePlague(LkState const& st, LkBotState& bs, Player* bot, std::vector<Player*> const& members, uint32 now)
{
    // The add tanks stand in the pile already
    if (!HasPlague(bot) || IsLkTankPlayer(bot) || bot->GetGUID() == st.standIn)
    {
        bs.plague = PlagueMode::None;
        bs.plagueValid = false;
        bs.plagueAdd.Clear();
        bs.plagueJump.Clear();
        return;
    }

    if (!IsolateMode(st))
    {
        if (bs.plague != PlagueMode::Walk && bs.plague != PlagueMode::Delivered)
        {
            bs.plagueValid = false;
            bs.plagueBlockedMs = 0;
        }

        JumpPick const jump = NearestJumpUnit(st, bot, members);
        bs.plague = jump.found && jump.add ? PlagueMode::Delivered : PlagueMode::Walk;
        bs.plagueJump = jump.found && jump.add ? jump.guid : ObjectGuid::Empty;

        AddInfo const* add = nullptr;
        for (AddInfo const& info : st.adds)
            if (info.guid == bs.plagueAdd)
                add = &info;

        bool const moved = !add || add->pos.GetExactDist2d(bs.plagueBasis) > RELATCH;
        if (bs.plagueValid && !moved)
            return;

        if (!bs.plagueValid && bs.plagueBlockedMs && getMSTimeDiff(bs.plagueBlockedMs, now) < SWEEP_RETRY_MS)
            return;

        bs.plagueValid = PlagueWalkSpot(st, bot, bs.plagueAdd, bs.plagueBasis, bs.plagueSpot);
        bs.plagueBlockedMs = bs.plagueValid ? 0 : now;
        return;
    }

    std::vector<HazardCircle> circles;
    for (Player* member : members)
        if (member != bot)
            circles.emplace_back(member->GetPosition(), PLAGUE_ISOLATE + PARK);

    if (bs.plague != PlagueMode::Isolate)
    {
        bs.plague = PlagueMode::Isolate;
        bs.plagueValid = false;
        bs.plagueBlockedMs = 0;
        bs.plagueAdd.Clear();
        bs.plagueJump.Clear();
    }
    else if (bs.plagueValid &&
             std::none_of(circles.begin(), circles.end(), [&](HazardCircle const& circle)
                          { return circle.first.GetExactDist2d(bs.plagueSpot) < PLAGUE_JUMP + PARK; }))
        return;

    if (circles.empty())
    {
        bs.plagueValid = false;
        return;
    }

    if (bs.plagueBlockedMs && getMSTimeDiff(bs.plagueBlockedMs, now) < SWEEP_RETRY_MS)
        return;

    FloorContext const ctx = FloorContextOf(bot);
    Position const from = bot->GetPosition();
    bool const winter = IsWinter(st.phase);
    auto const accept = [&](float x, float y)
    {
        // Out of Remorseless Winter's 45 yd
        if (winter && LK_CENTER.GetExactDist2d(x, y) < RING_INNER)
            return false;

        return FloorAccepts(bot, x, y, ctx) && SegmentClearOfTraps(st, from, Position(x, y, FLOOR_Z));
    };

    Position const found = FindNearestPositionClearOfHazards(bot, circles, PLAGUE_SWEEP, 2.0f,
                                                             static_cast<float>(M_PI) / 8.0f, nullptr, accept);
    bs.plagueValid = !(found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f);
    bs.plagueBlockedMs = bs.plagueValid ? 0 : now;
    if (bs.plagueValid)
        bs.plagueSpot = found;
}

// The spirit tank's own spot, or a walk toward a spirit of its out of taunt reach
Position SpiritTankStand(LkState const& st, Player* bot, size_t index)
{
    Position const spot = SpiritSpot(index, st.ringStandable);
    if (!st.ringStandable)
        return spot;

    SpiritInfo const* far = nullptr;
    for (SpiritInfo const& spirit : st.spirits)
    {
        auto const itr = st.spiritTank.find(spirit.guid);
        if (itr == st.spiritTank.end() || itr->second != bot->GetGUID() || spirit.victim == bot->GetGUID() ||
            spirit.pos.GetExactDist2d(spot) <= PICKUP_FROM)
            continue;

        if (!far || spirit.pos.GetExactDist2d(spot) < far->pos.GetExactDist2d(spot))
            far = &spirit;
    }

    if (!far)
        return spot;

    Position pickup = Toward(far->pos, spot, PICKUP_REACH);
    float const r = LK_CENTER.GetExactDist2d(pickup);
    pickup = AtRadius(pickup, std::clamp(r, RING_INNER + 1.0f, RING_OUTER - 1.0f));
    return LkFloorAccepts(bot, pickup.GetPositionX(), pickup.GetPositionY()) ? pickup : spot;
}

// Melee on the inner side of what the spirit tanks hold, out of the winter
bool MeleeSpiritStand(LkState const& st, Position& out)
{
    Position held;
    if (SpiritInfo const* spirit = HeldSpirit(st))
        held = spirit->pos;
    else if (AddInfo const* add = HeldLeftover(st))
        held = add->pos;
    else
        return false;

    Position const inner = Toward(held, LK_CENTER, SPIRIT_INNER);
    out = AtRadius(inner, std::max(LK_CENTER.GetExactDist2d(inner), RING_INNER + 0.5f));
    return true;
}

void StandFor(LkState const& st, Player* bot, Creature* boss, StandKind& kind, Position& wanted, std::string& note)
{
    kind = StandKind::None;
    note = "none";
    bool const live = boss->IsInCombat();
    IccLichKingPhaseId const phase = live ? st.phase : IccLichKingPhaseId::One;
    if (phase == IccLichKingPhaseId::One)
    {
        if (bot->GetGUID() == st.holder)
        {
            kind = live ? StandKind::Holder : StandKind::Opening;
            wanted = PolarSpot(live ? HOLDER_SPOT : OPENING_SPOT);
            note = "holder";
        }
        else if (bot->GetGUID() == st.freeTank || bot->GetGUID() == st.standIn)
        {
            kind = StandKind::AddTank;
            wanted = PolarSpot(ADD_TANK_SPOT);
            note = "addtank";
        }
        else if (StandsOnLattice(bot))
        {
            auto const itr = st.rangedNode.find(bot->GetGUID());
            if (itr != st.rangedNode.end() && itr->second < Lattice().size())
            {
                kind = StandKind::Ranged;
                wanted = Lattice()[itr->second];
                note = "ranged " + std::to_string(itr->second + 1);
            }
        }

        if (!live && kind != StandKind::None)
            note = "opening";

        return;
    }

    if (!IsWinter(phase) || st.quake)
        return;

    int const index = SpiritTankIndex(st, bot->GetGUID());
    if (index >= 0)
    {
        kind = StandKind::Spirit;
        wanted = SpiritTankStand(st, bot, static_cast<size_t>(index));
        note = "spirit";
        return;
    }

    if (IsRaidDpsBot(bot) && PlayerbotAI::IsMelee(bot) && st.ringStandable && MeleeSpiritStand(st, wanted))
    {
        kind = StandKind::Spirit;
        note = "spirit";
        return;
    }

    auto const itr = st.winterSlots.find(bot->GetGUID());
    if (itr == st.winterSlots.end())
        return;

    kind = StandKind::Winter;
    wanted = WinterSlotSpot(itr->second, st.tenNormal, st.ringStandable);
    char const* cluster = itr->second.ranged ? "ranged " : itr->second.guard ? "guard " : "tank ";
    note = std::string("winter ") + cluster + std::to_string(itr->second.index + 1);
}

void UpdateStand(LkState const& st, LkBotState& bs, Player* bot, Creature* boss, uint32 now)
{
    StandKind kind = StandKind::None;
    Position wanted;
    std::string note = "none";
    if (boss->IsInCombat() || IsIntro(boss))
        StandFor(st, bot, boss, kind, wanted, note);

    if (kind != bs.stand || (kind != StandKind::None && bs.standWanted.GetExactDist2d(wanted) > RELATCH))
    {
        bs.stand = kind;
        bs.standWanted = wanted;
        bs.standSpot = wanted;
        bs.standHeld = false;
        bs.standSinceMs = now;
        bs.standResting = false;
        bs.standBlockedMs = 0;
    }

    bs.standNote = note;
    if (kind == StandKind::None)
        return;

    if (bs.standResting)
    {
        if (getMSTimeDiff(bs.standRestMs, now) < STAND_REST_MS)
            return;

        bs.standResting = false;
        bs.standSinceMs = now;
    }

    bool const parked = bs.walk == WalkKind::Stand && bs.walkParked;
    if (parked || bs.walk != WalkKind::Stand)
        bs.standSinceMs = now;
    else if (getMSTimeDiff(bs.standSinceMs, now) > STAND_CAP_MS)
    {
        bs.standResting = true;
        bs.standRestMs = now;
        return;
    }

    // A trap on the stand, or across the walk to it, latches it to the nearest clear spot
    Position const from = bot->GetPosition();
    auto const fouled = [&](Position const& spot)
    {
        return NearestTrap(st, spot) < TRAP_CLEAR ||
               (from.GetExactDist2d(spot) > UNPARK && !SegmentClearOfTraps(st, from, spot));
    };

    if (!fouled(bs.standWanted))
    {
        bs.standSpot = bs.standWanted;
        bs.standHeld = false;
        return;
    }

    if (!bs.standHeld && bs.standSpot.GetExactDist2d(bs.standWanted) > 0.1f && !fouled(bs.standSpot))
        return;

    if (bs.standBlockedMs && getMSTimeDiff(bs.standBlockedMs, now) < SWEEP_RETRY_MS)
        return;

    Position clear;
    if (SweepAround(st, bot, bs.standWanted, clear))
    {
        bs.standSpot = clear;
        bs.standHeld = false;
        bs.standBlockedMs = 0;
        return;
    }

    bs.standHeld = true;
    bs.standBlockedMs = now;
}

void SetWalk(LkBotState& bs, Player* bot, WalkKind kind, Position const& spot)
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

bool StandResting(LkBotState const& bs) { return bs.standResting; }

std::string PlagueNote(LkBotState const& bs)
{
    switch (bs.plague)
    {
        case PlagueMode::Walk:
            return Tag("walk", bs.plagueAdd);
        case PlagueMode::Delivered:
            return Tag("delivered", bs.plagueJump);
        case PlagueMode::Isolate:
            return "isolate";
        default:
            return "none";
    }
}

// The walk the bot owes now, highest first, and whether anything of this fight owns its feet
void UpdateWalks(LkState& st, LkBotState& bs, Player* bot, Creature* boss, Unit* pick, uint32 now)
{
    if (UnableToAct(bot))
    {
        bs = LkBotState();
        return;
    }

    std::vector<Player*> const members = LkMembersOnMap(bot);
    UpdateQuakeWalk(st, bs, bot);
    UpdateTrap(st, bs, bot, boss, now);
    UpdatePlague(st, bs, bot, members, now);
    UpdateStand(st, bs, bot, boss, now);
    UpdateTrapReach(st, bs, bot, pick, now);

    bool const plagueWalk = bs.plague != PlagueMode::None && bs.plagueValid;
    bool const standWalk = bs.stand != StandKind::None && !bs.standHeld && !StandResting(bs);
    if (bs.quake)
        SetWalk(bs, bot, WalkKind::Quake, bs.quakeSpot);
    else if (st.quake)
        SetWalk(bs, bot, WalkKind::None, Position());
    else if (bs.trap)
        SetWalk(bs, bot, WalkKind::Trap, bs.trapSpot);
    else if (plagueWalk)
        SetWalk(bs, bot, WalkKind::Plague, bs.plagueSpot);
    else if (bs.reach && bs.reachValid)
        SetWalk(bs, bot, WalkKind::Reach, bs.reachSpot);
    else if (standWalk)
        SetWalk(bs, bot, WalkKind::Stand, bs.standSpot);
    else
        SetWalk(bs, bot, WalkKind::None, Position());

    bs.owned = bs.walk != WalkKind::None || st.quake || bs.reach ||
               (bs.stand != StandKind::None && !StandResting(bs));
}

// A shaman bot's Cleansing Totem strips a disease every 3 s: while a carrier lives, the water totem that
// replaces it
char const* TotemSwapFor(LkState const& st, Player* bot)
{
    PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
    if (!ai || st.carriers.empty() || bot->getClass() != CLASS_SHAMAN || !IsLkDutyBot(bot))
        return nullptr;

    ObjectGuid const guid = bot->m_SummonSlot[SUMMON_SLOT_TOTEM_WATER];
    Creature* totem = guid.IsEmpty() ? nullptr : bot->GetMap()->GetCreature(guid);
    if (!totem || totem->GetUInt32Value(UNIT_CREATED_BY_SPELL) != SPELL_CLEANSING_TOTEM)
        return nullptr;

    for (char const* name : WATER_TOTEM_SWAPS)
        if (ai->CanCastSpell(name, bot))
            return name;

    return nullptr;
}

void NoteBot(LkState const& st, LkBotState const& bs, Player* bot)
{
    if (!RaidObs::Active())
        return;

    NoteDerived(bot, "lk.stand", st.quake ? "quake" : bs.stand != StandKind::None ? bs.standNote : "none");
    if (st.heroic)
    {
        std::string trap = "none";
        if (bs.trap)
            trap = Tag("dodge", bs.trapGuid);
        else if (bs.reach)
            trap = Tag(bs.reachValid ? "reach" : "hold", bs.reachTrap);

        NoteDerived(bot, "lk.trap", trap);
    }

    NoteDerived(bot, "lk.plague", PlagueNote(bs));

    uint8 const cls = bot->getClass();
    // A DK or paladin damage dealer can stand in for the add tank
    if (IsLkTankPlayer(bot) || cls == CLASS_DEATH_KNIGHT || cls == CLASS_PALADIN)
    {
        auto const itr = st.taunts.find(bot->GetGUID());
        NoteDerived(bot, "lk.taunt",
                    itr != st.taunts.end() ? Tag(itr->second.rule, itr->second.unit) : std::string("none"));
    }

    if (cls == CLASS_PRIEST || cls == CLASS_PALADIN || cls == CLASS_SHAMAN)
    {
        auto const itr = st.cures.find(bot->GetGUID());
        char const* rule = nullptr;
        if (itr != st.cures.end())
            switch (itr->second.rule)
            {
                case CureRule::Delivered:
                    rule = "delivered";
                    break;
                case CureRule::AddTank:
                    rule = "addtank";
                    break;
                case CureRule::Isolated:
                    rule = "isolated";
                    break;
                case CureRule::Emergency:
                    rule = "emergency";
                    break;
                default:
                    break;
            }

        std::string cure = rule ? Tag(rule, itr->second.carrier) : std::string("none");
        if (!rule && TotemSwapFor(st, bot))
            cure = "totem";

        NoteDerived(bot, "lk.cure", cure);
    }

    if (cls == CLASS_PRIEST || cls == CLASS_PALADIN)
    {
        auto const itr = st.silence.find(bot->GetGUID());
        NoteDerived(bot, "lk.silence",
                    itr != st.silence.end() ? std::to_string(itr->second.GetCounter()) : std::string("none"));
    }

    if (cls == CLASS_HUNTER)
    {
        auto const itr = st.tranq.find(bot->GetGUID());
        NoteDerived(bot, "lk.enrage",
                    itr != st.tranq.end() ? std::to_string(itr->second.GetCounter()) : std::string("none"));
    }
}

uint32 HumanWindowHitMs(LkState const& st, Player* member, Creature* boss, bool& window)
{
    window = false;
    if (Aura const* reaper = member->GetAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LK_SOUL_REAPER, boss)))
    {
        window = true;
        return reaper->GetDuration() > 0 ? static_cast<uint32>(reaper->GetDuration()) : 0;
    }

    window = member->GetGUID() == st.holder && boss->GetVictim() == member &&
             boss->GetAuraCount(SPELL_LK_PLAGUE_SIPHON) >= SIPHON_DEFENSIVE_STACKS;
    return 0;
}

// A human tank has no trigger, so it gets told: on the holder's siphon ramp, on Soul Reaper and when low
void CallHumanTank(LkState& st, Player* sender, Creature* boss, uint32 now)
{
    if (st.humanCallRan && getMSTimeDiff(st.humanCallMs, now) < HUMAN_CALL_CHECK_MS)
        return;

    st.humanCallRan = true;
    st.humanCallMs = now;
    if (!boss->IsInCombat())
        return;

    for (Player* member : LkMembersOnMap(sender))
    {
        if (IsLkDutyBot(member) || !IsLkTankPlayer(member))
            continue;

        bool window = false;
        uint32 const hitInMs = HumanWindowHitMs(st, member, boss, window);
        bool const low = member->GetHealthPct() < TANK_LOW_PCT;
        if (!low && !window)
            continue;

        bool const reaper = member->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LK_SOUL_REAPER, boss));
        if (uint32 const spellId = ReadyTankDefensiveSpell(member, !reaper, 0, hitInMs))
            CallHumanTankDefensive(sender, member, spellId, hitInMs,
                                   low ? "you are low" : hitInMs ? "Soul Reaper lands" : "Plague Siphon is high",
                                   "lk.humancall");
    }
}

void FloorSnap(PlayerbotAI* botAI, Player* bot)
{
    if (!bot->IsAlive() || bot->GetVehicle() || bot->IsBeingTeleported())
        return;

    float const z = bot->GetPositionZ();
    float floorZ = FLOOR_Z;
    bool sunk = bot->GetExactDist2d(LK_CENTER) <= FLOOR_SNAP_RANGE && z < FLOOR_SNAP_Z;
    if (!sunk && bot->GetExactDist2d(LK_FROSTMOURNE) <= FM_FLOOR_SNAP_RANGE && z < FM_FLOOR_SNAP_Z)
    {
        sunk = true;
        floorZ = FM_FLOOR_Z;
    }

    if (!sunk || !IccRaidCheat(botAI, "lk.floorsnap"))
        return;

    bot->NearTeleportTo(bot->GetPositionX(), bot->GetPositionY(), floorZ, bot->GetOrientation());
}

bool IsSkullUnit(Unit* unit)
{
    if (!unit)
        return false;

    switch (unit->GetEntry())
    {
        case NPC_THE_LICH_KING:
        case NPC_LK_DRUDGE_GHOUL:
        case NPC_LK_SHAMBLING_HORROR:
        case NPC_LK_RAGING_SPIRIT:
        case NPC_LK_ICE_SPHERE:
        case NPC_LK_VALKYR:
        case NPC_LK_VILE_SPIRIT:
        case NPC_LK_WICKED_SPIRIT:
        case NPC_LK_SPIRIT_WARDEN:
            return true;
        default:
            return false;
    }
}

template <typename Info>
Info const* Lowest(std::vector<Info> const& list, std::function<bool(Info const&)> const& wanted)
{
    Info const* best = nullptr;
    for (Info const& info : list)
        if (wanted(info) && (!best || info.healthPct < best->healthPct))
            best = &info;

    return best;
}

Unit* WinterPick(Player* bot, LkState const& st, char const*& rule)
{
    rule = "winter";
    bool const ranged = IsRaidDpsBot(bot) && PlayerbotAI::IsRanged(bot);
    auto const inReach = [&](Position const& pos) { return !ranged || bot->GetExactDist2d(pos) <= RANGED_REACH; };

    if (SpiritTankIndex(st, bot->GetGUID()) >= 0)
    {
        ObjectGuid const own = bot->GetGUID();
        auto const mine = [&](SpiritInfo const& spirit)
        {
            auto const itr = st.spiritTank.find(spirit.guid);
            return spirit.inCombat && itr != st.spiritTank.end() && itr->second == own;
        };

        SpiritInfo const* onMe =
            Lowest<SpiritInfo>(st.spirits, [&](SpiritInfo const& s) { return mine(s) && s.victim == own; });
        SpiritInfo const* any = onMe ? onMe : Lowest<SpiritInfo>(st.spirits, mine);
        if (any)
            if (Unit* unit = FoeOf(bot, any->guid))
            {
                rule = "spirit";
                return unit;
            }

        if (own == st.freeTank)
            if (AddInfo const* add = Lowest<AddInfo>(st.adds, [](AddInfo const& a) { return a.inCombat; }))
                if (Unit* unit = FoeOf(bot, add->guid))
                {
                    rule = "leftover";
                    return unit;
                }

        return nullptr;
    }

    if (!IsRaidDpsBot(bot))
        return nullptr;

    if (ranged)
    {
        SphereInfo const* nearest = nullptr;
        for (SphereInfo const& sphere : st.spheres)
            if (sphere.inCombat && inReach(sphere.pos) && (!nearest || sphere.toVictim < nearest->toVictim))
                nearest = &sphere;

        if (nearest)
            if (Unit* unit = FoeOf(bot, nearest->guid))
            {
                rule = "sphere";
                return unit;
            }
    }
    else
    {
        if (SpiritInfo const* spirit = HeldSpirit(st))
            if (Unit* unit = FoeOf(bot, spirit->guid))
            {
                rule = "spirit";
                return unit;
            }

        if (AddInfo const* add = HeldLeftover(st))
            if (Unit* unit = FoeOf(bot, add->guid))
            {
                rule = "leftover";
                return unit;
            }
    }

    if (SpiritInfo const* spirit =
            Lowest<SpiritInfo>(st.spirits, [&](SpiritInfo const& s) { return s.inCombat && inReach(s.pos); }))
        if (Unit* unit = FoeOf(bot, spirit->guid))
        {
            rule = "spirit";
            return unit;
        }

    if (AddInfo const* add = Lowest<AddInfo>(st.adds, [&](AddInfo const& a) { return a.inCombat && inReach(a.pos); }))
        if (Unit* unit = FoeOf(bot, add->guid))
        {
            rule = "leftover";
            return unit;
        }

    return nullptr;
}

Unit* PhaseOnePick(Player* bot, LkState const& st, Creature* boss, char const*& rule)
{
    rule = "boss";
    if (bot->GetGUID() == st.freeTank)
    {
        ObjectGuid const own = bot->GetGUID();
        AddInfo const* loose = nullptr;
        for (AddInfo const& add : st.adds)
            if (add.emerged && add.inCombat && add.victim != own &&
                (!loose || bot->GetExactDist2d(add.pos) < bot->GetExactDist2d(loose->pos)))
                loose = &add;

        AddInfo const* pick =
            loose ? loose : Lowest<AddInfo>(st.adds, [&](AddInfo const& a) { return a.inCombat && a.victim == own; });
        if (pick)
            if (Unit* unit = FoeOf(bot, pick->guid))
            {
                rule = loose ? "loose" : "add";
                return unit;
            }
    }
    else if (bot->GetGUID() == st.standIn)
    {
        // Its swings on its own healthiest ghoul keep the threat and the plague's fodder alive longest
        ObjectGuid const own = bot->GetGUID();
        AddInfo const* loose = nullptr;
        AddInfo const* held = nullptr;
        for (AddInfo const& add : st.adds)
        {
            if (add.entry != NPC_LK_DRUDGE_GHOUL || !add.emerged || !add.inCombat)
                continue;

            if (add.victim != own)
            {
                if (!loose || bot->GetExactDist2d(add.pos) < bot->GetExactDist2d(loose->pos))
                    loose = &add;
            }
            else if (!held || add.healthPct > held->healthPct)
                held = &add;
        }

        if (AddInfo const* pick = loose ? loose : held)
            if (Unit* unit = FoeOf(bot, pick->guid))
            {
                rule = loose ? "loose" : "add";
                return unit;
            }
    }
    else if (bot->GetGUID() != st.holder && st.heroic && IsRaidDpsBot(bot) && PlayerbotAI::IsMelee(bot))
    {
        if (AddInfo const* add = Lowest<AddInfo>(st.adds, [](AddInfo const& a) { return a.frenzied && a.inCombat; }))
            if (Unit* unit = FoeOf(bot, add->guid))
            {
                rule = "frenzy";
                return unit;
            }
    }

    return boss;
}

Unit* LatePhasePick(Player* bot, LkState const& st, Creature* boss, char const*& rule)
{
    rule = "boss";
    ObjectGuid const own = bot->GetGUID();
    if (own == st.holder)
        return boss;

    if (own == st.freeTank)
    {
        SpiritInfo const* loose = nullptr;
        for (SpiritInfo const& spirit : st.spirits)
            if (spirit.inCombat && !spirit.passive && spirit.victim != own &&
                (!loose || bot->GetExactDist2d(spirit.pos) < bot->GetExactDist2d(loose->pos)))
                loose = &spirit;

        SpiritInfo const* pick =
            loose ? loose
                  : Lowest<SpiritInfo>(st.spirits, [&](SpiritInfo const& s) { return s.inCombat && s.victim == own; });
        if (pick)
            if (Unit* unit = FoeOf(bot, pick->guid))
            {
                rule = "spirit";
                return unit;
            }

        return boss;
    }

    if (!IsRaidDpsBot(bot))
        return boss;

    // A sphere left over from the winter still knocks its target off the platform
    if (PlayerbotAI::IsRanged(bot))
    {
        SphereInfo const* nearest = nullptr;
        for (SphereInfo const& sphere : st.spheres)
            if (sphere.inCombat && (!nearest || sphere.toVictim < nearest->toVictim))
                nearest = &sphere;

        if (nearest)
            if (Unit* unit = FoeOf(bot, nearest->guid))
            {
                rule = "sphere";
                return unit;
            }
    }

    if (SpiritInfo const* spirit = Lowest<SpiritInfo>(st.spirits, [](SpiritInfo const& s) { return s.inCombat; }))
        if (Unit* unit = FoeOf(bot, spirit->guid))
        {
            rule = "spirit";
            return unit;
        }

    return boss;
}

Unit* ResolveTarget(PlayerbotAI* botAI, LkState const& st, Creature* boss)
{
    Player* bot = botAI->GetBot();
    if (!IsLkTankPlayer(bot) && !IsRaidDpsBot(bot))
        return nullptr;

    bool const braked = st.brake.count(bot->GetGUID()) != 0;
    if (Unit* skull = GetRaidHumanSkull(botAI, &IsSkullUnit))
        if (!braked || skull != boss)
        {
            NoteDerived(bot, "lk.target", Tag("skull", skull));
            return skull;
        }

    // Combat is disallowed, so he can't be pulled, and he only takes damage while levitating
    if (st.phase == IccLichKingPhaseId::Outro)
    {
        bool const open = boss->IsLevitating() && bot->IsValidAttackTarget(boss);
        NoteDerived(bot, "lk.target", open ? Tag("outro", boss) : std::string("outro"));
        return open ? boss : nullptr;
    }

    char const* lateRule = nullptr;
    Unit* late = GetLkLateTarget(botAI, lateRule);
    if (late || IccLichKingInFrostmourne(bot))
    {
        NoteDerived(bot, "lk.target", late ? Tag(lateRule ? lateRule : "late", late) : std::string("frostmourne"));
        return late;
    }

    if (bot->GetVehicle() || HarvestStunned(bot))
    {
        NoteDerived(bot, "lk.target", "held");
        return nullptr;
    }

    char const* rule = "none";
    Unit* pick = nullptr;
    if (IsWinter(st.phase))
        pick = WinterPick(bot, st, rule);
    else if (st.phase == IccLichKingPhaseId::One)
        pick = PhaseOnePick(bot, st, boss, rule);
    else if (st.phase == IccLichKingPhaseId::Two || st.phase == IccLichKingPhaseId::Three)
        pick = LatePhasePick(bot, st, boss, rule);

    if (pick == boss && (!boss->IsInCombat() || !bot->IsValidAttackTarget(boss)))
        pick = nullptr;

    if (pick == boss && braked)
    {
        NoteDerived(bot, "lk.target", "brake");
        return nullptr;
    }

    NoteDerived(bot, "lk.target", pick ? Tag(rule, pick) : std::string(rule));
    return pick;
}
}  // namespace

bool IccLichKingInFrostmourne(Unit const* unit)
{
    return unit->GetExactDistSq(LK_FROSTMOURNE.GetPositionX(), LK_FROSTMOURNE.GetPositionY(),
                                LK_FROSTMOURNE.GetPositionZ()) < 40.0f * 40.0f;
}

IccLichKingPhaseId IccLichKingPhase(Player* bot, Unit* boss)
{
    LkState const* st = bot->GetInstanceId() ? lkStates.Find(bot->GetInstanceId()) : nullptr;
    IccLichKingPhaseId const phase =
        PhaseOf(boss, Harvested(bot), st && st->reachedThree, st ? st->winters : WINTERS_UNKNOWN);
    if (RaidObs::Active() && boss && boss->GetEntry() == NPC_THE_LICH_KING)
    {
        static char const* const NAMES[] = {"one", "winter1", "two", "winter2", "three", "frostmourne", "outro"};
        RaidObs::NoteDerived(bot, "lk.phase", NAMES[static_cast<uint8>(phase)]);
    }

    return phase;
}

namespace
{
// 1 or 2 by the winter's row, 0 when the boss casts neither
uint8 IccRemorselessWinterCastOf(Unit* boss)
{
    if (!boss || !boss->HasUnitState(UNIT_STATE_CASTING))
        return 0;

    for (uint32 const spellId : ICC_LK_FIRST_WINTER)
        if (boss->FindCurrentSpellBySpellId(spellId))
            return 1;

    for (uint32 const spellId : ICC_LK_SECOND_WINTER)
        if (boss->FindCurrentSpellBySpellId(spellId))
            return 2;

    return 0;
}
}  // namespace

bool IccBossCastingRemorselessWinter(Unit* boss) { return IccRemorselessWinterCastOf(boss) != 0; }

uint8 IccRemorselessWinterOf(Unit* boss)
{
    if (!boss)
        return 0;

    for (uint32 const spellId : ICC_LK_FIRST_WINTER)
        if (boss->HasAura(spellId))
            return 1;

    for (uint32 const spellId : ICC_LK_SECOND_WINTER)
        if (boss->HasAura(spellId))
            return 2;

    return IccRemorselessWinterCastOf(boss);
}

bool IccBossHasRemorselessWinter(Unit* boss) { return IccRemorselessWinterOf(boss) != 0; }

Creature* GetLkBoss(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsInWorld() || bot->GetMapId() != ICC_MAP_ID || !InRoom(bot))
        return nullptr;

    Creature* boss = BossUnit(bot);
    return boss && boss->IsAlive() ? boss : nullptr;
}

bool IsLkLive(PlayerbotAI* botAI) { return IsLive(GetLkBoss(botAI)); }

bool IsLkRingStandable(Player* bot) { return bot && RingStandable(bot); }

bool LkFloorAccepts(Player* bot, float x, float y) { return bot && FloorAccepts(bot, x, y, FloorContextOf(bot)); }

LkTankRole GetLkTankRole(PlayerbotAI* botAI, Player* player)
{
    LkState* st = player ? StateOf(botAI) : nullptr;
    if (!st)
        return LkTankRole::None;

    auto const itr = st->tank.find(player->GetGUID());
    return itr != st->tank.end() ? static_cast<LkTankRole>(itr->second) : LkTankRole::None;
}

Player* GetLkHolder(PlayerbotAI* botAI)
{
    LkState* st = StateOf(botAI);
    return st ? PlayerOf(botAI->GetBot(), st->holder) : nullptr;
}

Player* GetLkFreeTank(PlayerbotAI* botAI)
{
    LkState* st = StateOf(botAI);
    return st ? PlayerOf(botAI->GetBot(), st->freeTank) : nullptr;
}

std::vector<Player*> LkMembersOnMap(Player* bot)
{
    std::vector<Player*> members;
    Group* group = bot->GetGroup();
    if (!group)
    {
        if (bot->IsAlive())
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

bool IsLkDutyBot(Player* player) { return player && GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

bool IsLkTankPlayer(Player* player)
{
    return player && (PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true));
}

bool IsLkHealer(Player* player) { return player && PlayerbotAI::IsHeal(player) && !IsLkTankPlayer(player); }

uint32 LkReadyStun(Player* member, Unit* target)
{
    PlayerbotAI* ai = member && target ? GET_PLAYERBOT_AI(member) : nullptr;
    if (!ai || !member->IsAlive() || !target->IsAlive())
        return 0;

    for (ControlSpell const& entry : STUN_SPELLS)
        if (uint32 const spellId = ReadyControl(ai, member, target, entry, true))
            return spellId;

    if (member->getClass() == CLASS_HUNTER)
        return ReadyIntimidation(ai, member, target);

    if (member->getClass() == CLASS_DEATH_KNIGHT)
        return ReadyGnaw(member, target);

    return 0;
}

uint32 LkReadySlow(Player* member, Unit* target)
{
    PlayerbotAI* ai = member && target ? GET_PLAYERBOT_AI(member) : nullptr;
    if (!ai || !member->IsAlive() || !target->IsAlive())
        return 0;

    for (ControlSpell const& entry : SLOW_SPELLS)
        if (uint32 const spellId = ReadyControl(ai, member, target, entry, false))
            return spellId;

    return 0;
}

bool LkCastControl(PlayerbotAI* botAI, Unit* target, uint32 spellId)
{
    Player* bot = botAI->GetBot();
    if (!target || !spellId)
        return false;

    if (spellId == SPELL_GNAW)
    {
        Guardian* ghoul = bot->GetGuardianPet();
        return ghoul && ghoul->CastSpell(target, SPELL_GNAW, false) == SPELL_CAST_OK;
    }

    // Intimidation buffs the hunter, and its pet's next hit on its own target stuns
    if (spellId == SPELL_INTIMIDATION)
    {
        CommandPetAttack(botAI, target);
        return botAI->CastSpell(spellId, bot);
    }

    return botAI->CastSpell(spellId, target);
}

Unit* GetLkTarget(PlayerbotAI* botAI)
{
    Creature* boss = nullptr;
    LkState* st = LiveStateOf(botAI, &boss);
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

bool IsLkTargetHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if ((!IsLkTankPlayer(bot) && !IsRaidDpsBot(bot)) || !IsLkLive(botAI))
        return false;

    if (bot->GetVehicle() || HarvestStunned(bot) || IsLkLateTargetHeld(botAI))
        return true;

    return IsLkBraked(botAI) && !GetLkTarget(botAI);
}

bool IsLkMovementOwned(PlayerbotAI* botAI)
{
    LkState* st = StateOf(botAI);
    LkBotState const* bs = st ? BotOf(*st, botAI->GetBot()) : nullptr;
    return bs && bs->owned;
}

bool IsLkTankDefensiveHeld(PlayerbotAI* botAI)
{
    return IsLkLive(botAI) && botAI->GetBot()->GetHealthPct() >= TANK_LAST_RESORT_PCT;
}

namespace
{
bool WalkOf(PlayerbotAI* botAI, WalkKind kind, Position& spot)
{
    LkState* st = StateOf(botAI);
    LkBotState const* bs = st ? BotOf(*st, botAI->GetBot()) : nullptr;
    if (!bs || bs->walk != kind || bs->walkParked)
        return false;

    spot = bs->walkSpot;
    return true;
}
}  // namespace

bool GetLkQuakeWalk(PlayerbotAI* botAI, Position& spot) { return WalkOf(botAI, WalkKind::Quake, spot); }
bool GetLkTrapWalk(PlayerbotAI* botAI, Position& spot) { return WalkOf(botAI, WalkKind::Trap, spot); }
bool GetLkPlagueWalk(PlayerbotAI* botAI, Position& spot) { return WalkOf(botAI, WalkKind::Plague, spot); }
bool GetLkStandWalk(PlayerbotAI* botAI, Position& spot)
{
    return WalkOf(botAI, WalkKind::Stand, spot) || WalkOf(botAI, WalkKind::Reach, spot);
}

bool IsLkStandUrgent(PlayerbotAI* botAI)
{
    LkState* st = StateOf(botAI);
    LkBotState const* bs = st ? BotOf(*st, botAI->GetBot()) : nullptr;
    return bs && (bs->stand == StandKind::Winter || bs->stand == StandKind::Spirit);
}

uint32 GetLkTankDefensive(PlayerbotAI* botAI, char const* noteKind)
{
    Creature* boss = nullptr;
    LkState* st = LiveStateOf(botAI, &boss);
    Player* bot = botAI->GetBot();
    if (!st || !boss->IsInCombat() || !IsLkDutyBot(bot) || !IsLkTankPlayer(bot) || UnableToAct(bot))
        return 0;

    bool const low = bot->GetHealthPct() < TANK_LOW_PCT;
    bool const reaper = bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LK_SOUL_REAPER, boss));
    bool const siphon = st->holder == bot->GetGUID() && boss->GetVictim() == bot &&
                        boss->GetAuraCount(SPELL_LK_PLAGUE_SIPHON) >= SIPHON_DEFENSIVE_STACKS;
    bool enrage = false;
    if (st->freeTank == bot->GetGUID())
        for (AddInfo const& add : st->adds)
        {
            bool answered = false;
            for (auto const& entry : st->tranq)
                answered = answered || entry.second == add.guid;

            enrage = enrage || (add.enraged && add.victim == bot->GetGUID() && !answered);
        }

    if (!low && !reaper && !siphon && !enrage)
    {
        // Change-latched, so the way out of a window is written too
        if (noteKind)
            NoteDerived(bot, noteKind, "idle");

        return 0;
    }

    // Soul Reaper's second hit is shadow, so Anti-Magic Shell counts there. Plague Siphon raises his
    // physical damage, and an Enrage or a low tank means swings.
    return NextTankDefensive(botAI, bot, noteKind, !reaper);
}

Unit* GetLkTauntTarget(PlayerbotAI* botAI, uint32& aoeSpell)
{
    aoeSpell = 0;
    Creature* boss = nullptr;
    LkState* st = LiveStateOf(botAI, &boss);
    Player* bot = botAI->GetBot();
    if (!st || !IsLkDutyBot(bot) || (!IsLkTankPlayer(bot) && st->standIn != bot->GetGUID()) || UnableToAct(bot))
        return nullptr;

    auto const itr = st->taunts.find(bot->GetGUID());
    if (itr == st->taunts.end())
        return nullptr;

    Unit* unit = itr->second.unit == boss->GetGUID() ? boss : ObjectAccessor::GetCreature(*bot, itr->second.unit);
    if (!unit || !unit->IsAlive() || !bot->IsValidAttackTarget(unit))
        return nullptr;

    if (itr->second.aoe && bot->GetExactDist2d(unit) <= AOE_TAUNT_RADIUS)
        if (uint32 const spellId = AoeTauntSpell(bot))
        {
            aoeSpell = spellId;
            return unit;
        }

    if (!ClassTauntReady(botAI) || bot->GetExactDist(unit) > TAUNT_RANGE || !bot->IsWithinLOSInMap(unit) ||
        unit->GetDiminishing(DIMINISHING_TAUNT) >= DIMINISHING_LEVEL_TAUNT_IMMUNE)
        return nullptr;

    return unit;
}

Unit* GetLkCureTarget(PlayerbotAI* botAI, uint32& spellId)
{
    spellId = 0;
    LkState* st = LiveStateOf(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsLkDutyBot(bot))
        return nullptr;

    auto const itr = st->cures.find(bot->GetGUID());
    if (itr == st->cures.end() || itr->second.rule == CureRule::None)
        return nullptr;

    Player* carrier = PlayerOf(bot, itr->second.carrier);
    if (!carrier || !HasPlague(carrier))
        return nullptr;

    spellId = CureSpellFor(bot, carrier);
    return spellId ? carrier : nullptr;
}

Unit* GetLkTranquilizeTarget(PlayerbotAI* botAI)
{
    LkState* st = LiveStateOf(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsLkDutyBot(bot))
        return nullptr;

    auto const itr = st->tranq.find(bot->GetGUID());
    Unit* add = itr != st->tranq.end() ? FoeOf(bot, itr->second) : nullptr;
    return add && add->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LK_SHAMBLING_ENRAGE, add)) &&
                   TranquilizeFor(bot, add)
               ? add
               : nullptr;
}

Unit* GetLkStunTarget(PlayerbotAI* botAI, uint32& spellId)
{
    spellId = 0;
    LkState* st = LiveStateOf(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsLkDutyBot(bot))
        return nullptr;

    auto const itr = st->stun.find(bot->GetGUID());
    Unit* add = itr != st->stun.end() ? FoeOf(bot, itr->second) : nullptr;
    if (!add)
        return nullptr;

    spellId = LkReadyStun(bot, add);
    return spellId ? add : nullptr;
}

Unit* GetLkSilenceDispelTarget(PlayerbotAI* botAI, uint32& spellId)
{
    spellId = 0;
    LkState* st = LiveStateOf(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsLkDutyBot(bot))
        return nullptr;

    auto const itr = st->silence.find(bot->GetGUID());
    Player* member = itr != st->silence.end() ? PlayerOf(bot, itr->second) : nullptr;
    if (!member || !HasSoulShriek(member))
        return nullptr;

    spellId = SilenceDispelFor(bot, member);
    return spellId ? member : nullptr;
}

bool IsLkBraked(PlayerbotAI* botAI)
{
    LkState* st = LiveStateOf(botAI);
    return st && st->brake.count(botAI->GetBot()->GetGUID());
}

bool IsLkCureHeld(PlayerbotAI* botAI)
{
    LkState* st = LiveStateOf(botAI);
    return st && !st->carriers.empty();
}

char const* GetLkTotemSwap(PlayerbotAI* botAI)
{
    LkState* st = LiveStateOf(botAI);
    return st ? TotemSwapFor(*st, botAI->GetBot()) : nullptr;
}

bool IsLkPlagueCarrier(Unit* unit) { return unit && HasPlague(unit); }

void GetLkSplashAdds(PlayerbotAI* botAI, std::vector<Position>& out)
{
    out.clear();
    LkState* st = LiveStateOf(botAI);
    if (!st || st->phase != IccLichKingPhaseId::One)
        return;

    for (AddInfo const& add : st->adds)
        out.push_back(add.pos);
}

void TickLkEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || !bot->IsInWorld() || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId() || !InRoom(bot))
        return;

    // A wipe, a kill or an intro that evades ends the pull: the next one rebuilds every duty, and no
    // opening stand outlives its intro
    Creature* boss = BossUnit(bot);
    LkState* found = lkStates.Find(bot->GetInstanceId());
    bool const live = boss && boss->IsAlive() && IsLive(boss);
    if (!boss || !boss->IsAlive() || (!live && (!IsIntro(boss) || (found && found->pullMs))))
    {
        if (found)
            lkStates.Reset(bot->GetInstanceId());
        return;
    }

    // A saved rti of cross or star keeps a bot off the skull
    if (lkRtiChecked.For(bot->GetInstanceId()).insert(bot->GetGUID()).second)
    {
        Value<std::string>* rti = botAI->GetAiObjectContext()->GetValue<std::string>("rti");
        if (rti->Get() == "cross" || rti->Get() == "star")
            rti->Set("skull");
    }

    LkState& st = lkStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (live && !st.pullMs)
    {
        st.pullMs = now;
        // Bots that reach the room after the pull find his thresholds already behind him
        if (!boss->HealthAbovePct(70))
            st.winters |= WINTER_FIRST;
        if (!boss->HealthAbovePct(40))
            st.winters |= WINTER_SECOND;
    }

    Scan(st, boss, now);
    RefreshDuties(st, bot, boss, now);
    if (live)
        IccLichKingPhase(bot, boss);

    if (!IsLkDutyBot(bot))
        return;

    CallHumanTank(st, bot, boss, now);
    LkBotState& bs = st.bots[bot->GetGUID()];
    UpdateBrake(st, bot, boss);
    // Melee damage dealers hold no stand in phase one, so generic reach is the walk that meets a trap
    Unit* pick = nullptr;
    if (live && st.phase == IccLichKingPhaseId::One && !st.traps.empty() && IsRaidDpsBot(bot) &&
        PlayerbotAI::IsMelee(bot))
        pick = GetLkTarget(botAI);

    UpdateWalks(st, bs, bot, boss, pick, now);
    NoteBot(st, bs, bot);

    if (live)
        FloorSnap(botAI, bot);
}

RaidBurstWindow IccLichKingBurstWindow(PlayerbotAI* botAI)
{
    RaidBurstWindow window;
    Creature* boss = nullptr;
    LkState const* st = StateOf(botAI, &boss);
    window.allowLust = boss && boss->IsInCombat() &&
                       PhaseOf(boss, false, false, st ? st->winters : WINTERS_UNKNOWN) == IccLichKingPhaseId::One;
    return window;
}

std::vector<uint32> const& GetLkOpenerBosses(PlayerbotAI* /*botAI*/)
{
    static std::vector<uint32> const bosses = {NPC_THE_LICH_KING};
    return bosses;
}

uint8 IccLichKingResistSchools(PlayerbotAI* /*botAI*/, uint8 /*previous*/)
{
    return static_cast<uint8>(RAID_RESIST_SHADOW | RAID_RESIST_FROST);
}
