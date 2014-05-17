#ifndef FUNCTION_H
#define FUNCTION_H

#include "../ds/list.h"
#include "../ds/table.h"

typedef struct function_t function_t;

DECLARE_LIST(funlist, function_t);

DECLARE_TABLE(funtab, function_t);

function_t* function_new();

function_t* function_store(function_t* f);

bool function_eq(function_t* a, function_t* b);

bool function_sub(function_t* a, function_t* b);

void function_done();

#endif
