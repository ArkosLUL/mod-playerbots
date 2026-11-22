#ifndef PLAYERBOTS_RAID_TOCHELPERS_STAGING_H
#define PLAYERBOTS_RAID_TOCHELPERS_STAGING_H

#include "Define.h"
#include "ObjectDefines.h"
#include "Position.h"
#include "ToCEncounterGate.h"

class Player;
class PlayerbotAI;

namespace TrialOfTheCrusaderHelpers
{

enum class ToCStagingBoss : uint8
{
    None,
    Gormok,
    Worms,
    Icehowl,
    Jaraxxus,
    Champions,
    Twins,
    Anubarak
};

inline constexpr float STAGING_PARK = 1.0f;
inline constexpr float STAGING_UNPARK = 2.5f;
inline constexpr float STAGING_STAND_MOVED = 0.5f;
inline constexpr float STAGING_FLOOR_TOLERANCE = 3.0f;
inline constexpr float STAGING_DUTY_STAND = 3.0f;
inline constexpr float STAGING_RELEASE_MARGIN = 5.0f;
inline constexpr uint32 STAGING_DEAL_MS = 1000;
inline constexpr float STAGING_3D_GAP = 0.5f;

constexpr float ReleaseClearance(float bossReach)
{
    return STAGING_DUTY_STAND + STAGING_RELEASE_MARGIN + bossReach + DEFAULT_COMBAT_REACH + STAGING_3D_GAP;
}

static_assert(STAGING_PARK < STAGING_UNPARK, "a walk has to end inside its own trigger");

ToCStagingBoss GetToCStagedArrival(PlayerbotAI* botAI);
ToCStagingBoss GetToCStagingBoss(PlayerbotAI* botAI);

struct ToCStagingRead
{
    bool held = false;
    bool walk = false;
    bool duty = false;
    Position spot;
};

ToCStagingRead GetToCStagingRead(PlayerbotAI* botAI);
void UpdateToCStagingPark(PlayerbotAI* botAI);

bool IsToCStagingHeld(PlayerbotAI* botAI);
bool IsToCStagingRestHeld(PlayerbotAI* botAI);

Position GetFactionChampionsLandingCentre(Player* bot);

Position GetFactionChampionsChainPost(Player* bot, uint32 post, bool tank);

}

#endif
