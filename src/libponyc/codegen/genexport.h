#ifndef CODEGEN_GENEXPORT_H
#define CODEGEN_GENEXPORT_H

#include <platform.h>
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

/** Generate C headers for all exported types in the program. */
bool genexport_header(ast_t* program, pass_opt_t* opt);

/** Return true if the package contains any TK_EXPORT nodes. */
bool package_has_exports(ast_t* package);

/** Extract the C-facing name from a use statement (alias or path basename). */
const char* resolve_use_name(ast_t* use_node);

/** Return true if the method's return type or any parameter is a tuple.
 *  Pass class_typeparams and typeargs to resolve type parameters, or NULL
 *  when type parameter context is unavailable. */
bool export_has_tuple_types(ast_t* method, ast_t* class_typeparams,
  ast_t* typeargs);

PONY_EXTERN_C_END

#endif
