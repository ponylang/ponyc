#include "refer.h"
#include "../ast/id.h"
#include "../pass/expr.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <string.h>

/**
 * Make sure the definition of something occurs before its use. This is for
 * both fields and local variable.
 */
bool def_before_use(pass_opt_t* opt, ast_t* def, ast_t* use, const char* name)
{
  if((ast_line(def) > ast_line(use)) ||
     ((ast_line(def) == ast_line(use)) &&
      (ast_pos(def) > ast_pos(use))))
  {
    ast_error(opt->check.errors, use,
      "declaration of '%s' appears after use", name);
    ast_error_continue(opt->check.errors, def,
      "declaration of '%s' appears here", name);
    return false;
  }

  return true;
}

static bool is_this_incomplete(pass_opt_t* opt, ast_t* ast)
{
  // If we're in a default field initialiser, we're incomplete by definition.
  if(opt->check.frame->method == NULL)
    return true;

  // If we're not in a constructor, we're complete by definition.
  if(ast_id(opt->check.frame->method) != TK_NEW)
    return false;

  // Check if all fields have been marked as defined.
  ast_t* members = ast_childidx(opt->check.frame->type, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FLET:
      case TK_FVAR:
      case TK_EMBED:
      {
        sym_status_t status;
        ast_t* id = ast_child(member);
        ast_get(ast, ast_name(id), &status);

        if(status != SYM_DEFINED)
          return true;

        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  return false;
}

static bool is_assigned_to(ast_t* ast, bool check_result_needed)
{
  while(true)
  {
    ast_t* parent = ast_parent(ast);

    switch(ast_id(parent))
    {
      case TK_ASSIGN:
      {
        // Has to be the left hand side of an assignment (the first child).
        if(ast_child(parent) != ast)
          return false;

        if(!check_result_needed)
          return true;

        // The result of that assignment can't be used.
        return !is_result_needed(parent);
      }

      case TK_SEQ:
      {
        // Might be in a tuple on the left hand side.
        if(ast_childcount(parent) > 1)
          return false;

        break;
      }

      case TK_TUPLE:
        break;

      default:
        return false;
    }

    ast = parent;
  }
}

/**
 * Return true if this is a reference is just being used to call a constructor
 * on the type, in which case we don't care if it's status is SYM_DEFINED yet.
 *
 * Example:
 *   let a: Array[U8] = a.create()
 */
static bool is_constructed_from(ast_t* ast)
{
  ast_t* parent = ast_parent(ast);

  if(ast_id(parent) != TK_DOT)
    return false;

  AST_GET_CHILDREN(parent, left, right);
  if(left != ast)
    return false;

  ast_t* def = (ast_t*)ast_data(ast);

  // TK_LET and TK_VAR have their symtable point to the TK_ID child,
  // so if we encounter that here, we move up to the parent node.
  if(ast_id(def) == TK_ID)
    def = ast_parent(def);

  switch(ast_id(def))
  {
    case TK_VAR:
    case TK_LET:
    case TK_FVAR:
    case TK_FLET:
    case TK_EMBED:
    {
      ast_t* typeref = ast_childidx(def, 1);
      if((typeref == NULL) || (ast_data(typeref) == NULL))
        return false;

      ast_t* typedefn = (ast_t*)ast_data(typeref);
      ast_t* find = ast_get(typedefn, ast_name(right), NULL);

      return (find != NULL) && (ast_id(find) == TK_NEW);
    }

    default: {}
  }

  return false;
}

static bool valid_reference(pass_opt_t* opt, ast_t* ast, sym_status_t status)
{
  if(is_constructed_from(ast))
    return true;

  switch(status)
  {
    case SYM_DEFINED:
      return true;

    case SYM_CONSUMED:
      if(is_assigned_to(ast, true))
        return true;

      ast_error(opt->check.errors, ast,
        "can't use a consumed local in an expression");
      return false;

    case SYM_UNDEFINED:
      if(is_assigned_to(ast, true))
        return true;

      ast_error(opt->check.errors, ast,
        "can't use an undefined variable in an expression");
      return false;

    default: {}
  }

  pony_assert(0);
  return false;
}

static const char* suggest_alt_name(ast_t* ast, const char* name)
{
  pony_assert(ast != NULL);
  pony_assert(name != NULL);

  size_t name_len = strlen(name);

  if(is_name_private(name))
  {
    // Try without leading underscore
    const char* try_name = stringtab(name + 1);

    if(ast_get(ast, try_name, NULL) != NULL)
      return try_name;
  }
  else
  {
    // Try with a leading underscore
    char* buf = (char*)ponyint_pool_alloc_size(name_len + 2);
    buf[0] = '_';
    strncpy(buf + 1, name, name_len + 1);
    const char* try_name = stringtab_consume(buf, name_len + 2);

    if(ast_get(ast, try_name, NULL) != NULL)
      return try_name;
  }

  // Try with a different case (without crossing type/value boundary)
  ast_t* case_ast = ast_get_case(ast, name, NULL);
  if(case_ast != NULL)
  {
    ast_t* id = case_ast;

    if(ast_id(id) != TK_ID)
      id = ast_child(id);

    pony_assert(ast_id(id) == TK_ID);
    const char* try_name = ast_name(id);

    if(ast_get(ast, try_name, NULL) != NULL)
      return try_name;
  }

  // Give up
  return NULL;
}

static bool refer_this(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_THIS);

  // Can only use a this reference if it hasn't been consumed yet.
  sym_status_t status;
  ast_get(ast, stringtab("this"), &status);

  if(status == SYM_CONSUMED)
  {
    ast_error(opt->check.errors, ast,
      "can't use a consumed 'this' in an expression");
    return false;
  }
  pony_assert(status == SYM_NONE);

  // Mark the this reference as incomplete if not all fields are defined yet.
  if(is_this_incomplete(opt, ast))
    ast_setflag(ast, AST_FLAG_INCOMPLETE);

  return true;
}

bool refer_reference(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;

  const char* name = ast_name(ast_child(ast));

  // Handle the special case of the "don't care" reference (`_`)
  if(is_name_dontcare(name))
  {
    ast_setid(ast, TK_DONTCAREREF);

    return true;
  }

  // Everything we reference must be in scope, so we can use ast_get for lookup.
  sym_status_t status;
  ast_t* def = ast_get(ast, ast_name(ast_child(ast)), &status);

  // If nothing was found, we fail, but also try to suggest an alternate name.
  if(def == NULL)
  {
    const char* alt_name = suggest_alt_name(ast, name);

    if(alt_name == NULL)
      ast_error(opt->check.errors, ast, "can't find declaration of '%s'", name);
    else
      ast_error(opt->check.errors, ast,
        "can't find declaration of '%s', did you mean '%s'?", name, alt_name);

    return false;
  }

  // Save the found definition in the AST, so we don't need to look it up again.
  ast_setdata(ast, (void*)def);

  switch(ast_id(def))
  {
    case TK_PACKAGE:
    {
      // Only allowed if in a TK_DOT with a type.
      if(ast_id(ast_parent(ast)) != TK_DOT)
      {
        ast_error(opt->check.errors, ast,
          "a package can only appear as a prefix to a type");
        return false;
      }

      ast_setid(ast, TK_PACKAGEREF);
      return true;
    }

    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_TYPE:
    case TK_TYPEPARAM:
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
    {
      ast_setid(ast, TK_TYPEREF);

      ast_add(ast, ast_from(ast, TK_NONE));    // 1st child: package reference
      ast_append(ast, ast_from(ast, TK_NONE)); // 3rd child: type args

      return true;
    }

    case TK_FVAR:
    case TK_FLET:
    case TK_EMBED:
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    {
      // Transform to "this.f".
      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_child(ast));

      ast_t* self = ast_from(ast, TK_THIS);
      ast_add(dot, self);

      ast_replace(astp, dot);
      ast = *astp;
      return refer_this(opt, self) && refer_dot(opt, ast);
    }

    case TK_PARAM:
    {
      if(opt->check.frame->def_arg != NULL)
      {
        ast_error(opt->check.errors, ast,
          "can't reference a parameter in a default argument");
        return false;
      }

      if(!def_before_use(opt, def, ast, name))
        return false;

      if(!valid_reference(opt, ast, status))
        return false;

      ast_setid(ast, TK_PARAMREF);
      return true;
    }

    case TK_VAR:
    case TK_LET:
    case TK_MATCH_CAPTURE:
    {
      if(!def_before_use(opt, def, ast, name))
        return false;

      if(!valid_reference(opt, ast, status))
        return false;

      if(ast_id(def) == TK_VAR)
        ast_setid(ast, TK_VARREF);
      else
        ast_setid(ast, TK_LETREF);

      return true;
    }

    default: {}
  }

  pony_assert(0);
  return false;
}

