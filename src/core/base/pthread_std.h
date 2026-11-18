#ifndef VBAM_CORE_BASE_PTHREAD_STD_H_
#define VBAM_CORE_BASE_PTHREAD_STD_H_

// std::mutex and friends on POSIX threads, for a C++ library built without
// gthreads that runs where a pthreads library exists anyway: Retro68 with
// pthreads-retro68 (classic Mac OS, cooperative Thread Manager threads).
//
// Such a libstdc++'s <mutex>, <condition_variable> and <shared_mutex> leave
// out std::mutex and the rest, while std::lock_guard, std::unique_lock,
// std::shared_lock, std::scoped_lock and std::atomic are still there and work
// with any type that has the members. This header supplies the missing types
// with the standard interfaces, each a thin wrapper of the pthread object, so
// the code shared with the other ports locks and waits for real. It is the
// threaded counterpart of nothreads.h; the Retro68 build force-includes one of
// the two through retro68/prefix.h (VBAM_RETRO68_PTHREADS picks this one).
//
// Not here: std::thread (declared by the library, but without a constructor:
// start threads with pthread_create() or wxThread), std::this_thread::yield()
// (a no-op there: call sched_yield()) and thread_local (see thread_local.h).
// std::this_thread::sleep_for() works; it ends in usleep(), which yields.
//
// Timed waits measure on CLOCK_MONOTONIC (Microseconds() there); a deadline
// on another clock is converted to a duration from now first, as libstdc++
// itself does, and checked again against that clock on wake-up. The waits
// for a duration with a predicate keep their deadline on CLOCK_MONOTONIC too
// (MonotonicClock): that libstdc++'s steady_clock is its system_clock, on
// Retro68's gettimeofday(), which was seen to jump by hundreds of
// milliseconds between two calls.

#ifdef __cplusplus
#include <cstddef>  // for the library's configuration macros
#endif

#if defined(__cplusplus) && defined(__GLIBCXX__) && !defined(_GLIBCXX_HAS_GTHREADS)

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <time.h>

#include <bits/functexcept.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>

// Threads exist, but switch only where one blocks or yields.
#define VBAM_COOPERATIVE_THREADS 1
// std::thread cannot start a thread (see above).
#define VBAM_NO_STD_THREAD 1
// The compiler's thread_local has no runtime there (see thread_local.h).
#define VBAM_NO_THREAD_LOCAL 1

namespace vbam_pthread_std {

// CLOCK_MONOTONIC now plus rel, for the pthread_*_clock*() calls. A negative
// duration is a deadline already past; one too long to represent is clamped
// to about 34 years, which no wait reaches.
template <class Rep, class Period>
inline timespec MonotonicDeadline(const std::chrono::duration<Rep, Period>& rel) {
    using std::chrono::duration;
    using std::chrono::nanoseconds;

    constexpr int64_t kMaxSeconds = INT64_C(1) << 30;
    int64_t ns = 0;
    if (duration<double>(rel).count() >= static_cast<double>(kMaxSeconds))
        ns = kMaxSeconds * INT64_C(1000000000);
    else if (rel > rel.zero())
        ns = std::chrono::ceil<nanoseconds>(rel).count();

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    int64_t sec = static_cast<int64_t>(now.tv_sec) + ns / INT64_C(1000000000);
    int64_t nsec = static_cast<int64_t>(now.tv_nsec) + ns % INT64_C(1000000000);
    if (nsec >= INT64_C(1000000000)) {
        ++sec;
        nsec -= INT64_C(1000000000);
    }

    timespec ts;
    ts.tv_sec = static_cast<time_t>(sec);
    ts.tv_nsec = static_cast<long>(nsec);
    return ts;
}

// A chrono clock on CLOCK_MONOTONIC.
struct MonotonicClock {
    typedef std::chrono::nanoseconds duration;
    typedef duration::rep rep;
    typedef duration::period period;
    typedef std::chrono::time_point<MonotonicClock> time_point;
    static constexpr bool is_steady = true;

    static time_point now() noexcept {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return time_point(duration(static_cast<rep>(ts.tv_sec) * 1000000000 + ts.tv_nsec));
    }
};

// The pthread mutex behind std::mutex and the three other mutex types; the
// static initializer picks the kind, so each of them is constant-initialized
// like the standard's and works as a global before any constructor has run.
// pthreads-retro68 allocates the real object on first use.
class Mutex {
public:
    typedef pthread_mutex_t* native_handle_type;

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    void lock() {
        const int e = pthread_mutex_lock(&m_);
        if (e)
            std::__throw_system_error(e);
    }
    bool try_lock() noexcept { return pthread_mutex_trylock(&m_) == 0; }
    void unlock() { pthread_mutex_unlock(&m_); }

