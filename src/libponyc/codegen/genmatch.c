#include "genmatch.h"
#include "gentype.h"
#include "gendesc.h"
#include "genexpr.h"
#include "genoperator.h"
#include "genreference.h"
#include "genfun.h"
#include "../type/subtype.h"
#include "../type/alias.h"
#include "../type/viewpoint.h"
#include "../ds/stringtab.h"
#include <assert.h>

static bool type_match(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern_type, LLVMBasicBlockRef next_block);

static bool value_integer(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern, LLVMBasicBlockRef next_block)
{
  // TODO:
  return false;
}

static bool value_float(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern, LLVMBasicBlockRef next_block)
{
  // TODO:
  return false;
}

static bool value_string(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern, LLVMBasicBlockRef next_block)
{
  // We know both sides are strings, so do a structural comparison.
  ast_t* pattern_type = ast_type(pattern);
  gentype_t g;

  if(!gentype(c, pattern_type, &g))
    return false;

  LLVMValueRef args[2];
  args[0] = gen_string(c, pattern);
  args[1] = LLVMBuildBitCast(c->builder, value, g.use_type, "");

  LLVMValueRef fun = genfun_proto(c, &g, stringtab("eq"), NULL);
  LLVMValueRef test = codegen_call(c, fun, args, 2);

  LLVMBasicBlockRef continue_block = codegen_block(c, "pattern_continue");
  LLVMBuildCondBr(c->builder, test, continue_block, next_block);
  LLVMPositionBuilderAtEnd(c->builder, continue_block);

  return true;
}

static bool value_tuple(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern, LLVMBasicBlockRef next_block)
{
  // TODO:
  return false;
}

static bool value_match(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern, LLVMBasicBlockRef next_block)
{
  // If we haven't been passed a pattern, skip this.
  if(pattern == NULL)
    return true;

  switch(ast_id(pattern))
  {
    case TK_CALL:
      // This is always a non-arithmetic primitive constant. No need to match
      // the value since we've already matched the type.
      return true;

    case TK_INT:
      return value_integer(c, value, type, pattern, next_block);

    case TK_FLOAT:
      return value_float(c, value, type, pattern, next_block);

    case TK_STRING:
      return value_string(c, value, type, pattern, next_block);

    case TK_VAR:
    case TK_LET:
      return gen_assign_value(c, pattern, value, type) != NULL;

    case TK_TUPLE:
      return value_tuple(c, value, type, pattern, next_block);

    default: {}
  }

  assert(0);
  return false;
}

static bool check_trait(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern_type, LLVMBasicBlockRef next_block)
{
  LLVMValueRef test = gendesc_istrait(c, value, pattern_type);

  LLVMBasicBlockRef continue_block = codegen_block(c, "pattern_continue");
  LLVMBuildCondBr(c->builder, test, continue_block, next_block);
  LLVMPositionBuilderAtEnd(c->builder, continue_block);

  return true;
}

static bool check_entity(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern_type, LLVMBasicBlockRef next_block)
{
  gentype_t g;

  if(!gentype(c, pattern_type, &g))
    return false;

  LLVMValueRef left, right;

  if(is_bool(type))
  {
    // The only entities that are valid matches for a Bool are True and False.
    left = value;
    right = g.instance;
  } else if((g.instance != NULL) && (g.primitive == NULL)) {
    // Save a load by checking the value instead of the descriptor.
    left = LLVMBuildPtrToInt(c->builder, value, c->intptr, "");
    right = LLVMBuildPtrToInt(c->builder, g.instance, c->intptr, "");
  } else {
    // Check the descriptor.
    left = gendesc_fetch(c, value);
    left = LLVMBuildPtrToInt(c->builder, left, c->intptr, "");
    right = LLVMConstPtrToInt(g.desc, c->intptr);
  }

  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntEQ, left, right, "");

  LLVMBasicBlockRef continue_block = codegen_block(c, "pattern_continue");
  LLVMBuildCondBr(c->builder, test, continue_block, next_block);
  LLVMPositionBuilderAtEnd(c->builder, continue_block);

  return true;
}

static bool check_nominal(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern_type, LLVMBasicBlockRef next_block)
{
  ast_t* def = (ast_t*)ast_data(pattern_type);

  switch(ast_id(def))
  {
    case TK_TRAIT:
      return check_trait(c, value, type, pattern_type, next_block);

    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
      return check_entity(c, value, type, pattern_type, next_block);

    default: {}
  }

  assert(0);
  return false;
}