static bool refer_packageref_dot(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_DOT);
  AST_GET_CHILDREN(ast, left, right);
  pony_assert(ast_id(left) == TK_PACKAGEREF);
  pony_assert(ast_id(right) == TK_ID);

  // Must be a type in a package.
  const char* package_name = ast_name(ast_child(left));
  ast_t* package = ast_get(left, package_name, NULL);

  if(package == NULL)
  {
    ast_error(opt->check.errors, right, "can't access package '%s'",
      package_name);
    return false;
  }

  pony_assert(ast_id(package) == TK_PACKAGE);
  const char* type_name = ast_name(right);
  ast_t* def = ast_get(package, type_name, NULL);

  if(def == NULL)
  {
    ast_error(opt->check.errors, right, "can't find type '%s' in package '%s'",
      type_name, package_name);
    return false;
  }

  ast_setdata(ast, (void*)def);
  ast_setid(ast, TK_TYPEREF);

  ast_append(ast, ast_from(ast, TK_NONE)); // 3rd child: type args

  return true;
}

static bool refer_this_dot(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_DOT);
  AST_GET_CHILDREN(ast, left, right);
  pony_assert(ast_id(left) == TK_THIS);
  pony_assert(ast_id(right) == TK_ID);

  const char* name = ast_name(right);

  sym_status_t status;
  ast_t* def = ast_get(ast, name, &status);
  ast_setdata(ast, (void*)def);

  // If nothing was found, we fail, but also try to suggest an alternate name.
  if(def == NULL)
  {
    const char* alt_name = suggest_alt_name(ast, name);

    if(alt_name == NULL)
      ast_error(opt->check.errors, ast, "can't find declaration of '%s'", name);
    else
      ast_error(opt->check.errors, ast,
        "can't find declaration of '%s', did you mean '%s'?", name, alt_name);

    return false;
  }

  switch(ast_id(def))
  {
    case TK_FVAR:
    case TK_FLET:
    case TK_EMBED:
      if(!valid_reference(opt, ast, status))
        return false;
      break;

    default: {}
  }

  return true;
}

