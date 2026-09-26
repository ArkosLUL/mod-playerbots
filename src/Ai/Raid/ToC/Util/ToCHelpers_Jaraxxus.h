#ifndef PLAYERBOTS_RAID_TOCHELPERS_JARAXXUS_H
#define PLAYERBOTS_RAID_TOCHELPERS_JARAXXUS_H

#include <string>
#include <vector>

#include "PlayerbotAI.h"
#include "Position.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

constexpr uint32 SPELL_LEGION_FLAME_TRAIL = 66199;   // row 66199/68126/68127/68128
constexpr uint32 SPELL_MISTRESS_KISS = 66334;        // row 66334/67905/67906/67907

constexpr float JARAXXUS_LEGION_FLAME_RADIUS = 3.0f;       // 66877 row, centre to centre
constexpr float JARAXXUS_LEGION_FLAME_TRIGGER = 4.0f;      // dodge starts inside this
constexpr float JARAXXUS_LEGION_FLAME_CLEAR = 6.0f;        // a spot counts clear of a flame past this
constexpr float JARAXXUS_LEGION_FLAME_SEARCH = 15.0f;      // sweep maxRadius
constexpr float JARAXXUS_FLAME_CARRIER_MIN_LEG = 6.0f;     // carrier travel floor per leg
constexpr float JARAXXUS_FLAME_CARRIER_RAID_CLEAR = 8.0f;  // carrier spot from other members
constexpr float JARAXXUS_FLAME_CARRIER_BOSS_CLEAR = 15.0f; // carrier spot from the boss
constexpr float JARAXXUS_FLAME_ARRIVE = 1.5f;
constexpr float JARAXXUS_INTRO_STAND = 3.0f;               // main tank from him in the intro
constexpr float JARAXXUS_SCAN_RADIUS = 200.0f;
constexpr uint32 JARAXXUS_SCAN_MS = 200;

enum class JaraxxusFlameRole : uint8
{
    None,
    Carrier,
    Dodge
};

// Every reader below gives its empty answer off map 649.

// Alive and attackable, so null through the intro
Unit* GetJaraxxus(PlayerbotAI* botAI);
// Alive, NON_ATTACKABLE and out of combat, i.e. standing through the Fizzlebang scene
Unit* GetJaraxxusInIntro(PlayerbotAI* botAI);

bool JaraxxusHasNetherPower(Unit* jaraxxus);
// Stack count of whichever Nether Power id he carries, 0 without him. Writes jaraxxus.netherpower
// on every call, so call it before any early out that would leave the probe stale.
uint32 JaraxxusNetherPowerStacks(PlayerbotAI* botAI);
// Mage always, shaman or priest when not healing. A healing one only when no other remover is alive
// and nobody carries Incinerate Flesh.
bool JaraxxusIsNetherPowerRemover(Player* bot);

// Incinerate Flesh heal absorb on the unit, under the map difficulty's id
bool HasIncinerateFlesh(Unit* unit);
// Lowest guid alive group member carrying it
Unit* GetIncinerateFleshTarget(PlayerbotAI* botAI);

// Legion Flame's 2 s warning or its 6 s trail, either one means the flames follow this unit
bool IsLegionFlameCarrier(Unit* unit);
bool HasMistressKiss(Unit* unit);

// Nether Portal, Infernal Volcano, Mistress of Pain or Felflame Infernal
bool IsJaraxxusAdd(Unit* unit);
// A Mistress, an Infernal, or a selectable (heroic) portal or volcano
bool JaraxxusAnyAddAlive(PlayerbotAI* botAI);
// Kill order, lowest guid first within a kind: selectable portal, selectable volcano, Mistress,
// Infernal. Writes jaraxxus.focus, empty when none.
Unit* GetJaraxxusFocusAdd(PlayerbotAI* botAI);
// Index 0 takes Mistresses, index 1 Infernals, and each falls back to the other's kind (index 0 only
// with no living index 1 tank). Writes jaraxxus.addtank for the bot, erased when null.
Unit* GetJaraxxusAssistTankAdd(PlayerbotAI* botAI, uint8 index);

bool JaraxxusIsFelFireballCasting(Unit* boss);
// First interrupt the bot can cast on him right now, can pay for, and reaches from where it stands.
// Null for a bot mid-cast or without a bot AI.
char const* JaraxxusReadyInterrupt(Player* bot, Unit* boss);
// Fel Fireball being cast, the bot ready, and no living group member with a lower guid ready. Writes
// jaraxxus.interrupter on every call, so call it every tick he lives and the probe drops with the cast.
bool JaraxxusIsFelFireballInterrupter(Player* bot, Unit* boss);

// Leaves the bot in UNIT_STATE_CASTING: a cast time, or a channel unless withChannels is false.
// Unknown names answer false.
bool JaraxxusSpellHasCastTime(PlayerbotAI* botAI, std::string const& spell, bool withChannels = true);

// Group bots mid-cast while kissed or carrying Legion Flame. A casting bot runs no triggers, so
// another bot's tick has to break the cast for it.
std::vector<Player*> GetJaraxxusPinnedCasters(Player* bot);

JaraxxusFlameRole GetJaraxxusFlameRole(Player* bot);
// Bearing from the boss (ARENA_CENTER without him) to the bot, so the trail leads away from him
float GetLegionFlameCarrierHeading(Player* bot);
// One carrier leg or one dodge. Writes jaraxxus.flame: carrier, relaxed (flames only), tank (a carrier
// holding him, flames only), dodge or none.
bool DeriveLegionFlameSpot(Player* bot, JaraxxusFlameRole role, float heading, Position& spot);
bool IsLegionFlameSpotClear(PlayerbotAI* botAI, Position const& spot);
// The straight walk from the bot to `to` comes nearer a flame than JARAXXUS_LEGION_FLAME_CLEAR, or
// nearer than the bot already stands to one inside that
bool LegionFlameCrossesPath(Player* bot, Position const& to);
// A spot at most maxStep away, clear of every flame and walked to without crossing one, nearest anchor
// on the first ring that has one
bool DeriveLegionFlameDetour(Player* bot, Position const& anchor, float maxStep, Position& spot);
// Any Legion Flame on the floor
bool JaraxxusAnyLegionFlame(PlayerbotAI* botAI);

// Bots whose rti the Jaraxxus actions set, so only those get it put back once the adds are dead
void JaraxxusClaimRti(Player* bot);
bool JaraxxusOwnsRti(Player* bot);
void JaraxxusReleaseRti(Player* bot);

}

#endif
