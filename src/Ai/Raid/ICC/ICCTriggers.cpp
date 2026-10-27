/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers.h"
#include "GenericTriggers.h"
#include "GridNotifiers.h"
#include "ICCActions.h"
#include "ICCScripts.h"
#include "NearestNpcsValue.h"
#include "ObjectAccessor.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidDifficulty.h"
#include "RaidObs.h"
#include "Trigger.h"
#include "Vehicle.h"

//Lord Marrogwar
bool IccLmTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "lord marrowgar");
    if (!boss)
        return false;

    IccStripExperienced(bot);

    return true;
}

//Lady Deathwhisper
bool IccDarkReckoningTrigger::IsActive()
{
    if (bot->HasAura(SPELL_DARK_RECKONING))
        return true;

    return false;
}

bool IccLadyDeathwhisperTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "lady deathwhisper");
    if (!boss)
        return false;

    return true;
}

//Gunship Battle
bool IccRottingFrostGiantTankPositionTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "rotting frost giant");
    if (!boss)
        return false;

    IccMarkPull(bot, boss, "rotting-frost-giant");
    IccStripExperienced(bot);

    return true;
}

bool IccInCannonTrigger::IsActive()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    Vehicle* vehicle = bot->GetVehicle();
    if (!vehicleBase || !vehicle)
        return false;

    uint32 entry = vehicleBase->GetEntry();
    return entry == NPC_CANNONA || entry == NPC_CANNONH;
}

bool IccGunshipCannonNearTrigger::IsActive()
{
    if (bot->GetVehicle() || !PlayerbotAI::IsDps(bot))
        return false;

    if (!bot->FindNearestCreature(NPC_CANNONA, 100.0f) && !bot->FindNearestCreature(NPC_CANNONH, 100.0f))
        return false;

    return !IccGunshipCannonsFrozen(bot);
}

// Keyed on the enemy captain rather than on cannon friendliness, which the cannon-near trigger uses.
bool IccGunshipRocketJumpTrigger::IsActive() { return IccGunshipEnemyCaptain(bot, 100.0f) != nullptr; }

bool IccGunshipRocketPackSetupTrigger::IsActive()
{
    // Fires any time a bot is standing on a friendly gunship deck, regardless of
    // combat state. Lets bots walk to Zafod and equip the rocket pack before the
    // encounter starts (and keep it ready if they acquire it mid-fight).
    Unit* cannonA = bot->FindNearestCreature(NPC_CANNONA, 100.0f);
    if (cannonA && cannonA->IsFriendlyTo(bot))
        return true;

    Unit* cannonH = bot->FindNearestCreature(NPC_CANNONH, 100.0f);
    if (cannonH && cannonH->IsFriendlyTo(bot))
        return true;

    return false;
}

//DBS
bool IccDbsTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "deathbringer saurfang");
    if (!boss)
        return false;

    IccStripExperienced(bot);

    return true;
}

bool IccDogsTrigger::IsActive()
{
    Unit* dog = AI_VALUE2(Unit*, "find target", "stinky");
    if (!dog)
        dog = AI_VALUE2(Unit*, "find target", "precious");
    if (!dog)
        return false;

    IccMarkPull(bot, dog, "stinky-and-precious");
    return true;
}

//FESTERGUT
bool IccFestergutSporeCycle(Player* bot)
{
    bool const spores = IccAnyGroupMemberHasAura(bot, SPELL_GAS_SPORE);
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "fg.phase", spores ? "spore" : "normal");

    return spores;
}

bool IccFestergutGroupPositionTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "festergut");
    if (!boss)
        return false;

    IccStripExperienced(bot);

    return true;
}

bool IccFestergutSporeTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "festergut");
    if (!boss || PlayerbotAI::IsTank(bot))
        return false;

    return IccFestergutSporeCycle(bot);
}

bool IccFestergutAvoidMalleableGooTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "festergut");
    if (!boss)
        return false;

    // Tanks hold the boss at the fixed tank spot; goo can land on tanks but
    // moving would lose threat and let goo land on melee stack anyway.
    if (PlayerbotAI::IsTank(bot))
        return false;

    // During spore phase, position switching handles goo avoidance — free-dodge
    // would pull bots out of their assigned spore spots.
    if (IccFestergutSporeCycle(bot))
        return false;

    constexpr uint32 impactLifetimeMs = 8000;
    constexpr float gooDangerRadius = 12.0f;

    uint32 now = getMSTime();
    float botX = bot->GetPositionX();
    float botY = bot->GetPositionY();
    ObjectGuid botGuid = bot->GetGUID();

    IcecrownHelpers::IccInstanceState& st = IcecrownHelpers::IccState(bot->GetMap()->GetInstanceId());

    for (auto const& impact : st.malleableGoo)
    {
        if (getMSTimeDiff(impact.castTime, now) > impactLifetimeMs)
            continue;
        float dx = botX - impact.position.GetPositionX();
        float dy = botY - impact.position.GetPositionY();
        if (dx * dx + dy * dy < gooDangerRadius * gooDangerRadius)
        {
            uint32 waitUntil = impact.castTime + impactLifetimeMs;
            auto& slot = st.festergutGooWaitUntil[botGuid];
            if (waitUntil > slot)
                slot = waitUntil;
            if (RaidObs::Active())
                RaidObs::NoteDerived(bot, "fg.goohold", "dodge");
            return true;
        }
    }

    auto it = st.festergutGooWaitUntil.find(botGuid);
    if (it != st.festergutGooWaitUntil.end())
    {
        if (now < it->second)
        {
            if (RaidObs::Active())
                RaidObs::NoteDerived(bot, "fg.goohold", "hold");
            return true;
        }
        st.festergutGooWaitUntil.erase(it);
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "fg.goohold", "none");
    }

    return false;
}

//ROTFACE
bool IccRotfaceTankPositionTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "rotface");
    if (!boss || !(PlayerbotAI::IsTank(bot) || PlayerbotAI::IsMelee(bot)))
        return false;

    IccStripExperienced(bot);

    return true;
}

bool IccRotfaceGroupPositionTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "rotface");
    if (!boss)
        return false;

    return true;
}

bool IccRotfaceMoveAwayFromExplosionTrigger::IsActive()
{
    Creature* boss = bot->FindNearestCreature(NPC_BIG_OOZE, 100.0f);
    bool castingNow = boss && boss->IsAlive() &&
        boss->HasUnitState(UNIT_STATE_CASTING) && boss->FindCurrentSpellBySpellId(SPELL_UNSTABLE_OOZE_EXPLOSION);

    if (castingNow)
    {
        _wasCasting = true;
        _castEnded = false;
        return true;
    }

    // Cast just ended — record the time
    if (_wasCasting)
    {
        _wasCasting = false;
        _castEnded = true;
        _castEndMs = getMSTime();
    }

    // Stay active for 6 seconds after cast ended (2s wait + return movement)
    if (_castEnded && getMSTimeDiff(_castEndMs, getMSTime()) < 6000)
        return true;

    _castEnded = false;
    return false;
}

bool IccRotfaceAvoidVileGasTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "rotface");
    if (!boss)
        return false;

    uint32 const now = getMSTime();

    IcecrownHelpers::IccInstanceState& st = IcecrownHelpers::IccState(bot->GetMap()->GetInstanceId());
    bool const isVictim =
        st.rotfaceVileGas.victimGuid == bot->GetGUID() &&
        getMSTimeDiff(st.rotfaceVileGas.castTime, now) < 8000;
    if (isVictim)
        return true;

    if (botAI->HasAura("Vile Gas", bot))
        return true;

    auto const& waitMap = st.rotfaceVileGasWaitUntil;
    auto it = waitMap.find(bot->GetGUID());
    if (it != waitMap.end() && now < it->second)
        return true;

    return false;
}

//PP
uint8 IccPutricidePhase(Player* bot, Unit* boss)
{
    uint8 const phase = boss->HealthAbovePct(80) ? 1 : (boss->HealthAbovePct(35) ? 2 : 3);
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "pp.phase", std::to_string(phase));

    return phase;
}

void IccPutricideForgetBloatFlee(Player* bot)
{
    auto& lastDir = IcecrownHelpers::IccState(bot->GetMap()->GetInstanceId()).ppBloatLastDir;
    if (lastDir.erase(bot->GetGUID()) && RaidObs::Active())
        RaidObs::NoteDerived(bot, "pp.bloatflee", "none");
}

