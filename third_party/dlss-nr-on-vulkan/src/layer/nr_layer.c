/*
 * nr_layer — a Vulkan layer that hands the presented frame to DLSS-NR.
 *
 * Why a Vulkan layer, and not the route everyone else takes.
 *
 * Every published way of getting DLSS-NR into a game — OptiScaler's fork, the
 * ReShade bridges, the dual-GPU MGPU Bridge — loads NVIDIA's own nvngx_dlssnr.dll
 * and therefore needs an NVIDIA GPU (or, for the AMD lab, a PTX translation of it).
 * None of them can run here. What we have instead is a reimplementation that is
 * already Vulkan compute.
 *
 * And under Proton a DX12 game goes DX12 -> VKD3D-Proton -> Vulkan on ANV, which is
 * the same driver our shaders run on. So the frame we want is already a VkImage on
 * the device we already use: no D3D interop, no Windows DLL, no PCIe transfer. A
 * layer at vkQueuePresentKHR sees it. vkBasalt has done exactly this on Linux for
 * years, so the shape is proven.
 *
 * This file is the capture half: chain into the loader, force TRANSFER_SRC onto the
 * swapchain images, and copy the presented frame out on demand.
 *
 * Build: cc -O2 -shared -fPIC -o libnr_layer.so nr_layer.c -lvulkan
 */
#if defined(__linux__) && !defined(__ANDROID__) && !defined(_WIN32)
#define VK_USE_PLATFORM_XLIB_KHR   /* pulls in X11 headers, which macOS and Android do not have */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/time.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>

#define MAX_SWAPCHAINS 8
#define MAX_IMAGES 8

#ifdef _WIN32
#include <windows.h>
#endif

/* How the layer knows the game's frame is finished, and the game knows ours is.
 *
 * `idle` is what this started as: `vkQueueWaitIdle` around the transfer, which is a
 * sledgehammer — it waits for everything the game has submitted, not for the one copy we
 * care about, and it ignores the present's own semaphores entirely, so a game that renders
 * on one queue and presents on another can hand over an unfinished image. It was written
 * for a photo mode, where the stall is the point.
 *
 * `semaphore` is the proper route the comment on `transfer` has asked for since: our submit
 * waits on the present's semaphores, the present waits on ours instead, and only the
 * readback — which the host has to look at — waits on a fence of its own.
 *
 * Default `idle` until somebody has run `semaphore` through a real game on both a discrete
 * card and this iGPU. One environment variable either way. */
#define SYNC_SLOTS 4
/* How many of a present's wait semaphores this layer is prepared to take over. Taking
 * *some* of them is not an option: the ones left behind would stay signalled with nothing
 * left to consume them, since the present is redirected onto ours. A present that brings
 * more than this keeps all of its own and is handled the old way. */
#define NR_MAX_WAITS 16
static int sync_semaphores;

struct device_data {
	VkDevice device;
	VkPhysicalDevice physical;
	VkQueue queue;
	uint32_t queue_family;
	VkCommandPool pool;
	VkDeviceMemory staging_memory;
	VkBuffer staging;
	VkDeviceSize staging_size;
	void *mapped;
	unsigned char *result;          /* the processed frame, held while the trigger is up */
	VkDeviceSize result_size;
	int holding;
	/* One frame captured before the one being processed, so the daemon can be told
	 * which pixels did not move. Filled on the present after the trigger goes up and
	 * released when it goes down; nothing is copied while the trigger is down, so a
	 * game that never triggers pays nothing for this. */
	unsigned char *earlier;
	VkDeviceSize earlier_size;
	/* Present-time synchronisation, when `NR_LAYER_SYNC=semaphore`. A ring, because the
	 * write-back is never waited for: its command buffer, its fence and the semaphore it
	 * signals all have to stay untouched until the present that consumes them is through.
	 * Four is enough for any swapchain we have seen and costs three objects each. */
	VkCommandBuffer ring_commands[SYNC_SLOTS];
	VkFence ring_fence[SYNC_SLOTS];
	VkSemaphore ring_done[SYNC_SLOTS];
	int ring_used[SYNC_SLOTS];
	unsigned ring_next;
	int ring_ready;
	/* what the present being handled has handed over, and what it must wait on instead */
	const VkSemaphore *present_wait;
	uint32_t present_wait_count;
	VkSemaphore present_signal;
	int present_plain;                     /* this present keeps its own semaphores */
	int have_earlier;
	unsigned char *outgoing;        /* colour followed by the mask, for one send */
	VkDeviceSize outgoing_size;
	PFN_vkGetDeviceProcAddr get_device_proc;
	PFN_vkQueuePresentKHR present;
	PFN_vkCreateSwapchainKHR create_swapchain;
	PFN_vkDestroySwapchainKHR destroy_swapchain;
	PFN_vkGetDeviceQueue get_device_queue;
	PFN_vkGetDeviceQueue2 get_device_queue2;
	PFN_vkGetSwapchainImagesKHR get_swapchain_images;
	PFN_vkDestroyDevice destroy_device;
};

struct swapchain_data {
	VkSwapchainKHR swapchain;
	VkDevice device;
	VkImage images[MAX_IMAGES];
	uint32_t image_count;
	VkFormat format;
	VkExtent2D extent;
};

static struct device_data devices[8];
static struct swapchain_data swapchains[MAX_SWAPCHAINS];

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static PFN_vkGetInstanceProcAddr next_instance_proc;
static VkInstance layer_instance;
static unsigned long frame_counter;
static const char *capture_path;
static long capture_every;
static long live_every;
static const char *socket_path;
static const char *trigger_path;
static int ui_mask;

/* The frame goes to a daemon over a Unix socket rather than being processed in
 * process: the implementation is Python and this is a shared object living inside the
 * game. For a photo mode the game is meant to stall anyway, so the round trip is free;
 * a per-frame pass would need the graph ported to C. */
/* `reply_size` is deliberately separate from `payload_size`: the interface mask makes
 * the request larger than the answer, and reusing one size meant asking for bytes the
 * daemon never sends — the read hit EOF and every masked frame came back unchanged. */
