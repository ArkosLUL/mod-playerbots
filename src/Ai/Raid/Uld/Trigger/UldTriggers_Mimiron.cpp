#include "UldTriggers_Mimiron.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldEncounter_Mimiron.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTargets.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>
#include <RtiTargetValue.h>

using namespace EncounterHelpers;

bool MimironResetEncounterStateTrigger::IsActive()
{
    if (bot->GetMapId() != ULDUAR_MAP_ID || AI_VALUE(float, "disperse distance") < 0.0f)
        return false;

    for (auto const& guid : AI_VALUE(GuidVector, "possible targets"))
    {
        Unit* target = botAI->GetUnit(guid);
        if (!target || !target->IsAlive())
            continue;

        uint32 const entry = target->GetEntry();
        if (entry == NPC_LEVIATHAN_MKII || entry == NPC_VX001 || entry == NPC_AERIAL_COMMAND_UNIT)
            return false;
    }

    return true;
}

bool MimironShockBlastTrigger::IsActive()
{
    Unit* boss = GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII);

    if (!boss || !boss->IsAlive())
        return false;

    if (!boss->HasUnitState(UNIT_STATE_CASTING) || !boss->FindCurrentSpellBySpellId(SPELL_SHOCK_BLAST))
        return false;

    if (PlayerbotAI::IsMelee(bot))
        return true;

    return bot->GetExactDist2d(boss) < ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST;
}

bool MimironUnreachableTrigger::IsActive() { return GetMimironUnreachableMech(botAI, bot) != nullptr; }

bool MimironPhase1PositioningTrigger::IsActive()
{
    if (!PlayerbotAI::IsRanged(bot))
        return false;

    Unit* leviathanMkII = nullptr;

    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    for (ObjectGuid const& guid : targets)
    {
        Unit* target = botAI->GetUnit(guid);
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

    return AI_VALUE(float, "disperse distance") != GetMimironPhase1DisperseDistance(botAI);
}

bool MimironP3Wx2LaserBarrageTrigger::IsActive()
{
    Unit* boss = GetUldUnitByEntry(botAI, NPC_VX001);

    if (!boss || !boss->IsAlive())
        return false;

    return GetMimironSpinningUpSeconds(boss) >= 0.0f ||
           boss->HasAura(SPELL_P3WX2_LASER_BARRAGE_AURA_1) ||
           boss->HasAura(SPELL_P3WX2_LASER_BARRAGE_AURA_2);
}

bool MimironArcSpreadTrigger::IsActive()
{
    Position slot;
    if (!GetMimironSpreadSlot(botAI, bot, slot))
        return false;

    if (bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY()) <= ULDUAR_MIMIRON_SPREAD_TOLERANCE)
        return false;

    if (Unit* vx001 = GetUldUnitByEntry(botAI, NPC_VX001))
    {
        MimironBarrageWindow const window = GetMimironBarrageWindow(bot, vx001);
        if (window.valid)
        {
            float const speed = bot->GetSpeed(MOVE_RUN);
            float const travel =
                speed > 0.0f ? bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY()) / speed
                             : 0.0f;

            if (!IsMimironSpotBarrageSafe(vx001, window, bot->GetPosition(), 0.0f) ||
                !IsMimironSpotBarrageSafe(vx001, window, slot, travel))
                return false;
        }
    }

    if (IsMimironTankAnchorSlot(botAI, bot))
    {
        if (!IsMimironLegBombSafe(bot->GetPosition(), GetMimironFirefighterHazards(botAI), slot))
            return false;
    }
    else if (GetMimironSlotApproaches(botAI, bot, slot, GetMimironFirefighterHazards(botAI)).empty())
        return false;

    if (Unit* vx001 = GetUldUnitByEntry(botAI, NPC_VX001))
    {
        MimironRapidBurstWindow const burst = GetMimironRapidBurstWindow(botAI, bot, vx001);
        if (!IsMimironSpotRapidBurstSafe(vx001, burst, slot))
            return false;
    }

    return true;
}

bool MimironAerialCommandUnitTrigger::IsActive()
{
    Unit* aerialCommandUnit = GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
    if (!aerialCommandUnit || GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII) ||
        GetUldUnitByEntry(botAI, NPC_VX001))
        return false;

    if (PlayerbotAI::IsMainTank(bot) || PlayerbotAI::IsAssistTankOfIndex(bot, 0))
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        Unit* assaultBot = GetUldUnitByEntry(botAI, NPC_ASSAULT_BOT);
        Unit* focus = assaultBot ? assaultBot : aerialCommandUnit;

        return group->GetTargetIcon(RtiTargetValue::skullIndex) != focus->GetGUID();
    }

    return false;
}

