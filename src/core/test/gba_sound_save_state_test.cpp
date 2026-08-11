// Regression tests for the GBA PCM FIFO indices in a save state.
//
// soundReadGame restores pcm[n].readIndex and pcm[n].writeIndex as raw ints.
// They index a 32-byte ring, and emulation keeps them in range by re-masking
// after each increment -- but the mask is applied *after* the access:
//
//     dac = fifo[readIndex];              // then readIndex = (...) & 31
//     fifo[writeIndex] = data & 0xFF;     // then writeIndex = (...) & 31
//
// so the first audio operation after a load used the unmasked value. The
// second write is worse: fifo[writeIndex + 1] runs one byte past the ring even
// for writeIndex = 31, which is in range.
//
// pcm[] is file-static, so these tests observe the sanitized values by writing
// the state back out and reading the fields out of the blob.

#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include "core/base/file_util.h"
#include "core/gba/gba.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaSound.h"
#include "core/test/save_state_test_util.h"

// Defined in gbaSound.cpp but not declared in gbaSound.h. Brings up the APU
// and mixing buffers, which is all soundSaveGame/soundReadGame need. Going
// through soundInit() instead is not an option here: the fake core's
// systemSoundInit() returns no driver, so soundInit() fails before it gets
// this far.
extern void remake_stereo_buffer();

namespace {

// Field offsets within gba_state, which starts with the two Gba_Pcm_Fifo
// blocks: readIndex, count, writeIndex, fifo[32], dac, then 4 ints of
// expansion room.
constexpr size_t kPcm0ReadIndex = 0;
constexpr size_t kPcm0Count = 4;
constexpr size_t kPcm0WriteIndex = 8;
// dac is the member directly after fifo[32], so it is what a one-byte overrun
// of the ring lands in.
constexpr size_t kPcm0Dac = 8 + 4 + 32;
constexpr size_t kPcm1Base = 8 + 4 + 32 + 4 + 16;
constexpr size_t kPcm1ReadIndex = kPcm1Base;
constexpr size_t kPcm1Count = kPcm1Base + 4;
constexpr size_t kPcm1WriteIndex = kPcm1Base + 8;

class GbaSoundSaveStateTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        // soundReadGame reads SGCNT0_H out of the I/O register block, and
        // remake_stereo_buffer() refuses to run without it.
        if (g_ioMem == nullptr)
            g_ioMem = static_cast<uint8_t*>(calloc(1, 0x400));
        ASSERT_NE(g_ioMem, nullptr);
    }

    void SetUp() override { remake_stereo_buffer(); }

    // Produce a real save state, overwrite the FIFO index fields in it, load
    // it back, then save again and return the round-tripped blob.
    vbam_test::StateBlob RoundTrip(int read_index, int write_index,
        int count, const char* name, int dac = 0)
    {
        const vbam_test::TempStateFile file(name);

        gzFile out = file.OpenWrite();
        EXPECT_NE(out, nullptr);
        soundSaveGame(out);
        utilGzClose(out);

        vbam_test::StateBlob blob = file.ReadAll();
        EXPECT_GT(blob.size(), kPcm1WriteIndex + sizeof(int));

        blob.PatchAt<int>(kPcm0ReadIndex, read_index);
        blob.PatchAt<int>(kPcm0WriteIndex, write_index);
        blob.PatchAt<int>(kPcm0Count, count);
        blob.PatchAt<int>(kPcm0Dac, dac);
        blob.PatchAt<int>(kPcm1ReadIndex, read_index);
        blob.PatchAt<int>(kPcm1WriteIndex, write_index);
        blob.PatchAt<int>(kPcm1Count, count);

        const vbam_test::TempStateFile hostile(
            (std::string(name) + "_hostile").c_str());
        EXPECT_TRUE(hostile.Write(blob));

        gzFile in = hostile.OpenRead();
        EXPECT_NE(in, nullptr);
        soundReadGame(in, SAVE_GAME_VERSION_11);
        utilGzClose(in);

        // Write the now-loaded state back out to observe what was kept.
        const vbam_test::TempStateFile after(
            (std::string(name) + "_after").c_str());
        gzFile out2 = after.OpenWrite();
        EXPECT_NE(out2, nullptr);
        soundSaveGame(out2);
        utilGzClose(out2);

        return after.ReadAll();
    }
};

// A state written by a correct emulator must survive untouched.
TEST_F(GbaSoundSaveStateTest, ValidIndicesSurviveLoad)
{
    const vbam_test::StateBlob after = RoundTrip(7, 30, 8, "gba_sound_valid");

    EXPECT_EQ(after.ReadAt<int>(kPcm0ReadIndex), 7);
    EXPECT_EQ(after.ReadAt<int>(kPcm0WriteIndex), 30);
    EXPECT_EQ(after.ReadAt<int>(kPcm1ReadIndex), 7);
    EXPECT_EQ(after.ReadAt<int>(kPcm1WriteIndex), 30);
}

TEST_F(GbaSoundSaveStateTest, HostileIndicesAreMasked)
{
    const vbam_test::StateBlob after =
        RoundTrip(0x7FFFFFFF, 0x7FFFFFFF, 32, "gba_sound_hostile");

    for (size_t offset : { kPcm0ReadIndex, kPcm0WriteIndex, kPcm1ReadIndex,
             kPcm1WriteIndex }) {
        const int value = after.ReadAt<int>(offset);
        EXPECT_GE(value, 0) << "field at " << offset;
        EXPECT_LE(value, 31) << "field at " << offset;
    }
}

TEST_F(GbaSoundSaveStateTest, NegativeIndicesAreMasked)
{
    const vbam_test::StateBlob after =
        RoundTrip(-1, -12345, 16, "gba_sound_negative");

    for (size_t offset : { kPcm0ReadIndex, kPcm0WriteIndex, kPcm1ReadIndex,
             kPcm1WriteIndex }) {
        const int value = after.ReadAt<int>(offset);
        EXPECT_GE(value, 0) << "field at " << offset;
        EXPECT_LE(value, 31) << "field at " << offset;
    }
}

// The FIFO writes themselves must stay inside the ring. writeIndex = 31 is a
// value the load-time mask alone permits, yet write_fifo also stores at
// writeIndex + 1, which is byte 32 -- one past the end.
TEST_F(GbaSoundSaveStateTest, FifoWriteAtRingEndDoesNotRunPastIt)
{
    constexpr int kDacSentinel = 0x11223344;

    const vbam_test::StateBlob after =
        RoundTrip(0, 31, 0, "gba_sound_write_end", kDacSentinel);

    ASSERT_EQ(after.ReadAt<int>(kPcm0WriteIndex), 31);
    ASSERT_EQ(after.ReadAt<int>(kPcm0Dac), kDacSentinel);

    // Drive a FIFO write through the register the DMA path uses.
    soundEvent16(FIFOA_L, static_cast<uint16_t>(0xCCDD));

    const vbam_test::TempStateFile file("gba_sound_write_end_check");
    gzFile out = file.OpenWrite();
    ASSERT_NE(out, nullptr);
    soundSaveGame(out);
    utilGzClose(out);

    const vbam_test::StateBlob blob = file.ReadAll();

    // The second byte must have wrapped to fifo[0] rather than running into
    // dac, which is the next member of the struct.
    EXPECT_EQ(blob.ReadAt<int>(kPcm0Dac), kDacSentinel)
        << "write_fifo ran one byte past the ring and corrupted dac";

    const int write_index = blob.ReadAt<int>(kPcm0WriteIndex);
    EXPECT_GE(write_index, 0);
    EXPECT_LE(write_index, 31);
}

}  // namespace