static int exchange(const void *header, size_t header_size, const void *payload,
		    size_t payload_size, void *reply, size_t reply_size)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) return -1;
	struct timeval timeout = { .tv_sec = 60 };
	if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout) ||
	    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout)) {
		close(fd); return -1;
	}
	struct sockaddr_un address = { .sun_family = AF_UNIX };
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(address.sun_path, sizeof address.sun_path, "%s", socket_path);
#else
	snprintf(address.sun_path, sizeof address.sun_path, "%s", socket_path);
#endif

	if (connect(fd, (struct sockaddr *)&address, sizeof address) < 0) {
		fprintf(stderr, "[nr_layer] no daemon at %s\n", socket_path);
		close(fd);
		return -1;
	}
	const unsigned char *out = header;
	for (size_t sent = 0; sent < header_size; ) {
		ssize_t n = send(fd, out + sent, header_size - sent, MSG_NOSIGNAL);
		if (n < 0 && errno == EINTR) continue;
		if (n <= 0) { close(fd); return -1; }
		sent += (size_t)n;
	}
	out = payload;
	for (size_t sent = 0; sent < payload_size; ) {
		ssize_t n = send(fd, out + sent, payload_size - sent, MSG_NOSIGNAL);
		if (n < 0 && errno == EINTR) continue;
		if (n <= 0) { close(fd); return -1; }
		sent += (size_t)n;
	}
	unsigned char *in = reply;
	for (size_t got = 0; got < reply_size; ) {
		ssize_t n = read(fd, in + got, reply_size - got);
		if (n < 0 && errno == EINTR) continue;
		if (n <= 0) { close(fd); return -1; }
		got += (size_t)n;
	}
	close(fd);
	return 0;
}

static struct device_data *find_device(VkDevice device)
{
	for (int i = 0; i < 8; i++)
		if (devices[i].device == device) return &devices[i];
	return NULL;
}

/* Which device and queue family a VkQueue belongs to.
 *
 * `vkQueuePresentKHR` hands over a queue and nothing else. Taking the first live device
 * and the family of `pQueueCreateInfos[0]` is right for the single-queue case and wrong
 * the moment a game asks for a dedicated present or transfer family, which VKD3D-Proton
 * does: the command buffer would then be allocated from a pool of the wrong family and
 * submitted anyway, which is invalid. So the queues are recorded as they are handed out. */
#define MAX_QUEUES 16
static struct queue_data {
	VkQueue queue;
	VkDevice device;
	uint32_t family;
	int capture_ok;      /* the family can hold a copy, and the queue is not protected */
} queues[MAX_QUEUES];

static struct queue_data *find_queue(VkQueue queue)
{
	for (int i = 0; i < MAX_QUEUES; i++)
		if (queues[i].queue == queue) return &queues[i];
	return NULL;
}

/* Whether a copy can be recorded for this family at all. A present queue is not
 * required to support graphics, compute or transfer — some drivers expose a
 * present-only family — and recording `vkCmdCopyImageToBuffer` on one is invalid.
 * Ported from the parallel ProjectsCodex tree, which had this guard and we did not. */
static int family_can_capture(struct device_data *data, uint32_t family)
{
	if (!data || !next_instance_proc || !layer_instance) return 0;
	PFN_vkGetPhysicalDeviceQueueFamilyProperties properties =
		(PFN_vkGetPhysicalDeviceQueueFamilyProperties)
		next_instance_proc(layer_instance, "vkGetPhysicalDeviceQueueFamilyProperties");
	if (!properties) return 0;
	uint32_t count = 0;
	properties(data->physical, &count, NULL);
	if (!count || family >= count) return 0;
	VkQueueFamilyProperties *families = calloc(count, sizeof *families);
	if (!families) return 0;
	properties(data->physical, &count, families);
	int ok = (families[family].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT
						 | VK_QUEUE_TRANSFER_BIT)) != 0;
	free(families);
	return ok;
}

static void remember_queue(VkDevice device, uint32_t family, VkQueue queue, int capture_ok)
{
	if (!queue) return;
	pthread_mutex_lock(&lock);
	if (!find_queue(queue))
		for (int i = 0; i < MAX_QUEUES; i++)
			if (!queues[i].queue) {
				queues[i] = (struct queue_data){ queue, device, family, capture_ok };
				break;
			}
	pthread_mutex_unlock(&lock);
}

VKAPI_ATTR void VKAPI_CALL nr_GetDeviceQueue(VkDevice device, uint32_t family,
					     uint32_t index, VkQueue *queue)
{
	struct device_data *data = find_device(device);
	if (!data || !data->get_device_queue) return;
	data->get_device_queue(device, family, index, queue);
	remember_queue(device, family, *queue, family_can_capture(data, family));
}

VKAPI_ATTR void VKAPI_CALL nr_GetDeviceQueue2(VkDevice device,
					      const VkDeviceQueueInfo2 *info, VkQueue *queue)
{
	struct device_data *data = find_device(device);
	if (!data || !data->get_device_queue2) return;
	data->get_device_queue2(device, info, queue);
	/* Protected memory cannot be read back into a host-visible buffer, so a protected
	 * queue is never a capture source however capable its family is. */
	remember_queue(device, info->queueFamilyIndex, *queue,
		       (info->flags & VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT)
		       ? 0 : family_can_capture(data, info->queueFamilyIndex));
}

/* Four bytes a pixel is assumed everywhere downstream: the staging buffer is sized
 * `width * height * 4`, `vkCmdCopyImageToBuffer` derives its extent from the image, and
 * the daemon decodes exactly these five formats. An HDR swapchain — R16G16B16A16_SFLOAT
 * is eight — would have the driver copy twice what the buffer holds. So a format that
 * is not on this list is not tracked at all, and the layer stays out of the way. */
static int format_is_four_bytes(VkFormat format)
{
	switch (format) {
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SRGB:
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_SRGB:
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
	case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
		return 1;
	default:
		return 0;
	}
}

