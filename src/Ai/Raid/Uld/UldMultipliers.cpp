#include "UldMultipliers.h"

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PriestActions.h"
#include "UldBossHelper.h"

// Algalon the Observer
// Reserve Dispersion for the designated Big Bang soaker priest. Big Bang is unavoidable raid-wide
// damage; the soaker survives it via Dispersion (90% reduction). Blocking the normal low-mana /
// critical-health Dispersion casts keeps the cooldown up for every Big Bang.
float AlgalonMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CastDispersionAction*>(action))
        return 1.0f;

    Unit* boss = AI_VALUE2(Unit*, "find target", "algalon observer");
    if (!boss || !boss->IsAlive())
        return 1.0f;

    // Only the designated soaker priest reserves the cooldown; other priests disperse normally.
    if (GetAlgalonBigBangSoakerPriest(bot) != bot)
        return 1.0f;

    // During the actual Big Bang cast the soak action must be free to spend Dispersion.
    if (boss->HasUnitState(UNIT_STATE_CASTING) && boss->FindCurrentSpellBySpellId(SPELL_ALGALON_BIG_BANG))
        return 1.0f;

    // Otherwise block Dispersion so it is available for the next Big Bang.
    return 0.0f;
}
