#include <gtest/gtest.h>
#include "util.h"


#define TEST_COMPILE(src, pass) DO(test_compile(src, pass))
#define TEST_ERROR(src) DO(test_error(src, "syntax"))

class AnnotationsTest : public PassTest
{};


TEST_F(AnnotationsTest, AnnotateTypes)
{
  const char* src =
    "actor \\a, b\\ A\n"
    "class \\a, b\\ C\n"
    "primitive \\a, b\\ P\n"
    "struct \\a, b\\ S\n"
    "interface \\a, b\\ I\n"
    "trait \\a, b\\ T";

  TEST_COMPILE(src, "syntax");
}

TEST_F(AnnotationsTest, AnnotateMethods)
{
  const char* src =
    "actor A\n"
    "  new \\a, b\\ create() => None\n"
    "  be \\a, b\\ apply() => None\n"
    "  fun \\a, b\\ apply() => None";

  TEST_COMPILE(src, "syntax");
}

TEST_F(AnnotationsTest, AnnotateCompoundExpr)
{
  const char* src =
    "class C\n"
    "  fun apply() =>\n"
    "    if \\a, b\\ foo then\n"
    "      None\n"
    "    elseif \\a, b\\ bar then\n"
    "      None\n"
    "    else \\a, b\\ \n"
    "      None\n"
    "    end\n"

    "    ifdef \\a, b\\ \"foo\" then\n"
    "      None\n"
    "    elseif \\a, b\\ \"bar\" then\n"
    "      None\n"
    "    else \\a, b\\ \n"
    "      None\n"
    "    end\n"

    "    while \\a, b\\ foo do\n"
    "      None\n"
    "    else \\a, b\\ \n"
    "      None\n"
    "    end\n"

    "    repeat \\a, b\\ \n"
    "      None\n"
    "    until \\a, b\\ foo else \\a, b\\ \n"
    "      None\n"
    "    end\n"

    "    match \\a, b\\ x\n"
    "    | \\a, b\\ foo => None\n"
    "    else \\a, b\\ \n"
    "      None\n"
    "    end\n"

    "    try \\a, b\\ \n"
    "      None\n"
    "    else \\a, b\\ \n"
    "      None\n"
    "    then \\a, b\\ \n"
    "      None\n"
    "    end\n"

    "    recover \\a, b\\ foo end";

  TEST_COMPILE(src, "syntax");
}

TEST_F(AnnotationsTest, UnterminatedAnnotationError)
{
  const char* src =
    "class \\a C";

  TEST_ERROR(src);
}

TEST_F(AnnotationsTest, InvalidAnnotationError)
{
  const char* src =
    "class \\0a\\ C";

  TEST_ERROR(src);
}

TEST_F(AnnotationsTest, AnnotationsArePresent)
{
  const char* src =
    "class \\a, b\\ C";

  TEST_COMPILE(src, "scope");

  ast_t* ast = lookup_type("C");

  ast = ast_annotation(ast);

  ASSERT_TRUE((ast != NULL) && (ast_id(ast) == TK_ANNOTATION));
  ast = ast_child(ast);

  ASSERT_TRUE((ast != NULL) && (ast_id(ast) == TK_ID) &&
    (ast_name(ast) == stringtab("a")));
  ast = ast_sibling(ast);

  ASSERT_TRUE((ast != NULL) && (ast_id(ast) == TK_ID) &&
    (ast_name(ast) == stringtab("b")));
}

TEST_F(AnnotationsTest, AnnotateSugar)
{
  const char* src =
    "class C\n"
    "  fun test_for() =>\n"
    "    for \\a\\ i in x do\n"
    "      None\n"
    "    else \\b\\ \n"
    "      None\n"
    "    end\n"

    "  fun test_with() =>\n"
    "    with \\a\\ x = 42 do\n"
    "      None\n"
    "    else \\b\\ \n"
    "      None\n"
    "    end\n";

  TEST_COMPILE(src, "scope");

  ast_t* c_type = lookup_type("C");

  // Get the sugared `while` node.
  ast_t* ast = lookup_in(c_type, "test_for");
  ast = ast_childidx(ast_child(ast_childidx(ast, 6)), 1);

  ASSERT_TRUE(ast_has_annotation(ast, "a"));
  ASSERT_FALSE(ast_has_annotation(ast, "b"));
  ASSERT_TRUE(ast_has_annotation(ast_childidx(ast, 2), "b"));

  // Get the sugared `try` node.
  ast = lookup_in(c_type, "test_with");
  ast = ast_childidx(ast_child(ast_childidx(ast, 6)), 1);

  ASSERT_TRUE(ast_has_annotation(ast, "a"));
  ASSERT_FALSE(ast_has_annotation(ast, "b"));
  ASSERT_TRUE(ast_has_annotation(ast_childidx(ast, 1), "b"));
}

TEST_F(AnnotationsTest, AnnotateLambda)
{
  const char* src =
    "class C\n"
    "  fun apply() =>\n"
    "    {\\a\\() => None }";

  TEST_COMPILE(src, "expr");

  // Get the type of the lambda.
  ast_t* ast = lookup_type("$1$0");

  ASSERT_FALSE(ast_has_annotation(ast, "a"));
  ast = lookup_in(ast, "apply");

  ASSERT_TRUE(ast_has_annotation(ast, "a"));
}

TEST_F(AnnotationsTest, InternalAnnotation)
{
  const char* src =
    "actor \\ponyint\\ A\n";

  TEST_ERROR(src);
}

TEST_F(AnnotationsTest, StandardAnnotationLocationGood)
{
  const char* src =
    "struct \\packed\\ Foo\n"
    "  fun foo() =>\n"
    "    if \\likely\\ bar then None end\n"
    "    while \\unlikely\\ bar do None end\n"
    "    repeat None until \\likely\\ bar end\n"
    "    match bar | \\unlikely\\ bar => None end";

  TEST_COMPILE(src, "syntax");
}

TEST_F(AnnotationsTest, StandardAnnotationLocationBad)
{
  const char* src =
    "class \\packed\\ Foo\n"
    "  fun foo() =>\n"
    "    try \\likely\\ bar else None end\n"
    "    repeat \\unlikely\\ None until bar end";

  const char* errs[] = {
    "a 'packed' annotation can only appear on a struct declaration",
    "a 'likely' annotation can only appear on the condition of an if, while, "
      "or until, or on the case of a match",
    "a 'unlikely' annotation can only appear on the condition of an if, while, "
      "or until, or on the case of a match",
    NULL
  };

  DO(test_expected_errors(src, "syntax", errs));
}
