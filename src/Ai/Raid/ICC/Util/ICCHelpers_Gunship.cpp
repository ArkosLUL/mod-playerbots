/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Gunship.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

#include "Group.h"
#include "ICCDefinitions.h"
#include "ICCEncounterGate.h"
#include "InstanceScript.h"
#include "Playerbots.h"
#include "RaidDifficulty.h"
#include "RaidDirectTargeting.h"
#include "RaidHumanTankCall.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "Transport.h"
#include "Vehicle.h"

namespace
{
constexpr uint32 GSB_UNIT_SCAN_MS = 200;
constexpr uint32 GSB_ROSTER_MS = 1000;
// Boarders go without a tank on the captain this long into a freeze: a mage left alive loses the fight
constexpr uint32 GSB_NO_TANK_GO_MS = 15000;
// The board tank leaves after everyone else, or this long after the party was called home
constexpr uint32 GSB_TANK_LAST_MS = 10000;
// Cannon Blast and Incinerating Blast reach 150 yd
constexpr float GSB_CANNON_RANGE = 150.0f;
constexpr float GSB_HAZARD_SCAN = 80.0f;
constexpr uint32 GSB_HUMAN_CALL_MS = 1000;

GsbSide const GSB_ALLIANCE{false,
                           NPC_CANNONA,
                           NPC_HIGH_OVERLORD_SAURFANG,
                           NPC_KOR_KRON_BATTLE_MAGE,
                           NPC_KOR_KRON_AXETHROWER,
                           NPC_KOR_KRON_ROCKETEER,
                           NPC_GSB_KOR_KRON_REAVER,
                           NPC_GSB_KOR_KRON_SERGEANT,
                           2042.0f,
                           2025.0f,
                           466.57f,
                           Position(-389.26788f, 2045.9136f, 467.56168f),
                           Position(-383.01166f, 2022.8046f, 467.38193f),
                           Position(-370.04645f, 1993.3536f, 466.65656f),
                           Position(-17.55738f, -0.090421f, 21.18366f)};

GsbSide const GSB_HORDE{true,
                        NPC_CANNONH,
                        NPC_MURADIN_BRONZEBEARD,
                        NPC_SKYBREAKER_SORCERER,
                        NPC_SKYBREAKER_RIFLEMAN,
                        NPC_SKYBREAKER_MORTAR_SOLDIER,
                        NPC_GSB_SKYBREAKER_MARINE,
                        NPC_GSB_SKYBREAKER_SERGEANT,
                        2431.0f,
                        2445.0f,
                        470.79f,
                        Position(-444.74533f, 2420.1208f, 470.78748f),
                        Position(-449.17645f, 2449.2705f, 470.9257f),
                        Position(-449.5343f, 2477.2024f, 471.31906f),
                        Position(7.461699f, 0.158853f, 35.72989f)};

// Transport-local, 3 yd inside the core's outermost crew slots (cannons, gunners, mortars, captain)
struct DeckBox
{
    float minX;
    float maxX;
    float minY;
    float maxY;
};
constexpr DeckBox SKYBREAKER_BOX{-31.0f, 10.0f, -21.0f, 15.0f};
constexpr DeckBox ORGRIMS_HAMMER_BOX{-13.0f, 30.0f, -21.0f, 27.0f};

// Edge slots, transport-local: two rows 4 and 8.5 yd inboard of the cannon line, 5 yd apart, past the
// explosion's 4 yd. Orgrim's Hammer's deck rises along x, as its axethrower slots do.
constexpr float SKYBREAKER_EDGE_ROWS[] = {-20.5f, -16.0f};
constexpr float SKYBREAKER_EDGE_X0 = -29.0f;
constexpr uint32 SKYBREAKER_EDGE_COUNT = 7;
constexpr float SKYBREAKER_DECK_Z = 21.0f;
constexpr float ORGRIMS_HAMMER_EDGE_ROWS[] = {26.5f, 22.0f};
constexpr float ORGRIMS_HAMMER_EDGE_X0 = -12.0f;
constexpr uint32 ORGRIMS_HAMMER_EDGE_COUNT = 8;
constexpr float EDGE_SPACING = 5.0f;

float OrgrimsHammerDeckZ(float x) { return 34.35f + 0.068f * x; }

struct GsbStand
{
    Position spot;
    GsbStandKind kind = GsbStandKind::None;
    bool parked = false;
};

// The deck and window a tank's last defensive was cast for
struct GsbSpent
{
    GsbDeck deck = GsbDeck::None;
    uint8 window = 0;
};

struct GsbState
{
    bool wasLive = false;

    uint32 unitsMs = 0;
    ObjectGuid homeShip;
    ObjectGuid enemyShip;
    ObjectGuid captain;
    ObjectGuid mage;
    std::vector<ObjectGuid> cannons;
    std::vector<ObjectGuid> gunners;
    std::vector<ObjectGuid> mortars;
    std::vector<ObjectGuid> boarders;
    std::vector<Position> edgeSlots;

    uint32 rosterMs = 0;
    std::unordered_map<ObjectGuid, GsbDuty> duty;
    std::unordered_map<ObjectGuid, uint32> edgeSlot;
    bool rangedBoard = false;

    bool frozen = false;
    uint32 frozenSinceMs = 0;
    uint32 releasedMs = 0;
    RaidObs::ObsValue<uint32> retreat{"gsb.retreat", 0};
    RaidObs::ObsValue<uint32> fury{"gsb.fury", 0};

    std::unordered_map<ObjectGuid, GsbStand> stands;
    std::unordered_map<ObjectGuid, Position> notedCasts;

    // 1 or 2 while the captain's stacks put the board tank in a defensive window
    RaidObs::ObsValue<uint32> furyWindow{"gsb.furywindow", 0};
    std::unordered_map<ObjectGuid, GsbSpent> defensiveSpent;
    uint32 humanCallMs = 0;
};

RaidInstanceState<GsbState> sGsbState;

struct GsbHazard
{
    Position pos;
    float reach;
    char const* kind;
};

bool IsTankPlayer(Player* player) { return PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true); }

bool IsHealPlayer(Player* player) { return !IsTankPlayer(player) && PlayerbotAI::IsHeal(player); }

bool IsRangedDpsPlayer(Player* player)
{
    return !IsTankPlayer(player) && !IsHealPlayer(player) && PlayerbotAI::IsRangedDps(player);
}

