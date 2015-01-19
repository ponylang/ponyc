#include "use.h"
#include "../ast/ast.h"
#include "package.h"
#include "platformfuns.h"
#include "program.h"
#include "../pass/pass.h"
#include "../pass/scope.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <assert.h>


typedef struct use_t
{
  const char* scheme; // Interned textual identifier including :
  size_t scheme_len;  // Number of characters in scheme, including :
  bool allow_name;    // Is the name clause allowed
  use_handler_t handler;
  struct use_t* next;
} use_t;


static bool eval_condition(ast_t* ast, bool release, bool* error);


static use_t* handlers = NULL;
static use_t* default_handler = NULL;


// Find the handler for the given URI
static use_t* find_handler(ast_t* uri, const char** out_locator)
{
  assert(uri != NULL);
  assert(out_locator != NULL);
  assert(ast_id(uri) == TK_STRING);

  const char* text = ast_name(uri);
  const char* colon = strchr(text, ':');

  if(colon == NULL)
  {
    // No scheme specified, use default
    *out_locator = stringtab(text);
    return default_handler;
  }

  size_t scheme_len = colon - text + 1;  // +1 for colon

  // Search for matching handler
  for(use_t* p = handlers; p != NULL; p = p->next)
  {
    if(p->scheme_len == scheme_len &&
      strncmp(p->scheme, text, scheme_len) == 0)
    {
      // Matching scheme found
      *out_locator = stringtab(colon + 1);
      return p;
    }
  }

  // No match found
#ifdef PLATFORM_IS_WINDOWS
  if(colon == text + 1)
  {
    // Special case error message
    ast_error(uri, "Use scheme %c: not found. "
      "If this is an absolute path use prefix \"file:\"", text[0]);
    return NULL;
  }
#endif

  ast_error(uri, "Use scheme %.*s not found", (int)scheme_len, text);
  return NULL;
}


// Evalute the argument (if any) of the given condition call
static bool eval_call_arg(size_t expected_count, ast_t* positional,
  bool release, bool* error)
{
  assert(expected_count == 0 || expected_count == 1);
  assert(positional != NULL);
  assert(error != NULL);

  size_t actual_count = ast_childcount(positional);

  if(actual_count != expected_count)
  {
    ast_error(positional, "Invalid arguments");
    *error = true;
    return false;
  }

  if(actual_count == 0)
    return true;

  return eval_condition(ast_child(positional), release, error);
}


// Evaluate the given condition call
static bool eval_call(ast_t* ast, bool release, bool* error)
{
  assert(ast != NULL);
  assert(error != NULL);

  AST_GET_CHILDREN(ast, positional, named, lhs);
  assert(lhs != NULL);
  assert(positional != NULL);
  assert(named != NULL);

  if(ast_id(lhs) != TK_DOT || ast_id(named) != TK_NONE)
  {
    ast_error(ast, "Invalid use guard expression");
    *error = true;
    return false;
  }

  AST_GET_CHILDREN(lhs, left, call_id);

  bool left_val = eval_condition(left, release, error);

  if(*error)
    return false;

  const char* call_name = ast_name(call_id);

  if(strcmp(call_name, "op_and") == 0)
    return left_val & eval_call_arg(1, positional, release, error);

  if(strcmp(call_name, "op_or") == 0)
    return left_val | eval_call_arg(1, positional, release, error);

  if(strcmp(call_name, "op_xor") == 0)
    return left_val ^ eval_call_arg(1, positional, release, error);

  if(strcmp(call_name, "op_not") == 0)
    return eval_call_arg(0, positional, release, error) && !left_val;

  ast_error(ast, "Invalid use guard expression");
  *error = true;
  return false;
}


