#ifndef VGLTF_GRAPHICS_H
#define VGLTF_GRAPHICS_H

#include "alloc.h"
#include "maths.h"
#include "str.h"
#include <stdint.h>

typedef uint16_t vgltf_vertex_index;

struct vgltf_vertex {
  vgltf_vec3 position;
  vgltf_vec3 color;
  vgltf_vec2 texture_coordinates;
};

struct vgltf_mesh {
  struct vgltf_vertex *vertices;
  vgltf_vertex_index *indices;
  int vertex_count;
  int index_count;
};
bool vgltf_mesh_load_from_glb(struct vgltf_allocator *allocator,
                              struct vgltf_string_view path,
                              struct vgltf_mesh *mesh);
void vgltf_mesh_deinit(struct vgltf_allocator *allocator,
                       struct vgltf_mesh *mesh);

#endif // VGLTF_GRAPHICS_H