// A bot of its own, not a player, selfbot included: players hold no duty
bool IsDutyBot(Player* player) { return GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

bool IsSkybreaker(uint32 entry) { return entry == GO_GSB_SKYBREAKER_A || entry == GO_GSB_SKYBREAKER_H; }

GsbState* StateOf(Player* bot) { return bot->GetInstanceId() ? sGsbState.Find(bot->GetInstanceId()) : nullptr; }

Transport* Ship(Player* bot, ObjectGuid const& guid) { return guid ? bot->GetMap()->GetTransport(guid) : nullptr; }

Creature* AliveCreature(Player* bot, ObjectGuid const& guid)
{
    Creature* creature = guid ? ObjectAccessor::GetCreature(*bot, guid) : nullptr;
    return creature && creature->IsAlive() ? creature : nullptr;
}

template <class Fn>
void ForEachPassenger(Transport* ship, Fn&& fn)
{
    if (!ship)
        return;

    for (WorldObject* passenger : ship->GetPassengers())
        if (Creature* creature = passenger ? passenger->ToCreature() : nullptr)
            fn(creature);

    if (MotionTransport* motion = ship->ToMotionTransport())
        for (WorldObject* passenger : motion->GetStaticPassengers())
            if (Creature* creature = passenger ? passenger->ToCreature() : nullptr)
                fn(creature);
}

void BuildEdgeSlots(GsbState& state, Transport* home, GsbSide const& side)
{
    state.edgeSlots.clear();
    bool const sky = IsSkybreaker(home->GetEntry());
    float const* rows = sky ? SKYBREAKER_EDGE_ROWS : ORGRIMS_HAMMER_EDGE_ROWS;
    float const x0 = sky ? SKYBREAKER_EDGE_X0 : ORGRIMS_HAMMER_EDGE_X0;
    uint32 const count = sky ? SKYBREAKER_EDGE_COUNT : ORGRIMS_HAMMER_EDGE_COUNT;
    for (uint32 row = 0; row < 2; ++row)
    {
        std::vector<Position> slots;
        for (uint32 k = 0; k < count; ++k)
        {
            float x = x0 + EDGE_SPACING * k;
            float y = rows[row];
            float z = sky ? SKYBREAKER_DECK_Z : OrgrimsHammerDeckZ(x);
            home->CalculatePassengerPosition(x, y, z);
            slots.emplace_back(x, y, z);
        }

        // Healers take the first slots, so nearest the board tank's spot comes first
        std::sort(slots.begin(), slots.end(), [&side](Position const& a, Position const& b)
                  { return a.GetExactDist2d(side.landing) < b.GetExactDist2d(side.landing); });
        state.edgeSlots.insert(state.edgeSlots.end(), slots.begin(), slots.end());
    }
}

void RefreshUnits(GsbState& state, Player* bot, GsbSide const& side, uint32 now, bool live)
{
    if (state.unitsMs && getMSTimeDiff(state.unitsMs, now) < GSB_UNIT_SCAN_MS)
        return;

    state.unitsMs = now;
    InstanceScript* instance = bot->GetInstanceScript();
    if (!instance)
        return;

    state.homeShip = instance->GetGuidData(ICC_BOSS_GUNSHIP_BATTLE);
    state.enemyShip = instance->GetGuidData(GSB_DATA_ENEMY_GUNSHIP);
    state.captain.Clear();
    state.mage.Clear();
    state.cannons.clear();
    state.gunners.clear();
    state.mortars.clear();
    state.boarders.clear();

    Transport* home = Ship(bot, state.homeShip);
    if (home && live)
        BuildEdgeSlots(state, home, side);

    // Both ships: the portal moves a boarder onto the raid's deck but leaves it its own ship's passenger
    ObjectGuid mageFallback;
    auto const sort = [&state, &side, &mageFallback, bot](Creature* creature)
    {
        if (!creature->IsAlive())
            return;

        uint32 const entry = creature->GetEntry();
        if (entry == side.cannon)
        {
            if (creature->IsFriendlyTo(bot))
                state.cannons.push_back(creature->GetGUID());
        }
        else if (!creature->IsHostileTo(bot))
            return;
        else if (entry == side.captain)
        {
            if (!state.captain || creature->IsInCombat())
                state.captain = creature->GetGUID();
        }
        else if (entry == side.mage)
        {
            if (creature->FindCurrentSpellBySpellId(SPELL_BELOW_ZERO))
                state.mage = creature->GetGUID();
            // The two portal mages channel Shadow Channeling
            else if (!creature->FindCurrentSpellBySpellId(SPELL_GSB_SHADOW_CHANNELING))
                mageFallback = creature->GetGUID();
        }
        else if (entry == side.gunner)
            state.gunners.push_back(creature->GetGUID());
        else if (entry == side.mortar)
            state.mortars.push_back(creature->GetGUID());
        else if (entry == side.marine || entry == side.sergeant)
            state.boarders.push_back(creature->GetGUID());
    };
    ForEachPassenger(home, sort);
    ForEachPassenger(Ship(bot, state.enemyShip), sort);
    if (!state.mage)
        state.mage = mageFallback;
}

void RefreshRoster(GsbState& state, Player* bot, uint32 now)
{
    if (state.rosterMs && getMSTimeDiff(state.rosterMs, now) < GSB_ROSTER_MS)
        return;

    state.rosterMs = now;
    std::vector<Player*> tanks;
    std::vector<Player*> healers;
    std::vector<Player*> ranged;
    std::vector<Player*> melee;
    auto const add = [&](Player* member)
    {
        if (!member || !member->IsAlive() || !member->IsInWorld() || member->GetMapId() != bot->GetMapId() ||
            member->GetInstanceId() != bot->GetInstanceId() || !IsDutyBot(member))
            return;

        if (IsTankPlayer(member))
            tanks.push_back(member);
        else if (IsHealPlayer(member))
            healers.push_back(member);
        else if (IsRangedDpsPlayer(member))
            ranged.push_back(member);
        else
            melee.push_back(member);
    };

    if (Group* group = bot->GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            add(ref->GetSource());
    else
        add(bot);

    auto const byGuid = [](Player* a, Player* b) { return a->GetGUID() < b->GetGUID(); };
    std::sort(healers.begin(), healers.end(), byGuid);
    std::sort(ranged.begin(), ranged.end(), byGuid);
    std::sort(melee.begin(), melee.end(), byGuid);
    std::sort(tanks.begin(), tanks.end(), [](Player* a, Player* b)
              {
                  bool const mainA = PlayerbotAI::IsMainTank(a);
                  bool const mainB = PlayerbotAI::IsMainTank(b);
                  return mainA != mainB ? mainA : a->GetGUID() < b->GetGUID();
              });

    state.duty.clear();
    state.edgeSlot.clear();
    state.rangedBoard = melee.empty();

    for (size_t i = 0; i < tanks.size(); ++i)
        state.duty[tanks[i]->GetGUID()] = i == 0 ? GsbDuty::BoardTank : GsbDuty::HomeTank;

    size_t cannonsLeft = state.cannons.size();
    for (Player* member : ranged)
    {
        if (cannonsLeft)
        {
            --cannonsLeft;
            state.duty[member->GetGUID()] = GsbDuty::Cannon;
        }
        else
            state.duty[member->GetGUID()] = GsbDuty::HomeRanged;
    }

    for (Player* member : melee)
    {
        if (cannonsLeft)
        {
            --cannonsLeft;
            state.duty[member->GetGUID()] = GsbDuty::Cannon;
        }
        else
            state.duty[member->GetGUID()] = GsbDuty::Boarder;
    }

    size_t const homeHealers = IsRaid25Man(bot) ? 2 : 1;
    std::vector<Player*> homeHeal;
    std::vector<Player*> boardHeal;
    for (size_t i = 0; i < healers.size(); ++i)
    {
        bool const home = i < homeHealers;
        state.duty[healers[i]->GetGUID()] = home ? GsbDuty::HomeHeal : GsbDuty::BoardHeal;
        (home ? homeHeal : boardHeal).push_back(healers[i]);
    }

    // Ranged cannoneers take a slot too, for when Below Zero throws them out
    uint32 slot = 0;
    for (std::vector<Player*> const* members : {&homeHeal, &boardHeal, &ranged})
        for (Player* member : *members)
            state.edgeSlot[member->GetGUID()] = slot++;
}

GsbDuty DutyIn(GsbState const& state, Player* bot)
{
    auto const found = state.duty.find(bot->GetGUID());
    return found != state.duty.end() ? found->second : GsbDuty::None;
}

char const* DutyName(GsbDuty duty)
{
    switch (duty)
    {
        case GsbDuty::BoardTank:
            return "boardtank";
        case GsbDuty::HomeTank:
            return "hometank";
        case GsbDuty::Cannon:
            return "cannon";
        case GsbDuty::Boarder:
            return "boarder";
        case GsbDuty::BoardHeal:
            return "boardheal";
        case GsbDuty::HomeHeal:
            return "homeheal";
        case GsbDuty::HomeRanged:
            return "homeranged";
        default:
            return "none";
    }
}

char const* DeckName(GsbDeck deck)
{
    switch (deck)
    {
        case GsbDeck::Home:
            return "friendly";
        case GsbDeck::Enemy:
            return "enemy";
        case GsbDeck::Gap:
            return "between";
        case GsbDeck::Cannon:
            return "cannon";
        case GsbDeck::Falling:
            return "falling";
        default:
            return "none";
    }
}

char const* BoardName(GsbBoard board)
{
    switch (board)
    {
        case GsbBoard::Wait:
            return "wait";
        case GsbBoard::Go:
            return "go";
        case GsbBoard::Back:
            return "back";
        case GsbBoard::Retreat:
            return "retreat";
        default:
            return "home";
    }
}

char const* StandName(GsbStandKind kind)
{
    switch (kind)
    {
        case GsbStandKind::Tank:
            return "tank";
        case GsbStandKind::Landing:
            return "landing";
        case GsbStandKind::Launch:
            return "launch";
        case GsbStandKind::Exit:
            return "exit";
        case GsbStandKind::Edge:
            return "edge";
        default:
            return "none";
    }
}

// Rocket Pack Useable lasts while the bot is near Zafod and the battle isn't won
bool CanRocket(Player* bot) { return GsbHasRocketPack(bot) && bot->HasAura(SPELL_ROCKET_PACK_USEABLE); }

// Ejected melee cannoneers board, ranged ones only when the raid has no melee DPS bot
bool InBoardingParty(GsbState const& state, Player* bot, GsbDuty duty)
{
    switch (duty)
    {
        case GsbDuty::BoardTank:
        case GsbDuty::BoardHeal:
        case GsbDuty::Boarder:
            return true;
        case GsbDuty::Cannon:
            return !bot->GetVehicle() && (!IsRangedDpsPlayer(bot) || state.rangedBoard);
        case GsbDuty::HomeRanged:
            return state.rangedBoard;
        default:
            return false;
    }
}

bool CaptainHeld(GsbSide const& side, Creature* captain)
{
    Unit* victim = captain ? captain->GetVictim() : nullptr;
    Player* player = victim ? victim->ToPlayer() : nullptr;
    return player && IsTankPlayer(player) && GsbDeckOf(side, player) == GsbDeck::Enemy;
}

GsbBoard DeriveBoard(GsbState const& state, GsbSide const& side, Player* bot)
{
    GsbDuty const duty = DutyIn(state, bot);
    GsbDeck const deck = GsbDeckOf(side, bot);
    if (!state.frozen || !InBoardingParty(state, bot, duty))
        return deck == GsbDeck::Enemy ? GsbBoard::Back : GsbBoard::Home;

    if (state.retreat.Get())
        return GsbBoard::Retreat;

    if (duty == GsbDuty::BoardTank || deck == GsbDeck::Enemy)
        return GsbBoard::Go;

    if (CaptainHeld(side, AliveCreature(bot, state.captain)) ||
        getMSTimeDiff(state.frozenSinceMs, getMSTime()) > GSB_NO_TANK_GO_MS)
        return GsbBoard::Go;

    return GsbBoard::Wait;
}

bool OthersOnEnemyDeck(Player* bot, GsbSide const& side)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != bot && member->IsAlive() && member->IsInWorld() &&
            member->GetMapId() == bot->GetMapId() && GsbDeckOf(side, member) == GsbDeck::Enemy)
            return true;
    }

    return false;
}

