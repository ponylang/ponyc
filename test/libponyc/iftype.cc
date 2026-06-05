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


TEST_F(IftypeTest, SelfReferentialSubtypeConstraint)
{
  // An iftype whose subtype bound names the tested parameter through a
  // union desugars to a synthetic type parameter whose constraint
  // references itself. That self-referential constraint used to crash the
  // compiler (same recursion as ponylang/ponyc#2497); it must now be a
  // clean error.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[U8](0)\n"

    "  fun foo[A](x: A) =>\n"
    "    iftype A <: (A | None) then None end";

  TEST_ERROR(src, "can't appear directly in its own constraint");
}


TEST_F(IftypeTest, SelfReferentialSubtypeConstraintInLambda)
{
  // Same self-referential iftype constraint as above, but inside a lambda.
  // The lambda's body is processed through the object-literal catch-up
  // path, where the constraint's reference binds back to the original type
  // parameter rather than the synthetic iftype one. The self-reference
  // check must recognize both bindings, so this is rejected just like the
  // method-scope case (ponylang/ponyc#5404).
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let f = {[A](x: A) =>\n"
    "      iftype A <: (A | None) then None end}\n"
    "    f[U8](0)";

  TEST_ERROR(src, "can't appear directly in its own constraint");
}


TEST_F(IftypeTest, SelfReferentialSubtypeConstraintInObjectLiteral)
{
  // The same self-referential iftype constraint inside an object literal's
  // method. Object literals share the lambda catch-up path, so this must be
  // rejected too (ponylang/ponyc#5404).
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let o = object\n"
    "      fun foo[A](x: A) =>\n"
    "        iftype A <: (A | None) then None end\n"
    "    end\n"
    "    o.foo[U8](0)";

  TEST_ERROR(src, "can't appear directly in its own constraint");
}


TEST_F(IftypeTest, SelfReferentialSubtypeConstraintTupleInLambda)
{
  // A tuple iftype condition whose first element's supertype names that
  // element through a union, inside a lambda. The self-reference reaches the
  // synthetic parameter through a tuple member rather than a bare union, so
  // it exercises a distinct branch of the constraint walk (ponylang/ponyc
  // #5404).
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let f = {[A, B](x: A, y: B) =>\n"
    "      iftype (A, B) <: ((A | None), B) then None end}\n"
    "    f[U8, U8](0, 0)";

  TEST_ERROR(src, "can't appear directly in its own constraint");
}


TEST_F(IftypeTest, SelfReferentialSubtypeConstraintIntersectionInLambda)
{
  // The self-referential supertype reaches the synthetic parameter through an
  // intersection member rather than a union, inside a lambda (ponylang/ponyc
  // #5404).
  const char* src =
    "trait Tr\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let f = {[A](x: A) =>\n"
    "      iftype A <: (A & Tr) then None end}\n"
    "    f[U8](0)";

  TEST_ERROR(src, "can't appear directly in its own constraint");
}


TEST_F(IftypeTest, SelfReferentialSubtypeConstraintInNestedLambda)
{
  // The self-referential iftype is two catch-up copies deep — a lambda nested
  // inside another lambda. Resolving the parameter to the root of its
  // ast_data chain must still collapse both copies to one identity so this is
  // rejected like the single-level cases (ponylang/ponyc#5404).
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let outer = {() =>\n"
    "      let inner = {[A](x: A) =>\n"
    "        iftype A <: (A | None) then None end}\n"
    "      inner[U8](0)}\n"
    "    outer()";

  TEST_ERROR(src, "can't appear directly in its own constraint");
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
