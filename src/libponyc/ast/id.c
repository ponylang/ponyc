#include <assert.h>
#include "id.h"
#include "id_internal.h"


bool check_id(ast_t* id_node, const char* desc, int spec)
{
  assert(id_node != NULL);
  assert(ast_id(id_node) == TK_ID);
  assert(desc != NULL);

  const char* name = ast_name(id_node);
  assert(name != NULL);
  char prev = '\0';

  // Ignore leading $, handled by lexer
  if(*name == '$')
  {
    name++;
    prev = '$';
  }

  // Ignore leading _
  if(*name == '_')
  {
    name++;
    prev = '_';

    if((spec & ALLOW_LEADING_UNDERSCORE) == 0)
    {
      ast_error(id_node, "%s name \"%s\" cannot start with underscores", desc,
        ast_name(id_node));
      return false;
    }
  }

  if((spec & START_LOWER) != 0 && (*name < 'a' || *name > 'z'))
  {
    ast_error(id_node, "%s name \"%s\" must start a-z or _(a-z)", desc,
      ast_name(id_node));
    return false;
  }

  if((spec & START_UPPER) != 0 && (*name < 'A' || *name > 'Z'))
  {
    ast_error(id_node, "%s name \"%s\" must start A-Z or _(A-Z)", desc,
      ast_name(id_node));
    return false;
  }

  // Check each character looking for ticks and underscores
  for(; *name != '\0' && *name != '\''; name++)
  {
    if(*name == '_')
    {
      if((spec & ALLOW_UNDERSCORE) == 0)
      {
        ast_error(id_node, "%s name \"%s\" cannot contain underscores", desc,
          ast_name(id_node));
        return false;
      }

      if(prev == '_')
      {
        ast_error(id_node, "%s name \"%s\" cannot contain double underscores",
          desc, ast_name(id_node));
        return false;
      }
    }

    prev = *name;
  }

  // Only ticks (or nothing) left

  // Check for ending with _
  if(prev == '_')
  {
    ast_error(id_node, "%s name \"%s\" cannot end with underscores", desc,
      ast_name(id_node));
    return false;
  }

  if(*name == '\0')
    return true;

  // Should only be ticks left
  assert(*name == '\'');

  if((spec & ALLOW_TICK) == 0)
  {
    ast_error(id_node, "%s name \"%s\" cannot contain prime (')", desc,
      ast_name(id_node));
    return false;
  }

  for(; *name != '\0'; name++)
  {
    if(*name != '\'')
    {
      ast_error(id_node,
        "prime(') can only appear at the end of %s name \"%s\"", desc,
        ast_name(id_node));
      return false;
    }
  }

  return true;
}


bool check_id_type(ast_t* id_node, const char* entity_desc)
{
  // _?[A-Z][A-Za-z0-9]*
  return check_id(id_node, entity_desc,
    START_UPPER | ALLOW_LEADING_UNDERSCORE);
}

bool check_id_type_param(ast_t* id_node)
{
  // [A-Z][A-Za-z0-9]*
  return check_id(id_node, "type parameter",
    START_UPPER);
}

bool check_id_package(ast_t* id_node)
{
  // [a-z][A-Za-z0-9_]* (and no double or trailing underscores)
  return check_id(id_node, "package",
    START_LOWER | ALLOW_UNDERSCORE);
}

bool check_id_field(ast_t* id_node)
{
  // _?[a-z][A-Za-z0-9_]* (and no double or trailing underscores)
  return check_id(id_node, "field",
    START_LOWER | ALLOW_LEADING_UNDERSCORE | ALLOW_UNDERSCORE);
}

bool check_id_method(ast_t* id_node)
{
  // _?[a-z][A-Za-z0-9_]* (and no double or trailing underscores)
  return check_id(id_node, "method",
    START_LOWER | ALLOW_LEADING_UNDERSCORE | ALLOW_UNDERSCORE);
}

bool check_id_param(ast_t* id_node)
{
  // [a-z][A-Za-z0-9_]*'* (and no double or trailing underscores)
  return check_id(id_node, "parameter",
    START_LOWER | ALLOW_UNDERSCORE | ALLOW_TICK);
}

bool check_id_local(ast_t* id_node)
{
  // [a-z][A-Za-z0-9_]*'* (and no double or trailing underscores)
  return check_id(id_node, "local variable",
    START_LOWER | ALLOW_UNDERSCORE | ALLOW_TICK);
}


//bool is_id_type(const char* id);


//bool is_private(const char* id);
