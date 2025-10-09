#ifndef VBAM_CORE_BASE_CHECK_H_
#define VBAM_CORE_BASE_CHECK_H_

// This header defines a number of macros for checking conditions and crashing
// the program if they are not met.
// * VBAM_CHECK(condition) - crashes the program if the condition is not met.
// * VBAM_NOTREACHED() - crashes the program if this line of code is reached.
//   In release builds, this macro also tells the compiler that this code path
//   is unreachable, which can help the compiler generate better code.
// * VBAM_STRINGIFY(x) - converts the argument to a string literal.
// While a number of other macros are defined in this file, they are not
// intended for general use and should be avoided.

#if defined(__GNUC__) || defined(__clang__)

// GCC/Clang.
#define VBAM_IMMEDIATE_CRASH_DETAIL() __builtin_trap()
#define VBAM_INTRINSIC_UNREACHABLE_DETAIL() __builtin_unreachable()

#elif defined(_MSC_VER)  // defined(__GNUC__) || defined(__clang__)

// MSVC.
#define VBAM_IMMEDIATE_CRASH_DETAIL() __debugbreak()
#define VBAM_INTRINSIC_UNREACHABLE_DETAIL() __assume(0)

#else  // defined(__GNUC__) || defined(__clang__)

#error "Unsupported compiler"

#endif  // defined(__GNUC__) || defined(__clang__)

#define VBAM_STRINGIFY_DETAIL(x) #x
#define VBAM_STRINGIFY(x) VBAM_STRINGIFY_DETAIL(x)

#define VBAM_REQUIRE_SEMICOLON_DETAIL() \
    static_assert(true, "Require a semicolon after macros invocation.")

#define VBAM_CHECK(condition)                                                                \
    if (!(condition)) {                                                                      \
        fputs("CHECK failed at " __FILE__ ":" VBAM_STRINGIFY(__LINE__) ": " #condition "\n", \
              stderr);                                                                       \
        VBAM_IMMEDIATE_CRASH_DETAIL();                                                       \
    }                                                                                        \
    VBAM_REQUIRE_SEMICOLON_DETAIL()

#if defined(DEBUG)

#define VBAM_NOTREACHED()                                                                    \
    fputs("NOTREACHED code reached at " __FILE__ ":" VBAM_STRINGIFY(__LINE__) "\n", stderr); \
    VBAM_IMMEDIATE_CRASH_DETAIL()

#else  // defined(DEBUG)

#define VBAM_NOTREACHED() VBAM_INTRINSIC_UNREACHABLE_DETAIL()

#endif  // defined(DEBUG)

#endif  // VBAM_CORE_BASE_CHECK_H_
