/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_HUNTERTRIGGERS_H
#define PLAYERBOTS_HUNTERTRIGGERS_H

#include "CureTriggers.h"
#include "GenericTriggers.h"
#include "PlayerbotAI.h"
#include "Trigger.h"
#include <set>

class PlayerbotAI;

// Hunters skip mana potions and carry their in-combat mana on Aspect of the Viper, so it has to take
// over well before the hunter is dry. Leaving at 60% keeps the band wide enough that a hunter does not
// flip aspects every few seconds. The two halves live in different triggers - entry in
// HunterAspectOfTheViperTrigger, exit in HunterAspectOfTheDragonhawkTrigger - so keep them together here.
constexpr uint8 HUNTER_VIPER_ENTER_MANA_PCT = 30;
constexpr uint8 HUNTER_VIPER_LEAVE_MANA_PCT = 60;

// Buff and Out of Combat Triggers

class HunterAspectOfTheDragonhawkTrigger : public BuffTrigger
{
public:
    HunterAspectOfTheDragonhawkTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "aspect of the dragonhawk") {}
    bool IsActive() override;
};

class HunterAspectOfTheWildTrigger : public BuffTrigger
{
public:
    HunterAspectOfTheWildTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "aspect of the wild") {}
};

class HunterAspectOfTheViperTrigger : public BuffTrigger
{
public:
    HunterAspectOfTheViperTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "aspect of the viper") {}
    bool IsActive() override;
};

class HunterAspectOfThePackTrigger : public BuffTrigger
{
public:
    HunterAspectOfThePackTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "aspect of the pack") {}
    bool IsActive() override;
};

class TrueshotAuraTrigger : public BuffTrigger
{
public:
    TrueshotAuraTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "trueshot aura") {}
};

class NoTrackTrigger : public BuffTrigger
{
public:
    NoTrackTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "no track") {}
    bool IsActive() override;
};

class HunterLowAmmoTrigger : public AmmoCountTrigger
{
public:
    HunterLowAmmoTrigger(PlayerbotAI* botAI) : AmmoCountTrigger(botAI, "ammo", 1, 30) {}

    bool IsActive() override;
};

class HunterNoAmmoTrigger : public AmmoCountTrigger
{
public:
    HunterNoAmmoTrigger(PlayerbotAI* botAI) : AmmoCountTrigger(botAI, "ammo", 1, 10) {}
};

class HunterHasAmmoTrigger : public AmmoCountTrigger
{
public:
    HunterHasAmmoTrigger(PlayerbotAI* botAI) : AmmoCountTrigger(botAI, "ammo", 1, 10) {}

    bool IsActive() override;
};

// Cooldown Triggers

// The burst cooldowns below only say "off cooldown" - BurstWindowStrategy decides when they are
// actually spent, so a raid-winning fight no longer keeps them unused for the whole encounter.

class RapidFireTrigger : public SpellNoCooldownTrigger
{
public:
    RapidFireTrigger(PlayerbotAI* botAI) : SpellNoCooldownTrigger(botAI, "rapid fire") {}
};

class BestialWrathTrigger : public SpellNoCooldownTrigger
{
public:
    BestialWrathTrigger(PlayerbotAI* botAI) : SpellNoCooldownTrigger(botAI, "bestial wrath") {}
};

class IntimidationTrigger : public BuffTrigger
{
public:
    IntimidationTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "intimidation") {}
};

// Kill Command puts its aura on the hunter, not on the pet the action targets, so an aura check
// never worked here - the cooldown is what paces it.
class KillCommandTrigger : public SpellNoCooldownTrigger
{
public:
    KillCommandTrigger(PlayerbotAI* botAI) : SpellNoCooldownTrigger(botAI, "kill command") {}
};

class ChimeraShotNoCdTrigger : public SpellNoCooldownTrigger
{
public:
    ChimeraShotNoCdTrigger(PlayerbotAI* botAI) : SpellNoCooldownTrigger(botAI, "chimera shot") {}
};

class AimedShotNoCdTrigger : public SpellNoCooldownTrigger
{
public:
    AimedShotNoCdTrigger(PlayerbotAI* botAI) : SpellNoCooldownTrigger(botAI, "aimed shot") {}
};

class LockAndLoadTrigger : public BuffTrigger
{
public:
    LockAndLoadTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "lock and load") {}

    bool IsActive() override
    {
        return botAI->HasAura("lock and load", botAI->GetBot());
    }
};

// CC Triggers

class FreezingTrapTrigger : public HasCcTargetTrigger
{
public:
    FreezingTrapTrigger(PlayerbotAI* botAI) : HasCcTargetTrigger(botAI, "freezing trap") {}
};

class ConcussiveShotOnSnareTargetTrigger : public SnareTargetTrigger
{
public:
    ConcussiveShotOnSnareTargetTrigger(PlayerbotAI* botAI) : SnareTargetTrigger(botAI, "concussive shot") {}
};

