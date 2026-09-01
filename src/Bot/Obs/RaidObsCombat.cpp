/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidObsSession.h"

#include "Creature.h"
#include "Map.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "Timer.h"

#include <algorithm>
#include <vector>

namespace RaidObs
{
// What the raid puts out, as a running total per bot rather than a record each. The incoming stream
// is one record per hit because a death has to be rewound blow by blow; outgoing damage is only ever
// read as a rate, and a record per swing and tick would be tens of thousands of lines for a number the
// snapshot already carries four times a second.
//
// A pet, totem or guardian is credited to its owner, the way NoteCast resolves relevance: the bot
// chose to have it out, and its damage is the bot's throughput.
void AccrueDamageDealt(Unit* attacker, Unit* victim, uint32 amount)
{
    if (!attacker)
        return;

    ObsSession* session = SessionFor(attacker);
    if (!session)
        return;

    Player* owner = attacker->ToPlayer();
    if (!owner)
        owner = attacker->GetOwner() ? attacker->GetOwner()->ToPlayer() : nullptr;

    if (!owner || !session->Tracks(owner))
        return;

    // Raid on raid is friendly fire or a duel, not throughput, and the incoming stream already has it.
    // Tracks() refuses a non-player before it can name one, so a boss victim costs nothing here.
    if (session->Tracks(victim))
        return;

    session->bots[GuidKey(owner->GetGUID())].damageDealt += amount;
}

// --- combat events ------------------------------------------------------------

void NoteDamage(Unit* attacker, Unit* victim, SpellInfo const* spell, uint32 amount, int32 overkill,
                uint32 schoolMask, uint32 absorb, uint32 resist)
{
    if (!Active() || !victim || amount < g_cfg.minDamage)
        return;

    AccrueDamageDealt(attacker, victim, amount);

    ProbeTarget probe(victim);
    if (!probe)
        return;

    ObsSession& s = probe.Session();
    uint32 const now = getMSTime();

    if (attacker)
        s.EnsureUnit(attacker);

    uint64 const src = attacker ? GuidKey(attacker->GetGUID()) : 0;
    uint32 const spellId = spell ? spell->Id : 0;

    BotTrace& trace = s.bots[GuidKey(victim->GetGUID())];
    trace.damage.push_back({now, src, spellId, amount});
    while (!trace.damage.empty() && getMSTimeDiff(trace.damage.front().ms, now) > g_cfg.deathRewindMs)
        trace.damage.pop_front();

    s.EnsureSpell(spellId);

    std::string fields = "\"s\":" + std::to_string(src);
    fields += ",\"d\":" + std::to_string(GuidKey(victim->GetGUID()));
    fields += ",\"sp\":" + std::to_string(spellId);
    fields += ",\"a\":" + std::to_string(amount);
    fields += ",\"ok\":" + std::to_string(overkill);
    fields += ",\"sc\":" + std::to_string(schoolMask);
    fields += ",\"ab\":" + std::to_string(absorb);
    fields += ",\"rs\":" + std::to_string(resist);
    fields += ",\"hp\":" + Num(victim->GetHealthPct());

    s.Emit(now, "dmg", fields);
}

void NoteKillingBlow(Unit* attacker, Unit* victim, uint32 amount)
{
    if (!Active() || !victim || !amount || amount < victim->GetHealth())
        return;

    ProbeTarget probe(victim);
    if (!probe)
        return;

    if (attacker)
        probe.Session().EnsureUnit(attacker);

    BotTrace& trace = probe.Trace();
    trace.killBlowSource = attacker ? GuidKey(attacker->GetGUID()) : 0;
    trace.killBlowAmount = amount;
}

void NoteScriptedWipe(Player* bot)
{
    if (!Active() || !bot)
        return;

    ProbeTarget probe(bot);
    if (!probe)
        return;

    // Cleared in NoteDeath alongside the kill blow. If the command misses - the bot is already dead,
    // or out of the master's party - the flag simply waits for a death that never uses it.
    probe.Trace().scriptedWipe = true;
}

void NoteHeal(Unit* healer, Unit* target, SpellInfo const* spell, uint32 amount, uint32 overheal)
{
    if (!Active() || !g_cfg.logHeals || !target)
        return;

    ProbeTarget probe(target);
    if (!probe)
        return;

    ObsSession& s = probe.Session();
    s.EnsureSpell(spell ? spell->Id : 0);

    std::string fields = "\"s\":" + std::to_string(healer ? GuidKey(healer->GetGUID()) : 0);
    fields += ",\"d\":" + std::to_string(GuidKey(target->GetGUID()));
    fields += ",\"sp\":" + std::to_string(spell ? spell->Id : 0);
    fields += ",\"a\":" + std::to_string(amount);
    fields += ",\"oh\":" + std::to_string(overheal);
    fields += ",\"hp\":" + Num(target->GetHealthPct());

    s.Emit(getMSTime(), "heal", fields);
}

void NoteAbsorb(Unit* victim, Unit* absorbCaster, SpellInfo const* absorbSpell, uint32 amount)
{
    if (!Active() || !victim || !absorbSpell || !amount)
        return;

    ProbeTarget probe(victim);
    if (!probe)
        return;

    probe.Session().EnsureSpell(absorbSpell->Id);

    std::string fields = "\"d\":" + std::to_string(GuidKey(victim->GetGUID()));
    fields += ",\"s\":" + std::to_string(absorbCaster ? GuidKey(absorbCaster->GetGUID()) : 0);
    fields += ",\"sp\":" + std::to_string(absorbSpell->Id);
    fields += ",\"a\":" + std::to_string(amount);

    probe.Session().Emit(getMSTime(), "abs", fields);
}

// Runs inside Unit::_ApplyAura, so it gets the stamp the client-update hook below cannot: that one
// only fires on an apply once Unit::_UpdateSpells flushes the pending flag, while a removal goes out
// synchronously. Hodir's Fury lands and kills in the same tick, so without this the death record has
// only a removal to build from and falls back to the -1 sentinel. Fires twice on a stack refresh,
// which is harmless here because the stamp is idempotent and nothing is emitted.
void NoteAuraApplied(Unit* target, Aura* aura)
{
    if (!Active() || !target || !aura)
        return;

    ProbeTarget probe(target);
    if (!probe)
        return;

    // Same guard as NoteAura: appliedMs is the first application of the current uninterrupted run, not
    // the last refresh. The two feeds must not disagree about that.
    AuraState& state = probe.Trace().auras[aura->GetId()];
    uint32 const now = getMSTime();
    if (!state.appliedMs || state.removedMs)
        state.appliedMs = now;

    state.removedMs = 0;

    // Also filled in NoteAura, but an aura that finds no free visible slot fires neither client update,
    // so this is the only place a complete row for it gets written.
    SpellInfo const* info = aura->GetSpellInfo();

    state.caster = GuidKey(aura->GetCasterGUID());
    state.stacks = aura->GetStackAmount();
    state.duration = aura->GetDuration();
    state.positive = info && info->IsPositive();
}

void NoteAura(Unit* target, Aura* aura, bool removed)
{
    if (!Active() || !target || !aura)
        return;

    ProbeTarget probe(target);
    if (!probe)
        return;

    ObsSession& s = probe.Session();
    uint32 const now = getMSTime();
    uint32 const spellId = aura->GetId();
    uint32 const stacks = aura->GetStackAmount();
    int32 const duration = aura->GetDuration();
    uint64 const key = GuidKey(target->GetGUID());

    // Tracked ahead of the logAuras gate. This set is what a death record reads, and turning the aura
    // stream off must not also empty every death record.
    AuraState& state = s.bots[key].auras[spellId];
    if (removed)
    {
        state.removedMs = now;
    }
    else
    {
        if (!state.appliedMs || state.removedMs)
            state.appliedMs = now;

        state.removedMs = 0;
    }

    // Whether this helps or hurts, from the spell rather than from who cast it. The caster is not a
    // usable proxy in either direction: totems and pets buff from a creature guid, and Biting Cold is
    // applied to the player by the player, through the zone aura's trigger.
    SpellInfo const* info = aura->GetSpellInfo();

    state.caster = GuidKey(aura->GetCasterGUID());
    state.stacks = stacks;
    state.duration = duration;
    state.positive = info && info->IsPositive();

    if (!g_cfg.logAuras)
        return;

    s.EnsureSpell(spellId);

    // Null once the caster is gone, which EnsureUnit handles. The guid below stays valid either way,
    // so a caster that despawned before this fired keeps its bare number - the honest answer.
    s.EnsureUnit(aura->GetCaster());

    std::string fields = "\"d\":" + std::to_string(key);
    fields += ",\"s\":" + std::to_string(state.caster);
    fields += ",\"sp\":" + std::to_string(spellId);
    fields += ",\"r\":" + std::string(removed ? "1" : "0");
    fields += ",\"st\":" + std::to_string(stacks);
    fields += ",\"dur\":" + std::to_string(duration);
    fields += ",\"p\":" + std::string(state.positive ? "1" : "0");

    s.Emit(now, "aura", fields);
}

void NoteCast(Unit* caster, SpellInfo const* spell, Unit* target, uint32 castTimeMs, bool triggered)
{
    if (!Active() || !caster || !spell)
        return;

    ObsSession* session = SessionFor(caster);
    if (!session)
        return;

    ObsSession& s = *session;

    // Same instance is not the same pull: a Hodir trace picked up 253 casts from a Freya-area mob two
    // rooms away and none at all from its own raid. The roster, its pets and totems, and whatever is
    // being watched are the pull; everything else on the map is somebody else's.
    bool relevant = false;
    if (caster->IsPlayer())
        relevant = s.Tracks(caster);
    else if (s.watched.count(caster->GetGUID()))
        relevant = true;
    else if (Unit* owner = caster->GetOwner())
        relevant = owner->IsPlayer() && s.Tracks(owner);

    if (!relevant)
        return;

    s.EnsureUnit(caster);
    s.EnsureUnit(target);
    s.EnsureSpell(spell->Id);

    std::string fields = "\"s\":" + std::to_string(GuidKey(caster->GetGUID()));
    fields += ",\"sp\":" + std::to_string(spell->Id);
    fields += ",\"tgt\":" + std::to_string(target ? GuidKey(target->GetGUID()) : 0);
    fields += ",\"ct\":" + std::to_string(castTimeMs);
    // Written only when set, the way a move omits hpr/hms off anything but a wait, so the ability
    // casts this channel exists for stay the cheap case.
    if (triggered)
        fields += ",\"tr\":1";

    s.Emit(getMSTime(), "cast", fields);
}

void NoteDeath(Unit* unit, Unit* killer)
{
    if (!Active() || !unit || !unit->IsPlayer())
        return;

    ProbeTarget probe(unit);
    if (!probe)
        return;

    ObsSession& s = probe.Session();
    uint32 const now = getMSTime();
    uint64 const key = GuidKey(unit->GetGUID());
    BotTrace& trace = s.bots[key];

    // The pass the bot died in is the one worth reading and nothing else will close it.
    s.FlushTick(key, trace);

    std::string fields = "\"g\":" + std::to_string(key);
    fields += ",\"killer\":" + std::to_string(killer ? GuidKey(killer->GetGUID()) : 0);
    fields += ",\"x\":" + Num(unit->GetPositionX());
    fields += ",\"y\":" + Num(unit->GetPositionY());
    fields += ",\"z\":" + Num(unit->GetPositionZ());

    std::string dist = "{";
    bool firstDist = true;
    for (ObjectGuid guid : s.watched)
    {
        Creature* creature = s.map ? s.map->GetCreature(guid) : nullptr;
        if (!creature || !creature->IsInWorld())
            continue;

        if (!firstDist)
            dist += ",";
        firstDist = false;
        dist += "\"" + std::to_string(GuidKey(guid)) + "\":" + Num(unit->GetExactDist(creature));
    }
    dist += "}";
    fields += ",\"dist\":" + dist;

    // Read from the tracked set, never from the unit. Unit::Kill calls RemoveAllAurasOnDeath long
    // before OnUnitDeath fires, so by the time this runs the only things still applied are passives and
    // death-persistent auras - which is why v3 death records listed 57 talents and no boss debuff.
    // Anything dropped inside the grace window is still reported, flagged with when it came off.
    std::string auras = "[";
    bool firstAura = true;
    for (auto const& entry : trace.auras)
    {
        AuraState const& state = entry.second;
        if (state.removedMs && getMSTimeDiff(state.removedMs, now) > OBS_DEATH_AURA_GRACE_MS)
            continue;

        if (!firstAura)
            auras += ",";
        firstAura = false;
        auras += "[" + std::to_string(entry.first);
        auras += "," + std::to_string(state.stacks);
        auras += "," + std::to_string(state.duration);
        auras += "," + std::to_string(state.caster);
        auras += "," + std::to_string(state.appliedMs ? s.Stamp(state.appliedMs) : int64(-1));
        auras += "," + std::to_string(state.removedMs ? s.Stamp(state.removedMs) : int64(-1));
        auras += "," + std::string(state.positive ? "1" : "0");
        auras += "]";

        s.EnsureSpell(entry.first);
    }
    auras += "]";
    fields += ",\"auras\":" + auras;

    // Trimmed here as well as on push: the ring only ever loses its front when something new arrives,
    // so a bot that goes untouched for a minute and then dies would otherwise report the hits that
    // landed a minute ago as though they were the ones that killed it. Chronological, because the
    // question a rewind answers is what came last, not what hit hardest.
    std::vector<DamageEntry> rewind;
    for (DamageEntry const& entry : trace.damage)
        if (getMSTimeDiff(entry.ms, now) <= g_cfg.deathRewindMs)
            rewind.push_back(entry);

    std::sort(rewind.begin(), rewind.end(),
              [](DamageEntry const& a, DamageEntry const& b) { return a.ms < b.ms; });

    std::string rewindJson = "[";
    for (std::size_t i = 0; i < rewind.size(); ++i)
    {
        if (i)
            rewindJson += ",";
        rewindJson += "[" + std::to_string(s.Stamp(rewind[i].ms));
        rewindJson += "," + std::to_string(rewind[i].source);
        rewindJson += "," + std::to_string(rewind[i].spellId);
        rewindJson += "," + std::to_string(rewind[i].amount);
        rewindJson += "]";

        s.EnsureSpell(rewind[i].spellId);
    }
    rewindJson += "]";
    fields += ",\"rewind\":" + rewindJson;

    if (trace.killBlowAmount)
    {
        fields += ",\"blow\":[" + std::to_string(trace.killBlowSource);
        fields += "," + std::to_string(trace.killBlowAmount) + "]";
    }

    // Both of these reach Unit::Kill without passing DealDamage, so there is no blow to point at and
    // `killer` is the bot itself. Saying which one it was is the difference between a death worth
    // reading and one that should never have been counted.
    if (!trace.killBlowAmount && killer == unit)
        fields += trace.scriptedWipe ? ",\"cause\":\"reset\"" : ",\"cause\":\"self\"";

    trace.killBlowSource = 0;
    trace.killBlowAmount = 0;
    trace.scriptedWipe = false;

    if (trace.lastHpMs)
    {
        fields += ",\"hplast\":[" + Num(trace.lastHpPct);
        fields += "," + std::to_string(s.Stamp(trace.lastHpMs)) + "]";
    }

    // One row per verdict per distinct pass, carrying how many passes running produced it. A raw list
    // of every verdict ran to 1300 entries and made up most of the record; the passes repeat verbatim,
    // so this collapses to a handful of rows with the interleaving order intact.
    std::string acts = "[";
    bool firstAct = true;
    for (TickRecord const& record : trace.ticks)
    {
        for (TickEntry const& entry : record.entries)
        {
            if (!firstAct)
                acts += ",";
            firstAct = false;
            acts += "[" + std::to_string(s.Stamp(record.firstMs));
            acts += "," + std::to_string(s.Stamp(record.lastMs));
            acts += "," + Quoted(entry.action);
            acts += "," + Num(entry.relevance);
            acts += "," + Quoted(entry.veto ? "VETO:" + entry.verdict : entry.verdict);
            acts += "," + std::to_string(record.repeats);
            acts += "]";
        }
    }
    acts += "]";
    fields += ",\"acts\":" + acts;

    if (trace.hasMove)
    {
        std::string move = "{\"x\":" + Num(trace.lastMoveX);
        move += ",\"y\":" + Num(trace.lastMoveY);
        move += ",\"z\":" + Num(trace.lastMoveZ);
        move += ",\"by\":" + Quoted(trace.lastMoveBy);
        move += ",\"arrived\":" +
                std::string(unit->GetExactDist2d(trace.lastMoveX, trace.lastMoveY) < 3.0f ? "1" : "0");
        move += "}";
        fields += ",\"lastmove\":" + move;
    }

    s.Emit(now, "death", fields);
    s.Flush();
}
}  // namespace RaidObs
