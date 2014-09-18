#include "parsefix.h"
#include "../ast/token.h"
#include "../pkg/package.h"
#include "../type/assemble.h"
#include "../ds/stringtab.h"
#include <assert.h>


#define DEF_CLASS 0
#define DEF_ACTOR 1
#define DEF_PRIMITIVE 2
#define DEF_TRAIT 3
#define DEF_STRUCT 4
#define DEF_ENTITY_COUNT 5

#define DEF_FUN 0
#define DEF_BE 5
#define DEF_NEW 10
#define DEF_METHOD_COUNT 15


typedef enum tribool_t
{
  tb_no,
  tb_opt,
  tb_yes
} tribool_t;

typedef struct entity_def_t
{
  const char* desc;
  bool main_allowed;
  bool field_allowed;
  tribool_t cap;
  bool field;
} entity_def_t;

// Index by DEF_<ENTITY>
static const entity_def_t _entity_def[DEF_ENTITY_COUNT] =
{ //                   Main   field  cap
  { "class",           false, true,  tb_opt },
  { "actor",           true,  true,  tb_no  },
  { "data type",       false, false, tb_no  },
  { "trait",           false, false, tb_opt },
  { "structural type", false, false, tb_opt }
};


typedef struct method_def_t
{
  const char* desc;
  bool allowed;
  tribool_t cap;
  tribool_t name;
  tribool_t return_type;
  tribool_t error;
  tribool_t body;
} method_def_t;

