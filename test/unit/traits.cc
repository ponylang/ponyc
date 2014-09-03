#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/ast/builder.h"
#include "../../src/libponyc/pass/traits.h"
#include "../../src/libponyc/ds/stringtab.h"
PONY_EXTERN_C_END

#include "util.h"
#include "builtin_ast.h"
#include <gtest/gtest.h>


class TraitsTest: public testing::Test
{
protected:
  builder_t* builder;
  ast_t* ast;
  ast_t* target;

  virtual void SetUp()
  {
    free_errors();
  }

  virtual void TearDown()
  {
    builder_free(builder);

    if(HasFatalFailure())
      print_errors();
  }

  void build(const char* desc, const char* target_def)
  {
    DO(build_ast_from_string(desc, &ast, &builder));
    target = builder_find_sub_tree(builder, target_def);
    ASSERT_NE((void*)NULL, target);
  }
};


TEST_F(TraitsTest, ClassGetsTraitBody)
{
  const char* desc =
    "(package{scope}"
    "  (module{scope T U32}"
    "    (data{def U32} (id U32) x val x x)"
    "    (trait{scope}{def T} (id T) x x x"
    "      (members"
    "        (fun{scope} ref (id bar) x x" NOMINAL(U32) " x"
    "          (seq 1))))"
    "    (class{scope}{def Foo} (id Foo) x x"
    "      (types " NOMINAL(T) ")"
    "      x)))";

  const char* expected_bar =
    "(fun{scope} ref (id bar) x x"
    "  (nominal x (id U32) x val x)"
    "  x (seq 1))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target));
  DO(check_symtab_entry(target, "bar", expected_bar));
}


TEST_F(TraitsTest, ClassBodyNotOverriddenByTrait)
{
  const char* desc =
    "(package{scope}"
    "  (module{scope T U32}"
    "    (data{def U32} (id U32) x val x x)"
    "    (trait{scope}{def T} (id T) x x x"
    "      (members"
    "        (fun{scope} ref (id bar) x x " NOMINAL(U32) " x"
    "          (seq 1))))"  // Trait bar returns 1
    "    (class{scope bar}{def Foo} (id Foo) x x"
    "      (types " NOMINAL(T) ")"
    "      (members"
    "        (fun{scope}{def bar} ref (id bar) x x " NOMINAL(U32) " x"
    "          (seq 2))))"  // Class bar returns 2
    "))";

  const char* expected_bar =
    "(fun{scope} ref (id bar) x x"
    "  (nominal x (id U32) x val x)"
    "  x (seq 2))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target));
  DO(check_symtab_entry(target, "bar", expected_bar));
}


TEST_F(TraitsTest, ClassBodyNotOverriddenBy2Traits)
{
  const char* desc =
    "(package{scope}"
    "  (module{scope T1 T2 U32}"
    "    (data{def U32} (id U32) x val x x)"
    "    (trait{scope}{def T1} (id T1) x x x"
    "      (members"
    "        (fun{scope} ref (id bar) x x " NOMINAL(U32) " x"
    "          (seq 1))))"  // Trait T1.bar returns 1
    "    (trait{scope}{def T2} (id T2) x x x"
    "      (members"
    "        (fun{scope} ref (id bar) x x " NOMINAL(U32) " x"
    "          (seq 2))))"  // Trait T2.bar returns 2
    "    (class{scope bar}{def Foo} (id Foo) x x"
    "      (types " NOMINAL(T1) NOMINAL(T2) ")"
    "      (members"
    "        (fun{scope}{def bar} ref (id bar) x x " NOMINAL(U32) " x"
    "          (seq 3))))"  // Class Foo.bar returns 3
    "))";

  const char* expected_bar =
    "(fun{scope} ref (id bar) x x"
    "  (nominal x (id U32) x val x)"
    "  x (seq 3))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target));
  DO(check_symtab_entry(target, "bar", expected_bar));
}