bool IccPutricideGrowingOozePuddleTrigger::IsActive()
{
    // Early return if boss doesn't exist
    Unit* boss = AI_VALUE2(Unit*, "find target", "professor putricide");
    if (!boss)
        return false;

    GuidVector const& npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto const& npc : npcs)
    {
        if (Unit* unit = botAI->GetUnit(npc))
        {
            const uint32 entry = unit->GetEntry();
            if (entry == NPC_GROWING_OOZE_PUDDLE)
                return true;
        }
    }

    return false;
}

bool IccPutricideVolatileOozeTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "volatile ooze");
    if (!boss)
        return false;

    if (botAI->HasAura("Gaseous Bloat", bot))
        return false;

    return true;
}

bool IccPutricideGasCloudTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "gas cloud");
    if (!boss)
        return false;

    Unit* boss1 = AI_VALUE2(Unit*, "find target", "volatile ooze");

    bool hasGaseousBloat = botAI->HasAura("Gaseous Bloat", bot);
    // each bloat is a new cast on a new victim, so a direction kept from the last one is stale
    if (!hasGaseousBloat)
        IccPutricideForgetBloatFlee(bot);

    if (hasGaseousBloat && boss1)
        return true;

    if (boss1)
        return false;

    return true;
}

bool IccPutricideMutatedPlagueTrigger::IsActive()
{
    return AI_VALUE2(Unit*, "find target", "professor putricide") != nullptr;
}

bool IccPutricideMalleableGooTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "professor putricide");
    if (!boss)
        return false;

    Difficulty const diff = GetRaidMapDifficulty(bot);

    // Heroic cheat buffs — apply to all group members (bots + real players)
    if (boss && boss->IsInCombat() && sPlayerbotAIConfig.EnableICCBuffs &&
        (diff == RAID_DIFFICULTY_10MAN_HEROIC || diff == RAID_DIFFICULTY_25MAN_HEROIC))
    {
        if (Group* buffGroup = bot->GetGroup())
        {
            for (GroupReference* itr = buffGroup->GetFirstMember(); itr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member || !member->IsAlive() || !member->IsInWorld())
                    continue;

                IccApplyHeroicBuffToMember(bot, member, false, true, "pp.heroicbuff");

                if (PlayerbotAI::IsTank(member) && !member->HasAura(SPELL_SPITEFULL_FURY) &&
                    boss->GetVictim() != member)
                {
                    member->AddAura(SPELL_SPITEFULL_FURY, member);
                    IccNoteCheat(bot, "pp.spitefulfury", member);
                }
            }
        }
    }

    return true;
}

bool IccPutricideAbominationTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "professor putricide");
    if (!boss)
        return false;

    if (!PlayerbotAI::IsAssistTank(bot))
        return false;

    // Already piloting - keep action firing until vehicle drops.
    if (Unit* veh = bot->GetVehicleBase())
    {
        uint32 e = veh->GetEntry();
        if (e == NPC_MUTATED_ABOMINATION_10 || e == NPC_MUTATED_ABOMINATION_25)
            return true;
    }

    // Phase 3: boss takes toy back. No transformation.
    if (IccPutricidePhase(bot, boss) == 3)
        return false;

    // Someone else already piloting - do not drink.
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* m = itr->GetSource();
            if (!m || m == bot || !m->IsAlive())
                continue;
            if (Unit* vb = m->GetVehicleBase())
            {
                uint32 e = vb->GetEntry();
                if (e == NPC_MUTATED_ABOMINATION_10 || e == NPC_MUTATED_ABOMINATION_25)
                    return false;
            }
        }
    }

    // Require at least one Growing Ooze Puddle nearby.
    GuidVector const npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto const& g : npcs)
    {
        if (Unit* u = botAI->GetUnit(g))
            if (u->GetEntry() == NPC_GROWING_OOZE_PUDDLE)
                return true;
    }

    return false;
}

