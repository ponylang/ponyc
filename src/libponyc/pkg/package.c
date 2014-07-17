#include "package.h"
#include "../codegen/codegen.h"
#include "../pass/sugar.h"
#include "../pass/scope.h"
#include "../pass/names.h"
#include "../pass/traits.h"
#include "../pass/expr.h"
#include "../ast/source.h"
#include "../ast/parser.h"
#include "../ast/ast.h"
#include "../ast/token.h"
#include "../ds/stringtab.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>

#define EXTENSION ".pony"

typedef struct package_t
{
  const char* path;
  const char* id;
  size_t next_hygienic_id;
} package_t;

static strlist_t* search;

static bool filepath(const char *file, char* path)
{
  struct stat sb;

  if((realpath(file, path) != path)
    || (stat(path, &sb) != 0)
    || ((sb.st_mode & S_IFMT) != S_IFREG)
    )
  {
    return false;
  }

  char* p = strrchr(path, '/');

  if(p != NULL)
  {
    if(p != path)
    {
      p[0] = '\0';
    } else {
      p[1] = '\0';
    }
  }

  return true;
}

static bool execpath(const char* file, char* path)
{
  // if it contains a separator of any kind, it's an absolute or relative path
  if(strchr(file, '/') != NULL)
    return filepath(file, path);

  // it's just an executable name, so walk the path
  const char* env = getenv("PATH");

  if(env != NULL)
  {
    size_t flen = strlen(file);

    while(true)
    {
      char* p = strchr(env, ':');
      size_t len;

      if(p != NULL)
      {
        len = p - env;
      } else {
        len = strlen(env);
      }

      if((len + flen + 1) < FILENAME_MAX)
      {
        char check[FILENAME_MAX];
        strncpy(check, env, len);
        check[len++] = '/';
        strcpy(&check[len], file);

        if(filepath(check, path))
          return true;
      }

      if(p == NULL) { break; }
      env = p + 1;
    }
  }

  // try the current directory as a last resort
  return filepath(file, path);
}

static bool do_file(ast_t* package, const char* file)
{
  source_t* source = source_open(file);

  if(source == NULL)
  {
    errorf(file, "couldn't open file");
    return false;
  }

  ast_t* module = parser(source);

  if(module == NULL)
  {
    errorf(file, "couldn't parse file");
    source_close(source);
    return false;
  }

  ast_add(package, module);
  return true;
}

static bool do_path(ast_t* package, const char* path)
{
  DIR* dir = opendir(path);

  if(dir == NULL)
  {
    switch(errno)
    {
      case EACCES:
        errorf(path, "permission denied");
        break;

      case ENOENT:
        errorf(path, "does not exist");
        break;

      case ENOTDIR:
        errorf(path, "not a directory");
        break;

      default:
        errorf(path, "unknown error");
    }

    return false;
  }

  struct dirent dirent;
  struct dirent* d;
  bool r = true;

  while(!readdir_r(dir, &dirent, &d) && (d != NULL))
  {
    //if(d->d_type & DT_REG)
    {
      // handle only files with the specified extension
      const char* p = strrchr(d->d_name, '.');

      if(!p || strcmp(p, EXTENSION))
        continue;

      char fullpath[FILENAME_MAX];
      strcpy(fullpath, path);
      strcat(fullpath, "/");
      strcat(fullpath, d->d_name);

      r &= do_file(package, fullpath);
    }
  }

  closedir(dir);
  return r;
}

static const char* try_path(const char* base, const char* path)
{
  char composite[FILENAME_MAX];
  char file[FILENAME_MAX];

  if(base != NULL)
  {
    strcpy(composite, base);
    strcat(composite, "/");
    strcat(composite, path);
  } else {
    strcpy(composite, path);
  }

  if(realpath(composite, file) != file)
    return NULL;

  return stringtab(file);
}

