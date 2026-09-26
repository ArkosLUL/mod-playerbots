/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXACTIONS_H
#define PLAYERBOTS_NAXXACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "NaxxBossHelper.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidRedirectThreat.h"

// just for test
// class TryToGetBossAIAction : public Action
// {
// public:
//     TryToGetBossAIAction(PlayerbotAI* ai) : Action(ai, "try to get boss ai") {}

// public:
//     virtual bool Execute(Event event);
// };

class GrobbulusGoBehindAction : public MovementAction
{
public:
    static constexpr char const* Name = "grobbulus go behind the boss";

    GrobbulusGoBehindAction(PlayerbotAI* ai, float distance = 24.0f, float delta_angle = M_PI / 8)
        : MovementAction(ai, Name)
    {
        this->distance = distance;
        this->delta_angle = delta_angle;
    }
    virtual bool Execute(Event event);

protected:
    float distance, delta_angle;
};

class GrobbulusRotateAction : public RotateAroundTheCenterPointAction
{
public:
    static constexpr char const* Name = "rotate grobbulus";

    GrobbulusRotateAction(PlayerbotAI* botAI)
        : RotateAroundTheCenterPointAction(botAI, Name, 3281.23f, -3310.38f, 35.0f, 8, true, M_PI)
    {
    }
    virtual bool isUseful() override
    {
        return RotateAroundTheCenterPointAction::isUseful() && botAI->IsMainTank(bot) &&
               AI_VALUE2(bool, "has aggro", "boss target");
    }
    uint32 GetCurrWaypoint() override;
};

// MoveInsideAction names itself "move inside", so Name only registers.
class GrobblulusMoveCenterAction : public MoveInsideAction
{
public:
    static constexpr char const* Name = "grobbulus move center";

    GrobblulusMoveCenterAction(PlayerbotAI* ai) : MoveInsideAction(ai, 3281.23f, -3310.38f, 5.0f) {}
};

class GrobbulusMoveAwayAction : public MovementAction
{
public:
    static constexpr char const* Name = "grobbulus move away";

    GrobbulusMoveAwayAction(PlayerbotAI* ai, float distance = 18.0f)
        : MovementAction(ai, Name), distance(distance)
    {
    }
    bool Execute(Event event) override;

private:
    float distance;
};

class FaerlinaSacrificeWorshipperAction : public AttackAction
{
public:
    static constexpr char const* Name = "faerlina sacrifice worshipper";

    FaerlinaSacrificeWorshipperAction(PlayerbotAI* ai) : AttackAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

protected:
    Unit* GetTarget() override;
};

class HeiganDanceAction : public MovementAction
{
public:
    HeiganDanceAction(PlayerbotAI* ai, std::string const name) : MovementAction(ai, name), helper(ai) {}

protected:
    bool MoveToSafeZone(float tolerance);
    bool MoveToPlatform();

    HeiganBossHelper helper;
};

class HeiganDanceMeleeAction : public HeiganDanceAction
{
public:
    static constexpr char const* Name = "heigan dance melee";

    HeiganDanceMeleeAction(PlayerbotAI* ai) : HeiganDanceAction(ai, Name) {}
    virtual bool Execute(Event event);
};

class HeiganDispelDecrepitFeverAction : public Action
{
public:
    static constexpr char const* Name = "heigan dispel decrepit fever";

    HeiganDispelDecrepitFeverAction(PlayerbotAI* ai) : Action(ai, Name), helper(ai) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Unit* GetDecrepitFeverTarget() const;
    bool CanDispelDisease() const;

    HeiganBossHelper helper;
};

class HeiganDanceRangedAction : public HeiganDanceAction
{
public:
    static constexpr char const* Name = "heigan dance ranged";

    HeiganDanceRangedAction(PlayerbotAI* ai) : HeiganDanceAction(ai, Name) {}
    virtual bool Execute(Event event);
};

class ThaddiusPrepullSplitAction : public MovementAction
{
public:
    static constexpr char const* Name = "thaddius prepull split";

    ThaddiusPrepullSplitAction(PlayerbotAI* ai) : MovementAction(ai, Name), helper(ai) {}
    virtual bool Execute(Event event);
    virtual bool isUseful();

private:
    ThaddiusBossHelper helper;
};

class ThaddiusAttackNearestPetAction : public AttackAction
{
public:
    static constexpr char const* Name = "thaddius attack nearest pet";