GsbDeck WantedDeck(GsbState const& state, GsbSide const& side, Player* bot, GsbBoard board)
{
    if (board == GsbBoard::Go)
        return GsbDeck::Enemy;

    // The captain turns on whoever is left once his tank goes
    if ((board == GsbBoard::Back || board == GsbBoard::Retreat) && DutyIn(state, bot) == GsbDuty::BoardTank &&
        GsbDeckOf(side, bot) == GsbDeck::Enemy && OthersOnEnemyDeck(bot, side) &&
        getMSTimeDiff(state.releasedMs, getMSTime()) < GSB_TANK_LAST_MS)
        return GsbDeck::Enemy;

    return GsbDeck::Home;
}

uint32 FuryStacks(Creature* captain)
{
    Aura* aura = captain->GetAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_BATTLE_FURY2, captain));
    return aura ? aura->GetStackAmount() : 0;
}

// The percent one stack adds
int32 FuryPerStack(Creature* captain)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(sSpellMgr->GetSpellIdForDifficulty(SPELL_BATTLE_FURY2, captain));
    return info ? std::max(1, info->Effects[EFFECT_0].CalcValue()) : 10;
}

uint32 RetreatStacks(Creature* captain)
{
    int32 const perStack = FuryPerStack(captain);
    return static_cast<uint32>((GSB_FURY_RETREAT_BONUS + perStack - 1) / perStack);
}

// 0 below the first defensive window, else 1 or 2
uint8 FuryWindow(Creature* captain)
{
    int32 const bonus = static_cast<int32>(FuryStacks(captain)) * FuryPerStack(captain);
    if (bonus >= GSB_FURY_SECOND_WINDOW_BONUS)
        return 2;

    return bonus >= GSB_FURY_FIRST_WINDOW_BONUS ? 1 : 0;
}

