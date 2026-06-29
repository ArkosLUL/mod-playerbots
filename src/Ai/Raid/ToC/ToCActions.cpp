#include "ToCActions.h"
#include "ToCHelpers.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "Unit.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
// Return the alive worm (Acidmaw or Dreadscale) currently in the requested mobility state
Unit* FindWorm(PlayerbotAI* botAI, bool mobile)
{
    Unit* acidmaw = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ACIDMAW));
    if (acidmaw && IsWormMobile(acidmaw) == mobile)
        return acidmaw;

    Unit* dreadscale = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_DREADSCALE));
    if (dreadscale && IsWormMobile(dreadscale) == mobile)
        return dreadscale;

    return nullptr;
}
}

// Gormok the Impaler

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
    if (gormok->GetVictim() == bot)
    {
        const Position& position = ARENA_CENTER;
        const float distToPosition =
            bot->GetExactDist2d(position.GetPositionX(), position.GetPositionY());

        if (distToPosition > 12.0f)
        {
            const float dX = position.GetPositionX() - bot->GetPositionX();
            const float dY = position.GetPositionY() - bot->GetPositionY();
            const float moveDist = std::min(5.0f, distToPosition);
            const float moveX = bot->GetPositionX() + (dX / distToPosition) * moveDist;
            const float moveY = bot->GetPositionY() + (dY / distToPosition) * moveDist;

            return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, moveX, moveY, position.GetPositionZ(),
                          false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true, true);
        }
    }

    return false;
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

// Acidmaw & Dreadscale

bool WormsMainTankHoldMobileWormAction::Execute(Event /*event*/)
{
    Unit* worm = FindWorm(botAI, true);
    if (!worm)
        return false;

    MarkTargetWithSkull(bot, worm);
    SetRtiTarget(botAI, "skull", worm);

    if (AI_VALUE(Unit*, "current target") != worm)
        return Attack(worm);

    return false;
}

bool WormsAssistTankHoldStationaryWormAction::Execute(Event /*event*/)
{
    Unit* worm = FindWorm(botAI, false);
    if (!worm)
        return false;

    MarkTargetWithCross(bot, worm);

    if (AI_VALUE(Unit*, "current target") != worm)
        return Attack(worm);

    return false;
}

bool WormsSpreadAction::Execute(Event /*event*/)
{
    constexpr float minSpreadDistance = 8.0f;
    constexpr uint32 minInterval = 1000;
    if (Unit* nearestPlayer = GetNearestPlayerInRadius(bot, minSpreadDistance))
        return FleePosition(nearestPlayer->GetPosition(), minSpreadDistance, minInterval);

    return false;
}

bool WormsKeepMovingAction::Execute(Event /*event*/)
{
    // Burning damage ramps up while standing still, so keep the bot in motion around the arena
    const Position& center = ARENA_CENTER;
    const float distToCenter = bot->GetExactDist2d(center.GetPositionX(), center.GetPositionY());

    float destX;
    float destY;
    if (distToCenter > 8.0f)
    {
        const float dX = center.GetPositionX() - bot->GetPositionX();
        const float dY = center.GetPositionY() - bot->GetPositionY();
        destX = bot->GetPositionX() + (dX / distToCenter) * 5.0f;
        destY = bot->GetPositionY() + (dY / distToCenter) * 5.0f;
    }
    else
    {
        const float angle = bot->GetOrientation() + static_cast<float>(M_PI) / 2.0f;
        destX = bot->GetPositionX() + std::cos(angle) * 6.0f;
        destY = bot->GetPositionY() + std::sin(angle) * 6.0f;
    }

    return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, destX, destY, center.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT, true, false);
}

// Icehowl

