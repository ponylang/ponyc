#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <pass/scope.h>
#include <ast/stringtab.h>
#include <pass/pass.h>
#include <pkg/package.h>
#include <pkg/use.h>

#include "util.h"

class ScopeTest: public testing::Test
{
protected:
  ast_t* ast;
  ast_t* target;
  builder_t* builder;
  pass_opt_t opt;

  virtual void SetUp()
  {
    use_clear_handlers();
    use_register_std();
    package_add_magic("builtin", "primitive U32");
    pass_opt_init(&opt);
    limit_passes("scope1");
  }

  virtual void TearDown()
  {
    pass_opt_done(&opt);
    builder_free(builder);
  }

  void build(const char* desc)
  {
    DO(build_ast_from_string(desc, &ast, &builder));
    target = builder_find_sub_tree(builder, "target");
  }

  void symtab_entry(token_id scope_id, const char* name, ast_t* expect)
  {
    ast_t* scope = find_sub_tree(ast, scope_id);
    ASSERT_NE((void*)NULL, scope);
    symtab_t* symtab = ast_get_symtab(scope);
    ASSERT_NE((void*)NULL, symtab);
    ASSERT_EQ(expect, symtab_find(symtab, stringtab(name), NULL));
  }

  ast_result_t run_scope()
  {
    return ast_visit(&ast, pass_scope, NULL, &opt);
  }
};


TEST_F(ScopeTest, Var)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members (fvar{def target} (id foo) x x))))))";

  DO(build(tree));
  ASSERT_EQ(AST_OK, run_scope());

  DO(symtab_entry(TK_CLASS, "foo", target));
  DO(symtab_entry(TK_MODULE, "foo", NULL));
}


TEST_F(ScopeTest, Let)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members (flet{def target} (id foo) x x))))))";

  DO(build(tree));

  ASSERT_EQ(AST_OK, run_scope());

  DO(symtab_entry(TK_CLASS, "foo", target));
  DO(symtab_entry(TK_MODULE, "foo", NULL));
}


TEST_F(ScopeTest, Param)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (fun{scope} ref (id wombat) x"
    "        (params"
    "          (param{def target} (id foo) nominal x))"
    "        x x x))))))";

  DO(build(tree));

  ASSERT_EQ(AST_OK, run_scope());

  DO(symtab_entry(TK_FUN, "foo", target));
  DO(symtab_entry(TK_CLASS, "foo", NULL));
  DO(symtab_entry(TK_MODULE, "foo", NULL));
}


TEST_F(ScopeTest, New)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (new{scope}{def target} ref (id foo) x x x x x))))))";

  DO(build(tree));

  ASSERT_EQ(AST_OK, run_scope());

  DO(symtab_entry(TK_NEW, "foo", NULL));
  DO(symtab_entry(TK_CLASS, "foo", target));
  DO(symtab_entry(TK_MODULE, "foo", NULL));
}


TEST_F(ScopeTest, Be)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (be{scope}{def target} tag (id foo) x x x x x))))))";

  DO(build(tree));

  ASSERT_EQ(AST_OK, run_scope());

  DO(symtab_entry(TK_BE, "foo", NULL));
  DO(symtab_entry(TK_CLASS, "foo", target));
  DO(symtab_entry(TK_MODULE, "foo", NULL));
}


TEST_F(ScopeTest, Fun)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (fun{scope}{def target} ref (id foo) x x x x x))))))";

  DO(build(tree));

  ASSERT_EQ(AST_OK, run_scope());

  DO(symtab_entry(TK_FUN, "foo", NULL));
  DO(symtab_entry(TK_CLASS, "foo", target));
  DO(symtab_entry(TK_MODULE, "foo", NULL));
}


TEST_F(ScopeTest, TypeParam)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (fun{scope} ref (id wombat)"
    "        (typeparams (typeparam{def target} (id Foo) x x)) x x x x))))))";

  DO(build(tree));

  ASSERT_EQ(AST_OK, run_scope());

  DO(symtab_entry(TK_FUN, "Foo", target));
  DO(symtab_entry(TK_CLASS, "Foo", NULL));
  DO(symtab_entry(TK_MODULE, "Foo", NULL));
}


TEST_F(ScopeTest, Local)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (fun{scope} ref (id wombat) x x x x"
    "        (seq (= (var (idseq{def target} (id foo))) 3))))))))";

  DO(build(tree));
  ASSERT_EQ(AST_OK, run_scope());

  DO(symtab_entry(TK_FUN, "foo", ast_child(target)));
  DO(symtab_entry(TK_CLASS, "foo", NULL));
  DO(symtab_entry(TK_MODULE, "foo", NULL));
}


