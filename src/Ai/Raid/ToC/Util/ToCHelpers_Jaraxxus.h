#ifndef PLAYERBOTS_RAID_TOCHELPERS_JARAXXUS_H
#define PLAYERBOTS_RAID_TOCHELPERS_JARAXXUS_H

#include <string>
#include <vector>

#include "PlayerbotAI.h"
#include "Position.h"
#include "ToCData.h"
#include "ToCHelpers_Defensive.h"

namespace TrialOfTheCrusaderHelpers
{

constexpr uint32 SPELL_LEGION_FLAME_TRAIL = 66199;   // row 66199/68126/68127/68128
constexpr uint32 SPELL_MISTRESS_KISS = 66334;        // row 66334/67905/67906/67907
constexpr uint32 SPELL_BURNING_INFERNO = 66242;      // row 66242/67059/67060/67061
// The portal's own aura, whose periodic tick summons a Mistress: one at 8 s on normal, every 6 s on heroic
constexpr uint32 SPELL_NETHER_PORTAL_SPAWN = 66263;  // row 66263/67103/67104/67105

// Incinerate Flesh's last tick casts Burning Inferno, whose own ticks start a second later
constexpr uint32 JARAXXUS_INFERNO_FIRST_TICK_MS = 1000;
constexpr uint32 JARAXXUS_INFERNO_TICKS = 5;
// An Incinerate Flesh this close to running out with absorb left is a Burning Inferno coming
constexpr uint32 JARAXXUS_INFERNO_LEAD_MS = 2000;
// A carrier under this share of its health counts the rest of its trail as a hit
constexpr float JARAXXUS_CARRIER_DEFENSIVE_PCT = 50.0f;
// A carrier's next spot stays this close to a living healer
constexpr float JARAXXUS_CARRIER_HEALER_REACH = 30.0f;

// A Mistress' arrival hits everyone within 10 yd of her portal (66264 effect 1, a creature's area, centre
// to centre). Melee step out from this, to this, from this long before it.
constexpr float JARAXXUS_PORTAL_TRIGGER = 12.0f;
constexpr float JARAXXUS_PORTAL_CLEAR = 14.0f;
constexpr uint32 JARAXXUS_PORTAL_SPAWN_LEAD_MS = 1000;

// DPS bots stop on him at this share of his tank victim's threat, resume under the release share
constexpr float JARAXXUS_THREAT_CAP = 0.9f;
constexpr float JARAXXUS_THREAT_RELEASE = 0.8f;

constexpr float JARAXXUS_LEGION_FLAME_RADIUS = 3.0f;       // 66877 row, centre to centre
constexpr float JARAXXUS_LEGION_FLAME_TRIGGER = 4.0f;      // dodge starts inside this
constexpr float JARAXXUS_LEGION_FLAME_CLEAR = 6.0f;        // a spot counts clear of a flame past this
constexpr float JARAXXUS_LEGION_FLAME_SEARCH = 15.0f;      // sweep maxRadius
constexpr float JARAXXUS_FLAME_CARRIER_MIN_LEG = 6.0f;     // carrier travel floor per leg
constexpr float JARAXXUS_FLAME_CARRIER_RAID_CLEAR = 8.0f;  // carrier spot from other members
constexpr float JARAXXUS_FLAME_CARRIER_BOSS_CLEAR = 15.0f; // carrier spot from the boss
constexpr float JARAXXUS_FLAME_ARRIVE = 1.5f;
constexpr float JARAXXUS_SCAN_RADIUS = 200.0f;
constexpr uint32 JARAXXUS_SCAN_MS = 200;

enum class JaraxxusFlameRole : uint8
{
    None,
    Carrier,
    Dodge
};

// Every reader below gives its empty answer off map 649.

// Alive and attackable, so null through the intro and while a retry waits chained for the gossip
Unit* GetJaraxxus(PlayerbotAI* botAI);

bool JaraxxusHasNetherPower(Unit* jaraxxus);
// Stack count of whichever Nether Power id he carries, 0 without him. Writes jaraxxus.netherpower
// on every call, so call it before any early out that would leave the probe stale.
uint32 JaraxxusNetherPowerStacks(PlayerbotAI* botAI);
// Mage always, shaman or priest when not healing. A healing one only when no other remover is alive
// and nobody carries Incinerate Flesh.
bool JaraxxusIsNetherPowerRemover(Player* bot);

// Incinerate Flesh heal absorb on the unit, under the map difficulty's id
bool HasIncinerateFlesh(Unit* unit);
// Lowest guid alive group member carrying it. Writes jaraxxus.absorb, its absorb left, 0 with none.
Unit* GetIncinerateFleshTarget(PlayerbotAI* botAI);

// The Burning Inferno coming or running, as the raid takes it: from an Incinerate Flesh within
// JARAXXUS_INFERNO_LEAD_MS of running out with absorb left, or a Burning Inferno on a member. Nether
// Power's stacks count in the damage.
bool GetJaraxxusBurningInferno(PlayerbotAI* botAI, ToCPredictedHit& hit);
// The same hit as this bot takes it once its own Burning Inferno runs, else the raid's; and the rest of
// the trail on a Legion Flame carrier under JARAXXUS_CARRIER_DEFENSIVE_PCT.
bool GetJaraxxusPersonalHit(PlayerbotAI* botAI, ToCPredictedHit& hit);
// Before a predicted Burning Inferno lands: when the class nodes for the raid cooldowns are held
bool JaraxxusRaidCoverHeld(PlayerbotAI* botAI);
// A Burning Inferno coming or running, or the bot carrying Legion Flame: what Evasion and Deterrence
// would be spent on
bool JaraxxusMagicHitOnBot(PlayerbotAI* botAI);
// A group bot holding a raid cover inside its window, mid cast: it runs no triggers until the cast ends
bool JaraxxusRaidCoverPinned(Player* member);

// Legion Flame's 2 s warning or its 6 s trail, either one means the flames follow this unit
bool IsLegionFlameCarrier(Unit* unit);
bool HasMistressKiss(Unit* unit);

// Nether Portal, Infernal Volcano, Mistress of Pain or Felflame Infernal
bool IsJaraxxusAdd(Unit* unit);
// Kill order, lowest guid first within a kind: selectable portal, selectable volcano, Mistress,
// Infernal. Writes jaraxxus.focus, empty when none.
Unit* GetJaraxxusFocusAdd(PlayerbotAI* botAI);
// Index 0 takes Mistresses, index 1 Infernals, and each falls back to the other's kind (index 0 only
// with no living index 1 tank). Writes jaraxxus.addtank for the bot, erased when null.
Unit* GetJaraxxusAssistTankAdd(PlayerbotAI* botAI, uint8 index);

// For a melee DPS bot targeting it or near it: the heroic portal it keeps JARAXXUS_PORTAL_CLEAR from,
// from JARAXXUS_PORTAL_SPAWN_LEAD_MS before the portal's next Mistress until each of its Mistresses
// is on a tank or dead. Null otherwise. The hold rule and the walk out both read it. Writes
// jaraxxus.portal: hold or none.
Unit* GetJaraxxusPortalToClear(PlayerbotAI* botAI);
// A spot JARAXXUS_PORTAL_CLEAR from the portal and clear of flames, nearest him
bool DeriveJaraxxusPortalExit(Player* bot, Unit* portal, Position& spot);

// Latched per DPS bot: capped at JARAXXUS_THREAT_CAP of his tank victim's threat, freed under the
// release share. Nothing while his victim isn't a tank.
bool JaraxxusThreatCapped(Player* bot);

// The tank holds' trigger tests. The tank targeting rule reads the same calls, so it never zeroes
// tank assist for a tank no hold covers.
bool JaraxxusMainTankHolds(PlayerbotAI* botAI);
bool JaraxxusAssistTankHolds(PlayerbotAI* botAI, uint8 index);

// DPS bots only, while the gate is open (his adds outlive the kill): a player's skull on him or an
// add, else the kill-order add, else him once in combat. Writes jaraxxus.dps: skull, add, boss, none.
Unit* GetJaraxxusDpsTarget(PlayerbotAI* botAI);

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
// One carrier leg or one dodge. A carrier leg ends within JARAXXUS_CARRIER_HEALER_REACH of a living
// healer where any clear spot does. Writes jaraxxus.flame: carrier, relaxed (flames only), tank (a
// carrier holding him, flames only), far (no clear spot within a healer's reach), dodge or none.
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

}

#endif
