#include "gentype.h"
#include "genname.h"
#include "gendesc.h"
#include "genprim.h"
#include "gentrace.h"
#include "genfun.h"
#include "genopt.h"
#include "../pkg/package.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include "../debug/dwarf.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void make_box_type(compile_t* c, gentype_t* g)
{
  if(g->primitive == NULL)
    return;

  if(g->structure == NULL)
  {
    const char* box_name = genname_box(g->type_name);
    g->structure = LLVMGetTypeByName(c->module, box_name);

    if(g->structure == NULL)
      g->structure = LLVMStructCreateNamed(c->context, box_name);
  }

  if(LLVMIsOpaqueStruct(g->structure))
  {
    LLVMTypeRef elements[2];
    elements[0] = LLVMPointerType(g->desc_type, 0);
    elements[1] = g->primitive;
    LLVMStructSetBody(g->structure, elements, 2, false);
  }

  g->structure_ptr = LLVMPointerType(g->structure, 0);
}

static void make_global_descriptor(compile_t* c, gentype_t* g)
{
  // Fetch or create a descriptor type.
  if(g->underlying == TK_STRUCT)
    return;

  if(g->underlying == TK_TUPLETYPE)
    g->field_count = (int)ast_childcount(g->ast);

  // Check for an existing descriptor.
  g->desc_type = gendesc_type(c, g);
  g->desc = LLVMGetNamedGlobal(c->module, g->desc_name);

  if(g->desc != NULL)
    return;

  g->desc = LLVMAddGlobal(c->module, g->desc_type, g->desc_name);
  LLVMSetGlobalConstant(g->desc, true);
  LLVMSetLinkage(g->desc, LLVMInternalLinkage);
}

static void make_global_instance(compile_t* c, gentype_t* g)
{
  // Not a primitive type.
  if(g->underlying != TK_PRIMITIVE)
    return;

  // No instance for base types.
  if(g->primitive != NULL)
    return;

  // Check for an existing instance.
  const char* inst_name = genname_instance(g->type_name);
  g->instance = LLVMGetNamedGlobal(c->module, inst_name);

  if(g->instance != NULL)
    return;

  // Create a unique global instance.
  LLVMValueRef args[1];
  args[0] = g->desc;
  LLVMValueRef value = LLVMConstNamedStruct(g->structure, args, 1);

  g->instance = LLVMAddGlobal(c->module, g->structure, inst_name);
  LLVMSetInitializer(g->instance, value);
  LLVMSetGlobalConstant(g->instance, true);
  LLVMSetLinkage(g->instance, LLVMInternalLinkage);
}

/**
 * Return true if the type already exists or if we are only being asked to
 * generate a preliminary type, false otherwise.
 */