TEST_F(TraitsTest, NoClassBodyAnd2TraitBodies)
{
  const char* desc =
    "(package{scope}"
    "  (module{scope T1 T2 U32}"
    "    (data{def U32} (id U32) x val x x)"
    "    (trait{scope}{def T1} (id T1) x x x"
    "      (members"
    "        (fun{scope} ref (id bar) x x " NOMINAL(U32) " x"
    "          (seq 1))))"  // Trait T1.bar returns 1
    "    (trait{scope}{def T2} (id T2) x x x"
    "      (members"
    "        (fun{scope} ref (id bar) x x " NOMINAL(U32) " x"
    "          (seq 2))))"  // Trait T2.bar returns 2
    "    (class{scope}{def Foo} (id Foo) x x"
    "      (types " NOMINAL(T1) NOMINAL(T2) ")"
    "      x)"  // class Foo.bar not defined
    "))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_ERROR, pass_traits(&target));
}


TEST_F(TraitsTest, TransitiveTraits)
{
  const char* desc =
    "(package{scope}"
    "  (module{scope T1 T2 U32}"
    "    (data{def U32} (id U32) x val x x)"
    "    (trait{scope}{def T1} (id T1) x x x"
    "      (members"
    "        (fun{scope} ref (id bar) x x " NOMINAL(U32) " x"
    "          (seq 1))))"  // Trait T1.bar returns 1
    "    (trait{scope}{def T2} (id T2) x x"
    "      (types " NOMINAL(T1) ")" // T2 is T1
    "      members)"  // Trait T2.bar not defined
    "    (class{scope}{def Foo} (id Foo) x x"
    "      (types " NOMINAL(T2) ")" // Foo is T2
    "      x)"  // class Foo.bar not defined
    "))";

  const char* expected_bar =
    "(fun{scope} ref (id bar) x x"
    "  (nominal x (id U32) x val x)"
    "  x (seq 1))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target));
  DO(check_symtab_entry(target, "bar", expected_bar));

  // TODO: T2 should get method from T1
  //DO(check_symtab_entry(builder, "T2", "bar", expected_bar));
}


TEST_F(TraitsTest, NoBody)
{
  const char* desc =
    "(package{scope}"
    "  (module{scope T U32}"
    "    (data{def U32} (id U32) x val x x)"
    "    (trait{scope bar}{def T} (id T) x x x"
    "      (members"
    "        (fun{scope}{def bar} ref (id bar) x x " NOMINAL(U32) " x"
    "          x)))"  // Trait T1.bar has no body
    "    (class{scope}{def Foo} (id Foo) x x"
    "      (types " NOMINAL(T) ")" // Foo is T
    "      x)"  // class Foo.bar not defined
    "))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_ERROR, pass_traits(&target));
}


TEST_F(TraitsTest, TraitLoop)
{
  const char* desc =
    "(package{scope}"
    "  (module{scope T1 T2 T3}"
    "    (trait{scope}{def T1} (id T1) x x"
    "      (types " NOMINAL(T2) ")" // T1 is T2
    "      members)"
    "    (trait{scope}{def T2} (id T2) x x"
    "      (types " NOMINAL(T3) ")" // T2 is T3
    "      members)"
    "    (trait{scope}{def T3} (id T3) x x"
    "      (types " NOMINAL(T1) ")" // T3 is T1
    "      members)"
    "))";

  DO(build(desc, "T1"));
  ASSERT_EQ(AST_ERROR, pass_traits(&target));
}


TEST_F(TraitsTest, TraitAndClassNamesDontClash)
{
  const char* desc =
    "(package{scope}"
    "  (module{scope T U32}"
    "    (data{def U32} (id U32) x val x x)"
    "    (trait{scope bar}{def T} (id T) x x x"
    "      (members"
    "        (fun{scope}{def bar} ref (id bar) x"
    "          (params (param{def y} (id y)" NOMINAL(U32) " x))"
    "          " NOMINAL(U32) " x"
    "          (seq (paramref{dataref y} (id y))))))"
    "    (class{scope}{def Foo} (id Foo) x x"
    "      (types " NOMINAL(T) ")"  // Foo is T
    "      (members"
    "        (fvar (id y) " NOMINAL(U32) " x)))"
    "))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target));
}


