/*
 * nr_frame.c — `nr_frame.py` as a C library. See nr_frame.h for the contract.
 *
 * Three files of Python become one of C, and the order here follows them:
 *
 *   1. the weights: `nr_model.load_logical` (a safetensors reader) and the layout work
 *      `nr_resident.py` does on the way to the device — the attention-bias swizzle, the
 *      fused branched feed-forward, the folded global scale, the padded head;
 *   2. the graph: `nr_frame_resident.ResidentFrame._run` and every `record_*` in
 *      `nr_resident.py`, call for call, flag for flag, against libxmx's C entry points.
 *      Every dispatch the Python records, this records, in the same order with the same
 *      arguments, which is why the head comes out bit-identical;
 *   3. the frame: `nr_frame.build_features` and `nr_frame.compose`, on the passes
 *      `nr_image.c` already has.
 *
 * libxmx is reached through dlopen, from the directory this library lives in (or
 * `NR_XMX_DIR`), because it has no header and because the Python and the C must share one
 * copy of it when a test loads both into a process.
 *
 * Build: see the Makefile (`work/libnr_frame.so`). `-ffp-contract=off` is not optional:
 * the host maths here reproduces NumPy's float32 operation order, and a fused
 * multiply-add would move the last bit.
 */
#include "nr_frame.h"
#include "nr_image.h"
#include "nr_portable.h"
#include "nr_weights_embedded.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* errors                                                                      */
/* ------------------------------------------------------------------------- */

static char last_error[1536];

const char *nr_frame_error(void) { return (const char *)last_error; }

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
#define FAILF(...) do { sprintf_s(last_error, sizeof last_error, __VA_ARGS__); return -1; } while (0)
#define FAILP(...) do { sprintf_s(last_error, sizeof last_error, __VA_ARGS__); return NULL; } while (0)
#else
#define FAILF(...) do { snprintf(last_error, sizeof last_error, __VA_ARGS__); return -1; } while (0)
#define FAILP(...) do { snprintf(last_error, sizeof last_error, __VA_ARGS__); return NULL; } while (0)
#endif

/* ------------------------------------------------------------------------- */
/* libxmx, through dlopen                                                      */
/* ------------------------------------------------------------------------- */

struct xmx {
    nr_dl handle;

    char dir[1024];

    int (*open)(void);
    int (*init)(const char *);
    int (*res_init)(const char *, const char *, const char *, const char *, const char *, const char *);
    int (*portable)(void);
    int (*window_gather)(void);   /* optional: a runtime without it cannot gather */
    const char *(*error)(void);
    const char *(*device)(void);
    const char *(*path)(void);
    int (*buf_create_kind)(unsigned long long, int);
    int (*buf_host_visible)(int);
    void *(*buf_ptr)(int);
    int (*buf_upload)(int, const void *, unsigned long long, unsigned long long);
    int (*buf_download)(int, void *, unsigned long long, unsigned long long);
    int (*buf_zero)(int);
    int (*buf_destroy)(int);
    int (*begin)(void);
    int (*abort)(void);
    int (*sync)(int);
    int (*submit)(void);
    int (*rec_gemm)(int, int, int, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned,
                    unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);
    int (*rec_unary)(unsigned, int, int, int, int, unsigned, unsigned, float, unsigned, unsigned,
                     unsigned, unsigned, unsigned);
    int (*rec_row)(unsigned, int, int, int, int, unsigned, unsigned, unsigned, unsigned, unsigned, float);
    int (*rec_copy)(int, int, unsigned long long, unsigned long long, unsigned long long);
    /* the fused passes (xmx.h): every runtime exports them */
    int (*rec_gemm_residual)(int, int, int, int, int, unsigned, unsigned, unsigned, unsigned);
    int (*rec_gemm_window_residual)(int, int, int, int, int, unsigned, unsigned, unsigned, unsigned,
                                    unsigned, unsigned, unsigned, unsigned);
    int (*rec_gemm_qkv)(int, int, int, int, int, int, unsigned, unsigned, unsigned, unsigned);
    int (*rec_gemm_qkv_window)(int, int, int, int, int, int, unsigned, unsigned, unsigned, unsigned,
                               unsigned, unsigned, unsigned, unsigned, unsigned);
    int (*rec_gemm_dual)(int, int, int, int, unsigned, unsigned, unsigned);
    int (*rec_unary2)(unsigned, int, int, int, int, int, unsigned, unsigned, float, unsigned, unsigned,
                      unsigned, unsigned, unsigned);
    int (*rec_qkv)(int, int, int, int, int, unsigned, unsigned, unsigned);
    int (*window_init)(const char *, unsigned);
    int (*rec_window_attention)(int, int, int, int, int, unsigned, unsigned, unsigned);
    int (*ffn_init)(const char *);
    int (*rec_ffn)(int, int, int, int, int, int, unsigned, unsigned, unsigned, unsigned, unsigned);
    int (*graph_capture)(void);
    int (*graph_run)(int);
    int (*graph_destroy)(int);
    size_t (*embedded_shader)(const char *);   /* optional: a Makefile-built libxmx has none */
    /* optional too: sharing a host's Vulkan device, and closing (notes on nr_frame_adopt_vulkan) */
    int (*adopt)(void *, void *, void *, void *, unsigned, int, void *,
                 void (*)(void *), void (*)(void *), void *);
    void (*close)(void);
    int (*adopted)(void);
};

static struct xmx X;

static int own_directory(char *out, size_t cap)
{
    const char *forced = getenv("NR_XMX_DIR");

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    if (forced && *forced) { sprintf_s(out, cap, "%s", forced); return 0; }
#else
    if (forced && *forced) { snprintf(out, cap, "%s", forced); return 0; }
#endif

    return nr_dl_self_dir((const void *)&nr_frame_open, out, cap);
}

#define BIND(field, symbol) do { X.field = nr_dl_sym(X.handle, symbol); \
    if (!X.field) FAILF("libxmx has no %s", symbol); } while (0)

#ifdef NR_STATIC_XMX
#include "xmx.h"
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4152)
#endif

static int xmx_load(void)
{
    if (X.handle) return 0;
    if (own_directory(X.dir, sizeof X.dir)) FAILF("cannot locate this library's directory");
#ifdef NR_STATIC_XMX
    /* libdlssnr: the runtime's objects — libxmx's, libmetalmx's on Apple (NR_STATIC_METAL)
     * or libd3dmx's on Windows (NR_STATIC_D3D12) — are linked into this very library, so
     * there is nothing to load; the table points straight at them. */
    X.handle = (nr_dl)1;
    X.open = xmx_open; X.init = xmx_init; X.res_init = xmx_res_init;
    X.embedded_shader = xmx_embedded_shader;
    X.adopt = xmx_adopt; X.close = xmx_close; X.adopted = xmx_adopted;
    X.window_gather = xmx_window_gather;
    X.portable = xmx_portable; X.error = xmx_error; X.device = xmx_device; X.path = xmx_path;
    X.buf_create_kind = xmx_buf_create_kind; X.buf_host_visible = xmx_buf_host_visible;
    X.buf_ptr = xmx_buf_ptr; X.buf_upload = xmx_buf_upload; X.buf_download = xmx_buf_download;
    X.buf_zero = xmx_buf_zero; X.buf_destroy = xmx_buf_destroy;
    X.begin = xmx_begin; X.abort = xmx_abort; X.sync = xmx_sync; X.submit = xmx_submit;
    X.rec_gemm = xmx_rec_gemm; X.rec_unary = xmx_rec_unary; X.rec_row = xmx_rec_row;
    X.rec_copy = xmx_rec_copy;
    X.rec_gemm_residual = xmx_rec_gemm_residual; X.rec_gemm_window_residual = xmx_rec_gemm_window_residual;
    X.rec_gemm_qkv = xmx_rec_gemm_qkv; X.rec_gemm_qkv_window = xmx_rec_gemm_qkv_window;
    X.rec_gemm_dual = xmx_rec_gemm_dual; X.rec_unary2 = xmx_rec_unary2; X.rec_qkv = xmx_rec_qkv;
    X.window_init = xmx_window_init; X.rec_window_attention = xmx_rec_window_attention;
    X.ffn_init = xmx_ffn_init; X.rec_ffn = xmx_rec_ffn;
    X.graph_capture = xmx_graph_capture; X.graph_run = xmx_graph_run; X.graph_destroy = xmx_graph_destroy;
    return 0;
#else
    char path[1200];

#ifdef __APPLE__
    /* As xmx.py does, before MoltenVK is loaded: errors only, and no fast math — with it
     * on, every vendor rounding point in the graph moves (notes/phase67). */
    nr_setenv_default("MVK_CONFIG_LOG_LEVEL", "1");
    nr_setenv_default("MVK_CONFIG_FAST_MATH_ENABLED", "0");
#endif

    /* The compute runtime: libxmx (Vulkan) unless NR_GPU_BACKEND asks for libmetalmx (Metal
     * directly, built on Apple alone) or libd3dmx (Direct3D 12, built on Windows alone) —
     * the same entry points either way. */
    const char *runtime = "libxmx";
    const char *backend = getenv("NR_GPU_BACKEND");
    if (backend && !strcmp(backend, "metal")) {
#ifdef __APPLE__
        runtime = "libmetalmx";
#else
        FAILF("NR_GPU_BACKEND=metal: libmetalmx exists on macOS only");
#endif
    } else if (backend && !strcmp(backend, "d3d12")) {
#ifdef _WIN32
        runtime = "libd3dmx";
#else
        FAILF("NR_GPU_BACKEND=d3d12: libd3dmx exists on Windows only");
#endif
    } else if (backend && *backend && strcmp(backend, "vulkan")) {
        FAILF("NR_GPU_BACKEND must be 'vulkan', 'metal' or 'd3d12', not '%s'", backend);
    }

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(path, sizeof path, "%s/%s%s", X.dir, runtime, NR_SHARED_SUFFIX);
#else
    snprintf(path, sizeof path, "%s/%s%s", X.dir, runtime, NR_SHARED_SUFFIX);
#endif

    X.handle = nr_dl_open(path);
    if (!X.handle) FAILF("cannot load %s: %s", path, nr_dl_error());

    BIND(open, "xmx_open"); BIND(init, "xmx_init"); BIND(res_init, "xmx_res_init");
    X.embedded_shader = nr_dl_sym(X.handle, "xmx_embedded_shader");
    X.window_gather = nr_dl_sym(X.handle, "xmx_window_gather");
    X.adopt = nr_dl_sym(X.handle, "xmx_adopt");
    X.close = nr_dl_sym(X.handle, "xmx_close");
    X.adopted = nr_dl_sym(X.handle, "xmx_adopted");
    BIND(portable, "xmx_portable"); BIND(error, "xmx_error"); BIND(device, "xmx_device");
    BIND(path, "xmx_path");
    BIND(buf_create_kind, "xmx_buf_create_kind"); BIND(buf_host_visible, "xmx_buf_host_visible");
    BIND(buf_ptr, "xmx_buf_ptr"); BIND(buf_upload, "xmx_buf_upload");
    BIND(buf_download, "xmx_buf_download"); BIND(buf_zero, "xmx_buf_zero");
    BIND(buf_destroy, "xmx_buf_destroy");
    BIND(begin, "xmx_begin"); BIND(abort, "xmx_abort"); BIND(sync, "xmx_sync");
    BIND(submit, "xmx_submit");
    BIND(rec_gemm, "xmx_rec_gemm"); BIND(rec_unary, "xmx_rec_unary"); BIND(rec_row, "xmx_rec_row");
    BIND(rec_copy, "xmx_rec_copy");
    BIND(rec_gemm_residual, "xmx_rec_gemm_residual");
    BIND(rec_gemm_window_residual, "xmx_rec_gemm_window_residual");
    BIND(rec_gemm_qkv, "xmx_rec_gemm_qkv"); BIND(rec_gemm_qkv_window, "xmx_rec_gemm_qkv_window");
    BIND(rec_gemm_dual, "xmx_rec_gemm_dual"); BIND(rec_unary2, "xmx_rec_unary2");
    BIND(rec_qkv, "xmx_rec_qkv");
    BIND(window_init, "xmx_window_init"); BIND(rec_window_attention, "xmx_rec_window_attention");
    BIND(ffn_init, "xmx_ffn_init"); BIND(rec_ffn, "xmx_rec_ffn");
    BIND(graph_capture, "xmx_graph_capture"); BIND(graph_run, "xmx_graph_run");
    BIND(graph_destroy, "xmx_graph_destroy");
    return 0;
#endif
}

/* The shader `xmx.py` and `xmxres.py` choose, environment overrides included: the bare
 * name when libxmx carries the module compiled in (it loads that), else the file beside
 * the library. */
static const char *spv(char *buf, size_t cap, const char *env, const char *name)
{
    const char *forced = env ? getenv(env) : NULL;
    if (forced && *forced) return forced;
    if (X.embedded_shader && X.embedded_shader(name)) return name;

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(buf, cap, "%s/%s", X.dir, name);
#else
    snprintf(buf, cap, "%s/%s", X.dir, name);
#endif

    return buf;
}

/* Whether libxmx is open and its pipelines built; `nr_frame_shutdown` clears it. */
static int xmx_is_ready;

