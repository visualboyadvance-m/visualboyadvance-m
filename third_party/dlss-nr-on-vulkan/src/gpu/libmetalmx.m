/*
 * libmetalmx — libxmx on Metal: the same entry points, the same contract, driving Apple
 * silicon's own API instead of Vulkan through MoltenVK.
 *
 * Every `xmx_*` symbol in `xmx.h` is here, so `nr_frame.c`, `xmx.py` and `xmxres.py` bind
 * this library exactly as they bind libxmx (`NR_GPU_BACKEND=metal` makes them). Shader
 * names stay the SPIR-V names: `xmx_init("gemm_coopmat.spv")` runs the Metal kernel
 * `gemm_coopmat` from the metallib compiled into this binary (`nr_metallib.h`, bin2c over
 * `nr_shaders.metallib`); a path is reduced to its base name and looked up the same way,
 * and `XMX_METALLIB=/path/to.metallib` loads a file instead of the embedded library.
 *
 * What maps onto what:
 *
 *   VkBuffer + device address     MTLBuffer, its GPU address in the push block. The address
 *                                 is not read from `gpuAddress` (macOS 13): an
 *                                 `MTLArgumentEncoder` over one pointer argument (Metal 2,
 *                                 macOS 10.13) writes the same 64-bit value into an argument
 *                                 buffer, and `addr_ptr()` reads it back. The graph's
 *                                 buffers are `MTLStorageModeShared` on unified memory (so
 *                                 `xmx_buf_ptr` is `contents`, as HOST_CACHED was), and
 *                                 `MTLStorageModePrivate` under XMX_STAGING=1 or on a card
 *                                 without unified memory, reached by blit copies.
 *   VK_KHR_cooperative_matrix     `simdgroup_matrix` (`MTLGPUFamilyApple7` and up):
 *                                 `xmx_coopmat()` says so, and `XMX_PORTABLE=1` still picks
 *                                 the multiply-add kernels.
 *   specialization constant 0     function constant 0 (`MTLFunctionConstantValues`).
 *   push constants                `setBytes` of the same 64-byte struct at buffer index 0.
 *   vkCmdPipelineBarrier          `memoryBarrierWithScope:MTLBarrierScopeBuffers` inside a
 *                                 concurrent-dispatch encoder, one per pass unless
 *                                 `xmx_sync(0)` suppresses it — the same places.
 *   vkCmdCopyBuffer               a blit encoder between two compute encoders.
 *   a recorded command buffer     a host-side op list. A MTLCommandBuffer cannot be
 *                                 submitted twice, so a graph is the recorded list and
 *                                 `xmx_graph_run` encodes it again into a fresh command
 *                                 buffer each time (~1 ms for a frame's 1700 passes).
 *   vkCmdWriteTimestamp           counter sampling at encoder boundaries: with profiling
 *                                 on, every pass gets its own encoder with a timestamp pair.
 *   xmx_adopt                     refused: there is no Vulkan device here to adopt.
 *
 * Numerics are the shaders' business and they are compiled with `-fno-fast-math
 * -ffp-contract=off` (see `metal/nr_metal.h`); nothing here changes a value.
 *
 * Build (Apple only): clang -fobjc-arc -O2 -shared -fPIC -o libmetalmx.dylib libmetalmx.m
 *                     -framework Metal -framework Foundation
 */
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "xmx.h"
#ifdef NR_EMBEDDED_METALLIB
#include "nr_metallib.h"                      /* const unsigned char nr_metallib[]; bin2c */
#endif

#define FAIL(msg, r) do { snprintf(g.err, sizeof g.err, "%s (%d)", msg, (int)(r)); return -1; } while (0)
#define FAILNS(msg, error) do { snprintf(g.err, sizeof g.err, "%s: %s", msg, \
	(error) ? [[(error) localizedDescription] UTF8String] : "unknown error"); return -1; } while (0)

/* The fixed Metal objects live in globals of their own type: ARC manages those. Objects
 * kept in C tables — buffers, pipelines — are bridged to `void *` with a retain of their
 * own, and released where the table entry dies. */
static id<MTLDevice> dev;
static id<MTLCommandQueue> queue;
/* Orders one encoder after the previous within a command buffer. The graph's buffers are
 * untracked, so Metal would otherwise be free to overlap a blit with the compute encoder
 * that wrote its source; every encoder here updates the fence on its way out and the next
 * waits on it on its way in. */
static id<MTLFence> fence;
static id<MTLLibrary> library;
/* The address of a buffer, without `gpuAddress`: encode it as the one pointer argument of
 * this encoder into `addr_scratch` and read the 8 bytes back. Argument buffers hold real
 * device addresses, the same value `gpuAddress` reports, on every macOS with Metal 2. */
static id<MTLArgumentEncoder> addr_encoder;
static id<MTLBuffer> addr_scratch;
/* Timestamp sample buffers, one pair of samples per profiled pass. A sample buffer is
 * capped at 32 KB — 4096 timestamps, 2048 passes — so a frame's 1700-odd passes need a
 * second one and the limit below needs four. */
#define SAMPLE_PAIRS 2048
#define SAMPLE_BUFFERS 4
static id<MTLCounterSampleBuffer> samples[SAMPLE_BUFFERS];
static unsigned sample_buffers;                /* how many are allocated */

struct buf { void *b; void *p; uint64_t cap; };

struct push {
	uint64_t a, b, c, d;
	uint32_t m, n, k, batch, sa, sb, sc, flags;
	float p0, p1, p2, p3;
	uint32_t lda, ldb, ldc, spare;
};

struct desc_push { uint32_t M, N, K, sa, sb, sc, bt; };

static struct {
	void *pipe, *pipeb;                            /* the descriptor path */
	void *rgemm, *rtiled, *rstaged, *runary, *rrow, *rhistory;
	char *rpaths[5];
	unsigned specialize;
	unsigned tiling, tilem, tilen;
	int syncing;
	unsigned staging;
	int recording, recorded, rready;
	unsigned prof, prof_n; double ns_per_tick;
	char name[256]; char err[512]; char memory[256];
	int ready, lost, discrete, unmapped, coopmat, portable;
	struct buf A, B, C, stage;
} g;

#define MAX_SPECIALIZED 256
static struct { unsigned family, flags; void *pipeline; } specialized[MAX_SPECIALIZED];
static unsigned specialized_count;