void UpdateFreeze(PlayerbotAI* botAI, GsbState& state, uint32 now)
{
    Player* bot = botAI->GetBot();
    bool const frozen = GsbCannonsFrozen(botAI);
    Creature* captain = AliveCreature(bot, state.captain);
    if (frozen && !state.frozen)
    {
        state.frozenSinceMs = now;
        // Cleared so the next thaw writes its count even when it repeats this one
        state.fury = 0u;
    }

    if (!frozen && state.frozen)
    {
        state.releasedMs = now;
        state.fury = captain ? FuryStacks(captain) : 0u;
        if (state.retreat.Get())
            state.retreat = 0u;
    }

    state.furyWindow = captain ? static_cast<uint32>(FuryWindow(captain)) : 0u;
    state.frozen = frozen;
    if (!frozen || !captain)
        return;

    uint32 const stacks = FuryStacks(captain);
    if (!state.retreat.Get() && stacks >= RetreatStacks(captain))
    {
        state.retreat = stacks;
        state.releasedMs = now;
    }
    else if (state.retreat.Get() && !stacks)
        state.retreat = 0u;
}

bool AnyBoarderOnHomeDeck(GsbState const& state, GsbSide const& side, Player* bot)
{
    for (ObjectGuid const& guid : state.boarders)
        if (Creature* boarder = AliveCreature(bot, guid))
            if (GsbDeckOf(side, boarder) == GsbDeck::Home)
                return true;

    return false;
}

Position TankSpot(GsbSide const& side)
{
    float const dx = side.mageArea.GetPositionX() - side.landing.GetPositionX();
    float const dy = side.mageArea.GetPositionY() - side.landing.GetPositionY();
    float const length = std::max(0.1f, std::sqrt(dx * dx + dy * dy));
    return Position(side.landing.GetPositionX() + dx / length * GSB_TANK_SPOT_DEPTH,
                    side.landing.GetPositionY() + dy / length * GSB_TANK_SPOT_DEPTH,
                    side.landing.GetPositionZ());
}

bool ExitSpot(GsbState const& state, GsbSide const& side, Player* bot, Position& spot)
{
    Transport* home = Ship(bot, state.homeShip);
    if (!home)
        return false;

    float x = side.portalExit.GetPositionX();
    float y = side.portalExit.GetPositionY();
    float z = side.portalExit.GetPositionZ();
    home->CalculatePassengerPosition(x, y, z);
    spot = Position(x, y, z);
    return true;
}

bool EdgeSpot(GsbState const& state, Player* bot, Position& spot)
{
    auto const slot = state.edgeSlot.find(bot->GetGUID());
    if (slot == state.edgeSlot.end() || state.edgeSlots.empty())
        return false;

    spot = state.edgeSlots[slot->second % state.edgeSlots.size()];
    return true;
}

GsbStandKind DesiredStand(PlayerbotAI* botAI, GsbState const& state, GsbSide const& side, Position& spot)
{
    Player* bot = botAI->GetBot();
    GsbDeck const deck = GsbDeckOf(side, bot);
    GsbDuty const duty = DutyIn(state, bot);
    GsbBoard const board = DeriveBoard(state, side, bot);
    if (deck == GsbDeck::Enemy)
    {
        if (duty == GsbDuty::BoardTank)
        {
            spot = TankSpot(side);
            return GsbStandKind::Tank;
        }

        if (duty == GsbDuty::BoardHeal && board == GsbBoard::Go)
        {
            spot = side.landing;
            return GsbStandKind::Landing;
        }

        return GsbStandKind::None;
    }

    if (deck != GsbDeck::Home || board == GsbBoard::Go)
        return GsbStandKind::None;

    // No stand until the pack is on, it'd pull the bot back from its walk to Zafod
    if (GsbNeedsRocketPack(bot))
        return GsbStandKind::None;

    if (board == GsbBoard::Wait)
    {
        spot = side.launch;
        return GsbStandKind::Launch;
    }

    bool const boardersHere = AnyBoarderOnHomeDeck(state, side, bot);
    switch (duty)
    {
        case GsbDuty::BoardTank:
            if (boardersHere)
                return GsbStandKind::None;
            spot = side.launch;
            return GsbStandKind::Launch;
        case GsbDuty::HomeTank:
            return !boardersHere && ExitSpot(state, side, bot, spot) ? GsbStandKind::Exit : GsbStandKind::None;
        case GsbDuty::HomeHeal:
        case GsbDuty::BoardHeal:
        case GsbDuty::HomeRanged:
            return EdgeSpot(state, bot, spot) ? GsbStandKind::Edge : GsbStandKind::None;
        case GsbDuty::Cannon:
            if (bot->GetVehicle() || !IsRangedDpsPlayer(bot) || GsbWantsCannon(botAI))
                return GsbStandKind::None;
            return EdgeSpot(state, bot, spot) ? GsbStandKind::Edge : GsbStandKind::None;
        default:
            return GsbStandKind::None;
    }
}

void UpdateStand(PlayerbotAI* botAI, GsbState& state, GsbSide const& side)
{
    Player* bot = botAI->GetBot();
    GsbStand& stand = state.stands[bot->GetGUID()];
    Position spot;
    GsbStandKind const kind = bot->IsAlive() ? DesiredStand(botAI, state, side, spot) : GsbStandKind::None;
    if (kind == GsbStandKind::None)
    {
        stand.kind = GsbStandKind::None;
        RaidObs::NoteDerived(bot, "gsb.stand", "none");
        return;
    }

    if (stand.kind != kind || stand.spot.GetExactDist2d(spot) > GSB_STAND_PARK)
    {
        stand.spot = spot;
        stand.kind = kind;
        stand.parked = false;
    }

    float const distance = bot->GetExactDist2d(stand.spot);
    stand.parked = stand.parked ? distance <= GSB_STAND_UNPARK : distance <= GSB_STAND_PARK;
    RaidObs::NoteDerived(bot, "gsb.stand", StandName(kind));
}

GsbStand const* HeldStand(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    GsbState* state = StateOf(bot);
    if (!state || !GsbLive(botAI))
        return nullptr;

    auto const found = state->stands.find(bot->GetGUID());
    return found != state->stands.end() && found->second.kind != GsbStandKind::None ? &found->second : nullptr;
}

void NoteArtillery(GsbState& state, Player* bot, Creature* mortar, Position const& dest)
{
    Position& noted = state.notedCasts[mortar->GetGUID()];
    if (noted.GetExactDist2d(dest) < 0.5f)
        return;

    noted = dest;
    // The 5 s cast, then a flight of up to 5 s, which the marker covers
    RaidObs::NoteHazardCircle(bot->GetMap(), SPELL_GSB_ROCKET_ARTILLERY, dest, GSB_ARTILLERY_RADIUS, 10000);
}