//BPC
Unit* IccBpcEmpoweredPrince(PlayerbotAI* botAI, bool* present)
{
    struct Prince
    {
        char const* name;
        uint32 invocation;
        char const* phase;
    };
    static constexpr Prince PRINCES[] = {
        {"prince valanar", SPELL_INVOCATION_OF_BLOOD_VALANAR, "valanar"},
        {"prince keleseth", SPELL_INVOCATION_OF_BLOOD_KELESETH, "keleseth"},
        {"prince taldaram", SPELL_INVOCATION_OF_BLOOD_TALDARAM, "taldaram"},
    };

    auto* context = botAI->GetAiObjectContext();
    bool seen = false;
    Unit* empowered = nullptr;
    char const* phase = "none";
    for (Prince const& prince : PRINCES)
    {
        Unit* unit = AI_VALUE2(Unit*, "find target", prince.name);
        if (!unit)
            continue;

        seen = true;
        if (unit->HasAura(prince.invocation))
        {
            empowered = unit;
            phase = prince.phase;
            break;
        }
    }

    if (present)
        *present = seen;
    if (seen && RaidObs::Active())
        RaidObs::NoteDerived(botAI->GetBot(), "bpc.phase", phase);

    return empowered;
}

bool IccBpcKelesethTankTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "prince keleseth");
    if (!boss)
        return false;

    IccStripExperienced(bot);

    if (!PlayerbotAI::IsAssistTank(bot))
        return false;

    Aura* aura = botAI->GetAura("Shadow Prison", bot, false, true);
    if (aura)
        if (aura->GetStackAmount() > 18)
            return false;

    return true;
}

bool IccBpcMainTankTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    if (!IccAnyBloodPrincePresent(botAI))
        return false;

    IccStripExperienced(bot);

    return true;
}

bool IccBpcEmpoweredVortexTrigger::IsActive()
{
    // Tanks should ignore this mechanic
    if (PlayerbotAI::IsMainTank(bot) || PlayerbotAI::IsAssistTank(bot))
        return false;

    Unit* valanar = AI_VALUE2(Unit*, "find target", "prince valanar");
    if (!valanar)
        return false;

    Aura* aura = botAI->GetAura("Shadow Prison", bot, false, true);
    if (aura)
        if (aura->GetStackAmount() > 12)
            return false;

    if (IccBpcEmpoweredPrince(botAI) != valanar)
    {
        // Ends the spread window, so the next one writes its slot even if it lands on the same one.
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "bpc.spread", "none");
        return false;
    }

    return true;
}

bool IccBpcKineticBombTrigger::IsActive()
{
    if (!IccAnyBloodPrincePresent(botAI))
        return false;

    if (!PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsHeal(bot))
        return false;

    // Allow up to 18 stacks for bomb-assigned bots (multiplier handles assignment)
    if (Aura* aura = botAI->GetAura("Shadow Prison", bot, false, true))
    {
        if (aura->GetStackAmount() > 18)
            return false;
    }

    static const std::array<uint32, 4> bombEntries = {NPC_KINETIC_BOMB1, NPC_KINETIC_BOMB2, NPC_KINETIC_BOMB3,
                                                      NPC_KINETIC_BOMB4};
    const GuidVector bombs = AI_VALUE(GuidVector, "possible targets no los");

    bool bombFound = false;

    for (const auto entry : bombEntries)
    {
        for (auto const& guid : bombs)
        {
            if (Unit* unit = botAI->GetUnit(guid))
            {
                if (unit->GetEntry() == entry)
                {
                    // Check if bomb is within valid Z-axis range
                    if (unit->GetPositionZ() - bot->GetPositionZ() < 35.0f)
                    {
                        bombFound = true;
                        break;
                    }
                }
            }
        }
        if (bombFound)
            break;
    }

    return PlayerbotAI::IsRangedDps(bot) && bombFound;
}

bool IccBpcBallOfFlameTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot))
        return false;

    bool present = false;
    Unit* empowered = IccBpcEmpoweredPrince(botAI, &present);
    if (!present)
        return false;

    if (empowered && empowered->GetEntry() == NPC_PRINCE_TALDARAM)
        return true;

    // A ball chases its victim whoever is empowered, and the multiplier leaves that bot only the ball action.
    for (uint32 entry : {NPC_BALL_OF_FLAME, NPC_BALL_OF_INFERNO_FLAME})
        if (Unit* ball = bot->FindNearestCreature(entry, 100.0f))
            if (ball->GetVictim() == bot)
                return true;

    return false;
}

