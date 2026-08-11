// Regression tests for the GBA EEPROM state machine fields in a save state.
//
// eepromReadGame restores eepromMode, eepromByte, eepromBits and eepromAddress
// as raw ints. eepromAddress and eepromByte are then used as indices:
//
//     eepromData[(eepromAddress << 3) + eepromByte]
//     eepromBuffer[eepromByte]
//
// During emulation both are derived from command bits with explicit masking,
// so they cannot leave their buffers. Loading a state bypassed that, and since
// eepromMode is restored too, the bad access happens on the first EEPROM
// operation after the load rather than needing a command sequence first.

#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include "core/base/file_util.h"
#include "core/gba/gba.h"
#include "core/gba/gbaEeprom.h"
#include "core/gba/gbaGlobals.h"
#include "core/test/save_state_test_util.h"

// Defined in gba.cpp; eepromWrite bails out unless a DMA is in progress.
extern int cpuDmaCount;

// Defined in gbaEeprom.cpp but not declared in gbaEeprom.h.
extern int eepromMode;
extern int eepromByte;
extern int eepromBits;
extern int eepromAddress;
extern uint8_t eepromBuffer[16];

namespace {

// eepromData holds 1024 eight-byte words.
constexpr int kMaxEepromAddress = (SIZE_EEPROM_8K >> 3) - 1;

// The layout of eepromSaveData, followed by eepromSize and the EEPROM
// contents, which is what eepromReadGame reads for SAVE_GAME_VERSION_3+.
vbam_test::StateBlob MakeEepromState(int mode, int byte, int bits, int address,
    uint8_t data_fill)
{
    vbam_test::StateBlob blob;
    blob.Add<int>(mode);
    blob.Add<int>(byte);
    blob.Add<int>(bits);
    blob.Add<int>(address);
    blob.Add<bool>(true);            // eepromInUse
    blob.AddFill(SIZE_EEPROM_512);   // eepromData prefix in eepromSaveData
    blob.AddFill(16);                // eepromBuffer
    blob.Add<int>(SIZE_EEPROM_8K);   // eepromSize
    blob.AddFill(SIZE_EEPROM_8K, data_fill);
    return blob;
}

class GbaEepromSaveStateTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        eepromInit();
        eepromReset();
        eepromSetSize(SIZE_EEPROM_8K);
        // eepromWrite bails out unless a DMA is in progress.
        cpuDmaCount = 0x11;
    }

    void TearDown() override { cpuDmaCount = 0; }

    void LoadState(const vbam_test::StateBlob& blob, const char* name)
    {
        const vbam_test::TempStateFile file(name);
        ASSERT_TRUE(file.Write(blob));

        gzFile in = file.OpenRead();
        ASSERT_NE(in, nullptr);
        eepromReadGame(in, SAVE_GAME_VERSION_11);
        utilGzClose(in);
    }
};

TEST_F(GbaEepromSaveStateTest, ValidAddressesSurviveLoad)
{
    for (int address : { 0, 1, 63, 512, kMaxEepromAddress }) {
        LoadState(MakeEepromState(EEPROM_IDLE, 0, 0, address, 0x00),
            "gba_eeprom_valid_addr");
        ASSERT_EQ(eepromAddress, address);
    }
}

// Address 1024 puts (eepromAddress << 3) exactly at the end of eepromData;
// 0x10000 puts it 512 KiB past.
TEST_F(GbaEepromSaveStateTest, HostileAddressIsMasked)
{
    for (int address : { 1024, 1025, 0x10000, 0x7FFFFFFF, -1, -1024 }) {
        LoadState(MakeEepromState(EEPROM_IDLE, 0, 0, address, 0x00),
            "gba_eeprom_hostile_addr");
        ASSERT_GE(eepromAddress, 0) << "state address " << address;
        ASSERT_LE(eepromAddress, kMaxEepromAddress)
            << "state address " << address;
    }
}

// eepromByte is added to the word address, so it is part of the same index
// expression and needs bounding too.
// 0 through 8 are what the protocol produces; anything above is not.
TEST_F(GbaEepromSaveStateTest, HostileByteOffsetIsReset)
{
    for (int byte : { 9, 16, 4096, 0x7FFFFFFF, -1 }) {
        LoadState(MakeEepromState(EEPROM_IDLE, byte, 0, 0, 0x00),
            "gba_eeprom_hostile_byte");
        ASSERT_GE(eepromByte, 0) << "state byte " << byte;
        ASSERT_LE(eepromByte, 8) << "state byte " << byte;
    }
}

// The largest word address combined with the largest byte offset composes an
// index one past eepromData. A crafted state can pair them even though the
// state machine never does.
TEST_F(GbaEepromSaveStateTest, ReadAtMaxAddressAndByteStaysInBuffer)
{
    LoadState(MakeEepromState(EEPROM_READDATA2, 8, 0, kMaxEepromAddress, 0x00),
        "gba_eeprom_max_index");

    ASSERT_EQ(eepromAddress, kMaxEepromAddress);
    ASSERT_EQ(eepromByte, 8);

    // (1023 << 3) + 8 == 8192 == SIZE_EEPROM_8K. Reading must not touch it.
    for (int i = 0; i < 8; i++)
        ASSERT_EQ(eepromRead(0), 0) << "bit " << i;
}