bool refer_dot(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_DOT);
  AST_GET_CHILDREN(ast, left, right);

  switch(ast_id(left))
  {
    case TK_PACKAGEREF: return refer_packageref_dot(opt, ast);
    case TK_THIS:       return refer_this_dot(opt, ast);
    default: {}
  }

  return true;
}

static bool qualify_typeref(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  ast_t* typeref = ast_child(ast);

  // If the typeref already has type args, it can't get any more, so we'll
  // leave as TK_QUALIFY, so expr pass will sugar as qualified call to apply.
  if(ast_id(ast_childidx(typeref, 2)) == TK_TYPEARGS)
    return true;

  ast_t* def = (ast_t*)ast_data(typeref);
  pony_assert(def != NULL);

  // If the type isn't polymorphic it can't get type args at all, so we'll
  // leave as TK_QUALIFY, so expr pass will sugar as qualified call to apply.
  ast_t* typeparams = ast_childidx(def, 1);
  if(ast_id(typeparams) == TK_NONE)
    return true;

  // Now, we want to get rid of the inner typeref, take its children, and
  // convert this TK_QUALIFY node into a TK_TYPEREF node with type args.
  ast_setdata(ast, (void*)def);
  ast_setid(ast, TK_TYPEREF);

  pony_assert(typeref == ast_pop(ast));
  ast_t* package   = ast_pop(typeref);
  ast_t* type_name = ast_pop(typeref);
  ast_free(typeref);

  ast_add(ast, type_name);
  ast_add(ast, package);

  return true;
}

