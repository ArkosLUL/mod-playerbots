#include "UldTriggers_Freya.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldEncounter_Freya.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>

using namespace EncounterHelpers;

bool FreyaNearNatureBombTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    // Assist tank 0 eats the bomb: stepping out would lift the Conservator off the spore the melee are
    // sheltering on. The main tank has its own node, which moves Freya rather than just the bot.
    if (botAI->IsTank(bot))
        return false;

    return bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS) != nullptr;
}

bool FreyaTankNatureBombTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    if (!PlayerbotAI::IsMainTank(bot))
        return false;

    return bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS) != nullptr;
}

bool FreyaSetDpsPriorityTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    return PlayerbotAI::IsDps(bot);
}

bool FreyaTankAddsTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    // The ladder ends at Freya, so these two always have something - no need to gather the wave here.
    return PlayerbotAI::IsMainTank(bot) || PlayerbotAI::IsAssistTankOfIndex(bot, 0, true);
}

bool FreyaRedirectThreatTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE)
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");

    return boss && boss->IsAlive();
}

bool FreyaMoveToHealingSporeTrigger::IsActive()
{
    // Check for the Freya boss
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    // Conservator's Grip is a 50000 yd pacify-silence, so melee need a spore just as much as ranged.
    // Tanks are excluded for two different reasons: the add tank already ends up inside the aura by
    // walking the Conservator onto a spore, and moving it here as well would oscillate it between that
    // spore and the one nearest itself. The main tank is out because it is holding Freya on her anchor,
    // and a walk to a spore would take her along with it.
    if (botAI->IsTank(bot))
        return false;

    // By entry, not "find target": that value walks only this bot's threat list, and a pacified bot that
    // has not hit the Conservator yet is exactly the bot that needs a spore.
    if (!GetFirstAliveUnitByEntry(botAI, NPC_ANCIENT_CONSERVATOR))
        return false;

    // The pheromone aura is the thing that matters, and it is exact - a bot can be inside 6 yd of a
    // spore that is still growing and not have it yet.
    if (bot->HasAura(SPELL_POTENT_PHEROMONES))
        return false;

    Unit* spore = GetFreyaTargetSpore(botAI);
    if (!spore)
        return false;

    // Distance stands the node down as well as the aura does. Waiting on the aura alone leaves a window
    // where the bot is already standing on the spore and the node keeps ordering moves onto it, and each
    // of those clears the motion master out from under whatever else was walking somewhere.
    return bot->GetExactDist2d(spore) > ULDUAR_FREYA_SPORE_RADIUS - 1.0f;
}

bool FreyaBreakIronRootsTrigger::IsActive()
{
    if (!IsFreyaHardModeActive(botAI))
        return false;

    // Trapped by either the Ironbranch or the Freya-cast Iron Roots (each leaves its own DoT).
    return bot->HasAura(SPELL_IRON_ROOTS_DAMAGE) || bot->HasAura(SPELL_IRON_ROOTS_FREYA_DAMAGE);
}

bool FreyaDodgeUnstableSunBeamTrigger::IsActive()
{
    if (!IsFreyaHardModeActive(botAI))
        return false;

    // The beam stalkers are non-selectable, so they never show up in attack-target lists - scan the
    // raw nearby-npc list instead.
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (auto const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_FREYA_SUN_BEAM && unit->GetEntry() != NPC_FREYA_UNSTABLE_SUN_BEAM)
            continue;

        if (bot->GetExactDist2d(unit) < ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS)
            return true;
    }

    return false;
}

bool FreyaLasherAboutToBlowTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    // Tanks stay: one that walked would take Freya or the Conservator with it, and it survives the
    // blasts anyway. Ranged and healers are already a camp's width clear.
    if (botAI->IsTank(bot) || !PlayerbotAI::IsMelee(bot))
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    return !GetFreyaLowLasherPositions(botAI, state, ULDUAR_FREYA_LASHER_BAIL_PCT, ULDUAR_FREYA_DETONATE_RADIUS)
                .empty();
}