bool MimironRocketStrikeTrigger::IsActive()
{
    Unit* boss = GetUldUnitByEntry(botAI, NPC_VX001);

    if (!boss || !boss->IsAlive())
        return false;

    Creature* rocketStrikeN = bot->FindNearestCreature(NPC_ROCKET_STRIKE_N, ULDUAR_MIMIRON_ROOM_SEARCH_RANGE);

    if (!rocketStrikeN)
        return false;

    return bot->GetExactDist2d(rocketStrikeN) <= ULDUAR_MIMIRON_ROCKET_RUN_RADIUS;
}

bool MimironPhase4FocusTrigger::IsActive()
{
    if (!IsMimironPhase4(bot))
        return false;

    if (PlayerbotAI::IsMainTank(bot))
    {
        Unit* const focus = GetMimironPhase4TankFocus(botAI, bot);
        return focus && AI_VALUE(Unit*, "current target") != focus;
    }

    Unit* const acuFocus = GetMimironPhase4AcuTankFocus(botAI, bot);
    if (acuFocus && AI_VALUE(Unit*, "current target") != acuFocus)
        return true;

    return AI_VALUE(float, "disperse distance") != ULDUAR_MIMIRON_PHASE4_DISPERSE_DISTANCE;
}

bool MimironProximityMineTrigger::IsActive()
{
    if (PlayerbotAI::IsMelee(bot) && GetMimironRingFocus(botAI))
        return false;

    MimironP3Wx2LaserBarrageTrigger barrage(botAI);
    if (barrage.IsActive())
        return false;

    Position mine;
    return GetMimironCloseMine(botAI, bot, mine);
}

bool MimironBombBotTrigger::IsActive()
{
    return GetMimironBombBotBlast(botAI, bot, ULDUAR_MIMIRON_BOMB_BOT_BLAST_CLEARANCE).valid;
}

bool MimironBombBotTauntTrigger::IsActive()
{
    return ClassTauntReady(botAI) && GetMimironBombBotToTaunt(botAI, bot) != nullptr;
}

bool MimironBombTankHoldTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return target && target->GetEntry() != NPC_BOMB_BOT && IsMimironBombTank(botAI, bot);
}

bool MimironApproachTargetTrigger::IsActive()
{
    if (!IsMimironHardModeActive(botAI) || !IsMimironEngaged(botAI))
        return false;

    MimironP3Wx2LaserBarrageTrigger barrage(botAI);
    if (barrage.IsActive())
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    Position slot;
    if (GetMimironSpreadSlot(botAI, bot, slot))
        return false;

    Position approach;
    return GetMimironTargetApproach(botAI, bot, target, GetMimironApproachRange(botAI, bot),
                                    approach);
}

bool MimironDodgeFlamesTrigger::IsActive()
{
    if (!IsMimironHardModeActive(botAI))
        return false;

    MimironShockBlastTrigger shockBlast(botAI);
    if (shockBlast.IsActive())
        return false;

    MimironRocketStrikeTrigger rocketStrike(botAI);
    if (rocketStrike.IsActive())
        return false;

    MimironFrostBombTrigger frostBomb(botAI);
    if (frostBomb.IsActive())
        return false;

    if (Unit* vx001 = GetUldUnitByEntry(botAI, NPC_VX001))
    {
        MimironBarrageWindow const window = GetMimironBarrageWindow(bot, vx001);
        if (window.valid && !IsMimironSpotBarrageSafe(vx001, window, bot->GetPosition(), 0.0f))
            return false;
    }

    MimironFirefighterHazards const hazards = GetMimironFirefighterHazards(botAI);

    if (IsMimironSpotInFireBotSpray(hazards, bot->GetPosition(), ULDUAR_MIMIRON_FIREBOT_SPRAY_HALF_WIDTH))
        return false;

    uint32 nodes = 0;
    for (Position const& node : hazards.flames)
        if (bot->GetExactDist2d(node.GetPositionX(), node.GetPositionY()) < ULDUAR_MIMIRON_FLAMES_BURN_RADIUS)
            ++nodes;

    if (!nodes)
        return false;

    bool const lowHealth = bot->GetHealthPct() < ULDUAR_MIMIRON_FLAMES_DODGE_HEALTH_PCT;

    if (IsMimironCoreFetchDue(botAI, bot))
        return lowHealth;

    if (nodes >= ULDUAR_MIMIRON_FLAMES_DODGE_NODE_OVERRIDE || lowHealth)
        return true;

    if (bot->isMoving())
        return false;

    return bot->GetHealthPct() < ULDUAR_MIMIRON_FLAMES_PARKED_DODGE_HEALTH_PCT || !GetMimironRingFocus(botAI);
}