std::vector<GsbHazard> BuildHazards(PlayerbotAI* botAI)
{
    std::vector<GsbHazard> hazards;
    Player* bot = botAI->GetBot();
    GsbState* state = StateOf(bot);
    if (!state || !GsbLive(botAI))
        return hazards;

    for (ObjectGuid const& guid : state->mortars)
    {
        Creature* mortar = AliveCreature(bot, guid);
        Spell* cast = mortar ? mortar->FindCurrentSpellBySpellId(SPELL_GSB_ROCKET_ARTILLERY) : nullptr;
        if (!cast || !cast->m_targets.HasDst())
            continue;

        Position const dest = *cast->m_targets.GetDstPos();
        hazards.push_back({dest, GSB_ARTILLERY_RADIUS, "rocket"});
        if (RaidObs::Active())
            NoteArtillery(*state, bot, mortar, dest);
    }

    for (Position const& marker :
         EncounterHelpers::GetDynamicObjectPositions(bot, GSB_HAZARD_SCAN, SPELL_GSB_ARTILLERY_MARKER))
        hazards.push_back({marker, GSB_ARTILLERY_RADIUS, "rocket"});

    for (Position const& pitch :
         EncounterHelpers::GetDynamicObjectPositions(bot, GSB_HAZARD_SCAN, SPELL_GSB_BURNING_PITCH))
        hazards.push_back({pitch, GSB_PITCH_RADIUS + bot->GetCombatReach(), "pitch"});

    return hazards;
}

// Asked by the trigger, the rules and the action for every action the bot weighs
std::vector<GsbHazard> const& HazardsNow(PlayerbotAI* botAI)
{
    struct Memo
    {
        ObjectGuid bot;
        uint32 ms = 0;
        std::vector<GsbHazard> hazards;
    };
    thread_local Memo memo;
    Player* bot = botAI->GetBot();
    uint32 const now = getMSTime();
    if (memo.bot != bot->GetGUID() || memo.ms != now)
    {
        memo.bot = bot->GetGUID();
        memo.ms = now;
        memo.hazards = BuildHazards(botAI);
    }

    return memo.hazards;
}

GsbHazard const* HazardWithin(PlayerbotAI* botAI, float margin)
{
    Player* bot = botAI->GetBot();
    for (GsbHazard const& hazard : HazardsNow(botAI))
        if (bot->GetExactDist2d(hazard.pos) < hazard.reach + margin)
            return &hazard;

    return nullptr;
}

// Nothing walks a bot back into what it just stepped out of
bool SpotCovered(PlayerbotAI* botAI, Position const& spot)
{
    for (GsbHazard const& hazard : HazardsNow(botAI))
        if (spot.GetExactDist2d(hazard.pos) < hazard.reach + GSB_DODGE_CLEAR_MARGIN)
            return true;

    return false;
}

bool Airborne(Player* bot, GsbSide const& side)
{
    return bot->HasAura(SPELL_GSB_ROCKET_PACK_FLIGHT) || GsbDeckOf(side, bot) == GsbDeck::Gap;
}

// On deck and out of a cannon: a seated bot cannot step out
bool CanDodge(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    GsbSide const* side = GsbSideOf(bot);
    if (!side || !bot->IsAlive() || bot->GetVehicle() || !GsbLive(botAI))
        return false;

    GsbDeck const deck = GsbDeckOf(*side, bot);
    return deck == GsbDeck::Home || deck == GsbDeck::Enemy;
}

bool IsGsbHostile(Unit* unit)
{
    Creature* creature = unit ? unit->ToCreature() : nullptr;
    if (!creature)
        return false;

    for (GsbSide const* side : {&GSB_ALLIANCE, &GSB_HORDE})
    {
        uint32 const entry = creature->GetEntry();
        if (entry == side->captain || entry == side->mage || entry == side->gunner || entry == side->mortar ||
            entry == side->marine || entry == side->sergeant)
            return true;
    }

    return false;
}

uint8 Rank(Unit* unit)
{
    if (unit->HasAura(SPELL_GSB_BERSERK))
        return 4;
    if (unit->HasAura(SPELL_GSB_ELITE))
        return 3;
    if (unit->HasAura(SPELL_GSB_VETERAN))
        return 2;
    return unit->HasAura(SPELL_GSB_EXPERIENCED) ? 1 : 0;
}

bool HeldByTank(Unit* unit)
{
    Unit* victim = unit->GetVictim();
    Player* player = victim ? victim->ToPlayer() : nullptr;
    return player && IsTankPlayer(player);
}

// The oldest first: rank, then the closest to dying
bool KillsBefore(Unit* a, Unit* b)
{
    uint8 const rankA = Rank(a);
    uint8 const rankB = Rank(b);
    if (rankA != rankB)
        return rankA > rankB;

    float const pctA = a->GetHealthPct();
    float const pctB = b->GetHealthPct();
    if (pctA != pctB)
        return pctA < pctB;

    return a->GetGUID() < b->GetGUID();
}

Unit* PickBoarder(PlayerbotAI* botAI, GsbState const& state, GsbSide const& side, bool tank, bool ranged,
                  float range)
{
    Player* bot = botAI->GetBot();
    Unit* best = nullptr;
    for (ObjectGuid const& guid : state.boarders)
    {
        Creature* boarder = AliveCreature(bot, guid);
        if (!boarder || !boarder->IsInCombat() || GsbDeckOf(side, boarder) != GsbDeck::Home ||
            !bot->IsValidAttackTarget(boarder) || (ranged && !bot->IsWithinCombatRange(boarder, range)))
            continue;

        if (!best)
        {
            best = boarder;
            continue;
        }

        if (tank)
        {
            // Loose ones first, then the nearest
            bool const looseBest = !HeldByTank(best);
            bool const loose = !HeldByTank(boarder);
            if (loose != looseBest ? loose : bot->GetExactDist2d(boarder) < bot->GetExactDist2d(best))
                best = boarder;
        }
        else if (KillsBefore(boarder, best))
            best = boarder;
    }

    return best;
}

Unit* PickInRange(PlayerbotAI* botAI, std::vector<ObjectGuid> const& guids, float range)
{
    Player* bot = botAI->GetBot();
    Unit* best = nullptr;
    for (ObjectGuid const& guid : guids)
    {
        Creature* unit = AliveCreature(bot, guid);
        if (!unit || !unit->IsInCombat() || !bot->IsValidAttackTarget(unit) || !bot->IsWithinCombatRange(unit, range))
            continue;

        if (!best || Rank(unit) > Rank(best) ||
            (Rank(unit) == Rank(best) && bot->GetExactDist2d(unit) < bot->GetExactDist2d(best)))
            best = unit;
    }

    return best;
}

