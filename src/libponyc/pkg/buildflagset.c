#include "buildflagset.h"
#include "platformfuns.h"
#include "../ast/stringtab.h"
#include "../../libponyrt/ds/hash.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>


// Mutually exclusive platform flag groups.
static const char* _os_flags[] =
{
  OS_FREEBSD_NAME,
  OS_LINUX_NAME,
  OS_MACOSX_NAME,
  OS_WINDOWS_NAME,
  "unknown_OS",
  NULL  // Terminator.
};

static const char* _arch_flags[] =
{
  OS_X86_NAME,
  OS_ARM_NAME,
  "unknown_arch",
  NULL  // Terminator.
};

static const char* _size_flags[] =
{
  OS_LP64_NAME,
  OS_LLP64_NAME,
  OS_ILP32_NAME,
  "unknown_size",
  NULL  // Terminator.
};

static bool _stringtabed = false;


// Replace all the strings in the _os_flags, _arch_flags and _size_flags
// arrays with stringtab'ed versions the first time this is called.
// This method of initialisatino is obviously not at all concurrency safe, but
// it works with unit tests trivially.
static void stringtab_mutexgroups()
{
  if(_stringtabed)
    return;

  for(size_t i = 0; _os_flags[i] != NULL; i++)
    _os_flags[i] = stringtab(_os_flags[i]);

  for(size_t i = 0; _arch_flags[i] != NULL; i++)
    _arch_flags[i] = stringtab(_arch_flags[i]);

  for(size_t i = 0; _size_flags[i] != NULL; i++)
    _size_flags[i] = stringtab(_size_flags[i]);

  _stringtabed = true;
}


typedef struct flag_t
{
  const char* name;
  bool value;
} flag_t;


// Functions required for our hash table type.
static size_t flag_hash(flag_t* flag)
{
  assert(flag != NULL);
  return hash_ptr(flag->name);
}

static bool flag_cmp(flag_t* a, flag_t* b)
{
  assert(a != NULL);
  assert(b != NULL);

  return a->name == b->name;
}

static flag_t* flag_dup(flag_t* flag)
{
  assert(flag != NULL);

  flag_t* f = POOL_ALLOC(flag_t);
  memcpy(f, flag, sizeof(flag_t));
  return f;
}

static void flag_free(flag_t* flag)
{
  assert(flag != NULL);
  POOL_FREE(flag_t, flag);
}

DECLARE_HASHMAP(flagtab, flag_t);
DEFINE_HASHMAP(flagtab, flag_t, flag_hash, flag_cmp, pool_alloc_size,
  pool_free_size, flag_free);


struct buildflagset_t
{
  bool have_os_flags;
  bool have_arch_flags;
  bool have_size_flags;
  bool started_enum;
  bool first_config_ready;
  flagtab_t* flags;
  uint32_t enum_os_flags;
  uint32_t enum_arch_flags;
  uint32_t enum_size_flags;
  char* text_buffer;    // Buffer for printing config.
  size_t buffer_size;   // Size allocated for text_buffer.
};


buildflagset_t* buildflagset_create()
{
  buildflagset_t* p = POOL_ALLOC(buildflagset_t);
  assert(p != NULL);

  p->have_os_flags = false;
  p->have_arch_flags = false;
  p->have_size_flags = false;
  p->started_enum = false;
  p->flags = POOL_ALLOC(flagtab_t);
  flagtab_init(p->flags, 8);
  p->text_buffer = NULL;
  p->buffer_size = 0;

  return p;
}


void buildflagset_free(buildflagset_t* set)
{
  if(set != NULL)
  {
    flagtab_destroy(set->flags);
    POOL_FREE(flagtab_t, set->flags);

    if(set->buffer_size > 0)
      pool_free_size(set->buffer_size, set->text_buffer);

    POOL_FREE(buildflagset_t, set);
  }
}


// Determine the index of the given OS flag, if it is one.
// Returns: index of flag into _os_flags array, or <0 if not an OS flag.
static ssize_t os_index(const char* flag)
{
  assert(flag != NULL);

  stringtab_mutexgroups();

  for(size_t i = 0; _os_flags[i] != NULL; i++)
    if(flag == _os_flags[i])  // Match found.
      return i;

  // Match not found.
  return -1;
}


