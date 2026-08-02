#include "UldActions_Ignis.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <cmath>

#include "AiObjectContext.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Position.h"
#include "UldBossHelper.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "RtiValue.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <RtiTargetValue.h>
#include <TankAssistStrategy.h>

bool IgnisIronConstructAction::isUseful()
{
    IgnisIronConstructTrigger ignisIronConstructTrigger(botAI);
    return ignisIronConstructTrigger.IsActive();
}

bool IgnisIronConstructAction::Execute(Event /*event*/)
{
    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_IGNIS_IRON_CONSTRUCT)
            continue;

        if (unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            continue;

        MarkTargetWithSkull(bot, unit);
        SetRtiTarget(botAI, "skull", unit);
        return true;
    }

    return false;
}
