#include "paint.h"
#include "../../libponyrt/ds/hash.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

/** We use a greedy algorithm that gives a reasonable trade-off between
 * computation (at compiletime) and resulting colour density (runtime memory).
 * We also employ various heuristics to improve our results.
 *
 * Main algorithm.
 *
 * The basic approach is that we determine all the types that use each method
 * name and then we try to group names that have disjoint type sets.
 *
 * Note that we only deal with method names here. Polymorphic methods are
 * handled by name mangling before the painter is called.
 *
 * Step 1.
 * First we walk through the given set of reachable types finding all methods.
 * For each unique name we create a name record. In each of these records we
 * note which types use that name. We implement this using a bitmap. Because we
 * know in advance how many types there are we can store this as an array of
 * uints.
 *
 * Step 2.
 * Various heuristics (see below).
 *
 * Step 3.
 * We run through our name records. For each one we run through the colours
 * already assigned. If the set of types using the current name has no
 * intersection with the types using the current assigned colour, then that
 * name is also assigned to that colour. The name's type set is unioned into
 * the colour's type set.
 *
 * If a name is not compatible with any assigned colour (or there are no
 * colours assigned yet) then that name is assigned to the first free colour.
 *
 * Step 4.
 * Various heuristics (see below).
 *
 * Step 5.
 * For each method name in the set of reachable types we lookup our name record
 * and fill in which colour has been assigned.
 * While doing this we also determine the maximum colour used by each type and
 * hence its vtable size.
 *
 * Heuristics.
 *
 * No heuristics are implemented yet, but there are various planned. Most of
 * these reduce compiletime computation at the expense of vtable size.
 *
 * 1. A name that is used by most of the types is far less likely to be
 * compatible with other names, so checking against them is a waste of time.
 * For any name that is used by more than some proportion of the types,
 * don't try to combine that name with others.
 *
 * 2. Step 3 is O(n^2) on the number of names. To prevent this exploding for
 * large n break the names into multiple groups and combine each group
 * separately. Do not try and combination between these groups. A simple
 * approach is just to put the first n names in one group, the next n in
 * another, etc for some n. More complex approaches may or may not give better
 * results.
 *
 * 3. The resulting vtables are not used for correctness checking and do not
 * have to be tolerant of invalid accesses. Therefore they do not all have to
 * be the same size and each can stop after its last valid entry. This means
 * that the total size of the vtables can be reduced by reordering the colours.
 * As a first attempt simply sort the colours by the number of types that use
 * them.
 */

#define UNASSIGNED_COLOUR ((uint32_t)-1)


typedef struct name_record_t
{
  const char* name;       // Name including type params
  uint32_t colour;        // Colour assigned to name
  size_t typemap_size;    // Size of our type bitmap in uint64_ts
  uint64_t* type_map;     // Bitmap of types using this name
} name_record_t;


// We keep our name records in a hash map
static uint64_t name_record_hash(name_record_t* p)
{
  return hash_ptr(p->name);
}

static bool name_record_cmp(name_record_t* a, name_record_t* b)
{
  return a->name == b->name;
}

static void name_record_free(name_record_t* p)
{
  pool_free_size(p->typemap_size * sizeof(uint64_t), p->type_map);
  POOL_FREE(name_record_t, p);
}

DECLARE_HASHMAP(name_records, name_record_t);
DEFINE_HASHMAP(name_records, name_record_t, name_record_hash, name_record_cmp,
  pool_alloc_size, pool_free_size, name_record_free);


typedef struct colour_record_t
{
  uint32_t colour;        // Colour index
  uint64_t* type_map;     // Bitmap of types using this name
  struct colour_record_t* next;
} colour_record_t;


typedef struct painter_t
{
  name_records_t names;     // Name records
  colour_record_t* colours; // Linked list of colour records
  colour_record_t** colour_next;  // ..
  size_t typemap_size;      // Size of type bitmaps in uint64_ts
  uint32_t colour_count;    // Number of colours assigned
} painter_t;


