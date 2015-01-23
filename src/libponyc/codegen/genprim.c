#include "genprim.h"
#include "genname.h"
#include "gentype.h"
#include "genfun.h"
#include "gencall.h"
#include "gentrace.h"
#include "../pkg/platformfuns.h"
#include "../pass/names.h"
#include "../debug/dwarf.h"

ast_t* genprim(compile_t* c, ast_t* scope, const char* name, gentype_t* g)
{
  ast_t* ast = ast_from(scope, TK_NOMINAL);
  ast_add(ast, ast_from(scope, TK_NONE));
  ast_add(ast, ast_from(scope, TK_NONE));
  ast_add(ast, ast_from(scope, TK_NONE));
  ast_add(ast, ast_from_string(scope, name));
  ast_add(ast, ast_from(scope, TK_NONE));

  if(!names_nominal(c->opt, scope, &ast) || !gentype(c, ast, g))
  {
    ast_free_unattached(ast);
    return NULL;
  }

  return ast;
}

static void pointer_create(compile_t* c, gentype_t* g)
{
  const char* name = genname_fun(g->type_name, "create", NULL);

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, NULL, 0, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun);

  LLVMValueRef result = LLVMConstNull(g->use_type);

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer__create(compile_t* c, gentype_t* g, gentype_t* elem_g)
{
  // Set up a constant integer for the allocation size.
  size_t size = LLVMABISizeOfType(c->target_data, elem_g->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->i64, size, false);

  const char* name = genname_fun(g->type_name, "_create", NULL);
  LLVMValueRef fun = LLVMGetNamedFunction(c->module, name);

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, &c->i64, 1, false);
  fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun);

  LLVMValueRef len = LLVMGetParam(fun, 0);
  LLVMValueRef total = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef result = gencall_runtime(c, "pony_alloc", &total, 1, "");
  result = LLVMBuildBitCast(c->builder, result, g->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_realloc(compile_t* c, gentype_t* g, gentype_t* elem_g)
{
  // Set up a constant integer for the allocation size.
  size_t size = LLVMABISizeOfType(c->target_data, elem_g->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->i64, size, false);

  const char* name = genname_fun(g->type_name, "_realloc", NULL);

  LLVMTypeRef params[2];
  params[0] = g->use_type;
  params[1] = c->i64;

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, params, 2, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun);

  LLVMValueRef args[2];
  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  args[0] = LLVMBuildBitCast(c->builder, ptr, c->void_ptr, "");

  LLVMValueRef len = LLVMGetParam(fun, 1);
  args[1] = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef result = gencall_runtime(c, "pony_realloc", args, 2, "");
  result = LLVMBuildBitCast(c->builder, result, g->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_apply(compile_t* c, gentype_t* g, gentype_t* elem_g)
{
  const char* name = genname_fun(g->type_name, "_apply", NULL);

  LLVMTypeRef params[2];
  params[0] = g->use_type;
  params[1] = c->i64;

  LLVMTypeRef ftype = LLVMFunctionType(elem_g->use_type, params, 2, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun);

  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  LLVMValueRef index = LLVMGetParam(fun, 1);
  LLVMValueRef loc = LLVMBuildGEP(c->builder, ptr, &index, 1, "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, loc, "");
  result = LLVMBuildBitCast(c->builder, result, elem_g->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_update(compile_t* c, gentype_t* g, gentype_t* elem_g)
{
  const char* name = genname_fun(g->type_name, "_update", NULL);

  LLVMTypeRef params[3];
  params[0] = g->use_type;
  params[1] = c->i64;
  params[2] = elem_g->use_type;

  LLVMTypeRef ftype = LLVMFunctionType(elem_g->use_type, params, 3, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun);

  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  LLVMValueRef index = LLVMGetParam(fun, 1);
  LLVMValueRef loc = LLVMBuildGEP(c->builder, ptr, &index, 1, "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, loc, "");
  result = LLVMBuildBitCast(c->builder, result, elem_g->use_type, "");
  LLVMBuildStore(c->builder, LLVMGetParam(fun, 2), loc);

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_delete(compile_t* c, gentype_t* g, gentype_t* elem_g)
{
  // Set up a constant integer for the allocation size.
  size_t size = LLVMABISizeOfType(c->target_data, elem_g->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->i64, size, false);

  const char* name = genname_fun(g->type_name, "_delete", NULL);

  LLVMTypeRef params[4];
  params[0] = g->use_type;
  params[1] = c->i64;
  params[2] = c->i64;
  params[3] = c->i64;

  LLVMTypeRef ftype = LLVMFunctionType(elem_g->use_type, params, 4, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun);

  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  LLVMValueRef offset = LLVMGetParam(fun, 1);
  LLVMValueRef n = LLVMGetParam(fun, 2);
  LLVMValueRef len = LLVMGetParam(fun, 3);

  LLVMValueRef loc = LLVMBuildGEP(c->builder, ptr, &offset, 1, "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, loc, "");
  result = LLVMBuildBitCast(c->builder, result, elem_g->use_type, "");

  LLVMValueRef base = LLVMBuildPtrToInt(c->builder, ptr, c->i64, "");
  LLVMValueRef offset1 = LLVMBuildMul(c->builder, offset, l_size, "");
  LLVMValueRef offset2 = LLVMBuildAdd(c->builder, offset, n, "");
  offset2 = LLVMBuildMul(c->builder, offset2, l_size, "");

  LLVMValueRef args[3];
  args[0] = LLVMBuildAdd(c->builder, base, offset1, "");
  args[1] = LLVMBuildAdd(c->builder, base, offset2, "");
  args[2] = LLVMBuildMul(c->builder, len, l_size, "");

  args[0] = LLVMBuildIntToPtr(c->builder, args[0], c->void_ptr, "");
  args[1] = LLVMBuildIntToPtr(c->builder, args[1], c->void_ptr, "");

  gencall_runtime(c, "memmove", args, 3, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_u64(compile_t* c, gentype_t* g)
{
  const char* name = genname_fun(g->type_name, "u64", NULL);

  LLVMTypeRef ftype = LLVMFunctionType(c->i64, &g->use_type, 1, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun);

  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  LLVMValueRef result = LLVMBuildPtrToInt(c->builder, ptr, c->i64, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

bool genprim_pointer(compile_t* c, gentype_t* g, bool prelim)
{
  // No trace function is generated, so the "contents" of a pointer are never
  // traced. The only exception is for the pointer held by an array, which has
  // a special trace function generated in genprim_array_trace.
  ast_t* typeargs = ast_childidx(g->ast, 2);
  ast_t* typearg = ast_child(typeargs);

  gentype_t elem_g;
  bool ok;

  if(prelim)
    ok = gentype_prelim(c, typearg, &elem_g);
  else
    ok = gentype(c, typearg, &elem_g);

  if(!ok)
    return false;

  // Set the type to be a pointer to the element type.
  g->use_type = LLVMPointerType(elem_g.use_type, 0);

  // Stop here for a preliminary type.
  if(prelim)
    return true;

  // If there's already a create function, we're done.
  const char* name = genname_fun(g->type_name, "_create", NULL);
  LLVMValueRef fun = LLVMGetNamedFunction(c->module, name);

  if(fun != NULL)
    return true;
  
  // Emit debug symbol for this pointer type instance.
  dwarf_pointer(c->dwarf, g, &elem_g);

  pointer_create(c, g);
  pointer__create(c, g, &elem_g);

  pointer_realloc(c, g, &elem_g);
  pointer_apply(c, g, &elem_g);
  pointer_update(c, g, &elem_g);
  pointer_delete(c, g, &elem_g);
  pointer_u64(c, g);

  return genfun_methods(c, g);
}

void genprim_array_trace(compile_t* c, gentype_t* g)
{
  // Get the type argument for the array. This will be used to generate the
  // per-element trace call.
  ast_t* typeargs = ast_childidx(g->ast, 2);
  ast_t* typearg = ast_child(typeargs);

  const char* trace_name = genname_trace(g->type_name);
  LLVMValueRef trace_fn = codegen_addfun(c, trace_name, c->trace_type);

  codegen_startfun(c, trace_fn);
  LLVMSetFunctionCallConv(trace_fn, LLVMCCallConv);
  LLVMValueRef arg = LLVMGetParam(trace_fn, 0);

  LLVMBasicBlockRef cond_block = codegen_block(c, "cond");
  LLVMBasicBlockRef body_block = codegen_block(c, "body");
  LLVMBasicBlockRef post_block = codegen_block(c, "post");

  // Read the count and the base pointer.
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, g->use_type, "array");
  LLVMValueRef count_ptr = LLVMBuildStructGEP(c->builder, object, 1, "");
  LLVMValueRef count = LLVMBuildLoad(c->builder, count_ptr, "count");
  LLVMValueRef pointer_ptr = LLVMBuildStructGEP(c->builder, object, 3, "");
  LLVMValueRef pointer = LLVMBuildLoad(c->builder, pointer_ptr, "pointer");

  // Trace the base pointer.
  LLVMValueRef address = LLVMBuildBitCast(c->builder, pointer, c->void_ptr, "");
  gencall_runtime(c, "pony_trace", &address, 1, "");
  LLVMBuildBr(c->builder, cond_block);

  // While the index is less than the count, trace an element. The initial
  // index when coming from the entry block is zero.
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->i64, "");
  LLVMValueRef zero = LLVMConstInt(c->i64, 0, false);
  LLVMBasicBlockRef entry_block = LLVMGetEntryBasicBlock(trace_fn);
  LLVMAddIncoming(phi, &zero, &entry_block, 1);
  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntULT, phi, count, "");
  LLVMBuildCondBr(c->builder, test, body_block, post_block);

  // The phi node is the index. Get the element and trace it.
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef elem = LLVMBuildGEP(c->builder, pointer, &phi, 1, "elem");
  elem = LLVMBuildLoad(c->builder, elem, "");
  gentrace(c, elem, typearg);

  // Add one to the phi node and branch back to the cond block.
  LLVMValueRef one = LLVMConstInt(c->i64, 1, false);
  LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
  body_block = LLVMGetInsertBlock(c->builder);
  LLVMAddIncoming(phi, &inc, &body_block, 1);
  LLVMBuildBr(c->builder, cond_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMBuildRetVoid(c->builder);

  codegen_finishfun(c);
}

typedef struct num_conv_t
{
  const char* type_name;
  const char* fun_name;
  LLVMTypeRef type;
  int size;
  bool is_signed;
  bool is_float;
} num_conv_t;

static void number_conversions(compile_t* c)
{
  num_conv_t conv[] =
  {
    {"I8", "i8", c->i8, 8, true, false},
    {"I16", "i16", c->i16, 16, true, false},
    {"I32", "i32", c->i32, 32, true, false},
    {"I64", "i64", c->i64, 64, true, false},
    {"I128", "i128", c->i128, 128, true, false},

    {"U8", "u8", c->i8, 8, false, false},
    {"U16", "u16", c->i16, 16, false, false},
    {"U32", "u32", c->i32, 32, false, false},
    {"U64", "u64", c->i64, 64, false, false},
    {"U128", "u128", c->i128, 128, false, false},

    {"F32", "f32", c->f32, 32, false, true},
    {"F64", "f64", c->f64, 64, false, true},

    {NULL, NULL, NULL, false, false, false}
  };

  bool has_i128;
  os_is_target(OS_HAS_I128_NAME, c->opt->release, &has_i128);

  for(num_conv_t* from = conv; from->type_name != NULL; from++)
  {
    for(num_conv_t* to = conv; to->type_name != NULL; to++)
    {
      if(to->fun_name == NULL)
        continue;

      const char* name = genname_fun(from->type_name, to->fun_name, NULL);
      LLVMTypeRef f_type = LLVMFunctionType(to->type, &from->type, 1, false);
      LLVMValueRef fun = codegen_addfun(c, name, f_type);

      codegen_startfun(c, fun);
      LLVMValueRef arg = LLVMGetParam(fun, 0);
      LLVMValueRef result;

      if(from->is_float)
      {
        if(to->is_float)
        {
          if(from->size < to->size)
            result = LLVMBuildFPExt(c->builder, arg, to->type, "");
          else if(from->size > to->size)
            result = LLVMBuildFPTrunc(c->builder, arg, to->type, "");
          else
            result = arg;
        } else if(to->is_signed) {
          result = LLVMBuildFPToSI(c->builder, arg, to->type, "");
        } else {
          result = LLVMBuildFPToUI(c->builder, arg, to->type, "");
        }
      } else if(to->is_float) {
        if(from->is_signed)
          result = LLVMBuildSIToFP(c->builder, arg, to->type, "");
        else
          result = LLVMBuildUIToFP(c->builder, arg, to->type, "");
      } else if(from->size > to->size) {
          result = LLVMBuildTrunc(c->builder, arg, to->type, "");
      } else if(from->size < to->size) {
        if(from->is_signed)
          result = LLVMBuildSExt(c->builder, arg, to->type, "");
        else
          result = LLVMBuildZExt(c->builder, arg, to->type, "");
      } else {
        result = arg;
      }

      LLVMBuildRet(c->builder, result);
      codegen_finishfun(c);

      if(!has_i128 &&
        ((from->is_float && (to->size > 64)) ||
        (to->is_float && (from->size > 64)))
        )
      {
        LLVMDeleteFunction(fun);
      }
    }
  }
}

typedef struct num_cons_t
{
  const char* type_name;
  LLVMTypeRef type;
  LLVMTypeRef from;
  bool is_float;
} num_cons_t;

static void number_constructors(compile_t* c)
{
  num_cons_t cons[] =
  {
    {"I8", c->i8, c->i8, false},
    {"I16", c->i16, c->i16, false},
    {"I32", c->i32, c->i32, false},
    {"I64", c->i64, c->i64, false},
    {"I128", c->i128, c->i128, false},

    {"U8", c->i8, c->i8, false},
    {"U16", c->i16, c->i16, false},
    {"U32", c->i32, c->i32, false},
    {"U64", c->i64, c->i64, false},
    {"U128", c->i128, c->i128, false},

    {"F32", c->f32, c->f32, true},
    {"F64", c->f64, c->f64, true},

    {NULL, NULL, NULL, false}
  };

  for(num_cons_t* con = cons; con->type_name != NULL; con++)
  {
    const char* name = genname_fun(con->type_name, "create", NULL);
    LLVMTypeRef f_type = LLVMFunctionType(con->type, &con->from, 1, false);
    LLVMValueRef fun = codegen_addfun(c, name, f_type);

    codegen_startfun(c, fun);
    LLVMValueRef arg = LLVMGetParam(fun, 0);
    LLVMValueRef result;

    if(con->type == con->from)
      result = arg;
    else if(con->is_float)
      result = LLVMBuildFPTrunc(c->builder, arg, con->type, "");
    else
      result = LLVMBuildTrunc(c->builder, arg, con->type, "");

    LLVMBuildRet(c->builder, result);
    codegen_finishfun(c);
  }
}

static void special_number_constructors(compile_t* c)
{
  const char* name = genname_fun("F32", "pi", NULL);
  LLVMTypeRef f_type = LLVMFunctionType(c->f32, NULL, 0, false);
  LLVMValueRef fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  LLVMBuildRet(c->builder, LLVMConstReal(c->f32, 3.14159265358979323846));
  codegen_finishfun(c);

  name = genname_fun("F32", "e", NULL);
  f_type = LLVMFunctionType(c->f32, NULL, 0, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  LLVMBuildRet(c->builder, LLVMConstReal(c->f32, 2.71828182845904523536));
  codegen_finishfun(c);

  name = genname_fun("F64", "pi", NULL);
  f_type = LLVMFunctionType(c->f64, NULL, 0, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  LLVMBuildRet(c->builder, LLVMConstReal(c->f64, 3.14159265358979323846));
  codegen_finishfun(c);

  name = genname_fun("F64", "e", NULL);
  f_type = LLVMFunctionType(c->f64, NULL, 0, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  LLVMBuildRet(c->builder, LLVMConstReal(c->f64, 2.71828182845904523536));
  codegen_finishfun(c);
}

static void fp_as_bits(compile_t* c)
{
  const char* name;
  LLVMTypeRef f_type;
  LLVMValueRef fun, result;

  name = genname_fun("F32", "from_bits", NULL);
  f_type = LLVMFunctionType(c->f32, &c->i32, 1, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  result = LLVMBuildBitCast(c->builder, LLVMGetParam(fun, 0), c->f32, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  name = genname_fun("F32", "bits", NULL);
  f_type = LLVMFunctionType(c->i32, &c->f32, 1, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  result = LLVMBuildBitCast(c->builder, LLVMGetParam(fun, 0), c->i32, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  name = genname_fun("F64", "from_bits", NULL);
  f_type = LLVMFunctionType(c->f64, &c->i64, 1, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  result = LLVMBuildBitCast(c->builder, LLVMGetParam(fun, 0), c->f64, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  name = genname_fun("F64", "bits", NULL);
  f_type = LLVMFunctionType(c->i64, &c->f64, 1, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  result = LLVMBuildBitCast(c->builder, LLVMGetParam(fun, 0), c->i64, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void make_cpuid(compile_t* c)
{
  LLVMTypeRef elems[4] = {c->i32, c->i32, c->i32, c->i32};
  LLVMTypeRef r_type = LLVMStructTypeInContext(c->context, elems, 4, false);
  LLVMTypeRef f_type = LLVMFunctionType(r_type, &c->i32, 1, false);
  LLVMValueRef fun = codegen_addfun(c, "internal.x86.cpuid", f_type);
  LLVMSetFunctionCallConv(fun, LLVMCCallConv);
  codegen_startfun(c, fun);

  LLVMValueRef cpuid = LLVMConstInlineAsm(f_type,
    "cpuid", "={ax},={bx},={cx},={dx},{ax}", false, false);
  LLVMValueRef zero = LLVMConstInt(c->i32, 0, false);

  LLVMValueRef result = LLVMBuildCall(c->builder, cpuid, &zero, 1, "");
  LLVMBuildRet(c->builder, result);

  codegen_finishfun(c);
}

static void make_rdtscp(compile_t* c)
{
  // i64 @llvm.x86.rdtscp(i8*)
  LLVMTypeRef f_type = LLVMFunctionType(c->i64, &c->void_ptr, 1, false);
  LLVMValueRef rdtscp = LLVMAddFunction(c->module, "llvm.x86.rdtscp", f_type);

  // i64 @internal.x86.rdtscp(i32*)
  LLVMTypeRef i32_ptr = LLVMPointerType(c->i32, 0);
  f_type = LLVMFunctionType(c->i64, &i32_ptr, 1, false);
  LLVMValueRef fun = codegen_addfun(c, "internal.x86.rdtscp", f_type);
  LLVMSetFunctionCallConv(fun, LLVMCCallConv);
  codegen_startfun(c, fun);

  // Cast i32* to i8* and call the intrinsic.
  LLVMValueRef arg = LLVMGetParam(fun, 0);
  arg = LLVMBuildBitCast(c->builder, arg, c->void_ptr, "");
  LLVMValueRef result = LLVMBuildCall(c->builder, rdtscp, &arg, 1, "");
  LLVMBuildRet(c->builder, result);

  codegen_finishfun(c);
}

void genprim_builtins(compile_t* c)
{
  number_conversions(c);
  number_constructors(c);
  special_number_constructors(c);
  fp_as_bits(c);
  make_cpuid(c);
  make_rdtscp(c);
}
