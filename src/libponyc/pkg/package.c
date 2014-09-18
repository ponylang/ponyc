#include "package.h"
#include "../codegen/codegen.h"
#include "../ast/source.h"
#include "../ast/parser.h"
#include "../ast/ast.h"
#include "../ast/token.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

#include <platform.h>

#define EXTENSION ".pony"

#ifdef PLATFORM_IS_VISUAL_STUDIO
/** Disable warning about "getenv" begin unsafe. The alternatives, s_getenv and
 *  _dupenv_s are incredibly inconvenient and expensive to use. Since such a
 *  warning could make sense for other function calls, we only disable it for
 *  this file.
 */
#  pragma warning(disable:4996)
#endif

typedef struct package_t
{
  const char* path;
  const char* id;
  size_t next_hygienic_id;
} package_t;

typedef struct magic_package_t
{
  const char* path;
  const char* src;
  struct magic_package_t* next;
} magic_package_t;

static strlist_t* search;
static magic_package_t* magic_packages = NULL;
static bool report_build = true;


// Check whether the given path is a defined magic package
static bool is_magic_package(const char* path)
{
  for(magic_package_t* p = magic_packages; p != NULL; p = p->next)
  {
    if(path == p->path)
      return true;
  }

  return false;
}


// Build a magic package
static bool do_magic_package(ast_t* package, const char* path)
{
  for(magic_package_t* p = magic_packages; p != NULL; p = p->next)
  {
    if(path == p->path)
    {
      source_t* source = source_open_string(p->src);
      ast_t* module = parser(source);

      if(module == NULL)
      {
        errorf("internal", "couldn't parse package description");
        source_close(source);
        return false;
      }

      ast_add(package, module);
      return true;
    }
  }

  // Magic not found. Oops
  errorf("internal", "magic package %s not found", path);
  assert(false);
  return false;
}


