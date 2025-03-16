#include "log.h"
#include "platform.h"
#include "platform_sdl.h"
#include <SDL3/SDL_vulkan.h>

bool vgltf_platform_init(struct vgltf_platform *platform) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    VGLTF_LOG_ERR("SDL initialization failed: %s", SDL_GetError());
    goto err;
  }

  constexpr char WINDOW_TITLE[] = "VisibleGLTF";
  constexpr int WINDOW_WIDTH = 800;
  constexpr int WINDOW_HEIGHT = 600;
  SDL_Window *window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_WIDTH,
                                        WINDOW_HEIGHT, SDL_WINDOW_VULKAN);
  if (!window) {
    VGLTF_LOG_ERR("SDL window creation failed: %s", SDL_GetError());
    goto quit_sdl;
  }

  platform->window = window;

  return true;
quit_sdl:
  SDL_Quit();
err:
  return false;
}
void vgltf_platform_deinit(struct vgltf_platform *platform) {
  SDL_DestroyWindow(platform->window);
  SDL_Quit();
}
static enum vgltf_key vgltf_key_from_sdl_keycode(SDL_Keycode keycode) {
  switch (keycode) {
  case SDLK_A:
    return VGLTF_KEY_A;
  case SDLK_B:
    return VGLTF_KEY_B;
  case SDLK_C:
    return VGLTF_KEY_C;
  case SDLK_D:
    return VGLTF_KEY_D;
  case SDLK_E:
    return VGLTF_KEY_E;
  case SDLK_F:
    return VGLTF_KEY_F;
  case SDLK_G:
    return VGLTF_KEY_G;
  case SDLK_H:
    return VGLTF_KEY_H;
  case SDLK_I:
    return VGLTF_KEY_I;
  case SDLK_J:
    return VGLTF_KEY_J;
  case SDLK_K:
    return VGLTF_KEY_K;
  case SDLK_L:
    return VGLTF_KEY_L;
  case SDLK_M:
    return VGLTF_KEY_M;
  case SDLK_N:
    return VGLTF_KEY_N;
  case SDLK_O:
    return VGLTF_KEY_O;
  case SDLK_P:
    return VGLTF_KEY_P;
  case SDLK_Q:
    return VGLTF_KEY_Q;
  case SDLK_R:
    return VGLTF_KEY_R;
  case SDLK_S:
    return VGLTF_KEY_S;
  case SDLK_T:
    return VGLTF_KEY_T;
  case SDLK_U:
    return VGLTF_KEY_U;
  case SDLK_V:
    return VGLTF_KEY_V;
  case SDLK_W:
    return VGLTF_KEY_W;
  case SDLK_X:
    return VGLTF_KEY_X;
  case SDLK_Y:
    return VGLTF_KEY_Y;
  case SDLK_Z:
    return VGLTF_KEY_Z;
  case SDLK_ESCAPE:
    return VGLTF_KEY_ESCAPE;
  default:
    return VGLTF_KEY_UNKNOWN;
  }
}
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
char const *const *
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
