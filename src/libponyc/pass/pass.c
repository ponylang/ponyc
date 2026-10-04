#include "pass.h"
#include "syntax.h"
#include "sugar.h"
#include "scope.h"
#include "import.h"
#include "names.h"
#include "typealias_recursion.h"
#include "flatten.h"
#include "traits.h"
#include "refer.h"
#include "expr.h"
#include "completeness.h"
#include "verify.h"
#include "finalisers.h"
#include "timing.h"
#include "../expr/literal.h"
#include "../ast/ast.h"
#include "../ast/error.h"
#include "../ast/parser.h"
#include "../ast/treecheck.h"
#include "../codegen/codegen.h"
#include "../codegen/gencshim.h"
#include "../codegen/genexport.h"
#include "../pkg/package.h"
#include "../pkg/program.h"
#include "../pkg/buildflagset.h"
#include "../plugin/plugin.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef PLATFORM_IS_POSIX_BASED
#include <unistd.h>
#endif


bool limit_passes(pass_opt_t* opt, const char* pass)
{
  pass_id i = PASS_PARSE;

  while(true)
  {
    if(strcmp(pass, pass_name(i)) == 0)
    {
      opt->limit = i;
      return true;
    }

    if(i == PASS_ALL)
      return false;

    i = pass_next(i);
  }
}


const char* pass_name(pass_id pass)
{
  switch(pass)
  {
    case PASS_PARSE: return "parse";
    case PASS_SYNTAX: return "syntax";
    case PASS_SUGAR: return "sugar";
    case PASS_SCOPE: return "scope";
    case PASS_IMPORT: return "import";
    case PASS_NAME_RESOLUTION: return "name";
    case PASS_TYPEALIAS_RECURSION: return "typealias_recursion";
    case PASS_FLATTEN: return "flatten";
    case PASS_TRAITS: return "traits";
    case PASS_REFER: return "refer";
    case PASS_EXPR: return "expr";
    case PASS_COMPLETENESS: return "completeness";
    case PASS_VERIFY: return "verify";
    case PASS_FINALISER: return "final";
    case PASS_C: return "c";
    case PASS_REACH: return "reach";
    case PASS_PAINT: return "paint";
    case PASS_LLVM_IR: return "ir";
    case PASS_BITCODE: return "bitcode";
    case PASS_ALL: return "all";
    default: return "error";
  }
}


pass_id pass_next(pass_id pass)
{
  if(pass == PASS_ALL)  // Limit end of list
    return PASS_ALL;

  return (pass_id)(pass + 1);
}


pass_id pass_prev(pass_id pass)
{
  if(pass == PASS_PARSE)  // Limit start of list
    return PASS_PARSE;

  return (pass_id)(pass - 1);
}


void pass_opt_init(pass_opt_t* options)
{
  // Start with an empty typechecker frame.
  memset(options, 0, sizeof(pass_opt_t));

  // Default to serial expr. The CLI driver overrides this to 0 (auto-detect)
  // so that the command-line ponyc auto-detects the CPU count, while library
  // users (tests, tools) get serial by default and must opt in to parallel.
  options->jobs = 1;
  options->limit = PASS_ALL;
  options->verbosity = VERBOSITY_INFO;
  // The interned-string table must exist before anything that interns into it.
  options->strtab = stringtab_new();
  options->check.errors = errors_alloc();
  options->ast_print_width = 80;
  options->fat_lto = true;
  options->user_flags = userflags_create(options->strtab);
  frame_push(&options->check, NULL);
}


void pass_opt_done(pass_opt_t* options)
{
  plugin_unload(options);

  // free userflags if any
  userflags_free(options->user_flags);
  options->user_flags = NULL;

  // Free the error collection.
  errors_free(options->check.errors);
  options->check.errors = NULL;

  // Pop the initial typechecker frame.
  frame_pop(&options->check);
  pony_assert(options->check.frame == NULL);

  if(options->print_stats)
  {
    fprintf(stderr,
      "\nStats:"
      "\n  Names: " __zu
      "\n  Default caps: " __zu
      "\n",
      options->check.stats.names_count,
      options->check.stats.default_caps_count
      );
  }

  // Free the timing context if it survived to here. This covers the early-exit
  // option paths, which never start LLVM; reaching it after LLVM teardown would
  // be a bug (timing.h gives the reason), and the ponyc driver frees it before
  // then.
  if(options->timers != NULL)
  {
    pass_timers_free(options->timers);
    options->timers = NULL;
  }

  // Free the interned-string table last: every interned pointer handed out
  // during this compilation (including those inside the AST) dangles after
  // this, so nothing that holds one may be used past here.
  stringtab_free(options->strtab);
  options->strtab = NULL;
}


