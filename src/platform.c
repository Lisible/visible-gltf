#include "platform.h"

#define VGLTF_GENERATE_KEY_STRING(KEY) #KEY,
const char *vgltf_key_str[] = {VGLTF_FOREACH_KEY(VGLTF_GENERATE_KEY_STRING)};
#undef VGLTF_GENERATE_KEY_STRING