// BQL
IccBqlPhase IccBqlPhaseOf(Player* bot, Unit* boss)
{
    IccBqlPhase phase = IccBqlPhase::Ground;
    Creature* queen = boss->ToCreature();
    if (queen && queen->HasReactState(REACT_PASSIVE))
        phase = boss->IsLevitating() ? IccBqlPhase::Air : IccBqlPhase::Center;

    if (RaidObs::Active())
    {
        static char const* const NAMES[] = {"ground", "center", "air"};
        RaidObs::NoteDerived(bot, "bql.phase", NAMES[static_cast<uint8>(phase)]);
    }

    return phase;
}

bool IccBqlGroupPositionTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "blood-queen lana'thel");
    if (!boss)
        return false;

    if (IccAnyBloodPrincePresent(botAI))
        return false;

    IccStripExperienced(bot);

    return true;
}

bool IccBqlPactOfDarkfallenTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "blood-queen lana'thel");
    if (!boss)
        return false;

    if (IccAnyBloodPrincePresent(botAI))
        return false;

    Aura* aura = botAI->GetAura("Pact of the Darkfallen", bot);
    if (!aura)
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "bql.pact", "none");
        return false;
    }

    return true;
}

bool IccBqlVampiricBiteTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "blood-queen lana'thel");
    if (!boss)
        return false;

    if (IccAnyBloodPrincePresent(botAI))
        return false;

    Aura* aura = botAI->GetAura("Frenzied Bloodthirst", bot);
    if (!aura)
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "bql.bite", "none");
        return false;
    }

    return true;
}

// Sister Svalna
bool IccValkyreSpearTrigger::IsActive()
{
    // Check if there's a spear nearby
    if (bot->FindNearestCreature(NPC_SPEAR, 100.0f))
        return true;

    return false;
}

bool IccSisterSvalnaTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sister svalna");
    if (!boss)
        return false;

    if (bot->GetExactDist2d(boss) > 30.0f)
        return false;

    return true;
}

// VDW
bool IccValithriaGroupTrigger::IsActive()
{
    Unit* boss = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f);
    if (!boss)
        return false;

    IccStripExperienced(bot);

    return true;
}

bool IccValithriaZombieKiteTrigger::IsActive()
{
    Unit* boss = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f);
    if (!boss)
        return false;

    if (PlayerbotAI::IsTank(bot))
        return false;

    std::list<Creature*> zombies;
    bot->GetCreatureListWithEntryInGrid(zombies, NPC_BLISTERING_ZOMBIE, 100.0f);
    for (Creature* z : zombies)
    {
        if (z && z->IsAlive() && z->GetVictim() == bot)
            return true;
    }

    return false;
}

bool IccValithriaPortalTrigger::IsActive()
{
    Unit* boss = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f);
    if (!boss)
        return false;

    Aura* aura = botAI->GetAura("Twisted Nightmares", bot, false, true);
    if (aura && aura->GetStackAmount() >= 25)
        return false;

    // Only healers should use portals
    if (!PlayerbotAI::IsHeal(bot) || bot->HasAura(SPELL_DREAM_STATE))
        return false;

    Creature* worm = bot->FindNearestCreature(NPC_ROT_WORM, 100.0f);
    Creature* zombie = bot->FindNearestCreature(NPC_BLISTERING_ZOMBIE, 100.0f);

    if ((worm && worm->GetVictim() == bot) || (zombie && zombie->GetVictim() == bot))
        return false;

    auto const shouldHealRaid = IccValithriaShouldHealRaid(bot);
    if (!shouldHealRaid.has_value() || *shouldHealRaid)
        return false;

    // Find the nearest portal creature
    Creature* portal1 = bot->FindNearestCreature(NPC_DREAM_PORTAL, 100.0f);
    if (!portal1)
        portal1 = bot->FindNearestCreature(NPC_DREAM_PORTAL_PRE_EFFECT, 100.0f);

    Creature* portal2 = bot->FindNearestCreature(NPC_NIGHTMARE_PORTAL, 100.0f);
    if (!portal2)
        portal2 = bot->FindNearestCreature(NPC_NIGHTMARE_PORTAL_PRE_EFFECT, 100.0f);

    return portal1 || portal2;
}

