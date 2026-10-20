#include "components/filters_dlssnr/dlssnr.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <functional>
// SSE2 is baseline on x86-64 and NEON on AArch64, so the vector loops below need no run-time
// check; elsewhere the scalar loops are all there is.
#if defined(__SSE2__) || (defined(_M_X64) && !defined(_M_ARM64EC)) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define VBAM_DLSSNR_SSE2 1
#include <emmintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#define VBAM_DLSSNR_NEON 1
#include <arm_neon.h>
#endif
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <vector>

#include "nr_frame.h"
extern "C" {
#include "nr_image.h"
}

namespace dlssnr {

namespace {

// The model, shared by every Filter in the process. libxmx is one device with
// process-wide state: `mutex` guards everything here, `use` is held shared
// around every libnr_frame call on `frame` and exclusively to close it, and one
// Filter's frames are in the model at a time (`owner`), since the extent the
// graph is prepared for belongs to the model, not to a Filter.
//
// Lock order: a Filter's own mutex, then `mutex`, then `use`. A thread holding
// `use` takes neither of the others until it lets `use` go.
struct SharedModel {
    // Serializes every libnr_frame call and guards `frame`, `users`, `device`,
    // `opened_shared` and `share_error`. Held for the length of a pass, and of
    // a model open -- seconds, on a driver that compiles pipelines slowly.
    std::mutex mutex;
    std::condition_variable cv;  // `owner` changed
    std::shared_mutex use;
    nr_frame* frame = nullptr;
    // Bumped whenever the model closes, so a frame claimed on the old one is
    // dropped instead of meeting a graph that never saw its features.
    uint64_t generation = 0;
    std::string device;
    // The renderer's Vulkan objects to adopt, under `share_mutex` alone. That
    // lock is only ever held across a few assignments, so a renderer can
    // register or look up a share while a pass or an open holds `mutex`.
    // Taken inside `mutex` where both are needed; never the other way round.
    std::mutex share_mutex;
    // A renderer's Vulkan objects to adopt at the next open, if any.
    bool has_share = false;
    VulkanShare share;
    // A share registered since the model opened: the open one predates it and
    // has to be closed and reopened before it takes effect.
    bool share_pending = false;
    // The open model runs on the shared device.
    bool opened_shared = false;
    // Whose device that is: the lender the open model adopted, which need not be
    // the one registered now. A renderer rebuilt in place (GameArea::
    // ReinitPanel()) lends its new device before the old one is torn down, so
    // the share changes hands while a pass is still running on the old device;
    // withdrawing must wait for that pass whatever is registered by then. Set
    // under `mutex`; read without it to decide whether to take it.
    std::atomic<const void*> opened_owner{nullptr};
    // `frame && opened_shared`, readable without the lock. UsingSharedVulkan()
    // is a query on the render path, and `mutex` is held for a whole pass.
    std::atomic<bool> shared_live{false};
    // Why the last adoption attempt fell back to libxmx's own instance, if it did.
    std::string share_error;
    // How many Filters exist. Bumped without `mutex`, which a model open holds
    // for many seconds and a Filter is constructed on the UI thread.
    std::atomic<int> users{0};
    // The Filter whose frames are in the pipeline, and how many.
    const void* owner = nullptr;
    int owner_frames = 0;
    // The extent the graph was last prepared for (nr_frame_features_masked).
    int prepared_width = 0;
    int prepared_height = 0;
};

SharedModel& Shared() {
    // Deliberately never destroyed. A worker retired mid-pass outlives the
    // Filter that started it and touches this on its way out, which can land
    // after static destructors would have run.
    static SharedModel* const model = new SharedModel();
    return *model;
}

std::atomic<ImageLoader> g_image_loader{nullptr};

// Closes the model and releases the device libxmx holds (its own, or a shared
// one back to its renderer), after the calls in flight. Caller holds `mutex`.
void CloseLocked(SharedModel& s) {
    std::unique_lock<std::shared_mutex> use(s.use);
    if (s.frame) {
        nr_frame_close(s.frame);
        s.frame = nullptr;
    }
    nr_frame_shutdown();
    s.opened_shared = false;
    s.opened_owner.store(nullptr, std::memory_order_release);
    s.generation++;
    s.prepared_width = 0;
    s.prepared_height = 0;
    s.shared_live.store(false, std::memory_order_release);
}

// Opens the model, on the shared device when one is registered. Caller holds
// `mutex`. Returns false and fills `error` on failure.
bool OpenLocked(SharedModel& s, std::string* error) {
    s.share_error.clear();
    bool adopted = false;
    VulkanShare v;
    bool has_share;
    {
        std::lock_guard<std::mutex> share_lock(s.share_mutex);
        has_share = s.has_share;
        v = s.share;
        s.share_pending = false;
    }
    if (has_share) {
        if (nr_frame_adopt_vulkan(v.instance, v.physical_device, v.device, v.queue,
                                  v.queue_family,
                                  (v.cooperative_matrix ? 1 : 0) |
                                      (v.cooperative_matrix && v.workgroup_memory_explicit_layout ? 2 : 0) |
                                      (v.subgroup_size_control ? 4 : 0) |
                                      (v.subgroup_size_control && v.compute_full_subgroups ? 8 : 0),
                                  v.get_instance_proc_addr, v.lock, v.unlock,
                                  v.lock_context) == 0) {
            adopted = true;
        } else {
            s.share_error = nr_frame_error();
        }
    }
    // NULL: the weights compiled into libnr_frame (NR_EMBED_WEIGHTS).
    s.frame = nr_frame_open(nullptr);
    if (!s.frame && adopted) {
        // The renderer's device would not take the graph: try libxmx's own.
        s.share_error = nr_frame_error();
        nr_frame_shutdown();
        adopted = false;
        s.frame = nr_frame_open(nullptr);
    }
    if (!s.frame) {
        *error = nr_frame_error();
        if (error->empty())
            *error = "nr_frame_open failed";
        nr_frame_shutdown();
        return false;
    }
    s.opened_shared = adopted && nr_frame_shared_device() == 1;
    s.opened_owner.store(s.opened_shared ? v.owner : nullptr, std::memory_order_release);
    s.shared_live.store(s.frame != nullptr && s.opened_shared, std::memory_order_release);
    s.device = std::string(nr_frame_device(s.frame)) + " (" + nr_frame_runtime() + " runtime, " +
               nr_frame_gemm_path(s.frame) + ")";
    if (s.opened_shared)
        s.device += " [renderer's Vulkan device]";
    else if (!s.share_error.empty())
        s.device += " [own instance; sharing failed: " + s.share_error + "]";
    s.prepared_width = 0;
    s.prepared_height = 0;
    return true;
}

void AcquireModel() {
    Shared().users.fetch_add(1, std::memory_order_relaxed);
}

void ReleaseModel() {
    SharedModel& s = Shared();
    std::lock_guard<std::mutex> lock(s.mutex);
    // A Filter constructed between the decrement and the close just reopens on
    // its first Claim(); it cannot have claimed anything yet, since Claim()
    // takes `mutex`, which is held here.
    if (s.users.load(std::memory_order_acquire) > 0 &&
        s.users.fetch_sub(1, std::memory_order_acq_rel) == 1)
        CloseLocked(s);
}

}  // namespace

void SetImageLoader(ImageLoader loader) {
    g_image_loader.store(loader);
}

void ShareVulkan(const VulkanShare& share) {
    SharedModel& s = Shared();
    // Registration only, under the short lock: a renderer lends its device
    // while it is being constructed, on the UI thread, and `mutex` may be held
    // by a model open for many seconds. Whatever is open runs elsewhere; the
    // worker closes it at the start of its next pass and reopens here.
    std::lock_guard<std::mutex> share_lock(s.share_mutex);
    s.share = share;
    s.has_share = true;
    s.share_pending = true;
}

namespace {

// Whether `owner` lent the registered share or the device the open model runs
// on -- either way it has something to take back.
bool LentBy(SharedModel& s, const void* owner) {
    if (s.opened_owner.load(std::memory_order_acquire) == owner)
        return true;
    std::lock_guard<std::mutex> share_lock(s.share_mutex);
    return s.has_share && s.share.owner == owner;
}

// With `mutex` held: close the model if it runs on `owner`'s device, and drop
// `owner`'s share if it is still the registered one. A newer lender's share is
// left alone.
void TakeBackLocked(SharedModel& s, const void* owner) {
    if (s.opened_shared && s.opened_owner.load(std::memory_order_acquire) == owner)
        CloseLocked(s);
    std::lock_guard<std::mutex> share_lock(s.share_mutex);
    if (s.has_share && s.share.owner == owner) {
        s.has_share = false;
        s.share_pending = false;
        s.share = VulkanShare();
    }
}

}  // namespace

void WithdrawVulkanShare(const void* owner) {
    SharedModel& s = Shared();
    // Not the lender: say so without touching `mutex`, so a renderer that
    // never lent anything is never held up by a pass.
    if (!LentBy(s, owner))
        return;
    // The lender is about to destroy the device. This is the one place that
    // has to wait: the model may be running on it right now, and taking
    // `mutex` is what proves it has stopped.
    //
    // A pass is bounded, but an open compiles every pipeline and would hold
    // this for tens of seconds, so ask the runtime to abandon one in progress.
    // It stops after the pipeline it is already building; the frame that asked
    // for it is dropped and the next one opens again, on whatever device is
    // shared by then.
    nr_frame_cancel_open(1);
    std::lock_guard<std::mutex> lock(s.mutex);
    nr_frame_cancel_open(0);
    TakeBackLocked(s, owner);
}

bool WithdrawVulkanShareDeferred(const void* owner, void (*finish)(void*), void* context,
                                 unsigned grace_ms) {
    SharedModel& s = Shared();
    if (!LentBy(s, owner))
        return false;
    // As above: ask first, so an open in progress stops after the pipeline it
    // is already building rather than after all of them.
    nr_frame_cancel_open(1);
    std::unique_lock<std::mutex> lock(s.mutex, std::defer_lock);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(grace_ms);
    while (!lock.try_lock()) {
        if (std::chrono::steady_clock::now() >= deadline)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (lock.owns_lock()) {
        // It let go in time, so there is nothing to defer and the caller keeps
        // the teardown it already had.
        nr_frame_cancel_open(0);
        TakeBackLocked(s, owner);
        return false;
    }
    // Still inside a pipeline. Drop the share so nothing adopts the device
    // again, and let a thread of its own do the waiting. `share` itself is
    // left alone: the model holds the lock context it was given and goes on
    // calling it until it stops, which is exactly what the caller is being
    // told to keep alive until `finish`.
    {
        std::lock_guard<std::mutex> share_lock(s.share_mutex);
        if (s.has_share && s.share.owner == owner) {
            s.has_share = false;
            s.share_pending = false;
        }
    }
    std::thread([&s, owner, finish, context] {
        {
            std::lock_guard<std::mutex> lock(s.mutex);
            nr_frame_cancel_open(0);
            TakeBackLocked(s, owner);
        }
        // The model is closed and off the device: everything it was lent can go.
        finish(context);
    }).detach();
    return true;
}

bool UsingSharedVulkan() {
    // No lock: `mutex` is held for the length of a pass (and of a model open),
    // and callers ask this from the render path.
    return Shared().shared_live.load(std::memory_order_acquire);
}

namespace {

// `save_image` in nr_frame_main.c: clip to [0, 1], then `v * 255 + 0.5` to a byte.
inline uint8_t ToByte(float v) {
    v = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
    return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

// How far a pixel may move, in levels of 255, before the pass's correction to it is
// discounted -- and how far before it is dropped altogether. Below the first, the picture
// is what the pass was given (allowing for dithering and a palette that crawls) and the
// correction applies in full; above the second it is a different picture and the
// correction belongs to the one before it. Between, it fades. These bound how far a stale
// edge can carry: at 48 a pass is worth nothing to a pixel that changed by a fifth of its
// range, which covers the moving parts of a scrolling frame while leaving still panels,
// text and backgrounds -- most of the screen, most of the time -- fully denoised.
constexpr int kHoldLevels = 3;
constexpr int kFadeLevels = 20;

// How far the motion spreads before it is weighed. What the pass changed at a pixel it
// worked out from that pixel's surroundings, so a pixel whose own colour happens to have
// survived a scroll -- a flat cell that moved onto another flat cell -- still holds a
// correction belonging to an edge that has gone. Taking the largest move in a small
// neighbourhood instead of the pixel's own drops those with the edge that made them; a
// radius of 2 covers the width a sharpened edge occupies.
//
// Wider than a GBA pixel does not help: a pass's correction also carries a halo of the
// history around a sprite, but kReleaseRadius keeps that out of the pass in the first
// place. Spread wider here, the correction is dropped to the cast (kCastBits) over a box
// around anything that moves, and the box shows. The radius is for a frame at 2x; see
// MotionRadius() for larger ones.
constexpr int kMotionRadius = 2;

// kMotionRadius at a frame `height` tall. It was set on a GBA frame at 2x (320 tall), but
// the edge it covers is a GBA pixel wide at any scale, and so is the network's own halo
// around a sprite: measured on the flat sand around Link in the NES Zelda, a pass's
// correction stands out from the sand's for 2 pixels at xBRZ 2x and for 6 at 6x. At 6x the
// fixed 2 pixels were a third of a GBA pixel, so once Link had walked on, the rest of his
// halo stayed where he had been, laid down at full weight over sand that had never changed,
// and a faint light outline of him trailed behind him. So it grows with the frame, as
// kScrollReach does, and is never less than at 2x. On the motion grid (MotionStep()) it
// is rounded up to whole grid points.
inline int MotionRadius(int height) {
    return std::max(kMotionRadius, (kMotionRadius * height + 160) / 320);
}

// When the picture as a whole is moving, how much of it has to be moving before the
// correction beyond the cast (kCastBits) is wound down everywhere, as a percentage of the
// frame. A pixel counts as moving when it is past kHoldLevels -- the question here is how
// much of the frame is not standing still, not how much of it changed beyond recognition.
//
// Weighing a pixel on its own surroundings cannot see a scroll through a soft gradient:
// there each pixel's colour changes by a level or two, under kHoldLevels, so the gate reads
// a still picture and lays the correction down in full, out of place. Measured on a frame
// scrolled two pixels, the shallow half's pixels moved by 2.3 levels and kept 2.5 levels of
// a correction belonging to where they used to be -- faint, grey and patchy.
//
// The detailed parts of that same frame moved unmistakably, though, so the frame knows it
// is scrolling even where a gradient hides it. Below kScenePercentHold the picture is
// sitting still and nothing changes; at kScenePercentDrop it is all moving and the
// correction is worth nothing until the next pass lands, a few hundred milliseconds where
// nobody is studying the denoising anyway.
constexpr int kScenePercentHold = 3;
constexpr int kScenePercentDrop = 25;

// Up to how far a pixel may have moved, in levels, and still count towards the scene's
// motion. The scene weight is for a scroll the tracking below did not find -- there the
// gradients move by a level or two and the detail by a few tens -- and what counts is how
// much of the frame shows that kind of residue once the picture is lined up. A pixel that
// changed beyond this is something else: a sprite walking, a HUD element coming and going,
// water animating. Those are local, the pass is simply wrong there and the correction
// memory or the cast stands in for it, and they say nothing about the frame as a whole;
// counted in, a hero and a few pools wound the whole picture's denoising down by a fifth
// every time the camera followed him (measured on Minish Cap: 7% "moving", 2% mild), the
// picture pumping darker as he walked and back as he stopped. Nor does the strip of fresh
// picture a scroll brings in at the frame's edge count, which the pass never saw at all.
constexpr int kSceneMildLevels = 60;

// How many consecutive frames a pixel has to hold still (within kHoldLevels) before a
// pass's correction of it goes into the correction memory as background. A pass alone
// cannot tell background from a sprite standing on it; the live frames can: a sprite that
// walks changes its pixels every frame or two (it moves, and its walk cycle turns), while
// the ground does not change at all. Agreement between two consecutive passes was tried
// first and fails for exactly the sprite that matters, the slow walker -- his flat tunic
// overlaps itself from one pass to the next, so he became background and his own
// correction was remembered where he had been. The pixel has to have stood since the pass
// was given its frame as well (UpdateMemory()), so what it shows is what the pass was given.
constexpr int kMemoryStillFrames = 12;

// The passes' correction of the background, kept from one pass to the next: see
// Filter::Impl::memory_*. A pass has no correction for what was behind a sprite, so when
// the sprite walks on, the pixels it uncovers can only take the cast -- flat, where the
// sand around them keeps the pass's sharpening of every grain, and a few levels off --
// and a dull copy of the sprite follows it across the screen at the pass's latency,
// measured at 2 to 4 levels darker than the pass itself would make it. The background
// behind him was on screen before he got there, though, and some earlier pass corrected
// it. So the correction of every pixel that has held still for kMemoryStillFrames is
// remembered, carried along with the scroll between the passes, and where the pass on
// screen no longer matches the live picture, this memory is tried before the cast. A
// sprite standing still long enough becomes memory too, which costs nothing: when it
// leaves, the live colour no longer matches what is remembered, and the cast takes over
// as before.

// How fast the frame's weight may climb back, per frame, out of 256.
//
// The weight scales the whole correction, and the correction is a colour shift as much as a
// sharpening -- measured at intensity 150, a still frame's is about +2 red, +6 green and +7
// blue. Letting the weight jump means letting that cast switch on and off within a frame,
// so a picture with intermittent motion -- a blinking cursor, a flag, a cycling palette --
// pumps between two colour balances. Climbing back over about half a second instead makes
// the return invisible.
//
// Falling is quicker than climbing, but not instant either. Only a sustained scroll needs
// the frame weighed down at all -- a single frame's blink is covered by the pixel's own
// weight and its neighbours' -- and dropping within a frame would flash the cast off and
// on around every blink, which is the same pumping seen from the other side. Eight frames
// down reaches nothing well inside a scroll, while a blink barely moves it.
constexpr int kSceneRisePerFrame = 8;
constexpr int kSceneFallPerFrame = 32;

// How fast a newly finished pass replaces the one on screen, per frame, out of 256.
//
// A pass's correction is a colour balance as much as a sharpening, so whenever it differs
// from the one on screen -- which it does as soon as the picture the model was given
// differs, and a game's quiet screen is rarely frozen to the bit -- swapping it in whole
// lands that difference in a single frame, about once a second. Crossing over about a
// quarter of a second instead spreads it, and finishes long before the next pass arrives.
//
// Where the picture really is frozen and nothing else changes, every pass produces exactly
// the same correction (libnr_frame is bit-exact for a given input, checked at
// nr_frame_update, at the features/network/compose halves, and through this filter), so
// there is nothing to cross and this costs nothing. What it does cost is a fade-up over
// the first quarter second, the opening correction arriving out of nothing.
constexpr int kResultFadePerFrame = 16;

// The adaptive frame rate (Settings::target_fps). The pipeline starts a pass as soon as the
// one before lets it, so the GPU is busy however little the picture needs: on a Radeon 780M
// twenty passes a second, the GPU 90% busy and the package at 50 W against 14 idle, which a
// laptop sheds by slowing the CPU and the emulator with it. So the frame rate the host
// shows is measured here, by the calls to Apply32() -- one a drawn frame -- and the passes
// are spaced out while it falls short. Each frame still gets the newest pass laid over it
// (see Apply32()), so fewer passes means the denoising refreshes less often, not a slower
// picture.
//
// Every kGovernorWindowMs the rate over the window is held against the target: under
// kGovernorLowRatio the least time between the starts of two passes grows by a half (and
// by at least kGovernorStepMs), at kGovernorHighRatio or over for kGovernorRelaxWindows
// windows running it shrinks by a fifth, between the two it holds. The low mark keeps the
// rate within 5% of the target, inside the 7% asked of it, and the gap between the marks
// keeps the spacing from hunting.
//
// The edge is sharp -- a frame either fits its 16.7 ms or it does not -- so shrinking freely
// walked the spacing back over it every few seconds and lost a second each time (17 of 150
// under 93% in a five minute run on a hot laptop). So the spacing that fell short is
// remembered, kGovernorMarginRatio over it, and the shrinking stops there; the mark itself
// comes down by kGovernorForgetRatio a window, to find the room a cooling laptop gives back.
//
// A gap of kGovernorMaxGapMs, a pass a second, is the most it backs off. A rate held down by
// something else -- a display slower than the target under vsync, an emulator slow on its
// own, the host's drawing -- gains nothing from that and loses the denoising. So a rate
// still short after kGovernorTrialWindows windows at the most spacing is put to the test:
// the spacing is lifted for a window to settle and kGovernorTrialWindows more, and unless
// the rate without it is kGovernorGainRatio of the target lower, the governor stands down
// and lets the passes run freely for kGovernorRetryMs before it tries again, or sooner if
// the rate falls twice kGovernorGainRatio under what the test measured. Where it does
// help it goes back to the spacing and tests again after kGovernorRetestMs, twice that the
// time after, up to kGovernorMaxRetestMs; where the spacing matters the test is a stutter,
// so one window clearly worse without it ends the test there. A rate measured
// earlier would not do as the comparison: the first windows include the start-up, and a
// game's load changes from scene to scene. The spacing can matter a lot -- xBRZ 6x with the
// layer after the filter, on a hot 780M with 7 ms of stand-in GUI work: 38 fps unspaced,
// 57 at a pass a second -- since the filter's own work on the CPU each frame doubles when
// passes land often, on top of the GPU's heat.
//
// A call kGovernorPauseMs after the one before (a pause, a menu, a load) starts a new
// window rather than counting as a slow frame.
constexpr int kGovernorWindowMs = 500;
constexpr double kGovernorLowRatio = 0.95;
constexpr double kGovernorHighRatio = 0.985;
constexpr int kGovernorRelaxWindows = 2;
constexpr double kGovernorStepMs = 20.0;
constexpr double kGovernorMaxGapMs = 1000.0;
constexpr double kGovernorMinGapMs = 5.0;
constexpr double kGovernorMarginRatio = 1.2;
constexpr double kGovernorForgetRatio = 0.99;
constexpr int kGovernorPauseMs = 250;
constexpr int kGovernorTrialWindows = 4;
constexpr double kGovernorGainRatio = 0.02;
constexpr int kGovernorRetryMs = 60000;
constexpr int kGovernorRetestMs = 120000;
constexpr int kGovernorMaxRetestMs = 960000;

class Governor {
public:
    using Clock = std::chrono::steady_clock;

    // Counts a drawn frame at `now` against `target` frames a second; 0 turns it off.
    void Frame(Clock::time_point now, double target) {
        if (target <= 0.0) {
            *this = Governor();
            return;
        }
        if (frames_ < 0 || now - last_ > std::chrono::milliseconds(kGovernorPauseMs)) {
            start_ = now;
            frames_ = 0;
        } else {
            frames_++;
        }
        last_ = now;
        const double elapsed = std::chrono::duration<double, std::milli>(now - start_).count();
        if (elapsed < kGovernorWindowMs)
            return;
        const double ratio = frames_ * 1000.0 / elapsed / target;
        start_ = now;
        frames_ = 0;

        switch (state_) {
        case State::kStandingDown:
            // Over, or cut short by a rate fallen below the one the test measured: the
            // conditions it was made in (a cool chip, a light scene) are gone.
            trial_sum_ += ratio;
            if (++trial_windows_ >= kGovernorTrialWindows) {
                const bool fell =
                    trial_sum_ / trial_windows_ < unspaced_ratio_ - 2 * kGovernorGainRatio;
                trial_sum_ = 0.0;
                trial_windows_ = 0;
                if (fell)
                    until_ = now;
            }
            if (now < until_)
                return;
            state_ = State::kSpacing;
            break;
        case State::kTesting:
            // The first window after lifting the spacing lets the pipeline fill. One clearly
            // worse window settles it: where the spacing matters the test is a stutter.
            if (test_windows_++ > 0)
                test_sum_ += ratio;
            if (test_windows_ <= kGovernorTrialWindows &&
                !(test_windows_ > 1 && ratio + 2 * kGovernorGainRatio <= spaced_ratio_))
                return;
            unspaced_ratio_ = test_sum_ / (test_windows_ - 1);
            if (unspaced_ratio_ + kGovernorGainRatio <= spaced_ratio_) {
                gap_ms_ = saved_gap_ms_;
                floor_ms_ = saved_floor_ms_;
                state_ = State::kSpacing;
                until_ = now + retest_;
                retest_ = std::min(retest_ * 2, std::chrono::milliseconds(kGovernorMaxRetestMs));
            } else {
                retest_ = std::chrono::milliseconds(kGovernorRetestMs);
                floor_ms_ = 0.0;
                state_ = State::kStandingDown;
                until_ = now + std::chrono::milliseconds(kGovernorRetryMs);
            }
            trial_sum_ = 0.0;
            trial_windows_ = 0;
            relax_ = 0;
            return;
        case State::kSpacing:
            break;
        }

        // Short at the most spacing for long enough: test whether the spacing buys anything.
        if (gap_ms_ >= kGovernorMaxGapMs && ratio < kGovernorLowRatio) {
            trial_sum_ += ratio;
            if (++trial_windows_ >= kGovernorTrialWindows && now >= until_) {
                spaced_ratio_ = trial_sum_ / trial_windows_;
                saved_gap_ms_ = gap_ms_;
                saved_floor_ms_ = floor_ms_;
                gap_ms_ = 0.0;
                test_sum_ = 0.0;
                test_windows_ = 0;
                state_ = State::kTesting;
                return;
            }
        } else {
            trial_sum_ = 0.0;
            trial_windows_ = 0;
        }

        floor_ms_ *= kGovernorForgetRatio;
        if (ratio < kGovernorLowRatio) {
            floor_ms_ = std::min(kGovernorMaxGapMs,
                                 std::max(floor_ms_, gap_ms_ * kGovernorMarginRatio));
            gap_ms_ = std::min(kGovernorMaxGapMs,
                               std::max(gap_ms_ * 1.5, gap_ms_ + kGovernorStepMs));
            relax_ = 0;
        } else if (ratio >= kGovernorHighRatio) {
            if (++relax_ >= kGovernorRelaxWindows) {
                gap_ms_ = std::max(gap_ms_ * 0.8, floor_ms_);
                if (gap_ms_ < kGovernorMinGapMs)
                    gap_ms_ = 0.0;
                relax_ = 0;
            }
        } else {
            relax_ = 0;
        }
    }

    // Whether a pass may start at `now`, and that one did.
    bool MayStart(Clock::time_point now) const {
        return gap_ms_ <= 0.0 ||
               std::chrono::duration<double, std::milli>(now - started_).count() >= gap_ms_;
    }
    void Started(Clock::time_point now) { started_ = now; }

    double gap_ms() const { return gap_ms_; }

private:
    enum class State { kSpacing, kTesting, kStandingDown };

    Clock::time_point start_, last_, started_;
    int frames_ = -1;  // frames counted in the window, -1 before the first
    int relax_ = 0;
    double gap_ms_ = 0.0;
    double floor_ms_ = 0.0;  // the remembered edge, kGovernorMarginRatio over it
    State state_ = State::kSpacing;
    // kSpacing: no test before it; kStandingDown: the end of it.
    Clock::time_point until_;
    double trial_sum_ = 0.0;  // the rate over the windows short at the most spacing
    int trial_windows_ = 0;
    double spaced_ratio_ = 0.0;  // their mean, for the test
    double saved_gap_ms_ = 0.0, saved_floor_ms_ = 0.0;
    double test_sum_ = 0.0;  // the rate over the test's windows, the first left out
    int test_windows_ = 0;
    double unspaced_ratio_ = 0.0;  // their mean
    // How long the spacing is kept before the next test, doubling each time it holds up.
    std::chrono::milliseconds retest_{ kGovernorRetestMs };
};

// A pass's correction comes in two parts, and only one of them belongs to where things were.
// One is a colour cast: at intensity 150 the model shifts each colour by a few levels
// wherever it stands, so what the pass did to a colour it can do to the same colour anywhere.
// The other is the detail -- the edges it sharpened, and the halo they spread over the next
// few pixels -- which is tied to the picture the pass was given and turns into an outline of
// a sprite that has since walked off.
//
// So each pass also records the cast, as the mean change of each colour over the whole frame
// at kCastBits a channel (a GBA colour is five), and Apply32() lays that down wherever the
// picture has moved, keeping the pass's own correction for what stood still. Dropping the
// correction outright instead leaves the cast missing around anything that moves: a box of
// uncorrected picture following the sprite.
constexpr int kCastBits = 5;
constexpr size_t kCastEntries = size_t{1} << (3 * kCastBits);
// The neighbourhood a colour falls back to, two bits coarser a channel. At namespace scope
// rather than inside BuildCast(): the lambda there reads them without capturing, which is
// allowed for a constant expression and which MSVC rejects for a function-local one.
constexpr int kCoarseBits = kCastBits - 2;
constexpr size_t kCoarseEntries = size_t{1} << (3 * kCoarseBits);

inline size_t CastIndex(int r, int g, int b) {
    constexpr int shift = 8 - kCastBits;
    return (static_cast<size_t>(r >> shift) << (2 * kCastBits)) |
           (static_cast<size_t>(g >> shift) << kCastBits) | static_cast<size_t>(b >> shift);
}

// keep[i] stays all ones only where a[i] == b[i], over n bytes, sixteen at a time where the
// vector loop is built.
inline void KeepEqual(uint8_t* keep, const uint8_t* a, const uint8_t* b, int n) {
    int i = 0;
#if defined(VBAM_DLSSNR_SSE2)
    for (; i + 16 <= n; i += 16) {
        const __m128i x = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i));
        const __m128i y = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i));
        const __m128i k = _mm_loadu_si128(reinterpret_cast<const __m128i*>(keep + i));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(keep + i),
                         _mm_and_si128(k, _mm_cmpeq_epi8(x, y)));
    }
