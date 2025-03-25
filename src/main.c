#include "log.h"
#include "platform.h"
#include "renderer.h"

int main(void) {
  struct vgltf_platform platform = {};
  if (!vgltf_platform_init(&platform)) {
    VGLTF_LOG_ERR("Couldn't initialize the platform layer");
    goto err;
  }

  struct vgltf_renderer renderer = {};
  if (!vgltf_renderer_init(&renderer, &platform)) {
    VGLTF_LOG_ERR("Couldn't initialize the renderer");
    goto deinit_platform;
  }

  while (true) {
    struct vgltf_event event;
    while (vgltf_platform_poll_event(&platform, &event)) {
      if (event.type == VGLTF_EVENT_QUIT ||
          (event.type == VGLTF_EVENT_KEY_DOWN &&
           event.key.key == VGLTF_KEY_ESCAPE)) {
        goto out_main_loop;
      }
    }

    vgltf_renderer_triangle_pass(&renderer);
    renderer.current_frame =
        (renderer.current_frame + 1) % VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT;
  }
out_main_loop:
  vgltf_renderer_deinit(&renderer);
  vgltf_platform_deinit(&platform);
  return 0;
deinit_platform:
  vgltf_platform_deinit(&platform);
err:
  return 1;
}
