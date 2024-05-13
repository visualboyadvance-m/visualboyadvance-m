#ifndef VBAM_CORE_BASE_TEST_NOTREACHED_H_
#define VBAM_CORE_BASE_TEST_NOTREACHED_H_

#include <gtest/gtest.h>

#if defined(DEBUG)

#define VBAM_EXPECT_NOTREACHED(statement) EXPECT_DEATH(statement, "NOTREACHED")

#else  // defined(DEBUG)

// There is no way to test this in release builds as the compiler might optimize
// this code path away. Eat the statement to avoid unused variable warnings.
#define VBAM_EXPECT_NOTREACHED(statement) \
    if constexpr (false) { \
        statement; \
    } \
    static_assert(true, "")

#endif  // defined(DEBUG)

#endif  // VBAM_CORE_BASE_TEST_NOTREACHED_H_
