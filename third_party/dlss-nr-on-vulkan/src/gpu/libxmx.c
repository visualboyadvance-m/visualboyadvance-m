/*
 * libxmx — a resident Vulkan compute context for the Xe2 XMX GEMM.
 *
 * The subprocess runner built a whole Vulkan instance, device and pipeline for every
 * matmul, which cost ~80 ms of fixed overhead per call and made any timing
 * meaningless. This keeps all of that alive across calls and reuses growable
 * host-visible buffers, so a dispatch costs a memcpy, a submit and a fence wait.
 *
 * Build: cc -O2 -shared -fPIC -I<vulkan headers> -o libxmx.so libxmx.c -lvulkan
 * On macOS the same file links against MoltenVK directly (-lMoltenVK): the Vulkan
 * loader there hides a portability driver until asked, and the compute path needs
 * no layer between it and the driver. `make` chooses.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "nr_shaders_embedded.h"
#include "xmx.h"
#include "../ref/nr_portable.h"
#if defined(XMX_NO_VULKAN_LINK) && !defined(_WIN32)
#include <dlfcn.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

/* Every Vulkan entry point this file calls is a pointer resolved through one
 * vkGetInstanceProcAddr: the linked library's (MoltenVK, or the loader) when libxmx makes
 * its own instance, and the adopter's when a host hands over its instance and device
 * (`xmx_adopt`) — so an adopted device is always driven through the very library that
 * created it, even when the host loaded a different copy than the one linked here. */
#define XMX_VK_GLOBAL_FUNCS(F) \
	F(vkCreateInstance) F(vkEnumerateInstanceExtensionProperties)
#define XMX_VK_INSTANCE_FUNCS(F) \
	F(vkAllocateCommandBuffers) F(vkAllocateDescriptorSets) F(vkAllocateMemory) \
	F(vkBeginCommandBuffer) F(vkBindBufferMemory) F(vkCmdBindDescriptorSets) \
	F(vkCmdBindPipeline) F(vkCmdCopyBuffer) F(vkCmdDispatch) F(vkCmdFillBuffer) \
	F(vkCmdPipelineBarrier) F(vkCmdPushConstants) F(vkCmdResetQueryPool) \
	F(vkCmdWriteTimestamp) F(vkCreateBuffer) F(vkCreateCommandPool) \
	F(vkCreateComputePipelines) F(vkCreateDescriptorPool) F(vkCreateDescriptorSetLayout) \
	F(vkCreateDevice) F(vkCreateFence) F(vkCreatePipelineLayout) F(vkCreateQueryPool) \
	F(vkCreateShaderModule) F(vkDestroyBuffer) F(vkDestroyCommandPool) \
	F(vkDestroyDescriptorPool) F(vkDestroyDescriptorSetLayout) F(vkDestroyDevice) \
	F(vkDestroyFence) F(vkDestroyInstance) F(vkDestroyPipeline) F(vkDestroyPipelineLayout) \
	F(vkDestroyQueryPool) F(vkDestroyShaderModule) F(vkDeviceWaitIdle) F(vkEndCommandBuffer) \
	F(vkEnumerateDeviceExtensionProperties) F(vkEnumeratePhysicalDevices) \
	F(vkFreeCommandBuffers) F(vkFreeMemory) F(vkGetBufferDeviceAddress) \
	F(vkGetBufferMemoryRequirements) F(vkGetDeviceQueue) F(vkGetPhysicalDeviceMemoryProperties) \
	F(vkGetPhysicalDeviceProperties) F(vkGetPhysicalDeviceQueueFamilyProperties) \
	F(vkGetQueryPoolResults) F(vkMapMemory) F(vkQueueSubmit) F(vkQueueWaitIdle) \
	F(vkResetCommandBuffer) F(vkResetFences) F(vkUnmapMemory) F(vkUpdateDescriptorSets) \
	F(vkWaitForFences)
#define XMX_VK_DECLARE(name) static PFN_##name name;
XMX_VK_GLOBAL_FUNCS(XMX_VK_DECLARE)
XMX_VK_INSTANCE_FUNCS(XMX_VK_DECLARE)
#undef XMX_VK_DECLARE
static PFN_vkGetInstanceProcAddr xmx_gipa;

#ifdef XMX_NO_VULKAN_LINK
/* The static build (libdlssnr) links no Vulkan library of its own: the host has one —
 * linked, or loaded — and an adopted instance brings its own entry point anyway. For a
 * device libxmx opens itself, look for vkGetInstanceProcAddr in the process first, then
 * load the loader (or MoltenVK) by name. */
static PFN_vkGetInstanceProcAddr linked_gipa(void)
{
#ifdef _WIN32
	HMODULE h = GetModuleHandleA("vulkan-1.dll");
	if (!h) h = LoadLibraryA("vulkan-1.dll");
	return h ? (PFN_vkGetInstanceProcAddr)(void (*)(void))GetProcAddress(h, "vkGetInstanceProcAddr") : NULL;
#else
	void *p = dlsym(RTLD_DEFAULT, "vkGetInstanceProcAddr");
	if (p) return (PFN_vkGetInstanceProcAddr)p;
	static const char *const names[] = { "libvulkan.so.1", "libvulkan.so", "libvulkan.1.dylib",
					      "libvulkan.dylib", "libMoltenVK.dylib" };
	for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
		void *h = dlopen(names[i], RTLD_NOW | RTLD_GLOBAL);
		if (h && (p = dlsym(h, "vkGetInstanceProcAddr"))) return (PFN_vkGetInstanceProcAddr)p;
	}
	return NULL;
#endif
}
#else
/* The linked library's bootstrap symbol; the only one reached by name. */
extern VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *name);
static PFN_vkGetInstanceProcAddr linked_gipa(void) { return vkGetInstanceProcAddr; }
#endif

#ifdef _WIN32
#include <windows.h>
#endif

/* A lost device is recorded as well as described: it is the one failure after which nothing
 * on this device can succeed again, so a caller has to be able to tell it from the rest
 * without reading the message. It stays set. */
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
#define FAIL(msg, r) do { int fail_code = (int)(r); \
    if (fail_code == VK_ERROR_DEVICE_LOST) g.lost = 1; \
    sprintf_s(g.err, sizeof g.err, "%s (%d)", msg, fail_code); return -1; } while (0)
#else
#define FAIL(msg, r) do { int fail_code = (int)(r); \
	if (fail_code == VK_ERROR_DEVICE_LOST) g.lost = 1; \
	snprintf(g.err, sizeof g.err, "%s (%d)", msg, fail_code); return -1; } while (0)
#endif

struct buf { VkBuffer b; VkDeviceMemory m; void *p; VkDeviceSize cap; };

static struct {
	VkInstance inst; VkPhysicalDevice pd; VkDevice dev; VkQueue q; uint32_t qi;
	VkDescriptorSetLayout dsl; VkPipelineLayout pl; VkPipeline pipe;
	VkPipelineLayout plb; VkPipeline pipeb;
	VkDescriptorPool dpool; VkDescriptorSet set;
	VkCommandPool cpool; VkCommandBuffer cb; VkFence fence;
	struct buf A, B, C;
	/* resident path */
	VkPipelineLayout rpl; VkPipeline rgemm, rtiled, rstaged, runary, rrow, rhistory;
	char *rpaths[5];
	unsigned specialize;
	unsigned tiling, tilem, tilen;
	int syncing;
	unsigned staging;
	VkCommandBuffer rcb; VkFence rfence; int recording, recorded, rready;
	/* transfers have their own command buffer: the weights are created while the
	 * frame's graph is being recorded, so a staged upload cannot borrow `rcb` */
	VkCommandBuffer tcb; VkFence tfence;
	/* GPU-side profiling. One timestamp after each recorded pass, so pass i costs
	 * ts[i+1]-ts[i]; the barrier between passes makes that attribution exact. */
	VkQueryPool qpool; unsigned prof, prof_n; float ts_period;
	char name[256]; char err[256]; char memory[256]; char memory_read[256];
	int ready, lost, discrete, unmapped;
	/* `coopmat`: the device has VK_KHR_cooperative_matrix. `portable`: the GEMMs run on
	 * the plain multiply-add kernels instead — because the device has no matrix path
	 * (MoltenVK on Apple silicon), or because XMX_PORTABLE=1 asked for it here. */
	int coopmat, portable;
	struct buf stage;
	/* An adopted device belongs to the host: never destroyed here, and every submit on
	 * its queue is bracketed by the host's lock when one was given. */
	int adopted;
	void (*lock)(void *); void (*unlock)(void *); void *lock_ctx;
} g;

/* What `xmx_adopt` was handed, until `xmx_open` takes it. */
static struct {
	int set;
	VkInstance inst; VkPhysicalDevice pd; VkDevice dev; VkQueue q; uint32_t qi;
	int coopmat;
	PFN_vkGetInstanceProcAddr gipa;
	void (*lock)(void *); void (*unlock)(void *); void *ctx;
} adopt;

static void qlock(void) { if (g.lock) g.lock(g.lock_ctx); }
static void qunlock(void) { if (g.unlock) g.unlock(g.lock_ctx); }

#define MAX_SPECIALIZED 256
static struct {
	unsigned family, flags;
	VkPipeline pipeline;
} specialized[MAX_SPECIALIZED];
static unsigned specialized_count;

#define MAX_GRAPHS 128
static struct { VkCommandBuffer commands; int passes; unsigned stamps; unsigned char *kinds; }
	graphs[MAX_GRAPHS];

