#include "ToCActions.h"
#include "ToCHelpers.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Unit.h"
#include "Creature.h"
#include "WorldSession.h"
#include "WorldPacket.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

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

// Cast the bot's class taunt on target. Only tank classes have one; returns false for other classes or
// when the taunt is unavailable / on cooldown.
bool CastTankTaunt(PlayerbotAI* botAI, Player* bot, Unit* target)
{
    if (!target || !target->IsAlive())
        return false;

    switch (bot->getClass())
    {
        case CLASS_PALADIN:
            return botAI->CastSpell("hand of reckoning", target);
        case CLASS_DEATH_KNIGHT:
            return botAI->CastSpell("dark command", target);
        case CLASS_DRUID:
            return botAI->CastSpell("growl", target);
        case CLASS_WARRIOR:
            return botAI->CastSpell("taunt", target);
        default:
            return false;
    }
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
        Position const& position = ARENA_CENTER;
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

bool GormokTankSwapTauntAction::Execute(Event /*event*/)
{
    Unit* gormok = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_GORMOK));
    if (!gormok)
        return false;

    MarkTargetWithSkull(bot, gormok);
    SetRtiTarget(botAI, "skull", gormok);

    // Taunt to pull Gormok off the overloaded tank; the Impale bleed then decays on the old tank before
    // it stacks to a lethal amount.
    if (CastTankTaunt(botAI, bot, gormok))
        return true;

    // Taunt unavailable / on cooldown: at least commit melee onto the boss so threat keeps building.
    if (AI_VALUE(Unit*, "current target") != gormok)
        return Attack(gormok);

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
    Position const& center = ARENA_CENTER;
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

bool AvoidCreatureClusterAction::FleeFromCreatureCluster(uint32 entry)
{
    // Scan wider than the trigger radius so the escape vector runs from the centre of the whole cluster,
    // not the nearest patch, and does not push the bot from one patch straight into the next.
    constexpr float clusterRadius = 15.0f;
    Position center;
    if (!GetCreatureClusterCenter(bot, entry, clusterRadius, center))
        return false;

    bot->CastStop();

    constexpr float fleeDistance = 12.0f;
    constexpr uint32 minInterval = 500;
    return FleePosition(center, fleeDistance, minInterval);
}

bool WormsAvoidSlimePoolAction::Execute(Event /*event*/)
{
    return FleeFromCreatureCluster(static_cast<uint32>(ToCNpcs::NPC_SLIME_POOL));
}

bool WormsAvoidSweepAction::Execute(Event /*event*/)
{
    Unit* worm = GetWormCastingSweep(botAI);
    if (!worm)
        return false;

    // Step perpendicular to the worm's facing to clear the frontal Sweep cone, fleeing toward whichever
    // side the bot is already on.
    const float orientation = worm->GetOrientation();
    const float dirX = std::cos(orientation);
    const float dirY = std::sin(orientation);

    const float relX = bot->GetPositionX() - worm->GetPositionX();
    const float relY = bot->GetPositionY() - worm->GetPositionY();

    const float perpendicular = relX * dirY - relY * dirX;
    const float side = perpendicular >= 0.0f ? 1.0f : -1.0f;

    const float escapeX = dirY * side;
    const float escapeY = -dirX * side;

    bot->CastStop();

    constexpr float clearance = 12.0f;
    const float destX = bot->GetPositionX() + escapeX * clearance;
    const float destY = bot->GetPositionY() + escapeY * clearance;

    return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, destX, destY, bot->GetPositionZ(), false, false,
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
        Position const& position = ARENA_CENTER;
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

    bot->CastStop();

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
        Position const& position = ARENA_CENTER;
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

// Anub'arak

bool AnubarakMainTankHoldBossAction::Execute(Event /*event*/)
{
    Unit* anubarak = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ANUBARAK));
    if (!anubarak)
        return false;

    MarkTargetWithSkull(bot, anubarak);
    SetRtiTarget(botAI, "skull", anubarak);

    if (AI_VALUE(Unit*, "current target") != anubarak)
        return Attack(anubarak);

    // Anchor the boss near the centre of the nerubian pit so ranged have room to seed Permafrost
    // and the spike-chase target has space to kite
    if (anubarak->GetVictim() == bot)
    {
        Position const& position = ANUBARAK_PIT_CENTER;
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

bool AnubarakAssistTankHoldBurrowerAction::Execute(Event /*event*/)
{
    Unit* burrower = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_NERUBIAN_BURROWER));
    if (!burrower)
        return false;

    MarkTargetWithCross(bot, burrower);
    // Point this bot's own RTI at the burrower so its tank-assist target resolves to the add rather
    // than pulling it back to the skull-marked boss every tick (same idiom as the Jaraxxus adds).
    SetRtiTarget(botAI, "cross", burrower);

    if (AI_VALUE(Unit*, "current target") != burrower)
        return Attack(burrower);

    return false;
}

