#ifndef PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSDEFENCE_H
#define PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSDEFENCE_H

#include <array>
#include <vector>

#include "EncounterHelpers.h"
#include "PlayerbotAI.h"
#include "Position.h"
#include "ToCData.h"

struct RaidPredictedHit;

namespace TrialOfTheCrusaderHelpers
{

inline constexpr uint32 SPELL_UNSTABLE_AFFLICTION = 65812;
inline constexpr uint32 SPELL_HELLFIRE = 65816;
inline constexpr uint32 SPELL_HAND_OF_FREEDOM = 68757;
inline constexpr uint32 SPELL_POWER_WORD_SHIELD = 66099;
inline constexpr uint32 SPELL_RENEW = 66177;
inline constexpr uint32 SPELL_RIPTIDE = 66053;
inline constexpr uint32 SPELL_REJUVENATION = 66065;
inline constexpr uint32 SPELL_LIFEBLOOM = 66093;
inline constexpr uint32 SPELL_REGROWTH = 66067;

inline constexpr std::array<uint32, 6> DEATH_COIL_RANKS = {47860, 47859, 27223, 17926, 17925, 6789};

inline constexpr uint32 SPELL_POLYMORPH = 65801;
inline constexpr uint32 SPELL_FEAR = 65809;
inline constexpr uint32 SPELL_PSYCHIC_SCREAM = 65543;
inline constexpr uint32 SPELL_REPENTANCE = 66008;
inline constexpr uint32 SPELL_HAMMER_OF_JUSTICE_HOLY = 66613;
inline constexpr uint32 SPELL_HAMMER_OF_JUSTICE_RET = 66007;
inline constexpr uint32 SPELL_SILENCE = 65542;
inline constexpr uint32 SPELL_STRANGULATE = 66018;
inline constexpr uint32 SPELL_HEX = 66054;
inline constexpr uint32 SPELL_WYVERN_STING = 65877;
inline constexpr uint32 SPELL_BLADESTORM = 65947;
inline constexpr uint32 SPELL_HAND_OF_PROTECTION = 66009;
inline constexpr uint32 SPELL_DIVINE_SHIELD = 66010;
inline constexpr uint32 SPELL_EARTH_SHIELD = 66063;
inline constexpr uint32 SPELL_HEROISM = 65983;
inline constexpr uint32 SPELL_BLOODLUST = 65980;
inline constexpr uint32 SPELL_AVENGING_WRATH = 66011;
inline constexpr uint32 SPELL_BARKSKIN = 65860;

inline constexpr uint32 SPELL_MASS_DISPEL = 32375;

inline constexpr uint32 NPC_GROUNDING_TOTEM = 5925;
inline constexpr uint32 NPC_WINDFURY_TOTEM = 6112;
inline constexpr uint32 NPC_TREMOR_TOTEM = 5913;

Unit* FactionChampionsDispelCcTarget(PlayerbotAI* botAI, char const*& spell);

Unit* FactionChampionsMassDispelTarget(PlayerbotAI* botAI);

Unit* FactionChampionsPurgeTarget(PlayerbotAI* botAI, char const*& spell);

Unit* FactionChampionsPhysicalSwitchTarget(PlayerbotAI* botAI);

Unit* FactionChampionsTotemTarget(PlayerbotAI* botAI);

Unit* FactionChampionsChannelInterruptTarget(PlayerbotAI* botAI, char const*& spell);

bool FactionChampionsInAoe(PlayerbotAI* botAI);
std::vector<EncounterHelpers::HazardCircle> FactionChampionsAoeClearances(PlayerbotAI* botAI);
bool FactionChampionsReachIntoAoe(PlayerbotAI* botAI);

bool FactionChampionsDispelBackfires(PlayerbotAI* botAI, Unit* target);

Unit* FactionChampionsGripTarget(PlayerbotAI* botAI, Position& stand);

void FactionChampionsTakePullRequest(PlayerbotAI* botAI);

bool FactionChampionsRequestChain(PlayerbotAI* botAI, Player* requester);

bool FactionChampionsChainLink(PlayerbotAI* botAI);

bool FactionChampionsChainGripDue(PlayerbotAI* botAI);

Unit* FactionChampionsChainGripTarget(PlayerbotAI* botAI, Position& stand, bool& walkOnly);

void FactionChampionsTrackChainGrip(PlayerbotAI* botAI, Unit* target);

ObjectGuid FactionChampionsChainTarget(Player* bot);

bool FactionChampionsGripPending(PlayerbotAI* botAI);

bool FactionChampionsSpellHoldsTarget(uint32 spellId);

bool FactionChampionsGripGuarded(PlayerbotAI* botAI, Unit* champion);

bool FactionChampionsGripGuardActive(PlayerbotAI* botAI);

bool FactionChampionsChainGuarded(Player* bot, Unit* champion);

uint32 FactionChampionsPetAutocastToggle(PlayerbotAI* botAI, bool& enable);

Unit* FactionChampionsSnareTarget(PlayerbotAI* botAI, uint32& spellId);

Unit* FactionChampionsPeelTarget(PlayerbotAI* botAI, uint32& spellId);

bool FactionChampionsBreakablePeelAt(PlayerbotAI* botAI, Unit* champion);

Unit* FactionChampionsProtectTarget(PlayerbotAI* botAI, uint32& spellId);

bool FactionChampionsCorpseCrowded(PlayerbotAI* botAI);

Unit* FactionChampionsBattleResTarget(PlayerbotAI* botAI, uint32& spellId);

bool FactionChampionsPersonalHit(PlayerbotAI* botAI, RaidPredictedHit& hit);

Position FactionChampionsAoeDodgeSpot(PlayerbotAI* botAI);

ObjectGuid FactionChampionsNextFocus(PlayerbotAI* botAI);

Unit* FactionChampionsIceChains(PlayerbotAI* botAI, uint32& spellId);

bool FactionChampionsIceReach(PlayerbotAI* botAI, Position& stand);

Unit* FactionChampionsWallGrip(PlayerbotAI* botAI, Position& stand, uint32& spellId, bool* gripped = nullptr);

void FactionChampionsTrackWallGrip(PlayerbotAI* botAI, Unit* champion);

Unit* FactionChampionsFreedomTarget(PlayerbotAI* botAI, uint32& spellId);

}

#endif