    ThaddiusAttackNearestPetAction(PlayerbotAI* ai) : AttackAction(ai, Name), helper(ai) {}
    virtual bool Execute(Event event);
    virtual bool isUseful();

private:
    ThaddiusBossHelper helper;
};

// class ThaddiusMeleeToPlaceAction : public MovementAction
// {
// public:
//     ThaddiusMeleeToPlaceAction(PlayerbotAI* ai) : MovementAction(ai, "thaddius melee to place") {}
//     virtual bool Execute(Event event);
//     virtual bool isUseful();
// };

// class ThaddiusRangedToPlaceAction : public MovementAction
// {
// public:
//     ThaddiusRangedToPlaceAction(PlayerbotAI* ai) : MovementAction(ai, "thaddius ranged to place") {}
//     virtual bool Execute(Event event);
//     virtual bool isUseful();
// };

class ThaddiusRedirectThreatAction : public RaidRedirectThreatAction
{
public:
    static constexpr char const* Name = "thaddius redirect threat";

    ThaddiusRedirectThreatAction(PlayerbotAI* ai) : RaidRedirectThreatAction(ai, Name), helper(ai) {}

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;

private:
    ThaddiusBossHelper helper;
};

class ThaddiusMoveToPlatformAction : public MovementAction
{
public:
    static constexpr char const* Name = "thaddius move to platform";

    ThaddiusMoveToPlatformAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}
    virtual bool Execute(Event event);
    virtual bool isUseful();
};

class ThaddiusMovePolarityAction : public MovementAction
{
public:
    static constexpr char const* Name = "thaddius move polarity";

    ThaddiusMovePolarityAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}
    virtual bool Execute(Event event);
    virtual bool isUseful();
};

class RazuviousUseObedienceCrystalAction : public MovementAction
{
public:
    static constexpr char const* Name = "razuvious use obedience crystal";

    RazuviousUseObedienceCrystalAction(PlayerbotAI* ai) : MovementAction(ai, Name), helper(ai) {}
    bool Execute(Event event) override;

private:
    RazuviousBossHelper helper;
};

class RazuviousTargetAction : public AttackAction
{
public:
    static constexpr char const* Name = "razuvious target";

    RazuviousTargetAction(PlayerbotAI* ai) : AttackAction(ai, Name), helper(ai) {}
    bool Execute(Event event) override;

private:
    RazuviousBossHelper helper;
};

class HorsemanAttractAlternativelyAction : public AttackAction
{
public:
    static constexpr char const* Name = "horseman attract alternatively";

    HorsemanAttractAlternativelyAction(PlayerbotAI* ai) : AttackAction(ai, Name), helper(ai) {}
    bool Execute(Event event) override;

protected:
    FourhorsemanBossHelper helper;
};

class HorsemanAttactInOrderAction : public AttackAction
{
public:
    static constexpr char const* Name = "horseman attack in order";

    HorsemanAttactInOrderAction(PlayerbotAI* ai) : AttackAction(ai, Name), helper(ai) {}
    bool Execute(Event event) override;

protected:
    FourhorsemanBossHelper helper;
};

class FourhorsemanRedirectThreatAction : public RaidRedirectThreatAction
{
public:
    static constexpr char const* Name = "four horsemen redirect threat";

    FourhorsemanRedirectThreatAction(PlayerbotAI* ai) : RaidRedirectThreatAction(ai, Name), helper(ai) {}

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;

private:
    // Tank and horseman this bot is responsible for, both null when there is no assignment.
    std::pair<Player*, Unit*> GetAssignment();

    FourhorsemanBossHelper helper;
};

class SapphironGroundPositionAction : public MovementAction
{
public:
    SapphironGroundPositionAction(PlayerbotAI* ai) : MovementAction(ai, "sapphiron ground position"), helper(ai) {}
    bool Execute(Event event) override;

protected:
    SapphironBossHelper helper;
};

class SapphironFlightPositionAction : public MovementAction
{
public:
    SapphironFlightPositionAction(PlayerbotAI* ai) : MovementAction(ai, "sapphiron flight position"), helper(ai) {}
    bool Execute(Event event) override;

protected:
    // Result of a shelter attempt. Moving => this action owns the tick (cast-time
    // heals can't fire while the bot moves anyway). Sheltered => bot is stopped
    // behind its block, so the tick can yield to heals. None => nothing to do.
    enum class ShelterResult { None, Moving, Sheltered };

