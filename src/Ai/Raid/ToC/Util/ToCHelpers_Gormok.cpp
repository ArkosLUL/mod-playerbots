#include "ToCHelpers_Gormok.h"
#include "SpellMgr.h"
#include "Unit.h"

namespace TrialOfTheCrusaderHelpers
{

uint32 GetGormokImpaleStacks(Unit* unit)
{
    return unit ? unit->GetAuraCount(sSpellMgr->GetSpellIdForDifficulty(SPELL_IMPALE, unit)) : 0;
}

}
