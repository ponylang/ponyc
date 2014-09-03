#include "gentype.h"
#include "genname.h"
#include "gendesc.h"
#include "genprim.h"
#include "gencall.h"
#include "genfun.h"
#include "../pkg/package.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void make_global_descriptor(compile_t* c, gentype_t* g)
{
  // Tuples have no descriptor.
  if(g->underlying == TK_TUPLETYPE)
    return;

  // Fetch or create a descriptor type.
  ast_t* def = ast_data(g->ast);
  g->vtable_size = painter_get_vtable_size(c->painter, def);

  const char* desc_name = genname_descriptor(g->type_name);
  g->desc_type = gendesc_type(c, desc_name, g->vtable_size);

  // Check for an existing descriptor.
  g->desc = LLVMGetNamedGlobal(c->module, desc_name);

  if(g->desc != NULL)
    return;

  g->desc = LLVMAddGlobal(c->module, g->desc_type, desc_name);
  LLVMSetGlobalConstant(g->desc, true);
}

static void make_global_instance(compile_t* c, gentype_t* g)
{
  if(g->underlying != TK_DATA)
  {
    // Not a data type.
    g->instance = NULL;
    return;
  }

  if(g->primitive != NULL)
  {
    // A primitive type, use an uninitialised value.
    g->instance = LLVMGetUndef(g->primitive);
    return;
  }

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
}

/**
 * Return true if the type already exists or if we are only being asked to
 * generate a preliminary type, false otherwise.
 */
static bool setup_name(compile_t* c, ast_t* ast, gentype_t* g, bool prelim)
{
  if(ast_id(ast) == TK_NOMINAL)
  {
    ast_t* def = ast_data(ast);
    g->underlying = ast_id(def);
  } else {
    g->underlying = TK_TUPLETYPE;
  }

  // Find the primitive type, if there is one.
  AST_GET_CHILDREN(ast, pkg, id);
  const char* package = ast_name(pkg);
  const char* name = ast_name(id);

  if(!strcmp(package, "$1"))
  {
    if(!strcmp(name, "True"))
      g->primitive = LLVMInt1Type();
    else if(!strcmp(name, "False"))
      g->primitive = LLVMInt1Type();
    else if(!strcmp(name, "I8"))
      g->primitive = LLVMInt8Type();
    else if(!strcmp(name, "U8"))
      g->primitive = LLVMInt8Type();
    else if(!strcmp(name, "I16"))
      g->primitive = LLVMInt16Type();
    else if(!strcmp(name, "U16"))
      g->primitive = LLVMInt16Type();
    else if(!strcmp(name, "I32"))
      g->primitive = LLVMInt32Type();
    else if(!strcmp(name, "U32"))
      g->primitive = LLVMInt32Type();
    else if(!strcmp(name, "I64"))
      g->primitive = LLVMInt64Type();
    else if(!strcmp(name, "U64"))
      g->primitive = LLVMInt64Type();
    else if(!strcmp(name, "I128"))
      g->primitive = LLVMIntType(128);
    else if(!strcmp(name, "U128"))
      g->primitive = LLVMIntType(128);
    else if(!strcmp(name, "SIntLiteral"))
      g->primitive = LLVMIntType(128);
    else if(!strcmp(name, "UIntLiteral"))
      g->primitive = LLVMIntType(128);
    else if(!strcmp(name, "F16"))
      g->primitive = LLVMHalfType();
    else if(!strcmp(name, "F32"))
      g->primitive = LLVMFloatType();
    else if(!strcmp(name, "F64"))
      g->primitive = LLVMDoubleType();
    else if(!strcmp(name, "FloatLiteral"))
      g->primitive = LLVMDoubleType();
    else if(!strcmp(name, "_Pointer"))
      return genprim_pointer(c, g, prelim);
  }

  // Find or create the structure type.
  g->structure = LLVMGetTypeByName(c->module, g->type_name);

  if(g->structure == NULL)
    g->structure = LLVMStructCreateNamed(LLVMGetGlobalContext(), g->type_name);

  g->structure_ptr = LLVMPointerType(g->structure, 0);

  if(g->primitive != NULL)
    g->use_type = g->primitive;
  else
    g->use_type = g->structure_ptr;

  // Fill in our global descriptor.
  make_global_descriptor(c, g);

  // Fill in our global instance if the type is not opaque.
  if(!LLVMIsOpaqueStruct(g->structure))
  {
    make_global_instance(c, g);
    return true;
  }

  return prelim;
}

