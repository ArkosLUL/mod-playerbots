#ifndef PLAYERBOTS_RAID_TOCHELPERS_TWINVALKYR_H
#define PLAYERBOTS_RAID_TOCHELPERS_TWINVALKYR_H

#include "PlayerbotAI.h"
#include "Position.h"
#include "ToCData.h"

class Creature;
struct RaidPredictedHit;

namespace TrialOfTheCrusaderHelpers
{

inline constexpr uint32 SPELL_LIGHT_SHIELD = 65858;
inline constexpr uint32 SPELL_DARK_SHIELD = 65874;

inline constexpr uint32 NPC_CONCENTRATED_LIGHT = 34630;
inline constexpr uint32 NPC_CONCENTRATED_DARK = 34628;

inline constexpr float TWIN_ORB_TRIGGER_RADIUS = 2.75f;
inline constexpr float TWIN_ORB_BLAST_RADIUS = 6.0f;
inline constexpr float TWIN_ORB_DODGE_CLEARANCE = 4.0f;
inline constexpr float TWIN_ORB_DODGE_SEARCH_CLEARANCE = 5.0f;
inline constexpr float TWIN_ORB_DODGE_SEARCH_RADIUS = 12.0f;
inline constexpr float TWIN_ORB_HORIZON = 7.0f;
inline constexpr float TWIN_ORB_SPLASH_ALLY_RADIUS = 7.0f;

enum class TwinColour : uint8
{
    None,
    Light,
    Dark
};

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
    char const* label = "none";
    Position spot;
};

inline constexpr float TWIN_DOOR_PARK = 1.0f;
inline constexpr float TWIN_DOOR_UNPARK = 2.5f;
inline constexpr float TWIN_DOOR_MELEE_REACH = 12.0f;

Unit* GetFjola(PlayerbotAI* botAI);
Unit* GetEydis(PlayerbotAI* botAI);
bool IsTwinAttackable(Unit* twin);

TwinColour TwinColourOf(Unit* twin);
TwinColour EssenceOf(Unit* unit);

bool HasLightEssence(Unit* unit);
bool HasDarkEssence(Unit* unit);

bool HasLightTouch(Unit* unit);
bool HasDarkTouch(Unit* unit);

Unit* GetTwinCastingPact(PlayerbotAI* botAI);
Unit* GetShieldedTwin(PlayerbotAI* botAI);

Player* GetTwinTank(Player* bot, Unit* twin);
bool IsTwinTank(Player* bot, Unit* twin);

TwinEssenceWant GetWantedEssence(PlayerbotAI* botAI);
bool TwinValkyrMustSwapEssence(PlayerbotAI* botAI, bool urgentOnly);
bool TwinEssenceGossipDue(Player* bot, TwinEssenceWant const& want);
Creature* GetEssencePortal(Player* bot, TwinColour colour);
bool TwinValkyrArrived(PlayerbotAI* botAI);
bool TwinValkyrBoostEssenceWalk(PlayerbotAI* botAI, TwinEssenceReason reason, Creature* portal);

Unit* GetTwinDpsTarget(PlayerbotAI* botAI);

TwinDoorStand GetTwinDoorStand(Player* bot);
Position GetTwinDoorSpot(Player* bot, Unit* twin);
bool TwinValkyrDoorHold(PlayerbotAI* botAI);
bool TwinValkyrDoorWalkDue(PlayerbotAI* botAI);
bool TwinValkyrNextPortalDue(PlayerbotAI* botAI, Position& spot, Creature*& portal, TwinEssenceReason& reason);
bool TwinValkyrFetchDue(PlayerbotAI* botAI, Unit* twin);

bool GetTwinVortexHit(PlayerbotAI* botAI, RaidPredictedHit& hit);
bool GetTwinVortexRaidHit(PlayerbotAI* botAI, RaidPredictedHit& hit);
bool TwinValkyrRaidCoverHeld(PlayerbotAI* botAI);

bool TwinValkyrLustWindowOpen(PlayerbotAI* botAI);
bool TwinValkyrBurstHeld(PlayerbotAI* botAI);

char const* TwinReadyInterrupt(Player* bot, Unit* twin);
bool IsTwinPactInterrupter(PlayerbotAI* botAI, Unit* twin);

bool TwinOrbThreatens(PlayerbotAI* botAI, float clearance);
bool TwinOrbSpotClear(PlayerbotAI* botAI, Position const& spot, float clearance);
bool FindTwinOrbDodgeSpot(PlayerbotAI* botAI, Position& spot);

enum class TwinWalkWait : uint8
{
    None,
    Orb,
    Swap,
    Heal
};

struct TwinWaitBudget
{
    uint32 sinceMs = 0;
    uint32 lastMs = 0;
    bool Allow(uint32 now);
};

TwinWalkWait TwinValkyrWalkWait(PlayerbotAI* botAI, Position const& to, TwinEssenceReason reason, TwinColour swap,
                                TwinWaitBudget& budget);
bool TwinValkyrDodgeYields(PlayerbotAI* botAI);

}

#endif