/* A pass's kind is `family * 32 + subkind`, as in libxmx. */
#define MAX_STAMPS 8192
#define PROF_KINDS 256
enum { PK_GEMM = 0, PK_TILED, PK_STAGED, PK_UNARY, PK_ROW, PK_HISTORY, PK_COPY, PK_START = 7 };
static double prof_ms[PROF_KINDS];
static unsigned prof_hits[PROF_KINDS];

/* Device-resident buffers. */
#define MAX_RBUF 8192
struct rbuf { void *b; void *p; uint64_t addr, size; int live, mapped; };
static struct rbuf rbufs[MAX_RBUF];

/* What a recording is: the passes, kept host-side, encoded at submit. */
enum { OP_DISPATCH, OP_COPY, OP_BARRIER };
struct op {
	unsigned char kind, stamp;
	void *pipeline;                                /* unretained: the tables own it */
	struct push p;
	unsigned gx, gy, gz, tg;
	int src, dst; uint64_t so, to, bytes;
};
struct oplist { struct op *ops; unsigned n, cap; int passes, live; };
static struct oplist rec;
#define MAX_GRAPHS 128
static struct oplist graphs[MAX_GRAPHS];

static int op_push(struct oplist *l, const struct op *op)
{
	if (l->n == l->cap) {
		unsigned cap = l->cap ? l->cap * 2 : 1024;
		struct op *grown = realloc(l->ops, cap * sizeof *grown);
		if (!grown) return -1;
		l->ops = grown; l->cap = cap;
	}
	l->ops[l->n++] = *op;
	return 0;
}

static void op_free(struct oplist *l) { free(l->ops); memset(l, 0, sizeof *l); }

/* -- queries ------------------------------------------------------------ */

const char *xmx_error(void) { return g.err; }
int xmx_device_lost(void) { return g.lost; }
const char *xmx_device(void) { return g.name; }
const char *xmx_memory(void)
{
	if (!dev) return "not initialised";
	if (!g.memory[0])
		snprintf(g.memory, sizeof g.memory, "%s",
			 g.unmapped ? "private storage, the host reaches it by copies (MTLStorageModePrivate"
				      ": staging forced, or no unified memory)"
			 : g.discrete ? "shared storage on a card without unified memory: system memory"
			 : "unified memory (MTLStorageModeShared, one pool)");
	return g.memory;
}
int xmx_coopmat(void) { return dev ? g.coopmat : -1; }
int xmx_portable(void) { return dev ? g.portable : -1; }
const char *xmx_path(void)
{
	if (!dev) return "not opened";
	if (!g.portable) return "simdgroup matrix (Metal simdgroup_multiply_accumulate, fp16 x fp16 -> fp32)";
	return g.coopmat ? "portable multiply-add (XMX_PORTABLE=1; the device has simdgroup matrices)"
			 : "portable multiply-add (the device has no simdgroup matrix support)";
}
int xmx_staging_mode(void) { return g.unmapped; }

/* -- shaders --------------------------------------------------------------- */

/* The kernels the metallib carries, under the SPIR-V names every caller uses. */
static const char *const kernel_names[] = {
	"gemm_resident", "gemm_tiled", "gemm_staged", "resident", "attention", "attention_ab",
	"history", "gemm_coopmat", "gemm_batched", "gemm_f16acc", "gemm_portable",
	"gemm_portable_tiled", "gemm_portable_desc", "gemm_portable_batched",
};

/* `gemm_coopmat.spv`, `/some/dir/attention_ab.spv`, `resident` -> the kernel's name. */
static void kernel_name(const char *spv_path, char *out, size_t cap)
{
	const char *base = spv_path;
	for (const char *q = spv_path; *q; q++)
		if (*q == '/' || *q == '\\') base = q + 1;
	size_t n = strlen(base);
	const char *dot = strrchr(base, '.');
	if (dot && dot > base) n = (size_t)(dot - base);
	if (n >= cap) n = cap - 1;
	memcpy(out, base, n);
	out[n] = 0;
}

size_t xmx_embedded_shader(const char *name)
{
	if (!name) return 0;
	char stem[128];
	kernel_name(name, stem, sizeof stem);
	for (size_t i = 0; i < sizeof kernel_names / sizeof *kernel_names; i++)
		if (!strcmp(kernel_names[i], stem)) {
#ifdef NR_EMBEDDED_METALLIB
			return sizeof nr_metallib;
#else
			return getenv("XMX_METALLIB") ? 1 : 0;
#endif
		}
	return 0;
}

static int load_library(void)
{
	if (library) return 0;
	NSError *error = nil;
	const char *forced = getenv("XMX_METALLIB");
	if (forced && *forced) {
		library = [dev newLibraryWithURL:[NSURL fileURLWithPath:@(forced)] error:&error];
		if (!library) FAILNS("cannot load XMX_METALLIB", error);
		return 0;
	}
#ifdef NR_EMBEDDED_METALLIB
	dispatch_data_t data = dispatch_data_create(nr_metallib, sizeof nr_metallib, NULL,
						    DISPATCH_DATA_DESTRUCTOR_DEFAULT);
	library = [dev newLibraryWithData:data error:&error];
	if (!library) FAILNS("cannot load the embedded metallib", error);
	return 0;
#else
	FAIL("no metallib compiled into libmetalmx and XMX_METALLIB is not set", 0);
#endif
}

static int build_pipeline_spec(const char *spv_path, const unsigned *flags, void **out)
{
	if (load_library()) return -1;
	char stem[128];
	kernel_name(spv_path, stem, sizeof stem);
	NSError *error = nil;
	/* A kernel that reads a function constant is created through this call even with no
	 * constant set (Metal refuses the plain lookup): an empty table leaves
	 * `is_function_constant_defined` false and the flags come from the push block. */
	MTLFunctionConstantValues *values = [MTLFunctionConstantValues new];
	unsigned value = flags ? *flags : 0;
	if (flags) [values setConstantValue:&value type:MTLDataTypeUInt atIndex:0];
	id<MTLFunction> fn = [library newFunctionWithName:@(stem) constantValues:values error:&error];
	if (!fn) {
		snprintf(g.err, sizeof g.err, "no kernel '%s' in the metallib (from '%s')%s%s", stem, spv_path,
			 error ? ": " : "", error ? [[error localizedDescription] UTF8String] : "");
		return -1;
	}
	id<MTLComputePipelineState> state = [dev newComputePipelineStateWithFunction:fn error:&error];
	if (!state) FAILNS("pipeline", error);
	/* Every kernel here is written for a 32-wide simdgroup, as the GLSL is for a 32-wide
	 * subgroup; the row pass's staging and the GEMM lane mapping both assume it. */
	if (state.threadExecutionWidth != 32)
		FAIL("this device's simdgroup is not 32 wide, which the kernels assume", (int)state.threadExecutionWidth);
	*out = (void *)CFBridgingRetain(state);
	return 0;
}

