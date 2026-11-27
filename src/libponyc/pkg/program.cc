#include "program.h"
#include "package.h"
#include "../ast/stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <string.h>


// An inherited method being checked, and the error count when its check
// began.
typedef struct copy_check_t
{
  ast_t* entity;
  ast_t* method;
  size_t count;
  struct copy_check_t* next;
} copy_check_t;

// The type and method whose inherited copy held the literal a class was made
// from.
typedef struct anon_host_t
{
  ast_t* anon;
  ast_t* entity;
  ast_t* method;
  ast_t* site;
  struct anon_host_t* next;
} anon_host_t;

// A trait method and its template: a closed copy taken when the trait's
// methods were complete and bound where written, before any check of them.
typedef struct template_t
{
  ast_t* method;
  ast_t* copy;
} template_t;

static size_t template_hash(template_t* t)
{
  return ponyint_hash_ptr(t->method);
}

static bool template_cmp(template_t* a, template_t* b)
{
  return a->method == b->method;
}

static void template_free(template_t* t)
{
  ast_free(t->copy);
  POOL_FREE(template_t, t);
}

DECLARE_HASHMAP(template_map, template_map_t, template_t);
DEFINE_HASHMAP(template_map, template_map_t, template_t, template_hash,
  template_cmp, template_free);

// The class a literal written in trait text makes in its trait's own check,
// keyed by where the literal is written, with the type parameters that class
// was made from (the trait's and its method's), in its own parameters' order.
typedef struct literal_link_t
{
  source_t* source;
  size_t line;
  size_t pos;
  ast_t* def;
  ast_t* origins;
  struct literal_link_t* next;
} literal_link_t;

// Per program state.
typedef struct program_t
{
  module_group_list_t* module_groups;
  uint32_t next_module_id;
  strlist_t* libpaths;
  strlist_t* libs;
  // C shim objects emitted by gencshim, in module-walk order (deterministic).
  // The platform linkers append these to the link line in this order.
  // gencshim_done makes gencshim idempotent: AST passes are re-entry-guarded per
  // node, but a resumed ast_passes_program would otherwise recompile every
  // shim and append its objects to this list a second time.
  strlist_t* c_objects;
  bool gencshim_done;
  size_t lib_args_size;
  size_t lib_args_alloced;
  char* lib_args;

  // Embedded LLD support (set by program_lib_build_args_embedded)
  const char** embedded_paths;
  size_t embedded_path_count;
  const char** embedded_libs;
  size_t embedded_lib_count;

  // Scheduling of anonymous classes made from literals in trait text: the
  // traits and interfaces whose expr pass is running (innermost first), the
  // classes waiting for those checks to finish, in the order they were made,
  // and whether those classes are being finished now.
  astlist_t* trait_checks;
  astlist_t* deferred;
  bool draining;

  copy_check_t* copy_checks;
  anon_host_t* anon_hosts;

  // Parameters whose default argument is being checked, innermost first.
  astlist_t* default_checks;

  template_map_t templates;

  // Error counts when the running trait checks began (innermost first), and
  // the traits whose checks reported errors.
  astlist_t* trait_check_counts;
  astlist_t* failed_traits;

  literal_link_t* literal_links;

  // Classes made from literals in inherited copies that later copies of the
  // same literal can share (no fields, no type parameters).
  literal_link_t* copy_literals;

  // One renamed copy of each method type parameter (reify.c).
  literal_link_t* alpha_typeparams;

  // Ranges [start, end) of error indexes that get no copy note (see
  // program_error_without_copy_note), as start/end pairs.
  size_t* check_ranges;
  size_t check_range_count;
  size_t check_range_alloc;
} program_t;


// Append the given text to the program's lib args, handling reallocs.
static void append_to_args(program_t* program, const char* text)
{
  pony_assert(program != NULL);
  pony_assert(text != NULL);

  size_t text_len = strlen(text);
  size_t new_len = program->lib_args_size + text_len + 1; // +1 for terminator

  if(new_len > program->lib_args_alloced)
  {
    size_t new_alloc = 2 * new_len; // 2* so there's spare space for next arg
    char* new_args = (char*)ponyint_pool_alloc_size(new_alloc);
    memcpy(new_args, program->lib_args, program->lib_args_size + 1);
    ponyint_pool_free_size(program->lib_args_alloced, program->lib_args);
    program->lib_args = new_args;
    program->lib_args_alloced = new_alloc;
  }

  strcat(program->lib_args, text);
  program->lib_args_size = new_len - 1; // Don't add terminator to length
}


