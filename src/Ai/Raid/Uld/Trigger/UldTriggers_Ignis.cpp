#include "UldTriggers_Ignis.h"

#include "GameObject.h"
#include "Group.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "UldEncounter_Ignis.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>

static int32 GetIgnisBrittleTimeLeft(Unit* construct)
{
    if (!construct)
        return 0;

    if (Aura* brittle = construct->GetAura(SPELL_IGNIS_BRITTLE_10))
        return brittle->GetDuration();

    if (Aura* brittle = construct->GetAura(SPELL_IGNIS_BRITTLE_25))
        return brittle->GetDuration();

    return 0;
}

bool IgnisScorchedGroundTrigger::IsActive()
{
    if (!IsIgnisEngaged(botAI))
        return false;

    Unit* patch = GetIgnisNearestScorchedGround(botAI, bot);

    return patch && bot->GetExactDist2d(patch) <= ULDUAR_IGNIS_SCORCHED_GROUND_AVOID_RADIUS;
}

bool IgnisMainTankPositionTrigger::IsActive()
{
    if (!PlayerbotAI::IsMainTank(bot))
        return false;

    Unit* boss = GetEngagedIgnis(botAI);
    if (!boss || boss->GetVictim() != bot)
        return false;

    Position const spot = GetIgnisMainTankPosition(bot, boss);

    return bot->GetExactDist2d(&spot) > ULDUAR_IGNIS_TANK_SPOT_TOLERANCE;
}

bool IgnisConstructTankTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    if (!IsIgnisEngaged(botAI))
        return false;

    if (GetIgnisConstructTankIndex(bot) < 0)
        return false;

    return GetIgnisDrivenConstruct(botAI, bot) != nullptr;
}

bool IgnisAttackBrittleConstructTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot))
        return false;

    if (!IsIgnisEngaged(botAI))
        return false;

    Unit* construct = GetIgnisBrittleConstruct(botAI);
    if (!construct)
        return false;

    if (!PlayerbotAI::IsRanged(bot) &&
        GetIgnisBrittleTimeLeft(construct) > static_cast<int32>(ULDUAR_IGNIS_BRITTLE_MELEE_FALLBACK_MS))
        return false;

    return true;
}

bool IgnisAttackBossTrigger::IsActive()
{
    Unit* boss = GetEngagedIgnis(botAI);
    if (!boss)
        return false;

    if (GetIgnisHeldConstruct(botAI, bot))
        return false;

    if (AI_VALUE(Unit*, "current target") == boss)
        return false;

    IgnisAttackBrittleConstructTrigger brittle(botAI);

    return !brittle.IsActive();
}

bool IgnisMoltenConstructAvoidTrigger::IsActive()
{
    Unit* boss = GetEngagedIgnis(botAI);
    if (!boss)
        return false;

    if (GetIgnisHeldConstruct(botAI, bot))
        return false;

    if (boss->GetVictim() == bot)
        return false;

    Unit* molten = GetIgnisNearestMoltenConstruct(botAI, bot);

    return molten && bot->GetExactDist2d(molten) <= ULDUAR_IGNIS_MOLTEN_AVOID_RADIUS;
}

bool IgnisFlameJetsTrigger::IsActive()
{
    if (!bot->HasUnitState(UNIT_STATE_CASTING))
        return false;

    return GetIgnisIf(botAI, [](Creature const* ignis)
                      { return ignis->IsInCombat() && IsIgnisFlameJetsCasting(ignis); }) != nullptr;
}

bool IgnisSlagPotHealTrigger::IsActive()
{
    if (!PlayerbotAI::IsHeal(bot))
        return false;

    if (!IsIgnisEngaged(botAI))
        return false;

    Player* victim = GetIgnisSlagPotVictim(botAI);

    return victim && victim->GetHealthPct() < ULDUAR_IGNIS_SLAG_POT_HEAL_HP_PCT;
}