bool refer_qualify(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_QUALIFY);

  if(ast_id(ast_child(ast)) == TK_TYPEREF)
    return qualify_typeref(opt, ast);

  return true;
}

static bool assign_id(pass_opt_t* opt, ast_t* ast, bool let, bool need_value)
{
  pony_assert(ast_id(ast) == TK_ID);
  const char* name = ast_name(ast);

  sym_status_t status;
  ast_get(ast, name, &status);

  switch(status)
  {
    case SYM_UNDEFINED:
      if(need_value)
      {
        ast_error(opt->check.errors, ast,
          "the left side is undefined but its value is used");
        return false;
      }

      ast_setstatus(ast, name, SYM_DEFINED);
      return true;

    case SYM_DEFINED:
      if(let)
      {
        ast_error(opt->check.errors, ast,
          "can't assign to a let or embed definition more than once");
        return false;
      }

      return true;

    case SYM_CONSUMED:
    {
      bool ok = true;

      if(need_value)
      {
        ast_error(opt->check.errors, ast,
          "the left side is consumed but its value is used");
        ok = false;
      }

      if(let)
      {
        ast_error(opt->check.errors, ast,
          "can't assign to a let or embed definition more than once");
        ok = false;
      }

      if(opt->check.frame->try_expr != NULL)
      {
        ast_error(opt->check.errors, ast,
          "can't reassign to a consumed identifier in a try expression");
        ok = false;
      }

      if(ok)
        ast_setstatus(ast, name, SYM_DEFINED);

      return ok;
    }

    default: {}
  }

  pony_assert(0);
  return false;
}

static bool is_lvalue(pass_opt_t* opt, ast_t* ast, bool need_value)
{
  switch(ast_id(ast))
  {
    case TK_DONTCARE:
      return true;

    case TK_DONTCAREREF:
      // Can only assign to it if we don't need the value.
      return !need_value;

    case TK_VAR:
    case TK_LET:
      return assign_id(opt, ast_child(ast), ast_id(ast) == TK_LET, need_value);

    case TK_VARREF:
    {
      ast_t* id = ast_child(ast);
      return assign_id(opt, id, false, need_value);
    }

    case TK_LETREF:
    {
      ast_error(opt->check.errors, ast, "can't reassign to a let local");
      return false;
    }

    case TK_DOT:
    {
      AST_GET_CHILDREN(ast, left, right);

      if(ast_id(left) == TK_THIS)
      {
        ast_t* def = (ast_t*)ast_data(ast);

        if(def == NULL)
          return false;

        switch(ast_id(def))
        {
          case TK_FVAR:  return assign_id(opt, right, false, need_value);
          case TK_FLET:
          case TK_EMBED: return assign_id(opt, right, true, need_value);
          default:       return false;
        }
      }

      return true;
    }

    case TK_TUPLE:
    {
      // A tuple is an lvalue if every component expression is an lvalue.
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        if(!is_lvalue(opt, child, need_value))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_SEQ:
    {
      // A sequence is an lvalue if it has a single child that is an lvalue.
      // This is used because the components of a tuple are sequences.
      ast_t* child = ast_child(ast);

      if(ast_sibling(child) != NULL)
        return false;

      return is_lvalue(opt, child, need_value);
    }

    default: {}
  }

  return false;
}

static bool refer_pre_call(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_CALL);
  AST_GET_CHILDREN(ast, lhs, positional, named, question);

  // Run the args before the receiver, so that symbol status tracking
  // will see things like consumes in the right side first.
  if(!ast_passes_subtree(&positional, opt, PASS_REFER) ||
    !ast_passes_subtree(&named, opt, PASS_REFER))
    return false;

  return true;
}

