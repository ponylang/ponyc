#include "genlib.h"
#include "genobj.h"
#include "genheader.h"
#include "../reach/paint.h"
#include "../type/assemble.h"
#include <string.h>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <unistd.h>
#else
   //disable warnings of unlink being deprecated
#  pragma warning(disable:4996)
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
          reach(c->reachable, def, ast_name(m_id), NULL);
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

  paint(c->reachable);
  return true;
}

static bool generate_actor(compile_t* c, ast_t* ast)
{
  ast_t* id = ast_child(ast);
  ast_t* type = type_builtin(c->opt, ast, ast_name(id));

  if(type == NULL)
    return false;

  gentype_t g;
  bool ok = gentype(c, type, &g);
  ast_free_unattached(type);

  return ok;
}

static bool generate_actors(compile_t* c, ast_t* program)
{
  // Look for C-API actors in every package.
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
            if(!generate_actor(c, entity))
              return false;
          }
        }

        entity = ast_sibling(entity);
      }

      module = ast_sibling(module);
    }

    package = ast_sibling(package);
  }

  return true;
}

static bool link_lib(compile_t* c, const char* file_o)
{
  size_t len = strlen(c->filename);

#if defined(PLATFORM_IS_POSIX_BASED)
  VLA(char, libname, len + 4);
  memcpy(libname, "lib", 3);
  memcpy(libname + 3, c->filename, len + 1);

  const char* file_lib = suffix_filename(c->opt->output, libname, ".a");
  printf("Archiving %s\n", file_lib);

  len = 32 + strlen(file_lib) + strlen(file_o);
  VLA(char, cmd, len);

  snprintf(cmd, len, "ar -rcs %s %s", file_lib, file_o);

  if(system(cmd) != 0)
  {
    errorf(NULL, "unable to link");
    return false;
  }
#elif defined(PLATFORM_IS_WINDOWS)
  VLA(char, libname, len + 1);
  memcpy(libname, c->filename, len + 1);

  const char* file_lib = suffix_filename(c->opt->output, libname, ".lib");
  printf("Archiving %s\n", file_lib);

  vcvars_t vcvars;

  if(!vcvars_get(&vcvars))
  {
    errorf(NULL, "unable to link");
    return false;
  }

  len = 128 + strlen(file_lib) + strlen(file_o);
  VLA(char, cmd, len);

  snprintf(cmd, len, "cmd /C \"\"%s\" /NOLOGO /OUT:%s %s\"", vcvars.ar,
    file_lib, file_o);

  if(system(cmd) == -1)
  {
    errorf(NULL, "unable to link");
    return false;
  }
#endif

  return true;
}

bool genlib(compile_t* c, ast_t* program)
{
  if(!reachable_actors(c, program) ||
    !generate_actors(c, program) ||
    !genheader(c)
    )
    return false;

  const char* file_o = genobj(c);

  if(file_o == NULL)
    return false;

  if(c->opt->limit < PASS_ALL)
    return true;

  if(!link_lib(c, file_o))
    return false;

  unlink(file_o);
  return true;
}
