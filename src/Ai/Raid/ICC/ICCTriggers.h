/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_H
#define PLAYERBOTS_ICCTRIGGERS_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

enum CreatureIdsICC
{

    // Lord Marrowgar
    NPC_SPIKE1                          = 36619,  // 10N base
    NPC_SPIKE1_10H                      = 38233,  // 10H
    NPC_SPIKE1_25N                      = 38459,  // 25N
    NPC_SPIKE1_25H                      = 38460,  // 25H
    NPC_SPIKE2                          = 38711,  // 25N base
    NPC_SPIKE2_10H                      = 38970,
    NPC_SPIKE2_25N                      = 38971,
    NPC_SPIKE2_25H                      = 38972,
    NPC_SPIKE3                          = 38712,  // 25N base
    NPC_SPIKE3_10H                      = 38973,
    NPC_SPIKE3_25N                      = 38974,
    NPC_SPIKE3_25H                      = 38975,
    NPC_COLDFLAME                       = 36672,

    // Lady Deathwhisper
    NPC_DARNAVAN_10                     = 38472,
    NPC_DARNAVAN_25                     = 38485,
    NPC_SHADE                           = 38222,

    // Gunship Battle
    NPC_KOR_KRON_BATTLE_MAGE            = 37117,
    NPC_KOR_KRON_AXETHROWER             = 36968,
    NPC_KOR_KRON_ROCKETEER              = 36982,
    NPC_SKYBREAKER_SORCERER             = 37116,
    NPC_SKYBREAKER_RIFLEMAN             = 36969,
    NPC_SKYBREAKER_MORTAR_SOLDIER       = 36978,
    NPC_IGB_HIGH_OVERLORD_SAURFANG      = 36939,
    NPC_IGB_MURADIN_BRONZEBEARD         = 36948,
    NPC_CANNONA                         = 36838,
    NPC_CANNONH                         = 36839,
    NPC_MURADIN_BRONZEBEARD             = 36948,
    NPC_HIGH_OVERLORD_SAURFANG          = 36939,
    NPC_ZAFOD_BOOMBOX                   = 37184,
    ITEM_GOBLIN_ROCKET_PACK             = 49278,

    // Deathbringer Saurfang
    NPC_BLOOD_BEAST1                    = 38508,
    NPC_BLOOD_BEAST2                    = 38596,
    NPC_BLOOD_BEAST3                    = 38597,
    NPC_BLOOD_BEAST4                    = 38598,

    // Rotface
    NPC_PUDDLE                          = 37013,
    NPC_SMALL_OOZE                      = 36897,
    NPC_BIG_OOZE                        = 36899,

    // Putricide
    NPC_PROFESSOR_PUTRICIDE             = 36678,
    NPC_MALLEABLE_OOZE_STALKER          = 38556,
    NPC_GROWING_OOZE_PUDDLE             = 37690,
    NPC_CHOKING_GAS_BOMB                = 38159,

    // Blood Prince Council
    NPC_DARK_NUCLEUS                    = 38369,
    NPC_PRINCE_KELESETH                 = 37972,
    NPC_PRINCE_TALDARAM                 = 37973,
    NPC_PRINCE_VALANAR                  = 37970,
    NPC_KINETIC_BOMB1                   = 38454,
    NPC_KINETIC_BOMB2                   = 38775,
    NPC_KINETIC_BOMB3                   = 38776,
    NPC_KINETIC_BOMB4                   = 38777,
    NPC_BALL_OF_FLAME                   = 38332,
    NPC_BALL_OF_INFERNO_FLAME           = 38451,
    NPC_SHOCK_VORTEX                    = 38422,

    // Blood Queen Lana'thel
    NPC_SWARMING_SHADOWS                = 38163,

    // Sister Svalna
    NPC_SPEAR                           = 38248,
    ITEM_SPEAR                          = 50307,

