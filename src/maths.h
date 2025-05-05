#ifndef VGLTF_MATHS_H
#define VGLTF_MATHS_H

typedef float vgltf_vec_value_type;

constexpr double VGLTF_MATHS_PI = 3.14159265358979323846;
#define VGLTF_MATHS_DEG_TO_RAD(deg) (deg * VGLTF_MATHS_PI / 180.0)
#define VGLTF_MAX(x, y) ((x) > (y) ? (x) : (y))

typedef struct {
  vgltf_vec_value_type x;
  vgltf_vec_value_type y;
} vgltf_vec2;

typedef struct {
  vgltf_vec_value_type x;
  vgltf_vec_value_type y;
  vgltf_vec_value_type z;
} vgltf_vec3;
vgltf_vec3 vgltf_vec3_sub(vgltf_vec3 lhs, vgltf_vec3 rhs);
vgltf_vec3 vgltf_vec3_cross(vgltf_vec3 lhs, vgltf_vec3 rhs);
vgltf_vec_value_type vgltf_vec3_dot(vgltf_vec3 lhs, vgltf_vec3 rhs);

vgltf_vec_value_type vgltf_vec3_length(vgltf_vec3 vec);
vgltf_vec3 vgltf_vec3_normalized(vgltf_vec3 vec);

typedef vgltf_vec_value_type vgltf_mat_value_type;

// row major
typedef vgltf_mat_value_type vgltf_mat4[16];
void vgltf_mat4_multiply(vgltf_mat4 out, vgltf_mat4 lhs, vgltf_mat4 rhs);
void vgltf_mat4_rotate(vgltf_mat4 out, vgltf_mat4 matrix,
                     vgltf_mat_value_type angle_radians, vgltf_vec3 axis);
void vgltf_mat4_look_at(vgltf_mat4 out, vgltf_vec3 eye_position,
                      vgltf_vec3 target_position, vgltf_vec3 up_axis);
void vgltf_mat4_perspective(vgltf_mat4 out, vgltf_mat_value_type fov,
                          vgltf_mat_value_type aspect_ratio,
                          vgltf_mat_value_type near, vgltf_mat_value_type far);

// clang-format off
#define VGLTF_MAT4_IDENTITY { \
  1, 0, 0, 0, \
  0, 1, 0, 0, \
  0, 0, 1, 0, \
  0, 0, 0, 1, \
}
// clang-format on

#endif // VGLTF_MATHS_H
