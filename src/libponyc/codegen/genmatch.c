#include "genmatch.h"
#include "gentype.h"
#include "gendesc.h"
#include "genexpr.h"
#include "genoperator.h"
#include "genreference.h"
#include "gencall.h"
#include "../pass/expr.h"
#include "../type/subtype.h"
#include "../type/alias.h"
#include "../type/viewpoint.h"
#include "../type/lookup.h"
#include "../ast/stringtab.h"
#include <assert.h>

static bool check_type(compile_t* c, LLVMValueRef ptr, LLVMValueRef desc,
  ast_t* pattern_type, LLVMBasicBlockRef next_block);

static bool dynamic_match_ptr(compile_t* c, LLVMValueRef ptr,
  LLVMValueRef desc, ast_t* pattern, LLVMBasicBlockRef next_block);

static bool dynamic_match_object(compile_t* c, LLVMValueRef object,
  LLVMValueRef desc, ast_t* pattern, LLVMBasicBlockRef next_block);

static bool static_match(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern, LLVMBasicBlockRef next_block);

static ast_t* eq_param_type(ast_t* pattern)
{
  ast_t* pattern_type = ast_type(pattern);
  ast_t* fun = lookup(NULL, pattern, pattern_type, stringtab("eq"));

  AST_GET_CHILDREN(fun, cap, id, typeparams, params, result, partial);
  ast_t* param = ast_child(params);

  return ast_childidx(param, 1);
}

static LLVMValueRef pointer_to_fields(compile_t* c, LLVMValueRef object)
{
  // Skip the descriptor.
  size_t size = cast_checked(size_t, LLVMABISizeOfType(c->target_data, c->descriptor_ptr));
  LLVMValueRef offset = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef base = LLVMBuildPtrToInt(c->builder, object, c->intptr, "");
  LLVMValueRef result = LLVMBuildAdd(c->builder, base, offset, "");

  // Return as a c->intptr.
  return result;
}

static LLVMValueRef field_pointer(compile_t* c, LLVMValueRef base,
  LLVMValueRef offset)
{
  offset = LLVMBuildZExt(c->builder, offset, c->intptr, "");
  return LLVMBuildAdd(c->builder, base, offset, "");
}

static bool check_trait(compile_t* c, LLVMValueRef desc, ast_t* pattern_type,
  LLVMBasicBlockRef next_block)
{
  LLVMValueRef test = gendesc_istrait(c, desc, pattern_type);

  LLVMBasicBlockRef continue_block = codegen_block(c, "pattern_continue");
  LLVMBuildCondBr(c->builder, test, continue_block, next_block);
  LLVMPositionBuilderAtEnd(c->builder, continue_block);

  return true;
}

static bool check_entity(compile_t* c, LLVMValueRef desc, ast_t* pattern_type,
  LLVMBasicBlockRef next_block)
{
  gentype_t g;

  if(!gentype(c, pattern_type, &g))
    return false;

  LLVMValueRef left = LLVMBuildPtrToInt(c->builder, desc, c->intptr, "");
  LLVMValueRef right = LLVMConstPtrToInt(g.desc, c->intptr);
  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntEQ, left, right, "");

  LLVMBasicBlockRef continue_block = codegen_block(c, "pattern_continue");
  LLVMBuildCondBr(c->builder, test, continue_block, next_block);
  LLVMPositionBuilderAtEnd(c->builder, continue_block);

  return true;
}

static bool check_nominal(compile_t* c, LLVMValueRef desc, ast_t* pattern_type,
  LLVMBasicBlockRef next_block)
{
  ast_t* def = (ast_t*)ast_data(pattern_type);

  switch(ast_id(def))
  {
    case TK_INTERFACE:
    case TK_TRAIT:
      return check_trait(c, desc, pattern_type, next_block);

    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
      return check_entity(c, desc, pattern_type, next_block);

    default: {}
  }

  assert(0);
  return false;
}

static void check_cardinality(compile_t* c, LLVMValueRef desc, size_t size,
  LLVMBasicBlockRef next_block)
{
  LLVMValueRef field_count = gendesc_fieldcount(c, desc);
  LLVMValueRef count = LLVMConstInt(c->i32, size, false);
  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntEQ, count, field_count,
    "");

  LLVMBasicBlockRef continue_block = codegen_block(c, "pattern_continue");
  LLVMBuildCondBr(c->builder, test, continue_block, next_block);
  LLVMPositionBuilderAtEnd(c->builder, continue_block);
}

