#ifndef VGLTF_LOG_H
#define VGLTF_LOG_H

#include <stdio.h> // IWYU pragma: keep

enum vgltf_log_level {
  VGLTF_LOG_LEVEL_DBG,
  VGLTF_LOG_LEVEL_INFO,
  VGLTF_LOG_LEVEL_ERR,
};

extern const char *vgltf_log_level_str[];

#define VGLTF_LOG(level, ...)                                                    \
  do {                                                                         \
    fprintf(stderr, "[%s %s:%d] ", vgltf_log_level_str[level], __FILE__,         \
            __LINE__);                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\n");                                                     \
  } while (0)

#define VGLTF_LOG_DBG(...) VGLTF_LOG(VGLTF_LOG_LEVEL_DBG, __VA_ARGS__)
#define VGLTF_LOG_INFO(...) VGLTF_LOG(VGLTF_LOG_LEVEL_INFO, __VA_ARGS__)
#define VGLTF_LOG_ERR(...) VGLTF_LOG(VGLTF_LOG_LEVEL_ERR, __VA_ARGS__)

#endif // VGLTF_LOG_H
