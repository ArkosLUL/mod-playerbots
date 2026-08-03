/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_MAGEACTIONS_H
#define PLAYERBOTS_MAGEACTIONS_H

#include "GenericSpellActions.h"
#include "SharedDefines.h"
#include "UseItemAction.h"

class PlayerbotAI;

// Buff and Out of Combat Actions

class CastMoltenArmorAction : public CastBuffSpellAction
{
public:
    CastMoltenArmorAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "molten armor") {}
    std::vector<NextAction> getAlternatives() override;
};

class CastMageArmorAction : public CastBuffSpellAction
{
public:
    CastMageArmorAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "mage armor") {}
    std::vector<NextAction> getAlternatives() override;
};

class CastIceArmorAction : public CastBuffSpellAction
{
public:
    CastIceArmorAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "ice armor") {}
};

class CastFrostArmorAction : public CastBuffSpellAction
{
public:
    CastFrostArmorAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "frost armor") {}
};

class CastArcaneIntellectAction : public GroupBuffSpellAction
{
public:
    CastArcaneIntellectAction(PlayerbotAI* botAI) : GroupBuffSpellAction(botAI, "arcane intellect") {}
};

class CastArcaneIntellectOnPartyAction : public GroupBuffOnPartyAction
{
public:
    CastArcaneIntellectOnPartyAction(PlayerbotAI* botAI) : GroupBuffOnPartyAction(botAI, "arcane intellect") {}
};

class CastFocusMagicOnPartyAction : public CastSpellAction
{
public:
    CastFocusMagicOnPartyAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "focus magic") {}
    Unit* GetTarget() override;
};

class CastSummonWaterElementalAction : public CastBuffSpellAction
{
public:
    CastSummonWaterElementalAction(PlayerbotAI* botAI)
        : CastBuffSpellAction(botAI, "summon water elemental") {}
};

// Boost Actions

class CastCombustionAction : public CastBuffSpellAction
{
public:
    CastCombustionAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "combustion") {}
};

class CastArcanePowerAction : public CastBuffSpellAction
{
public:
    CastArcanePowerAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "arcane power") {}
};

class CastPresenceOfMindAction : public CastBuffSpellAction
{
public:
    CastPresenceOfMindAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "presence of mind") {}
};

class CastIcyVeinsAction : public CastBuffSpellAction
{
public:
    CastIcyVeinsAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "icy veins") {}
};

class CastColdSnapAction : public CastBuffSpellAction
{
public:
    CastColdSnapAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "cold snap") {}
};

// Defensive Actions

class CastFireWardAction : public CastBuffSpellAction
{
public:
    CastFireWardAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "fire ward") {}
};

class CastFrostWardAction : public CastBuffSpellAction
{
public:
    CastFrostWardAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "frost ward") {}
};

class CastIceBarrierAction : public CastBuffSpellAction
{
public:
    CastIceBarrierAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "ice barrier") {}
};

class CastInvisibilityAction : public CastBuffSpellAction
{
public:
    CastInvisibilityAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "invisibility") {}
};
class CastIceBlockAction : public CastBuffSpellAction
{
public:
    CastIceBlockAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "ice block") {}
};

class CastMirrorImageAction : public CastBuffSpellAction
{
public:
    CastMirrorImageAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "mirror image") {}
};

class CastBlinkBackAction : public CastSpellAction
{
public:
    CastBlinkBackAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "blink") {}
    bool Execute(Event event) override;
};

class CastManaShieldAction : public CastBuffSpellAction
{
public:
    CastManaShieldAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "mana shield") {}
};

// Utility Actions

class CastEvocationAction : public CastSpellAction
{
public:
    CastEvocationAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "evocation") {}
    std::string const GetTargetName() override { return "self target"; }
};

class CastConjureManaGemAction : public CastBuffSpellAction
{
public:
    CastConjureManaGemAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "conjure mana gem") {}
};

class CastConjureFoodAction : public CastBuffSpellAction
{
public:
    CastConjureFoodAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "conjure food") {}
};

class CastConjureWaterAction : public CastBuffSpellAction
{
public:
    CastConjureWaterAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "conjure water") {}
};

// Each gem rank falls back to the one below it through the ActionNode alternative chain, and the
// engine only walks that chain when an action reports IMPOSSIBLE - a USELESS node is dropped outright
// (Engine::DoNextAction). So "the gem is not in the bags" has to be an isPossible() answer, never an
// isUseful() one, or a mage holding an older gem than the rank it can conjure uses nothing at all.
class UseManaGemAction : public UseItemAction
{
public:
    UseManaGemAction(PlayerbotAI* botAI, std::string const name, uint32 itemId)
        : UseItemAction(botAI, name), itemId(itemId)
    {
    }

    bool isUseful() override;
    bool isPossible() override;

private:
    uint32 itemId;
};

class UseManaSapphireAction : public UseManaGemAction
{
public:
    UseManaSapphireAction(PlayerbotAI* botAI) : UseManaGemAction(botAI, "mana sapphire", 33312) {}
};

class UseManaEmeraldAction : public UseManaGemAction
{
public:
    UseManaEmeraldAction(PlayerbotAI* botAI) : UseManaGemAction(botAI, "mana emerald", 22044) {}
};

class UseManaRubyAction : public UseManaGemAction
{
public:
    UseManaRubyAction(PlayerbotAI* botAI) : UseManaGemAction(botAI, "mana ruby", 8008) {}
};

class UseManaCitrineAction : public UseManaGemAction
{
public:
    UseManaCitrineAction(PlayerbotAI* botAI) : UseManaGemAction(botAI, "mana citrine", 8007) {}
};

