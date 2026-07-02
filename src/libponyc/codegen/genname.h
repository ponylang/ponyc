#ifndef CODEGEN_GENNAME_H
#define CODEGEN_GENNAME_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

const char* genname_type(ast_t* ast, strtable_t* strtab);

const char* genname_type_and_cap(ast_t* ast, strtable_t* strtab);

const char* genname_alloc(const char* type, strtable_t* strtab);

const char* genname_traitmap(const char* type, strtable_t* strtab);

const char* genname_fieldlist(const char* type, strtable_t* strtab);

const char* genname_trace(const char* type, strtable_t* strtab);

const char* genname_dispatch(const char* type, strtable_t* strtab);

#if defined(USE_RUNTIME_TRACING)
const char* genname_get_behavior_name(const char* type, strtable_t* strtab);

const char* genname_behavior_name(const char* type, const char* name,
  strtable_t* strtab);
#endif

const char* genname_descriptor(const char* type, strtable_t* strtab);

const char* genname_instance(const char* type, strtable_t* strtab);

const char* genname_fun(token_id cap, const char* name, ast_t* typeargs,
  strtable_t* strtab);

const char* genname_be(const char* name, strtable_t* strtab);

const char* genname_box(const char* name, strtable_t* strtab);

const char* genname_unbox(const char* name, strtable_t* strtab);

const char* genname_unsafe(const char* name, strtable_t* strtab);

const char* genname_program_fn(const char* program, const char* name,
  strtable_t* strtab);

PONY_EXTERN_C_END

#endif
