/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxTriggers.h"
#include "NaxxSpellIds.h"
#include "Playerbots.h"
#include "Timer.h"
#include "Trigger.h"

bool MutatingInjectionMeleeTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "grobbulus");
    if (!boss)
    {
        return false;
    }
    return MutatingInjectionTrigger::IsActive() && !botAI->IsRanged(bot);
}

bool MutatingInjectionRangedTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "grobbulus");
    if (!boss)
    {
        return false;
    }
    return MutatingInjectionTrigger::IsActive() && botAI->IsRanged(bot);
}

bool AuraRemovedTrigger::IsActive()
{
    bool check = botAI->HasAura(name, bot, false, false, -1, true);
    bool ret = false;
    if (prev_check && !check)
    {
        ret = true;
    }
    prev_check = check;
    return ret;
}

bool MutatingInjectionRemovedTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "grobbulus");
    if (!boss)
    {
        return false;
    }
    return HasNoAuraTrigger::IsActive() && botAI->GetState() == BOT_STATE_COMBAT && botAI->IsRanged(bot);
}

bool GrobbulusCloudTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "grobbulus");
    if (!boss)
    {
        return false;
    }
    if (!botAI->IsMainTank(bot))
    {
        return false;
    }
    // bot->Yell("has aggro on " + boss->GetName() + " : " + to_string(AI_VALUE2(bool, "has aggro", "boss target")),
    // LANG_UNIVERSAL);
    if (!AI_VALUE2(bool, "has aggro", "boss target"))
    {
        return false;
    }
    uint32 now = getMSTime();
    bool poison_cloud_casting = false;
    if (boss->HasUnitState(UNIT_STATE_CASTING))
    {
        Spell* spell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell)
        {
            spell = boss->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        }
        if (spell)
        {
            poison_cloud_casting = NaxxSpellIds::MatchesAnySpellId(spell->GetSpellInfo(), {NaxxSpellIds::PoisonCloud});
        }
    }
    if (!poison_cloud_casting && last_cloud_ms != 0 && now - last_cloud_ms < CloudRotationDelayMs)
    {
        return false;
    }
    last_cloud_ms = now;
    return true;
}

bool HeiganMeleeTrigger::IsActive() { return helper.UpdateBossAI() && !botAI->IsRanged(bot); }

bool HeiganRangedTrigger::IsActive() { return helper.UpdateBossAI() && botAI->IsRanged(bot); }

bool HeiganDecrepitFeverTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }

    // Only relevant for dispellers; keep the check cheap and local.
    switch (bot->getClass())
    {
        case CLASS_PALADIN:
        case CLASS_PRIEST:
        case CLASS_SHAMAN:
            break;
        default:
            return false;
    }

    Group* group = bot->GetGroup();
    if (!group)
    {
        return false;
    }

    float range = botAI->GetRange("heal");
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || !member->IsAlive())
        {
            continue;
        }
        if (!NaxxSpellIds::HasAnyAura(member, {NaxxSpellIds::DecrepitFever10, NaxxSpellIds::DecrepitFever25}))
        {
            continue;
        }
        if (bot->IsWithinDistInMap(member, range))
        {
            return true;
        }
    }
    return false;
}

bool RazuviousTankTrigger::IsActive()
{
    Difficulty diff = bot->GetRaidDifficulty();
    if (diff == RAID_DIFFICULTY_10MAN_NORMAL)
    {
        return helper.UpdateBossAI() && botAI->IsTank(bot);
    }
    return helper.UpdateBossAI() && bot->getClass() == CLASS_PRIEST;
}

bool RazuviousNontankTrigger::IsActive()
{
    Difficulty diff = bot->GetRaidDifficulty();
    if (diff == RAID_DIFFICULTY_10MAN_NORMAL)
    {
        return helper.UpdateBossAI() && !(botAI->IsTank(bot));
    }
    return helper.UpdateBossAI() && !(bot->getClass() == CLASS_PRIEST);
}

// Cheap gate in front of every redirect trigger: only these two classes have anything to redirect.
static bool CanRedirectThreat(Player* bot)
{
    return bot->getClass() == CLASS_HUNTER || bot->getClass() == CLASS_ROGUE;
}

