#include "UldTriggers_Mimiron.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldEncounter_Mimiron.h"
#include "UldHardMode.h"
#include "UldScripts.h"
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
    // -1 is the DisperseDistanceValue default, so anything at or above zero was set by somebody.
    if (bot->GetMapId() != ULDUAR_MAP_ID || AI_VALUE(float, "disperse distance") < 0.0f)
        return false;

    // Same scan the phase 1 node does. Sight distance caps "possible targets" at 100 yd, so at any
    // other boss in here this finds nothing and the value goes back to default before the pull.
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

    // A latch, not a state test: the generic movement layer resets "disperse distance" whenever it
    // rebuilds a formation, so this node has to re-assert it. Both sides go through one accessor,
    // never a literal - drift between the two leaves the trigger permanently active.
    return AI_VALUE(float, "disperse distance") != GetMimironPhase1DisperseDistance(botAI);
}

bool MimironP3Wx2LaserBarrageTrigger::IsActive()
{
    // By entry. VX-001 never calls DoZoneInCombat in phase 4, so "find target" leaves any bot that has
    // not damaged it blind to a cone that kills in a single tick.
    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_VX001);

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
        return false;

    // Spinning Up is the 4s warning and 63274/63300 the 10s barrage itself, but only the latter two
    // are auras on VX-001 - 63414 puts its effects on the DB Target and the MK II, so asking VX-001 for
    // it comes back empty and the raid first hears about a barrage once the beams are already firing.
    // It is a channel on VX-001, which is how the encounter script tracks it too.
    return GetMimironSpinningUpSeconds(boss) >= 0.0f ||
           boss->HasAura(SPELL_P3WX2_LASER_BARRAGE_AURA_1) ||
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
    // burning on it - checking the bot's own surroundings would send it straight back. The tank
    // anchor is exempt: its spot sits under the mech that laid the mines.
    if (!IsMimironTankAnchorSlot(botAI, bot) && !IsMimironSpotSafe(bot, slot))
        return false;

    // Do not walk a bot back into a live Rapid Burst. Tested on the slot rather than through
    // MimironRapidBurstTrigger, which only answers true while the bot is still inside the cone - the
    // formation would otherwise reclaim it the instant the step worked and put it back for the
    // remaining ticks.
    if (Unit* vx001 = GetFirstAliveUnitByEntry(botAI, NPC_VX001))
    {
        MimironRapidBurstWindow const burst = GetMimironRapidBurstWindow(botAI, bot, vx001);
        if (!IsMimironSpotRapidBurstSafe(vx001, burst, slot))
            return false;
    }

    return bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY()) >
           ULDUAR_MIMIRON_SPREAD_TOLERANCE;
}

bool MimironRapidBurstTrigger::IsActive()
{
    Unit* vx001 = GetFirstAliveUnitByEntry(botAI, NPC_VX001);
    if (!vx001)
        return false;

    // The phase 4 main tank holds instead. Its spot is what keeps the chassis parked, and the Laser
    // Barrage cone radiates from a VX-001 that only stays still while the tank does.
    if (PlayerbotAI::IsMainTank(bot) && IsMimironPhase4(bot))
        return false;

    MimironRapidBurstWindow const window = GetMimironRapidBurstWindow(botAI, bot, vx001);

    // Zero escape means already outside the cone. Past the cap the walk does not finish inside the
    // 3 s window and the boss has re-aimed at somebody else before the bot arrives, so standing
    // still and eating it is the cheaper answer.
    return window.valid && window.escape > 0.0f &&
           window.escape <= ULDUAR_MIMIRON_RAPID_BURST_MAX_STEP;
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
    // A Bomb Bot runs 8.0 yd/s against a player's 7.0, so the one it is chasing cannot outrun it -
    // what kills it is that it also dies to almost nothing, and "mimiron set dps priority" hands that
    // bot the target. Everyone else steps out, which is all 5 yd costs. The test used to be any Bomb
    // Bot in spell range, which stood down bystanders the DPS list had already dropped for being too
    // close: inside the blast they neither shot nor moved.
    if (PlayerbotAI::IsRangedDps(bot) && GetMimironBombBotChasing(botAI, bot))
        return false;

    TooCloseToCreatureTrigger tooCloseToBombBot(botAI);
    return tooCloseToBombBot.TooCloseToCreature(NPC_BOMB_BOT, ULDUAR_MIMIRON_BOMB_BOT_RADIUS);
}

