#ifndef VGLTF_ALLOC_H
#define VGLTF_ALLOC_H

#include <stddef.h>

struct vgltf_allocator {
  void *(*allocate)(size_t size, void *ctx);
  void *(*allocate_aligned)(size_t alignment, size_t size, void *ctx);
  void *(*allocate_array)(size_t count, size_t item_size, void *ctx);
  void *(*reallocate)(void *ptr, size_t old_size, size_t new_size, void *ctx);
  void (*free)(void *ptr, void *ctx);
  void *ctx;
};

void *vgltf_allocator_allocate(struct vgltf_allocator *allocator, size_t size);
void *vgltf_allocator_allocate_aligned(struct vgltf_allocator *allocator,
                                     size_t alignment, size_t size);
void *vgltf_allocator_allocate_array(struct vgltf_allocator *allocator,
                                   size_t count, size_t item_size);
void *vgltf_allocator_reallocate(struct vgltf_allocator *allocator, void *ptr,
                               size_t old_size, size_t new_size);
void vgltf_allocator_free(struct vgltf_allocator *allocator, void *ptr);

extern thread_local struct vgltf_allocator system_allocator;

struct vgltf_arena {
  size_t capacity;
  size_t size;
  char *data;
};
void vgltf_arena_init(struct vgltf_allocator *allocator, struct vgltf_arena *arena,
                    size_t size);
void vgltf_arena_deinit(struct vgltf_allocator *allocator, struct vgltf_arena *arena);
void *vgltf_arena_allocate(struct vgltf_arena *arena, size_t size);
void *vgltf_arena_allocate_array(struct vgltf_arena *arena, size_t count,
                               size_t item_size);
void vgltf_arena_reset(struct vgltf_arena *arena);
struct vgltf_allocator vgltf_arena_allocator(struct vgltf_arena *arena);

#endif // VGLTF_ALLOC_H
