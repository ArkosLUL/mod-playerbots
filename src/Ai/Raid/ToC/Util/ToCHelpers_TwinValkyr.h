#ifndef PLAYERBOTS_RAID_TOCHELPERS_TWINVALKYR_H
#define PLAYERBOTS_RAID_TOCHELPERS_TWINVALKYR_H

#include "PlayerbotAI.h"
#include "Position.h"
#include "ToCData.h"

class Creature;
struct RaidPredictedHit;

namespace TrialOfTheCrusaderHelpers
{

// Shield of Lights/Darkness, put up right before the twin's Pact. 10N ids, remap against the twin.
// While it holds she's immune to interrupts.
constexpr uint32 SPELL_LIGHT_SHIELD = 65858;
constexpr uint32 SPELL_DARK_SHIELD = 65874;

constexpr uint32 NPC_CONCENTRATED_LIGHT = 34630;
constexpr uint32 NPC_CONCENTRATED_DARK = 34628;

// An orb blows up on the nearest player inside this, 2D
constexpr float TWIN_ORB_TRIGGER_RADIUS = 2.75f;
// Unleashed Light/Dark, absorbed by the matching essence
constexpr float TWIN_ORB_BLAST_RADIUS = 6.0f;
// The search clears a yard past what the trigger fires on, or the bot lands back on the rim
constexpr float TWIN_ORB_DODGE_CLEARANCE = 4.0f;
constexpr float TWIN_ORB_DODGE_SEARCH_CLEARANCE = 5.0f;
constexpr float TWIN_ORB_DODGE_SEARCH_RADIUS = 12.0f;
// About a second of orb flight. Any longer and a heroic wave covers most of the arena, so bots
// never stop dodging.
constexpr float TWIN_ORB_HORIZON = 7.0f;
// Popping an orb of your own colour still hits other-colour allies in the blast
constexpr float TWIN_ORB_SPLASH_ALLY_RADIUS = 7.0f;

enum class TwinColour : uint8
{
    None,
    Light,
    Dark
};

// Hold: keeps the colour it has through a running Vortex, so no walk
enum class TwinEssenceReason : uint8
{
    None,
    Base,
    Hold,
    Touch,
    Vortex
};

struct TwinEssenceWant
{
    TwinColour colour = TwinColour::None;
    TwinEssenceReason reason = TwinEssenceReason::None;
};

enum class TwinTankRole : uint8
{
    None,
    Fjola,
    Eydis,
    Both
};

enum class TwinDoorRole : uint8
{
    None,
    Tank,
    Melee,
    Ranged,
    Heal
};

struct TwinDoorStand
{
    TwinDoorRole role = TwinDoorRole::None;
    // the staging label: fjola, eydis, both, melee, ranged, healer
    char const* label = "none";
    Position spot;
};

// A walk back starts past UNPARK and ends inside PARK
constexpr float TWIN_DOOR_PARK = 1.0f;
constexpr float TWIN_DOOR_UNPARK = 2.5f;
// A twin further than this off her door spot is out of melee reach from the tunnel
constexpr float TWIN_DOOR_MELEE_REACH = 12.0f;

// Alive, else nullptr
Unit* GetFjola(PlayerbotAI* botAI);
Unit* GetEydis(PlayerbotAI* botAI);
// Both twins stay non-attackable until the script releases them at the pull
bool IsTwinAttackable(Unit* twin);

// Fjola Light, Eydis Dark, anything else None
TwinColour TwinColourOf(Unit* twin);
TwinColour EssenceOf(Unit* unit);

bool HasLightEssence(Unit* unit);
bool HasDarkEssence(Unit* unit);
bool HasAnyEssence(Unit* unit);

// Heroic Touch of Light / Touch of Darkness
bool HasLightTouch(Unit* unit);
bool HasDarkTouch(Unit* unit);

// From cast start to channel end
TwinColour ActiveVortexColour(PlayerbotAI* botAI);
Unit* GetTwinCastingPact(PlayerbotAI* botAI);
Unit* GetShieldedTwin(PlayerbotAI* botAI);

// Main tank on Fjola, first living assist on Eydis. A lone tank gets both.
Player* GetTwinTank(Player* bot, Unit* twin);
bool IsTwinTank(Player* bot, Unit* twin);

// A running Vortex first: a tank keeps its colour, and on normal so does the raid through a Light Vortex
// while a Fire Resistance Aura paladin had Aura Mastery ready at the cast start. Then a Touch, then Light
// for Eydis's tank and Dark for everyone else.
TwinEssenceWant GetWantedEssence(PlayerbotAI* botAI);
// urgentOnly: only a Touch or a Vortex counts
bool TwinValkyrMustSwapEssence(PlayerbotAI* botAI, bool urgentOnly);
// The portal of that colour with the shortest walk to it and on to the bot's door stand
Creature* GetEssencePortal(Player* bot, TwinColour colour);

// A player's skull on either twin, else the Pact twin while one casts (heroic either, normal only
// Fjola), else Fjola under a lone tank, else the twin of the other colour than the bot's essence. A
// melee bot skips a twin more than TWIN_DOOR_MELEE_REACH off her door spot.
Unit* GetTwinDpsTarget(PlayerbotAI* botAI);

// The bot's stand in the entrance tunnel, 51+ yd from the arena centre, so past every orb. Dealt by
// guid over the group's bots, so staging and the fight agree. Writes tv.door, role:x,y.
TwinDoorStand GetTwinDoorStand(Player* bot);
// Where the twin's tank holds her
Position GetTwinDoorSpot(Player* bot, Unit* twin);
// Every bot with a door stand, except a tank walking out to fetch its twin
bool TwinValkyrDoorHold(PlayerbotAI* botAI);
// A bot off its door stand, tanks aside, with no essence walk due and none waiting on the end of a
// running Vortex. Keeps the park latch.
bool TwinValkyrDoorWalkDue(PlayerbotAI* botAI);
// The twin is on someone else, past taunt range of her door spot, and hasn't closed on it for 2 s
bool TwinValkyrFetchDue(PlayerbotAI* botAI, Unit* twin);

// The running Vortex for a bot keeping its colour through it
bool GetTwinVortexHit(PlayerbotAI* botAI, RaidPredictedHit& hit);
// The running Vortex while the whole raid keeps Dark through a normal Light Vortex
bool GetTwinVortexRaidHit(PlayerbotAI* botAI, RaidPredictedHit& hit);
// Before that one's first tick
bool TwinValkyrRaidCoverHeld(PlayerbotAI* botAI);

// Heroic: Eydis's shield goes up, or the 3rd special starts with no Eydis Pact among them. Normal: both
// twins tanked at their door spots, or 20 s into the pull. Latched for the pull.
bool TwinValkyrLustWindowOpen(PlayerbotAI* botAI);
// Burst cooldowns wait for lust to land on the bot. With no lust coming they wait for the window only,
// and with lust coming at most 10 s past it.
bool TwinValkyrBurstHeld(PlayerbotAI* botAI);

// Only interrupts the twins aren't immune to, in range, off cooldown and affordable. Null for a real
// player.
char const* TwinReadyInterrupt(Player* bot, Unit* twin);
// Lowest guid holding a ready interrupt while the twin casts an unshielded Pact
bool IsTwinPactInterrupter(PlayerbotAI* botAI, Unit* twin);

// An orb's path counts when it's the wrong colour for the bot, or its own colour with an
// other-colour ally close enough to get splashed
bool TwinOrbThreatens(PlayerbotAI* botAI, float clearance);
bool TwinOrbSpotClear(PlayerbotAI* botAI, Position const& spot, float clearance);
bool FindTwinOrbDodgeSpot(PlayerbotAI* botAI, Position& spot);

}

#endif