bool AnubarakFocusBurrowerAction::Execute(Event /*event*/)
{
    Unit* burrower = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_NERUBIAN_BURROWER));
    if (!burrower)
        return false;

    // Note: a burrower submerges and resets at 80% HP unless it is damaged while standing on a
    // Permafrost patch. Bots focus it normally; forcing it onto Permafrost is a future refinement.
    MarkTargetWithCross(bot, burrower);
    SetRtiTarget(botAI, "cross", burrower);

    if (AI_VALUE(Unit*, "current target") != burrower)
        return Attack(burrower);

    return false;
}

bool AnubarakFocusScarabAction::Execute(Event /*event*/)
{
    Unit* scarab = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_SWARM_SCARAB));
    if (!scarab)
        return false;

    if (AI_VALUE(Unit*, "current target") != scarab)
        return Attack(scarab);

    return false;
}

bool AnubarakKiteSpikeToPermafrostAction::Execute(Event /*event*/)
{
    // Stop casting so the kite is never rooted in place by a channel
    bot->CastStop();

    // Locate the chasing spike up front: it is needed both to pick a safe Permafrost patch and for
    // the flee fallback below.
    constexpr float searchRadius = 60.0f;
    Unit* spike = GetNearestCreatureByEntry(bot, static_cast<uint32>(ToCNpcs::NPC_PURSUING_SPIKE), searchRadius);

    // Preferred: run through the nearest grounded Permafrost patch, which despawns the chasing
    // spike. Only do so when the patch is not on the spike's side of the bot, otherwise heading for
    // it would run the bot straight into the spike (an Impale) instead of away from it.
    if (Unit* permafrost = GetNearestPermafrost(bot, searchRadius))
    {
        bool safe = true;
        if (spike)
        {
            float const toPx = permafrost->GetPositionX() - bot->GetPositionX();
            float const toPy = permafrost->GetPositionY() - bot->GetPositionY();
            float const toSx = spike->GetPositionX() - bot->GetPositionX();
            float const toSy = spike->GetPositionY() - bot->GetPositionY();
            float const toPLen = std::sqrt(toPx * toPx + toPy * toPy);
            float const toSLen = std::sqrt(toSx * toSx + toSy * toSy);

            // Unsafe when the patch lies within ~60 deg of the spike's direction and the spike is
            // closer than the patch (i.e. the spike sits between the bot and the patch).
            if (toPLen > 0.1f && toSLen > 0.1f)
            {
                float const cosAngle = (toPx * toSx + toPy * toSy) / (toPLen * toSLen);
                if (cosAngle > 0.5f && toSLen < toPLen)
                    safe = false;
            }
        }

        if (safe)
        {
            return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, permafrost->GetPositionX(), permafrost->GetPositionY(),
                          permafrost->GetPositionZ(), false, false, false, false,
                          MovementPriority::MOVEMENT_COMBAT, true, false);
        }
    }

    // Fallback when no Permafrost has been seeded yet (or the only patch is past the spike): keep
    // kiting away from the spike. Bias the escape back toward the pit centre so the bot does not run
    // itself into a wall.
    if (!spike)
        return false;

    Position const& center = ANUBARAK_PIT_CENTER;
    // Direction away from the spike
    float fleeX = bot->GetPositionX() - spike->GetPositionX();
    float fleeY = bot->GetPositionY() - spike->GetPositionY();
    float const fleeLen = std::sqrt(fleeX * fleeX + fleeY * fleeY);
    if (fleeLen < 0.1f)
        return false;

    fleeX /= fleeLen;
    fleeY /= fleeLen;

    // Blend in a pull toward the centre so the kite circles the pit instead of leaving it
    float toCenterX = center.GetPositionX() - bot->GetPositionX();
    float toCenterY = center.GetPositionY() - bot->GetPositionY();
    float const toCenterLen = std::sqrt(toCenterX * toCenterX + toCenterY * toCenterY);
    if (toCenterLen > 0.1f)
    {
        fleeX += (toCenterX / toCenterLen) * 0.5f;
        fleeY += (toCenterY / toCenterLen) * 0.5f;
    }

    constexpr float kiteDistance = 15.0f;
    const float destX = bot->GetPositionX() + fleeX * kiteDistance;
    const float destY = bot->GetPositionY() + fleeY * kiteDistance;

    return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, destX, destY, center.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT, true, false);
}

