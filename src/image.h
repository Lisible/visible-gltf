#ifndef VGLTF_IMAGE_H
#define VGLTF_IMAGE_H

#include <stdint.h>
#include "str.h"

enum vgltf_image_format {
  VGLTF_IMAGE_FORMAT_R8G8B8A8,
};

struct vgltf_image {
  unsigned char* data;
  uint32_t width;
  uint32_t height;
  enum vgltf_image_format format;
};

bool vgltf_image_load_from_file(struct vgltf_image* image, struct vgltf_string_view path);
void vgltf_image_deinit(struct vgltf_image* image);

#endif // VGLTF_IMAGE_H
