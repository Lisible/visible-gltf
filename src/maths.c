#include "maths.h"
#include <math.h>
#include <string.h>

vgltf_vec3 vgltf_vec3_sub(vgltf_vec3 lhs, vgltf_vec3 rhs) {
  return (vgltf_vec3){.x = lhs.x - rhs.x, .y = lhs.y - rhs.y, .z = lhs.z - rhs.z};
}
vgltf_vec3 vgltf_vec3_cross(vgltf_vec3 lhs, vgltf_vec3 rhs) {
  return (vgltf_vec3){.x = lhs.y * rhs.z - lhs.z * rhs.y,
                    .y = lhs.z * rhs.x - lhs.x * rhs.z,
                    .z = lhs.x * rhs.y - lhs.y * rhs.x};
}
vgltf_vec_value_type vgltf_vec3_dot(vgltf_vec3 lhs, vgltf_vec3 rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}
vgltf_vec_value_type vgltf_vec3_length(vgltf_vec3 vec) {
  return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}
vgltf_vec3 vgltf_vec3_normalized(vgltf_vec3 vec) {
  vgltf_vec_value_type length = vgltf_vec3_length(vec);
  return (vgltf_vec3){
      .x = vec.x / length, .y = vec.y / length, .z = vec.z / length};
}
void vgltf_mat4_multiply(vgltf_mat4 out, vgltf_mat4 lhs, vgltf_mat4 rhs) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      out[i * 4 + j] =
          lhs[i * 4 + 0] * rhs[0 * 4 + j] + lhs[i * 4 + 1] * rhs[1 * 4 + j] +
          lhs[i * 4 + 2] * rhs[2 * 4 + j] + lhs[i * 4 + 3] * rhs[3 * 4 + j];
    }
  }
}
void vgltf_mat4_rotate(vgltf_mat4 out, vgltf_mat4 matrix,
                     vgltf_mat_value_type angle_radians, vgltf_vec3 axis) {
  vgltf_vec3 a = vgltf_vec3_normalized(axis);
  vgltf_vec_value_type c = cosf(angle_radians);
  vgltf_vec_value_type s = sinf(angle_radians);
  vgltf_vec_value_type t = 1.f - c;

  vgltf_mat4 rotation_matrix = {t * a.x * a.x + c,
                              t * a.x * a.y - s * a.z,
                              t * a.x * a.z + s * a.y,
                              0.f,
                              t * a.x * a.y + s * a.z,
                              t * a.y * a.y + c,
                              t * a.y * a.z - s * a.x,
                              0.f,
                              t * a.x * a.z - s * a.y,
                              t * a.y * a.z + s * a.x,
                              t * a.z * a.z + c,
                              0.f,
                              0.f,
                              0.f,
                              0.f,
                              1.f};

  vgltf_mat4_multiply(out, matrix, rotation_matrix);
}
void vgltf_mat4_look_at(vgltf_mat4 out, vgltf_vec3 eye_position,
                      vgltf_vec3 target_position, vgltf_vec3 up_axis) {
  vgltf_vec3 forward =
      vgltf_vec3_normalized(vgltf_vec3_sub(target_position, eye_position));
  vgltf_vec3 right = vgltf_vec3_normalized(vgltf_vec3_cross(forward, up_axis));
  vgltf_vec3 camera_up = vgltf_vec3_cross(right, forward);

  memcpy(out, (const vgltf_mat4)VGLTF_MAT4_IDENTITY, sizeof(vgltf_mat4));
  out[0 * 4 + 0] = right.x;
  out[1 * 4 + 0] = right.y;
  out[2 * 4 + 0] = right.z;
  out[0 * 4 + 1] = camera_up.x;
  out[1 * 4 + 1] = camera_up.y;
  out[2 * 4 + 1] = camera_up.z;
  out[0 * 4 + 2] = -forward.x;
  out[1 * 4 + 2] = -forward.y;
  out[2 * 4 + 2] = -forward.z;
  out[3 * 4 + 0] = -vgltf_vec3_dot(right, eye_position);
  out[3 * 4 + 1] = -vgltf_vec3_dot(camera_up, eye_position);
  out[3 * 4 + 2] = vgltf_vec3_dot(forward, eye_position);
}
void vgltf_mat4_perspective(vgltf_mat4 out, vgltf_mat_value_type fov_radians,
                          vgltf_mat_value_type aspect_ratio,
                          vgltf_mat_value_type near, vgltf_mat_value_type far) {
  float tan_half_fovy = tanf(fov_radians / 2.0f);
  out[0] = 1.f / (aspect_ratio * tan_half_fovy);
  out[1] = 0.0f;
  out[2] = 0.0f;
  out[3] = 0.0f;

  out[4] = 0.0f;
  out[5] = 1.f / tan_half_fovy;
  out[6] = 0.0f;
  out[7] = 0.0f;

  out[8] = 0.0f;
  out[9] = 0.0f;
  out[10] = -(far + near) / (far - near);
  out[11] = -1.0f;

  out[12] = 0.0f;
  out[13] = 0.0f;
  out[14] = -(2.0f * far * near) / (far - near);
  out[15] = 0.0f;
}
