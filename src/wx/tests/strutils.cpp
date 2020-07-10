#define CATCH_CONFIG_MAIN
#include "strutils.h"
#include "catch.hpp"

TEST_CASE("str_split() basic test", "[str_split]") {
    wxString foo = "foo|bar|baz";

    auto vec = str_split(foo, '|');

    REQUIRE(vec.size() == 3);

    REQUIRE(vec[0] == "foo");
    REQUIRE(vec[1] == "bar");
    REQUIRE(vec[2] == "baz");
}

TEST_CASE("str_split() multi-char separator", "[str_split]") {
    wxString foo = "foo|-|bar|-|baz";

    auto vec = str_split(foo, "|-|");

    REQUIRE(vec.size() == 3);

    REQUIRE(vec[0] == "foo");
    REQUIRE(vec[1] == "bar");
    REQUIRE(vec[2] == "baz");
}

TEST_CASE("str_split() skips empty tokens", "[str_split]") {
    wxString foo = "|-|foo|-||-|bar|-|baz|-|";

    auto vec = str_split(foo, "|-|");

    REQUIRE(vec.size() == 3);

    REQUIRE(vec[0] == "foo");
    REQUIRE(vec[1] == "bar");
    REQUIRE(vec[2] == "baz");
}

TEST_CASE("str_split() empty input", "[str_split]") {
    wxString foo;

    auto vec = str_split(foo, "|-|");

    REQUIRE(vec.size() == 0);
}

TEST_CASE("str_split() no tokens, just separators", "[str_split]") {
    wxString foo = "|-||-||-||-||-|";

    auto vec = str_split(foo, "|-|");

    REQUIRE(vec.size() == 0);
}

TEST_CASE("str_split_with_sep() basic test", "[str_split_with_sep]") {
    wxString foo = "foo|bar|baz|";

    auto vec = str_split_with_sep(foo, '|');

    REQUIRE(vec.size() == 4);

    REQUIRE(vec[0] == "foo");
    REQUIRE(vec[1] == "bar");
    REQUIRE(vec[2] == "baz");
    REQUIRE(vec[3] == "|");
}

TEST_CASE("str_split_with_sep() multi-char sep", "[str_split_with_sep]") {
    wxString foo = "foo|-|bar|-|baz|-|";

    auto vec = str_split_with_sep(foo, "|-|");

    REQUIRE(vec.size() == 4);

    REQUIRE(vec[0] == "foo");
    REQUIRE(vec[1] == "bar");
    REQUIRE(vec[2] == "baz");
    REQUIRE(vec[3] == "|-|");
}

TEST_CASE("str_split_with_sep() multiple sep tokens", "[str_split_with_sep]") {
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

