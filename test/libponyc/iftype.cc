#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "ir"))

#define TEST_ERROR(src, err) \
  { const char* errs[] = {err, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }


class IftypeTest : public PassTest
{};


TEST_F(IftypeTest, ThenClause_TypeConstraint)
{
  const char* src =
    "trait T\n"
    "class C is T\n"
    "  fun tag c() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[C](C)\n"

    "  fun foo[A: T](x: A) =>\n"
    "    iftype A <: C then\n"
    "      x.c()\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(IftypeTest, ElseClause_NoTypeConstraint)
{
  const char* src =
    "trait T\n"
    "class C is T\n"
    "  fun tag c() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[C](C)\n"

    "  fun foo[A: T](x: A) =>\n"
    "    iftype A <: C then\n"
    "      None\n"
    "    else\n"
    "      x.c()\n"
    "    end";

  TEST_ERROR(src, "couldn't find 'c' in 'T'");
}


TEST_F(IftypeTest, ThenClause_CapConstraint)
{
  const char* src =
    "class C\n"
    "  fun c() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[C](C)\n"

    "  fun foo[A: C](x: A) =>\n"
    "    iftype A <: C box then\n"
    "      x.c()\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(IftypeTest, ElseClause_NoCapConstraint)
{
  const char* src =
    "class C\n"
    "  fun c() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[C](C)\n"

    "  fun foo[A: C](x: A) =>\n"
    "    iftype A <: C box then\n"
    "      None\n"
    "    else\n"
    "      x.c()\n"
    "    end";

  TEST_ERROR(src, "receiver type is not a subtype of target type");
}


TEST_F(IftypeTest, Cond_InvalidType)
{
  const char* src =
    "trait T\n"
    "class C is T\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[C]()\n"

    "  fun foo[A: T]() =>\n"
    "    iftype C <: A then\n"
    "      None\n"
    "    end";

  TEST_ERROR(src, "the subtype in an iftype condition must be a type parameter "
    "or a tuple of type parameters");
}


TEST_F(IftypeTest, TupleCond_Valid)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"
    "class C1 is T1\n"
    "class C2 is T2\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[C1, C2]()\n"

    "  fun foo[A: T1, B: T2]() =>\n"
    "    iftype (A, B) <: (C1, C2) then\n"
    "      None\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(IftypeTest, TupleCond_InvalidType)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"
    "class C1 is T1\n"
    "class C2 is T2\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[C1, C2]()\n"

    "  fun foo[A: T1, B: T2]() =>\n"
    "    iftype (A, B) <: C1 then\n"
    "      None\n"
    "    end";

  TEST_ERROR(src, "iftype subtype is a tuple but supertype isn't");
}


TEST_F(IftypeTest, TupleCond_InvalidCardinality)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"
    "class C1 is T1\n"
    "class C2 is T2\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[C1, C2]()\n"

    "  fun foo[A: T1, B: T2]() =>\n"
    "    iftype (A, B) <: (C1, C2, C1) then\n"
    "      None\n"
    "    end";

  TEST_ERROR(src, "the subtype and the supertype in an iftype condition must "
    "have the same cardinality");
}

TEST_F(IftypeTest, UnconstrainedTypeparam)
{
  const char* src =
    "class C\n"
    "  fun tag c() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[C](C)\n"

    "  fun foo[A](x: A) =>\n"
    "    iftype A <: C then\n"
    "      x.c()\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(IftypeTest, RecursiveConstraint)
{
  const char* src =
    "trait T[X: T[X] #any]\n"
    "  fun tag m(): X\n"

    "class val C is T[C]\n"
    "  fun tag m(): C => C.create()\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[C](C)\n"

    "  fun foo[A](x': A) =>\n"
    "    var x = x'\n"
    "    iftype A <: T[A] then\n"
    "      x = x.m()\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(IftypeTest, Tuple_MutuallyRecursiveConstraint)
{
  const char* src =
    "trait T[X]\n"
    "  fun tag m(): X\n"

    "class val C is T[D]\n"
    "  fun tag m(): D => D.create()\n"

    "class val D is T[C]\n"
    "  fun tag m(): C => C.create()\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[C,D](C, D)\n"

    "  fun foo[A, B](x': A, y': B) =>\n"
    "    var x = x'\n"
    "    var y = y'\n"
    "    iftype (A, B) <: (T[B], T[A]) then\n"
    "      x = y.m()\n"
    "      y = x.m()\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(IftypeTest, NestedCond)
{
  const char* src =
    "trait T\n"
    "class C is T\n"
    "  fun tag c() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[C](C)\n"

    "  fun foo[A](x: A) =>\n"
    "    iftype A <: T then\n"
    "      iftype A <: C then\n"
    "        x.c()\n"
    "      end\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(IftypeTest, ReplaceTypeargs)
{
  const char* src =
    "class A\n"
    "  fun m() => None\n"
    "trait Cell[X]\n"
    "  fun get(): X\n"

    "actor Main\n"
    "  fun foo[X: Any #read](cell: Cell[X]) =>\n"
    "    iftype X <: A #read\n"
    "    then cell.get().m()\n"
    "    end\n"
    "  new create(env: Env) => None";
  TEST_COMPILE(src);
}

TEST_F(IftypeTest, InsideLambda)
{
  // From #3762
  const char* src =
    "primitive DoIt[T: (U8 | I8)]\n"
    "  fun apply(arg: T): T =>\n"
    "    let do_it = {(v: T): T =>\n"
    "      iftype T <: U8 then\n"
    "        v\n"
    "      else\n"
    "        v\n"
    "      end\n"
    "    }\n"
    "    do_it(arg)\n"

    "actor Main\n"
    "  new create(env: Env) => None";

    TEST_COMPILE(src);
}

TEST_F(IftypeTest, AsAroundIftypeWithNarrowedCall)
{
  // Regression test for #2042: `as` wrapping an `iftype` whose body calls a
  // method on a narrowed type parameter previously crashed the compiler with
  // a use-after-free during AST replacement in expr_as.
  const char* src =
    "class LitString\n"
    "interface AST\n"
    "interface HasDocs\n"
    "  fun val docs(): (LitString | None)\n"

    "actor Main\n"
    "  new create(env: Env) => None\n"
    "  fun foo[A: AST val](node: A) =>\n"
    "    try\n"
    "      iftype A <: HasDocs\n"
    "      then node.docs()\n"
    "      end as LitString\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(IftypeTest, AsAroundIftypeTupleTypeparam)
{
  // Variant of #2042 with a tuple typeparam constraint that narrows two
  // type parameters at once.
  const char* src =
    "class LitString\n"
    "interface AST\n"
    "interface HasDocs\n"
    "  fun val docs(): (LitString | None)\n"

    "actor Main\n"
    "  new create(env: Env) => None\n"
    "  fun foo[A: AST val, B: AST val](a: A, b: B) =>\n"
    "    try\n"
    "      iftype (A, B) <: (HasDocs, HasDocs)\n"
    "      then a.docs()\n"
    "      end as LitString\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(IftypeTest, AsAroundNestedIftypeWithNarrowedCall)
{
  // Variant of #2042 with a doubly-nested iftype that narrows the same
  // type parameter further before the method call.
  const char* src =
    "class LitString\n"
    "interface AST\n"
    "interface HasText\n"
    "interface HasDocs is HasText\n"
    "  fun val docs(): (LitString | None)\n"

    "actor Main\n"
    "  new create(env: Env) => None\n"
    "  fun foo[A: AST val](node: A) =>\n"
    "    try\n"
    "      iftype A <: HasText then\n"
    "        iftype A <: HasDocs\n"
    "        then node.docs()\n"
    "        end\n"
    "      end as LitString\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(IftypeTest, AsTupleAroundIftypeWithNarrowedCall)
{
  // Variant of #2042 with a tuple target type, exercising add_as_type's
  // tuple-recursion branch.
  const char* src =
    "class LitString\n"
    "interface AST\n"
    "interface HasDocs\n"
    "  fun val docs(): (LitString, LitString)\n"

    "actor Main\n"
    "  new create(env: Env) => None\n"
    "  fun foo[A: AST val](node: A) =>\n"
    "    try\n"
    "      iftype A <: HasDocs\n"
    "      then node.docs()\n"
    "      end as (LitString, LitString)\n"
    "    end";

  TEST_COMPILE(src);
}
