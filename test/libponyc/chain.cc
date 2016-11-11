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


class ChainTest : public PassTest
{};


TEST_F(ChainTest, Chain_AliasRefcap)
{
  const char* src =
    "class C\n"
    "  fun tag x() => None\n"

    "  fun apply() =>\n"
    "    let r: C ref = C\n"
    "    let r2: C ref = r.>x()\n"
    "    let v: C val = C\n"
    "    let v2: C val = v.>x()\n"
    "    let b: C box = C\n"
    "    let b2: C box = b.>x()\n"
    "    let t: C tag = C\n"
    "    let t2: C tag = t.>x()";

  TEST_COMPILE(src);
}

TEST_F(ChainTest, Chain_UniqueRefcap)
{
  const char* src =
    "class C\n"
    "  fun ref x() => None\n"

    "  fun apply() =>\n"
    "    let i: C iso = C\n"
    "    let i2: C iso = i.>x()\n"
    "    let t: C trn = C\n"
    "    let t2: C trn = t.>x()";

  TEST_ERRORS_2(src,
    "right side must be a subtype of left side",
    "right side must be a subtype of left side");
}

TEST_F(ChainTest, Chain_UniqueEphRecoverCall)
{
  const char* src =
    "class C\n"
    "  new iso create() => None\n"
    "  new trn trn_create() => None\n"
    "  fun ref x() => None\n"

    "  fun apply() =>\n"
    "    let i: C iso = C.>x()\n"
    "    let t: C trn = C.trn_create().>x()";

  TEST_COMPILE(src);
}

TEST_F(ChainTest, Chain_UniqueEphSubtypeMethodCap)
{
  const char* src =
    "class C\n"
    "  new iso create() => None\n"
    "  new trn trn_create() => None\n"
    "  fun tag x(c: C ref) => None\n"
    "  fun box y(c: C ref) => None\n"

    "  fun apply() =>\n"
    "    let r: C ref = C\n"
    "    let i: C iso = C.>x(r)\n"
    "    let t: C trn = C.trn_create().>y(r)";

  TEST_COMPILE(src);
}

TEST_F(ChainTest, Chain_UniqueEphSameMethodCap)
{
  const char* src =
    "class C\n"
    "  new iso create() => None\n"
    "  new trn trn_create() => None\n"
    "  fun iso x() => None\n"
    "  fun trn y() => None\n"

    "  fun apply() =>\n"
    "    let i: C iso = C.>x()\n"
    "    let t: C trn = C.trn_create().>y()";

  TEST_ERRORS_2(src,
    "right side must be a subtype of left side",
    "right side must be a subtype of left side");
}

TEST_F(ChainTest, Chain_UniqueEphNoRecoverCall)
{
  const char* src =
    "class C\n"
    "  new iso create() => None\n"
    "  new trn trn_create() => None\n"
    "  fun ref x(c: C ref) => None\n"

    "  fun apply() =>\n"
    "    let r: C ref = C\n"
    "    let i: C iso = C.>x(r)\n"
    "    let t: C trn = C.trn_create().>x(r)";

  TEST_ERRORS_2(src,
    "right side must be a subtype of left side",
    "right side must be a subtype of left side");
}
