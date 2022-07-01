#include <stdio.h>
#include <stdint.h>

struct Vector2U32 {
  uint32_t x;
  uint32_t y;
};
struct Vector2U32 build_vector2_u32(uint32_t x, uint32_t y) {
  return (struct Vector2U32) { .x = x, .y = y };
}

struct Vector3U32 {
  uint32_t x;
  uint32_t y;
  uint32_t z;
};
struct Vector3U32 build_vector3_u32(uint32_t x, uint32_t y, uint32_t z) {
  return (struct Vector3U32) { .x = x, .y = y, .z = z };
}

struct Vector2U64 {
  uint64_t x;
  uint64_t y;
};
struct Vector2U64 build_vector2_u64(uint64_t x, uint64_t y) {
  return (struct Vector2U64) { .x = x, .y = y };
}

struct Vector3U64 {
  uint64_t x;
  uint64_t y;
  uint64_t z;
};
struct Vector3U64 build_vector3_u64(uint64_t x, uint64_t y, uint64_t z) {
  return (struct Vector3U64) { .x = x, .y = y, .z = z };
}

struct Vector2F32 {
  float x;
  float y;
};
struct Vector2F32 build_vector2_f32(float x, float y) {
  return (struct Vector2F32) { .x = x, .y = y };
}

struct Vector3F32 {
  float x;
  float y;
  float z;
};
struct Vector3F32 build_vector3_f32(float x, float y, float z) {
  return (struct Vector3F32) { .x = x, .y = y, .z = z };
}

struct Vector4F32 {
  float x;
  float y;
  float z;
  float u;
};
struct Vector4F32 build_vector4_f32(float x, float y, float z, float u) {
  return (struct Vector4F32) { .x = x, .y = y, .z = z, .u = u };
}

struct Vector5F32 {
  float x;
  float y;
  float z;
  float u;
  float v;
};
struct Vector5F32 build_vector5_f32(float x, float y, float z, float u, float v) {
  return (struct Vector5F32) { .x = x, .y = y, .z = z, .u = u, .v = v };
}

struct Vector2F64 {
  double x;
  double y;
};
struct Vector2F64 build_vector2_f64(double x, double y) {
  return (struct Vector2F64) { .x = x, .y = y };
}

struct Vector3F64 {
  double x;
  double y;
  double z;
};
struct Vector3F64 build_vector3_f64(double x, double y, double z) {
  return (struct Vector3F64) { .x = x, .y = y, .z = z };
}

struct Vector4F64 {
  double x;
  double y;
  double z;
  double u;
};
struct Vector4F64 build_vector4_f64(double x, double y, double z, double u) {
  return (struct Vector4F64) { .x = x, .y = y, .z = z, .u = u };
}

struct Vector5F64 {
  double x;
  double y;
  double z;
  double u;
  double v;
};
struct Vector5F64 build_vector5_f64(double x, double y, double z, double u, double v) {
  return (struct Vector5F64) { .x = x, .y = y, .z = z, .u = u, .v = v };
}

struct Vector3MixA {
  uint8_t x;
  float y;
  uint64_t z;
};
struct Vector3MixA build_vector3_mix_a(uint8_t x, float y, uint64_t z) {
  return (struct Vector3MixA) { .x = x, .y = y, .z = z };
}

struct Vector3MixB {
  float x;
  float y;
  double z;
};
struct Vector3MixB build_vector3_mix_b(float x, float y, double z) {
  return (struct Vector3MixB) { .x = x, .y = y, .z = z };
}

struct Vector3MixC {
  double x;
  double y;
  float z;
};
struct Vector3MixC build_vector3_mix_c(double x, double y, float z) {
  return (struct Vector3MixC) { .x = x, .y = y, .z = z };
}