static bool check_structural(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern_type, LLVMBasicBlockRef next_block)
{
  // Currently, structural types that aren't supertypes aren't allowed in
  // patterns, because we don't have a scheme for checking structural typing
  // at runtime, so this will never be reached.
  return false;
}

static void check_cardinality(compile_t* c, LLVMValueRef value, size_t size,
  LLVMBasicBlockRef next_block)
{
  LLVMValueRef field_count = gendesc_fieldcount(c, value);
  LLVMValueRef count = LLVMConstInt(c->i32, size, false);
  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntEQ, count, field_count,
    "");

  LLVMBasicBlockRef continue_block = codegen_block(c, "pattern_continue");
  LLVMBuildCondBr(c->builder, test, continue_block, next_block);
  LLVMPositionBuilderAtEnd(c->builder, continue_block);
}

static bool check_object_is_tuple(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern_type, LLVMBasicBlockRef next_block)
{
  // Check cardinality.
  size_t count = ast_childcount(pattern_type);
  check_cardinality(c, value, count, next_block);

  // Check elements.
  ast_t* pattern_child = ast_child(pattern_type);

  for(size_t i = 0; i < count; i++)
  {
    // TODO:
    // LLVMValueRef field_desc = gendesc_fielddesc(c, value, i);
    // LLVMValueRef offset = LLVMBuildExtractValue(c->builder, field_desc, 0, "");
    // LLVMValueRef desc = LLVMBuildExtractValue(c->builder, field_desc, 1, "");
    // LLVMValueRef null_desc = LLVMBuildIsNull(c->builder, desc, "");
    //
    // LLVMBasicBlockRef object_block = codegen_block(c, "pattern_tuple_object");
    // LLVMBasicBlockRef entity_block = codegen_block(c, "pattern_tuple_entity");
    // LLVMBuildCondBr(c->builder, null_desc, object_block, entity_block);
    //
    // LLVMPositionBuilderAtEnd(c->builder, object_block);
    // LLVM

    pattern_child = ast_sibling(pattern_child);
  }

  return true;
}

static bool check_tuple_is_tuple(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern_type, LLVMBasicBlockRef next_block)
{
  // Match fields for type pairwise.
  ast_t* type_child = ast_child(type);
  ast_t* pattern_child = ast_child(pattern_type);
  int i = 0;

  while(type_child != NULL)
  {
    LLVMValueRef field = LLVMBuildExtractValue(c->builder, value, i, "");

    if(!type_match(c, field, type_child, pattern_child, next_block))
      return false;

    type_child = ast_sibling(type_child);
    pattern_child = ast_sibling(pattern_child);
    i++;
  }

  return true;
}

static bool check_tuple(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern_type, LLVMBasicBlockRef next_block)
{
  // If we know the value is a tuple type, match elements pairwise.
  if(ast_id(type) == TK_TUPLETYPE)
    return check_tuple_is_tuple(c, value, type, pattern_type, next_block);

  // We have to check if we are a tuple dynamically.
  return check_object_is_tuple(c, value, type, pattern_type, next_block);
}

static bool check_union(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern_type, LLVMBasicBlockRef next_block)
{
  // We have to match some component type.
  LLVMBasicBlockRef continue_block = codegen_block(c, "pattern_continue");
  ast_t* child = ast_child(pattern_type);

  while(child != NULL)
  {
    // If we don't match, try the next type if there is one. If there is
    // no next type, jump to the next case.
    ast_t* next_type = ast_sibling(child);
    LLVMBasicBlockRef nomatch_block;

    if(next_type != NULL)
      nomatch_block = codegen_block(c, "pattern_next");
    else
      nomatch_block = next_block;

    if(!type_match(c, value, type, child, nomatch_block))
      return false;

    // If we do match, jump to the continue block.
    LLVMBuildBr(c->builder, continue_block);
    child = next_type;
  }

  LLVMPositionBuilderAtEnd(c->builder, continue_block);
  return true;
}

static bool check_isect(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern_type, LLVMBasicBlockRef next_block)
{
  // We have to match all component types.
  ast_t* child = ast_child(pattern_type);

  while(child != NULL)
  {
    if(!type_match(c, value, type, child, next_block))
      return false;

    child = ast_sibling(child);
  }

  return true;
}

