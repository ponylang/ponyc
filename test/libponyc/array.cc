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


class ArrayTest : public PassTest
{};


TEST_F(ArrayTest, AsIsoAliasedElements)
{
  const char* src =
    "trait iso T\n"
    "class iso C1 is T\n"
    "class iso C2 is T\n"
    "class iso C3 is T\n"

    "primitive Foo\n"
    "  fun apply(): Array[T] =>\n"
    "    let c1: C1 = C1\n"
    "    let c2: C2 = C2\n"
    "    let c3: C3 = C3\n"
    "    [as T: c1; c2; c3]";

  TEST_ERRORS_1(src, "array element not a subtype of specified array type");
}

TEST_F(ArrayTest, NoAsIsoAliasedElements)
{
  const char* src =
    "trait iso T\n"
    "class iso C1 is T\n"
    "class iso C2 is T\n"
    "class iso C3 is T\n"

    "primitive Foo\n"
    "  fun apply(): Array[(C1 | C2 | C3)] =>\n"
    "    let c1: C1 = C1\n"
    "    let c2: C2 = C2\n"
    "    let c3: C3 = C3\n"
    "    [c1; c2; c3]";

  TEST_ERRORS_1(src,
    "array element not a subtype of specified array type");
}

TEST_F(ArrayTest, InferFromApplyInterfaceIsoElementNoArrowThis)
{
  const char* src =
    "trait iso T\n"
    "class iso C1 is T\n"
    "class iso C2 is T\n"
    "class iso C3 is T\n"

    "interface ArrayishOfT\n"
    "  fun apply(index: USize): T ?\n"

    "primitive Foo\n"
    "  fun apply(): ArrayishOfT =>\n"
    "    [C1; C2; C3]";

  TEST_ERRORS_1(src, "function body isn't the result type");
}

TEST_F(ArrayTest, InferFromArrayValWithAs)
{
  const char* src =
    "trait val T\n"
    "primitive P1 is T\n"
    "primitive P2 is T\n"
    "primitive P3 is T\n"

    "primitive Foo\n"
    "  fun apply(): Array[T] val =>\n"
    "    [as T: P1; P2; P3]";

  TEST_COMPILE(src);
}


TEST_F(ArrayTest, InferFromArrayIsoMustBeRecovered)
{
  const char* src =
    "trait T\n"
    "class C1 is T\n"
    "class C2 is T\n"
    "class C3 is T\n"

    "primitive Foo\n"
    "  fun apply(): Array[T] iso^ =>\n"
    "    let c1: C1 = C1\n"
    "    let c2: C2 = C2\n"
    "    let c3: C3 = C3\n"
    "    [c1; c2; c3]";

  TEST_ERRORS_1(src, "array element not a subtype of specified array type");
}


TEST_F(ArrayTest, InferFromArrayValMustBeRecovered)
{
  const char* src =
    "trait T\n"
    "class C1 is T\n"
    "class C2 is T\n"
    "class C3 is T\n"

    "primitive Foo\n"
    "  fun apply(): Array[T] val =>\n"
    "    let c1: C1 = C1\n"
    "    let c2: C2 = C2\n"
    "    let c3: C3 = C3\n"
    "    [c1; c2; c3]";

  TEST_ERRORS_1(src, "array element not a subtype of specified array type");
}


