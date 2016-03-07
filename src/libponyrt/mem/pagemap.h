#ifndef mem_pagemap_h
#define mem_pagemap_h

#include <platform.h>

PONY_EXTERN_C_BEGIN

void* ponyint_pagemap_get(const void* m);

void ponyint_pagemap_set(const void* m, void* v);

PONY_EXTERN_C_END

#endif