program_t* program_create()
{
  program_t* p = POOL_ALLOC(program_t);
  p->module_groups = NULL;
  p->next_module_id = 0;
  p->libpaths = NULL;
  p->libs = NULL;
  p->c_objects = NULL;
  p->gencshim_done = false;
  p->lib_args_size = -1;
  p->lib_args = NULL;

  p->embedded_paths = NULL;
  p->embedded_path_count = 0;
  p->embedded_libs = NULL;
  p->embedded_lib_count = 0;

  p->trait_checks = NULL;
  p->deferred = NULL;
  p->draining = false;
  p->copy_checks = NULL;
  p->anon_hosts = NULL;
  p->default_checks = NULL;
  template_map_init(&p->templates, 64);
  p->trait_check_counts = NULL;
  p->failed_traits = NULL;
  p->literal_links = NULL;
  p->copy_literals = NULL;
  p->alpha_typeparams = NULL;
  p->check_ranges = NULL;
  p->check_range_count = 0;
  p->check_range_alloc = 0;

  return p;
}


void program_free(program_t* program)
{
  pony_assert(program != NULL);

  module_group_list_free(program->module_groups);
  astlist_free(program->trait_checks);
  astlist_free(program->deferred);
  astlist_free(program->default_checks);
  template_map_destroy(&program->templates);
  astlist_free(program->trait_check_counts);
  astlist_free(program->failed_traits);

  if(program->check_ranges != NULL)
    ponyint_pool_free_size(program->check_range_alloc * sizeof(size_t),
      program->check_ranges);

  while(program->alpha_typeparams != NULL)
  {
    literal_link_t* next = program->alpha_typeparams->next;
    ast_free(program->alpha_typeparams->def);
    POOL_FREE(literal_link_t, program->alpha_typeparams);
    program->alpha_typeparams = next;
  }

  while(program->copy_literals != NULL)
  {
    literal_link_t* next = program->copy_literals->next;
    ast_free(program->copy_literals->origins);
    POOL_FREE(literal_link_t, program->copy_literals);
    program->copy_literals = next;
  }

  while(program->literal_links != NULL)
  {
    literal_link_t* next = program->literal_links->next;
    ast_free(program->literal_links->origins);
    POOL_FREE(literal_link_t, program->literal_links);
    program->literal_links = next;
  }

  while(program->copy_checks != NULL)
  {
    copy_check_t* next = program->copy_checks->next;
    POOL_FREE(copy_check_t, program->copy_checks);
    program->copy_checks = next;
  }

  while(program->anon_hosts != NULL)
  {
    anon_host_t* next = program->anon_hosts->next;
    POOL_FREE(anon_host_t, program->anon_hosts);
    program->anon_hosts = next;
  }

  strlist_free(program->libpaths);
  strlist_free(program->libs);
  strlist_free(program->c_objects);

  if(program->lib_args != NULL)
    ponyint_pool_free_size(program->lib_args_alloced, program->lib_args);

  if(program->embedded_paths != NULL)
    ponyint_pool_free_size(
      program->embedded_path_count * sizeof(const char*),
      program->embedded_paths);

  if(program->embedded_libs != NULL)
    ponyint_pool_free_size(
      program->embedded_lib_count * sizeof(const char*),
      program->embedded_libs);

  POOL_FREE(program_t, program);
}


uint32_t program_assign_module_id(ast_t* ast)
{
  pony_assert(ast != NULL);
  pony_assert(ast_id(ast) == TK_PROGRAM);

  program_t* data = (program_t*)ast_data(ast);
  pony_assert(data != NULL);

  return data->next_module_id++;
}

#if defined(PLATFORM_IS_WINDOWS)
#define INVALID_LOCATOR_CHARS "\t\r\n\"'`;$|&<>%*?[]{}"
#else
#define INVALID_LOCATOR_CHARS "\t\r\n\"'`;$|&<>%*?[]{}()"
#endif