bool AnubarakDestroyFrostSphereAction::Execute(Event /*event*/)
{
    // Target the nearest flying (still selectable, not yet grounded) Frost Sphere. Destroying it
    // drops a Permafrost patch the spike-chase target can be kited through.
    std::list<Creature*> spheres;
    bot->GetCreatureListWithEntryInGrid(spheres, static_cast<uint32>(ToCNpcs::NPC_FROST_SPHERE), 100.0f);

    Unit* nearest = nullptr;
    float nearestDist = 100.0f;
    for (Creature* sphere : spheres)
    {
        if (!sphere->IsAlive() || sphere->HasAura(static_cast<uint32>(ToCSpells::SPELL_PERMAFROST)))
            continue;

        float const dist = bot->GetExactDist2d(sphere);
        if (!nearest || dist < nearestDist)
        {
            nearest = sphere;
            nearestDist = dist;
        }
    }

    if (!nearest)
        return false;

    if (AI_VALUE(Unit*, "current target") != nearest)
        return Attack(nearest);

    return false;
}

// Faction Champions

bool FactionChampionsFocusPriorityAction::Execute(Event /*event*/)
{
    Unit* priority = GetPriorityFactionChampion(botAI);
    if (!priority)
        return false;

    // One designated bot owns the raid markers so they do not flicker between bots: skull on the kill
    // target (the shared focus mark), moon on a second healer for the per-class "cc" strategy to lock.
    if (IsMechanicTrackerBot(bot, TRIAL_OF_THE_CRUSADER_MAP_ID))
    {
        MarkTargetWithSkull(bot, priority);
        SetRtiTarget(botAI, "skull", priority);

        if (Unit* ccHealer = GetCcFactionChampionHealer(botAI, priority))
            SetRtiCcTarget(botAI, "moon", ccHealer);
    }

    if (AI_VALUE(Unit*, "current target") != priority)
        return Attack(priority);

    return false;
}

// Twin Val'kyr

bool TwinValkyrMainTankHoldLightTwinAction::Execute(Event /*event*/)
{
    Unit* fjola = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_FJOLA_LIGHTBANE));
    if (!fjola)
        return false;

    // Skull on Fjola is the shared focus mark: non-tank DPS follow it via the default "dps assist", and
    // because the twins share health, burning Fjola kills both. No separate DPS focus action is needed.
    MarkTargetWithSkull(bot, fjola);
    SetRtiTarget(botAI, "skull", fjola);

    if (AI_VALUE(Unit*, "current target") != fjola)
        return Attack(fjola);

    // Anchor the boss near the arena centre so melee stack there and ranged have room
    if (fjola->GetVictim() == bot)
    {
        Position const& position = ARENA_CENTER;
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

bool TwinValkyrAssistTankHoldDarkTwinAction::Execute(Event /*event*/)
{
    Unit* eydis = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_EYDIS_DARKBANE));
    if (!eydis)
        return false;

    MarkTargetWithCross(bot, eydis);
    // Point this off-tank's own RTI at Eydis so its tank-assist target resolves to her rather than
    // pulling back to the skull-marked Fjola every tick (same idiom as the Jaraxxus/Anub'arak adds).
    SetRtiTarget(botAI, "cross", eydis);

    if (AI_VALUE(Unit*, "current target") != eydis)
        return Attack(eydis);

    return false;
}

