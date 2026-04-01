#ifndef VBAM_WX_RPI_PROXY_RPI_PROXY_CLIENT_H_
#define VBAM_WX_RPI_PROXY_RPI_PROXY_CLIENT_H_

#if defined(_WIN32)

#include <windows.h>
#include <wx/string.h>
#include <cstdint>

#include "wx/rpi.h"
#include "wx/rpi-proxy/SharedFilterBuffer.h"

namespace rpi_proxy {

// Client class for communicating with the 32-bit RPI host process.
// This is used by the 64-bit VBA-M build to load and execute 32-bit RPI filter plugins.
class RpiProxyClient {
public:
    RpiProxyClient();
    ~RpiProxyClient();

    // Non-copyable
    RpiProxyClient(const RpiProxyClient&) = delete;
    RpiProxyClient& operator=(const RpiProxyClient&) = delete;

    // Load a 32-bit RPI plugin. Returns true on success.
    // On success, populates the info structure with plugin details.
    bool LoadPlugin(const wxString& path, RENDER_PLUGIN_INFO* info);

    // Apply the filter to the given pixel data.
    // srcPtr: source pixel buffer
    // srcPitch: source row stride in bytes
    // srcW/srcH: source dimensions
    // dstPtr: destination pixel buffer (must be large enough for scaled output)
    // dstPitch: destination row stride in bytes (used by plugin)
    // dstW/dstH: destination dimensions
    // thread_id: thread slot for multi-threaded filtering (0-7)
    // actualDstPitch: actual row stride of destination buffer (for stride conversion)
    //                 If 0, uses params->DstPitch (no conversion needed)
    void ApplyFilter(const RENDER_PLUGIN_OUTP* params, uint32_t thread_id = 0, uint32_t actualDstPitch = 0);

    // Start a filter thread in the host process.
    // This loads the plugin in a dedicated host thread for thread-safe filtering.
    // thread_id: thread slot (0 to kMaxFilterThreads-1)
    // Returns true on success.
    bool StartThread(uint32_t thread_id);

    // Stop a filter thread in the host process.
    // thread_id: thread slot to stop
    void StopThread(uint32_t thread_id);

    // Unload the current plugin
    void UnloadPlugin();

    // Check if a plugin is currently loaded
    bool IsLoaded() const { return is_loaded_; }

    // Get the loaded plugin's info
    const RENDER_PLUGIN_INFO* GetPluginInfo() const {
        return is_loaded_ ? &plugin_info_ : nullptr;
    }

    // Check if a DLL is a 32-bit PE file (used to determine if proxy is needed)
    static bool Is32BitDll(const wxString& path);

    // Check if the current process is 64-bit
    static bool IsProcess64Bit();

    // Check if we need to use the proxy for a given plugin
    static bool NeedsProxy(const wxString& pluginPath);

    // Check if the proxy is actively being used for rendering (has threads started)
    bool IsActivelyRendering() const;

    // Get the number of currently active threads (for saving before enumeration)
    uint32_t GetActiveThreadCount() const;

    // Restart threads after plugin reload (for restoring after enumeration)
    // Starts threads 0 to (count-1)
    void RestartThreads(uint32_t count);

    // Set the is_loaded_ state directly without side effects.
    // Used to restore state after enumeration without triggering UnloadPlugin.
    void SetLoadedState(bool loaded) { is_loaded_ = loaded; }

    // Save the current thread active states (returns a bitmask)
    uint32_t SaveThreadActiveStates() const;

    // Restore thread active states from a saved bitmask
    void RestoreThreadActiveStates(uint32_t states);

    // Get the shared/singleton proxy client instance.
    // Use this when you want to share a host process across multiple uses.
    // If the shared instance is actively rendering, consider creating a
    // temporary instance instead for enumeration to avoid interference.
    static RpiProxyClient* GetSharedInstance();

    // Release the shared instance (called on application shutdown)
    static void ReleaseSharedInstance();

    // Debug logging to file (can be called from other files)
    static void Log(const char* fmt, ...);

    // Reset ApplyFilter log counter (call after enumeration to see post-enumeration logs)
    static void ResetApplyFilterLogCount();

private:
    // Extract the embedded host exe to temp directory
    bool ExtractHostExe();

    // Launch the host process
    bool LaunchHost();

    // Terminate the host process (graceful shutdown)
    void TerminateHost();

    // Force terminate the host process immediately (for app exit)
    void ForceTerminateHost();

    // Initialize shared memory and events (main channel)
    bool InitializeIPC();

    // Clean up IPC resources
    void CleanupIPC();

    // Send a command and wait for response (main channel)
    bool SendCommand(FilterCommand cmd, uint32_t timeout_ms = 5000);

    // Initialize per-thread IPC channel
    bool InitializeThreadIPC(uint32_t thread_id);

    // Clean up per-thread IPC channel
    void CleanupThreadIPC(uint32_t thread_id);

    // Send a command on a thread's channel and wait for response
    bool SendThreadCommand(uint32_t thread_id, FilterCommand cmd, uint32_t timeout_ms = 5000);

    // Main channel shared memory (for control commands: LoadPlugin, UnloadPlugin, etc.)
    HANDLE shared_mem_handle_ = nullptr;
    SharedFilterBuffer* shared_mem_ = nullptr;

    // Main channel synchronization events
    HANDLE request_event_ = nullptr;
    HANDLE response_event_ = nullptr;

    // Mutex to protect main channel access from concurrent threads
    CRITICAL_SECTION main_channel_cs_;
    bool main_channel_cs_initialized_ = false;

    // Per-thread IPC state
    struct ThreadIPC {
        HANDLE shared_mem_handle = nullptr;
        SharedFilterBuffer* shared_mem = nullptr;
        HANDLE request_event = nullptr;
        HANDLE response_event = nullptr;
        uint32_t seq_counter = 0;  // Per-thread sequence counter
        bool active = false;
        CRITICAL_SECTION cs;       // Protects this thread's IPC during use
        bool cs_initialized = false;
    };
    ThreadIPC thread_ipc_[kMaxFilterThreads];

    // Main channel sequence counter for detecting stale responses
    uint32_t main_seq_counter_ = 0;

    // Host process
    HANDLE host_process_ = nullptr;
    wxString host_exe_path_;

    // Client process ID and instance ID (used for unique IPC names)
    // Instance ID ensures each proxy client gets its own IPC channel,
    // even within the same process (e.g., rendering vs Video Config enumeration)
    uint32_t client_pid_ = 0;
    uint32_t instance_id_ = 0;

    // Static counter for generating unique instance IDs
    static uint32_t s_next_instance_id_;

    // Shared singleton instance
    static RpiProxyClient* s_shared_instance_;

    // Plugin state
    bool is_loaded_ = false;
    RENDER_PLUGIN_INFO plugin_info_;
};

}  // namespace rpi_proxy

#endif  // _WIN32

#endif  // VBAM_WX_RPI_PROXY_RPI_PROXY_CLIENT_H_