void pass_opt_clone_for_worker(pass_opt_t* dst, pass_opt_t* src)
{
  memcpy(dst, src, sizeof(pass_opt_t));

  dst->check.frame = NULL;
  dst->check.errors = errors_alloc();
  memset(&dst->check.stats, 0, sizeof(typecheck_stats_t));

  dst->program_pass = PASS_EXPR;
  dst->check_tree = false;
  // pass_timers_t is not thread-safe; parallel workers don't time.
  dst->timers = NULL;

  frame_push(&dst->check, NULL);
}


typedef struct expr_worker_t
{
  ast_t* package;
  pass_opt_t opt;
  ast_result_t result;
} expr_worker_t;


static DECLARE_THREAD_FN(expr_worker_fn)
{
  expr_worker_t* w = (expr_worker_t*)arg;

  w->result = ast_visit(&w->package, pass_pre_expr, pass_expr, &w->opt,
    PASS_EXPR);

  ponyint_pool_thread_cleanup();
  return NULL;
}


static uint32_t detect_cpu_count()
{
#ifdef PLATFORM_IS_POSIX_BASED
  long n = sysconf(_SC_NPROCESSORS_ONLN);
  return (n > 0) ? (uint32_t)n : 1;
#elif defined(PLATFORM_IS_WINDOWS)
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return (si.dwNumberOfProcessors > 0) ? si.dwNumberOfProcessors : 1;
#else
  return 1;
#endif
}


static bool precheck_method_default_args(ast_t* method, pass_opt_t* options)
{
  ast_t* params = ast_childidx(method, 3);
  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    ast_t* def_arg = ast_childidx(param, 2);

    if((ast_id(def_arg) != TK_NONE) && (ast_type(def_arg) == NULL))
    {
      ast_t* child = ast_child(def_arg);

      if(ast_id(child) == TK_CALL)
        ast_settype(child, ast_from(child, TK_INFERTYPE));

      if(ast_visit_scope(&param, pass_pre_expr, pass_expr, options,
        PASS_EXPR) != AST_OK)
        return false;

      def_arg = ast_childidx(param, 2);
      ast_t* type = ast_childidx(param, 1);

      if(!coerce_literals(&def_arg, type, options))
        return false;
    }

    param = ast_sibling(param);
  }

  return true;
}


