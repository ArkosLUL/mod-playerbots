/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXACTIONS_H
#define PLAYERBOTS_NAXXACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericActions.h"
#include "MovementActions.h"
#include "NaxxBossHelper.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

// just for test
// class TryToGetBossAIAction : public Action
// {
// public:
//     TryToGetBossAIAction(PlayerbotAI* ai) : Action(ai, "try to get boss ai") {}

// public:
//     virtual bool Execute(Event event);
// };

// Hunter Misdirection and rogue Tricks of the Trade solve the same problem at the same moments, so
// the per-boss actions below only answer two questions: which tank should own the threat, and which
// unit do we spend the charges on.
class NaxxRedirectThreatAction : public AttackAction
{
public:
    NaxxRedirectThreatAction(PlayerbotAI* ai, std::string const name) : AttackAction(ai, name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

protected:
    // Either may return nullptr, which just means "nothing to redirect this tick".
    virtual Player* GetRedirectTank() = 0;
    virtual Unit* GetThreatDumpTarget() = 0;

    // Group tank that `target` is currently attacking, if any.
    Player* GetTankHolding(Unit* target);

    // Position of this bot among the group's living redirecters, in group order, so encounters with
    // one tank per boss can hand out different assignments. -1 when the bot cannot redirect.
    int32 GetRedirecterIndex();
};

class GrobbulusGoBehindAction : public MovementAction
{
public:
    GrobbulusGoBehindAction(PlayerbotAI* ai, float distance = 24.0f, float delta_angle = M_PI / 8)
        : MovementAction(ai, "grobbulus go behind")
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
    GrobbulusRotateAction(PlayerbotAI* botAI)
        : RotateAroundTheCenterPointAction(botAI, "rotate grobbulus", 3281.23f, -3310.38f, 35.0f, 8, true, M_PI)
    {
    }
    virtual bool isUseful() override
    {
        return RotateAroundTheCenterPointAction::isUseful() && botAI->IsMainTank(bot) &&
               AI_VALUE2(bool, "has aggro", "boss target");
    }
    uint32 GetCurrWaypoint() override;
};

class GrobblulusMoveCenterAction : public MoveInsideAction
{
public:
    GrobblulusMoveCenterAction(PlayerbotAI* ai) : MoveInsideAction(ai, 3281.23f, -3310.38f, 5.0f) {}
};

class GrobbulusMoveAwayAction : public MovementAction
{
public:
    GrobbulusMoveAwayAction(PlayerbotAI* ai, float distance = 18.0f)
        : MovementAction(ai, "grobbulus move away"), distance(distance)
    {
    }
    bool Execute(Event event) override;

private:
    float distance;
};

class FaerlinaSacrificeWorshipperAction : public AttackAction
{
public:
    FaerlinaSacrificeWorshipperAction(PlayerbotAI* ai) : AttackAction(ai, "faerlina sacrifice worshipper") {}

    bool Execute(Event event) override;
    bool isUseful() override;

protected:
    Unit* GetTarget() override;
};

class HeiganDanceAction : public MovementAction
{
public:
    HeiganDanceAction(PlayerbotAI* ai) : MovementAction(ai, "heigan dance"), helper(ai) {}

protected:
    bool MoveToSafeZone(float tolerance);
    bool MoveToPlatform();

