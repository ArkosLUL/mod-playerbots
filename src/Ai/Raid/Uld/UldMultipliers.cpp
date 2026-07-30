#include "UldMultipliers.h"

#include <set>
#include <string>

#include "BurstCooldowns.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PriestActions.h"
#include "RogueActions.h"
#include "Timer.h"
#include "UldBossHelper.h"
#include "UldHardMode.h"

// Algalon the Observer
// Reserve Dispersion for the designated Big Bang soaker priest. Big Bang is unavoidable raid-wide
// damage; the soaker survives it via Dispersion (90% reduction). Blocking the normal low-mana /
// critical-health Dispersion casts keeps the cooldown up for every Big Bang.
float AlgalonMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CastDispersionAction*>(action))
        return 1.0f;

    Unit* boss = AI_VALUE2(Unit*, "find target", "algalon observer");
    if (!boss || !boss->IsAlive())
        return 1.0f;

    // Only the designated soaker priest reserves the cooldown; other priests disperse normally.
    if (GetAlgalonBigBangSoakerPriest(bot) != bot)
        return 1.0f;

    // During the actual Big Bang cast the soak action must be free to spend Dispersion.
    if (boss->HasUnitState(UNIT_STATE_CASTING) && boss->FindCurrentSpellBySpellId(SPELL_ALGALON_BIG_BANG))
        return 1.0f;

    // Otherwise block Dispersion so it is available for the next Big Bang.
    return 0.0f;
}

// XT-002 Deconstructor
//
// The exposed Heart transfers its damage to XT, so it is the only real burst window the encounter
// offers - but spending cooldowns there is exactly what kills the Heart and triggers hard mode. The
// two modes therefore want opposite behaviour, and the config is the only statement of intent.
float XT002BurstWindowMultiplier::GetValue(Action* action)
{
    if (!action || !IsBurstCooldownAction(action->getName()))
        return 1.0f;

    uint32 now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        cachedValue = EvaluateWindow();
    }
    return cachedValue;
}

float XT002BurstWindowMultiplier::EvaluateWindow()
{
    Unit* xt002 = GetXT002(botAI);
    if (!xt002)
        return 1.0f;

    // Heartbreak is up: there will be no further Heart phase, so there is nothing left to save for.
    if (IsXT002HeartbreakActive(botAI))
        return 1.0f;

    if (GetXT002ExposedHeart(botAI))
        return IsXT002HardModeActive(botAI) ? 1.0f : 0.0f;

    // Normal mode: the last Heart phase is behind us below this, so the push is the window.
    if (!IsXT002HardModeActive(botAI) && xt002->GetHealthPct() < ULDUAR_XT002_FINAL_PUSH_HP_PCT)
        return 1.0f;

    return 0.0f;
}

float XT002TargetGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    Unit* xt002 = GetXT002(botAI);
    if (!xt002)
        return 1.0f;

    // The class-generic redirects buff whoever the group flags as main tank, which is the wrong sink
    // while a Pummeller is out; xt002 redirect threat action picks the tank that actually needs it.
    if (dynamic_cast<CastMisdirectionOnMainTankAction*>(action) ||
        dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action))
    {
        return 0.0f;
    }

    if (IsXT002HardModeActive(botAI))
        return 1.0f;

    // Normal mode safety floor. The attack-heart trigger goes false here too, but that only stops
    // bots from picking the Heart up again - this is what stops the ones already swinging at it from
    // landing the hit that flips the raid into hard mode.
    Unit* heart = GetXT002ExposedHeart(botAI);
    if (!heart || heart->GetHealthPct() > ULDUAR_XT002_HEART_SAFE_HP_PCT)
        return 1.0f;

    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    if (currentTarget != heart)
        return 1.0f;

    // Only the actions that would land a hit on the Heart are blocked. Blocking everything would
    // leave the bot parked in a Gravity Bomb or a Void Zone with no heals and no way to retarget.
    if (dynamic_cast<MovementAction*>(action) || dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    static std::set<std::string> const retargets = {"attack rti target", "xt002 mark kill target action",
                                                    "xt002 boombot ranged kill action", "xt002 pummeller taunt action",
                                                    "xt002 redirect threat action"};

    return retargets.count(action->getName()) ? 1.0f : 0.0f;
}
