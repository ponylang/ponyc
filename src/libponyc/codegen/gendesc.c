#include "gendesc.h"
#include "genname.h"
#include "gentype.h"
#include <assert.h>

static LLVMValueRef make_unbox_function(compile_t* c, const char* name,
  LLVMTypeRef type)
{
  LLVMValueRef fun = LLVMGetNamedFunction(c->module, name);

  if(fun == NULL)
    return LLVMConstNull(c->void_ptr);

  // Create a new unboxing function that forwards to the real function.
  LLVMTypeRef f_type = LLVMGetElementType(LLVMTypeOf(fun));
  size_t count = LLVMCountParamTypes(f_type);

  LLVMTypeRef params[count];
  LLVMGetParamTypes(f_type, params);
  LLVMTypeRef ret_type = LLVMGetReturnType(f_type);

  // It's the same type, but it takes the boxed type instead of the primitive
  // type as the receiver.
  params[0] = type;

  const char* unbox_name = genname_unbox(name);
  LLVMTypeRef unbox_type = LLVMFunctionType(ret_type, params, count, false);
  LLVMValueRef unbox_fun = LLVMAddFunction(c->module, unbox_name, unbox_type);

  // Extract the primitive type from element 1 and call the real function.
  LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef entry_block = LLVMAppendBasicBlock(unbox_fun, "entry");
  LLVMPositionBuilderAtEnd(c->builder, entry_block);

  LLVMValueRef this_ptr = LLVMGetParam(unbox_fun, 0);
  LLVMValueRef primitive_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 1, "");
  LLVMValueRef primitive = LLVMBuildLoad(c->builder, primitive_ptr, "");

  LLVMValueRef args[count];
  args[0] = primitive;

  for(size_t i = 1; i < count; i++)
    args[i] = LLVMGetParam(unbox_fun, i);

  LLVMValueRef result = LLVMBuildCall(c->builder, fun, args, count, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c, unbox_fun);

  // Put the insert cursor back where it was.
  LLVMPositionBuilderAtEnd(c->builder, insert_block);

  return LLVMConstBitCast(unbox_fun, c->void_ptr);
}

static LLVMValueRef make_function_ptr(compile_t* c, const char* name,
  LLVMTypeRef type)
{
  LLVMValueRef fun = LLVMGetNamedFunction(c->module, name);

  if(fun == NULL)
    return LLVMConstNull(type);

  return LLVMConstBitCast(fun, type);
}

LLVMTypeRef gendesc_type(compile_t* c, const char* desc_name, int vtable_size)
{
  LLVMTypeRef type = LLVMGetTypeByName(c->module, desc_name);

  if(type != NULL)
    return type;

  type = LLVMStructCreateNamed(LLVMGetGlobalContext(), desc_name);

  LLVMTypeRef params[8];
  params[0] = c->trace_fn; // trace
  params[1] = c->trace_fn; // serialise
  params[2] = c->trace_fn; // deserialise
  params[3] = c->dispatch_fn; // dispatch
  params[4] = c->final_fn; // finalise
  params[5] = LLVMInt64Type(); // size
  params[6] = c->void_ptr; // trait list
  params[7] = LLVMArrayType(c->void_ptr, vtable_size); // vtable

  LLVMStructSetBody(type, params, 8, false);
  return type;
}

void gendesc_prep(compile_t* c, ast_t* ast)
{
  assert(ast_id(ast) == TK_NOMINAL);

  ast_t* def = ast_data(ast);
  const char* name = genname_type(ast);
  const char* desc_name = genname_descriptor(name);
  size_t vtable_size = painter_get_vtable_size(c->painter, def);

  LLVMTypeRef desc_type = gendesc_type(c, desc_name, vtable_size);
  LLVMAddGlobal(c->module, desc_type, desc_name);
}

void gendesc_inst(compile_t* c, ast_t* ast)
{
  // If it's not a datatype, we don't need an instance.
  ast_t* def = ast_data(ast);

  if(ast_id(def) != TK_DATA)
    return;

  const char* name = genname_type(ast);
  LLVMTypeRef type = LLVMGetTypeByName(c->module, name);

  // If it's a primitive, we don't need an instance.
  if(LLVMGetTypeKind(type) != LLVMStructTypeKind)
    return;

  const char* inst_name = genname_instance(name);
  LLVMValueRef g_inst = LLVMAddGlobal(c->module, type, inst_name);

  const char* desc_name = genname_descriptor(name);
  LLVMValueRef g_desc = LLVMGetNamedGlobal(c->module, desc_name);

  LLVMValueRef args[1];
  args[0] = LLVMConstBitCast(g_desc, c->descriptor_ptr);

  LLVMValueRef inst = LLVMConstNamedStruct(type, args, 1);
  LLVMSetInitializer(g_inst, inst);
  LLVMSetGlobalConstant(g_inst, true);
}

void gendesc_init(compile_t* c, ast_t* ast, bool unbox)
{
  assert(ast_id(ast) == TK_NOMINAL);
  ast_t* def = ast_data(ast);

  const char* name = genname_type(ast);
  LLVMTypeRef type = LLVMGetTypeByName(c->module, name);
  LLVMTypeRef type_ptr = LLVMPointerType(type, 0);

  const char* desc_name = genname_descriptor(name);
  size_t vtable_size = painter_get_vtable_size(c->painter, def);

  // build the actual vtable
  LLVMValueRef vtable[vtable_size];

  for(size_t i = 0; i < vtable_size; i++)
    vtable[i] = LLVMConstNull(c->void_ptr);

  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_BE:
      case TK_FUN:
      {
        ast_t* id = ast_childidx(member, 1);
        const char* funname = ast_name(id);
        const char* fullname = genname_fun(name, funname, NULL);
        int colour = painter_get_colour(c->painter, funname);

        if(unbox)
          vtable[colour] = make_unbox_function(c, fullname, type_ptr);
        else
          vtable[colour] = make_function_ptr(c, fullname, c->void_ptr);

        assert(vtable[colour] != NULL);
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  LLVMTypeRef desc_type = gendesc_type(c, desc_name, vtable_size);

  // TODO: trait list
  LLVMValueRef args[8];
  args[0] = make_function_ptr(c, genname_trace(name), c->trace_fn);
  args[1] = make_function_ptr(c, genname_serialise(name), c->trace_fn);
  args[2] = make_function_ptr(c, genname_deserialise(name), c->trace_fn);
  args[3] = make_function_ptr(c, genname_dispatch(name), c->dispatch_fn);
  args[4] = make_function_ptr(c, genname_finalise(name), c->trace_fn);
  args[5] = LLVMConstInt(LLVMInt64Type(), LLVMABISizeOfType(c->target, type),
    false);
  args[6] = LLVMConstNull(c->void_ptr);
  args[7] = LLVMConstArray(c->void_ptr, vtable, vtable_size);

  LLVMValueRef desc = LLVMConstNamedStruct(desc_type, args, 8);
  LLVMValueRef g_desc = LLVMGetNamedGlobal(c->module, desc_name);
  LLVMSetInitializer(g_desc, desc);
  LLVMSetGlobalConstant(g_desc, true);
}