    // Valithria Dreamwalker
    NPC_VALITHRIA_DREAMWALKER           = 36789,
    NPC_DREAM_PORTAL                    = 37945,
    NPC_NIGHTMARE_PORTAL                = 38430,
    NPC_DREAM_CLOUD                     = 37985,
    NPC_NIGHTMARE_CLOUD                 = 38421,
    NPC_RISEN_ARCHMAGE                  = 37868,
    NPC_BLAZING_SKELETON                = 36791,
    NPC_SUPPRESSER                      = 37863,
    NPC_BLISTERING_ZOMBIE               = 37934,
    NPC_GLUTTONOUS_ABOMINATION          = 37886,
    NPC_ROT_WORM                        = 37907,
    NPC_COLUMN_OF_FROST                 = 37918,
    NPC_MANA_VOID                       = 38068,
    NPC_DREAM_PORTAL_PRE_EFFECT         = 38186,
    NPC_NIGHTMARE_PORTAL_PRE_EFFECT     = 38429,

    // Sindragosa
    NPC_SINDRAGOSA                      = 36853,
    NPC_TOMB1                           = 36980,
    NPC_TOMB2                           = 38320,
    NPC_TOMB3                           = 38321,
    NPC_TOMB4                           = 38322,

    // Lich King
    NPC_THE_LICH_KING                   = 36597,
    NPC_TERENAS_MENETHIL                = 36823,
    NPC_TERENAS_MENETHIL_HC             = 39217,
    NPC_SPIRIT_BOMB                     = 39189,
    NPC_WICKED_SPIRIT1                  = 39190,
    NPC_WICKED_SPIRIT2                  = 39287,
    NPC_WICKED_SPIRIT3                  = 39288,
    NPC_WICKED_SPIRIT4                  = 39289,
    NPC_SHADOW_TRAP                     = 39137,
    NPC_SHAMBLING_HORROR1               = 37698,
    NPC_SHAMBLING_HORROR2               = 39299,
    NPC_SHAMBLING_HORROR3               = 39300,
    NPC_SHAMBLING_HORROR4               = 39301,
    NPC_ICE_SPHERE1                     = 36633,
    NPC_ICE_SPHERE2                     = 39305,
    NPC_ICE_SPHERE3                     = 39306,
    NPC_ICE_SPHERE4                     = 39307,
    NPC_RAGING_SPIRIT1                  = 36701,
    NPC_RAGING_SPIRIT2                  = 39302,
    NPC_RAGING_SPIRIT3                  = 39303,
    NPC_RAGING_SPIRIT4                  = 39304,
    NPC_DRUDGE_GHOUL1                   = 37695,
    NPC_DRUDGE_GHOUL2                   = 39309,
    NPC_DRUDGE_GHOUL3                   = 39310,
    NPC_DRUDGE_GHOUL4                   = 39311,
    NPC_VALKYR_SHADOWGUARD1             = 36609,
    NPC_VALKYR_SHADOWGUARD2             = 39120,
    NPC_VALKYR_SHADOWGUARD3             = 39121,
    NPC_VALKYR_SHADOWGUARD4             = 39122,
    NPC_VILE_SPIRIT1                    = 37799,
    NPC_VILE_SPIRIT2                    = 39284,
    NPC_VILE_SPIRIT3                    = 39285,
    NPC_VILE_SPIRIT4                    = 39286,

};

enum SpellIdsICC
{
    // ICC cheat spells
    SPELL_EMPOWERED_BLOOD               = 70227, //70304 -->50%, 70227 /*100% more dmg, 100% more att speed*/
    SPELL_EXPERIENCED                   = 71188, //dmg 30% 20% att speed
    SPELL_NO_THREAT                     = 70115, //reduce threat
    SPELL_SPITEFULL_FURY                = 36886, //500% more threat
    SPELL_NITRO_BOOSTS                  = 54861, //Speed
    SPELL_MAGIC_BARRIER                 = 38112, //damage reduction shield
    SPELL_FROST_TRAP1                   = 13809, //Hunter slow trap
    SPELL_PAIN_SUPPRESION               = 69910, //40% dmg reduction
    SPELL_AGEIS_OF_DALARAN              = 71638, //268 all ress
    SPELL_CYCLONE                       = 33786,
    SPELL_HAMMER_OF_JUSTICE             = 10308, //stun
    // Class taunts, each a single rank
    SPELL_TAUNT_WARRIOR                 = 355,
    SPELL_TAUNT_PALADIN                 = 62124,  // Hand of Reckoning
    SPELL_TAUNT_DK                      = 56222,  // Dark Command
    SPELL_TAUNT_DRUID                   = 6795,   // Growl
    SPELL_VIPER_STING                   = 3034,

