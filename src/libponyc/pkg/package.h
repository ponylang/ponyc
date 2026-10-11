#ifndef PACKAGE_H
#define PACKAGE_H

#include <platform.h>
#include "../libponyrt/ds/list.h"
#include "../ast/stringtab.h"

#include <stdio.h>

PONY_EXTERN_C_BEGIN

typedef struct module_t module_t;
typedef struct module_group_t module_group_t;
typedef struct magic_module_t magic_module_t;
typedef struct pass_opt_t pass_opt_t;
typedef struct ast_t ast_t;
typedef struct typecheck_t typecheck_t;

DECLARE_LIST(module_group_list, module_group_list_t,
  module_group_t)

// Function that will handle a path in some way.
typedef bool (*path_fn)(const char* path, pass_opt_t* opt);

/**
 * Cat together the 2 given path fragments into the given buffer.
 * The first path fragment may be absolute, relative or NULL
 */
void path_cat(const char* part1, const char* part2, char result[FILENAME_MAX]);

bool handle_path_list(const char* paths, path_fn f, pass_opt_t* opt);

/**
 * Initialises the search directories. This is composed of a "modules"
 * directory relative to the executable, plus a collection of directories
 * specified in the PONYPATH environment variable.
 */
bool module_init(pass_opt_t* opt);

/**
 * TODO STA: this exists for turning libponyc into a reusable library
 * and needs to be rethought
 */
bool module_init_lib(pass_opt_t* opt, const char* pony_installation);

/**
 * Appends a list of paths to the list of paths that will be searched for
 * modules.
 * Path list is semicolon (;) separated on Windows and colon (:) separated on
 * Linux and MacOS.
 */
void module_add_paths(const char* paths, pass_opt_t* opt);

/**
 * Add a library search path for the linker. The path is searched before
 * auto-discovered system paths and does not affect Pony module resolution.
 */
void package_add_lib_path(const char* path, pass_opt_t* opt);

/**
 * Appends a list of paths to the list of modules allowed to do C FFI.
 * The list is semicolon (;) separated on Windows and colon (:) separated on
 * Linux and MacOS.
 * If this is never called, all modules are allowed to do C FFI.
 */
bool module_add_safe(const char* paths, pass_opt_t* opt);

/**
 * Add a magic module. When the module with the specified path is requested
 * the files will not be looked for and the source code given here will be used
 * instead. Each magic module can consist of only a single file.
 * The specified path is not expanded or normalised and must exactly match that
 * requested.
 */
void module_add_magic_src(const char* path, const char* src, pass_opt_t* opt);

/**
 * Add a magic module. Same as module_add_magic_src but uses an alternative
 * path instead.
 */
void module_add_magic_path(const char* path, const char* mapped_path,
  pass_opt_t* opt);

/**
 * Clear any magic modules that have been added.
 */
void module_clear_magic(pass_opt_t* opt);

/**
 * Load a program. The path specifies the module that represents the program.
 */
ast_t* program_load(const char* path, pass_opt_t* opt);

/**
 * Load a module. Used by program_load() and when handling 'use' statements.
 */
ast_t* module_load(ast_t* from, const char* path, pass_opt_t* opt);

/**
 * Create a module AST, set up its state and add it to the given program.
 * used by module_load. Should not be used elsewhere but for setting up fake
 * module for testing.
 */
ast_t* create_module(ast_t* program, const char* name,
  const char* qualified_name, pass_opt_t* opt);

/**
 * Free the module_t that is set as the ast_data of a module node.
 */
void module_free(module_t* module);

/**
 * Gets the module name, but not wrapped in an AST node.
 */
const char* module_name(ast_t* ast);

/**
 * Gets an AST ID node with a string set to the unique ID of the module. The
 * first module loaded will be $0, the second $1, etc.
 */
ast_t* module_id(ast_t* ast, pass_opt_t* opt);

/**
 * Gets the module path.
 */
const char* module_path(ast_t* module);

/**
 * Gets the module qualified name.
 */
const char* module_qualified_name(ast_t* module);

/**
 * Gets the last component of the module path.
 */
const char* module_filename(ast_t* module);

/**
 * Gets the symbol wart for the module.
 */
const char* module_symbol(ast_t* module);

/**
 * Gets a string set to a hygienic ID. Hygienic IDs are handed out on a
 * per-module basis. The first one will be $0, the second $1, etc.
 * The returned string will be a string table entry and should not be freed.
 */
const char* module_hygienic_id(typecheck_t* t, pass_opt_t* opt);

/**
 * Returns true if the current module is allowed to do C FFI.
 */
bool module_allow_ffi(typecheck_t* t);

/// Process a "cincludedir:" scheme use command.
bool use_cincludedir(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options);

/// Process a "cdefine:" scheme use command.
bool use_cdefine(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options);

/**
 * C include search paths for the module's shim sources, in insertion order.
 * Relative cincludedir: paths have already been resolved against the module
 * directory.
 */
strlist_t* module_c_includes(ast_t* module);

/**
 * C preprocessor defines for the module's shim sources, in insertion order.
 * Each entry is the raw directive text ("NAME" or "NAME=VALUE").
 */
strlist_t* module_c_defines(ast_t* module);

/**
 * Full paths of the module's C shim sources, in sorted order.
 */
strlist_t* module_c_sources(ast_t* module);

/**
 * The first cdefine:/cincludedir: directive recorded for the module, or NULL
 * if none. Used to locate the error when such a directive lands in a
 * module with no C sources.
 */
ast_t* module_c_first_flag_use(ast_t* module);

/**
 * Whether this module's C shims need -I<output> for export headers.
 */
bool module_needs_export_include(ast_t* module);

/**
 * Mark this module as needing -I<output> for export headers.
 */
void module_set_needs_export_include(ast_t* module);

/**
 * Gets the alias of a module in the current file from the hygienic ID
 * of that module. Returns NULL if there is no alias. The module must have
 * been imported in the current file.
 *
 * For example, if the module `foo` was imported in the current file with
 * `use foo = "foo"` and the global ID of the module "foo" is `$2`, the call
 * `module_alias_from_id(current_module_ast, "$2")` will return the string
 * "foo".
 */
const char* module_alias_from_id(ast_t* module, const char* id, pass_opt_t* opt);

/**
 * Adds a module to the dependency list of another module.
 */
void module_add_dependency(ast_t* module, ast_t* dep);

size_t module_group_index(ast_t* module);

module_group_t* module_group_new();

void module_group_free(module_group_t* group);

/**
 * Build a list of the dependency groups (the strongly connected components) in
 * the module dependency graph. The list is topologically sorted.
 */
module_group_list_t* module_dependency_groups(ast_t* first_module);

void module_group_dump(module_group_t* group);

/**
 * Cleans up the list of search directories.
 */
void module_done(pass_opt_t* opt);

bool is_path_absolute(const char* path);

bool is_path_relative(const char* path);

PONY_EXTERN_C_END

#endif