static bool refer_pre_assign(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_ASSIGN);
  AST_GET_CHILDREN(ast, left, right);

  // Run the right side before the left side, so that symbol status tracking
  // will see things like consumes in the right side first.
  if(!ast_passes_subtree(&right, opt, PASS_REFER))
    return false;

  return true;
}

static bool refer_assign(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_ASSIGN);
  AST_GET_CHILDREN(ast, left, right);

  if(!is_lvalue(opt, left, is_result_needed(ast)))
  {
    if(ast_id(left) == TK_DONTCAREREF)
    {
      ast_error(opt->check.errors, left,
        "can't read from '_'");
    } else {
      ast_error(opt->check.errors, ast,
        "left side must be something that can be assigned to");
    }
    return false;
  }

  return true;
}

static bool refer_consume(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_CONSUME);
  AST_GET_CHILDREN(ast, cap, term);

  const char* name = NULL;

  switch(ast_id(term))
  {
    case TK_VARREF:
    case TK_LETREF:
    case TK_PARAMREF:
    {
      ast_t* id = ast_child(term);
      name = ast_name(id);
      break;
    }

    case TK_THIS:
    {
      name = stringtab("this");
      break;
    }

    default:
      ast_error(opt->check.errors, ast,
        "consume must take 'this', a local, or a parameter");
      return false;
  }

  // Can't consume from an outer scope while in a loop condition.
  if((opt->check.frame->loop_cond != NULL) &&
    !ast_within_scope(opt->check.frame->loop_cond, ast, name))
  {
    ast_error(opt->check.errors, ast,
      "can't consume from an outer scope in a loop condition");
    return false;
  }

  ast_setstatus(ast, name, SYM_CONSUMED);

  return true;
}

static bool refer_pre_new(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  pony_assert(ast_id(ast) == TK_NEW);

  // Set all fields to undefined at the start of this scope.
  ast_t* members = ast_parent(ast);
  ast_t* member = ast_child(members);
  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
      {
        // Mark this field as SYM_UNDEFINED.
        AST_GET_CHILDREN(member, id, type, expr);
        ast_setstatus(ast, ast_name(id), SYM_UNDEFINED);
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  return true;
}

static bool refer_new(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_NEW);

  ast_t* members = ast_parent(ast);
  ast_t* member = ast_child(members);
  bool result = true;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
      {
        sym_status_t status;
        ast_t* id = ast_child(member);
        ast_t* def = ast_get(ast, ast_name(id), &status);

        if((def != member) || (status != SYM_DEFINED))
        {
          ast_error(opt->check.errors, def,
            "field left undefined in constructor");
          result = false;
        }

        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  if(!result)
    ast_error(opt->check.errors, ast,
      "constructor with undefined fields is here");

  return result;
}

static bool refer_local(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast != NULL);
  pony_assert(ast_type(ast) != NULL);

  AST_GET_CHILDREN(ast, id, type);
  pony_assert(type != NULL);

  bool is_dontcare = is_name_dontcare(ast_name(id));

  if(ast_id(type) == TK_NONE)
  {
    // No type specified, infer it later
    if(!is_dontcare && !is_assigned_to(ast, false))
    {
      ast_error(opt->check.errors, ast,
        "locals must specify a type or be assigned a value");
      return false;
    }
  }
  else if(ast_id(ast) == TK_LET)
  {
    // Let, check we have a value assigned
    if(!is_assigned_to(ast, false))
    {
      ast_error(opt->check.errors, ast,
        "can't declare a let local without assigning to it");
      return false;
    }
  }

  if(is_dontcare)
    ast_setid(ast, TK_DONTCARE);

  return true;
}

