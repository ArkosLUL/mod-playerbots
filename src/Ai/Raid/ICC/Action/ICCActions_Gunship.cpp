/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_Gunship.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "EquipAction.h"
#include "ICCHelpers_Shared.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "Vehicle.h"

namespace
{
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
constexpr uint32 WALK_STALL_MS = 500;
constexpr float DODGE_STEP = 1.5f;
constexpr float DODGE_SWEEP = 12.0f;
constexpr uint32 DODGE_HEADINGS = 16;
constexpr uint32 ZAFOD_GOSSIP_MENU_ID = 10885;
constexpr uint32 ZAFOD_GOSSIP_OPTION = 0;

bool ClearOf(std::vector<EncounterHelpers::HazardCircle> const& hazards, float x, float y, float margin)
{
    return std::none_of(hazards.begin(), hazards.end(), [x, y, margin](EncounterHelpers::HazardCircle const& hazard)
                        { return hazard.first.GetExactDist2d(x, y) < hazard.second + margin; });
}

bool SweepDeck(Player* bot, GsbDeck deck, std::vector<EncounterHelpers::HazardCircle> const& hazards, float margin,
               Position const& prefer, Position& spot)
{
    for (float radius = DODGE_STEP; radius <= DODGE_SWEEP; radius += DODGE_STEP)
    {
        bool found = false;
        float best = 0.0f;
        for (uint32 heading = 0; heading < DODGE_HEADINGS; ++heading)
        {
            float const angle = 2.0f * static_cast<float>(M_PI) * heading / DODGE_HEADINGS;
            float const x = bot->GetPositionX() + radius * std::cos(angle);
            float const y = bot->GetPositionY() + radius * std::sin(angle);
            float const z = bot->GetPositionZ();
            if (!ClearOf(hazards, x, y, margin) || !GsbOnDeck(bot, deck, x, y, z))
                continue;

            float const score = prefer.GetExactDist2d(x, y);
            if (!found || score < best)
            {
                spot = Position(x, y, z);
                best = score;
                found = true;
            }
        }

        if (found)
            return true;
    }

    return false;
}
}

Position const* IccGunshipWalkAction::WalkInFlight()
{
    if (!walking || !bot->isMoving() || !BookedOnWalkSpot())
        return nullptr;

    return &walkSpot;
}

bool IccGunshipWalkAction::WalkTo(Position const& spot, MovementPriority priority)
{
    if (bot->IsMovementPreventedByCasting())
        return false;

    uint32 const now = getMSTime();
    if (walking && !bot->isMoving() && getMSTimeDiff(walkIssuedMs, now) > WALK_STALL_MS && BookedOnWalkSpot())
        AI_VALUE(LastMovement&, "last movement").clear();

    walkSpot = spot;
    walking = true;
    walkIssuedMs = now;
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  false, priority);
}

bool IccGunshipWalkAction::BookedOnWalkSpot()
{
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    return std::fabs(last.lastMoveToX - walkSpot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - walkSpot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}

bool IccGunshipDodgeAction::Execute(Event /*event*/)
{
    if (!GsbDodging(botAI) || !botAI->CanMove())
        return false;

    GsbSide const* side = GsbSideOf(bot);
    if (!side)
        return false;

    std::vector<EncounterHelpers::HazardCircle> const hazards = GsbHazards(botAI);
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    if (!bot->isMoving() && last.msTime && getMSTimeDiff(last.msTime, getMSTime()) > WALK_STALL_MS)
        last.clear();

    if (bot->isMoving() && last.msTime && last.priority == MovementPriority::MOVEMENT_FORCED &&
        ClearOf(hazards, last.lastMoveToX, last.lastMoveToY, GSB_DODGE_TRIGGER_MARGIN))
        return true;

    Position prefer = bot->GetPosition();
    Position stand;
    if (GsbStandOf(botAI, stand) != GsbStandKind::None)
        prefer = stand;

    GsbDeck const deck = GsbDeckOf(*side, bot);
    Position spot;
    if (!SweepDeck(bot, deck, hazards, GSB_DODGE_CLEAR_MARGIN, prefer, spot) &&
        !SweepDeck(bot, deck, hazards, GSB_DODGE_TRIGGER_MARGIN + 0.5f, prefer, spot))
        return false;

    if (bot->GetMap()->IsHeroic() && bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);

    last.clear();
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_FORCED, true);
}

bool IccGunshipRocketJumpAction::Execute(Event /*event*/)
{
    Position spot;
    GsbJump const plan = GsbJumpPlan(botAI, spot);
    if (plan == GsbJump::Walk)
    {
        if (Position const* inFlight = WalkInFlight())
            if (inFlight->GetExactDist2d(spot) <= WALK_SPOT_TOLERANCE)
                return true;

        return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT);
    }

    DropWalk();
    if (plan != GsbJump::Fire)
        return false;

    Item* pack = bot->GetItemByEntry(ITEM_GOBLIN_ROCKET_PACK);
    if (!pack || bot->HasSpellCooldown(SPELL_ROCKET_PACK_USE) || bot->CanUseItem(pack) != EQUIP_ERR_OK ||
        bot->IsNonMeleeSpellCast(true))
        return false;

    if (bot->isMoving())
    {
        bot->GetMotionMaster()->Clear();
        bot->StopMoving();
        return true;
    }

    return Fire(pack, spot);
}

