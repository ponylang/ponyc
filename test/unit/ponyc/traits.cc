#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/builder.h>
#include <pass/traits.h>
#include <ds/stringtab.h>

#include "util.h"
#include "builtin_ast.h"

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
    "(package{scope}\n"
    "  (module{scope T U32}\n"
    "    (primitive{def U32} (id U32) x val x x)\n"
    "    (trait{scope}{def T} (id T) x x x\n"
    "      (members\n"
    "        (fun{scope} ref (id bar) x x" NOMINAL(U32) " x\n"
    "          (seq 1))))\n"
    "    (class{scope}{def Foo} (id Foo) x x\n"
    "      (types " NOMINAL(T) ")\n"
    "      x)))";

  const char* expected_bar =
    "(fun{scope} ref (id bar) x x\n"
    "  (nominal x (id U32) x val x)\n"
    "  x (seq 1))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target, NULL));
  DO(check_symtab_entry(target, "bar", expected_bar));
}


TEST_F(TraitsTest, ClassBodyNotOverriddenByTrait)
{
  const char* desc =
    "(package{scope}\n"
    "  (module{scope T U32}\n"
    "    (primitive{def U32} (id U32) x val x x)\n"
    "    (trait{scope}{def T} (id T) x x x\n"
    "      (members\n"
    "        (fun{scope} ref (id bar) x x " NOMINAL(U32) " x\n"
    "          (seq 1))))\n"  // Trait bar returns 1
    "    (class{scope bar}{def Foo} (id Foo) x x\n"
    "      (types " NOMINAL(T) ")\n"
    "      (members\n"
    "        (fun{scope}{def bar} ref (id bar) x x " NOMINAL(U32) " x\n"
    "          (seq 2))))\n"  // Class bar returns 2
    "))";

  const char* expected_bar =
    "(fun{scope} ref (id bar) x x\n"
    "  (nominal x (id U32) x val x)\n"
    "  x (seq 2))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target, NULL));
  DO(check_symtab_entry(target, "bar", expected_bar));
}


TEST_F(TraitsTest, ClassBodyNotOverriddenBy2Traits)
{
  const char* desc =
    "(package{scope}\n"
    "  (module{scope T1 T2 U32}\n"
    "    (primitive{def U32} (id U32) x val x x)\n"
    "    (trait{scope}{def T1} (id T1) x x x\n"
    "      (members\n"
    "        (fun{scope} ref (id bar) x x " NOMINAL(U32) " x\n"
    "          (seq 1))))\n"  // Trait T1.bar returns 1
    "    (trait{scope}{def T2} (id T2) x x x\n"
    "      (members\n"
    "        (fun{scope} ref (id bar) x x " NOMINAL(U32) " x\n"
    "          (seq 2))))\n"  // Trait T2.bar returns 2
    "    (class{scope bar}{def Foo} (id Foo) x x\n"
    "      (types " NOMINAL(T1) NOMINAL(T2) ")\n"
    "      (members\n"
    "        (fun{scope}{def bar} ref (id bar) x x " NOMINAL(U32) " x\n"
    "          (seq 3))))\n"  // Class Foo.bar returns 3
    "))";

  const char* expected_bar =
    "(fun{scope} ref (id bar) x x\n"
    "  (nominal x (id U32) x val x)\n"
    "  x (seq 3))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target, NULL));
  DO(check_symtab_entry(target, "bar", expected_bar));
}


TEST_F(TraitsTest, NoClassBodyAnd2TraitBodies)
{
  const char* desc =
    "(package{scope}\n"
    "  (module{scope T1 T2 U32}\n"
    "    (primitive{def U32} (id U32) x val x x)\n"
    "    (trait{scope}{def T1} (id T1) x x x\n"
    "      (members\n"
    "        (fun{scope} ref (id bar) x x " NOMINAL(U32) " x\n"
    "          (seq 1))))\n"  // Trait T1.bar returns 1
    "    (trait{scope}{def T2} (id T2) x x x\n"
    "      (members\n"
    "        (fun{scope} ref (id bar) x x " NOMINAL(U32) " x\n"
    "          (seq 2))))\n"  // Trait T2.bar returns 2
    "    (class{scope}{def Foo} (id Foo) x x\n"
    "      (types " NOMINAL(T1) NOMINAL(T2) ")\n"
    "      x)\n"  // class Foo.bar not defined
    "))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_ERROR, pass_traits(&target, NULL));
}


