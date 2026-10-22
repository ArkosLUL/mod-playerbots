#include "ToCActions_Opener.h"

#include "Player.h"
#include "Playerbots.h"
#include "ToCHelpers_Opener.h"

using namespace TrialOfTheCrusaderHelpers;

bool ToCOpenerStopSwingAction::Execute(Event /*event*/) { return bot->AttackStop(); }

bool ToCOpenerStopSwingAction::isUseful()
{
    return bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING) && IsToCOpenerHeld(botAI, bot->GetVictim());
}

Player* ToCOpenerRedirectAction::GetRedirectTank() { return GetToCOpenerRedirectTank(botAI); }
