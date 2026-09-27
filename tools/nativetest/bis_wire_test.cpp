/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BisWire.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

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

using U = std::vector<uint32_t>;

// checks a decoded block against the text it came from
std::string Encode(BisWire::Block const& block)
{
    std::string out;
    for (BisWire::Slot const& slot : block.slots)
    {
        if (!out.empty())
            out += ';';
        out += std::to_string(slot.simSlot) + ":";
        for (std::size_t i = 0; i < slot.items.size(); ++i)
            out += (i ? "," : "") + std::to_string(slot.items[i]);
        std::string const enhs = BisWire::Detail::Enhancements(slot);
        if (!enhs.empty())
            out += "(" + enhs + ")";
        if (slot.hasDelta)
            out += "+" + std::to_string(slot.delta);
    }
    return out;
}

BisWire::Block MustDecode(std::string const& payload)
{
    BisWire::Block block;
    std::string error;
    if (!BisWire::Decode(payload, block, error))
    {
        std::fprintf(stderr, "decoding %s: %s\n", payload.c_str(), error.c_str());
        std::exit(1);
    }
    CHECK(Encode(block) == payload);
    return block;
}

void DecodesEveryField()
{
    BisWire::Block block = MustDecode(
        "0:51227,50712,50713,51866(e59954,g41398,g40111,r31-37)+142;1:50633;3:47545,51933(e47898)+-3;16:50733(r32-36)");
    CHECK(block.slots.size() == 4);

    BisWire::Slot const& head = block.slots[0];
    CHECK(head.simSlot == 0);
    CHECK(head.items == (U{51227, 50712, 50713, 51866}));
    CHECK(head.enchantSpell == 59954);
    CHECK(head.gems == (U{41398, 40111}));
    CHECK(head.reforgeFrom == 31 && head.reforgeTo == 37);
    CHECK(head.hasDelta && head.delta == 142);

    BisWire::Slot const& neck = block.slots[1];
    CHECK(neck.simSlot == 1 && neck.items == U{50633});
    CHECK(neck.enchantSpell == 0 && neck.gems.empty() && neck.reforgeFrom == 0 && neck.reforgeTo == 0);
    CHECK(!neck.hasDelta && neck.delta == 0);

    BisWire::Slot const& back = block.slots[2];
    CHECK(back.simSlot == 3 && back.items == (U{47545, 51933}) && back.enchantSpell == 47898);
    CHECK(back.hasDelta && back.delta == -3);

    BisWire::Slot const& ranged = block.slots[3];
    CHECK(ranged.simSlot == 16 && ranged.items == U{50733});
    CHECK(ranged.enchantSpell == 0 && ranged.reforgeFrom == 32 && ranged.reforgeTo == 36);

    CHECK(block.Find(3) == &back);
    CHECK(block.Find(2) == nullptr);
}

void RoundTrips()
{
    CHECK(MustDecode("13:50363").slots[0].items == U{50363});
    CHECK(MustDecode("7:50620(g40125)").slots[0].gems == U{40125});

    BisWire::Slot zeroDelta = MustDecode("10:50402,50618+0").slots[0];
    CHECK(zeroDelta.hasDelta && zeroDelta.delta == 0);
    CHECK(MustDecode("11:50402,50618+-12").slots[0].delta == -12);

    BisWire::Block gaps = MustDecode("0:1;14:2(e3)");
    CHECK(gaps.slots.size() == 2 && gaps.slots[1].simSlot == 14 && gaps.slots[1].enchantSpell == 3);
}

void DecodesEverySlotFull()
{
    std::string payload;
    for (int slot = 0; slot < BisWire::SIM_SLOT_COUNT; ++slot)
    {
        if (slot)
            payload += ';';
        payload += std::to_string(slot) + ":" + std::to_string(40000 + slot) +
                   ",41000,42000,43000,44000,45000(e60000,g40111,g40117,g41398,r13-31)+" + std::to_string(slot * 7);
    }
    BisWire::Block block = MustDecode(payload);
    CHECK(block.slots.size() == BisWire::SIM_SLOT_COUNT);
    for (int slot = 0; slot < BisWire::SIM_SLOT_COUNT; ++slot)
    {
        BisWire::Slot const& s = block.slots[slot];
        CHECK(s.simSlot == slot);
        CHECK(s.items == (U{uint32_t(40000 + slot), 41000, 42000, 43000, 44000, 45000}));
        CHECK(s.enchantSpell == 60000 && s.gems == (U{40111, 40117, 41398}));
        CHECK(s.reforgeFrom == 13 && s.reforgeTo == 31);
        CHECK(s.hasDelta && s.delta == slot * 7);
    }
}