/* A pass's kind is `family * 32 + subkind`, so a unary or row pass is attributed to the
 * specific operation it runs rather than lumped in with its family. Families below. */
#define MAX_STAMPS 8192
#define PROF_KINDS 256
enum { PK_GEMM = 0, PK_TILED, PK_STAGED, PK_UNARY, PK_ROW, PK_HISTORY, PK_COPY, PK_START = 7 };
static unsigned char stamp_kind[MAX_STAMPS];
static double prof_ms[PROF_KINDS];
static unsigned prof_hits[PROF_KINDS];

/* Device-resident buffers. The graph's activations live here between blocks instead
 * of being read back to the host after every GEMM; on a shared-memory APU the mapping
 * is HOST_CACHED, so the host can still write inputs and read outputs in place. */
#define MAX_RBUF 8192
struct rbuf { VkBuffer b; VkDeviceMemory m; void *p; VkDeviceAddress addr;
		 VkDeviceSize size; int live, mapped; };
static struct rbuf rbufs[MAX_RBUF];

struct push {
	uint64_t a, b, c, d;
	uint32_t m, n, k, batch, sa, sb, sc, flags;
	float p0, p1, p2, p3;
	uint32_t lda, ldb, ldc, spare;
};

const char *xmx_error(void) { return g.err; }
int xmx_device_lost(void) { return g.lost; }
/* Asked before the first buffer exists — which is when the daemon logs it — the answer is
 * still knowable: run the same choice against every type the device has. */
static uint32_t memtype(uint32_t bits, VkMemoryPropertyFlags want, int host_read);
const char *xmx_memory(void)
{
	static char both[600];
	if (!g.ready) return "not initialised";
	if (!g.memory[0])
		memtype(~0u, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0);
	if (!g.memory_read[0])
		memtype(~0u, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 1);
	if (!g.memory[0]) return "no host-visible memory type";
    
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(both, sizeof both, "%s%s%s", g.memory,
         g.memory_read[0] ? "; " : "", g.memory_read);
#else
	snprintf(both, sizeof both, "%s%s%s", g.memory,
		 g.memory_read[0] ? "; " : "", g.memory_read);
#endif

	return both;
}
const char *xmx_device(void) { return g.name; }

/* Where the operands live, which is not the same question on the two kinds of GPU.
 *
 * On this shared-memory APU, HOST_CACHED first: memoryTypes[1] is
 * DEVICE_LOCAL|HOST_VISIBLE|HOST_COHERENT and memoryTypes[2] is the same plus
 * HOST_CACHED, and taking the first match landed on the uncached one, where reading the
 * result back ran at ~80 MB/s and buried a 1.35 TFLOP/s kernel: a 147456x32x128 GEMM
 * spent 1073 ms moving 85 MB.
 *
 * On a discrete GPU that preference is a trap. HOST_CACHED there means system memory, so
 * every operand would be read across PCIe while the card's own memory sits unused —
 * consistent with an Arc B580 measuring 50-141 GFLOP/s on shapes this iGPU runs at
 * 1027-3470. So device-local first there, which resizable BAR makes host-visible as well.
 * With the BAR unresized that window is 256 MB, far under the buffers a frame needs, and
 * preferring it would turn a slow run into a failed allocation — hence the heap-size floor,
 * which sends such a card back to system memory. Untested: there is no discrete GPU on the
 * machine this was written on. */
#define HOST_VISIBLE_VRAM_FLOOR (1024ull * 1024 * 1024)

/* Two ways to reach the card's memory, and the second one is not optional.
 *
 * Where the whole of VRAM is host-visible — a shared-memory APU, or a discrete card with
 * resizable BAR — the graph's buffers are mapped and the host writes into them directly.
 * Where it is not, a mapped buffer can only be system memory, which is the 50x trap
 * (`notes/phase63`). Then the graph's buffers are device-local and unmapped, and the host
 * reaches them through copies.
 *
 * `XMX_STAGING=1` forces the second path on hardware that would take the first. That is
 * how it is tested here: correctness does not depend on where the memory is, so the iGPU
 * can run the code a card without resizable BAR would run. */
static int want_unmapped(void)
{
	const char *forced = getenv("XMX_STAGING");
	if (forced && *forced) return atoi(forced) != 0;
	if (!g.discrete) return 0;
	VkPhysicalDeviceMemoryProperties mp;
	vkGetPhysicalDeviceMemoryProperties(g.pd, &mp);
	const VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
					 | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
		if ((mp.memoryTypes[i].propertyFlags & want) == want
		    && mp.memoryHeaps[mp.memoryTypes[i].heapIndex].size >= HOST_VISIBLE_VRAM_FLOOR)
			return 0;                      /* resizable BAR: map it and write in place */
	return 1;
}

static void note_memory(const VkPhysicalDeviceMemoryProperties *mp, uint32_t type, int host_read);

