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
using D = std::vector<int32_t>;

bool IsPairing(BisWire::Pairing const& pair, uint8_t rank, uint32_t item)
{
    return pair.rank == rank && pair.item == item;
}

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
        for (std::size_t i = 0; i < slot.pairings.size(); ++i)
            out += (i ? ",o" : "(o") + std::to_string(slot.pairings[i].rank) + "-" +
                   std::to_string(slot.pairings[i].item);
        if (!slot.pairings.empty())
            out += ")";
        for (std::size_t i = 0; i < slot.deltas.size(); ++i)
            out += (i ? "," : "+") + std::to_string(slot.deltas[i]);
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
        "0:51227,50712,50713,51866(e59954,g41398,g40111,r31-37)+4,21,37;1:50633;3:47545,51933(e47898)+-3;"
        "14:50730,50708,47520(e59621)(o2-50412,o3-0)+64,180;16:50733(r32-36)");
    CHECK(block.slots.size() == 5);

    BisWire::Slot const& head = block.slots[0];
    CHECK(head.simSlot == 0);
    CHECK(head.items == (U{51227, 50712, 50713, 51866}));
    CHECK(head.enchantSpell == 59954);
    CHECK(head.gems == (U{41398, 40111}));
    CHECK(head.reforgeFrom == 31 && head.reforgeTo == 37);
    CHECK(head.pairings.empty() && head.deltas == (D{4, 21, 37}));

    BisWire::Slot const& neck = block.slots[1];
    CHECK(neck.simSlot == 1 && neck.items == U{50633});
    CHECK(neck.enchantSpell == 0 && neck.gems.empty() && neck.reforgeFrom == 0 && neck.reforgeTo == 0);
    CHECK(neck.pairings.empty() && neck.deltas.empty());

    BisWire::Slot const& back = block.slots[2];
    CHECK(back.simSlot == 3 && back.items == (U{47545, 51933}) && back.enchantSpell == 47898);
    CHECK(back.deltas == D{-3});

    BisWire::Slot const& mainHand = block.slots[3];
    CHECK(mainHand.simSlot == 14 && mainHand.items == (U{50730, 50708, 47520}) && mainHand.enchantSpell == 59621);
    CHECK(mainHand.pairings.size() == 2);
    CHECK(IsPairing(mainHand.pairings[0], 2, 50412) && IsPairing(mainHand.pairings[1], 3, 0));
    CHECK(mainHand.deltas == (D{64, 180}));

    BisWire::Slot const& ranged = block.slots[4];
    CHECK(ranged.simSlot == 16 && ranged.items == U{50733});
    CHECK(ranged.enchantSpell == 0 && ranged.reforgeFrom == 32 && ranged.reforgeTo == 36);

    CHECK(block.Find(3) == &back);
    CHECK(block.Find(2) == nullptr);
}

void RoundTrips()
{
    CHECK(MustDecode("13:50363").slots[0].items == U{50363});
    CHECK(MustDecode("7:50620(g40125)").slots[0].gems == U{40125});

    CHECK(MustDecode("10:50402,50618+0").slots[0].deltas == D{0});
    CHECK(MustDecode("11:50402,50618+-12").slots[0].deltas == D{-12});
    CHECK(MustDecode("11:50402,50618,50619+-3,5").slots[0].deltas == (D{-3, 5}));

    BisWire::Slot pairedOnly = MustDecode("14:1,2,3(o3-0)").slots[0];
    CHECK(pairedOnly.enchantSpell == 0 && pairedOnly.deltas.empty());
    CHECK(pairedOnly.pairings.size() == 1 && IsPairing(pairedOnly.pairings[0], 3, 0));

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
                   ",41000,42000,43000,44000,45000(e60000,g40111,g40117,g41398,r13-31)(o2-46000,o6-0)+" +
                   std::to_string(slot * 7) + ",10,20,30,-40";
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
        CHECK(s.pairings.size() == 2 && IsPairing(s.pairings[0], 2, 46000) && IsPairing(s.pairings[1], 6, 0));
        CHECK(s.deltas == (D{slot * 7, 10, 20, 30, -40}));
    }
}

