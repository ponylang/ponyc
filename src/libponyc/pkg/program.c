#include "program.h"
#include "package.h"
#include "../ast/stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <assert.h>


// Per program state.
typedef struct program_t
{
  uint32_t next_package_id;
  strlist_t* libpaths;
  strlist_t* libs;
  size_t lib_args_size;
  size_t lib_args_alloced;
  char* lib_args;
} program_t;


// Append the given text to the program's lib args, handling reallocs.
static void append_to_args(program_t* program, const char* text)
{
  assert(program != NULL);
  assert(text != NULL);

  size_t text_len = strlen(text);
  size_t new_len = program->lib_args_size + text_len + 1; // +1 for terminator

  if(new_len > program->lib_args_alloced)
  {
    size_t new_alloc = 2 * new_len; // 2* so there's spare space for next arg
    program->lib_args = (char*)realloc(program->lib_args, new_alloc);
    program->lib_args_alloced = new_alloc;
    assert(new_len <= program->lib_args_alloced);
  }

  strcat(program->lib_args, text);
  program->lib_args_size = new_len - 1; // Don't add terminator to length
}


program_t* program_create()
{
  program_t* p = (program_t*)malloc(sizeof(program_t));
  p->next_package_id = 0;
  p->libpaths = NULL;
  p->libs = NULL;
  p->lib_args_size = -1;
  p->lib_args = NULL;

  return p;
}


void program_free(program_t* program)
{
  assert(program != NULL);

  strlist_free(program->libpaths);
  strlist_free(program->libs);
  free(program->lib_args);
  free(program);
}


uint32_t program_assign_pkg_id(ast_t* ast)
{
  assert(ast != NULL);
  assert(ast_id(ast) == TK_PROGRAM);

  program_t* data = (program_t*)ast_data(ast);
  assert(data != NULL);

  return data->next_package_id++;
}

static const char* quoted_locator(ast_t* use, const char* locator)
{
  assert(locator != NULL);

  if(strpbrk(locator, "\t\r\n\"'`;$|&<>%*?\\[]{}()") != NULL)
  {
    if(use != NULL)
      ast_error(use, "use URI contains invalid characters");

    return NULL;
  }

  size_t len = strlen(locator);
  char* quoted = (char*)pool_alloc_size(len + 3);
  quoted[0] = '"';
  memcpy(quoted + 1, locator, len);
  quoted[len + 1] = '"';
  quoted[len + 2] = '\0';

  return stringtab_consume(quoted, len + 3);
}

/// Process a "lib:" scheme use command.
bool use_library(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options)
{
  (void)name;
  (void)options;

  const char* libname = quoted_locator(use, locator);

  if(libname == NULL)
    return false;

  ast_t* p = ast_nearest(use, TK_PROGRAM);
  program_t* prog = (program_t*)ast_data(p);
  assert(prog->lib_args == NULL); // Not yet built args

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
  (void)options;

  const char* libpath = quoted_locator(use, locator);

  if(libpath == NULL)
    return false;

  ast_t* p = ast_nearest(use, TK_PROGRAM);
  program_t* prog = (program_t*)ast_data(p);
  assert(prog->lib_args == NULL); // Not yet built args

  if(strlist_find(prog->libpaths, libpath) != NULL) // Ignore duplicate
    return true;

  prog->libpaths = strlist_append(prog->libpaths, libpath);
  return true;
}


void program_lib_build_args(ast_t* program, const char* path_preamble,
  const char* global_preamble, const char* global_postamble,
  const char* lib_premable, const char* lib_postamble)
{
  assert(program != NULL);
  assert(ast_id(program) == TK_PROGRAM);
  assert(global_preamble != NULL);
  assert(global_postamble != NULL);
  assert(lib_premable != NULL);
  assert(lib_postamble != NULL);

  program_t* data = (program_t*)ast_data(program);
  assert(data != NULL);
  assert(data->lib_args == NULL); // Not yet built args

  // Start with an arbitrary amount of space
  data->lib_args_alloced = 256;
  data->lib_args = (char*)malloc(data->lib_args_alloced);
  data->lib_args[0] = '\0';
  data->lib_args_size = 0;

  // Library paths defined in the source code.
  for(strlist_t* p = data->libpaths; p != NULL; p = strlist_next(p))
  {
    const char* libpath = strlist_data(p);
    append_to_args(data, path_preamble);
    append_to_args(data, libpath);
    append_to_args(data, " ");
  }

  // Library paths from the command line and environment variable.
  for(strlist_t* p = package_paths(); p != NULL; p = strlist_next(p))
  {
    const char* libpath = quoted_locator(NULL, strlist_data(p));

    if(libpath == NULL)
      continue;

    append_to_args(data, path_preamble);
    append_to_args(data, libpath);
    append_to_args(data, " ");
  }

  // Library names.
  append_to_args(data, global_preamble);

  for(strlist_t* p = data->libs; p != NULL; p = strlist_next(p))
  {
    const char* lib = strlist_data(p);
    bool amble = !is_path_absolute(lib);

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
  assert(program != NULL);
  assert(ast_id(program) == TK_PROGRAM);

  program_t* data = (program_t*)ast_data(program);
  assert(data != NULL);
  assert(data->lib_args != NULL); // Args have been built

  return data->lib_args;
}
