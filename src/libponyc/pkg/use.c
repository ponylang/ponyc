#include "use.h"
#include "../ast/ast.h"
#include "package.h"
#include "platformfuns.h"
#include "program.h"
#include "../pass/pass.h"
#include "../pass/scope.h"
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


use_t* handlers = NULL;
use_t* default_handler = NULL;


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
    *out_locator = text;
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
      *out_locator = colon + 1;
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


// Evalute the children of the given binary condition node
static void eval_binary(ast_t* ast, bool* out_left, bool* out_right,
  bool release, bool* error)
{
  assert(ast != NULL);
  assert(out_left != NULL);
  assert(out_right != NULL);

  AST_GET_CHILDREN(ast, left, right);
  *out_left = eval_condition(left, release, error);
  *out_right = eval_condition(right, release, error);
}


// Evaluate the given condition AST
static bool eval_condition(ast_t* ast, bool release, bool* error)
{
  assert(ast != NULL);

  bool left, right;

  switch(ast_id(ast))
  {
  case TK_AND:
    eval_binary(ast, &left, &right, release, error);
    return left & right;

  case TK_OR:
    eval_binary(ast, &left, &right, release, error);
    return left | right;

  case TK_XOR:
    eval_binary(ast, &left, &right, release, error);
    return left ^ right;

  case TK_NOT:
    return !eval_condition(ast_child(ast), release, error);

  case TK_TUPLE:
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
    assert(0);
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
bool uri_command(ast_t* ast, ast_t* uri, ast_t* alias, pass_opt_t* options)
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
bool ffi_command(ast_t* alias)
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

  use_t* s = (use_t*)calloc(1, sizeof(use_t));
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
    free(p);
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
