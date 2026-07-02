#include "program.h"
#include "package.h"
#include "../ast/stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <string.h>


// Per program state.
typedef struct program_t
{
  package_group_list_t* package_groups;
  uint32_t next_package_id;
  strlist_t* libpaths;
  strlist_t* libs;
  // C shim objects emitted by gencshim, in package-walk order (deterministic).
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
  p->package_groups = NULL;
  p->next_package_id = 0;
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

  return p;
}


void program_free(program_t* program)
{
  pony_assert(program != NULL);

  package_group_list_free(program->package_groups);

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


uint32_t program_assign_pkg_id(ast_t* ast)
{
  pony_assert(ast != NULL);
  pony_assert(ast_id(ast) == TK_PROGRAM);

  program_t* data = (program_t*)ast_data(ast);
  pony_assert(data != NULL);

  return data->next_package_id++;
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
    ast_t* pkg_ast = ast_nearest(use, TK_PACKAGE);
    prefix = package_path(pkg_ast);
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
  for(strlist_t* p = opt->package_search_paths; p != NULL; p = strlist_next(p))
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
  for(strlist_t* p = opt->package_search_paths; p != NULL; p = strlist_next(p))
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

    for(strlist_t* p = opt->package_search_paths; p != NULL;
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

  if(data->package_groups == NULL)
  {
    ast_t* first_package = ast_child(program);
    pony_assert(first_package != NULL);
    data->package_groups = package_dependency_groups(first_package);
  }

  size_t i = 0;
  package_group_list_t* iter = data->package_groups;

  while(iter != NULL)
  {
    printf("Group " __zu "\n", i);
    package_group_t* group = package_group_list_data(iter);
    package_group_dump(group);
    putchar('\n');
    iter = package_group_list_next(iter);
    i++;
  }
}
