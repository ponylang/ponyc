#ifndef mem_pagemap_h
#define mem_pagemap_h

#ifdef __cplusplus
extern "C" {
#endif

void* pagemap_get(const void* m);

void pagemap_set(const void* m, void* v);

#ifdef __cplusplus
}
#endif

#endif
