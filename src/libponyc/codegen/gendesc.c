#include "gendesc.h"
#include "genname.h"
#include "gentype.h"
#include <assert.h>

static LLVMValueRef make_function_ptr(compile_t* c, const char* name,
  LLVMTypeRef type)
{
  LLVMValueRef func = LLVMGetNamedFunction(c->module, name);

  if(func == NULL)
    func = LLVMConstNull(type);
  else
    func = LLVMConstBitCast(func, type);

  return func;
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

void gendesc_prep(compile_t* c, ast_t* ast, LLVMTypeRef type)
{
  assert(ast_id(ast) == TK_NOMINAL);

  ast_t* def = ast_data(ast);
  const char* name = genname_type(ast);
  const char* desc_name = genname_descriptor(name);
  size_t vtable_size = painter_get_vtable_size(c->painter, def);

  LLVMTypeRef desc_type = gendesc_type(c, desc_name, vtable_size);
  LLVMValueRef g_desc = LLVMAddGlobal(c->module, desc_type, desc_name);

  // If this is a datatype and it isn't a primitive, create a single global
  // instance with the descriptor and no fields.
  if((ast_id(def) == TK_DATA) && (LLVMGetTypeKind(type) == LLVMStructTypeKind))
  {
    const char* inst_name = genname_instance(name);
    LLVMValueRef g_inst = LLVMAddGlobal(c->module, type, inst_name);

    LLVMValueRef args[1];
    args[0] = LLVMConstBitCast(g_desc, c->descriptor_ptr);

    LLVMValueRef inst = LLVMConstNamedStruct(type, args, 1);
    LLVMSetInitializer(g_inst, inst);
    LLVMSetGlobalConstant(g_inst, true);
  }
}

void gendesc_init(compile_t* c, ast_t* ast, LLVMTypeRef type)
{
  assert(ast_id(ast) == TK_NOMINAL);
  ast_t* def = ast_data(ast);
  const char* name = genname_type(ast);
  const char* desc_name = genname_descriptor(name);
  size_t vtable_size = painter_get_vtable_size(c->painter, def);

  // build the actual vtable
  LLVMValueRef vtable[vtable_size];

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
  args[7] = LLVMConstArray(c->void_ptr, vtable, vtable_size);;

  LLVMValueRef desc = LLVMConstNamedStruct(desc_type, args, 8);
  LLVMValueRef g_desc = LLVMGetNamedGlobal(c->module, desc_name);
  LLVMSetInitializer(g_desc, desc);
  LLVMSetGlobalConstant(g_desc, true);
}