// Index by DEF_<ENTITY> + DEF_<METHOD>
static const method_def_t _method_def[DEF_METHOD_COUNT] =
{ //                         allowed cap     name    return  error   body
  { "class function",         true,  tb_yes, tb_opt, tb_opt, tb_opt, tb_yes },
  { "actor function",         true,  tb_yes, tb_opt, tb_opt, tb_opt, tb_yes },
  { "data type function",     true,  tb_yes, tb_opt, tb_opt, tb_opt, tb_yes },
  { "trait function",         true,  tb_yes, tb_opt, tb_opt, tb_opt, tb_opt },
  { "structural function",    true,  tb_yes, tb_opt, tb_opt, tb_opt, tb_no  },
  { "class behaviour",        false },
  { "actor behaviour",        true,  tb_no,  tb_yes, tb_no,  tb_no,  tb_yes },
  { "data type behaviour",    false },
  { "trait behaviour",        true,  tb_no,  tb_yes, tb_no,  tb_no,  tb_opt },
  { "structural behaviour",   true,  tb_no,  tb_opt, tb_no,  tb_no,  tb_no  },
  { "class constructor",      true,  tb_no,  tb_opt, tb_no,  tb_opt, tb_opt },
  { "actor constructor",      true,  tb_no,  tb_opt, tb_no,  tb_no,  tb_opt },
  { "data type constructor",  false },
  { "trait constructor",      false },
  { "structural constructor", false }
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


// Check one specific part of a method or entity
static bool check_tribool(tribool_t allowed, ast_t* actual,
  const char* context, const char* part_desc)
{
  assert(actual != NULL);
  assert(context != NULL);
  assert(part_desc != NULL);

  if(allowed == tb_no && ast_id(actual) != TK_NONE)
  {
    ast_error(actual, "%s cannot specify %s", context, part_desc);
    return false;
  }

  if(allowed == tb_yes && ast_id(actual) == TK_NONE)
  {
    ast_error(actual, "%s must specify %s", context, part_desc);
    return false;
  }

  return true;
}


// Check whether the given method has any illegal parts
static bool check_method(ast_t* ast, int method_def_index)
{
  assert(ast != NULL);
  assert(method_def_index >= 0 && method_def_index < DEF_METHOD_COUNT);

  const method_def_t* def = &_method_def[method_def_index];

  if(!def->allowed)
  {
    ast_error(ast, "%ss are not allowed", def->desc);
    return false;
  }

  AST_GET_CHILDREN(ast, cap, id, ignore0, ignore1, return_type, error, arrow,
    body);

  // Remove the arrow node
  token_id arrow_id = ast_id(arrow);
  ast_remove(arrow, error);

  if(!check_tribool(def->cap, cap, def->desc, "receiver capability"))
    return false;

  if(ast_id(cap) == TK_ISO || ast_id(cap) == TK_TRN)
  {
    ast_error(cap, "receiver capability must not be iso or trn");
    return false;
  }

  if(!check_tribool(def->name, id, def->desc, "name"))
    return false;

  if(!check_tribool(def->return_type, return_type, def->desc, "return type"))
    return false;

  if(!check_tribool(def->error, error, def->desc, "?"))
    return false;

  if(!check_tribool(def->body, body, def->desc, "body"))
    return false;

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


// Check whether the given entity members are legal in their entity
static bool check_members(ast_t* members, int entity_def_index)
{
  assert(members != NULL);
  assert(entity_def_index >= 0 && entity_def_index < DEF_ENTITY_COUNT);

  const entity_def_t* def = &_entity_def[entity_def_index];
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FLET:
      case TK_FVAR:
        if(!def->field_allowed)
        {
          ast_error(member, "Can't have fields in %s", def->desc);
          return false;
        }
        break;

      case TK_NEW:
        if(!check_method(member, entity_def_index + DEF_NEW))
          return false;
        break;

      case TK_BE:
        if(!check_method(member, entity_def_index + DEF_BE))
          return false;
        break;

      case TK_FUN:
        if(!check_method(member, entity_def_index + DEF_FUN))
          return false;
        break;

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

  const entity_def_t* def = &_entity_def[entity_def_index];
  AST_GET_CHILDREN(ast, id, typeparams, defcap, provides, members);

  // Check if we're called Main
  if(!def->main_allowed && ast_name(id) == stringtab("Main"))
  {
    ast_error(ast, "Main must be an actor");
    return AST_ERROR;
  }

  if(!check_tribool(def->cap, defcap, def->desc, "default capability"))
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


static ast_result_t parse_fix_structural(ast_t* ast)
{
  assert(ast != NULL);

  const entity_def_t* def = &_entity_def[DEF_STRUCT];
  AST_GET_CHILDREN(ast, members, defcap);

  if(!check_tribool(def->cap, defcap, def->desc, "default capability"))
    return AST_ERROR;

  // Check for illegal members
  if(!check_members(members, DEF_STRUCT))
    return AST_ERROR;

  return AST_OK;
}


// TODO(andy): This should be moved to a later pass. It's pointless checking
// the types of the left and right children until type aliases have been
// resolved
/*
static bool check_arrow_left(ast_t* ast)
{
  assert(ast != NULL);

  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
      ast_error(ast, "can't use a type expression as a viewpoint");
      return false;

    case TK_NOMINAL:
    {
      AST_GET_CHILDREN(ast, ignore0, ignore1, ignore2, cap, ephemeral);

      if(!check_tribool(tb_no, cap, "viewpoint", "capability"))
        return false;

      if(!check_tribool(tb_no, ephemeral, "viewpoint", "ephemeral type"))
        return false;

      return true;
    }

    case TK_STRUCTURAL:
      ast_error(ast, "can't use a structural type as a viewpoint");
      return false;

    case TK_THISTYPE:
      return true;

    default:
      assert(0);
      return false;
  }
}


static bool check_arrow_right(ast_t* ast)
{
  assert(ast != NULL);

  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
      ast_error(ast, "can't use a type expression in a viewpoint");
      return false;

    case TK_NOMINAL:
    case TK_STRUCTURAL:
    case TK_ARROW:
      return true;

    case TK_THISTYPE:
      ast_error(ast, "can't use 'this' in a viewpoint");
      return false;

    default:
      assert(0);
      return false;
  }
}


static ast_result_t parse_fix_arrow(ast_t* ast)
{
  assert(ast != NULL);
  AST_GET_CHILDREN(ast, left, right);

  if(!check_arrow_left(left) || !check_arrow_right(right))
    return AST_ERROR;

  return AST_OK;
}
*/


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

  // TODO(andy): This allows some illegal cases through, eg ephemeral argument
  // of a function within a structural, where that whole structural is a return
  // type of a function
  if(ast_enclosing_method_type(ast) == NULL)
  {
    ast_error(ast,
      "ephemeral types can only appear in function return types");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t parse_fix_bang(ast_t* ast)
{
  // TODO: syntactic sugar for partial application
  /*
  a!method(b, c)

  {
    var $0: Receiver method_cap = a
    var $1: Param1 = b
    var $2: Param2 = c

    fun cap apply(remaining args on method): method_result =>
      $0.method($1, $2, remaining args on method)
  } cap ^

  cap
    never tag (need to read our receiver)
    never iso or trn (but can recover)
    val: ParamX val or tag, method_cap val or tag
    box: val <: ParamX, val <: method_cap
    ref: otherwise
  */
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

  ast_t* body = ast_childidx(case_ast, 3);

  if(ast_id(body) == TK_NONE)
  {
    ast_error(case_ast, "Last case in match must have a body");
    return AST_ERROR;
  }

  return AST_OK;
}


static ast_result_t parse_fix_ffi(ast_t* ast)
{
  assert(ast != NULL);
  AST_GET_CHILDREN(ast, id, typeargs, args);

  if(ast_child(typeargs) == NULL || ast_childidx(typeargs, 1) != NULL)
  {
    ast_error(typeargs, "FFI calls must specify a single return type");
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
  return pass_parse_fix(astp);
}


ast_result_t pass_parse_fix(ast_t** astp)
{
  assert(astp != NULL);
  ast_t* ast = *astp;
  assert(ast != NULL);

  switch(ast_id(ast))
  {
    case TK_TYPE:       return parse_fix_type_alias(ast);
    case TK_PRIMITIVE:  return parse_fix_entity(ast, DEF_PRIMITIVE);
    case TK_CLASS:      return parse_fix_entity(ast, DEF_CLASS);
    case TK_ACTOR:      return parse_fix_entity(ast, DEF_ACTOR);
    case TK_TRAIT:      return parse_fix_entity(ast, DEF_TRAIT);
    case TK_STRUCTURAL: return parse_fix_structural(ast);
    case TK_THISTYPE:   return parse_fix_thistype(ast);
    case TK_HAT:        return parse_fix_ephemeral(ast);
    case TK_BANG:       return parse_fix_bang(ast);
    case TK_MATCH:      return parse_fix_match(ast);
    case TK_AT:         return parse_fix_ffi(ast);
    case TK_CONSUME:    return parse_fix_consume(ast);
    case TK_LPAREN:
    case TK_LPAREN_NEW: return parse_fix_lparen(astp);
    default: break;
  }

  if(is_expr_infix(ast_id(ast)))
    return parse_fix_infix_expr(ast);

  return AST_OK;
}
