#ifndef VBAM_WX_RPI_PROXY_SHARED_FILTER_BUFFER_H_
#define VBAM_WX_RPI_PROXY_SHARED_FILTER_BUFFER_H_

#if defined(_WIN32)

#include <windows.h>
#include <cstdint>

namespace rpi_proxy {

// Maximum supported dimensions for filter operations
// GBA: 240x160, scaled 6x = 1440x960
// GB:  160x144, scaled 6x = 960x864
// Using 2048x2048 as safe maximum
constexpr uint32_t kMaxWidth = 2048;
constexpr uint32_t kMaxHeight = 2048;
constexpr uint32_t kMaxBytesPerPixel = 4;  // 32-bit RGBA

// Maximum buffer size for source or destination
// We need space for both src and dst buffers
constexpr uint32_t kMaxPixelBufferSize = kMaxWidth * kMaxHeight * kMaxBytesPerPixel;

// Total shared memory size (src buffer + dst buffer + header)
constexpr uint32_t kSharedMemorySize = (kMaxPixelBufferSize * 2) + 4096;

// IPC object name templates - include PID and instance ID for uniqueness
// This prevents conflicts when multiple proxy clients exist in the same process
// (e.g., one for rendering, one for Video Config plugin enumeration)
constexpr const wchar_t* kSharedMemoryNameFmt = L"Local\\VBAMRpiFilterBuffer_%u_%u";
constexpr const wchar_t* kRequestEventNameFmt = L"Local\\VBAMRpiFilterRequest_%u_%u";
constexpr const wchar_t* kResponseEventNameFmt = L"Local\\VBAMRpiFilterResponse_%u_%u";

// Maximum number of concurrent filter threads
constexpr uint32_t kMaxFilterThreads = 8;

// Per-thread IPC object name templates (include PID, instance ID, and thread ID)
constexpr const wchar_t* kThreadSharedMemoryNameFmt = L"Local\\VBAMRpiFilterBuffer_%u_%u_T%u";
constexpr const wchar_t* kThreadRequestEventNameFmt = L"Local\\VBAMRpiFilterRequest_%u_%u_T%u";
constexpr const wchar_t* kThreadResponseEventNameFmt = L"Local\\VBAMRpiFilterResponse_%u_%u_T%u";

// Helper to generate main channel IPC object names (pid = client process ID, instance_id = unique per proxy client)
inline void GetSharedMemoryName(uint32_t pid, uint32_t instance_id, wchar_t* buf, size_t buf_size) {
    swprintf_s(buf, buf_size, kSharedMemoryNameFmt, pid, instance_id);
}
inline void GetRequestEventName(uint32_t pid, uint32_t instance_id, wchar_t* buf, size_t buf_size) {
    swprintf_s(buf, buf_size, kRequestEventNameFmt, pid, instance_id);
}
inline void GetResponseEventName(uint32_t pid, uint32_t instance_id, wchar_t* buf, size_t buf_size) {
    swprintf_s(buf, buf_size, kResponseEventNameFmt, pid, instance_id);
}

// Helper to generate per-thread IPC object names (pid = client process ID, instance_id, thread_id = 0-7)
inline void GetThreadSharedMemoryName(uint32_t pid, uint32_t instance_id, uint32_t thread_id, wchar_t* buf, size_t buf_size) {
    swprintf_s(buf, buf_size, kThreadSharedMemoryNameFmt, pid, instance_id, thread_id);
}
inline void GetThreadRequestEventName(uint32_t pid, uint32_t instance_id, uint32_t thread_id, wchar_t* buf, size_t buf_size) {
    swprintf_s(buf, buf_size, kThreadRequestEventNameFmt, pid, instance_id, thread_id);
}
inline void GetThreadResponseEventName(uint32_t pid, uint32_t instance_id, uint32_t thread_id, wchar_t* buf, size_t buf_size) {
    swprintf_s(buf, buf_size, kThreadResponseEventNameFmt, pid, instance_id, thread_id);
}

// Command codes for IPC
enum class FilterCommand : uint32_t {
    None = 0,
    LoadPlugin = 1,
    ApplyFilter = 2,
    UnloadPlugin = 3,
    GetInfo = 4,
    Shutdown = 5,
    StartThread = 6,   // Start a filter thread in the host (thread_id in params)
    StopThread = 7     // Stop a filter thread in the host (thread_id in params)
};

// Result codes
enum class FilterResult : uint32_t {
    Success = 0,
    Failed = 1,
    PluginNotLoaded = 2,
    InvalidPlugin = 3,
    InvalidParameters = 4
};

// Plugin information structure (matches RENDER_PLUGIN_INFO layout)
struct PluginInfo {
    char name[60];
    uint32_t flags;
    uint32_t scale;  // Extracted scale factor
};

// Filter parameters for ApplyFilter command
struct FilterParams {
    uint32_t thread_id;  // Thread slot ID (0 to kMaxFilterThreads-1)
    uint32_t flags;
    uint32_t srcPitch;
    uint32_t srcW;
    uint32_t srcH;
    uint32_t dstPitch;
    uint32_t dstW;
    uint32_t dstH;
    uint32_t outW;
    uint32_t outH;
};

// Shared memory header structure
// This is placed at the start of the shared memory region
struct SharedFilterBuffer {
    // Version for compatibility checking
    uint32_t version;
    static constexpr uint32_t kCurrentVersion = 2;  // Bumped for sequence number support