// Determine the index of the given arch flag, if it is one.
// Returns: index of flag into _arch_flags array, or <0 if not an arch flag.
static ssize_t arch_index(const char* flag)
{
  assert(flag != NULL);

  stringtab_mutexgroups();

  for(size_t i = 0; _arch_flags[i] != NULL; i++)
    if(flag == _arch_flags[i])  // Match found.
      return i;

  // Match not found.
  return -1;
}


// Determine the index of the given size flag, if it is one.
// Returns: index of flag into _size_flags array, or <0 if not a size flag.
static ssize_t size_index(const char* flag)
{
  assert(flag != NULL);

  stringtab_mutexgroups();

  for(size_t i = 0; _size_flags[i] != NULL; i++)
    if(flag == _size_flags[i])  // Match found.
      return i;

  // Match not found.
  return -1;
}


void buildflagset_add(buildflagset_t* set, const char* flag)
{
  assert(set != NULL);
  assert(flag != NULL);
  assert(set->flags != NULL);
  assert(!set->started_enum);

  // First check for mutually exclusive groups.
  if(os_index(flag) >= 0)
  {
    // OS flag.
    set->have_os_flags = true;
    return;
  }

  if(arch_index(flag) >= 0)
  {
    // Arch flag.
    set->have_arch_flags = true;
    return;
  }

  if(size_index(flag) >= 0)
  {
    // Size flag.
    set->have_size_flags = true;
    return;
  }

  // Just a normal flag.
  flag_t f1 = {flag, false};
  flag_t* f2 = flagtab_get(set->flags, &f1);

  if(f2 != NULL)  // Flag already in our table.
    return;

  // Add flag to our table.
  flagtab_put(set->flags, flag_dup(&f1));
}


double buildflagset_configcount(buildflagset_t* set)
{
  assert(set != NULL);
  assert(set->flags != NULL);

  double r = 1.0;

  // First check exclusive groups.
  if(set->have_os_flags)
  {
    int count = 0;
    while(_os_flags[count] != NULL)
      count++;

    r *= count;
  }

  if(set->have_arch_flags)
  {
    int count = 0;
    while(_arch_flags[count] != NULL)
      count++;

    r *= count;
  }

  if(set->have_size_flags)
  {
    int count = 0;
    while(_size_flags[count] != NULL)
      count++;

    r *= count;
  }

  // Now check normal flags, each doubles number of configs.
  size_t i = HASHMAP_BEGIN;

  while(flagtab_next(set->flags, &i) != NULL)
    r *= 2;

  return r;
}


void buildflagset_startenum(buildflagset_t* set)
{
  assert(set != NULL);
  assert(set->flags != NULL);

  set->started_enum = true;
  set->first_config_ready = true;

  // Start with all flags false.
  set->enum_os_flags = 0;
  set->enum_arch_flags = 0;
  set->enum_size_flags = 0;

  size_t i = HASHMAP_BEGIN;
  flag_t* flag;

  while((flag = flagtab_next(set->flags, &i)) != NULL)
    flag->value = false;
}


bool buildflagset_next(buildflagset_t* set)
{
  assert(set != NULL);
  assert(set->flags != NULL);
  assert(set->started_enum);

  if(set->first_config_ready)
  {
    // First config is ready for us.
    set->first_config_ready = false;
    return true;
  }

  if(set->have_os_flags)
  {
    // First increase the OS flags.
    set->enum_os_flags++;

    if(_os_flags[set->enum_os_flags] != NULL)
      return true;

    set->enum_os_flags = 0;
  }

  if(set->have_arch_flags)
  {
    // Overflow to the arch flags.
    set->enum_arch_flags++;

    if(_arch_flags[set->enum_arch_flags] != NULL)
      return true;

    set->enum_arch_flags = 0;
  }

  if(set->have_size_flags)
  {
    // Overflow to the size flags.
    set->enum_size_flags++;

    if(_size_flags[set->enum_size_flags] != NULL)
      return true;

    set->enum_size_flags = 0;
  }

  // Overflow to the remaining flags.
  size_t i = HASHMAP_BEGIN;
  flag_t* flag;

  while((flag = flagtab_next(set->flags, &i)) != NULL)
  {
    if(!flag->value)
    {
      // Flag is false, set it true and we're done.
      flag->value = true;
      return true;
    }

    // Flag is true, set it to false and move on to next flag.
    flag->value = false;
  }

  // We've run out of flags, no more configs.
  return false;
}


