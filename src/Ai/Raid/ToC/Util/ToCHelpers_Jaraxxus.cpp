#include "ToCHelpers_Jaraxxus.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Unit.h"

using namespace EncounterHelpers;

namespace TrialOfTheCrusaderHelpers
{

bool JaraxxusHasNetherPower(Unit* jaraxxus)
{
    if (!jaraxxus)
        return false;

    return jaraxxus->HasAura(static_cast<uint32>(ToCSpells::SPELL_NETHER_POWER_10N)) ||
           jaraxxus->HasAura(static_cast<uint32>(ToCSpells::SPELL_NETHER_POWER_10H)) ||
           jaraxxus->HasAura(static_cast<uint32>(ToCSpells::SPELL_NETHER_POWER_25N)) ||
           jaraxxus->HasAura(static_cast<uint32>(ToCSpells::SPELL_NETHER_POWER_25H));
}

Unit* GetPriorityJaraxxusAdd(PlayerbotAI* botAI)
{
    if (Unit* mistress = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_MISTRESS_OF_PAIN)))
        return mistress;

    return GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_FEL_INFERNAL));
}

Unit* GetSecondaryJaraxxusAdd(PlayerbotAI* botAI)
{
    Unit* mistress = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_MISTRESS_OF_PAIN));
    Unit* infernal = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_FEL_INFERNAL));

    // Only meaningful when both adds are up; the priority add (Mistress) is held elsewhere
    if (mistress && infernal)
        return infernal;

    return nullptr;
}

}