    SapphironBossHelper helper;
    ShelterResult MoveToNearestIcebolt();
    void ResetShelterLatch();

    // Per-bot state, latched for the duration of one flight phase (see cache in
    // Engine::CreateActionNode — actions are created once per bot and reused).
    ObjectGuid assignedBlockGuid;
    bool sheltered = false;
};

class KelthuzadChooseTargetAction : public AttackAction
{
public:
    KelthuzadChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, "kel'thuzad choose target"), helper(ai) {}
    virtual bool Execute(Event event);

private:
    KelthuzadBossHelper helper;
};

class KelthuzadPositionAction : public MovementAction
{
public:
    KelthuzadPositionAction(PlayerbotAI* ai) : MovementAction(ai, "kel'thuzad position"), helper(ai) {}
    virtual bool Execute(Event event);

private:
    KelthuzadBossHelper helper;
};

class KelthuzadFleeShadowFissureAction : public MovementAction
{
public:
    KelthuzadFleeShadowFissureAction(PlayerbotAI* ai)
        : MovementAction(ai, "kel'thuzad flee shadow fissure"), helper(ai)
    {
    }
    bool Execute(Event event) override;

private:
    KelthuzadBossHelper helper;
};

class KelthuzadMisdirectBossToMainTankAction : public AttackAction
{
public:
    KelthuzadMisdirectBossToMainTankAction(PlayerbotAI* ai)
        : AttackAction(ai, "kel'thuzad misdirect boss to main tank"), helper(ai)
    {
    }
    bool Execute(Event event) override;

private:
    KelthuzadBossHelper helper;
};

class KelthuzadCycloneChainedAction : public MovementAction
{
public:
    KelthuzadCycloneChainedAction(PlayerbotAI* ai)
        : MovementAction(ai, "kel'thuzad cyclone chained"), helper(ai)
    {
    }
    bool Execute(Event event) override;

private:
    KelthuzadBossHelper helper;
};

class AnubrekhanChooseTargetAction : public AttackAction
{
public:
    static constexpr char const* Name = "anub'rekhan choose target";

    AnubrekhanChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, Name), helper(ai) {}
    bool Execute(Event event) override;

private:
    AnubrekhanBossHelper helper;
};

// 32 waypoints put 6.9 yd between them, so the main tank's kite tracks the circle instead of
// jumping across it.
class AnubrekhanPositionAction : public RotateAroundTheCenterPointAction
{
public:
    static constexpr char const* Name = "anub'rekhan position";

    AnubrekhanPositionAction(PlayerbotAI* ai)
        : RotateAroundTheCenterPointAction(ai, Name, AnubrekhanBossHelper::RoomCenterX,
                                           AnubrekhanBossHelper::RoomCenterY, AnubrekhanBossHelper::KiteRadius, 32),
          helper(ai)
    {
    }
    bool Execute(Event event) override;

private:
    bool KiteBoss();
    bool HoldAdds(Unit* boss);
    bool TakeRangedSlot(Unit* boss);
    bool TakeSwarmStack();
    // Rate-limited move to a slot the caller worked out; false when the bot is already parked there.
    bool MoveToSlot(float x, float y);

    AnubrekhanBossHelper helper;
};

class AnubrekhanRedirectThreatAction : public RaidRedirectThreatAction
{
public:
    static constexpr char const* Name = "anub'rekhan redirect threat";

    AnubrekhanRedirectThreatAction(PlayerbotAI* ai) : RaidRedirectThreatAction(ai, Name), helper(ai) {}

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;

private:
    AnubrekhanBossHelper helper;
};

class GluthChooseTargetAction : public AttackAction
{
public:
    static constexpr char const* Name = "gluth choose target";

    GluthChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, Name), helper(ai) {}
    bool Execute(Event event) override;

private:
    GluthBossHelper helper;
};

class GluthPositionAction : public RotateAroundTheCenterPointAction
{
public:
    static constexpr char const* Name = "gluth position";

    GluthPositionAction(PlayerbotAI* ai)
        : RotateAroundTheCenterPointAction(ai, Name, 3293.61f, -3149.01f, 12.0f, 12), helper(ai)
    {
    }
    bool Execute(Event event) override;

private:
    GluthBossHelper helper;
};

class GluthSlowdownAction : public Action
{
public:
    static constexpr char const* Name = "gluth slowdown";

