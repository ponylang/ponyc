#include "sugar.h"
#include "../ast/token.h"
#include "../pkg/package.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include "../ast/stringtab.h"
#include <assert.h>


static ast_t* make_create(ast_t* ast)
{
  BUILD(create, ast,
    NODE(TK_NEW, AST_SCOPE
    NONE          // cap
    ID("create")  // name
    NONE          // typeparams
    NONE          // params
    NONE          // return type
    NONE          // error
    NODE(TK_SEQ, NODE(TK_TRUE))
    NODE(TK_DBLARROW)));

  return create;
}


static bool has_member(ast_t* members, const char* name)
{
  name = stringtab(name);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    ast_t* id;

    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
        id = ast_child(member);
        break;

      default:
        id = ast_childidx(member, 1);
        break;
    }

    // Note, members may not have names yet
    if(ast_id(id) != TK_NONE && ast_name(id) == name)
      return true;

    member = ast_sibling(member);
  }

  return false;
}


static void add_default_constructor(ast_t* members)
{
  // If we have no uninitialised fields and no "create" member, add a "create"
  // constructor
  if(has_member(members, "create"))
    return;

  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      {
        ast_t* init = ast_childidx(member, 2);

        if(ast_id(init) == TK_NONE)
          return;

        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  ast_append(members, make_create(members));
}


static ast_t* make_nominal(ast_t* id, ast_t* typeargs)
{
  BUILD(type, id,
    NODE(TK_NOMINAL,
      NONE
      TREE(id)
      TREE(typeargs)
      NONE
      NONE));

  return type;
}


static void add_comparable(ast_t* id, ast_t* typeparams, ast_t* members)
{
  BUILD(typeargs, typeparams, NODE(TK_TYPEARGS));

  ast_t* typeparam = ast_child(typeparams);

  while(typeparam != NULL)
  {
    ast_t* p_id = ast_child(typeparam);
    ast_t* type = make_nominal(p_id, ast_from(id, TK_NONE));
    ast_append(typeargs, type);

    typeparam = ast_sibling(typeparam);
  }

  if(ast_child(typeargs) == NULL)
    ast_setid(typeargs, TK_NONE);

  ast_t* type = make_nominal(id, typeargs);

  if(!has_member(members, "eq"))
  {
    BUILD(eq, members,
      NODE(TK_FUN, AST_SCOPE
        NODE(TK_BOX)
        ID("eq")
        NONE
        NODE(TK_PARAMS,
          NODE(TK_PARAM,
            ID("that")
            TREE(type)
            NONE))
        NODE(TK_NOMINAL,
          NONE
          ID("Bool")
          NONE
          NONE
          NONE)
        NONE
        NODE(TK_SEQ,
          NODE(TK_IS,
            NODE(TK_THIS)
            NODE(TK_REFERENCE, ID("that"))))
        NODE(TK_DBLARROW)));

    ast_append(members, eq);
  }

  if(!has_member(members, "ne"))
  {
    BUILD(eq, members,
      NODE(TK_FUN, AST_SCOPE
        NODE(TK_BOX)
        ID("ne")
        NONE
        NODE(TK_PARAMS,
          NODE(TK_PARAM,
            ID("that")
            TREE(type)
            NONE))
        NODE(TK_NOMINAL,
          NONE
          ID("Bool")
          NONE
          NONE
          NONE)
        NONE
        NODE(TK_SEQ,
          NODE(TK_ISNT,
            NODE(TK_THIS)
            NODE(TK_REFERENCE, ID("that"))))
        NODE(TK_DBLARROW)));

    ast_append(members, eq);
  }

  ast_free_unattached(type);
}


static ast_result_t sugar_member(ast_t* ast, bool add_create, bool add_eq,
  token_id def_def_cap)
{
  AST_GET_CHILDREN(ast, id, typeparams, defcap, traits, members);

  if(add_create)
    add_default_constructor(members);

  if(ast_id(defcap) == TK_NONE)
    ast_setid(defcap, def_def_cap);

  if(add_eq)
    add_comparable(id, typeparams, members);

  return AST_OK;
}


static ast_result_t sugar_typeparam(ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, constraint);

  if(ast_id(constraint) == TK_NONE)
  {
    REPLACE(&constraint,
      NODE(TK_NOMINAL,
        NONE
        TREE(id)
        NONE
        NONE
        NONE));
  }

  return AST_OK;
}


