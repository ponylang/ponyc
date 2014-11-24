#include "parsefix.h"
#include "../ast/token.h"
#include "../pkg/package.h"
#include "../type/assemble.h"
#include "../ds/stringtab.h"
#include <string.h>
#include <assert.h>


// Allow all code, just fixup tree
static bool allow_all = false;


#define DEF_CLASS 0
#define DEF_ACTOR 1
#define DEF_PRIMITIVE 2
#define DEF_TRAIT 3
#define DEF_INTERFACE 4
#define DEF_ENTITY_COUNT 5

#define DEF_FUN 0
#define DEF_BE 5
#define DEF_NEW 10
#define DEF_METHOD_COUNT 15


typedef struct permission_def_t
{
  const char* desc;
  const char* permissions;
} permission_def_t;

// Element permissions are specified by strings with a single character for
// each element.
// Y indicates the element must be present.
// N indicates the element must not be present.
// X indicates the element is optional.
// The entire permission string being NULL indicates that the whole thing is
// not allowed.

#define ENTITY_MAIN 0
#define ENTITY_FIELD 2
#define ENTITY_CAP 4

// Index by DEF_<ENTITY>
static const permission_def_t _entity_def[DEF_ENTITY_COUNT] =
{ //                           Main
  //                           | field
  //                           | | cap
  //                           | | |
  { "class",                  "N X X" },
  { "actor",                  "X X N" },
  { "primitive",              "N N N" },
  { "trait",                  "N N X" },
  { "interface",              "N N X" }
};

#define METHOD_CAP 0
#define METHOD_AT 2
#define METHOD_NAME 4
#define METHOD_RETURN 6
#define METHOD_ERROR 8
#define METHOD_BODY 10

// Index by DEF_<ENTITY> + DEF_<METHOD>
static const permission_def_t _method_def[DEF_METHOD_COUNT] =
{ //                           cap
  //                           | at
  //                           | | name
  //                           | | | return
  //                           | | | | error
  //                           | | | | | body
  { "class function",         "Y N X X X Y" },
  { "actor function",         "Y X X X X Y" },
  { "primitive function",     "Y N X X X Y" },
  { "trait function",         "Y N X X X X" },
  { "interface function",     "Y N X X X X" },
  { "class behaviour",        NULL },
  { "actor behaviour",        "N X Y N N Y" },
  { "primitive behaviour",    NULL },
  { "trait behaviour",        "N N Y N N X" },
  { "interface behaviour",    "N N Y N N X" },
  { "class constructor",      "N N X N X Y" },
  { "actor constructor",      "N X X N N Y" },
  { "primitive constructor",  "N N X N X Y" },
  { "trait constructor",      NULL },
  { "interface constructor",  NULL }
};


/*
static bool typecheck_main(ast_t* ast)
{
  // TODO: check create exists, takes no type params and has correct sig (Env->None)
  const char* m = stringtab("Main");
  AST_GET_CHILDREN(ast, id, typeparams);

  if(ast_name(id) != m)
    return true;

  if(ast_id(ast) != TK_ACTOR) // Now checked elsewhere
  {
    ast_error(ast, "Main must be an actor");
    return false;
  }

  if(ast_id(typeparams) != TK_NONE) // Not currently checked
  {
    ast_error(ast, "Main cannot have type parameters");
    return false;
  }

  return true;
}
*/


static bool check_traits(ast_t* traits)
{
  assert(traits != NULL);
  ast_t* trait = ast_child(traits);

  while(trait != NULL)
  {
    if(ast_id(trait) != TK_NOMINAL)
    {
      ast_error(trait, "traits must be nominal types");
      return false;
    }

    AST_GET_CHILDREN(trait, ignore0, ignore1, ignore2, cap, ephemeral);

    if(ast_id(cap) != TK_NONE)
    {
      ast_error(cap, "can't specify a capability on a trait");
      return false;
    }

    if(ast_id(ephemeral) != TK_NONE)
    {
      ast_error(ephemeral, "a trait can't be ephemeral");
      return false;
    }

    trait = ast_sibling(trait);
  }

  return true;
}


