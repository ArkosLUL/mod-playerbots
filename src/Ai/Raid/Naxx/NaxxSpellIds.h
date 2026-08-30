/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXSPELLIDS_H
#define PLAYERBOTS_NAXXSPELLIDS_H

#include "PlayerbotAI.h"
#include <initializer_list>

// use src/server/scripts/Northrend/Naxxramas/naxxramas.h for CreatureId, NaxxramasSay, NaxxramasEvent, NaxxramasMisc
namespace NaxxSpellIds
{
    // Grand Widow Faerlina
    static constexpr uint32 FaerlinaFrenzy = 28798;
    static constexpr uint32 FaerlinaWidowsEmbrace = 28732;

    // Maexxna
    static constexpr uint32 MaexxnaWebWrapStun = 28622;
    static constexpr uint32 MaexxnaWebWrapEntry = 16486;
    static constexpr uint32 MaexxnaSpiderlingEntry = 17055;

    // Gothik the Harvester
    static constexpr uint32 GothikEntry = 16060;
    // POS_Y_GATE in boss_gothik.cpp: everything south of it is the living side.
    static constexpr float GothikGateY = -3360.78f;
    // His balcony perch sits at Z 285.5, both arena floors at Z ~267.7.
    static constexpr float GothikBalconyZ = 280.0f;
    static constexpr uint32 GothikLivingTraineeEntry = 16124;
    static constexpr uint32 GothikLivingKnightEntry = 16125;
    static constexpr uint32 GothikLivingRiderEntry = 16126;
    static constexpr uint32 GothikDeadTraineeEntry = 16127;
    static constexpr uint32 GothikDeadKnightEntry = 16148;
    static constexpr uint32 GothikDeadHorseEntry = 16149;
    static constexpr uint32 GothikDeadRiderEntry = 16150;

    // Heigan
    // Cast by the floor GameObjects, never by the boss - see instance_naxxramas HeiganEruptSections.
    static constexpr uint32 Eruption = 29371;
    static constexpr uint32 DecrepitFever10 = 29998;
    static constexpr uint32 DecrepitFever25 = 55011;
    static constexpr uint32 SpellDisruption = 29310;
    static constexpr uint32 PlagueCloud = 29350;
    static constexpr uint32 TeleportSelf = 30211;

    // Grobbulus
    static constexpr uint32 PoisonCloud = 28240;

    // Noth the Plaguebringer
    // 25-man remaps these through spelldifficulty_dbc, so anything matching on a cast or an aura has
    // to fall back to the name or to the dispel type.
    static constexpr uint32 CurseOfThePlaguebringer = 29213;
    static constexpr uint32 UnrelentingPlague = 29214;
    static constexpr uint32 Cripple = 29212;
    static constexpr uint32 Blink = 29208;
    static constexpr uint32 NothPlaguedWarriorEntry = 16984;
    static constexpr uint32 NothPlaguedChampionEntry = 16983;
    static constexpr uint32 NothPlaguedGuardianEntry = 16981;

    // Thaddius polarity
    static constexpr uint32 PositiveCharge10 = 28059;
    static constexpr uint32 PositiveCharge25 = 28062;
    static constexpr uint32 PositiveChargeStack = 29659;
    static constexpr uint32 NegativeCharge10 = 28084;
    static constexpr uint32 NegativeCharge25 = 28085;
    static constexpr uint32 NegativeChargeStack = 29660;
/*
    SPELL_MAGNETIC_PULL                 = 28337,
    SPELL_TESLA_SHOCK                   = 28099,
    SPELL_SHOCK_VISUAL                  = 28159,

    // Stalagg
    SPELL_POWER_SURGE                   = 54529,
    SPELL_STALAGG_CHAIN                 = 28096,

    // Feugen
    SPELL_STATIC_FIELD                  = 28135,
    SPELL_FEUGEN_CHAIN                  = 28111,

    // Thaddius
    SPELL_POLARITY_SHIFT                = 28089,
    SPELL_BALL_LIGHTNING                = 28299,
    SPELL_CHAIN_LIGHTNING               = 28167,
    SPELL_BERSERK                       = 27680,
    SPELL_THADDIUS_VISUAL_LIGHTNING     = 28136,
    SPELL_THADDIUS_SPAWN_STUN           = 28160,

    SPELL_POSITIVE_CHARGE               = 28062,
    SPELL_POSITIVE_CHARGE_STACK         = 29659,
    SPELL_NEGATIVE_CHARGE               = 28085,
    SPELL_NEGATIVE_CHARGE_STACK         = 29660,
    SPELL_POSITIVE_POLARITY             = 28059,
    SPELL_NEGATIVE_POLARITY             = 28084
*/
    // The Four Horsemen
    // Lady Blaumeux's Void Zone (28863) summons this; the puddle then channels Consumption on
    // everyone standing in it until it despawns.
    static constexpr uint32 FourHorsemenVoidZoneEntry = 16697;