static int build_pipeline(const char *path, void **out) { return build_pipeline_spec(path, NULL, out); }

static void release_pipeline(void **p)
{
	if (*p) { CFBridgingRelease(*p); *p = NULL; }
}

static unsigned block_size(const char *name, unsigned fallback)
{
	const char *value = getenv(name);
	if (!value) return fallback;
	int n = atoi(value);
	if (n > 0) return (unsigned)n;
	fprintf(stderr, "libmetalmx: %s=%s is not a positive block size; using %u\n", name, value, fallback);
	return fallback;
}

static int resident_pipeline(unsigned family, unsigned flags, void *fallback, void **out)
{
	unsigned mask = family < 3 ? 1u : (family == 3 ? 2u : 4u);
	*out = fallback;
	if (!(g.specialize & mask)) return 0;
	for (unsigned i = 0; i < specialized_count; i++)
		if (specialized[i].family == family && specialized[i].flags == flags) {
			*out = specialized[i].pipeline;
			return 0;
		}
	if (specialized_count == MAX_SPECIALIZED) return 0;
	if (build_pipeline_spec(g.rpaths[family], &flags, out)) return -1;
	specialized[specialized_count].family = family;
	specialized[specialized_count].flags = flags;
	specialized[specialized_count++].pipeline = *out;
	return 0;
}

int xmx_specialize(unsigned mask)
{
	if (g.recording) FAIL("cannot switch specialization during recording", 0);
	if (mask > 7) FAIL("specialization mask must be in 0..7", 0);
	g.specialize = mask;
	return 0;
}

unsigned xmx_specialized_count(void) { return specialized_count; }
unsigned xmx_specialization(void) { return g.specialize; }

/* -- the device ------------------------------------------------------------ */

static int want_unmapped(void)
{
	const char *forced = getenv("XMX_STAGING");
	if (forced && *forced) return atoi(forced) != 0;
	return g.discrete;
}

int xmx_adopt(void *inst, void *pd, void *device, void *q, unsigned qi, int coopmat, void *gipa,
	      void (*lock)(void *), void (*unlock)(void *), void *ctx)
{
	(void)inst; (void)pd; (void)device; (void)q; (void)qi; (void)coopmat; (void)gipa;
	(void)lock; (void)unlock; (void)ctx;
	FAIL("libmetalmx drives Metal directly: there is no Vulkan device to adopt", 0);
}

int xmx_adopted(void) { return dev ? 0 : -1; }

int xmx_open(void)
{
	if (dev) return 0;
	@autoreleasepool {
		dev = MTLCreateSystemDefaultDevice();
		if (!dev) FAIL("no Metal device", 0);
		snprintf(g.name, sizeof g.name, "%s", [[dev name] UTF8String]);
		g.discrete = ![dev hasUnifiedMemory];
		/* simdgroup_matrix is an Apple7 (A14 / M1) feature; a Mac with another vendor's
		 * card takes the multiply-add kernels. */
		g.coopmat = [dev supportsFamily:MTLGPUFamilyApple7] ? 1 : 0;
		const char *forced = getenv("XMX_PORTABLE");
		g.portable = !g.coopmat || (forced && *forced && atoi(forced) != 0);
		queue = [dev newCommandQueue];
		if (!queue) { dev = nil; FAIL("no command queue", 0); }
		fence = [dev newFence];
	}
	return 0;
}

static void free_buf(struct buf *b)
{
	if (b->b) CFBridgingRelease(b->b);
	memset(b, 0, sizeof *b);
}

void xmx_close(void)
{
	if (!dev) return;
	for (int i = 0; i < MAX_RBUF; i++) xmx_buf_destroy(i);
	for (int i = 0; i < MAX_GRAPHS; i++) xmx_graph_destroy(i);
	op_free(&rec);
	for (unsigned i = 0; i < specialized_count; i++) release_pipeline(&specialized[i].pipeline);
	specialized_count = 0;
	void **pipes[] = { &g.rgemm, &g.rtiled, &g.rstaged, &g.runary, &g.rrow, &g.rhistory, &g.pipe, &g.pipeb };
	for (size_t i = 0; i < sizeof pipes / sizeof *pipes; i++) release_pipeline(pipes[i]);
	for (int i = 0; i < 5; i++) free(g.rpaths[i]);
	struct buf *bufs[] = { &g.A, &g.B, &g.C, &g.stage };
	for (size_t i = 0; i < sizeof bufs / sizeof *bufs; i++) free_buf(bufs[i]);
	for (unsigned i = 0; i < SAMPLE_BUFFERS; i++) samples[i] = nil;
	sample_buffers = 0;
	library = nil;
	addr_encoder = nil;
	addr_scratch = nil;
	fence = nil;
	queue = nil;
	dev = nil;
	memset(&g, 0, sizeof g);
	memset(prof_ms, 0, sizeof prof_ms);
	memset(prof_hits, 0, sizeof prof_hits);
}

/* -- one-shot command buffers ------------------------------------------ */

/* Commit and wait. A failed command buffer is reported with Metal's own description; a
 * fault the device will not recover from on this queue is recorded as lost. */
static int finish(id<MTLCommandBuffer> cb, const char *what)
{
	[cb commit];
	[cb waitUntilCompleted];
	if (cb.status == MTLCommandBufferStatusError) {
		NSError *error = cb.error;
		if (error && (error.code == MTLCommandBufferErrorInternal || error.code == MTLCommandBufferErrorTimeout
			      || error.code == MTLCommandBufferErrorPageFault))
			g.lost = 1;
		snprintf(g.err, sizeof g.err, "%s: %s", what,
			 error ? [[error localizedDescription] UTF8String] : "command buffer failed");
		return -1;
	}
	return 0;
}

