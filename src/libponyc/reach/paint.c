#include "paint.h"
#include "../ast/symtab.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/** We use a greedy algorithm that gives a reasonable trade-off between
 * computation (at compiletime) and resulting colour density (runtime memory).
 * We also employ various heuristics to improve our results.
 *
 * Main algorithm.
 *
 * The basic approach is that we determine all the types that use each method
 * name and then we try to group names that have disjoint type sets.
 *
 * Step 1.
 * First we walk the given AST finding type definitions. We store these in a
 * list and also take a count. This is all done purely as an optimisation for
 * later steps.
 *
 * Step 2.
 * We walk through our types finding all function and behaviour definitions.
 * For each unique name we create a name record. In each of these records we
 * note which types use that name. We implement this using a bitmap. Because we
 * know in advance how many types there are we can store this as an array of
 * uints.
 *
 * Step 3.
 * Various heuristics (see below).
 *
 * Step 4.
 * We run through our name records. For each one we run through the colours
 * already assigned. If the set of types using the current name has no
 * intersection with the types using the current assigned colour, then that
 * name is also assigned to that colour. The name's type set is unioned into
 * the colour's type set.
 *
 * If a name is not compatible with any assigned colour (or there are no
 * colours assigned yet) then that name is assigned to the first free colour.
 *
 * Step 5.
 * Various heuristics (see below).
 *
 * Step 6.
 * For each type we run over the assigned colour and determine the size of the
 * vtable needed for that type. Since we store the colours in (reverse) order,
 * the largest used colour for a type is the first one we find.
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
 * 2. Step 4 is O(n^2) on the number of names. To prevent this exploding for
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


typedef struct name_record_t
{
  const char* name;       // Name including type params
  uint32_t colour;
  int type_count;         // Number of types using this name
  uint64_t* type_map;     // Bitmap of types using this name
  struct name_record_t* next;
} name_record_t;

typedef struct colour_record_t
{
  int colour;             // Colour index
  int type_count;         // Number of types using this name
  uint64_t* type_map;     // Bitmap of types using this name
} colour_record_t;


typedef struct painter_t
{
  name_record_t* names;
  name_record_t** name_next;
  symtab_t* name_table;   // Duplicate refs to name records for easy lookup
  colour_record_t* colours;
  size_t typemap_size;    // Number of uint64_ts required in type bitmaps
  int name_count;         // Number of method names used across all typedefs
  int colour_count;       // Number of colours assigned
} painter_t;


// Add a method name record
static name_record_t* add_name(painter_t* painter, const char* name)
{
  assert(painter != NULL);
  assert(name != NULL);

  name_record_t* n = POOL_ALLOC(name_record_t);
  n->name = name;
  n->colour = -1;
  n->type_count = 0;
  n->type_map = (uint64_t*)pool_alloc_size(
    painter->typemap_size * sizeof(uint64_t));
  n->next = NULL;
  *painter->name_next = n;
  painter->name_next = &n->next;
  symtab_add(painter->name_table, name, n, SYM_NONE);
  painter->name_count++;
  return n;
}


// Add a colour record
static colour_record_t* add_colour(painter_t* painter)
{
  assert(painter != NULL);

  int index = painter->colour_count;
  colour_record_t* n = &painter->colours[index];
  n->colour = index;
  n->type_count = 0;
  n->type_map = (uint64_t*)pool_alloc_size(
    painter->typemap_size * sizeof(uint64_t));
  painter->colour_count++;

  return n;
}


// Find the name record with the specified name
static name_record_t* find_name(painter_t* painter, const char* name)
{
  return (name_record_t*)symtab_find(painter->name_table, name, NULL);
}


// Check whether the given name can be assigned to the given colour.
// Assignment can occur as long as the sets of types using the anme and colour
// are distinct, ie there is no overlap in their bitmaps.
static bool is_name_compatible(painter_t* painter, colour_record_t* colour,
  name_record_t* name)
{
  assert(painter != NULL);
  assert(colour != NULL);
  assert(name != NULL);

  // Check for type bitmap intersection
  for(size_t i = 0; i < painter->typemap_size; i++)
  {
    if((colour->type_map[i] & name->type_map[i]) != 0)  // Type bitmaps overlap
      return false;
  }

  // Type bitmaps are distinct
  return true;
}


// Assign the given name to the given colour
static void assign_name_to_colour(painter_t* painter, colour_record_t* colour,
  name_record_t* name)
{
  assert(painter != NULL);
  assert(colour != NULL);
  assert(name != NULL);

  // Union the type bitmaps
  for(size_t i = 0; i < painter->typemap_size; i++)
    colour->type_map[i] |= name->type_map[i];

  colour->type_count += name->type_count;
  name->colour = colour->colour;
}


#if 0
// Clear all data from the painter in preperation for processing new data or
// deletion
static void painter_clear(painter_t* painter)
{
  assert(painter != NULL);

/*
  def_record_t* def = painter->typedefs;
  while(def != NULL)
  {
    def_record_t* next = def->next;
    POOL_FREE(def_record_t, def);
    def = next;
  }
*/

  name_record_t* n = painter->names;
  while(n != NULL)
  {
    name_record_t* next = n->next;
    pool_free_size(painter->typemap_size * sizeof(uint64_t), n->type_map);
    POOL_FREE(name_record_t, n);
    n = next;
  }

  if(painter->colours != NULL)
  {
    for(int i = 0; i < painter->colour_count; i++)
      pool_free_size(painter->typemap_size * sizeof(uint64_t),
        painter->colours[i].type_map);

    pool_free_size(painter->name_count * sizeof(colour_record_t),
      painter->colours);
    painter->colours = NULL;
  }

  symtab_free(painter->name_table);

//  painter->typedefs = NULL;
//  painter->typedef_next = &painter->typedefs;
  painter->names = NULL;
  painter->name_next = &painter->names;
  painter->name_table = NULL;
  painter->colours = NULL;
  painter->colour_count = 0;
//  painter->typemap_index = -1;
//  painter->typemap_mask = 0;
  painter->name_count = 0;
}
#endif