bool IccValithriaHealTrigger::IsActive()
{
    Unit* boss = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f);
    if (!boss)
        return false;

    // Only healers should use healing
    if (!PlayerbotAI::IsHeal(bot) || bot->HasAura(SPELL_DREAM_STATE) || bot->HealthBelowPct(50))
        return false;

    Creature* worm = bot->FindNearestCreature(NPC_ROT_WORM, 100.0f);
    Creature* zombie = bot->FindNearestCreature(NPC_BLISTERING_ZOMBIE, 100.0f);

    if ((worm && worm->GetVictim() == bot) || (zombie && zombie->GetVictim() == bot))
        return false;

    auto const shouldHealRaid = IccValithriaShouldHealRaid(bot);
    if (!shouldHealRaid.has_value() || *shouldHealRaid)
        return false;

    if (bot->GetHealthPct() < 50.0f)
        return false;

    // For Valithria healers, check portal logic
    // If no portal is found within 100 yards, we should heal
    if (!bot->FindNearestCreature(NPC_DREAM_PORTAL, 100.0f) && !bot->FindNearestCreature(NPC_NIGHTMARE_PORTAL, 100.0f))
        return true;

    if (bot->FindNearestCreature(NPC_DREAM_PORTAL, 10.0f) || bot->FindNearestCreature(NPC_NIGHTMARE_PORTAL, 10.0f))
        return false;

    // If portal is far but within 100 yards, heal while moving to it
    return true;
}

bool IccValithriaDreamCloudTrigger::IsActive()
{
    if (!bot->HasAura(SPELL_DREAM_STATE) || bot->HealthBelowPct(50))
        return false;

    return true;
}

//SINDRAGOSA
bool IccSindragosaGroupPositionTrigger::IsActive()
{
    Unit* boss = bot->FindNearestCreature(NPC_SINDRAGOSA, 200.0f);  // sindra
    if (!boss)
        return false;

    Difficulty diff = GetRaidMapDifficulty(bot);

    if (sPlayerbotAIConfig.EnableICCBuffs && boss->IsInCombat() && diff &&
        (diff == RAID_DIFFICULTY_10MAN_HEROIC || diff == RAID_DIFFICULTY_25MAN_HEROIC))
    {
        //-------CHEAT-------
        // Apply to every alive group member so real players benefit too,
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member || !member->IsAlive() || !member->IsInWorld())
                    continue;

                IccApplyHeroicBuffToMember(bot, member, false, true, "sg.heroicbuff");

                if (PlayerbotAI::IsMainTank(member) && boss->GetVictim() != member &&
                    !member->HasAura(SPELL_SPITEFULL_FURY))
                {
                    member->AddAura(SPELL_SPITEFULL_FURY, member);
                    IccNoteCheat(bot, "sg.spitefulfury", member);
                }
            }
        }
        //-------CHEAT-------
    }

    // Air phase: give all tanks nitro boosts so they can quickly reposition to tombs
    if (boss->IsInCombat() && PlayerbotAI::IsTank(bot) &&
        boss->GetExactDist2d(ICC_SINDRAGOSA_FLYING_POSITION.GetPositionX(), ICC_SINDRAGOSA_FLYING_POSITION.GetPositionY()) < 50.0f)
    {
        if (!bot->HasAura(SPELL_NITRO_BOOSTS) && IccRaidCheat(botAI, "sg.nitro"))
            bot->AddAura(SPELL_NITRO_BOOSTS, bot);
    }

    // Last phase: the main tank must keep tanking, never run to a tomb spot.
    // Strip its Frost Beacon so the tomb-positioning logic doesn't apply. The
    // assist tank keeps its beacon and is allowed to move to the beacon spot.
    if (PlayerbotAI::IsMainTank(bot) && bot->HasAura(SPELL_FROST_BEACON) && boss->HealthBelowPct(35) &&
        boss->GetExactDist2d(ICC_SINDRAGOSA_FLYING_POSITION.GetPositionX(),
                             ICC_SINDRAGOSA_FLYING_POSITION.GetPositionY()) >= 30.0f &&
        IccRaidCheat(botAI, "sg.frostbeacon"))
        bot->RemoveAura(SPELL_FROST_BEACON);

    if (PlayerbotAI::IsTank(bot))
    {
        // Strip Mystic Buffet: the cheat that makes single-tanking P3 survivable.
        Aura* buffet = botAI->GetAura("mystic buffet", bot, false, false);
        if (buffet && IccRaidCheat(botAI, "sg.mysticbuffet"))
            bot->RemoveAura(buffet->GetId());

        // Shield tanks while a tomb is up, Blistering Cold is casting, or a last
        // phase beacon is out (tomb about to form, healers already scattering).
        bool const shield = (boss->HasUnitState(UNIT_STATE_CASTING) && IccBossCastingBlisteringCold(boss)) ||
                            (boss->HealthBelowPct(35) && IccAnyGroupMemberHasAura(bot, SPELL_FROST_BEACON)) ||
                            !IccGetCreaturesByEntries(bot, {NPC_TOMB1, NPC_TOMB2, NPC_TOMB3, NPC_TOMB4}, 150.0f).empty();
        if (shield && !bot->HasAura(SPELL_MAGIC_BARRIER))
        {
            if (IccRaidCheat(botAI, "sg.magicbarrier"))
                bot->AddAura(SPELL_MAGIC_BARRIER, bot);
        }
        else if (!shield && bot->HasAura(SPELL_MAGIC_BARRIER))
            bot->RemoveAura(SPELL_MAGIC_BARRIER);

        // Assist tanks generate no threat while another main tank is alive, so
        // they can never win the threat race at the pull; dropped on promotion.
        bool mtAlive = false;
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (member && member != bot && member->IsAlive() && PlayerbotAI::IsMainTank(member))
                {
                    mtAlive = true;
                    break;
                }
            }
        }

        if (mtAlive && !PlayerbotAI::IsMainTank(bot))
        {
            if (!bot->HasAura(SPELL_NO_THREAT) && IccRaidCheat(botAI, "sg.nothreat"))
                bot->AddAura(SPELL_NO_THREAT, bot);
        }
        else if (bot->HasAura(SPELL_NO_THREAT))
            bot->RemoveAura(SPELL_NO_THREAT);
    }

    if (!boss || bot->HasAura(SPELL_FROST_BEACON) || boss->GetExactDist2d(ICC_SINDRAGOSA_FLYING_POSITION.GetPositionX(), ICC_SINDRAGOSA_FLYING_POSITION.GetPositionY()) < 50.0f)
        return false;

    return true;
}