#elif defined(VBAM_DLSSNR_NEON)
    for (; i + 16 <= n; i += 16)
        vst1q_u8(keep + i,
                 vandq_u8(vld1q_u8(keep + i), vceqq_u8(vld1q_u8(a + i), vld1q_u8(b + i))));
#endif
    for (; i < n; i++)
        keep[i] &= static_cast<uint8_t>(a[i] == b[i] ? 0xff : 0);
}

// The cast of a pass that turned `in` into `out` (RGB8, `pixels` long): for each colour the
// mean change, three values an entry.
//
// A colour the frame did not have takes the mean over its neighbourhood of colours instead
// (kCastBits - 2 a channel), and failing that the frame's. The live picture is not limited
// to the pass's colours: a scroll brings in what was off screen, and interframe blending
// mixes two frames into colours neither had. Leaving those at no change drops the cast
// exactly where the picture moves, and the correction flickers off with it.
//
// Where most of a colour stands clear of any edge -- the same colour all round at twice
// MotionRadius(), out of reach of the network's halo -- the mean is over those alone.
// The cast stands in where something has just moved away, and what it uncovers is mostly open
// ground; but the ground beside every edge in the frame carries the halo of that edge, and
// counted in, it pulled the sand's cast in the NES Zelda to -11, -7, -4 against the open
// sand's -7, -6, -1. Wherever Link walked off a spot the pass still had him on, a greyer,
// darker copy of him stood there until the next pass came. A colour that mostly appears
// beside edges -- a sprite's -- keeps the mean over all of it, which carries the sharpening of
// those edges: the cast is also what a sprite takes while it walks, and taking the mean of
// its few clear pixels instead left him paler than the pass makes him.
void BuildCast(const uint8_t* in, const uint8_t* out, int width, int height,
               std::vector<int16_t>* cast, std::vector<int64_t>* sums) {
    const auto coarse_of = [](size_t k) {
        const size_t r = k >> (2 * kCastBits), g = (k >> kCastBits) & ((1u << kCastBits) - 1),
                     b = k & ((1u << kCastBits) - 1);
        constexpr int drop = kCastBits - kCoarseBits;
        return ((r >> drop) << (2 * kCoarseBits)) | ((g >> drop) << kCoarseBits) | (b >> drop);
    };
    sums->assign((2 * kCastEntries + kCoarseEntries + 1) * 4, 0);
    int64_t* const fine = sums->data();
    int64_t* const clear = fine + kCastEntries * 4;
    int64_t* const coarse = clear + kCastEntries * 4;
    int64_t* const all = coarse + kCoarseEntries * 4;
    const int r = 2 * MotionRadius(height);
    const ptrdiff_t across = 3 * static_cast<ptrdiff_t>(r), down = across * width;
    const ptrdiff_t ring[8] = { -down - across, -down, -down + across, -across,
                                across,         down - across, down, down + across };
    // Each row's bytes against the same bytes at each of the eight offsets, a whole row at a
    // time so the compares run sixteen to a vector; a pixel is clear if all three of its are.
    thread_local std::vector<uint8_t> same;
    same.resize(static_cast<size_t>(width) * 3);
    for (int y = 0; y < height; y++) {
        const bool inside = y >= r && y < height - r && width > 2 * r;
        if (inside) {
            const size_t b0 = static_cast<size_t>(across), b1 = static_cast<size_t>(width - r) * 3;
            std::fill(same.begin(), same.end(), uint8_t{0});
            std::fill(same.begin() + b0, same.begin() + b1, uint8_t{0xff});
            for (const ptrdiff_t o : ring)
                KeepEqual(same.data() + b0, in + b0, in + b0 + o, static_cast<int>(b1 - b0));
        }
        for (int x = 0; x < width; x++, in += 3, out += 3) {
            const size_t k = CastIndex(in[0], in[1], in[2]);
            int64_t* e = fine + k * 4;
            for (int c = 0; c < 3; c++)
                e[c] += out[c] - in[c];
            e[3]++;
            if (inside && same[3 * x] & same[3 * x + 1] & same[3 * x + 2]) {
                e = clear + k * 4;
                for (int c = 0; c < 3; c++)
                    e[c] += out[c] - in[c];
                e[3]++;
            }
        }
    }
    for (size_t k = 0; k < kCastEntries; k++) {
        const int64_t* e = fine + k * 4;
        int64_t* g = coarse + coarse_of(k) * 4;
        for (int c = 0; c < 4; c++) {
            g[c] += e[c];
            all[c] += e[c];
        }
    }
    const auto mean = [](const int64_t* e, int c) {
        return static_cast<int16_t>(std::lround(static_cast<double>(e[c]) / e[3]));
    };
    cast->resize(kCastEntries * 3);
    for (size_t k = 0; k < kCastEntries; k++) {
        const int64_t* e = fine + k * 4;
        if (e[3] && 2 * clear[k * 4 + 3] >= e[3])
            e = clear + k * 4;
        if (!e[3])
            e = coarse + coarse_of(k) * 4;
        if (!e[3])
            e = all;
        for (int c = 0; c < 3; c++)
            (*cast)[3 * k + c] = e[3] ? mean(e, c) : 0;
    }
}

// How much of a pass's own correction a pixel keeps, out of 256, once the picture has moved
// by `moved` levels there since the pass was given it.
inline int MotionWeight(int moved) {
    return moved <= kHoldLevels
               ? 256
               : (moved >= kFadeLevels
                      ? 0
                      : 256 - (moved - kHoldLevels) * 256 / (kFadeLevels - kHoldLevels));
}

// The largest value within `r` of each pixel, in place, separably: across each row, then
// down, a whole row at a time so both passes run along memory.

// The overlay's per-frame work -- the motion sweeps, the dilation and the
// composition -- is row-independent: every band writes only its own rows and
// reads inputs that no band writes. Splitting it across a small persistent pool
// is therefore exact, not an approximation; the output is the same bytes.
//
// A pool rather than threads per frame: this runs sixty times a second, and
// creating threads that often costs more than the work.
class RowPool {
public:
    static RowPool& Get() {
        static RowPool pool;
        return pool;
    }
    int bands() const { return static_cast<int>(workers_.size()) + 1; }

    // Calls fn(y0, y1) over half-open row bands covering [0, height), the
    // caller taking one of them. Returns once every band has finished.
    //
    // `limit` caps the split for work too small to be worth spreading wide.
    // Measured on two 8-core/16-thread Ryzen APUs, one Linux/gcc and one
    // Windows/MSVC, over 400 runs of Dilate at 480x320: four bands is the best
    // either machine does, and eight is slower than not splitting at all on the
    // Windows one -- a Dilate call is a few tens of microseconds, and waking
    // seven threads costs more than the rows save.
    void Run(int height, const std::function<void(int, int)>& fn, int limit = 0) {
        // One job at a time: Apply32() runs here on the emulation thread and
        // ReleaseHistory() on the features stage's, and a second job started over a
        // running one would reset its row counter and its tally under it.
        std::lock_guard<std::mutex> one(run_mutex_);
        int n = bands();
        if (limit > 0)
            n = std::min(n, limit);
        // Not worth waking anyone for a handful of rows.
        if (n <= 1 || height < 2 * n) {
            fn(0, height);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            fn_ = &fn;
            height_ = height;
            bands_ = n;
            // A few rows at a time, taken as each thread comes free, rather than
            // one fixed slice each: the rows are not equally dear. measure()
            // leaves a row early where the hypothesis puts it off the pass's
            // edge, so a scroll makes the first or last rows nearly free, and a
            // fixed slice leaves whoever drew them idle while the rest finish.
            chunk_ = std::max(1, height / (n * chunk_divisor_));
            next_row_.store(0, std::memory_order_relaxed);
            // Every worker wakes and reports back, including the ones this job
            // is too small to use: they skip the work but still count down, so
            // the tally has to be all of them, not just the bands in use.
            remaining_ = static_cast<int>(workers_.size());
            ++generation_;
        }
        start_.notify_all();
        Take(fn, height);
        std::unique_lock<std::mutex> lock(mutex_);
        done_.wait(lock, [this] { return remaining_ == 0; });
        fn_ = nullptr;
    }

private:
    // Draws chunks until the rows run out. One atomic per chunk, not per row.
    void Take(const std::function<void(int, int)>& fn, int height) {
        const int chunk = chunk_;
        for (;;) {
            const int y = next_row_.fetch_add(chunk, std::memory_order_relaxed);
            if (y >= height)
                return;
            fn(y, std::min(height, y + chunk));
        }
    }
    RowPool() {
        const unsigned hw = std::thread::hardware_concurrency();
        // The frames here are small; past a handful of bands the waking costs
        // more than the rows save.
        int n = std::min(hw ? static_cast<int>(hw) : 1, 8);
        for (int i = 1; i < n; i++)
            workers_.emplace_back([this, i] { Work(i); });
    }
    ~RowPool() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            quit_ = true;
            ++generation_;
        }
        start_.notify_all();
        for (std::thread& t : workers_)
            if (t.joinable())
                t.join();
    }
    void Work(int index) {
        unsigned long long seen = 0;
        for (;;) {
            std::unique_lock<std::mutex> lock(mutex_);
            start_.wait(lock, [this, seen] { return quit_ || generation_ != seen; });
            if (quit_)
                return;
            seen = generation_;
            const std::function<void(int, int)>* fn = fn_;
            const int height = height_, n = bands_;
            lock.unlock();
            if (fn && index < n)
                Take(*fn, height);
            lock.lock();
            if (--remaining_ == 0) {
                lock.unlock();
                done_.notify_one();
            }
        }
    }
    std::vector<std::thread> workers_;
    std::mutex run_mutex_;  // held for a whole Run()
    std::mutex mutex_;
    std::condition_variable start_, done_;
    const std::function<void(int, int)>* fn_ = nullptr;
    int height_ = 0, bands_ = 0, remaining_ = 0, chunk_ = 1;
    // chunks per band; NR_CHUNK_DIV only so the choice can be measured
    int chunk_divisor_ = [] {
        const char* e = getenv("NR_CHUNK_DIV");
        const int v = e ? atoi(e) : 0;
        return v > 0 ? v : 4;
    }();
    std::atomic<int> next_row_{0};
    unsigned long long generation_ = 0;
    bool quit_ = false;
};

