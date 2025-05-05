#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

bool vgltf_image_load_from_file(struct vgltf_image *image,
                              struct vgltf_string_view path) {
  int width;
  int height;
  int tex_channels;
  image->data =
      stbi_load(path.data, &width, &height, &tex_channels, STBI_rgb_alpha);
  image->width = width;
  image->height = height;
  image->format = VGLTF_IMAGE_FORMAT_R8G8B8A8;

  return image->data != nullptr;
}

void vgltf_image_deinit(struct vgltf_image *image) { stbi_image_free(image->data); }