static struct swapchain_data *find_swapchain(VkSwapchainKHR swapchain)
{
	for (int i = 0; i < MAX_SWAPCHAINS; i++)
		if (swapchains[i].swapchain == swapchain) return &swapchains[i];
	return NULL;
}

/* The loader hands each layer a chain of get-proc-address functions; these walk it. */
static VkLayerInstanceCreateInfo *instance_chain(const VkInstanceCreateInfo *info)
{
	VkLayerInstanceCreateInfo *item = (VkLayerInstanceCreateInfo *)info->pNext;
	while (item && !(item->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO
			 && item->function == VK_LAYER_LINK_INFO))
		item = (VkLayerInstanceCreateInfo *)item->pNext;
	return item;
}

static VkLayerDeviceCreateInfo *device_chain(const VkDeviceCreateInfo *info)
{
	VkLayerDeviceCreateInfo *item = (VkLayerDeviceCreateInfo *)info->pNext;
	while (item && !(item->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO
			 && item->function == VK_LAYER_LINK_INFO))
		item = (VkLayerDeviceCreateInfo *)item->pNext;
	return item;
}

VKAPI_ATTR VkResult VKAPI_CALL nr_CreateInstance(const VkInstanceCreateInfo *info,
						 const VkAllocationCallbacks *allocator,
						 VkInstance *instance)
{
	VkLayerInstanceCreateInfo *link = instance_chain(info);
	if (!link) return VK_ERROR_INITIALIZATION_FAILED;
	next_instance_proc = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
	link->u.pLayerInfo = link->u.pLayerInfo->pNext;
	PFN_vkCreateInstance create =
		(PFN_vkCreateInstance)next_instance_proc(NULL, "vkCreateInstance");
	VkResult r = create(info, allocator, instance);
	if (r == VK_SUCCESS) {
		/* Physical-device entry points cannot be resolved against a NULL
		 * instance — only the handful of global ones can — so the instance has
		 * to be kept. */
		layer_instance = *instance;
		capture_path = getenv("NR_LAYER_CAPTURE");
		socket_path = getenv("NR_LAYER_SOCKET");
		trigger_path = getenv("NR_LAYER_TRIGGER");
		const char *mask = getenv("NR_LAYER_UI_MASK");
		ui_mask = mask && strcmp(mask, "0") != 0;
		const char *every = getenv("NR_LAYER_EVERY");
		capture_every = every ? strtol(every, NULL, 10) : 0;
		const char *sync = getenv("NR_LAYER_SYNC");
		sync_semaphores = sync && !strcmp(sync, "semaphore");
		const char *live = getenv("NR_LAYER_LIVE");
		live_every = live ? strtol(live, NULL, 10) : 0;
		if (live_every < 0) live_every = 0;
		if (live_every > 0)
			fprintf(stderr, "[nr_layer] live: every %ld%s present goes through the "
				"network, the frames between hold the last result\n",
				live_every, live_every == 1 ? "st" : "th");
		if (live_every > 0 && getenv("NR_LAYER_TRIGGER"))
			fprintf(stderr, "[nr_layer] live runs while the trigger exists; "
				"remove it to hand the game back\n");
		if (sync_semaphores)
			fprintf(stderr, "[nr_layer] sync: the present's own semaphores, not a "
				"queue idle\n");
		fprintf(stderr, "[nr_layer] active; socket=%s trigger=%s capture=%s every=%ld\n",
			socket_path ? socket_path : "(none)",
			trigger_path ? trigger_path : "(none)",
			capture_path ? capture_path : "(off)", capture_every);
		if (ui_mask)
			fprintf(stderr, "[nr_layer] ui mask on: the first present after the "
				"trigger is kept to find what held still\n");
	}
	return r;
}

VKAPI_ATTR VkResult VKAPI_CALL nr_CreateDevice(VkPhysicalDevice physical,
					       const VkDeviceCreateInfo *info,
					       const VkAllocationCallbacks *allocator,
					       VkDevice *device)
{
	VkLayerDeviceCreateInfo *link = device_chain(info);
	if (!link) return VK_ERROR_INITIALIZATION_FAILED;
	PFN_vkGetInstanceProcAddr next_instance = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
	PFN_vkGetDeviceProcAddr next_device = link->u.pLayerInfo->pfnNextGetDeviceProcAddr;
	link->u.pLayerInfo = link->u.pLayerInfo->pNext;

	PFN_vkCreateDevice create = (PFN_vkCreateDevice)next_instance(NULL, "vkCreateDevice");
	VkResult r = create(physical, info, allocator, device);
	if (r != VK_SUCCESS) return r;

	pthread_mutex_lock(&lock);
	struct device_data *data = NULL;
	for (int i = 0; i < 8; i++) if (!devices[i].device) { data = &devices[i]; break; }
	if (data) {
		memset(data, 0, sizeof *data);
		data->device = *device;
		data->physical = physical;
		data->get_device_proc = next_device;
		data->present = (PFN_vkQueuePresentKHR)next_device(*device, "vkQueuePresentKHR");
		data->get_device_queue =
			(PFN_vkGetDeviceQueue)next_device(*device, "vkGetDeviceQueue");
		data->get_device_queue2 =
			(PFN_vkGetDeviceQueue2)next_device(*device, "vkGetDeviceQueue2");
		data->destroy_swapchain =
			(PFN_vkDestroySwapchainKHR)next_device(*device, "vkDestroySwapchainKHR");
		data->create_swapchain =
			(PFN_vkCreateSwapchainKHR)next_device(*device, "vkCreateSwapchainKHR");
		data->get_swapchain_images =
			(PFN_vkGetSwapchainImagesKHR)next_device(*device, "vkGetSwapchainImagesKHR");
		data->destroy_device = (PFN_vkDestroyDevice)next_device(*device, "vkDestroyDevice");
		data->queue_family = info->queueCreateInfoCount
			? info->pQueueCreateInfos[0].queueFamilyIndex : 0;
	}
	pthread_mutex_unlock(&lock);
	return r;
}