// Print out a type map, for debugging only
static void print_typemap(size_t size, uint64_t* map)
{
  assert(map != NULL);

  for(size_t i = 0; i < size; i++)
  {
    printf("  ");

    for(uint64_t mask = 1; mask != 0; mask <<= 1)
      printf("%c", ((map[i] & mask) == 0) ? '.' : 'T');

    printf("\n");
  }
}


// This is not static so compiler doesn't complain about it not being used
void painter_print(painter_t* painter)
{
  assert(painter != NULL);

  printf("Painter typemaps are %lu bits\n",
    painter->typemap_size * sizeof(uint64_t) * 8);

  printf("Painter names:\n");

  size_t i = HASHMAP_BEGIN;
  name_record_t* name;

  while((name = name_records_next(&painter->names, &i)) != NULL)
  {
    printf("\"%s\" colour ", name->name);

    if(name->colour == UNASSIGNED_COLOUR)
      printf("unassigned\n");
    else
      printf("%u\n", name->colour);

    print_typemap(name->typemap_size, name->type_map);
  }

  printf("Painter has %u colours:\n", painter->colour_count);

  for(colour_record_t* c = painter->colours; c != NULL; c = c->next)
  {
    printf("  Colour %u\n", c->colour);
    print_typemap(painter->typemap_size, c->type_map);
  }

  printf("Painter end\n");
}


// Add a method name record
static name_record_t* add_name(painter_t* painter, const char* name)
{
  assert(painter != NULL);
  assert(name != NULL);

  size_t map_byte_count = painter->typemap_size * sizeof(uint64_t);

  name_record_t* n = POOL_ALLOC(name_record_t);
  n->name = name;
  n->colour = UNASSIGNED_COLOUR;
  n->typemap_size = painter->typemap_size;
  n->type_map = (uint64_t*)pool_alloc_size(map_byte_count);
  memset(n->type_map, 0, map_byte_count);

  name_records_put(&painter->names, n);
  return n;
}


// Add a colour record
static colour_record_t* add_colour(painter_t* painter)
{
  assert(painter != NULL);

  size_t map_byte_count = painter->typemap_size * sizeof(uint64_t);

  colour_record_t* n = POOL_ALLOC(colour_record_t);
  n->colour = painter->colour_count;
  n->type_map = (uint64_t*)pool_alloc_size(map_byte_count);
  n->next = NULL;
  memset(n->type_map, 0, map_byte_count);

  *painter->colour_next = n;
  painter->colour_next = &n->next;
  painter->colour_count++;
  return n;
}


// Find the name record with the specified name
static name_record_t* find_name(painter_t* painter, const char* name)
{
  name_record_t n = { name, 0, 0, NULL };
  return name_records_get(&painter->names, &n);
}


// Check whether the given name can be assigned to the given colour.
// Assignment can occur as long as the sets of types using the anme and colour
// are distinct, ie there is no overlap in their bitmaps.
static bool is_name_compatible(colour_record_t* colour, name_record_t* name)
{
  assert(colour != NULL);
  assert(name != NULL);

  // Check for type bitmap intersection
  for(size_t i = 0; i < name->typemap_size; i++)
  {
    if((colour->type_map[i] & name->type_map[i]) != 0)  // Type bitmaps overlap
      return false;
  }

  // Type bitmaps are distinct
  return true;
}


// Assign the given name to the given colour
static void assign_name_to_colour(colour_record_t* colour, name_record_t* name)
{
  assert(colour != NULL);
  assert(name != NULL);

  // Union the type bitmaps
  for(size_t i = 0; i < name->typemap_size; i++)
    colour->type_map[i] |= name->type_map[i];

  name->colour = colour->colour;
}