TEST_F(ArrayTest, InferFromArrayIsoMayBeExplicitlyRecovered)
{
  const char* src =
    "trait T\n"
    "class C1 is T\n"
    "class C2 is T\n"
    "class C3 is T\n"

    "primitive Foo\n"
    "  fun apply(): Array[T] iso^ =>\n"
    "    recover\n"
    "      let c1: C1 = C1\n"
    "      let c2: C2 = C2\n"
    "      let c3: C3 = C3\n"
    "      [c1; c2; c3]\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(ArrayTest, InferFromArrayValMayBeExplicitlyRecovered)
{
  const char* src =
    "trait T\n"
    "class C1 is T\n"
    "class C2 is T\n"
    "class C3 is T\n"

    "primitive Foo\n"
    "  fun apply(): Array[T] val =>\n"
    "    recover\n"
    "      let c1: C1 = C1\n"
    "      let c2: C2 = C2\n"
    "      let c3: C3 = C3\n"
    "      [c1; c2; c3]\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(ArrayTest, InferFromArrayBoxNeedNotRecovered)
{
  const char* src =
    "trait T\n"
    "class C1 is T\n"
    "class C2 is T\n"
    "class C3 is T\n"

    "primitive Foo\n"
    "  fun apply(): Array[T] box =>\n"
    "    let c1: C1 = C1\n"
    "    let c2: C2 = C2\n"
    "    let c3: C3 = C3\n"
    "    [c1; c2; c3]";

  TEST_COMPILE(src);
}


TEST_F(ArrayTest, EmptyArrayLiteral)
{
  const char* src =
    "trait T\n"

    "class Foo\n"
    "  let a1: Array[T] = []\n"
    "  let a2: (U64 | Array[T] | None) = []";

  TEST_COMPILE(src);
}


TEST_F(ArrayTest, EmptyArrayLiteralMustBeInferable)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"

    "primitive Foo\n"
    "  fun apply() =>\n"
    "    let a1 = []\n"
    "    let a2: (Array[T1] | Array[T2]) = []";

  TEST_ERRORS_2(src,
    "must specify the element type or it must be inferable",
    "must specify the element type or it must be inferable");
}


TEST_F(ArrayTest, EmbedFieldFromEmptyArrayLiteral)
{
  // An empty array literal desugars to a bare Array.create() constructor call,
  // which an embedded field requires. (A non-empty literal desugars to a
  // sequence ending in a reference, not a constructor -- see the negative case
  // below.)
  const char* src =
    "class Foo\n"
    "  embed _a: Array[U32] = []\n"
    "  fun ref push(x: U32) => _a.push(x)";

  TEST_COMPILE(src);
}


TEST_F(ArrayTest, EmbedFieldFromNonEmptyArrayLiteralFails)
{
  // A non-empty array literal desugars to a sequence whose last expression is
  // the temp-local reference, not a constructor, so it can't initialise an
  // embedded field. (The empty case above stays a bare constructor and does.)
  const char* src =
    "class Foo\n"
    "  embed _a: Array[U32] = [1]";

  TEST_ERRORS_1(src, "an embedded field must be assigned using a constructor");
}


TEST_F(ArrayTest, NonEmptyTupleArrayLiteral)
{
  // A multi-element literal desugars to a temp local filled by push() calls;
  // check a representative literal (including tuple elements) type checks.
  const char* src =
    "primitive Foo\n"
    "  fun apply(): Array[(U32, U32)] val =>\n"
    "    [(0x41, 0x5A); (0xC0, 0xD6); (0x100, 0x100)]";

  TEST_COMPILE(src);
}


TEST_F(ArrayTest, NestedArrayLiteral)
{
  // A literal whose elements are themselves array literals recurses through the
  // desugar (a sequence with a temp local, nested inside another).
  const char* src =
    "primitive Foo\n"
    "  fun apply(): Array[Array[U8]] =>\n"
    "    [[as U8: 1; 2]; [as U8: 3; 4]]";

  TEST_COMPILE(src);
}


TEST_F(ArrayTest, NonEmptyArrayLiteralCompilesThroughReach)
{
  // The desugared sequence is an expression-position rawseq, not a scoped seq.
  // The full-tree well-formedness check only runs once compilation proceeds
  // into reach, so a literal has to be compiled at least that far to exercise
  // it -- the "expr"-pass cases above stop short of it.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a: Array[U32] = [1; 2; 3]\n"
    "    env.out.print(a.size().string())";

  set_builtin(nullptr);

  DO(test_compile(src, "ir"));
}