/* Device-local for the graph, host-visibility not required. */
static uint32_t memtype_device(uint32_t bits)
{
	VkPhysicalDeviceMemoryProperties mp;
	vkGetPhysicalDeviceMemoryProperties(g.pd, &mp);
	for (uint32_t pass = 0; pass < 2; pass++)
		for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
			VkMemoryPropertyFlags f = mp.memoryTypes[i].propertyFlags;
			if (!(bits & (1u << i))) continue;
			if (!(f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) continue;
			/* first pass prefers memory the host cannot see, which on a discrete
			 * card is the card's own and on this APU does not exist */
			if (!pass && (f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) continue;
			note_memory(&mp, i, 0);
			return i;
		}
	return UINT32_MAX;
}

/* Say where the buffers went, in one line, because the answer decides everything about
 * this machine's speed and nobody can see it from outside: on a discrete card, operands in
 * system memory are read across PCIe, and that is what an unresized BAR leaves us with. */
static void note_memory(const VkPhysicalDeviceMemoryProperties *mp, uint32_t type, int host_read)
{
	VkMemoryPropertyFlags f = mp->memoryTypes[type].propertyFlags;
	double heap = (double)mp->memoryHeaps[mp->memoryTypes[type].heapIndex].size / (1 << 30);
	const char *where = (!host_read && g.unmapped)
		? ((f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
		   ? "card memory, unmapped by choice (staging forced)"
		   : "card memory, not host-visible: the host reaches it by copies")
		: host_read
		? ((f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
		   ? "readback cached"
		   : "READBACK UNCACHED - the host reads the head with a stride, and on a discrete "
		     "card that is a strided read across PCIe")
		: !g.discrete ? "shared memory (one pool)"
		: (f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		? "card memory"
		: "SYSTEM MEMORY ACROSS PCIE - resizable BAR is off, or its window is under 1 GiB";
	char *slot = host_read ? g.memory_read : g.memory;
	size_t room = host_read ? sizeof g.memory_read : sizeof g.memory;

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(slot, room, "%s: type %u, heap %.1f GiB,%s%s%s%s", where, type, heap,
         (f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? " DEVICE_LOCAL" : "",
         (f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? " HOST_VISIBLE" : "",
         (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? " HOST_COHERENT" : "",
         (f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? " HOST_CACHED" : "");
#else
	snprintf(slot, room, "%s: type %u, heap %.1f GiB,%s%s%s%s", where, type, heap,
		 (f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? " DEVICE_LOCAL" : "",
		 (f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? " HOST_VISIBLE" : "",
		 (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? " HOST_COHERENT" : "",
		 (f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? " HOST_CACHED" : "");
#endif
}
static uint32_t memtype(uint32_t bits, VkMemoryPropertyFlags want, int host_read)
{
	VkPhysicalDeviceMemoryProperties mp;
	vkGetPhysicalDeviceMemoryProperties(g.pd, &mp);
	/* A buffer the host reads is the exception to the rule above. The head comes back as
	 * four of every sixteen floats — a strided read — and uncached memory serves that at a
	 * fraction of its streaming rate, which on a discrete card is a fraction of PCIe. The
	 * device writes it once; the host reads it once. Cached wins that trade even when the
	 * cached memory is on the other side of the bus. */
	const VkMemoryPropertyFlags prefer[3] = {
		(host_read || !g.discrete) ? VK_MEMORY_PROPERTY_HOST_CACHED_BIT
					   : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		(host_read || !g.discrete) ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
					   : VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		0 };
	for (uint32_t pass = 0; pass < 3; pass++) {
		VkMemoryPropertyFlags need = want | prefer[pass];
		for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
			if (!(bits & (1u << i))) continue;
			if ((mp.memoryTypes[i].propertyFlags & need) != need) continue;
			if (g.discrete && !host_read && (need & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
			    && mp.memoryHeaps[mp.memoryTypes[i].heapIndex].size < HOST_VISIBLE_VRAM_FLOOR)
				continue;
			note_memory(&mp, i, host_read);
			return i;
		}
	}
	return UINT32_MAX;
}

/* The compiled-in module of that name, or NULL. `name` is a bare file name. */
static const struct nr_embedded_shader *embedded_shader(const char *name)
{
#ifdef NR_EMBEDDED_SHADERS
	for (size_t i = 0; i < nr_embedded_shader_count; i++)
		if (!strcmp(nr_embedded_shaders[i].name, name)) return &nr_embedded_shaders[i];
#endif
	(void)name;
	return NULL;
}

size_t xmx_embedded_shader(const char *name)
{
	const struct nr_embedded_shader *e = name ? embedded_shader(name) : NULL;
	return e ? e->size : 0;
}

/* The SPIR-V for `spv_path`: a bare name is the embedded module (nr_shaders_embedded.h);
 * a path opens that file; a path whose file is missing falls back to the embedded module
 * of the same base name. `*owned` is what to free, if anything. */
static const void *shader_code(const char *spv_path, size_t *len, void **owned)
{
	*owned = NULL;
	const char *base = spv_path;
	for (const char *q = spv_path; *q; q++)
		if (*q == '/' || *q == '\\') base = q + 1;
	const struct nr_embedded_shader *e = embedded_shader(base);
	if (e && base == spv_path) { *len = e->size; return e->data; }
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
	FILE *f = NULL;
	fopen_s(&f, spv_path, "rb");
#else
	FILE *f = fopen(spv_path, "rb");
#endif
	if (!f) {
		if (e) { *len = e->size; return e->data; }
		return NULL;
	}
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	void *code = malloc(n > 0 ? (size_t)n : 1);
	if (!code || n <= 0 || fread(code, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(code); return NULL; }
	fclose(f);
	*owned = code;
	*len = (size_t)n;
	return code;
}

static int build_pipeline_spec(const char *spv_path, VkPipelineLayout layout, VkPipeline *out,
			      const VkSpecializationInfo *specialization)
{
	size_t len = 0;
	void *owned = NULL;
	const void *code = shader_code(spv_path, &len, &owned);
	if (!code) FAIL("cannot open spv (no such file, and no embedded module of that name)", 0);
	VkShaderModuleCreateInfo smi = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
					 .codeSize = len, .pCode = code };
	VkShaderModule sm;
	VkResult r = vkCreateShaderModule(g.dev, &smi, NULL, &sm);
	free(owned);
	if (r) FAIL("shader module", r);
	VkComputePipelineCreateInfo cpi = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			   .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = sm, .pName = "main",
			   .pSpecializationInfo = specialization }, .layout = layout };
	r = vkCreateComputePipelines(g.dev, VK_NULL_HANDLE, 1, &cpi, NULL, out);
	vkDestroyShaderModule(g.dev, sm, NULL);
	if (r) FAIL("pipeline", r);
	return 0;
}

static int build_pipeline(const char *path, VkPipelineLayout layout, VkPipeline *out)
{
	return build_pipeline_spec(path, layout, out, NULL);
}

/* Freeze operation flags before compilation: dead transpose/publish/width branches
 * otherwise contribute to register pressure even on dispatches that do not use them.
 * Keep the unspecialized path for same-buffer A/B measurements and shader overrides. */
/* The block size of the tiled pipeline, from the environment.
 *
 * These are not free parameters. They have to match the `-DRM`/`-DRN` that
 * `work/gemm_tiled.spv` was built with — 2 and 2, so 16x32 — because the dispatch
 * divides the extent by them while each workgroup writes the block the *shader* has.
 * Too small and the tiles overlap and run off the bottom edge, which
 * `cooperativeMatrixRobustBufferAccess = false` will not catch; zero divides by zero
 * outright, and `unsigned` turns a negative into something no extent is a multiple of,
 * so the knob silently does nothing. Anything but a positive number is refused. */
static unsigned block_size(const char *name, unsigned fallback)
{
	const char *value = getenv(name);
	if (!value) return fallback;
	int n = atoi(value);
	if (n > 0) return (unsigned)n;
	fprintf(stderr, "libxmx: %s=%s is not a positive block size; using %u\n",
		name, value, fallback);
	return fallback;
}

static int resident_pipeline(unsigned family, unsigned flags, VkPipeline fallback,
			     VkPipeline *out)
{
	unsigned mask = family < 3 ? 1u : (family == 3 ? 2u : 4u);
	*out = fallback;
	if (!(g.specialize & mask)) return 0;
	for (unsigned i = 0; i < specialized_count; i++) {
		if (specialized[i].family == family && specialized[i].flags == flags) {
			*out = specialized[i].pipeline;
			return 0;
		}
	}
	if (specialized_count == MAX_SPECIALIZED) return 0;
	VkSpecializationMapEntry entry = { .constantID = 0, .offset = 0, .size = sizeof flags };
	VkSpecializationInfo info = { .mapEntryCount = 1, .pMapEntries = &entry,
				      .dataSize = sizeof flags, .pData = &flags };
	if (build_pipeline_spec(g.rpaths[family], g.rpl, out, &info)) return -1;
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

/* `host_read` as in `xmx_buf_create_kind`: C is the result the host reads back, A and B are
 * operands it only writes. The plain GEMM path is the benchmark's path, so getting this
 * wrong would mis-measure the very card the rule above exists for. */
static int ensure(struct buf *b, VkDeviceSize size, int host_read)
{
	if (b->cap >= size)
		return 0;
	if (b->b) { vkUnmapMemory(g.dev, b->m); vkDestroyBuffer(g.dev, b->b, NULL); vkFreeMemory(g.dev, b->m, NULL); }
	VkBufferCreateInfo bi = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size,
				  .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT };
	VkResult r = vkCreateBuffer(g.dev, &bi, NULL, &b->b);
	if (r) FAIL("vkCreateBuffer", r);
	VkMemoryRequirements mr;
	vkGetBufferMemoryRequirements(g.dev, b->b, &mr);
	uint32_t mt = memtype(mr.memoryTypeBits,
			      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, host_read);
	if (mt == UINT32_MAX) FAIL("no host-visible memory type", 0);
	VkMemoryAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				    .allocationSize = mr.size, .memoryTypeIndex = mt };
	r = vkAllocateMemory(g.dev, &ai, NULL, &b->m);
	if (r) FAIL("vkAllocateMemory", r);
	vkBindBufferMemory(g.dev, b->b, b->m, 0);
	r = vkMapMemory(g.dev, b->m, 0, size, 0, &b->p);
	if (r) FAIL("vkMapMemory", r);
	b->cap = size;
	return 0;
}

/* Whether an extension is in a list, so the device is asked only for what it has. */
static int has_extension(const VkExtensionProperties *list, uint32_t count, const char *name)
{
	for (uint32_t i = 0; i < count; i++)
		if (!strcmp(list[i].extensionName, name)) return 1;
	return 0;
}

/* The instance, the device and the queue — everything that decides which shaders can
 * run, and nothing that needs a shader. Split from `xmx_init` so a caller can ask
 * `xmx_coopmat()` before it chooses which SPIR-V to hand over. Idempotent. */
/* Resolve the entry points through `xmx_gipa`: the global ones before an instance
 * exists, the rest against it. */
static int resolve_global(void)
{
#define XMX_VK_LOAD(name) name = (PFN_##name)xmx_gipa(NULL, #name); \
	if (!name) FAIL("Vulkan library has no " #name, 0);
	XMX_VK_GLOBAL_FUNCS(XMX_VK_LOAD)
#undef XMX_VK_LOAD
	return 0;
}

static int resolve_instance(VkInstance inst)
{
#define XMX_VK_LOAD(name) name = (PFN_##name)xmx_gipa(inst, #name); \
	if (!name) FAIL("Vulkan instance has no " #name " (Vulkan 1.3 is needed)", 0);
	XMX_VK_INSTANCE_FUNCS(XMX_VK_LOAD)
#undef XMX_VK_LOAD
	return 0;
}

/* Share a host's Vulkan objects instead of creating an instance and a device. Call
 * before `xmx_open` (or after `xmx_close`); the next `xmx_open` takes them. The handles
 * are `void *` so a caller that binds this by name needs no Vulkan header.
 *
 *   inst, pd, dev   the host's VkInstance, VkPhysicalDevice and VkDevice. The device
 *                   must have Vulkan 1.3 and these features enabled: storageBuffer16BitAccess,
 *                   vulkanMemoryModel (+DeviceScope), shaderFloat16, bufferDeviceAddress,
 *                   scalarBlockLayout; VK_KHR_portability_subset where the device offers it.
 *   q, qi           a queue of family `qi`, which must support compute. If the host also
 *                   submits on `q`, it passes `lock`/`unlock` (called around every
 *                   vkQueueSubmit here, with `ctx`) and takes the same lock around its own
 *                   submits, presents and device-idle waits.
 *   coopmat         whether the host enabled VK_KHR_cooperative_matrix on `dev`.
 *   gipa            the host's vkGetInstanceProcAddr, the library that made `inst`.
 *
 * The host owns the objects: `xmx_close` releases everything libxmx made on them and
 * leaves them alone. The host must `xmx_close` before it destroys the device. */
int xmx_adopt(void *inst, void *pd, void *dev, void *q, unsigned qi, int coopmat, void *gipa,
	      void (*lock)(void *), void (*unlock)(void *), void *ctx)
{
	if (g.dev) FAIL("xmx_adopt: a device is open; xmx_close first", 0);
	if (!inst || !pd || !dev || !q || !gipa) FAIL("xmx_adopt: incomplete Vulkan objects", 0);
	adopt.set = 1;
	adopt.inst = (VkInstance)inst; adopt.pd = (VkPhysicalDevice)pd; adopt.dev = (VkDevice)dev;
	adopt.q = (VkQueue)q; adopt.qi = qi; adopt.coopmat = coopmat;
	adopt.gipa = (PFN_vkGetInstanceProcAddr)gipa;
	adopt.lock = lock; adopt.unlock = unlock; adopt.ctx = ctx;
	return 0;
}

/* 1 while an adopted device is open, 0 for libxmx's own, -1 when nothing is open. */
int xmx_adopted(void) { return g.dev ? g.adopted : -1; }

static int open_adopted(void)
{
	xmx_gipa = adopt.gipa;
	if (resolve_global() || resolve_instance(adopt.inst)) return -1;
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(adopt.pd, &props);
	if (props.apiVersion < VK_API_VERSION_1_3)
		FAIL("adopted device is below Vulkan 1.3", (int)props.apiVersion);
	uint32_t nq = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(adopt.pd, &nq, NULL);
	VkQueueFamilyProperties *qf = calloc(nq ? nq : 1, sizeof *qf);
	vkGetPhysicalDeviceQueueFamilyProperties(adopt.pd, &nq, qf);
	int compute = adopt.qi < nq && (qf[adopt.qi].queueFlags & VK_QUEUE_COMPUTE_BIT);
	free(qf);
	if (!compute) FAIL("adopted queue family has no compute", (int)adopt.qi);

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
	sprintf_s(g.name, sizeof g.name, "%s (shared)", props.deviceName);
#else
	snprintf(g.name, sizeof g.name, "%s (shared)", props.deviceName);
#endif
	g.discrete = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
	g.coopmat = adopt.coopmat;
	const char *forced = getenv("XMX_PORTABLE");
	g.portable = !g.coopmat || (forced && *forced && atoi(forced) != 0);
	g.inst = adopt.inst; g.pd = adopt.pd; g.q = adopt.q; g.qi = adopt.qi;
	g.lock = adopt.lock; g.unlock = adopt.unlock; g.lock_ctx = adopt.ctx;
	g.adopted = 1;
	g.dev = adopt.dev;
	adopt.set = 0;
	return 0;
}

int xmx_open(void)
{
	if (g.dev) return 0;
	if (adopt.set) return open_adopted();
	xmx_gipa = linked_gipa();
	if (!xmx_gipa) FAIL("no Vulkan library in the process (vkGetInstanceProcAddr not found)", 0);
	if (resolve_global()) return -1;
	uint32_t nie = 0;
	vkEnumerateInstanceExtensionProperties(NULL, &nie, NULL);
	VkExtensionProperties *ie = calloc(nie ? nie : 1, sizeof *ie);
	vkEnumerateInstanceExtensionProperties(NULL, &nie, ie);
	/* MoltenVK is a "portability" driver: behind the Vulkan loader on macOS it is
	 * invisible until the instance enables this and sets the flag. Linked directly, or
	 * on Linux, the extension is absent and nothing is asked for. */
	int portability = has_extension(ie, nie, "VK_KHR_portability_enumeration");
	free(ie);
	const char *iext[] = { "VK_KHR_portability_enumeration" };
	VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				  .pApplicationName = "libxmx", .apiVersion = VK_API_VERSION_1_3 };
	VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app,
				     .flags = portability ? VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR : 0u,
				     .enabledExtensionCount = portability ? 1u : 0u,
				     .ppEnabledExtensionNames = iext };
	VkResult r = vkCreateInstance(&ici, NULL, &g.inst);
	if (r) FAIL("vkCreateInstance", r);
	if (resolve_instance(g.inst)) return -1;

	uint32_t n = 0;
	vkEnumeratePhysicalDevices(g.inst, &n, NULL);
	if (!n) FAIL("no physical device", 0);
	VkPhysicalDevice *pds = calloc(n, sizeof *pds);
	vkEnumeratePhysicalDevices(g.inst, &n, pds);
	g.pd = pds[0];
	free(pds);
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(g.pd, &props);
    
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(g.name, sizeof g.name, "%s", props.deviceName);
#else
	snprintf(g.name, sizeof g.name, "%s", props.deviceName);
#endif

	g.discrete = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

	uint32_t nq = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(g.pd, &nq, NULL);
	VkQueueFamilyProperties *qf = calloc(nq, sizeof *qf);
	vkGetPhysicalDeviceQueueFamilyProperties(g.pd, &nq, qf);
	g.qi = UINT32_MAX;
	for (uint32_t i = 0; i < nq; i++)
		if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { g.qi = i; break; }
	free(qf);
	if (g.qi == UINT32_MAX) FAIL("no compute queue", 0);

	/* What the device has decides the kernels, not the other way round. Without
	 * VK_KHR_cooperative_matrix every GEMM runs on `gemm_portable*.comp`; the caller
	 * reads `xmx_coopmat()` / `xmx_portable()` and hands over the matching SPIR-V. */
	uint32_t nde = 0;
	vkEnumerateDeviceExtensionProperties(g.pd, NULL, &nde, NULL);
	VkExtensionProperties *de = calloc(nde ? nde : 1, sizeof *de);
	vkEnumerateDeviceExtensionProperties(g.pd, NULL, &nde, de);
	g.coopmat = has_extension(de, nde, VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME);
	int subset = has_extension(de, nde, "VK_KHR_portability_subset");
	free(de);
	const char *forced = getenv("XMX_PORTABLE");
	g.portable = !g.coopmat || (forced && *forced && atoi(forced) != 0);
	const char *ext[2]; uint32_t next = 0;
	if (g.coopmat) ext[next++] = VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME;
	if (subset) ext[next++] = "VK_KHR_portability_subset";   /* required when offered */

	VkPhysicalDeviceCooperativeMatrixFeaturesKHR cm = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR, .cooperativeMatrix = VK_TRUE };
	/* bufferDeviceAddress lets the resident path pass operands as 64-bit pointers in
	 * push constants, so a whole block of dispatches records into one command buffer
	 * without a descriptor pool. scalarBlockLayout matches the shaders' layout. */
	VkPhysicalDeviceVulkan12Features v12 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = g.coopmat ? (void *)&cm : NULL,
		.vulkanMemoryModel = VK_TRUE, .vulkanMemoryModelDeviceScope = VK_TRUE, .shaderFloat16 = VK_TRUE,
		.bufferDeviceAddress = VK_TRUE, .scalarBlockLayout = VK_TRUE };
	VkPhysicalDeviceVulkan11Features v11 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, .pNext = &v12,
		.storageBuffer16BitAccess = VK_TRUE };
	VkPhysicalDeviceFeatures2 f2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &v11 };
	float prio = 1.0f;
	VkDeviceQueueCreateInfo qci = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = g.qi, .queueCount = 1, .pQueuePriorities = &prio };
	VkDeviceCreateInfo dci = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = &f2,
				   .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci,
				   .enabledExtensionCount = next, .ppEnabledExtensionNames = ext };
	r = vkCreateDevice(g.pd, &dci, NULL, &g.dev);
	if (r) { g.dev = VK_NULL_HANDLE; FAIL("vkCreateDevice", r); }
	vkGetDeviceQueue(g.dev, g.qi, 0, &g.q);
	return 0;
}

/* Release everything libxmx made: buffers, graphs, pipelines, pools, fences — and its
 * own device and instance, unless they were adopted, in which case they go back to the
 * host untouched. `xmx_open` may be called again afterwards (an adoption handed over
 * since then is taken up). Every buffer id and graph id is dead after this. */
int xmx_buf_destroy(int id);
int xmx_graph_destroy(int id);

void xmx_close(void)
{
	if (!g.dev) { adopt.set = 0; return; }
	if (g.adopted) { qlock(); vkQueueWaitIdle(g.q); qunlock(); }
	else vkDeviceWaitIdle(g.dev);
	for (int i = 0; i < MAX_RBUF; i++) xmx_buf_destroy(i);
	for (int i = 0; i < MAX_GRAPHS; i++) xmx_graph_destroy(i);
	for (unsigned i = 0; i < specialized_count; i++) vkDestroyPipeline(g.dev, specialized[i].pipeline, NULL);
	specialized_count = 0;
	VkPipeline *pipes[] = { &g.rgemm, &g.rtiled, &g.rstaged, &g.runary, &g.rrow, &g.rhistory, &g.pipe, &g.pipeb };
	for (size_t i = 0; i < sizeof pipes / sizeof *pipes; i++) if (*pipes[i]) vkDestroyPipeline(g.dev, *pipes[i], NULL);
	if (g.rpl) vkDestroyPipelineLayout(g.dev, g.rpl, NULL);
	if (g.plb) vkDestroyPipelineLayout(g.dev, g.plb, NULL);
	if (g.pl) vkDestroyPipelineLayout(g.dev, g.pl, NULL);
	if (g.dsl) vkDestroyDescriptorSetLayout(g.dev, g.dsl, NULL);
	for (int i = 0; i < 5; i++) free(g.rpaths[i]);
	if (g.qpool) vkDestroyQueryPool(g.dev, g.qpool, NULL);
	if (g.fence) vkDestroyFence(g.dev, g.fence, NULL);
	if (g.rfence) vkDestroyFence(g.dev, g.rfence, NULL);
	if (g.tfence) vkDestroyFence(g.dev, g.tfence, NULL);
	if (g.cpool) vkDestroyCommandPool(g.dev, g.cpool, NULL);   /* frees cb, rcb, tcb */
	if (g.dpool) vkDestroyDescriptorPool(g.dev, g.dpool, NULL); /* frees set */
	struct buf *bufs[] = { &g.A, &g.B, &g.C, &g.stage };
	for (size_t i = 0; i < sizeof bufs / sizeof *bufs; i++)
		if (bufs[i]->b) {
			if (bufs[i]->p) vkUnmapMemory(g.dev, bufs[i]->m);
			vkDestroyBuffer(g.dev, bufs[i]->b, NULL);
			vkFreeMemory(g.dev, bufs[i]->m, NULL);
		}
	if (!g.adopted) {
		vkDestroyDevice(g.dev, NULL);
		vkDestroyInstance(g.inst, NULL);
	}
	memset(&g, 0, sizeof g);
	memset(graphs, 0, sizeof graphs);
	memset(prof_ms, 0, sizeof prof_ms);
	memset(prof_hits, 0, sizeof prof_hits);
	adopt.set = 0;
}

int xmx_coopmat(void) { return g.dev ? g.coopmat : -1; }
int xmx_portable(void) { return g.dev ? g.portable : -1; }
/* One line for a log: which GEMM kernels this device runs, and why. */
const char *xmx_path(void)
{
	if (!g.dev) return "not opened";
	if (!g.portable) return "cooperative matrix (VK_KHR_cooperative_matrix, fp16 x fp16 -> fp32)";
	return g.coopmat ? "portable multiply-add (XMX_PORTABLE=1; the device has cooperative matrix)"
			 : "portable multiply-add (the device has no VK_KHR_cooperative_matrix)";
}

int xmx_init(const char *spv_path)
{
	if (g.ready) return 0;
	if (xmx_open()) return -1;
	VkResult r;

	VkDescriptorSetLayoutBinding bind[3];
	for (int i = 0; i < 3; i++)
		bind[i] = (VkDescriptorSetLayoutBinding){ .binding = i, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							  .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT };
	VkDescriptorSetLayoutCreateInfo dl = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
					       .bindingCount = 3, .pBindings = bind };
	if ((r = vkCreateDescriptorSetLayout(g.dev, &dl, NULL, &g.dsl))) FAIL("dsl", r);
	VkPushConstantRange pcr = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = 12 };
	VkPipelineLayoutCreateInfo pli = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
					   .setLayoutCount = 1, .pSetLayouts = &g.dsl,
					   .pushConstantRangeCount = 1, .pPushConstantRanges = &pcr };
	if ((r = vkCreatePipelineLayout(g.dev, &pli, NULL, &g.pl))) FAIL("pipeline layout", r);

	if (build_pipeline(spv_path, g.pl, &g.pipe))
		return -1;

	VkDescriptorPoolSize ps = { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 3 };
	VkDescriptorPoolCreateInfo dpi = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
					   .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &ps };
	if ((r = vkCreateDescriptorPool(g.dev, &dpi, NULL, &g.dpool))) FAIL("descriptor pool", r);
	VkDescriptorSetAllocateInfo dsa = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
					    .descriptorPool = g.dpool, .descriptorSetCount = 1, .pSetLayouts = &g.dsl };
	if ((r = vkAllocateDescriptorSets(g.dev, &dsa, &g.set))) FAIL("descriptor set", r);

	VkCommandPoolCreateInfo cpci = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
					 .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
					 .queueFamilyIndex = g.qi };
	if ((r = vkCreateCommandPool(g.dev, &cpci, NULL, &g.cpool))) FAIL("command pool", r);
	VkCommandBufferAllocateInfo cba = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					    .commandPool = g.cpool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					    .commandBufferCount = 1 };
	if ((r = vkAllocateCommandBuffers(g.dev, &cba, &g.cb))) FAIL("command buffer", r);
	VkFenceCreateInfo fi = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	if ((r = vkCreateFence(g.dev, &fi, NULL, &g.fence))) FAIL("fence", r);

	g.ready = 1;
	return 0;
}