    template <class Rep, class Period>
    bool try_lock_for(const std::chrono::duration<Rep, Period>& rel) {
        const timespec ts = MonotonicDeadline(rel);
        return pthread_mutex_clocklock(&m_, CLOCK_MONOTONIC, &ts) == 0;
    }
    template <class Clock, class Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& atime) {
        return try_lock_for(atime - Clock::now());
    }

    native_handle_type native_handle() noexcept { return &m_; }

protected:
    explicit constexpr Mutex(pthread_mutex_t init) noexcept : m_(init) {}
    ~Mutex() { pthread_mutex_destroy(&m_); }

private:
    pthread_mutex_t m_;
};

// std::shared_mutex and std::shared_timed_mutex: a pthread rwlock, which
// pthreads-retro68 makes writer-preferring.
class RwLock {
public:
    typedef pthread_rwlock_t* native_handle_type;

    constexpr RwLock() noexcept : rw_(PTHREAD_RWLOCK_INITIALIZER) {}
    ~RwLock() { pthread_rwlock_destroy(&rw_); }
    RwLock(const RwLock&) = delete;
    RwLock& operator=(const RwLock&) = delete;

    void lock() {
        const int e = pthread_rwlock_wrlock(&rw_);
        if (e)
            std::__throw_system_error(e);
    }
    bool try_lock() noexcept { return pthread_rwlock_trywrlock(&rw_) == 0; }
    void unlock() { pthread_rwlock_unlock(&rw_); }

    void lock_shared() {
        const int e = pthread_rwlock_rdlock(&rw_);
        if (e)
            std::__throw_system_error(e);
    }
    bool try_lock_shared() noexcept { return pthread_rwlock_tryrdlock(&rw_) == 0; }
    void unlock_shared() { pthread_rwlock_unlock(&rw_); }

    native_handle_type native_handle() noexcept { return &rw_; }

protected:
    pthread_rwlock_t rw_;
};

}  // namespace vbam_pthread_std

namespace std {

class mutex : public vbam_pthread_std::Mutex {
public:
    constexpr mutex() noexcept : Mutex(PTHREAD_MUTEX_INITIALIZER) {}

private:
    // Not timed: std::mutex has no try_lock_for().
    using Mutex::try_lock_for;
    using Mutex::try_lock_until;
};

class recursive_mutex : public vbam_pthread_std::Mutex {
public:
    constexpr recursive_mutex() noexcept : Mutex(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP) {}

private:
    using Mutex::try_lock_for;
    using Mutex::try_lock_until;
};

class timed_mutex : public vbam_pthread_std::Mutex {
public:
    constexpr timed_mutex() noexcept : Mutex(PTHREAD_MUTEX_INITIALIZER) {}
};

class recursive_timed_mutex : public vbam_pthread_std::Mutex {
public:
    constexpr recursive_timed_mutex() noexcept : Mutex(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP) {}
};

class shared_mutex : public vbam_pthread_std::RwLock {};

class shared_timed_mutex : public vbam_pthread_std::RwLock {
public:
    template <class Rep, class Period>
    bool try_lock_for(const chrono::duration<Rep, Period>& rel) {
        const timespec ts = vbam_pthread_std::MonotonicDeadline(rel);
        return pthread_rwlock_clockwrlock(&rw_, CLOCK_MONOTONIC, &ts) == 0;
    }
    template <class Clock, class Duration>
    bool try_lock_until(const chrono::time_point<Clock, Duration>& atime) {
        return try_lock_for(atime - Clock::now());
    }

    template <class Rep, class Period>
    bool try_lock_shared_for(const chrono::duration<Rep, Period>& rel) {
        const timespec ts = vbam_pthread_std::MonotonicDeadline(rel);
        return pthread_rwlock_clockrdlock(&rw_, CLOCK_MONOTONIC, &ts) == 0;
    }
    template <class Clock, class Duration>
    bool try_lock_shared_until(const chrono::time_point<Clock, Duration>& atime) {
        return try_lock_shared_for(atime - Clock::now());
    }
};

enum class cv_status { no_timeout, timeout };

class condition_variable {
public:
    typedef pthread_cond_t* native_handle_type;

    constexpr condition_variable() noexcept : c_(PTHREAD_COND_INITIALIZER) {}
    ~condition_variable() { pthread_cond_destroy(&c_); }
    condition_variable(const condition_variable&) = delete;
    condition_variable& operator=(const condition_variable&) = delete;

    void notify_one() noexcept { pthread_cond_signal(&c_); }
    void notify_all() noexcept { pthread_cond_broadcast(&c_); }

