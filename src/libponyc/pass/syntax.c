#include "syntax.h"
#include "../ast/id.h"
#include "../ast/parser.h"
#include "../ast/token.h"
#include "../pkg/package.h"
#include "../type/assemble.h"
#include "../ast/stringtab.h"
#include <assert.h>


#define DEF_CLASS 0
#define DEF_ACTOR 1
#define DEF_PRIMITIVE 2
#define DEF_TRAIT 3
#define DEF_INTERFACE 4
#define DEF_TYPEALIAS 5
#define DEF_ENTITY_COUNT 6

#define DEF_FUN 0
#define DEF_BE 6
#define DEF_NEW 12
#define DEF_METHOD_COUNT 18


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
#define ENTITY_C_API 6

// Index by DEF_<ENTITY>
static const permission_def_t _entity_def[DEF_ENTITY_COUNT] =
{ //                           Main
  //                           | field
  //                           | | cap
  //                           | | | c_api
  { "class",                  "N X X N" },
  { "actor",                  "X X N X" },
  { "primitive",              "N N N N" },
  { "trait",                  "N N X N" },
  { "interface",              "N N X N" },
  { "type alias",             "N N N N" }
};

#define METHOD_CAP 0
#define METHOD_RETURN 2
#define METHOD_ERROR 4
#define METHOD_BODY 6

// Index by DEF_<ENTITY> + DEF_<METHOD>
static const permission_def_t _method_def[DEF_METHOD_COUNT] =
{ //                           cap
  //                           | return
  //                           | | error
  //                           | | | body
  { "class function",         "X X X Y" },
  { "actor function",         "X X X Y" },
  { "primitive function",     "X X X Y" },
  { "trait function",         "X X X X" },
  { "interface function",     "X X X X" },
  { "type alias function",    NULL },
  { "class behaviour",        NULL },
  { "actor behaviour",        "N N N Y" },
  { "primitive behaviour",    NULL },
  { "trait behaviour",        "N N N X" },
  { "interface behaviour",    "N N N X" },
  { "type alias behaviour",   NULL },
  { "class constructor",      "X N X Y" },
  { "actor constructor",      "N N N Y" },
  { "primitive constructor",  "N N X Y" },
  { "trait constructor",      "X N X N" },
  { "interface constructor",  "X N X N" },
  { "type alias constructor", NULL }
};


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


// Check whether the given node is a valid provides type
static bool check_provides_type(ast_t* type)
{
  assert(type != NULL);

  switch(ast_id(type))
  {
    case TK_NOMINAL:
    {
      AST_GET_CHILDREN(type, ignore0, ignore1, ignore2, cap, ephemeral);

      if(ast_id(cap) != TK_NONE)
      {
        ast_error(cap, "can't specify a capability in a provides type");
        return false;
      }

      if(ast_id(ephemeral) != TK_NONE)
      {
        ast_error(ephemeral, "can't specify ephemeral in a provides type");
        return false;
      }

      return true;
    }

    case TK_PROVIDES:
    case TK_ISECTTYPE:
      // Check all our children are also legal
      for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
      {
        if(!check_provides_type(p))
          return false;
      }

      return true;

    default:
      ast_error(type, "invalid provides type. Can only provide "
        "interfaces, traits and intersects of those.");
      return false;
  }
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
  bool r = true;

  const permission_def_t* def = &_method_def[method_def_index];

  if(def->permissions == NULL)
  {
    ast_error(ast, "%ss are not allowed", def->desc);
    return false;
  }

  AST_GET_CHILDREN(ast, cap, id, type_params, params, return_type,
    error, body, docstring);

  if(!check_permission(def, METHOD_CAP, cap, "receiver capability", cap))
    r = false;

  if(!check_id_method(id))
    r = false;

  if(!check_permission(def, METHOD_RETURN, return_type, "return type", ast))
    r = false;

  if(!check_permission(def, METHOD_ERROR, error, "?", ast))
    r = false;

  if(!check_permission(def, METHOD_BODY, body, "body", ast))
    r = false;

  if(ast_id(docstring) == TK_STRING)
  {
    if(ast_id(body) != TK_NONE)
    {
      ast_error(docstring,
        "methods with bodies must put docstrings in the body");
      r = false;
    }
  }

  return r;
}


