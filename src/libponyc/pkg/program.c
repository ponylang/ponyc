#include "program.h"
#include "../ast/stringtab.h"
#include <string.h>
#include <assert.h>


// Per program state
typedef struct program_t
{
  uint32_t next_package_id;
  strlist_t* libs;
  size_t lib_args_size;
  size_t lib_args_alloced;
  char* lib_args;
} program_t;


// Append the given text to the program's lib args, handling reallocs
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
  p->libs = NULL;
  p->lib_args_size = -1;
  p->lib_args = NULL;

  return p;
}


void program_free(program_t* program)
{
  assert(program != NULL);

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

/// Process a "lib:" scheme use command
bool use_library(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options)
{
  assert(use != NULL);
  assert(locator != NULL);
  (void)name;
  (void)options;

  if(strchr(locator, ' ') != NULL || strchr(locator, '\t') != NULL ||
    strchr(locator, '\r') != NULL || strchr(locator, '\n') != NULL)
  {
    ast_error(use, "lib names cannot contain whitespace");
    return false;
  }

  const char* libname = stringtab(locator);

  ast_t* p = ast_nearest(use, TK_PROGRAM);
  assert(p != NULL);

  program_t* prog = (program_t*)ast_data(p);
  assert(prog != NULL);
  assert(prog->lib_args == NULL); // Not yet built args

  if(strlist_find(prog->libs, libname) != NULL) // Ignore duplicate
    return true;

  prog->libs = strlist_append(prog->libs, libname);
  return true;
}


void program_lib_build_args(ast_t* program,
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

  if(data->libs == NULL)
  {
    // No libs, don't need to build anything;
    data->lib_args = (char*)calloc(1, sizeof(char));
    data->lib_args_size = 0;
    return;
  }

  // Start with an arbitrary amount of space
  data->lib_args_alloced = 2;
  data->lib_args = (char*)malloc(data->lib_args_alloced);
  data->lib_args[0] = '\0';
  data->lib_args_size = 0;

  append_to_args(data, " ");
  append_to_args(data, global_preamble);

  for(strlist_t* p = data->libs; p != NULL; p = strlist_next(p))
  {
    append_to_args(data, lib_premable);
    append_to_args(data, strlist_data(p));
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
