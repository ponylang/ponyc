#include "gendesc.h"
#include "genname.h"
#include "gentype.h"
#include "../type/reify.h"
#include <assert.h>

static LLVMValueRef make_unbox_function(compile_t* c, gentype_t* g,
  const char* name)
{
  LLVMValueRef fun = LLVMGetNamedFunction(c->module, name);

  if(fun == NULL)
    return LLVMConstNull(c->void_ptr);

  // Create a new unboxing function that forwards to the real function.
  LLVMTypeRef f_type = LLVMGetElementType(LLVMTypeOf(fun));
  int count = LLVMCountParamTypes(f_type);

  VLA(LLVMTypeRef, params, count);
  LLVMGetParamTypes(f_type, params);
  LLVMTypeRef ret_type = LLVMGetReturnType(f_type);

  // It's the same type, but it takes the boxed type instead of the primitive
  // type as the receiver.
  params[0] = g->structure_ptr;

  const char* unbox_name = genname_unbox(name);
  LLVMTypeRef unbox_type = LLVMFunctionType(ret_type, params, count, false);
  LLVMValueRef unbox_fun = codegen_addfun(c, unbox_name, unbox_type);
  codegen_startfun(c, unbox_fun);

  // Extract the primitive type from element 1 and call the real function.
  LLVMValueRef this_ptr = LLVMGetParam(unbox_fun, 0);
  LLVMValueRef primitive_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 1, "");
  LLVMValueRef primitive = LLVMBuildLoad(c->builder, primitive_ptr, "");

  VLA(LLVMValueRef, args, count);
  args[0] = primitive;

  for(int i = 1; i < count; i++)
    args[i] = LLVMGetParam(unbox_fun, i);

  LLVMValueRef result = LLVMBuildCall(c->builder, fun, args, count, "");
  LLVMSetInstructionCallConv(result, LLVMFastCallConv);

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

static LLVMValueRef make_type_id(compile_t* c, const char* type_name)
{
  // Generate a named constant for the type that is set to a unique integer
  // value for that type.
  const char* name = genname_typeid(type_name);
  LLVMValueRef global = LLVMGetNamedGlobal(c->module, name);

  // Return the constant initialiser, not the global.
  if(global != NULL)
    return LLVMGetInitializer(global);

  global = LLVMAddGlobal(c->module, c->i32, name);
  LLVMSetGlobalConstant(global, true);

  LLVMValueRef id = LLVMConstInt(c->i32, c->next_type_id++, false);
  LLVMSetInitializer(global, id);

  return id;
}

static int trait_count(gentype_t* g)
{
  switch(g->underlying)
  {
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
    {
      ast_t* def = (ast_t*)ast_data(g->ast);
      ast_t* traits = ast_childidx(def, 3);
      return ast_childcount(traits);
    }

    default: {}
  }

  return 0;
}

static LLVMValueRef make_trait_count(compile_t* c, gentype_t* g)
{
  return LLVMConstInt(c->i32, trait_count(g), false);
}

static LLVMValueRef make_trait_list(compile_t* c, gentype_t* g)
{
  // The list is an array of integers.
  int count = trait_count(g);
  LLVMTypeRef type = LLVMArrayType(c->i32, count);

  // If we have no traits, return a null pointer to a list.
  if(count == 0)
    return LLVMConstNull(LLVMPointerType(type, 0));

  // Create a constant array of trait identifiers.
  VLA(LLVMValueRef, list, count);

  ast_t* def = (ast_t*)ast_data(g->ast);
  ast_t* typeargs = ast_childidx(g->ast, 2);
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* traits = ast_childidx(def, 3);
  ast_t* trait = ast_child(traits);
  int i = 0;

  while(trait != NULL)
  {
    ast_t* r_trait = reify(trait, typeparams, typeargs);
    list[i++] = gendesc_typeid(c, r_trait);
    ast_free_unattached(r_trait);

    trait = ast_sibling(trait);
  }

  LLVMValueRef trait_array = LLVMConstArray(c->i32, list, count);

  // Create a global to hold the array.
  const char* name = genname_traitlist(g->type_name);
  LLVMValueRef global = LLVMAddGlobal(c->module, type, name);
  LLVMSetGlobalConstant(global, true);
  LLVMSetInitializer(global, trait_array);

  return global;
}

