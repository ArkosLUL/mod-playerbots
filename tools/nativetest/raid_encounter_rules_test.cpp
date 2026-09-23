/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidEncounterRules.h"

#include <cstdio>
#include <cstdlib>

// Not assert: these have to fail under NDEBUG too.
#define CHECK(cond)                                                                \
    do                                                                             \
    {                                                                              \
        if (!(cond))                                                               \
        {                                                                          \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            std::exit(1);                                                          \
        }                                                                          \
    } while (0)

namespace
{

using namespace RaidEncounterRules;

// What ClassifyAction returns for a few real actions.
constexpr FamilyMask FOLLOW = Family::AnyMovement | Family::Follow;
constexpr FamilyMask REACH_MELEE = Family::AnyMovement | Family::Reach;
constexpr FamilyMask REACH_HEAL = Family::AnyMovement | Family::ReachHeal;
constexpr FamilyMask TANK_FACE = Family::AnyMovement | Family::TankFace;
constexpr FamilyMask MELEE = Family::AnyMovement | Family::Attack | Family::Melee;
constexpr FamilyMask DPS_ASSIST = Family::AnyMovement | Family::Attack | Family::DpsAssist;
constexpr FamilyMask TANK_ASSIST = Family::AnyMovement | Family::Attack | Family::TankAssist;
constexpr FamilyMask DPS_AOE = Family::AnyMovement | Family::Attack | Family::DpsAoe;
constexpr FamilyMask BOSS_MOVER = Family::AnyMovement;
constexpr FamilyMask CHARGE = Family::Spell | Family::Charge;
constexpr FamilyMask FIREBALL = Family::Spell;
constexpr FamilyMask PLAIN = 0;

BossStates Instance(uint32_t count, uint64_t inProgress, uint64_t done)
{
    BossStates states;
    states.inInstance = true;
    states.encounterCount = count;
    states.inProgress = inProgress;
    states.done = done;
    return states;
}

void GateTruthTable()
{
    CHECK(GateOpen(BossStates(), 3));  // outside an instance

    CHECK(GateOpen(Instance(14, 0, 0), 3));            // nothing engaged, nothing done
    CHECK(GateOpen(Instance(14, 1u << 3, 0), 3));      // this one live
    CHECK(GateOpen(Instance(14, (1u << 3) | 1u, 0), 3));  // this one live beside another
    CHECK(!GateOpen(Instance(14, 1u << 5, 0), 3));     // another one live
    CHECK(!GateOpen(Instance(14, 0, 1u << 3), 3));     // this one done
    CHECK(!GateOpen(Instance(14, 1u << 3, 1u << 3), 3));  // done wins over live
    CHECK(GateOpen(Instance(14, 0, 1u << 5), 3));      // another one done
    CHECK(GateOpen(Instance(14, 1u << 5, 0), 14));     // an id the script doesn't have
    CHECK(GateOpen(Instance(64, 0, 0), 63));
    CHECK(!GateOpen(Instance(64, uint64_t(1) << 63, 0), 0));
}

void OwnMovementBlocksGenericMoversOnly()
{
    Rule const rule = OwnMovement({"boss dodge", "boss position"});

    CHECK(RuleCares(rule, FOLLOW, "follow"));
    CHECK(RuleCares(rule, TANK_FACE, "tank face"));
    CHECK(RuleCares(rule, REACH_HEAL, "reach party member to heal"));
    CHECK(RuleCares(rule, BOSS_MOVER, "boss soak"));  // a boss action that isn't a mover row

    CHECK(!RuleCares(rule, BOSS_MOVER, "boss dodge"));
    CHECK(!RuleCares(rule, BOSS_MOVER, "boss position"));
    CHECK(!RuleCares(rule, REACH_MELEE, "reach melee"));  // default keep
    CHECK(!RuleCares(rule, MELEE, "melee"));
    CHECK(!RuleCares(rule, DPS_ASSIST, "dps assist"));
    CHECK(!RuleCares(rule, CHARGE, "charge"));  // not a MovementAction
    CHECK(!RuleCares(rule, FIREBALL, "fireball"));
    CHECK(!RuleCares(rule, PLAIN, "boss interrupt"));
}

void OwnMovementKeepAndNarrowing()
{
    Rule const withHeal = OwnMovement({}, Family::Attack | Family::Reach | Family::ReachHeal);
    CHECK(!RuleCares(withHeal, REACH_HEAL, "reach party member to heal"));

    Rule const attackOnly = OwnMovement({"boss freeze step"}, Family::Attack);
    CHECK(RuleCares(attackOnly, REACH_MELEE, "reach melee"));
    CHECK(!RuleCares(attackOnly, MELEE, "melee"));
    CHECK(!RuleCares(attackOnly, BOSS_MOVER, "boss freeze step"));
    CHECK(RuleCares(attackOnly, BOSS_MOVER, "boss position"));  // a mover row left out of the narrowed set
}

void OwnTargetingBlocksNamedPickersOnly()
{
    Rule const rule = OwnTargeting();
    CHECK(RuleCares(rule, DPS_ASSIST, "dps assist"));
    CHECK(RuleCares(rule, TANK_ASSIST, "tank assist"));
    CHECK(!RuleCares(rule, DPS_AOE, "dps aoe"));
    CHECK(!RuleCares(rule, MELEE, "melee"));
    CHECK(!RuleCares(rule, FOLLOW, "follow"));

    Rule const withDebuff = OwnTargeting(Family::DpsAssist | Family::DebuffOnAttacker);
    CHECK(RuleCares(withDebuff, Family::Spell | Family::DebuffOnAttacker, "corruption on attacker"));
    CHECK(!RuleCares(withDebuff, TANK_ASSIST, "tank assist"));
}

void BlockTakesFamiliesAndNames()
{
    Rule const rule = Block(Family::Charge | Family::Blink, {"sprint", "boss scorch"});
    CHECK(RuleCares(rule, CHARGE, "charge"));
    CHECK(RuleCares(rule, Family::Spell | Family::Blink, "blink"));
    CHECK(RuleCares(rule, FIREBALL, "sprint"));
    CHECK(RuleCares(rule, BOSS_MOVER, "boss scorch"));
    CHECK(!RuleCares(rule, FIREBALL, "fireball"));
    CHECK(!RuleCares(rule, FOLLOW, "follow"));
}

void ExclusiveBlocksAttacksToo()
{
    Rule const none = Exclusive();
    CHECK(RuleCares(none, MELEE, "melee"));
    CHECK(RuleCares(none, REACH_MELEE, "reach melee"));
    CHECK(RuleCares(none, BOSS_MOVER, "boss position"));
    CHECK(!RuleCares(none, FIREBALL, "fireball"));

    Rule const ride = Exclusive(Family::Melee, {"boss board"});
    CHECK(!RuleCares(ride, MELEE, "melee"));
    CHECK(!RuleCares(ride, BOSS_MOVER, "boss board"));
    CHECK(RuleCares(ride, DPS_ASSIST, "dps assist"));
}

void RoleFilterStopsAtFirstMatch()
{
    int asked = 0;
    auto mainTank = [&](RoleMask role)
    {
        ++asked;
        return role == Role::MainTank;
    };

    CHECK(RoleMatches(Role::Any, mainTank));
    CHECK(asked == 0);

    CHECK(RoleMatches(Role::MainTank | Role::Ranged, mainTank));
    CHECK(asked == 1);  // MainTank is the lower bit, so Ranged is never asked

    asked = 0;
    CHECK(!RoleMatches(Role::Ranged | Role::Melee, mainTank));
    CHECK(asked == 2);
}

void EvaluateRunsChecksInOrder()
{
    int gates = 0;
    int roles = 0;
    int predicates = 0;
    auto run = [&](bool cares, bool gate, bool role, bool predicate)
    {
        gates = roles = predicates = 0;
        return Evaluate(
            cares,
            [&]
            {
                ++gates;
                return gate;
            },
            [&]
            {
                ++roles;
                return role;
            },
            [&]
            {
                ++predicates;
                return predicate;
            });
    };

    CHECK(run(false, true, true, true) == 1.0f);
    CHECK(gates == 0 && roles == 0 && predicates == 0);

    CHECK(run(true, false, true, true) == 1.0f);
    CHECK(gates == 1 && roles == 0 && predicates == 0);

    CHECK(run(true, true, false, true) == 1.0f);
    CHECK(gates == 1 && roles == 1 && predicates == 0);

    CHECK(run(true, true, true, false) == 1.0f);
    CHECK(predicates == 1);

    CHECK(run(true, true, true, true) == 0.0f);
}

}  // namespace

int main()
{
    GateTruthTable();
    OwnMovementBlocksGenericMoversOnly();
    OwnMovementKeepAndNarrowing();
    OwnTargetingBlocksNamedPickersOnly();
    BlockTakesFamiliesAndNames();
    ExclusiveBlocksAttacksToo();
    RoleFilterStopsAtFirstMatch();
    EvaluateRunsChecksInOrder();

    std::puts("raid_encounter_rules_test: all passed");
    return 0;
}