static void setup_tuple_fields(gentype_t* g)
{
  g->field_count = ast_childcount(g->ast);
  g->fields = calloc(g->field_count, sizeof(ast_t*));

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

  ast_t* def = ast_data(g->ast);

  if(ast_id(def) == TK_DATA)
    return;

  ast_t* typeargs = ast_childidx(g->ast, 2);
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member;

  member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
        g->field_count++;
        break;

      default: {}
    }

    member = ast_sibling(member);
  }

  if(g->field_count == 0)
    return;

  g->fields = calloc(g->field_count, sizeof(ast_t*));

  member = ast_child(members);
  size_t index = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      {
        g->fields[index] = reify(ast_type(member), typeparams, typeargs);
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
  for(size_t i = 0; i < g->field_count; i++)
    ast_free_unattached(g->fields[i]);

  free(g->fields);

  g->field_count = 0;
  g->fields = NULL;
}

static void make_dispatch(compile_t* c, gentype_t* g)
{
  // Do nothing if we're not an actor.
  if(g->underlying != TK_ACTOR)
    return;

  // Create a dispatch function.
  const char* dispatch_name = genname_dispatch(g->type_name);
  g->dispatch_fn = LLVMAddFunction(c->module, dispatch_name, c->dispatch_type);

  codegen_startfun(c, g->dispatch_fn);
  LLVMBasicBlockRef unreachable = LLVMAppendBasicBlock(g->dispatch_fn,
    "unreachable");

  LLVMValueRef this_ptr = LLVMGetParam(g->dispatch_fn, 0);
  LLVMSetValueName(this_ptr, "this");

  g->dispatch_msg = LLVMGetParam(g->dispatch_fn, 1);
  LLVMSetValueName(g->dispatch_msg, "msg");

  // Read the message ID.
  LLVMValueRef id_ptr = LLVMBuildStructGEP(c->builder, g->dispatch_msg, 0, "");
  LLVMValueRef id = LLVMBuildLoad(c->builder, id_ptr, "id");

  // Store a reference to the dispatch switch. When we build behaviours, we
  // will add cases to this switch statement based on message ID.
  g->dispatch_switch = LLVMBuildSwitch(c->builder, id, unreachable, 0);

  // Mark the default case as unreachable.
  LLVMPositionBuilderAtEnd(c->builder, unreachable);
  LLVMBuildUnreachable(c->builder);
  codegen_finishfun(c);
}

static bool make_trace(compile_t* c, gentype_t* g)
{
  // Do nothing if we have no fields.
  if(g->field_count == 0)
    return true;

  // Special case the array trace function.
  AST_GET_CHILDREN(g->ast, pkg, id);
  const char* package = ast_name(pkg);
  const char* name = ast_name(id);

  if(!strcmp(package, "$1") && !strcmp(name, "Array"))
  {
    genprim_array_trace(c, g);
    return true;
  }

  // Classes have a descriptor. Actors also have a pad.
  size_t extra = 1;

  if(g->underlying == TK_ACTOR)
    extra++;

  // Create a trace function.
  const char* trace_name = genname_trace(g->type_name);
  LLVMValueRef trace_fn = LLVMAddFunction(c->module, trace_name, c->trace_type);
  codegen_startfun(c, trace_fn);

  LLVMValueRef arg = LLVMGetParam(trace_fn, 0);
  LLVMSetValueName(arg, "arg");

  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, g->structure_ptr,
    "object");

  // If we don't ever trace anything, delete this function.
  bool need_trace = false;

  for(size_t i = 0; i < g->field_count; i++)
  {
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, i + extra, "");
    LLVMValueRef value = LLVMBuildLoad(c->builder, field, "");
    need_trace |= gencall_trace(c, value, g->fields[i]);
  }

  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);

  if(!need_trace)
    LLVMDeleteFunction(trace_fn);

  return true;
}