// See RowPool::Run: measured, not guessed.
constexpr int kDilateBands = 4;

// out[i] = max(out[i], in[i]) over n bytes, sixteen at a time where the vector loop is
// built; the scalar loop is what the compiler was already making of it.
inline void MaxInto(uint8_t* out, const uint8_t* in, int n) {
    int i = 0;
#if defined(VBAM_DLSSNR_SSE2)
    for (; i + 16 <= n; i += 16) {
        const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(out + i));
        const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out + i), _mm_max_epu8(a, b));
    }
#elif defined(VBAM_DLSSNR_NEON)
    for (; i + 16 <= n; i += 16)
        vst1q_u8(out + i, vmaxq_u8(vld1q_u8(out + i), vld1q_u8(in + i)));
#endif
    for (; i < n; i++)
        out[i] = std::max(out[i], in[i]);
}

// Dilates `count` images of one size together: each row as Dilate() always did it, in two
// Run()s for all of them rather than two each -- a scrolling frame dilates up to nine masks,
// and waking the pool costs more than one mask's rows.
void DilateAll(uint8_t* const* images, uint8_t* const* scratch, int count, int width,
               int height, int r) {
    const int rows = height * count;
    // Across, then down. The second pass reads what the first wrote for rows it
    // does not own, so the two cannot overlap -- hence two Run()s, not one.
    RowPool::Get().Run(rows, [&](int ya, int yb) {
        for (int j = ya; j < yb; j++) {
            const int k = j / height, y = j % height;
            const uint8_t* row = images[k] + static_cast<size_t>(y) * width;
            uint8_t* out = scratch[k] + static_cast<size_t>(y) * width;
            std::memcpy(out, row, static_cast<size_t>(width));
            for (int o = 1; o <= r; o++) {
                MaxInto(out, row + o, width - o);
                MaxInto(out + o, row, width - o);
            }
        }
    }, count == 1 ? kDilateBands : 0);
    RowPool::Get().Run(rows, [&](int ya, int yb) {
        for (int j = ya; j < yb; j++) {
            const int k = j / height, y = j % height;
            const uint8_t* across = scratch[k];
            uint8_t* out = images[k] + static_cast<size_t>(y) * width;
            const int y0 = std::max(0, y - r), y1 = std::min(height - 1, y + r);
            std::memcpy(out, across + static_cast<size_t>(y0) * width, static_cast<size_t>(width));
            for (int q = y0 + 1; q <= y1; q++)
                MaxInto(out, across + static_cast<size_t>(q) * width, width);
        }
    }, count == 1 ? kDilateBands : 0);
}

void Dilate(std::vector<uint8_t>* image, int width, int height, int r,
            std::vector<uint8_t>* scratch) {
    scratch->resize(static_cast<size_t>(width) * height);
    uint8_t* im = image->data();
    uint8_t* across = scratch->data();
    DilateAll(&im, &across, 1, width, height, r);
}

// The motion is worked out on a grid of every `step`th pixel each way when the frame is at
// least twice the extent the model works at (Settings::max_height): a frame that large is a
// filter's output scaled up, its GBA pixels several frame pixels wide, so the grid still
// sees every edge that moves, and the measuring, counting and spreading cost a step^2th of
// what they did -- at xBRZ 6x, 1440x960 against 480 tall, a quarter. Only the composition
// stays at full size. Below twice the extent the grid is the frame.
inline int MotionStep(int height, int filter_height) {
    return filter_height > 0 ? std::max(1, height / filter_height) : 1;
}

// `channels`-byte pixels of a `width` x `height` image at the centre of each `step` x `step`
// block (the last ones clipped to the image) into `out`, a grid of ceil(width / step) x
// ceil(height / step).
void Decimate(const uint8_t* src, int width, int height, int step, int channels,
              std::vector<uint8_t>* out) {
    const int gw = (width + step - 1) / step, gh = (height + step - 1) / step;
    out->resize(static_cast<size_t>(gw) * gh * channels);
    uint8_t* const o = out->data();
    RowPool::Get().Run(gh, [&](int ya, int yb) {
        for (int gy = ya; gy < yb; gy++) {
            const int y = std::min(gy * step + step / 2, height - 1);
            const uint8_t* row = src + static_cast<size_t>(y) * width * channels;
            uint8_t* d = o + static_cast<size_t>(gy) * gw * channels;
            // every point but the last is a whole block in; the last may be clipped
            const uint8_t* q = row + static_cast<size_t>(step / 2) * channels;
            const size_t stride = static_cast<size_t>(step) * channels;
            if (channels == 3) {
                for (int gx = 0; gx < gw - 1; gx++, d += 3, q += stride) {
                    d[0] = q[0];
                    d[1] = q[1];
                    d[2] = q[2];
                }
            } else {
                for (int gx = 0; gx < gw - 1; gx++, d += channels, q += stride)
                    for (int c = 0; c < channels; c++)
                        d[c] = q[c];
            }
            const int last = std::min((gw - 1) * step + step / 2, width - 1);
            q = row + static_cast<size_t>(last) * channels;
            for (int c = 0; c < channels; c++)
                d[c] = q[c];
        }
    });
}

// Back from the grid: each `step` x `step` block of `out` (`width` x `height`) takes the
// value of the grid point at its centre, for `count` grids at once.
void UndecimateAll(const uint8_t* const* grids, uint8_t* const* outs, int count, int width,
                   int height, int step) {
    const int gw = (width + step - 1) / step;
    RowPool::Get().Run(height * count, [&](int ya, int yb) {
        for (int j = ya; j < yb; j++) {
            const int k = j / height, y = j % height;
            const uint8_t* g = grids[k] + static_cast<size_t>(y / step) * gw;
            uint8_t* o = outs[k] + static_cast<size_t>(y) * width;
            if (y % step != 0) {  // the block's first row is already made
                std::memcpy(o, outs[k] + static_cast<size_t>(y - y % step) * width,
                            static_cast<size_t>(width));
                continue;
            }
            if (step == 2) {
                for (int gx = 0; gx < width / 2; gx++)
                    o[2 * gx] = o[2 * gx + 1] = g[gx];
                if (width & 1)
                    o[width - 1] = g[gw - 1];
            } else {
                for (int x = 0; x < width; x++)
                    o[x] = g[x / step];
            }
        }
    });
}

// How far the picture may scroll between a pass's input and the frame it is laid over, in
// GBA pixels -- scaled to the frame -- before Apply32() stops following it. A game's camera
// follows its hero, so while Link walks the whole background shifts by a pixel or two every
// few frames, and a pass is several frames old by the time it is shown. Compared where it
// stands, every edge of the background then reads as moved, the frame as a whole as
// scrolling, and the correction is wound down to the cast until the picture holds still
// again -- the denoising flickering off and back on as he walks. A scroll is a whole-pixel
// shift of the background, though, so Apply32() finds it and takes the pass's correction
// from where each pixel was, keeping the unshifted one where that fits better (the HUD,
// anything that stayed put).
constexpr int kScrollReach = 24;

// A luminance pyramid, each level half the one before, for finding the scroll.
struct Pyramid {
    static constexpr int kMaxLevels = 6;
    std::array<std::vector<uint8_t>, kMaxLevels> level;
    std::array<int, kMaxLevels> width{};
    std::array<int, kMaxLevels> height{};
    int levels = 0;
};

// Levels enough for kScrollReach: the top level is searched four pixels each way.
int PyramidLevels(int width, int height) {
    const int reach = kScrollReach * height / 160;
    int levels = 1;
    while (levels < Pyramid::kMaxLevels && (4 << (levels - 1)) < reach &&
           (width >> levels) >= 16 && (height >> levels) >= 16)
        levels++;
    return levels;
}

void BuildPyramid(const uint8_t* rgb, int width, int height, int levels, Pyramid* p) {
    p->levels = levels;
    p->width[0] = width;
    p->height[0] = height;
    std::vector<uint8_t>& base = p->level[0];
    base.resize(static_cast<size_t>(width) * height);
    // The full-size level is most of the work: a frame's worth of pixels, on the pool.
    uint8_t* const b = base.data();
    RowPool::Get().Run(height, [&](int ya, int yb) {
        const size_t end = static_cast<size_t>(yb) * width;
        for (size_t i = static_cast<size_t>(ya) * width; i < end; i++)
            b[i] = static_cast<uint8_t>((rgb[3 * i] + 2 * rgb[3 * i + 1] + rgb[3 * i + 2] + 2) >> 2);
    });
    for (int l = 1; l < levels; l++) {
        const int uw = p->width[l - 1];
        const int w = uw / 2;
        const int h = p->height[l - 1] / 2;
        p->width[l] = w;
        p->height[l] = h;
        const uint8_t* up = p->level[l - 1].data();
        std::vector<uint8_t>& out = p->level[l];
        out.resize(static_cast<size_t>(w) * h);
        for (int y = 0; y < h; y++) {
            const uint8_t* r0 = up + static_cast<size_t>(2 * y) * uw;
            const uint8_t* r1 = r0 + uw;
            for (int x = 0; x < w; x++)
                out[static_cast<size_t>(y) * w + x] = static_cast<uint8_t>(
                    (r0[2 * x] + r0[2 * x + 1] + r1[2 * x] + r1[2 * x + 1] + 2) >> 2);
        }
    }
}

// The mean difference between `live` and `pass` shifted by (dx, dy) at level `l`, every
// `step`th pixel of where they overlap; huge when they hardly do.
double ShiftCost(const Pyramid& live, const Pyramid& pass, int l, int dx, int dy, int step) {
    const int w = live.width[l];
    const int h = live.height[l];
    const int x0 = std::max(0, dx), x1 = std::min(w, w + dx);
    const int y0 = std::max(0, dy), y1 = std::min(h, h + dy);
    if ((x1 - x0) * 2 < w || (y1 - y0) * 2 < h)
        return 1e9;
    const uint8_t* a = live.level[l].data();
    const uint8_t* b = pass.level[l].data();
    uint64_t sum = 0, n = 0;
    for (int y = y0; y < y1; y += step) {
        const uint8_t* ar = a + static_cast<size_t>(y) * w;
        const uint8_t* br = b + static_cast<size_t>(y - dy) * w - dx;
        int x = x0;
#if defined(VBAM_DLSSNR_SSE2)
        if (step == 1 || step == 2 || step == 4) {
            // Sixteen bytes at a time, the ones between samples zeroed in both, so they add
            // nothing: the same integer total as the loop below.
            const __m128i keep = step == 1 ? _mm_set1_epi8(-1)
                               : step == 2 ? _mm_set1_epi16(0x00FF)
                                           : _mm_set1_epi32(0x000000FF);
            __m128i acc = _mm_setzero_si128();
            for (; x + 16 <= x1; x += 16)
                acc = _mm_add_epi64(acc, _mm_sad_epu8(
                    _mm_and_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(ar + x)), keep),
                    _mm_and_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(br + x)), keep)));
            alignas(16) uint64_t lanes[2];
            _mm_store_si128(reinterpret_cast<__m128i*>(lanes), acc);
            sum += lanes[0] + lanes[1];
            n += static_cast<uint64_t>((x - x0) / step);
        }
#elif defined(VBAM_DLSSNR_NEON)
        if (step == 1 || step == 2 || step == 4) {
            // As above: the absolute differences, those between samples masked off, summed.
            const uint8x16_t keep = step == 1 ? vdupq_n_u8(0xFF)
                                  : step == 2 ? vreinterpretq_u8_u16(vdupq_n_u16(0x00FF))
                                              : vreinterpretq_u8_u32(vdupq_n_u32(0x000000FF));
            uint32x4_t acc = vdupq_n_u32(0);
            for (; x + 16 <= x1; x += 16)
                acc = vpadalq_u16(
                    acc, vpaddlq_u8(vandq_u8(vabdq_u8(vld1q_u8(ar + x), vld1q_u8(br + x)), keep)));
            sum += vaddvq_u32(acc);
            n += static_cast<uint64_t>((x - x0) / step);
        }
#endif
        for (; x < x1; x += step, n++)
            sum += static_cast<uint64_t>(std::abs(ar[x] - br[x]));
    }
    return n ? static_cast<double>(sum) / static_cast<double>(n) : 1e9;
}

// The scroll from a pass's input to the live frame, live(x, y) ~ pass(x - dx, y - dy):
// searched at the top of the pyramid, then refined a level at a time.
void EstimateScroll(const Pyramid& live, const Pyramid& pass, int* dx, int* dy) {
    *dx = 0;
    *dy = 0;
    if (live.levels == 0 || live.levels != pass.levels || live.width[0] != pass.width[0] ||
        live.height[0] != pass.height[0])
        return;
    int bx = 0, by = 0;
    double best = 1e18;
    for (int l = live.levels - 1; l >= 0; l--) {
        const bool top = l == live.levels - 1;
        const int reach = top ? 4 : 2;
        const int cx = top ? 0 : 2 * bx;
        const int cy = top ? 0 : 2 * by;
        const int step = l == 0 ? 4 : l == 1 ? 2 : 1;
        // Every candidate's cost on the pool, then the choice in the order the loop always
        // made it, so the same one wins.
        const int side = 2 * reach + 1;
        std::array<double, 81> costs;
        RowPool::Get().Run(side * side, [&](int ka, int kb) {
            for (int k = ka; k < kb; k++)
                costs[k] = ShiftCost(live, pass, l, cx - reach + k % side, cy - reach + k / side,
                                     step);
        });
        best = 1e18;
        for (int k = 0; k < side * side; k++)
            if (costs[k] < best) {
                best = costs[k];
                bx = cx - reach + k % side;
                by = cy - reach + k / side;
            }
    }
    // Standing still wins a tie, so a still picture never wanders.
    if ((bx || by) && ShiftCost(live, pass, 0, 0, 0, 4) <= best + 0.5)
        bx = by = 0;
    *dx = bx;
    *dy = by;
}

// One way to line a pass up with the live frame: the mean of the pass shifted by (ax, ay)
// and by (bx, by), live(x, y) ~ (pass(x - ax, y - ay) + pass(x - bx, y - by)) / 2 -- a plain
// shift when the two are the same.
struct Hypothesis {
    int ax = 0, ay = 0, bx = 0, by = 0;
};

// |l - (a + b + 1) / 2| a byte at a time over n bytes: what MeasureRows asks of every
// channel, without looking at which channel a byte is. `_mm_avg_epu8` (NEON's `vrhaddq_u8`) is
// that very mean, and the two saturating differences are the absolute one.
inline void AbsDiffMean(const uint8_t* l, const uint8_t* a, const uint8_t* b, uint8_t* out,
                        size_t n) {
    size_t i = 0;
#if defined(VBAM_DLSSNR_SSE2)
    for (; i + 16 <= n; i += 16) {
        const __m128i lv = _mm_loadu_si128(reinterpret_cast<const __m128i*>(l + i));
        const __m128i g = _mm_avg_epu8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i)),
                                       _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i)));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out + i),
                         _mm_or_si128(_mm_subs_epu8(lv, g), _mm_subs_epu8(g, lv)));
    }
#elif defined(VBAM_DLSSNR_NEON)
    for (; i + 16 <= n; i += 16)
        vst1q_u8(out + i, vabdq_u8(vld1q_u8(l + i), vrhaddq_u8(vld1q_u8(a + i), vld1q_u8(b + i))));
#endif
    for (; i < n; i++) {
        const int g = (a[i] + b[i] + 1) >> 1;
        out[i] = static_cast<uint8_t>(l[i] > g ? l[i] - g : g - l[i]);
    }
}

// Over a motion mask -- or the least of two or three, a pixel at a time, where `b` and `c`
// are given -- how many pixels moved at all (past kHoldLevels) and how many of those only
// mildly (kSceneMildLevels at most). On the pool, sixteen pixels at a time.
void CountMotion(const uint8_t* a, const uint8_t* b, const uint8_t* c, size_t n,
                 size_t* moving, size_t* mild) {
    std::atomic<size_t> total_moving{0}, total_mild{0};
    constexpr int kRows = 64;  // pixels go to the pool in rows of n / kRows
    const size_t row = (n + kRows - 1) / kRows;
    RowPool::Get().Run(kRows, [&](int ya, int yb) {
        size_t i = std::min(n, static_cast<size_t>(ya) * row);
        const size_t end = std::min(n, static_cast<size_t>(yb) * row);
        size_t mv = 0, ml = 0;
#if defined(VBAM_DLSSNR_SSE2)
        const __m128i zero = _mm_setzero_si128();
        const __m128i one = _mm_set1_epi8(1);
        const __m128i hold = _mm_set1_epi8(static_cast<char>(kHoldLevels));
        const __m128i mild_top = _mm_set1_epi8(static_cast<char>(kSceneMildLevels));
        __m128i acc_moving = zero, acc_mild = zero;
        for (; i + 16 <= end; i += 16) {
            __m128i m = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i));
            if (b)
                m = _mm_min_epu8(m, _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i)));
            if (c)
                m = _mm_min_epu8(m, _mm_loadu_si128(reinterpret_cast<const __m128i*>(c + i)));
            // all ones where m <= kHoldLevels, and where m <= kSceneMildLevels
            const __m128i still = _mm_cmpeq_epi8(_mm_subs_epu8(m, hold), zero);
            const __m128i within = _mm_cmpeq_epi8(_mm_subs_epu8(m, mild_top), zero);
            acc_moving = _mm_add_epi64(acc_moving,
                                       _mm_sad_epu8(_mm_andnot_si128(still, one), zero));
            acc_mild = _mm_add_epi64(
                acc_mild, _mm_sad_epu8(_mm_and_si128(_mm_andnot_si128(still, within), one), zero));
        }
        alignas(16) uint64_t lanes[2];
        _mm_store_si128(reinterpret_cast<__m128i*>(lanes), acc_moving);
        mv += lanes[0] + lanes[1];
        _mm_store_si128(reinterpret_cast<__m128i*>(lanes), acc_mild);
        ml += lanes[0] + lanes[1];
