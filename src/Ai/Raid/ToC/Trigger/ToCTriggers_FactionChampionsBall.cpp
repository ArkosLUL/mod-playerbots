#include "ToCTriggers_FactionChampionsBall.h"

#include "Playerbots.h"
#include "Strategy.h"
#include "ToCHelpers_FactionChampions.h"
#include "ToCHelpers_FactionChampionsBall.h"
#include "ToCHelpers_FactionChampionsDefence.h"
#include "ToCHelpers_Shared.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
// Past the walk's own arrival, so a holder at its spot doesn't flip between walking and swinging
constexpr float HOLD_WALK_TRIGGER = 5.0f;
constexpr float STAND_TRIGGER = 3.0f;
}  // namespace

bool FactionChampionsHoldTrigger::IsActive()
{
    Unit* champion = FactionChampionsHoldOf(botAI).champion;
    return champion && (AI_VALUE(Unit*, "current target") != champion || ToCPetOffPick(botAI, champion));
}

bool FactionChampionsHoldWalkTrigger::IsActive()
{
    Position spot;
    return FactionChampionsHoldWalk(botAI, spot) && botAI->CanMove() && bot->GetExactDist2d(&spot) > HOLD_WALK_TRIGGER;
}

bool FactionChampionsHoldControlTrigger::IsActive()
{
    uint32 spellId = 0;
    return FactionChampionsHoldControl(botAI, spellId) != nullptr;
}

// Not inside a Bladestorm or Hellfire, the dodge walks out first
bool FactionChampionsBallStandTrigger::IsActive()
{
    Position spot;
    return FactionChampionsBallStand(botAI, spot) && botAI->CanMove() && !FactionChampionsInAoe(botAI) &&
           bot->GetExactDist2d(&spot) > STAND_TRIGGER;
}

bool FactionChampionsChasedTrigger::IsActive()
{
    Position spot;
    return botAI->CanMove() && FactionChampionsChased(botAI, spot);
}

bool FactionChampionsOpenerTrigger::IsActive()
{
    uint32 spellId = 0;
    return FactionChampionsOpener(botAI, spellId) != nullptr;
}

void AddToCFactionChampionsBallTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("faction champions opener", {
        NextAction("faction champions opener", ACTION_RAID + 4.4f) }));

    triggers.push_back(new TriggerNode("faction champions chased", {
        NextAction("faction champions chased", ACTION_RAID + 2.5f) }));

    triggers.push_back(new TriggerNode("faction champions hold control", {
        NextAction("faction champions hold control", ACTION_RAID + 2.2f) }));

    triggers.push_back(new TriggerNode("faction champions hold walk", {
        NextAction("faction champions hold walk", ACTION_RAID + 2.1f) }));

    triggers.push_back(new TriggerNode("faction champions hold", {
        NextAction("faction champions hold", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("faction champions ball stand", {
        NextAction("faction champions ball stand", ACTION_RAID + 1.9f) }));
}
