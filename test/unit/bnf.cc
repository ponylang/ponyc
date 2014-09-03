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
  "data U32 "
  "data None";


static void parse_good(const char* src, const char* expect)
{
  package_add_magic("prog", src);
  package_add_magic("builtin", builtin);
  limit_passes("parse");

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
      ast_print(expect_ast, 80);
      printf("Got:\n");
      ast_print(actual_ast, 80);
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
  limit_passes("parse");

  ASSERT_EQ((void*)NULL, program_load("prog"));
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
    "  be m2()"
    "  fun ref m3()";

  const char* expect =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Foo) x x x"
    "    (members"
    "      (flet (id f1) (nominal (id T1) x x x x) x)"
    "      (flet (id f2) (nominal (id T2) x x x x) 5)"
    "      (fvar (id f3) (nominal (id P3) (id T3) x x x) x)"
    "      (fvar (id f4) (nominal (id T4) x x x x) 9)"
    "      (new{scope} ref (id m1) x x x x x)"
    "      (be{scope} tag (id m2) x x x x x)"
    "      (fun{scope} ref (id m3) x x x x x)"
    ")))))";

  DO(parse_good(src, expect));
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