#elif defined(VBAM_DLSSNR_NEON)
        const uint8x16_t hold = vdupq_n_u8(kHoldLevels);
        const uint8x16_t mild_top = vdupq_n_u8(kSceneMildLevels);
        uint32x4_t acc_moving = vdupq_n_u32(0), acc_mild = vdupq_n_u32(0);
        for (; i + 16 <= end; i += 16) {
            uint8x16_t m = vld1q_u8(a + i);
            if (b)
                m = vminq_u8(m, vld1q_u8(b + i));
            if (c)
                m = vminq_u8(m, vld1q_u8(c + i));
            // 1 where m > kHoldLevels, and where it is kSceneMildLevels at most as well
            const uint8x16_t moved = vshrq_n_u8(vcgtq_u8(m, hold), 7);
            const uint8x16_t mildly = vandq_u8(moved, vcleq_u8(m, mild_top));
            acc_moving = vpadalq_u16(acc_moving, vpaddlq_u8(moved));
            acc_mild = vpadalq_u16(acc_mild, vpaddlq_u8(mildly));
        }
        mv += vaddvq_u32(acc_moving);
        ml += vaddvq_u32(acc_mild);
#endif
        for (; i < end; i++) {
            uint8_t m = a[i];
            if (b)
                m = std::min(m, b[i]);
            if (c)
                m = std::min(m, c[i]);
            mv += m > kHoldLevels;
            ml += m > kHoldLevels && m <= kSceneMildLevels;
        }
        total_moving.fetch_add(mv, std::memory_order_relaxed);
        total_mild.fetch_add(ml, std::memory_order_relaxed);
    });
    *moving = total_moving.load();
    *mild = total_mild.load();
}

// How far each pixel has moved since the pass was given the frame, if the pass
// is taken as the mean of itself shifted by `h.a` and by `h.b` -- one shift
// when they are the same: the largest step of the three channels, or all of it
// where a shift takes the pixel off the pass's edge.
//
// `valid`, when given, says which of `given`'s pixels hold anything (the
// correction memory's); a pixel reading one that does not has moved all of it.
//
// Whether a pixel is reachable at all depends only on its row, its column and
// the hypothesis -- never on what the pixels hold. So the rows that no shift
// can reach are filled whole, and in the rest the reachable span is computed
// once and the loop between its edges needs no checking at all.
void MeasureRows(const uint8_t* live, const uint8_t* given, const uint8_t* valid,
                 const Hypothesis& h, int width, int height, int y_begin, int y_end,
                 uint8_t* motion) {
    // the channels' differences for one row, then the largest of each pixel's three
    thread_local std::vector<uint8_t> diff;
    diff.resize(static_cast<size_t>(width) * 3);
    for (int y = y_begin; y < y_end; y++) {
        uint8_t* mv = motion + static_cast<size_t>(y) * width;
        const int ay = y - h.ay, by = y - h.by;
        if (ay < 0 || ay >= height || by < 0 || by >= height) {
            std::memset(mv, 255, static_cast<size_t>(width));
            continue;
        }
        // ax = x - h.ax and bx = x - h.bx both inside [0, width)
        const int x0 = std::max(0, std::max(h.ax, h.bx));
        const int x1 = std::min(width, std::min(h.ax, h.bx) + width);
        if (x1 <= x0) {
            std::memset(mv, 255, static_cast<size_t>(width));
            continue;
        }
        if (x0 > 0)
            std::memset(mv, 255, static_cast<size_t>(x0));
        if (x1 < width)
            std::memset(mv + x1, 255, static_cast<size_t>(width - x1));
        const size_t n = static_cast<size_t>(x1 - x0);
        AbsDiffMean(live + (static_cast<size_t>(y) * width + x0) * 3,
                    given + (static_cast<size_t>(ay) * width + (x0 - h.ax)) * 3,
                    given + (static_cast<size_t>(by) * width + (x0 - h.bx)) * 3, diff.data(),
                    n * 3);
        const uint8_t* d = diff.data();
        uint8_t* out = mv + x0;
        for (size_t i = 0; i < n; i++, d += 3)
            out[i] = std::max(d[0], std::max(d[1], d[2]));
        if (valid) {
            const uint8_t* av = valid + static_cast<size_t>(ay) * width + (x0 - h.ax);
            const uint8_t* bv = valid + static_cast<size_t>(by) * width + (x0 - h.bx);
            // all ones where either is empty, as a mask, so it vectorises
            for (size_t i = 0; i < n; i++)
                out[i] |= static_cast<uint8_t>(-static_cast<int>((av[i] == 0) | (bv[i] == 0)));
        }
    }
}


// The ways Apply32() tries for one pass, each with its motion mask, one byte a pixel.
struct Alignment {
    int count = 0;
    std::array<Hypothesis, 3> h;
    std::array<std::vector<uint8_t>, 3> motion;
    std::array<std::vector<uint8_t>, 3> grid;  // the same on the motion grid (MotionStep())
};

// One pass's alignment as the composition reads it: each way's motion mask and the pixel
// offsets of its two shifts. `count` 0 takes nothing of the pass.
struct Lookup {
    int count = 0;
    const uint8_t* motion[3] = {};
    ptrdiff_t a[3] = {}, b[3] = {};
};

// Everything ComposeRows() reads, for the frame on screen (`width` x `height`, RGB8 `live`)
// and up to three passes: the one on screen (`now_*`), the one before it while crossing
// over (`was_*`, `previous`) and the correction memory (`mem_*`). Deltas are RGB int16 a
// pixel, casts kCastEntries x 3. `weight` is MotionWeight() times the scene's, by motion.
struct ComposeInput {
    int width = 0, height = 0;
    const uint8_t* live = nullptr;
    uint8_t* dst = nullptr;
    int outstride = 0;
    int shift[3] = {};
    const int16_t* cast = nullptr;
    const int16_t* was_cast = nullptr;
    const int16_t* now_delta = nullptr;
    const int16_t* was_delta = nullptr;
    const int16_t* mem_delta = nullptr;
    Lookup now_l, was_l, mem_l;
    const int* weight = nullptr;
    int scene = 256;  // the scene's weight, which `weight` is MotionWeight() times
    int blend = 256;
    bool crossing = false;  // blend < 256
    bool previous = false;  // and the pass before is there to cross from
    bool detail = false;    // the scene's weight is above nothing
};

// Which way a pixel takes a pass -- the one its motion fits best, the first of equals --
// and how much of the pass's own correction it keeps: it reads the mean of the pass's
// pixels at that way's two offsets. A weight of 0 reads nothing, so the offsets need no
// check when a shift took the pixel off the pass's edge: that motion is 255.
inline int Choose(const Lookup& l, const int* weight, size_t i, int* best) {
    *best = 0;
    if (l.count == 0)
        return 0;
    uint8_t m = l.motion[0][i];
    for (int k = 1; k < l.count; k++)
        if (l.motion[k][i] < m) {
            m = l.motion[k][i];
            *best = k;
        }
    return weight[m];
}

// The composition of pixels [x_begin, x_end) of row y, a pixel at a time.
template <bool kCrossing, bool kPrevious, bool kDetail>
void ComposePixels(const ComposeInput& in, int y, int x_begin, int x_end) {
    const int width = in.width;
    const int16_t* const cast = in.cast;
    uint32_t* d = reinterpret_cast<uint32_t*>(in.dst + static_cast<size_t>(y) * in.outstride);
    const uint8_t* l = in.live + (static_cast<size_t>(y) * width + x_begin) * 3;
    const size_t row = static_cast<size_t>(y) * width;
    for (int x = x_begin; x < x_end; x++, l += 3) {
        const size_t i = row + x;
        const int16_t* const now_cast = cast + CastIndex(l[0], l[1], l[2]) * 3;
        int now_k = 0, was_k = 0, mem_k = 0;
        const int w = kDetail ? Choose(in.now_l, in.weight, i, &now_k) : 0;
        const int was_w = kDetail && kPrevious ? Choose(in.was_l, in.weight, i, &was_k) : 0;
        // The memory only matters where the pass on screen (or the one
        // before, while crossing) does not fit the live picture itself.
        const int mem_w = kDetail && (w < 256 || (kCrossing && was_w < 256))
                              ? Choose(in.mem_l, in.weight, i, &mem_k)
                              : 0;
        // Crossing over, how far this pixel has come: the frame's blend, and the rest of the
        // way where the new pass fits the live picture and nothing of the old side does --
        // there the old side is only its cast, flat, and it showed as a patch where a sprite
        // had been until the crossing was done. The vector loops work it out eight at a time.
        int pixel_blend = in.blend;
        if (kCrossing && kPrevious && kDetail)
            pixel_blend += (256 - pixel_blend) * (w * (256 - std::max(was_w, mem_w)) >> 8) >> 8;
        const size_t now_a = i + in.now_l.a[now_k], now_b = i + in.now_l.b[now_k];
        const size_t was_a = i + in.was_l.a[was_k], was_b = i + in.was_l.b[was_k];
        const size_t mem_a = i + in.mem_l.a[mem_k], mem_b = i + in.mem_l.b[mem_k];
        uint32_t p = 0;
        for (int c = 0; c < 3; c++) {
            // The cast for the colour now there, then what is remembered of the
            // background here as far as the live picture still matches it,
            // then the pass's own correction as far as it does.
            int delta = now_cast[c];
            const int remembered =
                mem_w > 0 ? (in.mem_delta[3 * mem_a + c] + in.mem_delta[3 * mem_b + c]) >> 1 : 0;
            if (mem_w > 0)
                delta += (remembered - delta) * mem_w >> 8;
            if (w > 0)
                delta += (((in.now_delta[3 * now_a + c] + in.now_delta[3 * now_b + c]) >> 1) -
                          delta) *
                             w >>
                         8;
            if (kCrossing) {
                // Nothing to come from before the first pass, or after a
                // resize: the correction then fades up out of nothing.
                int was = 0;
                if (kPrevious) {
                    was = in.was_cast[(now_cast - cast) + c];
                    if (mem_w > 0)
                        was += (remembered - was) * mem_w >> 8;
                    if (was_w > 0)
                        was += (((in.was_delta[3 * was_a + c] + in.was_delta[3 * was_b + c]) >>
                                 1) -
                                was) *
                                   was_w >>
                               8;
                }
                delta = was + ((delta - was) * pixel_blend >> 8);
            }
            const int r = l[c] + delta;
            p |= static_cast<uint32_t>(r < 0 ? 0 : r > 255 ? 255 : r) << in.shift[c];
        }
        d[x] = p;
    }
}

// Whether every way of every pass the composition may read reaches only pixels of the frame
// from row y -- the eight-at-a-time loads read a way's pixels whether or not a pixel takes
// it. Rows near the frame's top and bottom under a vertical scroll do not, and go a pixel at
// a time.
inline bool RowInBounds(const ComposeInput& in, int y) {
    const ptrdiff_t row = static_cast<ptrdiff_t>(y) * in.width;
    const ptrdiff_t px = static_cast<ptrdiff_t>(in.width) * in.height;
    for (const Lookup* l : { &in.now_l, &in.was_l, &in.mem_l })
        for (int k = 0; k < l->count; k++)
            for (const ptrdiff_t o : { l->a[k], l->b[k] })
                if (row + o < 0 || row + o + in.width > px)
                    return false;
    return true;
}

// The composition's weight table, MotionWeight(m) * scene / 256, worked out for eight lanes
// of motion rather than looked up: MotionWeight() clamps the motion to
// [kHoldLevels, kFadeLevels] and falls by 256 over the span between them, and the division
// by that span is a multiply by its reciprocal, exact for every multiple of 256 up to it.
static_assert(kFadeLevels - kHoldLevels == 17, "the reciprocal below is 1/17's");

#if defined(VBAM_DLSSNR_SSE2)
// floor(x * w / 256) in each of eight int16 lanes, |x * w| < 2^23: bits 8-23 of the 32-bit
// product, which is what `x * w >> 8` gives in int.
inline __m128i MulShift8(__m128i x, __m128i w) {
    return _mm_or_si128(_mm_slli_epi16(_mm_mulhi_epi16(x, w), 8),
                        _mm_srli_epi16(_mm_mullo_epi16(x, w), 8));
}

// Each of eight int16 lanes repeated for its pixel's three channels: lanes 0-7 of `w` to
// three vectors of eight channels, RGB a pixel.
inline void Expand3(__m128i w, __m128i out[3]) {
    out[0] = _mm_unpacklo_epi64(_mm_shufflelo_epi16(w, _MM_SHUFFLE(1, 0, 0, 0)),
                                _mm_shufflelo_epi16(w, _MM_SHUFFLE(2, 2, 1, 1)));
    out[1] = _mm_unpacklo_epi64(_mm_shufflelo_epi16(w, _MM_SHUFFLE(3, 3, 3, 2)),
                                _mm_unpackhi_epi64(_mm_shufflehi_epi16(w, _MM_SHUFFLE(1, 0, 0, 0)),
                                                   _mm_setzero_si128()));
    out[2] = _mm_unpackhi_epi64(_mm_shufflehi_epi16(w, _MM_SHUFFLE(2, 2, 1, 1)),
                                _mm_shufflehi_epi16(w, _MM_SHUFFLE(3, 3, 3, 2)));
}

// The weight table's entries for eight int16 lanes of motion; see above.
inline __m128i Weight8(__m128i motion, __m128i scene) {
    const __m128i t = _mm_min_epi16(_mm_max_epi16(motion, _mm_set1_epi16(kHoldLevels)),
                                    _mm_set1_epi16(kFadeLevels));
    const __m128i x = _mm_slli_epi16(_mm_sub_epi16(t, _mm_set1_epi16(kHoldLevels)), 8);
    // floor(x / 17) for x <= 17 * 256: x * ceil(2^20 / 17) >> 20
    const __m128i q =
        _mm_srli_epi16(_mm_mulhi_epu16(x, _mm_set1_epi16(static_cast<int16_t>(61681))), 4);
    const __m128i full = _mm_sub_epi16(_mm_set1_epi16(256), q);
    // full * scene >> 8, both at most 256: bits 8-23 of the unsigned product
    return _mm_or_si128(_mm_slli_epi16(_mm_mulhi_epu16(full, scene), 8),
                        _mm_srli_epi16(_mm_mullo_epi16(full, scene), 8));
}

// Choose() for eight pixels from `i`: the weight of the way each takes (int16 lanes) and which
// way that is (`k`, int16 lanes), the first of equals as Choose() takes it.
inline __m128i Choose8(const Lookup& l, __m128i scene, size_t i, __m128i* k) {
    const __m128i zero = _mm_setzero_si128();
    *k = zero;
    if (l.count == 0)
        return zero;
    __m128i m = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(l.motion[0] + i));
    __m128i best = zero;
    for (int q = 1; q < l.count; q++) {
        const __m128i mq = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(l.motion[q] + i));
        // mq < m: the smaller of the two is mq, and they differ
        const __m128i less =
            _mm_andnot_si128(_mm_cmpeq_epi8(mq, m), _mm_cmpeq_epi8(_mm_min_epu8(mq, m), mq));
        m = _mm_min_epu8(m, mq);
        best = _mm_or_si128(_mm_andnot_si128(less, best),
                            _mm_and_si128(less, _mm_set1_epi8(static_cast<char>(q))));
    }
    *k = _mm_unpacklo_epi8(best, zero);
    return Weight8(_mm_unpacklo_epi8(m, zero), scene);
}

