#include "UldActions_XT002.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Group.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "RtiTargetValue.h"
#include "UldBossHelper.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"

bool XT002MoveClearAction::isPossible() { return bot->CanFreeMove(); }

bool XT002MoveClearAction::MoveClearOf(std::vector<Unit*> const& avoid, float range)
{
    if (avoid.empty())
        return false;

    // Scored by how far the nearest unit ends up, capped at range: everything that clears the whole
    // group scores the same, so the first (nearest) such spot wins and the bot does not run further
    // than the mechanic needs.
    auto score = [&avoid, range](float x, float y)
    {
        float closest = std::numeric_limits<float>::max();
        for (Unit* unit : avoid)
            closest = std::min(closest, unit->GetExactDist2d(x, y));

        return std::min(closest, range);
    };

    int const directions = 8;
    float const increment = 3.0f;
    float bestX = 0.0f;
    float bestY = 0.0f;
    float bestZ = 0.0f;
    bool found = false;

    // A packed 25-man often leaves no spot inside the search ring that clears everyone. Starting from
    // the bot's own score means any improvement is taken instead - most of the raid still gets out of
    // the splash - and only a move that gains nothing is rejected.
    float bestScore = score(bot->GetPositionX(), bot->GetPositionY());

    for (int i = 0; i < directions; ++i)
    {
        float const angle = (i * 2 * M_PI) / directions;
        for (float distance = increment; distance <= (range + 5.0f); distance += increment)
        {
            float const moveX = bot->GetPositionX() + distance * cos(angle);
            float const moveY = bot->GetPositionY() + distance * sin(angle);
            float const moveZ = bot->GetPositionZ();

            float const candidate = score(moveX, moveY);
            if (candidate > bestScore && bot->IsWithinLOS(moveX, moveY, moveZ))
            {
                bestScore = candidate;
                bestX = moveX;
                bestY = moveY;
                bestZ = moveZ;
                found = true;
            }
        }
    }

    if (!found)
        return false;

    return MoveTo(bot->GetMapId(), bestX, bestY, bestZ, false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true);
}

bool XT002MoveAwayFromDebuffedAllyAction::Execute(Event /*event*/)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    uint32 const spellId = GetDebuffSpellId();

    std::vector<Unit*> debuffed;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        if (member->HasAura(spellId))
            debuffed.push_back(member);
    }

    return MoveClearOf(debuffed, range);
}

bool XT002GravityBombCarrierAction::Execute(Event /*event*/)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    std::vector<Unit*> allies;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        allies.push_back(member);
    }

    return MoveClearOf(allies, ULDUAR_XT002_DEBUFF_SPREAD_RADIUS);
}

bool XT002MarkKillTargetAction::Execute(Event /*event*/)
{
    Unit* killTarget = GetXT002KillTarget(botAI);
    if (!killTarget)
        return false;

    MarkTargetWithSkull(bot, killTarget);
    SetRtiTarget(botAI, "skull", killTarget);
    return true;
}

bool XT002BoombotRangedKillAction::Execute(Event /*event*/)
{
    Unit* boombot = GetFirstAliveUnitByEntry(botAI, PB_NPC_XT002_BOOMBOT);
    if (!boombot)
        return false;

    return Attack(boombot);
}

bool XT002AttackHeartAction::Execute(Event /*event*/)
{
    Unit* heart = GetXT002ExposedHeart(botAI);
    if (!heart)
        return false;

    return Attack(heart);
}

bool XT002PummellerTauntAction::Execute(Event /*event*/)
{
    Unit* pummeller = GetFirstAliveUnitByEntry(botAI, PB_NPC_XT002_PUMMELLER);
    if (!pummeller)
        return false;

    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
            return botAI->CastSpell("taunt", pummeller);
        case CLASS_PALADIN:
            return botAI->CastSpell("hand of reckoning", pummeller);
        case CLASS_DEATH_KNIGHT:
            return botAI->CastSpell("dark command", pummeller);
        case CLASS_DRUID:
            return botAI->CastSpell("growl", pummeller);
        default:
            return false;
    }
}

bool XT002RedirectThreatAction::isUseful()
{
    return bot->getClass() == CLASS_HUNTER || bot->getClass() == CLASS_ROGUE;
}

Player* XT002RedirectThreatAction::GetRedirectTank()
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    // A Pummeller is the off-tank's job, and it is the add most likely to peel onto a redirecter.
    if (GetFirstAliveUnitByEntry(botAI, PB_NPC_XT002_PUMMELLER))
    {
        if (Player* assistTank = GetGroupAssistTank(botAI, bot, 0))
            return assistTank;
    }

    // Otherwise feed whoever is actually holding XT, which survives a tank swap or a tank death.
    Unit* xt002 = GetXT002(botAI);
    if (xt002)
    {
        if (Unit* victim = xt002->GetVictim())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || !member->IsAlive() || member == bot)
                    continue;

                if (botAI->IsTank(member) && member == victim)
                    return member;
            }
        }
    }

    return GetGroupMainTank(botAI, bot);
}

bool XT002RedirectThreatAction::Execute(Event /*event*/)
{
    Player* tank = GetRedirectTank();
    if (!tank || tank == bot)
        return false;

    if (bot->getClass() == CLASS_ROGUE)
    {
        // Tricks redirects everything the rogue does for the next 6s, so there is no dump shot.
        return botAI->CanCastSpell("tricks of the trade", tank) && botAI->CastSpell("tricks of the trade", tank);
    }

    if (botAI->CanCastSpell("misdirection", tank))
        return botAI->CastSpell("misdirection", tank);

    // Misdirection only moves the threat of the next three shots, so spend them on XT rather than
    // leaving them to whatever the rotation picks - and never on an add the tank does not want.
    Unit* xt002 = GetXT002(botAI);
    if (xt002 && !IsXT002Submerged(botAI) && bot->HasAura(SPELL_MISDIRECTION) &&
        botAI->CanCastSpell("steady shot", xt002))
    {
        return botAI->CastSpell("steady shot", xt002);
    }

    return false;
}