/* The swapchain images are the frame we want, so they have to be copyable. The loader
 * lets a layer edit the create info on the way down; without TRANSFER_SRC the copy
 * below is invalid, and this is the one place it can be added. */
VKAPI_ATTR VkResult VKAPI_CALL nr_CreateSwapchainKHR(VkDevice device,
						     const VkSwapchainCreateInfoKHR *info,
						     const VkAllocationCallbacks *allocator,
						     VkSwapchainKHR *swapchain)
{
	struct device_data *data = find_device(device);
	if (!data) return VK_ERROR_INITIALIZATION_FAILED;
	const VkImageUsageFlags copies = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
				       | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	VkSwapchainCreateInfoKHR patched = *info;
	patched.imageUsage |= copies;
	int copyable = 1;
	VkResult r = data->create_swapchain(device, &patched, allocator, swapchain);
	if (r != VK_SUCCESS) {
		/* The surface may refuse transfer usage. Falling back keeps the game alive,
		 * but the images then lack TRANSFER_SRC and copying from them is invalid
		 * usage — so the swapchain is created and deliberately not tracked. Before
		 * this the fallback silently left us issuing an illegal copy every frame;
		 * the parallel ProjectsCodex tree checks the flags and we did not. */
		copyable = (info->imageUsage & copies) == copies;
		r = data->create_swapchain(device, info, allocator, swapchain);
	}
	if (r != VK_SUCCESS) return r;
	if (!copyable) {
		fprintf(stderr, "[nr_layer] swapchain refused transfer usage; capture off\n");
		return r;
	}
	/* One layer, and not protected: the copy reads a single 2-D image, and protected
	 * memory cannot reach a host-visible buffer at all. */
	if (info->imageArrayLayers != 1
	    || (info->flags & VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR)) {
		fprintf(stderr, "[nr_layer] swapchain is multi-layer or protected; capture off\n");
		return r;
	}

	if (!format_is_four_bytes(info->imageFormat)) {
		fprintf(stderr, "[nr_layer] swapchain format %d is not four bytes a pixel; "
			"leaving it alone\n", info->imageFormat);
		return r;
	}
	pthread_mutex_lock(&lock);
	struct swapchain_data *entry = NULL;
	for (int i = 0; i < MAX_SWAPCHAINS; i++)
		if (!swapchains[i].swapchain) { entry = &swapchains[i]; break; }
	if (!entry)
		fprintf(stderr, "[nr_layer] all %d swapchain slots are in use; this one is "
			"not tracked\n", MAX_SWAPCHAINS);
	if (entry) {
		memset(entry, 0, sizeof *entry);
		entry->swapchain = *swapchain;
		entry->device = device;
		entry->format = info->imageFormat;
		entry->extent = info->imageExtent;
		entry->image_count = MAX_IMAGES;
		VkResult got = data->get_swapchain_images(device, *swapchain,
							  &entry->image_count, entry->images);
		/* VK_INCOMPLETE means there are more images than the array holds, and
		 * `pImageIndices` may then name one past the end. Refusing is the only safe
		 * answer; the alternative is an out-of-bounds read every present. */
		if (got != VK_SUCCESS || !entry->image_count) {
			memset(entry, 0, sizeof *entry);
			pthread_mutex_unlock(&lock);
			fprintf(stderr, "[nr_layer] swapchain has more than %d images or none; "
				"capture off\n", MAX_IMAGES);
			return r;
		}
		fprintf(stderr, "[nr_layer] swapchain %ux%u format %d, %u images\n",
			entry->extent.width, entry->extent.height, entry->format,
			entry->image_count);
	}
	pthread_mutex_unlock(&lock);
	return r;
}

/* Everything this layer allocated per device, given back. The pointer to
 * `vkDestroyDevice` was being stored and never used: a staging buffer, its device memory
 * and its host mapping leaked on every device teardown, which a game that recreates its
 * device — a resolution change under some wrappers — does more than once. Ported from the
 * parallel ProjectsCodex tree. */
static void release_device(struct device_data *data)
{
	if (data->mapped) {
		PFN_vkUnmapMemory unmap =
			(PFN_vkUnmapMemory)data->get_device_proc(data->device, "vkUnmapMemory");
		if (unmap) unmap(data->device, data->staging_memory);
	}
	if (data->staging) {
		PFN_vkDestroyBuffer destroy =
			(PFN_vkDestroyBuffer)data->get_device_proc(data->device, "vkDestroyBuffer");
		if (destroy) destroy(data->device, data->staging, NULL);
	}
	if (data->staging_memory) {
		PFN_vkFreeMemory release =
			(PFN_vkFreeMemory)data->get_device_proc(data->device, "vkFreeMemory");
		if (release) release(data->device, data->staging_memory, NULL);
	}
	if (data->pool) {
		PFN_vkDestroyCommandPool destroy = (PFN_vkDestroyCommandPool)
			data->get_device_proc(data->device, "vkDestroyCommandPool");
		if (destroy) destroy(data->device, data->pool, NULL);
	}
	free(data->result);
	free(data->earlier);
	free(data->outgoing);
}

VKAPI_ATTR void VKAPI_CALL nr_DestroyDevice(VkDevice device,
					    const VkAllocationCallbacks *allocator)
{
	struct device_data *data = find_device(device);
	PFN_vkDestroyDevice next = data ? data->destroy_device : NULL;
	if (data) {
		pthread_mutex_lock(&lock);
		for (int i = 0; i < MAX_SWAPCHAINS; i++)
			if (swapchains[i].device == device)
				memset(&swapchains[i], 0, sizeof swapchains[i]);
		for (int i = 0; i < MAX_QUEUES; i++)
			if (queues[i].device == device)
				memset(&queues[i], 0, sizeof queues[i]);
		pthread_mutex_unlock(&lock);
		release_device(data);
		memset(data, 0, sizeof *data);
	}
	if (next) next(device, allocator);
}

/* Without this the eight slots are consumed one per window resize, exclusive-fullscreen
 * toggle or HDR renegotiation, after which nothing is tracked and the photo mode goes
 * quiet. Worse, a driver may reuse a handle, and then `find_swapchain` would match a
 * stale entry whose VkImages are long destroyed. */
