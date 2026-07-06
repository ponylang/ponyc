#include "package.h"
#include "program.h"
#include "use.h"
#include "../codegen/codegen.h"
#include "../ast/source.h"
#include "../ast/parser.h"
#include "../ast/ast.h"
#include "../ast/token.h"
#include "../expr/literal.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>


#define EXTENSION ".pony"
#define C_EXTENSION ".c"


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

DECLARE_HASHMAP(package_set, package_set_t, package_t)

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
  // C shim state. These strlists preserve insertion order (tail-append) and
  // gencshim reads them head->tail: c_includes/c_defines accumulate in AST order
  // (sorted-file order, then source order within a file) as the scope pass
  // visits use commands, and c_sources is appended in sorted-directory order.
  // That ordering is what makes the clang argv and the shim object link order
  // deterministic for reproducible builds - don't replace these with an
  // unordered container. c_define_uses parallels c_defines (the use command
  // that added each define) for the duplicate-definition error frame.
  strlist_t* c_includes;
  strlist_t* c_defines;
  astlist_t* c_define_uses;
  strlist_t* c_sources;
  // The first cdefine:/cincludedir: directive seen for this package, used to
  // locate the error when such a directive lands in a package with no .c
  // sources to apply it to.
  ast_t* c_first_flag_use;
  bool allow_ffi;
  bool on_stack;
};

// A strongly connected component in the package dependency graph
struct package_group_t
{
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

DECLARE_STACK(package_stack, package_stack_t, package_t)
DEFINE_STACK(package_stack, package_stack_t, package_t)

DEFINE_LIST(package_group_list, package_group_list_t, package_group_t,
  NULL, package_group_free)


static size_t package_hash(package_t* pkg)
{
  // Hash the full string instead of the stringtab pointer. We want a
  // deterministic hash in order to enable deterministic hashmap iteration,
  // which in turn enables reproducible compilation output.
  return (size_t)ponyint_hash_str(pkg->qualified_name);
}


static bool package_cmp(package_t* a, package_t* b)
{
  return a->qualified_name == b->qualified_name;
}


DEFINE_HASHMAP(package_set, package_set_t, package_t, package_hash,
  package_cmp, NULL)


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
  source_t* source = source_open(file_path, &error_msg, opt->strtab);

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
  size_t c_count = 0;
  size_t c_buf_size = 4 * sizeof(const char*);
  const char** c_entries = (const char**)ponyint_pool_alloc_size(c_buf_size);
  PONY_DIRINFO* d;

  while((d = pony_dir_entry_next(dir)) != NULL)
  {
    // Handle only files with the specified extensions that don't begin with
    // a dot. This avoids including UNIX hidden files in a build.
    const char* name = stringtab(opt->strtab, pony_dir_info_name(d));

    if(name[0] == '.')
      continue;

    // A directory is never a source file, whatever its name. Skip it before
    // classifying by extension: otherwise a directory named like a source
    // file (e.g. foo.pony) reaches source_open, which on some filesystems
    // opens it, misreads its length via ftell, and aborts the compiler with a
    // huge allocation. Filtering here covers every source extension.
    char fullpath[FILENAME_MAX];
    path_cat(dir_path, name, fullpath);

    struct stat s;
    if((stat(fullpath, &s) == 0) && S_ISDIR(s.st_mode))
      continue;

    const char* p = strrchr(name, '.');

    if(p == NULL)
      continue;

    if(strcmp(p, EXTENSION) == 0)
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
    else if(strcmp(p, C_EXTENSION) == 0)
    {
      // C shim sources. These are not parsed as Pony (they never join
      // entries[]); gencshim compiles them with the embedded clang and links
      // the objects into the program.
      if((c_count * sizeof(const char*)) == c_buf_size)
      {
        size_t new_buf_size = c_buf_size * 2;
        c_entries = (const char**)ponyint_pool_realloc_size(c_buf_size,
          new_buf_size, c_entries);
        c_buf_size = new_buf_size;
      }

      c_entries[c_count++] = name;
    }
  }

  pony_closedir(dir);

  // In order for compilation output to be reproducible, file parsing order
  // must be deterministic too.
  qsort(entries, count, sizeof(const char*), string_compare);
  bool r = true;