/* The descriptor path's growable shared buffers. */
static int ensure(struct buf *b, uint64_t size)
{
	if (b->cap >= size) return 0;
	free_buf(b);
	id<MTLBuffer> buffer = [dev newBufferWithLength:(NSUInteger)size options:MTLResourceStorageModeShared];
	if (!buffer) FAIL("newBufferWithLength", 0);
	b->b = (void *)CFBridgingRetain(buffer);
	b->p = [buffer contents];
	b->cap = size;
	return 0;
}

int xmx_init(const char *spv_path)
{
	if (g.ready) return 0;
	if (xmx_open()) return -1;
	@autoreleasepool {
		if (build_pipeline(spv_path, &g.pipe)) return -1;
	}
	g.ready = 1;
	return 0;
}

int xmx_reserve(unsigned M, unsigned N, unsigned K, void **pa, void **pb, void **pc)
{
	if (!g.ready) FAIL("not initialised", 0);
	if (ensure(&g.A, (uint64_t)M * K * 2) || ensure(&g.B, (uint64_t)K * N * 2) || ensure(&g.C, (uint64_t)M * N * 4))
		return -1;
	if (pa) *pa = g.A.p;
	if (pb) *pb = g.B.p;
	if (pc) *pc = g.C.p;
	return 0;
}

int xmx_reserve_bytes(unsigned long long a, unsigned long long b, unsigned long long c,
		      void **pa, void **pb, void **pc)
{
	if (!g.ready) FAIL("not initialised", 0);
	if (ensure(&g.A, a) || ensure(&g.B, b) || ensure(&g.C, c)) return -1;
	if (pa) *pa = g.A.p;
	if (pb) *pb = g.B.p;
	if (pc) *pc = g.C.p;
	return 0;
}

/* One dispatch of a descriptor-bound kernel, `iters` times with a barrier between. */
static int run_desc(void *pipeline, const struct desc_push *push, unsigned gx, unsigned gy, unsigned gz,
		    unsigned iters)
{
	@autoreleasepool {
		id<MTLCommandBuffer> cb = [queue commandBuffer];
		if (!cb) FAIL("command buffer", 0);
		id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoderWithDispatchType:MTLDispatchTypeConcurrent];
		[enc setComputePipelineState:(__bridge id<MTLComputePipelineState>)pipeline];
		[enc setBuffer:(__bridge id<MTLBuffer>)g.A.b offset:0 atIndex:0];
		[enc setBuffer:(__bridge id<MTLBuffer>)g.B.b offset:0 atIndex:1];
		[enc setBuffer:(__bridge id<MTLBuffer>)g.C.b offset:0 atIndex:2];
		[enc setBytes:push length:sizeof *push atIndex:3];
		for (unsigned i = 0; i < (iters ? iters : 1); i++) {
			[enc dispatchThreadgroups:MTLSizeMake(gx, gy, gz) threadsPerThreadgroup:MTLSizeMake(32, 1, 1)];
			if (i + 1 < iters) [enc memoryBarrierWithScope:MTLBarrierScopeBuffers];
		}
		[enc endEncoding];
		return finish(cb, "gemm");
	}
}

int xmx_gemm(unsigned M, unsigned N, unsigned K, const void *a, const void *b, void *c, unsigned iters)
{
	if (!g.ready) FAIL("not initialised", 0);
	uint64_t sa = (uint64_t)M * K * 2, sb = (uint64_t)K * N * 2, sc = (uint64_t)M * N * 4;
	if (ensure(&g.A, sa) || ensure(&g.B, sb) || ensure(&g.C, sc)) return -1;
	if (a) memcpy(g.A.p, a, sa);
	if (b) memcpy(g.B.p, b, sb);
	struct desc_push push = { M, N, K, 0, 0, 0, 0 };
	if (run_desc(g.pipe, &push, N / 16, M / 8, 1, iters)) return -1;
	if (c) memcpy(c, g.C.p, sc);
	return 0;
}

int xmx_init_batched(const char *spv_path)
{
	if (!g.ready) FAIL("not initialised", 0);
	if (g.pipeb) return 0;
	@autoreleasepool {
		return build_pipeline(spv_path, &g.pipeb);
	}
}

int xmx_gemm_batched(unsigned M, unsigned N, unsigned K, unsigned batch,
		     unsigned sa, unsigned sb, unsigned sc, unsigned bt)
{
	if (!g.pipeb) FAIL("batched pipeline not built", 0);
	struct desc_push push = { M, N, K, sa, sb, sc, bt };
	return run_desc(g.pipeb, &push, N / 16, M / 8, batch, 1);
}

/* -- the resident runtime ------------------------------------------------- */

int xmx_res_init(const char *gemm_spv, const char *unary_spv, const char *row_spv,
		 const char *history_spv, const char *tiled_spv, const char *staged_spv)
{
	if (!g.ready) FAIL("not initialised", 0);
	if (g.rready) return 0;
	const char *paths[] = { gemm_spv, tiled_spv, staged_spv, unary_spv, row_spv };
	for (unsigned i = 0; i < 5; i++) {
		g.rpaths[i] = strdup(paths[i]);
		if (!g.rpaths[i]) FAIL("pipeline path allocation", 0);
	}
	const char *spec = getenv("XMX_SPECIALIZE");
	g.specialize = spec ? (unsigned)atoi(spec) : 7;
	if (g.specialize > 7) FAIL("XMX_SPECIALIZE must be in 0..7", 0);
	@autoreleasepool {
		if (build_pipeline(gemm_spv, &g.rgemm) || build_pipeline(unary_spv, &g.runary)
		    || build_pipeline(row_spv, &g.rrow) || build_pipeline(history_spv, &g.rhistory)
		    || build_pipeline(tiled_spv, &g.rtiled) || build_pipeline(staged_spv, &g.rstaged))
			return -1;
	}
	const char *tile = getenv("XMX_TILE_K");
	g.tiling = tile ? atoi(tile) : 1;
	g.tilem = block_size("XMX_TILE_M", 16);
	g.tilen = block_size("XMX_TILE_N", 32);
	const char *sk = getenv("XMX_STAGE_K");
	g.staging = sk ? (unsigned)atoi(sk) : 128;
	g.unmapped = want_unmapped();
	g.rready = 1;
	return 0;
}

