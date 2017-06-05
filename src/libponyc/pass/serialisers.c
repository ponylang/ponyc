#include "serialisers.h"
#include "../type/lookup.h"
#include "../type/subtype.h"
#include "../type/reify.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

static bool entity_serialiser(pass_opt_t* opt, ast_t* entity,
                              const char* serialise_space, const char* serialise, const char* deserialise)
{
  ast_t* ast_serialise_space = ast_get(entity, serialise_space, NULL);
  ast_t* ast_serialise = ast_get(entity, serialise, NULL);
  ast_t* ast_deserialise = ast_get(entity, deserialise, NULL);

  if(ast_serialise_space == NULL && ast_serialise == NULL &&
    ast_deserialise == NULL)
    return true;

  if(ast_serialise_space == NULL || ast_serialise == NULL ||
    ast_deserialise == NULL)
  {
    ast_error(opt->check.errors, entity,
      "If a custom serialisation strategy exists then _serialise_space, "
      "_serialise and _deserialise must all be provided");
    return false;
  }

  return true;
}

static bool module_serialisers(pass_opt_t* opt, ast_t* module,
  const char* serialise_space, const char* serialise, const char* deserialise)
{
  ast_t* entity = ast_child(module);
  bool ok = true;

  while(entity != NULL)
  {
    switch(ast_id(entity))
    {
    case TK_CLASS:
      if(!entity_serialiser(opt, entity, serialise_space, serialise,
        deserialise))
        ok = false;
      break;

    default: {}
    }

    entity = ast_sibling(entity);
  }

  return ok;
}

static bool package_serialisers(pass_opt_t* opt, ast_t* package,
  const char* serialise_space, const char* serialise, const char* deserialise)
{
  ast_t* module = ast_child(package);
  bool ok = true;

  while(module != NULL)
  {
    if(!module_serialisers(opt, module, serialise_space, serialise,
      deserialise))
      ok = false;

    module = ast_sibling(module);
  }

  return ok;
}

bool pass_serialisers(ast_t* program, pass_opt_t* options)
{
  ast_t* package = ast_child(program);
  const char* serialise_space = stringtab("_serialise_space");
  const char* serialise = stringtab("_serialise");
  const char* deserialise = stringtab("_deserialise");
  bool ok = true;

  while(package != NULL)
  {
    if(!package_serialisers(options, package, serialise_space, serialise,
      deserialise))
      ok = false;

    package = ast_sibling(package);
  }

  return ok;
}
