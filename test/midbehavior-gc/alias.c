#include <stdint.h>

static void* saved;
static uint32_t finals;

void count_final(void)
{
  finals++;
}

uint32_t final_count(void)
{
  return finals;
}

void retain_alias(void* p)
{
  saved = p;
}

void* take_alias(void)
{
  void* p = saved;
  saved = 0;
  return p;
}
