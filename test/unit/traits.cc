extern "C" {
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/ast/builder.h"
#include "../../src/libponyc/pass/traits.h"
#include "../../src/libponyc/ds/stringtab.h"
#include "../../src/libponyc/pkg/package.h"
}
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
