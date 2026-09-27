/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BISWIRE_H
#define PLAYERBOTS_BISWIRE_H

// Reads the block payloads of the BisTooltipAC dataset (bistooltip_block.payload). Must accept
// exactly what the wowsimwotlk acbis decoder accepts. Std-only so tools/nativetest can build it.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace BisWire
{

constexpr std::size_t MAX_RANKS = 6;
constexpr uint8_t SIM_SLOT_COUNT = 17;

// Sim slot -> AC EQUIPMENT_SLOT
constexpr std::array<uint8_t, SIM_SLOT_COUNT> EQUIP_SLOT = {0, 1, 2, 14, 4, 8, 9, 5, 6, 7, 10, 11, 12, 13, 15, 16, 17};

// Same values as BisSpecTab in BisListMgr.h, which this header can't include.
constexpr uint8_t TAB_FERAL_TANK = 10;
constexpr uint8_t TAB_BLOOD_TANK = 11;
constexpr uint8_t TAB_FURY_PROT = 12;

struct Slot
{
    uint8_t simSlot = 0;
    // rank 1 first
    std::vector<uint32_t> items;
    // enchant, gems and reforge belong to rank 1 only, 0 for none
    uint32_t enchantSpell = 0;
    // socket order, empty sockets left out
    std::vector<uint32_t> gems;
    // ItemModType ids
    uint32_t reforgeFrom = 0;
    uint32_t reforgeTo = 0;
    // rank 1's score over rank 2's, can be negative
    bool hasDelta = false;
    int32_t delta = 0;
};

struct Block
{
    // strictly increasing simSlot
    std::vector<Slot> slots;

    Slot const* Find(uint8_t simSlot) const
    {
        for (Slot const& slot : slots)
            if (slot.simSlot == simSlot)
                return &slot;
        return nullptr;
    }
};

struct SpecKey
{
    uint8_t tab;
    // 1 for a second build on the same tab: Fire FFB, Destruction fire
    uint8_t variant;
};

namespace Detail
{

// Plain decimal only, no '+', spaces or leading zeros, so each value has exactly one spelling.
inline std::optional<int32_t> ParseInt(std::string_view s)
{
    std::string_view digits = s;
    bool negative = false;
    if (!digits.empty() && digits.front() == '-')
    {
        negative = true;
        digits.remove_prefix(1);
    }
    if (digits.empty() || (digits.front() == '0' && digits.size() > 1) || (digits == "0" && negative))
        return std::nullopt;
    int64_t value = 0;
    for (char c : digits)
    {
        if (c < '0' || c > '9')
            return std::nullopt;
        value = value * 10 + (c - '0');
        if (value > int64_t(INT32_MAX) + 1)
            return std::nullopt;
    }
    if (negative)
        value = -value;
    if (value > INT32_MAX)
        return std::nullopt;
    return int32_t(value);
}

inline std::vector<std::string_view> Split(std::string_view s, char sep)
{
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= s.size(); ++i)
    {
        if (i == s.size() || s[i] == sep)
        {
            parts.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return parts;
}

inline std::string Enhancements(Slot const& slot)
{
    std::string out;
    auto add = [&out](std::string const& enh)
    {
        if (!out.empty())
            out += ',';
        out += enh;
    };
    if (slot.enchantSpell)
        add("e" + std::to_string(slot.enchantSpell));
    for (uint32_t gem : slot.gems)
        add("g" + std::to_string(gem));
    if (slot.reforgeFrom)
        add("r" + std::to_string(slot.reforgeFrom) + "-" + std::to_string(slot.reforgeTo));
    return out;
}

inline bool Fail(std::string& error, std::string message)
{
    error = std::move(message);
    return false;
}

inline bool DecodeEnhancement(std::string_view enh, Slot& slot, std::string& error)
{
    if (enh.empty())
        return Fail(error, "empty enhancement");
    std::string_view const value = enh.substr(1);
    switch (enh.front())
    {
        case 'e':
        {
            if (slot.enchantSpell)
                return Fail(error, "two enchants");
            std::optional<int32_t> enchant = ParseInt(value);
            if (!enchant || *enchant <= 0)
                return Fail(error, "enchant \"" + std::string(value) + "\"");
            slot.enchantSpell = uint32_t(*enchant);
            return true;
        }
        case 'g':
        {
            std::optional<int32_t> gem = ParseInt(value);
            if (!gem || *gem <= 0)
                return Fail(error, "gem \"" + std::string(value) + "\"");
            slot.gems.push_back(uint32_t(*gem));
            return true;
        }
        case 'r':
        {
            if (slot.reforgeFrom)
                return Fail(error, "two reforges");
            std::size_t const dash = value.find('-');
            if (dash == std::string_view::npos)
                return Fail(error, "reforge \"" + std::string(value) + "\"");
            std::optional<int32_t> from = ParseInt(value.substr(0, dash));
            std::optional<int32_t> to = ParseInt(value.substr(dash + 1));
            if (!from || !to || *from <= 0 || *to <= 0)
                return Fail(error, "reforge \"" + std::string(value) + "\"");
            slot.reforgeFrom = uint32_t(*from);
            slot.reforgeTo = uint32_t(*to);
            return true;
        }
        default:
            return Fail(error, "unknown enhancement \"" + std::string(enh) + "\"");
    }
}

// <slot>:<item>[,<item>...][(<enh>[,<enh>...])][+<delta>], enh being e<spell>, g<gem> or r<from>-<to>
inline bool DecodeSlot(std::string_view text, Slot& slot, std::string& error)
{
    std::size_t const colon = text.find(':');
    if (colon == std::string_view::npos)
        return Fail(error, "no ':' after the slot");
    std::optional<int32_t> simSlot = ParseInt(text.substr(0, colon));
    if (!simSlot || *simSlot < 0 || *simSlot >= SIM_SLOT_COUNT)
        return Fail(error, "bad slot \"" + std::string(text.substr(0, colon)) + "\"");
    slot.simSlot = uint8_t(*simSlot);
    std::string_view rest = text.substr(colon + 1);

    if (std::size_t const plus = rest.find('+'); plus != std::string_view::npos)
    {
        std::optional<int32_t> delta = ParseInt(rest.substr(plus + 1));
        if (!delta)
            return Fail(error, "bad delta");
        slot.delta = *delta;
        slot.hasDelta = true;
        rest = rest.substr(0, plus);
    }

    if (std::size_t const open = rest.find('('); open != std::string_view::npos)
    {
        if (rest.back() != ')')
            return Fail(error, "unclosed '('");
        std::string_view const enhs = rest.substr(open + 1, rest.size() - open - 2);
        for (std::string_view enh : Split(enhs, ','))
            if (!DecodeEnhancement(enh, slot, error))
                return false;
        // every value parsed is canonical, so a mismatch can only be the order
        if (Enhancements(slot) != enhs)
            return Fail(error, "enhancements out of order, want enchant, gems, reforge");
        rest = rest.substr(0, open);
    }

    for (std::string_view itemText : Split(rest, ','))
    {
        std::optional<int32_t> item = ParseInt(itemText);
        if (!item || *item <= 0)
            return Fail(error, "bad item \"" + std::string(itemText) + "\"");
        for (uint32_t earlier : slot.items)
            if (earlier == uint32_t(*item))
                return Fail(error, "item " + std::to_string(*item) + " ranked twice");
        slot.items.push_back(uint32_t(*item));
    }
    if (slot.items.size() > MAX_RANKS)
        return Fail(error, "more than 6 items");
    if (slot.hasDelta && slot.items.size() < 2)
        return Fail(error, "a delta but no rank 2");
    return true;
}

}  // namespace Detail

// On failure block is left empty and error says why.
inline bool Decode(std::string_view payload, Block& block, std::string& error)
{
    block.slots.clear();
    if (payload.empty())
        return Detail::Fail(error, "empty payload");
    Block decoded;
    for (std::string_view text : Detail::Split(payload, ';'))
    {
        Slot slot;
        if (!Detail::DecodeSlot(text, slot, error))
            return Detail::Fail(error, "\"" + std::string(text) + "\": " + error);
        if (!decoded.slots.empty() && slot.simSlot <= decoded.slots.back().simSlot)
            return Detail::Fail(error, "slot " + std::to_string(slot.simSlot) + " comes after slot " +
                                           std::to_string(decoded.slots.back().simSlot));
        decoded.slots.push_back(std::move(slot));
    }
    block = std::move(decoded);
    return true;
}

// Adler-32 as 8 lowercase hex digits, what bistooltip_block.checksum holds
inline std::string Checksum(std::string_view s)
{
    uint32_t a = 1;
    uint32_t b = 0;
    for (unsigned char c : s)
    {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    char hex[9];
    std::snprintf(hex, sizeof(hex), "%08x", unsigned((b << 16) | a));
    return hex;
}

// classId and bistooltip_subject.spec_name -> the tab playerbots_bis_ranked uses for that spec
inline std::optional<SpecKey> SpecKeyFor(uint8_t classId, std::string_view specName)
{
    struct Entry
    {
        uint8_t classId;
        std::string_view spec;
        SpecKey key;
    };
    static constexpr Entry entries[] = {
        {1, "Arms", {0, 0}},
        {1, "Fury", {1, 0}},
        {1, "Protection", {2, 0}},
        {1, "Fury-Prot", {TAB_FURY_PROT, 0}},
        {2, "Holy", {0, 0}},
        {2, "Protection", {1, 0}},
        {2, "Retribution", {2, 0}},
        {3, "Beast mastery", {0, 0}},
        {3, "Marksmanship", {1, 0}},
        {3, "Survival", {2, 0}},
        {4, "Assassination", {0, 0}},
        {4, "Combat", {1, 0}},
        {4, "Subtlety", {2, 0}},
        {5, "Discipline", {0, 0}},
        {5, "Holy", {1, 0}},
        {5, "Shadow", {2, 0}},
        {6, "Blood dps", {0, 0}},
        {6, "Frost", {1, 0}},
        {6, "Unholy", {2, 0}},
        {6, "Blood tank", {TAB_BLOOD_TANK, 0}},
        {7, "Elemental", {0, 0}},
        {7, "Enhancement", {1, 0}},
        {7, "Restoration", {2, 0}},
        {8, "Arcane", {0, 0}},
        {8, "Fire", {1, 0}},
        {8, "Fire FFB", {1, 1}},
        {8, "Frost", {2, 0}},
        {9, "Affliction", {0, 0}},
        {9, "Demonology", {1, 0}},
        {9, "Destruction", {2, 0}},
        {9, "Destruction fire", {2, 1}},
        {11, "Balance", {0, 0}},
        {11, "Feral dps", {1, 0}},
        {11, "Restoration", {2, 0}},
        {11, "Feral tank", {TAB_FERAL_TANK, 0}},
    };
    for (Entry const& entry : entries)
        if (entry.classId == classId && entry.spec == specName)
            return entry.key;
    return std::nullopt;
}

}  // namespace BisWire

#endif
