#include "ToCActions_Jaraxxus.h"
#include "ToCData.h"
#include "ToCHelpers_Jaraxxus.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Unit.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

bool JaraxxusMainTankHoldBossAction::Execute(Event /*event*/)
{
    Unit* jaraxxus = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_JARAXXUS));
    if (!jaraxxus)
        return false;

    MarkTargetWithSkull(bot, jaraxxus);
    SetRtiTarget(botAI, "skull", jaraxxus);

    if (AI_VALUE(Unit*, "current target") != jaraxxus)
        return Attack(jaraxxus);

    // Keep the boss anchored near the centre so ranged can spread and adds stay grouped
    return DragBossToAnchor(jaraxxus, ARENA_CENTER);
}

bool JaraxxusAssistTankHoldAddAction::Execute(Event /*event*/)
{
    Unit* add = GetPriorityJaraxxusAdd(botAI);
    if (!add)
        return false;

    MarkTargetWithCross(bot, add);
    // Point this bot's own RTI at the add so its tank target resolves to the add. Otherwise the
    // default rti ("skull") keeps tank assist pulling it back to the skull-marked boss every tick.
    SetRtiTarget(botAI, "cross", add);

    if (AI_VALUE(Unit*, "current target") != add)
        return Attack(add);

    return false;
}

bool JaraxxusAssistTankHoldSecondAddAction::Execute(Event /*event*/)
{
    Unit* add = GetSecondaryJaraxxusAdd(botAI);
    if (!add)
        return false;

    MarkTargetWithSquare(bot, add);
    // Resolve this off-tank's target to the square-marked second add (see note above)
    SetRtiTarget(botAI, "square", add);

    if (AI_VALUE(Unit*, "current target") != add)
        return Attack(add);

    return false;
}

bool JaraxxusFocusAddAction::Execute(Event /*event*/)
{
    Unit* add = GetPriorityJaraxxusAdd(botAI);
    if (!add)
        return false;

    MarkTargetWithCross(bot, add);
    // Retarget this bot's RTI to the add. The default rti ("skull") sits on the boss, so without
    // this the engine falls through to "dps assist" each tick and yanks the bot back to the boss,
    // making it oscillate instead of committing to the add. Pointing rti at the add makes the
    // dps-assist target agree, so the add is killed first as intended.
    SetRtiTarget(botAI, "cross", add);

    if (AI_VALUE(Unit*, "current target") != add)
        return Attack(add);

    return false;
}

bool JaraxxusAvoidLegionFlameAction::Execute(Event /*event*/)
{
    return FleeFromCreatureCluster(static_cast<uint32>(ToCNpcs::NPC_LEGION_FLAME));
}

bool JaraxxusHealIncinerateTargetAction::Execute(Event /*event*/)
{
    // The afflicted player carries a healing-absorb shield; pour a direct heal into them
    Unit* target = nullptr;
    GuidVector const& members = AI_VALUE(GuidVector, "group members");
    for (ObjectGuid const& guid : members)
    {
        Unit* member = botAI->GetUnit(guid);
        if (member && member->IsAlive() && HasIncinerateFlesh(member))
        {
            target = member;
            break;
        }
    }

    if (!target)
        return false;

    static std::vector<std::string> const directHeals =
    {
        "greater heal", "flash heal", "penance",           // priest
        "healing touch", "nourish", "regrowth",            // druid
        "holy light", "flash of light", "holy shock",      // paladin
        "greater healing wave", "healing wave", "riptide", // shaman
    };

    for (std::string const& heal : directHeals)
    {
        if (botAI->CanCastSpell(heal, target))
            return botAI->CastSpell(heal, target);
    }

    return false;
}

bool JaraxxusRemoveNetherPowerAction::Execute(Event /*event*/)
{
    Unit* jaraxxus = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_JARAXXUS));
    if (!jaraxxus || !JaraxxusHasNetherPower(jaraxxus))
        return false;

    // Offensive magic dispel: spellsteal (mage), purge (shaman), dispel magic (priest)
    static std::vector<std::string> const dispels = { "spellsteal", "purge", "dispel magic" };
    for (std::string const& dispel : dispels)
    {
        if (botAI->CanCastSpell(dispel, jaraxxus))
            return botAI->CastSpell(dispel, jaraxxus);
    }

    return false;
}

bool JaraxxusInterruptFelFireballAction::Execute(Event /*event*/)
{
    Unit* jaraxxus = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_JARAXXUS));
    if (!jaraxxus)
        return false;

    // Pull a free damage dealer onto the boss so its always-on class interrupt
    // (Counterspell / Pummel / Kick / Mind Freeze ...) lands on the Fel Fireball cast.
    if (AI_VALUE(Unit*, "current target") != jaraxxus)
        return Attack(jaraxxus);

    return false;
}