  for(size_t i = 0; i < count; i++)
  {
    char fullpath[FILENAME_MAX];
    path_cat(dir_path, entries[i], fullpath);
    r &= parse_source_file(package, fullpath, opt);
  }

  // C shim sources are recorded in sorted order for the same reason: gencshim
  // compiles c_sources head->tail and appends each object to the link line
  // in that order, so this sort is what keeps the output reproducible.
  qsort(c_entries, c_count, sizeof(const char*), string_compare);

  package_t* pkg = (package_t*)ast_data(package);

  for(size_t i = 0; i < c_count; i++)
  {
    char fullpath[FILENAME_MAX];
    path_cat(dir_path, c_entries[i], fullpath);

    if(fullpath[0] == '\0')
    {
      errorf(errors, c_entries[i], "path to C source is too long");
      r = false;
      continue;
    }

    // Compiling a C shim is the package doing C, so --safe gates it exactly
    // like a C FFI call (verify_ffi_call): a package not on the safe list
    // doesn't get its .c compiled. The file is allowed to be there; what's
    // gated is compiling it. Checked here, at discovery, so it fails as
    // early as an unsafe FFI call would -- allow_ffi is already set
    // (create_package runs before this).
    if(!pkg->allow_ffi)
    {
      errorf(errors, fullpath, "this package isn't allowed to do C FFI, so "
        "its C source files can't be compiled as shims");
      r = false;
      continue;
    }

    pkg->c_sources = strlist_append(pkg->c_sources,
      stringtab(opt->strtab, fullpath));
  }

  ponyint_pool_free_size(buf_size, entries);
  ponyint_pool_free_size(c_buf_size, c_entries);
  return r;
}


// Check whether the directory specified by catting the given base and path
// exists
// @return The resulting directory path, which should not be deleted and is
// valid indefinitely. NULL is directory cannot be found.
static const char* try_path(const char* base, const char* path,
  bool* out_found_notdir, pass_opt_t* opt)
{
  char composite[FILENAME_MAX];
  char file[FILENAME_MAX];

  path_cat(base, path, composite);

  if(pony_realpath(composite, file) != file)
    return NULL;

  struct stat s;
  int err = stat(file, &s);

  if(err == -1)
    return NULL;

  if(!S_ISDIR(s.st_mode))
  {
    if(out_found_notdir != NULL)
      *out_found_notdir = true;

    return NULL;
  }

  return stringtab(opt->strtab, file);
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
static const char* try_package_path(const char* base, const char* path,
  bool* out_found_notdir, pass_opt_t* opt)
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

    const char* result = try_path(path2, path, out_found_notdir, opt);

    if(result != NULL)
      return result;
  } while(!is_root(path1));

  return NULL;
}


// Attempt to find the specified package directory in our search path
// @return The resulting directory path, which should not be deleted and is
// valid indefinitely. NULL is directory cannot be found.
static const char* find_path(ast_t* from, const char* path,
  bool* out_is_relative, bool* out_found_notdir, pass_opt_t* opt)
{
  if(out_is_relative != NULL)
    *out_is_relative = false;

  if(out_found_notdir != NULL)
    *out_found_notdir = false;

  // First check for an absolute path
  if(is_path_absolute(path))
    return try_path(NULL, path, out_found_notdir, opt);

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
  const char* result = try_path(base, path, out_found_notdir, opt);

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
      result = try_package_path(base, path, out_found_notdir, opt);

      if(result != NULL)
        return result;

      // Check ../pony_packages from the compiler target
      if((from != NULL) && (ast_id(from) == TK_PACKAGE))
      {
        ast_t* target = ast_child(ast_parent(from));
        package_t* pkg = (package_t*)ast_data(target);
        base = pkg->path;

        result = try_package_path(base, path, out_found_notdir, opt);

        if(result != NULL)
          return result;
      }
    }

    // Try the search paths
    for(strlist_t* p = opt->package_search_paths; p != NULL;
      p = strlist_next(p))
    {
      result = try_path(strlist_data(p), path, out_found_notdir, opt);

      if(result != NULL)
        return result;
    }
  }

  return NULL;
}


