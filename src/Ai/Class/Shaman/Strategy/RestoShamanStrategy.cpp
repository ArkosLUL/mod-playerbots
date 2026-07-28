/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RestoShamanStrategy.h"
#include "Playerbots.h"

RestoShamanStrategy::RestoShamanStrategy(PlayerbotAI* botAI) : GenericShamanStrategy(botAI)
{
    // No custom ActionNodeFactory needed
}

// ===== Trigger Initialization ===
void RestoShamanStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericShamanStrategy::InitTriggers(triggers);

    // Totem Triggers
    // Call of the elements re-drops the whole totem set whenever two slots are empty, so it has to yield to
    // the low and critical heal bands.
    triggers.push_back(new TriggerNode("call of the elements", { NextAction("call of the elements", ACTION_MEDIUM_HEAL + 1) }));
    triggers.push_back(new TriggerNode("low health", { NextAction("stoneclaw totem", ACTION_CRITICAL_HEAL + 11) }));
    triggers.push_back(new TriggerNode("medium mana", { NextAction("mana tide totem", ACTION_MEDIUM_HEAL + 6.5f) }));

    // Healing Triggers
    // Riptide leads every band: it is instant and grants Tidal Waves for the wave that follows. Chain heal is
    // in every band as the throughput spell and as the backstop that survives the save-mana veto.
    triggers.push_back(new TriggerNode("party member critical health", { NextAction("nature's swiftness", 58.0f),
                                                                         NextAction("riptide on party", ACTION_CRITICAL_HEAL + 6),
                                                                         NextAction("lesser healing wave on party", ACTION_CRITICAL_HEAL + 4),
                                                                         NextAction("healing wave on party", ACTION_CRITICAL_HEAL + 3),
                                                                         NextAction("chain heal on party", ACTION_CRITICAL_HEAL + 2) }));

    // Above the 55.0 totem re-drops, so the free instant heal is never traded for a totem.
    triggers.push_back(new TriggerNode("nature's swiftness active", { NextAction("healing wave on party", 56.0f) }));

    triggers.push_back(new TriggerNode("medium group heal setting", { NextAction("tidal force", ACTION_CRITICAL_HEAL + 6.5f),
                                                                      NextAction("chain heal on party", ACTION_CRITICAL_HEAL + 5),
                                                                      NextAction("riptide on party", ACTION_CRITICAL_HEAL + 4.5f) }));

    triggers.push_back(new TriggerNode("group heal setting", { NextAction("chain heal on party", ACTION_MEDIUM_HEAL + 8),
                                                               NextAction("riptide on party", ACTION_MEDIUM_HEAL + 7) }));

    // Half steps keep the band clear of the cure nodes (23/24) and wind shear (23).
    triggers.push_back(new TriggerNode("party member low health", { NextAction("riptide on party", ACTION_MEDIUM_HEAL + 5),
                                                                    NextAction("chain heal on party", ACTION_MEDIUM_HEAL + 4.5f),
                                                                    NextAction("healing wave on party", ACTION_MEDIUM_HEAL + 3.5f),
                                                                    NextAction("lesser healing wave on party", ACTION_MEDIUM_HEAL + 2.5f) }));

    triggers.push_back(new TriggerNode("party member medium health", { NextAction("riptide on party", ACTION_LIGHT_HEAL + 9),
                                                                       NextAction("chain heal on party", ACTION_LIGHT_HEAL + 8),
                                                                       NextAction("lesser healing wave on party", ACTION_LIGHT_HEAL + 7),
                                                                       NextAction("healing wave on party", ACTION_LIGHT_HEAL + 6) }));

    triggers.push_back(new TriggerNode("party member almost full health", { NextAction("riptide on party", ACTION_LIGHT_HEAL + 3),
                                                                            NextAction("chain heal on party", ACTION_LIGHT_HEAL + 2) }));

    triggers.push_back(new TriggerNode("earth shield on main tank", { NextAction("earth shield on main tank", ACTION_MEDIUM_HEAL + 6) }));

    // Dispel Triggers
    triggers.push_back(new TriggerNode("party member cleanse spirit poison", { NextAction("cleanse spirit poison on party", ACTION_DISPEL + 2) }));
    triggers.push_back(new TriggerNode("party member cleanse spirit disease", { NextAction("cleanse spirit disease on party", ACTION_DISPEL + 2) }));
    triggers.push_back(new TriggerNode("party member cleanse spirit curse",{ NextAction("cleanse spirit curse on party", ACTION_DISPEL + 2) }));

    // Range/Mana Triggers
    triggers.push_back(new TriggerNode("enemy too close for spell", { NextAction("flee", ACTION_MOVE + 9) }));
    triggers.push_back(new TriggerNode("party member to heal out of spell range", { NextAction("reach party member to heal", ACTION_CRITICAL_HEAL + 10) }));
    triggers.push_back(new TriggerNode("water shield", { NextAction("water shield", ACTION_LIGHT_HEAL + 0.5f) }));
}

void ShamanHealerDpsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("healer should attack", { NextAction("flame shock", ACTION_DEFAULT + 0.2f),
                                                                 NextAction("lava burst", ACTION_DEFAULT + 0.1f),
                                                                 NextAction("lightning bolt", ACTION_DEFAULT) }));

    triggers.push_back( new TriggerNode("medium aoe and healer should attack", { NextAction("chain lightning", ACTION_DEFAULT + 0.3f) }));
}