VKAPI_ATTR void VKAPI_CALL nr_DestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain,
						  const VkAllocationCallbacks *allocator)
{
	struct device_data *data = find_device(device);
	pthread_mutex_lock(&lock);
	for (int i = 0; i < MAX_SWAPCHAINS; i++)
		if (swapchains[i].swapchain == swapchain)
			memset(&swapchains[i], 0, sizeof swapchains[i]);
	pthread_mutex_unlock(&lock);
	if (data && data->destroy_swapchain)
		data->destroy_swapchain(device, swapchain, allocator);
}

static uint32_t memory_type(struct device_data *data, uint32_t bits,
			    VkMemoryPropertyFlags want)
{
	VkPhysicalDeviceMemoryProperties properties;
	PFN_vkGetPhysicalDeviceMemoryProperties get =
		(PFN_vkGetPhysicalDeviceMemoryProperties)next_instance_proc(
			layer_instance, "vkGetPhysicalDeviceMemoryProperties");
	if (!get) return UINT32_MAX;
	get(data->physical, &properties);
	for (uint32_t i = 0; i < properties.memoryTypeCount; i++)
		if ((bits & (1u << i))
		    && (properties.memoryTypes[i].propertyFlags & want) == want)
			return i;
	return UINT32_MAX;
}

/* Move the presented frame between the swapchain image and a host-visible buffer.
 *
 * `vkQueueWaitIdle` around the transfer is the blunt way to know the frame is
 * finished: the proper route waits on the present's own semaphores, which means taking
 * them over from the application. For a photo mode the game is meant to stall anyway,
 * so the stall is the point rather than a cost. It has to change before the pass runs
 * every frame.
 */
static int ensure_resources(struct device_data *data, VkDeviceSize needed)
{
	if (!data->pool) {
		VkCommandPoolCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = data->queue_family };
		PFN_vkCreateCommandPool create = (PFN_vkCreateCommandPool)
			data->get_device_proc(data->device, "vkCreateCommandPool");
		if (create(data->device, &info, NULL, &data->pool) != VK_SUCCESS) return -1;
	}
	if (data->staging_size >= needed) return 0;

	PFN_vkCreateBuffer create_buffer = (PFN_vkCreateBuffer)
		data->get_device_proc(data->device, "vkCreateBuffer");
	PFN_vkGetBufferMemoryRequirements requirements = (PFN_vkGetBufferMemoryRequirements)
		data->get_device_proc(data->device, "vkGetBufferMemoryRequirements");
	PFN_vkAllocateMemory allocate = (PFN_vkAllocateMemory)
		data->get_device_proc(data->device, "vkAllocateMemory");
	PFN_vkBindBufferMemory bind = (PFN_vkBindBufferMemory)
		data->get_device_proc(data->device, "vkBindBufferMemory");
	PFN_vkMapMemory map = (PFN_vkMapMemory)
		data->get_device_proc(data->device, "vkMapMemory");
	VkBufferCreateInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				    .size = needed,
				    .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					     | VK_BUFFER_USAGE_TRANSFER_SRC_BIT };
	if (create_buffer(data->device, &info, NULL, &data->staging) != VK_SUCCESS) return -1;
	VkMemoryRequirements mr;
	requirements(data->device, data->staging, &mr);
	uint32_t type = memory_type(data, mr.memoryTypeBits,
				    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
				    | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
				    | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
	if (type == UINT32_MAX)
		type = memory_type(data, mr.memoryTypeBits,
				   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
				   | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (type == UINT32_MAX) return -1;
	VkMemoryAllocateInfo allocation = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
					    .allocationSize = mr.size, .memoryTypeIndex = type };
	if (allocate(data->device, &allocation, NULL, &data->staging_memory) != VK_SUCCESS)
		return -1;
	bind(data->device, data->staging, data->staging_memory, 0);
	map(data->device, data->staging_memory, 0, VK_WHOLE_SIZE, 0, &data->mapped);
	data->staging_size = needed;
	free(data->result);
	data->result = malloc((size_t)needed);
	data->result_size = needed;
	return data->result ? 0 : -1;
}

static int ensure_ring(struct device_data *data)
{
	if (data->ring_ready) return 0;
	if (!data->pool) return -1;
	PFN_vkAllocateCommandBuffers allocate_commands = (PFN_vkAllocateCommandBuffers)
		data->get_device_proc(data->device, "vkAllocateCommandBuffers");
	PFN_vkCreateFence create_fence = (PFN_vkCreateFence)
		data->get_device_proc(data->device, "vkCreateFence");
	PFN_vkCreateSemaphore create_semaphore = (PFN_vkCreateSemaphore)
		data->get_device_proc(data->device, "vkCreateSemaphore");
	if (!allocate_commands || !create_fence || !create_semaphore) return -1;
	VkCommandBufferAllocateInfo alloc = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = data->pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = SYNC_SLOTS };
	if (allocate_commands(data->device, &alloc, data->ring_commands) != VK_SUCCESS)
		return -1;
	VkFenceCreateInfo fi = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkSemaphoreCreateInfo si = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	for (int i = 0; i < SYNC_SLOTS; i++) {
		if (create_fence(data->device, &fi, NULL, &data->ring_fence[i]) != VK_SUCCESS
		    || create_semaphore(data->device, &si, NULL, &data->ring_done[i]) != VK_SUCCESS)
			return -1;
		data->ring_used[i] = 0;
	}
	data->ring_ready = 1;
	return 0;
}