// Convert the given ID to a hygenic string. The resulting string should not be
// deleted and is valid indefinitely.
static const char* id_to_string(const char* prefix, size_t id, pass_opt_t* opt)
{
  if(prefix == NULL)
    prefix = "";

  size_t len = strlen(prefix);
  size_t buf_size = len + 32;
  char* buffer = (char*)ponyint_pool_alloc_size(buf_size);
  snprintf(buffer, buf_size, "%s$" __zu, prefix, id);
  return stringtab_consume(opt->strtab, buffer, buf_size);
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


static const char* string_to_symbol(const char* string, pass_opt_t* opt)
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

  return stringtab_consume(opt->strtab, buf, buf_size);
}


static const char* symbol_suffix(const char* symbol, size_t suffix,
  pass_opt_t* opt)
{
  size_t len = strlen(symbol);
  size_t buf_size = len + 32;
  char* buf = (char*)ponyint_pool_alloc_size(buf_size);
  snprintf(buf, buf_size, "%s" __zu, symbol, suffix);

  return stringtab_consume(opt->strtab, buf, buf_size);
}


static const char* create_package_symbol(ast_t* program, const char* filename,
  pass_opt_t* opt)
{
  const char* symbol = string_to_symbol(filename, opt);
  size_t suffix = 1;

  while(symbol_in_use(program, symbol))
  {
    symbol = symbol_suffix(symbol, suffix, opt);
    suffix++;
  }

  return symbol;
}


// Create a package AST, set up its state and add it to the given program
ast_t* create_package(ast_t* program, const char* name,
  const char* qualified_name, pass_opt_t* opt)
{
  ast_t* package = ast_blank(TK_PACKAGE);
  uint32_t pkg_id = program_assign_pkg_id(program);

  package_t* pkg = POOL_ALLOC(package_t);
  pkg->path = name;
  pkg->qualified_name = qualified_name;
  pkg->id = id_to_string(NULL, pkg_id, opt);

  const char* p = strrchr(pkg->path, PATH_SLASH);

  if(p == NULL)
    p = pkg->path;
  else
    p = p + 1;

  pkg->filename = stringtab(opt->strtab, p);

  if(pkg_id > 1)
    pkg->symbol = create_package_symbol(program, pkg->filename, opt);
  else
    pkg->symbol = NULL;

  pkg->ast = package;
  package_set_init(&pkg->dependencies, 1);
  pkg->group = NULL;
  pkg->group_index = -1;
  pkg->next_hygienic_id = 0;
  pkg->low_index = -1;
  pkg->c_includes = NULL;
  pkg->c_defines = NULL;
  pkg->c_define_uses = NULL;
  pkg->c_sources = NULL;
  pkg->c_first_flag_use = NULL;
  ast_setdata(package, pkg);

  ast_scope(package);
  ast_append(program, package);
  ast_set(program, pkg->path, package, SYM_NONE, false, opt->strtab);
  ast_set(program, pkg->id, package, SYM_NONE, false, opt->strtab);

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
    path = stringtab(opt->strtab, path);
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
  path = stringtab(opt->strtab, path);
  strlist_t* safe = opt->safe_packages;

  if(strlist_find(safe, path) == NULL)
    opt->safe_packages = strlist_append(safe, path);

  return true;
}


