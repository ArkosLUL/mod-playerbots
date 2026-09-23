#include "ToCActions_Gormok.h"
#include "ToCData.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Unit.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

bool GormokMainTankHoldBossAction::Execute(Event /*event*/)
{
    Unit* gormok = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_GORMOK));
    if (!gormok)
        return false;

    MarkTargetWithSkull(bot, gormok);
    SetRtiTarget(botAI, "skull", gormok);

    if (AI_VALUE(Unit*, "current target") != gormok)
        return Attack(gormok);

    // Keep the boss near the centre of the arena so ranged can spread and melee have room
    return DragBossToAnchor(gormok, ARENA_CENTER);
}

bool GormokFocusSnoboldAction::Execute(Event /*event*/)
{
    Unit* snobold = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_SNOBOLD_VASSAL));
    if (!snobold)
        return false;

    MarkTargetWithCross(bot, snobold);

    if (AI_VALUE(Unit*, "current target") != snobold)
        return Attack(snobold);

    return false;
}

bool GormokTankSwapTauntAction::Execute(Event /*event*/)
{
    Unit* gormok = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_GORMOK));
    if (!gormok)
        return false;

    MarkTargetWithSkull(bot, gormok);
    SetRtiTarget(botAI, "skull", gormok);

    // Taunt to pull Gormok off the overloaded tank; the Impale bleed then decays on the old tank before
    // it stacks to a lethal amount.
    if (CastClassTaunt(botAI, gormok))
        return true;

    // Taunt unavailable / on cooldown: at least commit melee onto the boss so threat keeps building.
    if (AI_VALUE(Unit*, "current target") != gormok)
        return Attack(gormok);

    return false;
}