static bool setup_name(compile_t* c, ast_t* ast, gentype_t* g, bool prelim)
{
  if(ast_id(ast) == TK_NOMINAL)
  {
    ast_t* def = (ast_t*)ast_data(ast);
    g->underlying = ast_id(def);

    // Find the primitive type, if there is one.
    AST_GET_CHILDREN(ast, pkg, id);
    const char* package = ast_name(pkg);
    const char* name = ast_name(id);

    bool ilp32 = target_is_ilp32(c->opt->triple);
    bool llp64 = target_is_llp64(c->opt->triple);
    bool lp64 = target_is_lp64(c->opt->triple);

    if(package == c->str_builtin)
    {
      if(name == c->str_Bool)
        g->primitive = c->i1;
      else if(name == c->str_I8)
        g->primitive = c->i8;
      else if(name == c->str_U8)
        g->primitive = c->i8;
      else if(name == c->str_I16)
        g->primitive = c->i16;
      else if(name == c->str_U16)
        g->primitive = c->i16;
      else if(name == c->str_I32)
        g->primitive = c->i32;
      else if(name == c->str_U32)
        g->primitive = c->i32;
      else if(name == c->str_I64)
        g->primitive = c->i64;
      else if(name == c->str_U64)
        g->primitive = c->i64;
      else if(name == c->str_I128)
        g->primitive = c->i128;
      else if(name == c->str_U128)
        g->primitive = c->i128;
      else if(ilp32 && name == c->str_ILong)
        g->primitive = c->i32;
      else if(ilp32 && name == c->str_ULong)
        g->primitive = c->i32;
      else if(ilp32 && name == c->str_ISize)
        g->primitive = c->i32;
      else if(ilp32 && name == c->str_USize)
        g->primitive = c->i32;
      else if(lp64 && name == c->str_ILong)
        g->primitive = c->i64;
      else if(lp64 && name == c->str_ULong)
        g->primitive = c->i64;
      else if(lp64 && name == c->str_ISize)
        g->primitive = c->i64;
      else if(lp64 && name == c->str_USize)
        g->primitive = c->i64;
      else if(llp64 && name == c->str_ILong)
        g->primitive = c->i32;
      else if(llp64 && name == c->str_ULong)
        g->primitive = c->i32;
      else if(llp64 && name == c->str_ISize)
        g->primitive = c->i64;
      else if(llp64 && name == c->str_USize)
        g->primitive = c->i64;
      else if(name == c->str_F32)
        g->primitive = c->f32;
      else if(name == c->str_F64)
        g->primitive = c->f64;
      else if(name == c->str_Pointer)
        return genprim_pointer(c, g, prelim);
      else if(name == c->str_Maybe)
        return genprim_maybe(c, g, prelim);
      else if(name == c->str_Platform)
        return true;
    }
  } else {
    g->underlying = TK_TUPLETYPE;
  }

  // Find or create the structure type.
  g->structure = LLVMGetTypeByName(c->module, g->type_name);

  if(g->structure == NULL)
    g->structure = LLVMStructCreateNamed(c->context, g->type_name);

  bool opaque = LLVMIsOpaqueStruct(g->structure) != 0;

  if(g->underlying == TK_TUPLETYPE)
  {
    // This is actually our primitive type.
    g->primitive = g->structure;
    g->structure = NULL;
  } else {
    g->structure_ptr = LLVMPointerType(g->structure, 0);
  }

  // Fill in our global descriptor.
  make_global_descriptor(c, g);

  if(g->primitive != NULL)
  {
    // We're primitive, so use the primitive type.
    g->use_type = g->primitive;
  } else {
    // We're not primitive, so use a pointer to our structure.
    g->use_type = g->structure_ptr;
  }

  if(!opaque)
  {
    // Fill in our global instance if the type is not opaque.
    make_global_instance(c, g);

    // Fill in a box type if we need one.
    make_box_type(c, g);

    return true;
  }

  return prelim;
}

static void setup_tuple_fields(gentype_t* g)
{
  g->field_count = (int)ast_childcount(g->ast);
  g->fields = (ast_t**)calloc(g->field_count, sizeof(ast_t*));
  g->field_keys = NULL;

  ast_t* child = ast_child(g->ast);
  size_t index = 0;

  while(child != NULL)
  {
    g->fields[index++] = child;
    child = ast_sibling(child);
  }
}

static void setup_type_fields(gentype_t* g)
{
  assert(ast_id(g->ast) == TK_NOMINAL);

  g->field_count = 0;
  g->fields = NULL;
  g->field_keys = NULL;

  ast_t* def = (ast_t*)ast_data(g->ast);

  if(ast_id(def) == TK_PRIMITIVE)
    return;

  ast_t* typeargs = ast_childidx(g->ast, 2);
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
      {
        g->field_count++;
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  if(g->field_count == 0)
    return;

  g->fields = (ast_t**)calloc(g->field_count, sizeof(ast_t*));
  g->field_keys = (token_id*)calloc(g->field_count, sizeof(token_id));

  member = ast_child(members);
  size_t index = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
      {
        AST_GET_CHILDREN(member, name, type, init);
        g->fields[index] = reify(ast_type(member), typeparams, typeargs);

        // TODO: Are we sure the AST source file is correct?
        ast_setpos(g->fields[index], NULL, ast_line(name), ast_pos(name));
        g->field_keys[index] = ast_id(member);
        index++;
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }
}

static void free_fields(gentype_t* g)
{
  for(int i = 0; i < g->field_count; i++)
    ast_free_unattached(g->fields[i]);

  free(g->fields);
  free(g->field_keys);

  g->field_count = 0;
  g->fields = NULL;
  g->field_keys = NULL;
}

static void make_dispatch(compile_t* c, gentype_t* g)
{
  // Do nothing if we're not an actor.
  if(g->underlying != TK_ACTOR)
    return;

  // Create a dispatch function.
  const char* dispatch_name = genname_dispatch(g->type_name);
  g->dispatch_fn = codegen_addfun(c, dispatch_name, c->dispatch_type);
  LLVMSetFunctionCallConv(g->dispatch_fn, LLVMCCallConv);
  codegen_startfun(c, g->dispatch_fn, false);

  LLVMBasicBlockRef unreachable = codegen_block(c, "unreachable");

  // Read the message ID.
  LLVMValueRef msg = LLVMGetParam(g->dispatch_fn, 2);
  LLVMValueRef id_ptr = LLVMBuildStructGEP(c->builder, msg, 1, "");
  LLVMValueRef id = LLVMBuildLoad(c->builder, id_ptr, "id");

  // Store a reference to the dispatch switch. When we build behaviours, we
  // will add cases to this switch statement based on message ID.
  g->dispatch_switch = LLVMBuildSwitch(c->builder, id, unreachable, 0);

  // Mark the default case as unreachable.
  LLVMPositionBuilderAtEnd(c->builder, unreachable);
  LLVMBuildUnreachable(c->builder);
  codegen_finishfun(c);
}

static bool trace_fields(compile_t* c, gentype_t* g, LLVMValueRef ctx,
  LLVMValueRef object, int extra)
{
  bool need_trace = false;

  for(int i = 0; i < g->field_count; i++)
  {
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, i + extra, "");

    if(g->field_keys[i] != TK_EMBED)
    {
      // Call the trace function indirectly depending on rcaps.
      LLVMValueRef value = LLVMBuildLoad(c->builder, field, "");
      need_trace |= gentrace(c, ctx, value, g->fields[i]);
    } else {
      // Call the trace function directly without marking the field.
      const char* fun = genname_trace(genname_type(g->fields[i]));
      LLVMValueRef trace_fn = LLVMGetNamedFunction(c->module, fun);

      if(trace_fn != NULL)
      {
        LLVMValueRef args[2];
        args[0] = ctx;
        args[1] = LLVMBuildBitCast(c->builder, field, c->object_ptr, "");

        LLVMBuildCall(c->builder, trace_fn, args, 2, "");
        need_trace = true;
      }
    }
  }

  return need_trace;
}

