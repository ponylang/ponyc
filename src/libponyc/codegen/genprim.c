#include "genprim.h"
#include "genname.h"
#include "gentype.h"
#include "gendesc.h"
#include "genfun.h"
#include "gencall.h"
#include <string.h>

LLVMTypeRef genprim_array(compile_t* c, ast_t* ast)
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

  type = LLVMStructCreateNamed(LLVMGetGlobalContext(), name);
  LLVMTypeRef elements[4];
  elements[0] = c->descriptor_ptr; // descriptor
  elements[1] = LLVMInt64Type(); // allocated size
  elements[2] = LLVMInt64Type(); // current length
  elements[3] = LLVMArrayType(elem_type, 0); // pointer to elements
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
  LLVMValueRef count = LLVMBuildExtractValue(c->builder, object, 2, "count");
  LLVMValueRef pointer_ptr = LLVMBuildStructGEP(c->builder, object, 3, "");
  LLVMValueRef pointer = LLVMBuildLoad(c->builder, pointer_ptr, "pointer");
  gencall_runtime(c, "pony_trace", &pointer, 1, "");
  LLVMBuildBr(c->builder, cond_block);

  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, LLVMInt64Type(), "");
  LLVMValueRef zero = LLVMConstInt(LLVMInt64Type(), 0, false);
  LLVMAddIncoming(phi, &zero, &entry_block, 1);
  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntULT, phi, count, "");
  LLVMBuildCondBr(c->builder, test, body_block, post_block);

  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef index[2];
  index[0] = zero;
  index[1] = phi;
  LLVMValueRef elem = LLVMBuildGEP(c->builder, pointer, index, 2, "elem");
  gencall_trace(c, elem, typearg);

  LLVMValueRef one = LLVMConstInt(LLVMInt64Type(), 1, false);
  LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
  LLVMAddIncoming(phi, &inc, &body_block, 1);
  LLVMBuildBr(c->builder, cond_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMBuildRetVoid(c->builder);

  if(!codegen_finishfun(c, trace_fn))
    return NULL;

  gendesc_prep(c, ast, type);

  if(!genfun_methods(c, ast))
    return NULL;

  gendesc_init(c, ast, type);
  return type;
}

LLVMTypeRef genprim(compile_t* c, ast_t* ast)
{
  // TODO: create the primitive descriptors
  // check for primitive types
  const char* name = ast_name(ast_childidx(ast, 1));

  if(!strcmp(name, "True") || !strcmp(name, "False"))
    return LLVMInt1Type();

  if(!strcmp(name, "I8") || !strcmp(name, "U8"))
    return LLVMInt8Type();

  if(!strcmp(name, "I16") || !strcmp(name, "U16"))
    return LLVMInt16Type();

  if(!strcmp(name, "I32") || !strcmp(name, "U32"))
    return LLVMInt32Type();

  if(!strcmp(name, "I64") || !strcmp(name, "U64"))
    return LLVMInt64Type();

  if(!strcmp(name, "I128") || !strcmp(name, "U128") ||
    !strcmp(name, "SIntLiteral") || !strcmp(name, "UIntLiteral")
    )
    return LLVMIntType(128);

  if(!strcmp(name, "F16"))
    return LLVMHalfType();

  if(!strcmp(name, "F32"))
    return LLVMFloatType();

  if(!strcmp(name, "F64") || !strcmp(name, "FloatLiteral"))
    return LLVMDoubleType();

  if(!strcmp(name, "Array"))
    return genprim_array(c, ast);

  return NULL;
}
