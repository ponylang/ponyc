#include "codegen.h"
#include "genlib.h"
#include "genexe.h"
#include "genprim.h"
#include "genname.h"
#include "gendesc.h"
#include "../pkg/package.h"
#include "../../libponyrt/mem/pool.h"

#include <platform.h>
#include <llvm-c/Initialization.h>
#include <string.h>
#include <assert.h>

struct compile_local_t
{
  const char* name;
  LLVMValueRef alloca;
};

static uint64_t compile_local_hash(compile_local_t* p)
{
  return hash_ptr(p->name);
}

static bool compile_local_cmp(compile_local_t* a, compile_local_t* b)
{
  return a->name == b->name;
}

static void compile_local_free(compile_local_t* p)
{
  POOL_FREE(compile_local_t, p);
}

DEFINE_HASHMAP(compile_locals, compile_local_t, compile_local_hash,
  compile_local_cmp, pool_alloc_size, pool_free_size, compile_local_free);

static void fatal_error(const char* reason)
{
  printf("%s\n", reason);
  print_errors();
}

static compile_frame_t* push_frame(compile_t* c)
{
  compile_frame_t* frame = POOL_ALLOC(compile_frame_t);
  memset(frame, 0, sizeof(compile_frame_t));
  compile_locals_init(&frame->locals, 0);

  if(c->frame != NULL)
  {
    frame->has_source = c->frame->has_source;
    frame->prev = c->frame;
  }

  c->frame = frame;
  return frame;
}

static void pop_frame(compile_t* c)
{
  compile_frame_t* frame = c->frame;
  compile_locals_destroy(&frame->locals);

  c->frame = frame->prev;
  POOL_FREE(compile_frame_t, frame);
}

static LLVMTargetMachineRef make_machine(pass_opt_t* opt)
{
  LLVMTargetRef target;
  char* err;

  if(LLVMGetTargetFromTriple(opt->triple, &target, &err) != 0)
  {
    errorf(NULL, "couldn't create target: %s", err);
    LLVMDisposeMessage(err);
    return NULL;
  }

  LLVMCodeGenOptLevel opt_level =
    opt->release ? LLVMCodeGenLevelAggressive : LLVMCodeGenLevelNone;

  LLVMRelocMode reloc = opt->library ? LLVMRelocPIC : LLVMRelocDefault;

  LLVMTargetMachineRef machine = LLVMCreateTargetMachine(target, opt->triple,
    opt->cpu, opt->features, opt_level, reloc, LLVMCodeModelDefault);

  if(machine == NULL)
  {
    errorf(NULL, "couldn't create target machine");
    return NULL;
  }

  return machine;
}

