#ifndef PACKAGE_H
#define PACKAGE_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ds/stringtab.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

typedef struct package_t package_t;

/**
 * Initialises the search directories. This is composed of a "packages"
 * directory relative to the executable, plus a collection of directories
 * specified in the PONYPATH environment variable.
 *
 * @param name The path to the executable file, generally argv[0]. The real
 *   path will be determined from argv[0].
 *
 * @param opt Pass options, used to initialise the code generator.
 */
bool package_init(const char* name, pass_opt_t* opt);

/**
 * Gets the list of search paths.
 */
strlist_t* package_paths();

/**
 * Appends a list of paths to the list of paths that will be searched for
 * packages.
 * Path list is semicolon (;) separated on Windows and colon (:) separated on
 * Linux and MacOS.
 */
void package_add_paths(const char* paths);

/**
 * Add a magic package. When the package with the specified path is requested
 * the files will not be looked for and the source code given here will be used
 * instead. Each magic package can consist of only a single module.
 * The specified path is not expanded or normalised and must exactly match that
 * requested.
 */
void package_add_magic(const char* path, const char* src);

/**
 * Suppress printing "building" message for each package.
 */
void package_suppress_build_message();

/**
 * Load a program. The path specifies the package that represents the program.
 */
ast_t* program_load(const char* path, pass_opt_t* options);

/**
 * Load a package. Used by program_load() and when handling 'use' statements.
 */
ast_t* package_load(ast_t* from, const char* path, pass_opt_t* options);

/**
 * Free the package_t that is set as the ast_data of a package node.
 */
void package_free(package_t* package);

/**
 * Gets the package name, but not wrapped in an AST node.
 */
const char* package_name(ast_t* ast);

/**
 * Gets an AST ID node with a string set to the unique ID of the packaged. The
 * first package loaded will be $0, the second $1, etc.
 */
ast_t* package_id(ast_t* ast);

/**
 * Gets the package path.
 */
const char* package_path(ast_t* package);

/**
 * Gets the last component of the package path.
 */
const char* package_filename(ast_t* package);

/**
 * Gets an AST ID node with a string set to a hygienic ID. Hygienic IDs are
 * handed out on a per-package basis. The first one will be $0, the second $1,
 * etc.
 */
ast_t* package_hygienic_id(ast_t* ast);

/**
 * Gets a string set to a hygienic ID. Hygienic IDs are handed out on a
 * per-package basis. The first one will be $0, the second $1, etc.
 * The returned string will be a string table entry and should not be freed.
 */
const char* package_hygienic_id_string(ast_t* ast);

/**
 * Cleans up the list of search directories and shuts down the code generator.
 */
void package_done(pass_opt_t* opt);

PONY_EXTERN_C_END

#endif
