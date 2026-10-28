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

// icecrown_citadel.h DataTypes: the instance's TeamId and the enemy ship's guid. The raid's own ship sits
// in the boss's slot, ICC_BOSS_GUNSHIP_BATTLE.
constexpr uint32 GSB_DATA_TEAMID_IN_INSTANCE = 27;
constexpr uint32 GSB_DATA_ENEMY_GUNSHIP = 40;

// icecrown_citadel.h CreaturesIds: the boarding waves
constexpr uint32 NPC_GSB_SKYBREAKER_MARINE = 36950;
constexpr uint32 NPC_GSB_SKYBREAKER_SERGEANT = 36961;
constexpr uint32 NPC_GSB_KOR_KRON_REAVER = 36957;
constexpr uint32 NPC_GSB_KOR_KRON_SERGEANT = 36960;
// icecrown_citadel.h GameObjectIds: both factions' Skybreakers share one passenger map, so one hull
constexpr uint32 GO_GSB_SKYBREAKER_A = 201580;
constexpr uint32 GO_GSB_SKYBREAKER_H = 201811;

// No difficulty row for any of these. A mortar's 69679 is a 5 s cast at a player's spot on the raid's
// ship; at launch its 5 s marker 71371 lands there, and the explosion (69680, 69687-69689) hits 4 yd
// round it, centre to centre, knocking back on heroic.
constexpr uint32 SPELL_GSB_ROCKET_ARTILLERY = 69679;
constexpr uint32 SPELL_GSB_ARTILLERY_MARKER = 71371;
constexpr float GSB_ARTILLERY_RADIUS = 4.0f;
// A pitch patch is 5 yd for 3 s, and a persistent aura adds the target's reach and its own 0.39
constexpr uint32 SPELL_GSB_BURNING_PITCH = 69660;
constexpr float GSB_PITCH_RADIUS = 5.39f;
constexpr uint32 SPELL_GSB_OVERHEAT = 69487;
// The portal mages' channel, the freeze mage's is SPELL_BELOW_ZERO
constexpr uint32 SPELL_GSB_SHADOW_CHANNELING = 43897;
// On from the jump until the landing
constexpr uint32 SPELL_GSB_ROCKET_PACK_FLIGHT = 68721;
// Battle Experience ranks
constexpr uint32 SPELL_GSB_EXPERIENCED = 71188;
constexpr uint32 SPELL_GSB_VETERAN = 71193;
constexpr uint32 SPELL_GSB_ELITE = 71195;
constexpr uint32 SPELL_GSB_BERSERK = 72525;

// Cannon Blast adds 6-10 heat and Overheat comes at 100, so 90 is the last level a Blast is safe below
constexpr uint32 GSB_INCINERATE_HEAT = 90;
// Each Battle Fury stack adds 10% on 10-man and 12% on 25-man (row 2155). The party leaves once he
// hits for three times his base.
constexpr int32 GSB_FURY_RETREAT_BONUS = 200;
// Rocket Pack reaches 40 yd, launch to landing is 24 (Alliance) or 30 yd (Horde)
constexpr float GSB_JUMP_READY = 2.5f;
constexpr float GSB_TANK_SPOT_DEPTH = 6.0f;
constexpr float GSB_TAUNT_RANGE = 30.0f;
// A stand parks within GSB_STAND_PARK and walks back once pushed past GSB_STAND_UNPARK
constexpr float GSB_STAND_PARK = 1.0f;
constexpr float GSB_STAND_UNPARK = 2.5f;
// A dodge fires inside a hazard's reach plus this, and lands past reach + park + margin
constexpr float GSB_DODGE_TRIGGER_MARGIN = 1.5f;
constexpr float GSB_DODGE_CLEAR_MARGIN = GSB_STAND_PARK + 1.5f;
static_assert(GSB_DODGE_TRIGGER_MARGIN < GSB_DODGE_CLEAR_MARGIN, "a dodge has to end outside its own trigger");

