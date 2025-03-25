#ifndef VGLTF_PLATFORM_H
#define VGLTF_PLATFORM_H

#include "log.h"
#include <stdint.h>
#include <stdlib.h>

#define VGLTF_PANIC(...)                                                       \
  do {                                                                         \
    VGLTF_LOG_ERR("panic: " __VA_ARGS__);                                      \
    exit(1);                                                                   \
  } while (0)

enum vgltf_event_type {
  VGLTF_EVENT_QUIT,
  VGLTF_EVENT_KEY_DOWN,
  VGLTF_EVENT_WINDOW_RESIZED,
  VGLTF_EVENT_UNKNOWN,
};

enum vgltf_key {
  VGLTF_KEY_A,
  VGLTF_KEY_B,
  VGLTF_KEY_C,
  VGLTF_KEY_D,
  VGLTF_KEY_E,
  VGLTF_KEY_F,
  VGLTF_KEY_G,
  VGLTF_KEY_H,
  VGLTF_KEY_I,
  VGLTF_KEY_J,
  VGLTF_KEY_K,
  VGLTF_KEY_L,
  VGLTF_KEY_M,
  VGLTF_KEY_N,
  VGLTF_KEY_O,
  VGLTF_KEY_P,
  VGLTF_KEY_Q,
  VGLTF_KEY_R,
  VGLTF_KEY_S,
  VGLTF_KEY_T,
  VGLTF_KEY_U,
  VGLTF_KEY_V,
  VGLTF_KEY_W,
  VGLTF_KEY_X,
  VGLTF_KEY_Y,
  VGLTF_KEY_Z,
  VGLTF_KEY_ESCAPE,
  VGLTF_KEY_UNKNOWN
};

struct vgltf_key_event {
  enum vgltf_key key;
};

struct vgltf_window_resized_event {
  int32_t width;
  int32_t height;
};

struct vgltf_event {
  enum vgltf_event_type type;
  union {
    struct vgltf_key_event key;
    struct vgltf_window_resized_event window_resized;
  };
};

struct vgltf_window_size {
  int width;
  int height;
};

struct vgltf_platform;
bool vgltf_platform_init(struct vgltf_platform *platform);
void vgltf_platform_deinit(struct vgltf_platform *platform);
bool vgltf_platform_poll_event(struct vgltf_platform *platform,
                               struct vgltf_event *event);
bool vgltf_platform_get_window_size(struct vgltf_platform *platform,
                                    struct vgltf_window_size *window_size);

// Vulkan specifics
#include "vulkan/vulkan_core.h"
char const *const *
vgltf_platform_get_vulkan_instance_extensions(struct vgltf_platform *platform,
                                              uint32_t *count);
bool vgltf_platform_create_vulkan_surface(struct vgltf_platform *platform,
                                          VkInstance instance,
                                          VkSurfaceKHR *surface);

#include "platform_sdl.h"

#endif // VGLTF_PLATFORM_H
