#include "ToCHelpers_Jormungars.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "SpellMgr.h"
#include "Unit.h"

using namespace EncounterHelpers;

namespace TrialOfTheCrusaderHelpers
{

Unit* GetWormCastingSweep(PlayerbotAI* botAI)
{
    uint32 const sweep = sSpellMgr->GetSpellIdForDifficulty(SPELL_SWEEP, botAI->GetBot());
    for (uint32 const entry : { static_cast<uint32>(ToCNpcs::NPC_ACIDMAW),
                                static_cast<uint32>(ToCNpcs::NPC_DREADSCALE) })
    {
        Unit* worm = GetFirstAliveUnitByEntry(botAI, entry);
        if (worm && worm->FindCurrentSpellBySpellId(sweep))
            return worm;
    }

    return nullptr;
}

bool IsWormMobile(Unit* worm)
{
    if (!worm)
        return false;

    uint32 const displayId = worm->GetDisplayId();
    return displayId == static_cast<uint32>(ToCDisplayIds::MODEL_ACIDMAW_MOBILE) ||
           displayId == static_cast<uint32>(ToCDisplayIds::MODEL_DREADSCALE_MOBILE);
}

}