// Check whether the given entity members are legal in their entity
static bool check_members(ast_t* members, int entity_def_index)
{
  assert(members != NULL);
  assert(entity_def_index >= 0 && entity_def_index < DEF_ENTITY_COUNT);
  bool r = true;

  const permission_def_t* def = &_entity_def[entity_def_index];
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FLET:
      case TK_FVAR:
      case TK_EMBED:
        if(def->permissions[ENTITY_FIELD] == 'N')
        {
          ast_error(member, "Can't have fields in %s", def->desc);
          r = false;
        }

        if(!check_id_field(ast_child(member)))
          r = false;
        break;

      case TK_NEW:
        if(!check_method(member, entity_def_index + DEF_NEW))
          r = false;
        break;

      case TK_BE:
      {
        if(!check_method(member, entity_def_index + DEF_BE))
          r = false;
        break;
      }

      case TK_FUN:
      {
        if(!check_method(member, entity_def_index + DEF_FUN))
          r = false;
        break;
      }

      default:
        ast_print(members);
        assert(0);
        return false;
    }

    member = ast_sibling(member);
  }

  return r;
}


// Check whether the given entity has illegal parts
static ast_result_t syntax_entity(ast_t* ast, int entity_def_index)
{
  assert(ast != NULL);
  assert(entity_def_index >= 0 && entity_def_index < DEF_ENTITY_COUNT);
  ast_result_t r = AST_OK;

  const permission_def_t* def = &_entity_def[entity_def_index];
  AST_GET_CHILDREN(ast, id, typeparams, defcap, provides, members, c_api);

  // Check if we're called Main
  if(def->permissions[ENTITY_MAIN] == 'N' && ast_name(id) == stringtab("Main"))
  {
    ast_error(ast, "Main must be an actor");
    r = AST_ERROR;
  }

  if(!check_id_type(id, def->desc))
    r = AST_ERROR;

  if(!check_permission(def, ENTITY_CAP, defcap, "default capability", defcap))
    r = AST_ERROR;

  if(!check_permission(def, ENTITY_C_API, c_api, "C api", c_api))
    r = AST_ERROR;

  if(ast_id(c_api) == TK_AT)
  {
    if(ast_id(typeparams) != TK_NONE)
    {
      ast_error(typeparams, "generic actor cannot specify C api");
      r = AST_ERROR;
    }
  }

  if(entity_def_index != DEF_TYPEALIAS)
  {
    // Check referenced traits
    if(ast_id(provides) != TK_NONE && !check_provides_type(provides))
      r = AST_ERROR;
  }
  else
  {
    // Check for a type alias
    if(ast_id(provides) == TK_NONE)
    {
      ast_error(provides, "a type alias must specify a type");
      r = AST_ERROR;
    }
  }

  // Check for illegal members
  if(!check_members(members, entity_def_index))
    r = AST_ERROR;

  return r;
}


static ast_result_t syntax_thistype(typecheck_t* t, ast_t* ast)
{
  assert(ast != NULL);
  ast_t* parent = ast_parent(ast);
  assert(parent != NULL);
  ast_result_t r = AST_OK;

  if(ast_id(parent) != TK_ARROW)
  {
    ast_error(ast, "in a type, 'this' can only be used as a viewpoint");
    r = AST_ERROR;
  }

  if(t->frame->method == NULL)
  {
    ast_error(ast, "can only use 'this' for a viewpoint in a method");
    r = AST_ERROR;
  }

  return r;
}


