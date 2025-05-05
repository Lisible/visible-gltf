#ifndef VGLTF_PLATFORM_H
#define VGLTF_PLATFORM_H

#include "log.h"
#include <stdint.h>
#include <stdlib.h> // IWYU pragma: keep

#define VGLTF_PANIC(...)                                                         \
  do {                                                                         \
    VGLTF_LOG_ERR("PANIC " __VA_ARGS__);                                         \
    exit(1);                                                                   \
  } while (0)

#define VGLTF_FOREACH_KEY(_M)                                                    \
  _M(A)                                                                        \
  _M(B)                                                                        \
  _M(C)                                                                        \
  _M(D)                                                                        \
  _M(E)                                                                        \
  _M(F)                                                                        \
  _M(G)                                                                        \
  _M(H)                                                                        \
  _M(I)                                                                        \
  _M(J)                                                                        \
  _M(K)                                                                        \
  _M(L)                                                                        \
  _M(M)                                                                        \
  _M(N)                                                                        \
  _M(O)                                                                        \
  _M(P)                                                                        \
  _M(Q)                                                                        \
  _M(R)                                                                        \
  _M(S)                                                                        \
  _M(T)                                                                        \
  _M(U)                                                                        \
  _M(V)                                                                        \
  _M(W)                                                                        \
  _M(X)                                                                        \
  _M(Y)                                                                        \
  _M(Z)                                                                        \
  _M(ESCAPE)

#define VGLTF_GENERATE_KEY_ENUM(KEY) VGLTF_KEY_##KEY,
enum vgltf_key {
  VGLTF_FOREACH_KEY(VGLTF_GENERATE_KEY_ENUM) VGLTF_KEY_COUNT,
  VGLTF_KEY_UNKNOWN
};
#undef VGLTF_GENERATE_KEY_ENUM
extern const char *vgltf_key_str[];

enum vgltf_event_type { VGLTF_EVENT_QUIT, VGLTF_EVENT_KEY_DOWN, VGLTF_EVENT_UNKNOWN };

struct vgltf_key_event {
  enum vgltf_key key;
};

struct vgltf_event {
  enum vgltf_event_type type;
  union {
    struct vgltf_key_event key;
  };
};

struct vgltf_window_size {
  int width;
  int height;
};

struct vgltf_platform;
bool vgltf_platform_init(struct vgltf_platform *platform);
void vgltf_platform_deinit(struct vgltf_platform *platform);
bool vgltf_platform_poll_event(struct vgltf_platform *platform,
                             struct vgltf_event *event);
bool vgltf_platform_get_window_size(struct vgltf_platform *platform,
                                  struct vgltf_window_size *window_size);
bool vgltf_platform_get_current_time_nanoseconds(long *time);
char *vgltf_platform_read_file_to_string(const char *filepath, size_t *out_size);
const char *const *
vgltf_platform_get_vulkan_instance_extensions(struct vgltf_platform *platform,
                                            uint32_t *count);

#include <vulkan/vulkan.h>
bool vgltf_platform_create_vulkan_surface(struct vgltf_platform *platform,
                                        VkInstance instance,
                                        VkSurfaceKHR *surface);

#include "platform_sdl.h"

#endif // VGLTF_PLATFORM_H
