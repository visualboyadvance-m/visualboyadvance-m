/* Present real frames through the layer, on a surface that needs no screen.
 *
 * Everything else about the layer is tested without ever presenting: the wire protocol
 * against a stand-in daemon, the interface detector on arrays, the manifest against a
 * loader. The one thing left out was the part that runs inside `vkQueuePresentKHR` — the
 * copy out, the copy back and, since `NR_LAYER_SYNC=semaphore`, who waits for whom. That
 * needed a game, and a game is not a test.
 *
 * `VK_EXT_headless_surface` gives a swapchain with no window. The frames are 64x32 and the
 * daemon is a Python stand-in that checks what it is sent and answers with a colour of its
 * own, so both directions are observable:
 *
 *   - the first pass over the images is cleared to a colour that says which frame it is,
 *     and the daemon must receive exactly that. A copy started before the clear finished
 *     would see the previous contents instead, which is what a missing wait looks like.
 *   - the second pass records nothing at all, so each image still holds what the layer
 *     wrote into it last time round. The daemon must now receive its own answer back.
 *
 * Exit 0 and a line per frame; the harness in `test_daemon.py` runs it in both sync modes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define WIDTH 64
#define HEIGHT 32

#define CHECK(what, r) do { \
	VkResult code_ = (r); \
	if (code_ != VK_SUCCESS) { fprintf(stderr, "%s: %d\n", what, (int)code_); return 2; } \
} while (0)

int main(int argc, char **argv)
{
	unsigned rounds = argc > 1 ? (unsigned)atoi(argv[1]) : 2;
	const char *layers[] = { "VK_LAYER_dlssnr_intel" };
	/* On macOS the loader hides MoltenVK — a "portability" driver — until the instance
	 * asks for it, which a game does too; the layer sits behind whatever the game asked. */
	const char *instance_ext[] = { VK_KHR_SURFACE_EXTENSION_NAME,
				       VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME,
				       VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME };
#ifdef __APPLE__
	const uint32_t instance_ext_count = 3;
	const VkInstanceCreateFlags instance_flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#else
	const uint32_t instance_ext_count = 2;
	const VkInstanceCreateFlags instance_flags = 0;