/* Zero-copy path: hand the caller the mapped operand buffers so it can build A in
 * place and read C in place, instead of memcpying both across. On a shared-memory
 * APU those copies buy nothing. `xmx_reserve` may reallocate, so the pointers it
 * returns are valid only until the next call. */
int xmx_reserve(unsigned M, unsigned N, unsigned K, void **pa, void **pb, void **pc)
{
	if (!g.ready) FAIL("not initialised", 0);
	if (ensure(&g.A, (VkDeviceSize)M * K * 2, 0) || ensure(&g.B, (VkDeviceSize)K * N * 2, 0)
	    || ensure(&g.C, (VkDeviceSize)M * N * 4, 1))
		return -1;
	if (pa) *pa = g.A.p;
	if (pb) *pb = g.B.p;
	if (pc) *pc = g.C.p;
	return 0;
}

/* iters > 1 dispatches the same work repeatedly inside one submit, so the caller can
 * time the GPU without host copies dominating. */
int xmx_gemm(unsigned M, unsigned N, unsigned K, const void *a, const void *b, void *c, unsigned iters)
{
	if (!g.ready) FAIL("not initialised", 0);
	VkDeviceSize sa = (VkDeviceSize)M * K * 2, sb = (VkDeviceSize)K * N * 2, sc = (VkDeviceSize)M * N * 4;
	if (ensure(&g.A, sa, 0) || ensure(&g.B, sb, 0) || ensure(&g.C, sc, 1))
		return -1;
	VkDescriptorBufferInfo dbi[3] = { { g.A.b, 0, sa }, { g.B.b, 0, sb }, { g.C.b, 0, sc } };
	VkWriteDescriptorSet w[3];
	for (int i = 0; i < 3; i++)
		w[i] = (VkWriteDescriptorSet){ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = g.set,
					       .dstBinding = i, .descriptorCount = 1,
					       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dbi[i] };
	vkUpdateDescriptorSets(g.dev, 3, w, 0, NULL);
	if (a) memcpy(g.A.p, a, sa);
	if (b) memcpy(g.B.p, b, sb);

	vkResetCommandBuffer(g.cb, 0);
	VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
					.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	vkBeginCommandBuffer(g.cb, &bi);
	vkCmdBindPipeline(g.cb, VK_PIPELINE_BIND_POINT_COMPUTE, g.pipe);
	vkCmdBindDescriptorSets(g.cb, VK_PIPELINE_BIND_POINT_COMPUTE, g.pl, 0, 1, &g.set, 0, NULL);
	unsigned pc[3] = { M, N, K };
	vkCmdPushConstants(g.cb, g.pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, 12, pc);
	VkMemoryBarrier mb = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			       .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
			       .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT };
	for (unsigned i = 0; i < (iters ? iters : 1); i++) {
		vkCmdDispatch(g.cb, N / 16, M / 8, 1);
		if (i + 1 < iters)
			vkCmdPipelineBarrier(g.cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
	}
	vkEndCommandBuffer(g.cb);
	vkResetFences(g.dev, 1, &g.fence);
	VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &g.cb };
	qlock();
	VkResult r = vkQueueSubmit(g.q, 1, &si, g.fence);
	qunlock();
	if (r) FAIL("submit", r);
	r = vkWaitForFences(g.dev, 1, &g.fence, VK_TRUE, 60ull * 1000000000ull);
	if (r) FAIL("fence wait", r);
	if (c) memcpy(c, g.C.p, sc);
	return 0;
}