static const char* quoted_locator(pass_opt_t* opt, ast_t* use,
  const char* locator)
{
  pony_assert(locator != NULL);

  if(strpbrk(locator, INVALID_LOCATOR_CHARS) != NULL)
  {
    if(use != NULL)
      ast_error(opt->check.errors, use, "use URI contains invalid characters");

    return NULL;
  }

  size_t len = strlen(locator);
  char* quoted = (char*)ponyint_pool_alloc_size(len + 3);
  quoted[0] = '"';
  memcpy(quoted + 1, locator, len);
  quoted[len + 1] = '"';
  quoted[len + 2] = '\0';

  return stringtab_consume(opt->strtab, quoted, len + 3);
}

/// Process a "lib:" scheme use command.
bool use_library(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options)
{
  (void)name;

  const char* libname = quoted_locator(options, use, locator);

  if(libname == NULL)
    return false;

  ast_t* p = ast_nearest(use, TK_PROGRAM);
  program_t* prog = (program_t*)ast_data(p);
  pony_assert(prog->lib_args == NULL); // Not yet built args

  if(strlist_find(prog->libs, libname) != NULL) // Ignore duplicate
    return true;

  prog->libs = strlist_append(prog->libs, libname);
  return true;
}


/// Process a "path:" scheme use command.
bool use_path(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options)
{
  (void)name;
  char absolute[FILENAME_MAX];
  const char* prefix = NULL;

  if(!is_path_absolute(locator)) {
    ast_t* pkg_ast = ast_nearest(use, TK_MODULE);
    prefix = module_path(pkg_ast);
  }

  path_cat(prefix, locator, absolute);
  const char* libpath = quoted_locator(options, use, absolute);

  if(libpath == NULL)
    return false;

  ast_t* prog_ast = ast_nearest(use, TK_PROGRAM);
  program_t* prog = (program_t*)ast_data(prog_ast);
  pony_assert(prog->lib_args == NULL); // Not yet built args

  if(strlist_find(prog->libpaths, libpath) != NULL) // Ignore duplicate
    return true;

  prog->libpaths = strlist_append(prog->libpaths, libpath);
  return true;
}


void program_lib_build_args(ast_t* program, pass_opt_t* opt,
  const char* path_preamble, const char* rpath_preamble,
  const char* global_preamble, const char* global_postamble,
  const char* lib_premable, const char* lib_postamble)
{
  pony_assert(program != NULL);
  pony_assert(ast_id(program) == TK_PROGRAM);
  pony_assert(global_preamble != NULL);
  pony_assert(global_postamble != NULL);
  pony_assert(lib_premable != NULL);
  pony_assert(lib_postamble != NULL);

  program_t* data = (program_t*)ast_data(program);
  pony_assert(data != NULL);
  pony_assert(data->lib_args == NULL); // Not yet built args
  pony_assert(data->embedded_paths == NULL); // Mutually exclusive

  // Start with an arbitrary amount of space
  data->lib_args_alloced = 256;
  data->lib_args = (char*)ponyint_pool_alloc_size(data->lib_args_alloced);
  data->lib_args[0] = '\0';
  data->lib_args_size = 0;

  // Library paths defined in the source code.
  for(strlist_t* p = data->libpaths; p != NULL; p = strlist_next(p))
  {
    const char* libpath = strlist_data(p);
    append_to_args(data, path_preamble);
    append_to_args(data, libpath);
    append_to_args(data, " ");

    if(rpath_preamble != NULL)
    {
      append_to_args(data, rpath_preamble);
      append_to_args(data, libpath);
      append_to_args(data, " ");
    }
  }

  // Library paths from the command line and environment variable.
  for(strlist_t* p = opt->module_search_paths; p != NULL; p = strlist_next(p))
  {
    const char* libpath = quoted_locator(opt, NULL, strlist_data(p));

    if(libpath == NULL)
      continue;

    append_to_args(data, path_preamble);
    append_to_args(data, libpath);
    append_to_args(data, " ");

    if(rpath_preamble != NULL)
    {
      append_to_args(data, rpath_preamble);
      append_to_args(data, libpath);
      append_to_args(data, " ");
    }
  }

  // Library names.
  append_to_args(data, global_preamble);

  for(strlist_t* p = data->libs; p != NULL; p = strlist_next(p))
  {
    const char* lib = strlist_data(p);
    bool amble = !is_path_absolute(&lib[1]);

    if(amble)
      append_to_args(data, lib_premable);

    append_to_args(data, lib);

    if(amble)
      append_to_args(data, lib_postamble);

    append_to_args(data, " ");
  }

  append_to_args(data, global_postamble);
}


