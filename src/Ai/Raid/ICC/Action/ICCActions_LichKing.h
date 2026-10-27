/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_LICHKING_H
#define PLAYERBOTS_ICCACTIONS_LICHKING_H

#include "Action.h"
#include "AttackAction.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ICCData.h"
#include "ICCHelpers_LichKing.h"
#include "ICCHelpers_Shared.h"
#include "ICCScripts.h"
#include "LastMovementValue.h"
#include "MovementActions.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <set>

class IccLichKingShadowTrapAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc lich king shadow trap";

    IccLichKingShadowTrapAction(PlayerbotAI* botAI)
        : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingNecroticPlagueAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc lich king necrotic plague";

    IccLichKingNecroticPlagueAction(PlayerbotAI* botAI)
        : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingWinterAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc lich king winter";

    IccLichKingWinterAction(PlayerbotAI* botAI)
        : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;

    bool IsPositionSafeFromDefile(float x, float y, float z, float minSafeDistance) const;
    bool IsPositionSafeFromShadowTraps(float x, float y) const;
    bool IsValidCollectibleAdd(Unit* unit) const;
    bool HandleTankPositioning();
    bool HandleMeleePositioning();
    bool HandleRangedPositioning();
    bool HandleMainTankAddManagement(Unit* boss, Position const* tankPos);
    bool HandleAssistTankAddManagement(Unit* boss, Position const* tankPos);
    bool HandlePetManagement();

private:
    static constexpr float PLATFORM_Z      = 840.857f;
    static constexpr float BEHIND_DISTANCE = 4.0f;
    bool FixPlatformPosition();
    bool ClearInvalidTarget();
    Position const* GetMainTankPosition();
    Position const* GetMainTankRangedPosition();
    bool TryMoveToPosition(float x, float y, float z, bool forced = true);
};

class IccLichKingAddsAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc lich king adds";

    IccLichKingAddsAction(PlayerbotAI* botAI)
        : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;

    bool HandleTeleportationFixes(Difficulty diff, Unit* terenas);
    bool HandleHeroicNonTankPositioning(Difficulty diff, Unit* terenas);
    bool HandleSpiritMarkingAndTargeting(Difficulty diff, Unit* terenas);
    bool HandleQuakeMechanics(Unit* boss);
    bool HandleShamblingHorrors(Unit* boss, bool hasPlague);
    bool HandleRagingSpiritFlanking(Unit* boss, bool hasPlague, Difficulty diff);
    bool HandleAssistTankAddManagement(Unit* boss, Difficulty diff);
    bool HandleMeleePositioning(Unit* boss, bool hasPlague, Difficulty diff);
    bool HandleMainTankTargeting(Unit* boss, Difficulty diff);
    bool HandleNonTankHeroicPositioning(Unit* boss, Difficulty diff, bool hasPlague);
    bool HandleRangedPositioning(Unit* boss, bool hasPlague, Difficulty diff);
    bool HandleDefileMechanics(Unit* boss, Difficulty diff);
    bool HandleCenterStacking(Unit* boss, Difficulty diff);
    bool HandleValkyrMechanics(Difficulty diff);
    bool HandleValkyrMarking(std::vector<Unit*> const& valkyrs, Difficulty diff);
    bool HandleValkyrAssignment(std::vector<Unit*> const& valkyrs);
    bool HandleVileSpiritMechanics();
    bool HandleIceSphereMechanics();
    bool ApplyCCToValkyr(Unit* valkyr);
    bool IsValkyr(Unit* unit);
    std::pair<float, float> DefileAwareStep(float tx, float ty,
                                           std::vector<Unit*> const& defiles,
                                           Difficulty diff);

private:
    // Read once per Execute, for the handlers it calls
    IccLichKingPhaseId _phase = IccLichKingPhaseId::One;
};

class IccLichKingSpiritBombAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc lich king spirit bomb";

    IccLichKingSpiritBombAction(PlayerbotAI* botAI)
        : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

    static bool IsBombThreatActive(PlayerbotAI* botAI, Player* bot);
};

#endif
