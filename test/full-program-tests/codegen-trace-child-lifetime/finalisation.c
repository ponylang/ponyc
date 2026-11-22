#include <stdint.h>

// Main owns each object and reads these counters on the same actor.
static uint32_t counts[3];

void record_final(uint32_t id)
{
  counts[id]++;
}

uint32_t final_count(uint32_t id)
{
  return counts[id];
}
