#include "ToCActions_Jaraxxus.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "EncounterHelpers.h"
#include "LastMovementValue.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Timer.h"
#include "ToCData.h"
#include "ToCHelpers_Jaraxxus.h"
#include "Unit.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

namespace
{
// how long a latched flame spot is trusted while the walk to it is in flight
constexpr uint32 FLAME_LATCH_MS = 3000;
// standing still this long past the issue means something stopped the walk short
constexpr uint32 FLAME_STALL_MS = 500;
// a gap this long between ticks is a new flame, nothing latched carries over
constexpr uint32 FLAME_WINDOW_GAP_MS = 2000;
// DragBossToAnchor's step, and how close to its anchor it stops
constexpr float DRAG_STEP = 5.0f;
constexpr float DRAG_DONE = 12.0f;
}  // namespace

bool JaraxxusInterruptFelFireballAction::Execute(Event /*event*/)
{
    Unit* jaraxxus = GetJaraxxus(botAI);
    if (!jaraxxus)
        return false;

    char const* interrupt = JaraxxusReadyInterrupt(bot, jaraxxus);
    return interrupt && botAI->CastSpell(interrupt, jaraxxus);
}

void JaraxxusAvoidLegionFlameAction::ClearLatches()
{
    _hasSpot = false;
    _hasHeading = false;
}

bool JaraxxusAvoidLegionFlameAction::IssueLeg(Position const& spot, uint32 now)
{
    // our last leg still holds the FORCED lock and a lock only yields to a higher priority, so step
    // it down for the replacement and put it back if nothing went out
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    bool const replacing =
        last.priority == MovementPriority::MOVEMENT_FORCED &&
        last.lastMoveShort.GetExactDist2d(_lastIssued.GetPositionX(), _lastIssued.GetPositionY()) < 1.0f;
    if (replacing)
        last.priority = MovementPriority::MOVEMENT_COMBAT;

    RaidObs::MoveOutcome const outcome =
        TryMoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_FORCED);

    if (outcome == RaidObs::MoveOutcome::Issued)
    {
        _legStart = bot->GetPosition();
        _lastIssued = spot;
        _spot = spot;
        _hasSpot = true;
        _issuedMs = now;
        return true;
    }

    if (replacing)
        last.priority = MovementPriority::MOVEMENT_FORCED;

    // already walking there
    if (outcome == RaidObs::MoveOutcome::Duplicate)
    {
        _spot = spot;
        _hasSpot = true;
        _issuedMs = now;
        return true;
    }

    _hasSpot = false;
    return false;
}

bool JaraxxusAvoidLegionFlameAction::Execute(Event /*event*/)
{
    uint32 const now = getMSTime();
    if (!_lastTickMs || getMSTimeDiff(_lastTickMs, now) > FLAME_WINDOW_GAP_MS)
        ClearLatches();
    _lastTickMs = now;

    JaraxxusFlameRole const role = GetJaraxxusFlameRole(bot);
    if (role == JaraxxusFlameRole::None)
    {
        ClearLatches();
        return false;
    }

    // a running cast pins the feet, the spline never launches while it lasts
    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);

    if (role != JaraxxusFlameRole::Carrier)
        _hasHeading = false;
    else if (!_hasHeading)
    {
        // a dodge picked before the flame landed on us isn't a leg away from the raid
        _hasSpot = false;
        _heading = GetLegionFlameCarrierHeading(bot);
        _hasHeading = true;
    }

    // every MoveTo clears the motion master, so re-deriving under a walk in flight would restart it
    // each tick
    if (_hasSpot)
    {
        uint32 const age = getMSTimeDiff(_issuedMs, now);
        bool const arrived = bot->GetExactDist2d(&_spot) <= JARAXXUS_FLAME_ARRIVE;
        bool const stalled = !arrived && !bot->isMoving() && age > FLAME_STALL_MS;

        if (!arrived && !stalled && age < FLAME_LATCH_MS && IsLegionFlameSpotClear(botAI, _spot))
            return true;

        _hasSpot = false;

        if (arrived && role == JaraxxusFlameRole::Dodge)
            return false;

        // the next leg carries straight on, never back over the trail
        if (role == JaraxxusFlameRole::Carrier)
            _heading = _legStart.GetAngle(&_spot);

        // MoveTo refuses the same point for 5 s, moving or not
        if (stalled)
            AI_VALUE(LastMovement&, "last movement").clear();
    }

    Position spot;
    if (!DeriveLegionFlameSpot(bot, role, _heading, spot))
        return false;

    return IssueLeg(spot, now);
}

bool JaraxxusBreakPinnedCastAction::Execute(Event /*event*/)
{
    for (Player* caster : GetJaraxxusPinnedCasters(bot))
        if (PlayerbotAI* casterAI = GET_PLAYERBOT_AI(caster))
            casterAI->RequestSpellInterrupt();

    return false;
}

bool JaraxxusIntroMainTankStandAction::Execute(Event /*event*/)
{
    Unit* jaraxxus = GetJaraxxusInIntro(botAI);
    if (!jaraxxus || bot->GetExactDist2d(jaraxxus) <= JARAXXUS_INTRO_STAND + 1.0f)
        return false;

    float const angle = jaraxxus->GetAngle(bot);
    float const x = jaraxxus->GetPositionX() + std::cos(angle) * JARAXXUS_INTRO_STAND;
    float const y = jaraxxus->GetPositionY() + std::sin(angle) * JARAXXUS_INTRO_STAND;
    return MoveTo(bot->GetMapId(), x, y, jaraxxus->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_NORMAL);
}