    HeiganBossHelper helper;
};

class HeiganDanceMeleeAction : public HeiganDanceAction
{
public:
    HeiganDanceMeleeAction(PlayerbotAI* ai) : HeiganDanceAction(ai) {}
    virtual bool Execute(Event event);
};

class HeiganDispelDecrepitFeverAction : public Action
{
public:
    HeiganDispelDecrepitFeverAction(PlayerbotAI* ai) : Action(ai, "heigan dispel decrepit fever"), helper(ai) {}
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
    HeiganDanceRangedAction(PlayerbotAI* ai) : HeiganDanceAction(ai) {}
    virtual bool Execute(Event event);
};

class ThaddiusAttackNearestPetAction : public AttackAction
{
public:
    ThaddiusAttackNearestPetAction(PlayerbotAI* ai) : AttackAction(ai, "thaddius attack nearest pet"), helper(ai) {}
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

class ThaddiusRedirectThreatAction : public NaxxRedirectThreatAction
{
public:
    ThaddiusRedirectThreatAction(PlayerbotAI* ai) : NaxxRedirectThreatAction(ai, "thaddius redirect threat"), helper(ai)
    {
    }

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;

private:
    ThaddiusBossHelper helper;
};

class ThaddiusMoveToPlatformAction : public MovementAction
{
public:
    ThaddiusMoveToPlatformAction(PlayerbotAI* ai) : MovementAction(ai, "thaddius move to platform") {}
    virtual bool Execute(Event event);
    virtual bool isUseful();
};

class ThaddiusMovePolarityAction : public MovementAction
{
public:
    ThaddiusMovePolarityAction(PlayerbotAI* ai) : MovementAction(ai, "thaddius move polarity") {}
    virtual bool Execute(Event event);
    virtual bool isUseful();
};

class RazuviousUseObedienceCrystalAction : public MovementAction
{
public:
    RazuviousUseObedienceCrystalAction(PlayerbotAI* ai)
        : MovementAction(ai, "razuvious use obedience crystal"), helper(ai)
    {
    }
    bool Execute(Event event) override;

private:
    RazuviousBossHelper helper;
};

class RazuviousTargetAction : public AttackAction
{
public:
    RazuviousTargetAction(PlayerbotAI* ai) : AttackAction(ai, "razuvious target"), helper(ai) {}
    bool Execute(Event event) override;

private:
    RazuviousBossHelper helper;
};

class HorsemanAttractAlternativelyAction : public AttackAction
{
public:
    HorsemanAttractAlternativelyAction(PlayerbotAI* ai) : AttackAction(ai, "horseman attract alternatively"), helper(ai)
    {
    }
    bool Execute(Event event) override;

protected:
    FourhorsemanBossHelper helper;
};

class HorsemanAttactInOrderAction : public AttackAction
{
public:
    HorsemanAttactInOrderAction(PlayerbotAI* ai) : AttackAction(ai, "horseman attact in order"), helper(ai) {}
    bool Execute(Event event) override;

protected:
    FourhorsemanBossHelper helper;
};

class FourhorsemanRedirectThreatAction : public NaxxRedirectThreatAction
{
public:
    FourhorsemanRedirectThreatAction(PlayerbotAI* ai)
        : NaxxRedirectThreatAction(ai, "four horsemen redirect threat"), helper(ai)
    {
    }

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

class AnubrekhanChooseTargetAction : public AttackAction
{
public:
    AnubrekhanChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, "anub'rekhan choose target"), helper(ai) {}
    bool Execute(Event event) override;

private:
    AnubrekhanBossHelper helper;
};

// 32 waypoints put 6.9 yd between them, so the main tank's kite tracks the circle instead of
// jumping across it.
class AnubrekhanPositionAction : public RotateAroundTheCenterPointAction
{
public:
    AnubrekhanPositionAction(PlayerbotAI* ai)
        : RotateAroundTheCenterPointAction(ai, "anub'rekhan position", AnubrekhanBossHelper::RoomCenterX,
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

class AnubrekhanRedirectThreatAction : public NaxxRedirectThreatAction
{
public:
    AnubrekhanRedirectThreatAction(PlayerbotAI* ai)
        : NaxxRedirectThreatAction(ai, "anub'rekhan redirect threat"), helper(ai)
    {
    }

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;

private:
    AnubrekhanBossHelper helper;
};

class GluthChooseTargetAction : public AttackAction
{
public:
    GluthChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, "gluth choose target"), helper(ai) {}
    bool Execute(Event event) override;

private:
    GluthBossHelper helper;
};

class GluthPositionAction : public RotateAroundTheCenterPointAction
{
public:
    GluthPositionAction(PlayerbotAI* ai)
        : RotateAroundTheCenterPointAction(ai, "gluth position", 3293.61f, -3149.01f, 12.0f, 12), helper(ai)
    {
    }
    bool Execute(Event event) override;

private:
    GluthBossHelper helper;
};

class GluthSlowdownAction : public Action
{
public:
    GluthSlowdownAction(PlayerbotAI* ai) : Action(ai, "gluth slowdown"), helper(ai) {}
    bool Execute(Event event) override;

private:
    GluthBossHelper helper;
};

class GluthTranquilizingShotAction : public Action
{
public:
    GluthTranquilizingShotAction(PlayerbotAI* ai) : Action(ai, "gluth tranquilizing shot"), helper(ai) {}
    bool Execute(Event event) override;

private:
    GluthBossHelper helper;
};

class GluthRedirectThreatAction : public NaxxRedirectThreatAction
{
public:
    GluthRedirectThreatAction(PlayerbotAI* ai) : NaxxRedirectThreatAction(ai, "gluth redirect threat"), helper(ai) {}

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
    LoathebPositionAction(PlayerbotAI* ai) : MovementAction(ai, "loatheb position"), helper(ai) {}
    virtual bool Execute(Event event);

private:
    LoathebBossHelper helper;
};

class LoathebChooseTargetAction : public AttackAction
{
public:
    LoathebChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, "loatheb choose target"), helper(ai) {}
    virtual bool Execute(Event event);

private:
    LoathebBossHelper helper;
};

class NothChooseTargetAction : public AttackAction
{
public:
    NothChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, "noth choose target"), helper(ai) {}
    bool Execute(Event event) override;

private:
    NothBossHelper helper;
};

class NothPositionAction : public MovementAction
{
public:
    NothPositionAction(PlayerbotAI* ai) : MovementAction(ai, "noth position"), helper(ai) {}
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
    NothDispelCurseAction(PlayerbotAI* ai) : Action(ai, "noth dispel curse"), helper(ai) {}
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
    MaexxnaAttackWebWrapAction(PlayerbotAI* ai) : AttackAction(ai, "maexxna attack web wrap") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MaexxnaTankSpiderlingsAction : public AttackAction
{
public:
    MaexxnaTankSpiderlingsAction(PlayerbotAI* ai) : AttackAction(ai, "maexxna tank spiderlings") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Gothik the Harvester
class GothikChooseTargetAction : public AttackAction
{
public:
    GothikChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, "gothik choose target"), helper(ai) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    GothikBossHelper helper;
};

class GothikStayOnLivingSideAction : public MovementAction
{
public:
    GothikStayOnLivingSideAction(PlayerbotAI* ai) : MovementAction(ai, "gothik stay on living side"), helper(ai) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    GothikBossHelper helper;
};

#endif