static bool check_tuple(compile_t* c, LLVMValueRef ptr, LLVMValueRef desc,
  ast_t* pattern_type, LLVMBasicBlockRef next_block)
{
  // First check cardinality.
  size_t size = ast_childcount(pattern_type);
  check_cardinality(c, desc, size, next_block);

  // If we get here, the match expression has the right cardinality.
  ast_t* pattern_child = ast_child(pattern_type);

  for(int i = 0; pattern_child != NULL; i++)
  {
    // Get the field offset and field descriptor from the tuple descriptor.
    LLVMValueRef field = gendesc_fielddesc(c, desc, i);
    LLVMValueRef offset = LLVMBuildExtractValue(c->builder, field, 0, "");
    LLVMValueRef field_desc = LLVMBuildExtractValue(c->builder, field, 1, "");
    LLVMValueRef field_ptr = field_pointer(c, ptr, offset);

    // If we have a null descriptor, load the object.
    LLVMBasicBlockRef null_block = codegen_block(c, "null_desc");
    LLVMBasicBlockRef nonnull_block = codegen_block(c, "nonnull_desc");
    LLVMBasicBlockRef continue_block = codegen_block(c, "merge_desc");
    LLVMValueRef test = LLVMBuildIsNull(c->builder, field_desc, "");
    LLVMBuildCondBr(c->builder, test, null_block, nonnull_block);

    // Load the object, load its descriptor, and continue from there.
    LLVMPositionBuilderAtEnd(c->builder, null_block);
    LLVMTypeRef ptr_type = LLVMPointerType(c->object_ptr, 0);
    LLVMValueRef object_ptr = LLVMBuildIntToPtr(c->builder, field_ptr,
      ptr_type, "");
    LLVMValueRef object = LLVMBuildLoad(c->builder, object_ptr, "");
    LLVMValueRef object_desc = gendesc_fetch(c, object);
    object_ptr = pointer_to_fields(c, object);

    if(!check_type(c, object_ptr, object_desc, pattern_child, next_block))
      return false;

    LLVMBuildBr(c->builder, continue_block);

    // Continue with the pointer and descriptor.
    LLVMPositionBuilderAtEnd(c->builder, nonnull_block);

    if(!check_type(c, field_ptr, field_desc, pattern_child, next_block))
      return false;

    LLVMBuildBr(c->builder, continue_block);

    // Merge the two branches.
    LLVMPositionBuilderAtEnd(c->builder, continue_block);
    pattern_child = ast_sibling(pattern_child);
  }

  return true;
}

static bool check_union(compile_t* c, LLVMValueRef ptr, LLVMValueRef desc,
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

    if(!check_type(c, ptr, desc, child, nomatch_block))
      return false;

    // If we do match, jump to the continue block.
    LLVMBuildBr(c->builder, continue_block);

    // Put the next union check, if there is one, in the nomatch block.
    LLVMPositionBuilderAtEnd(c->builder, nomatch_block);
    child = next_type;
  }

  // Continue codegen in the continue block, not in the next block.
  LLVMPositionBuilderAtEnd(c->builder, continue_block);
  return true;
}

static bool check_isect(compile_t* c, LLVMValueRef ptr, LLVMValueRef desc,
  ast_t* pattern_type, LLVMBasicBlockRef next_block)
{
  // We have to match all component types.
  ast_t* child = ast_child(pattern_type);

  while(child != NULL)
  {
    if(!check_type(c, ptr, desc, child, next_block))
      return false;

    child = ast_sibling(child);
  }

  return true;
}