// Determine the absolute path of the directory the current executable is in
// and add it to our search path
static bool add_pony_installation_dir(const char* path, pass_opt_t* opt)
{
  // TODO STA:
  // validate path and add error if it doesn't exist.
  // we should really validate it contains all the directories
  // we expect
  bool success = true;

  add_path(path, opt);

  // Allow ponyc to find the lib directory when it is installed. The install
  // layout must match this offset: Windows installs libs flat under <ver>/lib,
  // Unix under <ver>/lib/<arch>. See .known-couplings/install-arch-dir.md.
#ifdef PLATFORM_IS_WINDOWS
  success = add_relative_path(path, "..\\lib", opt);
#else
  const char* link_arch = opt->link_arch != NULL ? opt->link_arch
                                              : PONY_ARCH;
  size_t lib_len = 8 + strlen(link_arch);
  char* lib_path = (char*)ponyint_pool_alloc_size(lib_len);
  snprintf(lib_path, lib_len, "../lib/%s", link_arch);

  success = add_relative_path(path, lib_path, opt);

  ponyint_pool_free_size(lib_len, lib_path);

  if(!success)
    return false;

  // for when run from build directory
  lib_len = 5 + strlen(link_arch);
  lib_path = (char*)ponyint_pool_alloc_size(lib_len);
  snprintf(lib_path, lib_len, "lib/%s", link_arch);

  success = add_relative_path(path, lib_path, opt);

  ponyint_pool_free_size(lib_len, lib_path);
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

static bool add_exec_dir(pass_opt_t* opt)
{
  char path[FILENAME_MAX];
  bool success = get_compiler_exe_directory(path, opt->argv0);
  errors_t* errors = opt->check.errors;

  if(!success)
  {
    errorf(errors, NULL, "Error determining executable path or directory.");
    return false;
  }

  add_path(path, opt);

  return add_pony_installation_dir(path, opt);
}

bool package_init(pass_opt_t* opt)
{
  // Command line --path entries have already been added to
  // package_search_paths during option parsing. We need the standard library
  // paths to come first so that --path directories cannot shadow stdlib
  // packages. This is the same approach used to fix PONYPATH shadowing in
  // https://github.com/ponylang/ponyc/issues/3779 — save the existing paths,
  // add stdlib paths first, then re-append the saved paths after.
  strlist_t* cmdline_paths = opt->package_search_paths;
  opt->package_search_paths = NULL;

  if(!add_exec_dir(opt))
  {
    strlist_free(cmdline_paths);
    errorf(opt->check.errors, NULL, "Error adding package paths relative to ponyc binary location");
    return false;
  }

  // Re-append command line paths after the standard library paths.
  for(strlist_t* p = cmdline_paths; p != NULL; p = strlist_next(p))
  {
    const char* path = strlist_data(p);

    if(strlist_find(opt->package_search_paths, path) == NULL)
      opt->package_search_paths = strlist_append(opt->package_search_paths,
        path);
  }
  strlist_free(cmdline_paths);

  package_add_paths(getenv("PONYPATH"), opt);

  // Finally we add OS specific paths.
#ifdef PLATFORM_IS_POSIX_BASED
  add_path("/usr/local/lib", opt);
  add_path("/opt/local/lib", opt);
#endif
#ifdef PLATFORM_IS_DRAGONFLY
  add_path("/usr/local/cxx_atomics", opt);
#endif

  // Convert all the safe packages to their full paths.
  strlist_t* full_safe = NULL;
  strlist_t* safe = opt->safe_packages;

  while(safe != NULL)
  {
    const char* path;
    safe = strlist_pop(safe, &path);

    // Lookup (and hence normalise) path.
    path = find_path(NULL, path, NULL, NULL, opt);

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

  return true;
}

bool package_init_lib(pass_opt_t* opt, const char* pony_installation)
{
  // TODO STA: does this need more than this subset of package_init?
  package_add_paths(getenv("PONYPATH"), opt);
  if(!add_pony_installation_dir(pony_installation, opt))
  {
    errorf(opt->check.errors, NULL, "Error adding package paths relative to ponyc installation location");
    return false;
  }

  return true;
}

bool handle_path_list(const char* paths, path_fn f, pass_opt_t* opt)
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

      memcpy(path, paths, len);
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
  n->path = stringtab(opt->strtab, path);
  n->src = src;
  n->mapped_path = NULL;
  n->next = opt->magic_packages;
  opt->magic_packages = n;
}


void package_add_magic_path(const char* path, const char* mapped_path,
  pass_opt_t* opt)
{
  magic_package_t* n = POOL_ALLOC(magic_package_t);
  n->path = stringtab(opt->strtab, path);
  n->src = NULL;
  n->mapped_path = stringtab(opt->strtab, mapped_path);
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
  if(package_load(program, stringtab(opt->strtab, "builtin"), opt) == NULL ||
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
    bool found_notdir = false;
    full_path = find_path(from, path, &is_relative, &found_notdir, opt);

    if(full_path == NULL)
    {
      errorf(opt->check.errors, path, "couldn't locate this path");

      if(found_notdir)
        errorf_continue(opt->check.errors, path, "note that a compiler "
          "invocation or a 'use' directive must refer to a directory");

      return NULL;
    }

    if((from != program) && is_relative)
    {
      // Package to load is relative to from, build the qualified name
      // The qualified name should be relative to the program being built
      // and account for the relative path we were provided not being relative
      // the program being built
      ast_t* parent = ast_nearest(from, TK_PACKAGE);
      package_t* parent_pkg = (package_t*)ast_data(parent);

      if(parent_pkg != NULL)
      {
        const char* package_path = path;

        size_t relatives = 0;
        while(true)
        {
          if(strncmp("../", package_path, 3) == 0)
          {
            package_path += 3;
            relatives += 1;
          } else if(strncmp("./", package_path, 2) == 0)
          {
            package_path += 2;
          }
          else
          {
            break;
          }
        }

        const char* base_name = parent_pkg->qualified_name;
        size_t base_name_len = strlen(base_name);
        while(relatives > 0 && base_name_len > 0)
        {
          if(base_name[base_name_len - 1] == '/')
          {
            relatives -= 1;
          }

          base_name_len -= 1;
        }

        size_t package_path_len = strlen(package_path);
        size_t len = base_name_len + package_path_len + 2;
        char* q_name = (char*)ponyint_pool_alloc_size(len);
        memcpy(q_name, base_name, base_name_len);
        q_name[base_name_len] = '/';
        memcpy(q_name + base_name_len + 1, package_path, package_path_len);
        q_name[len - 1] = '\0';
        qualified_name = stringtab_consume(opt->strtab, q_name, len);
      }
    }

    // we are loading the package specified as program dir
    if(from == program)
    {
      // construct the qualified name from the basename of the full path
      const char* basepath = strrchr(full_path, PATH_SLASH);
      if(basepath == NULL)
      {
        basepath = full_path;
      } else {
        basepath = basepath + 1;
      }
      qualified_name = basepath;
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
    // A .c shim contributes no Pony AST, so a directory holding only .c
    // files is still not a package: shims travel with Pony code, they don't
    // replace it.
    ast_error(opt->check.errors, package,
      "no Pony source files in package '%s'", path);
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
    strlist_free(package->c_includes);
    strlist_free(package->c_defines);
    astlist_free(package->c_define_uses);
    strlist_free(package->c_sources);
    POOL_FREE(package_t, package);
  }
}


const char* package_name(ast_t* ast)
{
  package_t* pkg = (package_t*)ast_data(ast_nearest(ast, TK_PACKAGE));
  return (pkg == NULL) ? NULL : pkg->id;
}


ast_t* package_id(ast_t* ast, pass_opt_t* opt)
{
  return ast_from_string(ast, package_name(ast), opt->strtab);
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


const char* package_hygienic_id(typecheck_t* t, pass_opt_t* opt)
{
  pony_assert(t->frame->package != NULL);
  package_t* pkg = (package_t*)ast_data(t->frame->package);
  size_t id = pkg->next_hygienic_id++;

  return id_to_string(pkg->id, id, opt);
}


bool package_allow_ffi(typecheck_t* t)
{
  pony_assert(t->frame->package != NULL);
  package_t* pkg = (package_t*)ast_data(t->frame->package);
  return pkg->allow_ffi;
}


// The macro name of a cdefine: directive is the text before '=' (or the
// whole directive when there is no '='); the value after '=' is opaque.
static bool c_define_name_eq(const char* a, const char* b)
{
  size_t a_len = strcspn(a, "=");
  size_t b_len = strcspn(b, "=");

  return (a_len == b_len) && (strncmp(a, b, a_len) == 0);
}


bool use_cincludedir(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options)
{
  (void)name;
  pony_assert(use != NULL);
  pony_assert(locator != NULL);

  if(locator[0] == '\0')
  {
    ast_error(options->check.errors, use, "cincludedir: requires a path");
    return false;
  }

  char absolute[FILENAME_MAX];
  const char* prefix = NULL;

  if(!is_path_absolute(locator))
    prefix = package_path(ast_nearest(use, TK_PACKAGE));

  path_cat(prefix, locator, absolute);

  if(absolute[0] == '\0')
  {
    ast_error(options->check.errors, use, "cincludedir: path is too long");
    return false;
  }

  // The path is stored raw - not through quoted_locator() - because include
  // paths may legitimately contain characters that helper rejects (spaces in
  // particular). That's safe: it is handed to clang as a single argv element,
  // never through a shell. Duplicates are allowed; an include search list is
  // additive, so repeats are harmless.
  ast_t* pkg_ast = ast_nearest(use, TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(pkg_ast);
  pkg->c_includes = strlist_append(pkg->c_includes,
    stringtab(options->strtab, absolute));

  if(pkg->c_first_flag_use == NULL)
    pkg->c_first_flag_use = use;

  return true;
}


bool use_cdefine(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options)
{
  (void)name;
  pony_assert(use != NULL);
  pony_assert(locator != NULL);

  size_t name_len = strcspn(locator, "=");

  if(name_len == 0)
  {
    ast_error(options->check.errors, use, "cdefine: requires a macro name");
    return false;
  }

  // The macro name must be a C identifier. This is what keeps the duplicate
  // check sound: clang's -D accepts trickier forms (function-like macros
  // "FOO(x)=...", "NAME VALUE" with a space) whose effective macro name is
  // not the text before '=', so they would dodge the check and silently
  // shadow. Rejecting them is also the conservative scope: function-like
  // macro support can be added later if demand shows; un-rejecting is easy.
  bool valid_name = (locator[0] == '_') || isalpha((unsigned char)locator[0]);

  for(size_t i = 1; valid_name && (i < name_len); i++)
  {
    valid_name = (locator[i] == '_')
      || isalnum((unsigned char)locator[i]);
  }

  if(!valid_name)
  {
    ast_error(options->check.errors, use,
      "cdefine: macro name (the text before '=') must be a C identifier");
    return false;
  }

  ast_t* pkg_ast = ast_nearest(use, TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(pkg_ast);

  // A macro name is a declaration: defining it twice for one package is an
  // error even when the values match, like `let a = 1` twice. Guards have
  // already been evaluated (a guarded-out directive never reaches this
  // handler), so only directives active for the current target are checked.
  strlist_t* d = pkg->c_defines;
  astlist_t* u = pkg->c_define_uses;

  while(d != NULL)
  {
    const char* prior = strlist_data(d);

    if(c_define_name_eq(prior, locator))
    {
      if(strcmp(prior, locator) == 0)
        ast_error(options->check.errors, use,
          "C macro '%.*s' is already defined for this package",
          (int)name_len, locator);
      else
        ast_error(options->check.errors, use,
          "C macro '%.*s' is already defined for this package as '%s'",
          (int)name_len, locator, prior);

      ast_error_continue(options->check.errors, astlist_data(u),
        "first definition is here");
      return false;
    }

    d = strlist_next(d);
    u = astlist_next(u);
  }

  // The value (everything after '=', if any) is stored raw and handed to
  // clang as a single argv element - see use_cincludedir() for why that's safe.
  pkg->c_defines = strlist_append(pkg->c_defines, locator);
  pkg->c_define_uses = astlist_append(pkg->c_define_uses, use);

  if(pkg->c_first_flag_use == NULL)
    pkg->c_first_flag_use = use;

  return true;
}


strlist_t* package_c_includes(ast_t* package)
{
  pony_assert(package != NULL);
  pony_assert(ast_id(package) == TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(package);
  pony_assert(pkg != NULL);

  return pkg->c_includes;
}


strlist_t* package_c_defines(ast_t* package)
{
  pony_assert(package != NULL);
  pony_assert(ast_id(package) == TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(package);
  pony_assert(pkg != NULL);

  return pkg->c_defines;
}


strlist_t* package_c_sources(ast_t* package)
{
  pony_assert(package != NULL);
  pony_assert(ast_id(package) == TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(package);
  pony_assert(pkg != NULL);

  return pkg->c_sources;
}


ast_t* package_c_first_flag_use(ast_t* package)
{
  pony_assert(package != NULL);
  pony_assert(ast_id(package) == TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(package);
  pony_assert(pkg != NULL);

  return pkg->c_first_flag_use;
}


const char* package_alias_from_id(ast_t* module, const char* id,
  pass_opt_t* opt)
{
  pony_assert(ast_id(module) == TK_MODULE);

  const char* strtab_id = stringtab(opt->strtab, id);

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
  package_set_init(&group->members, 1);
  return group;
}


void package_group_free(package_group_t* group)
{
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


void package_group_dump(package_group_t* group)
{
  package_set_t deps;
  package_set_init(&deps, 1);

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


void package_done(pass_opt_t* opt)
{
  strlist_free(opt->package_search_paths);
  opt->package_search_paths = NULL;

  strlist_free(opt->safe_packages);
  opt->safe_packages = NULL;

  package_clear_magic(opt);
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
