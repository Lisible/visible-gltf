#include "log.h"
#include "platform.h"

int main(void) {
  struct vgltf_platform platform = {};
  if (!vgltf_platform_init(&platform)) {
    VGLTF_LOG_ERR("Couldn't initialize the platform layer");
    goto err;
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
  }
out_main_loop:

  vgltf_platform_deinit(&platform);
  return 0;
err:
  return 1;
}