bool IccSindragosaFrostBeaconTrigger::IsActive()
{
    Unit* boss = bot->FindNearestCreature(NPC_SINDRAGOSA, 200.0f);
    if (!boss)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    if (bot->HasAura(SPELL_FROST_BEACON))
        return true;

    Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); ++itr)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (!member || !member->IsAlive())
            continue;

        if (member->HasAura(SPELL_FROST_BEACON))  // If any player has Frost Beacon, keep trigger active
            return true;
    }

    return false;
}

bool IccSindragosaHotTrigger::IsActive()
{
    if (!PlayerbotAI::IsHeal(bot))
        return false;

    if (bot->HasAura(SPELL_FROST_BEACON))
        return false;

    Unit* boss = bot->FindNearestCreature(NPC_SINDRAGOSA, 200.0f);
    if (!boss)
        return false;

    return IccAnyGroupMemberHasAura(bot, SPELL_FROST_BEACON);
}

bool IccSindragosaBlisteringColdTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sindragosa");
    if (!boss)
        return false;

    if (PlayerbotAI::IsTank(bot))
        return false;

    // Don't move if any bot in group has ice tomb
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    float dist = bot->GetExactDist2d(boss->GetPositionX(), boss->GetPositionY());

    if (dist >= 33.0f)
        return false;

    bool isCasting = false;
    if (boss && boss->HasUnitState(UNIT_STATE_CASTING))
        isCasting = true;

    bool isBlisteringCold = IccBossCastingBlisteringCold(boss);

    return isCasting && isBlisteringCold;
}

bool IccSindragosaUnchainedMagicTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sindragosa");
    if (!boss)
        return false;

    Aura* aura = botAI->GetAura("unchained magic", bot, false, false);
    if (!aura)
        return false;

    bool isBlisteringCold = IccBossCastingBlisteringCold(boss);

    if (boss && boss->HasUnitState(UNIT_STATE_CASTING) && isBlisteringCold)
        return false;

    return true;
}

bool IccSindragosaChilledToTheBoneTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sindragosa");
    if (!boss)
        return false;

    Aura* aura = botAI->GetAura("Chilled to the Bone", bot, false, false);
    if (!aura)
        return false;

    bool isBlisteringCold = IccBossCastingBlisteringCold(boss);

    if (boss && boss->HasUnitState(UNIT_STATE_CASTING) && isBlisteringCold)
        return false;

    return true;
}

bool IccSindragosaMysticBuffetTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sindragosa");
    if (!boss)
        return false;

    if (PlayerbotAI::IsTank(bot))
        return false;

    if (boss->GetVictim() == bot)
        return false;

    // Last phase only. Do not gate on the bot's own Mystic Buffet aura:
    // hiding sheds it, so aura-less bots must still run the action or nobody
    // is left to mark and kill the tomb once the raid has shed its stacks.
    if (!boss->HealthBelowPct(35))
        return false;

    if (bot->HasAura(SPELL_FROST_BEACON))
        return false;

    // Blistering Cold takes priority over tomb-hiding in the last phase:
    // skip hiding so the bot can run to the safe spot instead.
    if (boss->HasUnitState(UNIT_STATE_CASTING) && IccBossCastingBlisteringCold(boss))
        return false;

    return true;
}

bool IccSindragosaFrostBombTrigger::IsActive()
{
    Unit* boss = bot->FindNearestCreature(NPC_SINDRAGOSA, 200.0f);
    if (!boss)
        return false;

    if (!bot->IsAlive())  // Skip if dead
        return false;

    // Tombed bots intentionally pass through: the action pins their group to
    // the current tomb's zone so when freed they don't migrate to the wrong
    // zone. The action returns false for tombed bots without moving them.
    if (boss->GetExactDist2d(ICC_SINDRAGOSA_FLYING_POSITION.GetPositionX(), ICC_SINDRAGOSA_FLYING_POSITION.GetPositionY()) < 50.0f && !boss->HealthBelowPct(25) && !boss->HealthAbovePct(99))
        return true;

    return false;
}

//LK

bool IccLichKingShadowTrapTrigger::IsActive()
{
    Unit* vdw = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f);
    if (vdw)
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "the lich king");
    if (!boss)
        return false;

    bool hasPlague = botAI->HasAura("Necrotic Plague", bot);
    if (hasPlague)
        return false;

    if (!PlayerbotAI::IsMainTank(bot))
        return false;

    if (boss->HealthBelowPct(65))
        return false;

    // search for all nearby traps
    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    std::vector<Unit*> nearbyTraps;

    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() == NPC_SHADOW_TRAP && bot->GetExactDist2d(unit) < 11.0f)  // shadow trap
            return true;
    }

    return false;
}

bool IccLichKingNecroticPlagueTrigger::IsActive()
{
    Unit* vdw = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f);
    if (vdw)
        return false;

    if (!AI_VALUE2(Unit*, "find target", "the lich king"))
        return false;

    return botAI->HasAura("Necrotic Plague", bot);
}

bool IccLichKingWinterTrigger::IsActive()
{
    Unit* vdw = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f);
    if (vdw)
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "the lich king");
    if (!boss)
        return false;

    return IccBossHasRemorselessWinter(boss);
}

bool IccLichKingAddsTrigger::IsActive()
{
    Unit* vdw = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f);
    if (vdw)
        return false;

    if (bot->HasAura(SPELL_HARVEST_SOUL_VALKYR))
        return false;

    if (botAI->HasAura("Necrotic Plague", bot))
        return false;

    if (bot->FindNearestCreature(NPC_TERENAS_MENETHIL_HC, 55.0f) ||
        bot->FindNearestCreature(NPC_TERENAS_MENETHIL, 55.0f))
        return true;

    if (!bot->FindNearestCreature(NPC_THE_LICH_KING, 100.0f))
        return false;

    return true;
}

bool IccLichKingSpiritBombTrigger::IsActive()
{
    return IccLichKingSpiritBombAction::IsBombThreatActive(botAI, bot);
}