// Pre-typecheck all default arguments in the given packages so that
// the parallel expr workers never trigger the lazy typechecking path
// in lookup_nominal (which would write into shared AST nodes).
static bool precheck_default_args(ast_t** packages, size_t count,
  pass_opt_t* options)
{
  for(size_t p = 0; p < count; p++)
  {
    ast_t* package = packages[p];
    ast_t* module = ast_child(package);

    while(module != NULL)
    {
      ast_t* entity = ast_child(module);

      while(entity != NULL)
      {
        switch(ast_id(entity))
        {
          case TK_ACTOR:
          case TK_CLASS:
          case TK_STRUCT:
          case TK_PRIMITIVE:
          case TK_TRAIT:
          case TK_INTERFACE:
          {
            ast_t* members = ast_childidx(entity, 4);
            ast_t* member = ast_child(members);

            while(member != NULL)
            {
              switch(ast_id(member))
              {
                case TK_METHODGROUP:
                {
                  ast_t* grouped = ast_child(member);

                  while(grouped != NULL)
                  {
                    if(!precheck_method_default_args(grouped, options))
                      return false;

                    grouped = ast_sibling(grouped);
                  }
                  break;
                }

                case TK_NEW:
                case TK_BE:
                case TK_FUN:
                {
                  if(!precheck_method_default_args(member, options))
                    return false;

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

        entity = ast_sibling(entity);
      }

      module = ast_sibling(module);
    }
  }

  return true;
}


static bool parallel_expr(ast_t* program, pass_opt_t* options)
{
  uint32_t jobs = options->jobs;

  if(jobs == 0)
    jobs = detect_cpu_count();

  ast_t* first_package = ast_child(program);

  ast_t*** layers;
  size_t* layer_sizes;
  size_t layer_count;

  package_layers(first_package, &layers, &layer_sizes, &layer_count);

  if(layer_count == 0)
  {
    ast_pass_record(program, PASS_EXPR);
    return true;
  }

  bool ok = true;

  for(size_t l = 0; l < layer_count && ok; l++)
  {
    size_t width = layer_sizes[l];

    if(width == 1)
    {
      // Single package in this layer — run it on the main thread.
      ast_result_t r = ast_visit(&layers[l][0], pass_pre_expr, pass_expr,
        options, PASS_EXPR);

      if(r == AST_FATAL || r == AST_ERROR)
        ok = false;

      continue;
    }

    // Pre-typecheck default arguments serially so the parallel workers
    // never trigger lookup_nominal's lazy typecheck path, which writes
    // into shared AST nodes.
    if(!precheck_default_args(layers[l], width, options))
    {
      ok = false;
      break;
    }

    uint32_t nworkers = (width < jobs) ? (uint32_t)width : jobs;

    // Process packages in batches of nworkers.
    for(size_t batch_start = 0; batch_start < width && ok;
        batch_start += nworkers)
    {
      uint32_t batch_size = (uint32_t)(width - batch_start);

      if(batch_size > nworkers)
        batch_size = nworkers;

      expr_worker_t* workers = (expr_worker_t*)ponyint_pool_alloc_size(
        batch_size * sizeof(expr_worker_t));
      pony_thread_id_t* threads = (pony_thread_id_t*)ponyint_pool_alloc_size(
        batch_size * sizeof(pony_thread_id_t));

      for(uint32_t i = 0; i < batch_size; i++)
      {
        workers[i].package = layers[l][batch_start + i];
        pass_opt_clone_for_worker(&workers[i].opt, options);
        workers[i].result = AST_OK;

        ponyint_thread_create(&threads[i], expr_worker_fn, 0, &workers[i]);
      }

      for(uint32_t i = 0; i < batch_size; i++)
      {
        ponyint_thread_join(threads[i]);

        if(workers[i].result == AST_FATAL || workers[i].result == AST_ERROR)
          ok = false;

        options->check.stats.names_count +=
          workers[i].opt.check.stats.names_count;
        options->check.stats.default_caps_count +=
          workers[i].opt.check.stats.default_caps_count;

        errors_merge(options->check.errors, workers[i].opt.check.errors);
        workers[i].opt.check.errors = NULL;

        frame_pop(&workers[i].opt.check);
        pony_assert(workers[i].opt.check.frame == NULL);
      }

      ponyint_pool_free_size(batch_size * sizeof(pony_thread_id_t), threads);
      ponyint_pool_free_size(batch_size * sizeof(expr_worker_t), workers);
    }
  }

  package_layers_free(layers, layer_sizes, layer_count);
  ast_pass_record(program, PASS_EXPR);
  return ok;
}


// Check whether we have reached the maximum pass we should currently perform.
// We check against both the specified last pass and the limit set in the
// options, if any.
// Returns true if we should perform the specified pass, false if we shouldn't.
static bool check_limit(ast_t** astp, pass_opt_t* options, pass_id pass,
  pass_id last_pass)
{
  pony_assert(astp != NULL);
  pony_assert(*astp != NULL);
  pony_assert(options != NULL);

  if(last_pass < pass || options->limit < pass)
    return false;

  if(ast_id(*astp) == TK_PROGRAM) // Update pass for catching up to
    options->program_pass = pass;

  return true;
}


// Perform an ast_visit pass, after checking the pass limits.
// Returns true to continue, false to stop processing and return the value in
// out_r.
static bool visit_pass(ast_t** astp, pass_opt_t* options, pass_id last_pass,
  bool* out_r, pass_id pass, ast_visit_t pre_fn, ast_visit_t post_fn)
{
  pony_assert(out_r != NULL);

  if(!check_limit(astp, options, pass, last_pass))
  {
    *out_r = true;
    return false;
  }

  //fprintf(stderr, "Pass %s (last %s) on %s\n", pass_name(pass),
  //  pass_name(last_pass), ast_get_print(*astp));

  if(ast_visit(astp, pre_fn, post_fn, options, pass) != AST_OK)
  {
    *out_r = false;
    return false;
  }

  return true;
}


bool module_passes(ast_t* package, pass_opt_t* options, source_t* source)
{
  // module_passes runs once per module, and the timers accumulate by
  // (package, pass), so a package's modules fold into one row per pass.
  const char* pkg = package_qualified_name(package);

  pass_timers_start(options->timers, pkg, pass_name(PASS_PARSE));
  bool parsed = pass_parse(package, source, options->check.errors,
    options->strtab, options->allow_test_symbols, options->parse_trace);
  pass_timers_stop(options->timers, pkg, pass_name(PASS_PARSE));

  if(!parsed)
    return false;

  if(options->limit < PASS_SYNTAX)
    return true;

  ast_t* module = ast_child(package);

  pass_timers_start(options->timers, pkg, pass_name(PASS_SYNTAX));
  ast_result_t r = ast_visit(&module, pass_syntax, NULL, options, PASS_SYNTAX);
  pass_timers_stop(options->timers, pkg, pass_name(PASS_SYNTAX));

  if(r != AST_OK)
    return false;

  if(options->check_tree)
    check_tree(module, options);
  return true;
}


// Peform the AST passes on the given AST up to the specified last pass
static bool ast_passes(ast_t** astp, pass_opt_t* options, pass_id last)
{
  pony_assert(astp != NULL);
  bool r;
  bool is_program = ast_id(*astp) == TK_PROGRAM;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_SYNTAX);

  if(!visit_pass(astp, options, last, &r, PASS_SUGAR, pass_sugar,
    pass_sugar_post))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_SUGAR);

  if(options->check_tree)
    check_tree(*astp, options);

  if(!visit_pass(astp, options, last, &r, PASS_SCOPE, pass_scope, NULL))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_SCOPE);

  if(!visit_pass(astp, options, last, &r, PASS_IMPORT, pass_import, NULL))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_IMPORT);

  if(!visit_pass(astp, options, last, &r, PASS_NAME_RESOLUTION, NULL,
    pass_names))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_NAME_RESOLUTION);

  if(!visit_pass(astp, options, last, &r, PASS_TYPEALIAS_RECURSION,
    pass_typealias_recursion, NULL))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_TYPEALIAS_RECURSION);

  if(!visit_pass(astp, options, last, &r, PASS_FLATTEN, NULL, pass_flatten))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_FLATTEN);

  if(!visit_pass(astp, options, last, &r, PASS_TRAITS, pass_traits, NULL))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_TRAITS);

  if(!visit_pass(astp, options, last, &r, PASS_REFER, pass_pre_refer,
    pass_refer))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_REFER);

  if(is_program && options->jobs != 1)
  {
    if(!check_limit(astp, options, PASS_EXPR, last))
      return true;

    if(!parallel_expr(*astp, options))
      return false;
  }
  else
  {
    if(!visit_pass(astp, options, last, &r, PASS_EXPR, pass_pre_expr,
      pass_expr))
      return r;
  }

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_EXPR);

  if(!visit_pass(astp, options, last, &r, PASS_COMPLETENESS, NULL,
    pass_completeness))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_COMPLETENESS);

  if(!visit_pass(astp, options, last, &r, PASS_VERIFY, NULL, pass_verify))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_VERIFY);

  if(!check_limit(astp, options, PASS_FINALISER, last))
    return true;

  if(!pass_finalisers(*astp, options))
    return false;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_FINALISER);

  // Freezing the AST is the last step of the AST passes. Tools that limit
  // compilation to the finaliser pass inspect and mutate the AST afterwards,
  // so it must stay unfrozen unless compilation proceeds into reach.
  if(!check_limit(astp, options, PASS_REACH, last))
    return true;

  if(options->check_tree)
    check_tree(*astp, options);

  ast_freeze(*astp);

  if(is_program)
  {
    if(options->verbosity >= VERBOSITY_TOOL_INFO)
      program_dump(*astp);
  }

  return true;
}