const char* program_lib_args(ast_t* program)
{
  pony_assert(program != NULL);
  pony_assert(ast_id(program) == TK_PROGRAM);

  program_t* data = (program_t*)ast_data(program);
  pony_assert(data != NULL);
  pony_assert(data->lib_args != NULL); // Args have been built

  return data->lib_args;
}


// Strip the surrounding quotes added by quoted_locator().
static const char* unquote(const char* quoted, pass_opt_t* opt)
{
  pony_assert(quoted != NULL);

  size_t len = strlen(quoted);
  pony_assert(len >= 2);
  pony_assert(quoted[0] == '"');
  pony_assert(quoted[len - 1] == '"');

  size_t unquoted_len = len - 2;
  char* buf = (char*)ponyint_pool_alloc_size(unquoted_len + 1);
  memcpy(buf, quoted + 1, unquoted_len);
  buf[unquoted_len] = '\0';

  return stringtab_consume(opt->strtab, buf, unquoted_len + 1);
}


void program_lib_build_args_embedded(ast_t* program, pass_opt_t* opt)
{
  pony_assert(program != NULL);
  pony_assert(ast_id(program) == TK_PROGRAM);

  program_t* data = (program_t*)ast_data(program);
  pony_assert(data != NULL);
  pony_assert(data->lib_args == NULL); // Mutually exclusive
  pony_assert(data->embedded_paths == NULL); // Not yet built

  // Count valid paths from source code and CLI/PONYPATH.
  // Two-pass: first count valid entries, then allocate exactly.
  size_t path_count = 0;
  for(strlist_t* p = data->libpaths; p != NULL; p = strlist_next(p))
    path_count++;
  for(strlist_t* p = opt->module_search_paths; p != NULL; p = strlist_next(p))
  {
    if(quoted_locator(opt, NULL, strlist_data(p)) != NULL)
      path_count++;
  }

  // Allocate and populate path array.
  if(path_count > 0)
  {
    data->embedded_paths = (const char**)ponyint_pool_alloc_size(
      path_count * sizeof(const char*));
    data->embedded_path_count = path_count;

    size_t i = 0;
    for(strlist_t* p = data->libpaths; p != NULL; p = strlist_next(p))
      data->embedded_paths[i++] = unquote(strlist_data(p), opt);

    for(strlist_t* p = opt->module_search_paths; p != NULL;
      p = strlist_next(p))
    {
      const char* quoted = quoted_locator(opt, NULL, strlist_data(p));
      if(quoted != NULL)
        data->embedded_paths[i++] = unquote(quoted, opt);
    }
  }

  // Count libraries.
  size_t lib_count = 0;
  for(strlist_t* p = data->libs; p != NULL; p = strlist_next(p))
    lib_count++;

  // Allocate and populate library array.
  if(lib_count > 0)
  {
    data->embedded_libs = (const char**)ponyint_pool_alloc_size(
      lib_count * sizeof(const char*));
    data->embedded_lib_count = lib_count;

    size_t i = 0;
    for(strlist_t* p = data->libs; p != NULL; p = strlist_next(p))
      data->embedded_libs[i++] = unquote(strlist_data(p), opt);
  }
}


bool program_gencshim_done(ast_t* program)
{
  pony_assert(program != NULL);
  pony_assert(ast_id(program) == TK_PROGRAM);

  program_t* data = (program_t*)ast_data(program);
  pony_assert(data != NULL);

  return data->gencshim_done;
}


void program_set_gencshim_done(ast_t* program)
{
  pony_assert(program != NULL);
  pony_assert(ast_id(program) == TK_PROGRAM);

  program_t* data = (program_t*)ast_data(program);
  pony_assert(data != NULL);

  data->gencshim_done = true;
}


// Unlike the embedded_libs accessors these walk the strlist (O(count) /
// O(index)), a deliberate divergence: shim counts are small (one entry per
// .c file in the program) and the list never converts to an array.
void program_add_c_object(ast_t* program, const char* path)
{
  pony_assert(program != NULL);
  pony_assert(ast_id(program) == TK_PROGRAM);
  pony_assert(path != NULL);

  program_t* data = (program_t*)ast_data(program);
  pony_assert(data != NULL);

  data->c_objects = strlist_append(data->c_objects, path);
}


