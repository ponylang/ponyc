#ifndef sched_mutemap_h
#define sched_mutemap_h

#include "../pony.h"
#include <platform.h>
#include "../ds/hash.h"

PONY_EXTERN_C_BEGIN

DECLARE_HASHMAP(ponyint_muteset, muteset_t, pony_actor_t);

typedef struct muteref_t
{
  pony_actor_t* key;
  muteset_t value;
} muteref_t;

DECLARE_HASHMAP(ponyint_mutemap, mutemap_t, muteref_t);

muteref_t* ponyint_muteref_alloc(pony_actor_t* key);

void ponyint_muteref_free(muteref_t* mref);

PONY_EXTERN_C_END

#endif
