#include "package.h"
#include "program.h"
#include "use.h"
#include "../codegen/codegen.h"
#include "../ast/source.h"
#include "../ast/parser.h"
#include "../ast/ast.h"
#include "../ast/token.h"
#include "../expr/literal.h"
#include "../../libponyrt/gc/serialise.h"
#include "../../libponyrt/mem/pool.h"
#include "../../libponyrt/sched/scheduler.h"
#include "ponyassert.h"
#include <blake2.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#if defined(PLATFORM_IS_LINUX)
#include <unistd.h>
#elif defined(PLATFORM_IS_MACOSX)
#include <mach-o/dyld.h>
#elif defined(PLATFORM_IS_BSD)
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#endif


#define EXTENSION ".pony"


#ifdef PLATFORM_IS_WINDOWS
# define PATH_SLASH '\\'
# define PATH_LIST_SEPARATOR ';'
#else
# define PATH_SLASH '/'
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


static const char* const simple_builtin =
  "primitive Bool\n"
  "  new create(a: Bool) => a\n"
  "primitive U8 is Real[U8]\n"
  "  new create(a: U8) => a\n"
  "primitive U32 is Real[U32]\n"
  "  new create(a: U32) => a\n"
  "trait val Real[A: Real[A] val]\n"
  "type Number is (U8 | U32)\n"
  "primitive None\n"
  "struct Pointer[A]\n"
  "class val Env\n"
  "  new _create(argc: U32, argv: Pointer[Pointer[U8]] val,\n"
  "    envp: Pointer[Pointer[U8]] val)\n"
  "  => None";


DECLARE_HASHMAP_SERIALISE(package_set, package_set_t, package_t)

// Per package state
struct package_t
{
  const char* path; // Absolute path
  const char* qualified_name; // For pretty printing, eg "builtin/U32"
  const char* id; // Hygienic identifier
  const char* filename; // Directory name
  const char* symbol; // Wart to use for symbol names
  ast_t* ast;
  package_set_t dependencies;
  package_group_t* group;
  size_t group_index;
  size_t next_hygienic_id;
  size_t low_index;
  bool allow_ffi;
  bool on_stack;
};

// Minimal package data structure for signature computation.
typedef struct package_signature_t
{
  const char* filename;
  package_group_t* group;
  size_t group_index;
} package_signature_t;

// A strongly connected component in the package dependency graph
struct package_group_t
{
  char* signature;
  package_set_t members;
};

// Per defined magic package state
struct magic_package_t
{
  const char* path;
  const char* src;
  const char* mapped_path;
  struct magic_package_t* next;
};

// Function that will handle a path in some way.
typedef bool (*path_fn)(const char* path, pass_opt_t* opt);

DECLARE_STACK(package_stack, package_stack_t, package_t)
DEFINE_STACK(package_stack, package_stack_t, package_t)

DEFINE_LIST_SERIALISE(package_group_list, package_group_list_t, package_group_t,
  NULL, package_group_free, package_group_pony_type())


static size_t package_hash(package_t* pkg)
{
  // Hash the full string instead of the stringtab pointer. We want a
  // deterministic hash in order to enable deterministic hashmap iteration,
  // which in turn enables deterministic package signatures.
  return (size_t)ponyint_hash_str(pkg->qualified_name);
}


static bool package_cmp(package_t* a, package_t* b)
{
  return a->qualified_name == b->qualified_name;
}


DEFINE_HASHMAP_SERIALISE(package_set, package_set_t, package_t, package_hash,
  package_cmp, NULL, package_pony_type())


// Find the magic source code associated with the given path, if any
static magic_package_t* find_magic_package(const char* path, pass_opt_t* opt)
{
  for(magic_package_t* p = opt->magic_packages; p != NULL; p = p->next)
  {
    if(path == p->path)
      return p;
  }

  return NULL;
}


// Attempt to parse the specified source file and add it to the given AST
// @return true on success, false on error
static bool parse_source_file(ast_t* package, const char* file_path,
  pass_opt_t* opt)
{
  pony_assert(package != NULL);
  pony_assert(file_path != NULL);
  pony_assert(opt != NULL);

  if(opt->print_filenames)
    printf("Opening %s\n", file_path);

  const char* error_msg = NULL;
  source_t* source = source_open(file_path, &error_msg);

  if(source == NULL)
  {
    if(error_msg == NULL)
      error_msg = "couldn't open file";

    errorf(opt->check.errors, file_path, "%s %s", error_msg, file_path);
    return false;
  }

  return module_passes(package, opt, source);
}


// Attempt to parse the specified source code and add it to the given AST
// @return true on success, false on error
static bool parse_source_code(ast_t* package, const char* src,
  pass_opt_t* opt)
{
  pony_assert(src != NULL);
  pony_assert(opt != NULL);

  if(opt->print_filenames)
    printf("Opening magic source\n");

  source_t* source = source_open_string(src);
  pony_assert(source != NULL);

  return module_passes(package, opt, source);
}


void path_cat(const char* part1, const char* part2, char result[FILENAME_MAX])
{
  size_t len1 = 0;
  size_t lensep = 0;

  if(part1 != NULL)
  {
    len1 = strlen(part1);
    lensep = 1;
  }

  size_t len2 = strlen(part2);

  if((len1 + lensep + len2) >= FILENAME_MAX)
  {
    result[0] = '\0';
    return;
  }

  if(part1 != NULL)
  {
    memcpy(result, part1, len1);
    result[len1] = PATH_SLASH;
    memcpy(&result[len1 + 1], part2, len2 + 1);
  } else {
    memcpy(result, part2, len2 + 1);
  }
}


