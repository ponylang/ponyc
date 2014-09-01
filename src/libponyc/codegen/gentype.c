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

/**
 * Return true if the type already exists or if we are only being asked to
 * generate a preliminary type, false otherwise.
 */
static bool setup_name(compile_t* c, ast_t* ast, gentype_t* g, bool prelim)
{
  g->ast = ast;
  g->type_name = genname_type(ast);
  g->primitive = NULL;

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
  g->type = LLVMGetTypeByName(c->module, g->type_name);

  if(g->type == NULL)
  {
    g->type = LLVMStructCreateNamed(LLVMGetGlobalContext(), g->type_name);
  } else if(!LLVMIsOpaqueStruct(g->type)) {
    return true;
  }

  if(ast_id(ast) == TK_NOMINAL)
  {
    // Only set up the descriptor once. Pointers have no descriptor, so it's
    // fine that this doesn't get executed for _Pointer.
    ast_t* def = (ast_t*)ast_data(ast);
    g->underlying = ast_id(def);
    g->vtable_size = painter_get_vtable_size(c->painter, def);

    const char* desc_name = genname_descriptor(g->type_name);
    g->desc_type = gendesc_type(c, desc_name, (int)g->vtable_size);
    g->desc = LLVMAddGlobal(c->module, g->desc_type, desc_name);
    LLVMSetGlobalConstant(g->desc, true);
  } else {
    g->underlying = TK_TUPLETYPE;
    g->vtable_size = 0;
    g->desc_type = NULL;
    g->desc = NULL;
  }

  return prelim;
}

static void setup_tuple_fields(gentype_t* g)
{
  g->field_count = ast_childcount(g->ast);
  g->fields = (ast_t**)calloc(g->field_count, sizeof(ast_t*));

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

  ast_t* def = (ast_t*)ast_data(g->ast);

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

  g->fields = (ast_t**)calloc(g->field_count, sizeof(ast_t*));

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

  LLVMTypeRef type_ptr = LLVMPointerType(g->type, 0);
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, type_ptr, "object");

  // If we don't ever trace anything, delete this function.
  bool need_trace = false;

  for(size_t i = 0; i < g->field_count; i++)
  {
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, (int)(i + extra), "");
    need_trace |= gencall_trace(c, field, g->fields[i]);
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
  PONY_VL_ARRAY(LLVMTypeRef, elements, g->field_count + extra);
  elements[0] = c->descriptor_ptr;

  if(g->underlying == TK_ACTOR)
    elements[1] = c->actor_pad;

  // Get a preliminary type for each field and set the struct body. This is
  // needed in case a struct for the type being generated here is required when
  // generating a field.
  for(size_t i = 0; i < g->field_count; i++)
  {
    elements[i + extra] = gentype_prelim(c, g->fields[i]);

    if(elements[i + extra] == NULL)
      return false;
  }

  LLVMStructSetBody(g->type, elements, (int)(g->field_count + extra), false);
  return true;
}

static bool make_components(compile_t* c, gentype_t* g)
{
  for(size_t i = 0; i < g->field_count; i++)
  {
    if(gentype(c, g->fields[i]) == NULL)
      return false;
  }

  return true;
}

static void make_global_instance(compile_t* c, gentype_t* g)
{
  if((g->underlying == TK_DATA) && (g->primitive == NULL))
  {
    LLVMValueRef args[1];
    args[0] = LLVMConstBitCast(g->desc, c->descriptor_ptr);
    LLVMValueRef value = LLVMConstNamedStruct(g->type, args, 1);

    const char* inst_name = genname_instance(g->type_name);
    g->instance = LLVMAddGlobal(c->module, g->type, inst_name);
    LLVMSetInitializer(g->instance, value);
    LLVMSetGlobalConstant(g->instance, true);
  } else {
    g->instance = NULL;
  }
}

static LLVMTypeRef make_result(gentype_t* g)
{
  // Primitives return the primitive type, not the boxed type.
  if(g->primitive != NULL)
    return g->primitive;

  return LLVMPointerType(g->type, 0);
}

static LLVMTypeRef make_nominal(compile_t* c, ast_t* ast, bool prelim)
{
  assert(ast_id(ast) == TK_NOMINAL);
  ast_t* def = (ast_t*)ast_data(ast);

  // For traits, just return a raw object pointer.
  if(ast_id(def) == TK_TRAIT)
    return c->object_ptr;

  // If we already exist or we're preliminary, we're done.
  gentype_t g;

  if(setup_name(c, ast, &g, prelim))
    return make_result(&g);

  // Generate a primitive type if we've encountered one.
  if(g.primitive != NULL)
  {
    // Element 0 is the type descriptor, element 1 is the boxed primitive type.
    // Primitive types have no trace function.
    LLVMTypeRef elements[2];
    elements[0] = c->descriptor_ptr;
    elements[1] = g.primitive;
    LLVMStructSetBody(g.type, elements, 2, false);
  } else {
    // Not a primitive type. Generate all the fields and a trace function.
    setup_type_fields(&g);
    bool ok = make_struct(c, &g) && make_trace(c, &g) && make_components(c, &g);
    free_fields(&g);

    if(!ok)
      return NULL;
  }

  // Create a unique global instance if we need one.
  make_global_instance(c, &g);

  // TODO: is this right?
  if(!genfun_methods(c, ast))
    return NULL;

  // TODO: is this right?
  // TODO: for actors: create a dispatch function, possibly a finaliser
  gendesc_init(c, &g);

  return make_result(&g);
}

static LLVMTypeRef make_tuple(compile_t* c, ast_t* ast)
{
  // An anonymous structure with no functions and no vtable.
  gentype_t g;

  if(setup_name(c, ast, &g, false))
    return make_result(&g);

  setup_tuple_fields(&g);
  bool ok = make_struct(c, &g) && make_trace(c, &g) && make_components(c, &g);
  free_fields(&g);

  if(!ok)
    return NULL;

  return make_result(&g);
}

LLVMTypeRef gentype_prelim(compile_t* c, ast_t* ast)
{
  if(ast_id(ast) == TK_NOMINAL)
    return make_nominal(c, ast, true);

  return gentype(c, ast);
}

LLVMTypeRef gentype(compile_t* c, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
    {
      // Special case Bool.
      if(is_bool(ast))
        return LLVMInt1Type();

      // Otherwise it's just a raw object pointer.
      return c->object_ptr;
    }

    case TK_ISECTTYPE:
      // Just a raw object pointer.
      return c->object_ptr;

    case TK_TUPLETYPE:
      return make_tuple(c, ast);

    case TK_NOMINAL:
      return make_nominal(c, ast, false);

    case TK_STRUCTURAL:
      // Just a raw object pointer.
      return c->object_ptr;

    default: {}
  }

  assert(0);
  return NULL;
}
