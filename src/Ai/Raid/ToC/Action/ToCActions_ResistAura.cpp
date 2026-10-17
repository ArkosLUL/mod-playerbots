#include "ToCActions_ResistAura.h"
#include "ToCHelpers_ResistAura.h"
#include "Playerbots.h"

using namespace TrialOfTheCrusaderHelpers;

bool ToCRaiseResistAuraAction::Execute(Event /*event*/)
{
    char const* aura = ToCResistAuraName(GetToCResistDuty(botAI));
    return aura && botAI->DoSpecificAction(aura, Event(), true);
}

// A queued basket can pop after the duty moved on
bool ToCRaiseResistAuraAction::isUseful() { return ToCResistAuraMissing(botAI); }
