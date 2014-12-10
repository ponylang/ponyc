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
  ast_t* start;
  builder_t* builder;

  virtual void SetUp()
  {
    use_clear_handlers();
    use_register_std();
  }

  virtual void TearDown()
  {
    builder_free(builder);
  }

  virtual void build(const char* desc)
  {
    DO(build_ast_from_string(desc, &ast, &builder));
    start = builder_find_sub_tree(builder, "start");
    ASSERT_NE((void*)NULL, start);
  }

  virtual void symtab_entry(token_id scope_id, const char* name, ast_t* expect)
  {
    ast_t* scope = find_sub_tree(ast, scope_id);
    ASSERT_NE((void*)NULL, scope);
    symtab_t* symtab = ast_get_symtab(scope);
    ASSERT_NE((void*)NULL, symtab);
    ASSERT_EQ(expect, symtab_find(symtab, stringtab(name), NULL));
  }
};


TEST_F(ScopeTest, Var)
{
  const char* tree =
    "(module{scope} (class{scope} (id Bar) x x x"
    "  (members (fvar{def start} (id foo) x x))))";

  DO(build(tree));

  ASSERT_EQ(AST_OK, pass_scope(&start, NULL));

  DO(symtab_entry(TK_CLASS, "foo", start));
  DO(symtab_entry(TK_MODULE, "foo", NULL));
}


TEST_F(ScopeTest, Let)
{
  const char* tree =
    "(module{scope} (class{scope} (id Bar) x x x"
    "  (members (flet{def start} (id foo) x x))))";

  DO(build(tree));

  ASSERT_EQ(AST_OK, pass_scope(&start, NULL));

  DO(symtab_entry(TK_CLASS, "foo", start));
  DO(symtab_entry(TK_MODULE, "foo", NULL));
}


TEST_F(ScopeTest, Param)
{
  const char* tree =
    "(module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (fun{scope} ref (id wombat) x"
    "        (params"
    "          (param{def start} (id foo) nominal x))))))";

  DO(build(tree));

  ASSERT_EQ(AST_OK, pass_scope(&start, NULL));

  DO(symtab_entry(TK_FUN, "foo", start));
  DO(symtab_entry(TK_CLASS, "foo", NULL));
  DO(symtab_entry(TK_MODULE, "foo", NULL));
}


TEST_F(ScopeTest, New)
{
  const char* tree =
    "(module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (new{scope}{def start} ref (id foo) x x x x x))))";

  DO(build(tree));

  ASSERT_EQ(AST_OK, pass_scope(&start, NULL));

  DO(symtab_entry(TK_NEW, "foo", NULL));
  DO(symtab_entry(TK_CLASS, "foo", start));
  DO(symtab_entry(TK_MODULE, "foo", NULL));
}


TEST_F(ScopeTest, Be)
{
  const char* tree =
    "(module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (be{scope}{def start} tag (id foo) x x x x x))))";

  DO(build(tree));

  ASSERT_EQ(AST_OK, pass_scope(&start, NULL));

  DO(symtab_entry(TK_BE, "foo", NULL));
  DO(symtab_entry(TK_CLASS, "foo", start));
  DO(symtab_entry(TK_MODULE, "foo", NULL));
}


TEST_F(ScopeTest, Fun)
{
  const char* tree =
    "(module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (fun{scope}{def start} ref (id foo) x x x x x))))";

  DO(build(tree));

  ASSERT_EQ(AST_OK, pass_scope(&start, NULL));

  DO(symtab_entry(TK_FUN, "foo", NULL));
  DO(symtab_entry(TK_CLASS, "foo", start));
  DO(symtab_entry(TK_MODULE, "foo", NULL));
}


TEST_F(ScopeTest, TypeParam)
{
  const char* tree =
    "(module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (fun{scope} ref (id wombat)"
    "        (typeparams (typeparam{def start} (id Foo) x x)) x x x x))))";

  DO(build(tree));

  ASSERT_EQ(AST_OK, pass_scope(&start, NULL));

  DO(symtab_entry(TK_FUN, "Foo", start));
  DO(symtab_entry(TK_CLASS, "Foo", NULL));
  DO(symtab_entry(TK_MODULE, "Foo", NULL));
}


TEST_F(ScopeTest, Local)
{
  const char* tree =
    "(module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (fun{scope} ref (id wombat) x x x x"
    "        (seq (= (var (idseq{def start} (id foo))) 3))))))";

  DO(build(tree));

  ast_t* id_ast = ast_child(start);

  pass_opt_t opt;
  pass_opt_init(&opt);
  ASSERT_EQ(AST_OK, pass_scope(&start, &opt));
  pass_opt_done(&opt);

  DO(symtab_entry(TK_FUN, "foo", id_ast));
  DO(symtab_entry(TK_CLASS, "foo", NULL));
  DO(symtab_entry(TK_MODULE, "foo", NULL));
}


