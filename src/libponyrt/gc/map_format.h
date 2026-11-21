#ifndef GC_MAP_FORMAT_H
#define GC_MAP_FORMAT_H

#include <stdint.h>

#define PONY_GC_MAP_ROOTS 16
#define PONY_GC_RETURN_SLOT_TAG 1

enum
{
  GC_MAP_REGISTER = 1,
  GC_MAP_DIRECT = 2,
  GC_MAP_INDIRECT = 3,
  GC_MAP_CONSTANT = 4,
};

typedef struct gc_map_location_t
{
  uint8_t kind;
  uint8_t size;
  uint16_t reg;
  int32_t offset;
} gc_map_location_t;

typedef struct gc_map_site_t
{
  int64_t function;
  int64_t trace;
  uint32_t return_offset;
  uint32_t location_count;
} gc_map_site_t;

#endif
