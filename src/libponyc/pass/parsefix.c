#include "parsefix.h"
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
#define METHOD_NAME 2
#define METHOD_RETURN 4
#define METHOD_ERROR 6
#define METHOD_BODY 8

// Index by DEF_<ENTITY> + DEF_<METHOD>
static const permission_def_t _method_def[DEF_METHOD_COUNT] =
{ //                           cap
  //                           | name
  //                           | | return
  //                           | | | error
  //                           | | | | body
  { "class function",         "X Y X X Y" },
  { "actor function",         "X Y X X Y" },
  { "primitive function",     "X Y X X Y" },
  { "trait function",         "X Y X X X" },
  { "interface function",     "X Y X X X" },
  { "type alias function",    NULL },
  { "class behaviour",        NULL },
  { "actor behaviour",        "N Y N N Y" },
  { "primitive behaviour",    NULL },
  { "trait behaviour",        "N Y N N X" },
  { "interface behaviour",    "N Y N N X" },
  { "type alias behaviour",   NULL },
  { "class constructor",      "X Y N X Y" },
  { "actor constructor",      "N Y N N Y" },
  { "primitive constructor",  "N Y N X Y" },
  { "trait constructor",      "X Y N X N" },
  { "interface constructor",  "X Y N X N" },
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

  if(def->permissions == NULL)
  {
    ast_error(ast, "%ss are not allowed", def->desc);
    return false;
  }

  AST_GET_CHILDREN(ast, cap, id, type_params, params, return_type,
    error, body, docstring);

  if(!check_permission(def, METHOD_CAP, cap, "receiver capability", cap))
    return false;

  if(!check_permission(def, METHOD_NAME, id, "name", id))
    return false;

  if(!check_permission(def, METHOD_RETURN, return_type, "return type", ast))
    return false;

  if(!check_permission(def, METHOD_ERROR, error, "?", ast))
    return false;

  if(!check_permission(def, METHOD_BODY, body, "body", ast))
    return false;

  if(!check_id_method(id))
    return false;

  if(ast_id(docstring) == TK_STRING)
  {
    if(ast_id(body) != TK_NONE)
    {
      ast_error(docstring,
        "methods with bodies must put docstrings in the body");
      return false;
    }
  } else {
    ast_t* first = ast_child(body);

    // TODO: Move this to sugar pass
    if((first != NULL) &&
      (ast_id(first) == TK_STRING) &&
      (ast_sibling(first) != NULL)
      )
    {
      ast_pop(body);
      ast_replace(&docstring, first);
    }
  }

  return true;
}


// Check whether the given entity members are legal in their entity
static bool check_members(ast_t* members, int entity_def_index)
{
  assert(members != NULL);
  assert(entity_def_index >= 0 && entity_def_index < DEF_ENTITY_COUNT);

  const permission_def_t* def = &_entity_def[entity_def_index];
  ast_t* member = ast_child(members);

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

        if(!check_id_field(ast_child(member)))
          return false;

        break;

      case TK_NEW:
        if(!check_method(member, entity_def_index + DEF_NEW))
          return false;
        break;

      case TK_BE:
      {
        if(!check_method(member, entity_def_index + DEF_BE))
          return false;
        break;
      }

      case TK_FUN:
      {
        if(!check_method(member, entity_def_index + DEF_FUN))
          return false;
        break;
      }

      default:
        ast_print(members);
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
  AST_GET_CHILDREN(ast, id, typeparams, defcap, provides, members, c_api);

  // Check if we're called Main
  if(def->permissions[ENTITY_MAIN] == 'N' && ast_name(id) == stringtab("Main"))
  {
    ast_error(ast, "Main must be an actor");
    return AST_ERROR;
  }

  if(!check_id_type(id, def->desc))
    return AST_ERROR;

  if(!check_permission(def, ENTITY_CAP, defcap, "default capability", defcap))
    return AST_ERROR;

  if(!check_permission(def, ENTITY_C_API, c_api, "C api", c_api))
    return AST_ERROR;

  if((ast_id(c_api) == TK_AT) && (ast_id(typeparams) != TK_NONE))
  {
    ast_error(c_api, "generic actor cannot specify C api");
    return AST_ERROR;
  }

  if(entity_def_index != DEF_TYPEALIAS)
  {
    // Check referenced traits
    if(!check_traits(provides))
      return AST_ERROR;
  }
  else
  {
    // Check for a single type alias
    if(ast_childcount(provides) != 1)
    {
      ast_error(provides, "a type alias must specify a single type");
      return AST_ERROR;
    }
  }

  // Check for illegal members
  if(!check_members(members, entity_def_index))
    return AST_ERROR;

  return AST_OK;
}


