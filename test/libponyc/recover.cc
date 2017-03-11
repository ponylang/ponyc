#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "expr"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }

#define TEST_ERRORS_2(src, err1, err2) \
  { const char* errs[] = {err1, err2, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }


class RecoverTest : public PassTest
{};


TEST_F(RecoverTest, CanRecover_NewRefToIso)
{
  const char* src =
    "class Class\n"
    "  new ref create() => None\n"

    "primitive Prim\n"
    "  fun apply(): Class iso =>\n"
    "    recover iso Class end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CanRecover_NewRefToVal)
{
  const char* src =
    "class Class\n"
    "  new ref create() => None\n"

    "primitive Prim\n"
    "  fun apply(): Class val =>\n"
    "    recover val Class end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CantRecover_NewValToIso)
{
  const char* src =
    "class Class\n"
    "  new val create() => None\n"

    "primitive Prim\n"
    "  fun apply(): Class iso =>\n"
    "    recover iso Class end";

  TEST_ERRORS_1(src, "can't recover to this capability");
}

TEST_F(RecoverTest, CantRecover_NewTagToVal)
{
  const char* src =
    "class Class\n"
    "  new tag create() => None\n"

    "primitive Prim\n"
    "  fun apply(): Class val =>\n"
    "    recover val Class end";

  TEST_ERRORS_1(src, "can't recover to this capability");
}

TEST_F(RecoverTest, CanSee_LetLocalRefAsTag)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply(): Wrap iso =>\n"
    "    let inner: Inner ref = Inner\n"
    "    recover Wrap(inner) end";

  TEST_ERRORS_1(src, "argument not a subtype of parameter");
}

TEST_F(RecoverTest, CanAccess_LetLocalVal)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply(): Wrap iso =>\n"
    "    let inner: Inner val = Inner\n"
    "    recover Wrap(inner) end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CanAccess_LetLocalConsumedIso)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply(): Wrap iso =>\n"
    "    let inner: Inner iso = Inner\n"
    "    recover Wrap(consume inner) end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CanSee_VarLocalRefAsTag)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply(): Wrap iso =>\n"
    "    var inner: Inner ref = Inner\n"
    "    recover Wrap(inner) end";

  TEST_ERRORS_1(src, "argument not a subtype of parameter");
}

TEST_F(RecoverTest, CanAccess_VarLocalVal)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply(): Wrap iso =>\n"
    "    var inner: Inner val = Inner\n"
    "    recover Wrap(inner) end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CanAccess_VarLocalConsumedIso)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply(): Wrap iso =>\n"
    "    var inner: Inner iso = Inner\n"
    "    recover Wrap(consume inner) end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CanSee_ParamRefAsTag)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply(inner: Inner ref): Wrap iso =>\n"
    "    recover Wrap(inner) end";

  TEST_ERRORS_1(src, "argument not a subtype of parameter");
}

TEST_F(RecoverTest, CanAccess_ParamVal)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply(inner: Inner val): Wrap iso =>\n"
    "    recover Wrap(inner) end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CanAccess_ParamConsumedIso)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "primitive Prim\n"
    "  fun apply(inner: Inner iso): Wrap iso =>\n"
    "    recover Wrap(consume inner) end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CanAccess_LetFieldVal)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  let inner: Inner val = Inner\n"
    "  fun apply(): Wrap iso =>\n"
    "    recover Wrap(inner) end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CanAccess_VarFieldVal)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  var inner: Inner val = Inner\n"
    "  fun apply(): Wrap iso =>\n"
    "    recover Wrap(inner) end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CanAccess_EmbedFieldVal)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  embed inner: Inner val = Inner\n"
    "  fun apply(): Wrap iso =>\n"
    "    recover Wrap(inner) end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CanAccess_FunReturnVal)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  fun inner(): Inner val => Inner\n"
    "  fun apply(): Wrap iso =>\n"
    "    recover Wrap(inner()) end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CanAccess_FieldExplicitThis)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  let inner: Inner val = Inner\n"
    "  fun apply(): Wrap iso =>\n"
    "    recover Wrap(this.inner) end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CantAccess_NonSendableField)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  let inner: Inner = Inner\n"
    "  fun apply(): Wrap iso =>\n"
    "    recover Wrap(inner) end";

  TEST_ERRORS_1(src, "can't access field of non-sendable object");
}

TEST_F(RecoverTest, CantAccess_AssignedField)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  var inner: Inner iso = recover Inner end\n"
    "  fun apply(): Wrap iso =>\n"
    "    recover Wrap(inner = recover Inner end) end";

  TEST_ERRORS_2(src, "can't access field of non-sendable object",
    "left side must be something that can be assigned to");
}