TEST_F(TraitsTest, TransitiveTraits)
{
  const char* desc =
    "(package{scope}\n"
    "  (module{scope T1 T2 U32}\n"
    "    (primitive{def U32} (id U32) x val x x)\n"
    "    (trait{scope}{def T1} (id T1) x x x\n"
    "      (members\n"
    "        (fun{scope} ref (id bar) x x " NOMINAL(U32) " x\n"
    "          (seq 1))))\n"  // Trait T1.bar returns 1
    "    (trait{scope}{def T2} (id T2) x x\n"
    "      (types " NOMINAL(T1) ")\n" // T2 is T1
    "      members)\n"  // Trait T2.bar not defined
    "    (class{scope}{def Foo} (id Foo) x x\n"
    "      (types " NOMINAL(T2) ")\n" // Foo is T2
    "      x)\n"  // class Foo.bar not defined
    "))";

  const char* expected_bar =
    "(fun{scope} ref (id bar) x x\n"
    "  (nominal x (id U32) x val x)\n"
    "  x (seq 1))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target, NULL));
  DO(check_symtab_entry(target, "bar", expected_bar));

  // TODO: T2 should get method from T1
  //DO(check_symtab_entry(builder, "T2", "bar", expected_bar));
}


TEST_F(TraitsTest, NoBody)
{
  const char* desc =
    "(package{scope}\n"
    "  (module{scope T U32}\n"
    "    (primitive{def U32} (id U32) x val x x)\n"
    "    (trait{scope bar}{def T} (id T) x x x\n"
    "      (members\n"
    "        (fun{scope}{def bar} ref (id bar) x x " NOMINAL(U32) " x\n"
    "          x)))\n"  // Trait T1.bar has no body
    "    (class{scope}{def Foo} (id Foo) x x\n"
    "      (types " NOMINAL(T) ")\n" // Foo is T
    "      x)\n"  // class Foo.bar not defined
    "))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_ERROR, pass_traits(&target, NULL));
}


TEST_F(TraitsTest, TraitLoop)
{
  const char* desc =
    "(package{scope}\n"
    "  (module{scope T1 T2 T3}\n"
    "    (trait{scope}{def T1} (id T1) x x\n"
    "      (types " NOMINAL(T2) ")\n" // T1 is T2
    "      members)\n"
    "    (trait{scope}{def T2} (id T2) x x\n"
    "      (types " NOMINAL(T3) ")\n" // T2 is T3
    "      members)\n"
    "    (trait{scope}{def T3} (id T3) x x\n"
    "      (types " NOMINAL(T1) ")\n" // T3 is T1
    "      members)\n"
    "))";

  DO(build(desc, "T1"));
  ASSERT_EQ(AST_ERROR, pass_traits(&target, NULL));
}


TEST_F(TraitsTest, TraitAndClassNamesDontClash)
{
  const char* desc =
    "(package{scope}\n"
    "  (module{scope T U32}\n"
    "    (primitive{def U32} (id U32) x val x x)\n"
    "    (trait{scope bar}{def T} (id T) x x x\n"
    "      (members\n"
    "        (fun{scope}{def bar} ref (id bar) x\n"
    "          (params (param{def y} (id y)" NOMINAL(U32) " x))\n"
    "          " NOMINAL(U32) " x\n"
    "          (seq (paramref{dataref y} (id y))))))\n"
    "    (class{scope}{def Foo} (id Foo) x x\n"
    "      (types " NOMINAL(T) ")\n"  // Foo is T
    "      (members\n"
    "        (fvar (id y) " NOMINAL(U32) " x)))\n"
    "))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target, NULL));
}


TEST_F(TraitsTest, MethodContravarianceClassToTrait)
{
  const char* desc =
    "(package{scope}\n"
    "  (module{scope A B T}\n"
    "    (trait{scope}{def A} (id A) x x x members)\n"
    "    (trait{scope}{def B} (id B) x x\n"
    "      (types " NOMINAL(A) ")\n" // B is A
    "      members)\n"
    "    (trait{scope}{def T} (id T) x x x\n"
    "      (members\n"
    "        (fun{scope} ref (id bar) x\n" // bar(y:B):A
    "          (params (param (id y)" NOMINAL(B) "x))\n"
    "          " NOMINAL(A) " x\n"
    "          (seq 1))))\n"
    "    (class{scope bar}{def Foo} (id Foo) x x\n"
    "      (types " NOMINAL(T) ")\n"  // Foo is T
    "      (members\n"
    "        (fun{scope}{def bar} ref (id bar) x\n" // bar(y:A):B
    "          (params (param (id y)" NOMINAL(A) "x))\n"
    "          " NOMINAL(B) " x\n"
    "          (seq 2))))\n"
    "))";

  const char* expected_bar =
    "(fun{scope} ref (id bar) x\n"
    "  (params\n"
    "    (param (id y) (nominal x (id A) x val x) x))\n"
    "  (nominal x (id B) x val x) x\n"
    "  (seq 2))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target, NULL));
  DO(check_symtab_entry(target, "bar", expected_bar));
}


