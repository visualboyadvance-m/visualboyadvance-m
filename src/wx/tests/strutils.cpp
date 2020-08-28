#include "strutils.h"

#include "tests.hpp"

TEST_CASE("str_split() basic test") {
    wxString foo = "foo|bar|baz";

    auto vec = str_split(foo, '|');

    REQUIRE(vec.size() == 3);

    REQUIRE(vec[0] == "foo");
    REQUIRE(vec[1] == "bar");
    REQUIRE(vec[2] == "baz");
}

TEST_CASE("str_split() multi-char separator") {
    wxString foo = "foo|-|bar|-|baz";

    auto vec = str_split(foo, "|-|");

    REQUIRE(vec.size() == 3);

    REQUIRE(vec[0] == "foo");
    REQUIRE(vec[1] == "bar");
    REQUIRE(vec[2] == "baz");
}

TEST_CASE("str_split() skips empty tokens") {
    wxString foo = "|-|foo|-||-|bar|-|baz|-|";

    auto vec = str_split(foo, "|-|");

    REQUIRE(vec.size() == 3);

    REQUIRE(vec[0] == "foo");
    REQUIRE(vec[1] == "bar");
    REQUIRE(vec[2] == "baz");
}

TEST_CASE("str_split() empty input") {
    wxString foo;

    auto vec = str_split(foo, "|-|");

    REQUIRE(vec.size() == 0);
}

TEST_CASE("str_split() no tokens, just separators") {
    wxString foo = "|-||-||-||-||-|";

    auto vec = str_split(foo, "|-|");

    REQUIRE(vec.size() == 0);
}

TEST_CASE("str_split_with_sep() basic test") {
    wxString foo = "foo|bar|baz|";

    auto vec = str_split_with_sep(foo, '|');

    REQUIRE(vec.size() == 4);

    REQUIRE(vec[0] == "foo");
    REQUIRE(vec[1] == "bar");
    REQUIRE(vec[2] == "baz");
    REQUIRE(vec[3] == "|");
}

TEST_CASE("str_split_with_sep() multi-char sep") {
    wxString foo = "foo|-|bar|-|baz|-|";

    auto vec = str_split_with_sep(foo, "|-|");

    REQUIRE(vec.size() == 4);

    REQUIRE(vec[0] == "foo");
    REQUIRE(vec[1] == "bar");
    REQUIRE(vec[2] == "baz");
    REQUIRE(vec[3] == "|-|");
}

TEST_CASE("str_split_with_sep() multiple sep tokens") {
    wxString foo = "|-|foo|-||-|bar|-|baz|-|";

    auto vec = str_split_with_sep(foo, "|-|");

    REQUIRE(vec.size() == 6);

    REQUIRE(vec[0] == "|-|");
    REQUIRE(vec[1] == "foo");
    REQUIRE(vec[2] == "|-|");
    REQUIRE(vec[3] == "bar");
    REQUIRE(vec[4] == "baz");
    REQUIRE(vec[5] == "|-|");
}