bool TwinValkyrEssenceActionBase::AcquireEssence(bool wantLight)
{
    uint32 const portalEntry = wantLight ? static_cast<uint32>(ToCNpcs::NPC_LIGHT_ESSENCE)
                                          : static_cast<uint32>(ToCNpcs::NPC_DARK_ESSENCE);

    // Portals sit at the arena corners; scan the whole arena so the bot can find one from anywhere
    Unit* portal = GetNearestCreatureByEntry(bot, portalEntry, 200.0f);
    if (!portal)
        return false;

    // Use 3D distance: the core's HandleGossipHelloOpcode -> GetNPCIfCanInteractWith gates on the 3D
    // IsWithinDistInMap, so a 2D check could report "in range" for an elevated portal and the gossip
    // would silently no-op.
    if (bot->GetDistance(portal) > INTERACTION_DISTANCE)
    {
        return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, portal->GetPositionX(), portal->GetPositionY(),
                      portal->GetPositionZ(), false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT, true, false);
    }

    // In range: trigger the portal's gossip-hello hook directly. The essence effect lives in the
    // creature's C++ OnGossipHello override, so HandleGossipHelloOpcode fires it (and casts the essence
    // aura) regardless of whether the NPC has DB gossip-menu items.
    bot->CastStop();
    bot->SetFacingToObject(portal);

    WorldPacket packet;
    packet << portal->GetGUID();
    bot->GetSession()->HandleGossipHelloOpcode(packet);

    // The gossip hook applies the essence synchronously (triggered CastSpell), so confirm the aura
    // actually landed. Report failure if it did not (portal not interactable / out of range) so the
    // trigger re-fires next tick instead of the action falsely claiming success.
    return wantLight ? HasLightEssence(bot) : HasDarkEssence(bot);
}

bool TwinValkyrSwapEssenceForVortexAction::Execute(Event /*event*/)
{
    // Whichever vortex is up dictates the colour the bot must match
    return AcquireEssence(TwinValkyrLightVortexActive(botAI));
}

bool TwinValkyrSwapEssenceForTouchAction::Execute(Event /*event*/)
{
    // Light Touch is mitigated by Light Essence; Dark Touch by Dark Essence
    return AcquireEssence(bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_LIGHT_TOUCH)));
}

bool TwinValkyrAcquireInitialEssenceAction::Execute(Event /*event*/)
{
    // Tanks lock the essence matching their assigned twin so its colour-typed melee (Light/Dark Twin
    // Spike) is mitigated for the whole fight: the main tank holds Fjola (Light) and any other tank
    // holds Eydis (Dark). Non-tanks default to Light at the pull; the vortex/touch swap logic converges
    // from there.
    bool const wantLight = !(botAI->IsTank(bot) && !botAI->IsMainTank(bot));
    return AcquireEssence(wantLight);
}

bool TwinValkyrInterruptPactAction::Execute(Event /*event*/)
{
    Unit* twin = GetTwinCastingPact(botAI);
    if (!twin)
        return false;

    // Pull a free damage dealer onto the casting twin so its always-on class interrupt (Counterspell /
    // Pummel / Kick / Mind Freeze ...) lands on the Twin's Pact channel. The damage-reflect shield the
    // twins carry during specials does not reflect interrupts, so the kick lands normally.
    if (AI_VALUE(Unit*, "current target") != twin)
        return Attack(twin);

    return false;
}
