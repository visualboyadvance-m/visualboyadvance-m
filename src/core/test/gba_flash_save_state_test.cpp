// Regression tests for the GBA flash save state fields.
//
// flashReadGame restores flashState, flashReadState, g_flashSize and flashBank
// as raw ints. flashBank indexes flashSaveMemory in 64 KiB steps and
// g_flashSize is the length passed to memset on a chip erase, so both are
// memory-safety relevant and neither was validated. The command handlers that
// set them during emulation do validate: FLASH_SETBANK masks the bank to one
// bit, and g_flashSize only ever comes from ROM detection.

#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include "core/base/file_util.h"
#include "core/gba/gba.h"
#include "core/gba/gbaFlash.h"
#include "core/test/save_state_test_util.h"

// Defined in gbaFlash.cpp but not declared in gbaFlash.h.
extern int flashState;
extern int flashReadState;
extern int flashBank;

namespace {

constexpr int kFlashReadArray = 0;
constexpr int kFlashCmd5 = 6;

// The layout of flashSaveData3, which is what flashReadGame reads for
// SAVE_GAME_VERSION_7 and later.
vbam_test::StateBlob MakeFlashState(int state, int read_state, int size,
    int bank, uint8_t memory_fill)
{
    vbam_test::StateBlob blob;
    blob.Add<int>(state);
    blob.Add<int>(read_state);
    blob.Add<int>(size);
    blob.Add<int>(bank);
    blob.AddFill(SIZE_FLASH1M, memory_fill);
    return blob;
}

class GbaFlashSaveStateTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        flashInit();
        flashReset();
        flashSetSize(SIZE_FLASH1M);
    }

    // Load a crafted state through the real deserializer.
    void LoadState(const vbam_test::StateBlob& blob, const char* name)
    {
        const vbam_test::TempStateFile file(name);
        ASSERT_TRUE(file.Write(blob));

        gzFile in = file.OpenRead();
        ASSERT_NE(in, nullptr);
        flashReadGame(in, SAVE_GAME_VERSION_11);
        utilGzClose(in);
    }
};

TEST_F(GbaFlashSaveStateTest, ValidBanksSurviveLoad)
{
    for (int bank = 0; bank <= 1; bank++) {
        LoadState(MakeFlashState(kFlashReadArray, kFlashReadArray,
                      SIZE_FLASH1M, bank, 0x00),
            "gba_flash_valid_bank");
        ASSERT_EQ(flashBank, bank);
    }
}

// Bank 2 puts (flashBank << 16) exactly at the end of flashSaveMemory; 127
// puts it around 8 MiB past.
TEST_F(GbaFlashSaveStateTest, HostileBankIsMasked)
{
    for (int bank : { 2, 3, 127, 0x7FFF, 0x7FFFFFFF }) {
        LoadState(MakeFlashState(kFlashReadArray, kFlashReadArray,
                      SIZE_FLASH1M, bank, 0x00),
            "gba_flash_hostile_bank");
        ASSERT_GE(flashBank, 0) << "bank " << bank;
        ASSERT_LE(flashBank, 1) << "bank " << bank;
    }
}

// flashBank is an int, so a negative value indexed backwards out of the
// buffer just as readily as a large one indexed forwards.
TEST_F(GbaFlashSaveStateTest, NegativeBankIsMasked)
{
    for (int bank : { -1, -2, -65536, -0x7FFFFFFF }) {
        LoadState(MakeFlashState(kFlashReadArray, kFlashReadArray,
                      SIZE_FLASH1M, bank, 0x00),
            "gba_flash_negative_bank");
        ASSERT_GE(flashBank, 0) << "bank " << bank;
        ASSERT_LE(flashBank, 1) << "bank " << bank;
    }
}

// The reads that follow a load must land inside flashSaveMemory. Filling the
// buffer with a known byte makes an out-of-bounds read visible without
// depending on a sanitizer being enabled.
TEST_F(GbaFlashSaveStateTest, ReadAfterHostileBankStaysInBuffer)
{
    LoadState(MakeFlashState(kFlashReadArray, kFlashReadArray, SIZE_FLASH1M,
                  0x4242, 0x5A),
        "gba_flash_read_after_load");

    EXPECT_EQ(flashRead(0x0000), 0x5A);
    EXPECT_EQ(flashRead(0xFFFF), 0x5A);
}