static void init_runtime(compile_t* c)
{
  c->str_1 = stringtab("$1");
  c->str_Bool = stringtab("Bool");
  c->str_I8 = stringtab("I8");
  c->str_I16 = stringtab("I16");
  c->str_I32 = stringtab("I32");
  c->str_I64 = stringtab("I64");
  c->str_I128 = stringtab("I128");
  c->str_U8 = stringtab("U8");
  c->str_U16 = stringtab("U16");
  c->str_U32 = stringtab("U32");
  c->str_U64 = stringtab("U64");
  c->str_U128 = stringtab("U128");
  c->str_F32 = stringtab("F32");
  c->str_F64 = stringtab("F64");
  c->str_Pointer = stringtab("Pointer");
  c->str_Array = stringtab("Array");
  c->str_Platform = stringtab("Platform");

  c->str_add = stringtab("add");
  c->str_sub = stringtab("sub");
  c->str_mul = stringtab("mul");
  c->str_div = stringtab("div");
  c->str_mod = stringtab("mod");
  c->str_neg = stringtab("neg");
  c->str_and = stringtab("op_and");
  c->str_or = stringtab("op_or");
  c->str_xor = stringtab("op_xor");
  c->str_not = stringtab("op_not");
  c->str_shl = stringtab("shl");
  c->str_shr = stringtab("shr");
  c->str_eq = stringtab("eq");
  c->str_ne = stringtab("ne");
  c->str_lt = stringtab("lt");
  c->str_le = stringtab("le");
  c->str_ge = stringtab("ge");
  c->str_gt = stringtab("gt");

  LLVMTypeRef type;
  LLVMTypeRef params[4];

  c->void_type = LLVMVoidTypeInContext(c->context);
  c->i1 = LLVMInt1TypeInContext(c->context);
  c->i8 = LLVMInt8TypeInContext(c->context);
  c->i16 = LLVMInt16TypeInContext(c->context);
  c->i32 = LLVMInt32TypeInContext(c->context);
  c->i64 = LLVMInt64TypeInContext(c->context);
  c->i128 = LLVMIntTypeInContext(c->context, 128);
  c->f32 = LLVMFloatTypeInContext(c->context);
  c->f64 = LLVMDoubleTypeInContext(c->context);
  c->intptr = LLVMIntPtrTypeInContext(c->context, c->target_data);

  // i8*
  c->void_ptr = LLVMPointerType(c->i8, 0);

  // forward declare object
  c->object_type = LLVMStructCreateNamed(c->context, "$object");
  c->object_ptr = LLVMPointerType(c->object_type, 0);

  // padding required in an actor between the descriptor and fields
  c->actor_pad = LLVMArrayType(c->i8, 265);

  // message
  params[0] = c->i32; // size
  params[1] = c->i32; // id
  c->msg_type = LLVMStructCreateNamed(c->context, "$message");
  c->msg_ptr = LLVMPointerType(c->msg_type, 0);
  LLVMStructSetBody(c->msg_type, params, 2, false);

  // trace
  // void (*)($object*)
  params[0] = c->object_ptr;
  c->trace_type = LLVMFunctionType(c->void_type, params, 1, false);
  c->trace_fn = LLVMPointerType(c->trace_type, 0);

  // dispatch
  // void (*)($object*, $message*)
  params[0] = c->object_ptr;
  params[1] = c->msg_ptr;
  c->dispatch_type = LLVMFunctionType(c->void_type, params, 2, false);
  c->dispatch_fn = LLVMPointerType(c->dispatch_type, 0);

  // void (*)($object*)
  params[0] = c->object_ptr;
  c->final_fn = LLVMPointerType(
    LLVMFunctionType(c->void_type, params, 1, false), 0);

  // descriptor, opaque version
  // We need this in order to build our own structure.
  const char* desc_name = genname_descriptor(NULL);
  c->descriptor_type = LLVMStructCreateNamed(c->context, desc_name);
  c->descriptor_ptr = LLVMPointerType(c->descriptor_type, 0);

  // field descriptor
  // Also needed to build a descriptor structure.
  params[0] = c->i32;
  params[1] = c->descriptor_ptr;
  c->field_descriptor = LLVMStructTypeInContext(c->context, params, 2, false);

  // descriptor, filled in
  c->descriptor_type = gendesc_type(c, NULL);

  // define object
  params[0] = c->descriptor_ptr;
  LLVMStructSetBody(c->object_type, params, 1, false);

  // $object* pony_create($desc*)
  params[0] = c->descriptor_ptr;
  type = LLVMFunctionType(c->object_ptr, params, 1, false);
  LLVMAddFunction(c->module, "pony_create", type);

  // void pony_sendv($object*, $message*);
  params[0] = c->object_ptr;
  params[1] = c->msg_ptr;
  type = LLVMFunctionType(c->void_type, params, 2, false);
  LLVMAddFunction(c->module, "pony_sendv", type);

  // i8* pony_alloc(i64)
  params[0] = c->i64;
  type = LLVMFunctionType(c->void_ptr, params, 1, false);
  LLVMAddFunction(c->module, "pony_alloc", type);

  // i8* pony_realloc(i8*, i64)
  params[0] = c->void_ptr;
  params[1] = c->i64;
  type = LLVMFunctionType(c->void_ptr, params, 2, false);
  LLVMAddFunction(c->module, "pony_realloc", type);

  // $message* pony_alloc_msg(i32, i32)
  params[0] = c->i32;
  params[1] = c->i32;
  type = LLVMFunctionType(c->msg_ptr, params, 2, false);
  LLVMAddFunction(c->module, "pony_alloc_msg", type);

  // void pony_trace(i8*)
  params[0] = c->void_ptr;
  type = LLVMFunctionType(c->void_type, params, 1, false);
  LLVMAddFunction(c->module, "pony_trace", type);

  // void pony_traceactor($object*)
  params[0] = c->object_ptr;
  type = LLVMFunctionType(c->void_type, params, 1, false);
  LLVMAddFunction(c->module, "pony_traceactor", type);

  // void pony_traceobject($object*, trace_fn)
  params[0] = c->object_ptr;
  params[1] = c->trace_fn;
  type = LLVMFunctionType(c->void_type, params, 2, false);
  LLVMAddFunction(c->module, "pony_traceobject", type);

  // void pony_traceunknown($object*)
  params[0] = c->object_ptr;
  type = LLVMFunctionType(c->void_type, params, 1, false);
  LLVMAddFunction(c->module, "pony_traceunknown", type);

  // void pony_trace_tag_or_actor($object*)
  params[0] = c->object_ptr;
  type = LLVMFunctionType(c->void_type, params, 1, false);
  LLVMAddFunction(c->module, "pony_trace_tag_or_actor", type);

  // void pony_gc_send()
  type = LLVMFunctionType(c->void_type, NULL, 0, false);
  LLVMAddFunction(c->module, "pony_gc_send", type);

  // void pony_gc_recv()
  type = LLVMFunctionType(c->void_type, NULL, 0, false);
  LLVMAddFunction(c->module, "pony_gc_recv", type);

  // void pony_send_done()
  type = LLVMFunctionType(c->void_type, NULL, 0, false);
  LLVMAddFunction(c->module, "pony_send_done", type);

  // void pony_recv_done()
  type = LLVMFunctionType(c->void_type, NULL, 0, false);
  LLVMAddFunction(c->module, "pony_recv_done", type);

  // i32 pony_init(i32, i8**)
  params[0] = c->i32;
  params[1] = LLVMPointerType(c->void_ptr, 0);
  type = LLVMFunctionType(c->i32, params, 2, false);
  LLVMAddFunction(c->module, "pony_init", type);

  // void pony_become($object*)
  params[0] = c->object_ptr;
  type = LLVMFunctionType(c->void_type, params, 1, false);
  LLVMAddFunction(c->module, "pony_become", type);

  // i32 pony_start(i32)
  params[0] = c->i32;
  type = LLVMFunctionType(c->i32, params, 1, false);
  LLVMAddFunction(c->module, "pony_start", type);

  // void pony_throw()
  type = LLVMFunctionType(c->void_type, NULL, 0, false);
  LLVMAddFunction(c->module, "pony_throw", type);

  // i32 pony_personality_v0(...)
  type = LLVMFunctionType(c->i32, NULL, 0, true);
  c->personality = LLVMAddFunction(c->module, "pony_personality_v0", type);

  // i8* memcpy(...)
  type = LLVMFunctionType(c->void_ptr, NULL, 0, true);
  LLVMAddFunction(c->module, "memcpy", type);

  // i8* memmove(...)
  type = LLVMFunctionType(c->void_ptr, NULL, 0, true);
  LLVMAddFunction(c->module, "memmove", type);
}

