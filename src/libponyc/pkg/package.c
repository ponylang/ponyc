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

#ifdef PLATFORM_IS_LINUX
#include <unistd.h>
#elif defined PLATFORM_IS_MACOSX
#include <mach-o/dyld.h>
#endif


#define EXTENSION ".pony"


#ifdef PLATFORM_IS_WINDOWS
# define PATH_SLASH '\\'
# define PATH_SLASH_STR "\\"
# define PATH_LIST_SEPARATOR ';'
#else
# define PATH_SLASH '/'
# define PATH_SLASH_STR "/"
# define PATH_LIST_SEPARATOR ':'
#endif


#ifdef PLATFORM_IS_VISUAL_STUDIO
/** Disable warning about "getenv" begin unsafe. The alternatives, s_getenv and
 *  _dupenv_s are incredibly inconvenient and expensive to use. Since such a
 *  warning could make sense for other function calls, we only disable it for
 *  this file.
 */
#  pragma warning(disable:4996)
#endif

// Per package state
typedef struct package_t
{
  const char* path; // Absolute path
  const char* id;
  size_t next_hygienic_id;
} package_t;

// Per defined magic package sate
typedef struct magic_package_t
{
  const char* path;
  const char* src;
  struct magic_package_t* next;
} magic_package_t;

// Global state
static strlist_t* search = NULL;
static magic_package_t* magic_packages = NULL;
static bool report_build = true;


// Find the magic source code associated with the given path, if any
static const char* find_magic_package(const char* path)
{
  for(magic_package_t* p = magic_packages; p != NULL; p = p->next)
  {
    if(path == p->path)
      return p->src;
  }

  return NULL;
}


// Attempt to parse the specified source file and add it to the given AST
// @return true on success, false on error
static bool parse_source_file(ast_t* package, const char* file_path)
{
  assert(package != NULL);
  assert(file_path != NULL);
  source_t* source = source_open(file_path);

  if(source == NULL)
  {
    errorf(file_path, "couldn't open file %s", file_path);
    return false;
  }

  ast_t* module = parser(source);

  if(module == NULL)
  {
    errorf(file_path, "couldn't parse file %s", file_path);
    source_close(source);
    return false;
  }

  ast_add(package, module);
  return true;
}


// Attempt to parse the specified source code and add it to the given AST
// @return true on success, false on error
static bool parse_source_code(ast_t* package, const char* src)
{
  assert(package != NULL);
  assert(src != NULL);
  source_t* source = source_open_string(src);
  assert(source != NULL);

  ast_t* module = parser(source);

  if(module == NULL)
  {
    source_close(source);
    return false;
  }

  ast_add(package, module);
  return true;
}


// Cat together the 2 given path fragments into the given buffer.
// The first path fragment may be absolute, relative or NULL
static void path_cat(const char* part1, const char* part2,
  char result[FILENAME_MAX])
{
  if(part1 != NULL)
  {
    strcpy(result, part1);
    strcat(result, PATH_SLASH_STR);
    strcat(result, part2);
  }
  else {
    strcpy(result, part2);
  }
}


// Attempt to parse the source files in the specified directory and add them to
// the given package AST
// @return true on success, false on error
static bool parse_files_in_dir(ast_t* package, const char* dir_path)
{
  PONY_ERRNO err = 0;
  PONY_DIR* dir = pony_opendir(dir_path, &err);

  if(dir == NULL)
  {
    switch(err)
    {
      case EACCES:  errorf(dir_path, "permission denied"); break;
      case ENOENT:  errorf(dir_path, "does not exist");    break;
      case ENOTDIR: errorf(dir_path, "not a directory");   break;
      default:      errorf(dir_path, "unknown error");     break;
    }

    return false;
  }

  PONY_DIRINFO dirent;
  PONY_DIRINFO* d;
  bool r = true;

  while(!pony_dir_entry_next(dir, &dirent, &d) && (d != NULL))
  {
    // Handle only files with the specified extension
    char* name = pony_dir_info_name(d);
    const char* p = strrchr(name, '.');

    if(p != NULL && strcmp(p, EXTENSION) == 0)
    {
      char fullpath[FILENAME_MAX];
      path_cat(dir_path, name, fullpath);
      r &= parse_source_file(package, fullpath);
    }
  }

  pony_closedir(dir);
  return r;
}


// Check whether the directory specified by catting the given abse and path
// exists
// @return The resulting directory path, which should not be deleted and is
// valid indefinitely. NULL is directory cannot be found.
static const char* try_path(const char* base, const char* path)
{
  char composite[FILENAME_MAX];
  char file[FILENAME_MAX];

  path_cat(base, path, composite);

  if(pony_realpath(composite, file) != file)
    return NULL;

  return stringtab(file);
}


