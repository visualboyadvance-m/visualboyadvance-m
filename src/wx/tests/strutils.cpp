#include "wx/strutils.h"

#include "tests.hpp"

TEST_CASE("strutils::split() basic test") {
    wxString foo = "foo|bar|baz";

    auto vec = strutils::split(foo, '|');

    CHECK(vec.size() == 3);

    CHECK(vec[0] == "foo");
    CHECK(vec[1] == "bar");
    CHECK(vec[2] == "baz");
}

TEST_CASE("strutils::split() multi-char separator") {
    wxString foo = "foo|-|bar|-|baz";

    auto vec = strutils::split(foo, "|-|");

    CHECK(vec.size() == 3);

    CHECK(vec[0] == "foo");
    CHECK(vec[1] == "bar");
    CHECK(vec[2] == "baz");
}

TEST_CASE("strutils::split() skips empty tokens") {
    wxString foo = "|-|foo|-||-|bar|-|baz|-|";

    auto vec = strutils::split(foo, "|-|");

    CHECK(vec.size() == 3);

    CHECK(vec[0] == "foo");
    CHECK(vec[1] == "bar");
    CHECK(vec[2] == "baz");
}

TEST_CASE("strutils::split() empty input") {
    wxString foo;

    auto vec = strutils::split(foo, "|-|");

    CHECK(vec.size() == 0);
}

TEST_CASE("strutils::split() no tokens, just separators") {
    wxString foo = "|-||-||-||-||-|";

    auto vec = strutils::split(foo, "|-|");

    CHECK(vec.size() == 0);
}

TEST_CASE("strutils::split_with_sep() basic test") {
    wxString foo = "foo|bar|baz|";

    auto vec = strutils::split_with_sep(foo, '|');

    CHECK(vec.size() == 4);

    CHECK(vec[0] == "foo");
    CHECK(vec[1] == "bar");
    CHECK(vec[2] == "baz");
    CHECK(vec[3] == "|");
}

TEST_CASE("strutils::split_with_sep() multi-char sep") {
    wxString foo = "foo|-|bar|-|baz|-|";

    auto vec = strutils::split_with_sep(foo, "|-|");

    CHECK(vec.size() == 4);

    CHECK(vec[0] == "foo");
    CHECK(vec[1] == "bar");
    CHECK(vec[2] == "baz");
    CHECK(vec[3] == "|-|");
}

TEST_CASE("strutils::split_with_sep() multiple sep tokens") {
    wxString foo = "|-|foo|-||-|bar|-|baz|-|";

    auto vec = strutils::split_with_sep(foo, "|-|");

    CHECK(vec.size() == 6);

    CHECK(vec[0] == "|-|");
    CHECK(vec[1] == "foo");
    CHECK(vec[2] == "|-|");
    CHECK(vec[3] == "bar");
    CHECK(vec[4] == "baz");
    CHECK(vec[5] == "|-|");
}
