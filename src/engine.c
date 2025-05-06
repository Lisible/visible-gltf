#include "engine.h"
#include "src/ui.h"

bool vgltf_engine_init(struct vgltf_engine *engine,
                       struct vgltf_platform *platform) {
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
  vgltf_ui_start_frame(&engine->ui);
  vgltf_ui_begin_layout(
      &engine->ui,
      &(const struct vgltf_ui_layout_infos){
          .size = {.width = VGLTF_UI_FIXED(400), .height = VGLTF_UI_FIXED(200)},
          .background = {.type = VGLTF_UI_BACKGROUND_TYPE_COLORED,
                         .colored = VGLTF_UI_COLOR_RED}});
  vgltf_ui_end_layout(&engine->ui);

  vgltf_renderer_render_frame(&engine->renderer);
}