TEST_F(TraitsTest, MethodContravarianceClassToTrait)
{
  const char* desc =
    "(package{scope}"
    "  (module{scope A B T}"
    "    (trait{scope}{def A} (id A) x x x members)"
    "    (trait{scope}{def B} (id B) x x"
    "      (types " NOMINAL(A) ")" // B is A
    "      members)"
    "    (trait{scope}{def T} (id T) x x x"
    "      (members"
    "        (fun{scope} ref (id bar) x" // bar(y:B):A
    "          (params (param (id y)" NOMINAL(B) "x))"
    "          " NOMINAL(A) " x"
    "          (seq 1))))"
    "    (class{scope bar}{def Foo} (id Foo) x x"
    "      (types " NOMINAL(T) ")"  // Foo is T
    "      (members"
    "        (fun{scope}{def bar} ref (id bar) x" // bar(y:A):B
    "          (params (param (id y)" NOMINAL(A) "x))"
    "          " NOMINAL(B) " x"
    "          (seq 2))))"
    "))";

  const char* expected_bar =
    "(fun{scope} ref (id bar) x"
    "  (params"
    "    (param (id y) (nominal x (id A) x val x) x))"
    "  (nominal x (id B) x val x) x"
    "  (seq 2))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target));
  DO(check_symtab_entry(target, "bar", expected_bar));
}


TEST_F(TraitsTest, MethodContravarianceTraitToTrait)
{
  const char* desc =
    "(package{scope}"
    "  (module{scope A B T1 T2}"
    "    (trait{scope}{def A} (id A) x x x members)"
    "    (trait{scope}{def B} (id B) x x"
    "      (types " NOMINAL(A) ")" // B is A
    "      members)"
    "    (trait{scope}{def T1} (id T1) x x x"
    "      (members"
    "        (fun{scope} ref (id bar) x" // T1.bar(y:A):B => 1
    "          (params (param (id y)" NOMINAL(A) "x))"
    "          " NOMINAL(B) " x"
    "          (seq 1))))"
    "    (trait{scope}{def T2} (id T2) x x x"
    "      (members"
    "        (fun{scope} ref (id bar) x" // T2.bar(y:B):A
    "          (params (param (id y)" NOMINAL(B) "x))"
    "          " NOMINAL(A) " x"
    "          x)))"
    "    (class{scope}{def Foo} (id Foo) x x"
    "      (types " NOMINAL(T1) NOMINAL(T2) ")"  // Foo is T1, T2
    "      x)"
    "))";

  const char* expected_bar =
    "(fun{scope} ref (id bar) x"
    "  (params"
    "    (param (id y) (nominal x (id A) x val x) x))"
    "  (nominal x (id B) x val x) x"
    "  (seq 1))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target));
  DO(check_symtab_entry(target, "bar", expected_bar));
}


TEST_F(TraitsTest, MethodReverseContravariance)
{
  const char* desc =
    "(package{scope}"
    "  (module{scope A B T U32}"
    "    (data{def U32} (id U32) x val x x)"
    "    (trait{scope}{def A} (id A) x x x members)"
    "    (trait{scope}{def B} (id B) x x"
    "      (types " NOMINAL(A) ")" // B is A
    "      members)"
    "    (trait{scope}{def T} (id T) x x x"
    "      (members"
    "        (fun{scope} ref (id bar) x" // bar(y:A):U32
    "          (params (param (id y)" NOMINAL(A) "x))"
    "          " NOMINAL(U32) " x"
    "          x)))"
    "    (class{scope bar}{def Foo} (id Foo) x x"
    "      (types " NOMINAL(T) ")"  // Foo is T
    "      (members"
    "        (fun{scope}{def bar} ref (id bar) x" // bar(y:B):U32
    "          (params (param (id y)" NOMINAL(B) "x))"
    "          " NOMINAL(U32) " x"
    "          (seq 2))))"
    "))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_ERROR, pass_traits(&target));
}


TEST_F(TraitsTest, MethodReverseCovariance)
{
  const char* desc =
    "(package{scope}"
    "  (module{scope A B T U32}"
    "    (data{def U32} (id U32) x val x x)"
    "    (trait{scope}{def A} (id A) x x x members)"
    "    (trait{scope}{def B} (id B) x x"
    "      (types " NOMINAL(A) ")" // B is A
    "      members)"
    "    (trait{scope}{def T} (id T) x x x"
    "      (members"
    "        (fun{scope} ref (id bar) x" // bar():B
    "          x " NOMINAL(B) " x x)))"
    "    (class{scope bar}{def Foo} (id Foo) x x"
    "      (types " NOMINAL(T) ")"  // Foo is T
    "      (members"
    "        (fun{scope}{def bar} ref (id bar) x" // bar():A
    "          x " NOMINAL(A) " x"
    "          (seq 2))))"
    "))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_ERROR, pass_traits(&target));
}


