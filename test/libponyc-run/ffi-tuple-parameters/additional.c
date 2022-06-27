// additional.c
#include <stdio.h>
#include <stdint.h>

struct Vector {
  uint64_t x;
  uint64_t y;
  //uint64_t z;
};

struct Vector build_vector(uint64_t init) {
  return (struct Vector) {.x=init, .y=init}; //, .z=init};
}
