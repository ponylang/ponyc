#include "mutation.h"

#include <string.h>

typedef enum
{
  MUTATION_DEFAULT,
  MUTATION_NONE,
  MUTATION_ONLY,
  MUTATION_EXCEPT,
} mutation_mode_t;

static const struct
{
  const char* name;
  uint16_t bit;
} categories[] = {
  {"aor", MUTATE_OP_AOR},
  {"bor", MUTATE_OP_BOR},
  {"ror", MUTATE_OP_ROR},
  {"sor", MUTATE_OP_SOR},
  {"uoi", MUTATE_OP_UOI},
  {"cr", MUTATE_OP_CR},
  {"rvr", MUTATE_OP_RVR},
  {"vcr", MUTATE_OP_VCR},
  {"amr", MUTATE_OP_AMR},
};

uint16_t mutation_category_bit(const char* name, size_t len)
{
  for(size_t i = 0; i < sizeof(categories) / sizeof(categories[0]); i++)
  {
    if((strlen(categories[i].name) == len) &&
      (memcmp(categories[i].name, name, len) == 0))
      return categories[i].bit;
  }

  return 0;
}

static mutation_mode_t mutation_mode(ast_t* annotation)
{
  if(annotation == NULL)
    return MUTATION_DEFAULT;

  const char* name = ast_name(annotation);
  if(strcmp(name, "nomutate") == 0)
    return MUTATION_NONE;
  if(strcmp(name, "mutate_only") == 0)
    return MUTATION_ONLY;
  if(strcmp(name, "mutate_except") == 0)
    return MUTATION_EXCEPT;
  return MUTATION_DEFAULT;
}

static ast_t* mutation_selector(ast_t* ast)
{
  ast_t* annotations = ast_annotation(ast);
  if(annotations == NULL)
    return NULL;

  for(ast_t* a = ast_child(annotations); a != NULL; a = ast_sibling(a))
  {
    if(mutation_mode(a) != MUTATION_DEFAULT)
      return a;
  }

  return NULL;
}

static bool selector_location(ast_t* declaration)
{
  switch(ast_id(declaration))
  {
    case TK_ACTOR:
    case TK_CLASS:
    case TK_PRIMITIVE:
    case TK_STRUCT:
      return true;

    case TK_FUN:
    case TK_BE:
    case TK_NEW:
      return ast_id(ast_childidx(declaration, 7)) != TK_NONE;

    default:
      return false;
  }
}

bool mutation_check_selector(pass_opt_t* opt, ast_t* annotation)
{
  const char* name = ast_name(annotation);
  ast_t* annotations = ast_parent(annotation);

  if(!selector_location(ast_parent(annotations)))
  {
    ast_error(opt->check.errors, annotation,
      "'%s' can only appear on a concrete type or a method with a body", name);
    return false;
  }

  for(ast_t* a = ast_child(annotations); a != NULL; a = ast_sibling(a))
  {
    if((a != annotation) && (mutation_mode(a) != MUTATION_DEFAULT))
    {
      ast_error(opt->check.errors, annotation,
        "'%s' cannot appear with another mutation annotation", name);
      return false;
    }
  }

  ast_t* arg = ast_child(annotation);
  if((arg == NULL) || (ast_id(arg) != TK_ID))
  {
    ast_error(opt->check.errors, annotation,
      "'%s' requires a list of mutation categories", name);
    return false;
  }

  bool ok = true;
  for(; arg != NULL; arg = ast_sibling(arg))
  {
    if(mutation_category_bit(ast_name(arg), ast_name_len(arg)) == 0)
    {
      ast_error(opt->check.errors, arg, "unknown mutation category '%s'",
        ast_name(arg));
      ok = false;
    }
  }

  return ok;
}

void mutation_inherit_body_selector(ast_t* method, ast_t* adopted_body)
{
  if(mutation_selector(adopted_body) != NULL)
    return;

  ast_t* selector = mutation_selector(method);
  if(selector == NULL)
    return;

  // An adopted body and its receiving signature can come from different traits.
  ast_t* annotations = ast_annotation(adopted_body);
  if(annotations == NULL)
  {
    annotations = ast_from(selector, TK_ANNOTATION);
    ast_setannotation(adopted_body, annotations);
  }

  ast_append(annotations, selector);
}

static uint16_t category_mask(ast_t* selector)
{
  uint16_t mask = 0;
  for(ast_t* arg = ast_child(selector); arg != NULL; arg = ast_sibling(arg))
    mask |= mutation_category_bit(ast_name(arg), ast_name_len(arg));
  return mask;
}

mutation_type_policy_t mutation_type_policy(ast_t* entity)
{
  mutation_type_policy_t policy =
    {NULL, ast_name(ast_child(entity)), MUTATE_OP_ALL};
  ast_t* selector = mutation_selector(entity);

  switch(mutation_mode(selector))
  {
    case MUTATION_ONLY:
      policy.limit = selector;
      policy.categories = category_mask(selector);
      break;

    case MUTATION_EXCEPT:
      policy.limit = selector;
      policy.categories &= (uint16_t)~category_mask(selector);
      break;

    case MUTATION_NONE:
      policy.categories = 0;
      break;

    default:
      break;
  }

  return policy;
}

static bool resolve_scope(pass_opt_t* opt, ast_t* scope, ast_t* selector,
  const mutation_type_policy_t* policy)
{
  uint16_t allowed = policy->categories;
  bool ok = true;

  switch(mutation_mode(selector))
  {
    case MUTATION_ONLY:
    {
      uint16_t requested = category_mask(selector);
      uint16_t outside_limit = requested & (uint16_t)~allowed;

      if((policy->limit != NULL) && (outside_limit != 0))
      {
        for(ast_t* arg = ast_child(selector); arg != NULL;
          arg = ast_sibling(arg))
        {
          uint16_t bit =
            mutation_category_bit(ast_name(arg), ast_name_len(arg));
          if((outside_limit & bit) != 0)
          {
            ast_error(opt->check.errors, arg,
              "type '%s' excludes mutation category '%s'", policy->type_name,
              ast_name(arg));
            ast_error_continue(opt->check.errors, policy->limit,
              "the type's mutation categories are limited here");
            outside_limit &= (uint16_t)~bit;
          }
        }

        ok = false;
      }

      allowed = requested;
      break;
    }

    case MUTATION_EXCEPT:
      allowed &= (uint16_t)~category_mask(selector);
      break;

    case MUTATION_NONE:
      allowed = 0;
      break;

    default:
      break;
  }

  if(opt->mutate)
    ast_exclude_mutations(scope, MUTATE_OP_ALL & ~allowed);

  return ok;
}

bool mutation_resolve_method(pass_opt_t* opt,
  const mutation_type_policy_t* policy, ast_t* method)
{
  ast_t* method_selector = mutation_selector(method);
  ast_t* body = ast_childidx(method, 7);
  ast_t* body_selector = mutation_selector(body);

  bool ok = true;
  if(ast_id(body) != TK_NONE)
    ok = resolve_scope(opt, body,
      body_selector != NULL ? body_selector : method_selector, policy);

  if(ok)
    ok = resolve_scope(opt, ast_childidx(method, 3), method_selector, policy);

  return ok;
}