// The composition of row y eight pixels at a time, ComposePixels()'s arithmetic channel by
// channel in int16 lanes; returns how far it got, a multiple of eight. Which way each pixel
// takes and its weights are found eight pixels at a time and repeated for each pixel's three
// channels; the cast, a table lookup by colour, a pixel at a time.
template <bool kCrossing, bool kPrevious, bool kDetail>
int ComposeRowSse2(const ComposeInput& in, int y) {
    const int width = in.width;
    const int end = width & ~7;
    const size_t row = static_cast<size_t>(y) * width;
    const uint8_t* const live = in.live + row * 3;
    uint32_t* const d = reinterpret_cast<uint32_t*>(in.dst + static_cast<size_t>(y) * in.outstride);
    const __m128i zero = _mm_setzero_si128();
    const __m128i blend = _mm_set1_epi16(static_cast<int16_t>(in.blend));
    const __m128i scene = _mm_set1_epi16(static_cast<int16_t>(in.scene));
    const __m128i full = _mm_set1_epi16(256);
    alignas(16) int16_t cast_now[24], cast_was[24];
    alignas(16) uint8_t out[32];
    // The mean of a pass's pixels at the way each lane's pixel takes: every way's mean,
    // eight lanes at a time from where that way puts them, and each lane keeps its own.
    const auto mean = [&](const Lookup& l, const int16_t* delta, __m128i ks, size_t j) {
        __m128i r = zero;
        for (int k = 0; k < l.count; k++) {
            const __m128i a =
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(delta + j + 3 * l.a[k]));
            const __m128i b =
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(delta + j + 3 * l.b[k]));
            const __m128i m = _mm_srai_epi16(_mm_add_epi16(a, b), 1);
            r = _mm_or_si128(
                r, _mm_and_si128(_mm_cmpeq_epi16(ks, _mm_set1_epi16(static_cast<int16_t>(k))), m));
        }
        return r;
    };
    for (int x = 0; x < end; x += 8) {
        const uint8_t* const l = live + static_cast<size_t>(x) * 3;
        const size_t i = row + x;
        for (int q = 0; q < 8; q++) {
            const size_t ci = CastIndex(l[3 * q], l[3 * q + 1], l[3 * q + 2]) * 3;
            for (int c = 0; c < 3; c++) {
                cast_now[3 * q + c] = in.cast[ci + c];
                if (kPrevious)
                    cast_was[3 * q + c] = in.was_cast[ci + c];
            }
        }
        __m128i w_now[3], w_was[3], w_mem[3], k_now[3], k_was[3], k_mem[3];
        __m128i b3[3] = { blend, blend, blend };
        if (kDetail) {
            __m128i kn, kw = zero, km;
            const __m128i wn = Choose8(in.now_l, scene, i, &kn);
            const __m128i ww = kPrevious ? Choose8(in.was_l, scene, i, &kw) : zero;
            // The memory only where the pass on screen (or the one before, while crossing)
            // does not fit the live picture itself.
            __m128i misfit = _mm_cmplt_epi16(wn, full);
            if (kCrossing)
                misfit = _mm_or_si128(misfit, _mm_cmplt_epi16(ww, full));
            const __m128i wm = _mm_and_si128(misfit, Choose8(in.mem_l, scene, i, &km));
            if (kCrossing && kPrevious) {
                // ComposePixels()'s pixel_blend
                const __m128i fast =
                    MulShift8(wn, _mm_sub_epi16(full, _mm_max_epi16(ww, wm)));
                Expand3(_mm_add_epi16(blend, MulShift8(_mm_sub_epi16(full, blend), fast)), b3);
            }
            Expand3(wn, w_now);
            Expand3(ww, w_was);
            Expand3(wm, w_mem);
            Expand3(kn, k_now);
            Expand3(kw, k_was);
            Expand3(km, k_mem);
        }
        __m128i r[3];
        for (int v = 0; v < 3; v++) {
            const size_t j = i * 3 + 8 * v;  // the first channel of these eight lanes
            __m128i delta = _mm_load_si128(reinterpret_cast<const __m128i*>(cast_now + 8 * v));
            __m128i remembered = zero;
            if (kDetail) {
                remembered = mean(in.mem_l, in.mem_delta, k_mem[v], j);
                delta = _mm_add_epi16(delta, MulShift8(_mm_sub_epi16(remembered, delta), w_mem[v]));
                delta = _mm_add_epi16(
                    delta, MulShift8(_mm_sub_epi16(mean(in.now_l, in.now_delta, k_now[v], j), delta),
                                     w_now[v]));
            }
            if (kCrossing) {
                __m128i was = zero;
                if (kPrevious) {
                    was = _mm_load_si128(reinterpret_cast<const __m128i*>(cast_was + 8 * v));
                    if (kDetail) {
                        was = _mm_add_epi16(was, MulShift8(_mm_sub_epi16(remembered, was), w_mem[v]));
                        was = _mm_add_epi16(
                            was, MulShift8(_mm_sub_epi16(mean(in.was_l, in.was_delta, k_was[v], j), was),
                                           w_was[v]));
                    }
                }
                delta = _mm_add_epi16(was, MulShift8(_mm_sub_epi16(delta, was), b3[v]));
            }
            r[v] = delta;
        }
        // The live channels widened to int16, the deltas added, clamped to a byte by the pack.
        const __m128i lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(l));
        const __m128i hi = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(l + 16));
        _mm_store_si128(reinterpret_cast<__m128i*>(out),
                        _mm_packus_epi16(_mm_add_epi16(_mm_unpacklo_epi8(lo, zero), r[0]),
                                         _mm_add_epi16(_mm_unpackhi_epi8(lo, zero), r[1])));
        _mm_store_si128(reinterpret_cast<__m128i*>(out + 16),
                        _mm_packus_epi16(_mm_add_epi16(_mm_unpacklo_epi8(hi, zero), r[2]), zero));
        for (int q = 0; q < 8; q++)
            d[x + q] = static_cast<uint32_t>(out[3 * q]) << in.shift[0] |
                       static_cast<uint32_t>(out[3 * q + 1]) << in.shift[1] |
                       static_cast<uint32_t>(out[3 * q + 2]) << in.shift[2];
    }
    return end;
}
#elif defined(VBAM_DLSSNR_NEON)
// floor(x * w / 256) in each of eight int16 lanes, |x * w| < 2^23: the 32-bit products
// narrowed from bit 8, which is what `x * w >> 8` gives in int.
inline int16x8_t MulShift8(int16x8_t x, int16x8_t w) {
    return vcombine_s16(vshrn_n_s32(vmull_s16(vget_low_s16(x), vget_low_s16(w)), 8),
                        vshrn_n_s32(vmull_s16(vget_high_s16(x), vget_high_s16(w)), 8));
}

// The weight table's entries for eight lanes of motion; see above.
inline int16x8_t Weight8(uint16x8_t motion, uint16x8_t scene) {
    const uint16x8_t t =
        vminq_u16(vmaxq_u16(motion, vdupq_n_u16(static_cast<uint16_t>(kHoldLevels))),
                  vdupq_n_u16(static_cast<uint16_t>(kFadeLevels)));
    const uint16x8_t x =
        vshlq_n_u16(vsubq_u16(t, vdupq_n_u16(static_cast<uint16_t>(kHoldLevels))), 8);
    // floor(x / 17) for x <= 17 * 256: x * ceil(2^20 / 17) >> 20
    const uint16x4_t r = vdup_n_u16(61681);
    const uint16x8_t q = vshrq_n_u16(vcombine_u16(vshrn_n_u32(vmull_u16(vget_low_u16(x), r), 16),
                                                  vshrn_n_u32(vmull_u16(vget_high_u16(x), r), 16)),
                                     4);
    const uint16x8_t full = vsubq_u16(vdupq_n_u16(256), q);
    // full * scene >> 8, both at most 256
    return vreinterpretq_s16_u16(
        vcombine_u16(vshrn_n_u32(vmull_u16(vget_low_u16(full), vget_low_u16(scene)), 8),
                     vshrn_n_u32(vmull_u16(vget_high_u16(full), vget_high_u16(scene)), 8)));
}

// Choose() for eight pixels from `i`: the weight of the way each takes and which way that is
// (`k`), the first of equals as Choose() takes it.
inline int16x8_t Choose8(const Lookup& l, uint16x8_t scene, size_t i, uint16x8_t* k) {
    *k = vdupq_n_u16(0);
    if (l.count == 0)
        return vdupq_n_s16(0);
    uint8x8_t m = vld1_u8(l.motion[0] + i);
    uint8x8_t best = vdup_n_u8(0);
    for (int q = 1; q < l.count; q++) {
        const uint8x8_t mq = vld1_u8(l.motion[q] + i);
        best = vbsl_u8(vclt_u8(mq, m), vdup_n_u8(static_cast<uint8_t>(q)), best);
        m = vmin_u8(m, mq);
    }
    *k = vmovl_u8(best);
    return Weight8(vmovl_u8(m), scene);
}

// The composition of row y eight pixels at a time, ComposePixels()'s arithmetic in int16
// lanes; returns how far it got, a multiple of eight. NEON's structured loads split the
// pixels into their channels, so each vector holds one channel of eight pixels and the ways
// and weights, found per pixel, apply to all three as they are; the cast, a table lookup by
// colour, goes a pixel at a time.
template <bool kCrossing, bool kPrevious, bool kDetail>
int ComposeRowNeon(const ComposeInput& in, int y) {
    const int width = in.width;
    const int end = width & ~7;
    const size_t row = static_cast<size_t>(y) * width;
    const uint8_t* const live = in.live + row * 3;
    uint32_t* const d = reinterpret_cast<uint32_t*>(in.dst + static_cast<size_t>(y) * in.outstride);
    const int16x8_t zero = vdupq_n_s16(0);
    const int16x8_t blend = vdupq_n_s16(static_cast<int16_t>(in.blend));
    const uint16x8_t scene = vdupq_n_u16(static_cast<uint16_t>(in.scene));
    const int16x8_t full = vdupq_n_s16(256);
    const int32x4_t shift[3] = { vdupq_n_s32(in.shift[0]), vdupq_n_s32(in.shift[1]),
                                 vdupq_n_s32(in.shift[2]) };
    alignas(16) int16_t cast_now[3][8], cast_was[3][8];
    // The mean of a pass's pixels at the way each pixel takes, channel by channel: every
    // way's mean, eight pixels at a time from where that way puts them, and each lane keeps
    // its own. The halving add is `(a + b) >> 1` in int.
    const auto mean = [&](const Lookup& l, const int16_t* delta, uint16x8_t ks, size_t i) {
        int16x8x3_t r = { { zero, zero, zero } };
        const int16_t* const p = delta + 3 * i;
        for (int k = 0; k < l.count; k++) {
            const int16x8x3_t a = vld3q_s16(p + 3 * l.a[k]);
            const int16x8x3_t b = vld3q_s16(p + 3 * l.b[k]);
            const uint16x8_t sel = vceqq_u16(ks, vdupq_n_u16(static_cast<uint16_t>(k)));
            for (int c = 0; c < 3; c++)
                r.val[c] = vbslq_s16(sel, vhaddq_s16(a.val[c], b.val[c]), r.val[c]);
        }
        return r;
    };
    for (int x = 0; x < end; x += 8) {
        const uint8_t* const l = live + static_cast<size_t>(x) * 3;
        const size_t i = row + x;
        for (int q = 0; q < 8; q++) {
            const size_t ci = CastIndex(l[3 * q], l[3 * q + 1], l[3 * q + 2]) * 3;
            for (int c = 0; c < 3; c++) {
                cast_now[c][q] = in.cast[ci + c];
                if (kPrevious)
                    cast_was[c][q] = in.was_cast[ci + c];
            }
        }
        int16x8_t w_now = zero, w_was = zero, w_mem = zero, pixel_blend = blend;
        int16x8x3_t remembered = { { zero, zero, zero } }, now_mean = remembered,
                    was_mean = remembered;
        if (kDetail) {
            uint16x8_t k_now, k_was = vdupq_n_u16(0), k_mem;
            w_now = Choose8(in.now_l, scene, i, &k_now);
            if (kPrevious)
                w_was = Choose8(in.was_l, scene, i, &k_was);
            // The memory only where the pass on screen (or the one before, while crossing)
            // does not fit the live picture itself.
            uint16x8_t misfit = vcltq_s16(w_now, full);
            if (kCrossing)
                misfit = vorrq_u16(misfit, vcltq_s16(w_was, full));
            w_mem = vandq_s16(vreinterpretq_s16_u16(misfit), Choose8(in.mem_l, scene, i, &k_mem));
            if (kCrossing && kPrevious) {
                // ComposePixels()'s pixel_blend
                const int16x8_t fast = MulShift8(w_now, vsubq_s16(full, vmaxq_s16(w_was, w_mem)));
                pixel_blend = vaddq_s16(blend, MulShift8(vsubq_s16(full, blend), fast));
            }
            remembered = mean(in.mem_l, in.mem_delta, k_mem, i);
            now_mean = mean(in.now_l, in.now_delta, k_now, i);
            if (kPrevious)
                was_mean = mean(in.was_l, in.was_delta, k_was, i);
        }
        const uint8x8x3_t lv = vld3_u8(l);
        uint32x4_t lo = vdupq_n_u32(0), hi = vdupq_n_u32(0);
        for (int c = 0; c < 3; c++) {
            int16x8_t delta = vld1q_s16(cast_now[c]);
            if (kDetail) {
                delta = vaddq_s16(delta, MulShift8(vsubq_s16(remembered.val[c], delta), w_mem));
                delta = vaddq_s16(delta, MulShift8(vsubq_s16(now_mean.val[c], delta), w_now));
            }
            if (kCrossing) {
                int16x8_t was = zero;
                if (kPrevious) {
                    was = vld1q_s16(cast_was[c]);
                    if (kDetail) {
                        was = vaddq_s16(was, MulShift8(vsubq_s16(remembered.val[c], was), w_mem));
                        was = vaddq_s16(was, MulShift8(vsubq_s16(was_mean.val[c], was), w_was));
                    }
                }
                delta = vaddq_s16(was, MulShift8(vsubq_s16(delta, was), pixel_blend));
            }
            // The live channel widened to int16, the delta added, clamped to a byte, and put
            // where the channel goes in the 32-bit pixel.
            const uint16x8_t v = vmovl_u8(
                vqmovun_s16(vaddq_s16(vreinterpretq_s16_u16(vmovl_u8(lv.val[c])), delta)));
            lo = vorrq_u32(lo, vshlq_u32(vmovl_u16(vget_low_u16(v)), shift[c]));
            hi = vorrq_u32(hi, vshlq_u32(vmovl_u16(vget_high_u16(v)), shift[c]));
        }
        vst1q_u32(d + x, lo);
        vst1q_u32(d + x + 4, hi);
    }
    return end;
}
#endif

// The composition of rows [y_begin, y_end): ComposeRowSse2() or ComposeRowNeon() where it
// can, ComposePixels() for the rest, the same bytes either way; `simd` false is
// ComposePixels() alone.
template <bool kCrossing, bool kPrevious, bool kDetail>
void ComposeRowsAs(const ComposeInput& in, int y_begin, int y_end, bool simd) {
    for (int y = y_begin; y < y_end; y++) {
        int x = 0;
#if defined(VBAM_DLSSNR_SSE2)
        if (simd && RowInBounds(in, y))
            x = ComposeRowSse2<kCrossing, kPrevious, kDetail>(in, y);
#elif defined(VBAM_DLSSNR_NEON)
        if (simd && RowInBounds(in, y))
            x = ComposeRowNeon<kCrossing, kPrevious, kDetail>(in, y);
#else
        (void)simd;
#endif
        ComposePixels<kCrossing, kPrevious, kDetail>(in, y, x, in.width);
    }
}

// The frame-wide choices as constants, so each case compiles to a loop of its own without
// the others' work. `previous` is only ever set while crossing. With the scene's weight at
// nothing (the whole frame moving), no pixel keeps any of a pass's own correction or the
// memory's -- every weight is 0 -- and the cast is all there is to lay down.
void ComposeRows(const ComposeInput& in, int y_begin, int y_end, bool simd = true) {
    const auto by_detail = [&](auto crossing_c, auto previous_c) {
        constexpr bool kCrossing = decltype(crossing_c)::value;
        constexpr bool kPrevious = decltype(previous_c)::value;
        if (in.detail)
            ComposeRowsAs<kCrossing, kPrevious, true>(in, y_begin, y_end, simd);
        else
            ComposeRowsAs<kCrossing, kPrevious, false>(in, y_begin, y_end, simd);
    };
    if (in.previous)
        by_detail(std::true_type{}, std::true_type{});
    else if (in.crossing)
        by_detail(std::true_type{}, std::false_type{});
    else
        by_detail(std::false_type{}, std::false_type{});
}

// The temporal gate's knobs, nr_frame_live's defaults (nr_frame_live_defaults(), which are
// nr_daemon.py's). nr_frame_defaults() leaves the release and the floor off, and the
// model's own gate is not local: it keeps about 0.6 of the previous prediction across the
// whole frame, so wherever a sprite has just been, that much of it stays in the history --
// a dark trail behind Link as he walks, behind his sword as he swings it. The release lets go of the history where the game's own pixel changed
// since the frame before, all of it by kReleaseLevels; the floor holds it where nothing
// changed, falling away by kHoldRampLevels.
constexpr float kHold = 1.0f;
constexpr float kHoldRampLevels = 4.0f;
constexpr float kReleaseLevels = 24.0f;

// How far, in pixels of the filter extent, a change to the picture lets go of the history
// around it. The network works from a pixel's surroundings, so its prediction for the sand
// beside a sprite carries a halo of the sprite's edges, and that goes into the history with
// it. When the sprite walks on, those pixels have not changed -- they were sand and still are
// -- so a release that looks only at the pixel itself keeps the halo, and the floor holds it
// there pass after pass: a pale outline left behind everything that moves. Releasing by the
// largest change nearby instead lets the halo go with the sprite that made it.
constexpr int kReleaseRadius = 8;

// The mean change of the network's input, 0-1, above which the history is dropped rather
// than released pixel by pixel (nr_frame_live's cut limit): a cut, a screen transition, or
// a scroll so fast that nothing in the history is still where it was.
constexpr double kCutLimit = 0.15;

// `read_png` in nr_frame_main.c: `byte / 255`, a division rather than a multiply by the
// reciprocal, so the float the network sees is the one the command gives it.
inline float FromByte(uint32_t v) {
    return static_cast<float>(v & 0xffu) / 255.0f;
}

// nr_frame_main.c's PROFILES, applied over nr_frame_defaults().
void ApplyProfile(nr_frame_params* p, uint32_t profile) {
    switch (profile) {
        case kProfileNatural:
            p->normalized_style = 1.0f / 128;
            p->local_tone = 1.0f;
            p->local_structure = 1.0f;
            break;
        case kProfileCinematic:
            p->normalized_style = 2.0f / 128;
            p->local_tone = 1.0f;
            p->local_structure = 1.0f;
            break;
        case kProfileNeutral:
            p->normalized_style = 0.0f;
            p->local_tone = 0.0f;
            p->local_structure = 0.0f;
            break;
        case kProfileVendor:
            p->normalized_style = 0.0f;
            p->local_tone = 1.0f;
            p->local_structure = 1.5f;
            break;
        case kProfileStandard:
        default:
            p->normalized_style = 0.0f;
            p->local_tone = 1.0f;
            p->local_structure = 1.0f;
            break;
    }
}

// nr_frame_main.c's main(): the defaults and the flags, then the profile, then the explicit
// overrides (`nr_frame.controls`).
void BuildParams(nr_frame_params* p, const Settings& settings) {
    nr_frame_defaults(p);
    p->intensity = settings.intensity;
    p->detail_strength = settings.detail_strength;
    p->colour_strength = settings.colour_strength;
    p->detail_radius = settings.detail_radius;
    p->frame_index = settings.frame_index;
    p->hold = kHold;
    p->slope = -255.0f * kHold / kHoldRampLevels;
    p->release = -255.0f / kReleaseLevels;
    ApplyProfile(p, settings.profile);
    if (settings.style_index >= 0)
        p->normalized_style = static_cast<float>(settings.style_index) / 128.0f;
    if (settings.local_tone >= 0.0f)
        p->local_tone = settings.local_tone;
    if (settings.local_structure >= 0.0f)
        p->local_structure = settings.local_structure;
    // --skin-structure / --auto-mask: either one turns the automatic mask on, the other left at -1.
    const bool have_skin = settings.skin_structure >= 0.0f;
    const bool have_auto = settings.auto_mask >= 0.0f;
    if (have_skin || have_auto) {
        p->automatic_mask = 1;
        p->skin_structure = have_skin ? settings.skin_structure : -1.0f;
        p->automatic_structure = have_auto ? settings.auto_mask : -1.0f;
    }
}

// nr_frame_main.c's resize(): `nr_image.bilinear`, one axis at a time through nr_resize_axis,
// the axis plan mapping pixel centres onto the source. The GPU's bilinear StretchRect in
// PCSX2 maps the centres the same way.
void ResizeBilinear(const float* source, int height, int width, int target_height,
                    int target_width, std::vector<float>* output) {
    constexpr int channels = 3;
    std::vector<float> current(source, source + static_cast<size_t>(height) * width * channels);
    std::vector<float> next;
    std::vector<int32_t> low, high;
    std::vector<float> weight;
    int h = height, w = width;
    for (int axis = 0; axis < 2; axis++) {
        const int extent = axis == 0 ? h : w;
        const int count = axis == 0 ? target_height : target_width;
        if (extent == count)
            continue;
        low.resize(count);
        high.resize(count);
        weight.resize(count);
        for (int i = 0; i < count; i++) {
            const float centre = (static_cast<float>(i) + 0.5f) *
                                     (static_cast<float>(extent) / static_cast<float>(count)) -
                                 0.5f;
            const float floored = std::floor(centre);
            const int lo = static_cast<int>(
                floored < 0.0f                                   ? 0.0f
                : floored > static_cast<float>(extent - 1) ? static_cast<float>(extent - 1)
                                                                 : floored);
            low[i] = lo;
            high[i] = lo + 1 > extent - 1 ? extent - 1 : lo + 1;
            const float frac = centre - static_cast<float>(lo);
            weight[i] = frac < 0.0f ? 0.0f : frac > 1.0f ? 1.0f : frac;
        }
        const int nh = axis == 0 ? count : h;
        const int nw = axis == 0 ? w : count;
        next.resize(static_cast<size_t>(nh) * nw * channels);
        nr_resize_axis(current.data(), static_cast<ptrdiff_t>(w) * channels, channels, 1, nh, nw,
                       channels, axis, low.data(), high.data(), weight.data(), next.data());
        current.swap(next);
        h = nh;
        w = nw;
    }
    output->swap(current);
}

