#include "colour.h"
#include "../ast/symtab.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
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

typedef struct def_record_t
{
  ast_t* ast;             // Defining AST
  int table_size;         // Required number of vtable entries
  int typemap_index;      // Position in type bitmaps
  uint64_t typemap_mask;  // "
  struct def_record_t* next;
} def_record_t;

typedef struct name_record_t
{
  const char* name;       // Name
  int colour;             // Assigned colour (vtable index)
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
  def_record_t* typedefs;
  def_record_t** typedef_next;
  name_record_t* names;
  name_record_t** name_next;
  symtab_t* name_table;   // Duplicate refs to name records for easy lookup
  colour_record_t* colours;
  uint64_t typemap_mask;  // Last assigned position in type bitmap
  int typemap_index;      // "
  int typemap_size;       // Number of uint64_ts required in type bitmaps
  int name_count;         // Number of method names used across all typedefs
  int colour_count;       // Number of colours assigned
  colour_opt_t options;
} painter_t;


#define UNASSIGNED_COLOUR -1


// Add a type definition record
static void add_def(painter_t* painter, ast_t* ast)
{
  assert(painter != NULL);
  assert(ast != NULL);

  // Advance to next position in type bitmap
  painter->typemap_mask <<= 1;

  if(painter->typemap_mask == 0)
  {
    painter->typemap_mask = 1;
    painter->typemap_index++;
  }

  // Setup definition record
  def_record_t* n = POOL_ALLOC(def_record_t);
  n->ast = ast;
  n->typemap_index = painter->typemap_index;
  n->typemap_mask = painter->typemap_mask;
  n->next = NULL;
  *painter->typedef_next = n;
  painter->typedef_next = &n->next;
}


// Add a method name record
static name_record_t* add_name(painter_t* painter, const char* name)
{
  assert(painter != NULL);
  assert(name != NULL);

  name_record_t* n = POOL_ALLOC(name_record_t);
  n->name = name;
  n->colour = UNASSIGNED_COLOUR;
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
  return (name_record_t*)symtab_get(painter->name_table, name, NULL);
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
  for(int i = 0; i < painter->typemap_size; i++)
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
  for(int i = 0; i < painter->typemap_size; i++)
    colour->type_map[i] |= name->type_map[i];

  colour->type_count += name->type_count;
  name->colour = colour->colour;
}


// Clear all data from the painter in preperation for processing new data or
// deletion
static void painter_clear(painter_t* painter)
{
  assert(painter != NULL);

  def_record_t* def = painter->typedefs;
  while(def != NULL)
  {
    def_record_t* next = def->next;
    POOL_FREE(def_record_t, def);
    def = next;
  }

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

  painter->typedefs = NULL;
  painter->typedef_next = &painter->typedefs;
  painter->names = NULL;
  painter->name_next = &painter->names;
  painter->name_table = NULL;
  painter->colours = NULL;
  painter->colour_count = 0;
  painter->typemap_index = -1;
  painter->typemap_mask = 0;
  painter->name_count = 0;
}


// Find concrete type definitions in the given AST
static void find_types(painter_t* painter, ast_t* ast)
{
  assert(painter != NULL);

  if(ast == NULL)
    return;

  switch(ast_id(ast))
  {
    case TK_PROGRAM:
    case TK_PACKAGE:
    case TK_MODULE:
    {
      // Recurse into children
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        find_types(painter, child);
        child = ast_sibling(child);
      }
      break;
    }

    case TK_ACTOR:
    case TK_CLASS:
    case TK_PRIMITIVE:
      // We have a definition
      add_def(painter, ast);
      break;

    default:
      // Not an interesting sub-tree
      break;
  }
}


// Step 2
static void find_names_types_use(painter_t* painter)
{
  for(def_record_t* def = painter->typedefs; def != NULL; def = def->next)
  {
    // We have a type definition, iterate through its members
    ast_t* members = ast_child(ast_childidx(def->ast, 4));

    for(ast_t* member = members; member != NULL; member = ast_sibling(member))
    {
      if(ast_id(member) == TK_FUN || ast_id(member) == TK_BE)
      {
        // We have a function or behaviour
        const char* name = ast_name(ast_childidx(member, 1));

        name_record_t* name_rec = find_name(painter, name);

        if(name_rec == NULL)  // This is the first use of this name
          name_rec = add_name(painter, name);

        // Mark this name as using the current type
        name_rec->type_map[def->typemap_index] |= def->typemap_mask;
        name_rec->type_count++;
      }
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
static void find_vtable_sizes(painter_t* painter)
{
  for(def_record_t* def = painter->typedefs; def != NULL; def = def->next)
  {
    // Check this type against our colours
    def->table_size = 0;
    for(int i = painter->colour_count - 1; i >= 0; i--)
    {
      colour_record_t* col = &painter->colours[i];

      if((col->type_map[def->typemap_index] & def->typemap_mask) != 0)
      {
        // Type uses this colour
        def->table_size = col->colour + 1;
        break;
      }
    }
  }
}


painter_t* painter_create()
{
  painter_t* p = POOL_ALLOC(painter_t);
  memset(p, 0, sizeof(painter_t));

  p->typemap_index = -1;
  return p;
}


colour_opt_t* painter_get_options(painter_t* painter)
{
  assert(painter != NULL);
  return &painter->options;
}


void painter_colour(painter_t* painter, ast_t* typedefs)
{
  assert(painter != NULL);
  assert(typedefs != NULL);

  painter_clear(painter);
  painter->name_table = symtab_new();
  find_types(painter, typedefs);

  // Determine the number of uint64_ts needed to contain our type bitmap
  painter->typemap_size = painter->typemap_index + 1;

  find_names_types_use(painter);

  // Allocate colour records
  painter->colours = (colour_record_t*)pool_alloc_size(
    painter->name_count * sizeof(colour_record_t));

  assign_colours_to_names(painter);
  find_vtable_sizes(painter);

  // If we're worried about memory we can free the colour records now
}


int painter_get_colour(painter_t* painter, const char* method_name)
{
  assert(painter != NULL);
  assert(method_name != NULL);

  // Lookup specified name record
  name_record_t* name_rec = find_name(painter, method_name);

  if(name_rec == NULL)  // Given name not found
    return -1;

  return name_rec->colour;
}


int painter_get_vtable_size(painter_t* painter, ast_t* type)
{
  assert(painter != NULL);
  assert(type != NULL);

  // Search for definition record
  for(def_record_t* def = painter->typedefs; def != NULL; def = def->next)
  {
    if(def->ast == type)  // Specified definition found
      return def->table_size;
  }

  // Specified definition not found, that's bad
  assert(false);
  return -1;
}


void painter_free(painter_t* painter)
{
  if(painter == NULL)
    return;

  painter_clear(painter);
  POOL_FREE(painter_t, painter);
}