TEST_F(TraitsTest, StructuralVsNominalContravariance)
{
  const char* desc =
    "(package{scope}"
    "  (module{scope U32 T1 T2}"
    "    (data{def U32} (id U32) x val x x)"
    "    (trait{scope}{def T1} (id T1) x x x"
    "      (members"
    "        (fun{scope} ref (id bar) x" // T1.bar(y:T2):U32
    "          (params (param (id y)" NOMINAL(T2) " x))"
    "          " NOMINAL(U32) " x"
    "          x)))"
    "    (trait{scope wombat}{def T2} (id T2) x x x"
    "      (members"
    "        (fun{scope}{def wombat} ref (id wombat) x" // T2.wombat(z:U32):U32
    "          (params (param (id z)" NOMINAL(U32) " x))"
    "          " NOMINAL(U32) " x"
    "          x)))"
    "    (class{scope bar}{def Foo} (id Foo) x x"
    "      (types " NOMINAL(T1) ")"  // Foo is T1
    "      (members"
             // Foo.bar(y:{fun wombat(y:U32):U32} ref):U32
    "        (fun{scope}{def bar} ref (id bar) x"
    "          (params (param (id y)"
    "            (structural{scope} (members"
    "              (fun{scope} ref (id wombat) x"  // y.wombat(U32):U32
    "                (types " NOMINAL(U32) ")"
                     NOMINAL(U32) " x x))"
    "              val x)"
    "            x))"
    "          " NOMINAL(U32) " x"
    "          (seq 2))))"
    "))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target));
}


TEST_F(TraitsTest, SelfStructuralContravariance)
{
  const char* desc =
    "(package{scope}"
    "  (module{scope U32 U64 T}"
    "    (data{def U32} (id U32) x val x x)"
    "    (data{def U64} (id U32) x val x x)"
    "    (trait{scope}{def T} (id T) x x x"
    "      (members"
    "        (fun{scope} ref (id bar) x" // T.bar(y:Foo):U64
    "          (params (param (id y)" NOMINAL(Foo) " x))"
               NOMINAL(U64) " x x)"
    "        (fun{scope}{def wombat} ref (id wombat) x" // T.wombat():U32 => 1
    "          params" NOMINAL(U32) " x"
    "          (seq 1))))"
    "    (class{scope bar}{def Foo} (id Foo) x x"
    "      (types " NOMINAL(T) ")"  // Foo is T
    "      (members"
             // Foo.bar(y:{fun wombat():U32} ref):U64
    "        (fun{scope}{def bar} ref (id bar) x"
    "          (params (param (id y)"
    "            (structural{scope} (members"
    "              (fun{scope} ref (id wombat) x"  // y.wombat():U32
    "                x " NOMINAL(U32) " x x))"
    "              val x)"
    "          x))"
    "          " NOMINAL(U64) " x"
    "          (seq 2))))"
    "))";

  const char* expected_wombat =
    "(fun{scope} ref (id wombat) x"
    "  params (nominal x (id U32) x val x) x"
    "  (seq 1))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target));
  DO(check_symtab_entry(target, "wombat", expected_wombat));
}
=======
#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/pass/traits.h"
#include "../../src/libponyc/ds/stringtab.h"
#include "../../src/libponyc/pass/pass.h"
#include "../../src/libponyc/pkg/package.h"
PONY_EXTERN_C_END

#include "util.h"
#include <gtest/gtest.h>


class TraitsTest: public testing::Test
{};


TEST(TraitsTest, ClassGetsTraitBody)
{
  const char* prog =
    " trait T"
    "   fun ref bar(): U32 => 1"
    " class Foo is T";

  const char* builtin =
    "data U32";

  const char* expected_bar =
    "(fun:scope ref (id bar) x x"
    "  (nominal (id hygid) (id U32) x val x)"
    "  x (seq:scope 1))";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ASSERT_NE((void*)NULL, program);

  ast_t* class_ast = find_sub_tree(program, TK_CLASS);
  symtab_t* class_symtab = ast_get_symtab(class_ast);
  ast_t* class_bar = (ast_t*)symtab_get(class_symtab, stringtab("bar"));
  ASSERT_NO_FATAL_FAILURE(check_tree(expected_bar, class_bar));

  ast_free(program);
}