// Evaluate the given condition AST
static bool eval_condition(ast_t* ast, bool release, bool* error)
{
  assert(ast != NULL);
  assert(error != NULL);

  switch(ast_id(ast))
  {
    case TK_CALL:
      return eval_call(ast, release, error);

    case TK_TUPLE:
      return eval_condition(ast_child(ast), release, error);

    case TK_SEQ:
      if(ast_childidx(ast, 1) != NULL)
      {
        ast_error(ast, "Sequence not allowed in use guard expression");
        *error = true;
        return false;
      }

      return eval_condition(ast_child(ast), release, error);

    case TK_REFERENCE:
      {
        const char* name = ast_name(ast_child(ast));
        bool result;
        if(os_is_target(name, release, &result))
          return result;

        ast_error(ast, "\"%s\" is not a valid use condition value\n", name);
        *error = true;
        return false;
      }
      return true;

    default:
      ast_error(ast, "Invalid use guard expression");
      *error = true;
      return false;
  }
}


// Handle condition, if any
// @return true if we should continue with use, false if we should skip it
static bool process_condition(ast_t* ast, ast_t* cond, bool release,
  bool* out_retval)
{
  assert(cond != NULL);
  assert(out_retval != NULL);

  if(ast_id(cond) == TK_NONE) // No condition provided
    return true;

  // Evaluate condition expression
  bool error = false;
  bool val = eval_condition(cond, release, &error);

  if(error)
  {
    // Couldn't evaluate condition, give up compilation
    *out_retval = false;
    return false;
  }

  if(!val)
  {
    // Condition is false, remove whole use command
    ast_erase(ast);
    *out_retval = true;
    return false;
  }

  // Condition is true
  // Free condition AST to prevent type check errors
  ast_erase(cond);
  return true;
}


// Handle a uri command
static bool uri_command(ast_t* ast, ast_t* uri, ast_t* alias,
  pass_opt_t* options)
{
  assert(uri != NULL);
  assert(alias != NULL);

  if(handlers == NULL)
  {
    ast_error(uri, "Internal error: no use scheme handlers registered");
    return false;
  }

  assert(default_handler != NULL);

  const char* locator;
  use_t* handler = find_handler(uri, &locator);

  if(handler == NULL) // Scheme not found
    return false;

  if(ast_id(alias) != TK_NONE && !handler->allow_name)
  {
    ast_error(alias, "Use scheme %s may not have an alias", handler->scheme);
    return false;
  }

  return handler->handler(ast, locator, alias, options);
}


// Handle an ffi command
static bool ffi_command(ast_t* alias)
{
  assert(alias != NULL);

  if(ast_id(alias) != TK_NONE)
  {
    ast_error(alias, "Use FFI may not have an alias");
    return false;
  }

  return true;
}


void use_register_std()
{
  use_register_handler("file:", true, use_package);
  use_register_handler("lib:", true, use_library);
}


void use_register_handler(const char* scheme, bool allow_name,
  use_handler_t handler)
{
  assert(scheme != NULL);
  assert(handler != NULL);

  use_t* s = POOL_ALLOC(use_t);
  s->scheme = stringtab(scheme);
  s->scheme_len = strlen(scheme);
  s->allow_name = allow_name;
  s->handler = handler;
  s->next = handlers;

  handlers = s;

  if(default_handler == NULL)
    default_handler = s;
}


void use_clear_handlers()
{
  use_t* p = handlers;

  while(p != NULL)
  {
    use_t* next = p->next;
    POOL_FREE(use_t, p);
    p = next;
  }

  handlers = NULL;
  default_handler = NULL;
}


bool use_command(ast_t* ast, pass_opt_t* options)
{
  assert(ast != NULL);
  assert(options != NULL);

  AST_GET_CHILDREN(ast, alias, spec, condition);
  assert(spec != NULL);
  assert(condition != NULL);

  bool r;
  if(!process_condition(ast, condition, options->release, &r))
    return r;

  // Guard condition is true (or not present)
  switch(ast_id(spec))
  {
  case TK_STRING:  return uri_command(ast, spec, alias, options);
  case TK_FFIDECL: return ffi_command(alias);

  default:
    assert(0);
    return false;
  }
}