static bool check_type(compile_t* c, LLVMValueRef ptr, LLVMValueRef desc,
  ast_t* pattern_type, LLVMBasicBlockRef next_block)
{
  switch(ast_id(pattern_type))
  {
    case TK_NOMINAL:
      // We are trying to capture the match expression as a nominal.
      return check_nominal(c, desc, pattern_type, next_block);

    case TK_TUPLETYPE:
      // We are trying to capture the match expression as a tuple.
      return check_tuple(c, ptr, desc, pattern_type, next_block);

    case TK_UNIONTYPE:
      // We are trying to capture the match expression as a union.
      return check_union(c, ptr, desc, pattern_type, next_block);

    case TK_ISECTTYPE:
      // We are trying to capture the match expression as an intersection.
      return check_isect(c, ptr, desc, pattern_type, next_block);

    case TK_ARROW:
      // We are trying to capture the match expression as a viewpoint type, so
      // try again with the right-hand side of the arrow.
      return check_type(c, ptr, desc, ast_childidx(pattern_type, 1),
        next_block);

    default: {}
  }

  assert(0);
  return false;
}

static bool check_value(compile_t* c, ast_t* pattern, ast_t* param_type,
  LLVMValueRef value, LLVMBasicBlockRef next_block)
{
  LLVMValueRef l_value = gen_expr(c, pattern);

  if(l_value == NULL)
    return false;

  gentype_t g;

  if(!gentype(c, param_type, &g))
    return false;

  LLVMValueRef r_value = gen_assign_cast(c, g.use_type, value, param_type);

  if(r_value == NULL)
    return false;

  LLVMValueRef test = gen_pattern_eq(c, pattern, r_value);

  if(test == NULL)
    return false;

  LLVMBasicBlockRef continue_block = codegen_block(c, "pattern_continue");
  LLVMBuildCondBr(c->builder, test, continue_block, next_block);
  LLVMPositionBuilderAtEnd(c->builder, continue_block);
  return true;
}

static bool dynamic_tuple_element(compile_t* c, LLVMValueRef ptr,
  LLVMValueRef desc, ast_t* pattern, LLVMBasicBlockRef next_block, int elem)
{
  // If we have a capture, generate the alloca now.
  switch(ast_id(pattern))
  {
    case TK_VAR:
    case TK_LET:
      if(gen_localdecl(c, pattern) == NULL)
        return false;
      break;

    default: {}
  }

  // Get the field offset and field descriptor from the tuple descriptor.
  LLVMValueRef field = gendesc_fielddesc(c, desc, elem);
  LLVMValueRef offset = LLVMBuildExtractValue(c->builder, field, 0, "");
  LLVMValueRef field_desc = LLVMBuildExtractValue(c->builder, field, 1, "");
  LLVMValueRef field_ptr = field_pointer(c, ptr, offset);

  // If we have a null descriptor, load the object.
  LLVMBasicBlockRef null_block = codegen_block(c, "null_desc");
  LLVMBasicBlockRef nonnull_block = codegen_block(c, "nonnull_desc");
  LLVMBasicBlockRef continue_block = codegen_block(c, "merge_desc");
  LLVMValueRef test = LLVMBuildIsNull(c->builder, field_desc, "");
  LLVMBuildCondBr(c->builder, test, null_block, nonnull_block);

  // Load the object, load its descriptor, and continue from there.
  LLVMPositionBuilderAtEnd(c->builder, null_block);
  LLVMTypeRef ptr_type = LLVMPointerType(c->object_ptr, 0);
  LLVMValueRef object_ptr = LLVMBuildIntToPtr(c->builder, field_ptr, ptr_type,
    "");
  LLVMValueRef object = LLVMBuildLoad(c->builder, object_ptr, "");
  LLVMValueRef object_desc = gendesc_fetch(c, object);

  if(!dynamic_match_object(c, object, object_desc, pattern, next_block))
    return false;

  LLVMBuildBr(c->builder, continue_block);

  // Continue with the pointer and descriptor.
  LLVMPositionBuilderAtEnd(c->builder, nonnull_block);

  if(!dynamic_match_ptr(c, field_ptr, field_desc, pattern, next_block))
    return false;

  LLVMBuildBr(c->builder, continue_block);

  // Merge the two branches.
  LLVMPositionBuilderAtEnd(c->builder, continue_block);
  return true;
}