    // Lord Marrowgar
    SPELL_LM_IMPALED                    = 69065,
    SPELL_LM_BONE_STORM                 = 69076,

    // Lady Deathwhisper
    SPELL_DARK_RECKONING                = 69483,
    SPELL_TOUCH_OF_INSIGNIFICANCE       = 71204,
    SPELL_LDW_MANA_BARRIER              = 70842,

    // Gunship Battle
    SPELL_DEATH_PLAGUE                  = 72865,
    SPELL_FROZEN_CANNON                 = 69704,
    SPELL_BELOW_ZERO                    = 69705,
    SPELL_ROCKET_PACK_USE               = 68645,
    SPELL_ROCKET_PACK_USEABLE           = 70348,
    SPELL_BATTLE_FURY1                  = 69637,
    SPELL_BATTLE_FURY2                  = 69638,
    SPELL_BATTLE_FURY3                  = 72306,
    SPELL_BATTLE_FURY4                  = 72307,
    SPELL_BATTLE_FURY5                  = 72308,

    // Festergut
    SPELL_GAS_SPORE                     = 69279,
    // Rotface
    SPELL_SLIME_SPRAY                   = 69508,
    SPELL_OOZE_FLOOD                    = 71215,
    SPELL_UNSTABLE_OOZE_EXPLOSION       = 69839,
    SPELL_OOZE_FLOOD_VISUAL             = 69785,
    // Festergut's Vile Gas, which Putricide casts from the balcony on Rotface heroic. The client DBC
    // remaps it per difficulty (69240/71218/73019/73020), so a cast hook sees 73019 or 73020 there.
    SPELL_VILE_GAS_H                    = 69240,

    // Putricide
    SPELL_GROW_AURA                     = 70347,

    // Blood Prince Council
    SPELL_EMPOWERED_SHOCK_VORTEX1       = 72039,
    SPELL_EMPOWERED_SHOCK_VORTEX2       = 73037,
    SPELL_EMPOWERED_SHOCK_VORTEX3       = 73038,
    SPELL_EMPOWERED_SHOCK_VORTEX4       = 73039,

    // Blood Queen Lana'thel
    SPELL_PACT_OF_THE_DARKFALLEN        = 71340,
    SPELL_BLOODBOLT_WHIRL               = 71772,

    // Sister Svalna
    SPELL_AETHER_SHIELD                 = 71463,

    // Valithria Dreamwalker
    SPELL_DREAM_STATE                   = 70766,
    SPELL_EMERALD_VIGOR                 = 70873,
    SPELL_ACID_BURST                    = 70744,

    // Sindragosa
    SPELL_FROST_BEACON                  = 70126,
    SPELL_ICE_TOMB                      = 70157,
    SPELL_FROST_BOMB_VISUAL             = 70022,
    SPELL_BLISTERING_COLD1              = 70123,
    SPELL_BLISTERING_COLD2              = 71047,
    SPELL_BLISTERING_COLD3              = 71048,
    SPELL_BLISTERING_COLD4              = 71049,
    SPELL_HAND_OF_FREEDOM               = 1044,
    ITEM_RED_SMOKE_FLARE                = 23769,
    ITEM_BLUE_SMOKE_FLARE               = 23770,

    // The Lich King
    SPELL_HARVEST_SOUL_VALKYR           = 68985,
    SPELL_QUAKE                         = 72262,
    SPELL_REMORSELESS_WINTER1           = 72259,
    SPELL_REMORSELESS_WINTER2           = 74273,
    SPELL_REMORSELESS_WINTER3           = 74274,
    SPELL_REMORSELESS_WINTER4           = 74275,
    SPELL_REMORSELESS_WINTER5           = 68981,
    SPELL_REMORSELESS_WINTER6           = 74270,
    SPELL_REMORSELESS_WINTER7           = 74271,
    SPELL_REMORSELESS_WINTER8           = 74272,
    SPELL_VALKYR_CARRY                  = 30440,
    SPELL_HARVEST_SOUL_LK               = 68980,
    SPELL_HARVEST_SOULS_LK_25           = 73654,
    SPELL_HARVEST_SOULS_LK_H1           = 74295,
    SPELL_HARVEST_SOULS_LK_H2           = 74296,
    SPELL_HARVEST_SOULS_LK_H3           = 74297,
};

