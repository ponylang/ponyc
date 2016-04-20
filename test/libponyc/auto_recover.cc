#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "expr"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }

#define TEST_ERRORS_2(src, err1, err2) \
  { const char* errs[] = {err1, err2, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }


class AutoRecoverTest : public PassTest
{};


TEST_F(AutoRecoverTest, CanAssign_NewRefToIso)
{
  const char* src =
    "class Class\n"
    "  new ref create() => None\n"

    "primitive Prim\n"
    "  fun apply() =>\n"
    "    let c: Class iso = Class";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CanAssign_NewRefToVal)
{
  const char* src =
    "class Class\n"
    "  new ref create() => None\n"

    "primitive Prim\n"
    "  fun apply() =>\n"
    "    let c: Class val = Class";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CantAssign_NewValToIso)
{
  const char* src =
    "class Class\n"
    "  new val create() => None\n"

    "primitive Prim\n"
    "  fun apply() =>\n"
    "    let c: Class iso = Class";

  TEST_ERRORS_1(src, "right side not a subtype of left side");
}

TEST_F(AutoRecoverTest, CantAssign_NewTagToVal)
{
  const char* src =
    "class Class\n"
    "  new tag create() => None\n"

    "primitive Prim\n"
    "  fun apply() =>\n"
    "    let c: Class val = Class";

  TEST_ERRORS_1(src, "right side not a subtype of left side");
}

TEST_F(AutoRecoverTest, CantAssign_ApplesToOranges)
{
  const char* src =
    "class val Apple\n"
    "  new val create() => None\n"
    "class val Orange\n"
    "  new val create() => None\n"

    "primitive Prim\n"
    "  fun apply() =>\n"
    "    let o: Orange val = Apple";

  TEST_ERRORS_1(src, "right side not a subtype of left side");
}

TEST_F(AutoRecoverTest, CanAssign_ArrayLiteralToIso)
{
  const char* src =
    "primitive Prim\n"
    "  fun apply() =>\n"
    "    let c: Array[U8] iso = [as U8: 1, 2, 3]";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CanPass_NewRefToIso)
{
  const char* src =
    "class Class\n"
    "  new ref create() => None\n"

    "primitive Prim\n"
    "  fun pass(c: Class iso) => None\n"
    "  fun apply() => pass(Class)"
    "  fun named() => pass(where c = Class)";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CanPass_NewRefToVal)
{
  const char* src =
    "class Class\n"
    "  new ref create() => None\n"

    "primitive Prim\n"
    "  fun pass(c: Class val) => None\n"
    "  fun apply() => pass(Class)"
    "  fun named() => pass(where c = Class)";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CantPass_NewValToIso)
{
  const char* src =
    "class Class\n"
    "  new val create() => None\n"

    "primitive Prim\n"
    "  fun pass(c: Class iso) => None\n"
    "  fun apply() => pass(Class)"
    "  fun named() => pass(where c = Class)";

  TEST_ERRORS_2(src,
    "argument not a subtype of parameter",
    "argument not a subtype of parameter");
}

TEST_F(AutoRecoverTest, CantPass_NewTagToVal)
{
  const char* src =
    "class Class\n"
    "  new tag create() => None\n"

    "primitive Prim\n"
    "  fun pass(c: Class val) => None\n"
    "  fun apply() => pass(Class)"
    "  fun named() => pass(where c = Class)";

  TEST_ERRORS_2(src,
    "argument not a subtype of parameter",
    "argument not a subtype of parameter");
}

TEST_F(AutoRecoverTest, CantPass_ApplesToOranges)
{
  const char* src =
    "class val Apple\n"
    "  new val create() => None\n"
    "class val Orange\n"
    "  new val create() => None\n"

    "primitive Prim\n"
    "  fun pass(o: Orange val) => None\n"
    "  fun apply() => pass(Apple)"
    "  fun named() => pass(where o = Apple)";

  TEST_ERRORS_2(src,
    "argument not a subtype of parameter",
    "argument not a subtype of parameter");
}

TEST_F(AutoRecoverTest, CanPass_ArrayLiteralToIso)
{
  const char* src =
    "primitive Prim\n"
    "  fun pass(a: Array[U8] iso, a2: Array[U8] iso) => None\n"
    "  fun apply() => pass([as U8: 1, 2, 3] where a2 = [as U8: 1, 2, 3])";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CanFFIPass_NewRefToIso)
{
  const char* src =
    "use @ffi_pass[None](c: Class iso)\n"

    "class Class\n"
    "  new ref create() => None\n"

    "primitive Prim\n"
    "  fun apply() => @ffi_pass(Class)";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CanFFIPass_NewRefToVal)
{
  const char* src =
    "use @ffi_pass[None](c: Class val)\n"

    "class Class\n"
    "  new ref create() => None\n"

    "primitive Prim\n"
    "  fun apply() => @ffi_pass(Class)";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CantFFIPass_NewValToIso)
{
  const char* src =
    "use @ffi_pass[None](c: Class iso)\n"

    "class Class\n"
    "  new val create() => None\n"

    "primitive Prim\n"
    "  fun apply() => @ffi_pass(Class)";

  TEST_ERRORS_1(src, "argument not a subtype of parameter");
}

TEST_F(AutoRecoverTest, CantFFIPass_NewTagToVal)
{
  const char* src =
    "use @ffi_pass[None](c: Class val)\n"

    "class Class\n"
    "  new tag create() => None\n"

    "primitive Prim\n"
    "  fun apply() => @ffi_pass(Class)";

  TEST_ERRORS_1(src, "argument not a subtype of parameter");
}

TEST_F(AutoRecoverTest, CantFFIPass_ApplesToOranges)
{
  const char* src =
    "use @ffi_pass[None](o: Orange val)\n"

    "class val Apple\n"
    "  new val create() => None\n"
    "class val Orange\n"
    "  new val create() => None\n"

    "primitive Prim\n"
    "  fun apply() => @ffi_pass(Apple)";

  TEST_ERRORS_1(src, "argument not a subtype of parameter");
}

TEST_F(AutoRecoverTest, CanFFIPass_ApplesToUnspecified)
{
  const char* src =
    "class val Apple\n"
    "  new val create() => None\n"

    "primitive Prim\n"
    "  fun apply() => @ffi_pass[None](Apple)";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CanFFIPass_ArrayLiteralToIso)
{
  const char* src =
    "use @ffi_pass[None](c: Array[U8] iso)\n"

    "primitive Prim\n"
    "  fun apply() => @ffi_pass([as U8: 1, 2, 3])";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CantAccess_LetLocalRef)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply() =>\n"
    "    let inner: Inner ref = Inner\n"
    "    let w: Wrap iso = Wrap.create(inner)";

  TEST_ERRORS_1(src, "right side not a subtype of left side");
}

