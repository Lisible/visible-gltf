#include "graphics.h"
#include "platform.h"
#include "src/alloc.h"
#define CGLTF_IMPLEMENTATION
#include "vendor/cgltf.h"

static void *cgltf_alloc_func(void *user, cgltf_size size) {
  struct vgltf_allocator *allocator = user;
  return vgltf_allocator_allocate(allocator, size);
}
static void cgltf_free_func(void *user, void *ptr) {
  struct vgltf_allocator *allocator = user;
  vgltf_allocator_free(allocator, ptr);
}

static cgltf_attribute *
find_cgltf_attribute_in_primitive(struct cgltf_primitive *primitive,
                                  cgltf_attribute_type type) {
  for (cgltf_size attribute_index = 0;
       attribute_index < primitive->attributes_count; attribute_index++) {
    cgltf_attribute *attribute = &primitive->attributes[attribute_index];
    if (attribute->type == type) {
      return attribute;
    }
  }

  return nullptr;
}

bool vgltf_mesh_load_from_glb(struct vgltf_allocator *allocator,
                              struct vgltf_string_view path,
                              struct vgltf_mesh *mesh) {
  VGLTF_LOG_DBG("Loading GLB from path %s", path.data);
  cgltf_memory_options memory_options = {.alloc_func = cgltf_alloc_func,
                                         .free_func = cgltf_free_func,
                                         .user_data = allocator};
  cgltf_options options = {.memory = memory_options};

  size_t glb_file_size;
  char *glb_file_data =
      vgltf_platform_read_file_to_string(path.data, &glb_file_size);
  if (!glb_file_data) {
    VGLTF_LOG_ERR("Couldn't read GLB file at path %s", path.data);
    goto err;
  }

  cgltf_data *parsed_glb;
  if (cgltf_parse(&options, glb_file_data, glb_file_size, &parsed_glb) !=
      cgltf_result_success) {
    VGLTF_LOG_ERR("Couldn't parse GLB file at path %s", path.data);
    goto deinit_file_data;
  }

  if (cgltf_load_buffers(&options, parsed_glb, path.data) !=
      cgltf_result_success) {
    VGLTF_LOG_ERR("Couldn't load glTF buffers for GLB file at path %s",
                  path.data);
    goto deinit_parsed_glb;
  }

  cgltf_size mesh_count = parsed_glb->meshes_count;
  VGLTF_LOG_DBG("- Mesh count: %zu", mesh_count);

  cgltf_mesh *first_mesh = &parsed_glb->meshes[0];
  VGLTF_LOG_DBG("- Mesh 0: \"%s\"", first_mesh->name);
  VGLTF_LOG_DBG("-- Primitive count: \"%zu\"", first_mesh->primitives_count);

  if (first_mesh->primitives_count < 1) {
    VGLTF_LOG_ERR("Expects at least one primitive in mesh");
    goto deinit_parsed_glb;
  }

  cgltf_primitive *first_primitive = &first_mesh->primitives[0];
  if (first_primitive->type != cgltf_primitive_type_triangles) {
    VGLTF_LOG_ERR("First primitive != cgltf_primitive_type_triangles, only "
                  "triangles are supported so far");
    goto deinit_parsed_glb;
  }

  cgltf_attribute *position_attribute = find_cgltf_attribute_in_primitive(
      first_primitive, cgltf_attribute_type_position);
  if (!position_attribute) {
    VGLTF_LOG_ERR("Position attribute not found");
    goto deinit_parsed_glb;
  }
  cgltf_attribute *normal_attribute = find_cgltf_attribute_in_primitive(
      first_primitive, cgltf_attribute_type_normal);
  if (!normal_attribute) {
    VGLTF_LOG_ERR("Normal attribute not found");
    goto deinit_parsed_glb;
  }

  VGLTF_LOG_DBG("attribute count: %zu", first_primitive->attributes_count);
  cgltf_attribute *texture_coordinate_attribute =
      find_cgltf_attribute_in_primitive(first_primitive,
                                        cgltf_attribute_type_texcoord);
  if (!texture_coordinate_attribute) {
    VGLTF_LOG_ERR("Texture coordinates  attribute not found");
    goto deinit_parsed_glb;
  }

  cgltf_size position_float_count =
      cgltf_accessor_unpack_floats(position_attribute->data, nullptr, 0);
  cgltf_size normal_float_count =
      cgltf_accessor_unpack_floats(normal_attribute->data, nullptr, 0);
  cgltf_size texture_coordinate_float_count = cgltf_accessor_unpack_floats(
      texture_coordinate_attribute->data, nullptr, 0);

  cgltf_float *position_floats = vgltf_allocator_allocate(
      allocator, position_float_count * sizeof(cgltf_float));
  cgltf_float *normal_floats = vgltf_allocator_allocate(
      allocator, normal_float_count * sizeof(cgltf_float));
  cgltf_float *texture_coordinate_floats = vgltf_allocator_allocate(
      allocator, texture_coordinate_float_count * sizeof(cgltf_float));

  cgltf_accessor_unpack_floats(position_attribute->data, position_floats,
                               position_float_count);
  cgltf_accessor_unpack_floats(normal_attribute->data, normal_floats,
                               normal_float_count);
  cgltf_accessor_unpack_floats(texture_coordinate_attribute->data,
                               texture_coordinate_floats,
                               texture_coordinate_float_count);

  cgltf_size vertex_count = position_float_count / 3;
  mesh->vertices = vgltf_allocator_allocate_array(allocator, vertex_count,
                                                  sizeof(struct vgltf_vertex));
  mesh->vertex_count = 0;
  for (cgltf_size vertex_index = 0; vertex_index < vertex_count;
       vertex_index++) {
    float x = position_floats[vertex_index * 3];
    float y = position_floats[vertex_index * 3 + 1];
    float z = position_floats[vertex_index * 3 + 2];
    float nx = normal_floats[vertex_index * 3];
    float ny = normal_floats[vertex_index * 3 + 1];
    float nz = normal_floats[vertex_index * 3 + 2];
    float u = texture_coordinate_floats[vertex_index * 2];
    float v = texture_coordinate_floats[vertex_index * 2 + 1];

    mesh->vertices[vertex_index] =
        (struct vgltf_vertex){.position = {x, y, z},
                              // TODO: change color to normal
                              .color = {nx, ny, nz},
                              .texture_coordinates = {u, v}};
    mesh->vertex_count++;
  }
  vgltf_allocator_free(allocator, position_floats);
  vgltf_allocator_free(allocator, normal_floats);
  vgltf_allocator_free(allocator, texture_coordinate_floats);

  cgltf_size index_count = cgltf_accessor_unpack_indices(
      first_primitive->indices, nullptr, sizeof(uint16_t), 0);
  if (index_count == 0) {
    VGLTF_LOG_ERR("Invalid component size for mesh indices");
    goto deinit_parsed_glb;
  }

  mesh->indices =
      vgltf_allocator_allocate_array(allocator, index_count, sizeof(uint16_t));
  cgltf_accessor_unpack_indices(first_primitive->indices, mesh->indices,
                                sizeof(uint16_t), index_count);
  mesh->index_count = index_count;

  VGLTF_LOG_DBG("vertex count: %d", mesh->vertex_count);
  VGLTF_LOG_DBG("index count: %d", mesh->index_count);

  cgltf_free(parsed_glb);
  SDL_free(glb_file_data);

  return true;
deinit_parsed_glb:
  cgltf_free(parsed_glb);
deinit_file_data:
  // FIXME SDL_free shouldn't be leaked here
  SDL_free(glb_file_data);
err:
  return false;
}
void vgltf_mesh_deinit(struct vgltf_allocator *allocator,
                       struct vgltf_mesh *mesh) {
  vgltf_allocator_free(allocator, mesh->vertices);
  vgltf_allocator_free(allocator, mesh->indices);
}