static bool make_struct(compile_t* c, gentype_t* g)
{
  size_t extra = 1;

  if(g->underlying == TK_ACTOR)
    extra++;

  // Create the type descriptor as element 0.
  LLVMTypeRef elements[g->field_count + extra];
  elements[0] = LLVMPointerType(g->desc_type, 0);

  if(g->underlying == TK_ACTOR)
    elements[1] = c->actor_pad;

  // Get a preliminary type for each field and set the struct body. This is
  // needed in case a struct for the type being generated here is required when
  // generating a field.
  for(size_t i = 0; i < g->field_count; i++)
  {
    gentype_t field_g;

    if(!gentype_prelim(c, g->fields[i], &field_g))
      return false;

    elements[i + extra] = field_g.use_type;
  }

  LLVMStructSetBody(g->structure, elements, g->field_count + extra, false);
  return true;
}

static bool make_components(compile_t* c, gentype_t* g)
{
  for(size_t i = 0; i < g->field_count; i++)
  {
    gentype_t field_g;

    if(!gentype(c, g->fields[i], &field_g))
      return false;
  }

  return true;
}

static bool make_nominal(compile_t* c, ast_t* ast, gentype_t* g, bool prelim)
{
  assert(ast_id(ast) == TK_NOMINAL);
  ast_t* def = ast_data(ast);

  // For traits, just return a raw object pointer.
  if(ast_id(def) == TK_TRAIT)
  {
    g->use_type = c->object_ptr;
    return true;
  }

  // If we already exist or we're preliminary, we're done.
  if(setup_name(c, ast, g, prelim))
    return true;

  // Generate a primitive type if we've encountered one.
  if(g->primitive != NULL)
  {
    // Element 0 is the type descriptor, element 1 is the boxed primitive type.
    // Primitive types have no trace function.
    LLVMTypeRef elements[2];
    elements[0] = g->desc_type;
    elements[1] = g->primitive;
    LLVMStructSetBody(g->structure, elements, 2, false);
  } else {
    // Not a primitive type. Generate all the fields and a trace function.
    setup_type_fields(g);
    bool ok = make_struct(c, g) && make_trace(c, g) && make_components(c, g);
    free_fields(g);

    if(!ok)
      return false;
  }

  // Generate a dispatch function if necessary.
  make_dispatch(c, g);

  // Create a unique global instance if we need one.
  make_global_instance(c, g);

  // Generate all the methods.
  if(!genfun_methods(c, g))
    return false;

  // TODO: for actors: create a finaliser
  gendesc_init(c, g);

  return true;
}

static bool make_tuple(compile_t* c, ast_t* ast, gentype_t* g)
{
  // An anonymous structure with no functions and no vtable.
  if(setup_name(c, ast, g, false))
    return true;

  setup_tuple_fields(g);
  bool ok = make_struct(c, g) && make_trace(c, g) && make_components(c, g);
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

    return make_nominal(c, ast, g, true);
  }

  return gentype(c, ast, g);
}

bool gentype(compile_t* c, ast_t* ast, gentype_t* g)
{
  memset(g, 0, sizeof(gentype_t));

  g->ast = ast;
  g->type_name = genname_type(ast);

  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
    {
      // Special case Bool. Otherwise it's just a raw object pointer.
      if(is_bool(ast))
      {
        g->primitive = LLVMInt1Type();
        g->use_type = g->primitive;
      } else {
        g->use_type = c->object_ptr;
      }

      return true;
    }

    case TK_ISECTTYPE:
      // Just a raw object pointer.
      g->use_type = c->object_ptr;
      return true;

    case TK_TUPLETYPE:
      return make_tuple(c, ast, g);

    case TK_NOMINAL:
      return make_nominal(c, ast, g, false);

    case TK_STRUCTURAL:
      // Just a raw object pointer.
      g->use_type = c->object_ptr;
      return true;

    default: {}
  }

  assert(0);
  return false;
}
