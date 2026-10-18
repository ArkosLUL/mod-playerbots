#ifndef PLAYERBOTS_RAID_TOCHELPERS_STAGING_H
#define PLAYERBOTS_RAID_TOCHELPERS_STAGING_H

#include "Define.h"
#include "Position.h"
#include "ToCEncounterGate.h"

class Player;
class PlayerbotAI;

// Pre-pull stands: every boss turns hostile on the same script point each pull, so bots wait on a
// dealt stand around it and the duty tank stands nearest. Nothing resolves off map 649.
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

// A parked bot walks back once pushed past UNPARK
constexpr float STAGING_PARK = 1.0f;
constexpr float STAGING_UNPARK = 2.5f;
// The deal moving a stand further than this drops the park and validates the new spot
constexpr float STAGING_STAND_MOVED = 0.5f;
// ValidateFloorPoint's answer is kept only this close to the raw stand
constexpr float STAGING_FLOOR_TOLERANCE = 3.0f;
// A forced attack takes the nearest unit: the duty tank stands this far from the release point and
// everyone else at least the margin further, which covers a 1 yd park either side and a pet 3 yd off
constexpr float STAGING_DUTY_STAND = 3.0f;
constexpr float STAGING_RELEASE_MARGIN = 5.0f;
constexpr uint32 STAGING_DEAL_MS = 1000;

static_assert(STAGING_PARK < STAGING_UNPARK, "a walk has to end inside its own trigger");

// The raid-wide half of the window, per instance per ms: Barrett out of gossip after it was seen at
// this stage (or a Jaraxxus retry), nothing live, and the next arrival by the script's guid slots.
// Stage 9 is Anub'arak while he isn't live.
ToCStagingBoss GetToCStagedArrival(PlayerbotAI* botAI);
// That, while the bot is alive, out of combat and in the arrival's room together with its master,
// else the group leader
ToCStagingBoss GetToCStagingBoss(PlayerbotAI* botAI);
ToCEncounter ToCStagingEncounter(PlayerbotAI* botAI);

struct ToCStagingRead
{
    bool held = false;  // in the bot's window with a dealt stand
    bool walk = false;  // not parked on it
    bool duty = false;  // a tank stand a forced attack walks onto
    Position spot;      // the stand, floor-validated
};

// Notes toc.stage, and for 2 s after the arrival closes every bot's last label again, since the trace
// opens at the engage. Only a stand the deal moved changes the park latch here, dropping it; parking
// is the trigger's write.
ToCStagingRead GetToCStagingRead(PlayerbotAI* botAI);
// Written by the trigger on every check: parked while it declines
void SetToCStagingParked(PlayerbotAI* botAI, bool parked);

// For the rules: no note and no park write
bool IsToCStagingHeld(PlayerbotAI* botAI);
// Held, and walking or on a duty stand
bool IsToCStagingRestHeld(PlayerbotAI* botAI);

// The middle of the Faction Champions' landing slots for the bot's faction, where the pack lands
Position GetFactionChampionsLandingCentre(Player* bot);

}

#endif
