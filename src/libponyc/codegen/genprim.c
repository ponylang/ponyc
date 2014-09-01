#include "genprim.h"
#include "genname.h"
#include "gentype.h"
#include "gendesc.h"
#include "genfun.h"
#include "gencall.h"
#include <string.h>

/**
 * This returns the primitive type rather than the boxed type. As a result, all
 * of the methods will take the primitive type as the receiver.
 */
static LLVMTypeRef make_primitive(compile_t* c, ast_t* ast, LLVMTypeRef l_type,
  bool prelim)
{
  // Return the preliminary type
  if(prelim)
    return l_type;

  const char* name = genname_type(ast);
  LLVMTypeRef type = LLVMGetTypeByName(c->module, name);

  if(type != NULL)
    return l_type;

  gendesc_prep(c, ast);
  type = LLVMStructCreateNamed(LLVMGetGlobalContext(), name);

  // Element 0 is the type descriptor, element 1 is the boxed primitive type.
  LLVMTypeRef elements[2];
  elements[0] = c->descriptor_ptr;
  elements[1] = l_type;
  LLVMStructSetBody(type, elements, 2, false);

  if(!genfun_methods(c, ast))
    return NULL;

  gendesc_init(c, ast, true);

  // No trace function is needed.
  return l_type;
}

static LLVMTypeRef make_pointer(compile_t* c, ast_t* ast, bool prelim)
{
  ast_t* typeargs = ast_childidx(ast, 2);
  ast_t* typearg = ast_child(typeargs);
  LLVMTypeRef elem_type;

  if(prelim)
    elem_type = gentype_prelim(c, typearg);
  else
    elem_type = gentype(c, typearg);

  if(elem_type == NULL)
    return NULL;

  LLVMTypeRef type = LLVMPointerType(elem_type, 0);

  if(prelim)
    return type;

  const char* type_name = genname_type(ast);

  // set up a constant integer for the allocation size
  size_t size = LLVMABISizeOfType(c->target, elem_type);
  LLVMValueRef l_size = LLVMConstInt(LLVMInt64Type(), size, false);

  // create
  const char* name = genname_fun(type_name, "create", NULL);
  LLVMValueRef fun = LLVMGetNamedFunction(c->module, name);

  // if there's already a create function, we're done
  if(fun != NULL)
    return type;

  LLVMTypeRef params[3];
  params[0] = LLVMInt64Type();

  LLVMTypeRef ftype = LLVMFunctionType(type, params, 1, false);
  fun = LLVMAddFunction(c->module, name, ftype);
  LLVMBasicBlockRef block = LLVMAppendBasicBlock(fun, "entry");
  LLVMPositionBuilderAtEnd(c->builder, block);

  LLVMValueRef len = LLVMGetParam(fun, 0);

  LLVMValueRef args[2];
  args[0] = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef result = gencall_runtime(c, "pony_alloc", args, 1, "");
  result = LLVMBuildBitCast(c->builder, result, type, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c, fun);

  // from
  name = genname_fun(type_name, "from", NULL);

  params[0] = type;
  params[1] = LLVMInt64Type();

  ftype = LLVMFunctionType(type, params, 2, false);
  fun = LLVMAddFunction(c->module, name, ftype);
  block = LLVMAppendBasicBlock(fun, "entry");
  LLVMPositionBuilderAtEnd(c->builder, block);

  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  args[0] = LLVMBuildBitCast(c->builder, ptr, c->void_ptr, "");

  len = LLVMGetParam(fun, 1);
  args[1] = LLVMBuildMul(c->builder, len, l_size, "");

  result = gencall_runtime(c, "pony_realloc", args, 2, "");
  result = LLVMBuildBitCast(c->builder, result, type, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c, fun);

  // apply
  name = genname_fun(type_name, "apply", NULL);

  params[0] = type;
  params[1] = LLVMInt64Type();

  ftype = LLVMFunctionType(elem_type, params, 2, false);
  fun = LLVMAddFunction(c->module, name, ftype);
  block = LLVMAppendBasicBlock(fun, "entry");
  LLVMPositionBuilderAtEnd(c->builder, block);

  ptr = LLVMGetParam(fun, 0);
  LLVMValueRef index = LLVMGetParam(fun, 1);
  LLVMValueRef loc = LLVMBuildGEP(c->builder, ptr, &index, 1, "");
  result = LLVMBuildLoad(c->builder, loc, "");
  result = LLVMBuildBitCast(c->builder, result, elem_type, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c, fun);

  // update
  name = genname_fun(type_name, "update", NULL);

  params[0] = type;
  params[1] = LLVMInt64Type();
  params[2] = elem_type;

  ftype = LLVMFunctionType(elem_type, params, 3, false);
  fun = LLVMAddFunction(c->module, name, ftype);
  block = LLVMAppendBasicBlock(fun, "entry");
  LLVMPositionBuilderAtEnd(c->builder, block);

  ptr = LLVMGetParam(fun, 0);
  index = LLVMGetParam(fun, 1);
  loc = LLVMBuildGEP(c->builder, ptr, &index, 1, "");
  result = LLVMBuildLoad(c->builder, loc, "");
  result = LLVMBuildBitCast(c->builder, result, elem_type, "");
  LLVMBuildStore(c->builder, LLVMGetParam(fun, 2), loc);

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c, fun);

  return type;
}

