#include <gtest/gtest.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN
#include <ast/ast.h>
#include <ast/builder.h>
#include <pkg/package.h>
#include <pass/pass.h>
PONY_EXTERN_C_END

#include "util.h"

static const char* builtin =
  "primitive U32 "
  "primitive None";


static void parse_good(const char* src, const char* expect)
{
  package_add_magic("prog", src);
  package_add_magic("builtin", builtin);
  limit_passes("parsefix");

  ast_t* actual_ast;
  DO(load_test_program("prog", &actual_ast));

  if(expect != NULL)
  {
    builder_t* builder;
    ast_t* expect_ast;
    DO(build_ast_from_string(expect, &expect_ast, &builder));

    bool r = build_compare_asts(expect_ast, actual_ast);

    if(!r)
    {
      printf("Expected:\n");
      ast_print(expect_ast);
      printf("Got:\n");
      ast_print(actual_ast);
    }

    ASSERT_TRUE(r);

    builder_free(builder);
  }

  ast_free(actual_ast);
}


static void parse_bad(const char* src)
{
  package_add_magic("prog", src);
  package_add_magic("builtin", builtin);
  limit_passes("parsefix");
  package_suppress_build_message();

  ASSERT_EQ((void*)NULL, program_load(stringtab("prog")));
}


class BnfTest: public testing::Test
{};


TEST(BnfTest, Rubbish)
{
  const char* src = "rubbish";

  DO(parse_bad(src));
}


TEST(BnfTest, Empty)
{
  const char* src = "";

  const char* expect =
    "(program{scope} (package{scope} (module{scope})))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, Class)
{
  const char* src =
    "class Foo"
    "  let f1:T1"
    "  let f2:T2 = 5"
    "  var f3:P3.T3"
    "  var f4:T4 = 9"
    "  new m1()"
    "  fun ref m2() => 1";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Foo) x x x"
    "    (members"
    "      (flet (id f1) (nominal (id T1) x x x x) x)"
    "      (flet (id f2) (nominal (id T2) x x x x) 5)"
    "      (fvar (id f3) (nominal (id P3) (id T3) x x x) x)"
    "      (fvar (id f4) (nominal (id T4) x x x x) 9)"
    "      (new{scope} x (id m1) x x x x x)"
    "      (fun{scope} ref (id m2) x x x x (seq 1))"
    ")))))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, ClassMinimal)
{
  const char* src = "class Foo";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Foo) x x x members)"
    ")))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, ClassCannotBeCalledMain)
{
  const char* src = "class Main";

  DO(parse_bad(src));
}


TEST(BnfTest, FieldMustHaveType)
{
  const char* src = "class Foo var bar";

  DO(parse_bad(src));
}


TEST(BnfTest, LetFieldMustHaveType)
{
  const char* src = "class Foo let bar";

  DO(parse_bad(src));
}




TEST(BnfTest, Use)
{
  const char* src =
    "use \"foo1\" "
    "use \"foo2\" as bar";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (use \"foo1\" x)"
    "  (use \"foo2\" (id bar))"
    ")))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, UseMustBeBeforeClass)
{
  const char* src = "class Foo use \"foo\"";

  DO(parse_bad(src));
}


TEST(BnfTest, Alias)
{
  const char* src = "type Foo is Bar";
  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (type (id Foo) (nominal (id Bar) x x x x)))))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, AliasMustHaveType)
{
  const char* src = "type Foo";

  DO(parse_bad(src));
}


// Method order