/* The batched pipeline is built on first use: the extra shader takes seven push
 * constants instead of three, so it needs its own pipeline layout. */
int xmx_init_batched(const char *spv_path)
{
	if (!g.ready) FAIL("not initialised", 0);
	if (g.pipeb) return 0;
	VkPushConstantRange pcr = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = 28 };
	VkPipelineLayoutCreateInfo pli = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
					   .setLayoutCount = 1, .pSetLayouts = &g.dsl,
					   .pushConstantRangeCount = 1, .pPushConstantRanges = &pcr };
	VkResult r = vkCreatePipelineLayout(g.dev, &pli, NULL, &g.plb);
	if (r) FAIL("batched pipeline layout", r);
	return build_pipeline(spv_path, g.plb, &g.pipeb);
}

/* Byte capacities, so the caller can lay the three tensors out itself. */
int xmx_reserve_bytes(unsigned long long a, unsigned long long b, unsigned long long c,
		      void **pa, void **pb, void **pc)
{
	if (!g.ready) FAIL("not initialised", 0);
	if (ensure(&g.A, a, 0) || ensure(&g.B, b, 0) || ensure(&g.C, c, 1))
		return -1;
	if (pa) *pa = g.A.p;
	if (pb) *pb = g.B.p;
	if (pc) *pc = g.C.p;
	return 0;
}

int xmx_gemm_batched(unsigned M, unsigned N, unsigned K, unsigned batch,
		     unsigned sa, unsigned sb, unsigned sc, unsigned bt)
{
	if (!g.pipeb) FAIL("batched pipeline not built", 0);
	VkDeviceSize sza = (VkDeviceSize)batch * sa * 2, szb = (VkDeviceSize)batch * sb * 2,
		     szc = (VkDeviceSize)batch * sc * 4;
	VkDescriptorBufferInfo dbi[3] = { { g.A.b, 0, sza }, { g.B.b, 0, szb }, { g.C.b, 0, szc } };
	VkWriteDescriptorSet w[3];
	for (int i = 0; i < 3; i++)
		w[i] = (VkWriteDescriptorSet){ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = g.set,
					       .dstBinding = i, .descriptorCount = 1,
					       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dbi[i] };
	vkUpdateDescriptorSets(g.dev, 3, w, 0, NULL);

	vkResetCommandBuffer(g.cb, 0);
	VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
					.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	vkBeginCommandBuffer(g.cb, &bi);
	vkCmdBindPipeline(g.cb, VK_PIPELINE_BIND_POINT_COMPUTE, g.pipeb);
	vkCmdBindDescriptorSets(g.cb, VK_PIPELINE_BIND_POINT_COMPUTE, g.plb, 0, 1, &g.set, 0, NULL);
	unsigned push[7] = { M, N, K, sa, sb, sc, bt };
	vkCmdPushConstants(g.cb, g.plb, VK_SHADER_STAGE_COMPUTE_BIT, 0, 28, push);
	vkCmdDispatch(g.cb, N / 16, M / 8, batch);
	vkEndCommandBuffer(g.cb);
	vkResetFences(g.dev, 1, &g.fence);
	VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &g.cb };
	qlock();
	VkResult r = vkQueueSubmit(g.q, 1, &si, g.fence);
	qunlock();
	if (r) FAIL("submit", r);
	r = vkWaitForFences(g.dev, 1, &g.fence, VK_TRUE, 60ull * 1000000000ull);
	if (r) FAIL("fence wait", r);
	return 0;
}