static int transfer(struct device_data *data, struct swapchain_data *chain, VkQueue queue,
		    uint32_t index, int to_image)
{
	PFN_vkAllocateCommandBuffers allocate_commands = (PFN_vkAllocateCommandBuffers)
		data->get_device_proc(data->device, "vkAllocateCommandBuffers");
	PFN_vkBeginCommandBuffer begin = (PFN_vkBeginCommandBuffer)
		data->get_device_proc(data->device, "vkBeginCommandBuffer");
	PFN_vkCmdPipelineBarrier barrier = (PFN_vkCmdPipelineBarrier)
		data->get_device_proc(data->device, "vkCmdPipelineBarrier");
	PFN_vkCmdCopyImageToBuffer copy_out = (PFN_vkCmdCopyImageToBuffer)
		data->get_device_proc(data->device, "vkCmdCopyImageToBuffer");
	PFN_vkCmdCopyBufferToImage copy_in = (PFN_vkCmdCopyBufferToImage)
		data->get_device_proc(data->device, "vkCmdCopyBufferToImage");
	PFN_vkEndCommandBuffer end = (PFN_vkEndCommandBuffer)
		data->get_device_proc(data->device, "vkEndCommandBuffer");
	PFN_vkQueueSubmit submit = (PFN_vkQueueSubmit)
		data->get_device_proc(data->device, "vkQueueSubmit");
	PFN_vkQueueWaitIdle wait = (PFN_vkQueueWaitIdle)
		data->get_device_proc(data->device, "vkQueueWaitIdle");
	PFN_vkFreeCommandBuffers free_commands = (PFN_vkFreeCommandBuffers)
		data->get_device_proc(data->device, "vkFreeCommandBuffers");

	int ringed = sync_semaphores && !data->present_plain && ensure_ring(data) == 0;
	unsigned slot = data->ring_next % SYNC_SLOTS;
	VkCommandBuffer commands;
	if (ringed) {
		/* The slot comes back round; whatever it was doing four presents ago is over
		 * by now, but saying so is the difference between reusing a command buffer and
		 * overwriting one still in flight. */
		if (data->ring_used[slot]) {
			PFN_vkWaitForFences wait_fences = (PFN_vkWaitForFences)
				data->get_device_proc(data->device, "vkWaitForFences");
			PFN_vkResetFences reset_fences = (PFN_vkResetFences)
				data->get_device_proc(data->device, "vkResetFences");
			wait_fences(data->device, 1, &data->ring_fence[slot], VK_TRUE,
				    10ull * 1000000000ull);
			reset_fences(data->device, 1, &data->ring_fence[slot]);
		}
		commands = data->ring_commands[slot];
		data->ring_next++;
	} else {
		VkCommandBufferAllocateInfo alloc = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = data->pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1 };
		if (allocate_commands(data->device, &alloc, &commands) != VK_SUCCESS) return -1;
	}
	VkCommandBufferBeginInfo beginning = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	begin(commands, &beginning);

	VkImageLayout working = to_image ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
					 : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	VkImageMemoryBarrier into = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		.dstAccessMask = to_image ? VK_ACCESS_TRANSFER_WRITE_BIT
					  : VK_ACCESS_TRANSFER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.newLayout = working,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = chain->images[index],
		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
	barrier(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &into);

	VkBufferImageCopy region = {
		.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.imageExtent = { chain->extent.width, chain->extent.height, 1 } };
	if (to_image)
		copy_in(commands, data->staging, chain->images[index], working, 1, &region);
	else
		copy_out(commands, chain->images[index], working, data->staging, 1, &region);

	VkImageMemoryBarrier back = into;
	back.srcAccessMask = into.dstAccessMask;
	back.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	back.oldLayout = working;
	back.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &back);
	end(commands);

	if (!ringed) {
		wait(queue);
		VkSubmitInfo submission = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					    .commandBufferCount = 1, .pCommandBuffers = &commands };
		submit(queue, 1, &submission, VK_NULL_HANDLE);
		wait(queue);
		free_commands(data->device, data->pool, 1, &commands);
		return 0;
	}

	/* The first submit of a present inherits the semaphores the game gave the present,
	 * so this copy happens after the frame is drawn rather than after the queue is
	 * empty.
	 *
	 * Only the write-back signals. A binary semaphore may not be signalled while it is
	 * already signalled, and nothing waits on a readback's: the host waits on its fence
	 * here, so by the time anything else runs that copy is finished and a semaphore
	 * would only be a signal nobody consumes — left standing until the ring came round
	 * and signalled it a second time, which is exactly what the spec forbids
	 * (`VUID-vkQueueSubmit-pSignalSemaphores-00067`). Drivers let it pass in silence,
	 * which is worse rather than better: three games and a headless test had nothing to
	 * say about it. */
	VkSemaphore signal = to_image ? data->ring_done[slot] : VK_NULL_HANDLE;
	VkPipelineStageFlags stages[NR_MAX_WAITS];
	uint32_t waits = data->present_wait_count;
	for (uint32_t i = 0; i < waits; i++) stages[i] = VK_PIPELINE_STAGE_TRANSFER_BIT;
	VkSubmitInfo submission = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				    .waitSemaphoreCount = waits,
				    .pWaitSemaphores = waits ? data->present_wait : NULL,
				    .pWaitDstStageMask = waits ? stages : NULL,
				    .commandBufferCount = 1, .pCommandBuffers = &commands,
				    .signalSemaphoreCount = signal ? 1 : 0,
				    .pSignalSemaphores = signal ? &signal : NULL };
	if (submit(queue, 1, &submission, data->ring_fence[slot]) != VK_SUCCESS) {
		data->ring_used[slot] = 0;
		return -1;
	}
	data->ring_used[slot] = 1;
	data->present_wait_count = 0;          /* consumed; the present must not wait again */
	/* A present with nothing to wait on is correct when only the readback ran: its fence
	 * is waited below, so the copy — and the game's own drawing, which it waited on — are
	 * finished before the present is even called. */
	if (signal) data->present_signal = signal;
	if (!to_image) {
		/* Only this direction stalls the host, and only on its own work: the pixels
		 * are about to be read out of the staging buffer. */
		PFN_vkWaitForFences wait_fences = (PFN_vkWaitForFences)
			data->get_device_proc(data->device, "vkWaitForFences");
		if (wait_fences(data->device, 1, &data->ring_fence[slot], VK_TRUE,
				10ull * 1000000000ull) != VK_SUCCESS)
			return -1;
	}
	return 0;
}

