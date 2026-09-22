/*
 * nr_frame — one RGB frame in, one RGB frame out, through the recovered graph, in C.
 *
 * The C library that `src/ref/nr_frame.py` is: the same 16-channel feature assembly,
 * the same 71-block graph recorded against `libxmx` dispatch for dispatch as
 * `nr_frame_resident.py` records it, the same head-to-RGB composition. The head it
 * produces is bit-identical to the Python resident path on the same device — the
 * dispatches are the same, so the bytes are — and `src/ref/test_nr_frame_c.py` checks
 * that. The three places it can differ from NumPy by the last bit are named where they
 * happen: the noise channels, the temporal gate's exponential, and the detail blur.
 *
 * Images are float32 RGB in [0, 1], row-major (height, width, 3); the head is
 * (height, width, 4). One `nr_frame` holds the weights on the device and the graph for
 * the most recent extent; a new extent rebuilds the graph and keeps the weights.
 *
 *     nr_frame *f = nr_frame_open("work/mlxw/dlssnr-logical.safetensors");   // or NULL: the weights compiled in
 *     nr_frame_params p; nr_frame_defaults(&p);
 *     nr_frame_update(f, colour, height, width, NULL, NULL, &p, output, NULL);
 *     nr_frame_close(f);
 */
#ifndef NR_FRAME_H
#define NR_FRAME_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nr_frame nr_frame;

typedef struct nr_frame_params {
    /* the composition: post-network, free to sweep */
    float intensity;          /* blend of the model's picture against the source; > 1 extrapolates */
    float detail_strength;    /* high-frequency weight of the change (1 = unchanged) */
    float colour_strength;    /* low-frequency weight of the change (1 = unchanged) */
    float detail_radius;      /* sigma of the split, in pixels */
    /* the conditioning: reaches the network, so a change costs a forward pass */
    float normalized_style;   /* vendor style index / 128 */
    float local_tone;
    float local_structure;
    int   frame_index;        /* seeds the three noise channels */
    /* the temporal path, used when `history` is given */
    float history_confidence; /* scales the model's own gate: 0 the still path, 1 its answer */
    float blend_scale;        /* half(0.73974609375), the recovered package's */
    float hold;               /* floor under the gate where `previous` matches the colour: */
    float slope;              /*   max(alpha, clamp(moved * slope + hold, 0, hold) * blend_scale) */
    /* the automatic mask: skin structure and automatic-mask structure, each -1 to follow
     * `local_structure`; off unless `automatic_mask` is set */
    int   automatic_mask;
    float skin_structure;
    float automatic_structure;
} nr_frame_params;

/* The values `nr_frame.py` uses when nothing is asked: the `standard` profile. */
void nr_frame_defaults(nr_frame_params *params);

/* Load the logical weights and open the device. NULL on failure; `nr_frame_error()`.
 * A NULL path means the weights compiled into this library — CMake compiles the bin2c
 * slices in the weights directory (NR_WEIGHTS_DIR, regenerated from the safetensors with
 * NR_BIN2C_WEIGHTS) — and fails with a message in a build without them. */
nr_frame *nr_frame_open(const char *weights_path);

/* The size of the embedded safetensors, or 0 when this build carries none. */
size_t nr_frame_embedded_weights_size(void);
void nr_frame_close(nr_frame *frame);

/* Share a host's Vulkan instance and device instead of letting libxmx create its own.
 * Call before the first `nr_frame_open` (or after `nr_frame_shutdown`); the next open
 * takes the objects. Handles are `void *` (VkInstance, VkPhysicalDevice, VkDevice,
 * VkQueue) so no Vulkan header is needed here. The device needs Vulkan 1.3 with
 * storageBuffer16BitAccess, vulkanMemoryModel (+DeviceScope), shaderFloat16,
 * bufferDeviceAddress and scalarBlockLayout enabled, plus VK_KHR_portability_subset
 * where offered; `queue` must belong to a compute-capable family `queue_family`. If
 * the host submits on the same queue it passes `lock`/`unlock` (bracketing every
 * submit here) and takes the same lock around its own submits, presents and idle
 * waits. `get_instance_proc_addr` is the host's vkGetInstanceProcAddr. 0 on success;
 * `nr_frame_error()` otherwise, and libxmx keeps making its own device.
 *
 * Refused, with a message, when the runtime behind this library is Metal (the Apple
 * libdlssnr, or NR_GPU_BACKEND=metal) or Direct3D 12 (a Windows libdlssnr built with
 * NR_DLSSNR_D3D12, or NR_GPU_BACKEND=d3d12): there is nothing Vulkan to adopt, and the
 * runtime opens its own device — or, on Direct3D 12, takes the host's through
 * `nr_frame_adopt_d3d12`. `nr_frame_runtime()` says which one is behind. */