bool ThaddiusRedirectThreatTrigger::IsActive()
{
    if (!CanRedirectThreat(bot) || !bot->IsInCombat())
    {
        return false;
    }
    if (!helper.UpdateBossAI())
    {
        return false;
    }

    if (helper.IsPhasePet())
    {
        // Pull only - after that both pets stay parked on their tanks.
        Unit* pet = helper.GetAssignedPetForBot();
        return pet && pet->GetHealthPct() > NAXX_PULL_HEALTH_PCT;
    }

    // The transition and the first seconds after Thaddius stands up are one fresh pull: he
    // activates with an empty threat table.
    Unit* boss = helper.GetBoss();
    return boss && boss->GetHealthPct() > NAXX_PULL_HEALTH_PCT;
}

bool FourhorsemanRedirectThreatTrigger::IsActive()
{
    if (!CanRedirectThreat(bot))
    {
        return false;
    }
    if (!helper.IsEncounterUp())
    {
        return false;
    }
    // Attractors have a corner to reach and a rotation to keep; nothing they do belongs on a tank.
    if (helper.IsAttracter(bot))
    {
        return false;
    }
    // Pull only. Once the attractor rotation starts, the horsemen change hands and a redirect
    // lands on the wrong player.
    return helper.JustStartCombat();
}

bool HorsemanAttractorsTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    return helper.IsAttracter(bot);
}

bool HorsemanExceptAttractorsTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    return !helper.IsAttracter(bot);
}

bool SapphironGroundTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    return helper.IsPhaseGround();
}

bool SapphironFlightTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    return helper.IsPhaseFlight();
}

bool GluthTrigger::IsActive() { return helper.UpdateBossAI(); }

bool GluthLowHealthZombieAoeTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }

    Unit* target = AI_VALUE(Unit*, "current target");
    return target && helper.IsZombieChow(target) && target->GetHealthPct() <= helper.decimatedZombiePct;
}

bool GluthMainTankMortalWoundTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    if (!botAI->IsAssistTankOfIndex(bot, 0, true))
    {
        return false;
    }
    Unit* mt = AI_VALUE(Unit*, "main tank");
    if (!mt)
    {
        return false;
    }
    Aura* aura = NaxxSpellIds::GetAnyAura(mt, {NaxxSpellIds::MortalWound10, NaxxSpellIds::MortalWound25});
    if (!aura)
    {
        aura = botAI->GetAura("mortal wound", mt, false, true);
    }
    if (!aura || aura->GetStackAmount() < 5)
    {
        return false;
    }
    return true;
}

bool GluthFrenzyTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER)
    {
        return false;
    }
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    Unit* boss = AI_VALUE2(Unit*, "find target", "gluth");
    if (!boss || !boss->IsInCombat())
    {
        return false;
    }
    if (!NaxxSpellIds::HasAnyAura(boss, {NaxxSpellIds::GluthFrenzy10, NaxxSpellIds::GluthFrenzy25}) &&
        !botAI->GetAura("frenzy", boss))
    {
        return false;
    }
    return botAI->CanCastSpell("tranquilizing shot", boss);
}

bool GluthRedirectThreatTrigger::IsActive()
{
    if (!CanRedirectThreat(bot))
    {
        return false;
    }
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    // The pull, and every Decimate, which hands a fresh zombie wave to the off tank.
    return helper.JustStartCombat() || helper.InDecimateWindow();
}

bool KelthuzadTrigger::IsActive() { return helper.UpdateBossAI(); }

bool KelthuzadShadowFissureTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }

    Unit* fissure = helper.GetNearestShadowFissure();
    return fissure && bot->IsWithinDistInMap(fissure, KelthuzadBossHelper::FISSURE_DANGER_RADIUS);
}

bool AnubrekhanTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "anub'rekhan");
    if (!boss)
        return false;

    return bot->IsInCombat() || boss->IsInCombat();
}

bool AnubrekhanLocustSwarmTrigger::IsActive()
{
    return helper.UpdateBossAI() && helper.IsSwarmFormation();
}

bool FaerlinaTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "grand widow faerlina");
    if (!boss)
    {
        return false;
    }
    return true;
}

