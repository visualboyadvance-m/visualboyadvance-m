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
