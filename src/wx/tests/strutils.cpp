#include "wx/config/strutils.h"

#include "tests.hpp"

TEST_CASE("config::str_split() basic test") {
    wxString foo = "foo|bar|baz";

    auto vec = config::str_split(foo, '|');

    CHECK(vec.size() == 3);

    CHECK(vec[0] == "foo");
    CHECK(vec[1] == "bar");
    CHECK(vec[2] == "baz");
}

TEST_CASE("config::str_split() multi-char separator") {
    wxString foo = "foo|-|bar|-|baz";

    auto vec = config::str_split(foo, "|-|");

    CHECK(vec.size() == 3);

    CHECK(vec[0] == "foo");
    CHECK(vec[1] == "bar");
    CHECK(vec[2] == "baz");
}

TEST_CASE("config::str_split() skips empty tokens") {
    wxString foo = "|-|foo|-||-|bar|-|baz|-|";

    auto vec = config::str_split(foo, "|-|");

    CHECK(vec.size() == 3);

    CHECK(vec[0] == "foo");
    CHECK(vec[1] == "bar");
    CHECK(vec[2] == "baz");
}

TEST_CASE("config::str_split() empty input") {
    wxString foo;

    auto vec = config::str_split(foo, "|-|");

    CHECK(vec.size() == 0);
}

TEST_CASE("config::str_split() no tokens, just separators") {
    wxString foo = "|-||-||-||-||-|";

    auto vec = config::str_split(foo, "|-|");

    CHECK(vec.size() == 0);
}

TEST_CASE("config::str_split_with_sep() basic test") {
    wxString foo = "foo|bar|baz|";

    auto vec = config::str_split_with_sep(foo, '|');

    CHECK(vec.size() == 4);

    CHECK(vec[0] == "foo");
    CHECK(vec[1] == "bar");
    CHECK(vec[2] == "baz");
    CHECK(vec[3] == "|");
}

TEST_CASE("config::str_split_with_sep() multi-char sep") {
    wxString foo = "foo|-|bar|-|baz|-|";

    auto vec = config::str_split_with_sep(foo, "|-|");

    CHECK(vec.size() == 4);

    CHECK(vec[0] == "foo");
    CHECK(vec[1] == "bar");
    CHECK(vec[2] == "baz");
    CHECK(vec[3] == "|-|");
}

TEST_CASE("config::str_split_with_sep() multiple sep tokens") {
    wxString foo = "|-|foo|-||-|bar|-|baz|-|";

    auto vec = config::str_split_with_sep(foo, "|-|");

    CHECK(vec.size() == 6);

    CHECK(vec[0] == "|-|");
    CHECK(vec[1] == "foo");
    CHECK(vec[2] == "|-|");
    CHECK(vec[3] == "bar");
    CHECK(vec[4] == "baz");
    CHECK(vec[5] == "|-|");
}
