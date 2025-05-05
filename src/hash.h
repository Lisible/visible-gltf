#ifndef VGLTF_HASH_H
#define VGLTF_HASH_H

#include <stddef.h>
#include <stdint.h>

uint64_t vgltf_hash_fnv_1a(const char *bytes, size_t nbytes);

#endif // VGLTF_HASH_H