/* ------------------------------------------------------------------------- */
/* The resident runtime.                                                      */
/*                                                                            */
/* Operands are 64-bit device addresses in the push constants rather than      */
/* descriptor bindings, so recording is just push-and-dispatch and a whole     */
/* block's dispatches go into one command buffer with one fence at the end,    */
/* instead of one submit per GEMM. Activations stay in device buffers between  */
/* passes; the point is not a faster kernel but the traffic that disappears.   */
/* ------------------------------------------------------------------------- */

int xmx_res_init(const char *gemm_spv, const char *unary_spv, const char *row_spv,
		 const char *history_spv, const char *tiled_spv, const char *staged_spv)
{
	if (!g.ready) FAIL("not initialised", 0);
	if (g.rready) return 0;
	VkPushConstantRange pcr = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = sizeof(struct push) };
	VkPipelineLayoutCreateInfo pli = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
					   .pushConstantRangeCount = 1, .pPushConstantRanges = &pcr };
	VkResult r = vkCreatePipelineLayout(g.dev, &pli, NULL, &g.rpl);
	if (r) FAIL("resident pipeline layout", r);
	const char *paths[] = { gemm_spv, tiled_spv, staged_spv, unary_spv, row_spv };
	for (unsigned i = 0; i < 5; i++) {
		g.rpaths[i] = nr_strdup(paths[i]);
		if (!g.rpaths[i]) FAIL("pipeline path allocation", 0);
	}
	const char *spec = getenv("XMX_SPECIALIZE");
	g.specialize = spec ? (unsigned)atoi(spec) : 7;
	if (g.specialize > 7) FAIL("XMX_SPECIALIZE must be in 0..7", 0);
	if (build_pipeline(gemm_spv, g.rpl, &g.rgemm) || build_pipeline(unary_spv, g.rpl, &g.runary)
	    || build_pipeline(row_spv, g.rpl, &g.rrow)
	    || build_pipeline(history_spv, g.rpl, &g.rhistory)
	    || build_pipeline(tiled_spv, g.rpl, &g.rtiled)
	    || build_pipeline(staged_spv, g.rpl, &g.rstaged))
		return -1;
	const char *tile = getenv("XMX_TILE_K");
	g.tiling = tile ? atoi(tile) : 1;
	g.tilem = block_size("XMX_TILE_M", 16);
	g.tilen = block_size("XMX_TILE_N", 32);
	const char *sk = getenv("XMX_STAGE_K");
	g.staging = sk ? (unsigned)atoi(sk) : 128;
	VkCommandBufferAllocateInfo cba = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					    .commandPool = g.cpool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					    .commandBufferCount = 1 };
	if ((r = vkAllocateCommandBuffers(g.dev, &cba, &g.rcb))) FAIL("resident command buffer", r);
	if ((r = vkAllocateCommandBuffers(g.dev, &cba, &g.tcb))) FAIL("transfer command buffer", r);
	VkFenceCreateInfo fi = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	if ((r = vkCreateFence(g.dev, &fi, NULL, &g.rfence))) FAIL("resident fence", r);
	if ((r = vkCreateFence(g.dev, &fi, NULL, &g.tfence))) FAIL("transfer fence", r);
	g.unmapped = want_unmapped();
	g.rready = 1;
	return 0;
}

/* `kind`: 0 the graph's own buffers, 1 a buffer the host reads back, 2 one it writes.
 *
 * Kinds 1 and 2 are always mapped — they exist to be touched by the host, and on an
 * unmapped device they are the staging the graph copies through. Kind 0 follows the
 * device: mapped where the host can see the card's memory, device-local and unmapped
 * where it cannot. */
int xmx_buf_create_kind(unsigned long long bytes, int kind)
{
	if (!g.rready) FAIL("resident runtime not initialised", 0);
	int id = -1;
	for (int i = 0; i < MAX_RBUF; i++)
		if (!rbufs[i].live) { id = i; break; }
	if (id < 0) FAIL("out of buffer slots", 0);
	struct rbuf *rb = &rbufs[id];
	VkBufferCreateInfo bi = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = bytes ? bytes : 4,
				  .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					   | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
					   | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT };
	VkResult r = vkCreateBuffer(g.dev, &bi, NULL, &rb->b);
	if (r) FAIL("vkCreateBuffer (resident)", r);
	VkMemoryRequirements mr;
	vkGetBufferMemoryRequirements(g.dev, rb->b, &mr);
	int unmapped = kind == 0 && g.unmapped;
	const char *failure = unmapped ? "no device-local memory type"
				       : "no host-visible memory type";
	uint32_t mt = unmapped ? memtype_device(mr.memoryTypeBits)
			       : memtype(mr.memoryTypeBits,
					 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
					 | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, kind == 1);
	if (mt == UINT32_MAX) goto failed_kind;
	VkMemoryAllocateFlagsInfo fl = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
					 .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT };
	VkMemoryAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = &fl,
				    .allocationSize = mr.size, .memoryTypeIndex = mt };
	failure = "vkAllocateMemory (resident)";
	if ((r = vkAllocateMemory(g.dev, &ai, NULL, &rb->m))) goto failed_kind;
	failure = "vkBindBufferMemory (resident)";
	if ((r = vkBindBufferMemory(g.dev, rb->b, rb->m, 0))) goto failed_kind;
	if (!unmapped) {
		failure = "vkMapMemory (resident)";
		if ((r = vkMapMemory(g.dev, rb->m, 0, VK_WHOLE_SIZE, 0, &rb->p))) goto failed_kind;
	}
	rb->mapped = !unmapped;
	VkBufferDeviceAddressInfo ai2 = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = rb->b };
	rb->addr = vkGetBufferDeviceAddress(g.dev, &ai2);
	rb->size = bi.size;
	rb->live = 1;
	return id;
failed_kind:
	if (rb->p) vkUnmapMemory(g.dev, rb->m);
	if (rb->b) vkDestroyBuffer(g.dev, rb->b, NULL);
	if (rb->m) vkFreeMemory(g.dev, rb->m, NULL);
	*rb = (struct rbuf){ 0 };
	FAIL(failure, r);
}

int xmx_buf_create(unsigned long long bytes) { return xmx_buf_create_kind(bytes, 0); }

int xmx_buf_host_visible(int id)
{
	return (id >= 0 && id < MAX_RBUF && rbufs[id].live && rbufs[id].mapped) ? 1 : 0;
}

int xmx_staging_mode(void) { return g.unmapped; }

/* One growable host-visible buffer, for the transfers that happen once rather than every
 * frame — the weights, and anything a test reads back. Per-frame traffic does not come
 * through here: it is recorded into the graph as a copy, so a frame is still one submit. */
static int submit_commands(VkCommandBuffer commands, int passes);

/* A transfer runs on its own command buffer and fence so it can happen while the frame's
 * graph is still being recorded, which is when the weights arrive. */
static int submit_transfer(void)
{
	VkResult r = vkEndCommandBuffer(g.tcb);
	if (r) FAIL("end transfer", r);
	if ((r = vkResetFences(g.dev, 1, &g.tfence))) FAIL("reset transfer fence", r);
	VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1,
			    .pCommandBuffers = &g.tcb };
	qlock();
	r = vkQueueSubmit(g.q, 1, &si, g.tfence);
	qunlock();
	if (r) FAIL("transfer submit", r);
	if ((r = vkWaitForFences(g.dev, 1, &g.tfence, VK_TRUE, 60ull * 1000000000ull)))
		FAIL("transfer fence", r);
	return 0;
}

static int begin_transfer(void)
{
	VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
					.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	vkResetCommandBuffer(g.tcb, 0);
	VkResult r = vkBeginCommandBuffer(g.tcb, &bi);
	if (r) FAIL("begin transfer", r);
	return 0;
}

static int ensure_stage(VkDeviceSize size)
{
	if (g.stage.cap >= size) return 0;
	if (g.stage.b) {
		vkUnmapMemory(g.dev, g.stage.m);
		vkDestroyBuffer(g.dev, g.stage.b, NULL);
		vkFreeMemory(g.dev, g.stage.m, NULL);
		g.stage = (struct buf){ 0 };
	}
	VkBufferCreateInfo bi = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size,
				  .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
					   | VK_BUFFER_USAGE_TRANSFER_DST_BIT };
	VkResult r = vkCreateBuffer(g.dev, &bi, NULL, &g.stage.b);
	if (r) FAIL("vkCreateBuffer (stage)", r);
	VkMemoryRequirements mr;
	vkGetBufferMemoryRequirements(g.dev, g.stage.b, &mr);
	uint32_t mt = memtype(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 1);
	if (mt == UINT32_MAX) FAIL("no host-visible staging type", 0);
	VkMemoryAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				    .allocationSize = mr.size, .memoryTypeIndex = mt };
	if ((r = vkAllocateMemory(g.dev, &ai, NULL, &g.stage.m))) FAIL("vkAllocateMemory (stage)", r);
	if ((r = vkBindBufferMemory(g.dev, g.stage.b, g.stage.m, 0))) FAIL("vkBindBufferMemory (stage)", r);
	if ((r = vkMapMemory(g.dev, g.stage.m, 0, VK_WHOLE_SIZE, 0, &g.stage.p))) FAIL("vkMapMemory (stage)", r);
	g.stage.cap = size;
	return 0;
}