static ast_result_t parse_fix_thistype(typecheck_t* t, ast_t* ast)
{
  assert(ast != NULL);
  ast_t* parent = ast_parent(ast);
  assert(parent != NULL);

  if(ast_id(parent) != TK_ARROW)
  {
    ast_error(ast, "in a type, 'this' can only be used as a viewpoint");
    return AST_ERROR;
  }

  if(t->frame->method == NULL)
  {
    ast_error(ast, "can only use 'this' for a viewpoint in a method");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t parse_fix_ephemeral(typecheck_t* t, ast_t* ast)
{
  assert(ast != NULL);

  if((t->frame->method_type == NULL) &&
    (t->frame->ffi_type == NULL) &&
    (t->frame->as_type == NULL))
  {
    ast_error(ast,
      "ephemeral types can only appear in 'as' expression types and function "
      "return types");

    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t parse_fix_borrowed(typecheck_t* t, ast_t* ast)
{
  assert(ast != NULL);

  if(ast_id(ast_parent(ast)) != TK_NOMINAL)
    return AST_OK;

  if(t->frame->local_type == NULL)
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
  AST_GET_CHILDREN(ast, id, typeargs, args, named_args);

  if(ast_id(id) == TK_ID && !check_id_ffi(id))
    return AST_ERROR;

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


static ast_result_t parse_fix_infix_expr(ast_t* ast)
{
  assert(ast != NULL);
  AST_GET_CHILDREN(ast, left, right);

  token_id op = ast_id(ast);

  assert(left != NULL);
  token_id left_op = ast_id(left);
  bool left_clash = (left_op != op) && is_expr_infix(left_op) &&
    ((AST_IN_PARENS & (uint64_t)ast_data(left)) == 0);

  assert(right != NULL);
  token_id right_op = ast_id(right);
  bool right_clash = (right_op != op) && is_expr_infix(right_op) &&
    ((AST_IN_PARENS & (uint64_t)ast_data(right)) == 0);

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
  AST_GET_CHILDREN(ast, cap, term);

  if(ast_id(term) != TK_REFERENCE)
  {
    ast_error(term, "Consume expressions must specify a single identifier");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t parse_fix_return(ast_t* ast, size_t max_value_count)
{
  assert(ast != NULL);

  ast_t* value_seq = ast_child(ast);

  size_t value_count = 0;

  if(value_seq != NULL)
  {
    assert(ast_id(value_seq) == TK_SEQ || ast_id(value_seq) == TK_NONE);
    value_count = ast_childcount(value_seq);
  }

  if(value_count > max_value_count)
  {
    ast_error(ast_childidx(value_seq, max_value_count), "Unreachable code");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t parse_fix_semi(ast_t* ast)
{
  assert(ast_parent(ast) != NULL);
  assert(ast_id(ast_parent(ast)) == TK_SEQ);

  bool any_newlines = ast_is_first_on_line(ast);  // Newline before ;
  bool last_in_seq = (ast_sibling(ast) == NULL);

  if((LAST_ON_LINE & (uint64_t)ast_data(ast)) != 0) // Newline after ;
    any_newlines = true;

  if(any_newlines || last_in_seq)
  {
    // Unnecessary ;
    ast_error(ast, "Unexpected semi colon, only use to separate expressions on"
      " the same line");
    return AST_FATAL;
  }

  return AST_OK;
}


static ast_result_t parse_fix_local(ast_t* ast)
{
  if(!check_id_local(ast_child(ast)))
    return AST_ERROR;

  return AST_OK;
}


static ast_result_t parse_fix_param(ast_t* ast)
{
  if(!check_id_param(ast_child(ast)))
    return AST_ERROR;

  return AST_OK;
}


static ast_result_t parse_fix_type_param(ast_t* ast)
{
  if(!check_id_type_param(ast_child(ast)))
    return AST_ERROR;

  return AST_OK;
}


static ast_result_t parse_fix_use(ast_t* ast)
{
  ast_t* id = ast_child(ast);

  if(ast_id(id) != TK_NONE && !check_id_package(id))
    return AST_ERROR;

  return AST_OK;
}


ast_result_t pass_parse_fix(ast_t** astp, pass_opt_t* options)
{
  typecheck_t* t = &options->check;

  assert(astp != NULL);
  ast_t* ast = *astp;
  assert(ast != NULL);

  token_id id = ast_id(ast);

  if(id == TK_PROGRAM || id == TK_PACKAGE || id == TK_MODULE)
    // These node all use the data field as pointers to stuff
      return AST_OK;

  if((TEST_ONLY & (uint64_t)ast_data(ast)) != 0)
  {
    // Test node, not allowed outside parse pass
    ast_error(ast, "Illegal character '$' found");
    return AST_FATAL;
  }

  if((MISSING_SEMI & (uint64_t)ast_data(ast)) != 0)
  {
    ast_error(ast, "Use a semi colon to separate expressions on the same line");
    return AST_FATAL;
  }

  ast_result_t r = AST_OK;

  switch(id)
  {
    case TK_SEMI:       r = parse_fix_semi(ast); break;
    case TK_TYPE:       r = parse_fix_entity(ast, DEF_TYPEALIAS); break;
    case TK_PRIMITIVE:  r = parse_fix_entity(ast, DEF_PRIMITIVE); break;
    case TK_CLASS:      r = parse_fix_entity(ast, DEF_CLASS); break;
    case TK_ACTOR:      r = parse_fix_entity(ast, DEF_ACTOR); break;
    case TK_TRAIT:      r = parse_fix_entity(ast, DEF_TRAIT); break;
    case TK_INTERFACE:  r = parse_fix_entity(ast, DEF_INTERFACE); break;
    case TK_THISTYPE:   r = parse_fix_thistype(t, ast); break;
    case TK_EPHEMERAL:  r = parse_fix_ephemeral(t, ast); break;
    case TK_BORROWED:   r = parse_fix_borrowed(t, ast); break;
    case TK_MATCH:      r = parse_fix_match(ast); break;
    case TK_FFIDECL:    r = parse_fix_ffi(ast, false); break;
    case TK_FFICALL:    r = parse_fix_ffi(ast, true); break;
    case TK_ELLIPSIS:   r = parse_fix_ellipsis(ast); break;
    case TK_CONSUME:    r = parse_fix_consume(ast); break;
    case TK_RETURN:
    case TK_BREAK:      r = parse_fix_return(ast, 1); break;
    case TK_CONTINUE:
    case TK_ERROR:      r = parse_fix_return(ast, 0); break;
    case TK_IDSEQ:      r = parse_fix_local(ast); break;
    case TK_PARAM:      r = parse_fix_param(ast); break;
    case TK_TYPEPARAM:  r = parse_fix_type_param(ast); break;
    case TK_USE:        r = parse_fix_use(ast); break;
    default: break;
  }

  if(is_expr_infix(id))
    r = parse_fix_infix_expr(ast);

  // Clear parse info flags
  ast_setdata(ast, 0);
  return r;
}