static int string_compare(const void* a, const void* b)
{
  return strcmp(*(const char**)a, *(const char**)b);
}


// Attempt to parse the source files in the specified directory and add them to
// the given package AST
// @return true on success, false on error
static bool parse_files_in_dir(ast_t* package, const char* dir_path,
  pass_opt_t* opt)
{
  PONY_ERRNO err = 0;
  PONY_DIR* dir = pony_opendir(dir_path, &err);
  errors_t* errors = opt->check.errors;

  if(dir == NULL)
  {
    switch(err)
    {
      case EACCES:  errorf(errors, dir_path, "permission denied"); break;
      case ENOENT:  errorf(errors, dir_path, "does not exist");    break;
      case ENOTDIR: errorf(errors, dir_path, "not a directory");   break;
      default:      errorf(errors, dir_path, "unknown error");     break;
    }

    return false;
  }

  size_t count = 0;
  size_t buf_size = 4 * sizeof(const char*);
  const char** entries = (const char**)ponyint_pool_alloc_size(buf_size);
  PONY_DIRINFO* d;

  while((d = pony_dir_entry_next(dir)) != NULL)
  {
    // Handle only files with the specified extension that don't begin with
    // a dot. This avoids including UNIX hidden files in a build.
    const char* name = stringtab(pony_dir_info_name(d));

    if(name[0] == '.')
      continue;

    const char* p = strrchr(name, '.');

    if((p != NULL) && (strcmp(p, EXTENSION) == 0))
    {
      if((count * sizeof(const char*)) == buf_size)
      {
        size_t new_buf_size = buf_size * 2;
        entries = (const char**)ponyint_pool_realloc_size(buf_size,
          new_buf_size, entries);
        buf_size = new_buf_size;
      }

      entries[count++] = name;
    }
  }

  pony_closedir(dir);

  // In order for package signatures to be deterministic, file parsing order
  // must be deterministic too.
  qsort(entries, count, sizeof(const char*), string_compare);
  bool r = true;

  for(size_t i = 0; i < count; i++)
  {
    char fullpath[FILENAME_MAX];
    path_cat(dir_path, entries[i], fullpath);
    r &= parse_source_file(package, fullpath, opt);
  }

  ponyint_pool_free_size(buf_size, entries);
  return r;
}


// Check whether the directory specified by catting the given base and path
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

  struct stat s;
  int err = stat(file, &s);

  if((err != -1) && S_ISDIR(s.st_mode))
    return stringtab(file);

  return NULL;
}


static bool is_root(const char* path)
{
  pony_assert(path != NULL);

#if defined(PLATFORM_IS_WINDOWS)
  pony_assert(path[0] != '\0');
  pony_assert(path[1] == ':');

  if((path[2] == '\0'))
    return true;

  if((path[2] == '\\') && (path[3] == '\0'))
    return true;
#else
  if((path[0] == '/') && (path[1] == '\0'))
    return true;
#endif

  return false;
}


// Try base/../pony_packages/path, and keep adding .. to look another level up
// until we are looking in /pony_packages/path
static const char* try_package_path(const char* base, const char* path)
{
  char path1[FILENAME_MAX];
  char path2[FILENAME_MAX];
  path_cat(NULL, base, path1);

  do
  {
    path_cat(path1, "..", path2);

    if(pony_realpath(path2, path1) != path1)
      break;

    path_cat(path1, "pony_packages", path2);

    const char* result = try_path(path2, path);

    if(result != NULL)
      return result;
  } while(!is_root(path1));

  return NULL;
}


// Attempt to find the specified package directory in our search path
// @return The resulting directory path, which should not be deleted and is
// valid indefinitely. NULL is directory cannot be found.
static const char* find_path(ast_t* from, const char* path,
  bool* out_is_relative, pass_opt_t* opt)
{
  if(out_is_relative != NULL)
    *out_is_relative = false;

  // First check for an absolute path
  if(is_path_absolute(path))
    return try_path(NULL, path);

  // Get the base directory
  const char* base;

  if((from == NULL) || (ast_id(from) == TK_PROGRAM))
  {
    base = NULL;
  } else {
    from = ast_nearest(from, TK_PACKAGE);
    package_t* pkg = (package_t*)ast_data(from);
    base = pkg->path;
  }

  // Try a path relative to the base
  const char* result = try_path(base, path);

  if(result != NULL)
  {
    if(out_is_relative != NULL)
      *out_is_relative = true;

    return result;
  }

  // If it's a relative path, don't try elsewhere
  if(!is_path_relative(path))
  {
    // Check ../pony_packages and further up the tree
    if(base != NULL)
    {
      result = try_package_path(base, path);

      if(result != NULL)
        return result;

      // Check ../pony_packages from the compiler target
      if((from != NULL) && (ast_id(from) == TK_PACKAGE))
      {
        ast_t* target = ast_child(ast_parent(from));
        package_t* pkg = (package_t*)ast_data(target);
        base = pkg->path;

        result = try_package_path(base, path);

        if(result != NULL)
          return result;
      }
    }

    // Try the search paths
    for(strlist_t* p = opt->package_search_paths; p != NULL;
      p = strlist_next(p))
    {
      result = try_path(strlist_data(p), path);

      if(result != NULL)
        return result;
    }
  }

  return NULL;
}


