#include <stdio.h>
#include <stdint.h>

struct Vector2U32 {
  uint32_t x;
  uint32_t y;
};
int sum_vector2_u32(struct Vector2U32 vec) {
  return (int)vec.x + (int)vec.y;
}

struct Vector3U32 {
  uint32_t x;
  uint32_t y;
  uint32_t z;
};
int sum_vector3_u32(struct Vector3U32 vec) {
  return (int)vec.x + (int)vec.y + (int)vec.z;
}

struct Vector2U64 {
  uint64_t x;
  uint64_t y;
};
int sum_vector2_u64(struct Vector2U64 vec) {
  return (int)vec.x + (int)vec.y;
}

struct Vector3U64 {
  uint64_t x;
  uint64_t y;
  uint64_t z;
};
int sum_vector3_u64(struct Vector3U64 vec) {
  return (int)vec.x + (int)vec.y + (int)vec.z;
}

struct Vector2F32 {
  float x;
  float y;
};
int sum_vector2_f32(struct Vector2F32 vec) {
  return (int)vec.x + (int)vec.y;
}

struct Vector3F32 {
  float x;
  float y;
  float z;
};
int sum_vector3_f32(struct Vector3F32 vec) {
  return (int)vec.x + (int)vec.y + (int)vec.z;
}

struct Vector4F32 {
  float x;
  float y;
  float z;
  float u;
};
int sum_vector4_f32(struct Vector4F32 vec) {
  return (int)vec.x + (int)vec.y + (int)vec.z + (int)vec.u;
}

struct Vector5F32 {
  float x;
  float y;
  float z;
  float u;
  float v;
};
int sum_vector5_f32(struct Vector5F32 vec) {
  return (int)vec.x + (int)vec.y + (int)vec.z + (int)vec.u + (int)vec.v;
}

struct Vector2F64 {
  double x;
  double y;
};
int sum_vector2_f64(struct Vector2F64 vec) {
  return (int)vec.x + (int)vec.y;
}

struct Vector3F64 {
  double x;
  double y;
  double z;
};
int sum_vector3_f64(struct Vector3F64 vec) {
  return (int)vec.x + (int)vec.y + (int)vec.z;
}

struct Vector4F64 {
  double x;
  double y;
  double z;
  double u;
};
int sum_vector4_f64(struct Vector4F64 vec) {
  return (int)vec.x + (int)vec.y + (int)vec.z + (int)vec.u;
}

struct Vector5F64 {
  double x;
  double y;
  double z;
  double u;
  double v;
};
int sum_vector5_f64(struct Vector5F64 vec) {
  return (int)vec.x + (int)vec.y + (int)vec.z + (int)vec.u + (int)vec.v;
}

struct Vector3MixA {
  uint8_t x;
  float y;
  uint64_t z;
};
int sum_vector3_mix_a(struct Vector3MixA vec) {
  return (int)vec.x + (int)vec.y + (int)vec.z;
}

struct Vector3MixB {
  float x;
  float y;
  double z;
};
int sum_vector3_mix_b(struct Vector3MixB vec) {
  return (int)vec.x + (int)vec.y + (int)vec.z;
}

struct Vector3MixC {
  double x;
  double y;
  float z;
};
int sum_vector3_mix_c(struct Vector3MixC vec) {
  return (int)vec.x + (int)vec.y + (int)vec.z;
}