#endif
	VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				  .pApplicationName = "nr present test",
				  .apiVersion = VK_API_VERSION_1_3 };
	VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				     .pApplicationInfo = &app, .flags = instance_flags,
				     /* `NR_TEST_NO_LAYER=1` runs the same frames with no layer
				      * at all, which is how a failure is told apart from a
				      * failure of ours */
				     .enabledLayerCount = getenv("NR_TEST_NO_LAYER") ? 0 : 1,
				     .ppEnabledLayerNames = layers,
				     .enabledExtensionCount = instance_ext_count,
				     .ppEnabledExtensionNames = instance_ext };
	VkInstance instance;
	CHECK("vkCreateInstance", vkCreateInstance(&ici, NULL, &instance));

	uint32_t n = 0;
	vkEnumeratePhysicalDevices(instance, &n, NULL);
	if (!n) { fprintf(stderr, "no physical device\n"); return 2; }
	VkPhysicalDevice *pds = calloc(n, sizeof *pds);
	vkEnumeratePhysicalDevices(instance, &n, pds);
	VkPhysicalDevice pd = pds[0];
	free(pds);

	PFN_vkCreateHeadlessSurfaceEXT create_surface = (PFN_vkCreateHeadlessSurfaceEXT)
		vkGetInstanceProcAddr(instance, "vkCreateHeadlessSurfaceEXT");
	if (!create_surface) { fprintf(stderr, "no headless surface\n"); return 2; }
	VkHeadlessSurfaceCreateInfoEXT hs = {
		.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT };
	VkSurfaceKHR surface;
	CHECK("vkCreateHeadlessSurfaceEXT", create_surface(instance, &hs, NULL, &surface));

	uint32_t families = 0, family = UINT32_MAX;
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &families, NULL);
	VkQueueFamilyProperties *props = calloc(families, sizeof *props);
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &families, props);
	for (uint32_t i = 0; i < families; i++) {
		VkBool32 can_present = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface, &can_present);
		if (can_present && (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) { family = i; break; }
	}
	free(props);
	if (family == UINT32_MAX) { fprintf(stderr, "no present queue\n"); return 2; }

	float priority = 1.0f;
	VkDeviceQueueCreateInfo qci = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = family, .queueCount = 1,
					.pQueuePriorities = &priority };
	const char *device_ext[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkDeviceCreateInfo dci = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				   .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci,
				   .enabledExtensionCount = 1,
				   .ppEnabledExtensionNames = device_ext };
	VkDevice device;
	CHECK("vkCreateDevice", vkCreateDevice(pd, &dci, NULL, &device));
	VkQueue queue;
	vkGetDeviceQueue(device, family, 0, &queue);

	VkSurfaceCapabilitiesKHR caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &caps);
	uint32_t images_wanted = caps.minImageCount < 2 ? 2 : caps.minImageCount;
	/* Only colour attachment, deliberately: the layer patches TRANSFER_SRC/DST into the
	 * create info on the way down, exactly as it does for a game that never asked. */
	VkSwapchainCreateInfoKHR sci = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface = surface,
		.minImageCount = images_wanted, .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
		.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		.imageExtent = { WIDTH, HEIGHT }, .imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
			    | (getenv("NR_TEST_TRANSFER_USAGE")
			       ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
			       : 0),
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = caps.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR, .clipped = VK_TRUE };
	VkSwapchainKHR chain;
	CHECK("vkCreateSwapchainKHR", vkCreateSwapchainKHR(device, &sci, NULL, &chain));
	uint32_t image_count = 0;
	vkGetSwapchainImagesKHR(device, chain, &image_count, NULL);
	VkImage *images = calloc(image_count, sizeof *images);
	vkGetSwapchainImagesKHR(device, chain, &image_count, images);

	VkCommandPoolCreateInfo cpi = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
					.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
					.queueFamilyIndex = family };
	VkCommandPool pool;
	CHECK("vkCreateCommandPool", vkCreateCommandPool(device, &cpi, NULL, &pool));
	VkCommandBufferAllocateInfo cba = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					    .commandPool = pool,
					    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					    .commandBufferCount = 1 };
	VkCommandBuffer commands;
	CHECK("vkAllocateCommandBuffers", vkAllocateCommandBuffers(device, &cba, &commands));
	VkSemaphoreCreateInfo semi = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkSemaphore acquired, rendered;
	CHECK("vkCreateSemaphore", vkCreateSemaphore(device, &semi, NULL, &acquired));
	CHECK("vkCreateSemaphore", vkCreateSemaphore(device, &semi, NULL, &rendered));
	VkFenceCreateInfo fci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkFence fence;
	CHECK("vkCreateFence", vkCreateFence(device, &fci, NULL, &fence));

	for (unsigned frame = 0; frame < rounds * image_count; frame++) {
		uint32_t index = 0;
		CHECK("vkAcquireNextImageKHR",
		      vkAcquireNextImageKHR(device, chain, UINT64_MAX, acquired,
					    VK_NULL_HANDLE, &index));
		int draw = frame < image_count;         /* the second pass leaves them alone */
		if (draw) {
			VkCommandBufferBeginInfo bi = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
			vkResetCommandBuffer(commands, 0);
			CHECK("vkBeginCommandBuffer", vkBeginCommandBuffer(commands, &bi));
			VkImageMemoryBarrier into = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = images[index],
				.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
			vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					     VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL,
					     1, &into);
			/* The frame number in the blue channel, so the daemon can say which
			 * frame it was handed and a stale copy is visible rather than merely
			 * suspected. Clearing is a real command with a real cost: whoever
			 * copies this image afterwards has to wait for it. */
			VkClearColorValue colour = { .float32 = { 0.25f, 0.5f,
								  (float)(frame + 1) / 255.0f, 1.0f } };
			VkImageSubresourceRange all = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			vkCmdClearColorImage(commands, images[index],
					     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &colour, 1, &all);
			VkImageMemoryBarrier out = into;
			out.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			out.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			out.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			out.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
					     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL,
					     0, NULL, 1, &out);
			CHECK("vkEndCommandBuffer", vkEndCommandBuffer(commands));
			VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					    .waitSemaphoreCount = 1, .pWaitSemaphores = &acquired,
					    .pWaitDstStageMask = &stage,
					    .commandBufferCount = 1, .pCommandBuffers = &commands,
					    .signalSemaphoreCount = 1,
					    .pSignalSemaphores = &rendered };
			vkResetFences(device, 1, &fence);
			CHECK("vkQueueSubmit", vkQueueSubmit(queue, 1, &si, fence));
		}
		VkPresentInfoKHR pi = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
					.waitSemaphoreCount = 1,
					.pWaitSemaphores = draw ? &rendered : &acquired,
					.swapchainCount = 1, .pSwapchains = &chain,
					.pImageIndices = &index };
		VkResult presented = vkQueuePresentKHR(queue, &pi);
		if (presented != VK_SUCCESS && presented != VK_SUBOPTIMAL_KHR)
			CHECK("vkQueuePresentKHR", presented);
		printf("frame %u image %u %s\n", frame, index, draw ? "drawn" : "untouched");
		fflush(stdout);
		if (draw)
			CHECK("vkWaitForFences",
			      vkWaitForFences(device, 1, &fence, VK_TRUE, 5ull * 1000000000ull));
		vkQueueWaitIdle(queue);                 /* the test's own pacing, not the layer's */
	}

	vkDeviceWaitIdle(device);
	vkDestroyFence(device, fence, NULL);
	vkDestroySemaphore(device, acquired, NULL);
	vkDestroySemaphore(device, rendered, NULL);
	vkDestroyCommandPool(device, pool, NULL);
	vkDestroySwapchainKHR(device, chain, NULL);
	free(images);
	vkDestroyDevice(device, NULL);
	vkDestroySurfaceKHR(instance, surface, NULL);
	vkDestroyInstance(instance, NULL);
	printf("presented %u frames of %dx%d\n", rounds * image_count, WIDTH, HEIGHT);
	return 0;
}
