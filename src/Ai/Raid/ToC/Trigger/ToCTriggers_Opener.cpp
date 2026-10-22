#include "ToCTriggers_Opener.h"

#include "Player.h"
#include "Playerbots.h"
#include "Strategy.h"
#include "ToCHelpers_Opener.h"

using namespace TrialOfTheCrusaderHelpers;

bool ToCOpenerSwingTrigger::IsActive()
{
    return IsToCOpenerHolding(botAI) && bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING) &&
           IsToCOpenerHeld(botAI, bot->GetVictim());
}

bool ToCOpenerRedirectTrigger::IsActive()
{
    Player* tank = GetToCOpenerRedirectTank(botAI);
    char const* spell = bot->getClass() == CLASS_ROGUE ? "tricks of the trade" : "misdirection";
    return tank && botAI->CanCastSpell(spell, tank);
}

void AddToCOpenerTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    // Above the resistance aura: the swing lands on the next attack timer, not the next GCD
    triggers.push_back(new TriggerNode("toc opener swing", {
        NextAction("toc opener stop swing", ACTION_RAID + 2) }));
    triggers.push_back(new TriggerNode("toc opener redirect", {
        NextAction("toc opener redirect", ACTION_RAID + 1) }));
}
