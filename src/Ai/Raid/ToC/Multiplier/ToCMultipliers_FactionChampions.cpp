#include "ToCMultipliers_FactionChampions.h"

#include <set>
#include <string>

#include "ToCEncounterGate.h"
#include "ToCHelpers_FactionChampions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "Playerbots.h"
#include "RaidSplash.h"
#include "RogueActions.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
// Bounces and tank splash the generic area test misses
std::set<std::string> const OWN_SPLASH = {"death and decay", "pestilence", "hammer of the righteous",
                                          "avenger's shield", "blood boil", "consecration", "swipe (bear)"};

// Harmful splash only: area buffs like Battle Shout land on the raid
bool Splashes(PlayerbotAI* botAI, Action* action)
{
    if (OWN_SPLASH.count(action->getName()))
        return true;

    if (!SpellSplashes(botAI, action))
        return false;

    CastSpellAction* cast = dynamic_cast<CastSpellAction*>(action);
    if (!cast)
        return true;

    uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", cast->getSpell())->Get();
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    return !info || !info->IsPositive();
}
}  // namespace

// Also every own splash: a Death and Decay or Consecration on the pack breaks every Polymorph and Fear in
// it, and Pestilence puts a DoT on each champion, which rules them out for both
float FactionChampionsSuppressAoeMultiplier::GetValue(Action* action)
{
    // AoE heals land on the raid, not the champions, so they stay
    if (!action || dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    if (action->getThreatType() != Action::ActionThreatType::Aoe && !Splashes(botAI, action))
        return 1.0f;

    return ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions) ? 0.0f : 1.0f;
}

float FactionChampionsThreatRedirectVetoMultiplier::GetValue(Action* action)
{
    bool const mainTankRedirect = dynamic_cast<CastMisdirectionOnMainTankAction*>(action) ||
                                  dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action);
    if (!mainTankRedirect && !dynamic_cast<CastTricksOfTheTradeAction*>(action))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return 1.0f;

    if (mainTankRedirect)
        return 0.0f;

    // Tricks on a melee dps still gives its 15% damage, so only veto it when it picks the tank
    Unit* mainTank = AI_VALUE(Unit*, "main tank");
    return mainTank && AI_VALUE(Unit*, "tricks of the trade target") == mainTank ? 0.0f : 1.0f;
}

bool FactionChampionsAntiFearTotemGuardMultiplier::FearWindowActive()
{
    return FactionChampionsFearWindowActive(botAI);
}

float FactionChampionsCcGuardMultiplier::GetValue(Action* action)
{
    bool const classCc = dynamic_cast<CastCrowdControlSpellAction*>(action);
    bool const spreadDot = dynamic_cast<CastDebuffSpellOnAttackerAction*>(action) ||
                           dynamic_cast<CastDebuffSpellOnMeleeAttackerAction*>(action);
    if ((!classCc && !spreadDot) || !ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return 1.0f;

    if (classCc)
        return 0.0f;

    // a spread DoT on a champion off the kill target blocks Polymorph, Fear and Hex on it for its whole run.
    // GetTarget last, it scans every attacker.
    Unit* target = action->GetTarget();
    return target && target->IsCreature() && IsFactionChampion(target->GetEntry()) &&
                   target != FactionChampionsKillTarget(botAI)
               ? 0.0f
               : 1.0f;
}

namespace
{
bool Interrupts(SpellInfo const* info)
{
    return info->HasEffect(SPELL_EFFECT_INTERRUPT_CAST) || info->HasAura(SPELL_AURA_MOD_SILENCE);
}

bool EndsWith(std::string const& text, std::string const& tail)
{
    return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
}
}  // namespace

float FactionChampionsLateInterruptMultiplier::GetValue(Action* action)
{
    CastSpellAction* cast = dynamic_cast<CastSpellAction*>(action);
    if (!cast || !ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return 1.0f;

    uint32 const spellId = AI_VALUE2(uint32, "spell id", cast->getSpell());
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info)
        return 1.0f;

    if (!Interrupts(info) && !EndsWith(action->getName(), " on enemy healer"))
        return FactionChampionsCounterspellHold(botAI) && info->CalcCastTime(bot) > 0 ? 0.0f : 1.0f;

    // GetTarget last, it scans every attacker
    Unit* target = action->GetTarget();
    if (!target || !target->IsCreature() || !IsFactionChampion(target->GetEntry()))
        return 1.0f;

    Spell* spell = target->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    return spell && spell->GetCastTimeRemaining() > LATE_INTERRUPT_MS ? 0.0f : 1.0f;
}

void AddToCFactionChampionsMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new FactionChampionsLateInterruptMultiplier(botAI));
    multipliers.push_back(new FactionChampionsSuppressAoeMultiplier(botAI));
    multipliers.push_back(new FactionChampionsThreatRedirectVetoMultiplier(botAI));
    multipliers.push_back(new FactionChampionsAntiFearTotemGuardMultiplier(botAI));
    multipliers.push_back(new FactionChampionsCcGuardMultiplier(botAI));
}

// Lust and every cooldown go at the pull, each early kill makes the rest easier
ToCBurstWindow ToCFactionChampionsBurstWindow(PlayerbotAI* /*botAI*/)
{
    return {true, true};
}