static const char* find_path(ast_t* from, const char* path)
{
  // absolute path
  if(path[0] == '/')
    return try_path(NULL, path);

  const char* result;

  if(ast_id(from) == TK_PROGRAM)
  {
    // try a path relative to the current working directory
    result = try_path(NULL, path);

    if(result != NULL)
      return result;
  } else {
    // try a path relative to the importing package
    from = ast_nearest(from, TK_PACKAGE);
    package_t* pkg = ast_data(from);
    result = try_path(pkg->path, path);

    if(result != NULL)
      return result;
  }

  // try the search paths
  strlist_t* p = search;

  while(p != NULL)
  {
    result = try_path(strlist_data(p), path);

    if(result != NULL)
      return result;

    p = strlist_next(p);
  }

  errorf(path, "couldn't locate this path");
  return NULL;
}

static bool do_passes(ast_t* ast)
{
  if(ast_visit(&ast, pass_sugar, NULL) != AST_OK)
    return false;

  if(ast_visit(&ast, pass_scope, NULL) != AST_OK)
    return false;

  if(ast_visit(&ast, NULL, pass_names) != AST_OK)
    return false;

  if(ast_visit(&ast, pass_traits, NULL) != AST_OK)
    return false;

  // recalculate scopes in the presence of flattened traits
  ast_clear(ast);

  if(ast_visit(&ast, pass_scope, NULL) != AST_OK)
    return false;

  if(ast_visit(&ast, NULL, pass_expr) != AST_OK)
    return false;

  return true;
}

static const char* id_to_string(size_t id)
{
  char buffer[32];
  snprintf(buffer, 32, "$%zu", id);
  return stringtab(buffer);
}

void package_init(const char* name)
{
  char path[FILENAME_MAX];

  if(execpath(name, path))
  {
    strcat(path, "/packages");
    search = strlist_push(search, stringtab(path));
  }

  package_paths(getenv("PONYPATH"));
}

void package_paths(const char* paths)
{
  if(paths == NULL)
    return;

  while(true)
  {
    char* p = strchr(paths, ':');
    size_t len;

    if(p != NULL)
    {
      len = p - paths;
    } else {
      len = strlen(paths);
    }

    if(len < FILENAME_MAX)
    {
      char path[FILENAME_MAX];

      strncpy(path, paths, len);
      path[len] = '\0';
      search = strlist_push(search, stringtab(path));
    }

    if(p == NULL)
      break;

    paths = p + 1;
  }
}

ast_t* program_load(const char* path, bool parse_only)
{
  ast_t* program = ast_blank(TK_PROGRAM);
  ast_scope(program);

  if(package_load(program, path, parse_only) == NULL)
  {
    ast_free(program);
    program = NULL;
  }

  return program;
}

bool program_compile(ast_t* program)
{
  return codegen(program);
}

ast_t* package_load(ast_t* from, const char* path, bool parse_only)
{
  const char* name = find_path(from, path);

  if(name == NULL)
    return NULL;

  ast_t* program = ast_nearest(from, TK_PROGRAM);
  ast_t* package = ast_get(program, name);

  if(package != NULL)
    return package;

  package = ast_blank(TK_PACKAGE);
  uintptr_t pkg_id = (uintptr_t)ast_data(program);
  ast_setdata(program, (void*)(pkg_id + 1));

  package_t* pkg = malloc(sizeof(package_t));
  pkg->path = name;
  pkg->id = id_to_string(pkg_id);
  pkg->next_hygienic_id = 0;
  ast_setdata(package, pkg);

  ast_scope(package);
  ast_append(program, package);
  ast_set(program, pkg->path, package);
  ast_set(program, pkg->id, package);

  printf("=== Building %s ===\n", name);

  if(!do_path(package, name))
    return NULL;

  if(!parse_only && !do_passes(package))
  {
    ast_error(package, "can't typecheck package '%s'", path);
    return NULL;
  }

  return package;
}

const char* package_name(ast_t* ast)
{
  package_t* pkg = ast_data(ast_nearest(ast, TK_PACKAGE));
  return pkg->id;
}

ast_t* package_id(ast_t* ast)
{
  return ast_from_string(ast, package_name(ast));
}

ast_t* package_hygienic_id(ast_t* ast)
{
  ast_t* package = ast_nearest(ast, TK_PACKAGE);
  assert(package != NULL);

  package_t* pkg = ast_data(package);
  size_t id = pkg->next_hygienic_id++;

  return ast_from_string(ast, id_to_string(id));
}

void package_done()
{
  strlist_free(search);
  search = NULL;
}
