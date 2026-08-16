#include "UldTriggers_Mimiron.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>
#include <RtiTargetValue.h>

bool MimironShockBlastTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "leviathan mk ii");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    if (!boss->HasUnitState(UNIT_STATE_CASTING) || !boss->FindCurrentSpellBySpellId(SPELL_SHOCK_BLAST))
    {
        return false;
    }

    if (botAI->IsMelee(bot))
    {
        return true;
    }
    else
    {
        return bot->GetDistance2d(boss) < 15.0f;
    }
}

bool MimironPhase1PositioningTrigger::IsActive()
{
    if (!botAI->IsRanged(bot))
    {
        return false;
    }

    Unit* leviathanMkII = nullptr;

    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    Unit* target = nullptr;
    for (auto i = targets.begin(); i != targets.end(); ++i)
    {
        target = botAI->GetUnit(*i);
        if (!target || !target->IsAlive())
            continue;

        if (target->GetEntry() == NPC_LEVIATHAN_MKII)
            leviathanMkII = target;
        else if (target->GetEntry() == NPC_VX001)
            return false;
        else if (target->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
            return false;
    }

    if (!leviathanMkII || !leviathanMkII->IsAlive())
        return false;

    return AI_VALUE(float, "disperse distance") != 6.0f;
}

bool MimironP3Wx2LaserBarrageTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "vx-001");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
        return false;

    // Spinning Up is the 4s warning, 63274/63300 the 10s barrage itself, and all three are auras on
    // VX-001. Current-spell checks miss the whole thing: Spinning Up is cast triggered, and the
    // damage retriggers every 100ms are instant, so they clear m_currentSpells in the same update.
    return boss->HasAura(SPELL_SPINNING_UP) || boss->HasAura(SPELL_P3WX2_LASER_BARRAGE_AURA_1) ||
           boss->HasAura(SPELL_P3WX2_LASER_BARRAGE_AURA_2);
}

bool MimironArcSpreadTrigger::IsActive()
{
    if (!GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) &&
        !GetFirstAliveUnitByEntry(botAI, NPC_VX001) &&
        !GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        return false;

    // Stand down for the whole Spinning Up window and barrage: the dodge owns positioning then, and
    // walking a bot back to its ring slot mid-cone kills it.
    MimironP3Wx2LaserBarrageTrigger barrage(botAI);
    if (barrage.IsActive())
        return false;

    // Nor while something more urgent is already moving the bot.
    if (bot->FindNearestCreature(NPC_ROCKET_STRIKE_N, 10.0f))
        return false;

    Position slot;
    if (!GetMimironSpreadSlot(botAI, bot, slot))
        return false;

    return bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY()) >
           ULDUAR_MIMIRON_SPREAD_TOLERANCE;
}

bool MimironAerialCommandUnitTrigger::IsActive()
{
    // Phase 3 only: the Aerial Command Unit is up on its own.
    Unit* aerialCommandUnit = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
    if (!aerialCommandUnit || GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) ||
        GetFirstAliveUnitByEntry(botAI, NPC_VX001))
        return false;

    if (botAI->IsMainTank(bot) || botAI->IsAssistTankOfIndex(bot, 0))
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        Unit* assaultBot = GetFirstAliveUnitByEntry(botAI, NPC_ASSAULT_BOT);
        Unit* focus = assaultBot ? assaultBot : aerialCommandUnit;

        // Purely so the icon tracks what the raid is actually killing; no bot reads it back.
        return group->GetTargetIcon(RtiTargetValue::skullIndex) != focus->GetGUID();
    }

    if (!botAI->IsRanged(bot))
        return false;

    return AI_VALUE(float, "disperse distance") != 5.0f;
}

bool MimironRocketStrikeTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "vx-001");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
        return false;

    Creature* rocketStrikeN = bot->FindNearestCreature(NPC_ROCKET_STRIKE_N, 100.0f);

    if (!rocketStrikeN)
        return false;

    return bot->GetDistance2d(rocketStrikeN->GetPositionX(), rocketStrikeN->GetPositionY()) <= 10.0f;
}

