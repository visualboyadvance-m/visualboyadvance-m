// POSIX sleeping for classic Mac OS (Retro68), whose newlib has neither
// sleep() nor usleep(); libstdc++'s std::this_thread::sleep_for() calls both.
//
// SDL_DelayNS() is the right wait there: SDL mixes audio on the caller's
// thread whenever it is pumped or delays, so waiting through it keeps the
// sound going and lets other applications run.
//
// With pthreads-retro68 (VBAM_RETRO68_PTHREADS) the library has weak sleep()
// and usleep() of its own, which these replace. Its threads are cooperative,
// so a wait must also let them run: the main thread waits through SDL in
// short slices and yields to the other threads between them. Any other
// thread waits in the library's nanosleep(), which stops it until the
// deadline and leaves the Toolbox and SDL's audio to the main thread.

#include <unistd.h>

#include <SDL3/SDL_timer.h>

#ifdef VBAM_RETRO68_PTHREADS
#include <pthread.h>
#include <sched.h>
#include <time.h>
#endif

namespace {

#ifdef VBAM_RETRO68_PTHREADS

// Static initialization runs on the main (application) thread.
const pthread_t g_main_thread = pthread_self();

// How long the main thread waits through SDL before letting the other
// threads have a turn.
constexpr Uint64 kMainSliceNs = SDL_NS_PER_MS;

void Wait(Uint64 ns) {
    if (!pthread_equal(pthread_self(), g_main_thread)) {
        timespec ts;
        ts.tv_sec = static_cast<time_t>(ns / SDL_NS_PER_SECOND);
        ts.tv_nsec = static_cast<long>(ns % SDL_NS_PER_SECOND);
        nanosleep(&ts, nullptr);
        return;
    }

    const Uint64 start = SDL_GetTicksNS();
    for (;;) {
        sched_yield();
        const Uint64 elapsed = SDL_GetTicksNS() - start;
        if (elapsed >= ns)
            break;
        const Uint64 left = ns - elapsed;
        SDL_DelayNS(left < kMainSliceNs ? left : kMainSliceNs);
    }
}

#else  // !VBAM_RETRO68_PTHREADS

void Wait(Uint64 ns) {
    SDL_DelayNS(ns);
}

#endif  // VBAM_RETRO68_PTHREADS

}  // namespace

extern "C" {

unsigned int sleep(unsigned int seconds) {
    Wait(static_cast<Uint64>(seconds) * SDL_NS_PER_SECOND);
    return 0;
}

int usleep(useconds_t microseconds) {
    Wait(static_cast<Uint64>(microseconds) * SDL_NS_PER_US);
    return 0;
}

}  // extern "C"