// Convert the given ID to a hygenic string. The resulting string should not be
// deleted and is valid indefinitely.
static const char* id_to_string(const char* prefix, size_t id)
{
  if(prefix == NULL)
    prefix = "";

  size_t len = strlen(prefix);
  size_t buf_size = len + 32;
  char* buffer = (char*)ponyint_pool_alloc_size(buf_size);
  snprintf(buffer, buf_size, "%s$" __zu, prefix, id);
  return stringtab_consume(buffer, buf_size);
}


static bool symbol_in_use(ast_t* program, const char* symbol)
{
  ast_t* package = ast_child(program);

  while(package != NULL)
  {
    package_t* pkg = (package_t*)ast_data(package);

    if(pkg->symbol == symbol)
      return true;

    package = ast_sibling(package);
  }

  return false;
}


static const char* string_to_symbol(const char* string)
{
  bool prefix = false;

  if(!((string[0] >= 'a') && (string[0] <= 'z')) &&
    !((string[0] >= 'A') && (string[0] <= 'Z')))
  {
    // If it doesn't start with a letter, prefix an underscore.
    prefix = true;
  }

  size_t len = strlen(string);
  size_t buf_size = len + prefix + 1;
  char* buf = (char*)ponyint_pool_alloc_size(buf_size);
  memcpy(buf + prefix, string, len + 1);

  if(prefix)
    buf[0] = '_';

  for(size_t i = prefix; i < len; i++)
  {
    if(
      (buf[i] == '_') ||
      ((buf[i] >= 'a') && (buf[i] <= 'z')) ||
      ((buf[i] >= '0') && (buf[i] <= '9'))
      )
    {
      // Do nothing.
    } else if((buf[i] >= 'A') && (buf[i] <= 'Z')) {
      // Force lower case.
      buf[i] |= 0x20;
    } else {
      // Smash a non-symbol character to an underscore.
      buf[i] = '_';
    }
  }

  return stringtab_consume(buf, buf_size);
}


static const char* symbol_suffix(const char* symbol, size_t suffix)
{
  size_t len = strlen(symbol);
  size_t buf_size = len + 32;
  char* buf = (char*)ponyint_pool_alloc_size(buf_size);
  snprintf(buf, buf_size, "%s" __zu, symbol, suffix);

  return stringtab_consume(buf, buf_size);
}


static const char* create_package_symbol(ast_t* program, const char* filename)
{
  const char* symbol = string_to_symbol(filename);
  size_t suffix = 1;

  while(symbol_in_use(program, symbol))
  {
    symbol = symbol_suffix(symbol, suffix);
    suffix++;
  }

  return symbol;
}


// Create a package AST, set up its state and add it to the given program
static ast_t* create_package(ast_t* program, const char* name,
  const char* qualified_name, pass_opt_t* opt)
{
  ast_t* package = ast_blank(TK_PACKAGE);
  uint32_t pkg_id = program_assign_pkg_id(program);

  package_t* pkg = POOL_ALLOC(package_t);
  pkg->path = name;
  pkg->qualified_name = qualified_name;
  pkg->id = id_to_string(NULL, pkg_id);

  const char* p = strrchr(pkg->path, PATH_SLASH);

  if(p == NULL)
    p = pkg->path;
  else
    p = p + 1;

  pkg->filename = stringtab(p);

  if(pkg_id > 1)
    pkg->symbol = create_package_symbol(program, pkg->filename);
  else
    pkg->symbol = NULL;

  pkg->ast = package;
  package_set_init(&pkg->dependencies, 1);
  pkg->group = NULL;
  pkg->group_index = -1;
  pkg->next_hygienic_id = 0;
  pkg->low_index = -1;
  ast_setdata(package, pkg);

  ast_scope(package);
  ast_append(program, package);
  ast_set(program, pkg->path, package, SYM_NONE, false);
  ast_set(program, pkg->id, package, SYM_NONE, false);

  strlist_t* safe = opt->safe_packages;

  if((safe != NULL) && (strlist_find(safe, pkg->path) == NULL))
    pkg->allow_ffi = false;
  else
    pkg->allow_ffi = true;

  pkg->on_stack = false;

  return package;
}


// Check that the given path exists and add it to our package search paths
static bool add_path(const char* path, pass_opt_t* opt)
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
  {
    path = stringtab(path);
    strlist_t* search = opt->package_search_paths;

    if(strlist_find(search, path) == NULL)
      opt->package_search_paths = strlist_append(search, path);
  }

  return true;
}

// Safely concatenate paths and add it to package search paths
static bool add_relative_path(const char* path, const char* relpath,
  pass_opt_t* opt)
{
  char buf[FILENAME_MAX];

  if(strlen(path) + strlen(relpath) >= FILENAME_MAX)
    return false;

  strcpy(buf, path);
  strcat(buf, relpath);
  return add_path(buf, opt);
}


static bool add_safe(const char* path, pass_opt_t* opt)
{
  path = stringtab(path);
  strlist_t* safe = opt->safe_packages;

  if(strlist_find(safe, path) == NULL)
    opt->safe_packages = strlist_append(safe, path);

  return true;
}