/* The GPU address of `buffer`, the way a macOS 11 SDK allows it. `[MTLBuffer gpuAddress]`
 * needs macOS 13; an argument encoder built from one `MTLDataTypePointer` descriptor has
 * existed since Metal 2 and stores exactly that address at offset 0 of the argument buffer
 * it encodes into. The encoder and its 8-byte shared scratch buffer are made once. */
static int buffer_address(id<MTLBuffer> buffer, uint64_t *out)
{
	if (!addr_encoder) {
		MTLArgumentDescriptor *arg = [MTLArgumentDescriptor argumentDescriptor];
		arg.dataType = MTLDataTypePointer;
		arg.index = 0;                         /* access left at its default: only the
							  address is wanted, nothing is dispatched */
		addr_encoder = [dev newArgumentEncoderWithArguments:@[arg]];
		if (!addr_encoder) FAIL("newArgumentEncoderWithArguments", 0);
		addr_scratch = [dev newBufferWithLength:addr_encoder.encodedLength
						options:MTLResourceStorageModeShared];
		if (!addr_scratch) { addr_encoder = nil; FAIL("newBufferWithLength (address scratch)", 0); }
	}
	[addr_encoder setArgumentBuffer:addr_scratch offset:0];
	[addr_encoder setBuffer:buffer offset:0 atIndex:0];
	uint64_t addr;
	memcpy(&addr, [addr_scratch contents], sizeof addr);
	if (!addr) FAIL("argument encoder produced a null address", 0);
	*out = addr;
	return 0;
}

/* `kind`: 0 the graph's own buffers, 1 a buffer the host reads back, 2 one it writes.
 * Kinds 1 and 2 are always shared; kind 0 is private when the device wants it so. Hazard
 * tracking is off for the graph's buffers: the recording carries its own barriers, as the
 * Vulkan one does, and Metal's tracking would only add work. */
int xmx_buf_create_kind(unsigned long long bytes, int kind)
{
	if (!g.rready) FAIL("resident runtime not initialised", 0);
	int slot = -1;
	for (int i = 0; i < MAX_RBUF; i++)
		if (!rbufs[i].live) { slot = i; break; }
	if (slot < 0) FAIL("out of buffer slots", 0);
	int unmapped = kind == 0 && g.unmapped;
	MTLResourceOptions options = (unmapped ? MTLResourceStorageModePrivate : MTLResourceStorageModeShared)
				   | MTLResourceHazardTrackingModeUntracked;
	@autoreleasepool {
		id<MTLBuffer> buffer = [dev newBufferWithLength:(NSUInteger)(bytes ? bytes : 4) options:options];
		if (!buffer) FAIL("newBufferWithLength (resident)", 0);
		struct rbuf *rb = &rbufs[slot];
		rb->b = (void *)CFBridgingRetain(buffer);
		rb->p = unmapped ? NULL : [buffer contents];
		rb->mapped = !unmapped;
		rb->size = bytes ? bytes : 4;
		rb->live = 1;
		if (buffer_address(buffer, &rb->addr)) { xmx_buf_destroy(slot); return -1; }
	}
	return slot;
}

int xmx_buf_create(unsigned long long bytes) { return xmx_buf_create_kind(bytes, 0); }

int xmx_buf_host_visible(int id)
{
	return (id >= 0 && id < MAX_RBUF && rbufs[id].live && rbufs[id].mapped) ? 1 : 0;
}

static int ensure_stage(uint64_t size)
{
	if (g.stage.cap >= size) return 0;
	return ensure(&g.stage, size);
}

/* A blit of its own, now, on the queue: the transfers that happen once rather than every
 * frame — the weights, a test's readback, a clear. */
static int blit_now(void (^body)(id<MTLBlitCommandEncoder> blit), const char *what)
{
	@autoreleasepool {
		id<MTLCommandBuffer> cb = [queue commandBuffer];
		if (!cb) FAIL("command buffer (transfer)", 0);
		id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
		body(blit);
		[blit endEncoding];
		return finish(cb, what);
	}
}

static int stage_copy(int to_device, int which, const void *src, void *dst,
		      unsigned long long offset, unsigned long long bytes)
{
	if (which < 0 || which >= MAX_RBUF || !rbufs[which].live) FAIL("buffer is not live", 0);
	if (offset + bytes > rbufs[which].size) FAIL("transfer runs past the buffer", 0);
	if (!bytes) return 0;
	if (rbufs[which].mapped) {
		unsigned char *p = (unsigned char *)rbufs[which].p + offset;
		if (to_device) memcpy(p, src, (size_t)bytes);
		else memcpy(dst, p, (size_t)bytes);
		return 0;
	}
	if (ensure_stage(bytes)) return -1;
	if (to_device) memcpy(g.stage.p, src, (size_t)bytes);
	id<MTLBuffer> stage = (__bridge id<MTLBuffer>)g.stage.b, target = (__bridge id<MTLBuffer>)rbufs[which].b;
	int bad = blit_now(^(id<MTLBlitCommandEncoder> blit) {
		if (to_device) [blit copyFromBuffer:stage sourceOffset:0 toBuffer:target
				   destinationOffset:(NSUInteger)offset size:(NSUInteger)bytes];
		else [blit copyFromBuffer:target sourceOffset:(NSUInteger)offset toBuffer:stage
			     destinationOffset:0 size:(NSUInteger)bytes];
	}, "transfer");
	if (bad) return -1;
	if (!to_device) memcpy(dst, g.stage.p, (size_t)bytes);
	return 0;
}

int xmx_buf_zero(int which)
{
	if (which < 0 || which >= MAX_RBUF || !rbufs[which].live) FAIL("buffer is not live", 0);
	if (rbufs[which].mapped) {
		memset(rbufs[which].p, 0, (size_t)rbufs[which].size);
		return 0;
	}
	/* Now, never into the recording: a recorded fill would run again on every replay and
	 * wipe a weight buffer's padding after the weights had been uploaded (libxmx.c). */
	id<MTLBuffer> target = (__bridge id<MTLBuffer>)rbufs[which].b;
	NSUInteger size = (NSUInteger)rbufs[which].size;
	return blit_now(^(id<MTLBlitCommandEncoder> blit) {
		[blit fillBuffer:target range:NSMakeRange(0, size) value:0];
	}, "fill");
}