static int xmx_ready(void)
{
    if (xmx_is_ready) return 0;
    if (xmx_load()) return -1;
    if (X.open()) FAILF("xmx_open: %s", X.error());
    int portable = X.portable();
    char b[6][1200];
    if (X.init(spv(b[0], sizeof b[0], NULL, portable ? "gemm_portable_desc.spv" : "gemm_coopmat.spv")))
        FAILF("xmx_init: %s", X.error());
    if (X.res_init(spv(b[0], sizeof b[0], "XMX_GEMM_SPV", portable ? "gemm_portable.spv" : "gemm_resident.spv"),
                   spv(b[1], sizeof b[1], "XMX_UNARY_SPV", "resident.spv"),
                   spv(b[2], sizeof b[2], "XMX_ROW_SPV", "attention.spv"),
                   spv(b[3], sizeof b[3], "XMX_HISTORY_SPV", "history.spv"),
                   spv(b[4], sizeof b[4], "XMX_TILED_SPV", portable ? "gemm_portable_tiled.spv" : "gemm_tiled.spv"),
                   spv(b[5], sizeof b[5], "XMX_STAGED_SPV", portable ? "gemm_portable.spv" : "gemm_staged.spv")))
        FAILF("xmx_res_init: %s", X.error());
    /* the fused window attention (both output layouts) and the fused feed-forward, as
     * `xmxres.Runtime` names them: the `_portable` twins on a device without matrix units */
    const char *window = spv(b[0], sizeof b[0], "XMX_WINDOW_SPV",
                             portable ? "window_attention_portable.spv" : "window_attention.spv");
    if (X.window_init(window, 0) || X.window_init(window, 1))
        FAILF("xmx_window_init: %s", X.error());
    if (X.ffn_init(spv(b[1], sizeof b[1], "XMX_FFN_SPV", portable ? "ffn_fused_portable.spv" : "ffn_fused.spv")))
        FAILF("xmx_ffn_init: %s", X.error());
    xmx_is_ready = 1;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* half precision, exactly as NumPy's astype                                   */
/* ------------------------------------------------------------------------- */

static float half_to_float(uint16_t h) { return nr_half_to_float(h); }
static uint16_t float_to_half(float f) { return nr_float_to_half(f); }
static float half_round(float f) { return nr_half_round(f); }

/* ------------------------------------------------------------------------- */
/* the logical weights: a safetensors reader                                   */
/* ------------------------------------------------------------------------- */

struct tensor {
    char *name;
    int ndim;
    long shape[8];
    size_t count;
    float *data;            /* float32, as `load_logical` hands them over */
};

struct weights {
    struct tensor *t;
    size_t n;
};

/* A JSON scanner for exactly what a safetensors header holds: an object of objects with
 * string, integer-array and string-valued members. Anything else is rejected. */
struct js { const char *p, *end; };

static void js_ws(struct js *j) { while (j->p < j->end && strchr(" \t\r\n", *j->p)) j->p++; }
static int js_ch(struct js *j, char c) { js_ws(j); if (j->p < j->end && *j->p == c) { j->p++; return 1; } return 0; }

static int js_string(struct js *j, char *out, size_t cap)
{
    js_ws(j);
    if (j->p >= j->end || *j->p != '"') return -1;
    j->p++;
    size_t n = 0;
    while (j->p < j->end && *j->p != '"') {
        char c = *j->p++;
        if (c == '\\' && j->p < j->end) c = *j->p++;    /* names carry no escapes that matter */
        if (out && n + 1 < cap) out[n] = c;
        n++;
    }
    if (j->p >= j->end) return -1;
    j->p++;
    if (out) out[n < cap ? n : cap - 1] = 0;
    return 0;
}

static int js_number(struct js *j, long long *out)
{
    js_ws(j);
    char *stop;
    errno = 0;
    long long v = strtoll(j->p, &stop, 10);
    if (stop == j->p || errno) return -1;
    j->p = stop;
    *out = v;
    return 0;
}

/* Skip any value: used for metadata values we do not read. */
static int js_skip(struct js *j)
{
    js_ws(j);
    if (j->p >= j->end) return -1;
    if (*j->p == '"') return js_string(j, NULL, 0);
    if (*j->p == '{' || *j->p == '[') {
        char open = *j->p++, close = open == '{' ? '}' : ']';
        int depth = 1;
        while (j->p < j->end && depth) {
            if (*j->p == '"') { if (js_string(j, NULL, 0)) return -1; continue; }
            if (*j->p == open) depth++;
            else if (*j->p == close) depth--;
            j->p++;
        }
        return depth ? -1 : 0;
    }
    while (j->p < j->end && !strchr(",}]", *j->p)) j->p++;
    return 0;
}

static int tensor_compare(const void *a, const void *b)
{
    return strcmp(((const struct tensor *)a)->name, ((const struct tensor *)b)->name);
}

static void weights_free(struct weights *w)
{
    for (size_t i = 0; i < w->n; i++) { free(w->t[i].name); free(w->t[i].data); }
    free(w->t);
    w->t = NULL; w->n = 0;
}

/* Where the safetensors bytes come from: a file, or the slices CMake compiled into this
 * library (nr_weights_embedded.h). The reader below asks for ranges and never for the
 * whole, so the 292 MB is converted tensor by tensor either way. */
struct source {
    FILE *f;                                    /* a file, or NULL for the embedded slices */
    const struct nr_embedded_chunk *chunks;
    size_t chunk_count;
    const char *label;                          /* the path, or "embedded weights" */
};

static int source_read(struct source *s, size_t offset, void *dst, size_t n)
{
    if (s->f) {
        if (fseek(s->f, (long)offset, SEEK_SET)) return -1;
        return fread(dst, 1, n, s->f) == n ? 0 : -1;
    }
    unsigned char *out = dst;
    size_t k = 0, start = 0;
    while (k < s->chunk_count && start + s->chunks[k].size <= offset) start += s->chunks[k++].size;
    while (n) {
        if (k == s->chunk_count) return -1;
        size_t within = offset - start;
        size_t take = s->chunks[k].size - within;
        if (take > n) take = n;
        memcpy(out, s->chunks[k].data + within, take);
        out += take; offset += take; n -= take;
        start += s->chunks[k].size; k++;
    }
    return 0;
}

static void source_close(struct source *s) { if (s->f) fclose(s->f); s->f = NULL; }

static int source_open(struct source *s, const char *path)
{
    memset(s, 0, sizeof *s);
    if (!path) {
#ifdef NR_EMBEDDED_WEIGHTS
        s->chunks = nr_embedded_weights_chunks;
        s->chunk_count = nr_embedded_weights_chunk_count;
        s->label = "embedded weights";
        return 0;
#else
        FAILF("no weights path, and this build of libnr_frame has no embedded weights (NR_EMBED_WEIGHTS with a weights directory, or NR_BIN2C_WEIGHTS and the safetensors)");
#endif
    }
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    fopen_s(&s->f, path, "rb");
#else
    s->f = fopen(path, "rb");
#endif
    if (!s->f) FAILF("cannot open %s: %s", path, strerror(errno));
    s->label = path;
    return 0;
}

size_t nr_frame_embedded_weights_size(void)
{
#ifdef NR_EMBEDDED_WEIGHTS
    return nr_embedded_weights_size;
#else
    return 0;
#endif
}

static int weights_load(struct weights *w, const char *path_or_null)
{
    struct source src;
    if (source_open(&src, path_or_null)) return -1;
    struct source *f = &src;
    const char *path = src.label;

    uint8_t lenb[8];
    if (source_read(f, 0, lenb, 8)) { source_close(f); FAILF("%s: not a safetensors file", path); }
    uint64_t hlen = 0;
    for (int i = 7; i >= 0; i--) hlen = (hlen << 8) | lenb[i];
    if (hlen > (1u << 26)) { source_close(f); FAILF("%s: header of %llu bytes", path, (unsigned long long)hlen); }
    char *header = malloc(hlen + 1);
    if (!header || source_read(f, 8, header, hlen)) { free(header); source_close(f); FAILF("%s: short header", path); }
    header[hlen] = 0;
    size_t base = 8 + hlen;

    struct js j = { header, header + hlen };
    if (!js_ch(&j, '{')) { free(header); source_close(f); FAILF("%s: header is not an object", path); }
    size_t cap = 700;
    w->t = calloc(cap, sizeof *w->t); w->n = 0;
    int logical = 0, entries = 0;
    char name[256], key[64], sval[128];
    while (!js_ch(&j, '}')) {
        /* the metadata object counts as an entry too: a comma follows it */
        if (entries++ && !js_ch(&j, ',')) goto bad;
        if (js_string(&j, name, sizeof name) || !js_ch(&j, ':')) break;
        if (!strcmp(name, "__metadata__")) {
            if (!js_ch(&j, '{')) break;
            while (!js_ch(&j, '}')) {
                js_ch(&j, ',');
                if (js_string(&j, key, sizeof key) || !js_ch(&j, ':')) goto bad;
                js_ws(&j);
                if (*j.p == '"') {
                    if (js_string(&j, sval, sizeof sval)) goto bad;
                    if (!strcmp(key, "fully_logical") && !strcmp(sval, "true")) logical = 1;
                } else if (js_skip(&j)) goto bad;
            }
            continue;
        }
        if (w->n == cap) { cap *= 2; w->t = realloc(w->t, cap * sizeof *w->t); }
        struct tensor *t = &w->t[w->n];
        memset(t, 0, sizeof *t);
        t->name = nr_strdup(name);
        int is16 = -1;
        long long off0 = -1, off1 = -1;
        if (!js_ch(&j, '{')) goto bad;
        while (!js_ch(&j, '}')) {
            js_ch(&j, ',');
            if (js_string(&j, key, sizeof key) || !js_ch(&j, ':')) goto bad;
            if (!strcmp(key, "dtype")) {
                if (js_string(&j, sval, sizeof sval)) goto bad;
                is16 = !strcmp(sval, "F16") ? 1 : !strcmp(sval, "F32") ? 0 : -1;
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
                if (is16 < 0) { sprintf_s(last_error, sizeof last_error, "%s: unsupported dtype %s", name, sval); goto fail; }
#else
                if (is16 < 0) { snprintf(last_error, sizeof last_error, "%s: unsupported dtype %s", name, sval); goto fail; }
#endif
            } else if (!strcmp(key, "shape")) {
                if (!js_ch(&j, '[')) goto bad;
                t->ndim = 0;
                while (!js_ch(&j, ']')) {
                    js_ch(&j, ',');
                    long long d;
                    if (js_number(&j, &d) || t->ndim == 8) goto bad;
                    t->shape[t->ndim++] = (long)d;
                }
            } else if (!strcmp(key, "data_offsets")) {
                if (!js_ch(&j, '[') || js_number(&j, &off0) || !js_ch(&j, ',') || js_number(&j, &off1) || !js_ch(&j, ']')) goto bad;
            } else if (js_skip(&j)) goto bad;
        }
        if (is16 < 0 || off0 < 0 || off1 < off0) goto bad;
        t->count = 1;
        for (int d = 0; d < t->ndim; d++) t->count *= (size_t)t->shape[d];
        size_t bytes = (size_t)(off1 - off0);

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
        if (bytes != t->count * (is16 ? 2 : 4)) { sprintf_s(last_error, sizeof last_error, "%s: %zu bytes for %zu elements", name, bytes, t->count); goto fail; }
#else
        if (bytes != t->count * (is16 ? 2 : 4)) { snprintf(last_error, sizeof last_error, "%s: %zu bytes for %zu elements", name, bytes, t->count); goto fail; }
#endif

        t->data = malloc(t->count * sizeof(float) + 4);
        if (!t->data) goto bad;
        if (is16) {
            uint16_t *raw = malloc(bytes + 2);
            if (!raw || source_read(f, base + (size_t)off0, raw, bytes)) { free(raw); goto bad; }
            for (size_t i = 0; i < t->count; i++) t->data[i] = half_to_float(raw[i]);
            free(raw);
        } else if (source_read(f, base + (size_t)off0, t->data, bytes)) goto bad;
        w->n++;
    }
    free(header);
    source_close(f);
    if (!logical) { weights_free(w); FAILF("%s: weights must declare fully_logical=true (the packed file is not a substitute)", path); }
    qsort(w->t, w->n, sizeof *w->t, tensor_compare);
    return 0;

bad:
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(last_error, sizeof last_error, "%s: malformed safetensors header near byte %ld", path, (long)(j.p - header));
#else
    snprintf(last_error, sizeof last_error, "%s: malformed safetensors header near byte %ld", path, (long)(j.p - header));
#endif

fail:
    free(header);
    source_close(f);
    weights_free(w);
    return -1;
}

static const struct tensor *weight(const struct weights *w, const char *name)
{
    struct tensor key = { .name = (char *)name };
    return bsearch(&key, w->t, w->n, sizeof *w->t, tensor_compare);
}

static const struct tensor *weightf(const struct weights *w, const char *fmt, int index)
{
    char name[128];

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(name, sizeof name, fmt, index);
#else
    snprintf(name, sizeof name, fmt, index);
#endif

    return weight(w, name);
}

/* ------------------------------------------------------------------------- */
/* device buffers                                                              */
/* ------------------------------------------------------------------------- */

enum { GRAPH = 0, HOST_READ = 1, HOST_WRITE = 2 };      /* xmxres.GRAPH / HOST_READ / HOST_WRITE */

/* A buffer holding `bytes` of `data` (or zeros): `Runtime.buffer_from`. */
static int buffer_with(const void *data, size_t bytes, int kind)
{
    int id = X.buf_create_kind(bytes ? bytes : 4, kind);
    if (id < 0) FAILF("xmx_buf_create: %s", X.error());
    if (!data) return id;
    if (X.buf_host_visible(id)) {
        memcpy(X.buf_ptr(id), data, bytes);
    } else if (X.buf_upload(id, data, 0, bytes)) {
        X.buf_destroy(id);
        FAILF("xmx_buf_upload: %s", X.error());
    }
    return id;
}

static int buffer_f16(const float *data, size_t count)
{
    uint16_t *h = malloc(count * 2 + 2);
    if (!h) FAILF("out of memory");
    for (size_t i = 0; i < count; i++) h[i] = float_to_half(data[i]);
    int id = buffer_with(h, count * 2, GRAPH);
    free(h);
    return id;
}

static int buffer_f32(const float *data, size_t count)
{
    return buffer_with(data, count * 4, GRAPH);
}

/* Host write into a HOST_WRITE buffer, host read out of a HOST_READ one:
 * `xmxres.host_write` / `host_view` for the two buffers the host touches. */
static int host_write(int id, const void *data, size_t bytes)
{
    if (X.buf_host_visible(id)) { memcpy(X.buf_ptr(id), data, bytes); return 0; }
    if (X.buf_upload(id, data, 0, bytes)) FAILF("xmx_buf_upload: %s", X.error());
    return 0;
}

static const void *host_read(int id, void *scratch, size_t bytes)
{
    if (X.buf_host_visible(id)) return X.buf_ptr(id);

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    if (X.buf_download(id, scratch, 0, bytes)) { sprintf_s(last_error, sizeof last_error, "xmx_buf_download: %s", X.error()); return NULL; }
#else
    if (X.buf_download(id, scratch, 0, bytes)) { snprintf(last_error, sizeof last_error, "xmx_buf_download: %s", X.error()); return NULL; }
#endif

    return scratch;
}

/* ------------------------------------------------------------------------- */
/* the weights on the device, laid out as nr_resident.py lays them out         */
/* ------------------------------------------------------------------------- */

/* `nr_model.FRAGMENT_SWIZZLE_INDICES`: stored offset of every logical (query, key) entry
 * of a 64x64 window bias, undoing the fused kernel's mma fragment order. */
static int fragment_index(int entry)
{
    int query = entry / 64, key = entry % 64;
    int qy = query / 8, qx = query % 8, ky = key / 8, kx = key % 8;
#define BIT(v, p) (((v) >> (p)) & 1)
    return (BIT(qy, 2) << 11) | (BIT(qx, 2) << 10) | (BIT(ky, 2) << 9) | (BIT(kx, 2) << 8)
         | (BIT(qy, 0) << 7) | (BIT(qx, 1) << 6) | (BIT(qx, 0) << 5) | (BIT(ky, 0) << 4)
         | (BIT(kx, 1) << 3) | (BIT(ky, 1) << 2) | (BIT(qy, 1) << 1) | BIT(kx, 0);
#undef BIT
}

/* `nr_model.recovered_window_origin`: the vendor window origin (y, x) of a block. */
static void window_origin(int block, int *oy, int *ox)
{
    int phase;
    if (block == 0) phase = 0;
    else if (block <= 4) phase = block - 1;
    else if (block <= 8) phase = block - 5;
    else if (block <= 14) phase = block - 9;
    else if (block <= 22) phase = block - 15;
    else if (block <= 30) phase = block - 23;
    else if (block >= 40 && block <= 55) phase = block - (block < 48 ? 40 : 48);
    else if (block >= 56 && block <= 61) phase = block - 54;
    else if (block >= 62 && block <= 69) phase = block - (block < 66 ? 62 : 66);
    else if (block == 70) phase = 1;
    else { *oy = *ox = 0; return; }
    static const int ys[4] = { 0, -4, 0, -4 }, xs[4] = { 0, -4, -4, 0 };
    *oy = ys[phase % 4];
    *ox = xs[phase % 4];
}

enum family { WINDOW, SPLIT, GLOBAL };

struct block_w {
    int loaded;
    enum family family;
    int index, heads, channels, groups, hidden_width;
    int branched;
    int oy, ox;
    float logit_cap;
    /* device buffers; -1 where the family has none */
    int qkv, out, bias, scale, attn_cos, ffn_cos;
    int expand, branch, ffn_out;          /* window: feed-forward */
    int first, project, weight3;          /* split: the group MLP */
    int ffn_proj;                         /* global: the wide feed-forward's projection */
};

struct edge_w { int loaded; int weight0, sine, out_channels; };

#define NEED(t, w, fmt, i) const struct tensor *t = weightf(w, fmt, i); \
    if (!t) FAILF("missing weight " fmt, i)

/* `recover_attention_bias_layout` where `uses_fragment_swizzle` says so, then upload. */
static int upload_bias(const struct tensor *bias, int heads)
{
    if (bias->ndim != 3 || bias->shape[1] != 64 || bias->shape[2] != 64)
        FAILF("attention bias must be [heads, 64, 64]");
    if (!(heads == 1 || heads == 16)) return buffer_f32(bias->data, bias->count);
    float *fixed = malloc(bias->count * sizeof(float));
    if (!fixed) FAILF("out of memory");
    for (long h = 0; h < bias->shape[0]; h++)
        for (int e = 0; e < 4096; e++)
            fixed[h * 4096 + e] = bias->data[h * 4096 + fragment_index(e)];
    int id = buffer_f32(fixed, bias->count);
    free(fixed);
    return id;
}

static int load_window_block(struct block_w *b, const struct weights *w, int index, int heads)
{
    b->family = WINDOW; b->index = index; b->heads = heads;
    window_origin(index, &b->oy, &b->ox);
    NEED(proj, w, "block%d.layer0.projection_weight", index);
    b->channels = (int)proj->shape[0];
    NEED(bias, w, "block%d.layer0.attn_bias", index);
    NEED(qkv, w, "block%d.layer0.qkv_weight", index);
    NEED(scale, w, "block%d.layer0.attn_scale", index);
    NEED(acos, w, "block%d.layer0.attn_cos_skip", index);
    NEED(fcos, w, "block%d.layer0.ffn_cos_skip", index);
    if ((b->qkv = buffer_f16(qkv->data, qkv->count)) < 0) return -1;
    if ((b->out = buffer_f16(proj->data, proj->count)) < 0) return -1;
    if ((b->bias = upload_bias(bias, heads)) < 0) return -1;
    if ((b->scale = buffer_f32(scale->data, scale->count)) < 0) return -1;
    if ((b->attn_cos = buffer_f32(acos->data, acos->count)) < 0) return -1;
    if ((b->ffn_cos = buffer_f32(fcos->data, fcos->count)) < 0) return -1;
    const struct tensor *expand = weightf(w, "block%d.layer0.ffn_expand_weight", index);
    b->branched = expand != NULL;
    if (b->branched) {
        /* `_fused_branched_weights`: W[oh, br, ih, k, j] -> E[oh][ih*32+k][br*32+j] is one
         * (C, 128) expansion per output head; P[oh, br, k, j] -> (128, 32) contracts as
         * stored. The gate and the E4M3 publish between them are elementwise. */
        NEED(branch, w, "block%d.layer0.ffn_branch_projection_weight", index);
        NEED(ffn_out, w, "block%d.layer0.ffn_output_projection_weight", index);
        if (expand->ndim != 5 || expand->shape[1] != 4 || expand->shape[3] != 32 || expand->shape[4] != 32
            || expand->shape[0] != expand->shape[2])
            FAILF("block%d: unexpected ffn_expand_weight shape", index);
        int G = (int)expand->shape[0];
        b->groups = G;
        b->hidden_width = G * 128;
        if (b->channels != G * 32) FAILF("block%d: branched feed-forward does not match C", index);
        size_t n = (size_t)G * (G * 32) * 128;
        float *fused = malloc(n * sizeof(float));
        if (!fused) FAILF("out of memory");
        for (int oh = 0; oh < G; oh++)
            for (int br = 0; br < 4; br++)
                for (int ih = 0; ih < G; ih++)
                    for (int k = 0; k < 32; k++)
                        for (int j = 0; j < 32; j++)
                            fused[((size_t)oh * (G * 32) + ih * 32 + k) * 128 + br * 32 + j] =
                                expand->data[((((size_t)oh * 4 + br) * G + ih) * 32 + k) * 32 + j];
        b->expand = buffer_f16(fused, n);
        free(fused);
        if (b->expand < 0) return -1;
        if ((b->branch = buffer_f16(branch->data, branch->count)) < 0) return -1;
        if ((b->ffn_out = buffer_f16(ffn_out->data, ffn_out->count)) < 0) return -1;
    } else {
        NEED(w1, w, "block%d.layer0.weight1", index);
        NEED(w2, w, "block%d.layer0.weight2", index);
        b->groups = 0;
        b->hidden_width = (int)w1->shape[1];
        if ((b->expand = buffer_f16(w1->data, w1->count)) < 0) return -1;
        if ((b->branch = buffer_f16(w2->data, w2->count)) < 0) return -1;
        b->ffn_out = -1;
    }
    b->first = b->project = b->weight3 = b->ffn_proj = -1;
    b->loaded = 1;
    return 0;
}

static int load_split_block(struct block_w *b, const struct weights *w, int index)
{
    b->family = SPLIT; b->index = index; b->heads = 16;
    window_origin(index, &b->oy, &b->ox);
    NEED(proj, w, "block%d.layer3.projection_weight", index);
    b->channels = (int)proj->shape[0];
    b->groups = b->channels / 64;
    b->hidden_width = b->groups * 256;
    NEED(bias, w, "block%d.layer2.attn_bias", index);
    NEED(first, w, "block%d.layer0.first_projection_weight", index);
    NEED(expand, w, "block%d.layer0.group_expand_weight", index);
    NEED(project, w, "block%d.layer0.group_project_weight", index);
    NEED(w3, w, "block%d.layer1.weight3", index);
    NEED(fcos, w, "block%d.layer1.ffn_cos_skip", index);
    NEED(qkv, w, "block%d.layer2.qkv_weight", index);
    NEED(scale, w, "block%d.layer2.attn_scale", index);
    NEED(acos, w, "block%d.layer3.attn_cos_skip", index);
    if ((b->first = buffer_f16(first->data, first->count)) < 0) return -1;
    if ((b->expand = buffer_f16(expand->data, expand->count)) < 0) return -1;
    if ((b->project = buffer_f16(project->data, project->count)) < 0) return -1;
    if ((b->weight3 = buffer_f16(w3->data, w3->count)) < 0) return -1;
    if ((b->ffn_cos = buffer_f32(fcos->data, fcos->count)) < 0) return -1;
    if ((b->qkv = buffer_f16(qkv->data, qkv->count)) < 0) return -1;
    if ((b->scale = buffer_f32(scale->data, scale->count)) < 0) return -1;
    if ((b->bias = upload_bias(bias, 16)) < 0) return -1;
    if ((b->out = buffer_f16(proj->data, proj->count)) < 0) return -1;
    if ((b->attn_cos = buffer_f32(acos->data, acos->count)) < 0) return -1;
    b->branch = b->ffn_out = b->ffn_proj = -1;
    b->branched = 0;
    b->loaded = 1;
    return 0;
}

static int load_global_block(struct block_w *b, const struct weights *w, int index)
{
    b->family = GLOBAL; b->index = index; b->heads = 32; b->oy = b->ox = 0;
    NEED(proj, w, "block%d.layer4.projection_weight", index);
    b->channels = (int)proj->shape[0];
    NEED(expand, w, "block%d.layer0.weight", index);
    NEED(ffn_proj, w, "block%d.layer1.weight", index);
    NEED(fcos, w, "block%d.layer1.ffn_cos_skip", index);
    NEED(qkv, w, "block%d.layer2.qkv_weight", index);
    NEED(scale, w, "block%d.layer2.attn_scale", index);
    NEED(acos, w, "block%d.layer4.attn_cos_skip", index);
    b->hidden_width = (int)expand->shape[1];
    if ((b->expand = buffer_f16(expand->data, expand->count)) < 0) return -1;
    if ((b->ffn_proj = buffer_f16(ffn_proj->data, ffn_proj->count)) < 0) return -1;
    if ((b->ffn_cos = buffer_f32(fcos->data, fcos->count)) < 0) return -1;
    if ((b->qkv = buffer_f16(qkv->data, qkv->count)) < 0) return -1;
    /* the global kernels fold sqrt(head_dim) into the per-head scale, in float32 */
    float folded[64];
    float root = (float)sqrt((double)(b->channels / b->heads));
    if (scale->count > 64) FAILF("block%d: too many heads", index);
    for (size_t i = 0; i < scale->count; i++) folded[i] = scale->data[i] * root;
    if ((b->scale = buffer_f32(folded, scale->count)) < 0) return -1;
    if ((b->out = buffer_f16(proj->data, proj->count)) < 0) return -1;
    if ((b->attn_cos = buffer_f32(acos->data, acos->count)) < 0) return -1;
    b->logit_cap = 3.0f;                  /* nr_model.GLOBAL_ATTENTION_LOGIT_CAP */
    b->bias = b->branch = b->ffn_out = b->first = b->project = b->weight3 = -1;
    b->groups = 0; b->branched = 0;
    b->loaded = 1;
    return 0;
}

static int load_edge(struct edge_w *e, const struct weights *w, int index, int up)
{
    NEED(w0, w, "block%d.layer0.weight0", index);
    if ((e->weight0 = buffer_f16(w0->data, w0->count)) < 0) return -1;
    e->out_channels = (int)w0->shape[1];
    e->sine = -1;
    if (up) {
        NEED(sine, w, "block%d.layer0.sin", index);
        if ((e->sine = buffer_f32(sine->data, sine->count)) < 0) return -1;
    }
    e->loaded = 1;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* the frame: weights, the scratch arena, and the graph for one extent          */
/* ------------------------------------------------------------------------- */

/* `xmxres.ScratchArena.ALIASES`: the roles a block's buffers alias, because their live
 * intervals are disjoint and the barriers between passes already order them. */
enum role {
    R_PROJECTION,      /* hidden16, proj, attended, attention, transition.padded, transition.projected */
    R_BRANCH_VALUE,    /* branch, v16 */
    R_INPUT_KEY,       /* value16, win16, ffn16, k16, transition.pooled16, transition.projected16 */
    R_QUERY_PROB,      /* heads16, core16, q16, probs16, merged16 */
    R_SCORES_CONTEXT,  /* scores, context, transition.upsampled */
    R_RESIDUAL,        /* ffn, transition.scaled */
    R_VALUE,           /* a block scratch's own `value`; never used by the frame */
    R_GLOBAL_VALUE,    /* the bottleneck's input, zero in its padding rows */
    R_GLOBAL_OUT,
    R_OUT,
    R_COUNT
};

#define MAX_NAMED 48

struct named { char name[24]; int id; size_t bytes; };

struct nr_frame {
    struct weights w;
    /* device weights, shared across extents */
    int adapter, merge_sin, merge_cos, merge_sincos, head;
    struct edge_w bottleneck, decoder_input, edges[71];
    struct block_w blocks[71];
    /* the graph for one extent */
    int height, width;                     /* the network extent */
    int levels[7][3];
    int planning;                          /* 1: size the arena, record nothing */
    size_t role_bytes[R_COUNT];
    int role_id[R_COUNT];
    struct named named[MAX_NAMED];
    int named_count;
    int graph;                             /* captured commands, or -1 */
    /* `xmxres.Runtime.__init__`: the environment switches, read once, the same defaults.
     * Each one changes which passes are recorded, and the Python and this file must agree
     * on every one of them for the head to be bit-identical. */
    struct {
        int fuse_qk, batch_ffn, joint_qkv, fuse_residual, fuse_window_residual,
            fuse_window_attention, fuse_attention_merge, qkv_epilogue, fuse_glue, fuse_ffn,
            fuse_branched_ffn, fuse_partition, input_fp16, compact_head;
    } opt;
    int head_stride;                       /* floats per pixel in what run_graph returns */
    double split[3];
    /* host scratch */
    float *features_host;                  /* (H, W, 16) at the network extent */
    float *head_host;                      /* (H, W, 16) when the head buffer is unmapped */
    float *noise;                          /* (H, W, 3) for the extent and frame index */
    int noise_index;
    int32_t *rows, *cols;
    float *composed, *scratch_a, *scratch_b;
    size_t composed_pixels;
};

static int pad8(int e) { return (e + 7) / 8 * 8; }
static int align16(int e) { return (e + 15) / 16 * 16; }
static int align64(int e) { return (e + 63) / 64 * 64; }

/* -- the arena ---------------------------------------------------------- */

static void arena_reset(struct nr_frame *f)
{
    for (int r = 0; r < R_COUNT; r++) {
        if (f->role_id[r] >= 0) X.buf_destroy(f->role_id[r]);
        f->role_id[r] = -1;
        f->role_bytes[r] = 0;
    }
}

/* `ScratchArena.buffer`: in the plan, the role grows to the largest request; afterwards
 * the first use allocates it. Returns the buffer id, or -1 while planning. */
static int arena(struct nr_frame *f, enum role r, size_t bytes)
{
    if (f->planning) {
        if (bytes > f->role_bytes[r]) f->role_bytes[r] = bytes;
        return -1;
    }
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    if (bytes > f->role_bytes[r]) { sprintf_s(last_error, sizeof last_error, "scratch role %d exceeds its plan", r); return -2; }
#else
    if (bytes > f->role_bytes[r]) { snprintf(last_error, sizeof last_error, "scratch role %d exceeds its plan", r); return -2; }
#endif

    if (f->role_id[r] < 0) {
        int id = buffer_with(NULL, f->role_bytes[r], GRAPH);
        if (id < 0) return -2;

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
        if (r == R_GLOBAL_VALUE && X.buf_zero(id)) { sprintf_s(last_error, sizeof last_error, "xmx_buf_zero: %s", X.error()); return -2; }
#else
        if (r == R_GLOBAL_VALUE && X.buf_zero(id)) { snprintf(last_error, sizeof last_error, "xmx_buf_zero: %s", X.error()); return -2; }
#endif

        f->role_id[r] = id;
    }
    return f->role_id[r];
}

/* `ResidentFrame.buffer`: a frame buffer by name, grown when a larger request arrives.
 * Two of them are the host's: the features go in, the head comes back. */
static int named_buffer(struct nr_frame *f, const char *name, size_t bytes)
{
    int kind = (!strcmp(name, "features") || !strcmp(name, "features_host16")) ? HOST_WRITE
             : (!strcmp(name, "head") || !strcmp(name, "head4")) ? HOST_READ : GRAPH;
    for (int i = 0; i < f->named_count; i++) {
        struct named *n = &f->named[i];
        if (strcmp(n->name, name)) continue;
        if (n->bytes >= bytes) return n->id;

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
        if (!f->planning) { sprintf_s(last_error, sizeof last_error, "buffer %s grew after the plan", name); return -2; }
#else
        if (!f->planning) { snprintf(last_error, sizeof last_error, "buffer %s grew after the plan", name); return -2; }
#endif

        X.buf_destroy(n->id);
        n->id = buffer_with(NULL, bytes, kind);
        n->bytes = bytes;
        return n->id < 0 ? -2 : n->id;
    }
    
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    if (f->named_count == MAX_NAMED) { sprintf_s(last_error, sizeof last_error, "too many frame buffers"); return -2; }
#else
    if (f->named_count == MAX_NAMED) { snprintf(last_error, sizeof last_error, "too many frame buffers"); return -2; }
#endif

    struct named *n = &f->named[f->named_count];
    
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(n->name, sizeof n->name, "%s", name);
#else
    snprintf(n->name, sizeof n->name, "%s", name);
#endif

    n->id = buffer_with(NULL, bytes, kind);
    n->bytes = bytes;
    if (n->id < 0) return -2;
    f->named_count++;
    return n->id;
}

static int named_bufferf(struct nr_frame *f, const char *fmt, int i, size_t bytes)
{
    char name[24];
    
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(name, sizeof name, fmt, i);
#else
    snprintf(name, sizeof name, fmt, i);
#endif

    return named_buffer(f, name, bytes);
}

/* -- recording primitives: xmxres.Runtime, flag for flag ------------------ */

enum { E4M3 = 0, GATE, HALF, TO_HALF, SCALE, RESIDUAL, FROM_HALF, PARTITION, REVERSE, ADD_BIAS,
       SPLIT_HEADS, MERGE_HEADS, POOL2, UPSAMPLE2, SCALE_CHANNEL, ADD, PAD_END,
       GATE_E4M3_HALF, E4M3_HALF, GATE_HALF, UPSAMPLE_MERGE };
enum { COSINE_PUBLISH = 0, SOFTMAX = 1 };
enum { EPI_NONE = 0, EPI_E4M3 = 1, EPI_GATE = 2, EPI_GATE_E4M3 = 3, EPI_HALF = 4 };

static unsigned publish(int epilogue, int narrow) { return ((unsigned)epilogue << 8) | (narrow ? 0x1000u : 0u); }
static unsigned reads(int a_half, int b_half) { return (a_half ? 0x8000u : 0u) | (b_half ? 0x10000u : 0u); }

/* Every recording call goes through here: nothing is recorded while planning, and a
 * negative buffer id (a role not yet allocated) can only happen while planning. */
#define REC(f, call) do { if (!(f)->planning) { if (call) FAILF("%s: %s", #call, X.error()); } } while (0)

struct dims { unsigned batch, height, width, across; };

static int unary(struct nr_frame *f, unsigned kind, int source, int second, int target, int third,
                 size_t count, unsigned channels, float scale, int epilogue, int narrow,
                 int a_half, int b_half, struct dims d, unsigned pad)
{
    if (second < 0) second = source;
    if (third < 0) third = source;
    REC(f, X.rec_unary(kind | publish(epilogue, narrow) | reads(a_half, b_half), source, second, target,
                       third, (unsigned)count, channels, scale, d.batch, d.height, d.width, d.across, pad));
    return 0;
}

static const struct dims NODIMS = { 0, 0, 0, 0 };

static int to_half(struct nr_frame *f, int s, int t, size_t n)   { return unary(f, TO_HALF, s, -1, t, -1, n, 0, 1.0f, 0, 0, 0, 0, NODIMS, 0); }
static int from_half(struct nr_frame *f, int s, int t, size_t n) { return unary(f, FROM_HALF, s, -1, t, -1, n, 0, 1.0f, 0, 0, 0, 0, NODIMS, 0); }
static int e4m3(struct nr_frame *f, int s, int t, size_t n)      { return unary(f, E4M3, s, -1, t, -1, n, 0, 1.0f, 0, 0, 0, 0, NODIMS, 0); }
static int e4m3_half(struct nr_frame *f, int s, int t, size_t n) { return unary(f, E4M3_HALF, s, -1, t, -1, n, 0, 1.0f, 0, 0, 0, 0, NODIMS, 0); }

/* `Runtime.window_extent`: the padded extent and pads of a shifted-window partition. */
static void window_extent(int height, int width, int oy, int ox, int size, int *ph, int *pw, int *top, int *left)
{
    int pt = -oy, pl = -ox;
    *ph = pt + height + ((size - (height + pt) % size) % size);
    *pw = pl + width + ((size - (width + pl) % size) % size);
    *top = pt; *left = pl;
}

static int residual(struct nr_frame *f, int branch, int skip, int cosine, int target, size_t count,
                    unsigned channels, int epilogue, int narrow, int a_half, int b_half,
                    int reverse, int height, int width, int oy, int ox)
{
    if (reverse) {
        int ph, pw, top, left;
        window_extent(height, width, oy, ox, 8, &ph, &pw, &top, &left);
        struct dims d = { 8, (unsigned)height, (unsigned)width, (unsigned)(pw / 8) };
        return unary(f, RESIDUAL | 0x4000u, branch, skip, target, cosine, count, channels, 1.0f,
                     epilogue, narrow, a_half, b_half, d, ((unsigned)top << 16) | (unsigned)left);
    }
    return unary(f, RESIDUAL, branch, skip, target, cosine, count, channels, 1.0f, epilogue, narrow,
                 a_half, b_half, NODIMS, 0);
}

static int partition(struct nr_frame *f, int source, int target, int height, int width, unsigned channels,
                     int oy, int ox, int narrow, int a_half)
{
    int ph, pw, top, left;
    window_extent(height, width, oy, ox, 8, &ph, &pw, &top, &left);
    struct dims d = { 8, (unsigned)height, (unsigned)width, (unsigned)(pw / 8) };
    return unary(f, PARTITION, source, -1, target, -1, (size_t)ph * pw * channels, channels, 1.0f,
                 0, narrow, a_half, 0, d, ((unsigned)top << 16) | (unsigned)left);
}

static int split_heads(struct nr_frame *f, int source, int target, unsigned windows, unsigned tokens,
                       unsigned channels, unsigned heads, unsigned part, int epilogue, int narrow)
{
    struct dims d = { heads, tokens, part, 0 };
    return unary(f, SPLIT_HEADS, source, -1, target, -1, (size_t)windows * tokens * channels, channels,
                 1.0f, epilogue, narrow, 0, 0, d, 0);
}

static int merge_heads(struct nr_frame *f, int source, int target, unsigned windows, unsigned tokens,
                       unsigned channels, unsigned heads, int epilogue, int narrow)
{
    struct dims d = { heads, tokens, 0, 0 };
    return unary(f, MERGE_HEADS, source, -1, target, -1, (size_t)windows * tokens * channels, channels,
                 1.0f, epilogue, narrow, 0, 0, d, 0);
}

static int pool2(struct nr_frame *f, int source, int target, int height, int width, unsigned channels,
                 int epilogue, int narrow, int a_half)
{
    struct dims d = { 0, (unsigned)height, (unsigned)width, 0 };
    return unary(f, POOL2, source, -1, target, -1, (size_t)(height / 2) * (width / 2) * channels, channels,
                 1.0f, epilogue, narrow, a_half, 0, d, 0);
}

static int upsample2(struct nr_frame *f, int source, int target, int source_width, int height, int width,
                     unsigned channels, int a_half)
{
    struct dims d = { 0, (unsigned)width, (unsigned)source_width, 0 };
    return unary(f, UPSAMPLE2, source, -1, target, -1, (size_t)height * width * channels, channels, 1.0f,
                 0, 0, a_half, 0, d, 0);
}

static int pad_end(struct nr_frame *f, int source, int target, int height, int width, int ph, int pw,
                   unsigned channels, int a_half, int narrow)
{
    struct dims d = { 0, (unsigned)height, (unsigned)width, (unsigned)pw };
    return unary(f, PAD_END, source, -1, target, -1, (size_t)ph * pw * channels, channels, 1.0f,
                 0, narrow, a_half, 0, d, 0);
}

static int scale_channel(struct nr_frame *f, int source, int factors, int target, size_t count,
                         unsigned channels, int a_half)
{
    return unary(f, SCALE_CHANNEL, source, -1, target, factors, count, channels, 1.0f, 0, 0, a_half, 0, NODIMS, 0);
}

static int add(struct nr_frame *f, int left, int right, int target, size_t count, int epilogue, int narrow)
{
    return unary(f, ADD, left, right, target, -1, count, 0, 1.0f, epilogue, narrow, 0, 0, NODIMS, 0);
}

static int cosine_publish(struct nr_frame *f, int source, int target, size_t rows, unsigned tokens,
                          unsigned heads, int scale, int narrow, int from_half_in, int qkv_part)
{
    unsigned flags = COSINE_PUBLISH | publish(0, narrow) | (from_half_in ? 0x8000u : 0u);
    if (qkv_part >= 0) flags |= 0x20000u | ((unsigned)qkv_part << 18);
    REC(f, X.rec_row(flags, source, source, target, scale >= 0 ? scale : source, (unsigned)rows, tokens,
                     heads, scale >= 0 ? 1u : 0u, 0, 0.0f));
    return 0;
}

static int softmax(struct nr_frame *f, int source, int target, size_t rows, unsigned width, unsigned stride,
                   float cap, int narrow, int bias, unsigned heads)
{
    unsigned flags = SOFTMAX | publish(0, narrow) | (bias >= 0 ? 0x2000u : 0u);
    REC(f, X.rec_row(flags, source, bias >= 0 ? bias : source, target, source, (unsigned)rows, width, heads,
                     0, stride, cap));
    return 0;
}

struct gemm_opt {
    unsigned batch;                 /* 0 -> 1 */
    unsigned long long sa, sb, sc;  /* per-batch strides, 0 -> dense */
    int transpose_b;
    unsigned lda, ldb, ldc;
    unsigned oa, ob, oc;
    int epilogue, narrow;
    unsigned extra;                 /* further flag bits: 0x10000 the compact head */
};

static int gemm(struct nr_frame *f, int a, int b, int c, unsigned rows, unsigned cols, unsigned inner,
                const struct gemm_opt *o)
{
    static const struct gemm_opt plain = { 0 };
    if (!o) o = &plain;
    if (rows % 8 || cols % 16 || inner % 16) FAILF("gemm %ux%ux%u is not tile-aligned", rows, cols, inner);
    unsigned batch = o->batch ? o->batch : 1;
    unsigned long long sa = o->sa, sb = o->sb, sc = o->sc;
    if (!sa && !sb && !sc) {
        sa = (unsigned long long)rows * inner;
        sb = o->transpose_b ? (unsigned long long)cols * inner : (unsigned long long)inner * cols;
        sc = (unsigned long long)rows * cols;
    }
    unsigned flags = (o->transpose_b ? 1u : 0u) | publish(o->epilogue, o->narrow) | o->extra;
    REC(f, X.rec_gemm(a, b, c, rows, cols, inner, batch, (unsigned)sa, (unsigned)sb, (unsigned)sc, flags,
                      o->lda, o->ldb, o->ldc, o->oa, o->ob, o->oc));
    return 0;
}

static int copy(struct nr_frame *f, int source, int target, size_t bytes)
{
    REC(f, X.rec_copy(source, target, bytes, 0, 0));
    return 0;
}

/* `Runtime.independent()`: no barrier between the dispatches inside. */
static int independent(struct nr_frame *f, int on)
{
    REC(f, X.sync(on ? 0 : 1));
    return 0;
}

/* -- the fused passes: xmxres.Runtime, flag for flag (notes/improve-fusions.md) --------- */

/* `Runtime.gemm_residual`: target = a @ b + skip * cosine, the residual in the GEMM's own
 * epilogue; with `reverse` the rows are a window block's, padded and in window order, and
 * the output lands straight back in the unpadded image. */
static int gemm_residual(struct nr_frame *f, int a, int b, int skip, int cosine, int target,
                         unsigned rows, unsigned cols, unsigned inner, int epilogue, int narrow,
                         int skip_half, int reverse, int height, int width, int oy, int ox)
{
    unsigned flags = publish(epilogue, narrow) | (skip_half ? 0x40000u : 0u);
    if (reverse) {
        int ph, pw, top, left;
        window_extent(height, width, oy, ox, 8, &ph, &pw, &top, &left);
        REC(f, X.rec_gemm_window_residual(a, b, target, skip, cosine, rows, cols, inner, flags,
                                          (unsigned)height, (unsigned)width, (unsigned)(pw / 8),
                                          ((unsigned)top << 16) | (unsigned)left));
        return 0;
    }
    REC(f, X.rec_gemm_residual(a, b, target, skip, cosine, rows, cols, inner, flags));
    return 0;
}

/* `Runtime.gemm_qkv`: the QKV projection finished in its own epilogue — Q and K
 * normalised, V published, (window, head, token, 32) halves out. With `window` the
 * operand is the image itself and the partition happens in the projection's loads. */
static int gemm_qkv(struct nr_frame *f, int a, int weight, int q, int k, int v, int scale,
                    unsigned rows, unsigned channels, unsigned heads, unsigned tokens,
                    int window, int height, int width, int oy, int ox, int image_half)
{
    if (window) {
        int ph, pw, top, left;
        window_extent(height, width, oy, ox, 8, &ph, &pw, &top, &left);
        REC(f, X.rec_gemm_qkv_window(a, weight, q, k, v, scale, rows, channels, heads, tokens,
                                     (unsigned)width, (unsigned)height, (unsigned)(pw / 8),
                                     ((unsigned)top << 16) | (unsigned)left, image_half ? 1u : 0u));
        return 0;
    }
    REC(f, X.rec_gemm_qkv(a, weight, q, k, v, scale, rows, channels, heads, tokens));
    return 0;
}

/* `Runtime.gemm_dual`: c = a @ b in float32, and the same values as half into `half_copy`. */
static int gemm_dual(struct nr_frame *f, int a, int b, int c, int half_copy, unsigned rows,
                     unsigned cols, unsigned inner)
{
    REC(f, X.rec_gemm_dual(a, b, c, half_copy, rows, cols, inner));
    return 0;
}

/* `Runtime.ffn_fused`: a feed-forward in one pass. One group of 32 channels with a skip is
 * the narrow blocks' whole feed-forward; `groups` without a skip is the branched blocks'
 * per-group expand and projection into `target`'s column slices. */
static int ffn_fused(struct nr_frame *f, int a, int expand, int projection, int target,
                     unsigned rows, unsigned channels, unsigned hidden, unsigned groups,
                     int skip, int cosine, int epilogue, int narrow, int skip_half)
{
    unsigned flags = publish(epilogue, narrow) | (skip_half ? 0x40000u : 0u);
    REC(f, X.rec_ffn(a, expand, projection, target, skip, cosine, rows, channels, hidden, groups, flags));
    return 0;
}

/* `Runtime.upsample_merge`: merged = upsample2(source) * sin + skip * cos, stored float32
 * and as half; `sincos` is the per-channel sin then cos. */
static int upsample_merge(struct nr_frame *f, int source, int skip, int sincos, int merged, int merged16,
                          int height, int width, int source_width, unsigned channels)
{
    REC(f, X.rec_unary2(UPSAMPLE_MERGE | reads(1, 1), source, skip, merged, sincos, merged16,
                        (unsigned)((size_t)height * width * channels), channels, 1.0f, 0,
                        (unsigned)width, (unsigned)source_width, 0, 0));
    return 0;
}

/* `Runtime.window_attention`: QK^T, softmax and PV for full 8x8 windows in one dispatch;
 * `merged` also does the head merge and publishes into `target` as half. */
static int window_attention(struct nr_frame *f, int q, int k, int v, int target, unsigned batches,
                            unsigned heads, int bias, int merged)
{
    REC(f, X.rec_window_attention(q, k, v, bias, target, batches, heads, merged ? 1u : 0u));
    return 0;
}

/* `Runtime.prepare_qkv`: Q, K and V from the float32 projection in one row dispatch. */
static int prepare_qkv(struct nr_frame *f, int source, int q, int k, int v, int scale,
                       unsigned windows, unsigned tokens, unsigned heads)
{
    REC(f, X.rec_qkv(source, q, k, v, scale, windows * tokens * heads, tokens, heads));
    return 0;
}

/* ------------------------------------------------------------------------- */
/* blocks: nr_resident.py, transcribed                                          */
/* ------------------------------------------------------------------------- */

/* `BlockScratch`: the working buffers of one window or split block at one extent. With
 * the arena every field is a role, so the struct is the geometry plus role handles. */
struct scratch {
    int height, width, tokens, windows, batch, hidden_width;
    size_t pixels, windowed;
    int value, value16, hidden16, heads16, branch, ffn, win16, proj, q16, k16, v16,
        scores, probs16, context, merged16, attended, out, core16;
    /* key16: K when the projection's own epilogue writes it — k16's role is the projection's
     * input, still being read. context16: the fused attention's merged output, in the
     * scores' and context's role, which the fused path leaves unused. */
    int key16, context16;
};

#define ROLE(field, role, bytes) do { s->field = arena(f, role, bytes); if (s->field == -2) return -1; } while (0)

static int block_scratch(struct nr_frame *f, const struct block_w *w, int height, int width, struct scratch *s)
{
    int ph, pw, top, left;
    window_extent(height, width, -4, -4, 8, &ph, &pw, &top, &left);
    memset(s, 0, sizeof *s);
    s->height = height; s->width = width; s->tokens = 64;
    s->windows = (ph / 8) * (pw / 8);
    s->batch = s->windows * w->heads;
    s->pixels = (size_t)height * width;
    size_t C = (size_t)w->channels;
    s->windowed = (size_t)s->windows * 64 * C;
    s->hidden_width = w->hidden_width;
    size_t hidden = (size_t)s->hidden_width;
    int wide = w->branched || w->family == SPLIT;
    ROLE(value, R_VALUE, s->pixels * C * 4);
    ROLE(value16, R_INPUT_KEY, s->pixels * C * 2);
    ROLE(hidden16, R_PROJECTION, s->pixels * hidden * 2);
    if (wide) ROLE(heads16, R_QUERY_PROB, s->pixels * C * 2); else s->heads16 = -1;
    ROLE(branch, R_BRANCH_VALUE, s->pixels * C * 4);
    ROLE(ffn, R_RESIDUAL, s->pixels * C * 4);
    ROLE(win16, R_INPUT_KEY, s->windowed * 2);
    ROLE(proj, R_PROJECTION, s->windowed * 3 * 4);
    ROLE(q16, R_QUERY_PROB, s->windowed * 2);
    ROLE(k16, R_INPUT_KEY, s->windowed * 2);
    ROLE(v16, R_BRANCH_VALUE, s->windowed * 2);
    ROLE(key16, R_PROJECTION, s->windowed * 2);
    ROLE(scores, R_SCORES_CONTEXT, (size_t)s->batch * 64 * 64 * 4);
    ROLE(probs16, R_QUERY_PROB, (size_t)s->batch * 64 * 64 * 2);
    ROLE(context, R_SCORES_CONTEXT, (size_t)s->batch * 64 * 32 * 4);
    ROLE(merged16, R_QUERY_PROB, s->windowed * 2);
    ROLE(context16, R_SCORES_CONTEXT, s->windowed * 2);
    ROLE(attended, R_PROJECTION, s->windowed * 4);
    ROLE(out, R_OUT, s->pixels * C * 4);
    if (w->family == SPLIT) ROLE(core16, R_QUERY_PROB, s->pixels * C * 2); else s->core16 = -1;
    return 0;
}

/* `GlobalScratch`: a bottleneck block over `tokens` tokens, padded to the tile. */
struct gscratch {
    int tokens, padded;
    int value, value16, hidden16, branch, ffn, ffn16, proj, q16, k16, v16, key16, scores, probs16,
        context, merged16, attention, out;
};

#define GROLE(field, role, bytes) do { s->field = arena(f, role, bytes); if (s->field == -2) return -1; } while (0)

static int global_scratch(struct nr_frame *f, const struct block_w *w, int tokens, struct gscratch *s)
{
    memset(s, 0, sizeof *s);
    /* `GlobalScratch`: whole 64-row blocks where the pad stays under an eighth, so the
     * bottleneck's deep-K GEMMs take the staged kernel; pad rows are zero and excluded from
     * the softmax, so every real row is bit-identical either way */
    int padded = align16(tokens);
    if (align64(tokens) * 8 <= padded * 9) padded = align64(tokens);
    s->tokens = tokens; s->padded = padded;
    size_t P = (size_t)s->padded, C = (size_t)w->channels, H = (size_t)w->hidden_width, heads = (size_t)w->heads;
    GROLE(value, R_GLOBAL_VALUE, P * C * 4);
    GROLE(value16, R_INPUT_KEY, P * C * 2);
    GROLE(hidden16, R_PROJECTION, P * H * 2);
    GROLE(branch, R_BRANCH_VALUE, P * C * 4);
    GROLE(ffn, R_RESIDUAL, P * C * 4);
    GROLE(ffn16, R_INPUT_KEY, P * C * 2);
    GROLE(proj, R_PROJECTION, P * C * 3 * 4);
    GROLE(q16, R_QUERY_PROB, P * C * 2);
    GROLE(k16, R_INPUT_KEY, P * C * 2);
    GROLE(v16, R_BRANCH_VALUE, P * C * 2);
    GROLE(key16, R_PROJECTION, P * C * 2);
    GROLE(scores, R_SCORES_CONTEXT, heads * P * P * 4);
    GROLE(probs16, R_QUERY_PROB, heads * P * P * 2);
    GROLE(context, R_SCORES_CONTEXT, heads * P * 32 * 4);
    GROLE(merged16, R_QUERY_PROB, P * C * 2);
    GROLE(attention, R_PROJECTION, P * C * 4);
    GROLE(out, R_GLOBAL_OUT, P * C * 4);
    return 0;
}

/* `TransitionScratch`: sized for the largest level that uses it, the request rounded up
 * to a power of two as the Python does. */
struct tscratch { int padded, pooled16, projected, projected16, upsampled, scaled; };

static int transition_scratch(struct nr_frame *f, size_t elements, struct tscratch *s)
{
    size_t rounded = 2;
    while (rounded < elements) rounded <<= 1;
    if ((s->padded = arena(f, R_PROJECTION, rounded * 4)) == -2) return -1;
    if ((s->pooled16 = arena(f, R_INPUT_KEY, rounded * 2)) == -2) return -1;
    if ((s->projected = arena(f, R_PROJECTION, rounded * 4)) == -2) return -1;
    if ((s->projected16 = arena(f, R_INPUT_KEY, rounded * 2)) == -2) return -1;
    if ((s->upsampled = arena(f, R_SCORES_CONTEXT, rounded * 4)) == -2) return -1;
    if ((s->scaled = arena(f, R_RESIDUAL, rounded * 4)) == -2) return -1;
    return 0;
}

#define TRY(call) do { if (call) return -1; } while (0)

/* `record_qkv`: split V; normalise Q and K straight out of the projection buffer. */
static int record_qkv(struct nr_frame *f, const struct block_w *w, int proj, int q16, int k16, int v16,
                      unsigned windows, unsigned tokens)
{
    unsigned C = (unsigned)w->channels, heads = (unsigned)w->heads;
    size_t rows = (size_t)windows * heads * tokens;
    if (f->opt.fuse_qk && f->opt.joint_qkv)
        return prepare_qkv(f, proj, q16, k16, v16, w->scale, windows, tokens, heads);
    if (f->opt.fuse_qk) {
        TRY(independent(f, 1));
        TRY(cosine_publish(f, proj, q16, rows, tokens, heads, w->scale, 1, 0, 0));
        TRY(cosine_publish(f, proj, k16, rows, tokens, heads, -1, 1, 0, 1));
        TRY(split_heads(f, proj, v16, windows, tokens, C, heads, 2, EPI_E4M3, 1));
        TRY(independent(f, 0));
        return 0;
    }
    TRY(independent(f, 1));
    TRY(split_heads(f, proj, q16, windows, tokens, C, heads, 0, EPI_HALF, 1));
    TRY(split_heads(f, proj, k16, windows, tokens, C, heads, 1, EPI_HALF, 1));
    TRY(split_heads(f, proj, v16, windows, tokens, C, heads, 2, EPI_E4M3, 1));
    TRY(independent(f, 0));
    TRY(independent(f, 1));
    TRY(cosine_publish(f, q16, q16, rows, tokens, heads, w->scale, 1, 1, -1));
    TRY(cosine_publish(f, k16, k16, rows, tokens, heads, -1, 1, 1, -1));
    TRY(independent(f, 0));
    return 0;
}

/* `record_qkv_projection`: the QKV projection and everything that prepares Q, K and V
 * after it. `*key` is the buffer K ended up in: `key16` under the epilogue, `k16` when the
 * projection went to memory as float32 and `record_qkv` read it back. With `window`, `a`
 * is the image (float32, or half with `image_half`) and the epilogue GEMM gathers its
 * window rows itself; without the epilogue the partition is recorded as it always was. */
static int record_qkv_projection(struct nr_frame *f, const struct block_w *w, int a,
                                 int proj, int q16, int k16, int v16, int key16, int win16,
                                 unsigned windows, unsigned tokens, int window, int height, int width,
                                 int image_half, int *key)
{
    unsigned C = (unsigned)w->channels, heads = (unsigned)w->heads;
    unsigned rows = windows * tokens;
    if (window) {
        if (f->opt.qkv_epilogue && rows % 64 == 0) {
            TRY(gemm_qkv(f, a, w->qkv, q16, key16, v16, w->scale, rows, C, heads, tokens,
                         1, height, width, w->oy, w->ox, image_half));
            *key = key16;
            return 0;
        }
        TRY(partition(f, a, win16, height, width, C, w->oy, w->ox, 1, image_half));
        a = win16;
    }
    if (f->opt.qkv_epilogue && rows % 16 == 0) {
        TRY(gemm_qkv(f, a, w->qkv, q16, key16, v16, w->scale, rows, C, heads, tokens, 0, 0, 0, 0, 0, 0));
        *key = key16;
        return 0;
    }
    TRY(gemm(f, a, w->qkv, proj, rows, 3 * C, C, NULL));
    TRY(record_qkv(f, w, proj, q16, k16, v16, windows, tokens));
    *key = k16;
    return 0;
}

/* `record_project_residual`: target = a @ weight + skip * cosine — fused into the GEMM's
 * epilogue, or the two passes the graph always had (`NR_FUSE_RESIDUAL=0`). */
static int record_project_residual(struct nr_frame *f, int a, int weight, int branch, int skip, int cosine,
                                   int target, unsigned rows, unsigned channels, unsigned inner,
                                   int epilogue, int skip_half, int narrow)
{
    if (f->opt.fuse_residual)
        return gemm_residual(f, a, weight, skip, cosine, target, rows, channels, inner, epilogue, narrow,
                             skip_half, 0, 0, 0, 0, 0);
    TRY(gemm(f, a, weight, branch, rows, channels, inner, NULL));
    return residual(f, branch, skip, cosine, target, (size_t)rows * channels, channels, epilogue, narrow,
                    0, skip_half, 0, 0, 0, 0, 0);
}

/* `_ffn_groups`: independent group products — one batched GEMM whose batch strides advance
 * the group (`NR_BATCH_FFN`), or one GEMM per group with the same tiles and rounding. */
static int ffn_groups(struct nr_frame *f, int a, int b, int c, unsigned rows, unsigned cols, unsigned inner,
                      unsigned groups, unsigned lda, unsigned ldb, unsigned ldc,
                      unsigned sa, unsigned sb, unsigned sc, int epilogue)
{
    if (f->opt.batch_ffn) {
        struct gemm_opt o = { .batch = groups, .sa = sa, .sb = sb, .sc = sc, .lda = lda, .ldb = ldb, .ldc = ldc,
                              .epilogue = epilogue, .narrow = 1 };
        return gemm(f, a, b, c, rows, cols, inner, &o);
    }
    TRY(independent(f, 1));
    for (unsigned g = 0; g < groups; g++) {
        struct gemm_opt o = { .lda = lda, .ldb = ldb, .ldc = ldc, .oa = g * sa, .ob = g * sb, .oc = g * sc,
                              .epilogue = epilogue, .narrow = 1 };
        TRY(gemm(f, a, b, c, rows, cols, inner, &o));
    }
    TRY(independent(f, 0));
    return 0;
}

/* `record_feed_forward`: branched or plain, into `s->ffn`. `source16` is a half copy of a
 * float32 input already written by the pass that produced it (the glue), so no to_half is
 * needed. `*ffn_half` says whether `s->ffn` was stored as half: the branched blocks publish
 * it as E4M3, which half holds exactly, so they store it narrow. */
static int record_feed_forward(struct nr_frame *f, const struct block_w *w, const struct scratch *s,
                               int source, int source_half, int source16, int *ffn_half)
{
    unsigned pixels = (unsigned)s->pixels, C = (unsigned)w->channels;
    unsigned hidden = (unsigned)s->hidden_width, groups = (unsigned)w->groups;
    int value16 = source_half ? source : (source16 >= 0 ? source16 : s->value16);
    if (!source_half && source16 < 0) TRY(to_half(f, source, s->value16, s->pixels * C));
    *ffn_half = 0;
    if (w->branched) {
        if (f->opt.fuse_branched_ffn && pixels % 16 == 0 && C % 16 == 0 && C == groups * 32) {
            /* every group's expand and projection in one pass, the hidden layer on chip */
            TRY(ffn_fused(f, value16, w->expand, w->branch, s->heads16, pixels, C, 128, groups,
                          -1, -1, EPI_E4M3, 1, 0));
        } else {
            TRY(ffn_groups(f, value16, w->expand, s->hidden16, pixels, 128, C, groups,
                           0, 0, hidden, 0, C * 128, 128, EPI_GATE_E4M3));
            TRY(ffn_groups(f, s->hidden16, w->branch, s->heads16, pixels, 32, 128, groups,
                           hidden, 0, C, 128, 128 * 32, 32, EPI_E4M3));
        }
        /* the fused multi-head kernels publish the residual before attention reads it,
         * which the residual now does on its way out — as half, which holds it exactly */
        TRY(record_project_residual(f, s->heads16, w->ffn_out, s->branch, source, w->ffn_cos, s->ffn,
                                    pixels, C, C, EPI_E4M3, source_half, 1));
        *ffn_half = 1;
        return 0;
    }
    if (f->opt.fuse_ffn && C == 32 && hidden % 32 == 0 && pixels % 16 == 0) {
        /* both GEMMs in one pass, the hidden layer never written (ffn_fused.comp) */
        return ffn_fused(f, value16, w->expand, w->branch, s->ffn, pixels, C, hidden, 1,
                         source, w->ffn_cos, 0, 0, source_half);
    }
    struct gemm_opt o = { .epilogue = EPI_GATE_E4M3, .narrow = 1 };
    TRY(gemm(f, value16, w->expand, s->hidden16, pixels, hidden, C, &o));
    return record_project_residual(f, s->hidden16, w->branch, s->branch, source, w->ffn_cos, s->ffn,
                                   pixels, C, hidden, 0, source_half, 0);
}

/* `record_split_feed_forward`: e4m3(x @ first), then a per-64-group 64 -> 256 -> 64 MLP. */
static int record_split_feed_forward(struct nr_frame *f, const struct block_w *w, const struct scratch *s,
                                     int source, int source_half)
{
    unsigned pixels = (unsigned)s->pixels, C = (unsigned)w->channels, groups = (unsigned)w->groups;
    unsigned wide = groups * 256;
    int value16 = source_half ? source : s->value16;
    if (!source_half) TRY(to_half(f, source, s->value16, s->pixels * C));
    struct gemm_opt first = { .epilogue = EPI_E4M3, .narrow = 1 };
    TRY(gemm(f, value16, w->first, s->heads16, pixels, C, C, &first));
    TRY(ffn_groups(f, s->heads16, w->expand, s->hidden16, pixels, 256, 64, groups,
                   C, 0, wide, 64, 64 * 256, 256, EPI_GATE));
    TRY(ffn_groups(f, s->hidden16, w->project, s->core16, pixels, 64, 256, groups,
                   wide, 0, C, 256, 256 * 64, 64, EPI_E4M3));
    return record_project_residual(f, s->core16, w->weight3, s->branch, source, w->ffn_cos, s->ffn,
                                   pixels, C, C, 0, source_half, 0);
}

/* `record_window_attention`: over `source`, in window order. With `to_target` the
 * output projection finishes the block — it adds `source * attn_cos` and writes straight
 * back into the unpadded image; otherwise the result goes to `s->attended` and the
 * closing residual's own gather reverses the windows. */
static int record_window_attention(struct nr_frame *f, const struct block_w *w, const struct scratch *s,
                                   int source, int to_target, int target, int publish_epilogue,
                                   int target_half, int source_half)
{
    unsigned C = (unsigned)w->channels, heads = (unsigned)w->heads, tokens = 64;
    int ph, pw, top, left;
    window_extent(s->height, s->width, w->oy, w->ox, 8, &ph, &pw, &top, &left);
    unsigned windows = (unsigned)((ph / 8) * (pw / 8));
    unsigned batch = windows * heads;
    int key;
    if (f->opt.fuse_partition) {
        /* the projection gathers its window rows from the image itself */
        TRY(record_qkv_projection(f, w, source, s->proj, s->q16, s->k16, s->v16, s->key16, s->win16,
                                  windows, tokens, 1, s->height, s->width, source_half, &key));
    } else {
        TRY(partition(f, source, s->win16, s->height, s->width, C, w->oy, w->ox, 1, source_half));
        TRY(record_qkv_projection(f, w, s->win16, s->proj, s->q16, s->k16, s->v16, s->key16, s->win16,
                                  windows, tokens, 0, 0, 0, 0, &key));
    }
    int fused = f->opt.fuse_window_attention && tokens == 64;
    int merged = fused && f->opt.fuse_attention_merge;
    int attended = merged ? s->context16 : s->merged16;
    if (fused) {
        TRY(window_attention(f, s->q16, key, s->v16, merged ? attended : s->context, batch, heads,
                             w->bias, merged));
    } else {
        struct gemm_opt qk = { .batch = batch, .sa = tokens * 32, .sb = tokens * 32, .sc = tokens * tokens,
                               .transpose_b = 1 };
        TRY(gemm(f, s->q16, key, s->scores, tokens, tokens, 32, &qk));
        TRY(softmax(f, s->scores, s->probs16, (size_t)batch * tokens, tokens, 0, 0.0f, 1, w->bias, heads));
        struct gemm_opt pv = { .batch = batch, .sa = tokens * tokens, .sb = tokens * 32, .sc = tokens * 32 };
        TRY(gemm(f, s->probs16, s->v16, s->context, tokens, 32, tokens, &pv));
    }
    if (!merged) TRY(merge_heads(f, s->context, s->merged16, windows, tokens, C, heads, EPI_E4M3, 1));
    if (to_target)
        return gemm_residual(f, attended, w->out, source, w->attn_cos, target, windows * tokens, C, C,
                             publish_epilogue, target_half, source_half, 1, s->height, s->width, w->oy, w->ox);
    return gemm(f, attended, w->out, s->attended, windows * tokens, C, C, NULL);
}

/* `record_block`: feed-forward, attention, both residuals. `prepared` is the block's
 * scratch when the caller allocated it first (the glue writes into its `value16` before the
 * block runs); `source16` is that half copy. */
static int record_block(struct nr_frame *f, const struct block_w *w, int height, int width, int source,
                        int target, int publish_epilogue, int source_half, int target_half,
                        int source16, const struct scratch *prepared)
{
    struct scratch own;
    const struct scratch *s = prepared;
    if (!s) { TRY(block_scratch(f, w, height, width, &own)); s = &own; }
    int ffn_half = 0;
    if (w->family == SPLIT) {
        if (source16 >= 0) FAILF("the split feed-forward takes no prepared half copy");
        TRY(record_split_feed_forward(f, w, s, source, source_half));
    } else {
        TRY(record_feed_forward(f, w, s, source, source_half, source16, &ffn_half));
    }
    if (f->opt.fuse_window_residual) {
        return record_window_attention(f, w, s, s->ffn, 1, target, publish_epilogue, target_half,
                                       ffn_half);
    }
    TRY(record_window_attention(f, w, s, s->ffn, 0, -1, 0, 0, ffn_half));
    return residual(f, s->attended, s->ffn, w->attn_cos, target, s->pixels * (size_t)w->channels,
                    (unsigned)w->channels, publish_epilogue, target_half, 0, ffn_half, 1, height, width,
                    w->oy, w->ox);
}

/* `record_global_block`: the wide feed-forward, then attention over every token. */
static int record_global_block(struct nr_frame *f, const struct block_w *w, const struct gscratch *s)
{
    unsigned C = (unsigned)w->channels, heads = (unsigned)w->heads, P = (unsigned)s->padded;
    unsigned hidden = (unsigned)w->hidden_width;
    size_t PC = (size_t)P * C;
    TRY(to_half(f, s->value, s->value16, PC));
    struct gemm_opt gate = { .epilogue = EPI_GATE_E4M3, .narrow = 1 };
    TRY(gemm(f, s->value16, w->expand, s->hidden16, P, hidden, C, &gate));
    TRY(record_project_residual(f, s->hidden16, w->ffn_proj, s->branch, s->value, w->ffn_cos, s->ffn,
                                P, C, hidden, 0, 0, 0));
    TRY(to_half(f, s->ffn, s->ffn16, PC));
    int key;
    TRY(record_qkv_projection(f, w, s->ffn16, s->proj, s->q16, s->k16, s->v16, s->key16, -1, 1, P,
                              0, 0, 0, 0, &key));
    struct gemm_opt qk = { .batch = heads, .sa = (unsigned long long)P * 32, .sb = (unsigned long long)P * 32,
                           .sc = (unsigned long long)P * P, .transpose_b = 1 };
    TRY(gemm(f, s->q16, key, s->scores, P, P, 32, &qk));
    /* no attention bias here, and the logits are clamped symmetrically */
    TRY(softmax(f, s->scores, s->probs16, (size_t)heads * P, (unsigned)s->tokens, P, w->logit_cap, 1, -1, 0));
    struct gemm_opt pv = { .batch = heads, .sa = (unsigned long long)P * P, .sb = (unsigned long long)P * 32,
                           .sc = (unsigned long long)P * 32 };
    TRY(gemm(f, s->probs16, s->v16, s->context, P, 32, P, &pv));
    TRY(merge_heads(f, s->context, s->merged16, 1, P, C, heads, EPI_E4M3, 1));
    return record_project_residual(f, s->merged16, w->out, s->attention, s->ffn, w->attn_cos, s->out,
                                   P, C, C, 0, 0, 0);
}

/* `record_downsample`: pool the unpublished output, publish it, then project. */
static int record_downsample(struct nr_frame *f, const struct edge_w *e, const struct tscratch *t, int source,
                             int target, int height, int width, unsigned C, int pad_to, int source_half,
                             int target_half)
{
    if (pad_to) {
        int ph = (height + pad_to - 1) / pad_to * pad_to, pw = (width + pad_to - 1) / pad_to * pad_to;
        TRY(pad_end(f, source, t->padded, height, width, ph, pw, C, source_half, source_half));
        source = t->padded; height = ph; width = pw;
    }
    unsigned pixels = (unsigned)((height / 2) * (width / 2));
    TRY(pool2(f, source, t->pooled16, height, width, C, EPI_E4M3, 1, source_half));
    struct gemm_opt o = { .epilogue = EPI_E4M3, .narrow = target_half };
    TRY(gemm(f, t->pooled16, e->weight0, target, pixels, (unsigned)e->out_channels, C, &o));
    return 0;
}

/* `record_plain_downsample`: block 30's bridge into the bottleneck, no publish between. */
static int record_plain_downsample(struct nr_frame *f, const struct edge_w *e, const struct tscratch *t,
                                   int source, int target, int height, int width, unsigned C, int pad_to,
                                   int source_half, int target_half)
{
    if (pad_to) {
        int ph = (height + pad_to - 1) / pad_to * pad_to, pw = (width + pad_to - 1) / pad_to * pad_to;
        TRY(pad_end(f, source, t->padded, height, width, ph, pw, C, source_half, source_half));
        source = t->padded; height = ph; width = pw;
    }
    unsigned pixels = (unsigned)((height / 2) * (width / 2));
    TRY(pool2(f, source, t->pooled16, height, width, C, EPI_HALF, 1, source_half));
    struct gemm_opt o = { .epilogue = EPI_E4M3, .narrow = target_half };
    TRY(gemm(f, t->pooled16, e->weight0, target, pixels, (unsigned)e->out_channels, C, &o));
    return 0;
}

/* `record_upsample_merge`: project, nearest-upsample onto the skip, add the scaled skip,
 * publish. */
static int record_upsample_merge(struct nr_frame *f, const struct edge_w *e, const struct tscratch *t,
                                 int source, int skip, int target, int sh, int sw, int height, int width,
                                 unsigned C, unsigned out_C, int source_half, int skip_half, int target_half)
{
    unsigned source_pixels = (unsigned)(sh * sw);
    int projected16 = source_half ? source : t->projected16;
    if (!source_half) TRY(to_half(f, source, t->projected16, (size_t)source_pixels * C));
    TRY(gemm(f, projected16, e->weight0, t->projected, source_pixels, out_C, C, NULL));
    size_t count = (size_t)height * width * out_C;
    TRY(independent(f, 1));
    TRY(upsample2(f, t->projected, t->upsampled, sw, height, width, out_C, 0));
    TRY(scale_channel(f, skip, e->sine, t->scaled, count, out_C, skip_half));
    TRY(independent(f, 0));
    TRY(add(f, t->upsampled, t->scaled, target, count, EPI_E4M3, target_half));
    return 0;
}

/* ------------------------------------------------------------------------- */
/* the graph for one extent: ResidentFrame._run in replay mode                  */
/* ------------------------------------------------------------------------- */

static const struct block_w *block(struct nr_frame *f, int index, int heads, enum family family)
{
    struct block_w *b = &f->blocks[index];
    if (b->loaded) return b;
    int r = family == SPLIT ? load_split_block(b, &f->w, index)
          : family == GLOBAL ? load_global_block(b, &f->w, index)
          : load_window_block(b, &f->w, index, heads);
    return r ? NULL : b;
}

static const struct edge_w *edge(struct nr_frame *f, int index, int up)
{
    struct edge_w *e = &f->edges[index];
    if (e->loaded) return e;
    return load_edge(e, &f->w, index, up) ? NULL : e;
}

static void plan_levels(struct nr_frame *f, int H, int W)
{
    int (*L)[3] = f->levels;
    L[0][0] = H;      L[0][1] = W;      L[0][2] = 32;
    L[1][0] = H / 2;  L[1][1] = W / 2;  L[1][2] = 32;
    L[2][0] = H / 4;  L[2][1] = W / 4;  L[2][2] = 64;
    L[3][0] = H / 8;  L[3][1] = W / 8;  L[3][2] = 128;
    L[4][0] = H / 16; L[4][1] = W / 16; L[4][2] = 256;
    L[5][0] = pad8(L[4][0]) / 2; L[5][1] = pad8(L[4][1]) / 2; L[5][2] = 512;
    L[6][0] = pad8(L[5][0]) / 2; L[6][1] = pad8(L[5][1]) / 2; L[6][2] = 1024;
}

#define BLOCK(var, index, heads, family) const struct block_w *var = block(f, index, heads, family); if (!var) return -1
#define EDGE(var, index, up) const struct edge_w *var = edge(f, index, up); if (!var) return -1
#define NAMED(var, name, bytes) int var = named_buffer(f, name, bytes); if (var == -2) return -1
#define NAMEDF(var, fmt, i, bytes) int var = named_bufferf(f, fmt, i, bytes); if (var == -2) return -1

static const struct { int first, last, transition, heads; } ENCODER[4] = {
    { 1, 3, 4, 1 }, { 5, 7, 8, 2 }, { 9, 13, 14, 4 }, { 15, 21, 22, 8 } };
static const struct { int transition, first, last, level, heads; } DECODER[4] = {
    { 48, 49, 55, 4, 8 }, { 56, 57, 61, 3, 4 }, { 62, 63, 65, 2, 2 }, { 66, 67, 69, 1, 1 } };

/* One pass over the graph. Planning, it sizes the arena; recording, it emits every
 * dispatch. The same code both times is what makes the plan sufficient. */
static int build(struct nr_frame *f)
{
    int H = f->height, W = f->width;
    size_t pixels = (size_t)H * W;
    int (*L)[3] = f->levels;

    NAMED(stem, "stem", pixels * 32 * 4);
    /* separate names keep a captured graph's addresses valid across NR_INPUT_FP16 */
    int source16;
    if (f->opt.input_fp16) {
        NAMED(half_in, "features_host16", pixels * 16 * 2);
        source16 = half_in;
    } else {
        NAMED(source, "features", pixels * 16 * 4);
        NAMED(features16, "features16", pixels * 16 * 2);
        source16 = features16;
        if (!f->planning && X.begin()) FAILF("xmx_begin: %s", X.error());
        TRY(to_half(f, source, features16, pixels * 16));
    }
    if (f->opt.input_fp16 && !f->planning && X.begin()) FAILF("xmx_begin: %s", X.error());
    /* block 0 at full resolution: the skip the post block merges and, pooled, the
     * encoder's input; every published buffer is stored narrow. Its scratch is taken
     * first, because under the glue the stem's GEMM also writes its `value16`. */
    BLOCK(block0, 0, 1, WINDOW);
    struct scratch scratch0;
    TRY(block_scratch(f, block0, H, W, &scratch0));
    if (f->opt.fuse_glue)
        TRY(gemm_dual(f, source16, f->adapter, stem, scratch0.value16, (unsigned)pixels, 32, 16));
    else
        TRY(gemm(f, source16, f->adapter, stem, (unsigned)pixels, 32, 16, NULL));

    NAMED(raw, "block0", pixels * 32 * 4);
    NAMED(full_skip, "full_skip", pixels * 32 * 2);
    int h = L[1][0], w = L[1][1], C = L[1][2];
    NAMED(l1, "l1", (size_t)h * w * 32 * 2);
    int value = l1;
    TRY(record_block(f, block0, H, W, stem, raw, 0, 0, 0,
                     f->opt.fuse_glue ? scratch0.value16 : -1, &scratch0));
    TRY(independent(f, 1));
    TRY(e4m3_half(f, raw, full_skip, pixels * 32));
    TRY(pool2(f, raw, value, H, W, 32, EPI_E4M3, 1, 0));
    TRY(independent(f, 0));

    int skips[7] = { -1, -1, -1, -1, -1, -1, -1 };
    int level = 1;
    for (int e = 0; e < 4; e++) {
        h = L[level][0]; w = L[level][1]; C = L[level][2];
        for (int index = ENCODER[e].first; index <= ENCODER[e].last; index++) {
            BLOCK(b, index, ENCODER[e].heads, WINDOW);
            TRY(record_block(f, b, h, w, value, value, EPI_E4M3, 1, 1, -1, NULL));
        }
        NAMEDF(skip, "skip%d", level, (size_t)h * w * C * 2);
        skips[level] = skip;
        TRY(copy(f, value, skip, (size_t)h * w * C * 2));
        BLOCK(tb, ENCODER[e].transition, ENCODER[e].heads, WINDOW);
        EDGE(down, ENCODER[e].transition, 0);
        int nh = L[level + 1][0], nw = L[level + 1][1], nC = L[level + 1][2];
        NAMED(unpublished, "unpublished", (size_t)h * w * C * 4);
        NAMEDF(nxt, "l%d", level + 1, (size_t)nh * nw * nC * 2);
        struct tscratch t;
        TRY(transition_scratch(f, (size_t)pad8(h) * pad8(w) * C, &t));
        TRY(record_block(f, tb, h, w, value, unpublished, 0, 1, 0, -1, NULL));
        TRY(record_downsample(f, down, &t, unpublished, nxt, h, w, (unsigned)C,
                              ENCODER[e].transition == 22 ? 8 : 0, 0, 1));
        value = nxt;
        level++;
    }

    /* the split family, then the bottleneck */
    h = L[5][0]; w = L[5][1]; C = L[5][2];
    for (int index = 23; index <= 30; index++) {
        BLOCK(b, index, 16, SPLIT);
        TRY(record_block(f, b, h, w, value, value, EPI_E4M3, 1, 1, -1, NULL));
    }
    NAMED(split_skip, "split_skip", (size_t)h * w * C * 2);
    TRY(copy(f, value, split_skip, (size_t)h * w * C * 2));
    int gh = L[6][0], gw = L[6][1], gC = L[6][2];
    NAMED(deep, "l6", (size_t)gh * gw * gC * 2);
    {
        struct tscratch t;
        TRY(transition_scratch(f, (size_t)pad8(h) * pad8(w) * C, &t));
        TRY(record_plain_downsample(f, &f->bottleneck, &t, value, deep, h, w, (unsigned)C, 8, 1, 1));
    }

    int tokens = gh * gw;
    for (int index = 31; index <= 38; index++) {
        BLOCK(b, index, 32, GLOBAL);
        struct gscratch s;
        TRY(global_scratch(f, b, tokens, &s));
        TRY(from_half(f, deep, s.value, (size_t)tokens * gC));
        TRY(record_global_block(f, b, &s));
        TRY(e4m3(f, s.out, s.out, (size_t)s.padded * gC));
        TRY(to_half(f, s.out, deep, (size_t)tokens * gC));
    }

    /* the decoder input merge, then the split family again */
    {
        struct tscratch t;
        TRY(transition_scratch(f, (size_t)h * w * C, &t));
        TRY(record_upsample_merge(f, &f->decoder_input, &t, deep, split_skip, value, gh, gw, h, w,
                                  (unsigned)gC, (unsigned)C, 1, 1, 1));
    }
    for (int index = 40; index <= 47; index++) {
        BLOCK(b, index, 16, SPLIT);
        TRY(record_block(f, b, h, w, value, value, EPI_E4M3, 1, 1, -1, NULL));
    }

    for (int d = 0; d < 4; d++) {
        int sl = DECODER[d].level;
        int sh = L[sl][0], sw = L[sl][1], sC = L[sl][2];
        EDGE(up, DECODER[d].transition, 1);
        NAMEDF(target, "d%d", sl, (size_t)sh * sw * sC * 2);
        struct tscratch t;
        TRY(transition_scratch(f, (size_t)sh * sw * (C > sC ? C : sC), &t));
        TRY(record_upsample_merge(f, up, &t, value, skips[sl], target, h, w, sh, sw, (unsigned)C,
                                  (unsigned)sC, 1, 1, 1));
        BLOCK(tb, DECODER[d].transition, DECODER[d].heads, WINDOW);
        TRY(record_block(f, tb, sh, sw, target, target, EPI_E4M3, 1, 1, -1, NULL));
        value = target; h = sh; w = sw; C = sC;
        for (int index = DECODER[d].first; index <= DECODER[d].last; index++) {
            BLOCK(b, index, DECODER[d].heads, WINDOW);
            TRY(record_block(f, b, h, w, value, value, EPI_E4M3, 1, 1, -1, NULL));
        }
    }

    /* back to full resolution, merged with block 0's output, then the head */
    NAMED(merged, "merged", pixels * 32 * 4);
    BLOCK(block70, 70, 1, WINDOW);
    struct scratch scratch70;
    TRY(block_scratch(f, block70, H, W, &scratch70));
    /* The head reads block 70's output as half and nothing reads it as float32, so the
     * block's closing residual stores half itself. */
    NAMED(out16, "out16", pixels * 32 * 2);
    if (f->opt.fuse_glue) {
        TRY(upsample_merge(f, value, full_skip, f->merge_sincos, merged, scratch70.value16, H, W, w, 32));
    } else {
        NAMED(upsampled, "upsampled", pixels * 32 * 4);
        TRY(upsample2(f, value, upsampled, w, H, W, 32, 1));
        TRY(scale_channel(f, upsampled, f->merge_sin, merged, pixels * 32, 32, 0));
        TRY(residual(f, merged, full_skip, f->merge_cos, merged, pixels * 32, 32, 0, 0, 0, 1, 0, 0, 0, 0, 0));
    }
    TRY(record_block(f, block70, H, W, merged, out16, 0, 0, 1,
                     f->opt.fuse_glue ? scratch70.value16 : -1, &scratch70));
    if (f->opt.compact_head) {
        /* only the four useful columns, at a row stride of four */
        NAMED(head4, "head4", pixels * 4 * 4);
        struct gemm_opt o = { .sa = pixels * 32, .sb = 32 * 16, .sc = pixels * 4, .ldc = 4, .extra = 0x10000u };
        TRY(gemm(f, out16, f->head, head4, (unsigned)pixels, 16, 32, &o));
    } else {
        NAMED(head, "head", pixels * 16 * 4);
        TRY(gemm(f, out16, f->head, head, (unsigned)pixels, 16, 32, NULL));
    }

    if (!f->planning) {
        f->graph = X.graph_capture();
        if (f->graph < 0) { f->graph = -1; FAILF("xmx_graph_capture: %s", X.error()); }
    }
    return 0;
}

static void release_extent(struct nr_frame *f)
{
    if (f->graph >= 0) { X.graph_destroy(f->graph); f->graph = -1; }
    for (int i = 0; i < f->named_count; i++) X.buf_destroy(f->named[i].id);
    f->named_count = 0;
    arena_reset(f);
    f->height = f->width = 0;
}

/* The graph for a network extent: planned, then recorded and captured once. */
static int prepare_extent(struct nr_frame *f, int H, int W)
{
    if (f->height == H && f->width == W && f->graph >= 0) return 0;
    if (H % 64 || W % 64 || H < 320 || W < 320) FAILF("network extent %dx%d is not vendor-aligned", H, W);
    release_extent(f);
    f->height = H; f->width = W;
    plan_levels(f, H, W);
    f->planning = 1;
    if (build(f)) { release_extent(f); return -1; }
    f->planning = 0;
    if (build(f)) { X.abort(); release_extent(f); return -1; }
    size_t pixels = (size_t)H * W;
    free(f->features_host); free(f->head_host); free(f->noise); free(f->rows); free(f->cols);
    f->features_host = malloc(pixels * 16 * sizeof(float));
    f->head_host = malloc(pixels * 16 * sizeof(float));
    f->noise = malloc(pixels * 3 * sizeof(float));
    f->rows = malloc((size_t)H * sizeof(int32_t));
    f->cols = malloc((size_t)W * sizeof(int32_t));
    f->noise_index = INT32_MIN;
    if (!f->features_host || !f->head_host || !f->noise || !f->rows || !f->cols) FAILF("out of memory");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* features: nr_frame.build_features, on nr_image's pass                        */
/* ------------------------------------------------------------------------- */

static int aligned_extent(int extent)
{
    int minimum = extent > 320 ? extent : 320;
    return (minimum + 63) / 64 * 64;
}

void nr_frame_geometry(int height, int width, int *network_height, int *network_width)
{
    if (network_height) *network_height = aligned_extent(height);
    if (network_width) *network_width = aligned_extent(width);
}

/* `NetworkGeometry.extended_indices`: the image at the origin, the extension mirroring
 * it without repeating the edge. */
static void extended_indices(int32_t *out, int count, int extent)
{
    for (int i = 0; i < count; i++) {
        int mirrored = 2 * extent - 2 - i;
        out[i] = i < extent ? i : (mirrored > 0 ? mirrored : 0);
    }
}

/* `features.deterministic_noise`, in float32 with NumPy's operation order. The last bit
 * of `logf`, `sqrtf`, `cosf` and `sinf` is the C library's rather than NumPy's, and the
 * two can differ there; `test_nr_frame_c.py` measures how often. */
static uint32_t shift_mix(uint32_t v)
{
    uint32_t shift = (v >> 28) + 4u;
    return (v ^ (v >> shift)) * 0x108EF2D9u;
}

static float uniform24(uint32_t v)
{
    uint32_t mixed = shift_mix(v);
    uint32_t bits = (mixed >> 30) ^ (mixed >> 8);
    return (float)(bits + 1u) * 5.960464477539063e-8f;
}

static void deterministic_noise(float *out, int height, int width, int frame_index)
{
    const float tau = 6.2831854820251465f;
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            uint32_t seed = (uint32_t)y * 0xD8163841u;
            seed ^= (uint32_t)x * 0x8DA6B343u;
            seed ^= (uint32_t)((uint32_t)frame_index * 0x9E3779B9u);
            seed ^= 0x243F6A88u;
            uint32_t m = shift_mix(seed);
            uint32_t mixed = m ^ (m >> 22);
            float ra = uniform24(mixed * 0xCAA5B80Du + 0x21DD796Bu);
            float ab = uniform24(mixed * 0x83232C31u + 0x3463E0ACu);
            float rb = uniform24(mixed * 0x2C9277B5u + 0xAC564B05u);
            float aa = uniform24(mixed * 0xFA6DC5F9u + 0x4712A88Eu);
            float radius_a = sqrtf(-2.0f * logf(ra));
            float radius_b = sqrtf(-2.0f * logf(rb));
            float angle_a = tau * aa, angle_b = tau * ab;
            float *o = out + ((size_t)y * width + x) * 3;
            o[0] = half_round(radius_b * cosf(angle_a));
            o[1] = half_round(radius_b * sinf(angle_a));
            o[2] = half_round(radius_a * cosf(angle_b));
        }
}

/* `make_features`' three ways of filling channels 10-14: a plain recipe, the automatic
 * mask (skin and automatic-mask structure, -1 following the local structure), or a
 * per-pixel control mask, whose green and blue scale tone and structure per pixel and
 * zero the three structure scalars. */
static int features_into(struct nr_frame *f, const float *colour, int height, int width, const float *history,
                         const float *mask, const nr_frame_params *p, float *out)
{
    int H = f->height, W = f->width;
    if (f->noise_index != p->frame_index) {
        deterministic_noise(f->noise, H, W, p->frame_index);
        f->noise_index = p->frame_index;
    }
    extended_indices(f->rows, H, height);
    extended_indices(f->cols, W, width);
    float controls[5] = { half_round(p->normalized_style), half_round(p->local_tone),
                          half_round(p->local_structure), -1.0f, -1.0f };
    if (mask) {
        controls[2] = controls[3] = controls[4] = 0.0f;
    } else if (p->automatic_mask) {
        int enabled = (p->skin_structure > p->automatic_structure ? p->skin_structure : p->automatic_structure) >= 0.0f;
        controls[2] = half_round(enabled ? 1.0f : p->local_structure);
        controls[3] = half_round(enabled ? (p->skin_structure >= 0.0f ? p->skin_structure : p->local_structure) : -1.0f);
        controls[4] = half_round(enabled ? (p->automatic_structure >= 0.0f ? p->automatic_structure : p->local_structure) : -1.0f);
    }
    nr_features(colour, (ptrdiff_t)width * 3, 3, 1,
                history, history ? (ptrdiff_t)width * 3 : 0, history ? 3 : 0, history ? 1 : 0,
                f->rows, f->cols, (size_t)H, (size_t)W, f->noise, controls, out);
    if (mask) {
        /* `half(mask[..., 1] * np.float32(local_tone_strength))`: the raw strength, not the
         * half-rounded one the scalar path uses */
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                const float *m = mask + ((size_t)f->rows[y] * width + f->cols[x]) * 3;
                float *o = out + ((size_t)y * W + x) * 16;
                o[11] = half_round(m[1] * p->local_tone);
                o[12] = half_round(m[2] * p->local_structure);
            }
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/* the network                                                                  */
/* ------------------------------------------------------------------------- */

static double now(void) { return nr_now(); }

/* features (H, W, 16) at the network extent -> the head (H, W, 16) as the device holds
 * it: the first four channels of each sixteen are the head. */
static const float *run_graph(struct nr_frame *f, const float *features)
{
    size_t pixels = (size_t)f->height * f->width;
    int fp16 = f->opt.input_fp16, compact = f->opt.compact_head;
    int source = fp16 ? named_buffer(f, "features_host16", pixels * 16 * 2)
                      : named_buffer(f, "features", pixels * 16 * 4);
    int head = compact ? named_buffer(f, "head4", pixels * 4 * 4) : named_buffer(f, "head", pixels * 16 * 4);
    if (source < 0 || head < 0) {
        snprintf(last_error, sizeof last_error, "graph buffers are missing");
        return NULL;
    }
    double t0 = now();
    if (fp16) {
        /* NR_INPUT_FP16: the features rounded to half on the host, as NumPy's astype does,
         * straight into the mapped input when it is mapped; no to_half pass on the device */
        size_t n = pixels * 16;
        uint16_t *dst = X.buf_host_visible(source) ? (uint16_t *)X.buf_ptr(source) : (uint16_t *)f->head_host;
        for (size_t i = 0; i < n; i++) dst[i] = float_to_half(features[i]);
        if (!X.buf_host_visible(source) && host_write(source, dst, n * 2)) return NULL;
    } else if (host_write(source, features, pixels * 16 * 4)) {
        return NULL;
    }
    double t1 = now();
    if (X.graph_run(f->graph) < 0) {
        snprintf(last_error, sizeof last_error, "xmx_graph_run: %s", X.error());
        return NULL;
    }
    double t2 = now();
    f->head_stride = compact ? 4 : 16;
    const float *out = host_read(head, f->head_host, pixels * (size_t)f->head_stride * 4);
    f->split[0] = t1 - t0; f->split[1] = t2 - t1; f->split[2] = now() - t2;
    return out;
}

double nr_frame_split(const nr_frame *f, int which) { return which >= 0 && which < 3 ? f->split[which] : 0.0; }

/* ------------------------------------------------------------------------- */
/* composition: nr_frame.compose                                                */
/* ------------------------------------------------------------------------- */

static float unit(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

/* `compose_detail`: result = source + colour * lowpass(change) + detail * highpass(change).
 * The kernel is `gaussian_kernel` and the blur the NumPy one (edge replication, one
 * axis then the other, accumulated in kernel order); `expf` stands where NumPy's
 * float32 exponential stood, and the two can differ in the last bit of a weight. */
static int compose_detail(struct nr_frame *f, const float *source, float *output, int height, int width,
                          float detail, float colour_s, float radius)
{
    if (detail == 1.0f && colour_s == 1.0f) return 0;
    if (!(radius > 0.0f) || !isfinite(radius) || !isfinite(detail) || !isfinite(colour_s))
        FAILF("strengths and radius must be finite and the radius positive");
    int extent = (int)ceilf(3.0f * radius);
    int taps = 2 * extent + 1;
    float *kernel = malloc((size_t)taps * sizeof(float));
    if (!kernel) FAILF("out of memory");
    float sum = 0.0f;
    float denominator = (float)(2.0 * (double)radius * (double)radius);   /* np.float32(2 * sigma * sigma) */
    for (int i = 0; i < taps; i++) {
        float o = (float)(i - extent);
        kernel[i] = expf(-o * o / denominator);
        sum += kernel[i];
    }
    for (int i = 0; i < taps; i++) kernel[i] = kernel[i] / sum;
    size_t pixels = (size_t)height * width, n = pixels * 3;
    if (f->composed_pixels < pixels) {
        free(f->scratch_a); free(f->scratch_b);
        f->scratch_a = malloc(n * sizeof(float));
        f->scratch_b = malloc(n * sizeof(float));
        f->composed_pixels = pixels;
        if (!f->scratch_a || !f->scratch_b) { free(kernel); FAILF("out of memory"); }
    }
    float *change = f->scratch_a, *low = f->scratch_b;
    for (size_t i = 0; i < n; i++) change[i] = output[i] - source[i];
    /* horizontal, into `low`; then vertical, back into `change`'s partner */
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            for (int c = 0; c < 3; c++) {
                float acc = 0.0f;
                for (int k = 0; k < taps; k++) {
                    int sx = x + k - extent;
                    sx = sx < 0 ? 0 : sx >= width ? width - 1 : sx;
                    acc += kernel[k] * change[((size_t)y * width + sx) * 3 + c];
                }
                low[((size_t)y * width + x) * 3 + c] = acc;
            }
    float *vertical = malloc(n * sizeof(float));
    if (!vertical) { free(kernel); FAILF("out of memory"); }
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            for (int c = 0; c < 3; c++) {
                float acc = 0.0f;
                for (int k = 0; k < taps; k++) {
                    int sy = y + k - extent;
                    sy = sy < 0 ? 0 : sy >= height ? height - 1 : sy;
                    acc += kernel[k] * low[((size_t)sy * width + x) * 3 + c];
                }
                vertical[((size_t)y * width + x) * 3 + c] = acc;
            }
    for (size_t i = 0; i < n; i++) {
        float lowpass = vertical[i];
        float term_low = colour_s * lowpass;
        float highpass = change[i] - lowpass;
        float term_high = detail * highpass;
        output[i] = unit(source[i] + term_low + term_high);
    }
    free(vertical);
    free(kernel);
    return 0;
}

/* `nr_frame.compose`. The head arrives with the device's stride of sixteen floats per
 * pixel over the network extent, and is read through it: no crop copy. */
/* `head` has `hy` floats per row and `hx` per pixel: 16 straight off the device, 4 for a
 * cropped head a caller hands back. `mask` is the control mask's RGB, its red the blend. */
static int compose(struct nr_frame *f, const float *head, ptrdiff_t hy, ptrdiff_t hx, const float *colour,
                   int height, int width, const float *history, const float *previous, const float *mask,
                   const nr_frame_params *p, float *output)
{
    ptrdiff_t s = (ptrdiff_t)width * 3;
    if (history) {
        /* `history_weight`: clip(sigmoid(half(logit)) * half(blend_scale), 0, 1), then the
         * confidence. `expf` here against NumPy's exp there: the one place the temporal
         * path can differ from the Python by a last bit. */
        size_t pixels = (size_t)height * width;
        if (f->composed_pixels < pixels) {
            free(f->scratch_a); free(f->scratch_b);
            f->scratch_a = malloc(pixels * 3 * sizeof(float));
            f->scratch_b = malloc(pixels * 3 * sizeof(float));
            f->composed_pixels = pixels;
            if (!f->scratch_a || !f->scratch_b) FAILF("out of memory");
        }
        float *alpha = f->scratch_a;
        float scale = half_round(p->blend_scale);
        float confidence = unit(p->history_confidence);
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                float logit = half_round(head[(ptrdiff_t)y * hy + (ptrdiff_t)x * hx + 3]);
                float a = unit(1.0f / (1.0f + expf(-logit)) * scale);
                if (p->history_confidence != 1.0f) a = a * confidence;
                alpha[(size_t)y * width + x] = a;
            }
        nr_compose_temporal(head, hy, hx, 1, colour, s, 3, 1, history, s, 3, 1,
                            previous, previous ? s : 0, previous ? 3 : 0, previous ? 1 : 0,
                            alpha, width, 1, mask, mask ? s : 0, mask ? 3 : 0, (size_t)height, (size_t)width,
                            p->intensity, p->blend_scale, previous ? p->hold : 0.0f, previous ? p->slope : 0.0f,
                            output);
    } else if (mask) {
        /* `compose_head` with a mask: blend = clip(red * intensity, 0, 1) — and past
         * intensity 1 `nr_frame.compose` takes its own branch, where the blend is not
         * clamped so that it can extrapolate */
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                const float *h = head + (ptrdiff_t)y * hy + (ptrdiff_t)x * hx;
                const float *rgb = colour + (size_t)y * s + (size_t)x * 3;
                float blend = mask[(size_t)y * s + (size_t)x * 3] * p->intensity;
                if (p->intensity <= 1.0f) blend = unit(blend);
                for (int c = 0; c < 3; c++) {
                    float source = rgb[c];
                    float predicted = unit(source + half_round(h[c]) * 0.25f);
                    output[((size_t)y * width + x) * 3 + c] = unit(source + blend * (predicted - source));
                }
            }
    } else {
        nr_compose(head, hy, hx, 1, colour, s, 3, 1, (size_t)height, (size_t)width, p->intensity, output);
    }
    return compose_detail(f, colour, output, height, width, p->detail_strength, p->colour_strength,
                          p->detail_radius);
}

/* ------------------------------------------------------------------------- */
/* the public API                                                               */
/* ------------------------------------------------------------------------- */

void nr_frame_defaults(nr_frame_params *p)
{
    memset(p, 0, sizeof *p);
    p->intensity = 1.0f; p->detail_strength = 1.0f; p->colour_strength = 1.0f; p->detail_radius = 4.0f;
    p->normalized_style = 0.0f; p->local_tone = 1.0f; p->local_structure = 1.0f; p->frame_index = 0;
    p->history_confidence = 1.0f; p->blend_scale = 0.73974609375f; p->hold = 0.0f; p->slope = 0.0f;
    p->automatic_mask = 0; p->skin_structure = -1.0f; p->automatic_structure = -1.0f;
}

const char *nr_frame_runtime(void)
{
#if defined(NR_STATIC_METAL)
    return "metal";
#elif defined(NR_STATIC_D3D12)
    return "d3d12";
#elif defined(NR_STATIC_XMX)
    return "vulkan";
#else
    const char *backend = getenv("NR_GPU_BACKEND");
    if (backend && !strcmp(backend, "metal")) return "metal";
    if (backend && !strcmp(backend, "d3d12")) return "d3d12";
    return "vulkan";
#endif
}

int nr_frame_adopt_vulkan(void *instance, void *physical_device, void *device, void *queue,
                          unsigned queue_family, int cooperative_matrix,
                          void *get_instance_proc_addr, void (*lock)(void *),
                          void (*unlock)(void *), void *lock_context)
{
    if (xmx_is_ready) FAILF("the device is open; nr_frame_shutdown first");
    if (!strcmp(nr_frame_runtime(), "d3d12"))
        FAILF("the runtime behind this library is Direct3D 12 (libd3dmx): nr_frame_adopt_d3d12 is the call");
    if (xmx_load()) return -1;
    if (!X.adopt) FAILF("this libxmx has no xmx_adopt");
    if (X.adopt(instance, physical_device, device, queue, queue_family, cooperative_matrix,
                get_instance_proc_addr, lock, unlock, lock_context))
        FAILF("xmx_adopt: %s", X.error());
    return 0;
}

int nr_frame_adopt_d3d12(void *device, void *queue, void (*lock)(void *), void (*unlock)(void *),
                         void *lock_context)
{
    if (xmx_is_ready) FAILF("the device is open; nr_frame_shutdown first");
    if (strcmp(nr_frame_runtime(), "d3d12"))
        FAILF("the runtime behind this library is %s, not Direct3D 12: nothing to adopt the device into",
              nr_frame_runtime());
    if (xmx_load()) return -1;
    if (!X.adopt) FAILF("this libd3dmx has no xmx_adopt");
    /* libd3dmx reads the device and the queue from libxmx's argument list and wants the
     * Vulkan-only arguments NULL */
    if (X.adopt(NULL, NULL, device, queue, 0, 0, NULL, lock, unlock, lock_context))
        FAILF("xmx_adopt: %s", X.error());
    return 0;
}

void nr_frame_shutdown(void)
{
    if (X.handle && X.close) X.close();
    xmx_is_ready = 0;
}

int nr_frame_shared_device(void)
{
    return (X.handle && X.adopted) ? X.adopted() : -1;
}

/* An NR_* switch as `os.environ.get(name, default) != "0"` reads it. */
static int env_switch(const char *name, int fallback)
{
    const char *value = getenv(name);
    if (!value) return fallback;
    return strcmp(value, "0") != 0;
}

nr_frame *nr_frame_open(const char *weights_path)
{
    if (xmx_ready()) return NULL;
    struct nr_frame *f = calloc(1, sizeof *f);
    if (!f) FAILP("out of memory");
    f->graph = -1;
    for (int r = 0; r < R_COUNT; r++) f->role_id[r] = -1;
    /* buffer ids start at zero, so "none" has to be -1 from the start */
    f->adapter = f->merge_sin = f->merge_cos = f->merge_sincos = f->head = -1;
    f->head_stride = 16;
    f->bottleneck.weight0 = f->bottleneck.sine = -1;
    f->decoder_input.weight0 = f->decoder_input.sine = -1;
    /* `xmxres.Runtime.__init__`, switch for switch and default for default */
    f->opt.fuse_qk = env_switch("NR_FUSE_QK", 1);
    f->opt.batch_ffn = env_switch("NR_BATCH_FFN", 1);
    f->opt.input_fp16 = env_switch("NR_INPUT_FP16", 0);
    f->opt.compact_head = env_switch("NR_COMPACT_HEAD", 0);
    f->opt.joint_qkv = env_switch("NR_JOINT_QKV", 0);
    f->opt.fuse_residual = env_switch("NR_FUSE_RESIDUAL", 1);
    f->opt.fuse_window_residual = env_switch("NR_FUSE_WINDOW_RESIDUAL", 1);
    f->opt.fuse_window_attention = env_switch("NR_FUSE_WINDOW_ATTENTION", 1);
    f->opt.fuse_attention_merge = env_switch("NR_FUSE_ATTENTION_MERGE", 1);
    f->opt.qkv_epilogue = env_switch("NR_QKV_EPILOGUE", 1);
    f->opt.fuse_glue = env_switch("NR_FUSE_GLUE", 1);
    f->opt.fuse_ffn = env_switch("NR_FUSE_FFN", 1);
    f->opt.fuse_branched_ffn = env_switch("NR_FUSE_BRANCHED_FFN", 0);
    /* off where the runtime cannot gather a window-ordered A, as xmxres.py decides it */
    f->opt.fuse_partition = env_switch("NR_FUSE_PARTITION", 1)
                            && X.window_gather && X.window_gather() == 1;
    if (weights_load(&f->w, weights_path)) { free(f); return NULL; }
    /* the six named weights `DeviceWeights` uploads before any block */
    const struct tensor *adapter = weight(&f->w, "block0.layer0.input_adapter_weight");
    const struct tensor *bottleneck = weight(&f->w, "block30.layer4.weight");
    const struct tensor *conv = weight(&f->w, "block39.layer0.conv_weight");
    const struct tensor *upsine = weight(&f->w, "block39.layer0.inp_upsample_sin");
    const struct tensor *msin = weight(&f->w, "block70.layer0.inp_merge_sin");
    const struct tensor *mcos = weight(&f->w, "block70.layer0.inp_merge_cos");
    const struct tensor *gain = weight(&f->w, "block70.layer0.out_gain");
    const struct tensor *oconv = weight(&f->w, "block70.layer0.out_conv_weight");
    if (!adapter || !bottleneck || !conv || !upsine || !msin || !mcos || !gain || !oconv) {
        nr_frame_close(f);
        FAILP("%s: the edge weights are missing; is this the logical file?", weights_path ? weights_path : "embedded weights");
    }
    if ((f->adapter = buffer_f16(adapter->data, adapter->count)) < 0) goto fail;
    if ((f->bottleneck.weight0 = buffer_f16(bottleneck->data, bottleneck->count)) < 0) goto fail;
    f->bottleneck.out_channels = (int)bottleneck->shape[1]; f->bottleneck.sine = -1; f->bottleneck.loaded = 1;
    if ((f->decoder_input.weight0 = buffer_f16(conv->data, conv->count)) < 0) goto fail;
    if ((f->decoder_input.sine = buffer_f32(upsine->data, upsine->count)) < 0) goto fail;
    f->decoder_input.out_channels = (int)conv->shape[1]; f->decoder_input.loaded = 1;
    if ((f->merge_sin = buffer_f32(msin->data, msin->count)) < 0) goto fail;
    if ((f->merge_cos = buffer_f32(mcos->data, mcos->count)) < 0) goto fail;
    {
        /* both, one after the other, for the pass that applies them together */
        float *sincos = malloc((msin->count + mcos->count) * sizeof(float));
        if (!sincos) { snprintf(last_error, sizeof last_error, "out of memory"); goto fail; }
        memcpy(sincos, msin->data, msin->count * sizeof(float));
        memcpy(sincos + msin->count, mcos->data, mcos->count * sizeof(float));
        f->merge_sincos = buffer_f32(sincos, msin->count + mcos->count);
        free(sincos);
        if (f->merge_sincos < 0) goto fail;
    }
    /* the head is 32 -> 4 in a (32, 16) matrix padded to the tile: gain over the first
     * sixteen rows, the convolution over the second, the first four columns the head */

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    if (gain->count != 64 || oconv->count != 64) { sprintf_s(last_error, sizeof last_error, "head weights are not (16, 4)"); goto fail; }
#else
    if (gain->count != 64 || oconv->count != 64) { snprintf(last_error, sizeof last_error, "head weights are not (16, 4)"); goto fail; }
#endif

    float headm[32 * 16] = { 0 };
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 4; c++) {
            headm[r * 16 + c] = gain->data[r * 4 + c];
            headm[(16 + r) * 16 + c] = oconv->data[r * 4 + c];
        }
    if ((f->head = buffer_f16(headm, 32 * 16)) < 0) goto fail;
    return f;
fail:
    nr_frame_close(f);
    return NULL;
}

void nr_frame_close(nr_frame *f)
{
    if (!f) return;
    release_extent(f);
    int *ids[] = { &f->adapter, &f->merge_sin, &f->merge_cos, &f->head,
                   &f->bottleneck.weight0, &f->decoder_input.weight0, &f->decoder_input.sine };
    for (size_t i = 0; i < sizeof ids / sizeof *ids; i++) if (*ids[i] >= 0) X.buf_destroy(*ids[i]);
    for (int i = 0; i < 71; i++) {
        struct block_w *b = &f->blocks[i];
        if (b->loaded) {
            int *bid[] = { &b->qkv, &b->out, &b->bias, &b->scale, &b->attn_cos, &b->ffn_cos, &b->expand,
                           &b->branch, &b->ffn_out, &b->first, &b->project, &b->weight3, &b->ffn_proj };
            for (size_t k = 0; k < sizeof bid / sizeof *bid; k++) if (*bid[k] >= 0) X.buf_destroy(*bid[k]);
        }
        if (f->edges[i].loaded) {
            X.buf_destroy(f->edges[i].weight0);
            if (f->edges[i].sine >= 0) X.buf_destroy(f->edges[i].sine);
        }
    }
    weights_free(&f->w);
    free(f->features_host); free(f->head_host); free(f->noise); free(f->rows); free(f->cols);
    free(f->scratch_a); free(f->scratch_b);
    free(f);
}

const char *nr_frame_device(nr_frame *f) { (void)f; return X.device ? X.device() : "not opened"; }
const char *nr_frame_gemm_path(nr_frame *f) { (void)f; return X.path ? X.path() : "not opened"; }

int nr_frame_features_masked(nr_frame *f, const float *colour, int height, int width, const float *history,
                             const float *control_mask, const nr_frame_params *params, float *features)
{
    nr_frame_params d;
    if (!params) { nr_frame_defaults(&d); params = &d; }
    if (height <= 0 || width <= 0 || !colour || !features) FAILF("features need a colour image and an output");
    if (prepare_extent(f, aligned_extent(height), aligned_extent(width))) return -1;
    return features_into(f, colour, height, width, history, control_mask, params, features);
}

int nr_frame_features(nr_frame *f, const float *colour, int height, int width, const float *history,
                      const nr_frame_params *params, float *features)
{
    return nr_frame_features_masked(f, colour, height, width, history, NULL, params, features);
}

int nr_frame_compose(nr_frame *f, const float *head, const float *colour, int height, int width,
                     const float *history, const float *previous, const float *control_mask,
                     const nr_frame_params *params, float *output)
{
    nr_frame_params d;
    if (!params) { nr_frame_defaults(&d); params = &d; }
    if (height <= 0 || width <= 0 || !head || !colour || !output) FAILF("compose needs a head, a colour image and an output");
    return compose(f, head, (ptrdiff_t)width * 4, 4, colour, height, width, history, previous, control_mask,
                   params, output);
}

int nr_frame_run_features(nr_frame *f, const float *features, int network_height, int network_width, float *head)
{
    if (!features || !head) FAILF("run_features needs features and a head");
    if (prepare_extent(f, network_height, network_width)) return -1;
    const float *wide = run_graph(f, features);
    if (!wide) return -1;
    size_t pixels = (size_t)network_height * network_width;
    for (size_t p = 0; p < pixels; p++) memcpy(head + p * 4, wide + p * f->head_stride, 4 * sizeof(float));
    return 0;
}

int nr_frame_update(nr_frame *f, const float *colour, int height, int width, const float *history,
                    const float *previous, const nr_frame_params *params, float *output, float *head_out)
{
    return nr_frame_update_masked(f, colour, height, width, history, previous, NULL, params, output, head_out);
}

int nr_frame_update_masked(nr_frame *f, const float *colour, int height, int width, const float *history,
                           const float *previous, const float *control_mask, const nr_frame_params *params,
                           float *output, float *head_out)
{
    nr_frame_params d;
    if (!params) { nr_frame_defaults(&d); params = &d; }
    if (height <= 0 || width <= 0 || !colour || !output) FAILF("update needs a colour image and an output");
    if (prepare_extent(f, aligned_extent(height), aligned_extent(width))) return -1;
    if (features_into(f, colour, height, width, history, control_mask, params, f->features_host)) return -1;
    const float *wide = run_graph(f, f->features_host);
    if (!wide) return -1;
    ptrdiff_t hx = f->head_stride, hy = (ptrdiff_t)f->width * hx;
    if (head_out)
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++)
                memcpy(head_out + ((size_t)y * width + x) * 4, wide + (size_t)y * hy + (size_t)x * hx, 4 * sizeof(float));
    return compose(f, wide, hy, hx, colour, height, width, history, previous, control_mask, params, output);
}