    GluthSlowdownAction(PlayerbotAI* ai) : Action(ai, Name), helper(ai) {}
    bool Execute(Event event) override;

private:
    GluthBossHelper helper;
};

class GluthTranquilizingShotAction : public Action
{
public:
    static constexpr char const* Name = "gluth tranquilizing shot";

    GluthTranquilizingShotAction(PlayerbotAI* ai) : Action(ai, Name), helper(ai) {}
    bool Execute(Event event) override;

private:
    GluthBossHelper helper;
};

class GluthRedirectThreatAction : public RaidRedirectThreatAction
{
public:
    static constexpr char const* Name = "gluth redirect threat";

    GluthRedirectThreatAction(PlayerbotAI* ai) : RaidRedirectThreatAction(ai, Name), helper(ai) {}

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;

private:
    // Tank and unit this bot is responsible for, both null when there is nothing to redirect.
    std::pair<Player*, Unit*> GetAssignment();

    GluthBossHelper helper;
};

class LoathebPositionAction : public MovementAction
{
public:
    static constexpr char const* Name = "loatheb position";

    LoathebPositionAction(PlayerbotAI* ai) : MovementAction(ai, Name), helper(ai) {}
    virtual bool Execute(Event event);

private:
    LoathebBossHelper helper;
};

class LoathebChooseTargetAction : public AttackAction
{
public:
    static constexpr char const* Name = "loatheb choose target";

    LoathebChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, Name), helper(ai) {}
    virtual bool Execute(Event event);

private:
    LoathebBossHelper helper;
};

class NothChooseTargetAction : public AttackAction
{
public:
    static constexpr char const* Name = "noth choose target";

    NothChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, Name), helper(ai) {}
    bool Execute(Event event) override;

private:
    NothBossHelper helper;
};

class NothPositionAction : public MovementAction
{
public:
    static constexpr char const* Name = "noth position";

    NothPositionAction(PlayerbotAI* ai) : MovementAction(ai, Name), helper(ai) {}
    bool Execute(Event event) override;

private:
    // Plagued Warriors cleave, so the tank holding them steps off anyone who wanders into the swing.
    static constexpr float CleaveSpread = 5.0f;
    // Where a bot being chased by a Plagued Champion parks so the add tank can pick the add up.
    // Outside CleaveSpread, or the tank keeps stepping away from the bot that just brought it in.
    static constexpr float AddTankHandoffDistance = 10.0f;
    // Adds dragged further than this leave the healers behind.
    static constexpr float HealerLeashDistance = 25.0f;
    static constexpr float MeleeCloseDistance = 10.0f;

    bool PositionAssistTank(Unit* currentTarget);
    bool DragChampionToAddTank();
    bool MoveToClamped(float x, float y);

    NothBossHelper helper;
};

class NothDispelCurseAction : public Action
{
public:
    static constexpr char const* Name = "noth dispel curse";

    NothDispelCurseAction(PlayerbotAI* ai) : Action(ai, Name), helper(ai) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Unit* GetAssignedTarget();
    // Position of this bot among the group's living decursers, in group order, so N decursers start
    // on N different targets instead of all racing for the first one. -1 when the bot cannot decurse.
    int32 GetDecurserIndex() const;

    NothBossHelper helper;
};

// class PatchwerkRangedPositionAction : public MovementAction
// {
// public:
//     PatchwerkRangedPositionAction(PlayerbotAI* ai) : MovementAction(ai, "patchwerk ranged position") {}
//     bool Execute(Event event) override;
// };

// Maexxna
class MaexxnaAttackWebWrapAction : public AttackAction
{
public:
    static constexpr char const* Name = "maexxna attack web wrap";

    MaexxnaAttackWebWrapAction(PlayerbotAI* ai) : AttackAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MaexxnaTankSpiderlingsAction : public AttackAction
{
public:
    static constexpr char const* Name = "maexxna tank spiderlings";

    MaexxnaTankSpiderlingsAction(PlayerbotAI* ai) : AttackAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Gothik the Harvester
class GothikChooseTargetAction : public AttackAction
{
public:
    static constexpr char const* Name = "gothik choose target";

    GothikChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, Name), helper(ai) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    GothikBossHelper helper;
};

class GothikStayOnLivingSideAction : public MovementAction
{
public:
    static constexpr char const* Name = "gothik stay on living side";

    GothikStayOnLivingSideAction(PlayerbotAI* ai) : MovementAction(ai, Name), helper(ai) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    GothikBossHelper helper;
};

#endif