bool ast_passes_program(ast_t* ast, pass_opt_t* options)
{
  if(!ast_passes(&ast, options, PASS_ALL))
    return false;

  if(options->limit >= PASS_C)
  {
    if(!genexport_header(ast, options))
      return false;
  }

  // PASS_C is not an AST pass: it compiles each package's C shim sources
  // with the embedded clang, recording the objects on the program for the
  // link. It runs here, on the shared side of the REACH boundary, so both
  // real builds and the test harness reach it through this single call
  // site, and so clang errors fail the build before codegen starts.
  // Front-end tools stop at limit <= PASS_FINALISER and never invoke clang.
  if(options->limit >= PASS_C)
    return gencshim(ast, options);

  return true;
}


bool ast_passes_type(ast_t** astp, pass_opt_t* options, pass_id last_pass)
{
  ast_t* ast = *astp;

  pony_assert(ast_id(ast) == TK_ACTOR || ast_id(ast) == TK_CLASS ||
    ast_id(ast) == TK_STRUCT || ast_id(ast) == TK_PRIMITIVE ||
    ast_id(ast) == TK_TRAIT || ast_id(ast) == TK_INTERFACE);

  // We don't have the right frame stack for an entity, set up appropriate
  // frames
  ast_t* module = ast_parent(ast);
  ast_t* package = ast_parent(module);

  frame_push(&options->check, NULL);
  frame_push(&options->check, package);
  frame_push(&options->check, module);

  bool ok = ast_passes(astp, options, last_pass);

  frame_pop(&options->check);
  frame_pop(&options->check);
  frame_pop(&options->check);

  return ok;
}


