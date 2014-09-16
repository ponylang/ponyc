#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/ast/builder.h"
#include "../../src/libponyc/pkg/package.h"
#include "../../src/libponyc/pass/pass.h"
PONY_EXTERN_C_END

#include "util.h"
#include <gtest/gtest.h>


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
