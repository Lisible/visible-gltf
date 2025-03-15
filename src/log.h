#ifndef VGLTF_LOG_H
#define VGLTF_LOG_H

#include <stdio.h>

enum vgltf_log_level {
  VGLTF_LOG_DEBUG,
  VGLTF_LOG_INFO,
  VGLTF_LOG_ERROR,
};
const char *vgltf_log_level_to_str(enum vgltf_log_level level);

#define VGLTF_LOG(level, ...)                                                  \
  do {                                                                         \
    fprintf(stderr, "[%s %s:%d] ", vgltf_log_level_to_str(level), __FILE__,    \
            __LINE__);                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\n");                                                     \
  } while (0)

#define VGLTF_LOG_DBG(...) VGLTF_LOG(VGLTF_LOG_DEBUG, __VA_ARGS__)
#define VGLTF_LOG_INFO(...) VGLTF_LOG(VGLTF_LOG_INFO, __VA_ARGS__)
#define VGLTF_LOG_ERR(...) VGLTF_LOG(VGLTF_LOG_ERROR, __VA_ARGS__)

#endif // VGLTF_LOG_H
