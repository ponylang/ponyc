#ifndef CODEGEN_GENNAME_H
#define CODEGEN_GENNAME_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

const char* genname_type(ast_t* ast);

const char* genname_type_and_cap(ast_t* ast);

const char* genname_alloc(const char* type);

const char* genname_traitlist(const char* type);

const char* genname_fieldlist(const char* type);

const char* genname_trace(const char* type);

const char* genname_serialise_trace(const char* type);

const char* genname_serialise(const char* type);

const char* genname_deserialise(const char* type);

const char* genname_dispatch(const char* type);

const char* genname_descriptor(const char* type);

const char* genname_instance(const char* type);

const char* genname_fun(token_id cap, const char* name, ast_t* typeargs);

const char* genname_be(const char* name);

const char* genname_box(const char* name);

const char* genname_unbox(const char* name);

const char* genname_program_fn(const char* program, const char* name);

PONY_EXTERN_C_END

#endif