TEST_F(AutoRecoverTest, CanAccess_LetLocalVal)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply() =>\n"
    "    let inner: Inner val = Inner\n"
    "    let w: Wrap iso = Wrap(inner)";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CanAccess_LetLocalConsumedIso)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply() =>\n"
    "    let inner: Inner iso = Inner\n"
    "    let w: Wrap iso = Wrap(consume inner)";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CantAccess_VarLocalRef)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply() =>\n"
    "    var inner: Inner ref = Inner\n"
    "    let w: Wrap iso = Wrap(inner)";

  TEST_ERRORS_1(src, "right side not a subtype of left side");
}

TEST_F(AutoRecoverTest, CanAccess_VarLocalVal)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply() =>\n"
    "    var inner: Inner val = Inner\n"
    "    let w: Wrap iso = Wrap(inner)";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CanAccess_VarLocalConsumedIso)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply() =>\n"
    "    var inner: Inner iso = Inner\n"
    "    let w: Wrap iso = Wrap(consume inner)";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CantAccess_ParamRef)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply(inner: Inner ref) =>\n"
    "    let w: Wrap iso = Wrap(inner)";

  TEST_ERRORS_1(src, "right side not a subtype of left side");
}

TEST_F(AutoRecoverTest, CanAccess_ParamVal)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply(inner: Inner val) =>\n"
    "    let w: Wrap iso = Wrap(inner)";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CanAccess_ParamConsumedIso)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply(inner: Inner iso) =>\n"
    "    let w: Wrap iso = Wrap(consume inner)";

  TEST_COMPILE(src);
}

TEST_F(AutoRecoverTest, CantAccess_LetFieldVal)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  let inner: Inner val = Inner\n"
    "  fun apply() =>\n"
    "    let w: Wrap iso = Wrap(inner)";

  TEST_ERRORS_1(src, "right side not a subtype of left side");
}

TEST_F(AutoRecoverTest, CantAccess_VarFieldVal)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  var inner: Inner val = Inner\n"
    "  fun apply() =>\n"
    "    let w: Wrap iso = Wrap(inner)";

  TEST_ERRORS_1(src, "right side not a subtype of left side");
}

TEST_F(AutoRecoverTest, CantAccess_EmbedFieldVal)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  embed inner: Inner val = Inner\n"
    "  fun apply() =>\n"
    "    let w: Wrap iso = Wrap(inner)";

  TEST_ERRORS_1(src, "right side not a subtype of left side");
}

TEST_F(AutoRecoverTest, CantAccess_FunReturnVal)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  fun inner(): Inner val => Inner\n"
    "  fun apply() =>\n"
    "    let w: Wrap iso = Wrap(inner())";

  TEST_ERRORS_1(src, "right side not a subtype of left side");
}