    // Sapphiron
    static constexpr uint32 Icebolt10 = 28522;
    static constexpr uint32 Icebolt25 = 28526;
    //static constexpr uint32 Chill25 = 55699;
    static constexpr uint32 Chill25 = 28547;
    static constexpr uint32 LifeDrain = 28542;
    static constexpr uint32 FrostMissile = 30101;
    static constexpr uint32 FrostExplosion = 28524;

    // Kel'Thuzad
    static constexpr uint32 FrostBlast = 27808;
    static constexpr uint32 DetonateMana = 27819;
    static constexpr uint32 ChainsOfKelthuzad = 28410;
    static constexpr uint32 ShadowFissure = 27810;
    static constexpr uint32 FrostBoltSingle = 28478;
    static constexpr uint32 FrostBoltMulti = 28479;
    // 25-man single-target Frost Bolt id from spelldifficulty_dbc (28478->55802).
    static constexpr uint32 FrostBoltSingle25 = 55802;
    // Druid Cyclone - used to lock down a raider charmed by Chains of Kel'Thuzad.
    static constexpr uint32 Cyclone = 33786;

    // Gluth
    static constexpr uint32 Decimate10 = 28374;
    static constexpr uint32 Decimate25 = 54426;
    static constexpr uint32 Decimate25Alt = 28375;
    static constexpr uint32 MortalWound10 = 25646;
    static constexpr uint32 MortalWound25 = 54378;
    static constexpr uint32 GluthFrenzy10 = 28371;
    static constexpr uint32 GluthFrenzy25 = 54427;
/*
    SPELL_MORTAL_WOUND                  = 25646,
    SPELL_ENRAGE                        = 28371,
    SPELL_DECIMATE                      = 28374,
    SPELL_DECIMATE_DAMAGE               = 28375,
    SPELL_BERSERK                       = 26662,
    SPELL_INFECTED_WOUND                = 29306,
    SPELL_CHOW_SEARCHER                 = 28404
*/
    // Anub'Rekhan
    static constexpr uint32 LocustSwarm10 = 28785;
    // 28786 exists in the DBC but boss_anubrekhan never casts it. Kept only so nobody re-adds it.
    static constexpr uint32 LocustSwarm10Alt = 28786;
    static constexpr uint32 LocustSwarm25 = 54021;  // 25-man Locust Swarm
    // Impale leaves no aura and no dynobject, so bots cannot react to it - the ids are here for
    // reference and the strategy pre-spreads instead.
    static constexpr uint32 Impale10 = 28783;
    static constexpr uint32 Impale25 = 56090;
    static constexpr uint32 SummonCorpseScarabs5 = 29105;   // from a dead player
    static constexpr uint32 SummonCorpseScarabs10 = 28864;  // from a dead Crypt Guard

    // Loatheb
    static constexpr uint32 NecroticAura10 = 55593;
    // Spore crit buff. Not referenced by this core's boss_loatheb.cpp, so it may never land.
    static constexpr uint32 FungalCreep = 29232;
/*
    SPELL_NECROTIC_AURA                         = 55593,
    SPELL_SUMMON_SPORE                          = 29234,
    SPELL_DEATHBLOOM                            = 29865,
    SPELL_INEVITABLE_DOOM                       = 29204,
    SPELL_BERSERK                               = 26662
*/
    inline bool HasAnyAura(Unit* unit, std::initializer_list<uint32> spellIds)
    {
        if (!unit)
        {
            return false;
        }

        for (uint32 spellId : spellIds)
        {
            if (unit->HasAura(spellId))
            {
                return true;
            }
        }
        return false;
    }

    inline Aura* GetAnyAura(Unit* unit, std::initializer_list<uint32> spellIds)
    {
        if (!unit)
        {
            return nullptr;
        }

        for (uint32 spellId : spellIds)
        {
            if (Aura* aura = unit->GetAura(spellId))
            {
                return aura;
            }
        }
        return nullptr;
    }

    inline bool MatchesAnySpellId(SpellInfo const* info, std::initializer_list<uint32> spellIds)
    {
        if (!info)
        {
            return false;
        }

        for (uint32 spellId : spellIds)
        {
            if (info->Id == spellId)
            {
                return true;
            }
        }
        return false;
    }
}  // namespace NaxxSpellIds

#endif
