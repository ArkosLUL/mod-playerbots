#include "ToCTriggers_FactionChampionsBall.h"

#include "Playerbots.h"
#include "Strategy.h"
#include "ToCHelpers_FactionChampionsBall.h"
#include "ToCHelpers_FactionChampionsDefence.h"
#include "ToCHelpers_Shared.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
// The champions on him move with him, so a few steps of slack before he walks back
constexpr float PACK_TANK_LEASH = 8.0f;
constexpr float STAND_TRIGGER = 3.0f;
}  // namespace

bool FactionChampionsPackTankReturnTrigger::IsActive()
{
    Position anchor;
    return FactionChampionsPackTankHeld(botAI, anchor) && botAI->CanMove() &&
           bot->GetExactDist2d(&anchor) > PACK_TANK_LEASH;
}

bool FactionChampionsPackTankAttackTrigger::IsActive()
{
    Unit* target = FactionChampionsPackTankTarget(botAI);
    return target && (AI_VALUE(Unit*, "current target") != target || ToCPetOffPick(botAI, target));
}

// Not inside a Bladestorm or Hellfire, the dodge walks out first
bool FactionChampionsBallStandTrigger::IsActive()
{
    Position spot;
    return FactionChampionsBallStand(botAI, spot) && botAI->CanMove() && !FactionChampionsInAoe(botAI) &&
           bot->GetExactDist2d(&spot) > STAND_TRIGGER;
}

bool FactionChampionsOpenerTrigger::IsActive()
{
    uint32 spellId = 0;
    Position spot;
    return FactionChampionsOpener(botAI, spellId, spot);
}

void AddToCFactionChampionsBallTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("faction champions opener", {
        NextAction("faction champions opener", ACTION_RAID + 4.4f) }));

    triggers.push_back(new TriggerNode("faction champions pack tank return", {
        NextAction("faction champions pack tank return", ACTION_RAID + 2.1f) }));

    triggers.push_back(new TriggerNode("faction champions pack tank attack", {
        NextAction("faction champions pack tank attack", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("faction champions ball stand", {
        NextAction("faction champions ball stand", ACTION_RAID + 1.9f) }));
}