bool FreyaRangedCampTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    // Melee and tanks are already stacked on whatever they are hitting, and a tank that left the camp
    // would take the boss or the Conservator with it.
    if (botAI->IsTank(bot) || !(PlayerbotAI::IsRangedDps(bot) || botAI->IsHeal(bot)))
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);
    if (state.detonatingLashers.empty())
        return false;

    Position const camp = GetFreyaLasherCampSpot(botAI, state);
    if (camp == Position())
        return false;

    // The tolerance exists to stop churn, not to license standing in a blast that is about to go off,
    // and a healer's 15 yd of slack is the whole Detonate radius. Only the lashers nearly dead count:
    // a healthy one detonates nothing, and walking away from a pack that moves faster than the bot
    // costs more damage than every detonation in the wave.
    if (!GetFreyaLowLasherPositions(botAI, state, ULDUAR_FREYA_LASHER_BAIL_PCT, ULDUAR_FREYA_DETONATE_RADIUS)
             .empty())
        return true;

    // Healers get the wider band: they also have to stay in range of the melee group and the tanks,
    // and pulling them all the way into the ball would leave the far half of the raid unhealed.
    float const tolerance =
        botAI->IsHeal(bot) ? ULDUAR_FREYA_HEALER_CAMP_TOLERANCE : ULDUAR_FREYA_RANGED_CAMP_TOLERANCE;

    return bot->GetExactDist2d(camp.GetPositionX(), camp.GetPositionY()) > tolerance;
}

bool FreyaFrostNovaLashersTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    if (bot->getClass() != CLASS_MAGE)
        return false;

    if (!botAI->CanCastSpell("frost nova", bot))
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    // The nova is a sphere on the caster, so what matters is how many lashers have closed on the mage,
    // not what state a pile somewhere else is in. Two, so a single lasher the mage is already killing
    // does not spend the cooldown.
    return CountFreyaLashersNear(bot->GetPosition(), state, ULDUAR_FREYA_FROST_NOVA_RADIUS) >=
           ULDUAR_FREYA_FROST_NOVA_MIN_LASHERS;
}

bool FreyaTrapLashersTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    if (bot->getClass() != CLASS_HUNTER)
        return false;

    if (!botAI->CanCastSpell("frost trap", bot))
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    // Wider than the trap's own 10 yd field, deliberately: the patch needs to be down and armed before
    // the lasher arrives, and one that has picked this bot is coming here at 8 yd/s whatever it does.
    return CountFreyaLashersNear(bot->GetPosition(), state, ULDUAR_FREYA_FROST_TRAP_ARM_RANGE) > 0;
}

bool FreyaSummonArmyTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    if (bot->getClass() != CLASS_DEATH_KNIGHT)
        return false;

    if (!botAI->CanCastSpell("army of the dead", bot))
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    return !state.detonatingLashers.empty();
}

bool FreyaGroundTremorHoldCastTrigger::IsActive()
{
    if (!IsFreyaHardModeActive(botAI))
        return false;

    if (!IsFreyaGroundTremorCasting(GetFirstAliveUnitByEntry(botAI, NPC_FREYA)))
        return false;

    return bot->HasUnitState(UNIT_STATE_CASTING);
}

bool FreyaNaturesFuryBailTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    // Tanks stay. Whatever they are holding walks after them, so a tank carrying the mark out of the
    // raid takes Freya or the Conservator into it instead.
    if (botAI->IsTank(bot))
        return false;

    if (!bot->HasAura(SPELL_NATURES_FURY_10) && !bot->HasAura(SPELL_NATURES_FURY_25))
        return false;

    return CountFreyaRaidNear(botAI, bot->GetPosition(), ULDUAR_FREYA_NATURES_FURY_RADIUS, bot) > 0;
}

bool FreyaStepOutOfSunbeamTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    // Melee and tanks are stacked on what they are hitting and cannot give up a second and a half of it.
    // They lose little by staying: thirteen of one pull's sixteen beams were aimed at a ranged bot or
    // its pet, which is not where the melee ball is standing.
    if (botAI->IsTank(bot) || !(PlayerbotAI::IsRangedDps(bot) || botAI->IsHeal(bot)))
        return false;

    Unit* target = GetFreyaSunbeamTarget(boss);
    if (!target || target == bot)
        return false;

    return bot->GetExactDist2d(target) <= ULDUAR_FREYA_SUNBEAM_AVOID_RADIUS;
}

bool FreyaTankHoldFreyaTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    // Only whoever actually has her: walking anyone else moves no boss, and a tank that has lost her
    // would drag the melee ring off to where she is not.
    if (!PlayerbotAI::IsMainTank(bot) || boss->GetVictim() != bot)
        return false;

    return boss->GetExactDist2d(&ULDUAR_FREYA_TANK_ANCHOR) > ULDUAR_FREYA_TANK_LEASH;
}