class ScareBeastTrigger : public HasCcTargetTrigger
{
public:
    ScareBeastTrigger(PlayerbotAI* botAI) : HasCcTargetTrigger(botAI, "scare beast") {}
};

class SilencingShotTrigger : public InterruptSpellTrigger
{
public:
    SilencingShotTrigger(PlayerbotAI* botAI) : InterruptSpellTrigger(botAI, "silencing shot") {}
};

// DoT/Debuff Triggers

class HuntersMarkTrigger : public DebuffTrigger
{
public:
    HuntersMarkTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "hunter's mark", 1, true, 0.5f) {}
    bool IsActive() override { return BuffTrigger::IsActive(); }
};

class ExplosiveShotTrigger : public DebuffTrigger
{
public:
    ExplosiveShotTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "explosive shot", 1, true) {}
    bool IsActive() override { return BuffTrigger::IsActive(); }
};

class BlackArrowTrigger : public DebuffTrigger
{
public:
    BlackArrowTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "black arrow", 1, true) {}
    bool IsActive() override;
};

class HunterNoStingsActiveTrigger : public DebuffTrigger
{
public:
    HunterNoStingsActiveTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "no stings") {}
    bool IsActive() override;
};

class SerpentStingOnAttackerTrigger : public DebuffOnAttackerTrigger
{
public:
    SerpentStingOnAttackerTrigger(PlayerbotAI* botAI) : DebuffOnAttackerTrigger(botAI, "serpent sting", true) {}
    bool IsActive() override;
};

// Damage/Combat Triggers

class AutoShotTrigger : public Trigger
{
public:
    AutoShotTrigger(PlayerbotAI* botAI) : Trigger(botAI, "auto shot") {}
};

class SwitchToRangedTrigger : public Trigger
{
public:
    SwitchToRangedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "switch to ranged") {}

    bool IsActive() override;
};

class SwitchToMeleeTrigger : public Trigger
{
public:
    SwitchToMeleeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "switch to melee") {}

    bool IsActive() override;
};

class MisdirectionOnMainTankTrigger : public BuffOnMainTankTrigger
{
public:
    MisdirectionOnMainTankTrigger(PlayerbotAI* botAI) : BuffOnMainTankTrigger(botAI, "misdirection", true) {}
};

class TargetRemoveEnrageTrigger : public TargetAuraDispelTrigger
{
public:
    TargetRemoveEnrageTrigger(PlayerbotAI* botAI) : TargetAuraDispelTrigger(botAI, "tranquilizing shot", DISPEL_ENRAGE) {}
};

class TargetRemoveMagicTrigger : public TargetAuraDispelTrigger
{
public:
    TargetRemoveMagicTrigger(PlayerbotAI* botAI) : TargetAuraDispelTrigger(botAI, "tranquilizing shot", DISPEL_MAGIC) {}
};

class ImmolationTrapNoCdTrigger : public SpellNoCooldownTrigger
{
public:
    ImmolationTrapNoCdTrigger(PlayerbotAI* botAI) : SpellNoCooldownTrigger(botAI, "immolation trap") {}
};

class TrapLauncherExplosiveNoCdTrigger : public SpellNoCooldownTrigger
{
public:
    TrapLauncherExplosiveNoCdTrigger(PlayerbotAI* botAI)
        : SpellNoCooldownTrigger(botAI, "trap launcher: explosive trap") {}
};

BEGIN_TRIGGER(HuntersPetDeadTrigger, Trigger)
END_TRIGGER()

BEGIN_TRIGGER(HuntersPetLowHealthTrigger, Trigger)
END_TRIGGER()

BEGIN_TRIGGER(HuntersPetMediumHealthTrigger, Trigger)
END_TRIGGER()

BEGIN_TRIGGER(HunterPetNotHappy, Trigger)
END_TRIGGER()

// A worm's Acid Spit is only worth its full 20% armor at 2/2, but pet autocast refuses to recast a
// spell whose aura is already on the target, so it stalls at 1/2. The bot casts it on cooldown
// instead, which stacks it and keeps the debuff from ever timing out.
class PetAcidSpitTrigger : public Trigger
{
public:
    PetAcidSpitTrigger(PlayerbotAI* botAI) : Trigger(botAI, "pet acid spit", 1) {}
    bool IsActive() override;
};

class VolleyChannelCheckTrigger : public Trigger
{
public:
    VolleyChannelCheckTrigger(PlayerbotAI* botAI, uint32 minEnemies = 2)
        : Trigger(botAI, "volley channel check"), minEnemies(minEnemies)
    {
    }

    bool IsActive() override;

protected:
    uint32 minEnemies;
    static const std::set<uint32> VOLLEY_SPELL_IDS;
};

#endif
