#ifndef VGLTF_ENGINE_H
#define VGLTF_ENGINE_H

#include "renderer/renderer.h"

struct vgltf_engine {
  struct vgltf_renderer renderer;
};

bool vgltf_engine_init(struct vgltf_engine *engine, struct vgltf_platform *platform);
void vgltf_engine_deinit(struct vgltf_engine *engine);
void vgltf_engine_run_frame(struct vgltf_engine *engine);

#endif // VGLTF_ENGINE_H
