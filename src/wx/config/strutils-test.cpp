#include "wx/config/strutils.h"

#include <gtest/gtest.h>

TEST(StrSplitTest, Basic) {
    wxString foo = "foo|bar|baz";

    auto vec = config::str_split(foo, '|');

    EXPECT_EQ(vec.size(), 3);

    EXPECT_EQ(vec[0], "foo");
    EXPECT_EQ(vec[1], "bar");
    EXPECT_EQ(vec[2], "baz");
}

TEST(StrSplitTest, MultiCharSep) {
    wxString foo = "foo|-|bar|-|baz";

    auto vec = config::str_split(foo, "|-|");

    EXPECT_EQ(vec.size(), 3);

    EXPECT_EQ(vec[0], "foo");
    EXPECT_EQ(vec[1], "bar");
    EXPECT_EQ(vec[2], "baz");
}

TEST(StrSplitTest, SkipEmptyToken) {
    wxString foo = "|-|foo|-||-|bar|-|baz|-|";

    auto vec = config::str_split(foo, "|-|");

    EXPECT_EQ(vec.size(), 3);

    EXPECT_EQ(vec[0], "foo");
    EXPECT_EQ(vec[1], "bar");
    EXPECT_EQ(vec[2], "baz");
}

TEST(StrSplitTest, EmptyInput) {
    wxString foo;

    auto vec = config::str_split(foo, "|-|");

    EXPECT_EQ(vec.size(), 0);
}

TEST(StrSplitTest, NoTokens) {
    wxString foo = "|-||-||-||-||-|";

    auto vec = config::str_split(foo, "|-|");

    EXPECT_EQ(vec.size(), 0);
}

TEST(StrSplitWithSepTest, Basic) {
    wxString foo = "foo|bar|baz|";

    auto vec = config::str_split_with_sep(foo, '|');

    EXPECT_EQ(vec.size(), 4);

    EXPECT_EQ(vec[0], "foo");
    EXPECT_EQ(vec[1], "bar");
    EXPECT_EQ(vec[2], "baz");
    EXPECT_EQ(vec[3], "|");
}

TEST(StrSplitWithSepTest, MultiCharSep) {
    wxString foo = "foo|-|bar|-|baz|-|";

    auto vec = config::str_split_with_sep(foo, "|-|");

    EXPECT_EQ(vec.size(), 4);

    EXPECT_EQ(vec[0], "foo");
    EXPECT_EQ(vec[1], "bar");
    EXPECT_EQ(vec[2], "baz");
    EXPECT_EQ(vec[3], "|-|");
}

TEST(StrSplitWithSepTest, MultipleSepTokens) {
    wxString foo = "|-|foo|-||-|bar|-|baz|-|";

    auto vec = config::str_split_with_sep(foo, "|-|");

    EXPECT_EQ(vec.size(), 6);

    EXPECT_EQ(vec[0], "|-|");
    EXPECT_EQ(vec[1], "foo");
    EXPECT_EQ(vec[2], "|-|");
    EXPECT_EQ(vec[3], "bar");
    EXPECT_EQ(vec[4], "baz");
    EXPECT_EQ(vec[5], "|-|");
}

TEST(UTF16ToUTF8Test, Basic) {
    const std::vector<uint16_t> utf16 = {'f', 'o', 'o', 0};
    auto vec = config::utf16_to_utf8_vector(utf16.data());

    ASSERT_EQ(vec.size(), 3);

    EXPECT_EQ(vec[0], 'f');
    EXPECT_EQ(vec[1], 'o');
    EXPECT_EQ(vec[2], 'o');
}

TEST(UTF16ToUTF8Test, MultiByte) {
    // U+20AC EURO SIGN.
    const std::vector<uint16_t> utf16 = {0x20AC, 0};
    auto vec = config::utf16_to_utf8_vector(utf16.data());

    ASSERT_EQ(vec.size(), 3);

    EXPECT_EQ(vec[0], 0xE2);
    EXPECT_EQ(vec[1], 0x82);
    EXPECT_EQ(vec[2], 0xAC);
}

TEST(UTF16ToUTF8Test, DualMultiByte) {
    // This is a variant of the above to test the buffer reset is done properly.
    const std::vector<uint16_t> utf16 = {0x20AC, 0x20AC, 0};
    auto vec = config::utf16_to_utf8_vector(utf16.data());

    ASSERT_EQ(vec.size(), 6);

    EXPECT_EQ(vec[0], 0xE2);
    EXPECT_EQ(vec[1], 0x82);
    EXPECT_EQ(vec[2], 0xAC);
    EXPECT_EQ(vec[3], 0xE2);
    EXPECT_EQ(vec[4], 0x82);
    EXPECT_EQ(vec[5], 0xAC);
}

TEST(UTF16ToUTF8Test, SurrogatePair) {
    // U+1F914 THINKING FACE.
    const std::vector<uint16_t> utf16 = {0xD83E, 0xDD14, 0};
    auto vec = config::utf16_to_utf8_vector(utf16.data());

    ASSERT_EQ(vec.size(), 4);

    EXPECT_EQ(vec[0], 0xF0);
    EXPECT_EQ(vec[1], 0x9F);
    EXPECT_EQ(vec[2], 0xA4);
    EXPECT_EQ(vec[3], 0x94);
}

TEST(UTF16ToUTF8Test, InvalidSurrogatePair) {
    // U+D800 HIGH SURROGATE.
    const std::vector<uint16_t> utf16 = {0xD800, 0};
    EXPECT_DEATH(config::utf16_to_utf8_vector(utf16.data()), ".*");
}

TEST(UTF16ToUTF8Test, InvalidSurrogatePair2) {
    // U+D800 HIGH SURROGATE followed by U+0020 SPACE.
    const std::vector<uint16_t> utf16 = {0xD800, 0x0020, 0};
    EXPECT_DEATH(config::utf16_to_utf8_vector(utf16.data()), ".*");
}

TEST(UTF16ToUTF8Test, InvalidSurrogatePair3) {
    // U+D800 HIGH SURROGATE followed by U+D800 HIGH SURROGATE.
    const std::vector<uint16_t> utf16 = {0xD800, 0xD800, 0};
    EXPECT_DEATH(config::utf16_to_utf8_vector(utf16.data()), ".*");
}

TEST(UTF16ToUTF8Test, FullString) {
    // "foo€🤔"
    const std::vector<uint16_t> utf16 = {'f', 'o', 'o', 0x20AC, 0xD83E, 0xDD14, 0};
    auto vec = config::utf16_to_utf8_vector(utf16.data());

    ASSERT_EQ(vec.size(), 10);

    EXPECT_EQ(vec[0], 'f');
    EXPECT_EQ(vec[1], 'o');
    EXPECT_EQ(vec[2], 'o');
    EXPECT_EQ(vec[3], 0xE2);
    EXPECT_EQ(vec[4], 0x82);
    EXPECT_EQ(vec[5], 0xAC);
    EXPECT_EQ(vec[6], 0xF0);
    EXPECT_EQ(vec[7], 0x9F);
    EXPECT_EQ(vec[8], 0xA4);
    EXPECT_EQ(vec[9], 0x94);
}
