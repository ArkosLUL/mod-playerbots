#include "ToCActions_TwinValkyr.h"
#include "ToCData.h"
#include "ToCHelpers_Shared.h"
#include "ToCHelpers_TwinValkyr.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Unit.h"
#include "WorldSession.h"
#include "WorldPacket.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

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
    return DragBossToAnchor(fjola, ARENA_CENTER);
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