TEST(TraitsTest, ClassBodyNotOverriddenByTrait)
{
  const char* prog =
    " trait T"
    "   fun ref bar(): U32 => 1"
    " class Foo is T"
    "   fun ref bar(): U32 => 2";

  const char* builtin =
    "data U32";

  const char* expected_bar =
    "(fun:scope ref (id bar) x x"
    "  (nominal (id hygid) (id U32) x val x)"
    "  x (seq:scope 2))";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_t* class_ast = find_sub_tree(program, TK_CLASS);
  symtab_t* class_symtab = ast_get_symtab(class_ast);
  ast_t* class_bar = (ast_t*)symtab_get(class_symtab, stringtab("bar"));
  ASSERT_NO_FATAL_FAILURE(check_tree(expected_bar, class_bar));

  ast_free(program);
}


TEST(TraitsTest, ClassBodyNotOverriddenBy2Traits)
{
  const char* prog =
    " trait T"
    "   fun ref bar(): U32 => 1"
    " trait U"
    "   fun ref bar(): U32 => 2"
    " class Foo is T, U"
    "   fun ref bar(): U32 => 3";

  const char* builtin =
    "data U32";

  const char* expected_bar =
    "(fun:scope ref (id bar) x x"
    "  (nominal (id hygid) (id U32) x val x)"
    "  x (seq:scope 3))";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_t* class_ast = find_sub_tree(program, TK_CLASS);
  symtab_t* class_symtab = ast_get_symtab(class_ast);
  ast_t* class_bar = (ast_t*)symtab_get(class_symtab, stringtab("bar"));
  ASSERT_NO_FATAL_FAILURE(check_tree(expected_bar, class_bar));

  ast_free(program);
}


TEST(TraitsTest, NoClassBodyAnd2TraitBodies)
{
  const char* prog =
    " trait T"
    "   fun ref bar(): U32 => 1"
    " trait U"
    "   fun ref bar(): U32 => 2"
    " class Foo is T, U";

  const char* builtin =
    "data U32";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("flatten");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  free_errors();

  ASSERT_EQ(AST_ERROR, ast_visit(&program, pass_traits, NULL));
  ASSERT_EQ(1, get_error_count());

  ast_free(program);
}


TEST(TraitsTest, TransitiveTraits)
{
  const char* prog =
    " trait T is U"
    " trait U"
    "   fun ref bar(): U32 => 1"
    " class Foo is T";

  const char* builtin =
    "data U32";

  const char* expected_bar =
    "(fun:scope ref (id bar) x x"
    "  (nominal (id hygid) (id U32) x val x)"
    "  x (seq:scope 1))";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_t* class_ast = find_sub_tree(program, TK_CLASS);
  symtab_t* class_symtab = ast_get_symtab(class_ast);
  ast_t* class_bar = (ast_t*)symtab_get(class_symtab, stringtab("bar"));
  ASSERT_NO_FATAL_FAILURE(check_tree(expected_bar, class_bar));

  ast_free(program);
}


TEST(TraitsTest, NoBody)
{
  const char* prog =
    " trait T"
    "   fun ref bar(): U32"
    " class Foo is T";

  const char* builtin =
    "data U32";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("flatten");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  free_errors();

  ASSERT_EQ(AST_ERROR, ast_visit(&program, pass_traits, NULL));
  ASSERT_EQ(1, get_error_count());

  ast_free(program);
}


TEST(TraitsTest, TraitLoop)
{
  const char* prog =
    " trait T is U"
    " trait U is V"
    " trait V is T";

  const char* builtin =
    "data U32";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("flatten");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  free_errors();

  ASSERT_EQ(AST_ERROR, ast_visit(&program, pass_traits, NULL));
  ASSERT_LE(1, get_error_count());

  ast_free(program);
}


TEST(TraitsTest, TraitAndClassNamesDontClash)
{
  const char* prog =
    " trait T"
    "   fun ref bar(x: U32): U32 => x"
    " class Foo is T"
    "   var x: U32";

  const char* builtin =
    "data U32";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_free(program);
}