// The core's world bounds of each deck, from the captains' and boarders' CanAIAttack: the Alliance raid's
// Skybreaker lies past y 2042 and Orgrim's Hammer short of 2025, the Horde raid's Orgrim's Hammer short of
// 2431 and the Skybreaker past 2445. A unit belongs to the ship on its side of the middle of that gap, but
// a dodge spot must lie inside the bound itself. A captain drops a victim at z 478 or above, one in flight.
constexpr float GSB_AIRBORNE_Z = 478.0f;
// Under the lowest deck point by this much is a fall
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
    // World points on the decks, inside the y bounds above. Every deck is a gameobject floor, so navprobe
    // can check none of them.
    Position launch;
    Position landing;
    Position mageArea;
    // Transport-local, from boss_icecrown_gunship_battle.cpp: where boarders land on the raid's ship
    Position portalExit;
};

enum class GsbDeck : uint8
{
    None,
    Home,
    Enemy,
    Gap,  // airborne
    Cannon,
    Falling
};

// Stored per bot, noted as gsb.duty
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
    Wait,  // at the launch point until a tank holds the captain
    Go,
    Back,  // the mage is dead
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
    Walk,  // to the launch or landing point
    Fire,
    InFlight
};

// Null off map 631 or outside an instance
GsbSide const* GsbSideOf(Player* bot);
bool GsbLive(PlayerbotAI* botAI);

GsbDeck GsbDeckAt(GsbSide const& side, Position const& pos);
// A player in a vehicle is in a cannon; anyone else by position, since a boarder stays a passenger of
// its own ship after the portal moves it onto the raid's
GsbDeck GsbDeckOf(GsbSide const& side, Unit* unit);
// Notes gsb.deck
GsbDeck GsbBotDeck(PlayerbotAI* botAI);

// Below Zero on the raid's cannons. Notes gsb.phase.
bool GsbCannonsFrozen(PlayerbotAI* botAI);
Creature* GsbCaptain(PlayerbotAI* botAI);
// The mage channelling Below Zero
Creature* GsbFreezeMage(PlayerbotAI* botAI);

GsbDuty GsbDutyOf(PlayerbotAI* botAI);
// Notes gsb.board
GsbBoard GsbBoardOf(PlayerbotAI* botAI);

// A player's skull, else the duty's pick; always a unit in combat. Notes gsb.target.
Unit* GsbTarget(PlayerbotAI* botAI);
// A tank's pick: the captain off a non-tank, or a boarder on the raid's deck nobody tanks
Unit* GsbTauntTarget(PlayerbotAI* botAI);

// The cross-deck move this bot owes: where to walk, or where to jump
GsbJump GsbJumpPlan(PlayerbotAI* botAI, Position& spot);
bool GsbInFlight(PlayerbotAI* botAI);
bool GsbHasRocketPack(Player* bot);

bool GsbHoldsStand(PlayerbotAI* botAI);
GsbStandKind GsbStandOf(PlayerbotAI* botAI, Position& spot);
// The stand wants a walk and nothing covers its spot
bool GsbStandWalk(PlayerbotAI* botAI, Position& spot);

// Hazards on the bot's deck, each with its reach. Mortar casts are noted as hazard circles.
std::vector<EncounterHelpers::HazardCircle> GsbHazards(PlayerbotAI* botAI);
// Out of a cannon, on a deck and inside a hazard's trigger. Notes gsb.dodge.
bool GsbDodging(PlayerbotAI* botAI);
// Inside a hazard's clearance, where a generic mover would walk the bot back in
bool GsbNearHazard(PlayerbotAI* botAI);
// Inside that deck's hull box, transport-local, and on that deck by the y bounds
bool GsbOnDeck(Player* bot, GsbDeck deck, float x, float y, float z);

// A cannoneer out of its cannon that should be in one
bool GsbWantsCannon(PlayerbotAI* botAI);
Creature* GsbFreeCannon(PlayerbotAI* botAI);
// Mortars, then gunners, then anything else on the enemy deck. Notes gsb.cannon.
Unit* GsbCannonTarget(PlayerbotAI* botAI, Unit* cannon);

// The unit cache, the roster, the freeze edges, the retreat latch, the stands, the wipe reset
void GsbTick(PlayerbotAI* botAI);

#endif