inline constexpr uint32 DEFILE_AURAS[]         = {72756, 74162, 74163, 74164};
inline constexpr uint32 DEFILE_CAST_ID         = 72762;
inline constexpr uint32 DEFILE_NPC_ID          = 38757;
inline constexpr size_t DEFILE_AURA_COUNT      = 4;

// Malleable Goo as Putricide casts it in his own fight and from the balcony on Festergut heroic. Neither
// id has a spelldifficulty row, so both hold on every difficulty despite the first one's name.
inline constexpr uint32 SPELL_MALLEABLE_GOO_10N     = 70852;
inline constexpr uint32 SPELL_MALLEABLE_GOO_BALCONY = 72296;

// All fanatics and adherents entry ids Lady Deathwhisper
static const std::array<uint32, 23> addEntriesLady    = {
                                                        37949, 38394, 38625, 38626, 38010, 38397, 39000, 39001,
                                                        38136, 38396, 38632, 38633, 37890, 38393, 38628, 38629,
                                                        38135, 38395, 38634, 38009, 38398, 38630, 38631};

const std::vector<uint32> spellEntriesFlood           = {
                                                        69782, 69783, 69796, 69797, 69798,
                                                        69799, 69801, 69802, 69795};

const std::vector<uint32> availableTargetsGS          = {
                                                        NPC_KOR_KRON_ROCKETEER,        NPC_KOR_KRON_AXETHROWER,       NPC_KOR_KRON_BATTLE_MAGE, NPC_IGB_HIGH_OVERLORD_SAURFANG,
                                                        NPC_SKYBREAKER_MORTAR_SOLDIER, NPC_SKYBREAKER_RIFLEMAN,       NPC_SKYBREAKER_SORCERER,  NPC_IGB_MURADIN_BRONZEBEARD};

//Lord Marrowgar
class IccLmTrigger : public Trigger
{
public:
    IccLmTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc lm") {}
    bool IsActive() override;
};

//Lady Deathwhisper
class IccDarkReckoningTrigger : public Trigger
{
public:
    IccDarkReckoningTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc dark reckoning") {}
    bool IsActive() override;
};

class IccLadyDeathwhisperTrigger : public Trigger
{
public:
    IccLadyDeathwhisperTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc lady deathwhisper") {}
    bool IsActive() override;
};

//Gunship Battle
class IccRottingFrostGiantTankPositionTrigger : public Trigger
{
public:
    IccRottingFrostGiantTankPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc rotting frost giant tank position") {}
    bool IsActive() override;
};

class IccInCannonTrigger : public Trigger
{
public:
    IccInCannonTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc in cannon") {}
    bool IsActive() override;
};

class IccGunshipCannonNearTrigger : public Trigger
{
public:
    IccGunshipCannonNearTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc gunship cannon near") {}
    bool IsActive() override;
};

class IccGunshipRocketJumpTrigger : public Trigger
{
public:
    IccGunshipRocketJumpTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc gunship rocket jump") {}
    bool IsActive() override;
};

class IccGunshipRocketPackSetupTrigger : public Trigger
{
public:
    IccGunshipRocketPackSetupTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc gunship rocket pack setup") {}
    bool IsActive() override;
};

//DBS
class IccDbsTrigger : public Trigger
{
public:
    IccDbsTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc dbs") {}
    bool IsActive() override;
};

class IccDogsTrigger : public Trigger
{
public:
    IccDogsTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc dogs") {}
    bool IsActive() override;
};

//FESTERGUT
// Festergut's spore cycle: true while any group member carries Gas Spore. Publishes fg.phase.
bool IccFestergutSporeCycle(Player* bot);

class IccFestergutGroupPositionTrigger : public Trigger
{
public:
    IccFestergutGroupPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc festergut group position") {}
    bool IsActive() override;
};

class IccFestergutSporeTrigger : public Trigger
{
public:
    IccFestergutSporeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc festergut spore") {}
    bool IsActive() override;
};

class IccFestergutAvoidMalleableGooTrigger : public Trigger
{
public:
    IccFestergutAvoidMalleableGooTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "icc festergut avoid malleable goo") {}
    bool IsActive() override;
};

