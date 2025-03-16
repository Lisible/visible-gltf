#ifndef VGLTF_RENDERER_H
#define VGLTF_RENDERER_H

#include "platform.h"
#include <vulkan/vulkan.h>

constexpr int VGLTF_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT = 32;
struct vgltf_renderer {
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue graphics_queue;
  VkQueue present_queue;
  VkDebugUtilsMessengerEXT debug_messenger;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  VkImage swapchain_images[VGLTF_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT];
  VkImageView swapchain_image_views[VGLTF_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT];
  VkFormat swapchain_image_format;
  VkExtent2D swapchain_extent;
  uint32_t swapchain_image_count;
};
bool vgltf_renderer_init(struct vgltf_renderer *renderer,
                         struct vgltf_platform *platform);
void vgltf_renderer_deinit(struct vgltf_renderer *renderer);

#endif // VGLTF_RENDERER_H
