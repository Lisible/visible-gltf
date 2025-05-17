#include "alloc.h"
#include "engine.h"
#include "graphics.h"

bool vgltf_engine_init(struct vgltf_engine *engine,
                       struct vgltf_platform *platform,
                       struct vgltf_string_view model_path) {
  struct vgltf_mesh mesh;

  if (!vgltf_renderer_init(&engine->renderer, platform)) {
    goto err;
  }

  vgltf_mesh_load_from_glb(&system_allocator, model_path, &mesh);
  vgltf_renderer_upload_mesh(&engine->renderer, &mesh);
  vgltf_mesh_deinit(&system_allocator, &mesh);

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
