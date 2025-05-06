
#ifndef VGLTF_UI_H
#define VGLTF_UI_H

#define VGLTF_UI_COLOR_RED (struct vgltf_ui_color){0xFF, 0x00, 0x00}
#define VGLTF_UI_COLOR_GREEN (struct vgltf_ui_color){0x00, 0xFF, 0x00}
#define VGLTF_UI_COLOR_BLUE (struct vgltf_ui_color){0x00, 0x00, 0xFF}

#define VGLTF_UI_FIXED(value) {.size = value}
#define VGLTF_UI_GROW(value) {.type = VGLTF_UI_SIZING_TYPE_GROW}

struct vgltf_ui_color {
  unsigned char r;
  unsigned char g;
  unsigned char b;
};

enum vgltf_ui_sizing_type {
  VGLTF_UI_SIZING_TYPE_FIXED,
  VGLTF_UI_SIZING_TYPE_GROW,
};

struct vgltf_ui_sizing {
  enum vgltf_ui_sizing_type type;
  float size;
};

struct vgltf_ui_2d_sizing {
  struct vgltf_ui_sizing width;
  struct vgltf_ui_sizing height;
};

enum vgltf_ui_background_type {
  VGLTF_UI_BACKGROUND_TYPE_TRANSPARENT,
  VGLTF_UI_BACKGROUND_TYPE_COLORED,
};

struct vgltf_ui_colored_background {
  struct vgltf_ui_color background_color;
};

struct vgltf_ui_background {
  enum vgltf_ui_background_type type;
  union {
    struct vgltf_ui_colored_background colored;
  };
};

struct vgltf_ui_layout_infos {
  struct vgltf_ui_2d_sizing size;
  struct vgltf_ui_background background;
};

struct vgltf_ui_root_size {
  float width;
  float height;
};

struct vgltf_ui {
  struct vgltf_ui_root_size root_size;
};
bool vgltf_ui_init(struct vgltf_ui *ui, struct vgltf_ui_root_size root_size);
void vgltf_ui_start_frame(struct vgltf_ui *ui);
void vgltf_ui_begin_layout(struct vgltf_ui *ui,
                           const struct vgltf_ui_layout_infos *layout_infos);
void vgltf_ui_end_layout(struct vgltf_ui *ui);

#endif // VGLTF_UI_H
