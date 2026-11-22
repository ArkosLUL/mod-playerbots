#include "ToCActions_FactionChampions.h"
#include "Pet.h"
#include "ToCHelpers_FactionChampions.h"
#include "ToCHelpers_FactionChampionsDefence.h"
#include "Playerbots.h"
#include "SpellMgr.h"

using namespace TrialOfTheCrusaderHelpers;

bool FactionChampionsAntiFearAction::FearWindowActive() { return FactionChampionsFearWindowActive(botAI); }

bool FactionChampionsFocusPriorityAction::Execute(Event /*event*/)
{
    return AttackDirect(FactionChampionsKillTarget(botAI));
}

bool FactionChampionsCounterspellKillTargetAction::Execute(Event /*event*/)
{
    Unit* killTarget = FactionChampionsKillTarget(botAI);
    return killTarget && botAI->CastSpell("counterspell", killTarget);
}

bool FactionChampionsCounterspellKillTargetAction::isUseful() { return FactionChampionsCounterspellDuty(botAI); }

bool FactionChampionsPetAutocastAction::Execute(Event /*event*/)
{
    bool enable = false;
    uint32 const spellId = FactionChampionsPetAutocastToggle(botAI, enable);
    Pet* pet = bot->GetPet();
    if (spellId && pet)
        if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId))
            pet->ToggleAutocast(info, enable);

    return false;
}