TEST(BnfTest, MethodsInOrder)
{
  const char* src =
    "actor Foo"
    "  new m1() => 1"
    "  be m2() => 2"
    "  fun ref m3() => 3";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (actor{scope} (id Foo) x x x"
    "    (members"
    "      (new{scope} x (id m1) x x x x (seq 1))"
    "      (be{scope} x (id m2) x x x x (seq 2))"
    "      (fun{scope} ref (id m3) x x x x (seq 3))"
    ")))))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, ConstructorAfterBehaviour)
{
  const char* src =
    "actor Foo"
    "  be m2() => 2"
    "  new m1() => 1";

  DO(parse_bad(src));
}


TEST(BnfTest, ConstructorAfterFucntion)
{
  const char* src =
    "actor Foo"
    "  fun ref m3() => 3"
    "  new m1() => 1";

  DO(parse_bad(src));
}


TEST(BnfTest, BehaviourAfterFunction)
{
  const char* src =
    "actor Foo"
    "  fun ref m3() => 3"
    "  be m2() => 2";

  DO(parse_bad(src));
}


// Double arrow

TEST(BnfTest, DoubleArrowPresent)
{
  const char* src =
    "trait Foo"
    "  fun ref f() => 1";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (trait{scope} (id Foo) x x x"
    "    (members"
    "      (fun{scope} ref (id f) x x x x (seq 1))"
    ")))))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, DoubleArrowMissing)
{
  const char* src =
    "trait Foo"
    "  fun ref f() 1";

  DO(parse_bad(src));
}


TEST(BnfTest, DoubleArrowWithoutBody)
{
  const char* src =
    "trait Foo"
    "  fun ref f() =>";

  DO(parse_bad(src));
}


// Operator lack of precedence

TEST(BnfTest, InfixSingleOp)
{
  const char* src = "class C fun ref f() => 1 + 2";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id C) x x x"
    "    (members (fun{scope} ref (id f) x x x x (seq"
    "      (+ 1 2)"
    ")))))))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, InfixRepeatedOp)
{
  const char* src = "class C fun ref f() => 1 + 2 + 3";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id C) x x x"
    "    (members (fun{scope} ref (id f) x x x x (seq"
    "      (+ (+ 1 2) 3)"
    ")))))))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, InfixTwoOpsWithParens)
{
  const char* src = "class C fun ref f() => (1 + 2) - 3";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id C) x x x"
    "    (members (fun{scope} ref (id f) x x x x (seq"
    "      (- (tuple (seq (+ 1 2))) 3)"
    ")))))))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, InfixTwoOpsWithoutParens)
{
  const char* src = "class C fun ref f() => 1 + 2 - 3";

  DO(parse_bad(src));
}


TEST(BnfTest, AssignInfixCombo)
{
  const char* src = "class C fun ref f() => 1 = 2 + 3 = 4";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id C) x x x"
    "    (members (fun{scope} ref (id f) x x x x (seq"
    "      (= 1 (= (+ 2 3) 4))"
    ")))))))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, AssignInfixPrefixPostfixSequenceCombo)
{
  const char* src = "class C fun ref f() => 1 = -2.a + 3.b 4";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id C) x x x"
    "    (members (fun{scope} ref (id f) x x x x (seq"
    "      (= 1 (+ (- (. 2 (id a))) (. 3 (id b))))"
    "      4"
    ")))))))";

  DO(parse_good(src, expect));
}


// Type operator lack of precedence

TEST(BnfTest, InfixTypeSingleOp)
{
  const char* src = "type T is (Foo | Bar)";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (type (id T)"
    "    (uniontype (nominal (id Foo) x x x x)(nominal (id Bar) x x x x))"
    "))))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, InfixTypeNeedsParens)
{
  const char* src = "type T is Foo | Bar";

  DO(parse_bad(src));
}


TEST(BnfTest, InfixTypeRepeatedOp)
{
  const char* src = "type T is (Foo | Bar | Wombat)";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (type (id T)"
    "    (uniontype"
    "      (uniontype"
    "        (nominal (id Foo) x x x x)"
    "        (nominal (id Bar) x x x x))"
    "      (nominal (id Wombat) x x x x))"
    "))))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, InfixTypeTwoOpsWithParens)
{
  const char* src = "type T is (Foo | (Bar & Wombat))";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (type (id T)"
    "    (uniontype"
    "      (nominal (id Foo) x x x x)"
    "      (isecttype"
    "        (nominal (id Bar) x x x x)"
    "        (nominal (id Wombat) x x x x)))"
    "))))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, InfixTypeTwoOpsWithoutParens)
{
  const char* src = "type T is (Foo | Bar & Wombat)";

  DO(parse_bad(src));
}


TEST(BnfTest, InfixTypeViewpointTupleCombo)
{
  const char* src = "type T is (Foo, Foo->Bar | Wombat, Foo & Bar)";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (type (id T)"
    "    (tupletype"
    "      (tupletype"
    "        (nominal (id Foo) x x x x)"
    "        (uniontype"
    "          (->"
    "            (nominal (id Foo) x x x x)"
    "            (nominal (id Bar) x x x x))"
    "          (nominal (id Wombat) x x x x)))"
    "      (isecttype"
    "        (nominal (id Foo) x x x x)"
    "        (nominal (id Bar) x x x x)))"
    "))))";

  DO(parse_good(src, expect));
}


// Variable declaration

TEST(BnfTest, SingleVariableDef)
{
  const char* src = "class C fun ref f() => var a";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id C) x x x"
    "    (members (fun{scope} ref (id f) x x x x (seq"
    "      (var (idseq (id a)) x)"
    ")))))))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, MultipleVariableDefs)
{
  const char* src = "class C fun ref f() => var (a, b, c)";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id C) x x x"
    "    (members (fun{scope} ref (id f) x x x x (seq"
    "      (var (idseq (id a)(id b)(id c)) x)"
    ")))))))";

  DO(parse_good(src, expect));
}


TEST(BnfTest, NestedVariableDefs)
{
  const char* src = "class C fun ref f() => var (a, (b, (c, d), e))";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id C) x x x"
    "    (members (fun{scope} ref (id f) x x x x (seq"
    "      (var (idseq (id a)(idseq (id b)(idseq (id c)(id d))(id e))) x)"
    ")))))))";

  DO(parse_good(src, expect));
}


// TODO(andy): Loads more tests needed here






/*
TEST(SugarTest, TraitMain)
{
const char* before = "(trait (id Main) x box x x)";

DO(test_bad_sugar(before, AST_ERROR));
}
*/
/*
TEST(SugarTest, LetFieldInTrait)
{
const char* before = "(trait (flet{def start} (id foo) x x))";

DO(test_bad_sugar(before, AST_ERROR));
}
*/

/*
TEST(SugarTest, LetFieldNotInTrait)
{
const char* before = "(class (flet{def start} (id foo) x x))";

DO(test_good_sugar(before, before));
}


TEST(SugarTest, VarFieldInTrait)
{
const char* before = "(trait (fvar{def start} (id foo) x x))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, VarFieldNotInTrait)
{
const char* before = "(class (fvar{def start} (id foo) x x))";

DO(test_good_sugar(before, before));
}
*/
/*
TEST(SugarTest, BehaviourInClass)
{
const char* before = "(class (be{def start} tag (id foo) x x x x x))";

DO(test_bad_sugar(before, AST_ERROR));
}
*/
/*
TEST(SugarTest, FunctionIso)
{
const char* before = "(fun iso (id foo) x x x x x)";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, FunctionTrn)
{
const char* before = "(fun trn (id foo) x x x x x)";

DO(test_bad_sugar(before, AST_ERROR));
}
*/
/*
TEST(SugarTest, CaseWithNoBodyOrFollowingCase)
{
const char* before = "(case (seq 1) x x x)";

DO(test_bad_sugar(before, AST_FATAL));
}
*/
// Pure type checking "sugar"
/*
TEST(SugarTest, TypeAliasGood)
{
const char* before = "(type (id foo) (nominal (id A) x x x x))";

DO(test_good_sugar(before, before));
}


TEST(SugarTest, TypeAliasMain)
{
const char* before = "(type (id Main) (nominal (id A) x x x x))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ClassGoodWithField)
{
const char* before = "(class (id Foo) x iso x (members (flet (id m) x x)))";

DO(test_good_sugar(before, before));
}


TEST(SugarTest, ClassMain)
{
const char* before = "(class (id Main) x iso x (members (flet (id m) x x)))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ClassNominalTrait)
{
const char* before =
"(class (id Foo) x iso (types (nominal x (id A) x x x))"
"  (members (flet (id m) x x)))";

DO(test_good_sugar(before, before));
}


TEST(SugarTest, ClassNonNominalTrait)
{
const char* before =
"(class (id Foo) x iso (types (thistype)) (members (flet (id m) x x)))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ClassTraitCapability)
{
const char* before =
"(class (id Foo) x iso (types (nominal x (id A) x ref x))"
"  (members (flet (id m) x x)))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ClassTraitEphemeral)
{
const char* before =
"(class (id Foo) x iso (types (nominal x (id A) x x ^))"
"  (members (flet (id m) x x)))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ClassSecondTraitEphemeral)
{
const char* before =
"(class (id Foo) x iso"
"  (types"
"    (nominal x (id A) x x x)"
"    (nominal x (id B) x x ^))"
"  (members (flet (id m) x x)))";

DO(test_bad_sugar(before, AST_ERROR));
}
*/
/*
TEST(SugarTest, ClassWithCreateBehaviour)
{
const char* before =
"(class (id Foo) x iso x (members (be ref (id create) x x x x (seq 3))))";

DO(test_bad_sugar(before, AST_ERROR));
}
*/

/*
TEST(SugarTest, ActorMain)
{
const char* before = "(actor (id Main) x x   x (members (flet (id m) x x)))";
const char* after  = "(actor (id Main) x tag x (members (flet (id m) x x)))";

DO(test_good_sugar(before, after));
}
*/

/*
TEST(SugarTest, ActorNominalTrait)
{
const char* before =
"(actor (id Foo) x x (types (nominal x (id A) x x x))"
"  (members (flet (id m) x x)))";

const char* after =
"(actor (id Foo) x tag (types (nominal x (id A) x x x))"
"  (members (flet (id m) x x)))";

DO(test_good_sugar(before, after));
}


TEST(SugarTest, ActorNonNominalTrait)
{
const char* before =
"(actor (id Foo) x x (types (thistype)) (members (flet (id m) x x)))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ActorTraitCapability)
{
const char* before =
"(actor (id Foo) x x (types (nominal x (id A) x ref x))"
"  (members (flet (id m) x x)))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ActorTraitEphemeral)
{
const char* before =
"(actor (id Foo) x x (types (nominal x (id A) x x ^))"
"  (members (flet (id m) x x)))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ActorSecondTraitEphemeral)
{
const char* before =
"(actor (id Foo) x x"
"  (types"
"    (nominal x (id A) x x x)"
"    (nominal x (id B) x x ^))"
"  (members (flet (id m) x x)))";

DO(test_bad_sugar(before, AST_ERROR));
}
*/


/*
TEST(SugarTest, ActorWithDefCap)
{
const char* before = "(actor (id foo) x ref x (members (flet (id m) x x)))";

DO(test_bad_sugar(before, AST_ERROR));
}
*/

/*
TEST(SugarTest, ViewpointGood)
{
const char* before = "(-> thistype (nominal x (id A) x x x))";

DO(test_good_sugar(before, before));
}


TEST(SugarTest, ViewpointLeftUnionType)
{
const char* before =
"(->"
"  (uniontype"
"    (nominal x (id A) x x x)"
"    (nominal x (id B) x x x)"
"  )"
"  (nominal x (id C) x x x))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointLeftIntersectionType)
{
const char* before =
"(->"
"  (isecttype"
"    (nominal x (id A) x x x)"
"    (nominal x (id B) x x x)"
"  )"
"  (nominal x (id C) x x x))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointLeftTupleType)
{
const char* before =
"(->"
"  (tupletype"
"    (nominal x (id A) x x x)"
"    (nominal x (id B) x x x)"
"  )"
"  (nominal x (id C) x x x))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointLeftStructural)
{
const char* before =
"(-> (structural members x x) (nominal x (id A) x x x))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointLeftNominalWithCapability)
{
const char* before =
"(-> (nominal x (id A) x ref x) (nominal x (id B) x x x))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointLeftNominalEphemeral)
{
const char* before =
"(-> (nominal x (id A) x x ^) (nominal x (id B) x x x))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointRightUnionType)
{
const char* before =
"(->"
"  (nominal x (id A) x x x)"
"  (uniontype"
"    (nominal x (id B) x x x)"
"    (nominal x (id C) x x x)))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointRightIntersectionType)
{
const char* before =
"(->"
"  (nominal x (id A) x x x)"
"  (isecttype"
"    (nominal x (id B) x x x)"
"    (nominal x (id C) x x x)))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointRightTupleType)
{
const char* before =
"(->"
"  (nominal x (id A) x x x)"
"  (tupletype"
"    (nominal x (id B) x x x)"
"    (nominal x (id C) x x x)))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointRightThis)
{
const char* before = "(-> (nominal x (id A) x x x) thistype)";

DO(test_bad_sugar(before, AST_ERROR));
}
*/

/*
TEST(SugarTest, ThisTypeNotInViewpoint)
{
const char* before =
"(tupletype thistype{def start} (nominal x (id A) x x x))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ThisTypeInViewpointNotInMethod)
{
const char* before = "(-> thistype{def start} (nominal x (id A) x x x))";

DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ThisTypeInViewpointInMethod)
{
const char* before =
"(fun x (id foo) x x x x"
"  (-> thistype{def start} (nominal x (id A) x x x)))";

DO(test_good_sugar(before, before));
}


TEST(SugarTest, EphemeralNotInMethodReturnType)
{
const char* before =
"(fun x (id foo) x"
"  (params (param (id bar) (nominal x (id A) x x ^{def start}) x))"
"  x x x)";

DO(test_bad_sugar(before, AST_ERROR));
}
*/
