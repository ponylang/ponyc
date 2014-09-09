#ifndef CODEGEN_GENNAME_H
#define CODEGEN_GENNAME_H

#include "codegen.h"

const char* genname_type(ast_t* ast);

const char* genname_trace(const char* type);

const char* genname_serialise(const char* type);

const char* genname_deserialise(const char* type);

const char* genname_dispatch(const char* type);

const char* genname_finalise(const char* type);

const char* genname_descriptor(const char* type);

const char* genname_instance(const char* type);

const char* genname_fun(const char* type, const char* name, ast_t* typeargs);

const char* genname_handler(const char* type, const char* name,
  ast_t* typeargs);

const char* genname_box(const char* name);

const char* genname_unbox(const char* name);

#endif