static bool refer_seq(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  pony_assert(ast_id(ast) == TK_SEQ);

  // If the last expression jumps away with no value, then we do too.
  if(ast_checkflag(ast_childlast(ast), AST_FLAG_JUMPS_AWAY))
    ast_setflag(ast, AST_FLAG_JUMPS_AWAY);

  // Propagate consumes forward if this seq is the body of a try expression.
  if(ast_has_scope(ast))
  {
    ast_t* parent = ast_parent(ast);

    switch(ast_id(parent))
    {
      case TK_TRY:
      case TK_TRY_NO_CHECK:
      {
        AST_GET_CHILDREN(parent, body, else_clause, then_clause);

        if(body == ast)
        {
          // Push our consumes, but not defines, to the else clause.
          ast_inheritbranch(else_clause, body);
          ast_consolidate_branches(else_clause, 2);
        } else if(else_clause == ast) {
          // Push our consumes, but not defines, to the then clause. This
          // includes the consumes from the body.
          ast_inheritbranch(then_clause, else_clause);
          ast_consolidate_branches(then_clause, 2);
        }
      }

      default: {}
    }
  }

  return true;
}

static bool refer_if(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  pony_assert((ast_id(ast) == TK_IF) || (ast_id(ast) == TK_IFDEF));
  AST_GET_CHILDREN(ast, cond, left, right);

  size_t branch_count = 0;

  if(!ast_checkflag(left, AST_FLAG_JUMPS_AWAY))
  {
    branch_count++;
    ast_inheritbranch(ast, left);
  }

  if(!ast_checkflag(right, AST_FLAG_JUMPS_AWAY))
  {
    branch_count++;
    ast_inheritbranch(ast, right);
  }

  ast_consolidate_branches(ast, branch_count);

  // If all branches jump away with no value, then we do too.
  if(branch_count == 0)
    ast_setflag(ast, AST_FLAG_JUMPS_AWAY);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);

  return true;
}

static bool refer_iftype(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  pony_assert(ast_id(ast) == TK_IFTYPE_SET);

  AST_GET_CHILDREN(ast, left_clause, right);
  AST_GET_CHILDREN(left_clause, sub, super, left);

  size_t branch_count = 0;

  if(!ast_checkflag(left, AST_FLAG_JUMPS_AWAY))
  {
    branch_count++;
    ast_inheritbranch(ast, left);
  }

  if(!ast_checkflag(right, AST_FLAG_JUMPS_AWAY))
  {
    branch_count++;
    ast_inheritbranch(ast, right);
  }

  ast_consolidate_branches(ast, branch_count);

  // If all branches jump away with no value, then we do too.
  if(branch_count == 0)
    ast_setflag(ast, AST_FLAG_JUMPS_AWAY);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);

  return true;
}

static bool refer_while(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_WHILE);
  AST_GET_CHILDREN(ast, cond, body, else_clause);

  // All consumes have to be in scope when the loop body finishes.
  errorframe_t errorf = NULL;
  if(!ast_all_consumes_in_scope(body, body, &errorf))
  {
    errorframe_report(&errorf, opt->check.errors);
    return false;
  }

  size_t branch_count = 0;

  // No symbol status is inherited from the loop body. Nothing from outside the
  // loop body can be consumed, and definitions in the body may not occur.
  if(!ast_checkflag(body, AST_FLAG_JUMPS_AWAY))
    branch_count++;

  if(!ast_checkflag(else_clause, AST_FLAG_JUMPS_AWAY))
  {
    branch_count++;
    ast_inheritbranch(ast, body);

    // Use a branch count of two instead of one. This means we will pick up any
    // consumes, but not any definitions, since definitions may not occur.
    ast_consolidate_branches(ast, 2);
  }

  // If all branches jump away with no value, then we do too.
  if(branch_count == 0)
    ast_setflag(ast, AST_FLAG_JUMPS_AWAY);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);

  return true;
}

