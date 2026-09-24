/* CPU image passes. Keep the NumPy operation order and every FP16 rounding point.
 * Built locally for the host CPU; no fast-math or fused multiply-add is allowed.
 * Allocation, shape validation and buffer lifetime belong to nr_image.py.
 *
 * Taken from the parallel ProjectsCodex tree, which wrote it and measured it
 * (`notes/phase57`): feature assembly 146 -> 24 ms, composition 53 -> 9, the two
 * resizes 60 -> 8. Extended here for the two passes this tree has and that one does
 * not — history in the feature channels, and the temporal composition with its floor.
 *
 * Every function is a transcription of the NumPy above it, not a reimplementation.
 * The outputs are required to be byte-identical, and `test_native_image.py` checks it.
 *
 * Each pass's outer row loop is split across threads, one contiguous band of rows each
 * (upstream's OpenMP `parallel for schedule(static)`). No row reads another row's result,
 * so which thread computes a row changes nothing about its bytes; at a 1080p output it
 * changes the composition from 15.5 ms to 3.3 on an eight-core Lunar Lake.
 *
 * The threads are this file's own small pool rather than OpenMP: Apple's clang has no
 * OpenMP, MSVC's is 2.0 (no unsigned loop variables), and this object is linked into
 * libdlssnr, which a host such as VBA-M links into a sandboxed application bundle — an
 * OpenMP runtime would be one more shared library to carry and sign. The pool waits
 * passively on a condition variable, which is what upstream's OMP_WAIT_POLICY=passive
 * asks for: between frames the threads have a whole graph to wait through.
 * `NR_HOST_THREADS` (else `OMP_NUM_THREADS`) sets the count, 1 runs every pass on the
 * calling thread; the default is one per online core, at most 64.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nr_image.h"
#include "nr_portable.h"

/* -- the row pool --------------------------------------------------------- */

#if defined(_WIN32) && (!defined(_WIN32_WINNT) || _WIN32_WINNT >= 0x0600)
#define NR_ROWS_WIN32 1
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
typedef SRWLOCK nr_lock;
typedef CONDITION_VARIABLE nr_cond;
#define nr_lock_take(l) AcquireSRWLockExclusive(l)
#define nr_lock_give(l) ReleaseSRWLockExclusive(l)
#define nr_cond_wait(c, l) SleepConditionVariableSRW((c), (l), INFINITE, 0)
#define nr_cond_wake_all(c) WakeAllConditionVariable(c)
#define nr_cond_wake_one(c) WakeConditionVariable(c)
#elif !defined(_WIN32) && !(defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__))
#define NR_ROWS_PTHREAD 1
#include <pthread.h>
#include <unistd.h>
typedef pthread_mutex_t nr_lock;
typedef pthread_cond_t nr_cond;
#define nr_lock_take(l) pthread_mutex_lock(l)
#define nr_lock_give(l) pthread_mutex_unlock(l)
#define nr_cond_wait(c, l) pthread_cond_wait((c), (l))
#define nr_cond_wake_all(c) pthread_cond_broadcast(c)
#define nr_cond_wake_one(c) pthread_cond_signal(c)
#endif

#define NR_ROWS_MAX 64

#if defined(NR_ROWS_WIN32) || defined(NR_ROWS_PTHREAD)
static struct {
    nr_lock lock;
    nr_cond wake, done;
    unsigned threads;               /* the workers and the caller */
    unsigned long generation;       /* bumped once per job */
    unsigned pending;               /* workers still on the current job */
    int busy;                       /* a job is running: a second caller goes serial */
    nr_rows_fn fn;
    const void *args;
    size_t rows;
} pool;

/* Band `i` of `n`: the contiguous rows OpenMP's static schedule would hand thread `i`. */
static void band(size_t rows, unsigned i, unsigned n, size_t *y0, size_t *y1)
{
    *y0 = (size_t)((unsigned long long)rows * i / n);
    *y1 = (size_t)((unsigned long long)rows * (i + 1) / n);
}