static bool make_trace(compile_t* c, gentype_t* g)
{
  // Do nothing if we have no fields.
  if(g->field_count == 0)
    return true;

  if(g->underlying == TK_CLASS)
  {
    // Special case the array trace function.
    AST_GET_CHILDREN(g->ast, pkg, id);
    const char* package = ast_name(pkg);
    const char* name = ast_name(id);

    if((package == c->str_builtin) && (name == c->str_Array))
    {
      genprim_array_trace(c, g);
      return true;
    }
  }

  // Create a trace function.
  const char* trace_name = genname_trace(g->type_name);
  LLVMValueRef trace_fn = codegen_addfun(c, trace_name, c->trace_type);

  codegen_startfun(c, trace_fn, false);
  LLVMSetFunctionCallConv(trace_fn, LLVMCCallConv);

  LLVMValueRef ctx = LLVMGetParam(trace_fn, 0);
  LLVMValueRef arg = LLVMGetParam(trace_fn, 1);
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, g->structure_ptr,
    "object");

  // If we don't ever trace anything, delete this function.
  int extra = 0;

  // Non-structs have a type descriptor.
  if(g->underlying != TK_STRUCT)
    extra++;

  // Actors have a pad.
  if(g->underlying == TK_ACTOR)
    extra++;

  bool need_trace = trace_fields(c, g, ctx, object, extra);

  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);

  if(!need_trace)
    LLVMDeleteFunction(trace_fn);

  return true;
}

static bool make_struct(compile_t* c, gentype_t* g)
{
  LLVMTypeRef type;
  int extra = 0;

  if(g->underlying != TK_TUPLETYPE)
  {
    type = g->structure;

    if(g->underlying != TK_STRUCT)
      extra++;
  } else {
    type = g->primitive;
  }

  if(g->underlying == TK_ACTOR)
    extra++;

  size_t buf_size = (g->field_count + extra) * sizeof(LLVMTypeRef);
  LLVMTypeRef* elements = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);

  // Create the type descriptor as element 0.
  if(extra > 0)
    elements[0] = LLVMPointerType(g->desc_type, 0);

  // Create the actor pad as element 1.
  if(g->underlying == TK_ACTOR)
    elements[1] = c->actor_pad;

  // Get a preliminary type for each field and set the struct body. This is
  // needed in case a struct for the type being generated here is required when
  // generating a field.
  for(int i = 0; i < g->field_count; i++)
  {
    gentype_t field_g;
    bool ok;

    if((g->field_keys != NULL) && (g->field_keys[i] == TK_EMBED))
    {
      ok = gentype(c, g->fields[i], &field_g);
      elements[i + extra] = field_g.structure;
    } else {
      ok = gentype_prelim(c, g->fields[i], &field_g);
      elements[i + extra] = field_g.use_type;
    }

    if(!ok)
    {
      ponyint_pool_free_size(buf_size, elements);
      return false;
    }
  }

  // An embedded field may have caused the current type to be fully generated
  // at this point. If so, finish gracefully.
  if(!LLVMIsOpaqueStruct(type))
  {
    g->done = true;
    return true;
  }

  LLVMStructSetBody(type, elements, g->field_count + extra, false);

  // Create a box type for tuples.
  if(g->underlying == TK_TUPLETYPE)
    make_box_type(c, g);

  ponyint_pool_free_size(buf_size, elements);
  return true;
}