void RejectsMalformed()
{
    char const* const rejects[] = {
        "", "0", "0:", ":1", "x:1", "17:1", "-1:1", "0:1,", "0:1,,2", "0:01", "0:+1", "0:1 ", "0:1,2,3,4,5,6,7",
        "0:1(", "0:1()", "0:1(e)", "0:1(e0)", "0:1(e1,e2)", "0:1(x1)", "0:1(r13)", "0:1(r13-31,r6-13)", "0:1(g-5)",
        "0:1(g2,e3)", "0:1(r13-31,g2)", "0:1(r13-31,e3)", "0:1,1",
        "0:1+", "0:1+x", "0:1+-0", "0:1+1+2", "0:1+5", "0:1;", "1:1;0:1", "0:1;0:2",
        "0:1,2+1,", "0:1,2+,1", "0:1,2+1,,2", "0:1,2+-0", "0:1,2+01", "0:1,2+1 ", "0:1,2,3+1", "0:1,2+1,2",
        "0:1,2,3+1,2+3",
        // pairings: a rank outside 2..items, repeated or descending, malformed, twice over, or ahead
        // of the enhancements
        "14:1,2()", "14:1,2(o)", "14:1,2(o2)", "14:1,2(o2-)", "14:1,2(o1-5)", "14:1,2(o7-5)",
        "14:1,2,3(o3-5,o2-6)", "14:1,2,3(o2-5,o2-6)", "14:1(o2-5)", "14:1,2(o2-5)(e59621)",
        "14:1,2,3(o2-5)(o3-6)", "14:1,2(x2-5)", "14:1,2(o2--5)", "14:1,2(o-2-5)", "14:1,2(o02-5)",
        "14:1,2(o2-05)", "14:1,2(o2-5", "14:1,2(e59621,o2-5)", "14:1,2(o2-5,e59621)", "14:1,2(o2-5)+1+2",
        "14:1,2(e3)(e4)", "14:1,2)", "14:1,2(o2-2147483648)",
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
    CHECK(edge.slots[0].items[0] == 2147483647u && edge.slots[0].deltas == D{INT32_MIN});
}

void ChecksumIsAdler32()
{
    CHECK(BisWire::Checksum("Wikipedia") == "11e60398");
    CHECK(BisWire::Checksum("") == "00000001");
}

// Subject 8 of an acbis export: a Survival hunter in phase 2.
void DecodesRealExport()
{
    std::string const payload =
        "0:46143,45361,45610,45164,40543,40344(e59954,g35503,g42143,r32-31)+12,13,19,21,25;"
        "1:45517,45820,44664,44659,45480,40369(g42143)+6,7,10,11,12;"
        "2:46145,45363,45227,45543,45245,40315(e61117,g45879,r31-32)+5,9,12,13,16;"
        "3:45461,45224,40403,45242,45486,44005(e55777,g45882,r31-32)+7,9,13,18,18;"
        "4:46141,45364,45524,45473,40503,40277(e60692,g45879,g45879,r31-36)+8,10,20,22,26;"
        "5:45454,40282,45108,40736,45611,39702(e57683,g45879,r31-36)+4,6,7,7,7;"
        "6:45444,45109,45325,45836,40242,40504(e54999,g45879,g45879,g42143,r31-36)+13,14,14,19,19;"
        "7:45553,45467,45827,45555,45491,40275(g45879,g45879,g45879)+2,7,7,8,9;"
        "8:46144,45362,45143,45536,45844,45504(e60584,g45879,g45879)+6,8,8,14,15;"
        "9:45562,45249,40549,45244,45564,40367(e44589,g45879,g45879)+2,3,6,10,10;"
        "10:45608,46322,45503,45525,45688,40717(e44645,g45882)+3,5,8,9,10;"
        "11:45157,46322,40074,45614,45451,45495(e44645,r31-32)+2,6,14,15,16;"
        "12:44253,44255,45263,40256,42413,44254+2,7,11,12,13;"
        "13:45609,45263,42413,40256,40684,40431+4,8,9,11,12;"
        "14:45613,45498,45233,45484,45607,45533(e60691,g45879,g45879,r36-32)(o4-45484,o5-45484)+5,6,10,10,11;"
        "16:45570,45327,45261,40385,45137,40265(e55076,r31-36)+4,7,13,18,24";
    CHECK(BisWire::Checksum(payload) == "df44eabd");

    BisWire::Block block = MustDecode(payload);
    CHECK(block.slots.size() == 16);
    BisWire::Slot const* chest = block.Find(4);
    CHECK(chest && chest->items[0] == 46141 && chest->enchantSpell == 60692);
    CHECK(chest->gems == (U{45879, 45879}));
    CHECK(chest->reforgeFrom == 31 && chest->reforgeTo == 36);
    CHECK(chest->deltas == (D{8, 10, 20, 22, 26}));
    BisWire::Slot const* waist = block.Find(7);
    CHECK(waist && waist->enchantSpell == 0 && waist->gems == (U{45879, 45879, 45879}));
    BisWire::Slot const* trinket = block.Find(12);
    CHECK(trinket && trinket->enchantSpell == 0 && trinket->gems.empty() && trinket->reforgeFrom == 0);
    CHECK(trinket->items == (U{44253, 44255, 45263, 40256, 42413, 44254}));
    BisWire::Slot const* mainHand = block.Find(14);
    CHECK(mainHand && mainHand->pairings.size() == 2);
    CHECK(IsPairing(mainHand->pairings[0], 4, 45484) && IsPairing(mainHand->pairings[1], 5, 45484));
    CHECK(mainHand->deltas == (D{5, 6, 10, 10, 11}));
    CHECK(block.Find(15) == nullptr);
    BisWire::Slot const* ranged = block.Find(16);
    CHECK(ranged && ranged->enchantSpell == 55076 && ranged->deltas == (D{4, 7, 13, 18, 24}));
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