bool MimironFireBotTrigger::IsActive()
{
    if (!IsMimironHardModeActive(botAI))
        return false;

    MimironFirefighterHazards const hazards = GetMimironFirefighterHazards(botAI);
    if (IsMimironSpotFireBotSafe(bot, hazards, bot->GetPosition(), ULDUAR_MIMIRON_FIREBOT_SIREN_CLEARANCE,
                                 ULDUAR_MIMIRON_FIREBOT_SPRAY_HALF_WIDTH))
        return false;

    if (IsMimironSpotInFireBotSpray(hazards, bot->GetPosition(), ULDUAR_MIMIRON_FIREBOT_SPRAY_HALF_WIDTH))
        return true;

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (bot->isMoving() && bot->GetMotionMaster()->GetDestination(x, y, z) &&
        IsMimironSpotFireBotSafe(bot, hazards, Position(x, y, z), ULDUAR_MIMIRON_FIREBOT_SIREN_STAND,
                                 ULDUAR_MIMIRON_FIREBOT_SPRAY_STAND_HALF_WIDTH))
        return false;

    return true;
}

bool MimironFrostBombTrigger::IsActive()
{
    if (!IsMimironHardModeActive(botAI))
        return false;

    TooCloseToCreatureTrigger tooCloseToFrostBomb(botAI);
    return tooCloseToFrostBomb.TooCloseToCreature(NPC_FROST_BOMB, ULDUAR_MIMIRON_FROST_BOMB_RADIUS);
}

bool MimironPlasmaBlastDefensiveTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot) && !PlayerbotAI::IsHeal(bot))
        return false;

    Unit* leviathanMkII = GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    if (!leviathanMkII || GetUldUnitByEntry(botAI, NPC_VX001) ||
        GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        return false;

    Creature* cannon = bot->FindNearestCreature(NPC_LEVIATHAN_MKII_CANNON, ULDUAR_MIMIRON_ROOM_SEARCH_RANGE);
    if (!cannon)
        return false;

    Spell* spell = GetMimironPlasmaBlastCast(cannon);
    if (!spell)
        return false;

    Unit* victim = spell->m_targets.GetUnitTarget();
    return victim && victim->IsAlive() && (victim == bot || PlayerbotAI::IsHeal(bot));
}

bool MimironRedirectThreatTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE)
        return false;

    if (!GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII) ||
        GetUldUnitByEntry(botAI, NPC_VX001) ||
        GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        return false;

    Unit* mainTank = AI_VALUE(Unit*, "main tank");
    return mainTank && mainTank != bot && mainTank->IsAlive();
}

bool MimironMagneticCoreTrigger::IsActive()
{
    if (GetMimironCoreCarrier(botAI) != bot)
        return false;

    if (!GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        return false;

    if (bot->HasItemCount(ITEM_MIMIRON_MAGNETIC_CORE, 1, false))
        return true;

    return GetMimironCoreCorpse(bot) != nullptr;
}

bool MimironPetControlTrigger::IsActive()
{
    if (!bot->GetGuardianPet())
        return false;

    if (IsMimironPhase4(bot))
        return true;

    return IsMimironPhase3(botAI);
}

bool MimironFrostResistanceTrigger::IsActive()
{
    if (bot->getClass() != CLASS_PALADIN || botAI->HasAura("frost resistance aura", bot))
        return false;

    return GetMimironFrostResistancePaladin(botAI, bot) == bot;
}

bool MimironSlowBombBotTrigger::IsActive()
{
    std::string const spell = GetMimironBombBotSnare(bot);
    if (spell.empty())
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || target->GetEntry() != NPC_BOMB_BOT)
        return false;

    if (GetMimironBombBotApproach(bot, target) < ULDUAR_MIMIRON_BOMB_BOT_SNARE_MIN_APPROACH)
        return false;

    return !botAI->HasAura(spell, target);
}

bool MimironSetDpsPriorityTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot))
        return false;

    return IsMimironEngaged(botAI) || (!PlayerbotAI::IsHeal(bot) && IsMimironFireBotCullActive(botAI, bot));
}