class UseManaJadeAction : public UseManaGemAction
{
public:
    UseManaJadeAction(PlayerbotAI* botAI) : UseManaGemAction(botAI, "mana jade", 5513) {}
};

class UseManaAgateAction : public UseManaGemAction
{
public:
    UseManaAgateAction(PlayerbotAI* botAI) : UseManaGemAction(botAI, "mana agate", 5514) {}
};

// CC, Interrupt, and Dispel Actions

class CastPolymorphAction : public CastCrowdControlSpellAction
{
public:
    CastPolymorphAction(PlayerbotAI* botAI) : CastCrowdControlSpellAction(botAI, "polymorph") {}
};

class CastSpellstealAction : public CastSpellAction
{
public:
    CastSpellstealAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "spellsteal") {}
};

class CastCounterspellAction : public CastSpellAction
{
public:
    CastCounterspellAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "counterspell") {}
};

class CastCounterspellOnEnemyHealerAction : public CastSpellOnEnemyHealerAction
{
public:
    CastCounterspellOnEnemyHealerAction(PlayerbotAI* botAI)
        : CastSpellOnEnemyHealerAction(botAI, "counterspell") {}
};

class CastFrostNovaAction : public CastSpellAction
{
public:
    CastFrostNovaAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "frost nova") {}
    bool isUseful() override;
};

class CastDeepFreezeAction : public CastSpellAction
{
public:
    CastDeepFreezeAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "deep freeze") {}
    bool isPossible() override { return true; }
};

class CastRemoveCurseAction : public CastCureSpellAction
{
public:
    CastRemoveCurseAction(PlayerbotAI* botAI) : CastCureSpellAction(botAI, "remove curse") {}
};

class CastRemoveLesserCurseAction : public CastCureSpellAction
{
public:
    CastRemoveLesserCurseAction(PlayerbotAI* botAI) : CastCureSpellAction(botAI, "remove lesser curse") {}
};

class CastRemoveCurseOnPartyAction : public CurePartyMemberAction
{
public:
    CastRemoveCurseOnPartyAction(PlayerbotAI* botAI) : CurePartyMemberAction(botAI, "remove curse", DISPEL_CURSE) {}
};

class CastRemoveLesserCurseOnPartyAction : public CurePartyMemberAction
{
public:
    CastRemoveLesserCurseOnPartyAction(PlayerbotAI* botAI)
        : CurePartyMemberAction(botAI, "remove lesser curse", DISPEL_CURSE) {}
};

// Damage and Debuff Actions

class CastFireballAction : public CastSpellAction
{
public:
    CastFireballAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "fireball") {}
};

class CastScorchAction : public CastSpellAction
{
public:
    CastScorchAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "scorch") {}
};

class CastFireBlastAction : public CastSpellAction
{
public:
    CastFireBlastAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "fire blast") {}
};

// Arcane Blast puts its stacking buff on the caster, not the target, so there is no aura on the
// enemy to check - a plain CastSpellAction, not one of the aura-aware variants.
class CastArcaneBlastAction : public CastSpellAction
{
public:
    CastArcaneBlastAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "arcane blast") {}
};

class CastArcaneBarrageAction : public CastSpellAction
{
public:
    CastArcaneBarrageAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "arcane barrage") {}
};

class CastArcaneMissilesAction : public CastSpellAction
{
public:
    CastArcaneMissilesAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "arcane missiles") {}
};

class CastPyroblastAction : public CastSpellAction
{
public:
    CastPyroblastAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "pyroblast") {}
};

class CastLivingBombAction : public CastDebuffSpellAction
{
public:
    CastLivingBombAction(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "living bomb", true) {}
    bool isUseful() override
    {
        return CastAuraSpellAction::isUseful();
    }
};

class CastLivingBombOnAttackersAction : public CastDebuffSpellOnAttackerAction
{
public:
    CastLivingBombOnAttackersAction(PlayerbotAI* botAI) : CastDebuffSpellOnAttackerAction(botAI, "living bomb", true) {}
    bool isUseful() override
    {
        return CastAuraSpellAction::isUseful();
    }
};

class CastFrostboltAction : public CastSpellAction
{
public:
    CastFrostboltAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "frostbolt") {}
};

class CastFrostfireBoltAction : public CastSpellAction
{
public:
    CastFrostfireBoltAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "frostfire bolt") {}
};

class CastIceLanceAction : public CastSpellAction
{
public:
    CastIceLanceAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "ice lance") {}
};

class CastBlizzardAction : public CastSpellAction
{
public:
    CastBlizzardAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "blizzard") {}
    ActionThreatType getThreatType() override { return ActionThreatType::Aoe; }
};

class CastConeOfColdAction : public CastSpellAction
{
public:
    CastConeOfColdAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "cone of cold") {}
    ActionThreatType getThreatType() override { return ActionThreatType::Aoe; }
    bool isUseful() override;
};

class CastFlamestrikeAction : public CastDebuffSpellAction
{
public:
    CastFlamestrikeAction(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "flamestrike", true, 0.0f) {}
    ActionThreatType getThreatType() override { return ActionThreatType::Aoe; }
};

class CastDragonsBreathAction : public CastSpellAction
{
public:
    CastDragonsBreathAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "dragon's breath") {}
    ActionThreatType getThreatType() override { return ActionThreatType::Aoe; }
    bool isUseful() override;
};

class CastBlastWaveAction : public CastSpellAction
{
public:
    CastBlastWaveAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "blast wave") {}
    ActionThreatType getThreatType() override { return ActionThreatType::Aoe; }
    bool isUseful() override;
};

#endif
