#include "genprim.h"
#include "genname.h"
#include "gentype.h"
#include "gencall.h"
#include "../pkg/platformfuns.h"

bool genprim_pointer(compile_t* c, gentype_t* g, bool prelim)
{
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

  // Set up a constant integer for the allocation size.
  size_t size = LLVMABISizeOfType(c->target_data, elem_g.use_type);
  LLVMValueRef l_size = LLVMConstInt(c->i64, size, false);

  // create
  const char* name = genname_fun(g->type_name, "create", NULL);
  LLVMValueRef fun = LLVMGetNamedFunction(c->module, name);

  // If there's already a create function, we're done.
  if(fun != NULL)
    return true;

  LLVMTypeRef params[4];
  params[0] = c->i64;

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, params, 1, false);
  fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun);

  LLVMValueRef len = LLVMGetParam(fun, 0);

  LLVMValueRef args[4];
  args[0] = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef result = gencall_runtime(c, "pony_alloc", args, 1, "");
  result = LLVMBuildBitCast(c->builder, result, g->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  // realloc
  name = genname_fun(g->type_name, "_realloc", NULL);

  params[0] = g->use_type;
  params[1] = c->i64;

  ftype = LLVMFunctionType(g->use_type, params, 2, false);
  fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun);

  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  args[0] = LLVMBuildBitCast(c->builder, ptr, c->void_ptr, "");

  len = LLVMGetParam(fun, 1);
  args[1] = LLVMBuildMul(c->builder, len, l_size, "");

  result = gencall_runtime(c, "pony_realloc", args, 2, "");
  result = LLVMBuildBitCast(c->builder, result, g->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  // apply
  name = genname_fun(g->type_name, "_apply", NULL);

  params[0] = g->use_type;
  params[1] = c->i64;

  ftype = LLVMFunctionType(elem_g.use_type, params, 2, false);
  fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun);

  ptr = LLVMGetParam(fun, 0);
  LLVMValueRef index = LLVMGetParam(fun, 1);
  LLVMValueRef loc = LLVMBuildGEP(c->builder, ptr, &index, 1, "");
  result = LLVMBuildLoad(c->builder, loc, "");
  result = LLVMBuildBitCast(c->builder, result, elem_g.use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  // update
  name = genname_fun(g->type_name, "_update", NULL);

  params[0] = g->use_type;
  params[1] = c->i64;
  params[2] = elem_g.use_type;

  ftype = LLVMFunctionType(elem_g.use_type, params, 3, false);
  fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun);

  ptr = LLVMGetParam(fun, 0);
  index = LLVMGetParam(fun, 1);
  loc = LLVMBuildGEP(c->builder, ptr, &index, 1, "");
  result = LLVMBuildLoad(c->builder, loc, "");
  result = LLVMBuildBitCast(c->builder, result, elem_g.use_type, "");
  LLVMBuildStore(c->builder, LLVMGetParam(fun, 2), loc);

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  // copy
  name = genname_fun(g->type_name, "_copy", NULL);

  params[0] = g->use_type;
  params[1] = c->i64;
  params[2] = g->use_type;
  params[3] = c->i64;

  ftype = LLVMFunctionType(c->i64, params, 4, false);
  fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun);

  LLVMValueRef base = LLVMGetParam(fun, 0);
  base = LLVMBuildPtrToInt(c->builder, base, c->i64, "");
  LLVMValueRef offset = LLVMGetParam(fun, 1);
  base = LLVMBuildAdd(c->builder, base, offset, "");

  args[0] = LLVMBuildIntToPtr(c->builder, base, c->void_ptr, "");
  args[1] = LLVMBuildBitCast(c->builder, LLVMGetParam(fun, 2), c->void_ptr, "");
  args[2] = LLVMGetParam(fun, 3);
  gencall_runtime(c, "memcpy", args, 3, "");

  LLVMBuildRet(c->builder, args[2]);
  codegen_finishfun(c);

  return true;
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
  LLVMSetValueName(arg, "arg");

  LLVMBasicBlockRef cond_block = codegen_block(c, "cond");
  LLVMBasicBlockRef body_block = codegen_block(c, "body");
  LLVMBasicBlockRef post_block = codegen_block(c, "post");

  // Read the count and the base pointer.
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, g->use_type, "array");
  LLVMValueRef count_ptr = LLVMBuildStructGEP(c->builder, object, 2, "");
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

  // The phi node is the index. Get the address and trace it.
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef elem = LLVMBuildGEP(c->builder, pointer, &phi, 1, "elem");
  gencall_trace(c, elem, typearg);

  // Add one to the phi node and branch back to the cond block.
  LLVMValueRef one = LLVMConstInt(c->i64, 1, false);
  LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
  LLVMAddIncoming(phi, &inc, &body_block, 1);
  LLVMBuildBr(c->builder, cond_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMBuildRetVoid(c->builder);

  codegen_finishfun(c);
}

