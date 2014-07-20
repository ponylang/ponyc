#ifndef PACKAGE_H
#define PACKAGE_H

#include "../ast/ast.h"

/**
 * Initialises the search directories. This is composed of a "packages"
 * directory relative to the executable, plus a collection of directories
 * specified in the PONYPATH environment variable.
 *
 * @param name The path to the executable file, generally argv[0]. The real
 *   path will be determined from argv[0].
 */
void package_init(const char* name);

/**
 * Appends a list of colon (:) separated paths to the list of paths that will
 * be searched for packages.
 */
void package_paths(const char* paths);

/**
 * Load a program. The path specifies the package that represents the program.
 * If parse_only is set to true, the program will not be typechecked, which
 * also means 'use' statements won't be followed.
 */
ast_t* program_load(const char* path, bool parse_only);

/**
 * Compile a program.
 */
bool program_compile(ast_t* program, int opt, bool print_llvm);

/**
 * Loads a package. Used by program_load() and when handling 'use' statements.
 */
ast_t* package_load(ast_t* from, const char* path, bool parse_only);

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
 * Gets the last component of the package path.
 */
const char* package_filename(ast_t* ast);

/**
 * Gets an AST ID node with a string set to a hygienic ID. Hygienic IDs are
 * handed out on a per-package basis. The first one will be $0, the second $1,
 * etc.
 */
ast_t* package_hygienic_id(ast_t* ast);

/**
 * Cleans up the list of search directories.
 */
void package_done();

#endif