int xmx_buf_upload(int id, const void *src, unsigned long long offset, unsigned long long bytes)
{
	if (!src) FAIL("upload source is null", 0);
	return stage_copy(1, id, src, NULL, offset, bytes);
}

int xmx_buf_download(int id, void *dst, unsigned long long offset, unsigned long long bytes)
{
	if (!dst) FAIL("download destination is null", 0);
	return stage_copy(0, id, NULL, dst, offset, bytes);
}

unsigned long long xmx_buf_total_bytes(void)
{
	unsigned long long total = 0;
	for (unsigned i = 0; i < MAX_RBUF; i++) if (rbufs[i].live) total += rbufs[i].size;
	return total;
}

void *xmx_buf_ptr(int id)
{
	return (id >= 0 && id < MAX_RBUF && rbufs[id].live) ? rbufs[id].p : NULL;
}

unsigned long long xmx_buf_bytes(int id)
{
	return (id >= 0 && id < MAX_RBUF && rbufs[id].live) ? rbufs[id].size : 0;
}

int xmx_buf_destroy(int id)
{
	if (id < 0 || id >= MAX_RBUF || !rbufs[id].live) return 0;
	CFBridgingRelease(rbufs[id].b);
	rbufs[id] = (struct rbuf){ 0 };
	return 0;
}

static uint64_t addr_of(int id)
{
	return (id >= 0 && id < MAX_RBUF && rbufs[id].live) ? rbufs[id].addr : 0;
}

/* -- recording -------------------------------------------------------------- */

int xmx_begin(void)
{
	if (!g.rready) FAIL("resident runtime not initialised", 0);
	if (g.recording) FAIL("already recording", 0);
	rec.n = 0;
	rec.passes = 0;
	g.recording = 1;
	g.recorded = 0;
	g.syncing = 1;
	return 0;
}

int xmx_abort(void)
{
	if (!g.recording) return 0;
	rec.n = 0;
	g.recording = 0;
	return 0;
}

/* One global barrier between passes, suppressed by `xmx_sync(0)` over a run of dispatches
 * known to be independent. */
static int barrier(void)
{
	if (!g.syncing) return 0;
	struct op op = { .kind = OP_BARRIER };
	if (op_push(&rec, &op)) FAIL("out of memory recording", 0);
	return 0;
}

int xmx_sync(int on)
{
	if (!g.recording) FAIL("not recording", 0);
	int was = g.syncing;
	g.syncing = on;
	if (on && !was) return barrier();
	return 0;
}

int xmx_rec_copy(int source, int target, unsigned long long bytes,
		 unsigned long long source_offset, unsigned long long target_offset)
{
	if (!g.recording) FAIL("not recording", 0);
	if (!addr_of(source) || !addr_of(target)) FAIL("copy buffer is not live", 0);
	struct rbuf *a = &rbufs[source], *b = &rbufs[target];
	if (!bytes || ((bytes | source_offset | target_offset) & 3) ||
	    source_offset > a->size || bytes > a->size - source_offset ||
	    target_offset > b->size || bytes > b->size - target_offset)
		FAIL("copy range must be aligned and within both buffers", 0);
	if (source == target && source_offset < target_offset + bytes &&
	    target_offset < source_offset + bytes) FAIL("overlapping copy", 0);
	struct op op = { .kind = OP_COPY, .stamp = PK_COPY * 32, .src = source, .dst = target,
			 .so = source_offset, .to = target_offset, .bytes = bytes };
	if (op_push(&rec, &op)) FAIL("out of memory recording", 0);
	g.recorded++;
	return 0;
}

static int dispatch(void *pipeline, const struct push *p, unsigned gx, unsigned gy, unsigned gz,
		    unsigned tg, unsigned family, unsigned subkind)
{
	struct op op = { .kind = OP_DISPATCH, .stamp = (unsigned char)(family * 32u + (subkind & 31u)),
			 .pipeline = pipeline, .p = *p, .gx = gx, .gy = gy, .gz = gz, .tg = tg };
	if (op_push(&rec, &op)) FAIL("out of memory recording", 0);
	if (barrier()) return -1;
	g.recorded++;
	return 0;
}

int xmx_rec_gemm(int a, int b, int c, unsigned M, unsigned N, unsigned K, unsigned batch,
		 unsigned sa, unsigned sb, unsigned sc, unsigned bt,
		 unsigned lda, unsigned ldb, unsigned ldc,
		 unsigned oa, unsigned ob, unsigned oc)
{
	if (!g.recording) FAIL("not recording", 0);
	struct push p = { .a = addr_of(a), .b = addr_of(b), .c = addr_of(c),
			  .m = M, .n = N, .k = K, .batch = batch,
			  .sa = sa, .sb = sb, .sc = sc, .flags = bt,
			  .lda = lda, .ldb = ldb, .ldc = ldc };
	if (!p.a || !p.b || !p.c) FAIL("gemm operand is not a live buffer", 0);
	p.a += (uint64_t)oa * 2; p.b += (uint64_t)ob * 2;
	p.c += (uint64_t)oc * ((bt & 0x1000u) ? 2 : 4);
	/* The same three-way choice as libxmx: the 64x32 staged kernel for deep, whole-block
	 * shapes; the 16x32 register block where both extents allow it; the 8x16 kernel for
	 * everything else. The staged kernel has no portable twin. */
	int staged = M % 64 == 0 && N % 32 == 0 && K % 32 == 0 && K >= g.staging;
	if (g.portable) staged = 0;
	int tiled = M % g.tilem == 0 && N % g.tilen == 0 && K >= g.tiling;
	void *pipeline;
	if (resident_pipeline(staged ? 2 : (tiled ? 1 : 0), bt,
			      staged ? g.rstaged : (tiled ? g.rtiled : g.rgemm), &pipeline)) return -1;
	unsigned gz = batch ? batch : 1;
	if (staged) return dispatch(pipeline, &p, N / 32, M / 64, gz, 128, PK_STAGED, bt);
	if (tiled)  return dispatch(pipeline, &p, N / g.tilen, M / g.tilem, gz, 32, PK_TILED, bt);
	return dispatch(pipeline, &p, (N + 15) / 16, (M + 7) / 8, gz, 32, PK_GEMM, bt);
}

