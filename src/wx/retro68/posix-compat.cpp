// POSIX sleeping for classic Mac OS (Retro68), whose newlib has neither
// sleep() nor usleep(); libstdc++'s std::this_thread::sleep_for() calls both.
//
// SDL_DelayNS() is the right wait there: with no threads, SDL mixes audio on
// the caller's thread whenever it is pumped or delays, so waiting through it
// keeps the sound going and lets other applications run.

#include <unistd.h>

#include <SDL3/SDL_timer.h>

extern "C" {

unsigned int sleep(unsigned int seconds) {
    SDL_DelayNS(static_cast<Uint64>(seconds) * SDL_NS_PER_SECOND);
    return 0;
}

int usleep(useconds_t microseconds) {
    SDL_DelayNS(static_cast<Uint64>(microseconds) * SDL_NS_PER_US);
    return 0;
}

}  // extern "C"