TEST_F(TraitsTest, MethodContravarianceTraitToTrait)
{
  const char* desc =
    "(package{scope}\n"
    "  (module{scope A B T1 T2}\n"
    "    (trait{scope}{def A} (id A) x x x members)\n"
    "    (trait{scope}{def B} (id B) x x\n"
    "      (types " NOMINAL(A) ")\n" // B is A
    "      members)\n"
    "    (trait{scope}{def T1} (id T1) x x x\n"
    "      (members\n"
    "        (fun{scope} ref (id bar) x\n" // T1.bar(y:A):B => 1
    "          (params (param (id y)" NOMINAL(A) "x))\n"
    "          " NOMINAL(B) " x\n"
    "          (seq 1))))\n"
    "    (trait{scope}{def T2} (id T2) x x x\n"
    "      (members\n"
    "        (fun{scope} ref (id bar) x\n" // T2.bar(y:B):A
    "          (params (param (id y)" NOMINAL(B) "x))\n"
    "          " NOMINAL(A) " x\n"
    "          x)))\n"
    "    (class{scope}{def Foo} (id Foo) x x\n"
    "      (types " NOMINAL(T1) NOMINAL(T2) ")\n"  // Foo is T1, T2
    "      x)\n"
    "))";

  const char* expected_bar =
    "(fun{scope} ref (id bar) x\n"
    "  (params\n"
    "    (param (id y) (nominal x (id A) x val x) x))\n"
    "  (nominal x (id B) x val x) x\n"
    "  (seq 1))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_OK, pass_traits(&target, NULL));
  DO(check_symtab_entry(target, "bar", expected_bar));
}


TEST_F(TraitsTest, MethodReverseContravariance)
{
  const char* desc =
    "(package{scope}\n"
    "  (module{scope A B T U32}\n"
    "    (primitive{def U32} (id U32) x val x x)\n"
    "    (trait{scope}{def A} (id A) x x x members)\n"
    "    (trait{scope}{def B} (id B) x x\n"
    "      (types " NOMINAL(A) ")\n" // B is A
    "      members)\n"
    "    (trait{scope}{def T} (id T) x x x\n"
    "      (members\n"
    "        (fun{scope} ref (id bar) x\n" // bar(y:A):U32
    "          (params (param (id y)" NOMINAL(A) "x))\n"
    "          " NOMINAL(U32) " x\n"
    "          x)))\n"
    "    (class{scope bar}{def Foo} (id Foo) x x\n"
    "      (types " NOMINAL(T) ")\n"  // Foo is T
    "      (members\n"
    "        (fun{scope}{def bar} ref (id bar) x\n" // bar(y:B):U32
    "          (params (param (id y)" NOMINAL(B) "x))\n"
    "          " NOMINAL(U32) " x\n"
    "          (seq 2))))\n"
    "))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_ERROR, pass_traits(&target, NULL));
}


TEST_F(TraitsTest, MethodReverseCovariance)
{
  const char* desc =
    "(package{scope}\n"
    "  (module{scope A B T U32}\n"
    "    (primitive{def U32} (id U32) x val x x)\n"
    "    (trait{scope}{def A} (id A) x x x members)\n"
    "    (trait{scope}{def B} (id B) x x\n"
    "      (types " NOMINAL(A) ")\n" // B is A
    "      members)\n"
    "    (trait{scope}{def T} (id T) x x x\n"
    "      (members\n"
    "        (fun{scope} ref (id bar) x\n" // bar():B
    "          x " NOMINAL(B) " x x)))\n"
    "    (class{scope bar}{def Foo} (id Foo) x x\n"
    "      (types " NOMINAL(T) ")\n"  // Foo is T
    "      (members\n"
    "        (fun{scope}{def bar} ref (id bar) x\n" // bar():A
    "          x " NOMINAL(A) " x\n"
    "          (seq 2))))\n"
    "))";

  DO(build(desc, "Foo"));
  ASSERT_EQ(AST_ERROR, pass_traits(&target, NULL));
}
