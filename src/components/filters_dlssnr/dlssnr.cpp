#include "components/filters_dlssnr/dlssnr.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include "nr_frame.h"

namespace dlssnr {

namespace {

// The model, shared by every Filter in the process. libxmx is one Vulkan device
// with process-wide state, so every libnr_frame call goes through `mutex`.
struct SharedModel {
    std::mutex mutex;
    nr_frame* frame = nullptr;
    int users = 0;
    std::string device;
    // A renderer's Vulkan objects to adopt at the next open, if any.
    bool has_share = false;
    VulkanShare share;
    // The open model runs on the shared device.
    bool opened_shared = false;
    // Why the last adoption attempt fell back to libxmx's own instance, if it did.
    std::string share_error;
};

SharedModel& Shared() {
    static SharedModel model;
    return model;
}

// Closes the model and releases the device libxmx holds (its own, or a shared
// one back to its renderer). Caller holds `mutex`.
void CloseLocked(SharedModel& s) {
    if (s.frame) {
        nr_frame_close(s.frame);
        s.frame = nullptr;
    }
    nr_frame_shutdown();
    s.opened_shared = false;
}

// Opens the model, on the shared device when one is registered. Caller holds
// `mutex`. Returns false and fills `error` on failure.
bool OpenLocked(SharedModel& s, std::string* error) {
    s.share_error.clear();
    bool adopted = false;
    if (s.has_share) {
        const VulkanShare& v = s.share;
        if (nr_frame_adopt_vulkan(v.instance, v.physical_device, v.device, v.queue,
                                  v.queue_family, v.cooperative_matrix ? 1 : 0,
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
    s.device = std::string(nr_frame_device(s.frame)) + ", " + nr_frame_gemm_path(s.frame);
    if (s.opened_shared)
        s.device += " [renderer's Vulkan device]";
    else if (!s.share_error.empty())
        s.device += " [own instance; sharing failed: " + s.share_error + "]";
    return true;
}

void AcquireModel() {
    SharedModel& s = Shared();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.users++;
}

void ReleaseModel() {
    SharedModel& s = Shared();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (s.users > 0 && --s.users == 0)
        CloseLocked(s);
}

// `save_image` in nr_frame_main.c: clip to [0, 1], then `v * 255 + 0.5` to a byte.
}  // namespace

void ShareVulkan(const VulkanShare& share) {
    SharedModel& s = Shared();
    std::lock_guard<std::mutex> lock(s.mutex);
    // Whatever is open runs elsewhere; close it so the next pass reopens here.
    CloseLocked(s);
    s.share = share;
    s.has_share = true;
}

void WithdrawVulkanShare(const void* owner) {
    SharedModel& s = Shared();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (!s.has_share || s.share.owner != owner)
        return;
    if (s.opened_shared)
        CloseLocked(s);
    s.has_share = false;
    s.share = VulkanShare();
}

bool UsingSharedVulkan() {
    SharedModel& s = Shared();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.frame && s.opened_shared;
}

namespace {

inline uint8_t ToByte(float v) {
    v = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
    return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

// `read_png` in nr_frame_main.c: `byte / 255`, a division rather than a multiply by the
// reciprocal, so the float the network sees is the one the command gives it.
inline float FromByte(uint32_t v) {
    return static_cast<float>(v & 0xffu) / 255.0f;
}

}  // namespace

struct Filter::Impl {
    // Hand-off state, under `mutex`.
    std::mutex mutex;
    std::condition_variable cv;
    bool quit = false;
    // The newest source frame, float RGB in [0, 1], (height, width, 3).
    std::vector<float> pending;
    int pending_width = 0;
    int pending_height = 0;
    bool has_pending = false;
    // The newest finished result, RGB8, (height, width, 3).
    std::vector<uint8_t> result;
    int result_width = 0;
    int result_height = 0;
    bool has_result = false;

    // Status, readable without the lock.
    std::atomic<bool> ready{false};
    std::atomic<bool> failed{false};
    std::atomic<double> last_ms{0.0};
    std::atomic<uint64_t> frames_done{0};
    // Written by the worker before `failed`/`ready` are published, read after.
    std::string error;
    std::string device;

    std::thread worker;

    void Run();
};

void Filter::Impl::Run() {
    AcquireModel();

    // Exactly what the `nr_frame` command does to a picture with no flags: the `standard`
    // profile (`nr_frame_defaults`: style 0, local tone 1, local structure 1, intensity 1),
    // frame index 0 for the noise channels, no control mask, and the *still* path — no
    // history in the feature channels and no temporal composition — so every frame stands
    // alone and the output for a given input is the same as `nr_frame IN.png OUT.png`.
    nr_frame_params params;
    nr_frame_defaults(&params);

    std::vector<float> input, output;

    for (;;) {
        int width, height;
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [&] { return quit || has_pending; });
            if (quit)
                break;
            input.swap(pending);
            has_pending = false;
            width = pending_width;
            height = pending_height;
        }

        const size_t count = static_cast<size_t>(width) * height * 3;
        output.resize(count);

        const auto start = std::chrono::steady_clock::now();
        int rc;
        {
            SharedModel& s = Shared();
            std::lock_guard<std::mutex> lock(s.mutex);
            // The model opens on the first pass, and again after a renderer
            // lent or withdrew its device (ShareVulkan / WithdrawVulkanShare).
            if (!s.frame) {
                if (!OpenLocked(s, &error)) {
                    failed.store(true);
                    break;
                }
                device = s.device;
                ready.store(true);
            }
            // With no history, no previous input and no mask this is the command's
            // sequence — features, the graph, the head cropped to the picture, the
            // composition against the untouched source — in one call.
            rc = nr_frame_update(s.frame, input.data(), height, width, nullptr, nullptr, &params,
                                 output.data(), nullptr);
            if (rc)
                error = nr_frame_error();
        }
        if (rc) {
            if (error.empty())
                error = "nr_frame_update failed";
            failed.store(true);
            break;
        }
        last_ms.store(std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - start)
                          .count());

        {
            std::lock_guard<std::mutex> lock(mutex);
            result.resize(count);
            for (size_t i = 0; i < count; i++)
                result[i] = ToByte(output[i]);
            result_width = width;
            result_height = height;
            has_result = true;
        }
        frames_done.fetch_add(1);
    }

    ReleaseModel();
}

Filter::Filter() : impl_(new Impl) {
    impl_->worker = std::thread([impl = impl_.get()] { impl->Run(); });
}

Filter::~Filter() {
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->quit = true;
    }
    impl_->cv.notify_all();
    if (impl_->worker.joinable())
        impl_->worker.join();
}

void Filter::Apply32(const uint8_t* src, int instride, uint8_t* dst, int outstride, int width,
                     int height, int red_shift, int green_shift, int blue_shift) {
    if (width <= 0 || height <= 0)
        return;
    Impl& im = *impl_;
    const size_t pixels = static_cast<size_t>(width) * height;

    bool wrote_result = false;
    {
        std::unique_lock<std::mutex> lock(im.mutex);

        if (im.has_result && im.result_width == width && im.result_height == height) {
            const uint8_t* r = im.result.data();
            for (int y = 0; y < height; y++) {
                uint32_t* d = reinterpret_cast<uint32_t*>(dst + static_cast<size_t>(y) * outstride);
                for (int x = 0; x < width; x++, r += 3) {
                    d[x] = (static_cast<uint32_t>(r[0]) << red_shift) |
                           (static_cast<uint32_t>(r[1]) << green_shift) |
                           (static_cast<uint32_t>(r[2]) << blue_shift);
                }
            }
            wrote_result = true;
        }

        // Hand the newest frame to the worker; a frame it never got to is
        // simply replaced, so the emulator is never held back by the network.
        if (!im.failed.load() && !im.quit) {
            im.pending.resize(pixels * 3);
            float* p = im.pending.data();
            for (int y = 0; y < height; y++) {
                const uint32_t* s =
                    reinterpret_cast<const uint32_t*>(src + static_cast<size_t>(y) * instride);
                for (int x = 0; x < width; x++, p += 3) {
                    const uint32_t v = s[x];
                    p[0] = FromByte(v >> red_shift);
                    p[1] = FromByte(v >> green_shift);
                    p[2] = FromByte(v >> blue_shift);
                }
            }
            im.pending_width = width;
            im.pending_height = height;
            im.has_pending = true;
            lock.unlock();
            im.cv.notify_one();
        }
    }

    if (!wrote_result) {
        // Nothing finished yet (or the model is unavailable): pass through.
        const size_t row_bytes = pixels / height * 4;
        for (int y = 0; y < height; y++)
            memcpy(dst + static_cast<size_t>(y) * outstride,
                   src + static_cast<size_t>(y) * instride, row_bytes);
    }
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
    return impl_->error;
}

std::string Filter::Device() const {
    if (!impl_->ready.load())
        return std::string();
    return impl_->device;
}

double Filter::LastFrameMs() const {
    return impl_->last_ms.load();
}

uint64_t Filter::FramesDone() const {
    return impl_->frames_done.load();
}

}  // namespace dlssnr