// A crafted state can pre-position the state machine so the next flash write
// erases immediately, without the usual 5-step command sequence. That is
// legitimate as far as the emulator goes -- what matters is that the erase
// stays inside the buffer.
TEST_F(GbaFlashSaveStateTest, SectorEraseAfterHostileBankStaysInBuffer)
{
    LoadState(MakeFlashState(kFlashCmd5, kFlashReadArray, SIZE_FLASH1M,
                  0x0100, 0x11),
        "gba_flash_erase_after_load");

    // 0x30 is SECTOR ERASE, which memsets 4 KiB at (flashBank << 16) +
    // (address & 0xF000).
    flashWrite(0xF000, 0x30);

    EXPECT_LE(flashBank, 1);
    // The erase must have hit one of the two real banks, not memory past them.
    const bool erased_bank0 = flashSaveMemory[0xF000] == 0xFF;
    const bool erased_bank1 = flashSaveMemory[0x1F000] == 0xFF;
    EXPECT_TRUE(erased_bank0 || erased_bank1);
}

// g_flashSize is the CHIP ERASE memset length. Only the two real chip sizes
// exist, and both must survive a load unchanged.
TEST_F(GbaFlashSaveStateTest, ValidFlashSizesSurviveLoad)
{
    for (int size : { SIZE_FLASH512, SIZE_FLASH1M }) {
        LoadState(MakeFlashState(kFlashReadArray, kFlashReadArray, size, 0,
                      0x00),
            "gba_flash_valid_size");
        ASSERT_EQ(g_flashSize, size);
    }
}

TEST_F(GbaFlashSaveStateTest, HostileFlashSizeIsRejected)
{
    for (int size : { -1, 0, 1, SIZE_FLASH1M + 1, 0x00100000, 0x7FFFFFFF }) {
        LoadState(MakeFlashState(kFlashReadArray, kFlashReadArray, size, 0,
                      0x00),
            "gba_flash_hostile_size");
        ASSERT_TRUE(g_flashSize == SIZE_FLASH512 || g_flashSize == SIZE_FLASH1M)
            << "state size " << size << " produced g_flashSize " << g_flashSize;
    }
}

// The overflow itself: a crafted state sets flashState to FLASH_CMD_5 so the
// next flash write of 0x10 runs CHIP ERASE with the attacker's length. Guard
// pages either side of flashSaveMemory are not available here, so assert on
// the sanitized length -- with 0x7FFFFFFF this memset ran ~2 GiB.
TEST_F(GbaFlashSaveStateTest, ChipEraseAfterHostileSizeStaysInBuffer)
{
    LoadState(MakeFlashState(kFlashCmd5, kFlashReadArray, 0x7FFFFFFF, 0, 0x11),
        "gba_flash_chip_erase");

    ASSERT_LE(g_flashSize, SIZE_FLASH1M);
    const int erase_length = g_flashSize;

    // 0x10 is CHIP ERASE: memset(flashSaveMemory, 0xff, g_flashSize).
    flashWrite(0x0000, 0x10);

    // Erased up to the sanitized length...
    EXPECT_EQ(flashSaveMemory[0], 0xFF);
    EXPECT_EQ(flashSaveMemory[erase_length - 1], 0xFF);

    // ...and not one byte beyond it. Anything past the buffer would have run
    // off the end of the BSS object entirely.
    if (erase_length < SIZE_FLASH1M)
        EXPECT_EQ(flashSaveMemory[erase_length], 0x11);
}

// Pre-version-7 states route their stored size through flashSetSize(), which
// stored it unconditionally.
TEST_F(GbaFlashSaveStateTest, FlashSetSizeRejectsHostileSizes)
{
    for (int size : { -1, 0, 0x7FFFFFFF }) {
        flashSetSize(size);
        ASSERT_TRUE(g_flashSize == SIZE_FLASH512 || g_flashSize == SIZE_FLASH1M)
            << "flashSetSize(" << size << ") stored " << g_flashSize;
    }

    flashSetSize(SIZE_FLASH1M);
    EXPECT_EQ(g_flashSize, SIZE_FLASH1M);
    flashSetSize(SIZE_FLASH512);
    EXPECT_EQ(g_flashSize, SIZE_FLASH512);
}

}  // namespace
