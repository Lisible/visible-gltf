#include "engine.h"

bool vgltf_engine_init(struct vgltf_engine *engine, struct vgltf_platform *platform) {
  if (!vgltf_renderer_init(&engine->renderer, platform)) {
    goto err;
  }

  return true;
err:
  return false;
}
void vgltf_engine_deinit(struct vgltf_engine *engine) {
  vgltf_renderer_deinit(&engine->renderer);
}
void vgltf_engine_run_frame(struct vgltf_engine *engine) {
  vgltf_renderer_render_frame(&engine->renderer);
}
