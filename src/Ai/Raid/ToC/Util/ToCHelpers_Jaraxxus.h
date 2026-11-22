#ifndef PLAYERBOTS_RAID_TOCHELPERS_JARAXXUS_H
#define PLAYERBOTS_RAID_TOCHELPERS_JARAXXUS_H

#include <string>
#include <vector>

#include "PlayerbotAI.h"
#include "Position.h"
#include "RaidDefensive.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

inline constexpr uint32 SPELL_LEGION_FLAME_TRAIL = 66199;
inline constexpr uint32 SPELL_MISTRESS_KISS = 66334;
inline constexpr uint32 SPELL_BURNING_INFERNO = 66242;
inline constexpr uint32 SPELL_NETHER_PORTAL_SPAWN = 66263;

inline constexpr float JARAXXUS_COMBAT_REACH = 5.0f;

inline constexpr uint32 JARAXXUS_INFERNO_FIRST_TICK_MS = 1000;
inline constexpr uint32 JARAXXUS_INFERNO_TICKS = 5;
inline constexpr uint32 JARAXXUS_INFERNO_LEAD_MS = 2000;
inline constexpr float JARAXXUS_CARRIER_DEFENSIVE_PCT = 50.0f;
inline constexpr float JARAXXUS_CARRIER_HEALER_REACH = 30.0f;

inline constexpr float JARAXXUS_PORTAL_TRIGGER = 12.0f;
inline constexpr float JARAXXUS_PORTAL_CLEAR = 14.0f;
inline constexpr uint32 JARAXXUS_PORTAL_SPAWN_LEAD_MS = 1000;

inline constexpr float JARAXXUS_THREAT_CAP = 0.9f;
inline constexpr float JARAXXUS_THREAT_RELEASE = 0.8f;

inline constexpr float JARAXXUS_LEGION_FLAME_RADIUS = 3.0f;
inline constexpr float JARAXXUS_LEGION_FLAME_TRIGGER = 4.0f;
inline constexpr float JARAXXUS_LEGION_FLAME_CLEAR = 6.0f;
inline constexpr float JARAXXUS_LEGION_FLAME_SEARCH = 15.0f;
inline constexpr float JARAXXUS_FLAME_CARRIER_MIN_LEG = 6.0f;
inline constexpr float JARAXXUS_FLAME_CARRIER_RAID_CLEAR = 8.0f;
inline constexpr float JARAXXUS_FLAME_CARRIER_BOSS_CLEAR = 15.0f;
inline constexpr float JARAXXUS_FLAME_ARRIVE = 1.5f;
inline constexpr float JARAXXUS_FLAME_STAND_REACH = 10.0f;
inline constexpr float JARAXXUS_DODGE_REACH_MARGIN = 0.5f;
inline constexpr float JARAXXUS_SCAN_RADIUS = 200.0f;
inline constexpr uint32 JARAXXUS_SCAN_MS = 200;

enum class JaraxxusFlameRole : uint8
{
    None,
    Carrier,
    Dodge
};

Unit* GetJaraxxus(PlayerbotAI* botAI);

bool JaraxxusHasNetherPower(Unit* jaraxxus);
uint32 JaraxxusNetherPowerStacks(PlayerbotAI* botAI);
bool JaraxxusIsNetherPowerRemover(Player* bot);

bool HasIncinerateFlesh(Unit* unit);
Unit* GetIncinerateFleshTarget(PlayerbotAI* botAI);

bool GetJaraxxusBurningInferno(PlayerbotAI* botAI, RaidPredictedHit& hit);
bool GetJaraxxusPersonalHit(PlayerbotAI* botAI, RaidPredictedHit& hit);
bool JaraxxusRaidCoverHeld(PlayerbotAI* botAI);
bool JaraxxusMagicHitOnBot(PlayerbotAI* botAI);
bool JaraxxusRaidCoverPinned(Player* member);

bool IsLegionFlameCarrier(Unit* unit);
bool HasMistressKiss(Unit* unit);

bool IsJaraxxusAdd(Unit* unit);
Unit* GetJaraxxusFocusAdd(PlayerbotAI* botAI);
Unit* GetJaraxxusAssistTankAdd(PlayerbotAI* botAI, uint8 index);

Unit* GetJaraxxusPortalToClear(PlayerbotAI* botAI);
bool DeriveJaraxxusPortalExit(Player* bot, Unit* portal, Position& spot);

bool JaraxxusThreatCapped(Player* bot);

bool JaraxxusMainTankHolds(PlayerbotAI* botAI);
bool JaraxxusAssistTankHolds(PlayerbotAI* botAI, uint8 index);

Unit* GetJaraxxusDpsTarget(PlayerbotAI* botAI);

bool JaraxxusIsFelFireballCasting(Unit* boss);
char const* JaraxxusReadyInterrupt(Player* bot, Unit* boss);
bool JaraxxusIsFelFireballInterrupter(Player* bot, Unit* boss);

bool JaraxxusSpellHasCastTime(PlayerbotAI* botAI, std::string const& spell, bool withChannels = true);

std::vector<Player*> GetJaraxxusPinnedCasters(Player* bot);

JaraxxusFlameRole GetJaraxxusFlameRole(Player* bot);
float GetLegionFlameCarrierHeading(Player* bot);
bool DeriveLegionFlameSpot(Player* bot, JaraxxusFlameRole role, float heading, Position& spot);
bool IsLegionFlameSpotClear(PlayerbotAI* botAI, Position const& spot);
bool LegionFlameCrossesPath(Player* bot, Position const& to);
bool DeriveLegionFlameDetour(Player* bot, Position const& anchor, float maxStep, Position& spot);
bool JaraxxusAnyLegionFlame(PlayerbotAI* botAI);
bool JaraxxusFlameNear(PlayerbotAI* botAI);

}

#endif
