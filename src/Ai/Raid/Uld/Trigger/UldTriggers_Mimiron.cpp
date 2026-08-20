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
    // By entry, not through "find target": that value walks the bot's own threat list, so it only ever
    // resolves a boss this bot is already on. A bot that has not damaged the mech has to dodge it too.
    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII);

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

    // Centre to centre. GetDistance2d would take off the MK II's combat reach of 8 and the bot's own
    // 1.5 first, which turned a 15 yd blast into a 24.5 yd panic and had the whole ranged ring running
    // from a spell that cannot touch it.
    return bot->GetExactDist2d(boss) < ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST;
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
    // By entry. VX-001 never calls DoZoneInCombat in phase 4, so "find target" leaves any bot that has
    // not damaged it blind to a cone that kills in a single tick.
    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_VX001);

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
    // Stand down for the whole Spinning Up window and barrage: the dodge owns positioning then, and
    // walking a bot back to its ring slot mid-cone kills it.
    MimironP3Wx2LaserBarrageTrigger barrage(botAI);
    if (barrage.IsActive())
        return false;

    // No "is a mech up" gate of its own. GetMimironSpreadSlot answers false when neither a live nor a
    // staging focus resolves, and that is also what keeps this quiet before the pull and after a wipe.
    Position slot;
    if (!GetMimironSpreadSlot(botAI, bot, slot))
        return false;

    // The test is on the slot, not the bot. A Rocket Strike prefers targets past 15 yd, which is the
    // ring itself, so a bot that dodged one is standing clear while its slot still has the marker
    // burning on it - checking the bot's own surroundings would send it straight back.
    if (!IsMimironTankAnchorSlot(botAI, bot) && !IsMimironSpotSafe(bot, slot))
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

    // Nothing for anyone else. Spacing in this phase comes from the staging slots, which already hold
    // the raid ULDUAR_MIMIRON_PHASE3_SPACING apart - more than the 5 yd a Bomb Bot blast covers.
    return false;
}

bool MimironRocketStrikeTrigger::IsActive()
{
    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_VX001);

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
        return false;

    Creature* rocketStrikeN = bot->FindNearestCreature(NPC_ROCKET_STRIKE_N, 100.0f);

    if (!rocketStrikeN)
        return false;

    return bot->GetDistance2d(rocketStrikeN->GetPositionX(), rocketStrikeN->GetPositionY()) <= 10.0f;
}

bool MimironPhase4FocusTrigger::IsActive()
{
    if (!IsMimironPhase4(bot))
        return false;

    if (botAI->IsMainTank(bot))
    {
        Unit* const focus = GetMimironPhase4Focus(botAI, bot, true);
        Unit* const current = AI_VALUE(Unit*, "current target");

        // A null focus means everything the tank may touch is already at the floor, so it has to stop
        // swinging - fire once while it is still attacking so the action can. No raid target icon any
        // more: nothing ever read it back, and a stale skull only ever misled the human raid leader.
        if (!focus)
            return current != nullptr || bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING);

        return current != focus;
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
    // 9000 damage in 3 yd, and a mine self-destructs after 35 s whether anyone is near it or not. Ten
    // scatter within 15 yd of the MK II every 30 s, which is where melee have to stand, so this fired
    // more or less continuously to dodge about 120 dps - and cost them their uptime for it.
    if (botAI->IsMelee(bot))
        return false;

    // The barrage action hands the tick to everything below it once a bot is clear, and this node
    // knows nothing about the cone. Suppressing it beats filtering: a mine is survivable, 20000 every
    // 250 ms is not.
    MimironP3Wx2LaserBarrageTrigger barrage(botAI);
    if (barrage.IsActive())
        return false;

    TooCloseToCreatureTrigger tooCloseToProximityMine(botAI);
    return tooCloseToProximityMine.TooCloseToCreature(NPC_PROXIMITY_MINE,
                                                     ULDUAR_MIMIRON_MINE_TRIGGER_RADIUS);
}

