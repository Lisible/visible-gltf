#include "engine.h"
#include "log.h"
#include "platform.h"

int main(void) {
  struct vgltf_platform platform = {};
  if (!vgltf_platform_init(&platform)) {
    VGLTF_LOG_ERR("Platform initialization failed");
    goto err;
  }

  struct vgltf_engine engine = {};
  if (!vgltf_engine_init(&engine, &platform)) {
    VGLTF_LOG_ERR("Couldn't initialize the engine");
    goto deinit_platform;
  }

  VGLTF_LOG_INFO("Starting main loop");
  while (true) {
    struct vgltf_event event;
    while (vgltf_platform_poll_event(&platform, &event)) {
      if (event.type == VGLTF_EVENT_QUIT || (event.type == VGLTF_EVENT_KEY_DOWN &&
                                           event.key.key == VGLTF_KEY_ESCAPE)) {
        goto out_main_loop;
      }
    }

    vgltf_engine_run_frame(&engine);
  }
out_main_loop:
  VGLTF_LOG_INFO("Exiting main loop");
  vgltf_engine_deinit(&engine);
  vgltf_platform_deinit(&platform);
  return 0;
deinit_platform:
  vgltf_platform_deinit(&platform);
err:
  return -1;
}
