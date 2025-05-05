#include "platform_sdl.h"
#include "log.h"
#include "platform.h"

bool vgltf_platform_init(struct vgltf_platform *platform) {
  VGLTF_LOG_INFO("Initializing SDL platform...");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    VGLTF_LOG_ERR("SDL initialization failed: %s", SDL_GetError());
    goto err;
  }

  platform->window = SDL_CreateWindow("vgltf", 800, 600, SDL_WINDOW_VULKAN);
  if (!platform->window) {
    VGLTF_LOG_ERR("Window creation failed: %s", SDL_GetError());
    goto deinit_sdl;
  }

  VGLTF_LOG_INFO("SDL platform initialized");
  return true;

deinit_sdl:
  SDL_Quit();
err:
  return false;
}
void vgltf_platform_deinit(struct vgltf_platform *platform) {
  SDL_DestroyWindow(platform->window);
  SDL_Quit();
  VGLTF_LOG_INFO("SDL platform deinitialized");
}

#define VGLTF_GENERATE_SDL_KEYCODE_MAPPING(KEY)                                \
  case SDLK_##KEY:                                                             \
    return VGLTF_KEY_##KEY;

static enum vgltf_key vgltf_key_from_sdl_keycode(SDL_Keycode key_code) {
  switch (key_code) {
    VGLTF_FOREACH_KEY(VGLTF_GENERATE_SDL_KEYCODE_MAPPING)
  default:
    return VGLTF_KEY_UNKNOWN;
  }
}

#undef VGLTF_GENERATE_SDL_KEYCODE_MAPPING

bool vgltf_platform_poll_event(struct vgltf_platform *platform,
                               struct vgltf_event *event) {
  (void)platform;
  SDL_Event sdl_event;
  bool pending_events = SDL_PollEvent(&sdl_event);
  if (pending_events) {
    switch (sdl_event.type) {
    case SDL_EVENT_QUIT:
      event->type = VGLTF_EVENT_QUIT;
      break;
    case SDL_EVENT_KEY_DOWN:
      event->type = VGLTF_EVENT_KEY_DOWN;
      event->key.key = vgltf_key_from_sdl_keycode(sdl_event.key.key);
      break;
    default:
      event->type = VGLTF_EVENT_UNKNOWN;
      break;
    }
  }

  return pending_events;
}
bool vgltf_platform_get_window_size(struct vgltf_platform *platform,
                                    struct vgltf_window_size *window_size) {
  return SDL_GetWindowSize(platform->window, &window_size->width,
                           &window_size->height);
}
bool vgltf_platform_get_current_time_nanoseconds(long *time) {
  if (!SDL_GetCurrentTime(time)) {
    VGLTF_LOG_ERR("'SDL_GetCurrentTime failed: %s", SDL_GetError());
    goto err;
  }

  return true;
err:
  return false;
}

char *vgltf_platform_read_file_to_string(const char *filepath,
                                         size_t *out_size) {
  char *file_data = SDL_LoadFile(filepath, out_size);
  if (!file_data) {
    VGLTF_LOG_ERR("Couldn't load file: %s", SDL_GetError());
    return NULL;
  }

  return file_data;
}

#include <SDL3/SDL_vulkan.h>

const char *const *
vgltf_platform_get_vulkan_instance_extensions(struct vgltf_platform *platform,
                                              uint32_t *count) {
  (void)platform;
  return SDL_Vulkan_GetInstanceExtensions(count);
}
bool vgltf_platform_create_vulkan_surface(struct vgltf_platform *platform,
                                          VkInstance instance,
                                          VkSurfaceKHR *surface) {
  return SDL_Vulkan_CreateSurface(platform->window, instance, nullptr, surface);
}