    void wait(unique_lock<mutex>& lock) {
        const int e = pthread_cond_wait(&c_, lock.mutex()->native_handle());
        if (e)
            __throw_system_error(e);
    }

    template <class Predicate>
    void wait(unique_lock<mutex>& lock, Predicate pred) {
        while (!pred())
            wait(lock);
    }

    template <class Rep, class Period>
    cv_status wait_for(unique_lock<mutex>& lock, const chrono::duration<Rep, Period>& rel) {
        const timespec ts = vbam_pthread_std::MonotonicDeadline(rel);
        return pthread_cond_clockwait(&c_, lock.mutex()->native_handle(), CLOCK_MONOTONIC,
                                      &ts) == ETIMEDOUT
                   ? cv_status::timeout
                   : cv_status::no_timeout;
    }

    template <class Rep, class Period, class Predicate>
    bool wait_for(unique_lock<mutex>& lock, const chrono::duration<Rep, Period>& rel,
                  Predicate pred) {
        return wait_until(lock, vbam_pthread_std::MonotonicClock::now() + rel, std::move(pred));
    }

    template <class Clock, class Duration>
    cv_status wait_until(unique_lock<mutex>& lock,
                         const chrono::time_point<Clock, Duration>& atime) {
        wait_for(lock, atime - Clock::now());
        return Clock::now() < atime ? cv_status::no_timeout : cv_status::timeout;
    }

    template <class Clock, class Duration, class Predicate>
    bool wait_until(unique_lock<mutex>& lock, const chrono::time_point<Clock, Duration>& atime,
                    Predicate pred) {
        while (!pred()) {
            if (wait_until(lock, atime) == cv_status::timeout)
                return pred();
        }
        return true;
    }

    native_handle_type native_handle() noexcept { return &c_; }

private:
    pthread_cond_t c_;
};

// Waits with any lock: the caller's lock is released while holding an
// internal mutex, so a notify between the two cannot be lost, and taken back
// after the internal one is released, so the two are never held in the other
// order.
class condition_variable_any {
public:
    condition_variable_any() = default;
    condition_variable_any(const condition_variable_any&) = delete;
    condition_variable_any& operator=(const condition_variable_any&) = delete;

    void notify_one() noexcept {
        lock_guard<mutex> guard(m_);
        cv_.notify_one();
    }
    void notify_all() noexcept {
        lock_guard<mutex> guard(m_);
        cv_.notify_all();
    }

    template <class Lock>
    void wait(Lock& lock) {
        unique_lock<mutex> internal(m_);
        Relock<Lock> relock(lock, internal);
        cv_.wait(internal);
    }

    template <class Lock, class Predicate>
    void wait(Lock& lock, Predicate pred) {
        while (!pred())
            wait(lock);
    }

    template <class Lock, class Rep, class Period>
    cv_status wait_for(Lock& lock, const chrono::duration<Rep, Period>& rel) {
        unique_lock<mutex> internal(m_);
        Relock<Lock> relock(lock, internal);
        return cv_.wait_for(internal, rel);
    }

    template <class Lock, class Rep, class Period, class Predicate>
    bool wait_for(Lock& lock, const chrono::duration<Rep, Period>& rel, Predicate pred) {
        return wait_until(lock, vbam_pthread_std::MonotonicClock::now() + rel, std::move(pred));
    }

    template <class Lock, class Clock, class Duration>
    cv_status wait_until(Lock& lock, const chrono::time_point<Clock, Duration>& atime) {
        wait_for(lock, atime - Clock::now());
        return Clock::now() < atime ? cv_status::no_timeout : cv_status::timeout;
    }

    template <class Lock, class Clock, class Duration, class Predicate>
    bool wait_until(Lock& lock, const chrono::time_point<Clock, Duration>& atime,
                    Predicate pred) {
        while (!pred()) {
            if (wait_until(lock, atime) == cv_status::timeout)
                return pred();
        }
        return true;
    }

private:
    // Unlocks the caller's lock on construction (the internal one is held),
    // and on the way out, normal or by exception, releases the internal lock
    // before taking the caller's back.
    template <class Lock>
    class Relock {
    public:
        Relock(Lock& lock, unique_lock<mutex>& internal) : lock_(lock), internal_(internal) {
            lock_.unlock();
        }
        ~Relock() {
            internal_.unlock();
            lock_.lock();
        }

    private:
        Lock& lock_;
        unique_lock<mutex>& internal_;
    };

    mutex m_;
    condition_variable cv_;
};

}  // namespace std

#endif  // __cplusplus && __GLIBCXX__ && !_GLIBCXX_HAS_GTHREADS

#endif  // VBAM_CORE_BASE_PTHREAD_STD_H_