static bool dynamic_tuple_ptr(compile_t* c, LLVMValueRef ptr, LLVMValueRef desc,
  ast_t* pattern, LLVMBasicBlockRef next_block)
{
  // First check cardinality.
  size_t size = ast_childcount(pattern);
  check_cardinality(c, desc, size, next_block);

  // If we get here, the match expression has the right cardinality.
  ast_t* pattern_child = ast_child(pattern);

  for(int i = 0; pattern_child != NULL; i++)
  {
    switch(ast_id(pattern_child))
    {
      case TK_SEQ:
      {
        // Skip over the SEQ node.
        ast_t* pattern_expr = ast_child(pattern_child);

        if(!dynamic_tuple_element(c, ptr, desc, pattern_expr, next_block, i))
          return false;
        break;
      }

      case TK_DONTCARE:
        // Ignore the element.
        break;

      default: assert(0);
    }

    pattern_child = ast_sibling(pattern_child);
  }

  return true;
}

static bool dynamic_value_ptr(compile_t* c, LLVMValueRef ptr,
  LLVMValueRef desc, ast_t* pattern, LLVMBasicBlockRef next_block)
{
  // Get the type of the right-hand side of the pattern's eq() function.
  ast_t* param_type = eq_param_type(pattern);

  // Check the runtime type. We pass a pointer to the fields because we may
  // still need to match a tuple type inside a type expression.
  if(!check_type(c, ptr, desc, param_type, next_block))
    return false;

  // We now know that ptr points to something of type pattern_type, and that
  // it isn't a boxed primitive, as that would go through the other path, ie
  // dynamic_match_object(). We also know it isn't an unboxed tuple. We can
  // load from ptr with a type based on the static type of the pattern.
  gentype_t g;

  if(!gentype(c, param_type, &g))
    return false;

  LLVMTypeRef ptr_type = LLVMPointerType(g.use_type, 0);
  ptr = LLVMBuildIntToPtr(c->builder, ptr, ptr_type, "");
  LLVMValueRef value = LLVMBuildLoad(c->builder, ptr, "");

  return check_value(c, pattern, param_type, value, next_block);
}

static bool dynamic_capture_ptr(compile_t* c, LLVMValueRef ptr,
  LLVMValueRef desc, ast_t* pattern, LLVMBasicBlockRef next_block)
{
  // Here, ptr is a pointer to a tuple field. It could be a primitive, an
  // object, or a nested tuple.
  ast_t* pattern_type = ast_type(pattern);

  // Check the runtime type. We pass a pointer to the fields because we may
  // still need to match a tuple type inside a type expression.
  if(!check_type(c, ptr, desc, pattern_type, next_block))
    return false;

  // We now know that ptr points to something of type pattern_type, and that
  // it isn't a boxed primitive or tuple, as that would go through the other
  // path, ie dynamic_match_object(). We also know it isn't an unboxed tuple.
  // We can load from ptr with a type based on the static type of the pattern.
  gentype_t g;

  if(!gentype(c, pattern_type, &g))
    return false;

  LLVMTypeRef ptr_type = LLVMPointerType(g.use_type, 0);
  ptr = LLVMBuildIntToPtr(c->builder, ptr, ptr_type, "");
  LLVMValueRef value = LLVMBuildLoad(c->builder, ptr, "");

  return gen_assign_value(c, pattern, value, pattern_type) != NULL;
}

static bool dynamic_match_ptr(compile_t* c, LLVMValueRef ptr,
  LLVMValueRef desc, ast_t* pattern, LLVMBasicBlockRef next_block)
{
  switch(ast_id(pattern))
  {
    case TK_NONE:
      return true;

    case TK_VAR:
    case TK_LET:
      // Capture the match expression (or element thereof).
      return dynamic_capture_ptr(c, ptr, desc, pattern, next_block);

    case TK_TUPLE:
    {
      // Treat a one element tuple as its component expression.
      ast_t* child = ast_child(pattern);

      if(ast_sibling(child) == NULL)
        return dynamic_match_ptr(c, ptr, desc, child, next_block);

      // Destructure the match expression (or element thereof).
      return dynamic_tuple_ptr(c, ptr, desc, pattern, next_block);
    }

    default:
      // Test the match expression (or element thereof).
      return dynamic_value_ptr(c, ptr, desc, pattern, next_block);
  }

  return true;
}