TEST_F(ScopeTest, MultipleLocals)
{
  const char* tree =
    "(module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (fun{scope} ref (id wombat) x x x x"
    "        (seq (= (var"
    "          (idseq{def start} (id foo)(id bar)(id aardvark))) 3))))))";

  DO(build(tree));

  AST_GET_CHILDREN(start, foo_ast, bar_ast, aardvark_ast);

  pass_opt_t opt;
  pass_opt_init(&opt);
  ASSERT_EQ(AST_OK, pass_scope(&start, &opt));
  pass_opt_done(&opt);

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
    "(package{scope} (module{scope}"
    "  (actor{scope}{def start} (id Foo) x x x x)))";

  DO(build(tree));

  pass_opt_t opt;
  pass_opt_init(&opt);
  ASSERT_EQ(AST_OK, pass_scope(&start, &opt));
  pass_opt_done(&opt);

  DO(symtab_entry(TK_ACTOR, "Foo", NULL));
  DO(symtab_entry(TK_MODULE, "Foo", NULL));
  DO(symtab_entry(TK_PACKAGE, "Foo", start));
}


TEST_F(ScopeTest, Class)
{
  const char* tree =
    "(package{scope} (module{scope}"
    "  (class{scope}{def start} (id Foo) x x x x)))";

  DO(build(tree));

  pass_opt_t opt;
  pass_opt_init(&opt);
  ASSERT_EQ(AST_OK, pass_scope(&start, &opt));
  pass_opt_done(&opt);

  DO(symtab_entry(TK_CLASS, "Foo", NULL));
  DO(symtab_entry(TK_MODULE, "Foo", NULL));
  DO(symtab_entry(TK_PACKAGE, "Foo", start));
}


TEST_F(ScopeTest, Data)
{
  const char* tree =
    "(package{scope} (module{scope}"
    "  (primitive{scope}{def start} (id Foo) x x x x)))";

  DO(build(tree));

  pass_opt_t opt;
  pass_opt_init(&opt);
  ASSERT_EQ(AST_OK, pass_scope(&start, &opt));
  pass_opt_done(&opt);

  DO(symtab_entry(TK_PRIMITIVE, "Foo", NULL));
  DO(symtab_entry(TK_MODULE, "Foo", NULL));
  DO(symtab_entry(TK_PACKAGE, "Foo", start));
}


TEST_F(ScopeTest, Trait)
{
  const char* tree =
    "(package{scope} (module{scope}"
    "  (trait{scope}{def start} (id Foo) x x x x)))";

  DO(build(tree));

  pass_opt_t opt;
  pass_opt_init(&opt);
  ASSERT_EQ(AST_OK, pass_scope(&start, &opt));
  pass_opt_done(&opt);

  DO(symtab_entry(TK_TRAIT, "Foo", NULL));
  DO(symtab_entry(TK_MODULE, "Foo", NULL));
  DO(symtab_entry(TK_PACKAGE, "Foo", start));
}


TEST_F(ScopeTest, Type)
{
  const char* tree =
    "(package{scope} (module{scope} (type{def start} (id Foo) x)))";

  DO(build(tree));

  pass_opt_t opt;
  pass_opt_init(&opt);
  ASSERT_EQ(AST_OK, pass_scope(&start, &opt));
  pass_opt_done(&opt);

  DO(symtab_entry(TK_MODULE, "Foo", NULL));
  DO(symtab_entry(TK_PACKAGE, "Foo", start));
}


TEST_F(ScopeTest, NonTypeCannotHaveTypeName)
{
  const char* tree =
    "(package{scope} (module{scope} (class{scope} (id Bar) x x x"
    "  (members (fvar{def start} (id Foo) x x)))))";

  DO(build(tree));

  pass_opt_t opt;
  pass_opt_init(&opt);
  ASSERT_EQ(AST_ERROR, pass_scope(&start, &opt));
  pass_opt_done(&opt);
}


TEST_F(ScopeTest, TypeCannotHaveNonTypeName)
{
  const char* tree =
    "(module{scope} (class{scope}{def start} (id rar) x x x x))";

  DO(build(tree));

  pass_opt_t opt;
  pass_opt_init(&opt);
  ASSERT_EQ(AST_ERROR, pass_scope(&start, &opt));
  pass_opt_done(&opt);
}


TEST_F(ScopeTest, SameScopeNameClash)
{
  const char* tree =
    "(module{scope} (class{scope} (id Bar) x x x"
    "  (members"
    "    (fvar{def start} (id foo) x x)"
    "    (flet{def bad}   (id foo) x x))))";

  DO(build(tree));

  pass_opt_t opt;
  pass_opt_init(&opt);
  ASSERT_EQ(AST_OK, pass_scope(&start, &opt));

  ast_t* bad_ast = builder_find_sub_tree(builder, "bad");
  ASSERT_EQ(AST_ERROR, pass_scope(&bad_ast, &opt));
  pass_opt_done(&opt);
}