Unit* ResolveTarget(PlayerbotAI* botAI, char const*& why)
{
    why = "none";
    Player* bot = botAI->GetBot();
    GsbSide const* side = GsbSideOf(bot);
    GsbState* state = StateOf(bot);
    if (!side || !state || !bot->IsAlive() || bot->GetVehicle() || !GsbLive(botAI))
        return nullptr;

    GsbDeck const deck = GsbDeckOf(*side, bot);
    if (deck != GsbDeck::Home && deck != GsbDeck::Enemy)
        return nullptr;

    bool const tank = IsTankPlayer(bot);
    if (!tank && IsHealPlayer(bot))
        return nullptr;

    bool const ranged = IsRangedDpsPlayer(bot);
    float const range = botAI->GetRange("spell");
    if (Unit* skull = GetRaidHumanSkull(botAI, &IsGsbHostile))
        if (GsbDeckOf(*side, skull) == deck || (ranged && bot->IsWithinCombatRange(skull, range)))
        {
            why = "skull";
            return skull;
        }

    if (deck == GsbDeck::Enemy)
    {
        Creature* pick = AliveCreature(bot, tank ? state->captain : state->mage);
        if (!pick || !pick->IsInCombat() || GsbDeckOf(*side, pick) != GsbDeck::Enemy || !bot->IsValidAttackTarget(pick))
            return nullptr;

        why = tank ? "captain" : "mage";
        return pick;
    }

    if (Unit* boarder = PickBoarder(botAI, *state, *side, tank, ranged, range))
    {
        why = "boarder";
        return boarder;
    }

    if (!ranged)
        return nullptr;

    if (Unit* gunner = PickInRange(botAI, state->gunners, range))
    {
        why = "gunner";
        return gunner;
    }

    if (Unit* mortar = PickInRange(botAI, state->mortars, range))
    {
        why = "mortar";
        return mortar;
    }

    return nullptr;
}

// 1: a wave on the tank with the party across, or a Veteran. 2: an Elite
uint8 WaveWindow(GsbState const& state, GsbSide const& side, Player* tank)
{
    uint32 held = 0;
    uint8 rank = 0;
    for (ObjectGuid const& guid : state.boarders)
    {
        Creature* boarder = AliveCreature(tank, guid);
        if (!boarder || boarder->GetVictim() != tank || GsbDeckOf(side, boarder) != GsbDeck::Home ||
            !tank->IsWithinMeleeRange(boarder))
            continue;

        ++held;
        rank = std::max(rank, Rank(boarder));
    }

    if (rank >= GSB_WAVE_RANK_SECOND)
        return 2;

    if (rank >= GSB_WAVE_RANK_FIRST || (held >= GSB_WAVE_ON_TANK && OthersOnEnemyDeck(tank, side)))
        return 1;

    return 0;
}

// The window a tank stands in: the captain's stacks on his deck, the boarders on its own
uint8 DefensiveWindow(GsbState const& state, GsbSide const& side, Player* tank, GsbDeck deck)
{
    if (deck == GsbDeck::Home)
        return WaveWindow(state, side, tank);

    if (deck != GsbDeck::Enemy)
        return 0;

    Creature* captain = AliveCreature(tank, state.captain);
    if (!captain || captain->GetVictim() != tank)
        return 0;

    return FuryWindow(captain);
}

uint8 SpentWindow(GsbState const& state, Player* tank, GsbDeck deck)
{
    auto const found = state.defensiveSpent.find(tank->GetGUID());
    if (found == state.defensiveSpent.end() || found->second.deck != deck)
        return 0;

    return found->second.window;
}

bool IsTankDuty(GsbDuty duty) { return duty == GsbDuty::BoardTank || duty == GsbDuty::HomeTank; }

// the latch ends with its window, so the next one gets a button
void UpdateDefensive(PlayerbotAI* botAI, GsbState& state, GsbSide const& side)
{
    Player* bot = botAI->GetBot();
    if (!IsTankDuty(DutyIn(state, bot)))
        return;

    if (!DefensiveWindow(state, side, bot, GsbDeckOf(side, bot)))
        state.defensiveSpent.erase(bot->GetGUID());
}

// a human holding the captain gets a call at the same windows
void CallHumanTank(PlayerbotAI* botAI, GsbState& state, uint32 now)
{
    if (state.humanCallMs && getMSTimeDiff(state.humanCallMs, now) < GSB_HUMAN_CALL_MS)
        return;

    state.humanCallMs = now;
    Player* bot = botAI->GetBot();
    Creature* captain = AliveCreature(bot, state.captain);
    Unit* victim = captain ? captain->GetVictim() : nullptr;
    Player* human = victim ? victim->ToPlayer() : nullptr;
    if (!human || !human->IsAlive() || IsDutyBot(human))
        return;

    bool const low = human->GetHealthPct() < GSB_TANK_DEFENSIVE_PCT;
    if (!low && !FuryWindow(captain))
        return;

    uint32 const spellId = ReadyTankDefensiveSpell(human, true, 0, 0);
    if (spellId)
        CallHumanTankDefensive(bot, human, spellId, 0, low ? "you are low" : "Battle Fury is stacking",
                               "gsb.humancall");
}

void ResetLegacyRti(PlayerbotAI* botAI)
{
    Value<std::string>* rti = botAI->GetAiObjectContext()->GetValue<std::string>("rti");
    if (rti->Get() == "star")
        rti->Set("skull");
}
}  // namespace

GsbSide const* GsbSideOf(Player* bot)
{
    if (!bot || bot->GetMapId() != ICC_MAP_ID)
        return nullptr;

    InstanceScript* instance = bot->GetInstanceScript();
    if (!instance)
        return nullptr;

    return instance->GetData(GSB_DATA_TEAMID_IN_INSTANCE) == TEAM_HORDE ? &GSB_HORDE : &GSB_ALLIANCE;
}

bool GsbLive(PlayerbotAI* botAI) { return IccGunshipDefinition().Live(botAI); }

GsbDeck GsbDeckAt(GsbSide const& side, Position const& pos)
{
    float const z = pos.GetPositionZ();
    if (z >= GSB_AIRBORNE_Z)
        return GsbDeck::Gap;

    if (z < side.deckZ - GSB_FALL_DROP)
        return GsbDeck::Falling;

    float const split = (side.homeBoundY + side.enemyBoundY) / 2.0f;
    float const y = pos.GetPositionY();
    return (side.horde ? y <= split : y >= split) ? GsbDeck::Home : GsbDeck::Enemy;
}

GsbDeck GsbDeckOf(GsbSide const& side, Unit* unit)
{
    if (!unit)
        return GsbDeck::None;

    if (unit->IsPlayer() && unit->GetVehicle())
        return GsbDeck::Cannon;

    return GsbDeckAt(side, unit->GetPosition());
}

GsbDeck GsbBotDeck(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    GsbSide const* side = GsbSideOf(bot);
    GsbDeck const deck = side ? GsbDeckOf(*side, bot) : GsbDeck::None;
    RaidObs::NoteDerived(bot, "gsb.deck", DeckName(deck));
    return deck;
}