    // Command and result (volatile to ensure cross-process visibility)
    volatile FilterCommand command;
    volatile FilterResult result;

    // Sequence number for detecting stale responses
    // Client sets request_seq before signaling, host copies to response_seq when done
    // If response_seq != request_seq, the response is stale (from a previous timed-out command)
    volatile uint32_t request_seq;
    volatile uint32_t response_seq;

    // Plugin path for LoadPlugin command (null-terminated wide string)
    wchar_t pluginPath[MAX_PATH];

    // Plugin info returned from LoadPlugin/GetInfo
    PluginInfo pluginInfo;

    // Filter parameters for ApplyFilter
    FilterParams filterParams;

    // Offsets to pixel data within shared memory
    uint32_t srcBufferOffset;
    uint32_t dstBufferOffset;

    // Reserved for future use
    uint8_t reserved[256];

    // Helper to get source buffer pointer
    void* GetSrcBuffer() {
        return reinterpret_cast<uint8_t*>(this) + srcBufferOffset;
    }

    const void* GetSrcBuffer() const {
        return reinterpret_cast<const uint8_t*>(this) + srcBufferOffset;
    }

    // Helper to get destination buffer pointer
    void* GetDstBuffer() {
        return reinterpret_cast<uint8_t*>(this) + dstBufferOffset;
    }

    const void* GetDstBuffer() const {
        return reinterpret_cast<const uint8_t*>(this) + dstBufferOffset;
    }

    // Initialize the buffer structure
    void Initialize() {
        version = kCurrentVersion;
        command = FilterCommand::None;
        result = FilterResult::Success;
        request_seq = 0;
        response_seq = 0;
        pluginPath[0] = L'\0';
        memset(&pluginInfo, 0, sizeof(pluginInfo));
        memset(&filterParams, 0, sizeof(filterParams));
        // Source buffer starts after the header (aligned to 4KB)
        srcBufferOffset = 4096;
        // Destination buffer starts after source buffer
        dstBufferOffset = srcBufferOffset + kMaxPixelBufferSize;
    }
};

// Ensure header fits in reserved space
static_assert(sizeof(SharedFilterBuffer) <= 4096,
    "SharedFilterBuffer header must fit in 4KB");

}  // namespace rpi_proxy

#endif  // _WIN32

#endif  // VBAM_WX_RPI_PROXY_SHARED_FILTER_BUFFER_H_
