#include "parallel.h"
#include "expr.h"
#include "timing.h"
#include "../ast/ast.h"
#include "../ast/error.h"
#include "../ast/frame.h"
#include "../ast/stringtab.h"
#include "../expr/literal.h"
#include "../pkg/package.h"
#include "../type/subtype_cache.h"
#include "../type/type_assume.h"
#include "../../libponyrt/mem/pool.h"
#include "../../common/threads.h"
#include "ponyassert.h"
#include <string.h>
#include <stdlib.h>

#ifdef PLATFORM_IS_POSIX_BASED
#include <unistd.h>
#endif


// Typecheck all default arguments in a method's parameter list. This mirrors
// the lazy typecheck in lookup.c:lookup_nominal but runs eagerly so that the
// parallel expr pass never needs to write to a dependency package's AST.
static bool typecheck_method_defaults(ast_t* method, pass_opt_t* opt)
{
  pony_assert(ast_id(method) == TK_FUN || ast_id(method) == TK_BE ||
    ast_id(method) == TK_NEW);

  ast_t* params = ast_childidx(method, 3);
  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    ast_t* type = ast_childidx(param, 1);
    ast_t* def_arg = ast_childidx(param, 2);

    if((ast_id(def_arg) != TK_NONE) && (ast_type(def_arg) == NULL))
    {
      ast_t* child = ast_child(def_arg);

      if(ast_id(child) == TK_CALL)
        ast_settype(child, ast_from(child, TK_INFERTYPE));

      if(ast_visit_scope(&param, pass_pre_expr, pass_expr, opt,
        PASS_EXPR) != AST_OK)
        return false;

      def_arg = ast_childidx(param, 2);

      if(!coerce_literals(&def_arg, type, opt))
        return false;
    }

    param = ast_sibling(param);
  }

  return true;
}

// Walk all type definitions in a package and typecheck their methods' default
// arguments. Returns false on fatal error.
static bool typecheck_defaults_in_package(ast_t* package, pass_opt_t* opt)
{
  ast_t* module = ast_child(package);

  while(module != NULL)
  {
    if(ast_id(module) != TK_MODULE)
    {
      module = ast_sibling(module);
      continue;
    }

    ast_t* type_def = ast_child(module);

    while(type_def != NULL)
    {
      switch(ast_id(type_def))
      {
        case TK_PRIMITIVE:
        case TK_STRUCT:
        case TK_CLASS:
        case TK_ACTOR:
        case TK_TRAIT:
        case TK_INTERFACE:
        {
          ast_t* members = ast_childidx(type_def, 4);
          ast_t* member = ast_child(members);

          while(member != NULL)
          {
            switch(ast_id(member))
            {
              case TK_FUN:
              case TK_BE:
              case TK_NEW:
                if(!typecheck_method_defaults(member, opt))
                  return false;
                break;

              case TK_METHODGROUP:
              {
                ast_t* m = ast_child(member);
                while(m != NULL)
                {
                  if(!typecheck_method_defaults(m, opt))
                    return false;
                  m = ast_sibling(m);
                }
                break;
              }

              default:
                break;
            }

            member = ast_sibling(member);
          }
          break;
        }

        default:
          break;
      }

      type_def = ast_sibling(type_def);
    }

    module = ast_sibling(module);
  }

  return true;
}

// Pre-typecheck all default arguments in all packages. This eliminates cross-
// package writes during the parallel expr pass: the lazy typecheck in
// lookup.c:lookup_nominal checks ast_type(def_arg) == NULL and skips the
// typecheck when the type is already set.
static bool typecheck_all_defaults(ast_t* program, pass_opt_t* opt)
{
  ast_t* package = ast_child(program);

  while(package != NULL)
  {
    if(ast_id(package) == TK_PACKAGE)
    {
      if(!typecheck_defaults_in_package(package, opt))
        return false;
    }

    package = ast_sibling(package);
  }

  return true;
}


typedef struct thread_arg_t
{
  ast_t* package;
  pass_opt_t opt;
  strtable_t* main_strtab;
  ast_result_t result;
} thread_arg_t;


static void clone_opt(pass_opt_t* dst, pass_opt_t* src)
{
  memcpy(dst, src, sizeof(pass_opt_t));

  dst->check.frame = NULL;
  frame_push(&dst->check, NULL);

  dst->check.errors = errors_alloc();
  memset(&dst->check.stats, 0, sizeof(typecheck_stats_t));

  dst->strtab = stringtab_new();
  dst->check_tree = false;

  dst->timers = NULL;
}


static void merge_opt(pass_opt_t* main_opt, pass_opt_t* thread_opt)
{
  errors_append(main_opt->check.errors, thread_opt->check.errors);

  main_opt->check.stats.names_count +=
    thread_opt->check.stats.names_count;
  main_opt->check.stats.default_caps_count +=
    thread_opt->check.stats.default_caps_count;

  stringtab_merge(main_opt->strtab, thread_opt->strtab);
  stringtab_free(thread_opt->strtab);
  thread_opt->strtab = NULL;

  frame_pop(&thread_opt->check);

  errors_free(thread_opt->check.errors);
  thread_opt->check.errors = NULL;

}


