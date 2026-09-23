#include "ToCHelpers_Gormok.h"
#include "Unit.h"

namespace TrialOfTheCrusaderHelpers
{

uint32 GetGormokImpaleStacks(Unit* unit)
{
    return unit ? unit->GetAuraCount(static_cast<uint32>(ToCSpells::SPELL_IMPALE)) : 0;
}

}
