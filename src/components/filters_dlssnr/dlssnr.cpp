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
#if defined(__SSE2__) || (defined(_M_X64) && !defined(_M_ARM64EC)) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
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
                                      (v.cooperative_matrix && v.workgroup_memory_explicit_layout ? 2 : 0),
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

void WithdrawVulkanShare(const void* owner) {
    SharedModel& s = Shared();
    {
        // Not the lender: say so without touching `mutex`, so a renderer that
        // never lent anything is never held up by a pass.
        std::lock_guard<std::mutex> share_lock(s.share_mutex);
        if (!s.has_share || s.share.owner != owner)
            return;
    }
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
    std::lock_guard<std::mutex> share_lock(s.share_mutex);
    if (!s.has_share || s.share.owner != owner)
        return;
    if (s.opened_shared)
        CloseLocked(s);
    s.has_share = false;
    s.share_pending = false;
    s.share = VulkanShare();
}

bool WithdrawVulkanShareDeferred(const void* owner, void (*finish)(void*), void* context,
                                 unsigned grace_ms) {
    SharedModel& s = Shared();
    {
        std::lock_guard<std::mutex> share_lock(s.share_mutex);
        if (!s.has_share || s.share.owner != owner)
            return false;
    }
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
        std::lock_guard<std::mutex> share_lock(s.share_mutex);
        if (s.has_share && s.share.owner == owner) {
            if (s.opened_shared)
                CloseLocked(s);
            s.has_share = false;
            s.share_pending = false;
            s.share = VulkanShare();
        }
        return false;
    }
    // Still inside a pipeline. Drop the share so nothing adopts the device
    // again, and let a thread of its own do the waiting. `share` itself is
    // left alone: the model holds the lock context it was given and goes on
    // calling it until it stops, which is exactly what the caller is being
    // told to keep alive until `finish`.
    {
        std::lock_guard<std::mutex> share_lock(s.share_mutex);
        s.has_share = false;
        s.share_pending = false;
    }
    std::thread([&s, finish, context] {
        {
            std::lock_guard<std::mutex> lock(s.mutex);
            nr_frame_cancel_open(0);
            if (s.opened_shared)
                CloseLocked(s);
            std::lock_guard<std::mutex> share_lock(s.share_mutex);
            s.share = VulkanShare();
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
// Wider does not help: a pass's correction also carries a halo around a sprite, but that
// came from the history, and kReleaseRadius keeps it out of the pass in the first place.
// Spread wider here, the correction is dropped to the cast (kCastBits) over a box around
// anything that moves, and the box shows.
constexpr int kMotionRadius = 2;

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
// correction was remembered where he had been. Twelve frames is longer than a pass is
// old, so what has stood that long is also what the pass was given there.
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

// The cast of a pass that turned `in` into `out` (RGB8, `pixels` long): for each colour the
// mean change, three values an entry.
//
// A colour the frame did not have takes the mean over its neighbourhood of colours instead
// (kCastBits - 2 a channel), and failing that the frame's. The live picture is not limited
// to the pass's colours: a scroll brings in what was off screen, and interframe blending
// mixes two frames into colours neither had. Leaving those at no change drops the cast
// exactly where the picture moves, and the correction flickers off with it.
void BuildCast(const uint8_t* in, const uint8_t* out, size_t pixels, std::vector<int16_t>* cast,
               std::vector<int64_t>* sums) {
    const auto coarse_of = [](size_t k) {
        const size_t r = k >> (2 * kCastBits), g = (k >> kCastBits) & ((1u << kCastBits) - 1),
                     b = k & ((1u << kCastBits) - 1);
        constexpr int drop = kCastBits - kCoarseBits;
        return ((r >> drop) << (2 * kCoarseBits)) | ((g >> drop) << kCoarseBits) | (b >> drop);
    };
    sums->assign((kCastEntries + kCoarseEntries + 1) * 4, 0);
    int64_t* const fine = sums->data();
    int64_t* const coarse = fine + kCastEntries * 4;
    int64_t* const all = coarse + kCoarseEntries * 4;
    for (size_t i = 0; i < pixels; i++, in += 3, out += 3) {
        int64_t* e = fine + CastIndex(in[0], in[1], in[2]) * 4;
        for (int c = 0; c < 3; c++)
            e[c] += out[c] - in[c];
        e[3]++;
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

// out[i] = max(out[i], in[i]) over n bytes. SSE2 is baseline on x86-64, so the
// sixteen-at-a-time form needs no run-time check; elsewhere the scalar loop is
// what the compiler was already making of it.

// See RowPool::Run: measured, not guessed.
constexpr int kDilateBands = 4;

inline void MaxInto(uint8_t* out, const uint8_t* in, int n) {
    int i = 0;
#if defined(__SSE2__) || (defined(_M_X64) && !defined(_M_ARM64EC)) ||     (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
    for (; i + 16 <= n; i += 16) {
        const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(out + i));
        const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out + i), _mm_max_epu8(a, b));
    }
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
    for (size_t i = 0; i < base.size(); i++)
        base[i] = static_cast<uint8_t>((rgb[3 * i] + 2 * rgb[3 * i + 1] + rgb[3 * i + 2] + 2) >> 2);
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
#if defined(__SSE2__) || (defined(_M_X64) && !defined(_M_ARM64EC)) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
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
        best = 1e18;
        for (int y = cy - reach; y <= cy + reach; y++)
            for (int x = cx - reach; x <= cx + reach; x++) {
                const double cost = ShiftCost(live, pass, l, x, y, step);
                if (cost < best) {
                    best = cost;
                    bx = x;
                    by = y;
                }
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
// channel, without looking at which channel a byte is. `_mm_avg_epu8` is that very mean, and
// the two saturating differences are the absolute one.
inline void AbsDiffMean(const uint8_t* l, const uint8_t* a, const uint8_t* b, uint8_t* out,
                        size_t n) {
    size_t i = 0;
#if defined(__SSE2__) || (defined(_M_X64) && !defined(_M_ARM64EC)) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
    for (; i + 16 <= n; i += 16) {
        const __m128i lv = _mm_loadu_si128(reinterpret_cast<const __m128i*>(l + i));
        const __m128i g = _mm_avg_epu8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i)),
                                       _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i)));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out + i),
                         _mm_or_si128(_mm_subs_epu8(lv, g), _mm_subs_epu8(g, lv)));
    }