int nr_frame_adopt_vulkan(void *instance, void *physical_device, void *device, void *queue,
                          unsigned queue_family, int cooperative_matrix,
                          void *get_instance_proc_addr, void (*lock)(void *),
                          void (*unlock)(void *), void *lock_context);

/* The Direct3D 12 counterpart, for the libd3dmx runtime: share the host's ID3D12Device and
 * ID3D12CommandQueue (as `void *`, so no D3D header is needed here). Lists are recorded
 * for the queue's type, direct or compute. The device must offer Shader Model 6.2 and
 * native 16-bit shader operations. If the host also submits on `queue` it passes
 * `lock`/`unlock` (bracketing every ExecuteCommandLists here) and takes the same lock
 * around its own submits and presents. Refused when the runtime behind this library is
 * not Direct3D 12. 0 on success; `nr_frame_error()` otherwise. */
int nr_frame_adopt_d3d12(void *device, void *queue, void (*lock)(void *), void (*unlock)(void *),
                         void *lock_context);

/* Release the device libxmx holds — its own, or an adopted one back to its host,
 * untouched. Every `nr_frame` must have been closed first. The next `nr_frame_open`
 * opens the device again (adopting whatever `nr_frame_adopt_vulkan` handed over since).
 * A host must call this before destroying a device it shared. */
void nr_frame_shutdown(void);

/* 1 while an adopted (shared) device is open, 0 for libxmx's own, -1 when none is. */
int nr_frame_shared_device(void);

/* The compute runtime behind this library: "vulkan" (libxmx), "metal" (libmetalmx — the
 * Apple libdlssnr, or a shared build under NR_GPU_BACKEND=metal) or "d3d12" (libd3dmx — a
 * Windows libdlssnr built with NR_DLSSNR_D3D12, or NR_GPU_BACKEND=d3d12). Known without
 * opening a device, so a host can decide which device, if any, to share. */
const char *nr_frame_runtime(void);

/* The last failure, for the calling thread's most recent call. */
const char *nr_frame_error(void);
const char *nr_frame_device(nr_frame *frame);
const char *nr_frame_gemm_path(nr_frame *frame);

/* The network extent for an output extent: at least 320, a multiple of 64. */
void nr_frame_geometry(int height, int width, int *network_height, int *network_width);

/* colour (h, w, 3) -> output (h, w, 3). `history` is the previous output or NULL;
 * `previous` the previous *input* or NULL, for the floor. `head_out` (h, w, 4) optional.
 * 0 on success. */
int nr_frame_update(nr_frame *frame, const float *colour, int height, int width,
                    const float *history, const float *previous,
                    const nr_frame_params *params, float *output, float *head_out);

/* The same with a per-pixel control mask (h, w, 3): red scales the blend in the
 * composition, green the tone and blue the structure that reach the network. */
int nr_frame_update_masked(nr_frame *frame, const float *colour, int height, int width,
                           const float *history, const float *previous, const float *control_mask,
                           const nr_frame_params *params, float *output, float *head_out);

/* The halves, for a caller that wants to sit between them: features, the network, and
 * the composition of a cropped (h, w, 4) head — free to repeat at another intensity. */
int nr_frame_features_masked(nr_frame *frame, const float *colour, int height, int width,
                             const float *history, const float *control_mask,
                             const nr_frame_params *params, float *features);
int nr_frame_compose(nr_frame *frame, const float *head, const float *colour, int height, int width,
                     const float *history, const float *previous, const float *control_mask,
                     const nr_frame_params *params, float *output);

int nr_frame_features(nr_frame *frame, const float *colour, int height, int width,
                      const float *history, const nr_frame_params *params,
                      float *features /* (network_height, network_width, 16) */);
int nr_frame_run_features(nr_frame *frame, const float *features,
                          int network_height, int network_width,
                          float *head /* (network_height, network_width, 4) */);

/* Seconds spent, in the last run, writing the input, running the graph, reading the
 * head: 0, 1, 2. On a discrete card the outer two are the bus. */
double nr_frame_split(const nr_frame *frame, int which);

#ifdef __cplusplus
}
#endif
#endif
