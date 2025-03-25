#ifndef VGLTF_RENDERER_H
#define VGLTF_RENDERER_H

#include "platform.h"
#include <vulkan/vulkan.h>

constexpr int VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT = 2;
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
  VkRenderPass render_pass;
  VkPipelineLayout pipeline_layout;
  VkPipeline graphics_pipeline;
  VkFramebuffer
      swapchain_framebuffers[VGLTF_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT];
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer[VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT];
  VkSemaphore
      image_available_semaphores[VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT];
  VkSemaphore
      render_finished_semaphores[VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT];
  VkFence in_flight_fences[VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT];
  struct vgltf_window_size window_size;
  uint32_t current_frame;
  bool framebuffer_resized;
};
bool vgltf_renderer_init(struct vgltf_renderer *renderer,
                         struct vgltf_platform *platform);
void vgltf_renderer_deinit(struct vgltf_renderer *renderer);
bool vgltf_renderer_triangle_pass(struct vgltf_renderer *renderer);
void vgltf_renderer_on_window_resized(struct vgltf_renderer *renderer,
                                      struct vgltf_window_size size);
#endif // VGLTF_RENDERER_H