bool JaraxxusMainTankHoldBossAction::Execute(Event /*event*/)
{
    Unit* jaraxxus = GetJaraxxus(botAI);
    if (!jaraxxus)
        return false;

    MarkTargetWithSkull(bot, jaraxxus);
    SetRtiTarget(botAI, "skull", jaraxxus);

    if (AI_VALUE(Unit*, "current target") != jaraxxus)
        return Attack(jaraxxus);

    float const toCenter = bot->GetExactDist2d(ARENA_CENTER);
    if (jaraxxus->GetVictim() != bot || toCenter <= DRAG_DONE)
        return false;

    // only the step about to be walked: a tank-carried trail runs along the whole way back for 60 s
    float const scale = std::min(DRAG_STEP, toCenter) / toCenter;
    Position const step(bot->GetPositionX() + (ARENA_CENTER.GetPositionX() - bot->GetPositionX()) * scale,
                        bot->GetPositionY() + (ARENA_CENTER.GetPositionY() - bot->GetPositionY()) * scale,
                        bot->GetPositionZ());
    if (!LegionFlameCrossesPath(bot, step))
        return DragBossToAnchor(jaraxxus, ARENA_CENTER);

    Position detour;
    if (!DeriveLegionFlameDetour(bot, ARENA_CENTER, DRAG_STEP, detour))
        return false;

    return MoveTo(bot->GetMapId(), detour.GetPositionX(), detour.GetPositionY(), detour.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT, true, true);
}

bool JaraxxusAssistTankHoldAction::HoldAdd(uint8 index)
{
    Unit* add = GetJaraxxusAssistTankAdd(botAI, index);
    if (!add)
        return false;

    if (index == 0)
    {
        MarkTargetWithSquare(bot, add);
        SetRtiTarget(botAI, "square", add);
    }
    else
    {
        MarkTargetWithDiamond(bot, add);
        SetRtiTarget(botAI, "diamond", add);
    }
    JaraxxusClaimRti(bot);

    bool attacked = false;
    if (AI_VALUE(Unit*, "current target") != add)
        attacked = Attack(add);

    // the add is this tank's alone, so it comes back off anyone else, tanks too: Fel Streak resets the
    // Infernal's threat onto whoever it charged, and a taunt copies the top threat back
    Unit* victim = add->GetVictim();
    if (victim && victim != bot && victim->ToPlayer() && CastClassTaunt(botAI, add))
        return true;

    return attacked;
}

bool JaraxxusAssistTankHoldAddAction::Execute(Event /*event*/) { return HoldAdd(0); }

bool JaraxxusAssistTankHoldSecondAddAction::Execute(Event /*event*/) { return HoldAdd(1); }

bool JaraxxusRemoveNetherPowerAction::Execute(Event /*event*/)
{
    Unit* jaraxxus = GetJaraxxus(botAI);
    if (!jaraxxus || !JaraxxusHasNetherPower(jaraxxus))
        return false;

    static std::vector<std::string> const dispels = {"spellsteal", "purge", "dispel magic"};
    for (std::string const& dispel : dispels)
    {
        if (botAI->CanCastSpell(dispel, jaraxxus))
            return botAI->CastSpell(dispel, jaraxxus);
    }

    return false;
}

bool JaraxxusFocusAddAction::Execute(Event /*event*/)
{
    Unit* add = GetJaraxxusFocusAdd(botAI);
    if (!add)
        return false;

    MarkTargetWithCross(bot, add);
    // the default rti sits on the boss, so without this dps assist yanks the bot back to him every tick
    SetRtiTarget(botAI, "cross", add);
    JaraxxusClaimRti(bot);
    CommandPetAttack(botAI, add);

    if (AI_VALUE(Unit*, "current target") != add)
        return Attack(add);

    return false;
}

bool JaraxxusResetFocusAction::Execute(Event /*event*/)
{
    SetRtiTarget(botAI, "skull");
    context->GetValue<Unit*>("rti target")->Set(nullptr);
    JaraxxusReleaseRti(bot);
    return false;
}

bool JaraxxusHealIncinerateTargetAction::Execute(Event /*event*/)
{
    Unit* target = GetIncinerateFleshTarget(botAI);
    if (!target)
        return false;

    // instants, HoTs and a channel first
    static std::vector<std::string> const heals = {
        "riptide", "holy shock", "penance", "renew", "rejuvenation", "lifebloom", "wild growth",
        "prayer of mending", "flash heal", "greater heal", "nourish", "healing touch", "regrowth",
        "flash of light", "holy light", "healing wave",
    };

    // the kiss punishes anything still casting on its 0.5 s check, and the bot's own AI cancels a heal
    // still casting on a full-health target, which is what the absorb keeps this one reading
    bool const kissed = HasMistressKiss(bot);
    bool const fullHealth = target->IsFullHealth();
    for (std::string const& heal : heals)
    {
        if (kissed && JaraxxusSpellHasCastTime(botAI, heal))
            continue;

        if (fullHealth && JaraxxusSpellHasCastTime(botAI, heal, false))
            continue;

        if (botAI->CanCastSpell(heal, target))
            return botAI->CastSpell(heal, target);
    }

    return false;
}