bool MimironBombBotTrigger::IsActive()
{
    // A Bomb Bot runs 8.0 yd/s against a player's 7.0, so nobody outruns one - what kills it is that it
    // also dies to almost nothing. Ranged DPS that can reach it shoot it instead ("mimiron set dps
    // priority" hands them the target); healers and melee keep the sidestep, which is all 5 yd costs.
    if (PlayerbotAI::IsRangedDps(bot) &&
        bot->FindNearestCreature(NPC_BOMB_BOT, sPlayerbotAIConfig.spellDistance))
        return false;

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

    // The cannon is a passenger of the MK II and is what actually casts. Taunting while it casts is
    // always one cast too late - the victim was resolved when the cast began and nothing moves it now -
    // so this deliberately fires in the gaps instead, and the taunt owns the next cast 22 s out.
    Creature* cannon = bot->FindNearestCreature(NPC_LEVIATHAN_MKII_CANNON, 100.0f);
    if (!cannon || cannon->FindCurrentSpellBySpellId(SPELL_MIMIRON_PLASMA_BLAST))
        return false;

    // Whoever is not already holding it takes it. That alternates the two tanks by itself, one taunt
    // each per 22s cycle, so each tank only taunts every 44s and never trips the 15s taunt-DR window.
    return leviathanMkII->GetVictim() != bot;
}

bool MimironMagneticCoreTrigger::IsActive()
{
    // One bot per instance owns this, so two carriers cannot burn two cores on the same landing.
    if (GetMimironCoreCarrier(botAI) != bot)
        return false;

    Unit* aerialCommandUnit = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
    if (!aerialCommandUnit)
        return false;

    // Already grounded; nothing to do until it lifts off again.
    if (!aerialCommandUnit->HasUnitMovementFlag(MOVEMENTFLAG_HOVER))
        return false;

    if (bot->HasItemCount(ITEM_MIMIRON_MAGNETIC_CORE, 1, false))
        return true;

    // Corpses linger 25 s and the Assault Bot dies wherever the raid stopped it, so the search has to
    // cover the room: the old 5 yd test only passed if the carrier happened to already be standing on
    // one, which is why the core never reached the Aerial Command Unit. The walk itself is the action's.
    return bot->FindNearestCreature(NPC_ASSAULT_BOT, ULDUAR_MIMIRON_CORE_SEARCH_RANGE, false) != nullptr;
}

bool MimironPetControlTrigger::IsActive()
{
    if (!bot->GetGuardianPet())
        return false;

    if (IsMimironPhase4(bot))
        return true;

    // Phase 3, tested the same way MimironAerialCommandUnitTrigger does. Not by MOVEMENTFLAG_HOVER on
    // its own: the flag survives the phase 3 defeat and the vehicle boarding, so it says nothing about
    // which phase this is, and keying off it used to stop every pet in the raid for all of phase 4.
    return GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT) &&
           !GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) &&
           !GetFirstAliveUnitByEntry(botAI, NPC_VX001);
}

bool MimironSlowBombBotTrigger::IsActive()
{
    std::string const spell = GetMimironBombBotSnare(bot);
    if (spell.empty())
        return false;

    // No target handling of its own. "mimiron set dps priority" already puts ranged DPS on a Bomb Bot
    // once one is inside casting range, and reading the target back is what keeps this node from
    // fighting it - which also means healers never snare, deliberately.
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || target->GetEntry() != NPC_BOMB_BOT)
        return false;

    // Below this it is already on top of somebody and the global is better spent on damage.
    if (GetMimironBombBotApproach(bot, target) < ULDUAR_MIMIRON_BOMB_BOT_SNARE_MIN_APPROACH)
        return false;

    return !botAI->HasAura(spell, target);
}

bool MimironSetDpsPriorityTrigger::IsActive()
{
    // Tanks hold what the tank nodes give them; this one owns "current target" for everybody else.
    if (botAI->IsTank(bot))
        return false;

    // Engaged, not present: the action calls Attack() directly, and MK II sits in the room from the
    // moment the raid walks in.
    return IsMimironEngaged(botAI);
}