static LLVMValueRef make_vtable(compile_t* c, gentype_t* g)
{
  if(g->vtable_size == 0)
    return LLVMConstArray(c->void_ptr, NULL, 0);

  VLA(LLVMValueRef, vtable, g->vtable_size);

  for(int i = 0; i < g->vtable_size; i++)
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
          vtable[colour] = make_unbox_function(c, g, fullname);
        else
          vtable[colour] = make_function_ptr(c, fullname, c->void_ptr);

        assert(vtable[colour] != NULL);
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  return LLVMConstArray(c->void_ptr, vtable, g->vtable_size);
}

LLVMTypeRef gendesc_type(compile_t* c, gentype_t* g)
{
  const char* desc_name;
  int count;
  int vtable_size;

  if(g != NULL)
  {
    desc_name = g->desc_name;
    count = trait_count(g);
    vtable_size = g->vtable_size;
  } else {
    desc_name = genname_descriptor(NULL);
    count = 0;
    vtable_size = 0;
  }

  LLVMTypeRef type = LLVMGetTypeByName(c->module, desc_name);

  if(type != NULL)
    return type;

  type = LLVMStructCreateNamed(c->context, desc_name);

  LLVMTypeRef params[10];
  params[0] = c->i32; // id
  params[1] = c->i32; // size
  params[2] = c->i32; // trait count
  params[3] = c->trace_fn; // trace
  params[4] = c->trace_fn; // serialise
  params[5] = c->trace_fn; // deserialise
  params[6] = c->dispatch_fn; // dispatch
  params[7] = c->final_fn; // finalise
  params[8] = LLVMPointerType(LLVMArrayType(c->i32, count), 0); // trait list
  params[9] = LLVMArrayType(c->void_ptr, vtable_size); // vtable

  LLVMStructSetBody(type, params, 10, false);
  return type;
}

void gendesc_init(compile_t* c, gentype_t* g)
{
  // TODO: Build tuple reflector.

  // Initialise the global descriptor.
  LLVMValueRef args[10];
  uint32_t size = (uint32_t)LLVMABISizeOfType(c->target_data, g->structure);

  // Generate a separate type ID for every type.
  args[0] = make_type_id(c, g->type_name);
  args[1] = LLVMConstInt(c->i32, size, false);
  args[2] = make_trait_count(c, g);
  args[3] = make_function_ptr(c, genname_trace(g->type_name), c->trace_fn);
  args[4] = make_function_ptr(c, genname_serialise(g->type_name), c->trace_fn);
  args[5] = make_function_ptr(c, genname_deserialise(g->type_name),
    c->trace_fn);
  args[6] = make_function_ptr(c, genname_dispatch(g->type_name),
    c->dispatch_fn);
  args[7] = make_function_ptr(c, genname_finalise(g->type_name), c->final_fn);
  args[8] = make_trait_list(c, g);
  args[9] = make_vtable(c, g);

  LLVMValueRef desc = LLVMConstNamedStruct(g->desc_type, args, 10);
  LLVMSetInitializer(g->desc, desc);
  LLVMSetGlobalConstant(g->desc, true);
}

static LLVMValueRef desc_field(compile_t* c, LLVMValueRef object, int index)
{
  LLVMValueRef desc = gendesc_fetch(c, object);
  LLVMValueRef ptr = LLVMBuildStructGEP(c->builder, desc, index, "");
  return LLVMBuildLoad(c->builder, ptr, "");
}

LLVMValueRef gendesc_fetch(compile_t* c, LLVMValueRef object)
{
  LLVMValueRef ptr = LLVMBuildStructGEP(c->builder, object, 0, "");
  return LLVMBuildLoad(c->builder, ptr, "");
}

LLVMValueRef gendesc_trace(compile_t* c, LLVMValueRef object)
{
  return desc_field(c, object, 3);
}