// The three stages of nr_frame_main.c, each on its own thread. With history the previous
// output is what the next frame's features and composition need, so only one frame is in
// the network or composition at a time and what overlaps is the next frame's conversion to
// float; without it the stages each hold a frame.
enum StageIndex : int {
    kStageFeatures,  // RGB8 -> colour (scaled to max_height), nr_frame_features_masked()
    kStageNetwork,   // nr_frame_run_features(), the head cropped
    kStageCompose,   // nr_frame_compose(), colour -> RGB8 (scaled back)
    kStageCount
};

// One frame's buffers, handed from stage to stage. There are as many as stages, so the
// pipeline is full when every stage holds one.
struct Packet {
    std::vector<uint8_t> rgb;  // the frame, (height, width, 3); the result replaces it
    // The frame the pass was given, kept when the result replaces `rgb`: Apply32() lays the
    // pass over the picture on screen by what it changed, which needs what it started from.
    std::vector<uint8_t> src_rgb;
    int width = 0;
    int height = 0;
    int filter_width = 0;  // the extent the model sees, at most max_height tall
    int filter_height = 0;
    int network_width = 0;
    int network_height = 0;
    nr_frame_params params;
    bool want_history = false;  // Settings::history
    bool use_history = false;   // the history and previous input belong to this frame
    std::string control_mask;   // Settings::control_mask
    bool use_mask = false;
    // Claimed on the shared model (Claim()), at this model generation.
    bool claimed = false;
    uint64_t generation = 0;
    bool new_extent = false;
    std::chrono::steady_clock::time_point start;
    uint64_t frame = 0;  // Filter::Impl::frames_applied when it was given the frame

    std::vector<float> colour;    // (filter_height, filter_width, 3)
    std::vector<float> features;  // (network_height, network_width, 16)
    std::vector<float> wide;      // network head, (network_height, network_width, 4)
    std::vector<float> head;      // the head cropped to (filter_height, filter_width, 4)
    std::vector<float> output;    // (filter_height, filter_width, 3)
    // The prediction before the intensity, the mask and the detail split, which is what
    // the next frame's history is (nr_frame.h): `output` carries those, and carrying them
    // back round puts them inside the loop.
    std::vector<float> neural;    // (filter_height, filter_width, 3)
    // The previous input as the composition's gate sees it (ReleaseHistory()).
    std::vector<float> gate_previous;  // (filter_height, filter_width, 3), when use_history
    std::vector<uint8_t> moved;        // (filter_height, filter_width), ReleaseHistory()'s
    std::vector<uint8_t> dilate;
    std::vector<float> mask;      // the control mask at the filter extent, when use_mask
    std::vector<float> scratch;
    // The pass's colour cast (BuildCast()), from `src_rgb` to `rgb`.
    std::vector<int16_t> cast;
    std::vector<int64_t> cast_sums;
};

// The control mask, loaded and resized by the features stage only.
struct ControlMask {
    std::string path;
    bool loaded = false;
    int source_width = 0;
    int source_height = 0;
    std::vector<float> source;  // (source_height, source_width, 3)
    int width = 0;
    int height = 0;
    std::vector<float> resized;  // (height, width, 3)
};

}  // namespace

struct Filter::Impl {
    // Pipeline state, under `mutex`.
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> quit{false};

    std::array<std::thread, kStageCount> threads;
    std::array<Packet, kStageCount> packets;
    std::vector<Packet*> free_packets;
    std::array<Packet*, kStageCount> queued{};  // waiting for a stage
    std::array<bool, kStageCount> active{};     // a stage is working on a packet

    ControlMask control_mask;  // features stage only

    // The temporal history: the previous output and the previous input (for the floor), at
    // the filter extent. Written by the compose stage, read by the frame after it, which
    // the features stage only lets in once the composition is done, so the stages never
    // touch them at the same time.
    std::vector<float> history;
    std::vector<float> previous;
    int history_width = 0;
    int history_height = 0;
    bool have_history = false;
    std::atomic<bool> reset_history{false};

    // The newest finished frame, RGB8 at the frame's size, and what Apply32() shows.
    bool result_ready = false;
    std::vector<uint8_t> result;
    // The frame that pass was given, the same size as `result`; Apply32() needs both.
    std::vector<uint8_t> result_src;
    std::vector<int16_t> result_cast;
    int result_width = 0;
    int result_height = 0;
    uint64_t result_frame = 0;  // Packet::frame
    std::vector<uint8_t> display;
    std::vector<uint8_t> display_src;
    std::vector<int16_t> display_cast;
    std::vector<int16_t> display_delta;  // display - display_src, what the pass changed
    Pyramid display_pyramid;  // of display_src, for finding the scroll
    // The pass before it, and how far across the two the composition currently is: a new
    // pass arrives at 0 and takes over as this reaches 256.
    std::vector<uint8_t> previous_display;
    std::vector<uint8_t> previous_display_src;
    std::vector<int16_t> previous_cast;
    std::vector<int16_t> previous_delta;
    Pyramid previous_pyramid;
    int previous_width = 0;
    int previous_height = 0;
    int blend = 256;
    // The background's correction (see the note above kMemoryStillFrames): the frame a
    // correction was taken from, the correction, and whether a pixel holds one, in the
    // coordinates of the pass on screen (`display_src`). The scratch set is for shifting it.
    std::vector<uint8_t> memory_src;
    std::vector<int16_t> memory_delta;
    std::vector<uint8_t> memory_valid;
    std::vector<uint8_t> memory_src_scratch;
    std::vector<int16_t> memory_delta_scratch;
    std::vector<uint8_t> memory_valid_scratch;
    int memory_width = 0;
    int memory_height = 0;
    Alignment memory_alignment;  // the memory's motion masks, under the pass's hypotheses
    // How long each live pixel has held still (kMemoryStillFrames), and the live frame
    // before this one to tell; screen coordinates, reset wherever the picture changes.
    std::vector<uint8_t> age;
    std::vector<uint8_t> age_diff;  // the channels' change since the last frame, for `age`
    std::vector<uint8_t> live_previous;
    // Calls to Apply32() so far, and the count when the pass on screen was given its frame:
    // how many frames old the pass is, for UpdateMemory(). And UpdateMemory()'s mask of
    // what is near anything that moved since, on the motion grid, with its scratch.
    uint64_t frames_applied = 0;
    uint64_t display_frame = 0;
    std::vector<uint8_t> memory_near;
    std::vector<uint8_t> memory_near_scratch;
    // Apply32()'s live frame as RGB8 and its pyramid; how it lines up with the pass on
    // screen and the one before it; and the dilation's scratch. Members so the frame does not allocate.
    std::vector<uint8_t> live;
    Pyramid live_pyramid;
    std::array<Alignment, 2> alignment;  // the pass on screen, the one before it
    std::vector<std::vector<uint8_t>> dilate_scratch;  // one per motion mask (DilateAll)
    // The live frame and the passes' inputs on the motion grid (MotionStep()). The passes'
    // only change when one lands, so they are made once each after that, at `grid_step`.
    std::vector<uint8_t> live_grid, display_grid, previous_grid, memory_src_grid,
        memory_valid_grid;
    bool display_grid_fresh = false, previous_grid_fresh = false, memory_grid_fresh = false;
    int grid_step = 0;
    // The frame weight actually in use, carried between frames so it can only climb back
    // gradually -- see kSceneRisePerFrame.
    int scene_weight = 256;
    int display_width = 0;
    int display_height = 0;

    // Whether passes have stopped changing anything: the last pass was given the same
    // frame, settings and mask as the one before it and gave the same picture, so another
    // on that frame would too (the network is deterministic, and with history the pass is
    // at its fixed point). Apply32() then queues none until something changes. Set by the
    // compose stage, read by Apply32(), under `mutex`; the copies are the compose stage's
    // and change only under `mutex`.
    bool settled = false;
    std::vector<uint8_t> settled_src;  // what the last pass was given
    std::vector<uint8_t> settled_out;  // and what it gave
    std::vector<float> settled_neural;  // and the history it left, when it kept one
    nr_frame_params settled_params{};
    std::string settled_mask;
    bool settled_history = false;
    int settled_width = 0;
    int settled_height = 0;
    int settled_filter_width = 0;
    int settled_filter_height = 0;

    Governor governor;  // Apply32()'s, under `mutex`

    // Status, readable without the pipeline lock.
    std::atomic<bool> ready{false};
    std::atomic<bool> failed{false};
    std::atomic<double> last_ms{0.0};
    std::atomic<uint64_t> frames_done{0};
    // Innermost lock: nothing is taken while it is held.
    mutable std::mutex status_mutex;
    std::string error;
    std::string device;

    void Fail(const std::string& why);

    // Caller holds `mutex`.
    Packet* TakePacket(std::unique_lock<std::mutex>& lock, int stage);
    bool PassPacket(std::unique_lock<std::mutex>& lock, int stage, Packet* p);
    void DropPacket(Packet* p);

    // Runs on whichever thread lets go of the last reference -- a stage on its
    // way out, once ~Filter() has detached it. What ~Filter() used to do after
    // joining, which it can no longer wait to do.
    ~Impl();

    bool Claim(Packet* p, std::string* why);
    void Unclaim(Packet* p);
    nr_frame* Borrow(Packet* p, std::shared_lock<std::shared_mutex>* use, bool prepare);
    void Unprepare(const Packet* p);

    void ConvertColour(Packet* p);
    bool PrepareControlMask(Packet* p);
    void ReleaseHistory(Packet* p);
    bool BuildFeatures(Packet* p);
    bool RunNetwork(Packet* p);
    bool Compose(Packet* p);

    void FeaturesStage();
    void NetworkStage();
    void ComposeStage();

    // Brings the correction memory up to date with the pass that just reached the
    // screen (`display_*`), the one before it still in `previous_*`, given the live frame
    // (RGB8, the frame's size) and `age`. Caller holds `mutex`.
    void UpdateMemory(const uint8_t* liveb, int step);
};

void Filter::Impl::Fail(const std::string& why) {
    {
        std::lock_guard<std::mutex> lock(status_mutex);
        if (failed.load())
            return;
        error = why.empty() ? std::string("DLSS NR failed") : why;
    }
    failed.store(true);
}

// Waits for work at a stage. Returns nullptr when the pipeline is quitting.
Packet* Filter::Impl::TakePacket(std::unique_lock<std::mutex>& lock, int stage) {
    cv.wait(lock, [&] { return quit.load() || queued[stage]; });
    if (quit.load())
        return nullptr;

    Packet* p = queued[stage];
    queued[stage] = nullptr;
    active[stage] = true;
    cv.notify_all();  // the previous stage may be waiting for this slot
    return p;
}

// Hands a packet to the next stage, waiting for it to take the one before. False when quitting.
bool Filter::Impl::PassPacket(std::unique_lock<std::mutex>& lock, int stage, Packet* p) {
    active[stage] = false;
    cv.wait(lock, [&] { return quit.load() || !queued[stage + 1]; });
    if (quit.load()) {
        DropPacket(p);
        return false;
    }

    queued[stage + 1] = p;
    cv.notify_all();
    return true;
}

// Caller holds `mutex`: gives a packet back, so Apply32() can use it again.
void Filter::Impl::DropPacket(Packet* p) {
    Unclaim(p);
    free_packets.push_back(p);
    cv.notify_all();
}

// Takes the shared model for `p`, opening it if nobody has yet, and waiting while another
// Filter's frames are in it. False when quitting, or with `why` set if opening failed.
bool Filter::Impl::Claim(Packet* p, std::string* why) {
    SharedModel& s = Shared();
    std::unique_lock<std::mutex> lock(s.mutex);
    // Timed, so that ~Filter() can set `quit` and go without taking `mutex` to
    // notify: it runs on the UI thread, and `mutex` is held for the length of
    // a model open. Unclaim() still notifies, so the wait for another Filter to
    // leave the model ends as promptly as it ever did; the timeout is only the
    // backstop that retires a stage nobody will notify again.
    while (!quit.load() && s.owner && s.owner != this)
        s.cv.wait_for(lock, std::chrono::milliseconds(50));
    if (quit.load())
        return false;

    if (!s.frame && !OpenLocked(s, why)) {
        // Cancelled by WithdrawVulkanShare() rather than broken. Say nothing,
        // so the stage drops this frame and the next one opens again instead
        // of the filter disabling itself.
        if (nr_frame_open_cancelled())
            why->clear();
        return false;
    }

    s.owner = this;
    s.owner_frames++;
    p->claimed = true;
    p->generation = s.generation;
    p->new_extent = p->filter_width != s.prepared_width || p->filter_height != s.prepared_height;
    {
        std::lock_guard<std::mutex> status(status_mutex);
        device = s.device;
    }
    ready.store(true);
    return true;
}

void Filter::Impl::Unclaim(Packet* p) {
    if (!p->claimed)
        return;
    p->claimed = false;
    SharedModel& s = Shared();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (s.owner == this && --s.owner_frames <= 0) {
        s.owner = nullptr;
        s.owner_frames = 0;
        s.cv.notify_all();
    }
}

// The model for one libnr_frame call on `p`, held in `use` until the caller lets it go.
// nullptr if the model closed (a renderer lent or withdrew its device) since `p` was
// claimed; the frame is then dropped. `prepare` records that the call prepares the graph
// for `p`'s extent.
nr_frame* Filter::Impl::Borrow(Packet* p, std::shared_lock<std::shared_mutex>* use,
                               bool prepare) {
    SharedModel& s = Shared();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (!s.frame || s.generation != p->generation)
        return nullptr;
    if (prepare) {
        s.prepared_width = p->filter_width;
        s.prepared_height = p->filter_height;
    }
    *use = std::shared_lock<std::shared_mutex>(s.use);
    return s.frame;
}

// The graph may be half prepared after a failed features call: prepare it again next time.
void Filter::Impl::Unprepare(const Packet* p) {
    SharedModel& s = Shared();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (s.generation == p->generation) {
        s.prepared_width = 0;
        s.prepared_height = 0;
    }
}

// byte / 255, like image_io.py and nr_frame_main.c. A frame taller than max_height is
// scaled down first and rounded to bytes, as PCSX2's GPU scale into an RGBA8 target does.
void Filter::Impl::ConvertColour(Packet* p) {
    const size_t pixels = static_cast<size_t>(p->width) * p->height;
    const size_t filter_pixels = static_cast<size_t>(p->filter_width) * p->filter_height;
    nr_frame_geometry(p->filter_height, p->filter_width, &p->network_height, &p->network_width);
    p->features.resize(static_cast<size_t>(p->network_height) * p->network_width * 16);

    if (p->filter_width == p->width && p->filter_height == p->height) {
        p->colour.resize(pixels * 3);
        for (size_t i = 0; i < pixels * 3; i++)
            p->colour[i] = FromByte(p->rgb[i]);
        return;
    }

    p->scratch.resize(pixels * 3);
    for (size_t i = 0; i < pixels * 3; i++)
        p->scratch[i] = FromByte(p->rgb[i]);
    ResizeBilinear(p->scratch.data(), p->height, p->width, p->filter_height, p->filter_width,
                   &p->colour);
    for (size_t i = 0; i < filter_pixels * 3; i++)
        p->colour[i] = FromByte(ToByte(p->colour[i]));
}

// Loads the control mask when its path changes, and resizes it when the filter extent does.
// nr_frame refuses a mask of another shape; a game's frame changes size, so this resizes it
// the way nr_frame's --size resizes a picture.
bool Filter::Impl::PrepareControlMask(Packet* p) {
    ControlMask& mask = control_mask;
    if (p->control_mask != mask.path) {
        mask = ControlMask();
        mask.path = p->control_mask;
        const ImageLoader loader = g_image_loader.load();
        std::vector<uint8_t> rgb;
        int w = 0, h = 0;
        if (!mask.path.empty() && loader && loader(mask.path, &w, &h, &rgb) && w > 0 && h > 0 &&
            rgb.size() >= static_cast<size_t>(w) * h * 3) {
            // byte / 255 of R, G, B, as read_png() reduces any PNG to 8-bit RGB
            mask.source_width = w;
            mask.source_height = h;
            mask.source.resize(static_cast<size_t>(w) * h * 3);
            for (size_t i = 0; i < mask.source.size(); i++)
                mask.source[i] = FromByte(rgb[i]);
            mask.loaded = true;
        }
    }

    if (!mask.loaded)
        return false;

    if (mask.width != p->filter_width || mask.height != p->filter_height) {
        ResizeBilinear(mask.source.data(), mask.source_height, mask.source_width,
                       p->filter_height, p->filter_width, &mask.resized);
        mask.width = p->filter_width;
        mask.height = p->filter_height;
    }
    p->mask = mask.resized;
    return true;
}

// Drops the history on a cut, and otherwise lets go of it where the game's own picture
// changed nearby (kReleaseRadius) since the frame before.
//
// The history goes to the composition only, never to the features, though nr_frame takes it
// in both. Handed to the network, it closes a loop: the network darkens and sharpens what it
// is shown, the result is the next history, and on a picture holding still the darkening
// compounds pass after pass -- measured on Minish Cap at intensity 150, the mean shift of the
// picture crept from 2.7 to 5.1 levels over six seconds of standing still, and up to three
// times that on the cliffs, while without the history it stays put. Wherever something then
// moves, the history there is let go and the picture there comes back light, so a light
// patch follows every sprite and darkens slowly back in behind it: a trail of its own. The
// composition alone only averages the model's successive answers, which is what the history
// is for -- the denoising holding steady from one pass to the next -- and cannot drift.
//
// The composition releases and floors its blend by the change at each pixel alone, measured
// against `previous` -- the one thing it reads `previous` for. So it is handed a stand-in
// that differs from the colour by the change nearby instead, and its gate then lets go of
// the halo around a sprite as well as the sprite.
void Filter::Impl::ReleaseHistory(Packet* p) {
    const int w = p->filter_width;
    const int hgt = p->filter_height;
    const size_t pixels = static_cast<size_t>(w) * hgt;
    const float* const c = p->colour.data();
    const float* const b = previous.data();
    double cut = 0.0;
    for (size_t i = 0; i < pixels * 3; i++)
        cut += std::fabs(static_cast<double>(c[i]) - b[i]);
    if (pixels && cut / static_cast<double>(pixels * 3) > kCutLimit) {
        p->use_history = false;
        return;
    }

    p->moved.resize(pixels);
    for (size_t i = 0; i < pixels; i++) {
        float m = 0.0f;
        for (int k = 0; k < 3; k++)
            m = std::max(m, std::fabs(c[3 * i + k] - b[3 * i + k]));
        p->moved[i] = ToByte(m);
    }
    Dilate(&p->moved, w, hgt, kReleaseRadius, &p->dilate);

    p->gate_previous.resize(pixels * 3);
    for (size_t i = 0; i < pixels; i++) {
        const float m = FromByte(p->moved[i]);
        for (int k = 0; k < 3; k++) {
            const float ci = c[3 * i + k];
            p->gate_previous[3 * i + k] = ci >= m ? ci - m : ci + m;
        }
    }
}