// eepromByte legitimately reaches 8. In the WRITEDATA phase eepromBits runs
// from 1 to 0x41 and eepromByte advances every eighth bit, so it is 8 for the
// last step of a 64-bit transfer -- the step that commits eepromBuffer to
// eepromData. A save state taken there must reload with the counter intact,
// or the in-flight battery write resumes at the wrong byte.
TEST_F(GbaEepromSaveStateTest, ByteOffsetEightSurvivesLoad)
{
    LoadState(MakeEepromState(EEPROM_WRITEDATA, 8, 0x40, 4, 0x00),
        "gba_eeprom_byte_eight");

    EXPECT_EQ(eepromByte, 8);
}

// The same value has to survive a full save/load round trip of a transfer
// driven through the real state machine, not just a synthetic blob.
TEST_F(GbaEepromSaveStateTest, InFlightWriteTransferSurvivesRoundTrip)
{
    // Drive a real 8 KiB write command: 17 bits of "1 0 <14-bit address>".
    // Bit 1 clear selects a write; the address bits below encode word 0x105,
    // which is inside the 1024 words an 8 KiB EEPROM holds -- an address a
    // game would actually issue.
    static const int kCommand[0x11] = {
        1, 0,                          // write
        0, 0, 0, 0, 0, 1,              // address bits 13..8  -> 0x01
        0, 0, 0, 0, 0, 1, 0, 1,        // address bits  7..0  -> 0x05
        0,                             // trailing bit
    };

    eepromReset();
    eepromMode = EEPROM_IDLE;
    for (int i = 0; i < 0x11; i++)
        eepromWrite(0, static_cast<uint8_t>(kCommand[i]));

    ASSERT_EQ(eepromMode, EEPROM_WRITEDATA);
    ASSERT_EQ(eepromAddress, 0x105);

    while (eepromBits < 0x40)
        eepromWrite(0, 1);

    const int mode_before = eepromMode;
    const int byte_before = eepromByte;
    const int bits_before = eepromBits;
    const int address_before = eepromAddress;

    const vbam_test::TempStateFile file("gba_eeprom_inflight");
    gzFile out = file.OpenWrite();
    ASSERT_NE(out, nullptr);
    eepromSaveGame(out);
    utilGzClose(out);

    eepromReset();

    gzFile in = file.OpenRead();
    ASSERT_NE(in, nullptr);
    eepromReadGame(in, SAVE_GAME_VERSION_11);
    utilGzClose(in);

    EXPECT_EQ(eepromMode, mode_before);
    EXPECT_EQ(eepromByte, byte_before);
    EXPECT_EQ(eepromBits, bits_before);
    EXPECT_EQ(eepromAddress, address_before);
}

TEST_F(GbaEepromSaveStateTest, HostileBitCountIsReset)
{
    for (int bits : { 0x42, 0x1000, 0x7FFFFFFF, -1 }) {
        LoadState(MakeEepromState(EEPROM_IDLE, 0, bits, 0, 0x00),
            "gba_eeprom_hostile_bits");
        ASSERT_GE(eepromBits, 0) << "state bits " << bits;
        ASSERT_LE(eepromBits, 0x41) << "state bits " << bits;
    }
}

TEST_F(GbaEepromSaveStateTest, HostileModeFallsBackToIdle)
{
    for (int mode : { 5, 99, -1, 0x7FFFFFFF }) {
        LoadState(MakeEepromState(mode, 0, 0, 0, 0x00),
            "gba_eeprom_hostile_mode");
        ASSERT_GE(eepromMode, EEPROM_IDLE) << "state mode " << mode;
        ASSERT_LE(eepromMode, EEPROM_WRITEDATA) << "state mode " << mode;
    }
}

// The out-of-bounds read the report describes: a state parked in
// EEPROM_READDATA2 with an address past the buffer, read immediately.
TEST_F(GbaEepromSaveStateTest, ReadAfterHostileAddressStaysInBuffer)
{
    LoadState(MakeEepromState(EEPROM_READDATA2, 0, 0, 0x10000, 0x00),
        "gba_eeprom_read_oob");

    // Every byte of eepromData is 0, so any bit read back as 1 came from
    // outside the buffer.
    for (int i = 0; i < 64; i++)
        ASSERT_EQ(eepromRead(0), 0) << "bit " << i;
}

// The out-of-bounds write: a state parked one bit short of the end of a write
// transfer, so the next eepromWrite commits eepromBuffer to eepromData.
TEST_F(GbaEepromSaveStateTest, WriteAfterHostileAddressStaysInBuffer)
{
    LoadState(MakeEepromState(EEPROM_WRITEDATA, 0, 0x3F, 0x10000, 0x00),
        "gba_eeprom_write_oob");

    ASSERT_LE(eepromAddress, kMaxEepromAddress);
    const int target = eepromAddress << 3;

    memset(eepromBuffer, 0xC3, sizeof(eepromBuffer));
    eepromWrite(0, 1);

    // The commit must have landed inside eepromData, at the masked address.
    ASSERT_LE(target + 8, SIZE_EEPROM_8K);
    for (int i = 0; i < 8; i++)
        EXPECT_EQ(eepromData[target + i], eepromBuffer[i]) << "byte " << i;
}

}  // namespace
