/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include <functional>

#include "ICCActions_RaidWide.h"
#include "ICCEncounterGate.h"
#include "ICCSeams.h"
#include "ICCTriggers_RaidWide.h"
#include "PlayerbotAI.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool IccResistPaladinHolding(PlayerbotAI* botAI) { return IsRaidResistPaladinHolding(botAI, ICC_RESIST_AURA); }

bool IccResistHunterHolding(PlayerbotAI* botAI) { return IsRaidResistHunterHolding(botAI, ICC_RESIST_AURA); }

void DefineRaidWide(EncounterBuilder& e)
{
    // Half steps, so no encounter row ties with these
    e.Node<IccResistAuraTrigger, IccRaiseResistAuraAction>(ACTION_RAID + 1.5f);
    // Above the resistance aura: the swing lands on the next attack timer, not the next GCD
    e.Node<IccOpenerSwingTrigger, IccOpenerStopSwingAction>(ACTION_RAID + 2.5f);
    e.Node<IccOpenerRedirectTrigger, IccOpenerRedirectAction>(ACTION_RAID + 0.5f);

    // The aura slot is exclusive, so the paladin's own buff strategy trades it back on the next GCD. The
    // resistance auras are on the list too: the raise action casts outside the queue.
    e.Block("icc resistance aura hold", Role::Any, IccResistPaladinHolding, 0,
            {"devotion aura", "retribution aura", "concentration aura", "crusader aura", "sanctity aura",
             "fire resistance aura", "frost resistance aura", "shadow resistance aura"});

    // Every aspect, not just Dragonhawk: a vetoed node still pushes its alternatives, and the hunter
    // buff chain runs dragonhawk, hawk, monkey.
    e.Block("icc nature aspect hold", Role::Any, IccResistHunterHolding, 0,
            {"aspect of the viper", "aspect of the dragonhawk", "aspect of the hawk", "aspect of the monkey",
             "aspect of the cheetah", "aspect of the pack"});

    e.Multiplier<RaidOpenerHoldMultiplier>(Family::Spell | Family::Attack, "icc opener hold", std::cref(ICC_OPENER));
}
}  // namespace

EncounterDefinition const& IccRaidWideDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_NONE, IccRaidWideGate, nullptr, &DefineRaidWide);
    return definition;
}
