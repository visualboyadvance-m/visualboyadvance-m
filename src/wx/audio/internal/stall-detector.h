#ifndef WX_AUDIO_INTERNAL_STALL_DETECTOR_H_
#define WX_AUDIO_INTERNAL_STALL_DETECTOR_H_

#include <chrono>

namespace audio {
namespace internal {

// Tells a sound driver whether the emulator stopped calling write() for a while.
//
// A driver's queue paces the emulator, and it can run dry two ways. A frame or two
// that took too long -- the emulator falling briefly behind -- is made up by running
// the next frames back to back until the queue is full again, which keeps the average
// at full speed. But when the emulator did not run at all for a while -- a live window
// resize, the fullscreen animation, anything that holds the event loop -- that same
// catch-up is a visible burst of fast-forward, and the driver tops the queue up with
// silence instead. This is what tells the two apart: no write() for longer than any
// frame takes.
class StallDetector {
public:
    // Call once per write(). True when the previous write() was longer ago than a
    // stall; false on the first write after construction.
    bool Stalled() {
        const auto now = std::chrono::steady_clock::now();
        const bool stalled = seen_ && now - last_ > kStall;
        last_ = now;
        seen_ = true;
        return stalled;
    }

private:
    // Several frames even at the slowest the emulator runs while playing (a slow frame
    // with DLSS NR is 17-70 ms), and less than a resize or the fullscreen animation.
    static constexpr std::chrono::milliseconds kStall{100};

    std::chrono::steady_clock::time_point last_;
    bool seen_ = false;
};

}  // namespace internal
}  // namespace audio

#endif  // WX_AUDIO_INTERNAL_STALL_DETECTOR_H_