size_t program_c_object_count(ast_t* program)
{
  pony_assert(program != NULL);
  pony_assert(ast_id(program) == TK_PROGRAM);

  program_t* data = (program_t*)ast_data(program);
  pony_assert(data != NULL);

  size_t count = 0;

  for(strlist_t* p = data->c_objects; p != NULL; p = strlist_next(p))
    count++;

  return count;
}


const char* program_c_object_at(ast_t* program, size_t index)
{
  pony_assert(program != NULL);
  pony_assert(ast_id(program) == TK_PROGRAM);

  program_t* data = (program_t*)ast_data(program);
  pony_assert(data != NULL);

  strlist_t* p = data->c_objects;

  for(size_t i = 0; i < index; i++)
  {
    pony_assert(p != NULL);
    p = strlist_next(p);
  }

  pony_assert(p != NULL);
  return strlist_data(p);
}


size_t program_lib_path_count(ast_t* program)
{
  pony_assert(program != NULL);
  pony_assert(ast_id(program) == TK_PROGRAM);

  program_t* data = (program_t*)ast_data(program);
  pony_assert(data != NULL);

  return data->embedded_path_count;
}


const char* program_lib_path_at(ast_t* program, size_t index)
{
  pony_assert(program != NULL);
  pony_assert(ast_id(program) == TK_PROGRAM);

  program_t* data = (program_t*)ast_data(program);
  pony_assert(data != NULL);
  pony_assert(index < data->embedded_path_count);

  return data->embedded_paths[index];
}


size_t program_lib_count(ast_t* program)
{
  pony_assert(program != NULL);
  pony_assert(ast_id(program) == TK_PROGRAM);

  program_t* data = (program_t*)ast_data(program);
  pony_assert(data != NULL);

  return data->embedded_lib_count;
}


const char* program_lib_at(ast_t* program, size_t index)
{
  pony_assert(program != NULL);
  pony_assert(ast_id(program) == TK_PROGRAM);

  program_t* data = (program_t*)ast_data(program);
  pony_assert(data != NULL);
  pony_assert(index < data->embedded_lib_count);

  return data->embedded_libs[index];
}


void program_dump(ast_t* program)
{
  pony_assert(program != NULL);
  pony_assert(ast_id(program) == TK_PROGRAM);

  program_t* data = (program_t*)ast_data(program);
  pony_assert(data != NULL);

  if(data->module_groups == NULL)
  {
    ast_t* first_module = ast_child(program);
    pony_assert(first_module != NULL);
    data->module_groups = module_dependency_groups(first_module);
  }

  size_t i = 0;
  module_group_list_t* iter = data->module_groups;

  while(iter != NULL)
  {
    printf("Group " __zu "\n", i);
    module_group_t* group = module_group_list_data(iter);
    module_group_dump(group);
    putchar('\n');
    iter = module_group_list_next(iter);
    i++;
  }
}


static program_t* program_of(ast_t* ast)
{
  ast_t* program = ast_nearest(ast, TK_PROGRAM);
  pony_assert(program != NULL);
  return (program_t*)ast_data(program);
}


void program_trait_check_begin(ast_t* entity, size_t error_count)
{
  program_t* p = program_of(entity);
  p->trait_checks = astlist_push(p->trait_checks, entity);
  // The list holds node pointers, so the count is stored cast to one.
  p->trait_check_counts = astlist_push(p->trait_check_counts,
    (ast_t*)(uintptr_t)error_count);
}


void program_trait_check_end(ast_t* entity, size_t error_count)
{
  program_t* p = program_of(entity);
  ast_t* top = NULL;
  p->trait_checks = astlist_pop(p->trait_checks, &top);
  pony_assert(top == entity);
  (void)top;

  ast_t* count = NULL;
  p->trait_check_counts = astlist_pop(p->trait_check_counts, &count);
  size_t start = (size_t)(uintptr_t)count;

  if(start != error_count)
  {
    p->failed_traits = astlist_push(p->failed_traits, entity);

    program_add_unnoted_range(entity, start, error_count);
  }
}