static void init_module(compile_t* c, ast_t* program, pass_opt_t* opt)
{
  c->opt = opt;

  // Get the first package and the builtin package.
  ast_t* package = ast_child(program);
  ast_t* builtin = ast_sibling(package);

  // If we have only one package, we are compiling builtin itself.
  if(builtin == NULL)
    builtin = package;

  c->reachable = reach_new();
  reach_primitives(c->reachable, opt, builtin);

  // The name of the first package is the name of the program.
  c->filename = package_filename(package);

  // LLVM context and machine settings.
  c->context = LLVMContextCreate();
  c->machine = make_machine(opt);
  c->target_data = LLVMGetTargetMachineData(c->machine);

  // Create a module.
  c->module = LLVMModuleCreateWithNameInContext(c->filename, c->context);

  // Set the target triple.
  LLVMSetTarget(c->module, opt->triple);

  // IR builder.
  c->builder = LLVMCreateBuilderInContext(c->context);

  // Pass manager builder.
  c->pmb = LLVMPassManagerBuilderCreate();
  LLVMPassManagerBuilderSetOptLevel(c->pmb, opt->release ? 3 : 0);

  if(opt->release)
    LLVMPassManagerBuilderUseInlinerWithThreshold(c->pmb, 275);

  // Module pass manager.
  c->mpm = LLVMCreatePassManager();
  LLVMAddTargetData(c->target_data, c->mpm);
  LLVMPassManagerBuilderPopulateModulePassManager(c->pmb, c->mpm);

  // LTO pass manager.
  c->lpm = LLVMCreatePassManager();
  LLVMAddTargetData(c->target_data, c->lpm);
  LLVMPassManagerBuilderPopulateLTOPassManager(c->pmb, c->lpm, true, true);

  // Function pass manager.
  c->fpm = LLVMCreateFunctionPassManagerForModule(c->module);
  LLVMAddTargetData(c->target_data, c->fpm);
  LLVMPassManagerBuilderPopulateFunctionPassManager(c->pmb, c->fpm);
  LLVMInitializeFunctionPassManager(c->fpm);

  // Empty frame stack.
  c->frame = NULL;
}

