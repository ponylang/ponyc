// additional.c
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
