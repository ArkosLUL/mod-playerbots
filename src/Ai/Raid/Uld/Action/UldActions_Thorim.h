#ifndef PLAYERBOTS_ULDACTIONS_THORIM_H
#define PLAYERBOTS_ULDACTIONS_THORIM_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldEncounter_Thorim.h"
#include "UldTriggers.h"
#include "Vehicle.h"

// Both tank nodes want the same two things: be on the boss, then pull him off whoever has him.
class ThorimTakeBossAction : public AttackAction
{
public:
    ThorimTakeBossAction(PlayerbotAI* ai, std::string const name) : AttackAction(ai, name) {}

    bool Execute(Event event) override;
};

class ThorimUnbalancingStrikeSwapAction : public ThorimTakeBossAction
{
public:
    ThorimUnbalancingStrikeSwapAction(PlayerbotAI* ai)
        : ThorimTakeBossAction(ai, "thorim unbalancing strike swap action")
    {
    }

    bool isUseful() override;
};

// The phase change hands the boss to whoever he happens to look at, and nothing else in the encounter
// puts a tank on him: the target guard lets anything through once he is off the balcony, so the tank
// branch of the dps picker can never fire, and phase 2 positioning wants him already tanked before it
// will walk him anywhere. Without this the raid spent 15 s watching him eat the ranged camp.
class ThorimTankPickupAction : public ThorimTakeBossAction
{
public:
    ThorimTankPickupAction(PlayerbotAI* ai) : ThorimTakeBossAction(ai, "thorim tank pickup action") {}

    bool isUseful() override;
};

// Attacks the add the encounter wants dead, rather than putting a raid icon on it. An icon is a sticky
// override that RtiTargetValue hands back before the smart picker ever runs, and nothing clears it
// until the bot leaves combat, so a mark that lands on the wrong unit cannot be taken back.
class ThorimDpsPriorityAction : public AttackAction
{
public:
    ThorimDpsPriorityAction(PlayerbotAI* ai) : AttackAction(ai, "thorim dps priority action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimArenaPositioningAction : public MovementAction
{
public:
    ThorimArenaPositioningAction(PlayerbotAI* ai) : MovementAction(ai, "thorim arena positioning action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Shared by the two nodes that walk the corridor lanes, so the six-waypoint walk exists once.
class ThorimLaneMovementAction : public MovementAction
{
public:
    ThorimLaneMovementAction(PlayerbotAI* ai, std::string const name) : MovementAction(ai, name) {}

protected:
    bool MoveToGauntletWaypoint(bool leftLane, uint8 index, bool forceCombatPriority);
};

class ThorimGauntletPositioningAction : public ThorimLaneMovementAction
{
public:
    ThorimGauntletPositioningAction(PlayerbotAI* ai)
        : ThorimLaneMovementAction(ai, "thorim gauntlet positioning action")
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Walks the upper hallway to Thorim's platform, then drops into the arena behind him. Owns the jump
// outright - the corridor node used to, behind a 0.5 yd arrival test it could never reliably meet.
class ThorimBalconyAdvanceAction : public MovementAction
{
public:
    ThorimBalconyAdvanceAction(PlayerbotAI* ai) : MovementAction(ai, "thorim balcony advance action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Cross to the same progress point in the lane the Runic Colossus is not smashing.
class ThorimRunicSmashAction : public ThorimLaneMovementAction
{
public:
    ThorimRunicSmashAction(PlayerbotAI* ai) : ThorimLaneMovementAction(ai, "thorim runic smash action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Step out of the damage shield's reach without dropping the target, so ranged and instant abilities
// keep landing while the bot heals back up.
class ThorimRunicBarrierBailAction : public MovementAction
{
public:
    ThorimRunicBarrierBailAction(PlayerbotAI* ai) : MovementAction(ai, "thorim runic barrier bail action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Sends a strayed pet back to the arena. Not a MovementAction: it moves the pet, not the bot, so it
// has no business in the movement guards or the leash exemption lists.
class ThorimPetLeashAction : public Action
{
public:
    ThorimPetLeashAction(PlayerbotAI* ai) : Action(ai, "thorim pet leash action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Steps out of the charged Thunder Orb's field, by the shortest way out rather than across the room.
class ThorimChargedOrbAction : public MovementAction
{
public:
    ThorimChargedOrbAction(PlayerbotAI* ai) : MovementAction(ai, "thorim charged orb action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Walks an arena squad member back inside the box boss_thorim.cpp scans for a living player.
class ThorimArenaLeashAction : public MovementAction
{
public:
    ThorimArenaLeashAction(PlayerbotAI* ai) : MovementAction(ai, "thorim arena leash action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimResetEncounterStateAction : public Action
{
public:
    ThorimResetEncounterStateAction(PlayerbotAI* ai) : Action(ai, "thorim reset encounter state action") {}

    bool Execute(Event event) override;
};

class ThorimFallFromFloorAction : public Action
{
public:
    ThorimFallFromFloorAction(PlayerbotAI* botAI) : Action(botAI, "thorim fall from floor action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimPhase2PositioningAction : public MovementAction
{
public:
    ThorimPhase2PositioningAction(PlayerbotAI* ai) : MovementAction(ai, "thorim phase 2 positioning action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Hard mode: clear Sif's moving Blizzard ground AoE. The phase 2 camp takes a short cone-safe step off
// the zones instead of the generic flee. Anyone else still gets the flee.
class ThorimSifBlizzardAction : public MoveAwayFromCreatureAction
{
public:
    ThorimSifBlizzardAction(PlayerbotAI* ai)
        : MoveAwayFromCreatureAction(ai, "thorim sif blizzard action", NPC_SIF_BLIZZARD,
                                     ULDUAR_THORIM_SIF_BLIZZARD_RADIUS)
    {
    }

    bool Execute(Event event) override;
};

// Hard mode: ranged/healers back off so Sif's point-blank Frost Nova misses.
class ThorimSifFrostNovaAction : public MoveAwayFromCreatureAction
{
public:
    ThorimSifFrostNovaAction(PlayerbotAI* ai)
        : MoveAwayFromCreatureAction(ai, "thorim sif frost nova action", NPC_SIF,
                                     ULDUAR_THORIM_SIF_FROST_NOVA_RADIUS)
    {
    }
};

#endif
