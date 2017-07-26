#include <gtest/gtest.h>
#include <platform.h>

#include <../libponyrt/mem/pool.h>
#include <../libponyrt/sched/scheduler.h>

#include "util.h"

#include <memory>

#define TEST_COMPILE(src, pass) DO(test_compile(src, pass))

#define TEST_COMPILE_RESUME(pass) DO(test_compile_resume(pass))

class CompilerSerialisationTest : public PassTest
{
public:
  void test_pass_ast(const char* pass);

  void test_pass_reach(const char* pass);
};

static const char* src =
  "actor Main\n"
  "  new create(env: Env) =>\n"
  "    let x = recover Array[U8] end\n"
  "    x.push(42)\n"
  "    foo(consume x)\n"

  "  be foo(x: Array[U8] val) =>\n"
  "    try\n"
  "      if x(0)? == 42 then\n"
  "        @pony_exitcode[None](U32(1))\n"
  "      end\n"
  "    end";

static void* s_alloc_fn(pony_ctx_t* ctx, size_t size)
{
  (void)ctx;
  return ponyint_pool_alloc_size(size);
}

static void s_throw_fn()
{
  throw std::exception{};
}

typedef struct ponyint_array_t
{
  void* desc;
  size_t size;
  size_t alloc;
  char* ptr;
} ponyint_array_t;

struct pool_deleter
{
  size_t size;

  void operator()(void* ptr)
  {
    ponyint_pool_free_size(size, ptr);
  }
};

struct ast_deleter
{
  void operator()(ast_t* ptr)
  {
    ast_free(ptr);
  }
};

struct reach_deleter
{
  void operator()(reach_t* ptr)
  {
    reach_free(ptr);
  }
};

void CompilerSerialisationTest::test_pass_ast(const char* pass)
{
  set_builtin(NULL);

  TEST_COMPILE(src, pass);

  pony_ctx_t ctx;
  memset(&ctx, 0, sizeof(pony_ctx_t));
  ponyint_array_t array;
  memset(&array, 0, sizeof(ponyint_array_t));

  pony_serialise(&ctx, program, ast_pony_type(), &array, s_alloc_fn, s_throw_fn);
  std::unique_ptr<char, pool_deleter> array_guard{array.ptr};
  array_guard.get_deleter().size = array.size;
  std::unique_ptr<ast_t, ast_deleter> new_guard{
    (ast_t*)pony_deserialise(&ctx, ast_pony_type(), &array, s_alloc_fn,
      s_alloc_fn, s_throw_fn)};

  ast_t* new_program = new_guard.get();

  ASSERT_NE(new_program, (void*)NULL);

  DO(check_ast_same(program, new_program));

  new_guard.release();
  new_guard.reset(program);
  program = new_program;
  package = ast_child(program);
  module = ast_child(package);

  TEST_COMPILE_RESUME("ir");

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}

void CompilerSerialisationTest::test_pass_reach(const char* pass)
{
  set_builtin(NULL);

  TEST_COMPILE(src, pass);

  ASSERT_NE(compile, (void*)NULL);

  reach_t* r = compile->reach;

  pony_ctx_t ctx;
  memset(&ctx, 0, sizeof(pony_ctx_t));
  ponyint_array_t array;
  memset(&array, 0, sizeof(ponyint_array_t));

  pony_serialise(&ctx, r, reach_pony_type(), &array, s_alloc_fn, s_throw_fn);
  std::unique_ptr<char, pool_deleter> array_guard{array.ptr};
  array_guard.get_deleter().size = array.size;
  std::unique_ptr<reach_t, reach_deleter> new_guard{
    (reach_t*)pony_deserialise(&ctx, reach_pony_type(), &array, s_alloc_fn,
      s_alloc_fn, s_throw_fn)};

  reach_t* new_r = new_guard.get();

  ASSERT_NE(new_r, (void*)NULL);
  
  new_guard.release();
  new_guard.reset(r);
  compile->reach = new_r;

  TEST_COMPILE_RESUME("ir");

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}

TEST_F(CompilerSerialisationTest, SerialiseAfterSugar)
{
  test_pass_ast("sugar");
}

TEST_F(CompilerSerialisationTest, SerialiseAfterScope)
{
  test_pass_ast("scope");
}

TEST_F(CompilerSerialisationTest, SerialiseAfterImport)
{
  test_pass_ast("import");
}

TEST_F(CompilerSerialisationTest, SerialiseAfterName)
{
  test_pass_ast("name");
}

TEST_F(CompilerSerialisationTest, SerialiseAfterFlatten)
{
  test_pass_ast("flatten");
}

TEST_F(CompilerSerialisationTest, SerialiseAfterTraits)
{
  test_pass_ast("traits");
}

TEST_F(CompilerSerialisationTest, SerialiseAfterRefer)
{
  test_pass_ast("refer");
}

TEST_F(CompilerSerialisationTest, SerialiseAfterExpr)
{
  test_pass_ast("expr");
}

TEST_F(CompilerSerialisationTest, SerialiseAfterVerify)
{
  test_pass_ast("verify");
}

TEST_F(CompilerSerialisationTest, SerialiseAfterFinal)
{
  test_pass_ast("final");
}

TEST_F(CompilerSerialisationTest, SerialiseAfterReach)
{
  test_pass_reach("reach");
}

TEST_F(CompilerSerialisationTest, SerialiseAfterPaint)
{
  test_pass_reach("paint");
}
