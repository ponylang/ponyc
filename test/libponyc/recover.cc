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

  TEST_ERRORS_1(src, "argument not assignable to parameter");
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

  TEST_ERRORS_1(src, "argument not assignable to parameter");
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

  TEST_ERRORS_1(src, "argument not assignable to parameter");
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

  TEST_ERRORS_1(src, "can't access non-sendable field of non-sendable object");
}

// Issue #4244
TEST_F(RecoverTest, CantRecover_BoxFieldToVal_V1)
{
  const char* src =
    "class Foo\n"
    "class Bar\n"
    "  let f: Foo box\n"
    "  new create(f': Foo box) => f = f'\n"
    "  fun get_f(): Foo val => recover val f end\n";

  TEST_ERRORS_1(src, "can't access non-sendable field of non-sendable object inside of a recover expression");
}

// Issue #4244
TEST_F(RecoverTest, CantRecover_BoxFieldToVal_V2)
{
  const char* src =
    "class Foo\n"
    "class Bar\n"
    "  let f: Foo box\n"
    "  new create(f': Foo box) => f = f'\n"
    "  fun get_f(): Foo val =>\n"
    "    let b: this->(Bar box) = this\n"
    "    recover val b.f end\n";

  TEST_ERRORS_1(src, "can't access non-sendable field of non-sendable object inside of a recover expression");
}

// Issue #4244
TEST_F(RecoverTest, CantRecover_BoxFieldToVal_V3)
{
  const char* src =
    "class Foo\n"
    "class Bar\n"
    "  let f: Foo box\n"
    "  new create(f': Foo box) => f = f'\n"
    "  fun get_f(): Foo val =>\n"
    "    let b: Bar box = this\n"
    "    recover val b.f end\n";

  TEST_ERRORS_1(src, "can't access non-sendable field of non-sendable object inside of a recover expression");
}