#endif
    for (; i < n; i++) {
        const int g = (a[i] + b[i] + 1) >> 1;
        out[i] = static_cast<uint8_t>(l[i] > g ? l[i] - g : g - l[i]);
    }
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
};

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
    // Apply32()'s live frame as RGB8 and its pyramid; how it lines up with the pass on
    // screen and the one before it; and the dilation's scratch. Members so the frame does not allocate.
    std::vector<uint8_t> live;
    Pyramid live_pyramid;
    std::array<Alignment, 2> alignment;  // the pass on screen, the one before it
    std::vector<std::vector<uint8_t>> dilate_scratch;  // one per motion mask (DilateAll)
    // The frame weight actually in use, carried between frames so it can only climb back
    // gradually -- see kSceneRisePerFrame.
    int scene_weight = 256;
    int display_width = 0;
    int display_height = 0;

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
    void UpdateMemory(const uint8_t* liveb);
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
            BuildCast(p->src_rgb.data(), p->rgb.data(), static_cast<size_t>(p->width) * p->height,
                      &p->cast, &p->cast_sums);
        lock.lock();

        if (ok) {
            // The shown buffer comes back as this packet's, for a later frame.
            result.swap(p->rgb);
            result_src.swap(p->src_rgb);
            result_cast.swap(p->cast);
            result_width = p->width;
            result_height = p->height;
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
void Filter::Impl::UpdateMemory(const uint8_t* liveb) {
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
    for (size_t i = 0; i < px; i++) {
        bool take = !memory_valid[i];
        if (!take && have_age && age[i] >= kMemoryStillFrames) {
            const uint8_t* a = now + i * 3;
            const uint8_t* b = liveb + i * 3;
            take = std::abs(a[0] - b[0]) <= kHoldLevels && std::abs(a[1] - b[1]) <= kHoldLevels &&
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
        // A frame arriving while the pipeline is full is skipped.
        if (!im.failed.load() && !im.quit.load() && !im.free_packets.empty() &&
            !im.queued[kStageFeatures]) {
            Packet* p = im.free_packets.back();
            im.free_packets.pop_back();
            p->rgb.resize(static_cast<size_t>(width) * height * 3);
            uint8_t* o = p->rgb.data();
            for (int y = 0; y < height; y++) {
                const uint32_t* s =
                    reinterpret_cast<const uint32_t*>(src + static_cast<size_t>(y) * instride);
                for (int x = 0; x < width; x++, o += 3) {
                    const uint32_t v = s[x];
                    o[0] = static_cast<uint8_t>(v >> red_shift);
                    o[1] = static_cast<uint8_t>(v >> green_shift);
                    o[2] = static_cast<uint8_t>(v >> blue_shift);
                }
            }
            p->width = width;
            p->height = height;
            p->filter_width = filter_width;
            p->filter_height = filter_height;
            BuildParams(&p->params, settings);
            p->want_history = settings.history;
            p->control_mask = settings.control_mask;
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
            im.result_ready = false;
            if (im.display_src.size() ==
                static_cast<size_t>(im.display_width) * im.display_height * 3)
            {
                BuildPyramid(im.display_src.data(), im.display_width, im.display_height,
                             PyramidLevels(im.display_width, im.display_height),
                             &im.display_pyramid);
                im.display_delta.resize(im.display_src.size());
                for (size_t i = 0; i < im.display_src.size(); i++)
                    im.display_delta[i] = static_cast<int16_t>(im.display[i] - im.display_src[i]);
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
        const size_t px = static_cast<size_t>(width) * height;
        if (!im.display.empty() && im.display.size() == im.display_src.size() &&
            im.display_width == width && im.display_height == height &&
            im.display_cast.size() == kCastEntries * 3 &&
            im.display_delta.size() == im.display.size()) {
            const uint8_t* const in = im.display_src.data();
            const int16_t* const cast = im.display_cast.data();

            // The live frame, read in full before anything is written: dst may alias src.
            im.live.resize(px * 3);
            for (int y = 0; y < height; y++) {
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
            const uint8_t* const live = im.live.data();

            // How long each pixel has held still, for the correction memory.
            if (im.age.size() != px || im.live_previous.size() != px * 3) {
                im.age.assign(px, 0);
                im.live_previous.assign(live, live + px * 3);
            } else {
                // Each channel's change sixteen at a time (the mean of the previous frame with
                // itself is the previous frame), then a pixel is still if none moved.
                const uint8_t* lp = im.live_previous.data();
                im.age_diff.resize(px * 3);
                AbsDiffMean(live, lp, lp, im.age_diff.data(), px * 3);
                const uint8_t* d = im.age_diff.data();
                uint8_t* age = im.age.data();
                for (size_t i = 0; i < px; i++, d += 3) {
                    const bool still = std::max(d[0], std::max(d[1], d[2])) <= kHoldLevels;
                    age[i] = still ? static_cast<uint8_t>(age[i] + (age[i] < 255)) : 0;
                }
                std::memcpy(im.live_previous.data(), live, px * 3);
            }
            if (landed)
                im.UpdateMemory(live);

            // How far each pixel has moved since the pass was given the frame, if the pass
            // is taken as the mean of itself shifted by `h.a` and by `h.b` -- one shift
            // when they are the same: the largest step of the three channels, or all of it
            // where a shift takes the pixel off the pass's edge.
            //
            // `valid`, when given, says which of `given`'s pixels hold anything (the
            // correction memory's); a pixel reading one that does not has moved all of it.
            const auto measure = [&](const uint8_t* given, const uint8_t* valid,
                                     const Hypothesis& h, uint8_t* motion) {
                RowPool::Get().Run(height, [&](int y_begin, int y_end) {
                    MeasureRows(live, given, valid, h, width, height, y_begin, y_end, motion);
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
                std::vector<uint8_t>& still = al->motion[0];
                still.resize(px);
                measure(given, nullptr, al->h[0], still.data());
                // Whether anything is worth lining up at all: any motion counts here.
                size_t count = 0;
                for (size_t i = 0; i < px; i++)
                    count += still[i] > kHoldLevels;
                if (count * 100 <= px * kScenePercentHold)
                    return count;
                // What the scene is weighed by: the mild residue (kSceneMildLevels).
                const auto mild = [&](uint8_t m) { return m > kHoldLevels && m <= kSceneMildLevels; };
                count = 0;
                for (size_t i = 0; i < px; i++)
                    count += mild(still[i]);
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
                    al->motion[k].resize(px);
                    measure(given, nullptr, al->h[k], al->motion[k].data());
                }
                count = 0;
                for (size_t i = 0; i < px; i++)
                    count += mild(std::min({ al->motion[0][i], al->motion[1][i], al->motion[2][i] }));
                return count;
            };
            Alignment& now_al = im.alignment[0];
            Alignment& was_al = im.alignment[1];
            was_al.count = 0;
            const size_t moved_pixels = track(in, im.display_pyramid, &now_al);

            // How much of the frame is unmistakably moving, and so how much of the pass's
            // own correction is worth laying down at all beyond the cast. None of it, when
            // the frame is all moving.
            const int scene = static_cast<int>(moved_pixels * 100 / px);
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
                    track(im.previous_display_src.data(), im.previous_pyramid, &was_al);
                if (have_memory) {
                    mem_al.count = now_al.count;
                    for (int k = 0; k < now_al.count; k++) {
                        mem_al.h[k] = now_al.h[k];
                        mem_al.motion[k].resize(px);
                        measure(im.memory_src.data(), im.memory_valid.data(), mem_al.h[k],
                                mem_al.motion[k].data());
                    }
                }
                uint8_t* masks[9];
                uint8_t* across[9];
                int n_masks = 0;
                for (Alignment* al : { &now_al, &was_al, &mem_al })
                    for (int k = 0; k < al->count; k++)
                        masks[n_masks++] = al->motion[k].data();
                if (im.dilate_scratch.size() < static_cast<size_t>(n_masks))
                    im.dilate_scratch.resize(static_cast<size_t>(n_masks));
                for (int k = 0; k < n_masks; k++) {
                    im.dilate_scratch[k].resize(px);
                    across[k] = im.dilate_scratch[k].data();
                }
                DilateAll(masks, across, n_masks, width, height, kMotionRadius);
            }

            // Which way a pixel takes a pass -- the one its motion fits best -- and how much of
            // the pass's own correction it keeps: it reads the mean of the pass's pixels at
            // the hypothesis's two offsets. A weight of 0 reads nothing, so the offsets need
            // no check when a shift took the pixel off the pass's edge: that motion is 255.
            std::array<int, 256> weight;  // MotionWeight() times the scene's, by motion
            for (int m = 0; m < 256; m++)
                weight[m] = MotionWeight(m) * scene_weight / 256;
            struct Lookup {
                int count = 0;
                const uint8_t* motion[3] = {};
                ptrdiff_t a[3] = {}, b[3] = {};
            };
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
            const Lookup now_l = lookup(now_al, scene_weight > 0);
            const Lookup was_l = lookup(was_al, scene_weight > 0 && have_previous);
            const Lookup mem_l = lookup(mem_al, have_memory);
            const auto choose = [&weight](const Lookup& l, size_t i, size_t* a, size_t* b) {
                if (l.count == 0)
                    return 0;
                int best = 0;
                uint8_t m = l.motion[0][i];
                for (int k = 1; k < l.count; k++)
                    if (l.motion[k][i] < m) {
                        m = l.motion[k][i];
                        best = k;
                    }
                *a = i + l.a[best];
                *b = i + l.b[best];
                return weight[m];
            };

            const int shift[3] = { red_shift, green_shift, blue_shift };
            // Shifts rather than divisions below: at a full weight of 256 they are exact, so
            // a still picture still gets the pass's own output.
            const int16_t* const now_delta = im.display_delta.data();
            const int16_t* const mem_delta = have_memory ? im.memory_delta.data() : nullptr;
            const int16_t* const was_cast = have_previous ? im.previous_cast.data() : nullptr;
            const int16_t* const was_delta = have_previous ? im.previous_delta.data() : nullptr;
            // The two frame-wide choices as constants, so each case compiles to a loop of its
            // own without the other's work. have_previous is only ever set while crossing.
            const auto rows = [&](auto crossing_c, auto previous_c, int y_begin, int y_end) {
            constexpr bool kCrossing = decltype(crossing_c)::value;
            constexpr bool kPrevious = decltype(previous_c)::value;
            for (int y = y_begin; y < y_end; y++) {
                uint32_t* d = reinterpret_cast<uint32_t*>(dst + static_cast<size_t>(y) * outstride);
                const uint8_t* l = live + static_cast<size_t>(y) * width * 3;
                const size_t row = static_cast<size_t>(y) * width;
                for (int x = 0; x < width; x++, l += 3) {
                    const size_t i = row + x;
                    const int16_t* const now_cast = cast + CastIndex(l[0], l[1], l[2]) * 3;
                    size_t now_a = 0, now_b = 0, was_a = 0, was_b = 0, mem_a = 0, mem_b = 0;
                    const int w = choose(now_l, i, &now_a, &now_b);
                    const int was_w = kPrevious ? choose(was_l, i, &was_a, &was_b) : 0;
                    // The memory only matters where the pass on screen (or the one
                    // before, while crossing) does not fit the live picture itself.
                    const int mem_w = (w < 256 || (kCrossing && was_w < 256))
                                          ? choose(mem_l, i, &mem_a, &mem_b)
                                          : 0;
                    uint32_t p = 0;
                    for (int c = 0; c < 3; c++) {
                        // The cast for the colour now there, then what is remembered of the
                        // background here as far as the live picture still matches it,
                        // then the pass's own correction as far as it does.
                        int delta = now_cast[c];
                        const int remembered =
                            mem_w > 0 ? (mem_delta[3 * mem_a + c] + mem_delta[3 * mem_b + c]) >> 1 : 0;
                        if (mem_w > 0)
                            delta += (remembered - delta) * mem_w >> 8;
                        if (w > 0)
                            delta += (((now_delta[3 * now_a + c] + now_delta[3 * now_b + c]) >> 1) -
                                      delta) *
                                         w >>
                                     8;
                        if (kCrossing) {
                            // Nothing to come from before the first pass, or after a
                            // resize: the correction then fades up out of nothing.
                            int was = 0;
                            if (kPrevious) {
                                was = was_cast[(now_cast - cast) + c];
                                if (mem_w > 0)
                                    was += (remembered - was) * mem_w >> 8;
                                if (was_w > 0)
                                    was += (((was_delta[3 * was_a + c] +
                                              was_delta[3 * was_b + c]) >>
                                             1) -
                                            was) *
                                               was_w >>
                                           8;
                            }
                            delta = was + ((delta - was) * blend >> 8);
                        }
                        const int r = l[c] + delta;
                        p |= static_cast<uint32_t>(r < 0 ? 0 : r > 255 ? 255 : r) << shift[c];
                    }
                    d[x] = p;
                }
            }
            };
            RowPool::Get().Run(height, [&](int y_begin, int y_end) {
                if (have_previous)
                    rows(std::true_type{}, std::true_type{}, y_begin, y_end);
                else if (crossing)
                    rows(std::true_type{}, std::false_type{}, y_begin, y_end);
                else
                    rows(std::false_type{}, std::false_type{}, y_begin, y_end);
            });
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
