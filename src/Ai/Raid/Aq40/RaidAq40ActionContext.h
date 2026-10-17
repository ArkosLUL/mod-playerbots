#ifndef _PLAYERBOT_RAIDAQ40ACTIONCONTEXT_H
#define _PLAYERBOT_RAIDAQ40ACTIONCONTEXT_H

#include "Action.h"
#include "BossAuraActions.h"
#include "NamedObjectContext.h"
#include "RaidAq40Actions.h"

class RaidAq40ActionContext : public NamedObjectContext<Action>
{
public:
    RaidAq40ActionContext()
    {
        creators["viscidus nature resistance action"] = &RaidAq40ActionContext::viscidus_nature_resistance_action;
        creators["princess huhuran nature resistance action"] = &RaidAq40ActionContext::huhuran_nature_resistance_action;
        // creators["aq40 move from other emperor"] = &RaidAq40ActionContext::move_from_other_emperor;
        // creators["aq40 attack emperor vek'lor"] = &RaidAq40ActionContext::attack_emperor_veklor;
        // creators["aq40 attack emperor vek'nilash"] = &RaidAq40ActionContext::attack_emperor_veknilash;
        // creators["aq40 attack emperor pests"] = &RaidAq40ActionContext::attack_emperor_pests;
        // creators["aq40 move towards emperor vek'lor"] = &RaidAq40ActionContext::move_towards_emperor_veklor;
        // creators["aq40 move towards emperor vek'nilash"] = &RaidAq40ActionContext::move_towards_emperor_veknilash;
        // creators["searing pain"] = &RaidAq40ActionContext::warlock_searing_pain;
        creators["frostbolt"] = &RaidAq40ActionContext::mage_cast_frostbolt;
        creators["aq40 melee viscidus"] = &RaidAq40ActionContext::melee_viscidus;
        // creators["aq40 tank anchor position"] = &RaidAq40ActionContext::tank_anchor_position;
        // creators["aq40 move to room center"] = &RaidAq40ActionContext::move_to_room_center;
        // creators["aq40 move from veklor"] = &RaidAq40ActionContext::move_from_veklor;
        creators["aq40 ouro burrowed flee"] = &RaidAq40ActionContext::ouro_burrowed_flee;
        creators["aq40 cthun1 get positioned"] = &RaidAq40ActionContext::cthun1_get_positioned;
        creators["aq40 cthun2 get positioned"] = &RaidAq40ActionContext::cthun2_get_positioned;
    }

private:
    static Action* viscidus_nature_resistance_action(PlayerbotAI* botAI) { return new BossNatureResistanceAction(botAI, "viscidus"); }
    static Action* huhuran_nature_resistance_action(PlayerbotAI* botAI) { return new BossNatureResistanceAction(botAI, "princess huhuran"); }
    // static Action* move_from_other_emperor(PlayerbotAI* botAI) { return new Aq40MoveFromOtherEmperorAction(botAI); }
    // static Action* attack_emperor_veklor(PlayerbotAI* botAI) { return new Aq40AttackEmperorVekLorAction(botAI); }
    // static Action* attack_emperor_veknilash(PlayerbotAI* botAI) { return new Aq40AttackEmperorVekNilashAction(botAI); }
    // static Action* attack_emperor_pests(PlayerbotAI* botAI) { return new Aq40AttackEmperorPestsAction(botAI); }
    // static Action* move_towards_emperor_veklor(PlayerbotAI* botAI) { return (Action *)(new Aq40MoveTowardsEmperorVekLorAction(botAI)); }
    // static Action* move_towards_emperor_veknilash(PlayerbotAI* botAI) { return (Action *)(new Aq40MoveTowardsEmperorVekNilashAction(botAI)); }
    // static Action* warlock_searing_pain(PlayerbotAI* botAI) { return new Aq40WarlockCastSearingPainAction(botAI); }
    static Action* mage_cast_frostbolt(PlayerbotAI* botAI) { return new Aq40MageCastFrostboltAction(botAI); }
    static Action* melee_viscidus(PlayerbotAI* botAI) { return new Aq40MeleeViscidusAction(botAI); }
    // static Action* tank_anchor_position(PlayerbotAI* botAI) { return new Aq40TankAnchorPositionAction(botAI); }
    // static Action* move_to_room_center(PlayerbotAI* botAI) { return new Aq40MoveToRoomCenterAction(botAI); }
    // static Action* move_from_veklor(PlayerbotAI* botAI) { return new Aq40MoveFromVeklorAction(botAI); }
    static Action* ouro_burrowed_flee(PlayerbotAI* botAI) { return new Aq40OuroBurrowedFleeAction(botAI); }
    static Action* cthun1_get_positioned(PlayerbotAI* botAI) { return new Aq40Cthun1PositionAction(botAI); }
    static Action* cthun2_get_positioned(PlayerbotAI* botAI) { return new Aq40Cthun2PositionAction(botAI); }
};

#endif