static void worker_loop(unsigned index)
{
    unsigned long seen = 0;
    for (;;) {
        nr_lock_take(&pool.lock);
        while (pool.generation == seen) nr_cond_wait(&pool.wake, &pool.lock);
        seen = pool.generation;
        nr_rows_fn fn = pool.fn;
        const void *args = pool.args;
        size_t rows = pool.rows, y0, y1;
        unsigned n = pool.threads;
        nr_lock_give(&pool.lock);
        band(rows, index, n, &y0, &y1);
        if (y0 < y1) fn(args, y0, y1);
        nr_lock_take(&pool.lock);
        if (--pool.pending == 0) nr_cond_wake_one(&pool.done);
        nr_lock_give(&pool.lock);
    }
}

static unsigned wanted_threads(void)
{
    const char *names[] = { "NR_HOST_THREADS", "OMP_NUM_THREADS" };
    for (int i = 0; i < 2; i++) {
        const char *v = getenv(names[i]);
        int n = v ? atoi(v) : 0;
        if (n > 0) return n > NR_ROWS_MAX ? NR_ROWS_MAX : (unsigned)n;
    }
#ifdef NR_ROWS_WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    long cores = (long)info.dwNumberOfProcessors;
#else
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    if (cores < 1) cores = 1;
    return cores > NR_ROWS_MAX ? NR_ROWS_MAX : (unsigned)cores;
}

#ifdef NR_ROWS_WIN32
/* worker_loop never returns; once MSVC inlines it the return is C4702 under /WX. */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
static DWORD WINAPI worker_main(LPVOID arg) { worker_loop((unsigned)(uintptr_t)arg); return 0; }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
static INIT_ONCE pool_once = INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK pool_start(PINIT_ONCE once, PVOID param, PVOID *context)
{
    (void)once; (void)param; (void)context;
    InitializeSRWLock(&pool.lock);
    InitializeConditionVariable(&pool.wake);
    InitializeConditionVariable(&pool.done);
    unsigned want = wanted_threads(), have = 1;
    for (; have < want; have++) {
        HANDLE t = CreateThread(NULL, 0, worker_main, (LPVOID)(uintptr_t)have, 0, NULL);
        if (!t) break;
        CloseHandle(t);
    }
    pool.threads = have;
    return TRUE;
}
static void pool_init(void) { InitOnceExecuteOnce(&pool_once, pool_start, NULL, NULL); }
#else
static void *worker_main(void *arg) { worker_loop((unsigned)(uintptr_t)arg); return NULL; }
static pthread_once_t pool_once = PTHREAD_ONCE_INIT;
static void pool_start(void)
{
    pthread_mutex_init(&pool.lock, NULL);
    pthread_cond_init(&pool.wake, NULL);
    pthread_cond_init(&pool.done, NULL);
    unsigned want = wanted_threads(), have = 1;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    for (; have < want; have++) {
        pthread_t t;
        if (pthread_create(&t, &attr, worker_main, (void *)(uintptr_t)have) != 0) break;
    }
    pthread_attr_destroy(&attr);
    pool.threads = have;
}
static void pool_init(void) { pthread_once(&pool_once, pool_start); }
#endif

unsigned nr_host_threads(void)
{
    pool_init();
    return pool.threads;
}

void nr_parallel_rows(size_t rows, nr_rows_fn fn, const void *args)
{
    if (rows == 0) return;
    pool_init();
    unsigned n = pool.threads;
    /* A few rows are not worth a wake-up; and a job already running (a second host
     * thread, or a pass called from inside a band) takes the calling thread alone. */
    if (n <= 1 || rows < 2u * n) { fn(args, 0, rows); return; }
    nr_lock_take(&pool.lock);
    if (pool.busy) { nr_lock_give(&pool.lock); fn(args, 0, rows); return; }
    pool.busy = 1;
    pool.fn = fn; pool.args = args; pool.rows = rows;
    pool.pending = n - 1;
    pool.generation++;
    nr_cond_wake_all(&pool.wake);
    nr_lock_give(&pool.lock);
    size_t y0, y1;
    band(rows, 0, n, &y0, &y1);
    if (y0 < y1) fn(args, y0, y1);
    nr_lock_take(&pool.lock);
    while (pool.pending) nr_cond_wait(&pool.done, &pool.lock);
    pool.busy = 0;
    nr_lock_give(&pool.lock);
}
#else
unsigned nr_host_threads(void) { return 1; }
void nr_parallel_rows(size_t rows, nr_rows_fn fn, const void *args) { if (rows) fn(args, 0, rows); }
#endif