void program_add_unnoted_range(ast_t* anchor, size_t start, size_t end)
{
  program_t* p = program_of(anchor);

  if(start == end)
    return;

  if(p->check_range_count + 2 > p->check_range_alloc)
  {
    size_t alloc = (p->check_range_alloc == 0) ? 16 :
      p->check_range_alloc * 2;
    size_t* ranges = (size_t*)ponyint_pool_alloc_size(alloc * sizeof(size_t));

    if(p->check_ranges != NULL)
    {
      memcpy(ranges, p->check_ranges, p->check_range_count * sizeof(size_t));
      ponyint_pool_free_size(p->check_range_alloc * sizeof(size_t),
        p->check_ranges);
    }

    p->check_ranges = ranges;
    p->check_range_alloc = alloc;
  }

  p->check_ranges[p->check_range_count++] = start;
  p->check_ranges[p->check_range_count++] = end;
}


bool program_error_without_copy_note(ast_t* anchor, size_t index)
{
  program_t* p = program_of(anchor);

  for(size_t i = 0; i < p->check_range_count; i += 2)
  {
    if((index >= p->check_ranges[i]) && (index < p->check_ranges[i + 1]))
      return true;
  }

  return false;
}


bool program_trait_check_failed(ast_t* entity)
{
  return astlist_find(program_of(entity)->failed_traits, entity) != NULL;
}


bool program_trait_check_in_progress(ast_t* entity)
{
  program_t* p = program_of(entity);
  return astlist_find(p->trait_checks, entity) != NULL;
}


bool program_any_trait_check(ast_t* ast)
{
  program_t* p = program_of(ast);
  return (p->trait_checks != NULL) || (p->default_checks != NULL);
}


void program_default_check_begin(ast_t* anchor, ast_t* param)
{
  program_t* p = program_of(anchor);
  p->default_checks = astlist_push(p->default_checks, param);
}


bool program_default_check_end(ast_t* anchor, ast_t* param)
{
  program_t* p = program_of(anchor);

  if((p->default_checks == NULL) ||
    (astlist_data(p->default_checks) != param))
    return false;

  ast_t* top = NULL;
  p->default_checks = astlist_pop(p->default_checks, &top);
  return true;
}


bool program_default_check_in_progress(ast_t* anchor, ast_t* param)
{
  return astlist_find(program_of(anchor)->default_checks, param) != NULL;
}


bool program_any_default_check(ast_t* anchor)
{
  return program_of(anchor)->default_checks != NULL;
}


astlist_t* program_default_checks(ast_t* anchor)
{
  return program_of(anchor)->default_checks;
}


void program_defer_class(ast_t* entity)
{
  program_t* p = program_of(entity);
  p->deferred = astlist_append(p->deferred, entity);
}


bool program_is_deferred(ast_t* entity)
{
  return astlist_find(program_of(entity)->deferred, entity) != NULL;
}


ast_t* program_next_deferred(ast_t* ast)
{
  program_t* p = program_of(ast);

  if(p->deferred == NULL)
    return NULL;

  ast_t* entity = NULL;
  p->deferred = astlist_pop(p->deferred, &entity);
  return entity;
}


bool program_draining(ast_t* ast)
{
  return program_of(ast)->draining;
}


void program_set_draining(ast_t* ast, bool draining)
{
  program_of(ast)->draining = draining;
}


void program_push_copy_check(ast_t* anchor, ast_t* entity, ast_t* method,
  size_t count)
{
  program_t* p = program_of(anchor);
  copy_check_t* c = POOL_ALLOC(copy_check_t);
  c->entity = entity;
  c->method = method;
  c->count = count;
  c->next = p->copy_checks;
  p->copy_checks = c;
}


bool program_pop_copy_check(ast_t* anchor, ast_t* method, ast_t** entity,
  size_t* count)
{
  program_t* p = program_of(anchor);
  copy_check_t* c = p->copy_checks;

  if((c == NULL) || (c->method != method))
    return false;

  *entity = c->entity;
  *count = c->count;
  p->copy_checks = c->next;
  POOL_FREE(copy_check_t, c);
  return true;
}


void program_set_anon_host(ast_t* anon, ast_t* entity, ast_t* method,
  ast_t* site)
{
  program_t* p = program_of(anon);
  anon_host_t* h = POOL_ALLOC(anon_host_t);
  h->anon = anon;
  h->entity = entity;
  h->method = method;
  h->site = site;
  h->next = p->anon_hosts;
  p->anon_hosts = h;
}


