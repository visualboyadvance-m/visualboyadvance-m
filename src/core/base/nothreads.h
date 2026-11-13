#ifndef VBAM_CORE_BASE_NOTHREADS_H_
#define VBAM_CORE_BASE_NOTHREADS_H_

// Single-threaded stand-ins for the standard library's locking types.
//
// The C++ library of a toolchain for a platform without preemptive threads
// (Retro68: classic Mac OS) is built without gthreads, and its <mutex>,
// <condition_variable> and <shared_mutex> then leave out std::mutex and the
// rest, while std::lock_guard, std::unique_lock, std::shared_lock and
// std::atomic are still there. A program with one thread never contends for a lock, so the code
// that guards state shared with other threads keeps working unchanged once
// these exist: locking always succeeds at once and notifying wakes nobody.
//
// What cannot work is waiting for another thread, as there is none; the
// frontends do not start threads on such a platform (see
// VBAM_SINGLE_THREADED), so a wait whose condition is false is a bug and
// aborts rather than hanging the machine.
//
// The Retro68 build force-includes it through retro68/prefix.h.

#ifdef __cplusplus
#include <cstddef>  // for the library's configuration macros
#endif

#if defined(__cplusplus) && defined(__GLIBCXX__) && !defined(_GLIBCXX_HAS_GTHREADS)

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <shared_mutex>

#define VBAM_SINGLE_THREADED 1

namespace std {

class mutex {
public:
    constexpr mutex() noexcept = default;
    mutex(const mutex&) = delete;
    mutex& operator=(const mutex&) = delete;

    void lock() {}
    bool try_lock() { return true; }
    void unlock() {}
};

class recursive_mutex : public mutex {};
class timed_mutex : public mutex {
public:
    template <class Rep, class Period>
    bool try_lock_for(const chrono::duration<Rep, Period>&) { return true; }
    template <class Clock, class Duration>
    bool try_lock_until(const chrono::time_point<Clock, Duration>&) { return true; }
};
class recursive_timed_mutex : public timed_mutex {};

class shared_mutex : public mutex {
public:
    void lock_shared() {}
    bool try_lock_shared() { return true; }
    void unlock_shared() {}
};
class shared_timed_mutex : public timed_mutex {
public:
    void lock_shared() {}
    bool try_lock_shared() { return true; }
    void unlock_shared() {}
};

enum class cv_status { no_timeout, timeout };

class condition_variable {
public:
    condition_variable() noexcept = default;
    condition_variable(const condition_variable&) = delete;
    condition_variable& operator=(const condition_variable&) = delete;

    void notify_one() noexcept {}
    void notify_all() noexcept {}

    // A wait without a predicate may return spuriously.
    void wait(unique_lock<mutex>&) {}

    template <class Predicate>
    void wait(unique_lock<mutex>&, Predicate pred) {
        if (!pred())
            Deadlock();
    }

    template <class Rep, class Period>
    cv_status wait_for(unique_lock<mutex>&, const chrono::duration<Rep, Period>&) {
        return cv_status::timeout;
    }

    template <class Rep, class Period, class Predicate>
    bool wait_for(unique_lock<mutex>&, const chrono::duration<Rep, Period>&, Predicate pred) {
        return pred();
    }

    template <class Clock, class Duration>
    cv_status wait_until(unique_lock<mutex>&, const chrono::time_point<Clock, Duration>&) {
        return cv_status::timeout;
    }

    template <class Clock, class Duration, class Predicate>
    bool wait_until(unique_lock<mutex>&, const chrono::time_point<Clock, Duration>&,
                    Predicate pred) {
        return pred();
    }

private:
    [[noreturn]] static void Deadlock() {
        fputs("condition_variable::wait: nothing can satisfy the predicate "
              "in a single-threaded program\n", stderr);
        abort();
    }
};

class condition_variable_any : public condition_variable {
public:
    template <class Lock>
    void wait(Lock&) {}

    template <class Lock, class Predicate>
    void wait(Lock&, Predicate pred) {
        if (!pred())
            abort();
    }
};

}  // namespace std

#endif  // __cplusplus && !_GLIBCXX_HAS_GTHREADS

#endif  // VBAM_CORE_BASE_NOTHREADS_H_