static ast_result_t sugar_new(typecheck_t* t, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result);

  // Return type is This ref^ for classes, This val^ for primitives, and
  // This tag^ for actors.
  assert(ast_id(result) == TK_NONE);
  token_id tcap;

  switch(ast_id(t->frame->type))
  {
    case TK_PRIMITIVE: tcap = TK_VAL; break;
    case TK_ACTOR: tcap = TK_TAG; break;
    default: tcap = TK_REF; break;
  }

  ast_replace(&result, type_for_this(t, ast, tcap, TK_EPHEMERAL));
  return AST_OK;
}


static ast_result_t sugar_be(typecheck_t* t, ast_t* ast)
{
  // Return type is This tag
  ast_t* result = ast_childidx(ast, 4);
  assert(ast_id(result) == TK_NONE);

  ast_replace(&result, type_for_this(t, ast, TK_TAG, TK_NONE));
  return AST_OK;
}


static ast_result_t sugar_fun(ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body);

  // Return value is not specified, set it to None
  if(ast_id(result) == TK_NONE)
  {
    ast_t* type = type_sugar(ast, NULL, "None");
    ast_replace(&result, type);
  }

  // If the return type is None, add a None at the end of the body.
  if(is_none(result) && (ast_id(body) != TK_NONE))
  {
    BUILD(ref, body, NODE(TK_REFERENCE, ID("None")));
    ast_append(body, ref);
  }

  return AST_OK;
}


// If the given tree is a TK_NONE expand it to a source None
static void expand_none(ast_t* ast)
{
  if(ast_id(ast) != TK_NONE)
    return;

  ast_setid(ast, TK_SEQ);
  ast_scope(ast);
  BUILD(ref, ast, NODE(TK_REFERENCE, ID("None")));
  ast_add(ast, ref);
}


static ast_result_t sugar_return(ast_t* ast)
{
  ast_t* return_value = ast_child(ast);
  expand_none(return_value);
  return AST_OK;
}


static ast_result_t sugar_else(ast_t* ast)
{
  ast_t* else_clause = ast_childidx(ast, 2);
  expand_none(else_clause);
  return AST_OK;
}


static ast_result_t sugar_try(ast_t* ast)
{
  AST_GET_CHILDREN(ast, ignore, else_clause, then_clause);

  expand_none(else_clause);
  expand_none(then_clause);

  return AST_OK;
}


static ast_result_t sugar_for(typecheck_t* t, ast_t** astp)
{
  AST_EXTRACT_CHILDREN(*astp, for_idseq, for_type, for_iter, for_body,
    for_else);

  expand_none(for_else);
  const char* iter_name = package_hygienic_id(t);

  REPLACE(astp,
    NODE(TK_SEQ,
      NODE(TK_ASSIGN,
        TREE(for_iter)
        NODE(TK_LET, NODE(TK_IDSEQ, ID(iter_name)) NONE))
      NODE(TK_WHILE, AST_SCOPE
        NODE(TK_SEQ,
          NODE(TK_CALL,
            NONE NONE
            NODE(TK_DOT, NODE(TK_REFERENCE, ID(iter_name)) ID("has_next"))))
        NODE(TK_SEQ, AST_SCOPE
          NODE_ERROR_AT(TK_ASSIGN, for_idseq,
            NODE(TK_CALL,
              NONE NONE
              NODE(TK_DOT, NODE(TK_REFERENCE, ID(iter_name)) ID("next")))
            NODE(TK_LET, TREE(for_idseq) TREE(for_type)))
          TREE(for_body))
        TREE(for_else))));

  return AST_OK;
}


static ast_result_t sugar_with(typecheck_t* t, ast_t** astp)
{
  AST_EXTRACT_CHILDREN(*astp, withexpr, body, else_clause);
  expand_none(else_clause);

  BUILD(replace, *astp,
    NODE(TK_SEQ,
      NODE(TK_TRY2, AST_SCOPE
        NODE(TK_SEQ, AST_SCOPE
          TREE(body))
        NODE(TK_SEQ, AST_SCOPE
          TREE(else_clause))
        NODE(TK_SEQ, AST_SCOPE))));

  ast_t* tryexpr = ast_child(replace);
  AST_GET_CHILDREN(tryexpr, try_body, try_else, try_then);

  ast_t* withelem = ast_child(withexpr);

  while(withelem != NULL)
  {
    AST_GET_CHILDREN(withelem, idseq, type, init);
    const char* init_name = package_hygienic_id(t);

    BUILD(assign, idseq,
      NODE(TK_ASSIGN,
        TREE(init)
        NODE(TK_LET, NODE(TK_IDSEQ, ID(init_name)) NONE)));

    BUILD(local, idseq,
      NODE(TK_ASSIGN,
        NODE(TK_REFERENCE, ID(init_name))
        NODE(TK_LET, TREE(idseq) TREE(type))));

    BUILD(dispose, idseq,
      NODE_ERROR_AT(TK_CALL, idseq,
        NONE NONE
        NODE(TK_DOT, NODE(TK_REFERENCE, ID(init_name)) ID("dispose"))));

    ast_add(replace, assign);
    ast_add(try_body, local);
    ast_add(try_else, local);
    ast_add(try_then, dispose);

    withelem = ast_sibling(withelem);
  }

  ast_replace(astp, replace);
  return AST_OK;
}