bool program_anon_host(ast_t* anon, ast_t** entity, ast_t** method)
{
  for(anon_host_t* h = program_of(anon)->anon_hosts; h != NULL; h = h->next)
  {
    if(h->anon == anon)
    {
      *entity = h->entity;
      *method = h->method;
      return true;
    }
  }

  return false;
}


ast_t* program_anon_site(ast_t* anon)
{
  for(anon_host_t* h = program_of(anon)->anon_hosts; h != NULL; h = h->next)
  {
    if(h->anon == anon)
      return h->site;
  }

  return NULL;
}


void program_set_template(ast_t* anchor, ast_t* method, ast_t* copy)
{
  program_t* p = program_of(anchor);
  template_t* t = POOL_ALLOC(template_t);
  t->method = method;
  t->copy = copy;
  template_map_put(&p->templates, t);
}


ast_t* program_template(ast_t* anchor, ast_t* method)
{
  program_t* p = program_of(anchor);
  template_t key;
  key.method = method;
  size_t index = HASHMAP_UNKNOWN;
  template_t* t = template_map_get(&p->templates, &key, &index);
  return (t == NULL) ? NULL : t->copy;
}


void program_set_literal_link(ast_t* literal, ast_t* def, ast_t* origins)
{
  program_t* p = program_of(def);

  // A literal's class is linked once, when the trait's own check makes it.
#ifndef PONY_NDEBUG
  ast_t* unused;
  pony_assert(program_literal_link(def, literal, &unused) == NULL);
#endif
  literal_link_t* l = POOL_ALLOC(literal_link_t);
  l->source = ast_source(literal);
  l->line = ast_line(literal);
  l->pos = ast_pos(literal);
  l->def = def;
  l->origins = origins;
  l->next = p->literal_links;
  p->literal_links = l;
}


void program_add_copy_literal(ast_t* literal, ast_t* def, ast_t* text)
{
  program_t* p = program_of(def);
  literal_link_t* l = POOL_ALLOC(literal_link_t);
  l->source = ast_source(literal);
  l->line = ast_line(literal);
  l->pos = ast_pos(literal);
  l->def = def;
  l->origins = text;
  l->next = p->copy_literals;
  p->copy_literals = l;
}


ast_t* program_next_copy_literal(ast_t* anchor, ast_t* literal, ast_t* after)
{
  bool past = (after == NULL);

  for(literal_link_t* l = program_of(anchor)->copy_literals; l != NULL;
    l = l->next)
  {
    if((l->source != ast_source(literal)) || (l->line != ast_line(literal)) ||
      (l->pos != ast_pos(literal)))
      continue;

    if(past)
      return l->def;

    if(l->def == after)
      past = true;
  }

  return NULL;
}


ast_t* program_alpha_typeparam(ast_t* typeparam)
{
  program_t* p = program_of(typeparam);

  for(literal_link_t* l = p->alpha_typeparams; l != NULL; l = l->next)
  {
    if(l->origins == typeparam)
      return l->def;
  }

  ast_t* alpha = ast_dup(typeparam);
  ast_setdata(alpha, alpha);

  literal_link_t* l = POOL_ALLOC(literal_link_t);
  l->source = NULL;
  l->line = 0;
  l->pos = 0;
  l->def = alpha;
  l->origins = typeparam;
  l->next = p->alpha_typeparams;
  p->alpha_typeparams = l;
  return alpha;
}


ast_t* program_copy_literal_text(ast_t* anchor, ast_t* def)
{
  for(literal_link_t* l = program_of(anchor)->copy_literals; l != NULL;
    l = l->next)
  {
    if(l->def == def)
      return l->origins;
  }

  return NULL;
}


ast_t* program_literal_link(ast_t* anchor, ast_t* literal, ast_t** origins)
{
  for(literal_link_t* l = program_of(anchor)->literal_links; l != NULL;
    l = l->next)
  {
    if((l->source == ast_source(literal)) && (l->line == ast_line(literal)) &&
      (l->pos == ast_pos(literal)))
    {
      *origins = l->origins;
      return l->def;
    }
  }

  return NULL;
}