static LLVMTypeRef make_array(compile_t* c, ast_t* ast, bool prelim)
{
  ast_t* typeargs = ast_childidx(ast, 2);
  ast_t* typearg = ast_child(typeargs);
  LLVMTypeRef elem_type = gentype(c, typearg);

  if(elem_type == NULL)
    return NULL;

  // if we've already generated this, we're done
  const char* name = genname_type(ast);
  LLVMTypeRef type = LLVMGetTypeByName(c->module, name);

  if(type != NULL)
    return LLVMPointerType(type, 0);

  gendesc_prep(c, ast);

  // TODO: have to generate the _Pointer type, handle preliminary vs final gen

  type = LLVMStructCreateNamed(LLVMGetGlobalContext(), name);
  LLVMTypeRef elements[4];
  elements[0] = c->descriptor_ptr; // descriptor
  elements[1] = LLVMInt64Type(); // allocated size
  elements[2] = LLVMInt64Type(); // current length
  elements[3] = LLVMPointerType(elem_type, 0); // pointer to elements
  LLVMStructSetBody(type, elements, 4, false);

  // trace function
  const char* trace_name = genname_trace(name);
  LLVMValueRef trace_fn = LLVMAddFunction(c->module, trace_name, c->trace_type);

  LLVMValueRef arg = LLVMGetParam(trace_fn, 0);
  LLVMSetValueName(arg, "arg");

  LLVMBasicBlockRef entry_block = LLVMAppendBasicBlock(trace_fn, "entry");
  LLVMBasicBlockRef cond_block = LLVMAppendBasicBlock(trace_fn, "cond");
  LLVMBasicBlockRef body_block = LLVMAppendBasicBlock(trace_fn, "body");
  LLVMBasicBlockRef post_block = LLVMAppendBasicBlock(trace_fn, "post");

  LLVMPositionBuilderAtEnd(c->builder, entry_block);
  LLVMTypeRef type_ptr = LLVMPointerType(type, 0);
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, type_ptr, "array");
  LLVMValueRef count_ptr = LLVMBuildStructGEP(c->builder, object, 2, "");
  LLVMValueRef count = LLVMBuildLoad(c->builder, count_ptr, "count");
  LLVMValueRef pointer_ptr = LLVMBuildStructGEP(c->builder, object, 3, "");
  LLVMValueRef pointer = LLVMBuildLoad(c->builder, pointer_ptr, "pointer");
  pointer = LLVMBuildBitCast(c->builder, pointer, c->void_ptr, "");
  gencall_runtime(c, "pony_trace", &pointer, 1, "");
  LLVMBuildBr(c->builder, cond_block);

  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, LLVMInt64Type(), "");
  LLVMValueRef zero = LLVMConstInt(LLVMInt64Type(), 0, false);
  LLVMAddIncoming(phi, &zero, &entry_block, 1);
  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntULT, phi, count, "");
  LLVMBuildCondBr(c->builder, test, body_block, post_block);

  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef elem = LLVMBuildGEP(c->builder, pointer, &phi, 1, "elem");
  gencall_trace(c, elem, typearg);

  LLVMValueRef one = LLVMConstInt(LLVMInt64Type(), 1, false);
  LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
  LLVMAddIncoming(phi, &inc, &body_block, 1);
  LLVMBuildBr(c->builder, cond_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMBuildRetVoid(c->builder);

  if(!codegen_finishfun(c, trace_fn))
    return NULL;

  if(!genfun_methods(c, ast))
    return NULL;

  gendesc_init(c, ast, false);
  return type;
}

LLVMTypeRef genprim(compile_t* c, ast_t* ast, bool prelim)
{
  // Check for primitive types.
  const char* name = ast_name(ast_childidx(ast, 1));

  if(!strcmp(name, "True") || !strcmp(name, "False"))
    return make_primitive(c, ast, LLVMInt1Type(), prelim);

  if(!strcmp(name, "I8") || !strcmp(name, "U8"))
    return make_primitive(c, ast, LLVMInt8Type(), prelim);

  if(!strcmp(name, "I16") || !strcmp(name, "U16"))
    return make_primitive(c, ast, LLVMInt16Type(), prelim);

  if(!strcmp(name, "I32") || !strcmp(name, "U32"))
    return make_primitive(c, ast, LLVMInt32Type(), prelim);

  if(!strcmp(name, "I64") || !strcmp(name, "U64"))
    return make_primitive(c, ast, LLVMInt64Type(), prelim);

  if(!strcmp(name, "I128") || !strcmp(name, "U128") ||
    !strcmp(name, "SIntLiteral") || !strcmp(name, "UIntLiteral")
    )
    return make_primitive(c, ast, LLVMIntType(128), prelim);

  if(!strcmp(name, "F16"))
    return make_primitive(c, ast, LLVMHalfType(), prelim);

  if(!strcmp(name, "F32"))
    return make_primitive(c, ast, LLVMFloatType(), prelim);

  if(!strcmp(name, "F64") || !strcmp(name, "FloatLiteral"))
    return make_primitive(c, ast, LLVMDoubleType(), prelim);

  if(!strcmp(name, "Array"))
    return make_array(c, ast, prelim);

  if(!strcmp(name, "_Pointer"))
    return make_pointer(c, ast, prelim);

  return GEN_NOTYPE;
}

void genprim_builtins(compile_t* c)
{
  // TODO: All the builtin functions.
}