static bool filepath(const char *file, char* path)
{
  struct stat sb;

  if((pony_realpath(file, path) != path)
    || (stat(path, &sb) != 0)
    || ((sb.st_mode & S_IFMT) != S_IFREG)
    )
  {
    return false;
  }

  char *p = strrchr(path, '/');

#ifdef PLATFORM_IS_WINDOWS
  char *b = strrchr(path, '\\');
  if(p < b) p = b;
#endif

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
  bool is_path = strchr(file, '/') != NULL;

#ifdef PLATFORM_IS_WINDOWS
  is_path |= strchr(file, '\\') != NULL;
#endif

  if(is_path)
    return filepath(file, path);

  // it's just an executable name, so walk the path
  const char* env = getenv("PATH");

  if(env != NULL)
  {
    size_t flen = strlen(file);

    while(true)
    {
      char* p = strchr((char*)env, ':');
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
#ifdef PLATFORM_IS_POSIX_BASED
        check[len++] = '/';
#elif defined(PLATFORM_IS_WINDOWS)
        check[len++] = '\\';
#endif
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

static bool do_path(bool is_magic, ast_t* package, const char* path)
{
  if(is_magic)
    return do_magic_package(package, path);

  PONY_ERRNO err = 0;
  PONY_DIR* dir = pony_opendir(path, &err);

  if(dir == NULL)
  {
    switch(err)
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

  PONY_DIRINFO dirent;
  PONY_DIRINFO* d;
  bool r = true;

  while(!pony_dir_entry_next(dir, &dirent, &d) && (d != NULL))
  {
    //if(d->d_type & DT_REG)
    {
      // handle only files with the specified extension
      char* name = pony_dir_info_name(d);
      const char* p = strrchr(name, '.');

      if(!p || strcmp(p, EXTENSION))
        continue;

      char fullpath[FILENAME_MAX];
      strcpy(fullpath, path);
#ifdef PLATFORM_IS_POSIX_BASED
      strcat(fullpath, "/");
#elif defined(PLATFORM_IS_WINDOWS)
      strcat(fullpath, "\\");
#endif
      strcat(fullpath, name);

      r &= do_file(package, fullpath);
    }
  }

  pony_closedir(dir);
  return r;
}

static const char* try_path(const char* base, const char* path)
{
  char composite[FILENAME_MAX];
  char file[FILENAME_MAX];

  if(base != NULL)
  {
    strcpy(composite, base);
#ifdef PLATFORM_IS_POSIX_BASED
    strcat(composite, "/");
#elif defined(PLATFORM_IS_WINDOWS)
    strcat(composite, "\\");
#endif
    strcat(composite, path);
  } else {
    strcpy(composite, path);
  }

  if(pony_realpath(composite, file) != file)
    return NULL;

  return stringtab(file);
}

static const char* find_path(ast_t* from, const char* path)
{
  bool is_absolute = false;

#ifdef PLATFORM_IS_POSIX_BASED
  is_absolute = (path[0] == '/');
#elif defined(PLATFORM_IS_WINDOWS)
  is_absolute = (path[1] == ':');
#endif

  if(is_absolute)
    return try_path(NULL, path);

  const char* result;

  if(ast_id(from) == TK_PROGRAM)
  {
    // try a path relative to the current working directory
    result = try_path(NULL, path);

    if(result != NULL)
      return result;
  }
  else {
    // try a path relative to the importing package
    from = ast_nearest(from, TK_PACKAGE);
    package_t* pkg = (package_t*)ast_data(from);
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

static const char* id_to_string(size_t id)
{
  char buffer[32];
  snprintf(buffer, 32, "$"__zu, id);
  return stringtab(buffer);
}

static ast_t* create_package(ast_t* program, const char* name)
{
  ast_t* package = ast_blank(TK_PACKAGE);
  uintptr_t pkg_id = (uintptr_t)ast_data(program);
  ast_setdata(program, (void*)(pkg_id + 1));

  package_t* pkg = (package_t*)malloc(sizeof(package_t));
  pkg->path = name;
  pkg->id = id_to_string(pkg_id);
  pkg->next_hygienic_id = 0;
  ast_setdata(package, pkg);

  ast_scope(package);
  ast_append(program, package);
  ast_set(program, pkg->path, package, SYM_NONE);
  ast_set(program, pkg->id, package, SYM_NONE);

  return package;
}

static void add_path(const char* path)
{
  struct stat s;
  int err = stat(path, &s);

  if((err != -1) && S_ISDIR(s.st_mode))
    search = strlist_push(search, stringtab(path));
}

bool package_init(const char* name, pass_opt_t* opt)
{
  if(!codegen_init(opt))
    return false;

  char path[FILENAME_MAX];

  if(execpath(name, path))
    add_path(path);

  package_add_paths(getenv("PONYPATH"));
  return true;
}

strlist_t* package_paths()
{
  return search;
}

void package_add_paths(const char* paths)
{
  if(paths == NULL)
    return;

  while(true)
  {
#ifdef PLATFORM_IS_WINDOWS
    const char* p = strchr(paths, ';');
#else
    const char* p = strchr(paths, ':');
#endif
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
      add_path(path);
    }

    if(p == NULL)
      break;

    paths = p + 1;
  }
}

void package_add_magic(const char* path, const char* src)
{
  magic_package_t* n = (magic_package_t*)malloc(sizeof(magic_package_t));
  n->path = stringtab(path);
  n->src = src;
  n->next = magic_packages;
  magic_packages = n;
}

void package_suppress_build_message()
{
  report_build = false;
}

ast_t* program_load(const char* path)
{
  ast_t* program = ast_blank(TK_PROGRAM);
  ast_scope(program);

  if(package_load(program, path) == NULL)
  {
    ast_free(program);
    program = NULL;
  }

  return program;
}

ast_t* package_load(ast_t* from, const char* path)
{
  bool magic = is_magic_package(path);
  const char* name = magic ? path : find_path(from, path);

  if(name == NULL)
    return NULL;

  ast_t* program = ast_nearest(from, TK_PROGRAM);
  ast_t* package = ast_get(program, name, NULL);

  if(package != NULL)
    return package;

  package = create_package(program, name);

  if(report_build)
    printf("=== Building %s ===\n", name);

  if(!do_path(magic, package, name))
    return NULL;

  if(!package_passes(package))
  {
    ast_error(package, "can't typecheck package '%s'", path);
    return NULL;
  }

  return package;
}

const char* package_name(ast_t* ast)
{
  package_t* pkg = (package_t*)ast_data(ast_nearest(ast, TK_PACKAGE));
  return (pkg == NULL) ? NULL : pkg->id;
}

ast_t* package_id(ast_t* ast)
{
  return ast_from_string(ast, package_name(ast));
}

const char* package_filename(ast_t* ast)
{
  package_t* pkg = (package_t*)ast_data(ast_nearest(ast, TK_PACKAGE));

#ifdef PLATFORM_IS_POSIX_BASED
  const char* p = strrchr(pkg->path, '/');
#elif defined(PLATFORM_IS_WINDOWS)
  const char* p = strrchr(pkg->path, '\\');
#endif

  if(p == NULL)
    return pkg->path;

  return p + 1;
}

ast_t* package_hygienic_id(ast_t* ast)
{
  return ast_from_string(ast, package_hygienic_id_string(ast));
}

const char* package_hygienic_id_string(ast_t* ast)
{
  ast_t* package = ast_nearest(ast, TK_PACKAGE);

  if(package == NULL)
  {
    // We are not within a package, we must be testing
    return stringtab("hygid");
  }

  package_t* pkg = (package_t*)ast_data(package);
  size_t id = pkg->next_hygienic_id++;

  return id_to_string(id);
}

void package_done(pass_opt_t* opt)
{
  codegen_shutdown(opt);

  strlist_free(search);
  search = NULL;

  magic_package_t*p = magic_packages;
  while(p != NULL)
  {
    magic_package_t* next = p->next;
    free(p);
    p = next;
  }

  magic_packages = NULL;
}
