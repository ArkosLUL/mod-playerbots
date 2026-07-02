#include "VoAStrategy.h"
#include "VoAMultipliers.h"
#include "Action.h"
#include "Strategy.h"
#include "Trigger.h"
#include "vector"

void RaidVoAStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    //
    // Emalon the Storm Watcher
    //
    triggers.push_back(new TriggerNode(
        "emalon lighting nova trigger",
        { NextAction("emalon lighting nova action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "emalon mark boss trigger",
        { NextAction("emalon mark boss action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "emalon overcharge trigger",
        { NextAction("emalon overcharge action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "emalon fall from floor trigger",
        { NextAction("emalon fall from floor action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "emalon nature resistance trigger",
        { NextAction("emalon nature resistance action", ACTION_RAID) }));

    //
    // Koralon the Flame Watcher
    //

    triggers.push_back(new TriggerNode(
        "koralon fire resistance trigger",
        { NextAction("koralon fire resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "koralon mark boss trigger",
        { NextAction("koralon mark boss action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "koralon flaming cinder spread trigger",
        { NextAction("koralon flaming cinder spread action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "koralon burning breath trigger",
        { NextAction("koralon burning breath action", ACTION_EMERGENCY) }));

    //
    // Archavon the Stone Watcher
    //

    triggers.push_back(new TriggerNode(
        "archavon mark boss trigger",
        { NextAction("archavon mark boss action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "archavon rock shards spread trigger",
        { NextAction("archavon rock shards spread action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "archavon nature resistance trigger",
        { NextAction("archavon nature resistance action", ACTION_RAID) }));

    //
    // Toravon the Ice Watcher
    //

    triggers.push_back(new TriggerNode(
        "toravon mark boss trigger",
        { NextAction("toravon mark boss action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "toravon frost resistance trigger",
        { NextAction("toravon frost resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "toravon freezing ground trigger",
        { NextAction("toravon freezing ground action", ACTION_EMERGENCY) }));

    triggers.push_back(new TriggerNode(
        "toravon frozen orb avoid trigger",
        { NextAction("toravon frozen orb avoid action", ACTION_EMERGENCY) }));
}

void RaidVoAStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    //
    // Koralon the Flame Watcher
    //
    multipliers.push_back(new KoralonBurningBreathMultiplier(botAI));
    multipliers.push_back(new ToravonAvoidMultiplier(botAI));
}