static ast_result_t sugar_case(ast_t* ast)
{
  ast_t* body = ast_childidx(ast, 2);

  if(ast_id(body) != TK_NONE)
    return AST_OK;

  // We have no body, take a copy of the next case with a body
  ast_t* next = ast;
  ast_t* next_body = body;

  while(ast_id(next_body) == TK_NONE)
  {
    next = ast_sibling(next);
    assert(next != NULL);
    assert(ast_id(next) == TK_CASE);
    next_body = ast_childidx(next, 2);
  }

  ast_replace(&body, next_body);
  return AST_OK;
}


static ast_result_t sugar_update(ast_t** astp)
{
  ast_t* ast = *astp;
  assert(ast_id(ast) == TK_ASSIGN);

  AST_GET_CHILDREN(ast, value, call);

  if(ast_id(call) != TK_CALL)
    return AST_OK;

  // We are of the form:  x(y) = z
  // Replace us with:     x.update(y where value = z)
  AST_EXTRACT_CHILDREN(call, positional, named, expr);

  // If there are no named arguments yet, named will be a TK_NONE
  ast_setid(named, TK_NAMEDARGS);

  // Embed the value in a SEQ
  ast_t* value_seq = ast_from(value, TK_SEQ);
  ast_add(value_seq, value);

  // Build a new namedarg
  BUILD(namedarg, ast,
    NODE(TK_NAMEDARG,
      ID("value")
      TREE(value_seq)
      ));

  // Append the named arg to our existing list
  ast_append(named, namedarg);

  // Replace with the update call
  REPLACE(astp,
    NODE(TK_CALL,
      TREE(positional)
      TREE(named)
      NODE(TK_DOT, TREE(expr) ID("update"))
      ));

  return AST_OK;
}


static ast_result_t sugar_object(pass_opt_t* opt, ast_t** astp)
{
  typecheck_t* t = &opt->check;
  ast_t* ast = *astp;
  AST_GET_CHILDREN(ast, provides, members);
  ast_t* c_id = ast_from_string(ast, package_hygienic_id(t));

  // Create a new anonymous type.
  BUILD(def, ast,
    NODE(TK_CLASS, AST_SCOPE
      TREE(c_id)
      NONE
      NONE
      TREE(provides)
      NODE(TK_MEMBERS)
      NONE
      NONE));

  // We will have a create method in the type.
  BUILD(create, members,
    NODE(TK_NEW, AST_SCOPE
      NONE
      ID("create")
      NONE
      NODE(TK_PARAMS)
      NONE
      NONE
      NODE(TK_SEQ)
      NODE(TK_DBLARROW)));

  // We will replace object..end with $0.create(...)
  BUILD(call, ast,
    NODE(TK_CALL,
      NODE(TK_POSITIONALARGS)
      NONE
      NODE(TK_DOT, NODE(TK_REFERENCE, TREE(c_id)) ID("create"))));

  ast_t* create_params = ast_childidx(create, 3);
  ast_t* create_body = ast_childidx(create, 6);
  ast_t* call_args = ast_child(call);
  ast_t* class_members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      {
        AST_GET_CHILDREN(member, id, type, init);
        ast_t* p_id = ast_from_string(id, package_hygienic_id(t));

        // The field is: var/let id: type
        BUILD(field, member,
          NODE(ast_id(member),
            TREE(id)
            TREE(type)
            NONE));

        // The param is: $0: type
        BUILD(param, member,
          NODE(TK_PARAM,
            TREE(p_id)
            TREE(type)
            NONE));

        // The body of create contains: id = consume $0
        BUILD(assign, init,
          NODE(TK_ASSIGN,
            NODE(TK_CONSUME, NODE(TK_REFERENCE, TREE(p_id)))
            NODE(TK_REFERENCE, TREE(id))));

        ast_append(class_members, field);
        ast_append(create_params, param);
        ast_append(create_body, assign);
        ast_append(call_args, init);
        break;
      }

      default:
        // Keep all the methods as they are.
        ast_append(class_members, member);
        break;
    }

    member = ast_sibling(member);
  }

  // End the constructor with None, in case it has no parameters.
  BUILD(none, ast, NODE(TK_REFERENCE, ID("None")));
  ast_append(create_body, none);

  // Add the create function at the end.
  ast_append(class_members, create);

  // Add the new type to the current module.
  ast_t* module = ast_nearest(ast, TK_MODULE);
  ast_append(module, def);

  // Replace object..end with $0.create(...)
  ast_replace(astp, call);

  // Sugar the call.
  return ast_visit(astp, pass_sugar, NULL, opt);
}


