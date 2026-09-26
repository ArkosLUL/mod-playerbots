#ifndef PLAYERBOTS_RAID_TOCHELPERS_TWINVALKYR_H
#define PLAYERBOTS_RAID_TOCHELPERS_TWINVALKYR_H

#include "PlayerbotAI.h"
#include "Position.h"
#include "ToCData.h"

class Creature;

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

// Ordered by priority, Touch highest
enum class TwinEssenceReason : uint8
{
    None,
    Base,
    Shield,
    Vortex,
    Touch
};

struct TwinEssenceWant
{
    TwinColour colour = TwinColour::None;
    TwinEssenceReason reason = TwinEssenceReason::None;
};

// Alive, else nullptr
Unit* GetFjola(PlayerbotAI* botAI);
Unit* GetEydis(PlayerbotAI* botAI);

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

TwinEssenceWant GetWantedEssence(PlayerbotAI* botAI);
// urgentOnly: only a Touch or a Vortex counts
bool TwinValkyrMustSwapEssence(PlayerbotAI* botAI, bool urgentOnly);
Creature* GetEssencePortal(Player* bot, TwinColour colour);

// The Pact twin while one casts, else Fjola under a lone tank, else the twin of the other colour than
// the bot's essence
Unit* GetTwinDpsTarget(PlayerbotAI* botAI);
// "cross" for Eydis, "skull" for anything else
char const* TwinRtiIcon(Unit* twin);

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