/* Which pixels are the interface.
 *
 * The shipped feature never has to ask: it inserts the pass before the interface is
 * drawn ("UI remains downstream"). A layer at `vkQueuePresentKHR` sees the composed
 * frame and has to work it out, and the one signal available is motion — an interface
 * holds still while the scene under it does not.
 *
 * That signal cannot separate an interface over a still scene from a still scene, so
 * when almost nothing moved the mask is refused rather than guessed: `settled` returns
 * 0 and the frame is sent unmasked, which is what a photo mode of a paused scene wants.
 */
static uint32_t settled(const unsigned char *now, const unsigned char *before,
			uint32_t pixels, unsigned char *mask)
{
	uint32_t held = 0;
	for (uint32_t i = 0; i < pixels; i++) {
		const unsigned char *a = now + 4 * i, *b = before + 4 * i;
		int moved = abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2]) > 6;
		mask[i] = moved ? 0u : 0xFFu;
		held += !moved;
	}
	return held;
}

/* Whether a mask is worth sending at all. Over nine tenths held still means nothing
 * moved, so an interface cannot be told from a still scene and the mask would cover the
 * whole frame. Under a fiftieth means there is no interface worth a second plane on the
 * wire. Between them the signal is real. */
static int mask_worth_sending(uint32_t held, uint32_t pixels)
{
	return held < (uint32_t)((uint64_t)pixels * 9 / 10) && held > pixels / 50;
}

static int process_frame(struct device_data *data, struct swapchain_data *chain,
			 VkQueue queue, uint32_t index)
{
	VkDeviceSize needed = (VkDeviceSize)chain->extent.width * chain->extent.height * 4;
	if (ensure_resources(data, needed)) return -1;
	if (transfer(data, chain, queue, index, 0)) return -1;

	uint32_t pixels = chain->extent.width * chain->extent.height;
	int masked = 0;
	if (ui_mask && data->have_earlier && data->earlier_size >= needed) {
		if (data->outgoing_size < needed + pixels) {
			unsigned char *grown = realloc(data->outgoing, (size_t)needed + pixels);
			if (!grown) return -1;
			data->outgoing = grown;
			data->outgoing_size = needed + pixels;
		}
		memcpy(data->outgoing, data->mapped, (size_t)needed);
		uint32_t held = settled(data->mapped, data->earlier, pixels,
					data->outgoing + needed);
		masked = mask_worth_sending(held, pixels);
		fprintf(stderr, "[nr_layer] %u%% of the frame held still; ui mask %s\n",
			100u * held / pixels, masked ? "sent" : "refused");
	}

	uint32_t header[4] = { masked ? 0x314E524Eu : 0x304E524Eu, chain->extent.width,
			       chain->extent.height, (uint32_t)chain->format };
	if (capture_path) {
		FILE *file = fopen(capture_path, "wb");
		if (file) {
			fwrite(header, sizeof header, 1, file);
			fwrite(data->mapped, 1, (size_t)needed, file);
			fclose(file);
		}
	}
	if (!socket_path) return -1;
	/* The mask travels immediately after the colour, one byte a pixel. */
	if (masked) {
		if (exchange(header, sizeof header, data->outgoing,
			     (size_t)needed + pixels, data->result, (size_t)needed)) {
			fprintf(stderr, "[nr_layer] the daemon did not answer; frame unchanged\n");
			return -1;
		}
		memcpy(data->mapped, data->result, (size_t)needed);
		fprintf(stderr, "[nr_layer] processed %ux%u with a ui mask\n",
			chain->extent.width, chain->extent.height);
		return 0;
	}
	if (exchange(header, sizeof header, data->mapped, (size_t)needed, data->result,
		     (size_t)needed)) {
		fprintf(stderr, "[nr_layer] the daemon did not answer; frame unchanged\n");
		return -1;
	}
	memcpy(data->mapped, data->result, (size_t)needed);
	fprintf(stderr, "[nr_layer] processed %ux%u\n", chain->extent.width,
		chain->extent.height);
	return 0;
}

/* The present, with its waits replaced by ours if a transfer consumed them. A semaphore
 * wait is a consume: having taken the game's, we must not hand them to the present as
 * well, and the present must wait on the copy that took them. */
static VkResult present_now(struct device_data *data, VkQueue queue,
			    const VkPresentInfoKHR *info)
{
	if (!sync_semaphores || !data || !data->present_signal)
		return data->present(queue, info);
	VkPresentInfoKHR patched = *info;
	patched.waitSemaphoreCount = 1;
	patched.pWaitSemaphores = &data->present_signal;
	return data->present(queue, &patched);
}