TEST(TraitsTest, MethodContravarianceClassToTrait)
{
  const char* prog =
    " trait A"
    " trait B is A"
    " trait C is A"
    " trait T"
    "   fun ref bar(x: B): A => x"
    " class Foo is T"
    "   fun ref bar(x: A): B => x";

  const char* builtin =
    "data U32";

  const char* expected_bar =
    "(fun:scope ref (id bar) x"
    "  (params"
    "    (param (id x) (nominal (id hygid) (id A) x ref x) x))"
    "  (nominal (id hygid) (id B) x ref x) x"
    "  (seq:scope (reference (id x))))";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_t* class_ast = find_sub_tree(program, TK_CLASS);
  symtab_t* class_symtab = ast_get_symtab(class_ast);
  ast_t* class_bar = (ast_t*)symtab_get(class_symtab, stringtab("bar"));
  ASSERT_NO_FATAL_FAILURE(check_tree(expected_bar, class_bar));
  ast_free(program);
}


TEST(TraitsTest, MethodContravarianceTraitToTrait)
{
  const char* prog =
    " trait A"
    " trait B is A"
    " trait C is A"
    " trait T"
    "   fun ref bar(x: A): B => x"
    " trait U"
    "   fun ref bar(x: B): A"
    " class Foo is T, U";

  const char* builtin =
    "data U32";

  const char* expected_bar =
    "(fun:scope ref (id bar) x"
    "  (params"
    "    (param (id x) (nominal (id hygid) (id A) x ref x) x))"
    "  (nominal (id hygid) (id B) x ref x) x"
    "  (seq:scope (reference (id x))))";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_t* class_ast = find_sub_tree(program, TK_CLASS);
  symtab_t* class_symtab = ast_get_symtab(class_ast);
  ast_t* class_bar = (ast_t*)symtab_get(class_symtab, stringtab("bar"));
  ASSERT_NO_FATAL_FAILURE(check_tree(expected_bar, class_bar));
  ast_free(program);
}


TEST(TraitsTest, MethodReverseContravariance)
{
  const char* prog =
    " trait A"
    " trait B is A"
    " trait C is A"
    " trait T"
    "   fun ref bar(x: A) => None"
    " class Foo is T"
    "   fun ref bar(x: B) => None";

  const char* builtin =
    "data None";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("flatten");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  free_errors();

  ASSERT_EQ(AST_ERROR, ast_visit(&program, pass_traits, NULL));
  ASSERT_LE(1, get_error_count());

  ast_free(program);
}


TEST(TraitsTest, MethodReverseCovariance)
{
  const char* prog =
    " trait A"
    " trait B is A"
    " trait C is A"
    " trait T"
    "   fun ref bar(): B => None"
    " class Foo is T"
    "   fun ref bar(): A => None";

  const char* builtin =
    "data None";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("flatten");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  free_errors();

  ASSERT_EQ(AST_ERROR, ast_visit(&program, pass_traits, NULL));
  ASSERT_LE(1, get_error_count());

  ast_free(program);
}


TEST(TraitsTest, StructuralVsNominalContravariance)
{
  const char* prog =
    " trait T"
    "   fun ref bar(x: U)"
    " trait U"
    "   fun ref wombat() => None"
    " class Foo is T"
    "   fun ref bar(x: {fun ref wombat()})";

  const char* builtin =
    "data None";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_free(program);
}


TEST(TraitsTest, SelfStructuralContravariance)
{
  const char* prog =
    " trait T"
    "   fun ref bar(x: Foo)"
    "   fun ref wombat() => None"
    " class Foo is T"
    "   fun ref bar(x: {fun ref wombat()}) => None";

  const char* builtin =
    "data None";

  const char* expected_bar =
    "(fun:scope ref (id bar) x"
    "  (params"
    "    (param (id x)"
    "      (structural:scope"
    "        (members"
    "          (fun:scope ref (id wombat) x x"
    "            (nominal (id hygid) (id None) x val x)"
    "            x x))"
    "        ref x) x))"
    "  (nominal (id hygid) (id None) x val x) x"
    "  (seq:scope (reference (id None)) (reference (id None))))";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_t* class_ast = find_sub_tree(program, TK_CLASS);
  symtab_t* class_symtab = ast_get_symtab(class_ast);
  ast_t* class_bar = (ast_t*)symtab_get(class_symtab, stringtab("bar"));
  ASSERT_NO_FATAL_FAILURE(check_tree(expected_bar, class_bar));
  ast_free(program);
}
>>>>>>> 9aa6d9f7e4ef857e695022ace806f357d9b4e881