// Determine the absolute path of the directory the current executable is in
// and add it to our search path
static bool add_exec_dir(pass_opt_t* opt)
{
  char path[FILENAME_MAX];
  bool success;
  errors_t* errors = opt->check.errors;

#ifdef PLATFORM_IS_WINDOWS
  // Specified size *includes* nul terminator
  GetModuleFileName(NULL, path, FILENAME_MAX);
  success = (GetLastError() == ERROR_SUCCESS);
#elif defined PLATFORM_IS_LINUX
  // Specified size *excludes* nul terminator
  ssize_t r = readlink("/proc/self/exe", path, FILENAME_MAX - 1);
  success = (r >= 0);

  if(success)
    path[r] = '\0';
#elif defined PLATFORM_IS_BSD
  int mib[4];
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;

  size_t len = FILENAME_MAX;
  int r = sysctl(mib, 4, path, &len, NULL, 0);
  success = (r == 0);
#elif defined PLATFORM_IS_MACOSX
  char exec_path[FILENAME_MAX];
  uint32_t size = sizeof(exec_path);
  int r = _NSGetExecutablePath(exec_path, &size);
  success = (r == 0);

  if(success)
  {
    pony_realpath(exec_path, path);
  }
#else
#  error Unsupported platform for exec_path()
#endif

  if(!success)
  {
    errorf(errors, NULL, "Error determining executable path");
    return false;
  }

  // We only need the directory
  char *p = strrchr(path, PATH_SLASH);

  if(p == NULL)
  {
    errorf(errors, NULL, "Error determining executable path (%s)", path);
    return false;
  }

  p++;
  *p = '\0';
  add_path(path, opt);

  // Allow ponyc to find the lib directory when it is installed.
#ifdef PLATFORM_IS_WINDOWS
  success = add_relative_path(path, "..\\lib", opt);
#else
  success = add_relative_path(path, "../lib", opt);
#endif

  if(!success)
    return false;

  // Allow ponyc to find the packages directory when it is installed.
#ifdef PLATFORM_IS_WINDOWS
  success = add_relative_path(path, "..\\packages", opt);
#else
  success = add_relative_path(path, "../packages", opt);
#endif

  if(!success)
    return false;

  // Check two levels up. This allows ponyc to find the packages directory
  // when it is built from source.
#ifdef PLATFORM_IS_WINDOWS
  success = add_relative_path(path, "..\\..\\packages", opt);
#else
  success = add_relative_path(path, "../../packages", opt);
#endif

  if(!success)
    return false;

  return true;
}


bool package_init(pass_opt_t* opt)
{
  // package_add_paths for command line paths has already been done. Here, we
  // append the paths from an optional environment variable, and then the paths
  // that are relative to the compiler location on disk.
  package_add_paths(getenv("PONYPATH"), opt);
  if(!add_exec_dir(opt))
  {
    errorf(opt->check.errors, NULL, "Error adding package paths relative to ponyc binary location");
    return false;
  }

  // Finally we add OS specific paths.
#ifdef PLATFORM_IS_POSIX_BASED
  add_path("/usr/local/lib", opt);
  add_path("/opt/local/lib", opt);
#endif

  // Convert all the safe packages to their full paths.
  strlist_t* full_safe = NULL;
  strlist_t* safe = opt->safe_packages;

  while(safe != NULL)
  {
    const char* path;
    safe = strlist_pop(safe, &path);

    // Lookup (and hence normalise) path.
    path = find_path(NULL, path, NULL, opt);

    if(path == NULL)
    {
      strlist_free(full_safe);
      strlist_free(safe);
      opt->safe_packages = NULL;
      return false;
    }

    full_safe = strlist_push(full_safe, path);
  }

  opt->safe_packages = full_safe;

  if(opt->simple_builtin)
    package_add_magic_src("builtin", simple_builtin, opt);

  return true;
}


static bool handle_path_list(const char* paths, path_fn f, pass_opt_t* opt)
{
  if(paths == NULL)
    return true;

  bool ok = true;

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
      errorf(opt->check.errors, NULL, "Path too long in %s", paths);
    } else {
      char path[FILENAME_MAX];

      strncpy(path, paths, len);
      path[len] = '\0';
      ok = f(path, opt) && ok;
    }

    if(p == NULL) // No more separators
      break;

    paths = p + 1;
  }

  return ok;
}

void package_add_paths(const char* paths, pass_opt_t* opt)
{
  handle_path_list(paths, add_path, opt);
}


bool package_add_safe(const char* paths, pass_opt_t* opt)
{
  add_safe("builtin", opt);
  return handle_path_list(paths, add_safe, opt);
}


void package_add_magic_src(const char* path, const char* src, pass_opt_t* opt)
{
  magic_package_t* n = POOL_ALLOC(magic_package_t);
  n->path = stringtab(path);
  n->src = src;
  n->mapped_path = NULL;
  n->next = opt->magic_packages;
  opt->magic_packages = n;
}


void package_add_magic_path(const char* path, const char* mapped_path,
  pass_opt_t* opt)
{
  magic_package_t* n = POOL_ALLOC(magic_package_t);
  n->path = stringtab(path);
  n->src = NULL;
  n->mapped_path = mapped_path;
  n->next = opt->magic_packages;
  opt->magic_packages = n;
}


void package_clear_magic(pass_opt_t* opt)
{
  magic_package_t* p = opt->magic_packages;

  while(p != NULL)
  {
    magic_package_t* next = p->next;
    POOL_FREE(magic_package_t, p);
    p = next;
  }

  opt->magic_packages = NULL;
}