// Check permission for one specific element of a method or entity
static bool check_permission(const permission_def_t* def, int element,
  ast_t* actual, const char* context, ast_t* report_at)
{
  assert(def != NULL);
  assert(actual != NULL);
  assert(context != NULL);

  char permission = def->permissions[element];

  assert(permission == 'Y' || permission == 'N' || permission == 'X');

  if(permission == 'N' && ast_id(actual) != TK_NONE)
  {
    ast_error(actual, "%s cannot specify %s", def->desc, context);
    return false;
  }

  if(permission == 'Y' && ast_id(actual) == TK_NONE)
  {
    ast_error(report_at, "%s must specify %s", def->desc, context);
    return false;
  }

  return true;
}


// Check whether the given method has any illegal parts
static bool check_method(ast_t* ast, int method_def_index)
{
  assert(ast != NULL);
  assert(method_def_index >= 0 && method_def_index < DEF_METHOD_COUNT);

  const permission_def_t* def = &_method_def[method_def_index];

  if(!allow_all && def->permissions == NULL)
  {
    ast_error(ast, "%ss are not allowed", def->desc);
    return false;
  }

  AST_GET_CHILDREN(ast, cap, id, type_params, params, return_type,
    error, body, c_api, arrow);

  if(allow_all)
    return true;

  if(!check_permission(def, METHOD_CAP, cap, "receiver capability", cap))
    return false;

  if(ast_id(cap) == TK_ISO || ast_id(cap) == TK_TRN)
  {
    ast_error(cap, "receiver capability must not be iso or trn");
    return false;
  }

  if(!check_permission(def, METHOD_AT, c_api, "C callback", c_api))
    return false;

  if(!check_permission(def, METHOD_NAME, id, "name", id))
    return false;

  if(!check_permission(def, METHOD_RETURN, return_type, "return type", ast))
    return false;

  if(!check_permission(def, METHOD_ERROR, error, "?", ast))
    return false;

  if(!check_permission(def, METHOD_BODY, body, "body", ast))
    return false;

  token_id arrow_id = ast_id(arrow);

  if(arrow_id == TK_DBLARROW && ast_id(body) == TK_NONE)
  {
    ast_error(body, "Method body expected after =>");
    return false;
  }

  if(arrow_id == TK_NONE && ast_id(body) == TK_SEQ)
  {
    ast_error(body, "Missing => before method body");
    return false;
  }

  return true;
}


// Check that we haven't already seen the given kind of method
static bool check_seen_member(ast_t* member, const char* member_kind,
  const char* seen_name, const char* seen_kind)
{
  if(seen_name == NULL)
    return true;

  ast_error(member, "%s %s comes after %s %s", member_kind,
    ast_name(ast_childidx(member, 1)), seen_kind, seen_name);
  return false;
}


// Check whether the given entity members are legal in their entity
static bool check_members(ast_t* members, int entity_def_index)
{
  assert(members != NULL);
  assert(entity_def_index >= 0 && entity_def_index < DEF_ENTITY_COUNT);

  const permission_def_t* def = &_entity_def[entity_def_index];
  ast_t* member = ast_child(members);
  const char* be_name = NULL;
  const char* fun_name = NULL;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FLET:
      case TK_FVAR:
        if(def->permissions[ENTITY_FIELD] == 'N')
        {
          ast_error(member, "Can't have fields in %s", def->desc);
          return false;
        }
        break;

      case TK_NEW:
        if(!check_method(member, entity_def_index + DEF_NEW) ||
          !check_seen_member(member, "constructor", be_name, "behaviour") ||
          !check_seen_member(member, "constructor", fun_name, "function"))
          return false;

        break;

      case TK_BE:
      {
        if(!check_method(member, entity_def_index + DEF_BE) ||
          !check_seen_member(member, "behaviour", fun_name, "function"))
          return false;

        ast_t* id = ast_childidx(member, 1);

        if(ast_id(id) != TK_NONE)
          be_name = ast_name(id);
        else
          be_name = "apply";
        break;
      }

      case TK_FUN:
      {
        if(!check_method(member, entity_def_index + DEF_FUN))
          return false;

        ast_t* id = ast_childidx(member, 1);

        if(ast_id(id) != TK_NONE)
          fun_name = ast_name(id);
        else
          fun_name = "apply";
        break;
      }

      default:
        assert(0);
        return false;
    }

    member = ast_sibling(member);
  }

  return true;
}


