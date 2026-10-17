#include "ToCActions_FactionChampions.h"
#include "Pet.h"
#include "ToCHelpers_FactionChampions.h"
#include "Playerbots.h"
#include "SpellMgr.h"

using namespace TrialOfTheCrusaderHelpers;

// false, so the bot still gets its own action this tick
bool FactionChampionsClearCcIconsAction::Execute(Event /*event*/)
{
    FactionChampionsClearStaleCcIcons(botAI);
    return false;
}

bool FactionChampionsAntiFearAction::FearWindowActive() { return FactionChampionsFearWindowActive(botAI); }

bool FactionChampionsFocusPriorityAction::Execute(Event /*event*/)
{
    return AttackDirect(FactionChampionsKillTarget(botAI));
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

// The non-combat toggle pet spell turns it back on after the fight. False, so the bot still gets its own
// action this tick.
bool FactionChampionsPetTauntOffAction::Execute(Event /*event*/)
{
    uint32 const spellId = FactionChampionsPetTauntAutocast(botAI);
    Pet* pet = bot->GetPet();
    if (spellId && pet)
        if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId))
            pet->ToggleAutocast(info, false);

    return false;
}