LLVMValueRef gendesc_dispatch(compile_t* c, LLVMValueRef object)
{
  return desc_field(c, object, 6);
}

LLVMValueRef gendesc_traitcount(compile_t* c, LLVMValueRef object)
{
  return desc_field(c, object, 2);
}

LLVMValueRef gendesc_vtable(compile_t* c, LLVMValueRef object, int colour)
{
  LLVMValueRef desc = gendesc_fetch(c, object);
  LLVMValueRef vtable = LLVMBuildStructGEP(c->builder, desc, 9, "");

  LLVMValueRef gep[2];
  gep[0] = LLVMConstInt(c->i32, 0, false);
  gep[1] = LLVMConstInt(c->i32, colour, false);

  LLVMValueRef func_ptr = LLVMBuildGEP(c->builder, vtable, gep, 2, "");
  return LLVMBuildLoad(c->builder, func_ptr, "");
}

LLVMValueRef gendesc_typeid(compile_t* c, ast_t* type)
{
  return make_type_id(c, genname_type(type));
}

LLVMValueRef gendesc_isentity(compile_t* c, LLVMValueRef object, ast_t* type)
{
  gentype_t g;

  if(!gentype(c, type, &g))
    return NULL;

  LLVMValueRef desc = gendesc_fetch(c, object);
  desc = LLVMBuildPtrToInt(c->builder, desc, c->intptr, "");
  LLVMValueRef desc2 = LLVMConstPtrToInt(g.desc, c->intptr);

  return LLVMBuildICmp(c->builder, LLVMIntEQ, desc, desc2, "");
}

LLVMValueRef gendesc_istrait(compile_t* c, LLVMValueRef object, ast_t* type)
{
  // Get the trait identifier.
  LLVMValueRef trait_id = gendesc_typeid(c, type);

  // Read the count and the trait list from the descriptor.
  LLVMValueRef desc = gendesc_fetch(c, object);
  LLVMValueRef count_ptr = LLVMBuildStructGEP(c->builder, desc, 2, "");
  LLVMValueRef count = LLVMBuildLoad(c->builder, count_ptr, "");
  LLVMValueRef list_ptr = LLVMBuildStructGEP(c->builder, desc, 8, "");
  LLVMValueRef list = LLVMBuildLoad(c->builder, list_ptr, "");

  LLVMBasicBlockRef entry_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef cond_block = codegen_block(c, "cond");
  LLVMBasicBlockRef body_block = codegen_block(c, "body");
  LLVMBasicBlockRef post_block = codegen_block(c, "post");
  LLVMBuildBr(c->builder, cond_block);

  // While the index is less than the count, check an ID.
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->i32, "");
  LLVMValueRef zero = LLVMConstInt(c->i32, 0, false);
  LLVMAddIncoming(phi, &zero, &entry_block, 1);

  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntULT, phi, count, "");
  LLVMBuildCondBr(c->builder, test, body_block, post_block);

  // The phi node is the index. Get ID and compare it.
  LLVMPositionBuilderAtEnd(c->builder, body_block);

  LLVMValueRef gep[2];
  gep[0] = LLVMConstInt(c->i32, 0, false);
  gep[1] = phi;

  LLVMValueRef id_ptr = LLVMBuildGEP(c->builder, list, gep, 2, "");
  LLVMValueRef id = LLVMBuildLoad(c->builder, id_ptr, "");
  LLVMValueRef test_id = LLVMBuildICmp(c->builder, LLVMIntEQ, id, trait_id, "");

  // Add one to the phi node.
  LLVMValueRef one = LLVMConstInt(c->i32, 1, false);
  LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
  LLVMAddIncoming(phi, &inc, &body_block, 1);

  // Either to the post block or back to the condition.
  LLVMBuildCondBr(c->builder, test_id, post_block, cond_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef result = LLVMBuildPhi(c->builder, c->i1, "");
  LLVMAddIncoming(result, &test, &cond_block, 1);
  LLVMAddIncoming(result, &test_id, &body_block, 1);

  return result;
}