static bool dynamic_value_object(compile_t* c, LLVMValueRef object,
  LLVMValueRef desc, ast_t* pattern, LLVMBasicBlockRef next_block)
{
  // Get the type of the right-hand side of the pattern's eq() function.
  ast_t* param_type = eq_param_type(pattern);

  // Build a base pointer that skips the object header.
  LLVMValueRef ptr = pointer_to_fields(c, object);

  // Check the runtime type. We pass a pointer to the fields because we may
  // still need to match a tuple type inside a type expression.
  if(!check_type(c, ptr, desc, param_type, next_block))
    return false;

  return check_value(c, pattern, param_type, object, next_block);
}

static bool dynamic_capture_object(compile_t* c, LLVMValueRef object,
  LLVMValueRef desc, ast_t* pattern, LLVMBasicBlockRef next_block)
{
  ast_t* pattern_type = ast_type(pattern);

  // Build a base pointer that skips the object header.
  LLVMValueRef ptr = pointer_to_fields(c, object);

  // Check the runtime type. We pass a pointer to the fields because we may
  // still need to match a tuple type inside a type expression.
  if(!check_type(c, ptr, desc, pattern_type, next_block))
    return false;

  // As long as the type is correct, we can assign it, with gen_assign_value()
  // handling boxing and unboxing. We pass the type of the pattern as the type
  // of the object.
  return gen_assign_value(c, pattern, object, pattern_type) != NULL;
}

static bool dynamic_match_object(compile_t* c, LLVMValueRef object,
  LLVMValueRef desc, ast_t* pattern, LLVMBasicBlockRef next_block)
{
  switch(ast_id(pattern))
  {
    case TK_NONE:
      return true;

    case TK_VAR:
    case TK_LET:
      // Capture the match expression (or element thereof).
      return dynamic_capture_object(c, object, desc, pattern, next_block);

    case TK_TUPLE:
    {
      // Treat a one element tuple as its component expression.
      ast_t* child = ast_child(pattern);

      if(ast_sibling(child) == NULL)
        return dynamic_match_object(c, object, desc, child, next_block);

      // Build a base pointer that skips the object header.
      LLVMValueRef ptr = pointer_to_fields(c, object);

      // Destructure the match expression (or element thereof).
      return dynamic_tuple_ptr(c, ptr, desc, pattern, next_block);
    }

    default:
      // Test the match expression (or element thereof).
      return dynamic_value_object(c, object, desc, pattern, next_block);
  }

  return true;
}

static bool static_tuple_from_tuple(compile_t* c, LLVMValueRef value,
  ast_t* type, ast_t* pattern, LLVMBasicBlockRef next_block)
{
  // The match expression is a tuple. The type checker will have made sure
  // it's the right cardinality and each element has a useful type relation
  // with the pattern.
  assert(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMStructTypeKind);

  // We have a static type.
  ast_t* type_child = ast_child(type);
  ast_t* pattern_child = ast_child(pattern);

  // Destructure the tuple and continue pattern matching on each element.
  for(int i = 0; pattern_child != NULL; i++)
  {
    switch(ast_id(pattern_child))
    {
      case TK_SEQ:
      {
        // Skip over the SEQ node.
        ast_t* pattern_expr = ast_child(pattern_child);
        LLVMValueRef elem = LLVMBuildExtractValue(c->builder, value, i, "");

        if(!static_match(c, elem, type_child, pattern_expr, next_block))
          return false;

        break;
      }

      case TK_DONTCARE:
        // Ignore the element.
        break;

      default: assert(0);
    }

    type_child = ast_sibling(type_child);
    pattern_child = ast_sibling(pattern_child);
  }

  assert(type_child == NULL);
  return true;
}

static bool static_tuple(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern, LLVMBasicBlockRef next_block)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      // Read the dynamic type and get a base pointer.
      LLVMValueRef desc = gendesc_fetch(c, value);
      LLVMValueRef ptr = pointer_to_fields(c, value);
      return dynamic_tuple_ptr(c, ptr, desc, pattern, next_block);
    }

    case TK_TUPLETYPE:
      return static_tuple_from_tuple(c, value, type, pattern, next_block);

    case TK_ARROW:
      return static_tuple(c, value, ast_childidx(type, 1), pattern,
        next_block);

    default: {}
  }

  // Can't match.
  LLVMBuildBr(c->builder, next_block);
  return true;
}