static bool type_match(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern_type, LLVMBasicBlockRef next_block)
{
  switch(ast_id(pattern_type))
  {
    case TK_NOMINAL:
      return check_nominal(c, value, type, pattern_type, next_block);

    case TK_STRUCTURAL:
      return check_structural(c, value, type, pattern_type, next_block);

    case TK_TUPLETYPE:
      return check_tuple(c, value, type, pattern_type, next_block);

    case TK_UNIONTYPE:
      return check_union(c, value, type, pattern_type, next_block);

    case TK_ISECTTYPE:
      return check_isect(c, value, type, pattern_type, next_block);

    default: {}
  }

  assert(0);
  return false;
}

static bool pattern_match(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern, LLVMBasicBlockRef next_block)
{
  // Do nothing if we have no pattern.
  if(ast_id(pattern) == TK_NONE)
    return true;

  ast_t* pattern_type = ast_type(pattern);

  // No dynamic type check if we are statically the pattern type.
  if(!is_subtype(type, pattern_type))
  {
    if(!type_match(c, value, type, pattern_type, next_block))
      return false;
  }

  if(!value_match(c, value, type, pattern, next_block))
    return false;

  return true;
}

static bool guard_match(compile_t* c, ast_t* guard,
  LLVMBasicBlockRef next_block)
{
  if(ast_id(guard) == TK_NONE)
    return true;

  LLVMValueRef test = gen_expr(c, guard);

  if(test == NULL)
    return false;

  LLVMBasicBlockRef continue_block = codegen_block(c, "pattern_continue");
  LLVMBuildCondBr(c->builder, test, continue_block, next_block);
  LLVMPositionBuilderAtEnd(c->builder, continue_block);
  return true;
}

static bool case_body(compile_t* c, ast_t* body,
  LLVMBasicBlockRef post_block, LLVMValueRef phi, LLVMTypeRef phi_type)
{
  LLVMValueRef body_value = gen_expr(c, body);

  // If it returns, we don't branch to the post block.
  if(body_value == GEN_NOVALUE)
    return true;

  ast_t* body_type = ast_type(body);
  body_value = gen_assign_cast(c, phi_type, body_value, body_type);

  if(body_value == NULL)
    return false;

  LLVMBuildBr(c->builder, post_block);

  LLVMBasicBlockRef block = LLVMGetInsertBlock(c->builder);
  LLVMAddIncoming(phi, &body_value, &block, 1);

  return true;
}

LLVMValueRef gen_match(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, match_expr, cases, else_expr);

  ast_t* type = ast_type(ast);
  gentype_t phi_type;

  // We will have no type if all branches have return statements.
  if((type != NULL) && !gentype(c, type, &phi_type))
    return NULL;

  ast_t* match_type = alias(ast_type(match_expr));
  LLVMValueRef match_value = gen_expr(c, match_expr);

  LLVMBasicBlockRef pattern_block = codegen_block(c, "case_pattern");
  LLVMBasicBlockRef else_block = codegen_block(c, "match_else");
  LLVMBasicBlockRef post_block = codegen_block(c, "match_post");
  LLVMBasicBlockRef next_block;

  // Jump to the first case.
  LLVMBuildBr(c->builder, pattern_block);

  // Start the post block so that a case can modify the phi node.
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type.use_type, "");

  // Iterate over the cases.
  ast_t* the_case = ast_child(cases);

  while(the_case != NULL)
  {
    ast_t* next_case = ast_sibling(the_case);

    if(next_case != NULL)
      next_block = codegen_block(c, "case_pattern");
    else
      next_block = else_block;

    AST_GET_CHILDREN(the_case, pattern, guard, body);

    // Check the pattern.
    LLVMPositionBuilderAtEnd(c->builder, pattern_block);

    if(!pattern_match(c, match_value, match_type, pattern, next_block))
    {
      ast_free_unattached(match_type);
      return NULL;
    }

    // Check the guard.
    if(!guard_match(c, guard, next_block))
    {
      ast_free_unattached(match_type);
      return NULL;
    }

    // Case body.
    if(!case_body(c, body, post_block, phi, phi_type.use_type))
    {
      ast_free_unattached(match_type);
      return NULL;
    }

    the_case = next_case;
    pattern_block = next_block;
  }

  ast_free_unattached(match_type);

  // Else body.
  LLVMPositionBuilderAtEnd(c->builder, else_block);

  if(!case_body(c, else_expr, post_block, phi, phi_type.use_type))
    return NULL;

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  return phi;
}