bool MimironPhase4MarkDpsTrigger::IsActive()
{
    Unit* leviathanMkII = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    Unit* vx001 = GetFirstAliveUnitByEntry(botAI, NPC_VX001);
    Unit* aerialCommandUnit = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);

    if (!leviathanMkII || !vx001 || !aerialCommandUnit)
        return false;

    if (botAI->IsMainTank(bot))
    {
        Unit* focus = leviathanMkII;
        if (vx001->GetHealth() > focus->GetHealth())
            focus = vx001;
        if (aerialCommandUnit->GetHealth() > focus->GetHealth())
            focus = aerialCommandUnit;

        // Fires on the target as well as the icon: the mech with the most health left changes hands
        // through the phase, and the tank has to follow it whether or not the mark is already right.
        return AI_VALUE(Unit*, "current target") != focus ||
               (bot->GetGroup() && bot->GetGroup()->GetTargetIcon(RtiTargetValue::skullIndex) != focus->GetGUID());
    }

    // Non-tanks only come here to be spread for Hand Pulse; their target belongs to
    // "mimiron set dps priority".
    return AI_VALUE(float, "disperse distance") != 4.0f;
}

//
// Mimiron
//
bool MimironProximityMineTrigger::IsActive()
{
    TooCloseToCreatureTrigger tooCloseToProximityMine(botAI);
    return tooCloseToProximityMine.TooCloseToCreature(NPC_PROXIMITY_MINE,
                                                     ULDUAR_MIMIRON_MINE_CLEARANCE + 1.0f);
}

bool MimironBombBotTrigger::IsActive()
{
    TooCloseToCreatureTrigger tooCloseToBombBot(botAI);
    return tooCloseToBombBot.TooCloseToCreature(NPC_BOMB_BOT, ULDUAR_MIMIRON_BOMB_BOT_RADIUS);
}

bool MimironDodgeFlamesTrigger::IsActive()
{
    if (!IsMimironHardModeActive(botAI))
        return false;

    // The fire nodes are non-selectable trigger creatures, so they never show up in attack-target
    // lists - scan the raw nearby-npc list instead.
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (auto const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_FLAMES_SPREAD && unit->GetEntry() != NPC_FLAMES_INITIAL)
            continue;

        if (bot->GetExactDist2d(unit) < ULDUAR_MIMIRON_FLAMES_RADIUS)
            return true;
    }

    return false;
}

bool MimironFrostBombTrigger::IsActive()
{
    if (!IsMimironHardModeActive(botAI))
        return false;

    TooCloseToCreatureTrigger tooCloseToFrostBomb(botAI);
    return tooCloseToFrostBomb.TooCloseToCreature(NPC_FROST_BOMB, ULDUAR_MIMIRON_FROST_BOMB_RADIUS);
}

bool MimironPlasmaBlastTrigger::IsActive()
{
    if (!PlayerbotAI::IsMainTank(bot) && !botAI->IsAssistTankOfIndex(bot, 0))
        return false;

    // Phase 1 only. From phase 4 on, the main tank has to keep the chassis parked: VX-001 rides it,
    // and the Laser Barrage cone radiates from wherever it is standing.
    Unit* leviathanMkII = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    if (!leviathanMkII || GetFirstAliveUnitByEntry(botAI, NPC_VX001) ||
        GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        return false;

    // The cannon is a passenger of the MK II and is what actually casts.
    Creature* cannon = bot->FindNearestCreature(NPC_LEVIATHAN_MKII_CANNON, 100.0f);
    if (!cannon || !cannon->FindCurrentSpellBySpellId(SPELL_MIMIRON_PLASMA_BLAST))
        return false;

    // Whoever is not already holding it takes it. That alternates the two tanks by itself, one taunt
    // each per 22s cycle, which stays clear of the 15s taunt-DR reset.
    return leviathanMkII->GetVictim() != bot;
}

bool MimironMagneticCoreTrigger::IsActive()
{
    // One bot per instance owns this, so two carriers cannot burn two cores on the same landing.
    if (!IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID))
        return false;

    Unit* aerialCommandUnit = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
    if (!aerialCommandUnit)
        return false;

    // Already grounded; nothing to do until it lifts off again.
    if (!aerialCommandUnit->HasUnitMovementFlag(MOVEMENTFLAG_HOVER))
        return false;

    if (bot->HasItemCount(ITEM_MIMIRON_MAGNETIC_CORE, 1, false))
        return true;

    // Corpses linger 25s, so finding a dead Assault Bot in reach is the same test a player makes
    // before looting one.
    return bot->FindNearestCreature(NPC_ASSAULT_BOT, ULDUAR_MIMIRON_CORE_LOOT_RANGE, false) != nullptr;
}

bool MimironSetDpsPriorityTrigger::IsActive()
{
    // Tanks hold what the tank nodes give them; this one owns "current target" for everybody else.
    if (botAI->IsTank(bot))
        return false;

    return GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) ||
           GetFirstAliveUnitByEntry(botAI, NPC_VX001) ||
           GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
}
