#include "export-numeric-types_export.h"

double call_add_f64(double x, double y)
{
  return NumericOps_add_f64(x, y);
}

uint32_t call_add_u32(uint32_t x, uint32_t y)
{
  return NumericOps_add_u32(x, y);
}

bool call_flag(bool x)
{
  return NumericOps_flag(x);
}

float call_add_f32(float x, float y)
{
  return NumericOps_add_f32(x, y);
}

intptr_t call_add_isize(intptr_t x, intptr_t y)
{
  return NumericOps_add_isize(x, y);
}
