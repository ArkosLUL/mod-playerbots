#ifndef _PLAYERBOT_RAIDAQ40TRIGGERCONTEXT_H
#define _PLAYERBOT_RAIDAQ40TRIGGERCONTEXT_H

#include "AiObjectContext.h"
#include "BossAuraTriggers.h"
#include "NamedObjectContext.h"
#include "RaidAq40Triggers.h"

class RaidAq40TriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidAq40TriggerContext()
    {
        creators["viscidus nature resistance trigger"] = &RaidAq40TriggerContext::viscidus_nature_resistance_trigger;
        creators["princess huhuran nature resistance trigger"] = &RaidAq40TriggerContext::huhuran_nature_resistance_trigger;
        // creators["aq40 has emperor aggro"] = &RaidAq40TriggerContext::has_emperor_aggro;
        // creators["aq40 warlock tank emperor"] = &RaidAq40TriggerContext::warlock_tank_emperor;
        creators["aq40 mage frostbolt viscidus"] = &RaidAq40TriggerContext::mage_frostbolt_viscidus;
        creators["aq40 melee viscidus"] = &RaidAq40TriggerContext::melee_viscidus;
        // creators["aq40 target emperor vek'lor"] = &RaidAq40TriggerContext::target_emperor_veklor;
        // creators["aq40 target emperor vek'nilash"] = &RaidAq40TriggerContext::target_emperor_veknilash;
        // creators["aq40 target emperor pests"] = &RaidAq40TriggerContext::target_emperor_pests;
        // creators["aq40 approach emperor vek'lor"] = &RaidAq40TriggerContext::approach_emperor_veklor;
        // creators["aq40 approach emperor vek'nilash"] = &RaidAq40TriggerContext::approach_emperor_veknilash;
        // creators["aq40 tank anchor"] = &RaidAq40TriggerContext::tank_anchor;
        // creators["aq40 center position"] = &RaidAq40TriggerContext::center_position;
        // creators["aq40 emperor pre teleport"] = &RaidAq40TriggerContext::emperor_pre_teleport;
        // creators["aq40 near veklor"] = &RaidAq40TriggerContext::near_veklor;
        creators["aq40 ouro burrowed"] = &RaidAq40TriggerContext::ouro_burrowed;
        creators["aq40 cthun1 started"] = &RaidAq40TriggerContext::cthun1_started;
        creators["aq40 cthun2 started"] = &RaidAq40TriggerContext::cthun2_started;
    }

private:
    static Trigger* viscidus_nature_resistance_trigger(PlayerbotAI* botAI) { return new BossNatureResistanceTrigger(botAI, "viscidus"); }
    static Trigger* huhuran_nature_resistance_trigger(PlayerbotAI* botAI) { return new BossNatureResistanceTrigger(botAI, "princess huhuran"); }
    // static Trigger* has_emperor_aggro(PlayerbotAI* botAI) { return new Aq40HasEmperorAggroTrigger(botAI); }
    // static Trigger* warlock_tank_emperor(PlayerbotAI* botAI) { return new Aq40WarlockTankEmperorTrigger(botAI); }
    static Trigger* mage_frostbolt_viscidus(PlayerbotAI* botAI) { return new Aq40MageFrostboltViscidusTrigger(botAI); }
    static Trigger* melee_viscidus(PlayerbotAI* botAI) { return new Aq40MeleeViscidusTrigger(botAI); }
    // static Trigger* target_emperor_veklor(PlayerbotAI* botAI) { return new Aq40TargetEmperorVekLorTrigger(botAI); }
    // static Trigger* target_emperor_veknilash(PlayerbotAI* botAI) { return new Aq40TargetEmperorVekNilashTrigger(botAI); }
    // static Trigger* target_emperor_pests(PlayerbotAI* botAI) { return new Aq40TargetEmperorPestsTrigger(botAI); }
    // static Trigger* approach_emperor_veklor(PlayerbotAI* botAI) { return new Aq40ApproachEmperorVekLorTrigger(botAI); }
    // static Trigger* approach_emperor_veknilash(PlayerbotAI* botAI) { return new Aq40ApproachEmperorVekNilashTrigger(botAI); }
    // static Trigger* tank_anchor(PlayerbotAI* botAI) { return new Aq40TankAnchorTrigger(botAI); }
    // static Trigger* center_position(PlayerbotAI* botAI) { return new Aq40CenterPositionTrigger(botAI); }
    // static Trigger* emperor_pre_teleport(PlayerbotAI* botAI) { return new Aq40EmperorPreTeleportTrigger(botAI); }
    // static Trigger* near_veklor(PlayerbotAI* botAI) { return new Aq40NearVeklorTrigger(botAI); }
    static Trigger* ouro_burrowed(PlayerbotAI* botAI) { return new Aq40OuroBurrowedTrigger(botAI); }
    static Trigger* cthun1_started(PlayerbotAI* botAI) { return new Aq40Cthun1StartedTrigger(botAI); }
    static Trigger* cthun2_started(PlayerbotAI* botAI) { return new Aq40Cthun2StartedTrigger(botAI); }
};

#endif