static bool refer_repeat(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_REPEAT);
  AST_GET_CHILDREN(ast, cond, body, else_clause);

  // All consumes have to be in scope when the loop body finishes.
  errorframe_t errorf = NULL;
  if(!ast_all_consumes_in_scope(body, body, &errorf))
  {
    errorframe_report(&errorf, opt->check.errors);
    return false;
  }

  size_t branch_count = 0;

  // No symbol status is inherited from the loop body. Nothing from outside the
  // loop body can be consumed, and definitions in the body may not occur.
  if(!ast_checkflag(body, AST_FLAG_JUMPS_AWAY))
    branch_count++;

  if(!ast_checkflag(else_clause, AST_FLAG_JUMPS_AWAY))
  {
    branch_count++;
    ast_inheritbranch(ast, else_clause);

    // Use a branch count of two instead of one. This means we will pick up any
    // consumes, but not any definitions, since definitions may not occur.
    ast_consolidate_branches(ast, 2);
  }

  // If all branches jump away with no value, then we do too.
  if(branch_count == 0)
    ast_setflag(ast, AST_FLAG_JUMPS_AWAY);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);

  return true;
}

static bool refer_match(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  pony_assert(ast_id(ast) == TK_MATCH);
  AST_GET_CHILDREN(ast, expr, cases, else_clause);

  size_t branch_count = 0;

  if(!ast_checkflag(cases, AST_FLAG_JUMPS_AWAY))
  {
    branch_count++;
    ast_inheritbranch(ast, cases);
  }

  if(ast_id(else_clause) == TK_NONE)
  {
    branch_count++;
  }
  else if(!ast_checkflag(else_clause, AST_FLAG_JUMPS_AWAY))
  {
    branch_count++;
    ast_inheritbranch(ast, else_clause);
  }

  // If all branches jump away with no value, then we do too.
  if(branch_count == 0)
    ast_setflag(ast, AST_FLAG_JUMPS_AWAY);

  ast_consolidate_branches(ast, branch_count);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);

  return true;
}

static bool refer_cases(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_CASES);
  ast_t* the_case = ast_child(ast);

  if(the_case == NULL)
  {
    ast_error(opt->check.errors, ast, "match must have at least one case");
    return false;
  }

  size_t branch_count = 0;

  while(the_case != NULL)
  {
    AST_GET_CHILDREN(the_case, pattern, guard, body);

    if(!ast_checkflag(body, AST_FLAG_JUMPS_AWAY))
    {
      branch_count++;
      ast_inheritbranch(ast, the_case);
    }

    the_case = ast_sibling(the_case);
  }

  if(branch_count == 0)
    ast_setflag(ast, AST_FLAG_JUMPS_AWAY);

  ast_consolidate_branches(ast, branch_count);

  return true;
}

static bool refer_try(pass_opt_t* opt, ast_t* ast)
{
  pony_assert((ast_id(ast) == TK_TRY) || (ast_id(ast) == TK_TRY_NO_CHECK));
  AST_GET_CHILDREN(ast, body, else_clause, then_clause);

  size_t branch_count = 0;

  if(!ast_checkflag(body, AST_FLAG_JUMPS_AWAY))
    branch_count++;

  if(!ast_checkflag(else_clause, AST_FLAG_JUMPS_AWAY))
    branch_count++;

  if(ast_checkflag(then_clause, AST_FLAG_JUMPS_AWAY))
  {
    ast_error(opt->check.errors, then_clause,
      "then clause always terminates the function");
    return false;
  }

  // If all branches jump away with no value, then we do too.
  if(branch_count == 0)
    ast_setflag(ast, AST_FLAG_JUMPS_AWAY);

  // Push the symbol status from the then clause to our parent scope.
  ast_inheritstatus(ast_parent(ast), then_clause);

  return true;
}

static bool refer_recover(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  pony_assert(ast_id(ast) == TK_RECOVER);
  AST_GET_CHILDREN(ast, cap, expr);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), expr);

  return true;
}

static bool refer_break(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_BREAK);

  if(opt->check.frame->loop_body == NULL)
  {
    ast_error(opt->check.errors, ast, "must be in a loop");
    return false;
  }

  errorframe_t errorf = NULL;
  if(!ast_all_consumes_in_scope(opt->check.frame->loop_body, ast, &errorf))
  {
    errorframe_report(&errorf, opt->check.errors);
    return false;
  }

  // break is always the last expression in a sequence
  pony_assert(ast_sibling(ast) == NULL);

  ast_setflag(ast, AST_FLAG_JUMPS_AWAY);

  return true;
}