bool GsbCannonsFrozen(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    GsbState* state = StateOf(bot);
    if (!state || state->cannons.empty())
        return false;

    bool frozen = false;
    for (ObjectGuid const& guid : state->cannons)
        if (Creature* cannon = AliveCreature(bot, guid))
            frozen = frozen || cannon->HasAura(SPELL_BELOW_ZERO);

    RaidObs::NoteDerived(bot, "gsb.phase", frozen ? "frozen" : "fight");
    return frozen;
}

GsbBoard GsbBoardOf(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    GsbSide const* side = GsbSideOf(bot);
    GsbState* state = StateOf(bot);
    if (!side || !state || !GsbLive(botAI))
        return GsbBoard::Home;

    GsbBoard const board = DeriveBoard(*state, *side, bot);
    RaidObs::NoteDerived(bot, "gsb.board", BoardName(board));
    return board;
}

Unit* GsbTarget(PlayerbotAI* botAI)
{
    struct Memo
    {
        ObjectGuid bot;
        uint32 ms = 0;
        Unit* pick = nullptr;
    };
    // Asked by the trigger, the action and the targeting rule for every action the bot weighs
    thread_local Memo memo;
    Player* bot = botAI->GetBot();
    uint32 const now = getMSTime();
    if (memo.bot != bot->GetGUID() || memo.ms != now)
    {
        char const* why = "none";
        memo.bot = bot->GetGUID();
        memo.ms = now;
        memo.pick = ResolveTarget(botAI, why);
        RaidObs::NoteDerived(bot, "gsb.target", why);
    }

    return memo.pick;
}

Unit* GsbTauntTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    GsbSide const* side = GsbSideOf(bot);
    GsbState* state = StateOf(bot);
    if (!side || !state || !bot->IsAlive() || !IsTankPlayer(bot) || !GsbLive(botAI) ||
        !EncounterHelpers::ClassTauntReady(botAI))
        return nullptr;

    GsbDeck const deck = GsbDeckOf(*side, bot);
    if (deck == GsbDeck::Enemy)
    {
        Creature* captain = AliveCreature(bot, state->captain);
        return captain && DutyIn(*state, bot) == GsbDuty::BoardTank && captain->IsInCombat() &&
                       !HeldByTank(captain) && bot->GetExactDist2d(captain) <= GSB_TAUNT_RANGE
                   ? captain
                   : nullptr;
    }

    if (deck != GsbDeck::Home)
        return nullptr;

    Unit* best = nullptr;
    for (ObjectGuid const& guid : state->boarders)
    {
        Creature* boarder = AliveCreature(bot, guid);
        if (!boarder || !boarder->IsInCombat() || !boarder->GetVictim() || HeldByTank(boarder) ||
            GsbDeckOf(*side, boarder) != GsbDeck::Home || bot->GetExactDist2d(boarder) > GSB_TAUNT_RANGE)
            continue;

        if (!best || bot->GetExactDist2d(boarder) < bot->GetExactDist2d(best))
            best = boarder;
    }

    return best;
}

uint32 GsbTankDefensive(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    GsbSide const* side = GsbSideOf(bot);
    GsbState* state = StateOf(bot);
    if (!side || !state || !bot->IsAlive() || bot->GetVehicle() || !IsTankDuty(DutyIn(*state, bot)) ||
        !GsbLive(botAI))
        return 0;

    GsbDeck const deck = GsbDeckOf(*side, bot);
    if (deck != GsbDeck::Home && deck != GsbDeck::Enemy)
        return 0;

    bool const open = DefensiveWindow(*state, *side, bot, deck) > SpentWindow(*state, bot, deck);
    if (!open && bot->GetHealthPct() >= GSB_TANK_DEFENSIVE_PCT)
    {
        // Change-latched, so the way out of a window is written too
        RaidObs::NoteDerived(bot, "gsb.defensive", "idle");
        return 0;
    }

    return NextTankDefensive(botAI, bot, "gsb.defensive", true);
}

void GsbSpentTankDefensive(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    GsbSide const* side = GsbSideOf(bot);
    GsbState* state = StateOf(bot);
    if (!side || !state)
        return;

    GsbDeck const deck = GsbDeckOf(*side, bot);
    uint8 const window = std::max(SpentWindow(*state, bot, deck), DefensiveWindow(*state, *side, bot, deck));
    state->defensiveSpent[bot->GetGUID()] = GsbSpent{deck, window};
}

bool GsbTankDefensiveHeld(PlayerbotAI* botAI)
{
    return GsbLive(botAI) && botAI->GetBot()->GetHealthPct() >= GSB_TANK_LAST_RESORT_PCT;
}

bool GsbHasRocketPack(Player* bot)
{
    Item* pack = bot->GetItemByEntry(ITEM_GOBLIN_ROCKET_PACK);
    return pack && pack->IsEquipped();
}

bool GsbNeedsRocketPack(Player* bot)
{
    Transport* transport = bot->GetTransport();
    if (!transport || bot->GetVehicle() || GsbHasRocketPack(bot))
        return false;

    InstanceScript* instance = bot->GetInstanceScript();
    if (!instance || transport->GetGUID() != instance->GetGuidData(ICC_BOSS_GUNSHIP_BATTLE))
        return false;

    return bot->FindNearestCreature(NPC_ZAFOD_BOOMBOX, GSB_ZAFOD_RANGE) != nullptr;
}

GsbJump GsbJumpPlan(PlayerbotAI* botAI, Position& spot)
{
    Player* bot = botAI->GetBot();
    GsbSide const* side = GsbSideOf(bot);
    GsbState* state = StateOf(bot);
    if (!side || !state || !bot->IsAlive() || !GsbLive(botAI))
        return GsbJump::None;

    if (Airborne(bot, *side))
        return GsbJump::InFlight;

    GsbDeck const deck = GsbDeckOf(*side, bot);
    if (deck != GsbDeck::Home && deck != GsbDeck::Enemy)
        return GsbJump::None;

    GsbDeck const wanted = WantedDeck(*state, *side, bot, GsbBoardOf(botAI));
    if (wanted == deck || !CanRocket(bot))
        return GsbJump::None;

    Position const& from = deck == GsbDeck::Home ? side->launch : side->landing;
    if (bot->GetExactDist2d(from) > GSB_JUMP_READY)
    {
        // Don't walk onto a covered jump point, the dodge would just walk the bot back out
        if (SpotCovered(botAI, from))
            return GsbJump::None;

        spot = from;
        return GsbJump::Walk;
    }

    spot = deck == GsbDeck::Home ? side->landing : side->launch;
    return GsbJump::Fire;
}

// Asked by the rules for every move a bot weighs, so no full jump plan here
bool GsbInFlight(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    GsbSide const* side = GsbSideOf(bot);
    return side && bot->IsAlive() && GsbLive(botAI) && Airborne(bot, *side);
}