void RejectsMalformed()
{
    char const* const rejects[] = {
        "", "0", "0:", ":1", "x:1", "17:1", "-1:1", "0:1,", "0:1,,2", "0:01", "0:+1", "0:1 ", "0:1,2,3,4,5,6,7",
        "0:1(", "0:1()", "0:1(e)", "0:1(e0)", "0:1(e1,e2)", "0:1(x1)", "0:1(r13)", "0:1(r13-31,r6-13)", "0:1(g-5)",
        "0:1(g2,e3)", "0:1(r13-31,g2)", "0:1(r13-31,e3)", "0:1,1",
        "0:1+", "0:1+x", "0:1+-0", "0:1+1+2", "0:1+5", "0:1;", "1:1;0:1", "0:1;0:2",
        // values that parse as numbers but aren't valid ids
        "0:0", "0:-5", "0:1(e-5)", "0:1(g0)", "0:1(r0-13)", "0:1(r13-0)", "0:1(r13--5)",
        // int32 bounds
        "0:2147483648", "0:1(e2147483648)", "0:1,2+2147483648", "0:1,2+-2147483649",
    };
    for (char const* payload : rejects)
    {
        BisWire::Block block;
        block.slots.emplace_back();
        std::string error;
        bool const decoded = BisWire::Decode(payload, block, error);
        if (decoded)
            std::fprintf(stderr, "decoded %s\n", payload);
        CHECK(!decoded);
        CHECK(!error.empty());
        CHECK(block.slots.empty());
    }
    BisWire::Block edge = MustDecode("0:2147483647,1+-2147483648");
    CHECK(edge.slots[0].items[0] == 2147483647u && edge.slots[0].delta == INT32_MIN);
}

void ChecksumIsAdler32()
{
    CHECK(BisWire::Checksum("Wikipedia") == "11e60398");
    CHECK(BisWire::Checksum("") == "00000001");
}

// Subject 1 of an acbis export: an Unholy DK in phase 3.
void DecodesRealExport()
{
    std::string const payload =
        "0:47943,48488,47674,47717,45472,49466(e59954,g41285,g40111)+-5;"
        "1:47110,47105,47060,47915,45459,46040(g40146,r31-36)+53;"
        "2:48486,47972,48485,48478,47697,47969(e61117,g40111,r31-36)+26;"
        "3:47547,48674,47545,47192,46971,48673(e55777,g40111,r31-36)+-49;"
        "4:47086,48490,47082,48481,46965,47004(e60692,g40129,g40111,g40146,r31-32)+71;"
        "5:46967,47155,47572,47935,45663,47074(e57683,g40146,g40111,r37-36)+-51;"
        "6:48489,48482,48480,47240,47917,47234(e54999,g40146,g40111,r31-36)+37;"
        "7:47002,47112,45241,47925,47614,46999(e54736,g40111,g40111,g40111,r37-36)+-331;"
        "8:47132,47121,46975,48484,45134,48487(e60584,g49110,g40111,g40146)+78;"
        "9:47154,45599,47077,47150,47933,47109(e55016,g40146,g40111,r37-36)+20;"
        "10:47920,45534,47729,47075,46959,47578(e44645,g40111,g42142)+-8;"
        "11:46966,47075,46959,47578,45534,47729(e44645,g40146)+38;"
        "12:45609,48722,47734,40531,47115,42987+-242;"
        "13:47131,48722,47734,47115,42987,40531+24;"
        "14:47001,47156,47973,47506,47971,47967(e53344,g42142,r32-36)+-146;"
        "15:47001,47526,47973,47156,47966,47506(e53341,g42142,r32-36)+-96;"
        "16:45254,45144,47672,47673+124";
    CHECK(BisWire::Checksum(payload) == "0e44db29");

    BisWire::Block block = MustDecode(payload);
    CHECK(block.slots.size() == 17);
    BisWire::Slot const* chest = block.Find(4);
    CHECK(chest && chest->items[0] == 47086 && chest->enchantSpell == 60692);
    CHECK(chest->gems == (U{40129, 40111, 40146}));
    CHECK(chest->reforgeFrom == 31 && chest->reforgeTo == 32);
    BisWire::Slot const* waist = block.Find(7);
    CHECK(waist && waist->gems == (U{40111, 40111, 40111}) && waist->delta == -331);
    BisWire::Slot const* trinket = block.Find(12);
    CHECK(trinket && trinket->enchantSpell == 0 && trinket->gems.empty() && trinket->reforgeFrom == 0);
    BisWire::Slot const* relic = block.Find(16);
    CHECK(relic && relic->items == (U{45254, 45144, 47672, 47673}) && relic->delta == 124);
}

