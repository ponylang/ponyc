#include <assert.h>
#include "id.h"


// ID spec flags
#define START_UPPER               0x01
#define START_LOWER               0x02
#define ALLOW_LEADING_UNDERSCORE  0x04
#define ALLOW_UNDERSCORE          0x08
#define ALLOW_DOUBLE_UNDERSCORE   0x10
#define ALLOW_TRAILING_UNDERSCORE 0x20
#define ALLOW_TICK                0x40


/* Check that the name in the given ID node meets the given spec.
 * If name is illegal an error will be generated.
 * The spec is supplied as a set of the above #defined flags.
 * This function is not static as it is linked to by the unit tests.
 */
bool check_id(ast_t* id_node, const char* desc, int spec)
{
  assert(id_node != NULL);
  assert(ast_id(id_node) == TK_ID);
  assert(desc != NULL);

  const char* name = ast_name(id_node);
  assert(name != NULL);
  char prev = '\0';

  // Ignore leading _
  if(*name == '_')
  {
    name++;
    prev = '_';

    if((spec & ALLOW_LEADING_UNDERSCORE) == 0)
    {
      ast_error(id_node, "%s names cannot start with underscores", desc);
      return false;
    }
  }

  if((spec & START_LOWER) != 0 && (*name < 'a' || *name > 'z'))
  {
    ast_error(id_node, "%s names must start a-z or _(a-z)", desc);
    return false;
  }

  if((spec & START_UPPER) != 0 && (*name < 'A' || *name > 'Z'))
  {
    ast_error(id_node, "%s names must start A-Z or _(A-Z)", desc);
    return false;
  }

  // Check each character looking for ticks and underscores
  for(; *name != '\0' && *name != '\''; name++)
  {
    if(*name == '_')
    {
      if((spec & ALLOW_UNDERSCORE) == 0)
      {
        ast_error(id_node, "%s names cannot contain underscores", desc);
        return false;
      }

      if((spec & ALLOW_DOUBLE_UNDERSCORE) == 0 && prev == '_')
      {
        ast_error(id_node, "%s names cannot contain double underscores", desc);
        return false;
      }
    }

    prev = *name;
  }

  // Only ticks (or nothing) left

  // Check for ending with _
  if((spec & ALLOW_TRAILING_UNDERSCORE) == 0 && prev == '_')
  {
    ast_error(id_node, "%s names cannot end with underscores", desc);
    return false;
  }

  if(*name == '\0')
    return true;

  // Should only be ticks left
  assert(*name == '\'');

  if((spec & ALLOW_TICK) == 0)
  {
    ast_error(id_node, "%s names cannot contain prime (')", desc);
    return false;
  }

  for(; *name != '\0'; name++)
  {
    if(*name != '\'')
    {
      ast_error(id_node, "prime(') can only appear at the end of identifiers");
      return false;
    }
  }

  return true;
}


bool check_id_type(ast_t* id_node, const char* entity_desc)
{
  return check_id(id_node, entity_desc,
    START_UPPER | ALLOW_LEADING_UNDERSCORE);
}

bool check_id_type_param(ast_t* id_node)
{
  return check_id(id_node, "type parameter",
    START_UPPER);
}

bool check_id_package(ast_t* id_node)
{
  return check_id(id_node, "package",
    START_LOWER | ALLOW_UNDERSCORE);
}

bool check_id_field(ast_t* id_node)
{
  return check_id(id_node, "field",
    START_LOWER | ALLOW_LEADING_UNDERSCORE | ALLOW_UNDERSCORE);
}

bool check_id_method(ast_t* id_node)
{
  return check_id(id_node, "method",
    START_LOWER | ALLOW_LEADING_UNDERSCORE | ALLOW_UNDERSCORE);
}

bool check_id_param(ast_t* id_node)
{
  return check_id(id_node, "parameter",
    START_LOWER | ALLOW_LEADING_UNDERSCORE | ALLOW_UNDERSCORE | ALLOW_TICK);
}

bool check_id_local(ast_t* id_node)
{
  return check_id(id_node, "local variable",
    START_LOWER | ALLOW_LEADING_UNDERSCORE | ALLOW_UNDERSCORE | ALLOW_TICK);
}

bool check_id_ffi(ast_t* id_node)
{
  return check_id(id_node, "FFI",
    ALLOW_LEADING_UNDERSCORE | ALLOW_UNDERSCORE | ALLOW_DOUBLE_UNDERSCORE |
    ALLOW_TRAILING_UNDERSCORE);
}


//bool is_id_type(const char* id);


//bool is_private(const char* id);
