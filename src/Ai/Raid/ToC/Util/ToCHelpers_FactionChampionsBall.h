#ifndef PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSBALL_H
#define PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSBALL_H

#include "Define.h"
#include "Position.h"

class Player;
class PlayerbotAI;
class Unit;

namespace TrialOfTheCrusaderHelpers
{

bool FactionChampionsCycloneStand(PlayerbotAI* botAI, Position& spot);

bool FactionChampionsStaysPut(PlayerbotAI* botAI);

bool FactionChampionsKitesFrom(Player* member, Unit* champion);

bool FactionChampionsStepOff(PlayerbotAI* botAI, Position& spot);

Unit* FactionChampionsOpener(PlayerbotAI* botAI, uint32& spellId);

bool FactionChampionsChased(PlayerbotAI* botAI, Position& spot);

bool FactionChampionsKite(PlayerbotAI* botAI, Position& spot);

bool FactionChampionsKiteHold(PlayerbotAI* botAI);

uint32 FactionChampionsSpeedBoost(PlayerbotAI* botAI);

inline constexpr uint32 SPELL_DEMONIC_CIRCLE_SUMMON = 48018;

struct FactionChampionsEscapeCast
{
    uint32 spellId = 0;
    Unit* target = nullptr;
    bool face = false;
    float facing = 0.0f;
};

bool FactionChampionsEscape(PlayerbotAI* botAI, FactionChampionsEscapeCast& cast);

bool FactionChampionsCircleDue(PlayerbotAI* botAI);

}

#endif