bool MimironDodgeFlamesTrigger::IsActive()
{
    if (!IsMimironHardModeActive(botAI))
        return false;

    // Stand down for anything that kills outright. The dodge issues at MOVEMENT_FORCED and so do all
    // of these, and IsWaitingForLastMove wants strictly greater priority, so equal FORCED blocks:
    // whichever ran first owns the lock for its whole leg. A flame node ticks about 3100 against a
    // 22000 pool and fires ~2500 times a pull, so it wins on volume and the escapes lose. Measured:
    // seven bots issued a Shock Blast escape, had it cancelled by a fire leg 1.5 s later, and died
    // to the blast still 9 to 15 yd out.
    MimironP3Wx2LaserBarrageTrigger barrage(botAI);
    if (barrage.IsActive())
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

    // Last of the five: this one walks the group to build its cone window, where the others answer
    // off a cast bar or a nearby creature.
    MimironRapidBurstTrigger rapidBurst(botAI);
    if (rapidBurst.IsActive())
        return false;

    // The fire nodes are non-selectable trigger creatures, so they never show up in attack-target
    // lists - scan the raw nearby-npc list instead.
    uint32 nodes = 0;
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (auto const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_FLAMES_SPREAD && unit->GetEntry() != NPC_FLAMES_INITIAL)
            continue;

        if (bot->GetExactDist2d(unit) < ULDUAR_MIMIRON_FLAMES_RADIUS)
            ++nodes;
    }

    if (!nodes)
        return false;

    // Standing in it beats the round trip while the bot can afford it. One node ticks about 3100
    // against a 22000 to 24000 pool, so a healthy bot has seven ticks of margin and the dodge costs
    // 24 yd of uptime; two nodes halve that margin to about four seconds, which is not enough to
    // notice a health bar moving and then walk out, so they overrule the health gate outright.
    return nodes >= ULDUAR_MIMIRON_FLAMES_DODGE_NODE_OVERRIDE ||
           bot->GetHealthPct() < ULDUAR_MIMIRON_FLAMES_DODGE_HEALTH_PCT;
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

    // Phase 1 only. Nothing later casts it: the cannon is part of the MK II.
    Unit* leviathanMkII = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    if (!leviathanMkII || GetFirstAliveUnitByEntry(botAI, NPC_VX001) ||
        GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        return false;

    // While the cast is up, not between casts: the victim was resolved when it began, and the whole
    // point is having the button down before the first of the six ticks lands.
    Creature* cannon = bot->FindNearestCreature(NPC_LEVIATHAN_MKII_CANNON, 100.0f);
    if (!cannon)
        return false;

    Spell* spell = cannon->FindCurrentSpellBySpellId(SPELL_MIMIRON_PLASMA_BLAST);
    if (!spell)
        return false;

    Unit* victim = spell->m_targets.GetUnitTarget();
    return victim && victim->IsAlive() && (victim == bot || PlayerbotAI::IsHeal(bot));
}

bool MimironRedirectThreatTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE)
        return false;

    // Phase 1 only, and only while somebody else is the main tank.
    if (!GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) ||
        GetFirstAliveUnitByEntry(botAI, NPC_VX001) ||
        GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
    {
        return false;
    }

    Unit* mainTank = AI_VALUE(Unit*, "main tank");
    return mainTank && mainTank != bot && mainTank->IsAlive();
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