// Check whether the given entity has illegal parts
static ast_result_t parse_fix_entity(ast_t* ast, int entity_def_index)
{
  assert(ast != NULL);
  assert(entity_def_index >= 0 && entity_def_index < DEF_ENTITY_COUNT);

  const permission_def_t* def = &_entity_def[entity_def_index];
  AST_GET_CHILDREN(ast, id, typeparams, defcap, provides, members);

  // Check if we're called Main
  if(def->permissions[ENTITY_MAIN] == 'N' && ast_name(id) == stringtab("Main"))
  {
    ast_error(ast, "Main must be an actor");
    return AST_ERROR;
  }

  if(!check_permission(def, ENTITY_CAP,  defcap, "default capability", defcap))
    return AST_ERROR;

  // Check referenced traits
  if(!check_traits(provides))
    return AST_ERROR;

  // Check for illegal members
  if(!check_members(members, entity_def_index))
    return AST_ERROR;

  return AST_OK;
}


static ast_result_t parse_fix_type_alias(ast_t* ast)
{
  // Check if we're called Main
  if(ast_name(ast_child(ast)) == stringtab("Main"))
  {
    ast_error(ast, "Main must be an actor");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t parse_fix_thistype(ast_t* ast)
{
  assert(ast != NULL);
  ast_t* parent = ast_parent(ast);
  assert(parent != NULL);

  if(ast_id(parent) != TK_ARROW)
  {
    ast_error(ast, "in a type, 'this' can only be used as a viewpoint");
    return AST_ERROR;
  }

  if(ast_enclosing_method(ast) == NULL)
  {
    ast_error(ast, "can only use 'this' for a viewpoint in a method");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t parse_fix_ephemeral(ast_t* ast)
{
  assert(ast != NULL);

  if((ast_enclosing_method_type(ast) == NULL) &&
    (ast_enclosing_ffi_type(ast) == NULL))
  {
    ast_error(ast, "ephemeral types can only appear in function return types");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t parse_fix_borrowed(ast_t* ast)
{
  assert(ast != NULL);

  if(ast_id(ast_parent(ast)) != TK_NOMINAL)
    return AST_OK;

  if(ast_enclosing_local_type(ast) == NULL)
  {
    ast_error(ast, "borrowed types can only appear in parameters or locals");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t parse_fix_match(ast_t* ast)
{
  assert(ast != NULL);

  // The last case must have a body
  ast_t* cases = ast_childidx(ast, 1);
  assert(cases != NULL);
  assert(ast_id(cases) == TK_CASES);

  ast_t* case_ast = ast_child(cases);

  if(case_ast == NULL)  // There are no bodies
    return AST_OK;

  while(ast_sibling(case_ast) != NULL)
    case_ast = ast_sibling(case_ast);

  ast_t* body = ast_childidx(case_ast, 2);

  if(ast_id(body) == TK_NONE)
  {
    ast_error(case_ast, "Last case in match must have a body");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t parse_fix_ffi(ast_t* ast, bool return_optional)
{
  assert(ast != NULL);
  AST_GET_CHILDREN(ast, idseq, typeargs, args, named_args);

  // Prefix '@' to the concatenated name.
  assert(idseq != NULL);
  ast_t* id = ast_child(idseq);

  // Space for @ and terminator.
  size_t len = 2;

  while(id != NULL)
  {
    // Space for trailing dot if needed.
    const char* name = ast_name(id);
    len += strlen(name) + 1;
    id = ast_sibling(id);
  }

  VLA(char, new_name, len);
  new_name[0] = '@';

  id = ast_child(idseq);
  size_t offset = 1;

  while(id != NULL)
  {
    const char* name = ast_name(id);
    len = strlen(name);
    memcpy(new_name + offset, name, len);
    offset += len;

    id = ast_sibling(id);

    if(id != NULL)
    {
      new_name[offset] = '.';
      offset++;
    }
  }

  new_name[offset] = '\0';
  id = ast_from_string(idseq, new_name);
  ast_replace(&idseq, id);

  if((ast_child(typeargs) == NULL && !return_optional) ||
    ast_childidx(typeargs, 1) != NULL)
  {
    ast_error(typeargs, "FFIs must specify a single return type");
    return AST_ERROR;
  }

  for(ast_t* p = ast_child(args); p != NULL; p = ast_sibling(p))
  {
    if(ast_id(p) == TK_PARAM)
    {
      ast_t* def_val = ast_childidx(p, 2);
      assert(def_val != NULL);

      if(ast_id(def_val) != TK_NONE)
      {
        ast_error(def_val, "FFIs parameters cannot have default values");
        return AST_ERROR;
      }
    }
  }

  if(ast_id(named_args) != TK_NONE)
  {
    ast_error(typeargs, "FFIs cannot take named arguments");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t parse_fix_ellipsis(ast_t* ast)
{
  assert(ast != NULL);

  ast_t* fn = ast_parent(ast_parent(ast));
  assert(fn != NULL);

  if(ast_id(fn) != TK_FFIDECL)
  {
    ast_error(ast, "... may only appear in FFI declarations");
    return AST_ERROR;
  }

  if(ast_sibling(ast) != NULL)
  {
    ast_error(ast, "... must be the last parameter");
    return AST_ERROR;
  }

  return AST_OK;
}


static bool is_expr_infix(token_id id)
{
  switch(id)
  {
    case TK_AND:
    case TK_OR:
    case TK_XOR:
    case TK_PLUS:
    case TK_MINUS:
    case TK_MULTIPLY:
    case TK_DIVIDE:
    case TK_MOD:
    case TK_LSHIFT:
    case TK_RSHIFT:
    case TK_IS:
    case TK_ISNT:
    case TK_EQ:
    case TK_NE:
    case TK_LT:
    case TK_LE:
    case TK_GE:
    case TK_GT:
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      return true;

    default:
      return false;
  }
}


static ast_result_t parse_fix_infix_expr(ast_t* ast)
{
  assert(ast != NULL);
  AST_GET_CHILDREN(ast, left, right);

  token_id op = ast_id(ast);

  assert(left != NULL);
  token_id left_op = ast_id(left);
  bool left_clash = (left_op != op) && is_expr_infix(left_op);

  assert(right != NULL);
  token_id right_op = ast_id(right);
  bool right_clash = (right_op != op) && is_expr_infix(right_op);

  if(left_clash || right_clash)
  {
    ast_error(ast,
      "Operator precedence is not supported. Parentheses required.");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t parse_fix_consume(ast_t* ast)
{
  AST_GET_CHILDREN(ast, term);

  if(ast_id(term) != TK_REFERENCE)
  {
    ast_error(term, "Consume expressions must specify a single identifier");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t parse_fix_lparen(ast_t** astp)
{
  // Remove TK_LPAREN nodes
  ast_t* child = ast_pop(*astp);
  assert(child != NULL);
  ast_replace(astp, child);

  // The recursive descent pass won't now process our child because it thinks
  // that's us. So we have to process our child explicitly.
  return pass_parse_fix(astp, NULL);
}


static ast_result_t parse_fix_docstring(ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, typeparams, defcap, provides, docstring, members);

  // Move the docstring to the end
  ast_append(ast, docstring);
  ast_remove(docstring);

  return AST_OK;
}


static ast_result_t parse_fix_nominal(ast_t* ast)
{
  // If we didn't have a package, the first two children will be ID NONE
  // change them to NONE ID so the package is always first
  ast_t* package = ast_child(ast);
  ast_t* type = ast_sibling(package);

  if(ast_id(type) == TK_NONE)
  {
    ast_pop(ast);
    ast_pop(ast);
    ast_add(ast, package);
    ast_add(ast, type);
  }

  return AST_OK;
}


static ast_result_t parse_fix_nodeorder(ast_t* ast)
{
  // Swap the order of the nodes. We want to typecheck the right side before
  // the left side to make sure we handle consume tracking correctly. This
  // applies to TK_ASSIGN and TK_CALL.
  ast_t* left = ast_pop(ast);
  ast_append(ast, left);
  return AST_OK;
}


static ast_result_t parse_fix_test_scope(ast_t* ast)
{
  // Only allowed in testing
  if(!allow_all)
  {
    ast_error(ast, "Unexpected use command in method body");
    return AST_FATAL;
  }

  // Replace tests cope with a normal sequence scope
  ast_setid(ast, TK_SEQ);

  if(ast_id(ast_child(ast)) == TK_COLON)
    ast_scope(ast);

  ast_free(ast_pop(ast));
  return AST_OK;
}


ast_result_t pass_parse_fix(ast_t** astp, pass_opt_t* options)
{
  assert(astp != NULL);
  ast_t* ast = *astp;
  assert(ast != NULL);

  token_id id = ast_id(ast);

  // Functions that fix up the tree
  switch(id)
  {
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
    case TK_TRAIT:
    case TK_INTERFACE:  parse_fix_docstring(ast); break;
    case TK_NOMINAL:    return parse_fix_nominal(ast);
    case TK_ASSIGN:
    case TK_CALL:       return parse_fix_nodeorder(ast);
    case TK_TEST_SCOPE: return parse_fix_test_scope(ast);

    default: break;
  }

  if(allow_all)
  {
    // Don't check anything, just fix up the tree
    if(id == TK_FUN || id == TK_BE || id == TK_NEW)
      check_method(ast, 0);

    return AST_OK;
  }

  // Functions that just check for illegal code
  switch(id)
  {
    case TK_TYPE:       return parse_fix_type_alias(ast);
    case TK_PRIMITIVE:  return parse_fix_entity(ast, DEF_PRIMITIVE);
    case TK_CLASS:      return parse_fix_entity(ast, DEF_CLASS);
    case TK_ACTOR:      return parse_fix_entity(ast, DEF_ACTOR);
    case TK_TRAIT:      return parse_fix_entity(ast, DEF_TRAIT);
    case TK_INTERFACE:  return parse_fix_entity(ast, DEF_INTERFACE);
    case TK_THISTYPE:   return parse_fix_thistype(ast);
    case TK_EPHEMERAL:  return parse_fix_ephemeral(ast);
    case TK_BORROWED:   return parse_fix_borrowed(ast);
    case TK_MATCH:      return parse_fix_match(ast);
    case TK_FFIDECL:    return parse_fix_ffi(ast, false);
    case TK_FFICALL:    return parse_fix_ffi(ast, true);
    case TK_ELLIPSIS:   return parse_fix_ellipsis(ast);
    case TK_CONSUME:    return parse_fix_consume(ast);
    case TK_LPAREN:
    case TK_LPAREN_NEW: return parse_fix_lparen(astp);
    default: break;
  }

  if(is_expr_infix(id))
    return parse_fix_infix_expr(ast);

  return AST_OK;
}


void parse_fix_allow_all(bool allow)
{
  allow_all = allow;
}
