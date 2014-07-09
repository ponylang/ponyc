#ifndef TYPE_NOMINAL_H
#define TYPE_NOMINAL_H

#include "../ast/ast.h"

/// Checks if an identifier is a valid type name.
bool is_type_id(const char* s);

/// Creates an AST node for a builtin type, where 'name' is not in the stringtab
ast_t* nominal_builtin(ast_t* from, const char* name);

/// Creates an AST node for a builtin type with 1 typearg
ast_t* nominal_builtin1(ast_t* from, const char* name, ast_t* typearg0);

/// Creates an AST node for a package and type, both in the stringtab
ast_t* nominal_type(ast_t* from, const char* package, const char* name);

/// Creates an AST node for a package and type with 1 typearg
ast_t* nominal_type1(ast_t* from, const char* package, const char* name,
  ast_t* typearg0);

/// Creates an AST node for a package and type, both in the stringtab
/// Does not validate the type. Use at the sugar stage only.
ast_t* nominal_sugar(ast_t* from, const char* package, const char* name);

/// Makes sure type arguments are within constraints
bool nominal_valid(ast_t* scope, ast_t** ast);

ast_t* nominal_def(ast_t* scope, ast_t* nominal);

#endif
