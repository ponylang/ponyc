#include <gtest/gtest.h>
#include "util.h"

void Main_runtime_override_defaults_oo(void* opt)
{
  (void)opt;
  return;
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
