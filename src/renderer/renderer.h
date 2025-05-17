#ifndef VGLTF_RENDERER_H
#define VGLTF_RENDERER_H

#include "../graphics.h"
#include "../maths.h"
#include "../platform.h"
#include "vma_usage.h"
#include <vulkan/vulkan.h>

VkVertexInputBindingDescription vgltf_vertex_binding_description(void);

struct vgltf_vertex_input_attribute_descriptions {
  VkVertexInputAttributeDescription descriptions[3];
  uint32_t count;
};
struct vgltf_vertex_input_attribute_descriptions
vgltf_vertex_attribute_descriptions(void);

struct vgltf_renderer_uniform_buffer_object {
  alignas(16) vgltf_mat4 model;
  alignas(16) vgltf_mat4 view;
  alignas(16) vgltf_mat4 projection;
};

struct vgltf_renderer_allocated_buffer {
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo info;
};

struct vgltf_renderer_allocated_image {
  VkImage image;
  VmaAllocation allocation;
  VmaAllocationInfo info;
};

struct vgltf_vk_instance {
  VkInstance instance;
};

struct vgltf_vk_device {
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue graphics_queue;
  VkQueue present_queue;
  VmaAllocator allocator;
};

struct vgltf_vk_surface {
  VkSurfaceKHR surface;
};

constexpr int VGLTF_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT = 32;
struct vgltf_vk_swapchain {
  VkSwapchainKHR swapchain;
  VkFormat swapchain_image_format;
  VkImage swapchain_images[VGLTF_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT];
  VkImageView swapchain_image_views[VGLTF_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT];
  VkExtent2D swapchain_extent;
  uint32_t swapchain_image_count;
};

struct vgltf_vk_pipeline {
  VkPipelineLayout layout;
  VkPipeline pipeline;
};

constexpr int VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT = 2;
constexpr int VGLTF_RENDERER_VERTEX_BUFFER_CAPACITY = 300000;
constexpr int VGLTF_RENDERER_VERTEX_STAGING_BUFFER_CAPACITY = 1024;
constexpr int VGLTF_RENDERER_INDEX_BUFFER_CAPACITY = 300000;
constexpr int VGLTF_RENDERER_INDEX_STAGING_BUFFER_CAPACITY = 1024;
struct vgltf_renderer {
  struct vgltf_vk_instance instance;
  struct vgltf_vk_device device;
  VkDebugUtilsMessengerEXT debug_messenger;
  struct vgltf_vk_surface surface;
  struct vgltf_vk_swapchain swapchain;
  struct vgltf_renderer_allocated_image depth_image;
  VkImageView depth_image_view;

  VkRenderPass render_pass;
  VkDescriptorSetLayout descriptor_set_layout;

  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_sets[VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT];
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

  struct vgltf_renderer_allocated_buffer
      uniform_buffers[VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT];
  void *mapped_uniform_buffers[VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT];

  uint32_t mip_level_count;
  struct vgltf_renderer_allocated_image texture_image;
  VkImageView texture_image_view;
  VkSampler texture_sampler;
  struct vgltf_renderer_allocated_buffer vertex_buffer;
  struct vgltf_renderer_allocated_buffer vertex_staging_buffer;
  int vertex_count;
  struct vgltf_renderer_allocated_buffer index_buffer;
  struct vgltf_renderer_allocated_buffer index_staging_buffer;
  int index_count;

  struct vgltf_window_size window_size;
  uint32_t current_frame;
  bool framebuffer_resized;
};
bool vgltf_renderer_init(struct vgltf_renderer *renderer,
                         struct vgltf_platform *platform);
void vgltf_renderer_deinit(struct vgltf_renderer *renderer);
void vgltf_renderer_upload_mesh(struct vgltf_renderer *renderer,
                                const struct vgltf_mesh *mesh);
bool vgltf_renderer_render_frame(struct vgltf_renderer *renderer);
void vgltf_renderer_on_window_resized(struct vgltf_renderer *renderer,
                                      struct vgltf_window_size size);

#endif // VGLTF_RENDERER_H
