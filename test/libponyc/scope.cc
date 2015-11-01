#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <pass/scope.h>
#include <ast/stringtab.h>
#include <pass/pass.h>
#include <pkg/package.h>
#include <pkg/use.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "scope"))
#define TEST_ERROR(src) DO(test_error(src, "scope"))


class ScopeTest : public PassTest
{};


TEST_F(ScopeTest, Actor)
{
  const char* src = "actor Foo";

  TEST_COMPILE(src);

  DO(lookup_type("Foo", TK_ACTOR));
  ASSERT_EQ(1, ref_count(package, "Foo"));
}


TEST_F(ScopeTest, Class)
{
  const char* src = "class Foo";

  TEST_COMPILE(src);

  DO(lookup_type("Foo", TK_CLASS));
  ASSERT_EQ(1, ref_count(package, "Foo"));
}


TEST_F(ScopeTest, Primitive)
{
  const char* src = "primitive Foo";

  TEST_COMPILE(src);

  DO(lookup_type("Foo", TK_PRIMITIVE));
  ASSERT_EQ(1, ref_count(package, "Foo"));
}


TEST_F(ScopeTest, Trait)
{
  const char* src = "trait Foo";

  TEST_COMPILE(src);

  DO(lookup_type("Foo", TK_TRAIT));
  ASSERT_EQ(1, ref_count(package, "Foo"));
}


TEST_F(ScopeTest, Interface)
{
  const char* src = "interface Foo";

  TEST_COMPILE(src);

  DO(lookup_type("Foo", TK_INTERFACE));
  ASSERT_EQ(1, ref_count(package, "Foo"));
}


TEST_F(ScopeTest, TypeAlias)
{
  const char* src = "type Foo is Bar";

  TEST_COMPILE(src);

  DO(lookup_type("Foo", TK_TYPE));
  ASSERT_EQ(1, ref_count(package, "Foo"));
}


TEST_F(ScopeTest, VarField)
{
  const char* src = "class C var foo: U32";

  TEST_COMPILE(src);

  DO(lookup_member("C", "foo", TK_FVAR));
  ASSERT_EQ(1, ref_count(package, "foo"));
}


TEST_F(ScopeTest, LetField)
{
  const char* src = "class C let foo: U32";

  TEST_COMPILE(src);

  DO(lookup_member("C", "foo", TK_FLET));
  ASSERT_EQ(1, ref_count(package, "foo"));
}


TEST_F(ScopeTest, Be)
{
  const char* src = "actor A be foo() => None";

  TEST_COMPILE(src);

  DO(lookup_member("A", "foo", TK_BE));
  ASSERT_EQ(1, ref_count(package, "foo"));
}


TEST_F(ScopeTest, New)
{
  const char* src = "actor A new foo() => None";

  TEST_COMPILE(src);

  DO(lookup_member("A", "foo", TK_NEW));
  ASSERT_EQ(1, ref_count(package, "foo"));
}


TEST_F(ScopeTest, Fun)
{
  const char* src = "actor A fun foo() => None";

  TEST_COMPILE(src);

  DO(lookup_member("A", "foo", TK_FUN));
  ASSERT_EQ(1, ref_count(package, "foo"));
}


TEST_F(ScopeTest, FunParam)
{
  const char* src = "actor A fun foo(bar: U32) => None";

  TEST_COMPILE(src);

  DO(lookup_member("A", "foo", TK_FUN));
  DO(lookup("bar", TK_PARAM));
  ASSERT_EQ(1, ref_count(package, "bar"));
}


TEST_F(ScopeTest, TypeParam)
{
  const char* src = "actor A fun foo[T]() => None";

  TEST_COMPILE(src);

  DO(lookup_member("A", "foo", TK_FUN));
  DO(lookup("T", TK_TYPEPARAM));
  ASSERT_EQ(1, ref_count(package, "T"));
}


TEST_F(ScopeTest, Local)
{
  const char* src = "actor A fun foo() => var bar: U32 = 3";

  TEST_COMPILE(src);

  DO(lookup_member("A", "foo", TK_FUN));
  DO(lookup("bar", TK_ID));
  ASSERT_EQ(1, ref_count(package, "bar"));
}


TEST_F(ScopeTest, MultipleLocals)
{
  const char* src =
    "actor A\n"
    "  fun wombat() =>\n"
    "    (var foo: Type1, var bar: Type2, let aardvark: Type3)";

  TEST_COMPILE(src);

  DO(lookup_member("A", "wombat", TK_FUN));
  ast_t* wombat_ast = walk_ast;

  DO(lookup_in(wombat_ast, "foo", TK_ID));
  DO(lookup_in(wombat_ast, "bar", TK_ID));
  DO(lookup_in(wombat_ast, "aardvark", TK_ID));

  ASSERT_EQ(1, ref_count(package, "foo"));
  ASSERT_EQ(1, ref_count(package, "bar"));
  ASSERT_EQ(1, ref_count(package, "aardvark"));
}