static ast_result_t syntax_arrowtype(ast_t* ast)
{
  assert(ast != NULL);

  ast_t* rhs = ast_childidx(ast, 1);

  if(ast_child(rhs) == NULL && ast_id(rhs) == TK_THISTYPE)
  {
    ast_error(ast, "'this' cannot appear to the right of a viewpoint");
    return AST_ERROR;
  }

  if(ast_child(rhs) == NULL && ast_id(rhs) == TK_BOXTYPE)
  {
    ast_error(ast, "'box' cannot appear to the right of a viewpoint");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t syntax_match(ast_t* ast)
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


static ast_result_t syntax_ffi(ast_t* ast, bool return_optional)
{
  assert(ast != NULL);
  AST_GET_CHILDREN(ast, id, typeargs, args, named_args);
  ast_result_t r = AST_OK;

  // We don't check FFI names are legal, if the lexer allows it so do we

  if((ast_child(typeargs) == NULL && !return_optional) ||
    ast_childidx(typeargs, 1) != NULL)
  {
    ast_error(typeargs, "FFIs must specify a single return type");
    r = AST_ERROR;
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
        r = AST_ERROR;
      }
    }
  }

  if(ast_id(named_args) != TK_NONE)
  {
    ast_error(typeargs, "FFIs cannot take named arguments");
    r = AST_ERROR;
  }

  return r;
}


static ast_result_t syntax_ellipsis(ast_t* ast)
{
  assert(ast != NULL);
  ast_result_t r = AST_OK;

  ast_t* fn = ast_parent(ast_parent(ast));
  assert(fn != NULL);

  if(ast_id(fn) != TK_FFIDECL)
  {
    ast_error(ast, "... may only appear in FFI declarations");
    r = AST_ERROR;
  }

  if(ast_sibling(ast) != NULL)
  {
    ast_error(ast, "... must be the last parameter");
    r = AST_ERROR;
  }

  return r;
}