int xmx_rec_unary(unsigned kind, int a, int b, int c, int d, unsigned n, unsigned channels,
		  float p0, unsigned batch, unsigned sa, unsigned sb, unsigned sc, unsigned k)
{
	if (!g.recording) FAIL("not recording", 0);
	struct push p = { .a = addr_of(a), .b = addr_of(b), .c = addr_of(c), .d = addr_of(d),
			  .m = n, .n = channels, .flags = kind, .p0 = p0,
			  .batch = batch, .sa = sa, .sb = sb, .sc = sc, .k = k };
	if (!p.a || !p.c) FAIL("unary operand is not a live buffer", 0);
	void *pipeline;
	if (resident_pipeline(3, kind, g.runary, &pipeline)) return -1;
	return dispatch(pipeline, &p, (n + 255) / 256, 1, 1, 256, PK_UNARY, kind);
}

int xmx_rec_row(unsigned kind, int a, int b, int c, int d, unsigned rows, unsigned width,
		unsigned heads, unsigned scaled, unsigned stride, float cap)
{
	if (!g.recording) FAIL("not recording", 0);
	struct push p = { .a = addr_of(a), .b = addr_of(b), .c = addr_of(c), .d = addr_of(d),
			  .m = rows, .n = width, .k = scaled, .batch = heads, .flags = kind,
			  .sa = stride, .p0 = cap };
	if (!p.a || !p.c) FAIL("row operand is not a live buffer", 0);
	void *pipeline;
	if (resident_pipeline(4, kind, g.rrow, &pipeline)) return -1;
	return dispatch(pipeline, &p, (rows + 31) / 32, 1, 1, 32, PK_ROW, kind);
}

int xmx_rec_history(int history, int motion, int out, unsigned pixels, unsigned channels,
		    unsigned height, unsigned width, unsigned absolute)
{
	if (!g.recording) FAIL("not recording", 0);
	struct push p = { .a = addr_of(history), .b = addr_of(motion), .c = addr_of(out),
			  .m = pixels, .n = channels, .k = height, .batch = width,
			  .flags = absolute };
	if (!p.a || !p.b || !p.c) FAIL("history operand is not a live buffer", 0);
	return dispatch(g.rhistory, &p, (pixels + 63) / 64, 1, 1, 64, PK_HISTORY, absolute & 1u);
}

/* -- encoding and submission ----------------------------------------------- */

/* Buffers reached through raw addresses are not bound, so Metal has to be told they are
 * in use: every live buffer, once per encoder. */
static void use_all(id<MTLComputeCommandEncoder> enc, __unsafe_unretained id<MTLResource> *list, unsigned count)
{
	if (count) [enc useResources:list count:count usage:MTLResourceUsageRead | MTLResourceUsageWrite];
}

static unsigned char stamp_kind[MAX_STAMPS];

/* Where stamp `i` samples: buffer i / SAMPLE_PAIRS, indices 2 * (i % SAMPLE_PAIRS) and + 1. */
static id<MTLCounterSampleBuffer> sample_buffer(unsigned stamp) { return samples[stamp / SAMPLE_PAIRS]; }
static NSUInteger sample_index(unsigned stamp) { return 2 * (stamp % SAMPLE_PAIRS); }

static void collect(unsigned stamps)
{
	if (!g.prof || !sample_buffers || !stamps) return;
	for (unsigned first = 0; first < stamps; first += SAMPLE_PAIRS) {
		unsigned count = stamps - first < SAMPLE_PAIRS ? stamps - first : SAMPLE_PAIRS;
		NSData *resolved = [sample_buffer(first) resolveCounterRange:NSMakeRange(0, 2 * count)];
		if (!resolved || resolved.length < 2 * count * sizeof(MTLCounterResultTimestamp)) return;
		const MTLCounterResultTimestamp *t = resolved.bytes;
		for (unsigned i = 0; i < count; i++) {
			uint64_t start = t[2 * i].timestamp, end = t[2 * i + 1].timestamp;
			if (start == MTLCounterErrorValue || end == MTLCounterErrorValue || end < start) continue;
			prof_ms[stamp_kind[first + i]] += (double)(end - start) * g.ns_per_tick * 1e-6;
			prof_hits[stamp_kind[first + i]]++;
		}
	}
}