static bool refer_continue(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_CONTINUE);

  if(opt->check.frame->loop_body == NULL)
  {
    ast_error(opt->check.errors, ast, "must be in a loop");
    return false;
  }

  errorframe_t errorf = NULL;
  if(!ast_all_consumes_in_scope(opt->check.frame->loop_body, ast, &errorf))
  {
    errorframe_report(&errorf, opt->check.errors);
    return false;
  }

  // continue is always the last expression in a sequence
  pony_assert(ast_sibling(ast) == NULL);

  ast_setflag(ast, AST_FLAG_JUMPS_AWAY);

  return true;
}

static bool refer_return(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_RETURN);

  // return is always the last expression in a sequence
  pony_assert(ast_sibling(ast) == NULL);

  ast_setflag(ast, AST_FLAG_JUMPS_AWAY);

  if((ast_id(opt->check.frame->method) == TK_NEW) &&
    is_this_incomplete(opt, ast))
  {
    ast_error(opt->check.errors, ast,
      "all fields must be defined before constructor returns");
    return false;
  }

  return true;
}

static bool refer_error(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  // error is always the last expression in a sequence
  pony_assert(ast_sibling(ast) == NULL);

  ast_setflag(ast, AST_FLAG_JUMPS_AWAY);
  return true;
}

static bool refer_compile_error(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  // compile_error is always the last expression in a sequence
  pony_assert(ast_sibling(ast) == NULL);

  ast_setflag(ast, AST_FLAG_JUMPS_AWAY);
  return true;
}

ast_result_t pass_pre_refer(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;
  bool r = true;

  switch(ast_id(ast))
  {
    case TK_NEW:    r = refer_pre_new(options, ast); break;
    case TK_CALL:   r = refer_pre_call(options, ast); break;
    case TK_ASSIGN: r = refer_pre_assign(options, ast); break;

    default: {}
  }

  if(!r)
  {
    pony_assert(errors_get_count(options->check.errors) > 0);
    return AST_ERROR;
  }

  return AST_OK;
}

ast_result_t pass_refer(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;
  bool r = true;

  switch(ast_id(ast))
  {
    case TK_REFERENCE: r = refer_reference(options, astp); break;
    case TK_DOT:       r = refer_dot(options, ast); break;
    case TK_QUALIFY:   r = refer_qualify(options, ast); break;

    case TK_ASSIGN:    r = refer_assign(options, ast); break;
    case TK_CONSUME:   r = refer_consume(options, ast); break;

    case TK_THIS:      r = refer_this(options, ast); break;
    case TK_NEW:       r = refer_new(options, ast); break;
    case TK_VAR:
    case TK_LET:       r = refer_local(options, ast); break;

    case TK_SEQ:       r = refer_seq(options, ast); break;
    case TK_IFDEF:
    case TK_IF:        r = refer_if(options, ast); break;
    case TK_IFTYPE_SET:
                       r = refer_iftype(options, ast); break;
    case TK_WHILE:     r = refer_while(options, ast); break;
    case TK_REPEAT:    r = refer_repeat(options, ast); break;
    case TK_MATCH:     r = refer_match(options, ast); break;
    case TK_CASES:     r = refer_cases(options, ast); break;
    case TK_TRY_NO_CHECK:
    case TK_TRY:       r = refer_try(options, ast); break;
    case TK_RECOVER:   r = refer_recover(options, ast); break;
    case TK_BREAK:     r = refer_break(options, ast); break;
    case TK_CONTINUE:  r = refer_continue(options, ast); break;
    case TK_RETURN:    r = refer_return(options, ast); break;
    case TK_ERROR:     r = refer_error(options, ast); break;
    case TK_COMPILE_ERROR:
                       r = refer_compile_error(options, ast); break;

    default: {}
  }

  if(!r)
  {
    pony_assert(errors_get_count(options->check.errors) > 0);
    return AST_ERROR;
  }

  return AST_OK;
}
