#include "package.h"
#include "../type/typechecker.h"
#include "../ast/source.h"
#include "../ast/parser.h"
#include "../ast/ast.h"
#include "../ds/stringtab.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#define EXTENSION ".pony"

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

static bool do_file(ast_t* package, const char* file, int verbose)
{
  if(verbose > 0)
    printf("Building %s\n", file);

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

static bool do_path(ast_t* package, const char* path, int verbose)
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
    if(d->d_type & DT_REG)
    {
      // handle only files with the specified extension
      const char* p = strrchr(d->d_name, '.');

      if(!p || strcmp(p, EXTENSION))
        continue;

      char fullpath[FILENAME_MAX];
      strcpy(fullpath, path);
      strcat(fullpath, "/");
      strcat(fullpath, d->d_name);

      r &= do_file(package, fullpath, verbose);
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
    result = try_path(ast_data(from), path);

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

/**
 * Initialises the search directories. This is composed of a "packages"
 * directory relative to the executable, plus a collection of directories
 * specified in the PONYPATH environment variable.
 *
 * @param name The path to the executable file, generally argv[0]. The real
 *   path will be determined from argv[0].
 */
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

ast_t* program_load(const char* path, int verbose)
{
  ast_t* program = ast_new(TK_PROGRAM, 0, 0, NULL);
  ast_scope(program);

  if(package_load(program, path, verbose) == NULL)
  {
    ast_free(program);
    program = NULL;
  }

  return program;
}

ast_t* package_load(ast_t* from, const char* path, int verbose)
{
  const char* name = find_path(from, path);

  if(name == NULL)
    return NULL;

  ast_t* program = ast_nearest(from, TK_PROGRAM);
  ast_t* package = ast_get(program, name);

  if(package != NULL)
    return package;

  package = ast_new(TK_PACKAGE, 0, 0, (void*)name);
  ast_scope(package);
  ast_add(program, package);
  ast_set(program, name, package);

  printf("=== Building %s ===\n", name);

  if(!do_path(package, name, verbose))
    return NULL;


  if(!typecheck(package, verbose))
  {
    ast_error(package, "can't typecheck package '%s'", path);
    return NULL;
  }

  return package;
}

/**
 * Cleans up the list of search directories.
 */
void package_done()
{
  strlist_free(search);
  search = NULL;
}
