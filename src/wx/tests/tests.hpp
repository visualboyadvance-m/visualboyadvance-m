#ifndef TESTS_HPP
#define TESTS_HPP

#ifdef _MSC_VER
#  define DOCTEST_CONFIG_USE_STD_HEADERS
#endif

#define DOCTEST_THREAD_LOCAL // Avoid MinGW thread_local bug.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest.h>

#endif