bool Filter::Impl::BuildFeatures(Packet* p) {
    std::shared_lock<std::shared_mutex> use;
    nr_frame* frame = Borrow(p, &use, true);
    if (!frame)
        return false;

    // No history: see ReleaseHistory().
    const float* mask = p->use_mask ? p->mask.data() : nullptr;
    if (nr_frame_features_masked(frame, p->colour.data(), p->filter_height, p->filter_width,
                                 nullptr, mask, &p->params, p->features.data()) != 0) {
        Fail(std::string("nr_frame_features_masked() failed: ") + nr_frame_error());
        use.unlock();
        Unprepare(p);
        return false;
    }
    return true;
}

bool Filter::Impl::RunNetwork(Packet* p) {
    const int W = p->network_width;
    p->wide.resize(static_cast<size_t>(p->network_height) * W * 4);
    p->head.resize(static_cast<size_t>(p->filter_width) * p->filter_height * 4);
    {
        std::shared_lock<std::shared_mutex> use;
        nr_frame* frame = Borrow(p, &use, false);
        if (!frame)
            return false;
        if (nr_frame_run_features(frame, p->features.data(), p->network_height, W,
                                  p->wide.data()) != 0) {
            Fail(std::string("nr_frame_run_features() failed: ") + nr_frame_error());
            return false;
        }
    }

    // geometry.crop(head)
    for (int y = 0; y < p->filter_height; y++)
        memcpy(p->head.data() + static_cast<size_t>(y) * p->filter_width * 4,
               p->wide.data() + static_cast<size_t>(y) * W * 4,
               static_cast<size_t>(p->filter_width) * 4 * sizeof(float));
    return true;
}

bool Filter::Impl::Compose(Packet* p) {
    const size_t filter_count = static_cast<size_t>(p->filter_width) * p->filter_height * 3;
    p->output.resize(filter_count);
    p->neural.resize(filter_count);
    {
        std::shared_lock<std::shared_mutex> use;
        nr_frame* frame = Borrow(p, &use, false);
        if (!frame)
            return false;
        const float* hist = p->use_history ? history.data() : nullptr;
        const float* prev = p->use_history ? p->gate_previous.data() : nullptr;
        const float* mask = p->use_mask ? p->mask.data() : nullptr;
        // `_neural` for the prediction as well as the picture: the picture is what goes
        // on screen, the prediction is what the next frame's history has to be.
        if (nr_frame_compose_neural(frame, p->head.data(), p->colour.data(), p->filter_height,
                                    p->filter_width, hist, prev, mask, &p->params,
                                    p->output.data(), p->neural.data()) != 0) {
            Fail(std::string("nr_frame_compose_neural() failed: ") + nr_frame_error());
            return false;
        }
    }

    // value * 255 + 0.5, then back up to the frame's size the way PCSX2 draws its 8-bit
    // upload with a bilinear StretchRect.
    const size_t count = static_cast<size_t>(p->width) * p->height * 3;
    // The input moves aside instead of being overwritten -- a swap, so the buffer the result
    // is written into is the one `src_rgb` held last time round.
    p->src_rgb.swap(p->rgb);
    p->rgb.resize(count);
    if (p->filter_width == p->width && p->filter_height == p->height) {
        for (size_t i = 0; i < count; i++)
            p->rgb[i] = ToByte(p->output[i]);
        return true;
    }
    p->scratch.resize(filter_count);
    for (size_t i = 0; i < filter_count; i++)
        p->scratch[i] = FromByte(ToByte(p->output[i]));
    std::vector<float> up;
    ResizeBilinear(p->scratch.data(), p->filter_height, p->filter_width, p->height, p->width,
                   &up);
    for (size_t i = 0; i < count; i++)
        p->rgb[i] = ToByte(up[i]);
    return true;
}

// Inside libframe the stages touch different parts of the nr_frame (the features the index
// tables, noise and the graph's extent, the network the graph and its buffers, the
// composition its temporal gate table and detail split). What they share is the extent the
// graph is prepared for, which nr_frame_features_masked() changes, and with history the
// previous frame's output. So the conversion to float and the mask run straight away, and
// the features then wait until the network (and, with history, the composition) has
// nothing left.
void Filter::Impl::FeaturesStage() {
    std::unique_lock<std::mutex> lock(mutex);
    while (Packet* p = TakePacket(lock, kStageFeatures)) {
        lock.unlock();
        p->start = std::chrono::steady_clock::now();
        ConvertColour(p);
        p->use_mask = PrepareControlMask(p);
        std::string why;
        const bool claimed = Claim(p, &why);
        if (!claimed && !why.empty())
            Fail(why);
        lock.lock();

        if (!claimed) {
            active[kStageFeatures] = false;
            DropPacket(p);
            if (quit.load())
                break;
            continue;
        }

        const bool new_extent = p->new_extent;
        const bool want_history = p->want_history;
        cv.wait(lock, [&] {
            if (quit.load())
                return true;
            const bool network_idle = !queued[kStageNetwork] && !active[kStageNetwork];
            const bool compose_idle = !queued[kStageCompose] && !active[kStageCompose];
            return (!new_extent || network_idle) &&
                   (!want_history || (network_idle && compose_idle));
        });
        if (quit.load()) {
            DropPacket(p);
            break;
        }

        if (reset_history.exchange(false) || p->filter_width != history_width ||
            p->filter_height != history_height)
            have_history = false;
        p->use_history = want_history && have_history;
        lock.unlock();

        // `history` and `previous` are the compose stage's, which is idle until this frame
        // passes on (the wait above), so they are read unlocked.
        if (p->use_history)
            ReleaseHistory(p);

        const bool ok = BuildFeatures(p);

        lock.lock();
        if (!ok) {
            active[kStageFeatures] = false;
            DropPacket(p);
            continue;
        }
        if (!PassPacket(lock, kStageFeatures, p))
            break;
    }
    active[kStageFeatures] = false;
    cv.notify_all();
}

void Filter::Impl::NetworkStage() {
    std::unique_lock<std::mutex> lock(mutex);
    while (Packet* p = TakePacket(lock, kStageNetwork)) {
        lock.unlock();
        const bool ok = RunNetwork(p);
        lock.lock();

        if (!ok) {
            active[kStageNetwork] = false;
            DropPacket(p);
            continue;
        }
        if (!PassPacket(lock, kStageNetwork, p))
            break;
    }
    active[kStageNetwork] = false;
    cv.notify_all();
}

void Filter::Impl::ComposeStage() {
    std::unique_lock<std::mutex> lock(mutex);
    while (Packet* p = TakePacket(lock, kStageCompose)) {
        lock.unlock();
        const bool ok = Compose(p);
        if (ok)
            BuildCast(p->src_rgb.data(), p->rgb.data(), p->width, p->height, &p->cast,
                      &p->cast_sums);
        // Only this stage writes the settled copies, so it reads them unlocked.
        const bool same = ok && p->width == settled_width && p->height == settled_height &&
                          p->filter_width == settled_filter_width &&
                          p->filter_height == settled_filter_height &&
                          p->want_history == settled_history && p->control_mask == settled_mask &&
                          std::memcmp(&p->params, &settled_params, sizeof p->params) == 0 &&
                          p->src_rgb == settled_src && p->rgb == settled_out &&
                          (!p->want_history || p->neural == settled_neural);
        lock.lock();

        settled = same;
        if (ok && !same) {
            settled_src = p->src_rgb;
            settled_out = p->rgb;
            if (p->want_history)
                settled_neural = p->neural;
            settled_params = p->params;
            settled_mask = p->control_mask;
            settled_history = p->want_history;
            settled_width = p->width;
            settled_height = p->height;
            settled_filter_width = p->filter_width;
            settled_filter_height = p->filter_height;
        }

        if (ok) {
            // The shown buffer comes back as this packet's, for a later frame.
            result.swap(p->rgb);
            result_src.swap(p->src_rgb);
            result_cast.swap(p->cast);
            result_width = p->width;
            result_height = p->height;
            result_frame = p->frame;
            result_ready = true;

            // This output and input are the next frame's history and previous input.
            // Without history nothing reads them, and a frame turning history back on
            // starts afresh.
            if (!p->want_history) {
                have_history = false;
            } else {
                // The prediction, not the picture. The picture carries the intensity,
                // and the history goes to the next frame's features as well as its
                // blend, so carrying the intensity round would have it compound: the
                // vendor's own path keeps this value (nr_frame.h on `neural`).
                history.swap(p->neural);
                previous.swap(p->colour);
                history_width = p->filter_width;
                history_height = p->filter_height;
                have_history = true;
            }
            last_ms.store(std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - p->start)
                              .count());
            frames_done.fetch_add(1);
        } else {
            have_history = false;
        }
        active[kStageCompose] = false;
        DropPacket(p);  // the features stage waits for this frame
    }
    active[kStageCompose] = false;
    cv.notify_all();
}

// The pass that just landed is remembered wherever the live picture has held still for
// kMemoryStillFrames and still shows what the pass was given (so the pixel is background,
// and the pass's correction is a correction of it). The memory keeps its old entry under
// anything that moves, so the background's correction survives under a sprite walking
// over it. A pixel with nothing remembered yet takes the new pass as it is. The memory
// lives in the coordinates of the pass before, and moves with the scroll between the two
// passes first; `live` is the frame on screen now, screen coordinates, which the pass's
// are too once nothing has moved for that long.
//
// Nor is a pixel remembered near anything that has moved since the pass was given its
// frame, within twice MotionRadius(): the pass's correction there carries the network's halo
// around the sprite, and remembered, the halo is laid back down wherever the memory stands
// in for the pass. Under a scroll that follows a walking sprite it did so all the time --
// the sand beside Link holds still on screen while the ground scrolls under him, so it was
// remembered pass after pass with his halo in it, the memory scrolled on with the ground,
// and the old pass, which no longer fits where Link had been, gave way to it there: a patch
// a level lighter than the sand around it trailed him wherever he walked. And the pixel
// itself has to have held still since the pass was given its frame as well as for
// kMemoryStillFrames, which at the larger extents a pass is older than.
void Filter::Impl::UpdateMemory(const uint8_t* liveb, int step) {
    const int w = display_width;
    const int h = display_height;
    const size_t px = static_cast<size_t>(w) * h;
    if (display_src.size() != px * 3 || display_delta.size() != px * 3)
        return;
    const bool fresh = memory_width != w || memory_height != h || memory_valid.size() != px ||
                       memory_src.size() != px * 3 || memory_delta.size() != px * 3;
    if (fresh) {
        memory_width = w;
        memory_height = h;
        memory_src.assign(px * 3, 0);
        memory_delta.assign(px * 3, 0);
        memory_valid.assign(px, 0);
    }
    const bool have_prev = !fresh && previous_width == w && previous_height == h &&
                           previous_display_src.size() == px * 3;
    // display(x, y) ~ previous(x - dx, y - dy): the memory was in the previous pass's
    // coordinates, so it moves by the same shift, and what shifts off the frame is gone.
    int dx = 0, dy = 0;
    if (have_prev) {
        EstimateScroll(display_pyramid, previous_pyramid, &dx, &dy);
        if (dx || dy) {
            memory_src_scratch.resize(px * 3);
            memory_delta_scratch.resize(px * 3);
            memory_valid_scratch.assign(px, 0);
            for (int y = 0; y < h; y++) {
                const int sy = y - dy;
                if (sy < 0 || sy >= h)
                    continue;
                const int x0 = std::max(0, dx), x1 = std::min(w, w + dx);
                if (x1 <= x0)
                    continue;
                const size_t to = static_cast<size_t>(y) * w + x0;
                const size_t from = static_cast<size_t>(sy) * w + (x0 - dx);
                const size_t n = static_cast<size_t>(x1 - x0);
                std::memcpy(memory_src_scratch.data() + to * 3, memory_src.data() + from * 3, n * 3);
                std::memcpy(memory_delta_scratch.data() + to * 3, memory_delta.data() + from * 3,
                            n * 3 * sizeof(int16_t));
                std::memcpy(memory_valid_scratch.data() + to, memory_valid.data() + from, n);
            }
            memory_src.swap(memory_src_scratch);
            memory_delta.swap(memory_delta_scratch);
            memory_valid.swap(memory_valid_scratch);
        }
    }
    const uint8_t* const now = display_src.data();
    const int16_t* const delta = display_delta.data();
    const bool have_age = liveb && age.size() == px;
    // What has moved since the pass's frame, spread by the halo, on the motion grid.
    const int gw = (w + step - 1) / step, gh = (h + step - 1) / step;
    const int still = static_cast<int>(
        std::min<uint64_t>(255, kMemoryStillFrames + (frames_applied - display_frame)));
    if (have_age) {
        Decimate(age.data(), w, h, step, 1, &memory_near);
        for (uint8_t& a : memory_near)
            a = a < still ? 255 : 0;
        Dilate(&memory_near, gw, gh, (2 * MotionRadius(h) + step - 1) / step,
               &memory_near_scratch);
    }
    RowPool::Get().Run(h, [&](int ya, int yb) {
        for (int y = ya; y < yb; y++) {
            const uint8_t* const near =
                have_age ? memory_near.data() + static_cast<size_t>(y / step) * gw : nullptr;
            for (int x = 0; x < w; x++) {
                const size_t i = static_cast<size_t>(y) * w + x;
                bool take = !memory_valid[i];
                if (!take && near && !near[x / step]) {
                    const uint8_t* a = now + i * 3;
                    const uint8_t* b = liveb + i * 3;
                    take = std::abs(a[0] - b[0]) <= kHoldLevels &&
                           std::abs(a[1] - b[1]) <= kHoldLevels &&
                           std::abs(a[2] - b[2]) <= kHoldLevels;
                }
                if (take) {
                    std::memcpy(memory_src.data() + i * 3, now + i * 3, 3);
                    memory_delta[3 * i] = delta[3 * i];
                    memory_delta[3 * i + 1] = delta[3 * i + 1];
                    memory_delta[3 * i + 2] = delta[3 * i + 2];
                    memory_valid[i] = 1;
                }
            }
        }
    });
}

Filter::Filter() : impl_(std::make_shared<Impl>()) {
    AcquireModel();
    const std::shared_ptr<Impl>& im = impl_;
    for (Packet& p : im->packets)
        im->free_packets.push_back(&p);
    // Each stage holds a reference of its own, so the state outlives this
    // object while a stage is still retiring -- see ~Filter().
    im->threads[kStageFeatures] = std::thread([im] { im->FeaturesStage(); });
    im->threads[kStageNetwork] = std::thread([im] { im->NetworkStage(); });
    im->threads[kStageCompose] = std::thread([im] { im->ComposeStage(); });
}

Filter::Impl::~Impl() {
    // Frames that were queued between stages still hold the model.
    for (Packet& p : packets)
        Unclaim(&p);
    ReleaseModel();
    // Paired with the request in ~Filter(). Cleared here rather than there
    // because this is the end of the open's own lifetime: ReleaseModel() has
    // just taken the model lock, which an open holds, so by now there is no
    // open of ours left to abandon and the next Filter may open freely.
    nr_frame_cancel_open(0);
}

Filter::~Filter() {
    // Before anything else, and before any lock: a stage may be inside a model
    // open, which compiles every compute pipeline and takes tens of seconds
    // cold. Whoever ends up waiting on the model lock -- ReleaseModel() here,
    // or the panel, or the next Filter -- waits for that open unless it is
    // asked to stop. WithdrawVulkanShare() asks when the renderer takes its
    // device back; nothing asked when the filter simply goes away, so closing
    // the window during a cold open waited out the whole compile.
    //
    // The open stops after the pipeline it is already building. Claim() treats
    // a cancelled open as a dropped frame rather than a failure, so a Filter
    // that outlives this one reopens on its next pass instead of disabling
    // itself.
    nr_frame_cancel_open(1);
    Impl& im = *impl_;
    {
        std::lock_guard<std::mutex> lock(im.mutex);
        im.quit.store(true);
    }
    im.cv.notify_all();
    // A features stage waiting for another Filter to leave the model wakes on
    // `quit` by itself; see the wait in Claim(). Notifying it here would mean
    // taking the model lock, which is held for the length of a model open.

    // Detach rather than join. A stage may be inside a model open, which
    // compiles every compute pipeline and cannot be interrupted; joining would
    // hold up whoever destroys the panel for as long as the driver takes. An
    // idle stage is gone at once; a busy one finishes what it started, sees
    // `quit`, and drops its reference to `impl_`. The last one to go disposes
    // of it -- ~Impl() unclaims the packets and releases the model, which is
    // what this destructor used to do once the threads had joined.
    //
    // Moving each thread out first leaves Impl::threads empty, so that later
    // disposal destroys thread objects that own nothing.
    for (std::thread& t : im.threads) {
        std::thread stage = std::move(t);
        if (stage.joinable())
            stage.detach();
    }
    impl_.reset();
}