static ast_result_t syntax_infix_expr(ast_t* ast)
{
  assert(ast != NULL);
  AST_GET_CHILDREN(ast, left, right);

  token_id op = ast_id(ast);

  assert(left != NULL);
  token_id left_op = ast_id(left);
  bool left_clash = (left_op != op) && is_expr_infix(left_op) &&
    !ast_checkflag(left, AST_FLAG_IN_PARENS);

  assert(right != NULL);
  token_id right_op = ast_id(right);
  bool right_clash = (right_op != op) && is_expr_infix(right_op) &&
    !ast_checkflag(right, AST_FLAG_IN_PARENS);

  if(left_clash || right_clash)
  {
    ast_error(ast,
      "Operator precedence is not supported. Parentheses required.");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t syntax_consume(ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, term);

  switch(ast_id(term))
  {
    case TK_THIS:
    case TK_REFERENCE:
      return AST_OK;

    default: {}
  }

  ast_error(term, "Consume expressions must specify a single identifier");
  return AST_ERROR;
}


static ast_result_t syntax_return(pass_opt_t* options, ast_t* ast,
  size_t max_value_count)
{
  assert(ast != NULL);

  ast_t* value_seq = ast_child(ast);
  assert(ast_id(value_seq) == TK_SEQ || ast_id(value_seq) == TK_NONE);
  size_t value_count = ast_childcount(value_seq);

  if(value_count > max_value_count)
  {
    ast_error(ast_childidx(value_seq, max_value_count), "Unreachable code");
    return AST_ERROR;
  }

  if(ast_id(ast) == TK_RETURN)
  {
    if(options->check.frame->method_body == NULL)
    {
      ast_error(ast, "return must occur in a method body");
      return AST_ERROR;
    }

    if(value_count > 0)
    {
      if(ast_id(options->check.frame->method) == TK_NEW)
      {
        ast_error(ast,
          "A return in a constructor must not have an expression");
        return AST_ERROR;
      }

      if(ast_id(options->check.frame->method) == TK_BE)
      {
        ast_error(ast,
          "A return in a behaviour must not have an expression");
        return AST_ERROR;
      }
    }
  }

  return AST_OK;
}


static ast_result_t syntax_semi(ast_t* ast)
{
  assert(ast_parent(ast) != NULL);
  assert(ast_id(ast_parent(ast)) == TK_SEQ);

  if(ast_checkflag(ast, AST_FLAG_BAD_SEMI))
  {
    ast_error(ast, "Unexpected semi colon, only use to separate expressions on"
      " the same line");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t syntax_local(ast_t* ast)
{
  if(!check_id_local(ast_child(ast)))
    return AST_ERROR;

  return AST_OK;
}


static ast_result_t syntax_embed(ast_t* ast)
{
  if(ast_id(ast_parent(ast)) != TK_MEMBERS)
  {
    ast_error(ast, "Local variables cannot be embedded");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t syntax_param(ast_t* ast)
{
  if(!check_id_param(ast_child(ast)))
    return AST_ERROR;

  return AST_OK;
}


static ast_result_t syntax_type_param(ast_t* ast)
{
  if(!check_id_type_param(ast_child(ast)))
    return AST_ERROR;

  return AST_OK;
}


static ast_result_t syntax_use(ast_t* ast)
{
  ast_t* id = ast_child(ast);

  if(ast_id(id) != TK_NONE && !check_id_package(id))
    return AST_ERROR;

  return AST_OK;
}


static ast_result_t syntax_lambda_capture(ast_t* ast)
{
  AST_GET_CHILDREN(ast, name, type, value);

  if(ast_id(type) != TK_NONE && ast_id(value) == TK_NONE)
  {
    ast_error(ast, "value missing for lambda expression capture (cannot "
      "specify type without value)");
    return AST_ERROR;
  }

  return AST_OK;
}


ast_result_t pass_syntax(ast_t** astp, pass_opt_t* options)
{
  typecheck_t* t = &options->check;

  assert(astp != NULL);
  ast_t* ast = *astp;
  assert(ast != NULL);

  token_id id = ast_id(ast);

  // These node all use the data field as pointers to stuff
  if(id == TK_PROGRAM || id == TK_PACKAGE || id == TK_MODULE)
    return AST_OK;

  if(ast_checkflag(ast, AST_FLAG_TEST_ONLY))
  {
    // Test node, not allowed outside parse pass
    ast_error(ast, "Illegal character '$' found");
    return AST_ERROR;
  }

  ast_result_t r = AST_OK;

  switch(id)
  {
    case TK_SEMI:       r = syntax_semi(ast); break;
    case TK_TYPE:       r = syntax_entity(ast, DEF_TYPEALIAS); break;
    case TK_PRIMITIVE:  r = syntax_entity(ast, DEF_PRIMITIVE); break;
    case TK_CLASS:      r = syntax_entity(ast, DEF_CLASS); break;
    case TK_ACTOR:      r = syntax_entity(ast, DEF_ACTOR); break;
    case TK_TRAIT:      r = syntax_entity(ast, DEF_TRAIT); break;
    case TK_INTERFACE:  r = syntax_entity(ast, DEF_INTERFACE); break;
    case TK_THISTYPE:   r = syntax_thistype(t, ast); break;
    case TK_ARROW:      r = syntax_arrowtype(ast); break;
    case TK_MATCH:      r = syntax_match(ast); break;
    case TK_FFIDECL:    r = syntax_ffi(ast, false); break;
    case TK_FFICALL:    r = syntax_ffi(ast, true); break;
    case TK_ELLIPSIS:   r = syntax_ellipsis(ast); break;
    case TK_CONSUME:    r = syntax_consume(ast); break;
    case TK_RETURN:
    case TK_BREAK:      r = syntax_return(options, ast, 1); break;
    case TK_CONTINUE:
    case TK_ERROR:      r = syntax_return(options, ast, 0); break;
    case TK_LET:
    case TK_VAR:        r = syntax_local(ast); break;
    case TK_EMBED:      r = syntax_embed(ast); break;
    case TK_PARAM:      r = syntax_param(ast); break;
    case TK_TYPEPARAM:  r = syntax_type_param(ast); break;
    case TK_USE:        r = syntax_use(ast); break;
    case TK_LAMBDACAPTURE:
                        r = syntax_lambda_capture(ast); break;
    default: break;
  }

  if(is_expr_infix(id))
    r = syntax_infix_expr(ast);

  if(ast_checkflag(ast, AST_FLAG_MISSING_SEMI))
  {
    ast_error(ast,
      "Use a semi colon to separate expressions on the same line");
    r = AST_ERROR;
  }

  return r;
}
