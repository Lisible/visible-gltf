#include "str.h"
#include "alloc.h"
#include "hash.h"
#include "platform.h"
#include <assert.h>
#include <stdarg.h>
#include <string.h>

struct vgltf_string_view vgltf_string_view_from_literal(const char *str) {
  assert(str);
  size_t length = strlen(str);
  return (struct vgltf_string_view){.length = length, .data = str};
}
struct vgltf_string_view vgltf_string_view_from_string(struct vgltf_string string) {
  return (struct vgltf_string_view){.length = string.length, .data = string.data};
}
char vgltf_string_view_at(const struct vgltf_string_view *string_view,
                        size_t index) {
  assert(string_view);
  assert(index < string_view->length);
  return string_view->data[index];
}
bool vgltf_string_view_eq(struct vgltf_string_view view,
                        struct vgltf_string_view other) {
  return view.length == other.length &&
         (strncmp(view.data, other.data, view.length) == 0);
}
size_t vgltf_string_view_length(const struct vgltf_string_view *string_view) {
  assert(string_view);
  return string_view->length;
}

uint64_t vgltf_string_view_hash(const struct vgltf_string_view view) {
  return vgltf_hash_fnv_1a(view.data, view.length);
}

int vgltf_string_view_utf8_codepoint_at_offset(struct vgltf_string_view view,
                                             size_t offset,
                                             uint32_t *codepoint) {
  assert(codepoint);
  assert(offset < view.length);

  const unsigned char *s = (unsigned char *)&view.data[offset];

  int size;
  if ((*s & 0x80) == 0) {
    *codepoint = *s;
    size = 1;
  } else if ((*s & 0xE0) == 0xC0) {
    *codepoint = *s & 0x1f;
    size = 2;
  } else if ((*s & 0xF0) == 0xE0) {
    *codepoint = *s & 0x0f;
    size = 3;
  } else if ((*s & 0xF8) == 0xF0) {
    *codepoint = *s & 0x07;
    size = 4;
  } else {
    VGLTF_LOG_ERR("Invalid UTF-8 sequence");
    return 0;
  }

  for (int i = 1; i < size; i++) {
    if ((s[i] & 0xC0) != 0x80) {
      VGLTF_LOG_ERR("Invalid UTF-8 continuation byte");
      return 0;
    }

    *codepoint = (*codepoint << 6) | (s[i] & 0x3F);
  }

  return size;
}
int vgltf_string_utf8_encode_codepoint(uint32_t codepoint,
                                     char encoded_codepoint[4]) {
  assert(encoded_codepoint);
  if (codepoint > 0x10FFFF) {
    return -1;
  }

  if (codepoint <= 0x7F) {
    encoded_codepoint[0] = (uint8_t)codepoint;
    return 1;
  } else if (codepoint <= 0x7FF) {
    encoded_codepoint[0] = 0xC0 | ((codepoint >> 6) & 0x1F);
    encoded_codepoint[1] = 0x80 | (codepoint & 0x3F);
    return 2;
  } else if (codepoint <= 0xFFFF) {
    encoded_codepoint[0] = 0xE0 | ((codepoint >> 12) & 0x0F);
    encoded_codepoint[1] = 0x80 | ((codepoint >> 6) & 0x3F);
    encoded_codepoint[2] = 0x80 | (codepoint & 0x3F);
    return 3;
  } else {
    encoded_codepoint[0] = 0xF0 | ((codepoint >> 18) & 0x07);
    encoded_codepoint[1] = 0x80 | ((codepoint >> 12) & 0x3F);
    encoded_codepoint[2] = 0x80 | ((codepoint >> 6) & 0x3F);
    encoded_codepoint[3] = 0x80 | (codepoint & 0x3F);
    return 4;
  }
}

struct vgltf_string
vgltf_string_from_null_terminated(struct vgltf_allocator *allocator,
                                const char *str) {
  assert(allocator);
  assert(str);
  struct vgltf_string string;
  size_t length = strlen(str);
  char *data = vgltf_allocator_allocate(allocator, length + 1);
  if (!data) {
    VGLTF_PANIC("Couldn't allocate string");
  }
  strncpy(data, str, length);
  string.length = length;
  string.data = data;
  return string;
}
struct vgltf_string vgltf_string_clone(struct vgltf_allocator *allocator,
                                   const struct vgltf_string string) {
  assert(allocator);

  size_t length = string.length;
  char *data = vgltf_allocator_allocate(allocator, length + 1);
  memcpy(data, string.data, length);
  data[length] = '\0';

  return (struct vgltf_string){.data = data, .length = length};
}
struct vgltf_string vgltf_string_concatenate(struct vgltf_allocator *allocator,
                                         struct vgltf_string_view head,
                                         struct vgltf_string_view tail) {
  assert(allocator);
  size_t length = head.length + tail.length;
  char *data = vgltf_allocator_allocate(allocator, length + 1);
  memcpy(data, head.data, head.length);
  memcpy(data + head.length, tail.data, tail.length);
  data[length] = '\0';
  return (struct vgltf_string){.data = data, .length = length};
}
struct vgltf_string vgltf_string_formatted(struct vgltf_allocator *allocator,
                                       struct vgltf_string_view fmt, ...) {
  va_list args;
  va_start(args, fmt);
  struct vgltf_string formatted_string =
      vgltf_string_vformatted(allocator, fmt, args);
  va_end(args);

  return formatted_string;
}
struct vgltf_string vgltf_string_vformatted(struct vgltf_allocator *allocator,
                                        struct vgltf_string_view fmt,
                                        va_list args) {
  assert(allocator);
  char str[1024];
  size_t length = vsnprintf(str, 1024, fmt.data, args);
  char *data = vgltf_allocator_allocate(allocator, length + 1);
  memcpy(data, str, length);
  data[length] = '\0';
  return (struct vgltf_string){.data = data, .length = length};
}
void vgltf_string_deinit(struct vgltf_allocator *allocator,
                       struct vgltf_string *string) {
  assert(allocator);
  assert(string);
  vgltf_allocator_free(allocator, string->data);
}
size_t vgltf_string_length(const struct vgltf_string *string) {
  return string->length;
}
bool vgltf_string_eq_view(const struct vgltf_string string,
                        const struct vgltf_string_view view) {
  return string.length == view.length &&
         (strncmp(string.data, view.data, string.length) == 0);
}
uint64_t vgltf_string_hash(const struct vgltf_string string) {
  return vgltf_hash_fnv_1a(string.data, string.length);
}
bool vgltf_string_eq(struct vgltf_string string, struct vgltf_string other) {
  return string.length == other.length &&
         (strncmp(string.data, other.data, string.length) == 0);
}
