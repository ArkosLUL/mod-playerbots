#include "ToCActions_Anubarak.h"
#include "ToCData.h"
#include "ToCHelpers_Shared.h"
#include "ToCHelpers_Anubarak.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Unit.h"
#include "Creature.h"

#include <cmath>

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

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
    return DragBossToAnchor(anubarak, ANUBARAK_PIT_CENTER);
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
    // Destroying a flying Frost Sphere drops a Permafrost patch the spike-chase target can be kited
    // through.
    std::list<Creature*> spheres;
    bot->GetCreatureListWithEntryInGrid(spheres, static_cast<uint32>(ToCNpcs::NPC_FROST_SPHERE), 100.0f);

    Unit* nearest = nullptr;
    float nearestDist = 100.0f;
    for (Creature* sphere : spheres)
    {
        if (!IsFrostSphereFlying(sphere))
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