bool IcehowlMainTankHoldBossAction::Execute(Event /*event*/)
{
    Unit* icehowl = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ICEHOWL));
    if (!icehowl)
        return false;

    MarkTargetWithSkull(bot, icehowl);
    SetRtiTarget(botAI, "skull", icehowl);

    if (AI_VALUE(Unit*, "current target") != icehowl)
        return Attack(icehowl);

    if (icehowl->GetVictim() == bot)
    {
        const Position& position = ARENA_CENTER;
        const float distToPosition =
            bot->GetExactDist2d(position.GetPositionX(), position.GetPositionY());

        if (distToPosition > 12.0f)
        {
            const float dX = position.GetPositionX() - bot->GetPositionX();
            const float dY = position.GetPositionY() - bot->GetPositionY();
            const float moveDist = std::min(5.0f, distToPosition);
            const float moveX = bot->GetPositionX() + (dX / distToPosition) * moveDist;
            const float moveY = bot->GetPositionY() + (dY / distToPosition) * moveDist;

            return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, moveX, moveY, position.GetPositionZ(),
                          false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true, true);
        }
    }

    return false;
}

bool IcehowlClearChargePathAction::Execute(Event /*event*/)
{
    Unit* icehowl = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ICEHOWL));
    if (!icehowl)
        return false;

    // Self-contained guard: only dodge when actually standing in the charge lane
    constexpr float corridorHalfWidth = 14.0f;
    if (!IsBotInChargeCorridor(bot, icehowl, corridorHalfWidth))
        return false;

    const float orientation = icehowl->GetOrientation();
    const float dirX = std::cos(orientation);
    const float dirY = std::sin(orientation);

    const float relX = bot->GetPositionX() - icehowl->GetPositionX();
    const float relY = bot->GetPositionY() - icehowl->GetPositionY();

    // Signed perpendicular offset from the charge lane; flee toward whichever side the bot is on
    const float perpendicular = relX * dirY - relY * dirX;
    const float side = perpendicular >= 0.0f ? 1.0f : -1.0f;

    // Unit vector perpendicular to the charge direction, pointing away from the lane
    const float escapeX = dirY * side;
    const float escapeY = -dirX * side;

    // Inside the corridor (guaranteed by the guard above), so this is always positive
    const float clearance = corridorHalfWidth - std::fabs(perpendicular) + 3.0f;

    botAI->InterruptSpell();

    const float destX = bot->GetPositionX() + escapeX * clearance;
    const float destY = bot->GetPositionY() + escapeY * clearance;

    return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, destX, destY, bot->GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT, true, false);
}

// Lord Jaraxxus

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
    if (jaraxxus->GetVictim() == bot)
    {
        const Position& position = ARENA_CENTER;
        const float distToPosition =
            bot->GetExactDist2d(position.GetPositionX(), position.GetPositionY());

        if (distToPosition > 12.0f)
        {
            const float dX = position.GetPositionX() - bot->GetPositionX();
            const float dY = position.GetPositionY() - bot->GetPositionY();
            const float moveDist = std::min(5.0f, distToPosition);
            const float moveX = bot->GetPositionX() + (dX / distToPosition) * moveDist;
            const float moveY = bot->GetPositionY() + (dY / distToPosition) * moveDist;

            return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, moveX, moveY, position.GetPositionZ(),
                          false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true, true);
        }
    }

    return false;
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
    // Legion Flame lays a trail of patches; flee away from the centre of the whole nearby
    // cluster (scanning wider than the trigger radius) so the escape vector does not push the
    // bot from the nearest patch straight into the next one in the line.
    constexpr float clusterRadius = 15.0f;
    Position flameCenter;
    if (!GetCreatureClusterCenter(bot, static_cast<uint32>(ToCNpcs::NPC_LEGION_FLAME), clusterRadius, flameCenter))
        return false;

    botAI->InterruptSpell();

    // Step away from the fire and keep a comfortable buffer
    constexpr float fleeDistance = 12.0f;
    constexpr uint32 minInterval = 500;
    return FleePosition(flameCenter, fleeDistance, minInterval);
}

bool JaraxxusHealIncinerateTargetAction::Execute(Event /*event*/)
{
    // The afflicted player carries a healing-absorb shield; pour a direct heal into them
    Unit* target = nullptr;
    GuidVector const& members = AI_VALUE(GuidVector, "group members");
    for (ObjectGuid const& guid : members)
    {
        Unit* member = botAI->GetUnit(guid);
        if (member && member->IsAlive() &&
            member->HasAura(static_cast<uint32>(ToCSpells::SPELL_INCINERATE_FLESH)))
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
