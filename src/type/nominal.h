#ifndef TYPE_NOMINAL_H
#define TYPE_NOMINAL_H

#include "../ast/ast.h"

/// Checks if an identifier is a valid type name.
bool is_type_id(const char* s);

/// Creates an AST node for a builtin type, where 'name' is not in the stringtab
ast_t* nominal_builtin(ast_t* from, const char* name);

/// Creates an AST node for a package and type, both in the stringtab
ast_t* nominal_type(ast_t* from, const char* package, const char* name);

/// Gets the type, trait, class or actor for a nominal type.
ast_t* nominal_def(ast_t* scope, ast_t* nominal);

/// Makes sure type arguments are within constraints
bool nominal_valid(ast_t* scope, ast_t* nominal);

#endif