ast_t* program_load(const char* path, pass_opt_t* opt)
{
  ast_t* program = ast_blank(TK_PROGRAM);
  ast_scope(program);

  opt->program_pass = PASS_PARSE;

  // Always load builtin package first, then the specified one.
  if(package_load(program, stringtab("builtin"), opt) == NULL ||
    package_load(program, path, opt) == NULL)
  {
    ast_free(program);
    return NULL;
  }

  // Reorder packages so specified package is first.
  ast_t* builtin = ast_pop(program);
  ast_append(program, builtin);

  if(!ast_passes_program(program, opt))
  {
    ast_free(program);
    return NULL;
  }

  return program;
}


ast_t* package_load(ast_t* from, const char* path, pass_opt_t* opt)
{
  pony_assert(from != NULL);

  magic_package_t* magic = find_magic_package(path, opt);
  const char* full_path = path;
  const char* qualified_name = path;
  ast_t* program = ast_nearest(from, TK_PROGRAM);

  if(magic == NULL)
  {
    // Lookup (and hence normalise) path
    bool is_relative = false;
    full_path = find_path(from, path, &is_relative, opt);

    if(full_path == NULL)
    {
      errorf(opt->check.errors, path, "couldn't locate this path");
      return NULL;
    }

    if((from != program) && is_relative)
    {
      // Package to load is relative to from, build the qualified name
      // The qualified name should be relative to the program being built
      package_t* from_pkg = (package_t*)ast_data(ast_child(program));

      if(from_pkg != NULL)
      {
        const char* base_name = from_pkg->qualified_name;
        size_t base_name_len = strlen(base_name);
        size_t path_len = strlen(path);
        size_t len = base_name_len + path_len + 2;
        char* q_name = (char*)ponyint_pool_alloc_size(len);
        memcpy(q_name, base_name, base_name_len);
        q_name[base_name_len] = '/';
        memcpy(q_name + base_name_len + 1, path, path_len);
        q_name[len - 1] = '\0';
        qualified_name = stringtab_consume(q_name, len);
      }
    }
  }

  ast_t* package = ast_get(program, full_path, NULL);

  // Package already loaded
  if(package != NULL)
    return package;

  package = create_package(program, full_path, qualified_name, opt);

  if(opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, "Building %s -> %s\n", path, full_path);

  if(magic != NULL)
  {
    if(magic->src != NULL)
    {
      if(!parse_source_code(package, magic->src, opt))
        return NULL;
    } else if(magic->mapped_path != NULL) {
      if(!parse_files_in_dir(package, magic->mapped_path, opt))
        return NULL;
    } else {
      return NULL;
    }
  }
  else
  {
    if(!parse_files_in_dir(package, full_path, opt))
      return NULL;
  }

  if(ast_child(package) == NULL)
  {
    ast_error(opt->check.errors, package,
      "no source files in package '%s'", path);
    return NULL;
  }

  if(!ast_passes_subtree(&package, opt, opt->program_pass))
  {
    // If these passes failed, don't run future passes.
    ast_setflag(package, AST_FLAG_PRESERVE);
    return NULL;
  }

  return package;
}