static bool static_value(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern, LLVMBasicBlockRef next_block)
{
  // Get the type of the right-hand side of the pattern's eq() function.
  ast_t* param_type = eq_param_type(pattern);

  if(!is_subtype(type, param_type))
  {
    // We should have an object_ptr. Anything else should have been rejected
    // by the type checker. If we don't, it's a reified generic that could have
    // matched in the type checker, but not now.
    if(LLVMTypeOf(value) != c->object_ptr)
    {
      LLVMBasicBlockRef continue_block = codegen_block(c, "pattern_continue");
      LLVMValueRef zero = LLVMConstInt(c->i1, 0, false);
      LLVMBuildCondBr(c->builder, zero, continue_block, next_block);
      LLVMPositionBuilderAtEnd(c->builder, continue_block);
      return true;
    }

    // Switch to dynamic value checking.
    LLVMValueRef desc = gendesc_fetch(c, value);
    return dynamic_value_object(c, value, desc, pattern, next_block);
  }

  return check_value(c, pattern, param_type, value, next_block);
}

static bool static_capture(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern, LLVMBasicBlockRef next_block)
{
  // The pattern is a capture. Make sure we are the right type, then assign.
  ast_t* pattern_type = ast_type(pattern);

  // Generate the alloca
  if(gen_localdecl(c, pattern) == NULL)
    return false;

  if(!is_subtype(type, pattern_type))
  {
    // We should have an object_ptr. Anything else should have been rejected
    // by the type checker. If we don't, it's a reified generic that could have
    // matched in the type checker, but not now.
    if(LLVMTypeOf(value) != c->object_ptr)
    {
      LLVMBasicBlockRef continue_block = codegen_block(c, "pattern_continue");
      LLVMValueRef zero = LLVMConstInt(c->i1, 0, false);
      LLVMBuildCondBr(c->builder, zero, continue_block, next_block);
      LLVMPositionBuilderAtEnd(c->builder, continue_block);
      return true;
    }

    // Switch to dynamic capture.
    LLVMValueRef desc = gendesc_fetch(c, value);
    return dynamic_capture_object(c, value, desc, pattern, next_block);
  }

  return gen_assign_value(c, pattern, value, type) != NULL;
}

static bool static_match(compile_t* c, LLVMValueRef value, ast_t* type,
  ast_t* pattern, LLVMBasicBlockRef next_block)
{
  switch(ast_id(pattern))
  {
    case TK_NONE:
      return true;

    case TK_VAR:
    case TK_LET:
      // Capture the match expression (or element thereof).
      return static_capture(c, value, type, pattern, next_block);

    case TK_TUPLE:
    {
      // Treat a one element tuple as its component expression.
      ast_t* child = ast_child(pattern);

      if(ast_sibling(child) == NULL)
      {
        // Pass on the element in the seq instead of the seq.
        assert(ast_id(child) == TK_SEQ);
        child = ast_child(child);
        return static_match(c, value, type, child, next_block);
      }

      // Destructure the match expression (or element thereof).
      return static_tuple(c, value, type, pattern, next_block);
    }

    default:
      // Test the match expression (or element thereof).
      return static_value(c, value, type, pattern, next_block);
  }

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

  if(is_result_needed(body))
  {
    ast_t* body_type = ast_type(body);
    body_value = gen_assign_cast(c, phi_type, body_value, body_type);

    if(body_value == NULL)
      return false;

    LLVMBasicBlockRef block = LLVMGetInsertBlock(c->builder);
    LLVMAddIncoming(phi, &body_value, &block, 1);
  }

  LLVMBuildBr(c->builder, post_block);
  return true;
}

LLVMValueRef gen_match(compile_t* c, ast_t* ast)
{
  bool needed = is_result_needed(ast);
  ast_t* type = ast_type(ast);
  AST_GET_CHILDREN(ast, match_expr, cases, else_expr);

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
  LLVMValueRef phi;

  if(needed)
    phi = LLVMBuildPhi(c->builder, phi_type.use_type, "");
  else
    phi = GEN_NOTNEEDED;

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

    if(!static_match(c, match_value, match_type, pattern, next_block))
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
