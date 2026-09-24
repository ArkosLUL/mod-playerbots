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
    static constexpr char const* Name = "thorim unbalancing strike swap action";

    ThorimUnbalancingStrikeSwapAction(PlayerbotAI* ai)
        : ThorimTakeBossAction(ai, Name)
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
    static constexpr char const* Name = "thorim tank pickup action";

    ThorimTankPickupAction(PlayerbotAI* ai) : ThorimTakeBossAction(ai, Name) {}

    bool isUseful() override;
};

// Attacks the add the encounter wants dead, rather than putting a raid icon on it. An icon is a sticky
// override that RtiTargetValue hands back before the smart picker ever runs, and nothing clears it
// until the bot leaves combat, so a mark that lands on the wrong unit cannot be taken back.
class ThorimDpsPriorityAction : public AttackAction
{
public:
    static constexpr char const* Name = "thorim dps priority action";

    ThorimDpsPriorityAction(PlayerbotAI* ai) : AttackAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimArenaPositioningAction : public MovementAction
{
public:
    static constexpr char const* Name = "thorim arena positioning action";

    ThorimArenaPositioningAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}

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
    static constexpr char const* Name = "thorim gauntlet positioning action";

    ThorimGauntletPositioningAction(PlayerbotAI* ai)
        : ThorimLaneMovementAction(ai, Name)
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
    static constexpr char const* Name = "thorim balcony advance action";

    ThorimBalconyAdvanceAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Cross to the same progress point in the lane the Runic Colossus is not smashing.
class ThorimRunicSmashAction : public ThorimLaneMovementAction
{
public:
    static constexpr char const* Name = "thorim runic smash action";

    ThorimRunicSmashAction(PlayerbotAI* ai) : ThorimLaneMovementAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Step out of the damage shield's reach without dropping the target, so ranged and instant abilities
// keep landing while the bot heals back up.
class ThorimRunicBarrierBailAction : public MovementAction
{
public:
    static constexpr char const* Name = "thorim runic barrier bail action";

    ThorimRunicBarrierBailAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Sends a strayed pet back to the arena. Not a MovementAction: it moves the pet, not the bot, so it
// has no business in the movement guards or the leash exemption lists.
class ThorimPetLeashAction : public Action
{
public:
    static constexpr char const* Name = "thorim pet leash action";

    ThorimPetLeashAction(PlayerbotAI* ai) : Action(ai, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Steps out of the charged Thunder Orb's field, by the shortest way out rather than across the room.
class ThorimChargedOrbAction : public MovementAction
{
public:
    static constexpr char const* Name = "thorim charged orb action";

    ThorimChargedOrbAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Walks an arena squad member back inside the box boss_thorim.cpp scans for a living player.
class ThorimArenaLeashAction : public MovementAction
{
public:
    static constexpr char const* Name = "thorim arena leash action";

    ThorimArenaLeashAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimResetEncounterStateAction : public Action
{
public:
    static constexpr char const* Name = "thorim reset encounter state action";

    ThorimResetEncounterStateAction(PlayerbotAI* ai) : Action(ai, Name) {}

    bool Execute(Event event) override;
};

class ThorimFallFromFloorAction : public Action
{
public:
    static constexpr char const* Name = "thorim fall from floor action";

    ThorimFallFromFloorAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimPhase2PositioningAction : public MovementAction
{
public:
    static constexpr char const* Name = "thorim phase 2 positioning action";

    ThorimPhase2PositioningAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Hard mode: clear Sif's moving Blizzard ground AoE. The phase 2 camp takes a short cone-safe step off
// the zones instead of the generic flee. Anyone else still gets the flee.
class ThorimSifBlizzardAction : public MoveAwayFromCreatureAction
{
public:
    static constexpr char const* Name = "thorim sif blizzard action";

    ThorimSifBlizzardAction(PlayerbotAI* ai)
        : MoveAwayFromCreatureAction(ai, Name, NPC_SIF_BLIZZARD,
                                     ULDUAR_THORIM_SIF_BLIZZARD_RADIUS)
    {
    }

    bool Execute(Event event) override;
};

#endif