VKAPI_ATTR VkResult VKAPI_CALL nr_QueuePresentKHR(VkQueue queue,
						  const VkPresentInfoKHR *info)
{
	frame_counter++;
	struct device_data *data = NULL;
	/* Resolve the queue rather than taking the first live device: with two devices the
	 * first is not necessarily this one, and the family decides which pool is legal. */
	struct queue_data *owner = find_queue(queue);
	if (owner && !owner->capture_ok) {
		/* A present-only or protected queue: nothing can be recorded on it. Find the
		 * device only to reach its `present` pointer. */
		struct device_data *host = find_device(owner->device);
		if (host) return host->present(queue, info);
	}
	if (owner) {
		data = find_device(owner->device);
		if (data && data->queue_family != owner->family) {
			data->queue_family = owner->family;
			/* Destroy rather than drop: command buffers are allocated and freed
			 * per transfer, so nothing outlives the pool, and a dropped handle
			 * would leak once per family change. */
			if (data->pool) {
				PFN_vkDestroyCommandPool destroy = (PFN_vkDestroyCommandPool)
					data->get_device_proc(data->device, "vkDestroyCommandPool");
				if (destroy) destroy(data->device, data->pool, NULL);
				data->pool = VK_NULL_HANDLE;   /* rebuilt for the right family */
			}
		}
	}
	if (!data)
		for (int i = 0; i < 8; i++) if (devices[i].device) { data = &devices[i]; break; }
	if (!data) return VK_ERROR_INITIALIZATION_FAILED;

	/* Offered, not yet taken: a transfer claims them, and if none runs this present goes
	 * through untouched with its own. */
	data->present_plain = info->waitSemaphoreCount > NR_MAX_WAITS;
	data->present_wait = info->pWaitSemaphores;
	data->present_wait_count = (sync_semaphores && !data->present_plain)
		? info->waitSemaphoreCount : 0;
	data->present_signal = VK_NULL_HANDLE;

	/* Live mode is a slideshow rather than a photo: every Nth present goes through the
	 * network and the frames between re-blit the last result, so the picture is steady
	 * instead of alternating with the game's own. The trigger file is not consulted —
	 * an `access()` per present is a syscall the hot path does not need — and neither
	 * is the interface mask, whose detector is built around a frame that was asked for.
	 *
	 * The rate this can hold is set by the daemon's `--render-scale`, not by N: the
	 * network's cost follows the extent it is given (notes/phase37). N only decides how
	 * many game frames each rendered one covers. */
	if (live_every > 0) {
		/* The trigger keeps its meaning — "do the thing" — so the effect can be
		 * turned on and off mid-game without restarting it. With no trigger
		 * configured, live mode simply always runs. */
		int on = !trigger_path || access(trigger_path, F_OK) == 0;
		if (!on) data->holding = 0;
		for (uint32_t i = 0; on && i < info->swapchainCount; i++) {
			struct swapchain_data *chain = find_swapchain(info->pSwapchains[i]);
			if (!chain || info->pImageIndices[i] >= chain->image_count) continue;
			uint32_t index = info->pImageIndices[i];
			VkDeviceSize want = (VkDeviceSize)chain->extent.width
					  * chain->extent.height * 4;
			if (frame_counter % (unsigned long)live_every == 0) {
				if (process_frame(data, chain, queue, index) == 0) {
					data->holding = 1;
					transfer(data, chain, queue, index, 1);
				}
			} else if (data->holding && data->result_size == want) {
				memcpy(data->mapped, data->result, (size_t)data->result_size);
				transfer(data, chain, queue, index, 1);
			}
		}
		return present_now(data, queue, info);
	}

	/* A file is the trigger, not a key: it works the same on X11 and Wayland, needs
	 * no input hooking inside another process's window, and can be set from a script
	 * or a hotkey daemon. While it exists the processed frame is held on screen. */
	int wanted = trigger_path && access(trigger_path, F_OK) == 0;
	if (!wanted && capture_every > 0)
		wanted = frame_counter % (unsigned long)capture_every == 0;

	for (uint32_t i = 0; i < info->swapchainCount; i++) {
		struct swapchain_data *chain = find_swapchain(info->pSwapchains[i]);
		if (!chain || info->pImageIndices[i] >= chain->image_count) continue;
		uint32_t index = info->pImageIndices[i];
		if (wanted && ui_mask && !data->holding && !data->have_earlier) {
			/* Spend the first present after the trigger keeping the frame, and
			 * process the next one against it. One frame of extra latency on a
			 * pass that already takes a second, and nothing at all while the
			 * trigger is down. */
			VkDeviceSize needed = (VkDeviceSize)chain->extent.width
					      * chain->extent.height * 4;
			if (ensure_resources(data, needed) == 0
			    && transfer(data, chain, queue, index, 0) == 0) {
				if (data->earlier_size < needed) {
					unsigned char *grown = realloc(data->earlier, (size_t)needed);
					if (grown) { data->earlier = grown; data->earlier_size = needed; }
				}
				if (data->earlier_size >= needed) {
					memcpy(data->earlier, data->mapped, (size_t)needed);
					data->have_earlier = 1;
				}
			}
		} else if (wanted && !data->holding) {
			if (process_frame(data, chain, queue, index) == 0) {
				data->holding = 1;
				transfer(data, chain, queue, index, 1);
			}
		} else if (wanted && data->holding
			   && data->result_size == (VkDeviceSize)chain->extent.width
						 * chain->extent.height * 4) {
			memcpy(data->mapped, data->result, (size_t)data->result_size);
			transfer(data, chain, queue, index, 1);
		} else if (!wanted) {
			data->holding = 0;
			data->have_earlier = 0;
		}
	}
	return present_now(data, queue, info);
}

#define INTERCEPT(name) if (!strcmp(pName, "vk" #name)) return (PFN_vkVoidFunction)nr_##name

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL nr_GetDeviceProcAddr(VkDevice device,
							      const char *pName)
{
	INTERCEPT(QueuePresentKHR);
	INTERCEPT(CreateSwapchainKHR);
	INTERCEPT(DestroyDevice);
	INTERCEPT(DestroySwapchainKHR);
	INTERCEPT(GetDeviceQueue);
	INTERCEPT(GetDeviceQueue2);
	struct device_data *data = find_device(device);
	return data ? data->get_device_proc(device, pName) : NULL;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL nr_GetInstanceProcAddr(VkInstance instance,
								const char *pName)
{
	INTERCEPT(CreateInstance);
	INTERCEPT(CreateDevice);
	INTERCEPT(GetInstanceProcAddr);
	INTERCEPT(GetDeviceProcAddr);
	INTERCEPT(QueuePresentKHR);
	INTERCEPT(CreateSwapchainKHR);
	INTERCEPT(DestroyDevice);
	INTERCEPT(DestroySwapchainKHR);
	INTERCEPT(GetDeviceQueue);
	INTERCEPT(GetDeviceQueue2);
	return next_instance_proc ? next_instance_proc(instance, pName) : NULL;
}

VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(
	VkNegotiateLayerInterface *interface)
{
	if (interface->loaderLayerInterfaceVersion < 2)
		return VK_ERROR_INITIALIZATION_FAILED;
	interface->loaderLayerInterfaceVersion = 2;
	interface->pfnGetInstanceProcAddr = nr_GetInstanceProcAddr;
	interface->pfnGetDeviceProcAddr = nr_GetDeviceProcAddr;
	interface->pfnGetPhysicalDeviceProcAddr = NULL;
	return VK_SUCCESS;
}
