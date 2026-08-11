// Regression tests for the GB cheat save state deserialization overflow.
//
// gbCheatsReadGame's legacy (version <= 8) branch reads gbXxCheat records,
// whose cheatDesc is 100 bytes, and hands them to gbAddGgCheat/gbAddGsCheat,
// which used to strcpy them into gbCheat::cheatDesc -- 32 bytes. A crafted
// save state therefore wrote up to 68 attacker-controlled bytes past the field
// and on through the rest of the global gbCheatList array.

#include <cstring>
#include <string>

#include <gtest/gtest.h>

#include "core/base/file_util.h"
#include "core/gb/gbCheats.h"
#include "core/test/save_state_test_util.h"

namespace {

// Valid per gbVerifyGgCode: 3 hex, '-', 3 hex, decoding to an address outside
// the 0x8000-0x9fff and >= 0xc000 ranges it rejects.
constexpr char kValidGgCode[] = "01A-B0F";
// Valid per gbVerifyGsCode: exactly 8 hex digits.
constexpr char kValidGsCode[] = "01FF00C0";

class GbCheatSaveStateTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        gbCheatRemoveAll();
        // Fill the entry the overflow would run into with a known pattern so
        // the test detects the corruption without needing a sanitizer.
        memset(&gbCheatList[1], kCanary, sizeof(gbCheatList[1]));
    }

    void TearDown() override { gbCheatRemoveAll(); }

    static bool CanaryIntact()
    {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&gbCheatList[1]);
        for (size_t i = 0; i < sizeof(gbCheatList[1]); i++) {
            if (p[i] != kCanary)
                return false;
        }
        return true;
    }

    static constexpr uint8_t kCanary = 0xA5;
};

TEST_F(GbCheatSaveStateTest, GgCheatDescriptionIsTruncatedNotOverflowed)
{
    const std::string desc(sizeof(gbXxCheat::cheatDesc) - 1, 'A');

    ASSERT_TRUE(gbAddGgCheat(kValidGgCode, desc.c_str()));

    EXPECT_LT(strlen(gbCheatList[0].cheatDesc), sizeof(gbCheatList[0].cheatDesc));
    EXPECT_EQ(gbCheatList[0].cheatDesc[sizeof(gbCheatList[0].cheatDesc) - 1], '\0');
    EXPECT_TRUE(CanaryIntact()) << "overflowed into the next gbCheatList entry";
}

TEST_F(GbCheatSaveStateTest, GsCheatDescriptionIsTruncatedNotOverflowed)
{
    const std::string desc(sizeof(gbXxCheat::cheatDesc) - 1, 'B');

    ASSERT_TRUE(gbAddGsCheat(kValidGsCode, desc.c_str()));

    EXPECT_LT(strlen(gbCheatList[0].cheatDesc), sizeof(gbCheatList[0].cheatDesc));
    EXPECT_EQ(gbCheatList[0].cheatDesc[sizeof(gbCheatList[0].cheatDesc) - 1], '\0');
    EXPECT_TRUE(CanaryIntact()) << "overflowed into the next gbCheatList entry";
}

// The description that survives truncation must still be the leading bytes of
// what was asked for, so ordinary cheat names keep working.
TEST_F(GbCheatSaveStateTest, ShortDescriptionIsPreservedExactly)
{
    ASSERT_TRUE(gbAddGgCheat(kValidGgCode, "Infinite lives"));

    EXPECT_STREQ(gbCheatList[0].cheatDesc, "Infinite lives");
    EXPECT_STREQ(gbCheatList[0].cheatCode, kValidGgCode);
}

// End to end: a crafted legacy save state, exactly as gbCheatsReadGame reads
// it -- gbGgOn, record count, then the gbXxCheat records themselves.
TEST_F(GbCheatSaveStateTest, CraftedLegacyStateDoesNotOverflow)
{
    gbXxCheat hostile;
    memset(&hostile, 'C', sizeof(hostile));
    memcpy(hostile.cheatCode, kValidGgCode, sizeof(kValidGgCode));
    hostile.cheatDesc[sizeof(hostile.cheatDesc) - 1] = '\0';

    vbam_test::StateBlob blob;
    blob.Add<int>(1);          // gbGgOn
    blob.Add<int>(1);          // record count
    blob.AddBytes(&hostile, sizeof(hostile));
    blob.Add<int>(0);          // gbGsOn

    const vbam_test::TempStateFile file("gb_cheats_legacy");
    ASSERT_TRUE(file.Write(blob));

    gzFile in = file.OpenRead();
    ASSERT_NE(in, nullptr);
    gbCheatsReadGame(in, 8);
    utilGzClose(in);

    ASSERT_EQ(gbCheatNumber, 1);
    EXPECT_LT(strlen(gbCheatList[0].cheatDesc), sizeof(gbCheatList[0].cheatDesc));
    EXPECT_TRUE(CanaryIntact()) << "crafted save state overflowed gbCheatList";
}

// A record count larger than gbCheatList must not be walked. Before the fix
// the loop ran the full attacker-supplied count, each iteration hitting EOF
// and re-adding the last record read.
TEST_F(GbCheatSaveStateTest, CraftedLegacyStateRecordCountIsClamped)
{
    gbXxCheat record;
    memset(&record, 0, sizeof(record));
    memcpy(record.cheatCode, kValidGgCode, sizeof(kValidGgCode));
    strcpy(record.cheatDesc, "cheat");

    vbam_test::StateBlob blob;
    blob.Add<int>(1);            // gbGgOn
    blob.Add<int>(0x7FFFFFFF);   // record count: absurd
    blob.AddBytes(&record, sizeof(record));
    blob.Add<int>(0);            // gbGsOn

    const vbam_test::TempStateFile file("gb_cheats_count");
    ASSERT_TRUE(file.Write(blob));

    gzFile in = file.OpenRead();
    ASSERT_NE(in, nullptr);
    gbCheatsReadGame(in, 8);
    utilGzClose(in);

    EXPECT_LE(gbCheatNumber, MAX_CHEATS);
}

// A negative count must not be treated as "read forever" either.
TEST_F(GbCheatSaveStateTest, CraftedLegacyStateNegativeCountAddsNothing)
{
    vbam_test::StateBlob blob;
    blob.Add<int>(1);    // gbGgOn
    blob.Add<int>(-1);   // record count
    blob.Add<int>(0);    // gbGsOn

    const vbam_test::TempStateFile file("gb_cheats_negative");
    ASSERT_TRUE(file.Write(blob));

    gzFile in = file.OpenRead();
    ASSERT_NE(in, nullptr);
    gbCheatsReadGame(in, 8);
    utilGzClose(in);

    EXPECT_EQ(gbCheatNumber, 0);
    EXPECT_TRUE(CanaryIntact());
}

}  // namespace