TEST_F(ScopeTest, ParentScopeNameClash)
{
  const char* tree =
    "(module{scope}"
    "  (class{scope} (id Bar) x x x"
    "    (members"
    "      (fun{scope}{def start} ref (id foo) x"
    "        (params"
    "          (param{def bad} (id foo) nominal x)) x x x))))";

  DO(build(tree));

  pass_opt_t opt;
  pass_opt_init(&opt);
  ASSERT_EQ(AST_OK, pass_scope(&start, &opt));

  ast_t* bad_ast = builder_find_sub_tree(builder, "bad");
  ASSERT_EQ(AST_ERROR, pass_scope(&bad_ast, &opt));
  pass_opt_done(&opt);
}


TEST_F(ScopeTest, SiblingScopeNoClash)
{
  const char* tree =
    "(program{scope} (package{scope}"
    "  (module{scope}{def start}"
    "    (class{scope} (id Bar1) x x x"
    "      (members"
    "        (fun{scope} ref (id foo) x x x x x)))"
    "    (class{scope} (id Bar2) x x x"
    "      (members"
    "        (fun{scope} ref (id foo) x x x x x))))))";

  DO(build(tree));

  ASSERT_EQ(AST_OK, ast_visit(&start, pass_scope, NULL, NULL));
}


TEST_F(ScopeTest, Package)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}{def start})))";

  const char* builtin =
    "primitive U32";

  DO(build(tree));

  package_add_magic("builtin", builtin);

  pass_opt_t opt;
  pass_opt_init(&opt);
  limit_passes(&opt, "scope1");
  ASSERT_EQ(AST_OK, pass_scope(&start, &opt));
  pass_opt_done(&opt);

  // Builtin types go in the module symbol table
  symtab_t* package_symtab = ast_get_symtab(start);
  ASSERT_NE((void*)NULL, symtab_find(package_symtab, stringtab("U32"), NULL));
}


TEST_F(ScopeTest, Use)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (use{def start} x \"test\" x))))";

  const char* used_package =
    "class Foo";

  const char* builtin =
    "primitive U32";

  DO(build(tree));

  package_add_magic("builtin", builtin);
  package_add_magic("test", used_package);

  pass_opt_t opts;
  pass_opt_init(&opts);
  limit_passes(&opts, "scope1");
  ASSERT_EQ(AST_OK, pass_scope(&start, &opts));
  pass_opt_done(&opts);

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

  const char* builtin =
    "primitive U32";

  DO(build(tree));

  package_add_magic("builtin", builtin);
  package_add_magic("test", used_package);

  pass_opt_t opts;
  pass_opt_init(&opts);
  limit_passes(&opts, "scope1");
  ASSERT_EQ(AST_OK, pass_scope(&start, &opts));
  pass_opt_done(&opts);

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
    "  (use{def start} x \"test\" (reference (id debug))))))";

  const char* used_package =
    "class Foo";

  const char* builtin =
    "primitive U32";

  DO(build(tree));

  package_add_magic("builtin", builtin);
  package_add_magic("test", used_package);

  pass_opt_t opts;
  pass_opt_init(&opts);
  limit_passes(&opts, "scope1");
  ASSERT_EQ(AST_OK, pass_scope(&start, &opts));
  pass_opt_done(&opts);

  // Use imported types go in the module symbol table
  ast_t* module = find_sub_tree(ast, TK_MODULE);
  symtab_t* module_symtab = ast_get_symtab(module);
  ASSERT_NE((void*)NULL, symtab_find(module_symtab, stringtab("Foo"), NULL));
}


TEST_F(ScopeTest, UseConditionFalse)
{
  const char* tree =
    "(program{scope} (package{scope} (module{scope}"
    "  (use{def start} x \"test\" (reference (id debug))))))";

  const char* used_package =
    "class Foo";

  const char* builtin =
    "primitive U32";

  DO(build(tree));

  package_add_magic("builtin", builtin);
  package_add_magic("test", used_package);

  pass_opt_t opts;
  pass_opt_init(&opts);
  limit_passes(&opts, "scope1");
  ASSERT_EQ(AST_OK, pass_scope(&start, &opts));
  pass_opt_done(&opts);

  // Nothing should be imported
  ast_t* module = find_sub_tree(ast, TK_MODULE);
  symtab_t* module_symtab = ast_get_symtab(module);
  ASSERT_EQ((void*)NULL, symtab_find(module_symtab, stringtab("Foo"), NULL));
}