bool FaerlinaFrenzyTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "grand widow faerlina");
    if (!boss)
    {
        return false;
    }

    // Only relevant during the actual encounter.
    if (!bot->IsInCombat() && !boss->IsInCombat())
    {
        return false;
    }

    // Frenzy is handled either via a sacrifice (Widow's Embrace) or by removing the enrage.
    if (boss->HasAura(NaxxSpellIds::FaerlinaWidowsEmbrace))
    {
        return false;
    }

    return boss->HasAura(NaxxSpellIds::FaerlinaFrenzy);
}

bool MaexxnaTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "maexxna");
    if (!boss)
    {
        return false;
    }
    return !botAI->IsTank(bot);
}

bool MaexxnaWebWrapTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "maexxna");
    if (!boss)
    {
        return false;
    }

    // Only relevant during the actual encounter.
    if (!bot->IsInCombat() && !boss->IsInCombat())
    {
        return false;
    }

    // Prefer ranged DPS/casters to break cocoons with minimal movement.
    if (botAI->IsTank(bot) || botAI->IsHeal(bot) || !botAI->IsRanged(bot))
    {
        return false;
    }

    // If any member is web-wrapped, we want to break it.
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive())
                continue;

            if (member->HasAura(NaxxSpellIds::MaexxnaWebWrapStun))
                return true;
        }
    }

    // Or if the cocoon NPC exists in our target list.
    static constexpr uint32 MaexxnaWebWrapEntry = 16486;
    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->GetEntry() == NaxxSpellIds::MaexxnaWebWrapEntry)
            return true;
    }

    return false;
}

bool MaexxnaSpiderlingsTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "maexxna");
    if (!boss)
        return false;

    if (!bot->IsInCombat() && !boss->IsInCombat())
        return false;

    if (!botAI->IsTank(bot) || botAI->IsMainTank(bot))
        return false;

    GuidVector attackers = AI_VALUE(GuidVector, "attackers");
    for (ObjectGuid const& guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->GetEntry() == NaxxSpellIds::MaexxnaSpiderlingEntry)
            return true;
    }

    return false;
}

bool GothikTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }

    Unit* boss = helper.GetBoss();
    return bot->IsInCombat() || (boss && boss->IsInCombat());
}

bool GothikWrongSideTrigger::IsActive()
{
    return helper.UpdateBossAI() && !GothikBossHelper::IsLiveSide(bot);
}

// bool PatchwerkTankTrigger::IsActive()
// {
//     Unit* boss = AI_VALUE2(Unit*, "find target", "patchwerk");
//     if (!boss)
//     {
//         return false;
//     }
//     return !botAI->IsTank(bot) && !botAI->IsRanged(bot);
// }

// bool PatchwerkRangedTrigger::IsActive()
// {
//     Unit* boss = AI_VALUE2(Unit*, "find target", "patchwerk");
//     if (!boss)
//     {
//         return false;
//     }
//     return !botAI->IsTank(bot) && botAI->IsRanged(bot);
// }

// bool PatchwerkNonTankTrigger::IsActive()
// {
//     Unit* boss = AI_VALUE2(Unit*, "find target", "patchwerk");
//     if (!boss)
//     {
//         return false;
//     }
//     return !botAI->IsTank(bot);
// }

bool LoathebTrigger::IsActive() { return helper.UpdateBossAI(); }

bool NothTrigger::IsActive() { return helper.UpdateBossAI(); }

bool NothCurseTrigger::IsActive()
{
    if (!helper.UpdateBossAI() || !NaxxCanDispelCurse(botAI, bot))
    {
        return false;
    }
    return !helper.GetCursedMembers().empty();
}

bool NothBlinkTrigger::IsActive()
{
    if (!helper.UpdateBossAI() || !botAI->IsMainTank(bot))
    {
        return false;
    }
    if (!helper.IsBlinkWindow())
    {
        return false;
    }

    // Taunt targets "current target", so wait for the choose-target node to put the boss back there
    // rather than pulling an add off the assist tank.
    Unit* boss = helper.GetBoss();
    return boss && boss->GetVictim() != bot && AI_VALUE(Unit*, "current target") == boss;
}

bool ThaddiusPhasePetTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    return helper.IsPhasePet();
}

bool ThaddiusPhaseTransitionTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    return helper.IsPhaseTransition();
}

bool ThaddiusPhaseThaddiusTrigger::IsActive()
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    return helper.IsPhaseThaddius();
}