//ROTFACE
class IccRotfaceTankPositionTrigger : public Trigger
{
public:
    IccRotfaceTankPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc rotface tank position") {}
    bool IsActive() override;
};

class IccRotfaceGroupPositionTrigger : public Trigger
{
public:
    IccRotfaceGroupPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc rotface group position") {}
    bool IsActive() override;
};

class IccRotfaceMoveAwayFromExplosionTrigger : public Trigger
{
public:
    IccRotfaceMoveAwayFromExplosionTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "icc rotface move away from explosion"), _castEndMs(0), _castEnded(false), _wasCasting(false)
    {
    }
    bool IsActive() override;

private:
    uint32 _castEndMs;
    bool _castEnded;
    bool _wasCasting;
};

class IccRotfaceAvoidVileGasTrigger : public Trigger
{
public:
    IccRotfaceAvoidVileGasTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "icc rotface avoid vile gas") {}
    bool IsActive() override;
};

//PP
// Professor Putricide's phase, 1 to 3, from the health his DamageTaken hook tests: phase 2 at 80% and
// below, phase 3 at 35% and below. Publishes pp.phase.
uint8 IccPutricidePhase(Player* bot, Unit* boss);
// Drops the bot's committed Gaseous Bloat flee direction, so the next bloat starts without one.
void IccPutricideForgetBloatFlee(Player* bot);

class IccPutricideVolatileOozeTrigger : public Trigger
{
public:
    IccPutricideVolatileOozeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc putricide volatile ooze") {}
    bool IsActive() override;
};

class IccPutricideGasCloudTrigger : public Trigger
{
public:
    IccPutricideGasCloudTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc putricide gas cloud") {}
    bool IsActive() override;
};

class IccPutricideGrowingOozePuddleTrigger : public Trigger
{
public:
    IccPutricideGrowingOozePuddleTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc putricide growing ooze puddle") {}
    bool IsActive() override;
};

class IccPutricideMutatedPlagueTrigger : public Trigger
{
public:
    IccPutricideMutatedPlagueTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc putricide mutated plague") {}
    bool IsActive() override;
};

class IccPutricideMalleableGooTrigger : public Trigger
{
public:
    IccPutricideMalleableGooTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc putricide malleable goo") {}
    bool IsActive() override;
};

class IccPutricideAbominationTrigger : public Trigger
{
public:
    IccPutricideAbominationTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc putricide abomination") {}
    bool IsActive() override;
};

//BPC
class IccBpcKelesethTankTrigger : public Trigger
{
public:
    IccBpcKelesethTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc bpc keleseth tank") {}
    bool IsActive() override;
};

class IccBpcMainTankTrigger : public Trigger
{
public:
    IccBpcMainTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc bpc main tank") {}
    bool IsActive() override;
};

class IccBpcEmpoweredVortexTrigger : public Trigger
{
public:
    IccBpcEmpoweredVortexTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc bpc empowered vortex") {}
    bool IsActive() override;
};

class IccBpcKineticBombTrigger : public Trigger
{
public:
    IccBpcKineticBombTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc bpc kinetic bomb") {}
    bool IsActive() override;
};

class IccBpcBallOfFlameTrigger : public Trigger
{
public:
    IccBpcBallOfFlameTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc bpc ball of flame") {}
    bool IsActive() override;
};

//Bql
class IccBqlGroupPositionTrigger : public Trigger
{
public:
    IccBqlGroupPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc bql group position") {}
    bool IsActive() override;
};

class IccBqlPactOfDarkfallenTrigger : public Trigger
{
public:
    IccBqlPactOfDarkfallenTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc bql pact of darkfallen") {}
    bool IsActive() override;
};

class IccBqlVampiricBiteTrigger : public Trigger
{
public:
    IccBqlVampiricBiteTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc bql vampiric bite") {}
    bool IsActive() override;
};

// Sister Svalna
// Aether Shield is up from each Impaling Spear until a Hurl Spear breaks it. Publishes ss.shield.
bool IccSvalnaShielded(Player* bot, Unit* svalna);

class IccValkyreSpearTrigger : public Trigger
{
public:
    IccValkyreSpearTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc valkyre spear") {}
    bool IsActive() override;
};

class IccSisterSvalnaTrigger : public Trigger
{
public:
    IccSisterSvalnaTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc sister svalna") {}
    bool IsActive() override;
};