void Filter::Apply32(const uint8_t* src, int instride, uint8_t* dst, int outstride, int width,
                     int height, int red_shift, int green_shift, int blue_shift,
                     const Settings& settings) {
    if (width <= 0 || height <= 0)
        return;
    Impl& im = *impl_;

    // The network is expensive, so run it at no more than the configured height.
    int filter_width = width;
    int filter_height = height;
    if (settings.max_height > 0 && height > settings.max_height) {
        filter_height = settings.max_height;
        filter_width = std::max(1, (width * settings.max_height + height / 2) / height);
    }

    bool wrote_result = false;
    bool notify = false;
    bool landed = false;  // a new pass reached the screen this frame
    {
        std::lock_guard<std::mutex> lock(im.mutex);

        // Read the source before writing the result: as a post-pass over the display
        // filter's output dst aliases src, and taking the result first would feed the
        // model its own previous output.
        //
        // The frame as RGB8, once, on the pool: what the model is given, what the settled
        // check compares and what the composition lays the pass over. Each of the three
        // used to convert it again, the first two a pixel at a time on this thread -- at
        // xBRZ 6x a millisecond or more a frame.
        const size_t px = static_cast<size_t>(width) * height;
        im.live.resize(px * 3);
        RowPool::Get().Run(height, [&](int y_begin, int y_end) {
            for (int y = y_begin; y < y_end; y++) {
                const uint32_t* s =
                    reinterpret_cast<const uint32_t*>(src + static_cast<size_t>(y) * instride);
                uint8_t* o = im.live.data() + static_cast<size_t>(y) * width * 3;
                for (int x = 0; x < width; x++, o += 3) {
                    const uint32_t v = s[x];
                    o[0] = static_cast<uint8_t>(v >> red_shift);
                    o[1] = static_cast<uint8_t>(v >> green_shift);
                    o[2] = static_cast<uint8_t>(v >> blue_shift);
                }
            }
        });

        // Once the passes have settled (see `settled`), the same frame with the same
        // settings gets no more of them. On a still picture the network ran flat out for
        // nothing: twenty passes a second on a Radeon 780M, a package drawing 50 W instead
        // of 14, and a laptop that heats up and throttles the emulator with it.
        nr_frame_params params;
        BuildParams(&params, settings);
        bool unchanged = false;
        if (im.settled && !im.reset_history.load() && width == im.settled_width &&
            height == im.settled_height && filter_width == im.settled_filter_width &&
            filter_height == im.settled_filter_height &&
            settings.history == im.settled_history && settings.control_mask == im.settled_mask &&
            std::memcmp(&params, &im.settled_params, sizeof params) == 0) {
            unchanged = im.live == im.settled_src;
        }

        const auto now = std::chrono::steady_clock::now();
        im.governor.Frame(now, settings.target_fps);
        im.frames_applied++;

        // A frame arriving while the pipeline is full is skipped.
        if (!unchanged && im.governor.MayStart(now) && !im.failed.load() && !im.quit.load() &&
            !im.free_packets.empty() && !im.queued[kStageFeatures]) {
            im.governor.Started(now);
            Packet* p = im.free_packets.back();
            im.free_packets.pop_back();
            p->rgb = im.live;
            p->width = width;
            p->height = height;
            p->filter_width = filter_width;
            p->filter_height = filter_height;
            p->params = params;
            p->want_history = settings.history;
            p->control_mask = settings.control_mask;
            p->frame = im.frames_applied;
            im.queued[kStageFeatures] = p;
            notify = true;
        }

        // A pass that lands while the one before is still crossing over waits for the
        // crossing to finish, and the newest pass by then is the one that goes up. Landing
        // at once, it made the pass on screen the one to cross from -- dropping the part of
        // the picture that was still the pass before it -- so every pass that came sooner
        // than kResultFadePerFrame allows was a step in the picture: on a card where passes
        // land every six frames, a face fading in on Advance Wars' intro went from 28 levels
        // darker to 44 in one frame and back to 6 in another. A resized frame does not wait.
        const bool crossing_done = im.blend >= 256 || im.result_width != im.display_width ||
                                   im.result_height != im.display_height;
        if (im.result_ready && crossing_done) {
            // What is on screen becomes what the new pass crosses over from.
            im.previous_display.swap(im.display);
            im.previous_display_src.swap(im.display_src);
            im.previous_grid.swap(im.display_grid);
            im.previous_grid_fresh = im.display_grid_fresh;
            im.display_grid_fresh = false;
            im.memory_grid_fresh = false;  // UpdateMemory() takes the new pass below
            im.previous_cast.swap(im.display_cast);
            std::swap(im.previous_pyramid, im.display_pyramid);
            im.previous_delta.swap(im.display_delta);
            im.previous_width = im.display_width;
            im.previous_height = im.display_height;
            im.blend = 0;
            im.display.swap(im.result);
            im.display_src.swap(im.result_src);
            im.display_cast.swap(im.result_cast);
            im.display_width = im.result_width;
            im.display_height = im.result_height;
            im.display_frame = im.result_frame;
            im.result_ready = false;
            if (im.display_src.size() ==
                static_cast<size_t>(im.display_width) * im.display_height * 3)
            {
                BuildPyramid(im.display_src.data(), im.display_width, im.display_height,
                             PyramidLevels(im.display_width, im.display_height),
                             &im.display_pyramid);
                im.display_delta.resize(im.display_src.size());
                const size_t row = static_cast<size_t>(im.display_width) * 3;
                RowPool::Get().Run(im.display_height, [&](int ya, int yb) {
                    for (size_t i = ya * row; i < yb * row; i++)
                        im.display_delta[i] =
                            static_cast<int16_t>(im.display[i] - im.display_src[i]);
                });
                landed = true;
            }
        }

        // Lay the newest finished pass over the frame on screen *now*, by what it
        // changed rather than as the picture itself.
        //
        // A pass is several frames old -- hundreds of milliseconds at the extents the
        // post-filter stage works at. Showing its output as the picture means the screen
        // only ever changes when a pass lands, which at one or two passes a second is a
        // slideshow however good each frame looks. Adding what the pass changed instead
        // lets the emulator's own frames through at their own rate, with the denoising
        // riding on top and refreshing whenever a pass finishes.
        //
        // What the pass changed is only meaningful where the picture still looks like what
        // the pass was given, though; added blind, an old frame's edges land on top of the
        // new one's and read as a double image. So each pixel's correction is weighted by
        // how far the picture has moved since -- where it stands, or where the background
        // scrolled to (kScrollReach), whichever fits: the pass's own where nothing has
        // changed, its cast for the colour now there where it has (see kCastBits). On a
        // still picture every pixel is untouched, the weights are all one, and this
        // reproduces the pass's output exactly.
        if (!im.display.empty() && im.display.size() == im.display_src.size() &&
            im.display_width == width && im.display_height == height &&
            im.display_cast.size() == kCastEntries * 3 &&
            im.display_delta.size() == im.display.size()) {
            const uint8_t* const in = im.display_src.data();
            const int16_t* const cast = im.display_cast.data();

            // How long each pixel of the live frame (`im.live`, above) has held still, for
            // the correction memory: each channel's change sixteen at a time (the mean of
            // the previous frame with itself is the previous frame), then a pixel is still
            // if none moved. A row at a time on the pool, each row's own.
            const bool fresh_age = im.age.size() != px || im.live_previous.size() != px * 3;
            if (fresh_age) {
                im.age.assign(px, 0);
                im.live_previous.resize(px * 3);
            }
            im.age_diff.resize(px * 3);
            RowPool::Get().Run(height, [&](int y_begin, int y_end) {
                for (int y = y_begin; y < y_end; y++) {
                    const size_t first = static_cast<size_t>(y) * width;
                    const uint8_t* const row = im.live.data() + first * 3;
                    uint8_t* const lp = im.live_previous.data() + first * 3;
                    if (!fresh_age) {
                        uint8_t* const diff = im.age_diff.data() + first * 3;
                        AbsDiffMean(row, lp, lp, diff, static_cast<size_t>(width) * 3);
                        const uint8_t* d = diff;
                        uint8_t* age = im.age.data() + first;
                        for (int x = 0; x < width; x++, d += 3) {
                            const bool still =
                                std::max(d[0], std::max(d[1], d[2])) <= kHoldLevels;
                            age[x] = still ? static_cast<uint8_t>(age[x] + (age[x] < 255)) : 0;
                        }
                    }
                    std::memcpy(lp, row, static_cast<size_t>(width) * 3);
                }
            });
            const uint8_t* const live = im.live.data();
            if (landed)
                im.UpdateMemory(live, MotionStep(height, filter_height));

            // How far each pixel has moved since the pass was given the frame, if the pass
            // is taken as the mean of itself shifted by `h.a` and by `h.b` -- one shift
            // when they are the same: the largest step of the three channels, or all of it
            // where a shift takes the pixel off the pass's edge.
            //
            // `valid`, when given, says which of `given`'s pixels hold anything (the
            // correction memory's); a pixel reading one that does not has moved all of it.
            //
            // All of it on the motion grid (MotionStep()): `given` and `valid` are on the
            // grid, and so is `motion`; the hypothesis is in frame pixels, rounded to the grid.
            const int step = MotionStep(height, filter_height);
            const int gw = (width + step - 1) / step, gh = (height + step - 1) / step;
            const size_t gpx = static_cast<size_t>(gw) * gh;
            const uint8_t* live_grid = live;
            if (step > 1) {
                Decimate(live, width, height, step, 3, &im.live_grid);
                live_grid = im.live_grid.data();
            }
            if (step != im.grid_step) {
                im.grid_step = step;
                im.display_grid_fresh = im.previous_grid_fresh = im.memory_grid_fresh = false;
            }
            const auto on_grid = [&](const uint8_t* full, int channels, std::vector<uint8_t>* buf,
                                     bool* fresh) -> const uint8_t* {
                if (step == 1)
                    return full;
                if (!*fresh || buf->size() != gpx * channels) {
                    Decimate(full, width, height, step, channels, buf);
                    *fresh = true;
                }
                return buf->data();
            };
            const auto grid_of = [&](Alignment* al, int k) -> std::vector<uint8_t>& {
                return step > 1 ? al->grid[k] : al->motion[k];
            };
            const auto measure = [&](const uint8_t* given, const uint8_t* valid,
                                     const Hypothesis& h, uint8_t* motion) {
                const auto g = [&](int v) {
                    return (v >= 0 ? v + step / 2 : v - step / 2) / step;
                };
                const Hypothesis hg =
                    step > 1 ? Hypothesis{ g(h.ax), g(h.ay), g(h.bx), g(h.by) } : h;
                RowPool::Get().Run(gh, [&](int y_begin, int y_end) {
                    MeasureRows(live_grid, given, valid, hg, gw, gh, y_begin, y_end, motion);
                });
            };
            // The ways to take one pass: where it stands, and, when that leaves enough of
            // the frame moving to be worth asking, where the scroll took it -- and the mean
            // of the two shifts either side of that, which is what interframe blending makes
            // of the frame in the middle of a scroll step: the background half where it was
            // and half where it went, matching neither. Returns how many pixels moved
            // unmistakably whichever way they are taken.
            bool have_live_pyramid = false;
            const auto track = [&](const uint8_t* given, const Pyramid& pyramid,
                                   Alignment* al) {
                al->count = 1;
                al->h[0] = Hypothesis();
                std::vector<uint8_t>& still = grid_of(al, 0);
                still.resize(gpx);
                measure(given, nullptr, al->h[0], still.data());
                // Whether anything is worth lining up at all: any motion counts here.
                size_t count = 0, mild = 0;
                CountMotion(still.data(), nullptr, nullptr, gpx, &count, &mild);
                if (count * 100 <= gpx * kScenePercentHold)
                    return count;
                // What the scene is weighed by: the mild residue (kSceneMildLevels).
                count = mild;
                if (!have_live_pyramid) {
                    BuildPyramid(live, width, height, PyramidLevels(width, height),
                                 &im.live_pyramid);
                    have_live_pyramid = true;
                }
                int dx = 0, dy = 0;
                EstimateScroll(im.live_pyramid, pyramid, &dx, &dy);
                if (dx == 0 && dy == 0)
                    return count;
                const int ux = (dx > 0) - (dx < 0), uy = (dy > 0) - (dy < 0);
                al->h[1] = Hypothesis{ dx, dy, dx, dy };
                al->h[2] = Hypothesis{ dx - ux, dy - uy, dx + ux, dy + uy };
                al->count = 3;
                for (int k = 1; k < al->count; k++) {
                    grid_of(al, k).resize(gpx);
                    measure(given, nullptr, al->h[k], grid_of(al, k).data());
                }
                size_t moving = 0;
                CountMotion(grid_of(al, 0).data(), grid_of(al, 1).data(), grid_of(al, 2).data(),
                            gpx, &moving, &count);
                return count;
            };
            Alignment& now_al = im.alignment[0];
            Alignment& was_al = im.alignment[1];
            was_al.count = 0;
            const size_t moved_pixels =
                track(on_grid(in, 3, &im.display_grid, &im.display_grid_fresh),
                      im.display_pyramid, &now_al);

            // How much of the frame is unmistakably moving, and so how much of the pass's
            // own correction is worth laying down at all beyond the cast. None of it, when
            // the frame is all moving.
            const int scene = static_cast<int>(moved_pixels * 100 / gpx);
            const int target =
                scene <= kScenePercentHold
                    ? 256
                    : (scene >= kScenePercentDrop
                           ? 0
                           : 256 - (scene - kScenePercentHold) * 256 /
                                       (kScenePercentDrop - kScenePercentHold));
            if (target < im.scene_weight)
                im.scene_weight = std::max(target, im.scene_weight - kSceneFallPerFrame);
            else if (target > im.scene_weight)
                im.scene_weight = std::min(target, im.scene_weight + kSceneRisePerFrame);
            const int scene_weight = im.scene_weight;

            // And cross over from the pass before, so a new one does not arrive as a step.
            if (im.blend < 256)
                im.blend = std::min(256, im.blend + kResultFadePerFrame);
            const int blend = im.blend;
            const bool crossing = blend < 256;
            const bool have_previous =
                crossing && im.previous_display.size() == im.display.size() &&
                im.previous_display_src.size() == im.display.size() &&
                im.previous_cast.size() == kCastEntries * 3 &&
                im.previous_delta.size() == im.display.size() && im.previous_width == width &&
                im.previous_height == height;

            // Spread the motion (kMotionRadius), and weigh the pass before by the same rule
            // against the frame it was given. Passes land faster than they cross over, so
            // much of what is on screen is the older of the two, made where things stood a
            // pass earlier still; laid down unweighed, it leaves what it corrected behind
            // as a trail.
            // And the correction memory (Impl::memory_*), kept in the coordinates of the pass
            // on screen, so lined up the same ways it is; with its own motion, since it
            // remembers what the picture was when each correction was made.
            Alignment& mem_al = im.memory_alignment;
            mem_al.count = 0;
            const bool have_memory = scene_weight > 0 && im.memory_width == width &&
                                     im.memory_height == height && im.memory_valid.size() == px &&
                                     im.memory_src.size() == px * 3 &&
                                     im.memory_delta.size() == px * 3;
            if (scene_weight > 0) {
                if (have_previous)
                    track(on_grid(im.previous_display_src.data(), 3, &im.previous_grid,
                                  &im.previous_grid_fresh),
                          im.previous_pyramid, &was_al);
                if (have_memory) {
                    // Both of the memory's copies go stale together, when a pass lands.
                    bool src_fresh = im.memory_grid_fresh, valid_fresh = im.memory_grid_fresh;
                    const uint8_t* mem_src =
                        on_grid(im.memory_src.data(), 3, &im.memory_src_grid, &src_fresh);
                    const uint8_t* mem_valid =
                        on_grid(im.memory_valid.data(), 1, &im.memory_valid_grid, &valid_fresh);
                    im.memory_grid_fresh = true;
                    mem_al.count = now_al.count;
                    for (int k = 0; k < now_al.count; k++) {
                        mem_al.h[k] = now_al.h[k];
                        grid_of(&mem_al, k).resize(gpx);
                        measure(mem_src, mem_valid, mem_al.h[k], grid_of(&mem_al, k).data());
                    }
                }
                uint8_t* masks[9];
                uint8_t* across[9];
                uint8_t* fulls[9];
                int n_masks = 0;
                for (Alignment* al : { &now_al, &was_al, &mem_al })
                    for (int k = 0; k < al->count; k++) {
                        al->motion[k].resize(px);
                        fulls[n_masks] = al->motion[k].data();
                        masks[n_masks++] = grid_of(al, k).data();
                    }
                if (im.dilate_scratch.size() < static_cast<size_t>(n_masks))
                    im.dilate_scratch.resize(static_cast<size_t>(n_masks));
                for (int k = 0; k < n_masks; k++) {
                    im.dilate_scratch[k].resize(gpx);
                    across[k] = im.dilate_scratch[k].data();
                }
                // The radius in frame pixels, on the grid: rounded up, so the spread is never
                // less than it would be at full size.
                DilateAll(masks, across, n_masks, gw, gh,
                          (MotionRadius(height) + step - 1) / step);
                if (step > 1)
                    UndecimateAll(masks, fulls, n_masks, width, height, step);
            }

            std::array<int, 256> weight;  // MotionWeight() times the scene's, by motion
            for (int m = 0; m < 256; m++)
                weight[m] = MotionWeight(m) * scene_weight / 256;
            const auto lookup = [&](const Alignment& al, bool use) {
                Lookup l;
                l.count = use ? al.count : 0;
                for (int k = 0; k < l.count; k++) {
                    l.motion[k] = al.motion[k].data();
                    l.a[k] = -(static_cast<ptrdiff_t>(al.h[k].ay) * width + al.h[k].ax);
                    l.b[k] = -(static_cast<ptrdiff_t>(al.h[k].by) * width + al.h[k].bx);
                }
                return l;
            };
            ComposeInput ci;
            ci.width = width;
            ci.height = height;
            ci.live = live;
            ci.dst = dst;
            ci.outstride = outstride;
            ci.shift[0] = red_shift;
            ci.shift[1] = green_shift;
            ci.shift[2] = blue_shift;
            ci.cast = cast;
            ci.was_cast = have_previous ? im.previous_cast.data() : nullptr;
            // Shifts rather than divisions: at a full weight of 256 they are exact, so a
            // still picture still gets the pass's own output.
            ci.now_delta = im.display_delta.data();
            ci.was_delta = have_previous ? im.previous_delta.data() : nullptr;
            ci.mem_delta = have_memory ? im.memory_delta.data() : nullptr;
            ci.now_l = lookup(now_al, scene_weight > 0);
            ci.was_l = lookup(was_al, scene_weight > 0 && have_previous);
            ci.mem_l = lookup(mem_al, have_memory);
            ci.weight = weight.data();
            ci.scene = scene_weight;
            ci.blend = blend;
            ci.crossing = crossing;
            ci.previous = have_previous;
            ci.detail = scene_weight > 0;
            RowPool::Get().Run(height,
                               [&](int y_begin, int y_end) { ComposeRows(ci, y_begin, y_end); });
            wrote_result = true;
        }
    }

    if (notify)
        im.cv.notify_all();

    if (!wrote_result && dst != src) {
        // Nothing finished yet (or the model is unavailable): pass through.
        // Nothing to do when dst aliases src, and memcpy would not allow it.
        const size_t row_bytes = static_cast<size_t>(width) * 4;
        for (int y = 0; y < height; y++)
            memcpy(dst + static_cast<size_t>(y) * outstride,
                   src + static_cast<size_t>(y) * instride, row_bytes);
    }
}

void Filter::ResetHistory() {
    // Taken by the features stage before the next frame's features.
    impl_->reset_history.store(true);
}

double Filter::PassGapMs() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->governor.gap_ms();
}

bool Filter::Ready() const {
    return impl_->ready.load();
}

bool Filter::Failed() const {
    return impl_->failed.load();
}

std::string Filter::Error() const {
    if (!impl_->failed.load())
        return std::string();
    std::lock_guard<std::mutex> lock(impl_->status_mutex);
    return impl_->error;
}

std::string Filter::Device() const {
    if (!impl_->ready.load())
        return std::string();
    std::lock_guard<std::mutex> lock(impl_->status_mutex);
    return impl_->device;
}

double Filter::LastFrameMs() const {
    return impl_->last_ms.load();
}

uint64_t Filter::FramesDone() const {
    return impl_->frames_done.load();
}

}  // namespace dlssnr
