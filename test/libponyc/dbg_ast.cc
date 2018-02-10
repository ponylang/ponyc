#include <gtest/gtest.h>
#include <platform.h>

#include <ast/astbuild.h>
#include <ast/ast.h>
#include <pass/scope.h>
#include <ast/stringtab.h>
#include <pass/pass.h>
#include <pkg/package.h>
#include <pkg/use.h>

#include "util.h"

#define DBG_ENABLED true
#include "../../src/libponyrt/dbg/dbg.h"

#define DBG_AST_ENABLED true
#define DBG_AST_PRINT_WIDTH 40
#include "../../src/libponyc/ast/dbg_ast.h"


#define TEST_COMPILE(src) DO(test_compile(src, "scope"))
#define TEST_ERROR(src) DO(test_error(src, "scope"))


class DbgAstTest : public PassTest
{
#ifndef _MSC_VER
  protected:
    virtual void SetUp() {
      memfile = fmemopen(buffer, sizeof(buffer), "w+");
      ASSERT_TRUE(memfile != NULL);

      // Create dc set bit 0
      dc = dbg_ctx_create_with_dst_file(1, memfile);
      dbg_sb(dc, 0, 1);
      // Create a "program" but I'm not sure what second parameter
      // to BUILD should be, for now just create an TK_ID?
      ast_t* node = ast_blank(TK_ID);
      ast_set_name(node, "test");
      BUILD(prog, node,
        NODE(TK_PACKAGE, AST_SCOPE
          NODE(TK_MODULE, AST_SCOPE
            NODE(TK_ACTOR, AST_SCOPE
              ID("main")
              NONE
              NODE(TK_TAG)
              NONE
              NODE(TK_MEMBERS,
                NODE(TK_NEW, AST_SCOPE
                  NODE(TK_TAG)
                  ID("create")  // name
                  NONE          // typeparams
                  NONE          // params
                  NONE          // return type
                  NONE          // error
                  NODE(TK_SEQ, NODE(TK_TRUE))
                  NONE
                  NONE
                )
              )
            )
          )
        )
      );
      program = prog;
    }

    virtual void TearDown() {
      dbg_ctx_destroy(dc);
      fclose(memfile);
    }

    char buffer[8192];
    FILE* memfile;
    dbg_ctx_t* dc;
    ast_t* program;
#endif
};

#ifndef _MSC_VER
TEST_F(DbgAstTest, DbgAst)
{
  const char* expected =
      "TestBody:  program (package:scope\n"
      "  (module:scope\n"
      "    (actor:scope\n"
      "      (id main)\n"
      "      x\n"
      "      tag\n"
      "      x\n"
      "      (members\n"
      "        (new:scope\n"
      "          tag\n"
      "          (id create)\n"
      "          x\n"
      "          x\n"
      "          x\n"
      "          x\n"
      "          (seq true)\n"
      "          x\n"
      "          x\n"
      "        )\n"
      "      )\n"
      "    )\n"
      "  )\n"
      ")\n\n";

  //ast_print(program, DBG_AST_PRINT_WIDTH);
  DBG_AST(dc, 0, program);
  fflush(memfile);

  // Test if successful
  //printf("expected len=%zu buffer len=%zu\n",
  //  strlen(expected), strlen(buffer));
  if(strcmp(expected, buffer) != 0)
  {
    ASSERT_TRUE(strcmp(expected, buffer) == 0) <<
      "expected:" << std::endl <<
      "\"" << expected << "\"" << std::endl <<
      "got:" << std::endl <<
      "\"" << buffer << "\"";
  }
}

TEST_F(DbgAstTest, DbgAstf)
{
  const char* expected =
    "TestBody:  ast_child(actor)=(id main)\n\n";

  ast_t* module = ast_child(program);
  ast_t* actor = ast_child(module);
  ast_t* id = ast_child(actor);
  DBG_ASTF(dc, 0, id, "ast_child(actor)=");
  fflush(memfile);
  //printf("expected len=%zu buffer len=%zu\n",
  //  strlen(expected), strlen(buffer));
  if(strcmp(expected, buffer) != 0)
  {
    ASSERT_TRUE(strcmp(expected, buffer) == 0) <<
      "expected:" << std::endl <<
      "\"" << expected << "\"" << std::endl <<
      "got:" << std::endl <<
      "\"" << buffer << "\"";
  }
}

TEST_F(DbgAstTest, DbgAstp)
{
  const char* expected =
    "TestBody: id[0]: (id main)\n"
    "\n"
    "TestBody: id[-1]: (actor:scope\n"
    "  (id main)\n"
    "  x\n"
    "  tag\n"
    "  x\n"
    "  (members\n"
    "    (new:scope\n"
    "      tag\n"
    "      (id create)\n"
    "      x\n"
    "      x\n"
    "      x\n"
    "      x\n"
    "      (seq true)\n"
    "      x\n"
    "      x\n"
    "    )\n"
    "  )\n"
    ")\n\n";

  ast_t* module = ast_child(program);
  ast_t* actor = ast_child(module);
  ast_t* id = ast_child(actor);
  DBG_ASTP(dc, 0, id, 2);
  fflush(memfile);
  //printf("expected len=%zu buffer len=%zu\n",
  //  strlen(expected), strlen(buffer));
  if(strcmp(expected, buffer) != 0)
  {
    ASSERT_TRUE(strcmp(expected, buffer) == 0) <<
      "expected:" << std::endl <<
      "\"" << expected << "\"" << std::endl <<
      "got:" << std::endl <<
      "\"" << buffer << "\"";
  }
}

TEST_F(DbgAstTest, DbgAsts)
{
  const char* expected =
    "TestBody id[0]: (id main)\n"
    "\n"
    "TestBody id[1]: x\n"
    "\n"
    "TestBody id[2]: tag\n"
    "\n"
    "TestBody id[3]: x\n"
    "\n"
    "TestBody id[4]: (members\n"
    "  (new:scope\n"
    "    tag\n"
    "    (id create)\n"
    "    x\n"
    "    x\n"
    "    x\n"
    "    x\n"
    "    (seq true)\n"
    "    x\n"
    "    x\n"
    "  )\n"
    ")\n\n";

  ast_t* module = ast_child(program);
  ast_t* actor = ast_child(module);
  ast_t* id = ast_child(actor);
  DBG_ASTS(dc, 0, id);
  fflush(memfile);
  //printf("expected len=%zu buffer len=%zu\n",
  //  strlen(expected), strlen(buffer));
  if(strcmp(expected, buffer) != 0)
  {
    ASSERT_TRUE(strcmp(expected, buffer) == 0) <<
      "expected:" << std::endl <<
      "\"" << expected << "\"" << std::endl <<
      "got:" << std::endl <<
      "\"" << buffer << "\"";
  }
}
#endif
