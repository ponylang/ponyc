#include "genlib.h"
#include "genopt.h"
#include "genobj.h"
#include "genheader.h"
#include "genprim.h"
#include "../reach/paint.h"
#include "../type/assemble.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <unistd.h>
#endif

static bool reachable_methods(compile_t* c, ast_t* ast)
{
  ast_t* id = ast_child(ast);
  ast_t* type = type_builtin(c->opt, ast, ast_name(id));

  ast_t* def = (ast_t*)ast_data(type);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_NEW:
      case TK_BE:
      case TK_FUN:
      {
        AST_GET_CHILDREN(member, cap, m_id, typeparams);

        // Mark all non-polymorphic methods as reachable.
        if(ast_id(typeparams) == TK_NONE)
          reach(c->reachable, &c->next_type_id, type, ast_name(m_id), NULL);
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  ast_free_unattached(type);
  return true;
}

static bool reachable_actors(compile_t* c, ast_t* program)
{
  printf("Library reachability\n");

  // Look for C-API actors in every package.
  bool found = false;
  ast_t* package = ast_child(program);

  while(package != NULL)
  {
    ast_t* module = ast_child(package);

    while(module != NULL)
    {
      ast_t* entity = ast_child(module);

      while(entity != NULL)
      {
        if(ast_id(entity) == TK_ACTOR)
        {
          ast_t* c_api = ast_childidx(entity, 5);

          if(ast_id(c_api) == TK_AT)
          {
            // We have an actor marked as C-API.
            if(!reachable_methods(c, entity))
              return false;

            found = true;
          }
        }

        entity = ast_sibling(entity);
      }

      module = ast_sibling(module);
    }

    package = ast_sibling(package);
  }

  if(!found)
  {
    errorf(NULL, "no C-API actors found in package '%s'", c->filename);
    return false;
  }

  printf("Selector painting\n");
  paint(c->reachable);
  return true;
}

static bool link_lib(compile_t* c, const char* file_o)
{
#if defined(PLATFORM_IS_POSIX_BASED)
  const char* file_lib = suffix_filename(c->opt->output, "lib", c->filename,
    ".a");
  printf("Archiving %s\n", file_lib);

  size_t len = 32 + strlen(file_lib) + strlen(file_o);
  char* cmd = (char*)ponyint_pool_alloc_size(len);

#if defined(PLATFORM_IS_MACOSX)
  snprintf(cmd, len, "/usr/bin/ar -rcs %s %s", file_lib, file_o);
#else
  snprintf(cmd, len, "ar -rcs %s %s", file_lib, file_o);
#endif

  if(system(cmd) != 0)
  {
    errorf(NULL, "unable to link: %s", cmd);
    ponyint_pool_free_size(len, cmd);
    return false;
  }

  ponyint_pool_free_size(len, cmd);
#elif defined(PLATFORM_IS_WINDOWS)
  const char* file_lib = suffix_filename(c->opt->output, "", c->filename,
    ".lib");
  printf("Archiving %s\n", file_lib);

  vcvars_t vcvars;

  if(!vcvars_get(&vcvars))
  {
    errorf(NULL, "unable to link: no vcvars");
    return false;
  }

  size_t len = 128 + strlen(file_lib) + strlen(file_o);
  char* cmd = (char*)ponyint_pool_alloc_size(len);

  snprintf(cmd, len, "cmd /C \"\"%s\" /NOLOGO /OUT:%s %s\"", vcvars.ar,
    file_lib, file_o);

  if(system(cmd) == -1)
  {
    errorf(NULL, "unable to link: %s", cmd);
    ponyint_pool_free_size(len, cmd);
    return false;
  }

  ponyint_pool_free_size(len, cmd);
#endif

  return true;
}

bool genlib(compile_t* c, ast_t* program)
{
  if(!reachable_actors(c, program) ||
    !gentypes(c) ||
    !genheader(c)
    )
    return false;

  if(!genopt(c))
    return false;

  const char* file_o = genobj(c);

  if(file_o == NULL)
    return false;

  if(c->opt->limit < PASS_ALL)
    return true;

  if(!link_lib(c, file_o))
    return false;

#ifdef PLATFORM_IS_WINDOWS
  _unlink(file_o);
#else
  unlink(file_o);
#endif

  return true;
}
