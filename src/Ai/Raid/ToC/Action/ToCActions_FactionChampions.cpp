#include "ToCActions_FactionChampions.h"
#include "ToCHelpers_FactionChampions.h"
#include "Playerbots.h"

using namespace TrialOfTheCrusaderHelpers;

// false, so the bot still gets its own action this tick
bool FactionChampionsMarkTargetsAction::Execute(Event /*event*/)
{
    FactionChampionsApplyMarks(botAI);
    return false;
}

bool FactionChampionsAntiFearAction::FearWindowActive() { return FactionChampionsFearWindowActive(botAI); }

bool FactionChampionsFocusPriorityAction::Execute(Event /*event*/)
{
    Unit* killTarget = FactionChampionsKillTarget(botAI);
    if (!killTarget || AI_VALUE(Unit*, "current target") == killTarget)
        return false;

    return Attack(killTarget);
}

// false, so the bot still gets its own action this tick
bool FactionChampionsSetCcIconAction::Execute(Event /*event*/)
{
    FactionChampionsApplyCcIcon(botAI);
    return false;
}

bool FactionChampionsCounterspellKillTargetAction::Execute(Event /*event*/)
{
    Unit* killTarget = FactionChampionsKillTarget(botAI);
    return killTarget && botAI->CastSpell("counterspell", killTarget);
}

// A queued basket can pop ticks after its trigger fired, once the heal is over
bool FactionChampionsCounterspellKillTargetAction::isUseful() { return FactionChampionsCounterspellDuty(botAI); }

// false, so the bot still gets its own action this tick
bool ToCRestoreRtiCcAction::Execute(Event /*event*/)
{
    FactionChampionsRestoreRtiCc(botAI);
    return false;
}
