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
constexpr float STAND_TRIGGER = 3.0f;
}  // namespace

// While our breakable CC holds its champion, until the holder has stopped swinging at it
bool FactionChampionsHoldTrigger::IsActive()
{
    Unit* champion = FactionChampionsHoldOf(botAI).champion;
    if (!champion)
        return false;

    if (FactionChampionsCcProtected(botAI, champion))
    {
        Pet* pet = bot->GetPet();
        return bot->GetVictim() || !bot->GetTarget().IsEmpty() || AI_VALUE(Unit*, "current target") ||
               (pet && pet->GetVictim());
    }

    return AI_VALUE(Unit*, "current target") != champion || ToCPetOffPick(botAI, champion);
}

bool FactionChampionsHoldControlTrigger::IsActive()
{
    uint32 spellId = 0;
    return FactionChampionsHoldControl(botAI, spellId) != nullptr;
}

// Not inside a Bladestorm or Hellfire, the dodge walks out first
bool FactionChampionsCycloneStandTrigger::IsActive()
{
    Position spot;
    return FactionChampionsCycloneStand(botAI, spot) && botAI->CanMove() && !FactionChampionsInAoe(botAI) &&
           bot->GetExactDist2d(&spot) > STAND_TRIGGER;
}

bool FactionChampionsStepOffTrigger::IsActive()
{
    Position spot;
    return botAI->CanMove() && !FactionChampionsInAoe(botAI) && FactionChampionsStepOff(botAI, spot);
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

bool FactionChampionsEscapeTrigger::IsActive()
{
    FactionChampionsEscapeCast cast;
    return FactionChampionsEscape(botAI, cast);
}

bool FactionChampionsDemonicCircleTrigger::IsActive() { return FactionChampionsCircleDue(botAI); }

void AddToCFactionChampionsBallTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("faction champions opener", {
        NextAction("faction champions opener", ACTION_RAID + 4.4f) }));

    // instants, cast before the Nitro run they free up
    triggers.push_back(new TriggerNode("faction champions escape", {
        NextAction("faction champions escape", ACTION_RAID + 2.6f) }));

    triggers.push_back(new TriggerNode("faction champions chased", {
        NextAction("faction champions chased", ACTION_RAID + 2.5f) }));

    triggers.push_back(new TriggerNode("faction champions step off", {
        NextAction("faction champions step off", ACTION_RAID + 2.4f) }));

    triggers.push_back(new TriggerNode("faction champions hold control", {
        NextAction("faction champions hold control", ACTION_RAID + 2.2f) }));

    triggers.push_back(new TriggerNode("faction champions hold", {
        NextAction("faction champions hold", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("faction champions cyclone stand", {
        NextAction("faction champions cyclone stand", ACTION_RAID + 1.9f) }));

    // pre-pull, above the staging walk, which returns false once parked
    triggers.push_back(new TriggerNode("faction champions demonic circle", {
        NextAction("faction champions demonic circle", ACTION_RAID + 1) }));
}
