#include <gtest/gtest.h>
#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }

//
// A list of test cases for fundamental cap safety requirements.
// Each test here should be as simple as possible.
//
class CapSafetyTest: public PassTest
{};

TEST_F(CapSafetyTest, NoWriteToVal)
{
  const char* src =
    "class Y\n"
    "class X\n"
    "  var y: Y = Y\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: X val = recover X end\n"
    "    x.y = Y\n";
  TEST_ERROR(src);
}
TEST_F(CapSafetyTest, NoWriteToBox)
{
  const char* src =
    "class Y\n"
    "class X\n"
    "  var y: Y = Y\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: X box = X\n"
    "    x.y = Y\n";
  TEST_ERROR(src);
}
TEST_F(CapSafetyTest, WriteToRef)
{
  const char* src =
    "class Y\n"
    "class X\n"
    "  var y: Y = Y\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: X ref = X\n"
    "    x.y = Y\n";
  TEST_COMPILE(src);
}
TEST_F(CapSafetyTest, NoCallRefOnVal)
{
  const char* src =
    "class Y\n"
    "class X\n"
    "  fun ref mutateme() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: X val = recover X end\n"
    "    x.mutate()\n";
  TEST_ERROR(src);
}
TEST_F(CapSafetyTest, NoCallRefOnBox)
{
  const char* src =
    "class Y\n"
    "class X\n"
    "  fun ref mutateme() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: X box = X\n"
    "    x.mutateme()\n";
  TEST_ERROR(src);
}
TEST_F(CapSafetyTest, CallRefOnRef)
{
  const char* src =
    "class Y\n"
    "class X\n"
    "  fun ref mutateme() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: X ref = X\n"
    "    x.mutateme()\n";
  TEST_COMPILE(src);
}
TEST_F(CapSafetyTest, CallValOnVal)
{
  const char* src =
    "class Y\n"
    "class X\n"
    "  fun val shareme() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: X val = recover X end\n"
    "    x.shareme()\n";
  TEST_COMPILE(src);
}
TEST_F(CapSafetyTest, NoCallValOnBox)
{
  const char* src =
    "class Y\n"
    "class X\n"
    "  fun val shareme() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: X box = X\n"
    "    x.shareme()\n";
  TEST_ERROR(src);
}
TEST_F(CapSafetyTest, NoCallValOnRef)
{
  const char* src =
    "class Y\n"
    "class X\n"
    "  fun val shareme() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: X ref = X\n"
    "    x.shareme()\n";
  TEST_ERROR(src);
}
TEST_F(CapSafetyTest, NoWriteRefToIso)
{
  const char* src =
    "class Y\n"
    "class X\n"
    "  var y: Y = Y\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: X iso = recover X end\n"
    "    let y: Y ref = Y\n"
    "    x.y = y\n";
  TEST_ERROR(src);
}
TEST_F(CapSafetyTest, NoWriteBoxToIso)
{
  const char* src =
    "class Y\n"
    "class X\n"
    "  var y: Y box = Y\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: X iso = recover X end\n"
    "    let y: Y box = Y\n"
    "    x.y = y\n";
  TEST_ERROR(src);
}
TEST_F(CapSafetyTest, WriteValToIso)
{
  const char* src =
    "class Y\n"
    "class X\n"
    "  var y: Y val = Y\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: X iso = recover X end\n"
    "    let y: Y val = Y\n"
    "    x.y = y\n";
  TEST_COMPILE(src);
}