bool GsbHoldsStand(PlayerbotAI* botAI) { return HeldStand(botAI) != nullptr; }

GsbStandKind GsbStandOf(PlayerbotAI* botAI, Position& spot)
{
    GsbStand const* stand = HeldStand(botAI);
    if (!stand)
        return GsbStandKind::None;

    spot = stand->spot;
    return stand->kind;
}

bool GsbStandWalk(PlayerbotAI* botAI, Position& spot)
{
    GsbStand const* stand = HeldStand(botAI);
    if (!stand || stand->parked || SpotCovered(botAI, stand->spot))
        return false;

    spot = stand->spot;
    return true;
}

std::vector<EncounterHelpers::HazardCircle> GsbHazards(PlayerbotAI* botAI)
{
    std::vector<EncounterHelpers::HazardCircle> circles;
    for (GsbHazard const& hazard : HazardsNow(botAI))
        circles.push_back({hazard.pos, hazard.reach});

    return circles;
}

bool GsbDodging(PlayerbotAI* botAI)
{
    GsbHazard const* hazard = CanDodge(botAI) ? HazardWithin(botAI, GSB_DODGE_TRIGGER_MARGIN) : nullptr;
    RaidObs::NoteDerived(botAI->GetBot(), "gsb.dodge", hazard ? hazard->kind : "none");
    return hazard != nullptr;
}

bool GsbNearHazard(PlayerbotAI* botAI)
{
    return CanDodge(botAI) && HazardWithin(botAI, GSB_DODGE_CLEAR_MARGIN + 1.0f) != nullptr;
}

bool GsbOnDeck(Player* bot, GsbDeck deck, float x, float y, float z)
{
    GsbSide const* side = GsbSideOf(bot);
    GsbState* state = StateOf(bot);
    if (!side || !state || GsbDeckAt(*side, Position(x, y, z)) != deck)
        return false;

    bool const insideBound = deck == GsbDeck::Home ? (side->horde ? y <= side->homeBoundY : y >= side->homeBoundY)
                                                   : (side->horde ? y >= side->enemyBoundY : y <= side->enemyBoundY);
    if (!insideBound)
        return false;

    Transport* ship = deck == GsbDeck::Home    ? Ship(bot, state->homeShip)
                      : deck == GsbDeck::Enemy ? Ship(bot, state->enemyShip)
                                               : nullptr;
    if (!ship)
        return false;

    ship->CalculatePassengerOffset(x, y, z);
    DeckBox const& box = IsSkybreaker(ship->GetEntry()) ? SKYBREAKER_BOX : ORGRIMS_HAMMER_BOX;
    return x >= box.minX && x <= box.maxX && y >= box.minY && y <= box.maxY;
}

bool GsbWantsCannon(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    GsbSide const* side = GsbSideOf(bot);
    GsbState* state = StateOf(bot);
    if (!side || !state || !bot->IsAlive() || bot->GetVehicle() || DutyIn(*state, bot) != GsbDuty::Cannon ||
        state->frozen)
        return false;

    bool const live = GsbLive(botAI);
    // Before the battle the ship still flies, so only the transport says which deck the bot is on
    bool const onHome = live ? GsbDeckOf(*side, bot) == GsbDeck::Home
                             : bot->GetTransport() && bot->GetTransport()->GetGUID() == state->homeShip;
    if (!onHome)
        return false;

    // Pack first before the battle. A melee cannoneer also needs it to board once Below Zero throws it out.
    if (!GsbHasRocketPack(bot) && (!live || !IsRangedDpsPlayer(bot)))
        return false;

    return GsbFreeCannon(botAI) != nullptr;
}

Creature* GsbFreeCannon(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    GsbState* state = StateOf(bot);
    if (!state)
        return nullptr;

    Creature* best = nullptr;
    for (ObjectGuid const& guid : state->cannons)
    {
        Creature* cannon = AliveCreature(bot, guid);
        if (!cannon || !cannon->IsFriendlyTo(bot) || cannon->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) ||
            cannon->HasAura(SPELL_BELOW_ZERO) || !cannon->GetVehicleKit() ||
            !cannon->GetVehicleKit()->GetAvailableSeatCount())
            continue;

        if (!best || bot->GetExactDist(cannon) < bot->GetExactDist(best))
            best = cannon;
    }

    return best;
}

Unit* GsbCannonTarget(PlayerbotAI* botAI, Unit* cannon)
{
    Player* bot = botAI->GetBot();
    GsbSide const* side = GsbSideOf(bot);
    GsbState* state = StateOf(bot);
    if (!side || !state || !cannon)
        return nullptr;

    if (cannon->HasAura(SPELL_GSB_OVERHEAT))
    {
        RaidObs::NoteDerived(bot, "gsb.cannon", "overheat");
        return nullptr;
    }

    auto const nearest = [bot, cannon, side](std::vector<ObjectGuid> const& guids) -> Unit*
    {
        Unit* best = nullptr;
        for (ObjectGuid const& guid : guids)
        {
            Creature* unit = AliveCreature(bot, guid);
            if (!unit || !unit->IsHostileTo(bot) || GsbDeckOf(*side, unit) != GsbDeck::Enemy ||
                cannon->GetExactDist(unit) > GSB_CANNON_RANGE)
                continue;

            if (!best || cannon->GetExactDist(unit) < cannon->GetExactDist(best))
                best = unit;
        }

        return best;
    };

    char const* aim = "none";
    Unit* pick = nearest(state->mortars);
    if (pick)
        aim = "mortar";
    else if ((pick = nearest(state->gunners)))
        aim = "gunner";
    else if ((pick = nearest({state->captain})))
        aim = "other";

    RaidObs::NoteDerived(bot, "gsb.cannon", aim);
    return pick;
}

void GsbTick(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    ResetLegacyRti(botAI);
    GsbSide const* side = GsbSideOf(bot);
    if (!side)
        return;

    uint32 const instanceId = bot->GetInstanceId();
    bool const live = GsbLive(botAI);
    if (!live)
    {
        GsbState* state = sGsbState.Find(instanceId);
        if (state && state->wasLive)
            sGsbState.Reset(instanceId);

        // Before the battle only a bot aboard has work: the rocket pack and the cannon seats
        if (!bot->GetTransport())
            return;
    }

    GsbState& state = sGsbState.For(instanceId);
    state.wasLive = state.wasLive || live;
    uint32 const now = getMSTime();
    RefreshUnits(state, bot, *side, now, live);
    RefreshRoster(state, bot, now);
    RaidObs::NoteDerived(bot, "gsb.duty", DutyName(DutyIn(state, bot)));
    if (!live)
        return;

    UpdateFreeze(botAI, state, now);
    GsbBotDeck(botAI);
    GsbBoardOf(botAI);
    UpdateStand(botAI, state, *side);
    UpdateDefensive(botAI, state, *side);
    CallHumanTank(botAI, state, now);
}