void genprim_platform(compile_t* c, gentype_t* g)
{
  LLVMTypeRef f_type = LLVMFunctionType(c->i1, &g->use_type, 1, false);

  const char* name = genname_fun(g->type_name, OS_LINUX_NAME, NULL);
  LLVMValueRef fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
#ifdef PLATFORM_IS_LINUX
  LLVMValueRef result = LLVMConstInt(c->i1, 1, false);
#else
  LLVMValueRef result = LLVMConstInt(c->i1, 0, false);
#endif
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  name = genname_fun(g->type_name, OS_MACOSX_NAME, NULL);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
#ifdef PLATFORM_IS_MACOSX
  result = LLVMConstInt(c->i1, 1, false);
#else
  result = LLVMConstInt(c->i1, 0, false);
#endif
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  name = genname_fun(g->type_name, OS_WINDOWS_NAME, NULL);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
#ifdef PLATFORM_IS_WINDOWS
  result = LLVMConstInt(c->i1, 1, false);
#else
  result = LLVMConstInt(c->i1, 0, false);
#endif
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  name = genname_fun(g->type_name, OS_DEBUG_NAME, NULL);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  result = LLVMConstInt(c->i1, !c->release, false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

typedef struct prim_conv_t
{
  const char* type_name;
  const char* fun_name;
  LLVMTypeRef type;
  int size;
  bool is_signed;
  bool is_float;
} prim_conv_t;

static void primitive_conversions(compile_t* c)
{
  prim_conv_t conv[] =
  {
    {"$1_I8", "i8", c->i8, 8, true, false},
    {"$1_I16", "i16", c->i16, 16, true, false},
    {"$1_I32", "i32", c->i32, 32, true, false},
    {"$1_I64", "i64", c->i64, 64, true, false},
    {"$1_I128", "i128", c->i128, 128, true, false},

    {"$1_U8", "u8", c->i8, 8, false, false},
    {"$1_U16", "u16", c->i16, 16, false, false},
    {"$1_U32", "u32", c->i32, 32, false, false},
    {"$1_U64", "u64", c->i64, 64, false, false},
    {"$1_U128", "u128", c->i128, 128, false, false},

    {"$1_F32", "f32", c->f32, 32, false, true},
    {"$1_F64", "f64", c->f64, 64, false, true},

    {"$1_SIntLiteral", NULL, c->i128, 128, true, false},
    {"$1_UIntLiteral", NULL, c->i128, 128, false, false},

    {NULL, NULL, NULL, false, false}
  };

  for(prim_conv_t* from = conv; from->type_name != NULL; from++)
  {
    for(prim_conv_t* to = conv; to->type_name != NULL; to++)
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
#ifdef PLATFORM_IS_WINDOWS
        } else if(to->size > 64) {
          // TODO: Windows runtime doesn't have float to 128 bit conversion.
          result = LLVMGetUndef(to->type);
#endif
        } else if(to->is_signed) {
          result = LLVMBuildFPToSI(c->builder, arg, to->type, "");
        } else {
          result = LLVMBuildFPToUI(c->builder, arg, to->type, "");
        }
      } else if(to->is_float) {
#ifdef PLATFORM_IS_WINDOWS
        if(from->size > 64)
        {
          // TODO: Windows runtime doesn't have 128 bit to float conversion.
          result = LLVMGetUndef(to->type);
        } else
#endif
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
    }
  }
}

static void special_number_constructors(compile_t* c)
{
  const char* name = genname_fun("$1_F32", "pi", NULL);
  LLVMTypeRef f_type = LLVMFunctionType(c->f32, NULL, 0, false);
  LLVMValueRef fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  LLVMBuildRet(c->builder, LLVMConstReal(c->f32, 3.14159265358979323846));
  codegen_finishfun(c);

  name = genname_fun("$1_F32", "e", NULL);
  f_type = LLVMFunctionType(c->f32, NULL, 0, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  LLVMBuildRet(c->builder, LLVMConstReal(c->f32, 2.71828182845904523536));
  codegen_finishfun(c);

  name = genname_fun("$1_F64", "pi", NULL);
  f_type = LLVMFunctionType(c->f64, NULL, 0, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  LLVMBuildRet(c->builder, LLVMConstReal(c->f64, 3.14159265358979323846));
  codegen_finishfun(c);

  name = genname_fun("$1_F64", "e", NULL);
  f_type = LLVMFunctionType(c->f64, NULL, 0, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  LLVMBuildRet(c->builder, LLVMConstReal(c->f64, 2.71828182845904523536));
  codegen_finishfun(c);
}

static void fp_as_bits(compile_t* c)
{
  const char* name = genname_fun("$1_F32", "from_bits", NULL);
  LLVMTypeRef f_type = LLVMFunctionType(c->f32, &c->i32, 1, false);
  LLVMValueRef fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(fun, 0),
    c->f32, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  name = genname_fun("$1_F32", "bits", NULL);
  f_type = LLVMFunctionType(c->i32, &c->f32, 1, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  result = LLVMBuildBitCast(c->builder, LLVMGetParam(fun, 0), c->i32, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  name = genname_fun("$1_F64", "from_bits", NULL);
  f_type = LLVMFunctionType(c->f64, &c->i64, 1, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  result = LLVMBuildBitCast(c->builder, LLVMGetParam(fun, 0), c->f64, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  name = genname_fun("$1_F64", "bits", NULL);
  f_type = LLVMFunctionType(c->i64, &c->f64, 1, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun);
  result = LLVMBuildBitCast(c->builder, LLVMGetParam(fun, 0), c->i64, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

void genprim_builtins(compile_t* c)
{
  primitive_conversions(c);
  special_number_constructors(c);
  fp_as_bits(c);
}
