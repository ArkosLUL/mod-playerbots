/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCDATA_H
#define PLAYERBOTS_ICCDATA_H

#include "Common.h"
#include <cstddef>

constexpr uint32 ICC_MAP_ID = 631;

enum CreatureIdsICC
{

    NPC_SPIKE1                          = 36619,
    NPC_SPIKE1_10H                      = 38233,
    NPC_SPIKE1_25N                      = 38459,
    NPC_SPIKE1_25H                      = 38460,
    NPC_SPIKE2                          = 38711,
    NPC_SPIKE2_10H                      = 38970,
    NPC_SPIKE2_25N                      = 38971,
    NPC_SPIKE2_25H                      = 38972,
    NPC_SPIKE3                          = 38712,
    NPC_SPIKE3_10H                      = 38973,
    NPC_SPIKE3_25N                      = 38974,
    NPC_SPIKE3_25H                      = 38975,
    NPC_COLDFLAME                       = 36672,

    NPC_ROTTING_FROST_GIANT_10          = 38490,
    NPC_ROTTING_FROST_GIANT_25          = 38494,

    NPC_STINKY                          = 37025,
    NPC_PRECIOUS                        = 37217,

    NPC_DARKFALLEN_ADVISOR              = 37571,
    NPC_DARKFALLEN_ARCHMAGE             = 37664,
    NPC_DARKFALLEN_BLOOD_KNIGHT         = 37595,
    NPC_DARKFALLEN_NOBLE                = 37663,

    NPC_DARNAVAN_10                     = 38472,
    NPC_DARNAVAN_25                     = 38485,
    NPC_SHADE                           = 38222,

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

    NPC_BLOOD_BEAST1                    = 38508,
    NPC_BLOOD_BEAST2                    = 38596,
    NPC_BLOOD_BEAST3                    = 38597,
    NPC_BLOOD_BEAST4                    = 38598,

    NPC_PUDDLE                          = 37013,
    NPC_SMALL_OOZE                      = 36897,
    NPC_BIG_OOZE                        = 36899,

    NPC_PROFESSOR_PUTRICIDE             = 36678,
    NPC_MALLEABLE_OOZE_STALKER          = 38556,
    NPC_GROWING_OOZE_PUDDLE             = 37690,
    NPC_CHOKING_GAS_BOMB                = 38159,

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

    NPC_SWARMING_SHADOWS                = 38163,

    NPC_SPEAR                           = 38248,
    ITEM_SPEAR                          = 50307,

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

    NPC_SINDRAGOSA                      = 36853,
    NPC_TOMB1                           = 36980,
    NPC_TOMB2                           = 38320,
    NPC_TOMB3                           = 38321,
    NPC_TOMB4                           = 38322,

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
    SPELL_EMPOWERED_BLOOD               = 70227,
    SPELL_EXPERIENCED                   = 71188,
    SPELL_NO_THREAT                     = 70115,
    SPELL_SPITEFULL_FURY                = 36886,
    SPELL_MAGIC_BARRIER                 = 38112,
    SPELL_FROST_TRAP1                   = 13809,
    SPELL_PAIN_SUPPRESION               = 69910,
    SPELL_AGEIS_OF_DALARAN              = 71638,
    SPELL_CYCLONE                       = 33786,
    SPELL_HAMMER_OF_JUSTICE             = 10308,
    SPELL_TAUNT_WARRIOR                 = 355,
    SPELL_TAUNT_PALADIN                 = 62124,
    SPELL_TAUNT_DK                      = 56222,
    SPELL_TAUNT_DRUID                   = 6795,
    SPELL_VIPER_STING                   = 3034,

    SPELL_LM_IMPALED                    = 69065,
    SPELL_LM_BONE_STORM                 = 69076,

    SPELL_DARK_RECKONING                = 69483,
    SPELL_TOUCH_OF_INSIGNIFICANCE       = 71204,
    SPELL_LDW_MANA_BARRIER              = 70842,

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

    SPELL_GAS_SPORE                     = 69279,
    SPELL_SLIME_SPRAY                   = 69508,
    SPELL_OOZE_FLOOD                    = 71215,
    SPELL_UNSTABLE_OOZE_EXPLOSION       = 69839,
    SPELL_OOZE_FLOOD_VISUAL             = 69785,
    SPELL_VILE_GAS_H                    = 69240,

    SPELL_GROW_AURA                     = 70347,

    SPELL_INVOCATION_OF_BLOOD_VALANAR   = 70952,
    SPELL_INVOCATION_OF_BLOOD_KELESETH  = 70981,
    SPELL_INVOCATION_OF_BLOOD_TALDARAM  = 70982,
    SPELL_EMPOWERED_SHOCK_VORTEX1       = 72039,
    SPELL_EMPOWERED_SHOCK_VORTEX2       = 73037,
    SPELL_EMPOWERED_SHOCK_VORTEX3       = 73038,
    SPELL_EMPOWERED_SHOCK_VORTEX4       = 73039,

    SPELL_PACT_OF_THE_DARKFALLEN        = 71340,
    SPELL_BLOODBOLT_WHIRL               = 71772,

    SPELL_AETHER_SHIELD                 = 71463,

    SPELL_DREAM_STATE                   = 70766,
    SPELL_EMERALD_VIGOR                 = 70873,
    SPELL_ACID_BURST                    = 70744,

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

    SPELL_HARVEST_SOUL_VALKYR           = 68985,
    SPELL_QUAKE                         = 72262,
    SPELL_VALKYR_CARRY                  = 30440,
};

inline constexpr uint32 DEFILE_AURAS[]         = {72756, 74162, 74163, 74164};
inline constexpr uint32 DEFILE_CAST_ID         = 72762;
inline constexpr uint32 DEFILE_NPC_ID          = 38757;
inline constexpr size_t DEFILE_AURA_COUNT      = 4;

inline constexpr uint32 SPELL_MALLEABLE_GOO_10N     = 70852;
inline constexpr uint32 SPELL_MALLEABLE_GOO_BALCONY = 72296;

#endif