static int stage_copy(int to_device, int id, const void *src, void *dst,
		      unsigned long long offset, unsigned long long bytes)
{
	if (id < 0 || id >= MAX_RBUF || !rbufs[id].live) FAIL("buffer is not live", 0);
	if (offset + bytes > rbufs[id].size) FAIL("transfer runs past the buffer", 0);
	if (!bytes) return 0;
	if (rbufs[id].mapped) {
		unsigned char *p = (unsigned char *)rbufs[id].p + offset;
		if (to_device) memcpy(p, src, (size_t)bytes);
		else memcpy(dst, p, (size_t)bytes);
		return 0;
	}
	if (ensure_stage((VkDeviceSize)bytes)) return -1;
	if (to_device) memcpy(g.stage.p, src, (size_t)bytes);
	if (begin_transfer()) return -1;
	VkBufferCopy region = { .srcOffset = to_device ? 0 : offset,
				.dstOffset = to_device ? offset : 0, .size = bytes };
	if (to_device) vkCmdCopyBuffer(g.tcb, g.stage.b, rbufs[id].b, 1, &region);
	else vkCmdCopyBuffer(g.tcb, rbufs[id].b, g.stage.b, 1, &region);
	if (submit_transfer()) return -1;
	if (!to_device) memcpy(dst, g.stage.p, (size_t)bytes);
	return 0;
}

/* Clearing a buffer the host cannot address is the device's job, and it has to work
 * whether or not a graph is being recorded: the scratch arena allocates and clears its
 * roles lazily, in the middle of the recording that is about to use them. */
static int stage_copy(int to_device, int id, const void *src, void *dst,
		      unsigned long long offset, unsigned long long bytes);

int xmx_buf_zero(int id)
{
	if (id < 0 || id >= MAX_RBUF || !rbufs[id].live) FAIL("buffer is not live", 0);
	if (rbufs[id].mapped) {
		memset(rbufs[id].p, 0, (size_t)rbufs[id].size);
		return 0;
	}
	/* Never into the graph, even while one is being recorded: a recorded fill runs again
	 * on every replay, and `buffer_from` zeroes a weight buffer's padding *before*
	 * uploading the weights — so the graph would wipe them on its way past, every frame.
	 * Clearing means now, on the transfer queue, like the memset it replaces. */
	if (rbufs[id].size & 3) {
		/* vkCmdFillBuffer works in whole words and may not run past the buffer, so a
		 * size that is not a multiple of four would leave a tail dirty. Rare enough to
		 * pay for with a copy rather than a second code path. */
		unsigned char *zeros = calloc(1, (size_t)rbufs[id].size);
		if (!zeros) FAIL("out of memory clearing a buffer", 0);
		int bad = stage_copy(1, id, zeros, NULL, 0, rbufs[id].size);
		free(zeros);
		return bad;
	}
	if (begin_transfer()) return -1;
	vkCmdFillBuffer(g.tcb, rbufs[id].b, 0, rbufs[id].size, 0);
	return submit_transfer();
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
	return (id >= 0 && id < MAX_RBUF && rbufs[id].live) ? (unsigned long long)rbufs[id].size : 0;
}

int xmx_buf_destroy(int id)
{
	if (id < 0 || id >= MAX_RBUF || !rbufs[id].live) return 0;
	if (rbufs[id].mapped) vkUnmapMemory(g.dev, rbufs[id].m);
	vkDestroyBuffer(g.dev, rbufs[id].b, NULL);
	vkFreeMemory(g.dev, rbufs[id].m, NULL);
	rbufs[id] = (struct rbuf){ 0 };
	return 0;
}

static VkDeviceAddress addr_of(int id)
{
	return (id >= 0 && id < MAX_RBUF && rbufs[id].live) ? rbufs[id].addr : 0;
}

/* Close a pass with a timestamp. Bottom-of-pipe, so it lands after the dispatch has
 * finished rather than after it was issued. Costs one query per pass and nothing at all
 * when profiling is off, which is why it can live on the hot path. */
static void stamp(unsigned family, unsigned subkind)
{
	if (!g.prof || !g.qpool || g.prof_n >= MAX_STAMPS) return;
	stamp_kind[g.prof_n] = (unsigned char)(family * 32u + (subkind & 31u));
	vkCmdWriteTimestamp(g.rcb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, g.qpool, g.prof_n);
	g.prof_n++;
}

int xmx_begin(void)
{
	if (!g.rready) FAIL("resident runtime not initialised", 0);
	if (g.recording) FAIL("already recording", 0);
	vkResetCommandBuffer(g.rcb, 0);
	/* Captured graphs are replayed, so ONE_TIME_SUBMIT is deliberately absent. */
	VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	VkResult r = vkBeginCommandBuffer(g.rcb, &bi);
	if (r) FAIL("begin resident recording", r);
	g.recording = 1;
	g.recorded = 0;
	g.syncing = 1;
	g.prof_n = 0;
	if (g.prof && g.qpool) {
		vkCmdResetQueryPool(g.rcb, g.qpool, 0, MAX_STAMPS);
		stamp(PK_START, 0);       /* the zero point every later stamp is measured from */
	}
	return 0;
}

int xmx_abort(void)
{
	if (!g.recording) return 0;
	VkResult r = vkResetCommandBuffer(g.rcb, 0);
	if (r) FAIL("abort resident recording", r);
	g.recording = 0;
	return 0;
}

/* One global barrier between passes. Most passes consume the previous one's output,
 * so this is right by default; where a run of dispatches is known to be independent —
 * the per-head GEMMs of a branched feed-forward write disjoint slices, the three head
 * splits read one buffer and write three — `xmx_sync(0)` suppresses it and `xmx_sync(1)`
 * closes the run with a single barrier. */
static void barrier(void)
{
	if (!g.syncing) return;
	VkMemoryBarrier mb = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			       .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
			       .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT };
	vkCmdPipelineBarrier(g.rcb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
}

/* Copy a skip without a host fence. Both barriers cover execution dependencies
 * (including write-after-read), and make writes visible between compute/transfer. */
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
	VkMemoryBarrier before = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT };
	vkCmdPipelineBarrier(g.rcb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &before, 0, NULL, 0, NULL);
	VkBufferCopy region = { .srcOffset = source_offset, .dstOffset = target_offset, .size = bytes };
	vkCmdCopyBuffer(g.rcb, a->b, b->b, 1, &region);
	VkMemoryBarrier after = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
				 VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT };
	vkCmdPipelineBarrier(g.rcb, VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 1, &after, 0, NULL, 0, NULL);
	stamp(PK_COPY, 0);
	g.recorded++;
	return 0;
}

/* Suppress or restore the barrier between dispatches. Restoring emits one, closing the
 * independent run. */
