#include "log.h"

const char *vgltf_log_level_to_str(enum vgltf_log_level level) {
  switch (level) {
  case VGLTF_LOG_ERROR:
    return "error";
  case VGLTF_LOG_INFO:
    return "info";
  case VGLTF_LOG_DEBUG:
    return "debug";
  }
}