static int run_ops(const struct oplist *l)
{
	@autoreleasepool {
		id<MTLCommandBuffer> cb = [queue commandBuffer];
		if (!cb) FAIL("command buffer", 0);
		unsigned live = 0;
		for (int i = 0; i < MAX_RBUF; i++) live += rbufs[i].live ? 1 : 0;
		__unsafe_unretained id<MTLResource> *list = (__unsafe_unretained id<MTLResource> *)calloc(live ? live : 1, sizeof *list);
		if (!list) FAIL("out of memory listing buffers", 0);
		for (int i = 0, n = 0; i < MAX_RBUF; i++)
			if (rbufs[i].live) list[n++] = (__bridge id<MTLResource>)rbufs[i].b;

		int profiling = g.prof && sample_buffers != 0;
		unsigned stamp_limit = sample_buffers * SAMPLE_PAIRS;
		unsigned stamps = 0;
		id<MTLComputeCommandEncoder> enc = nil;
		for (unsigned i = 0; i < l->n; i++) {
			const struct op *op = &l->ops[i];
			if (op->kind == OP_BARRIER) {
				if (enc) [enc memoryBarrierWithScope:MTLBarrierScopeBuffers];
				continue;
			}
			if (op->kind == OP_COPY) {
				if (enc) { [enc updateFence:fence]; [enc endEncoding]; enc = nil; }
				id<MTLBlitCommandEncoder> blit;
				if (profiling && stamps < stamp_limit) {
					MTLBlitPassDescriptor *d = [MTLBlitPassDescriptor blitPassDescriptor];
					d.sampleBufferAttachments[0].sampleBuffer = sample_buffer(stamps);
					d.sampleBufferAttachments[0].startOfEncoderSampleIndex = sample_index(stamps);
					d.sampleBufferAttachments[0].endOfEncoderSampleIndex = sample_index(stamps) + 1;
					stamp_kind[stamps++] = op->stamp;
					blit = [cb blitCommandEncoderWithDescriptor:d];
				} else {
					blit = [cb blitCommandEncoder];
				}
				[blit waitForFence:fence];
				[blit copyFromBuffer:(__bridge id<MTLBuffer>)rbufs[op->src].b sourceOffset:(NSUInteger)op->so
					    toBuffer:(__bridge id<MTLBuffer>)rbufs[op->dst].b destinationOffset:(NSUInteger)op->to
						size:(NSUInteger)op->bytes];
				[blit updateFence:fence];
				[blit endEncoding];
				continue;
			}
			if (profiling && stamps < stamp_limit) {
				/* Metal samples timestamps at encoder boundaries, not between dispatches,
				 * so a profiled pass is an encoder of its own. */
				if (enc) { [enc updateFence:fence]; [enc endEncoding]; enc = nil; }
				MTLComputePassDescriptor *d = [MTLComputePassDescriptor computePassDescriptor];
				d.dispatchType = MTLDispatchTypeConcurrent;
				d.sampleBufferAttachments[0].sampleBuffer = sample_buffer(stamps);
				d.sampleBufferAttachments[0].startOfEncoderSampleIndex = sample_index(stamps);
				d.sampleBufferAttachments[0].endOfEncoderSampleIndex = sample_index(stamps) + 1;
				stamp_kind[stamps++] = op->stamp;
				enc = [cb computeCommandEncoderWithDescriptor:d];
				[enc waitForFence:fence];
				use_all(enc, list, live);
			} else if (!enc) {
				enc = [cb computeCommandEncoderWithDispatchType:MTLDispatchTypeConcurrent];
				[enc waitForFence:fence];
				use_all(enc, list, live);
			}
			[enc setComputePipelineState:(__bridge id<MTLComputePipelineState>)op->pipeline];
			[enc setBytes:&op->p length:sizeof op->p atIndex:0];
			[enc dispatchThreadgroups:MTLSizeMake(op->gx, op->gy, op->gz)
			    threadsPerThreadgroup:MTLSizeMake(op->tg, 1, 1)];
			if (profiling) { [enc updateFence:fence]; [enc endEncoding]; enc = nil; }
		}
		if (enc) { [enc updateFence:fence]; [enc endEncoding]; }
		free(list);
		if (finish(cb, "resident submit")) return -1;
		if (profiling) collect(stamps);
		return l->passes;
	}
}

int xmx_submit(void)
{
	if (!g.recording) FAIL("not recording", 0);
	g.recording = 0;
	rec.passes = g.recorded;
	return run_ops(&rec);
}

int xmx_graph_capture(void)
{
	if (!g.recording) FAIL("not recording", 0);
	int id;
	for (id = 0; id < MAX_GRAPHS && graphs[id].live; id++);
	if (id == MAX_GRAPHS) FAIL("out of graph slots", 0);
	/* The recording becomes the graph; a fresh list takes over for the next one. */
	graphs[id] = rec;
	graphs[id].passes = g.recorded;
	graphs[id].live = 1;
	memset(&rec, 0, sizeof rec);
	g.recording = 0;
	return id;
}

int xmx_graph_run(int id)
{
	if (g.recording) FAIL("cannot replay during recording", 0);
	if (id < 0 || id >= MAX_GRAPHS || !graphs[id].live) FAIL("graph is not live", 0);
	return run_ops(&graphs[id]);
}

int xmx_graph_destroy(int id)
{
	if (id < 0 || id >= MAX_GRAPHS || !graphs[id].live) return 0;
	op_free(&graphs[id]);
	return 0;
}

/* -- profiling ---------------------------------------------------------------- */

static double now_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

/* Timestamps at encoder boundaries, which is where Apple GPUs can take them; a profiled
 * frame therefore runs one encoder per pass rather than one per block, and its total is
 * not the frame that would have run. Off by default and free when off. Returns -1 when
 * the device cannot sample, and says why. */
int xmx_profile(int on)
{
	if (!on) { g.prof = 0; return 0; }
	if (!dev) FAIL("profiling needs an initialised device", 0);
	if (!sample_buffers) {
		@autoreleasepool {
			if (![dev supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary])
				FAIL("this device cannot sample timestamps at encoder boundaries", 0);
			id<MTLCounterSet> timestamps = nil;
			for (id<MTLCounterSet> set in dev.counterSets)
				if ([set.name isEqualToString:MTLCommonCounterSetTimestamp]) timestamps = set;
			if (!timestamps) FAIL("this device has no timestamp counter set", 0);
			MTLCounterSampleBufferDescriptor *d = [MTLCounterSampleBufferDescriptor new];
			d.counterSet = timestamps;
			d.storageMode = MTLStorageModeShared;
			d.sampleCount = 2 * SAMPLE_PAIRS;
			for (unsigned i = 0; i < SAMPLE_BUFFERS; i++) {
				NSError *error = nil;
				samples[i] = [dev newCounterSampleBufferWithDescriptor:d error:&error];
				if (!samples[i]) FAILNS("counter sample buffer", error);
				sample_buffers = i + 1;
			}
			/* GPU ticks to nanoseconds, measured rather than assumed: two paired samples
			 * a known wall-clock interval apart. */
			MTLTimestamp cpu0, gpu0, cpu1, gpu1;
			double wall0 = now_ns();
			[dev sampleTimestamps:&cpu0 gpuTimestamp:&gpu0];
			usleep(50000);
			[dev sampleTimestamps:&cpu1 gpuTimestamp:&gpu1];
			double wall1 = now_ns();
			g.ns_per_tick = gpu1 > gpu0 ? (wall1 - wall0) / (double)(gpu1 - gpu0) : 1.0;
			(void)cpu0; (void)cpu1;
		}
	}
	g.prof = 1;
	return 0;
}

void xmx_profile_reset(void)
{
	memset(prof_ms, 0, sizeof prof_ms);
	memset(prof_hits, 0, sizeof prof_hits);
}

double xmx_profile_ms(unsigned kind) { return kind < PROF_KINDS ? prof_ms[kind] : 0.0; }
unsigned xmx_profile_count(unsigned kind) { return kind < PROF_KINDS ? prof_hits[kind] : 0u; }
