// This file contains old tests that need to be reimplemented elsewhere.
// Once a test has been reimplemented it should be removed from this file, so
// this then serves as a todo list. Once all the tests have gone this file
// should be removed.
// andy 2014/10/02


/*


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
    "      (nominal (id Foo) x x x x)"
    "      (uniontype"
    "        (->"
    "          (nominal (id Foo) x x x x)"
    "          (nominal (id Bar) x x x x))"
    "        (nominal (id Wombat) x x x x))"
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