TEST_F(ScopeTest, MultipleLocals)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (fun{scope} ref (id wombat) x x x x"
    "        (seq (= (var"
    "          (idseq{def target} (id foo)(id bar)(id aardvark))) 3))))))))";

  DO(build(tree));

  AST_GET_CHILDREN(target, foo_ast, bar_ast, aardvark_ast);

  ASSERT_EQ(AST_OK, run_scope());

  DO(symtab_entry(TK_FUN, "foo", foo_ast));
  DO(symtab_entry(TK_FUN, "bar", bar_ast));
  DO(symtab_entry(TK_FUN, "aardvark", aardvark_ast));
  DO(symtab_entry(TK_CLASS, "foo", NULL));
  DO(symtab_entry(TK_CLASS, "bar", NULL));
  DO(symtab_entry(TK_CLASS, "aardvark", NULL));
  DO(symtab_entry(TK_MODULE, "foo", NULL));
  DO(symtab_entry(TK_MODULE, "bar", NULL));
  DO(symtab_entry(TK_MODULE, "aardvark", NULL));
}


TEST_F(ScopeTest, Actor)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (actor{scope}{def target} (id Foo) x x x x))))";

  DO(build(tree));
  ASSERT_EQ(AST_OK, run_scope());

  DO(symtab_entry(TK_ACTOR, "Foo", NULL));
  DO(symtab_entry(TK_MODULE, "Foo", NULL));
  DO(symtab_entry(TK_PACKAGE, "Foo", target));
}


TEST_F(ScopeTest, Class)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope}{def target} (id Foo) x x x x))))";

  DO(build(tree));
  ASSERT_EQ(AST_OK, run_scope());

  DO(symtab_entry(TK_CLASS, "Foo", NULL));
  DO(symtab_entry(TK_MODULE, "Foo", NULL));
  DO(symtab_entry(TK_PACKAGE, "Foo", target));
}


TEST_F(ScopeTest, Data)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (primitive{scope}{def target} (id Foo) x x x x))))";

  DO(build(tree));
  ASSERT_EQ(AST_OK, run_scope());

  DO(symtab_entry(TK_PRIMITIVE, "Foo", NULL));
  DO(symtab_entry(TK_MODULE, "Foo", NULL));
  DO(symtab_entry(TK_PACKAGE, "Foo", target));
}


TEST_F(ScopeTest, Trait)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (trait{scope}{def target} (id Foo) x x x x))))";

  DO(build(tree));
  ASSERT_EQ(AST_OK, run_scope());

  DO(symtab_entry(TK_TRAIT, "Foo", NULL));
  DO(symtab_entry(TK_MODULE, "Foo", NULL));
  DO(symtab_entry(TK_PACKAGE, "Foo", target));
}


TEST_F(ScopeTest, Type)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (type{def target} (id Foo) x))))";

  DO(build(tree));
  ASSERT_EQ(AST_OK, run_scope());

  DO(symtab_entry(TK_MODULE, "Foo", NULL));
  DO(symtab_entry(TK_PACKAGE, "Foo", target));
}


TEST_F(ScopeTest, NonTypeCannotHaveTypeName)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members (fvar (id Foo) x x))))))";

  DO(build(tree));
  ASSERT_EQ(AST_ERROR, run_scope());
}


TEST_F(ScopeTest, TypeCannotHaveNonTypeName)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id rar) x x x x))))";

  DO(build(tree));
  ASSERT_EQ(AST_ERROR, run_scope());
}


TEST_F(ScopeTest, SameScopeNameClash)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (fvar (id foo) x x)"
    "      (flet (id foo) x x))))))";

  DO(build(tree));
  ASSERT_EQ(AST_ERROR, run_scope());
}


TEST_F(ScopeTest, ParentScopeNameClash)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (fun{scope} ref (id foo) x"
    "        (params"
    "          (param (id foo) nominal x)) x x x))))))";

  DO(build(tree));
  ASSERT_EQ(AST_ERROR, run_scope());
}


TEST_F(ScopeTest, SiblingScopeNoClash)
{
  const char* tree =
    "(program{scope} (package{scope}"
    "  (module{scope}"
    "    (class{scope} (id Bar1) x x x"
    "      (members"
    "        (fun{scope} ref (id foo) x x x x x)))"
    "    (class{scope} (id Bar2) x x x"
    "      (members"
    "        (fun{scope} ref (id foo) x x x x x))))))";

  DO(build(tree));
  ASSERT_EQ(AST_OK, run_scope());
}


TEST_F(ScopeTest, Package)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope})))";

  DO(build(tree));
  ASSERT_EQ(AST_OK, run_scope());

  // Builtin types go in the module symbol table
  ast_t* module = find_sub_tree(ast, TK_MODULE);
  symtab_t* package_symtab = ast_get_symtab(module);
  ASSERT_NE((void*)NULL, symtab_find(package_symtab, stringtab("U32"), NULL));
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