TEST_F(RecoverTest, CantAccess_ThisOriginallyTagField)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  let inner: Inner val = Inner\n"
    "  fun tag apply(): Wrap iso =>\n"
    "    recover Wrap(inner) end";

  TEST_ERRORS_1(src, "can't read a field through Class tag");
}

TEST_F(RecoverTest, CantAccess_LocalOriginallyTagField)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  let inner: Inner val = Inner\n"
    "  fun apply(): Wrap iso =>\n"
    "    let c: Class tag = Class\n"
    "    recover Wrap(c.inner) end";

  TEST_ERRORS_1(src, "can't read a field through Class tag");
}

TEST_F(RecoverTest, CantAccess_ParamOriginallyTagField)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  let inner: Inner val = Inner\n"
    "  fun apply(c: Class tag): Wrap iso =>\n"
    "    recover Wrap(c.inner) end";

  TEST_ERRORS_1(src, "can't read a field through Class tag");
}

TEST_F(RecoverTest, CanAccess_LocalSendableField)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  let inner: Inner val = Inner\n"
    "  fun apply(): Wrap iso =>\n"
    "    let c: Class ref = Class\n"
    "    recover Wrap(c.inner) end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CanAccess_ParamSendableField)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  let inner: Inner val = Inner\n"
    "  fun apply(c: Class ref): Wrap iso =>\n"
    "    recover Wrap(c.inner) end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CanAccess_MutableMethod)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  fun ref inner(): Inner val => Inner\n"
    "  fun ref apply(): Wrap iso =>\n"
    "    recover Wrap(inner()) end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CantAccess_MethodArgNonSendable)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  new create() => None\n"
    "  fun inner(c: Class): Inner val => Inner\n"
    "  fun apply(): Wrap iso =>\n"
    "    recover Wrap(inner(Class)) end";

  TEST_ERRORS_1(src, "can't call method on non-sendable object");
}

TEST_F(RecoverTest, CantAccess_MethodReturnNonSendable)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  fun inner(): Inner => Inner\n"
    "  fun apply(): Wrap iso =>\n"
    "    recover Wrap(inner()) end";

  TEST_ERRORS_1(src, "can't call method on non-sendable object");
}

TEST_F(RecoverTest, CanDoPartialApplication_TagWithLowerToTag)
{
  const char* src =
    "class Class\n"
    "  fun tag func() => None"
    "  fun apply() =>\n"
    "    recover this~func() end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CantDoPartialApplication_RefWithLowerToTag)
{
  const char* src =
    "class Class\n"
    "  fun ref func() => None"
    "  fun apply() =>\n"
    "    recover this~func() end";

  TEST_ERRORS_1(src, "receiver type is not a subtype of target type");
}

TEST_F(RecoverTest, CanRecover_TupleMutableAlias)
{
  const char* src =
    "class Foo\n"
    "  fun apply() =>\n"
    "    let x: (Foo, Foo) = recover\n"
    "      let y: Foo = Foo\n"
    "      (y, y)\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CantRecover_TupleMutableLift)
{
  const char* src =
    "class Foo\n"
    "  fun apply() =>\n"
    "    let x: (Foo iso, Foo iso) = recover\n"
    "      let y: Foo = Foo\n"
    "      (y, y)\n"
    "    end";

  TEST_ERRORS_1(src, "right side must be a subtype of left side");
}

TEST_F(RecoverTest, CanRecover_TupleMutableToImmutable)
{
  const char* src =
    "class Foo\n"
    "  fun apply() =>\n"
    "    let x: (Foo val, Foo val) = recover val\n"
    "      let y: Foo = Foo\n"
    "      (y, y)\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CanRecover_TupleInUnionNoInnerLift)
{
  const char* src =
    "class Foo\n"
    "  fun apply() =>\n"
    "    let x: (Foo iso | (Foo, Foo)) = recover\n"
    "      let y: Foo = Foo\n"
    "      let z: (Foo | (Foo, Foo)) = (y, y)\n"
    "      z\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CantRecover_TupleInUnionInnerLift)
{
  const char* src =
    "class Foo\n"
    "  fun apply() =>\n"
    "    let x: (Foo iso | (Foo iso, Foo iso)) = recover\n"
    "      let y: Foo = Foo\n"
    "      let z: (Foo | (Foo, Foo)) = (y, y)\n"
    "      z\n"
    "    end";

  TEST_ERRORS_1(src, "right side must be a subtype of left side");
}

TEST_F(RecoverTest, CantAccess_NonSendableLocalAssigned)
{
  const char* src =
    "class Foo\n"
    "  fun apply() =>\n"
    "    var a: String ref = String\n"
    "    recover\n"
    "      let b: String ref = String\n"
    "      a = b\n"
    "    end";

  TEST_ERRORS_2(src, "can't access a non-sendable local defined outside",
    "left side must be something that can be assigned to");
}
