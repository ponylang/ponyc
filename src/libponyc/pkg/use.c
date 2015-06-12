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


/** Use commands are of the form:
 *   use "[scheme:]locator" [as name] [where condition]
 *
 * Each scheme has a handler function. The first handler ("package:") is used
 * as the default for commands that do not specify a scheme.
 *
 * An extra scheme handler for "test:" can be setup for testing.
 */


struct
{
  const char* scheme; // Interned textual identifier including :
  size_t scheme_len;  // Number of characters in scheme, including :
  bool allow_name;    // Is the name clause allowed
  use_handler_t handler;
} handlers[] =
{
  {"package:", 5, true, use_package},
  {"lib:", 4, false, use_library},
  {"path:", 5, false, use_path},

  {"test:", 5, false, NULL},  // For testing
  {NULL, 0, false, NULL}  // Terminator
};


void use_test_handler(use_handler_t handler, bool allow_alias)
{
  for(int i = 0; handlers[i].scheme != NULL; i++)
  {
    if(strcmp(handlers[i].scheme, "test:") == 0)
    {
      handlers[i].allow_name = allow_alias;
      handlers[i].handler = handler;
      return;
    }
  }
}


static bool eval_condition(ast_t* ast, bool release, bool* error);


// Find the index of the handler for the given URI
static int find_handler(ast_t* uri, const char** out_locator)
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
    return 0;
  }

  size_t scheme_len = colon - text + 1;  // +1 for colon

  // Search for matching handler
  for(int i = 0; handlers[i].scheme != NULL; i++)
  {
    if(handlers[i].scheme_len == scheme_len &&
      strncmp(handlers[i].scheme, text, scheme_len) == 0)
    {
      if(handlers[i].handler == NULL) // No handler provided (probably test:)
        break;

      // Matching scheme found
      *out_locator = stringtab(colon + 1);
      return i;
    }
  }

  // No match found
#ifdef PLATFORM_IS_WINDOWS
  if(colon == text + 1)
  {
    // Special case error message
    ast_error(uri, "Use scheme %c: not found. "
      "If this is an absolute path use prefix \"package:\"", text[0]);
    return -1;
  }
#endif

  ast_error(uri, "Use scheme %.*s not found", (int)scheme_len, text);
  return -1;
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

    case TK_TRUE:
      return true;

    case TK_FALSE:
      return false;

    default:
      ast_error(ast, "Invalid use guard expression");
      *error = true;
      return false;
  }
}


// Handle condition, if any
static ast_result_t process_condition(ast_t* cond, bool release)
{
  assert(cond != NULL);

  if(ast_id(cond) == TK_NONE) // No condition provided
    return AST_OK;

  // Evaluate condition expression
  bool error = false;
  bool val = eval_condition(cond, release, &error);

  if(error) // Couldn't evaluate condition, give up compilation
    return AST_ERROR;

  if(!val)  // Condition is false
    return AST_IGNORE;

  // Condition is true
  return AST_OK;
}


// Handle a uri command
static bool uri_command(ast_t* ast, ast_t* uri, ast_t* alias,
  pass_opt_t* options)
{
  assert(uri != NULL);
  assert(alias != NULL);

  const char* locator;
  int index = find_handler(uri, &locator);

  if(index < 0) // Scheme not found
    return false;

  if(ast_id(alias) != TK_NONE && !handlers[index].allow_name)
  {
    ast_error(alias, "Use scheme %s may not have an alias",
      handlers[index].scheme);
    return false;
  }

  assert(handlers[index].handler != NULL);
  return handlers[index].handler(ast, locator, alias, options);
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


ast_result_t use_command(ast_t* ast, pass_opt_t* options)
{
  assert(ast != NULL);
  assert(options != NULL);

  AST_GET_CHILDREN(ast, alias, spec, condition);
  assert(spec != NULL);
  assert(condition != NULL);

  ast_result_t r = process_condition(condition, options->release);

  if(r != AST_OK)
    return r;

  // Guard condition is true (or not present)
  switch(ast_id(spec))
  {
  case TK_STRING:
    if(!uri_command(ast, spec, alias, options))
      return AST_ERROR;

    break;

  case TK_FFIDECL:
    if(!ffi_command(alias))
      return AST_ERROR;

    break;

  default:
    assert(0);
    return AST_ERROR;
  }

  return AST_OK;
}