ast_result_t sugar_binop(ast_t** astp, const char* fn_name)
{
  AST_GET_CHILDREN(*astp, left, right);

  REPLACE(astp,
    NODE(TK_CALL,
      NODE(TK_POSITIONALARGS, NODE(TK_SEQ, TREE(right)))
      NONE
      NODE(TK_DOT, TREE(left) ID(fn_name))
      ));

  return AST_OK;
}


ast_result_t sugar_unop(ast_t** astp, const char* fn_name)
{
  AST_GET_CHILDREN(*astp, expr);

  REPLACE(astp,
    NODE(TK_CALL,
      NONE NONE
      NODE(TK_DOT, TREE(expr) ID(fn_name))
      ));

  return AST_OK;
}


ast_result_t pass_sugar(ast_t** astp, pass_opt_t* options)
{
  typecheck_t* t = &options->check;
  ast_t* ast = *astp;
  assert(ast != NULL);

  switch(ast_id(ast))
  {
    case TK_PRIMITIVE:  return sugar_member(ast, true, true, TK_VAL);
    case TK_CLASS:      return sugar_member(ast, true, false, TK_REF);
    case TK_ACTOR:      return sugar_member(ast, true, false, TK_TAG);
    case TK_TRAIT:
    case TK_INTERFACE:  return sugar_member(ast, false, false, TK_REF);
    case TK_TYPEPARAM:  return sugar_typeparam(ast);
    case TK_NEW:        return sugar_new(t, ast);
    case TK_BE:         return sugar_be(t, ast);
    case TK_FUN:        return sugar_fun(ast);
    case TK_RETURN:
    case TK_BREAK:      return sugar_return(ast);
    case TK_IF:
    case TK_MATCH:
    case TK_WHILE:
    case TK_REPEAT:     return sugar_else(ast);
    case TK_TRY:        return sugar_try(ast);
    case TK_FOR:        return sugar_for(t, astp);
    case TK_WITH:       return sugar_with(t, astp);
    case TK_CASE:       return sugar_case(ast);
    case TK_ASSIGN:     return sugar_update(astp);
    case TK_OBJECT:     return sugar_object(options, astp);
    case TK_PLUS:       return sugar_binop(astp, "add");
    case TK_MINUS:      return sugar_binop(astp, "sub");
    case TK_MULTIPLY:   return sugar_binop(astp, "mul");
    case TK_DIVIDE:     return sugar_binop(astp, "div");
    case TK_MOD:        return sugar_binop(astp, "mod");
    case TK_LSHIFT:     return sugar_binop(astp, "shl");
    case TK_RSHIFT:     return sugar_binop(astp, "shr");
    case TK_AND:        return sugar_binop(astp, "op_and");
    case TK_OR:         return sugar_binop(astp, "op_or");
    case TK_XOR:        return sugar_binop(astp, "op_xor");
    case TK_EQ:         return sugar_binop(astp, "eq");
    case TK_NE:         return sugar_binop(astp, "ne");
    case TK_LT:         return sugar_binop(astp, "lt");
    case TK_LE:         return sugar_binop(astp, "le");
    case TK_GE:         return sugar_binop(astp, "ge");
    case TK_GT:         return sugar_binop(astp, "gt");
    case TK_UNARY_MINUS:return sugar_unop(astp, "neg");
    case TK_NOT:        return sugar_unop(astp, "op_not");
    default:            return AST_OK;
  }
}
