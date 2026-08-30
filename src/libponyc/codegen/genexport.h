#ifndef CODEGEN_GENEXPORT_H
#define CODEGEN_GENEXPORT_H

#include <platform.h>
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

/** Generate C headers for all exported types in the program. */
bool genexport_header(ast_t* program, pass_opt_t* opt);

/** Return true if the package contains any types with the \c_api\ annotation. */
bool package_has_exports(ast_t* package, pass_opt_t* opt);

/** Replace characters not valid in a C identifier with underscores, in place. */
void sanitize_c_name(char* name);

/** Extract the C-facing name from a use statement (alias or path basename). */
const char* resolve_use_name(ast_t* use_node);

/** Resolve a TK_TYPE alias to its underlying definition. For non-alias
 *  nodes, sets *def to node unchanged. Returns false when the alias
 *  cannot be resolved (caller should skip the node). */
bool resolve_export_alias(ast_t* node, token_id nid, ast_t** def,
  token_id* def_nid, ast_t** typeargs, ast_t** class_typeparams);

/** Return true if the method should be exported: public, non-bare,
 *  non-generic, non-partial, no tuple types. */
bool should_export_method(ast_t* method, ast_t* class_typeparams,
  ast_t* typeargs);

PONY_EXTERN_C_END

#endif
