// Regression tests for the CGB bank indices taken from a save state.
//
// gbReadSaveState restores register_VBK and register_SVBK as raw bytes and
// used them directly as bank numbers when rebuilding gbMemoryMap. The register
// write handlers mask them (VBK to 1 bit, SVBK to 3), but the save state path
// did not, so a crafted state pointed the map entries for 0x8000-0x9fff and
// 0xd000-0xdfff outside the VRAM/WRAM allocations. Every later CPU access
// through those ranges then read or wrote out of bounds.
//
// gbApplySaveStateBanks() is the code the loader runs; driving it directly
// keeps the test free of ROM and emulator setup.

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "core/base/sizes.h"
#include "core/gb/gbGlobals.h"

// Defined in gb.cpp but not declared in gbGlobals.h.
extern uint8_t register_SVBK;

namespace {

class GbBankSaveStateTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        saved_vram_ = gbVram;
        saved_wram_ = gbWram;

        vram_.assign(kGBVRamSize, 0);
        wram_.assign(kGBWRamSize, 0);

        gbVram = vram_.data();
        gbWram = wram_.data();
    }

    void TearDown() override
    {
        gbVram = saved_vram_;
        gbWram = saved_wram_;
    }

    // gbMemoryMap entries address 4 KiB pages, so a page is in bounds only if
    // both it and the following 0x1000 bytes lie inside the buffer.
    bool VramPageInBounds(int entry) const
    {
        const uint8_t* p = gbMemoryMap[entry];
        return p >= vram_.data() && p + 0x1000 <= vram_.data() + vram_.size();
    }

    bool WramPageInBounds(int entry) const
    {
        const uint8_t* p = gbMemoryMap[entry];
        return p >= wram_.data() && p + 0x1000 <= wram_.data() + wram_.size();
    }

    std::vector<uint8_t> vram_;
    std::vector<uint8_t> wram_;
    uint8_t* saved_vram_ = nullptr;
    uint8_t* saved_wram_ = nullptr;
};

// 0 and 1 are the only values the VBK register can hold, and both must keep
// selecting the bank they always did.
TEST_F(GbBankSaveStateTest, ValidVramBanksAreUnchanged)
{
    register_SVBK = 1;

    register_VBK = 0;
    gbApplySaveStateBanks();
    EXPECT_EQ(gbMemoryMap[0x08], vram_.data());
    EXPECT_EQ(gbMemoryMap[0x09], vram_.data() + 0x1000);

    register_VBK = 1;
    gbApplySaveStateBanks();
    EXPECT_EQ(gbMemoryMap[0x08], vram_.data() + 0x2000);
    EXPECT_EQ(gbMemoryMap[0x09], vram_.data() + 0x3000);
}

// register_VBK = 2 puts the map entry exactly at the end of the 16 KiB
// buffer; 255 puts it roughly 2 MiB past it.
TEST_F(GbBankSaveStateTest, HostileVramBankStaysInsideVram)
{
    register_SVBK = 1;

    for (int bank = 2; bank <= 255; bank++) {
        register_VBK = static_cast<uint8_t>(bank);
        gbApplySaveStateBanks();

        ASSERT_TRUE(VramPageInBounds(0x08))
            << "register_VBK=" << bank << " escaped gbVram at map entry 0x08";
        ASSERT_TRUE(VramPageInBounds(0x09))
            << "register_VBK=" << bank << " escaped gbVram at map entry 0x09";
    }
}

// The masked bank must match what the VBK register write handler would have
// produced for the same byte, so a state written by a correct emulator round
// trips unchanged.
TEST_F(GbBankSaveStateTest, VramBankMatchesRegisterWriteMasking)
{
    register_SVBK = 1;

    for (int value = 0; value <= 255; value++) {
        register_VBK = static_cast<uint8_t>(value);
        gbApplySaveStateBanks();

        const int expected = value & 1;
        ASSERT_EQ(gbMemoryMap[0x08], vram_.data() + expected * 0x2000)
            << "register_VBK=" << value;
    }
}

}  // namespace
