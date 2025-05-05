#ifndef VGLTF_STR_H
#define VGLTF_STR_H

#include "alloc.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h> // IWYU pragma: keep

#define SV(str)                                                                \
  (struct vgltf_string_view) { .data = str, .length = strlen(str) }

struct vgltf_string;
struct vgltf_string_view {
  const char *data;
  size_t length;
};

struct vgltf_string_view vgltf_string_view_from_literal(const char *str);
struct vgltf_string_view vgltf_string_view_from_string(struct vgltf_string string);
size_t vgltf_string_view_length(const struct vgltf_string_view *string_view);
char vgltf_string_view_at(const struct vgltf_string_view *string_view,
                        size_t index);
bool vgltf_string_view_eq(struct vgltf_string_view view,
                        struct vgltf_string_view other);
uint64_t vgltf_string_view_hash(const struct vgltf_string_view view);
// Fetches the next utf8 codepoint in the string at the given offset
// Returns the size of the codepoint in bytes, 0 in case of error
int vgltf_string_view_utf8_codepoint_at_offset(struct vgltf_string_view view,
                                             size_t offset,
                                             uint32_t *codepoint);
// codepoint has to be a char[4]
int vgltf_string_utf8_encode_codepoint(uint32_t codepoint,
                                     char encoded_codepoint[4]);

struct vgltf_string {
  char *data;
  size_t length;
};
struct vgltf_string
vgltf_string_from_null_terminated(struct vgltf_allocator *allocator,
                                const char *str);
struct vgltf_string vgltf_string_clone(struct vgltf_allocator *allocator,
                                   const struct vgltf_string string);
struct vgltf_string vgltf_string_concatenate(struct vgltf_allocator *allocator,
                                         struct vgltf_string_view head,
                                         struct vgltf_string_view tail);
struct vgltf_string vgltf_string_formatted(struct vgltf_allocator *allocator,
                                       struct vgltf_string_view fmt, ...);
struct vgltf_string vgltf_string_vformatted(struct vgltf_allocator *allocator,
                                        struct vgltf_string_view fmt,
                                        va_list args);
void vgltf_string_deinit(struct vgltf_allocator *allocator,
                       struct vgltf_string *string);
size_t vgltf_string_length(const struct vgltf_string *string);
bool vgltf_string_eq_view(const struct vgltf_string string,
                        const struct vgltf_string_view view);
uint64_t vgltf_string_hash(const struct vgltf_string string);
bool vgltf_string_eq(struct vgltf_string string, struct vgltf_string other);

#endif // VGLTF_STR_H
