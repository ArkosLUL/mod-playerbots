/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_GUNSHIP_H
#define PLAYERBOTS_ICCHELPERS_GUNSHIP_H

#include <vector>

#include "Define.h"
#include "EncounterHelpers.h"
#include "ICCData.h"
#include "Position.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;

constexpr uint32 GSB_DATA_TEAMID_IN_INSTANCE = 27;
constexpr uint32 GSB_DATA_ENEMY_GUNSHIP = 40;

constexpr uint32 NPC_GSB_SKYBREAKER_MARINE = 36950;
constexpr uint32 NPC_GSB_SKYBREAKER_SERGEANT = 36961;
constexpr uint32 NPC_GSB_KOR_KRON_REAVER = 36957;
constexpr uint32 NPC_GSB_KOR_KRON_SERGEANT = 36960;
constexpr uint32 GO_GSB_SKYBREAKER_A = 201580;
constexpr uint32 GO_GSB_SKYBREAKER_H = 201811;

constexpr uint32 SPELL_GSB_ROCKET_ARTILLERY = 69679;
constexpr uint32 SPELL_GSB_ARTILLERY_MARKER = 71371;
constexpr float GSB_ARTILLERY_RADIUS = 4.0f;
constexpr uint32 SPELL_GSB_BURNING_PITCH = 69660;
constexpr float GSB_PITCH_RADIUS = 5.0f;
constexpr uint32 SPELL_GSB_OVERHEAT = 69487;
constexpr uint32 SPELL_GSB_SHADOW_CHANNELING = 43897;
constexpr uint32 SPELL_GSB_ROCKET_PACK_FLIGHT = 68721;
constexpr uint32 SPELL_GSB_EXPERIENCED = 71188;
constexpr uint32 SPELL_GSB_VETERAN = 71193;
constexpr uint32 SPELL_GSB_ELITE = 71195;
constexpr uint32 SPELL_GSB_BERSERK = 72525;

constexpr uint32 GSB_INCINERATE_HEAT = 90;
constexpr int32 GSB_FURY_RETREAT_BONUS = 200;
constexpr int32 GSB_FURY_FIRST_WINDOW_BONUS = 50;
constexpr int32 GSB_FURY_SECOND_WINDOW_BONUS = 100;
static_assert(GSB_FURY_SECOND_WINDOW_BONUS < GSB_FURY_RETREAT_BONUS, "the party leaves after the last window");
constexpr float GSB_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float GSB_TANK_LAST_RESORT_PCT = 25.0f;
constexpr uint32 GSB_WAVE_ON_TANK = 3;
constexpr uint8 GSB_WAVE_RANK_FIRST = 2;
constexpr uint8 GSB_WAVE_RANK_SECOND = 3;
constexpr float GSB_JUMP_READY = 2.5f;
constexpr float GSB_ZAFOD_RANGE = 150.0f;
constexpr float GSB_TANK_SPOT_DEPTH = 6.0f;
constexpr float GSB_TAUNT_RANGE = 30.0f;
constexpr float GSB_STAND_PARK = 1.0f;
constexpr float GSB_STAND_UNPARK = 2.5f;
constexpr float GSB_DODGE_TRIGGER_MARGIN = 1.5f;
constexpr float GSB_DODGE_CLEAR_MARGIN = GSB_STAND_PARK + 1.5f;
static_assert(GSB_DODGE_TRIGGER_MARGIN < GSB_DODGE_CLEAR_MARGIN, "a dodge has to end outside its own trigger");

constexpr float GSB_AIRBORNE_Z = 478.0f;
constexpr float GSB_FALL_DROP = 6.0f;

struct GsbSide
{
    bool horde;
    uint32 cannon;
    uint32 captain;
    uint32 mage;
    uint32 gunner;
    uint32 mortar;
    uint32 marine;
    uint32 sergeant;
    float homeBoundY;
    float enemyBoundY;
    float deckZ;
    Position launch;
    Position landing;
    Position mageArea;
    Position portalExit;
};

enum class GsbDeck : uint8
{
    None,
    Home,
    Enemy,
    Gap,
    Cannon,
    Falling
};

enum class GsbDuty : uint8
{
    None,
    BoardTank,
    HomeTank,
    Cannon,
    Boarder,
    BoardHeal,
    HomeHeal,
    HomeRanged
};

enum class GsbBoard : uint8
{
    Home,
    Wait,
    Go,
    Back,
    Retreat
};

enum class GsbStandKind : uint8
{
    None,
    Tank,
    Landing,
    Launch,
    Exit,
    Edge
};

enum class GsbJump : uint8
{
    None,
    Walk,
    Fire,
    InFlight
};

GsbSide const* GsbSideOf(Player* bot);
bool GsbLive(PlayerbotAI* botAI);

GsbDeck GsbDeckAt(GsbSide const& side, Position const& pos);
GsbDeck GsbDeckOf(GsbSide const& side, Unit* unit);
GsbDeck GsbBotDeck(PlayerbotAI* botAI);

bool GsbCannonsFrozen(PlayerbotAI* botAI);

GsbBoard GsbBoardOf(PlayerbotAI* botAI);

Unit* GsbTarget(PlayerbotAI* botAI);
Unit* GsbTauntTarget(PlayerbotAI* botAI);

uint32 GsbTankDefensive(PlayerbotAI* botAI);
void GsbSpentTankDefensive(PlayerbotAI* botAI);
bool GsbTankDefensiveHeld(PlayerbotAI* botAI);

GsbJump GsbJumpPlan(PlayerbotAI* botAI, Position& spot);
bool GsbInFlight(PlayerbotAI* botAI);
bool GsbHasRocketPack(Player* bot);
bool GsbNeedsRocketPack(Player* bot);

bool GsbHoldsStand(PlayerbotAI* botAI);
GsbStandKind GsbStandOf(PlayerbotAI* botAI, Position& spot);
bool GsbStandWalk(PlayerbotAI* botAI, Position& spot);

std::vector<EncounterHelpers::HazardCircle> GsbHazards(PlayerbotAI* botAI);
bool GsbDodging(PlayerbotAI* botAI);
bool GsbNearHazard(PlayerbotAI* botAI);
bool GsbOnDeck(Player* bot, GsbDeck deck, float x, float y, float z);

bool GsbWantsCannon(PlayerbotAI* botAI);
Creature* GsbFreeCannon(PlayerbotAI* botAI);
Unit* GsbCannonTarget(PlayerbotAI* botAI, Unit* cannon);

void GsbTick(PlayerbotAI* botAI);

#endif
