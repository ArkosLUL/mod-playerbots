#ifndef PLAYERBOTS_RAID_TOCHELPERS_DEFENSIVE_H
#define PLAYERBOTS_RAID_TOCHELPERS_DEFENSIVE_H

#include <string>
#include <vector>

#include "Define.h"
#include "SharedDefines.h"

class Player;
class PlayerbotAI;

// Raid cooldowns and personal buttons spent ahead of a hit the encounter sees coming. The encounter
// says when the hit lands and what it does, this says who answers it and with what. Nothing resolves
// off map 649.
namespace TrialOfTheCrusaderHelpers
{

struct ToCPredictedHit
{
    // getMSTime() of its first damage, already past once it's running
    uint32 startMs = 0;
    // first damage to last
    uint32 durationMs = 0;
    // per player, what is still to come of it, before resistance and damage taken modifiers
    uint32 damage = 0;
    // damage read off the member's own health loss, so resistance and running cuts are already in it
    bool measured = false;
    // four or more FC champions on the bot, now or at their next pick
    bool focus = false;
    SpellSchoolMask school = SPELL_SCHOOL_MASK_NONE;
    uint8 casterLevel = 83;
    // members who mustn't channel, null for nobody
    bool (*noChannel)(Player*) = nullptr;
};

enum class ToCRaidCover : uint8
{
    None,
    AuraMastery,
    DivineSacrifice,
    AntiMagicZone,
    DivineHymn,
    Tranquility,
};

// A cover goes up from this long before the first damage until this long after it: Aura Mastery and
// Divine Guardian last 6 s, so either one cast in the window still runs past a 5 s hit's last tick.
constexpr uint32 TOC_COVER_WINDOW_LEAD_MS = 1500;
constexpr uint32 TOC_COVER_WINDOW_LATE_MS = 1000;
// Covers are dealt once the hit would leave this many members under the floor
constexpr uint32 TOC_COVER_MIN_UNDER = 3;
// What a member should still have left after the hit, of its max health
constexpr float TOC_DEFENSIVE_FLOOR_PCT = 25.0f;
// A personal button goes up from this long before the first damage
constexpr uint32 TOC_PERSONAL_LEAD_MS = 1000;

// The raid cooldown this bot casts now, None otherwise. Dealt on the first read where the hit would leave
// TOC_COVER_MIN_UNDER members under the floor, one bot per cooldown and one cooldown per bot, among bots
// that know it, have it off cooldown and can pay for it; latched for the hit, cast only inside its
// window. Aura Mastery only from a paladin holding its own resistance aura of the hit's school, Divine
// Sacrifice only with Divine Guardian, Anti-Magic Zone only with TOC_COVER_MIN_UNDER members inside it.
// Pass null with no hit coming, which drops the deal. Writes toc.cover.
ToCRaidCover GetToCRaidCover(PlayerbotAI* botAI, ToCPredictedHit const* hit);
// The bot's own rank of it, 0 when it doesn't know it
uint32 ToCRaidCoverSpellId(PlayerbotAI* botAI, ToCRaidCover cover);
bool IsToCRaidCoverChannel(ToCRaidCover cover);
// After a cast went out: Divine Sacrifice comes off again once Divine Guardian is up, since under a hit
// on everyone its 30% redirect drains the paladin.
void FinishToCRaidCover(PlayerbotAI* botAI, ToCRaidCover cover);

// The paladin is talented into Divine Guardian, so its Divine Sacrifice cuts damage to the whole raid
bool ToCHasDivineGuardian(Player* paladin);

// Before the first damage: the class nodes for the raid cooldowns are held over this, so none goes out
// early or twice.
bool ToCRaidHitLeadIn(ToCPredictedHit const& hit);
std::vector<std::string> ToCRaidCoverClassNodes();

struct ToCPersonalPick
{
    uint32 spellId = 0;
    char const* consumable = nullptr;

    explicit operator bool() const { return spellId || consumable; }
};

// The bot's own answer to a hit that would leave it under the floor, from TOC_PERSONAL_LEAD_MS before
// the first damage: NextMagicDefensive's row for what is left of the hit (NextPhysicalDefensive's, then
// Evasion, Deterrence or Pain Suppression, for a physical one), and on a lethal one with no row
// Dispersion, the class immunity, then a healthstone at 70% or under. A non-tank paladin on a measured
// hit takes Divine Shield first, a mage on a focus hit Ice Block, the GCD or a cast in progress
// notwithstanding. Nothing while one runs or while it channels a raid cover. Writes toc.personal.
ToCPersonalPick GetToCPersonalDefensive(PlayerbotAI* botAI, ToCPredictedHit const* hit);

}

#endif