bool IccGunshipRocketJumpAction::Fire(Item* pack, Position const& destination)
{
    uint8 const bagIndex = pack->GetBagSlot();
    uint8 const slot = pack->GetSlot();
    uint8 const castCount = 1;
    uint32 const spellId = SPELL_ROCKET_PACK_USE;
    ObjectGuid const itemGuid = pack->GetGUID();
    uint32 const glyphIndex = 0;
    uint8 const castFlags = 0;

    WorldPacket packet(CMSG_USE_ITEM);
    packet << bagIndex << slot << castCount << spellId << itemGuid << glyphIndex << castFlags;
    packet << uint32(TARGET_FLAG_DEST_LOCATION);
    packet.appendPackGUID(0);
    packet << destination.GetPositionX() << destination.GetPositionY() << destination.GetPositionZ();
    bot->GetSession()->HandleUseItemOpcode(packet);
    return true;
}

bool IccGunshipTauntAction::Execute(Event /*event*/)
{
    Unit* target = GsbTauntTarget(botAI);
    if (!target || !IccCastClassTaunt(bot, botAI, target))
        return false;

    AttackDirect(target);
    return true;
}

bool IccGunshipTankDefensiveAction::Execute(Event /*event*/)
{
    uint32 const spellId = GsbTankDefensive(botAI);
    if (!spellId || !botAI->CastSpell(spellId, bot))
        return false;

    GsbSpentTankDefensive(botAI);
    return true;
}

bool IccGunshipEnterCannonAction::Execute(Event /*event*/)
{
    if (bot->GetVehicle() || !GsbWantsCannon(botAI))
        return false;

    Creature* cannon = GsbFreeCannon(botAI);
    if (!cannon)
        return false;

    if (bot->GetDistance(cannon) > INTERACTION_DISTANCE)
        return MoveTo(cannon);

    botAI->RemoveShapeshift();
    bot->GetMotionMaster()->Clear();
    bot->StopMoving();
    cannon->HandleSpellClick(bot);
    if (!bot->IsOnVehicle(cannon))
        return false;

    WorldPacket emptyPacket;
    bot->GetSession()->HandleCancelMountAuraOpcode(emptyPacket);
    return true;
}

bool IccCannonFireAction::Execute(Event /*event*/)
{
    Unit* cannon = bot->GetVehicleBase();
    if (!cannon || !bot->GetVehicle())
        return false;

    Unit* target = GsbCannonTarget(botAI, cannon);
    if (!target)
        return false;

    if (cannon->GetPower(POWER_ENERGY) >= GSB_INCINERATE_HEAT)
        return TryCast("incinerating blast", target, cannon);

    return TryCast("cannon blast", target, cannon);
}

bool IccCannonFireAction::TryCast(char const* spell, Unit* target, Unit* cannon)
{
    uint32 const spellId = AI_VALUE2(uint32, "vehicle spell id", spell);
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info || !botAI->CanCastVehicleSpell(spellId, target))
        return false;

    uint32 const heat = cannon->GetPower(POWER_ENERGY);
    if (!botAI->CastVehicleSpell(spellId, target) || cannon->GetPower(POWER_ENERGY) == heat)
        return false;

    cannon->AddSpellCooldown(spellId, 0, std::max(info->RecoveryTime, info->CategoryRecoveryTime));
    return true;
}

bool IccGunshipStandAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GsbStandWalk(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= WALK_SPOT_TOLERANCE)
            return true;

    return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT);
}

bool IccGunshipRocketPackSetupAction::Execute(Event /*event*/)
{
    Item* pack = bot->GetItemByEntry(ITEM_GOBLIN_ROCKET_PACK);
    if (!pack)
        return AcquireRocketPack();

    return !pack->IsEquipped() && EquipRocketPack();
}

bool IccGunshipRocketPackSetupAction::AcquireRocketPack()
{
    Creature* zafod = bot->FindNearestCreature(NPC_ZAFOD_BOOMBOX, GSB_ZAFOD_RANGE);
    if (!zafod)
        return false;

    if (bot->GetDistance(zafod) > INTERACTION_DISTANCE)
        return MoveTo(bot->GetMapId(), zafod->GetPositionX(), zafod->GetPositionY(), zafod->GetPositionZ(), false,
                      false, false, false, MovementPriority::MOVEMENT_NORMAL);

    bot->GetMotionMaster()->Clear();
    bot->StopMoving();
    bot->SetFacingToObject(zafod);

    WorldPacket gossipHello(CMSG_GOSSIP_HELLO);
    gossipHello << zafod->GetGUID();
    bot->GetSession()->HandleGossipHelloOpcode(gossipHello);

    WorldPacket gossipSelect(CMSG_GOSSIP_SELECT_OPTION);
    gossipSelect << zafod->GetGUID();
    gossipSelect << uint32(ZAFOD_GOSSIP_MENU_ID) << uint32(ZAFOD_GOSSIP_OPTION);
    bot->GetSession()->HandleGossipSelectOptionOpcode(gossipSelect);
    return true;
}

bool IccGunshipRocketPackSetupAction::EquipRocketPack()
{
    EquipAction* equip = dynamic_cast<EquipAction*>(botAI->GetAiObjectContext()->GetAction("equip"));
    if (!equip)
        return false;

    ItemIds ids;
    ids.insert(ITEM_GOBLIN_ROCKET_PACK);
    equip->EquipItems(ids, true);
    return true;
}