TEST_F(RecoverTest, CantAccess_AssignedField)
{
  const char* src =
    "class Inner\n"
    "class Wrap\n"
    "  new create(s: Inner box) => None\n"

    "class Class\n"
    "  var inner: Inner iso = recover Inner end\n"
    "  fun ref apply(): Wrap iso =>\n"
    "    recover Wrap(inner = recover Inner end) end";

  TEST_ERRORS_2(src, "can't assign to field of non-sendable object",
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

TEST_F(RecoverTest, CantAccess_NonSendableLocalAssigned)
{
  const char* src =
    "class Foo\n"
    "  fun apply() =>\n"
    "    var nonlocal: String ref = String\n"
    "    recover\n"
    "      nonlocal = String\n"
    "    end";

  TEST_ERRORS_2(src, "can't access a non-sendable local defined outside",
    "left side must be something that can be assigned to");
}

TEST_F(RecoverTest, CanAccess_LocalAssigned)
{
  const char* src =
    "class Foo\n"
    "  fun apply() =>\n"
    "    recover\n"
    "      var local: String ref = String\n"
    "      local = String\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CantWrite_NonLocalField)
{
  const char* src =
    "class StringRef\n"
    "  var str: String = String\n"
    "class Foo\n"
    "  fun apply() =>\n"
    "    var nonlocal: StringRef ref = StringRef\n"
    "    recover\n"
    "      nonlocal.str = String\n"
    "    end";

  TEST_ERRORS_2(src, "can't access a non-sendable local defined outside",
    "left side must be something that can be assigned to");
}

TEST_F(RecoverTest, CanAccess_LocalField)
{
  const char* src =
    "class StringRef\n"
    "  var str: String = String\n"
    "class Foo\n"
    "  fun apply() =>\n"
    "    recover\n"
    "      var local: StringRef ref = StringRef\n"
    "      local.str = String\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CanAccess_DeepLocalFields)
{
  const char* src =
    "class StringRef\n"
    "  var str: String ref = String\n"
    "class StringRefRef\n"
    "  var strref: StringRef = StringRef\n"
    "class Foo\n"
    "  fun apply() =>\n"
    "    recover\n"
    "      var local: StringRefRef ref = StringRefRef\n"
    "      local.strref.str = String\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CantWrite_This_NonSendable)
{
  const char* src =
    "class StringRef\n"
    "  var str: String ref = String\n"
    "  fun apply() =>\n"
    "    recover\n"
    "      let str': String ref = String\n"
    "      this.str = str'\n"
    "    end";

  TEST_ERRORS_2(src, "can't access non-sendable field of non-sendable "
            "object inside of a recover expression",
    "left side must be something that can be assigned to");
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

TEST_F(RecoverTest, CantRecover_TupleMutableAlias)
{
  const char* src =
    "class Foo\n"
    "  fun apply() =>\n"
    "    let x: (Foo, Foo) = recover\n"
    "      let y: Foo = Foo\n"
    "      (y, y)\n"
    "    end";

  TEST_ERRORS_1(src, "can't recover to this capability");
}

TEST_F(RecoverTest, CanRecover_TupleMutableSingleLift)
{
  const char* src =
    "class Foo\n"
    "  fun apply() =>\n"
    "    let x: (Foo iso, Foo iso) = recover\n"
    "      let y: Foo = Foo\n"
    "      (y, Foo)\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CantRecover_TupleMutableMultipleLifts)
{
  const char* src =
    "class Foo\n"
    "  fun apply() =>\n"
    "    let x: (Foo iso, Foo iso) = recover\n"
    "      let y: Foo = Foo\n"
    "      (y, y)\n"
    "    end";

  TEST_ERRORS_1(src, "can't recover to this capability");
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

TEST_F(RecoverTest, CantRecover_TupleMutableToMutableAndImmutable)
{
  const char* src =
    "class Foo\n"
    "  fun apply() =>\n"
    "    let x: (Foo iso, Foo val) = recover\n"
    "      let y: Foo = Foo\n"
    "      let z: Foo box = Foo\n"
    "      (y, z)\n"
    "    end";

  TEST_ERRORS_1(src, "can't recover to this capability");
}

TEST_F(RecoverTest, CantRecover_TupleInUnionAlias)
{
  const char* src =
    "class Foo\n"
    "  fun apply() =>\n"
    "    let x: (Foo iso | (Foo, Foo)) = recover\n"
    "      let y: Foo = Foo\n"
    "      let z: (Foo | (Foo, Foo)) = (y, y)\n"
    "      z\n"
    "    end";

  TEST_ERRORS_1(src, "can't recover to this capability");
}

TEST_F(RecoverTest, CanRecover_TupleInUnionSingleLift)
{
  const char* src =
    "class Foo\n"
    "  fun apply() =>\n"
    "    let x: (Foo iso | (Foo iso, Foo iso)) = recover\n"
    "      let y: Foo = Foo\n"
    "      let z: (Foo | (Foo, Foo iso)) = (y, Foo)\n"
    "      consume z\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CantRecover_TupleInUnionMultipleLifts)
{
  const char* src =
    "class Foo\n"
    "  fun apply() =>\n"
    "    let x: (Foo iso | (Foo iso, Foo iso)) = recover\n"
    "      let y: Foo = Foo\n"
    "      let z: (Foo | (Foo, Foo)) = (y, y)\n"
    "      z\n"
    "    end";

  TEST_ERRORS_1(src, "can't recover to this capability");
}

TEST_F(RecoverTest, CantReturnTrn_TrnAutoRecovery)
{
  const char* src =
    "class A\n"
    "class Extract\n"
    "  var a: A trn = A\n"
    "  fun ref extract_trn(): A trn^ =>\n"
    "    a = A\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let bad = recover trn Extract end\n"
    "    let a_trn: A trn = bad.extract_trn()\n";

  TEST_ERRORS_1(src,
    "receiver type is not a subtype of target type")
}

TEST_F(RecoverTest, CanReturnBox_TrnAutoRecovery)
{
  const char* src =
    "class A\n"
    "class Extract\n"
    "  var a: A trn = A\n"
    "  fun ref extract_box(): A box =>\n"
    "    a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let good = recover trn Extract end\n"
    "    let a_box: A box = good.extract_box()\n";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CantWriteBox_TrnAutoRecovery)
{
  const char* src =
    "class A\n"
    "class Store\n"
    "  var _a: A box = A\n"
    "  fun ref store(a: A box) =>\n"
    "    _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let dest: Store trn = recover trn Store end\n"
    "    let a_box: A box = A\n"
    "    dest.store(a_box)\n";

  TEST_ERRORS_1(src,
    "receiver type is not a subtype of target type")
}

TEST_F(RecoverTest, CantWriteTrnAlias_TrnAutoRecovery)
{
  const char* src =
    "class A\n"
    "class Store\n"
    "  var _a: A box = A\n"
    "  fun ref store(a: A box) =>\n"
    "    _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let dest: Store trn = recover trn Store end\n"
    "    let a_trn: A trn = A\n"
    "    dest.store(a_trn)\n";

  TEST_ERRORS_1(src,
    "receiver type is not a subtype of target type")
}

TEST_F(RecoverTest, CanWriteTrn_TrnAutoRecovery)
{
  const char* src =
    "class A\n"
    "class Store\n"
    "  var _a: A box = A\n"
    "  fun ref store(a: A box) =>\n"
    "    _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let dest: Store trn = recover trn Store end\n"
    "    let a_trn: A trn = A\n"
    "    dest.store(consume a_trn)\n";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, LetIso_CtorAutoRecovery)
{
  const char* src =
    "class A\n"
    "  new ref create() =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a_iso: A iso = A\n";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, VarIso_CtorAutoRecovery)
{
  const char* src =
    "class A\n"
    "  new ref create() =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var a_iso: A iso = A\n";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, LetTrn_CtorAutoRecovery)
{
  const char* src =
    "class A\n"
    "  new ref create() =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var a_trn: A trn = A\n";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, LetVal_RefCtorAutoRecovery)
{
  const char* src =
    "class A\n"
    "  new ref create() =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var a_val: A val = A\n";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, LetVal_BoxCtorAutoRecovery)
{
  const char* src =
    "class A\n"
    "  new box create() =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var a_val: A val = A\n";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, LetIso_ArgsCtorAutoRecovery)
{
  const char* src =
    "class A\n"
    "  new ref create(s: String box) =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let s: String val = \"\"\n"
    "    let a_iso: A iso = A(s)\n";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, LetTrn_ArgsCtorAutoRecovery)
{
  const char* src =
    "class A\n"
    "  new ref create(s: String box) =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let s: String val = \"\"\n"
    "    let a_trn: A trn = A(s)\n";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, LetVal_ArgsCtorAutoRecovery)
{
  const char* src =
    "class A\n"
    "  new ref create(s: String box) =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let s: String val = \"\"\n"
    "    let a_val: A val = A(s)\n";

  TEST_COMPILE(src);
}

TEST_F(RecoverTest, CantAutoRecover_CtorArgWithNonSendableArg)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let s: String ref = String\n"
    "    Bar.take_foo(Foo.from_str(s))\n"

    "class Foo\n"
    "  new from_u8(v: U8) =>\n"
    "    None\n"
    "  new from_str(s: String ref) =>\n"
    "    None\n"

    "primitive Bar\n"
    "  fun take_foo(foo: Foo iso) =>\n"
    "    None\n";

  TEST_ERRORS_1(src, "argument not assignable to parameter");
}

TEST_F(RecoverTest, CantAutoRecover_CtorAssignmentWithNonSendableArg)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let s: String ref = String\n"
    "    let bar: Foo iso = Foo.from_str(s)\n"

    "class Foo\n"
    "  new from_u8(v: U8) =>\n"
    "    None\n"
    "  new from_str(s: String ref) =>\n"
    "    None\n"

    "primitive Bar\n"
    "  fun take_foo(foo: Foo iso) =>\n"
    "    None\n";

  TEST_ERRORS_1(src, "right side must be a subtype of left side");
}

TEST_F(RecoverTest, CantAutoRecover_CtorParamToComplexTypeWithNonSendableArg)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let bar: (Foo iso | String ref) = Foo.from_u8(123)\n"

    "class Foo\n"
    "  new from_u8(v: U8) =>\n"
    "    None\n"
    "  new from_str(s: String ref) =>\n"
    "    None\n"

    "primitive Bar\n"
    "  fun take_foo(foo: Foo iso) =>\n"
    "    None\n";

  TEST_ERRORS_1(src, "right side must be a subtype of left side");
}