static void codegen_cleanup(compile_t* c)
{
  while(c->frame != NULL)
    pop_frame(c);

  LLVMDisposePassManager(c->fpm);
  LLVMDisposePassManager(c->mpm);
  LLVMDisposePassManager(c->lpm);
  LLVMPassManagerBuilderDispose(c->pmb);
  LLVMDisposeBuilder(c->builder);
  LLVMDisposeModule(c->module);
  LLVMContextDispose(c->context);
  LLVMDisposeTargetMachine(c->machine);
  reach_free(c->reachable);
}

bool codegen_init(pass_opt_t* opt)
{
  LLVMInitializeNativeTarget();
  LLVMInitializeAllTargets();
  LLVMInitializeAllTargetMCs();
  LLVMInitializeAllAsmPrinters();
  LLVMInitializeAllAsmParsers();
  LLVMEnablePrettyStackTrace();
  LLVMInstallFatalErrorHandler(fatal_error);

  LLVMPassRegistryRef passreg = LLVMGetGlobalPassRegistry();
  LLVMInitializeCore(passreg);
  LLVMInitializeTransformUtils(passreg);
  LLVMInitializeScalarOpts(passreg);
  LLVMInitializeObjCARCOpts(passreg);
  LLVMInitializeVectorization(passreg);
  LLVMInitializeInstCombine(passreg);
  LLVMInitializeIPO(passreg);
  LLVMInitializeInstrumentation(passreg);
  LLVMInitializeAnalysis(passreg);
  LLVMInitializeIPA(passreg);
  LLVMInitializeCodeGen(passreg);
  LLVMInitializeTarget(passreg);

  // Default triple, cpu and features.
  if(opt->triple != NULL)
    opt->triple = LLVMCreateMessage(opt->triple);
  else
    opt->triple = LLVMGetDefaultTargetTriple();

  if(opt->cpu != NULL)
    opt->cpu = LLVMCreateMessage(opt->cpu);
  else
    opt->cpu = LLVMGetHostCPUName();

  if(opt->features != NULL)
    opt->features = LLVMCreateMessage(opt->features);
  else
    opt->features = LLVMCreateMessage("");

  return true;
}

void codegen_shutdown(pass_opt_t* opt)
{
  LLVMDisposeMessage(opt->triple);
  LLVMDisposeMessage(opt->cpu);
  LLVMDisposeMessage(opt->features);

  LLVMShutdown();
}

bool codegen(ast_t* program, pass_opt_t* opt)
{
  printf("Generating\n");

  compile_t c;
  memset(&c, 0, sizeof(compile_t));

  init_module(&c, program, opt);
  init_runtime(&c);
  genprim_builtins(&c);

  // Emit debug info for this compile unit.
  dwarf_init(&c.dwarf, c.opt, c.target_data, c.module);
  dwarf_compileunit(&c.dwarf, program);

  bool ok;

  if(c.opt->library)
    ok = genlib(&c, program);
  else
    ok = genexe(&c, program);

  codegen_cleanup(&c);
  return ok;
}

LLVMValueRef codegen_addfun(compile_t* c, const char* name, LLVMTypeRef type)
{
  // Add the function and set the calling convention.
  LLVMValueRef fun = LLVMAddFunction(c->module, name, type);

  if(!c->opt->library)
    LLVMSetFunctionCallConv(fun, GEN_CALLCONV);

  if(c->opt->no_restrict)
    return fun;

  // Set the noalias attribute on all arguments. This is fortran-like semantics
  // for parameter aliasing, similar to C restrict.
  LLVMValueRef arg = LLVMGetFirstParam(fun);

  while(arg != NULL)
  {
    LLVMTypeRef type = LLVMTypeOf(arg);

    if(LLVMGetTypeKind(type) == LLVMPointerTypeKind)
      LLVMAddAttribute(arg, LLVMNoAliasAttribute);

    arg = LLVMGetNextParam(arg);
  }

  return fun;
}

