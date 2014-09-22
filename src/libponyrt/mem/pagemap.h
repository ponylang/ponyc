#ifndef mem_pagemap_h
#define mem_pagemap_h

#include <platform.h>

PONY_EXTERN_C_BEGIN

void* pagemap_get(const void* m);

void pagemap_set(const void* m, void* v);

PONY_EXTERN_C_END

#endif