/* -- the passes ----------------------------------------------------------- */

static float half(float value) { return nr_half_round(value); }

static float unit(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

struct decode8_args { const uint8_t *source; int bgra; float *output; };

static void decode8_rows(const void *args, size_t p0, size_t p1)
{
    const struct decode8_args *a = args;
    const uint8_t *source = a->source;
    int bgra = a->bgra;
    float *output = a->output;
    for (size_t p = p0; p < p1; ++p) {
        output[p * 3] = (float)source[p * 4 + (bgra ? 2 : 0)] / 255.0f;
        output[p * 3 + 1] = (float)source[p * 4 + 1] / 255.0f;
        output[p * 3 + 2] = (float)source[p * 4 + (bgra ? 0 : 2)] / 255.0f;
    }
}

void nr_decode8(const uint8_t *source, size_t pixels, int bgra, float *output)
{
    struct decode8_args a = { source, bgra, output };
    nr_parallel_rows(pixels, decode8_rows, &a);
}

struct encode8_args {
    const float *image; ptrdiff_t sy, sx, sc; const uint8_t *raw; size_t width; int bgra;
    uint8_t *output;
};

static void encode8_rows(const void *args, size_t y0, size_t y1)
{
    const struct encode8_args *a = args;
    const float *image = a->image;
    ptrdiff_t sy = a->sy, sx = a->sx, sc = a->sc;
    const uint8_t *raw = a->raw;
    size_t width = a->width;
    int bgra = a->bgra;
    uint8_t *output = a->output;
    for (size_t y = y0; y < y1; ++y) {
        for (size_t x = 0; x < width; ++x) {
            const float *rgb = image + (ptrdiff_t)y * sy + (ptrdiff_t)x * sx;
            size_t p = y * width + x;
            for (size_t c = 0; c < 3; ++c) {
                float value = rgb[(ptrdiff_t)c * sc];
                /* NumPy's byte cast maps NaN to zero; do not cast NaN in C. */
                value = value == value ? unit(value) : 0.0f;
                output[p * 4 + (bgra ? 2 - c : c)] = (uint8_t)(value * 255.0f + 0.5f);
            }
            output[p * 4 + 3] = raw[p * 4 + 3];
        }
    }
}

void nr_encode8(const float *image, ptrdiff_t sy, ptrdiff_t sx, ptrdiff_t sc,
                 const uint8_t *raw, size_t height, size_t width, int bgra,
                 uint8_t *output)
{
    struct encode8_args a = { image, sy, sx, sc, raw, width, bgra, output };
    nr_parallel_rows(height, encode8_rows, &a);
}

struct compose_args {
    const float *head; ptrdiff_t hy, hx, hc; const float *colour; ptrdiff_t sy, sx, sc;
    size_t width; float blend; float *output;
};

static void compose_rows(const void *args, size_t y0, size_t y1)
{
    const struct compose_args *a = args;
    const float *head = a->head, *colour = a->colour;
    ptrdiff_t hy = a->hy, hx = a->hx, hc = a->hc, sy = a->sy, sx = a->sx, sc = a->sc;
    size_t width = a->width;
    float blend = a->blend;
    float *output = a->output;
    for (size_t y = y0; y < y1; ++y) {
        for (size_t x = 0; x < width; ++x) {
            const float *h = head + (ptrdiff_t)y * hy + (ptrdiff_t)x * hx;
            const float *rgb = colour + (ptrdiff_t)y * sy + (ptrdiff_t)x * sx;
            for (size_t c = 0; c < 3; ++c) {
                float source = rgb[(ptrdiff_t)c * sc];
                float predicted = unit(source + half(h[(ptrdiff_t)c * hc]) * 0.25f);
                output[(y * width + x) * 3 + c] = unit(source + blend * (predicted - source));
            }
        }
    }
}

void nr_compose(const float *head, ptrdiff_t hy, ptrdiff_t hx, ptrdiff_t hc,
                const float *colour, ptrdiff_t sy, ptrdiff_t sx, ptrdiff_t sc,
                size_t height, size_t width, float intensity, float *output)
{
    /* Our intensity > 1 extrapolates; the vendor recipe clamps negative intensity.
     * Keep both subtract/add operations even at intensity 1: simplifying to the
     * prediction would remove an FP32 rounding and can change encoded pixels.
     */
    float blend = intensity > 1.0f ? intensity : unit(intensity);
    struct compose_args a = { head, hy, hx, hc, colour, sy, sx, sc, width, blend, output };
    nr_parallel_rows(height, compose_rows, &a);
}

/* `history` is this tree's addition: the previous output, at the same logical extent as
 * the colour and mirrored onto the network extent the same way, standing in channels 7-9
 * where the first-frame layout repeats the colour. Identity reprojection, because a
 * layer at vkQueuePresentKHR has no motion vectors (notes/phase54). NULL for a still
 * frame, which is then bit-identical to the vendor's own first-frame layout.
 */
struct features_args {
    const float *colour; ptrdiff_t sy, sx, sc; const float *history; ptrdiff_t ty, tx, tc;
    const int32_t *rows, *columns; size_t width; const float *noise, *controls;
    float *output;
};

static void features_rows(const void *args, size_t y0, size_t y1)
{
    const struct features_args *a = args;
    const float *colour = a->colour, *history = a->history, *noise = a->noise;
    const float *controls = a->controls;
    ptrdiff_t sy = a->sy, sx = a->sx, sc = a->sc, ty = a->ty, tx = a->tx, tc = a->tc;
    const int32_t *rows = a->rows, *columns = a->columns;
    size_t width = a->width;
    float *output = a->output;
    for (size_t y = y0; y < y1; ++y) {
        const float *row = colour + rows[y] * sy;
        const float *old = history ? history + rows[y] * ty : 0;
        for (size_t x = 0; x < width; ++x) {
            size_t pixel = y * width + x;
            const float *rgb = row + columns[x] * sx;
            const float *was = old ? old + columns[x] * tx : 0;
            float *out = output + pixel * 16;
            for (size_t c = 0; c < 3; ++c) {
                float scaled = half(half(half(rgb[(ptrdiff_t)c * sc]) - 0.5f) * 0.125f);
                out[c] = noise[pixel * 3 + c];
                out[4 + c] = scaled;
                out[7 + c] = was
                    ? half(half(half(was[(ptrdiff_t)c * tc]) - 0.5f) * 0.125f)
                    : scaled;
            }
            out[3] = 1.0f;
            for (size_t c = 0; c < 5; ++c) out[10 + c] = controls[c];
            out[15] = 0.0f;
        }
    }
}

void nr_features(const float *colour, ptrdiff_t sy, ptrdiff_t sx, ptrdiff_t sc,
                 const float *history, ptrdiff_t ty, ptrdiff_t tx, ptrdiff_t tc,
                 const int32_t *rows, const int32_t *columns,
                 size_t height, size_t width, const float *noise,
                 const float *controls, float *output)
{
    struct features_args a = { colour, sy, sx, sc, history, ty, tx, tc, rows, columns,
                               width, noise, controls, output };
    nr_parallel_rows(height, features_rows, &a);
}

/* One axis at a time: the intermediate is deliberately rounded to FP32 before
 * the second axis. Coordinates/weights come from the unchanged NumPy formula.
 * Signed strides permit padded crops and reversed views without another copy.
 */
struct resize_args {
    const float *source; ptrdiff_t sy, sx, sc; size_t width, channels; int axis;
    const int32_t *low, *high; const float *weight; float *output;
};

static void resize_rows(const void *args, size_t y0, size_t y1)
{
    const struct resize_args *r = args;
    const float *source = r->source, *weight = r->weight;
    ptrdiff_t sy = r->sy, sx = r->sx, sc = r->sc;
    size_t width = r->width, channels = r->channels;
    int axis = r->axis;
    const int32_t *low = r->low, *high = r->high;
    float *output = r->output;
    for (size_t y = y0; y < y1; ++y) {
        if (axis == 0 && sx == (ptrdiff_t)channels && sc == 1) {
            const float *a = source + low[y] * sy;
            const float *b = source + high[y] * sy;
            float w = weight[y], other = 1.0f - w;
            for (size_t i = 0; i < width * channels; ++i)
                output[y * width * channels + i] = a[i] * other + b[i] * w;
        } else {
            for (size_t x = 0; x < width; ++x) {
                size_t index = axis == 0 ? y : x;
                const float *a = axis == 0 ? source + low[y] * sy + (ptrdiff_t)x * sx
                                          : source + (ptrdiff_t)y * sy + low[x] * sx;
                const float *b = axis == 0 ? source + high[y] * sy + (ptrdiff_t)x * sx
                                          : source + (ptrdiff_t)y * sy + high[x] * sx;
                float w = weight[index], other = 1.0f - w;
                for (size_t c = 0; c < channels; ++c)
                    output[(y * width + x) * channels + c] =
                        a[(ptrdiff_t)c * sc] * other + b[(ptrdiff_t)c * sc] * w;
            }
        }
    }
}

void nr_resize_axis(const float *source, ptrdiff_t sy, ptrdiff_t sx, ptrdiff_t sc,
                    size_t height, size_t width, size_t channels, int axis,
                    const int32_t *low, const int32_t *high,
                    const float *weight, float *output)
{
    struct resize_args r = { source, sy, sx, sc, width, channels, axis, low, high, weight,
                             output };
    nr_parallel_rows(height, resize_rows, &r);
}


/* The temporal composition, which this tree has and the other does not.
 *
 * The model's own history weight comes from `table`: NumPy's sigmoid evaluated once on
 * every half value, indexed here by the half logit's sixteen bits. `expf` and NumPy's
 * float32 exponential do not agree in the last bit, and the contract for all of this is
 * byte-identical output, not nearly — but the logit is rounded to half before the
 * sigmoid, so there are only 65536 inputs and NumPy can own every one of them.
 * `confidence` then scales it, as NumPy does. With no table, `gate` carries the weight
 * already computed, confidence and all.
 *
 * `previous` is the game's own frame from the present before, or NULL. Where it is
 * unchanged the history is right for that pixel by construction, so the gate gets a
 * floor: full at no change, gone by four levels of 255, never above `scale`
 * (notes/phase54). `mask` is the interface control mask's red channel, or NULL.
 */
struct temporal_args {
    const float *head; ptrdiff_t hy, hx, hc;
    const float *colour; ptrdiff_t sy, sx, sc;
    const float *history; ptrdiff_t ry, rx, rc;
    const float *previous; ptrdiff_t py, px, pc;
    const float *gate; ptrdiff_t gy, gx;
    const float *table; float confidence;
    const float *mask; ptrdiff_t my, mx;
    size_t width; float intensity, scale, hold, slope; float *output;
};

static void temporal_rows(const void *args, size_t y0, size_t y1)
{
    const struct temporal_args *a = args;
    const float *head = a->head, *colour = a->colour, *history = a->history;
    const float *previous = a->previous, *gate = a->gate, *table = a->table, *mask = a->mask;
    ptrdiff_t hy = a->hy, hx = a->hx, hc = a->hc, sy = a->sy, sx = a->sx, sc = a->sc;
    ptrdiff_t ry = a->ry, rx = a->rx, rc = a->rc, py = a->py, px = a->px, pc = a->pc;
    ptrdiff_t gy = a->gy, gx = a->gx, my = a->my, mx = a->mx;
    size_t width = a->width;
    float confidence = a->confidence, intensity = a->intensity, scale = a->scale;
    float hold = a->hold, slope = a->slope;
    float *output = a->output;
    for (size_t y = y0; y < y1; ++y) {
        for (size_t x = 0; x < width; ++x) {
            const float *h = head + (ptrdiff_t)y * hy + (ptrdiff_t)x * hx;
            const float *rgb = colour + (ptrdiff_t)y * sy + (ptrdiff_t)x * sx;
            const float *was = history + (ptrdiff_t)y * ry + (ptrdiff_t)x * rx;
            float alpha;
            if (table) {
                /* The model's gate looked up rather than recomputed: `nr_frame.gate_table`
                 * holds NumPy's own expression evaluated on every half value, so the exp
                 * that kept the gate in NumPy is inside the table, and the rounding to half
                 * here is the one `half` does. Then the confidence, as NumPy applies it. */
                alpha = table[nr_float_to_half(h[3 * hc])];
                if (confidence != 1.0f) alpha *= confidence;
            } else {
                alpha = gate[(ptrdiff_t)y * gy + (ptrdiff_t)x * gx];
            }
            if (previous) {
                const float *before = previous + (ptrdiff_t)y * py + (ptrdiff_t)x * px;
                float moved = 0.0f;
                for (size_t c = 0; c < 3; ++c) {
                    float step = rgb[(ptrdiff_t)c * sc] - before[(ptrdiff_t)c * pc];
                    if (step < 0.0f) step = -step;
                    if (step > moved) moved = step;
                }
                /* `moved * slope + hold`, clamped to [0, hold], and `slope` arrives
                 * already folded: NumPy multiplies by one constant and adds another,
                 * and `clip(1 - moved * 255 / ramp, 0, 1) * hold` is the same value by
                 * algebra and a different one in float32. */
                float floored = moved * slope + hold;
                if (floored < 0.0f) floored = 0.0f;
                if (floored > hold) floored = hold;
                if (floored > 1.0f) floored = 1.0f;     /* `compose` clips the floor to [0, 1] */
                floored *= scale;
                if (floored > alpha) alpha = floored;
            }
            /* No clamp on the blend: the NumPy this transcribes does not clamp it
             * either, and the vendor's clamp is the one thing our composition
             * deliberately drops so that intensity above 1 can extrapolate. */
            float blend = intensity;
            if (mask) blend *= mask[(ptrdiff_t)y * my + (ptrdiff_t)x * mx];
            for (size_t c = 0; c < 3; ++c) {
                float source = rgb[(ptrdiff_t)c * sc];
                float predicted = unit(source + half(h[(ptrdiff_t)c * hc]) * 0.25f);
                predicted += alpha * (was[(ptrdiff_t)c * rc] - predicted);
                output[(y * width + x) * 3 + c] = unit(source + blend * (predicted - source));
            }
        }
    }
}

void nr_compose_temporal(const float *head, ptrdiff_t hy, ptrdiff_t hx, ptrdiff_t hc,
                         const float *colour, ptrdiff_t sy, ptrdiff_t sx, ptrdiff_t sc,
                         const float *history, ptrdiff_t ry, ptrdiff_t rx, ptrdiff_t rc,
                         const float *previous, ptrdiff_t py, ptrdiff_t px, ptrdiff_t pc,
                         const float *gate, ptrdiff_t gy, ptrdiff_t gx,
                         const float *table, float confidence,
                         const float *mask, ptrdiff_t my, ptrdiff_t mx,
                         size_t height, size_t width, float intensity,
                         float scale, float hold, float slope, float *output)
{
    struct temporal_args a = { head, hy, hx, hc, colour, sy, sx, sc, history, ry, rx, rc,
                               previous, py, px, pc, gate, gy, gx, table, confidence,
                               mask, my, mx, width, intensity, scale, hold, slope, output };
    nr_parallel_rows(height, temporal_rows, &a);
}