void package_free(package_t* package)
{
  if(package != NULL)
  {
    package_set_destroy(&package->dependencies);
    POOL_FREE(package_t, package);
  }
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


const char* package_path(ast_t* package)
{
  pony_assert(package != NULL);
  pony_assert(ast_id(package) == TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(package);

  return pkg->path;
}


const char* package_qualified_name(ast_t* package)
{
  pony_assert(package != NULL);
  pony_assert(ast_id(package) == TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(package);

  return pkg->qualified_name;
}


const char* package_filename(ast_t* package)
{
  pony_assert(package != NULL);
  pony_assert(ast_id(package) == TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(package);
  pony_assert(pkg != NULL);

  return pkg->filename;
}


const char* package_symbol(ast_t* package)
{
  pony_assert(package != NULL);
  pony_assert(ast_id(package) == TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(package);
  pony_assert(pkg != NULL);

  return pkg->symbol;
}


const char* package_hygienic_id(typecheck_t* t)
{
  pony_assert(t->frame->package != NULL);
  package_t* pkg = (package_t*)ast_data(t->frame->package);
  size_t id = pkg->next_hygienic_id++;

  return id_to_string(pkg->id, id);
}


bool package_allow_ffi(typecheck_t* t)
{
  pony_assert(t->frame->package != NULL);
  package_t* pkg = (package_t*)ast_data(t->frame->package);
  return pkg->allow_ffi;
}


const char* package_alias_from_id(ast_t* module, const char* id)
{
  pony_assert(ast_id(module) == TK_MODULE);

  const char* strtab_id = stringtab(id);

  ast_t* use = ast_child(module);
  while(ast_id(use) == TK_USE)
  {
    ast_t* imported = (ast_t*)ast_data(use);
    pony_assert((imported != NULL) && (ast_id(imported) == TK_PACKAGE));

    package_t* pkg = (package_t*)ast_data(imported);
    pony_assert(pkg != NULL);

    if(pkg->id == strtab_id)
    {
      ast_t* alias = ast_child(use);
      if(ast_id(alias) == TK_NONE)
        return NULL;

      return ast_name(alias);
    }

    use = ast_sibling(use);
  }

  pony_assert(false);
  return NULL;
}


void package_add_dependency(ast_t* package, ast_t* dep)
{
  pony_assert(ast_id(package) == TK_PACKAGE);
  pony_assert(ast_id(dep) == TK_PACKAGE);

  if(package == dep)
    return;

  package_t* pkg = (package_t*)ast_data(package);
  package_t* pkg_dep = (package_t*)ast_data(dep);

  pony_assert(pkg != NULL);
  pony_assert(pkg_dep != NULL);

  size_t index = HASHMAP_UNKNOWN;
  package_t* in_set = package_set_get(&pkg->dependencies, pkg_dep, &index);

  if(in_set != NULL)
    return;

  package_set_putindex(&pkg->dependencies, pkg_dep, index);
}


const char* package_signature(ast_t* package)
{
  pony_assert(ast_id(package) == TK_PACKAGE);

  package_t* pkg = (package_t*)ast_data(package);
  pony_assert(pkg->group != NULL);

  return package_group_signature(pkg->group);
}


size_t package_group_index(ast_t* package)
{
  pony_assert(ast_id(package) == TK_PACKAGE);

  package_t* pkg = (package_t*)ast_data(package);
  pony_assert(pkg->group_index != (size_t)-1);

  return pkg->group_index;
}


package_group_t* package_group_new()
{
  package_group_t* group = POOL_ALLOC(package_group_t);
  group->signature = NULL;
  package_set_init(&group->members, 1);
  return group;
}


void package_group_free(package_group_t* group)
{
  if(group->signature != NULL)
    ponyint_pool_free_size(SIGNATURE_LENGTH, group->signature);

  package_set_destroy(&group->members);
  POOL_FREE(package_group_t, group);
}


static void make_dependency_group(package_t* package,
  package_group_list_t** groups, package_stack_t** stack, size_t* index)
{
  pony_assert(!package->on_stack);
  package->group_index = package->low_index = (*index)++;
  *stack = package_stack_push(*stack, package);
  package->on_stack = true;

  size_t i = HASHMAP_BEGIN;
  package_t* dep;

  while((dep = package_set_next(&package->dependencies, &i)) != NULL)
  {
    if(dep->group_index == (size_t)-1)
    {
      make_dependency_group(dep, groups, stack, index);

      if(dep->low_index < package->low_index)
        package->low_index = dep->low_index;
    } else if(dep->on_stack && (dep->group_index < package->low_index)) {
      package->low_index = dep->group_index;
    }
  }

  if(package->group_index == package->low_index)
  {
    package_group_t* group = package_group_new();
    package_t* member;
    size_t i = 0;

    do
    {
      *stack = package_stack_pop(*stack, &member);
      member->on_stack = false;
      member->group = group;
      member->group_index = i++;
      package_set_put(&group->members, member);
    } while(package != member);

    *groups = package_group_list_push(*groups, group);
  }
}


// A dependency group is a strongly connected component in the dependency graph.
package_group_list_t* package_dependency_groups(ast_t* first_package)
{
  package_group_list_t* groups = NULL;
  package_stack_t* stack = NULL;
  size_t index = 0;

  while(first_package != NULL)
  {
    pony_assert(ast_id(first_package) == TK_PACKAGE);
    package_t* package = (package_t*)ast_data(first_package);

    if(package->group_index == (size_t)-1)
      make_dependency_group(package, &groups, &stack, &index);

    first_package = ast_sibling(first_package);
  }

  pony_assert(stack == NULL);
  return package_group_list_reverse(groups);
}


static void print_signature(const char* sig)
{
  for(size_t i = 0; i < SIGNATURE_LENGTH; i++)
    printf("%02hhX", sig[i]);
}


void package_group_dump(package_group_t* group)
{
  package_set_t deps;
  package_set_init(&deps, 1);

  fputs("Signature: ", stdout);

  if(group->signature != NULL)
    print_signature(group->signature);
  else
    fputs("(NONE)", stdout);

  putchar('\n');

  puts("Members:");

  size_t i = HASHMAP_BEGIN;
  package_t* member;

  while((member = package_set_next(&group->members, &i)) != NULL)
  {
    printf("  %s\n", member->filename);

    size_t j = HASHMAP_BEGIN;
    package_t* dep;

    while((dep = package_set_next(&member->dependencies, &j)) != NULL)
    {
      size_t k = HASHMAP_UNKNOWN;
      package_t* in_set = package_set_get(&group->members, dep, &k);

      if(in_set == NULL)
      {
        k = HASHMAP_UNKNOWN;
        in_set = package_set_get(&deps, dep, &k);

        if(in_set == NULL)
          package_set_putindex(&deps, dep, k);
      }
    }
  }

  puts("Dependencies:");

  i = HASHMAP_BEGIN;

  while((member = package_set_next(&deps, &i)) != NULL)
    printf("  %s\n", member->filename);

  package_set_destroy(&deps);
}


// *_signature_* handles the current group, *_dep_signature_* handles the direct
// dependencies. Indirect dependencies are ignored, they are covered by the
// signature of the direct dependencies.
// Some data is traced but not serialised. This is to avoid redundant
// information.


static void package_dep_signature_serialise_trace(pony_ctx_t* ctx,
  void* object)
{
  package_t* package = (package_t*)object;

  string_trace(ctx, package->filename);
  pony_traceknown(ctx, package->group, package_group_dep_signature_pony_type(),
    PONY_TRACE_MUTABLE);
}

static void package_signature_serialise_trace(pony_ctx_t* ctx,
  void* object)
{
  package_t* package = (package_t*)object;

  string_trace(ctx, package->filename);
  // The group has already been traced.

  size_t i = HASHMAP_BEGIN;
  package_t* dep;

  while((dep = package_set_next(&package->dependencies, &i)) != NULL)
    pony_traceknown(ctx, dep, package_dep_signature_pony_type(),
      PONY_TRACE_MUTABLE);

  pony_traceknown(ctx, package->ast, ast_signature_pony_type(),
    PONY_TRACE_MUTABLE);
}


static void package_signature_serialise(pony_ctx_t* ctx, void* object,
  void* buf, size_t offset, int mutability)
{
  (void)mutability;

  package_t* package = (package_t*)object;
  package_signature_t* dst = (package_signature_t*)((uintptr_t)buf + offset);

  dst->filename = (const char*)pony_serialise_offset(ctx,
    (char*)package->filename);
  dst->group = (package_group_t*)pony_serialise_offset(ctx, package->group);
  dst->group_index = package->group_index;
}


static pony_type_t package_dep_signature_pony =
{
  0,
  sizeof(package_signature_t),
  0,
  0,
  NULL,
  NULL,
  package_dep_signature_serialise_trace,
  package_signature_serialise, // Same function for both package and package_dep.
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};


pony_type_t* package_dep_signature_pony_type()
{
  return &package_dep_signature_pony;
}


static pony_type_t package_signature_pony =
{
  0,
  sizeof(package_signature_t),
  0,
  0,
  NULL,
  NULL,
  package_signature_serialise_trace,
  package_signature_serialise,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};


pony_type_t* package_signature_pony_type()
{
  return &package_signature_pony;
}


static void package_group_dep_signature_serialise_trace(pony_ctx_t* ctx,
  void* object)
{
  package_group_t* group = (package_group_t*)object;

  pony_assert(group->signature != NULL);
  pony_serialise_reserve(ctx, group->signature, SIGNATURE_LENGTH);
}


static void package_group_signature_serialise_trace(pony_ctx_t* ctx,
  void* object)
{
  package_group_t* group = (package_group_t*)object;

  pony_assert(group->signature == NULL);

  size_t i = HASHMAP_BEGIN;
  package_t* member;

  while((member = package_set_next(&group->members, &i)) != NULL)
  {
    pony_traceknown(ctx, member, package_signature_pony_type(),
      PONY_TRACE_MUTABLE);
  }
}


static void package_group_signature_serialise(pony_ctx_t* ctx, void* object,
  void* buf, size_t offset, int mutability)
{
  (void)ctx;
  (void)mutability;

  package_group_t* group = (package_group_t*)object;
  package_group_t* dst = (package_group_t*)((uintptr_t)buf + offset);

  if(group->signature != NULL)
  {
    uintptr_t ptr_offset = pony_serialise_offset(ctx, group->signature);
    char* dst_sig = (char*)((uintptr_t)buf + ptr_offset);
    memcpy(dst_sig, group->signature, SIGNATURE_LENGTH);
    dst->signature = (char*)ptr_offset;
  } else {
    dst->signature = NULL;
  }
}


static pony_type_t package_group_dep_signature_pony =
{
  0,
  sizeof(const char*),
  0,
  0,
  NULL,
  NULL,
  package_group_dep_signature_serialise_trace,
  package_group_signature_serialise, // Same function for both group and group_dep.
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};


pony_type_t* package_group_dep_signature_pony_type()
{
  return &package_group_dep_signature_pony;
}


static pony_type_t package_group_signature_pony =
{
  0,
  sizeof(const char*),
  0,
  0,
  NULL,
  NULL,
  package_group_signature_serialise_trace,
  package_group_signature_serialise,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};


pony_type_t* package_group_signature_pony_type()
{
  return &package_group_signature_pony;
}


static void* s_alloc_fn(pony_ctx_t* ctx, size_t size)
{
  (void)ctx;
  return ponyint_pool_alloc_size(size);
}


static void s_throw_fn()
{
  pony_assert(false);
}


// TODO: Make group signature indiependent of package load order.
const char* package_group_signature(package_group_t* group)
{
  if(group->signature == NULL)
  {
    pony_ctx_t ctx;
    memset(&ctx, 0, sizeof(pony_ctx_t));
    ponyint_array_t array;
    memset(&array, 0, sizeof(ponyint_array_t));
    char* buf = (char*)ponyint_pool_alloc_size(SIGNATURE_LENGTH);

    pony_serialise(&ctx, group, package_group_signature_pony_type(), &array,
      s_alloc_fn, s_throw_fn);
    int status = blake2b(buf, SIGNATURE_LENGTH, array.ptr, array.size, NULL, 0);
    (void)status;
    pony_assert(status == 0);

    group->signature = buf;
    ponyint_pool_free_size(array.size, array.ptr);
  }

  return group->signature;
}


void package_done(pass_opt_t* opt)
{
  strlist_free(opt->package_search_paths);
  opt->package_search_paths = NULL;

  strlist_free(opt->safe_packages);
  opt->safe_packages = NULL;

  package_clear_magic(opt);
}


static void package_serialise_trace(pony_ctx_t* ctx, void* object)
{
  package_t* package = (package_t*)object;

  string_trace(ctx, package->path);
  string_trace(ctx, package->qualified_name);
  string_trace(ctx, package->id);
  string_trace(ctx, package->filename);

  if(package->symbol != NULL)
    string_trace(ctx, package->symbol);

  pony_traceknown(ctx, package->ast, ast_pony_type(), PONY_TRACE_MUTABLE);
  package_set_serialise_trace(ctx, &package->dependencies);

  if(package->group != NULL)
    pony_traceknown(ctx, package->group, package_group_pony_type(),
      PONY_TRACE_MUTABLE);
}


static void package_serialise(pony_ctx_t* ctx, void* object, void* buf,
  size_t offset, int mutability)
{
  (void)mutability;

  package_t* package = (package_t*)object;
  package_t* dst = (package_t*)((uintptr_t)buf + offset);

  dst->path = (const char*)pony_serialise_offset(ctx, (char*)package->path);
  dst->qualified_name = (const char*)pony_serialise_offset(ctx,
    (char*)package->qualified_name);
  dst->id = (const char*)pony_serialise_offset(ctx, (char*)package->id);
  dst->filename = (const char*)pony_serialise_offset(ctx,
    (char*)package->filename);
  dst->symbol = (const char*)pony_serialise_offset(ctx, (char*)package->symbol);

  dst->ast = (ast_t*)pony_serialise_offset(ctx, package->ast);
  package_set_serialise(ctx, &package->dependencies, buf,
    offset + offsetof(package_t, dependencies), PONY_TRACE_MUTABLE);
  dst->group = (package_group_t*)pony_serialise_offset(ctx, package->group);

  dst->group_index = package->group_index;
  dst->next_hygienic_id = package->next_hygienic_id;
  dst->low_index = package->low_index;
  dst->allow_ffi = package->allow_ffi;
  dst->on_stack = package->on_stack;
}


static void package_deserialise(pony_ctx_t* ctx, void* object)
{
  package_t* package = (package_t*)object;

  package->path = string_deserialise_offset(ctx, (uintptr_t)package->path);
  package->qualified_name = string_deserialise_offset(ctx,
    (uintptr_t)package->qualified_name);
  package->id = string_deserialise_offset(ctx, (uintptr_t)package->id);
  package->filename = string_deserialise_offset(ctx,
    (uintptr_t)package->filename);
  package->symbol = string_deserialise_offset(ctx, (uintptr_t)package->symbol);

  package->ast = (ast_t*)pony_deserialise_offset(ctx, ast_pony_type(),
    (uintptr_t)package->ast);
  package_set_deserialise(ctx, &package->dependencies);
  package->group = (package_group_t*)pony_deserialise_offset(ctx,
    package_group_pony_type(), (uintptr_t)package->group);
}


static pony_type_t package_pony =
{
  0,
  sizeof(package_t),
  0,
  0,
  NULL,
  NULL,
  package_serialise_trace,
  package_serialise,
  package_deserialise,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};


pony_type_t* package_pony_type()
{
  return &package_pony;
}


static void package_group_serialise_trace(pony_ctx_t* ctx, void* object)
{
  package_group_t* group = (package_group_t*)object;

  if(group->signature != NULL)
    pony_serialise_reserve(ctx, group->signature, SIGNATURE_LENGTH);

  package_set_serialise_trace(ctx, &group->members);
}


static void package_group_serialise(pony_ctx_t* ctx, void* object, void* buf,
  size_t offset, int mutability)
{
  (void)ctx;
  (void)mutability;

  package_group_t* group = (package_group_t*)object;
  package_group_t* dst = (package_group_t*)((uintptr_t)buf + offset);

  uintptr_t ptr_offset = pony_serialise_offset(ctx, group->signature);
  dst->signature = (char*)ptr_offset;

  if(group->signature != NULL)
  {
    char* dst_sig = (char*)((uintptr_t)buf + ptr_offset);
    memcpy(dst_sig, group->signature, SIGNATURE_LENGTH);
  }

  package_set_serialise(ctx, &group->members, buf,
    offset + offsetof(package_group_t, members), PONY_TRACE_MUTABLE);
}


static void package_group_deserialise(pony_ctx_t* ctx, void* object)
{
  package_group_t* group = (package_group_t*)object;

  group->signature = (char*)pony_deserialise_block(ctx,
    (uintptr_t)group->signature, SIGNATURE_LENGTH);
  package_set_deserialise(ctx, &group->members);
}


static pony_type_t package_group_pony =
{
  0,
  sizeof(package_group_t),
  0,
  0,
  NULL,
  NULL,
  package_group_serialise_trace,
  package_group_serialise,
  package_group_deserialise,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};


pony_type_t* package_group_pony_type()
{
  return &package_group_pony;
}


bool is_path_absolute(const char* path)
{
  // Begins with /
  if(path[0] == '/')
    return true;

#if defined(PLATFORM_IS_WINDOWS)
  // Begins with \ or ?:
  if(path[0] == '\\')
    return true;

  if((path[0] != '\0') && (path[1] == ':'))
    return true;
#endif

  return false;
}


bool is_path_relative(const char* path)
{
  if(path[0] == '.')
  {
    // Begins with ./
    if(path[1] == '/')
      return true;

#if defined(PLATFORM_IS_WINDOWS)
    // Begins with .\ on windows
    if(path[1] == '\\')
      return true;
#endif

    if(path[1] == '.')
    {
      // Begins with ../
      if(path[2] == '/')
        return true;

#if defined(PLATFORM_IS_WINDOWS)
      // Begins with ..\ on windows
      if(path[2] == '\\')
        return true;
#endif
    }
  }

  return false;
}