bool ast_passes_subtree(ast_t** astp, pass_opt_t* options, pass_id last_pass)
{
  return ast_passes(astp, options, last_pass);
}


bool generate_passes(ast_t* program, pass_opt_t* options)
{
  if(options->limit < PASS_REACH)
    return true;

  return codegen(program, options);
}


void ast_pass_record(ast_t* ast, pass_id pass)
{
  pony_assert(ast != NULL);

  if(pass == PASS_ALL)
    return;

  ast_clearflag(ast, AST_FLAG_PASS_MASK);
  ast_setflag(ast, (int)pass);
}


ast_result_t ast_visit(ast_t** ast, ast_visit_t pre, ast_visit_t post,
  pass_opt_t* options, pass_id pass)
{
  pony_assert(ast != NULL);
  pony_assert(*ast != NULL);

  pass_id ast_pass = (pass_id)ast_checkflag(*ast, AST_FLAG_PASS_MASK);

  if(ast_pass >= pass)  // This pass already done for this AST node
    return AST_OK;

  // Do not process this subtree
  if((pass > PASS_SYNTAX) && ast_checkflag(*ast, AST_FLAG_PRESERVE))
    return AST_OK;

  typecheck_t* t = &options->check;
  bool pop = frame_push(t, *ast);

  // Per-package front-end timing. A package subtree is entered once per pass,
  // so wrapping its whole visit attributes that pass's work on the package. The
  // span is inclusive rather than exclusive: use_package loads a dependency
  // from the scope pass, so the dependency's parse and syntax run inside this
  // package's scope span and are counted in both rows.
  //
  // The context is tested before the node id because this runs on every AST
  // node and ast_id is an out-of-line call, so a build without --pass-timings
  // pays one predictable branch.
  //
  // The name is captured here because pre/post may replace *ast;
  // package_qualified_name returns an interned pointer that outlives this call.
  bool time_pkg = (options->timers != NULL) && (ast_id(*ast) == TK_PACKAGE);
  const char* pkg_qname = time_pkg ? package_qualified_name(*ast) : NULL;

  if(time_pkg)
    pass_timers_start(options->timers, pkg_qname, pass_name(pass));

  ast_result_t ret = AST_OK;
  bool ignore = false;

  if(pre != NULL)
  {
    switch(pre(ast, options))
    {
      case AST_OK:
        break;

      case AST_IGNORE:
        ignore = true;
        break;

      case AST_ERROR:
        ret = AST_ERROR;
        break;

      case AST_FATAL:
        goto fatal;
    }
  }

  if(!ignore && ((pre != NULL) || (post != NULL)))
  {
    ast_t* child = ast_child(*ast);

    while(child != NULL)
    {
      switch(ast_visit(&child, pre, post, options, pass))
      {
        case AST_OK:
          break;

        case AST_IGNORE:
          // Can never happen
          pony_assert(0);
          break;

        case AST_ERROR:
          ret = AST_ERROR;
          break;

        case AST_FATAL:
          goto fatal;
      }

      child = ast_sibling(child);
    }
  }

  if(!ignore && post != NULL)
  {
    switch(post(ast, options))
    {
      case AST_OK:
      case AST_IGNORE:
        break;

      case AST_ERROR:
        ret = AST_ERROR;
        break;

      case AST_FATAL:
        goto fatal;
    }
  }

  if(time_pkg)
    pass_timers_stop(options->timers, pkg_qname, pass_name(pass));

  if(pop)
    frame_pop(t);

  ast_pass_record(*ast, pass);
  return ret;

  // Shared by the three fatal arms above, so that stopping the timer cannot be
  // missed by a return added later.
fatal:
  if(time_pkg)
    pass_timers_stop(options->timers, pkg_qname, pass_name(pass));

  ast_pass_record(*ast, pass);

  if(pop)
    frame_pop(t);

  return AST_FATAL;
}


ast_result_t ast_visit_scope(ast_t** ast, ast_visit_t pre, ast_visit_t post,
  pass_opt_t* options, pass_id pass)
{
  typecheck_t* t = &options->check;
  ast_t* module = ast_nearest(*ast, TK_MODULE);
  ast_t* package = ast_parent(module);
  pony_assert(module != NULL);
  pony_assert(package != NULL);

  frame_push(t, NULL);
  frame_push(t, package);
  frame_push(t, module);

  ast_result_t ret = ast_visit(ast, pre, post, options, pass);

  frame_pop(t);
  frame_pop(t);
  frame_pop(t);

  return ret;
}