static bool make_components(compile_t* c, gentype_t* g)
{
  for(int i = 0; i < g->field_count; i++)
  {
    gentype_t field_g;

    if(!gentype(c, g->fields[i], &field_g))
      return false;

    dwarf_field(&c->dwarf, g, &field_g, i);
  }

  return true;
}

static bool make_nominal(compile_t* c, ast_t* ast, gentype_t* g, bool prelim)
{
  assert(ast_id(ast) == TK_NOMINAL);
  ast_t* def = (ast_t*)ast_data(ast);
  token_id id = ast_id(def);

  // For traits, just return a raw object pointer.
  switch(id)
  {
    case TK_INTERFACE:
    case TK_TRAIT:
      g->underlying = id;
      g->use_type = c->object_ptr;
      dwarf_trait(&c->dwarf, g);
      return true;

    default: {}
  }

  // If we already exist or we're preliminary, we're done.
  if(setup_name(c, ast, g, prelim))
    return true;

  if(g->primitive == NULL)
  {
    // Not a primitive type. Generate all the fields and a trace function.
    setup_type_fields(g);

    // Forward declare debug symbols for this nominal, if needed.
    // At this point, this can only be TK_STRUCT, TK_CLASS, TK_PRIMITIVE, or
    // TK_ACTOR ast nodes. TK_TYPE has been translated to any of the former
    // during reification.
    dwarf_forward(&c->dwarf, g);

    bool ok = make_struct(c, g);

    if(!g->done)
      ok = ok && make_trace(c, g) && make_components(c, g);

    if(!ok)
    {
      free_fields(g);
      return false;
    }

    // Finalise symbols for composite type.
    if(!g->done)
      dwarf_composite(&c->dwarf, g);
  } else {
    // Emit debug symbols for a basic type (U8, U16, U32...)
    dwarf_basic(&c->dwarf, g);

    // Create a box type.
    make_box_type(c, g);
  }

  if(!g->done)
  {
    // Generate a dispatch function if necessary.
    make_dispatch(c, g);

    // Create a unique global instance if we need one.
    make_global_instance(c, g);

    // Generate all the methods.
    if(!genfun_methods(c, g))
    {
      free_fields(g);
      return false;
    }

    if(g->underlying != TK_STRUCT)
      gendesc_init(c, g);

    // Finish off the dispatch function.
    if(g->underlying == TK_ACTOR)
    {
      codegen_startfun(c, g->dispatch_fn, false);
      codegen_finishfun(c);
    }

    // Finish the dwarf frame.
    dwarf_finish(&c->dwarf);
  }

  free_fields(g);
  g->done = true;
  return true;
}

static bool make_tuple(compile_t* c, ast_t* ast, gentype_t* g)
{
  // An anonymous structure with no functions and no vtable.
  if(setup_name(c, ast, g, false))
    return true;

  setup_tuple_fields(g);

  dwarf_forward(&c->dwarf, g);

  bool ok = make_struct(c, g) && make_components(c, g);

  // Finalise debug symbols for tuple type.
  dwarf_composite(&c->dwarf, g);
  dwarf_finish(&c->dwarf);

  // Generate a descriptor.
  gendesc_init(c, g);

  free_fields(g);
  return ok;
}

bool gentype_prelim(compile_t* c, ast_t* ast, gentype_t* g)
{
  if(ast_id(ast) == TK_NOMINAL)
  {
    memset(g, 0, sizeof(gentype_t));

    g->ast = ast;
    g->type_name = genname_type(ast);
    g->desc_name = genname_descriptor(g->type_name);

    return make_nominal(c, ast, g, true);
  }

  return gentype(c, ast, g);
}

bool gentype(compile_t* c, ast_t* ast, gentype_t* g)
{
  memset(g, 0, sizeof(gentype_t));

  if(ast == NULL)
    return false;

  if(contains_dontcare(ast))
    return true;

  g->ast = ast;
  g->type_name = genname_type(ast);
  g->desc_name = genname_descriptor(g->type_name);

  switch(ast_id(ast))
  {
    case TK_NOMINAL:
      return make_nominal(c, ast, g, false);

    case TK_TUPLETYPE:
      return make_tuple(c, ast, g);

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      // Just a raw object pointer.
      g->underlying = ast_id(ast);
      g->use_type = c->object_ptr;
      return true;

    default: {}
  }

  assert(0);
  return false;
}