bool buildflagset_get(buildflagset_t* set, const char* flag)
{
  assert(set != NULL);
  assert(flag != NULL);
  assert(set->flags != NULL);
  assert(set->started_enum);

  // First check mutually exclusive groups.
  ssize_t index = os_index(flag);

  if(index >= 0)  // OS platform flag.
    return set->enum_os_flags == (uint32_t)index;

  index = arch_index(flag);

  if(index >= 0)  // Arch platform flag.
    return set->enum_arch_flags == (uint32_t)index;

  index = size_index(flag);

  if(index >= 0)  // Size platform flag.
    return set->enum_size_flags == (uint32_t)index;

  // Just a normal flag.
  flag_t f1 = {flag, false};
  flag_t* f2 = flagtab_get(set->flags, &f1);

  // Flag MUST have been added previously.
  assert(f2 != NULL);

  return f2->value;
}


// Print the given string to the given pointer within the given set's buffer,
// if there's room.
// Advance the location pointer whether the text is written or not.
static void print_str(const char* s, buildflagset_t* set, char** pointer)
{
  assert(s != NULL);
  assert(set != NULL);
  assert(pointer != NULL);

  size_t len = strlen(s);

  if((*pointer + len) < (set->text_buffer + set->buffer_size))
  {
    // There's enough space.
    snprintf(*pointer, len + 1, "%s", s);
  }

  *pointer += len;
}


// Print the given flag to the given pointer within the given set's buffer, if
// there's room.
// Advance the location pointer whether the text is written or not.
static void print_flag(const char* flag, bool sense, buildflagset_t* set,
  char** pointer)
{
  assert(flag != NULL);
  assert(set != NULL);

  if(*pointer != set->text_buffer)
    print_str(" ", set, pointer);

  if(!sense)
    print_str("!", set, pointer);

  print_str(flag, set, pointer);
}


const char* buildflagset_print(buildflagset_t* set)
{
  assert(set != NULL);
  assert(set->flags != NULL);

  char* p = set->text_buffer;

  // First the mutually exclusive flags.
  if(set->have_os_flags)
    print_flag(_os_flags[set->enum_os_flags], true, set, &p);

  if(set->have_arch_flags)
    print_flag(_arch_flags[set->enum_arch_flags], true, set, &p);

  if(set->have_size_flags)
    print_flag(_size_flags[set->enum_size_flags], true, set, &p);

  // Next the normal flags, in any order.
  size_t i = HASHMAP_BEGIN;
  flag_t* flag;

  while((flag = flagtab_next(set->flags, &i)) != NULL)
    print_flag(flag->name, flag->value, set, &p);

  if(p == set->text_buffer) // No flags, all configs match.
    print_str("all configs", set, &p);

  // Check buffer was big enough for it all.
  size_t size_needed = (p - set->text_buffer) + 1; // +1 for terminator.

  if(size_needed > set->buffer_size)
  {
    // Buffer we have isn't big enough, make it bigger then go round again.
    if(set->buffer_size > 0)
      pool_free_size(set->buffer_size, set->text_buffer);

    set->text_buffer = (char*)pool_alloc_size(size_needed);
    set->buffer_size = size_needed;
    set->text_buffer[0] = '\0';
    buildflagset_print(set);
  }

  // Add terminator.
  assert(set->text_buffer != NULL);
  set->text_buffer[size_needed - 1] = '\0';
  return set->text_buffer;
}


static flagtab_t* _user_flags = NULL;

bool define_build_flag(const char* name)
{
  assert(name != NULL);

  if(_user_flags == NULL)
  {
    // Initialise flags table.
    _user_flags = POOL_ALLOC(flagtab_t);
    flagtab_init(_user_flags, 8);
  }

  flag_t f1 = {stringtab(name), false};
  flag_t* f2 = flagtab_get(_user_flags, &f1);

  if(f2 != NULL)  // Flag already in our table.
    return false;

  // Add flag to our table.
  flagtab_put(_user_flags, flag_dup(&f1));
  return true;
}


bool is_build_flag_defined(const char* name)
{
  assert(name != NULL);

  if(_user_flags == NULL)
    // Table is not initialised, so no flags are defined.
    return false;

  flag_t f1 = { stringtab(name), false };
  flag_t* f2 = flagtab_get(_user_flags, &f1);

  return f2 != NULL;
}