// Step 2
static void find_names_types_use(painter_t* painter, reachable_types_t* types)
{
  assert(painter != NULL);
  assert(types != NULL);

  size_t type_i = HASHMAP_BEGIN;
  size_t typemap_index = 0;
  uint64_t typemap_mask = 1;
  reachable_type_t* type;

  while((type = (reachable_type_t*)reachable_types_next(types, &type_i))
    != NULL)
  {
    assert(typemap_index < painter->typemap_size);
    size_t meth_name_i = HASHMAP_BEGIN;
    reachable_method_name_t* meth_name;

    while((meth_name = (reachable_method_name_t*)reachable_method_names_next(
      &type->methods, &meth_name_i)) != NULL)
    {
      size_t method_i = HASHMAP_BEGIN;
      reachable_method_t* method;

      while((method = (reachable_method_t*)reachable_methods_next(
        &meth_name->r_methods, &method_i)) != NULL)
      {
        const char* name = method->name;

        name_record_t* name_rec = find_name(painter, name);

        if(name_rec == NULL)  // This is the first use of this name
          name_rec = add_name(painter, name);

        // Mark this name as using the current type
        name_rec->type_map[typemap_index] |= typemap_mask;
        name_rec->type_count++;
      }
    }

    typemap_mask <<= 1;

    if(typemap_mask == 0)
    {
      typemap_mask = 1;
      typemap_index++;
    }
  }
}


// Step 4
static void assign_colours_to_names(painter_t* painter)
{
  for(name_record_t* name = painter->names; name != NULL; name = name->next)
  {
    // We have a name, try to match it with an existing colour
    colour_record_t* colour = NULL;

    for(int i = 0; i < painter->colour_count; i++)
    {
      if(is_name_compatible(painter, &painter->colours[i], name))
      {
        // Name is compatible with colour
        colour = &painter->colours[i];
        break;
      }
    }

    if(colour == NULL)
    {
      // Name is not compatible with any existing colour, assign a new one
      colour = add_colour(painter);
    }

    assign_name_to_colour(painter, colour, name);
  }
}


// Step 6
static void find_vtable_sizes(painter_t* painter, reachable_types_t* types)
{
  assert(painter != NULL);
  assert(types != NULL);

  size_t type_i = HASHMAP_BEGIN;
  int typemap_index = 0;
  uint64_t typemap_mask = 1;
  reachable_type_t* type;

  while((type = (reachable_type_t*)reachable_types_next(types, &type_i))
    != NULL)
  {
    // Check this type against our colours
    type->vtable_size = 0;

    for(int i = painter->colour_count - 1; i >= 0; i--)
    {
      colour_record_t* col = &painter->colours[i];

      if((col->type_map[typemap_index] & typemap_mask) != 0)
      {
        // Type uses this colour
        type->vtable_size = col->colour + 1;
        break;
      }
    }

    typemap_mask <<= 1;

    if(typemap_mask == 0)
    {
      typemap_mask = 1;
      typemap_index++;
    }
  }
}


static void distribute_colours(painter_t* painter, reachable_types_t* types)
{
  assert(painter != NULL);
  assert(types != NULL);

  size_t type_i = HASHMAP_BEGIN;
  reachable_type_t* type;

  while((type = (reachable_type_t*)reachable_types_next(types, &type_i))
    != NULL)
  {
    size_t meth_name_i = HASHMAP_BEGIN;
    reachable_method_name_t* meth_name;

    while((meth_name = (reachable_method_name_t*)reachable_method_names_next(
      &type->methods, &meth_name_i)) != NULL)
    {
      size_t method_i = HASHMAP_BEGIN;
      reachable_method_t* method;

      while((method = (reachable_method_t*)reachable_methods_next(
        &meth_name->r_methods, &method_i)) != NULL)
      {
        const char* name = method->name;

        name_record_t* name_rec = find_name(painter, name);
        assert(name_rec != NULL);
        method->vtable_index = name_rec->colour;
      }
    }
  }
}


void paint(reachable_types_t* types)
{
  assert(types != NULL);

  size_t type_count = reachable_types_size(types);

  if(type_count == 0) // Nothing to paint
    return;

  painter_t painter;
  memset(&painter, 0, sizeof(painter_t));

  painter.name_table = symtab_new();
  painter.names = NULL;
  painter.name_next = &painter.names;
  painter.colours = NULL;
  painter.colour_count = 0;
  painter.name_count = 0;

  // Determine the number of uint64_ts needed to contain our type bitmap
  painter.typemap_size = ((type_count - 1) / 64) + 1;

  find_names_types_use(&painter, types);

  // Allocate colour records
  painter.colours = (colour_record_t*)pool_alloc_size(
    painter.name_count * sizeof(colour_record_t));

  assign_colours_to_names(&painter);
  find_vtable_sizes(&painter, types);
  distribute_colours(&painter, types);

  //painter_clear(&painter);
}
