#include <gtest/gtest.h>
#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))

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
    "    x.mutateme()\n";

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

TEST_F(CapSafetyTest, NoWriteArrow)
{
  const char* src =
    "actor Main"
    "  var f: U64 = 1"
    "  new create(env: Env) =>"
    "    _start()"
    "  fun _start() =>"
    "    f = 2";

  TEST_ERROR(src);
}

// When a 'this->' viewpoint parameter can't accept the argument because the
// receiver's capability reduces it to a generic cap (here #read), the error is
// accompanied by two explanatory frames: one describing the 'this->' viewpoint
// dependency, and one describing what the generic cap means.
TEST_F(CapSafetyTest, ThisArrowGenericCapHints)
{
  const char* src =
    "class Item\n"
    "class Wrap[A]\n"
    "  fun get(alt: this->A): this->A => alt\n"
    "actor Main\n"
    "  let w: Wrap[Item] = Wrap[Item]\n"
    "  new create(env: Env) => None\n"
    "  fun use_it(i: Item): None =>\n"
    "    w.get(i)\n"
    "    None\n";

  const char* errs[] = { "argument not assignable to parameter", NULL };
  const char* frame0[] = {
    "argument type is",
    "parameter type requires",
    "this parameter's type is adapted through the receiver's",
    "is not a subcap of #read",
    "#read is a generic capability standing for",
    NULL
  };
  const char** frames[] = { frame0, NULL };

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

// The 'this->' viewpoint hint must NOT appear when the failure is a nominal
// (entity) mismatch rather than a capability one: here the argument is the
// wrong type entirely (Other vs Item) and both caps are ref. The exact frame
// count (3) catches the hint firing spuriously.
TEST_F(CapSafetyTest, ThisArrowHintAbsentForNominalMismatch)
{
  const char* src =
    "class Item\n"
    "class Other\n"
    "class Wrap[A]\n"
    "  fun get(alt: this->A): None => None\n"
    "actor Main\n"
    "  let w: Wrap[Item] = Wrap[Item]\n"
    "  new create(env: Env) => None\n"
    "  fun use_it(o: Other): None =>\n"
    "    w.get(o)\n";

  const char* errs[] = { "argument not assignable to parameter", NULL };
  const char* frame0[] = {
    "argument type is",
    "parameter type requires",
    "Other ref is not a subtype of Item",
    NULL
  };
  const char** frames[] = { frame0, NULL };

  DO(test_expected_error_frames(src, "expr", errs, frames));
}
