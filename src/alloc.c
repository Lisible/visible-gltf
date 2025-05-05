#include "alloc.h"
#include "maths.h"
#include "platform.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void *vgltf_allocator_allocate(struct vgltf_allocator *allocator, size_t size) {
  assert(allocator);
  return allocator->allocate(size, allocator->ctx);
}
void *vgltf_allocator_allocate_aligned(struct vgltf_allocator *allocator,
                                     size_t alignment, size_t size) {
  assert(allocator);
  return allocator->allocate_aligned(alignment, size, allocator->ctx);
}
void *vgltf_allocator_allocate_array(struct vgltf_allocator *allocator,
                                   size_t count, size_t item_size) {
  assert(allocator);
  return allocator->allocate_array(count, item_size, allocator->ctx);
}
void *vgltf_allocator_reallocate(struct vgltf_allocator *allocator, void *ptr,
                               size_t old_size, size_t new_size) {
  assert(allocator);
  return allocator->reallocate(ptr, old_size, new_size, allocator->ctx);
}
void vgltf_allocator_free(struct vgltf_allocator *allocator, void *ptr) {
  assert(allocator);
  allocator->free(ptr, allocator->ctx);
}

static void *memory_allocate(size_t size, void *ctx) {
  (void)ctx;
  void *ptr = malloc(size);
  if (!ptr) {
    VGLTF_PANIC("Couldn't allocate memory (out of mem?)");
  }
  return ptr;
}

static void *memory_allocate_aligned(size_t alignment, size_t size, void *ctx) {
  (void)ctx;
#ifdef VGLTF_PLATFORM_WINDOWS
  void *ptr = _aligned_malloc(size, VGLTF_MAX(alignment, sizeof(void *)));
#else
  void *ptr = aligned_alloc(VGLTF_MAX(alignment, sizeof(void *)), size);
#endif
  if (!ptr) {
    VGLTF_PANIC("Couldn't allocate aligned memory (out of mem?)");
  }
  return ptr;
}

static void *memory_allocate_array(size_t count, size_t item_size, void *ctx) {
  (void)ctx;
  void *ptr = calloc(count, item_size);
  if (!ptr) {
    VGLTF_PANIC("Couldn't allocate memory (out of mem?)");
  }
  return ptr;
}

static void *memory_reallocate(void *ptr, size_t old_size, size_t new_size,
                               void *ctx) {
  (void)old_size;
  (void)ctx;
  ptr = realloc(ptr, new_size);
  if (!ptr) {
    VGLTF_PANIC("Couldn't allocate memory (out of mem?)");
  }
  return ptr;
}

static void memory_free(void *ptr, void *ctx) {
  (void)ctx;
  free(ptr);
}

thread_local struct vgltf_allocator system_allocator = {
    .allocate = memory_allocate,
    .allocate_aligned = memory_allocate_aligned,
    .allocate_array = memory_allocate_array,
    .reallocate = memory_reallocate,
    .free = memory_free};

void vgltf_arena_init(struct vgltf_allocator *allocator, struct vgltf_arena *arena,
                    size_t size) {
  assert(allocator);
  assert(arena);
  arena->size = 0;
  arena->capacity = size;
  arena->data = vgltf_allocator_allocate(allocator, size);
}
void vgltf_arena_deinit(struct vgltf_allocator *allocator,
                      struct vgltf_arena *arena) {
  assert(allocator);
  assert(arena);
  vgltf_allocator_free(allocator, arena->data);
}
void *vgltf_arena_allocate(struct vgltf_arena *arena, size_t size) {
  assert(arena);
  assert(arena->size + size <= arena->capacity);
  void *ptr = arena->data + arena->size;
  arena->size += size;
  return ptr;
}

void *vgltf_arena_allocate_array(struct vgltf_arena *arena, size_t count,
                               size_t item_size) {
  assert(arena);
  void *ptr = vgltf_arena_allocate(arena, count * item_size);
  memset(ptr, 0, count * item_size);
  return ptr;
}

void vgltf_arena_reset(struct vgltf_arena *arena) {
  assert(arena);
  arena->size = 0;
}

static void *arena_allocator_allocate(size_t size, void *ctx) {
  assert(ctx);
  return vgltf_arena_allocate(ctx, size);
}
static void *arena_allocator_allocate_aligned(size_t alignment, size_t size,
                                              void *ctx) {
  assert(ctx);
  if (alignment < sizeof(void *) || (alignment & (alignment - 1)) != 0) {
    return NULL;
  }

  void *ptr = vgltf_arena_allocate(ctx, size + alignment - 1 + sizeof(void *));
  if (!ptr) {
    return NULL;
  }

  return (void *)(((uintptr_t)ptr + sizeof(void *) + alignment - 1) &
                  ~(alignment - 1));
}

static void *arena_allocator_allocate_array(size_t count, size_t item_size,
                                            void *ctx) {
  assert(ctx);
  return vgltf_arena_allocate_array(ctx, count, item_size);
}

static void *arena_allocator_reallocate(void *ptr, size_t old_size,
                                        size_t new_size, void *ctx) {
  assert(ptr);
  assert(ctx);

  void *new_ptr = vgltf_arena_allocate(ctx, new_size);
  memcpy(new_ptr, ptr, old_size);
  return new_ptr;
}

static void arena_allocator_free(void *ptr, void *ctx) {
  assert(ctx);
  (void)ptr;
}

struct vgltf_allocator vgltf_arena_allocator(struct vgltf_arena *arena) {
  return (struct vgltf_allocator){
      .ctx = arena,
      .allocate = arena_allocator_allocate,
      .allocate_aligned = arena_allocator_allocate_aligned,
      .allocate_array = arena_allocator_allocate_array,
      .reallocate = arena_allocator_reallocate,
      .free = arena_allocator_free};
}
