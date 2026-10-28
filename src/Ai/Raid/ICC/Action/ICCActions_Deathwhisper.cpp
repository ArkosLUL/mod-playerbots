/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_Deathwhisper.h"

#include <algorithm>
#include <cmath>

#include "EncounterHelpers.h"
#include "ICCHelpers_Shared.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "SpellMgr.h"
#include "Timer.h"

namespace
{
// MoveTo refuses a repeat of its last point for up to 5 s even once the bot has stopped, so a walk a
// knockback or an item use cut short is dropped this long after it went out.
constexpr uint32 LDW_STALL_MS = 500;
// A walk passing this close to the shade chasing the bot gets swung at on the way.
constexpr float LDW_SHADE_CONTACT = 6.0f;
constexpr float LDW_CC_STANDOFF = 3.0f;
}  // namespace

bool IccDeathwhisperDodgeAction::Dodge(std::vector<LdwHazard> const& hazards, LdwHazardKind kind)
{
    if (hazards.empty())
        return false;

    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    if (!bot->isMoving() && last.msTime && getMSTimeDiff(last.msTime, getMSTime()) > LDW_STALL_MS)
        last.clear();

    auto outsideAll = [&hazards](float x, float y)
    {
        return std::none_of(hazards.begin(), hazards.end(), [x, y](LdwHazard const& hazard)
                            { return hazard.pos.GetExactDist2d(x, y) < hazard.trigger; });
    };

    if (bot->isMoving() && last.msTime && last.priority == MovementPriority::MOVEMENT_FORCED &&
        outsideAll(last.lastMoveToX, last.lastMoveToY))
        return true;

    Position const from = bot->GetPosition();
    auto accept = [&hazards, &from](float x, float y)
    {
        Position const to(x, y, from.GetPositionZ());
        for (LdwHazard const& hazard : hazards)
        {
            if (hazard.kind != LdwHazardKind::Shade || from.GetExactDist2d(hazard.pos) < LDW_SHADE_CONTACT)
                continue;

            if (EncounterHelpers::SegmentCrossesCircle(from, to, {hazard.pos, LDW_SHADE_CONTACT}))
                return false;
        }

        return true;
    };

    // Melee keep as close to their target as the hazards allow.
    Unit* target = AI_VALUE(Unit*, "current target");
    Position const prefer = target ? target->GetPosition() : from;

    // A spot behind a torch post costs the bot its target, but a shade's blast kills, so there the
    // nearest clear spot wins.
    bool const lethal = kind == LdwHazardKind::Shade || kind == LdwHazardKind::Blast;
    auto acceptInSight = [&accept, target, &from, lethal](float x, float y)
    { return accept(x, y) && (lethal || !target || target->IsWithinLOS(x, y, from.GetPositionZ())); };

    std::vector<EncounterHelpers::HazardCircle> circles;
    for (LdwHazard const& hazard : hazards)
        circles.push_back({hazard.pos, hazard.clear});

    Position spot = EncounterHelpers::FindNearestPositionClearOfHazards(bot, circles, LDW_DODGE_SWEEP, 2.0f,
                                                                        static_cast<float>(M_PI) / 8.0f, &prefer,
                                                                        acceptInSight);
    if (LdwIsEmpty(spot) && !lethal && target)
        spot = EncounterHelpers::FindNearestPositionClearOfHazards(bot, circles, LDW_DODGE_SWEEP, 2.0f,
                                                                   static_cast<float>(M_PI) / 8.0f, &prefer, accept);

    if (LdwIsEmpty(spot))
    {
        // Overlapping pools can leave nothing clear by the margin; clearing the reach itself still saves the hit.
        circles.clear();
        for (LdwHazard const& hazard : hazards)
            circles.push_back({hazard.pos, hazard.trigger + 0.5f});

        spot = EncounterHelpers::FindNearestPositionClearOfHazards(bot, circles, LDW_DODGE_SWEEP, 2.0f,
                                                                   static_cast<float>(M_PI) / 8.0f, &prefer);
    }

    if (LdwIsEmpty(spot))
        return false;

    // A shade or its blast kills, so a cast that would pin the bot gives way.
    if ((kind == LdwHazardKind::Shade || kind == LdwHazardKind::Blast) && bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);

    if (kind == LdwHazardKind::Shade && EncounterHelpers::NitroBoostsReady(bot))
        EncounterHelpers::UseNitroBoosts(bot);

    // The bot's own forced walk would refuse this re-aim at the same priority.
    last.clear();
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_FORCED, true);
}

bool IccDarkReckoningAction::Execute(Event /*event*/)
{
    return Dodge(LdwDarkReckoningHazards(botAI), LdwHazardKind::DarkReckoning);
}

bool IccLadyDeathwhisperDodgeAction::Execute(Event /*event*/)
{
    LdwHazardKind const kind = LdwDodgeKind(botAI);
    if (kind == LdwHazardKind::None)
        return false;

    return Dodge(LdwHazardsFor(botAI), kind);
}

bool IccLadyDeathwhisperFrostboltAction::Execute(Event /*event*/)
{
    Creature* boss = LdwBoss(botAI);
    char const* interrupt = boss ? LdwReadyInterrupt(bot, boss) : nullptr;
    return interrupt && botAI->CastSpell(interrupt, boss);
}

bool IccLadyDeathwhisperDefensiveAction::Execute(Event /*event*/)
{
    uint32 const defensive = LdwTankDefensive(botAI, true);
    return defensive && botAI->CastSpell(defensive, bot);
}

bool IccLadyDeathwhisperDominateMindAction::Execute(Event /*event*/)
{
    char const* spell = nullptr;
    bool waiting = false;
    Unit* victim = LdwCcDuty(botAI, spell, waiting);
    if (!victim || !spell)
        return false;

    uint32 const spellId = AI_VALUE2(uint32, "spell id", spell);
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info)
        return false;

    float const range = bot->GetSpellMaxRangeForTarget(victim, info);
    if (!bot->IsWithinCombatRange(victim, range) || !bot->IsWithinLOSInMap(victim))
        return MoveTo(victim, std::max(range - LDW_CC_STANDOFF, 1.0f), MovementPriority::MOVEMENT_COMBAT);

    // In place under a Cyclone: the cast goes out the tick it fades.
    if (waiting)
        return false;

    LdwLeaveFormFor(botAI, spell);
    return botAI->CastSpell(spell, victim);
}

bool IccLadyDeathwhisperTauntAction::Execute(Event /*event*/)
{
    Unit* target = LdwLooseAddToTaunt(botAI);
    if (!target && LdwSwapTaunt(botAI))
        target = LdwBoss(botAI);

    if (!target || !IccCastClassTaunt(bot, botAI, target))
        return false;

    // A taunt moves the add, not the bot's own target.
    AttackDirect(target);
    return true;
}
