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

  PONY_VL_ARRAY(LLVMTypeRef, params, count);
  LLVMGetParamTypes(f_type, params);
  LLVMTypeRef ret_type = LLVMGetReturnType(f_type);

  // It's the same type, but it takes the boxed type instead of the primitive
  // type as the receiver.
  params[0] = type;

  const char* unbox_name = genname_unbox(name);
  LLVMTypeRef unbox_type = LLVMFunctionType(ret_type, params, (int)count, false);
  LLVMValueRef unbox_fun = LLVMAddFunction(c->module, unbox_name, unbox_type);
  codegen_startfun(c, unbox_fun);

  // Extract the primitive type from element 1 and call the real function.
  LLVMValueRef this_ptr = LLVMGetParam(unbox_fun, 0);
  LLVMValueRef primitive_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 1, "");
  LLVMValueRef primitive = LLVMBuildLoad(c->builder, primitive_ptr, "");

  PONY_VL_ARRAY(LLVMValueRef, args, count);
  args[0] = primitive;

  for(size_t i = 1; i < count; i++)
    args[i] = LLVMGetParam(unbox_fun, (unsigned int)i);

  LLVMValueRef result = LLVMBuildCall(c->builder, fun, args, (unsigned int)count, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

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

void gendesc_init(compile_t* c, gentype_t* g)
{
  // Build the vtable.
  LLVMTypeRef type_ptr = LLVMPointerType(g->type, 0);
  PONY_VL_ARRAY(LLVMValueRef, vtable, g->vtable_size);

  for(size_t i = 0; i < g->vtable_size; i++)
    vtable[i] = LLVMConstNull(c->void_ptr);

  ast_t* def = (ast_t*)ast_data(g->ast);
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
        int colour = painter_get_colour(c->painter, funname);

        const char* fullname = genname_fun(g->type_name, funname, NULL);

        if(g->primitive != NULL)
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

  // TODO: Build the trait list.

  // Initialise the global descriptor.
  LLVMValueRef args[8];

  args[0] = make_function_ptr(c, genname_trace(g->type_name), c->trace_fn);
  args[1] = make_function_ptr(c, genname_serialise(g->type_name), c->trace_fn);
  args[2] = make_function_ptr(c, genname_deserialise(g->type_name),
    c->trace_fn);
  args[3] = make_function_ptr(c, genname_dispatch(g->type_name),
    c->dispatch_fn);
  args[4] = make_function_ptr(c, genname_finalise(g->type_name), c->trace_fn);
  args[5] = LLVMConstInt(LLVMInt64Type(), LLVMABISizeOfType(c->target, g->type),
    false);
  args[6] = LLVMConstNull(c->void_ptr);
  args[7] = LLVMConstArray(c->void_ptr, vtable, (int)g->vtable_size);

  LLVMValueRef desc = LLVMConstNamedStruct(g->desc_type, args, 8);
  LLVMSetInitializer(g->desc, desc);
  LLVMSetGlobalConstant(g->desc, true);
}