TEST_F(ScopeTest, NestedLocals)
{
  const char* src =
    "actor A\n"
    "  fun wombat() =>"
    "    (var foo: Type1, (var bar: Type2, let aardvark: Type3))";

  TEST_COMPILE(src);

  DO(lookup_member("A", "wombat", TK_FUN));
  ast_t* wombat_ast = walk_ast;

  DO(lookup_in(wombat_ast, "foo", TK_ID));
  DO(lookup_in(wombat_ast, "bar", TK_ID));
  DO(lookup_in(wombat_ast, "aardvark", TK_ID));

  ASSERT_EQ(1, ref_count(package, "foo"));
  ASSERT_EQ(1, ref_count(package, "bar"));
  ASSERT_EQ(1, ref_count(package, "aardvark"));
}


TEST_F(ScopeTest, SameScopeNameClash)
{
  const char* src1 =
    "actor A\n"
    "  fun foo() =>\n"
    "    var bar1: U32 = 3\n"
    "    var bar2: U32 = 4";

  TEST_COMPILE(src1);

  const char* src2 =
    "actor A\n"
    "  fun foo() =>\n"
    "    var bar: U32 = 3\n"
    "    var bar: U32 = 4";

  TEST_ERROR(src2);
}


TEST_F(ScopeTest, ParentScopeNameClash)
{
  const char* src1 =
    "actor A\n"
    "  fun foo(foo2:U32) => 3";

  TEST_COMPILE(src1);

  const char* src2 =
    "actor A\n"
    "  fun foo(foo:U32) => 3";

  TEST_ERROR(src2);
}


TEST_F(ScopeTest, SiblingScopeNoClash)
{
  const char* src =
    "actor A1\n"
    "  fun foo(foo2:U32) => 3\n"
    "actor A2\n"
    "  fun foo(foo2:U32) => 3";

  TEST_COMPILE(src);
}


TEST_F(ScopeTest, Package)
{
  const char* src = "actor A";

  DO(test_compile(src, "import"));

  // Builtin types go in the module symbol table
  DO(lookup_in(module, "U32", TK_PRIMITIVE));
  ASSERT_EQ(1, ref_count(package, "U32"));
}


/*
TEST_F(ScopeTest, Use)
{
  const char* src =
    "use \n"
    "actor A";

  TEST_COMPILE(src);

  // Builtin types go in the module symbol table
  WALK_TREE(module);
  LOOKUP("U32", TK_PRIMITIVE);

  ASSERT_EQ(1, ref_count(package, "U32"));
}


TEST_F(ScopeTest, Use)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (use{def start} x \"test\" x))))";

  const char* used_package =
    "class Foo";

  package_add_magic("test", used_package);

  DO(build(tree));
  ASSERT_EQ(AST_OK, run_scope());

  // Use imported types go in the module symbol table
  ast_t* module = find_sub_tree(ast, TK_MODULE);
  symtab_t* module_symtab = ast_get_symtab(module);
  ASSERT_NE((void*)NULL, symtab_find(module_symtab, stringtab("Foo"), NULL));
}


TEST_F(ScopeTest, UseAs)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (use{def start} (id bar) \"test\" x))))";

  const char* used_package =
    "class Foo";

  package_add_magic("test", used_package);

  DO(build(tree));
  ASSERT_EQ(AST_OK, run_scope());

  // Use imported types go in the module symbol table
  ast_t* module = find_sub_tree(ast, TK_MODULE);
  symtab_t* module_symtab = ast_get_symtab(module);
  ASSERT_NE((void*)NULL, symtab_find(module_symtab, stringtab("bar"), NULL));
  ASSERT_EQ((void*)NULL, symtab_find(module_symtab, stringtab("Foo"), NULL));
}


TEST_F(ScopeTest, UseConditionTrue)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (use x \"test\" (reference (id debug))))))";

  const char* used_package =
    "class Foo";

  package_add_magic("test", used_package);

  DO(build(tree));
  ASSERT_EQ(AST_OK, run_scope());

  // Use imported types go in the module symbol table
  ast_t* module = find_sub_tree(ast, TK_MODULE);
  symtab_t* module_symtab = ast_get_symtab(module);
  ASSERT_NE((void*)NULL, symtab_find(module_symtab, stringtab("Foo"), NULL));
}


TEST_F(ScopeTest, UseConditionFalse)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (use x \"test\" (call x x (. (reference (id debug)) (id op_not)))))))";

  const char* used_package =
    "class Foo";

  package_add_magic("test", used_package);

  DO(build(tree));
  ASSERT_EQ(AST_OK, run_scope());

  // Nothing should be imported
  ast_t* module = find_sub_tree(ast, TK_MODULE);
  symtab_t* module_symtab = ast_get_symtab(module);
  ASSERT_EQ((void*)NULL, symtab_find(module_symtab, stringtab("Foo"), NULL));
}
*/