static DECLARE_THREAD_FN(expr_thread)
{
  thread_arg_t* targ = (thread_arg_t*)arg;

  stringtab_set_fallback(targ->main_strtab);

  targ->result = ast_visit(&targ->package, pass_pre_expr, pass_expr,
    &targ->opt, PASS_EXPR);

  stringtab_clear_fallback();

  subtype_cache_done();
  type_assume_done();
  ponyint_pool_thread_cleanup();

#ifdef PLATFORM_IS_POSIX_BASED
  return NULL;
#else
  return 0;
#endif
}


static size_t get_thread_count(void)
{
  const char* env = getenv("PONYC_PARALLEL_EXPR_THREADS");

  if(env != NULL)
  {
    int n = atoi(env);

    if(n > 0)
      return (size_t)n;
  }

#ifdef PLATFORM_IS_POSIX_BASED
  long n = sysconf(_SC_NPROCESSORS_ONLN);

  if(n > 0)
    return (size_t)n;
#elif defined(PLATFORM_IS_WINDOWS)
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return (size_t)si.dwNumberOfProcessors;
#endif

  return 4;
}


parallel_result_t parallel_expr(ast_t** astp, pass_opt_t* options)
{
  ast_t* program = *astp;
  pony_assert(ast_id(program) == TK_PROGRAM);

  const char* env = getenv("PONYC_PARALLEL_EXPR");

  if(env != NULL && strcmp(env, "0") == 0)
    return PARALLEL_FALLBACK;

  package_layers_t* pl = package_compute_layers(program);

  if(pl == NULL)
    return PARALLEL_FALLBACK;

  size_t max_threads = get_thread_count();

  options->program_pass = PASS_EXPR;

  if(!typecheck_all_defaults(program, options))
  {
    package_layers_free(pl);
    return PARALLEL_ERROR;
  }

  bool any_error = false;

  for(size_t layer = 0; layer < pl->count; layer++)
  {
    package_layer_t* l = &pl->layers[layer];

    if(l->count == 0)
      continue;

    if(l->count == 1)
    {
      ast_result_t r = ast_visit(&l->packages[0], pass_pre_expr, pass_expr,
        options, PASS_EXPR);

      if(r == AST_FATAL)
      {
        package_layers_free(pl);
        return PARALLEL_FATAL;
      }

      if(r == AST_ERROR)
        any_error = true;

      continue;
    }

    size_t n = l->count;
    thread_arg_t* targs = (thread_arg_t*)ponyint_pool_alloc_size(
      n * sizeof(thread_arg_t));
    pony_thread_id_t* threads = (pony_thread_id_t*)ponyint_pool_alloc_size(
      n * sizeof(pony_thread_id_t));

    memset(threads, 0, n * sizeof(pony_thread_id_t));

    strtable_t* main_strtab = options->strtab;

    for(size_t i = 0; i < n; i++)
    {
      clone_opt(&targs[i].opt, options);
      targs[i].package = l->packages[i];
      targs[i].main_strtab = main_strtab;
      targs[i].result = AST_OK;
    }

    for(size_t i = 0; i < n && i < max_threads; i++)
    {
      if(!ponyint_thread_create(&threads[i], expr_thread, 0, &targs[i]))
        threads[i] = 0;
    }

    // These run on the main thread concurrently with spawned threads.
    // Safe: main_strtab is read-only (via fallback), each cloned opt
    // writes only its own package AST, and typecheck_all_defaults
    // eliminated the only cross-package write path.
    for(size_t i = max_threads; i < n; i++)
    {
      stringtab_set_fallback(main_strtab);
      targs[i].result = ast_visit(&targs[i].package, pass_pre_expr,
        pass_expr, &targs[i].opt, PASS_EXPR);
      stringtab_clear_fallback();
    }

    for(size_t i = 0; i < n && i < max_threads; i++)
    {
      if(threads[i] == 0)
      {
        stringtab_set_fallback(main_strtab);
        targs[i].result = ast_visit(&targs[i].package, pass_pre_expr,
          pass_expr, &targs[i].opt, PASS_EXPR);
        stringtab_clear_fallback();
      }
    }

    for(size_t i = 0; i < n && i < max_threads; i++)
    {
      if(threads[i] != 0)
        ponyint_thread_join(threads[i]);
    }

    bool layer_fatal = false;

    for(size_t i = 0; i < n; i++)
    {
      if(targs[i].result == AST_FATAL)
        layer_fatal = true;

      if(targs[i].result == AST_ERROR)
        any_error = true;
    }

    for(size_t i = 0; i < n; i++)
      merge_opt(options, &targs[i].opt);

    ponyint_pool_free_size(n * sizeof(thread_arg_t), targs);
    ponyint_pool_free_size(n * sizeof(pony_thread_id_t), threads);

    if(layer_fatal)
    {
      package_layers_free(pl);
      return PARALLEL_FATAL;
    }
  }

  package_layers_free(pl);

  ast_pass_record(program, PASS_EXPR);

  return any_error ? PARALLEL_ERROR : PARALLEL_OK;
}