// Step 1
static void find_names_types_use(painter_t* painter, reachable_types_t* types)
{
  assert(painter != NULL);
  assert(types != NULL);

  size_t i = HASHMAP_BEGIN;
  size_t typemap_index = 0;
  uint64_t typemap_mask = 1;
  reachable_type_t* type;

  while((type = reachable_types_next(types, &i)) != NULL)
  {
    assert(typemap_index < painter->typemap_size);
    size_t j = HASHMAP_BEGIN;
    reachable_method_name_t* mn;

    while((mn = reachable_method_names_next(&type->methods, &j)) != NULL)
    {
      size_t k = HASHMAP_BEGIN;
      reachable_method_t* method;

      while((method = reachable_methods_next(&mn->r_methods, &k)) != NULL)
      {
        const char* name = method->name;

        name_record_t* name_rec = find_name(painter, name);

        if(name_rec == NULL)  // This is the first use of this name
          name_rec = add_name(painter, name);

        // Mark this name as using the current type
        name_rec->type_map[typemap_index] |= typemap_mask;
      }
    }

    // Advance to next type bitmap entry
    typemap_mask <<= 1;

    if(typemap_mask == 0)
    {
      typemap_mask = 1;
      typemap_index++;
    }
  }
}


// Step 3
static void assign_colours_to_names(painter_t* painter)
{
  size_t i = HASHMAP_BEGIN;
  name_record_t* name;

  while((name = name_records_next(&painter->names, &i)) != NULL)
  {
    // We have a name, try to match it with an existing colour
    colour_record_t* colour = NULL;

    for(colour_record_t* c = painter->colours; c != NULL; c = c->next)
    {
      if(is_name_compatible(c, name))
      {
        // Name is compatible with colour
        colour = c;
        break;
      }
    }

    if(colour == NULL)
    {
      // Name is not compatible with any existing colour, assign a new one
      colour = add_colour(painter);
    }

    assign_name_to_colour(colour, name);
  }
}


// Step 5
static void distribute_info(painter_t* painter, reachable_types_t* types)
{
  assert(painter != NULL);
  assert(types != NULL);

  size_t i = HASHMAP_BEGIN;
  reachable_type_t* type;

  // Iterate over all types
  while((type = reachable_types_next(types, &i)) != NULL)
  {
    if(reachable_method_names_size(&type->methods) == 0)
      continue;

    size_t j = HASHMAP_BEGIN;
    reachable_method_name_t* mn;
    uint32_t max_colour = 0;

    // Iterate over all method names in type
    while((mn = reachable_method_names_next(&type->methods, &j)) != NULL)
    {
      size_t k = HASHMAP_BEGIN;
      reachable_method_t* method;

      while((method = reachable_methods_next(&mn->r_methods, &k)) != NULL)
      {
        // Store colour assigned to name in reachable types set
        const char* name = method->name;

        name_record_t* name_rec = find_name(painter, name);
        assert(name_rec != NULL);

        uint32_t colour = name_rec->colour;
        method->vtable_index = colour;

        if(colour > max_colour)
          max_colour = colour;
      }
    }

    // Store vtable size for type
    type->vtable_size = max_colour + 1;
  }
}


// Free everything we've allocated
static void painter_tidy(painter_t* painter)
{
  assert(painter != NULL);

  size_t map_byte_count = painter->typemap_size * sizeof(uint64_t);

  name_records_destroy(&painter->names);

  colour_record_t* c = painter->colours;

  while(c != NULL)
  {
    colour_record_t* next = c->next;
    pool_free_size(map_byte_count, c->type_map);
    POOL_FREE(sizeof(colour_record_t), c);
    c = next;
  }
}


void paint(reachable_types_t* types)
{
  assert(types != NULL);

  size_t type_count = reachable_types_size(types);

  if(type_count == 0) // Nothing to paint
    return;

  painter_t painter;
  name_records_init(&painter.names, 8);
  painter.colours = NULL;
  painter.colour_next = &painter.colours;
  painter.colour_count = 0;

  // Determine the size needed to contain our type bitmap, round up to next
  // whole uint64_t
  painter.typemap_size = ((type_count - 1) / 64) + 1;

  // Step 1
  find_names_types_use(&painter, types);

  // Step 3
  assign_colours_to_names(&painter);
  // painter_print(&painter);

  // Step 5
  distribute_info(&painter, types);

  painter_tidy(&painter);
}
