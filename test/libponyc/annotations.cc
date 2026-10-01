#include <gtest/gtest.h>
#include "util.h"


#define TEST_COMPILE(src, pass) DO(test_compile(src, pass))
#define TEST_ERROR(src) DO(test_error(src, "syntax"))

class AnnotationsTest : public PassTest
{};

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
    (ast_name(ast) == stringtab(opt.strtab, "a")));
  ast = ast_sibling(ast);

  ASSERT_TRUE((ast != NULL) && (ast_id(ast) == TK_ID) &&
    (ast_name(ast) == stringtab(opt.strtab, "b")));
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
    "    end\n";

  TEST_COMPILE(src, "scope");

  ast_t* c_type = lookup_type("C");

  // Get the sugared `while` node.
  ast_t* ast = lookup_in(c_type, "test_for");
  ast = ast_childidx(ast_child(ast_childidx(ast, 6)), 1);

  ASSERT_TRUE(ast_has_annotation(ast, "a", opt.strtab));
  ASSERT_FALSE(ast_has_annotation(ast, "b", opt.strtab));
  ASSERT_TRUE(ast_has_annotation(ast_childidx(ast, 2), "b", opt.strtab));

  // Get the sugared `with` node.
  ast = lookup_in(c_type, "test_with");
  ast = ast_childidx(ast_child(ast_childidx(ast, 6)), 1);

  ASSERT_TRUE(ast_has_annotation(ast, "a", opt.strtab));
  ASSERT_FALSE(ast_has_annotation(ast, "b", opt.strtab));
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

  ASSERT_FALSE(ast_has_annotation(ast, "a", opt.strtab));
  ast = lookup_in(ast, "apply");

  ASSERT_TRUE(ast_has_annotation(ast, "a", opt.strtab));
}

TEST_F(AnnotationsTest, InternalAnnotation)
{
  const char* src =
    "actor \\ponyint\\ A\n";

  TEST_ERROR(src);
}

TEST_F(AnnotationsTest, StandardAnnotationLocationBad)
{
  const char* src =
    "class \\packed, exhaustive, by_value\\ Foo\n"
    "  fun foo() =>\n"
    "    try \\likely\\ bar else None end\n"
    "    repeat \\unlikely\\ None until bar end";

  const char* errs[] = {
    "a 'packed' annotation can only appear on a struct declaration",
    "an 'exhaustive' annotation can only appear on a match expression",
    "a 'by_value' annotation can only appear on a parameter or return "
      "type of an FFI declaration",
    "a 'likely' annotation can only appear on the condition of an if, while, "
      "or until, or on the case of a match",
    "a 'unlikely' annotation can only appear on the condition of an if, while, "
      "or until, or on the case of a match",
    NULL
  };

  DO(test_expected_errors(src, "syntax", errs));
}

TEST_F(AnnotationsTest, InlineOnBehavior)
{
  const char* src =
    "actor A\n"
    "  be \\inline\\ foo() => None";

  const char* errs[] = {
    "an 'inline' annotation can only appear on a fun declaration",
    NULL
  };

  DO(test_expected_errors(src, "syntax", errs));
}

TEST_F(AnnotationsTest, InlineOnConstructor)
{
  const char* src =
    "class C\n"
    "  new \\inline\\ create() => None";

  const char* errs[] = {
    "an 'inline' annotation can only appear on a fun declaration",
    NULL
  };

  DO(test_expected_errors(src, "syntax", errs));
}

TEST_F(AnnotationsTest, NoinlineOnConstructor)
{
  const char* src =
    "class C\n"
    "  new \\noinline\\ create() => None";

  const char* errs[] = {
    "a 'noinline' annotation can only appear on a fun declaration",
    NULL
  };

  DO(test_expected_errors(src, "syntax", errs));
}

TEST_F(AnnotationsTest, NoinlineOnBehavior)
{
  const char* src =
    "actor A\n"
    "  be \\noinline\\ foo() => None";

  const char* errs[] = {
    "a 'noinline' annotation can only appear on a fun declaration",
    NULL
  };

  DO(test_expected_errors(src, "syntax", errs));
}

TEST_F(AnnotationsTest, NoinlineWithArgument)
{
  const char* src =
    "class C\n"
    "  fun \\noinline(42)\\ foo(): U64 => 42";

  const char* errs[] = {
    "annotation 'noinline' does not accept an argument",
    NULL
  };

  DO(test_expected_errors(src, "syntax", errs));
}

TEST_F(AnnotationsTest, InlineArgumentTooLarge)
{
  const char* src =
    "class C\n"
    "  fun \\inline(18446744073709551616)\\ foo(): U64 => 42";

  const char* errs[] = {
    "'inline' argument is too large",
    NULL
  };

  DO(test_expected_errors(src, "syntax", errs));
}

TEST_F(AnnotationsTest, DuplicateInline)
{
  const char* src =
    "class C\n"
    "  fun \\inline, inline(500)\\ foo(): U64 => 42";

  const char* errs[] = {
    "duplicate 'inline' annotations on the same method",
    "duplicate 'inline' annotations on the same method",
    NULL
  };

  DO(test_expected_errors(src, "syntax", errs));
}

TEST_F(AnnotationsTest, InlineAndNoinlineConflict)
{
  const char* src =
    "class C\n"
    "  fun \\inline, noinline\\ foo(): U64 => 42";

  const char* errs[] = {
    "'inline' and 'noinline' cannot both appear on the same method",
    "'inline' and 'noinline' cannot both appear on the same method",
    NULL
  };

  DO(test_expected_errors(src, "syntax", errs));
}

TEST_F(AnnotationsTest, InlineZeroArgument)
{
  const char* src =
    "class C\n"
    "  fun \\inline(0)\\ foo(): U64 => 42";

  const char* errs[] = {
    "'inline' argument must be a positive integer",
    NULL
  };

  DO(test_expected_errors(src, "syntax", errs));
}

TEST_F(AnnotationsTest, NonInlineAnnotationWithArgument)
{
  const char* src =
    "actor \\nodoc(42)\\ A";

  const char* errs[] = {
    "annotation 'nodoc' does not accept an argument",
    NULL
  };

  DO(test_expected_errors(src, "syntax", errs));
}

TEST_F(AnnotationsTest, AnnotationWithValue)
{
  const char* src =
    "class C\n"
    "  fun \\inline(500)\\ foo(): U64 => 42";

  TEST_COMPILE(src, "scope");

  ast_t* c_type = lookup_type("C");
  ast_t* ast = lookup_in(c_type, "foo");

  lexint_t* value = NULL;
  ASSERT_TRUE(ast_annotation_value(ast, "inline", opt.strtab, &value));
  ASSERT_TRUE(value != NULL);
  ASSERT_EQ(value->low, (uint64_t)500);

  ASSERT_FALSE(ast_annotation_value(ast, "noinline", opt.strtab, &value));
}

TEST_F(AnnotationsTest, InlineWithoutValue)
{
  const char* src =
    "class C\n"
    "  fun \\inline\\ foo(): U64 => 42";

  TEST_COMPILE(src, "scope");

  ast_t* c_type = lookup_type("C");
  ast_t* ast = lookup_in(c_type, "foo");

  lexint_t* value = NULL;
  ASSERT_TRUE(ast_annotation_value(ast, "inline", opt.strtab, &value));
  ASSERT_TRUE(value == NULL);
}