void TablesMatchTheExport()
{
    CHECK(BisWire::EQUIP_SLOT[0] == 0);    // head
    CHECK(BisWire::EQUIP_SLOT[3] == 14);   // back
    CHECK(BisWire::EQUIP_SLOT[5] == 8);    // wrist
    CHECK(BisWire::EQUIP_SLOT[7] == 5);    // waist
    CHECK(BisWire::EQUIP_SLOT[14] == 15);  // main hand
    CHECK(BisWire::EQUIP_SLOT[16] == 17);  // ranged

    auto key = [](uint8_t classId, char const* spec, uint8_t tab, uint8_t variant)
    {
        std::optional<BisWire::SpecKey> found = BisWire::SpecKeyFor(classId, spec);
        return found && found->tab == tab && found->variant == variant;
    };
    CHECK(key(1, "Fury", 1, 0));
    CHECK(key(1, "Fury-Prot", BisWire::TAB_FURY_PROT, 0));
    CHECK(key(2, "Protection", 1, 0));
    CHECK(key(5, "Holy", 1, 0));
    CHECK(key(6, "Unholy", 2, 0));
    CHECK(key(6, "Blood tank", BisWire::TAB_BLOOD_TANK, 0));
    CHECK(key(8, "Fire", 1, 0));
    CHECK(key(8, "Fire FFB", 1, 1));
    CHECK(key(9, "Destruction fire", 2, 1));
    CHECK(key(11, "Feral tank", BisWire::TAB_FERAL_TANK, 0));
    CHECK(key(11, "Restoration", 2, 0));

    // every spec_name acbis writes
    struct Specs
    {
        uint8_t classId;
        std::vector<char const*> names;
    };
    Specs const exported[] = {
        {1, {"Arms", "Fury", "Fury-Prot", "Protection"}},
        {2, {"Holy", "Protection", "Retribution"}},
        {3, {"Beast mastery", "Marksmanship", "Survival"}},
        {4, {"Assassination", "Combat", "Subtlety"}},
        {5, {"Discipline", "Holy", "Shadow"}},
        {6, {"Blood dps", "Blood tank", "Frost", "Unholy"}},
        {7, {"Elemental", "Enhancement", "Restoration"}},
        {8, {"Arcane", "Fire", "Fire FFB", "Frost"}},
        {9, {"Affliction", "Demonology", "Destruction", "Destruction fire"}},
        {11, {"Balance", "Feral dps", "Feral tank", "Restoration"}},
    };
    for (Specs const& specs : exported)
        for (char const* name : specs.names)
        {
            if (!BisWire::SpecKeyFor(specs.classId, name))
                std::fprintf(stderr, "no key for class %u %s\n", unsigned(specs.classId), name);
            CHECK(BisWire::SpecKeyFor(specs.classId, name));
        }

    CHECK(!BisWire::SpecKeyFor(10, "Fury"));
    CHECK(!BisWire::SpecKeyFor(1, "Holy"));
    CHECK(!BisWire::SpecKeyFor(6, "Blood"));
    CHECK(!BisWire::SpecKeyFor(11, "feral tank"));
}

}  // namespace

int main()
{
    DecodesEveryField();
    RoundTrips();
    DecodesEverySlotFull();
    RejectsMalformed();
    ChecksumIsAdler32();
    DecodesRealExport();
    TablesMatchTheExport();
    std::puts("bis_wire_test: ok");
    return 0;
}