// Attempt to find the specified package directory in our search path
// @return The resulting directory path, which should not be deleted and is
// valid indefinitely. NULL is directory cannot be found.
static const char* find_path(ast_t* from, const char* path)
{
  // First check for an absolute path
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
    // Try a path relative to the current working directory
    result = try_path(NULL, path);

    if(result != NULL)
      return result;
  }
  else
  {
    // Try a path relative to the importing package
    from = ast_nearest(from, TK_PACKAGE);
    package_t* pkg = (package_t*)ast_data(from);
    result = try_path(pkg->path, path);

    if(result != NULL)
      return result;
  }

  // Try the search paths
  for(strlist_t* p = search; p != NULL; p = strlist_next(p))
  {
    result = try_path(strlist_data(p), path);

    if(result != NULL)
      return result;
  }

  errorf(path, "couldn't locate this path");
  return NULL;
}


// Convert the given ID to a hygenic string. The resulting string should not be
// deleted and is valid indefinitely.
static const char* id_to_string(size_t id)
{
  char buffer[32];
  snprintf(buffer, 32, "$"__zu, id);
  return stringtab(buffer);
}


// Create a package AST, set up its state and add it to the given program
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


// Check that the given path exists and add it to our package search paths
static void add_path(const char* path)
{
#ifdef PLATFORM_IS_WINDOWS
  // The Windows implementation of stat() cannot cope with trailing a \ on a
  // directory name, so we remove it here if present. It is not safe to modify
  // the given path since it may come direct from getenv(), so we copy.
  char buf[FILENAME_MAX];
  strcpy(buf, path);
  size_t len = strlen(path);

  if(path[len - 1] == '\\')
  {
    buf[len - 1] = '\0';
    path = buf;
  }
#endif

  struct stat s;
  int err = stat(path, &s);

  if((err != -1) && S_ISDIR(s.st_mode))
    search = strlist_append(search, stringtab(path));
}


// Determine the absolute path of the directory the current executable is in
// and add it to our search path
static void add_exec_dir(const char* file)
{
  char path[FILENAME_MAX];
  bool success;

#ifdef PLATFORM_IS_WINDOWS
  GetModuleFileName(NULL, path, FILENAME_MAX);
  success = (GetLastError() == ERROR_SUCCESS);
#elif defined PLATFORM_IS_LINUX
  int r = readlink("/proc/self/exe", path, FILENAME_MAX);
  success = (r >= 0);
#elif defined PLATFORM_IS_MACOSX
  uint32_t size = sizeof(path);
  int r = _NSGetExecutablePath(path, &size);
  success = (r == 0);
#else
#  error Unsupported platform for exec_path()
#endif

  if(!success)
  {
    errorf(NULL, "Error determining executable path");
    return;
  }

  // We only need the directory
  char *p = strrchr(path, PATH_SLASH);

  if(p == NULL)
  {
    errorf(NULL, "Error determining executable path (%s)", path);
    return;
  }

  p[1] = '\0';
  add_path(path);
}


bool package_init(const char* name, pass_opt_t* opt)
{
  if(!codegen_init(opt))
    return false;

  package_add_paths(getenv("PONYPATH"));

#ifndef PLATFORM_IS_WINDOWS
  add_path("/usr/local/lib/pony");
  add_path("/usr/local/lib");
#endif

  add_exec_dir(name);
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
    // Find end of next path
    const char* p = strchr(paths, PATH_LIST_SEPARATOR);
    size_t len;

    if(p != NULL)
    {
      // Separator found
      len = p - paths;
    } else {
      // Separator not found, this is the last path in the list
      len = strlen(paths);
    }

    if(len >= FILENAME_MAX)
    {
      errorf(NULL, "Path too long in %s", paths);
    }
    else
    {
      char path[FILENAME_MAX];

      strncpy(path, paths, len);
      path[len] = '\0';
      add_path(path);
    }

    if(p == NULL) // No more separators
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


ast_t* program_load(const char* path, pass_opt_t* options)
{
  ast_t* program = ast_blank(TK_PROGRAM);
  ast_scope(program);

  if(package_load(program, path, options) == NULL)
  {
    ast_free(program);
    program = NULL;
  }

  return program;
}


ast_t* package_load(ast_t* from, const char* path, pass_opt_t* options)
{
  const char* magic = find_magic_package(path);
  const char* name = path;

  if(magic == NULL)
  {
    name = find_path(from, path);
    if(name == NULL)
      return NULL;
  }

  ast_t* program = ast_nearest(from, TK_PROGRAM);
  ast_t* package = ast_get(program, name, NULL);

  if(package != NULL)
    return package;

  package = create_package(program, name);

  if(report_build)
    printf("=== Building %s ===\n", name);

  if(magic != NULL)
  {
    if(!parse_source_code(package, magic))
      return NULL;
  }
  else
  {
    if(!parse_files_in_dir(package, name))
      return NULL;
  }

  if(ast_child(package) == NULL)
  {
    ast_error(package, "no source files in package '%s'", path);
    return NULL;
  }

  if(!package_passes(package, options))
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


const char* package_filename(ast_t* package)
{
  assert(package != NULL);
  assert(ast_id(package) == TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(package);
  assert(pkg != NULL);

  const char* p = strrchr(pkg->path, PATH_SLASH);

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
