#include "components/filters_dlssnr/dlssnr.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
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
constexpr int kMotionRadius = 2;

// When the picture as a whole is moving, how much of it has to be moving before the
// correction is wound down everywhere, as a percentage of the frame. A pixel counts as
// moving when it is past kHoldLevels -- the question here is how much of the frame is not
// standing still, not how much of it changed beyond recognition.
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
    std::vector<float> mask;      // the control mask at the filter extent, when use_mask
    std::vector<float> scratch;
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
    int result_width = 0;
    int result_height = 0;
    std::vector<uint8_t> display;
    std::vector<uint8_t> display_src;
    // The pass before it, and how far across the two the composition currently is: a new
    // pass arrives at 0 and takes over as this reaches 256.
    std::vector<uint8_t> previous_display;
    std::vector<uint8_t> previous_display_src;
    int previous_width = 0;
    int previous_height = 0;
    int blend = 256;
    // Apply32()'s motion mask and its horizontal dilation, one byte a pixel. Members so
    // the frame does not allocate.
    std::vector<uint8_t> motion;
    std::vector<uint8_t> motion_wide;
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
    bool BuildFeatures(Packet* p);
    bool RunNetwork(Packet* p);
    bool Compose(Packet* p);

    void FeaturesStage();
    void NetworkStage();
    void ComposeStage();
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

bool Filter::Impl::BuildFeatures(Packet* p) {
    std::shared_lock<std::shared_mutex> use;
    nr_frame* frame = Borrow(p, &use, true);
    if (!frame)
        return false;

    const float* hist = p->use_history ? history.data() : nullptr;
    const float* mask = p->use_mask ? p->mask.data() : nullptr;
    if (nr_frame_features_masked(frame, p->colour.data(), p->filter_height, p->filter_width,
                                 hist, mask, &p->params, p->features.data()) != 0) {
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
        const float* prev = p->use_history ? previous.data() : nullptr;
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
        lock.lock();

        if (ok) {
            // The shown buffer comes back as this packet's, for a later frame.
            result.swap(p->rgb);
            result_src.swap(p->src_rgb);
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
}

Filter::~Filter() {
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

        if (im.result_ready) {
            // What is on screen becomes what the new pass crosses over from.
            im.previous_display.swap(im.display);
            im.previous_display_src.swap(im.display_src);
            im.previous_width = im.display_width;
            im.previous_height = im.display_height;
            im.blend = 0;
            im.display.swap(im.result);
            im.display_src.swap(im.result_src);
            im.display_width = im.result_width;
            im.display_height = im.result_height;
            im.result_ready = false;
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
        // how far the picture has moved since: full where nothing has changed, nothing
        // where it has. On a still picture every pixel is untouched, the weights are all
        // one, and this reproduces the pass's output exactly.
        if (!im.display.empty() && im.display.size() == im.display_src.size() &&
            im.display_width == width && im.display_height == height) {
            const uint8_t* const out = im.display.data();
            const uint8_t* const in = im.display_src.data();
            const size_t px = static_cast<size_t>(width) * height;
            im.motion.resize(px);
            im.motion_wide.resize(px);
            size_t moved_pixels = 0;

            // How far each pixel has moved since the pass was given the frame.
            for (int y = 0; y < height; y++) {
                const uint32_t* s =
                    reinterpret_cast<const uint32_t*>(src + static_cast<size_t>(y) * instride);
                const uint8_t* a = in + static_cast<size_t>(y) * width * 3;
                uint8_t* mv = im.motion.data() + static_cast<size_t>(y) * width;
                for (int x = 0; x < width; x++, a += 3) {
                    const uint32_t v = s[x];
                    const int live[3] = { static_cast<int>((v >> red_shift) & 0xffu),
                                          static_cast<int>((v >> green_shift) & 0xffu),
                                          static_cast<int>((v >> blue_shift) & 0xffu) };
                    int m = 0;
                    for (int c = 0; c < 3; c++) {
                        const int d = live[c] > a[c] ? live[c] - a[c] : a[c] - live[c];
                        if (d > m)
                            m = d;
                    }
                    if (m > kHoldLevels)
                        moved_pixels++;
                    mv[x] = static_cast<uint8_t>(m);
                }
            }

            // How much of the frame is unmistakably moving, and so how much of the
            // correction is worth laying down at all. Nothing, when it is all moving:
            // the source then goes through untouched and the spreading below is skipped.
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
                im.previous_width == width && im.previous_height == height;

            if (scene_weight > 0) {
                // Spread it, separably: across, then down as the composition reads it.
                for (int y = 0; y < height; y++) {
                    const uint8_t* mv = im.motion.data() + static_cast<size_t>(y) * width;
                    uint8_t* wide = im.motion_wide.data() + static_cast<size_t>(y) * width;
                    for (int x = 0; x < width; x++) {
                        const int x0 = x > kMotionRadius ? x - kMotionRadius : 0;
                        const int x1 = x + kMotionRadius < width - 1 ? x + kMotionRadius : width - 1;
                        int m = 0;
                        for (int k = x0; k <= x1; k++)
                            if (mv[k] > m)
                                m = mv[k];
                        wide[x] = static_cast<uint8_t>(m);
                    }
                }

                const int shift[3] = { red_shift, green_shift, blue_shift };
                for (int y = 0; y < height; y++) {
                    const int y0 = y > kMotionRadius ? y - kMotionRadius : 0;
                    const int y1 = y + kMotionRadius < height - 1 ? y + kMotionRadius : height - 1;
                    const uint32_t* s =
                        reinterpret_cast<const uint32_t*>(src + static_cast<size_t>(y) * instride);
                    uint32_t* d = reinterpret_cast<uint32_t*>(dst + static_cast<size_t>(y) * outstride);
                    const uint8_t* a = in + static_cast<size_t>(y) * width * 3;
                    const uint8_t* b = out + static_cast<size_t>(y) * width * 3;
                    const size_t row = static_cast<size_t>(y) * width * 3;
                    const uint8_t* pa =
                        have_previous ? im.previous_display_src.data() + row : nullptr;
                    const uint8_t* pb =
                        have_previous ? im.previous_display.data() + row : nullptr;
                    for (int x = 0; x < width; x++, a += 3, b += 3) {
                        int moved = 0;
                        for (int k = y0; k <= y1; k++) {
                            const uint8_t m = im.motion_wide[static_cast<size_t>(k) * width + x];
                            if (m > moved)
                                moved = m;
                        }
                        // dst may alias src; this reads the pixel before it writes it.
                        const uint32_t v = s[x];
                        const int local = moved <= kHoldLevels
                                              ? 256
                                              : (moved >= kFadeLevels
                                                     ? 0
                                                     : 256 - (moved - kHoldLevels) * 256 /
                                                                 (kFadeLevels - kHoldLevels));
                        const int w = local * scene_weight / 256;
                        if (w == 0) {
                            d[x] = v;
                            continue;
                        }
                        uint32_t p = 0;
                        for (int c = 0; c < 3; c++) {
                            const int live = static_cast<int>((v >> shift[c]) & 0xffu);
                            int delta = b[c] - a[c];
                            if (crossing) {
                                // Nothing to come from before the first pass, or after a
                                // resize: the correction then fades up out of nothing.
                                const int was = have_previous ? pb[3 * x + c] - pa[3 * x + c] : 0;
                                delta = was + (delta - was) * blend / 256;
                            }
                            const int r = live + delta * w / 256;
                            p |= static_cast<uint32_t>(r < 0 ? 0 : r > 255 ? 255 : r) << shift[c];
                        }
                        d[x] = p;
                    }
                }
                wrote_result = true;
            }
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