void codegen_startfun(compile_t* c, LLVMValueRef fun, bool has_source)
{
  compile_frame_t* frame = push_frame(c);

  frame->fun = fun;
  frame->restore_builder = LLVMGetInsertBlock(c->builder);
  frame->has_source = has_source;

  if(LLVMCountBasicBlocks(fun) == 0)
  {
    LLVMBasicBlockRef block = codegen_block(c, "entry");
    LLVMPositionBuilderAtEnd(c->builder, block);
  }
}

void codegen_finishfun(compile_t* c)
{
  if(c->frame->restore_builder != NULL)
    LLVMPositionBuilderAtEnd(c->builder, c->frame->restore_builder);

  pop_frame(c);
}

void codegen_pushloop(compile_t* c, LLVMBasicBlockRef continue_target,
  LLVMBasicBlockRef break_target)
{
  compile_frame_t* frame = push_frame(c);

  frame->fun = frame->prev->fun;
  frame->restore_builder = frame->prev->restore_builder;
  frame->break_target = break_target;
  frame->continue_target = continue_target;
  frame->invoke_target = frame->prev->invoke_target;
}

void codegen_poploop(compile_t* c)
{
  pop_frame(c);
}

void codegen_pushtry(compile_t* c, LLVMBasicBlockRef invoke_target)
{
  compile_frame_t* frame = push_frame(c);

  frame->fun = frame->prev->fun;
  frame->restore_builder = frame->prev->restore_builder;
  frame->break_target = frame->prev->break_target;
  frame->continue_target = frame->prev->continue_target;
  frame->invoke_target = invoke_target;
}

void codegen_poptry(compile_t* c)
{
  pop_frame(c);
}

LLVMValueRef codegen_getlocal(compile_t* c, const char* name)
{
  compile_frame_t* frame = c->frame;

  compile_local_t k;
  k.name = name;

  while(frame != NULL)
  {
    compile_local_t* p = compile_locals_get(&frame->locals, &k);

    if(p != NULL)
      return p->alloca;

    frame = frame->prev;
  }

  return NULL;
}

void codegen_setlocal(compile_t* c, const char* name, LLVMValueRef alloca)
{
  compile_local_t* p = POOL_ALLOC(compile_local_t);
  p->name = name;
  p->alloca = alloca;

  compile_locals_put(&c->frame->locals, p);
}

LLVMValueRef codegen_fun(compile_t* c)
{
  return c->frame->fun;
}

LLVMBasicBlockRef codegen_block(compile_t* c, const char* name)
{
  return LLVMAppendBasicBlockInContext(c->context, c->frame->fun, name);
}

LLVMValueRef codegen_call(compile_t* c, LLVMValueRef fun, LLVMValueRef* args,
  size_t count)
{
  LLVMValueRef result = LLVMBuildCall(c->builder, fun, args, (int)count, "");

  if(!c->opt->library)
    LLVMSetInstructionCallConv(result, GEN_CALLCONV);

  return result;
}

bool codegen_hassource(compile_t* c)
{
  return c->frame->has_source;
}

const char* suffix_filename(const char* dir, const char* file,
  const char* extension)
{
  // Copy to a string with space for a suffix.
  size_t len = strlen(dir) + strlen(file) + strlen(extension) + 3;
  VLA(char, filename, len + 1);

  // Start with no suffix.
  snprintf(filename, len, "%s/%s%s", dir, file, extension);
  int suffix = 0;

  while(suffix < 100)
  {
    // Overwrite files but not directories.
    struct stat s;
    int err = stat(filename, &s);

    if((err == -1) || !S_ISDIR(s.st_mode))
      break;

    snprintf(filename, len, "%s/%s%d%s", dir, file, ++suffix, extension);
  }

  if(suffix >= 100)
  {
    errorf(NULL, "couldn't pick an unused file name");
    return NULL;
  }

  return stringtab(filename);
}