int xmx_sync(int on)
{
	if (!g.recording) FAIL("not recording", 0);
	int was = g.syncing;
	g.syncing = on;
	if (on && !was) barrier();
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
	/* Element offsets are folded into the addresses, so a sub-matrix needs no shader
	 * support: A and B are half, and C is float unless the epilogue narrows it. */
	p.a += (uint64_t)oa * 2; p.b += (uint64_t)ob * 2;
	p.c += (uint64_t)oc * ((bt & 0x1000u) ? 2 : 4);
	/* The register-tiled kernel keeps a 16x32 block of the output in one subgroup's
	 * registers, which needs both extents to be a whole block; the 8x16 kernel takes
	 * everything else. Slice writes make the N test exact rather than conservative —
	 * a tile past the slice would land in the neighbouring one.
	 *
	 * 16x32 and not larger: a 32x32 block halves the workgroup count again and is
	 * *slower* in a frame, because the shapes here then stop having enough workgroups
	 * to fill the machine. It still wins in isolation, which is why the two disagree.
	 * The environment overrides exist so that trade can be re-measured. */
	/* Operand staging pays only where there is reuse to amortise it. It wins up to
	 * 24 % on a deep-K shape in isolation and loses half as much again on the shallow-K
	 * ones that dominate this graph, where the output write is the cost and there is
	 * nothing to reuse; K >= 128 is where it stops losing. Over a whole frame the two
	 * are indistinguishable — see notes/phase22-staging-and-storage.md. */
	int staged = M % 64 == 0 && N % 32 == 0 && K % 32 == 0 && K >= g.staging;
	/* The staged kernel has no portable twin — its 128-lane 64x32 geometry exists to feed
	 * matrix units — so without them every shape goes to the 8x16 and 16x32 kernels,
	 * whose portable builds take the same dispatch. */
	if (g.portable) staged = 0;
	int tiled = M % g.tilem == 0 && N % g.tilen == 0 && K >= g.tiling;
	VkPipeline pipeline;
	if (resident_pipeline(staged ? 2 : (tiled ? 1 : 0), bt,
			      staged ? g.rstaged : (tiled ? g.rtiled : g.rgemm), &pipeline)) return -1;
	vkCmdBindPipeline(g.rcb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	vkCmdPushConstants(g.rcb, g.rpl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof p, &p);
	if (staged) {
		vkCmdDispatch(g.rcb, N / 32, M / 64, batch ? batch : 1);
	} else {
		if (tiled) vkCmdDispatch(g.rcb, N / g.tilen, M / g.tilem, batch ? batch : 1);
		else       vkCmdDispatch(g.rcb, (N + 15) / 16, (M + 7) / 8, batch ? batch : 1);
	}
	barrier();
	stamp(staged ? PK_STAGED : (tiled ? PK_TILED : PK_GEMM), bt);
	g.recorded++;
	return 0;
}

int xmx_rec_unary(unsigned kind, int a, int b, int c, int d, unsigned n, unsigned channels,
		  float p0, unsigned batch, unsigned sa, unsigned sb, unsigned sc, unsigned k)
{
	if (!g.recording) FAIL("not recording", 0);
	struct push p = { .a = addr_of(a), .b = addr_of(b), .c = addr_of(c), .d = addr_of(d),
			  .m = n, .n = channels, .flags = kind, .p0 = p0,
			  .batch = batch, .sa = sa, .sb = sb, .sc = sc, .k = k };
	if (!p.a || !p.c) FAIL("unary operand is not a live buffer", 0);
	VkPipeline pipeline;
	if (resident_pipeline(3, kind, g.runary, &pipeline)) return -1;
	vkCmdBindPipeline(g.rcb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	vkCmdPushConstants(g.rcb, g.rpl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof p, &p);
	vkCmdDispatch(g.rcb, (n + 255) / 256, 1, 1);
	barrier();
	stamp(PK_UNARY, kind);
	g.recorded++;
	return 0;
}

/* Row-wise passes: the cosine publish reduces 32 channels through the kernel's own
 * fragment tree, the softmax reduces a window's tokens. One invocation per row. */
int xmx_rec_row(unsigned kind, int a, int b, int c, int d, unsigned rows, unsigned width,
		unsigned heads, unsigned scaled, unsigned stride, float cap)
{
	if (!g.recording) FAIL("not recording", 0);
	struct push p = { .a = addr_of(a), .b = addr_of(b), .c = addr_of(c), .d = addr_of(d),
			  .m = rows, .n = width, .k = scaled, .batch = heads, .flags = kind,
			  .sa = stride, .p0 = cap };
	if (!p.a || !p.c) FAIL("row operand is not a live buffer", 0);
	VkPipeline pipeline;
	if (resident_pipeline(4, kind, g.rrow, &pipeline)) return -1;
	vkCmdBindPipeline(g.rcb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	vkCmdPushConstants(g.rcb, g.rpl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof p, &p);
	vkCmdDispatch(g.rcb, (rows + 31) / 32, 1, 1);
	barrier();
	stamp(PK_ROW, kind);
	g.recorded++;
	return 0;
}

/* The temporal path's five-tap reprojection: one invocation per pixel. */
int xmx_rec_history(int history, int motion, int out, unsigned pixels, unsigned channels,
		    unsigned height, unsigned width, unsigned absolute)
{
	if (!g.recording) FAIL("not recording", 0);
	struct push p = { .a = addr_of(history), .b = addr_of(motion), .c = addr_of(out),
			  .m = pixels, .n = channels, .k = height, .batch = width,
			  .flags = absolute };
	if (!p.a || !p.b || !p.c) FAIL("history operand is not a live buffer", 0);
	vkCmdBindPipeline(g.rcb, VK_PIPELINE_BIND_POINT_COMPUTE, g.rhistory);
	vkCmdPushConstants(g.rcb, g.rpl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof p, &p);
	vkCmdDispatch(g.rcb, (pixels + 63) / 64, 1, 1);
	barrier();
	stamp(PK_HISTORY, absolute & 1u);
	g.recorded++;
	return 0;
}

/* Read the timestamps back and add each pass to its kind's running total. Called only
 * once the fence has signalled, so every query is available; WAIT is passed anyway
 * because a driver may still report a query as not-ready immediately after. */
static void collect(unsigned stamps, const unsigned char *kinds)
{
	if (!g.prof || !g.qpool || stamps < 2) return;
	static uint64_t ticks[MAX_STAMPS];
	if (vkGetQueryPoolResults(g.dev, g.qpool, 0, stamps, sizeof ticks[0] * stamps, ticks,
				  sizeof ticks[0],
				  VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) != VK_SUCCESS)
		return;
	for (unsigned i = 1; i < stamps; i++) {
		if (ticks[i] < ticks[i - 1]) continue;      /* a wrapped counter is not a duration */
		prof_ms[kinds[i]] += (double)(ticks[i] - ticks[i - 1]) * g.ts_period * 1e-6;
		prof_hits[kinds[i]]++;
	}
}

static int submit_commands(VkCommandBuffer commands, int passes)
{
	VkResult r = vkResetFences(g.dev, 1, &g.rfence);
	if (r) FAIL("reset resident fence", r);
	VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1,
			    .pCommandBuffers = &commands };
	qlock();
	r = vkQueueSubmit(g.q, 1, &si, g.rfence);
	qunlock();
	if (r) FAIL("resident submit", r);
	if ((r = vkWaitForFences(g.dev, 1, &g.rfence, VK_TRUE, 60ull * 1000000000ull)))
		FAIL("resident fence wait", r);
	return passes;
}

int xmx_submit(void)
{
	if (!g.recording) FAIL("not recording", 0);
	g.recording = 0;
	VkResult r = vkEndCommandBuffer(g.rcb);
	if (r) FAIL("end resident recording", r);
	int passes = submit_commands(g.rcb, g.recorded);
	if (passes >= 0) collect(g.prof_n, stamp_kind);
	return passes;
}

/* A graph owns its command buffer; later recording (including temporal history)
 * uses a different one. Callers keep referenced buffers alive until graph_destroy. */
int xmx_graph_capture(void)
{
	if (!g.recording) FAIL("not recording", 0);
	int id;
	for (id = 0; id < MAX_GRAPHS && graphs[id].commands; id++);
	if (id == MAX_GRAPHS) FAIL("out of graph slots", 0);
	VkCommandBuffer replacement;
	VkCommandBufferAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = g.cpool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
	VkResult r = vkAllocateCommandBuffers(g.dev, &ai, &replacement);
	if (r) FAIL("graph command buffer", r);
	r = vkEndCommandBuffer(g.rcb);
	if (r) { vkFreeCommandBuffers(g.dev, g.cpool, 1, &replacement); FAIL("end graph recording", r); }
	graphs[id].commands = g.rcb;
	graphs[id].passes = g.recorded;
	graphs[id].stamps = g.prof_n;
	free(graphs[id].kinds);
	graphs[id].kinds = NULL;
	if (g.prof_n) {
		graphs[id].kinds = malloc(g.prof_n);
		if (graphs[id].kinds) memcpy(graphs[id].kinds, stamp_kind, g.prof_n);
		else graphs[id].stamps = 0;
	}
	g.rcb = replacement;
	g.recording = 0;
	return id;
}

int xmx_graph_run(int id)
{
	if (g.recording) FAIL("cannot replay during recording", 0);
	if (id < 0 || id >= MAX_GRAPHS || !graphs[id].commands) FAIL("graph is not live", 0);
	int passes = submit_commands(graphs[id].commands, graphs[id].passes);
	if (passes >= 0 && graphs[id].kinds) collect(graphs[id].stamps, graphs[id].kinds);
	return passes;
}

/* Turn GPU-side profiling on or off.
 *
 * Off by default and free when off: `stamp()` returns on the first test. On, every
 * recorded pass gains one timestamp query, which is a command-buffer write and not a
 * synchronisation point, so the frame it measures is the frame that would have run.
 *
 * Returns 0, or -1 if the queue family cannot timestamp at all — some do not, and a
 * silent zero would be worse than a refusal.
 */
int xmx_profile(int on)
{
	if (!on) { g.prof = 0; return 0; }
	if (!g.dev) FAIL("profiling needs an initialised device", 0);
	if (!g.qpool) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(g.pd, &props);
		if (props.limits.timestampPeriod == 0.0f)
			FAIL("this device does not support timestamps", 0);
		uint32_t families = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(g.pd, &families, NULL);
		VkQueueFamilyProperties *qf = malloc(families * sizeof *qf);
		if (!qf) FAIL("queue family properties", 0);
		vkGetPhysicalDeviceQueueFamilyProperties(g.pd, &families, qf);
		uint32_t bits = g.qi < families ? qf[g.qi].timestampValidBits : 0;
		free(qf);
		if (!bits) FAIL("this queue family cannot write timestamps", 0);
		g.ts_period = props.limits.timestampPeriod;
		VkQueryPoolCreateInfo qi = { .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
					     .queryType = VK_QUERY_TYPE_TIMESTAMP,
					     .queryCount = MAX_STAMPS };
		VkResult r = vkCreateQueryPool(g.dev, &qi, NULL, &g.qpool);
		if (r) FAIL("timestamp query pool", r);
	}
	g.prof = 1;
	return 0;
}

void xmx_profile_reset(void)
{
	memset(prof_ms, 0, sizeof prof_ms);
	memset(prof_hits, 0, sizeof prof_hits);
}

/* Milliseconds and pass count for one kind, where a kind is `family * 32 + subkind`.
 * Reading a kind that never ran gives 0, which is the honest answer. */
double xmx_profile_ms(unsigned kind)
{
	return kind < PROF_KINDS ? prof_ms[kind] : 0.0;
}

unsigned xmx_profile_count(unsigned kind)
{
	return kind < PROF_KINDS ? prof_hits[kind] : 0u;
}

int xmx_graph_destroy(int id)
{
	if (id < 0 || id >= MAX_GRAPHS || !graphs[id].commands) return 0;
	vkFreeCommandBuffers(g.dev, g.cpool, 1, &graphs[id].commands);
	graphs[id].commands = VK_NULL_HANDLE;
	free(graphs[id].kinds);
	graphs[id].kinds = NULL;
	graphs[id].stamps = 0;
	return 0;
}