// Valithria Dreamwalker
// Every Dream or Nightmare Portal and pre-effect within 100 yd. Publishes vd.wave: open while a real portal
// stands, pre while a pre-effect still shows its 15 s visual, none otherwise.
std::vector<Creature*> IccValithriaPortals(Player* bot);

class IccValithriaGroupTrigger : public Trigger
{
public:
    IccValithriaGroupTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc valithria group") {}
    bool IsActive() override;
};

class IccValithriaZombieKiteTrigger : public Trigger
{
public:
    IccValithriaZombieKiteTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc valithria zombie kite") {}
    bool IsActive() override;
};

class IccValithriaPortalTrigger : public Trigger
{
public:
    IccValithriaPortalTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc valithria portal") {}
    bool IsActive() override;
};

class IccValithriaHealTrigger : public Trigger
{
public:
    IccValithriaHealTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc valithria heal") {}
    bool IsActive() override;
};

class IccValithriaDreamCloudTrigger : public Trigger
{
public:
    IccValithriaDreamCloudTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc valithria dream cloud") {}
    bool IsActive() override;
};

//SINDRAGOSA
enum class IccSindragosaPhaseId : uint8
{
    Ground,
    Air,
    Last,
};

// Sindragosa's phase as her script runs it: Air from takeoff to touchdown, while it holds her passive,
// else Last once her health is no longer above 35%. Publishes sg.phase.
IccSindragosaPhaseId IccSindragosaPhase(Player* bot, Unit const* boss);
// In the air and within `radius` of her far air station, where the Frost Bombs fall. 30 yd covers that
// station alone, 50 yd also the near one 49.6 yd off, where she casts the Frost Beacons.
bool IccSindragosaOverAirStation(Player* bot, Unit const* boss, float radius);

class IccSindragosaGroupPositionTrigger : public Trigger
{
public:
    IccSindragosaGroupPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc sindragosa group position") {}
    bool IsActive() override;
};

class IccSindragosaFrostBeaconTrigger : public Trigger
{
public:
    IccSindragosaFrostBeaconTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc sindragosa frost beacon") {}
    bool IsActive() override;
};

class IccSindragosaHotTrigger : public Trigger
{
public:
    IccSindragosaHotTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc sindragosa hot") {}
    bool IsActive() override;
};

class IccSindragosaBlisteringColdTrigger : public Trigger
{
public:
    IccSindragosaBlisteringColdTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc sindragosa blistering cold") {}
    bool IsActive() override;
};

class IccSindragosaUnchainedMagicTrigger : public Trigger
{
public:
    IccSindragosaUnchainedMagicTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc sindragosa unchained magic") {}
    bool IsActive() override;
};

class IccSindragosaChilledToTheBoneTrigger : public Trigger
{
public:
    IccSindragosaChilledToTheBoneTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc sindragosa chilled to the bone") {}
    bool IsActive() override;
};

class IccSindragosaMysticBuffetTrigger : public Trigger
{
public:
    IccSindragosaMysticBuffetTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc sindragosa mystic buffet") {}
    bool IsActive() override;
};

class IccSindragosaFrostBombTrigger : public Trigger
{
public:
    IccSindragosaFrostBombTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc sindragosa frost bomb") {}
    bool IsActive() override;
};

//LICH KING
class IccLichKingShadowTrapTrigger : public Trigger
{
public:
    IccLichKingShadowTrapTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc lich king shadow trap") {}
    bool IsActive() override;
};

class IccLichKingNecroticPlagueTrigger : public Trigger
{
public:
    IccLichKingNecroticPlagueTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc lich king necrotic plague") {}
    bool IsActive() override;
};

class IccLichKingWinterTrigger : public Trigger
{
public:
    IccLichKingWinterTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc lich king winter") {}
    bool IsActive() override;
};

class IccLichKingAddsTrigger : public Trigger
{
public:
    IccLichKingAddsTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc lich king adds") {}
    bool IsActive() override;
};

class IccLichKingSpiritBombTrigger : public Trigger
{
public:
    IccLichKingSpiritBombTrigger(PlayerbotAI* botAI) : Trigger(botAI, "icc lich king spirit bomb") {}
    bool IsActive() override;
};

#endif
