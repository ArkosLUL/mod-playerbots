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
std::set<std::string> const OWN_SPLASH = {"death and decay", "pestilence", "hammer of the righteous",
                                          "avenger's shield", "blood boil", "consecration", "swipe (bear)",
                                          "fire elemental totem", "searing totem", "starfall"};
}

bool FactionChampionsSuppressAoeMultiplier::Splashes(Action* action)
{
    RaidSplashCache::Facts& facts = _splash.Of(action);
    bool const ownSplash = _ownSplash.Get(action, [action] { return OWN_SPLASH.count(action->getName()) != 0; });

    if (ownSplash || facts.summon || facts.multiTarget)
        return true;

    if (!SpellSplashes(botAI, action, _splash))
        return false;

    if (!facts.cast)
        return true;

    uint32 const spellId = facts.spellId.Get(botAI, facts.cast);
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    return !info || !info->IsPositive();
}

float FactionChampionsSuppressAoeMultiplier::GetValue(Action* action)
{
    if (!action || _splash.Of(action).heal)
        return 1.0f;

    if (!_splash.Of(action).aoeThreat && !Splashes(action))
        return 1.0f;

    return ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions) ? 0.0f : 1.0f;
}

float FactionChampionsThreatRedirectVetoMultiplier::GetValue(Action* action)
{
    auto const classify = [action]
    {
        Facts facts;
        facts.mainTankRedirect = dynamic_cast<CastMisdirectionOnMainTankAction*>(action) ||
                                 dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action);
        facts.tricks = dynamic_cast<CastTricksOfTheTradeAction*>(action);
        return facts;
    };

    Facts const& facts = _facts.Get(action, classify);
    if (!facts.mainTankRedirect && !facts.tricks)
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return 1.0f;

    if (facts.mainTankRedirect)
        return 0.0f;

    Unit* mainTank = AI_VALUE(Unit*, "main tank");
    return mainTank && AI_VALUE(Unit*, "tricks of the trade target") == mainTank ? 0.0f : 1.0f;
}

bool FactionChampionsAntiFearTotemGuardMultiplier::FearWindowActive()
{
    return FactionChampionsFearWindowActive(botAI);
}

float FactionChampionsCcGuardMultiplier::GetValue(Action* action)
{
    auto const classify = [action]
    {
        Facts facts;
        facts.classCc = dynamic_cast<CastCrowdControlSpellAction*>(action);
        facts.spreadDot = dynamic_cast<CastDebuffSpellOnAttackerAction*>(action) ||
                          dynamic_cast<CastDebuffSpellOnMeleeAttackerAction*>(action);
        return facts;
    };

    Facts const& facts = _facts.Get(action, classify);
    if ((!facts.classCc && !facts.spreadDot) || !ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return 1.0f;

    if (facts.classCc)
        return 0.0f;

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
}

float FactionChampionsLateInterruptMultiplier::GetValue(Action* action)
{
    auto const classify = [action]
    {
        Facts facts;
        facts.cast = dynamic_cast<CastSpellAction*>(action);
        facts.healerKick = facts.cast && EndsWith(action->getName(), " on enemy healer");
        return facts;
    };

    Facts& facts = _facts.Get(action, classify);
    if (!facts.cast || !ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return 1.0f;

    uint32 const spellId = facts.spellId.Get(botAI, facts.cast);
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info)
        return 1.0f;

    if (!Interrupts(info) && !facts.healerKick)
        return FactionChampionsCounterspellHold(botAI) && info->CalcCastTime(bot) > 0 ? 0.0f : 1.0f;

    Unit* target = action->GetTarget();
    if (!target || !target->IsCreature() || !IsFactionChampion(target->GetEntry()))
        return 1.0f;

    Spell* spell = target->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    return spell && spell->GetCastTimeRemaining() > LATE_INTERRUPT_MS ? 0.0f : 1.0f;
}

RaidBurstWindow ToCFactionChampionsBurstWindow(PlayerbotAI* /*botAI*/)
{
    return {true, true};
}
