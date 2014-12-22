#include "codegen.h"
#include "genlib.h"
#include "genexe.h"
#include "genprim.h"
#include "genname.h"
#include "gendesc.h"
#include "../debug/dwarf.h"
#include "../pkg/package.h"
#include "../../libponyrt/mem/pool.h"

#include <platform.h>
#include <llvm-c/Initialization.h>
#include <string.h>
#include <assert.h>

static LLVMPassManagerBuilderRef pmb;
static LLVMPassManagerRef mpm;
static LLVMPassManagerRef lpm;

static void fatal_error(const char* reason)
{
  printf("%s\n", reason);
  print_errors();
}

static compile_frame_t* push_frame(compile_t* c)
{
  compile_frame_t* frame = POOL_ALLOC(compile_frame_t);
  memset(frame, 0, sizeof(compile_frame_t));

  frame->prev = c->frame;
  c->frame = frame;

  return frame;
}

static void pop_frame(compile_t* c)
{
  compile_frame_t* frame = c->frame;
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

static int behaviour_index(gentype_t* g, const char* name)
{
  ast_t* def = (ast_t*)ast_data(g->ast);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);
  int index = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_NEW:
      case TK_BE:
      {
        AST_GET_CHILDREN(member, ignore, id);

        if(ast_name(id) == name)
          return index;

        index++;
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  return -1;
}

static void codegen_main(compile_t* c, gentype_t* main_g, gentype_t* env_g)
{
  LLVMTypeRef params[2];
  params[0] = c->i32;
  params[1] = LLVMPointerType(LLVMPointerType(c->i8, 0), 0);

  LLVMTypeRef ftype = LLVMFunctionType(c->i32, params, 2, false);
  LLVMValueRef func = LLVMAddFunction(c->module, "main", ftype);

  codegen_startfun(c, func);

  LLVMValueRef args[3];
  args[0] = LLVMGetParam(func, 0);
  LLVMSetValueName(args[0], "argc");

  args[1] = LLVMGetParam(func, 1);
  LLVMSetValueName(args[1], "argv");

  // Initialise the pony runtime with argc and argv, getting a new argc.
  args[0] = gencall_runtime(c, "pony_init", args, 2, "argc");

  // Create the main actor and become it.
  LLVMValueRef m = gencall_create(c, main_g);
  LLVMValueRef object = LLVMBuildBitCast(c->builder, m, c->object_ptr, "");
  gencall_runtime(c, "pony_become", &object, 1, "");

  // Create an Env on the main actor's heap.
  const char* env_name = "Env";
  const char* env_create = genname_fun(env_name, "_create", NULL);
  args[2] = args[1];
  args[1] = LLVMBuildZExt(c->builder, args[0], c->i64, "");
  args[0] = gencall_alloc(c, env_g);
  LLVMValueRef env = gencall_runtime(c, env_create, args, 3, "env");
  LLVMSetInstructionCallConv(env, GEN_CALLCONV);

  // Create a type for the message.
  LLVMTypeRef f_params[4];
  f_params[0] = c->i32;
  f_params[1] = c->i32;
  f_params[2] = c->void_ptr;
  f_params[3] = LLVMTypeOf(env);
  LLVMTypeRef msg_type = LLVMStructTypeInContext(c->context, f_params, 4,
    false);
  LLVMTypeRef msg_type_ptr = LLVMPointerType(msg_type, 0);

  // Allocate the message, setting its size and ID.
  int index = behaviour_index(main_g, stringtab("create"));
  args[1] = LLVMConstInt(c->i32, 0, false);
  args[0] = LLVMConstInt(c->i32, index, false);
  LLVMValueRef msg = gencall_runtime(c, "pony_alloc_msg", args, 2, "");
  LLVMValueRef msg_ptr = LLVMBuildBitCast(c->builder, msg, msg_type_ptr, "");

  // Set the message contents.
  LLVMValueRef env_ptr = LLVMBuildStructGEP(c->builder, msg_ptr, 3, "");
  LLVMBuildStore(c->builder, env, env_ptr);

  // Trace the message.
  gencall_runtime(c, "pony_gc_send", NULL, 0, "");
  const char* env_trace = genname_trace(env_name);

  args[0] = LLVMBuildBitCast(c->builder, env, c->object_ptr, "");
  args[1] = LLVMGetNamedFunction(c->module, env_trace);
  gencall_runtime(c, "pony_traceobject", args, 2, "");
  gencall_runtime(c, "pony_send_done", NULL, 0, "");

  // Send the message.
  args[0] = object;
  args[1] = msg;
  gencall_runtime(c, "pony_sendv", args, 2, "");

  // Start the runtime.
  LLVMValueRef zero = LLVMConstInt(c->i32, 0, false);
  LLVMValueRef rc = gencall_runtime(c, "pony_start", &zero, 1, "");

  // Return the runtime exit code.
  LLVMBuildRet(c->builder, rc);

  codegen_finishfun(c);

  // External linkage for main().
  LLVMSetLinkage(func, LLVMExternalLinkage);
}

static bool codegen_program(compile_t* c, ast_t* program)
{
  // The first package is the main package. It has to have a Main actor.
  const char* main_actor = stringtab("Main");
  ast_t* package = ast_child(program);
  ast_t* main_def = ast_get(package, main_actor, NULL);

  if(main_def == NULL)
  {
    errorf(NULL, "no Main actor found in package '%s'", c->filename);
    return false;
  }

  // Generate the Main actor and the Env class.
  gentype_t main_g;
  ast_t* main_ast = genprim(c, main_def, main_actor, &main_g);

  if(main_ast == NULL)
    return false;

  const char* env_class = stringtab("Env");

  gentype_t env_g;
  ast_t* env_ast = genprim(c, main_def, env_class, &env_g);

  if(env_ast == NULL)
  {
    ast_free_unattached(main_ast);
    return false;
  }

  codegen_main(c, &main_g, &env_g);

  ast_free_unattached(main_ast);
  ast_free_unattached(env_ast);
  return true;
}

static bool generate_actors(compile_t* c, ast_t* program)
{
  // Look for C-API actors in every package.
  bool found = false;
  ast_t* package = ast_child(program);

  while(package != NULL)
  {
    ast_t* module = ast_child(package);

    while(module != NULL)
    {
      ast_t* entity = ast_child(module);

      while(entity != NULL)
      {
        if(ast_id(entity) == TK_ACTOR)
        {
          ast_t* c_api = ast_childidx(entity, 5);

          if(ast_id(c_api) == TK_AT)
          {
            // We have an actor marked as C-API.
            ast_t* id = ast_child(entity);

            // Generate the actor.
            gentype_t g;
            ast_t* ast = genprim(c, entity, ast_name(id), &g);

            if(ast == NULL)
              return false;

            found = true;
          }
        }

        entity = ast_sibling(entity);
      }

      module = ast_sibling(module);
    }

    package = ast_sibling(package);
  }

  if(!found)
  {
    errorf(NULL, "no C-API actors found in package '%s'", c->filename);
    return false;
  }

  return true;
}

static bool codegen_library(compile_t* c, pass_opt_t* opt, ast_t* program)
{
  // Open a header file.
  const char* file_h = suffix_filename(opt->output, c->filename, ".h");
  c->header = fopen(file_h, "wt");

  if(c->header == NULL)
  {
    errorf(NULL, "couldn't write to %s", file_h);
    return false;
  }

  fprintf(c->header,
    "#ifndef pony_%s_h\n"
    "#define pony_%s_h\n"
    "\n"
    "/* This is an auto-generated header file. Do not edit. */\n"
    "\n"
    "#include <stdint.h>\n"
    "#include <stdbool.h>\n"
    "\n"
    "#ifdef __cplusplus\n"
    "extern \"C\" {\n"
    "#endif\n"
    "\n"
    "#ifdef _MSC_VER\n"
    "typedef struct __int128_t { uint64_t low; int64_t high; } __int128_t;\n"
    "typedef struct __uint128_t { uint64_t low; uint64_t high; } __uint128_t;\n"
    "#endif\n"
    "\n",
    c->filename,
    c->filename
    );

  c->header_buf = printbuf_new();
  bool ok = generate_actors(c, program);

  fwrite(c->header_buf->m, 1, c->header_buf->offset, c->header);
  printbuf_free(c->header_buf);
  c->header_buf = NULL;

  fprintf(c->header,
    "\n"
    "#ifdef __cplusplus\n"
    "}\n"
    "#endif\n"
    "\n"
    "#endif\n"
    );

  fclose(c->header);
  c->header = NULL;

  if(!ok)
    unlink(file_h);

  return ok;
}

static void init_module(compile_t* c, ast_t* program, pass_opt_t* opt)
{
  c->mpm = mpm;
  c->lpm = lpm;

  c->painter = painter_create();
  painter_colour(c->painter, program);

  // The name of the first package is the name of the program.
  c->filename = package_filename(ast_child(program));

  // Keep track of whether or not we're optimising or emitting debug symbols.
  c->release = opt->release;
  c->library = opt->library;
  c->symbols = opt->symbols;
  c->ieee_math = opt->ieee_math;
  c->no_restrict = opt->no_restrict;

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

  // Function pass manager.
  c->fpm = LLVMCreateFunctionPassManagerForModule(c->module);
  LLVMAddTargetData(c->target_data, c->fpm);
  LLVMPassManagerBuilderPopulateFunctionPassManager(pmb, c->fpm);
  LLVMInitializeFunctionPassManager(c->fpm);

  // Empty frame stack.
  c->frame = NULL;
}

static void codegen_cleanup(compile_t* c)
{
  while(c->frame != NULL)
    pop_frame(c);

  LLVMDisposePassManager(c->fpm);
  LLVMDisposeBuilder(c->builder);
  LLVMDisposeModule(c->module);
  LLVMContextDispose(c->context);
  LLVMDisposeTargetMachine(c->machine);
  painter_free(c->painter);
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

  LLVMTargetMachineRef machine = make_machine(opt);

  if(machine == NULL)
  {
    errorf(NULL, "couldn't create target machine");
    return false;
  }

  LLVMTargetDataRef target_data = LLVMGetTargetMachineData(machine);

  // Pass manager builder.
  pmb = LLVMPassManagerBuilderCreate();
  LLVMPassManagerBuilderSetOptLevel(pmb, opt->release ? 3 : 0);

  if(opt->release)
    LLVMPassManagerBuilderUseInlinerWithThreshold(pmb, 275);

  // Module pass manager.
  mpm = LLVMCreatePassManager();
  LLVMAddTargetData(target_data, mpm);
  LLVMPassManagerBuilderPopulateModulePassManager(pmb, mpm);

  // LTO pass manager.
  lpm = LLVMCreatePassManager();
  LLVMAddTargetData(target_data, lpm);
  LLVMPassManagerBuilderPopulateLTOPassManager(pmb, lpm, true, true);

  LLVMDisposeTargetMachine(machine);
  return true;
}

void codegen_shutdown(pass_opt_t* opt)
{
  LLVMDisposePassManager(mpm);
  LLVMDisposePassManager(lpm);
  LLVMPassManagerBuilderDispose(pmb);

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
<<<<<<< HEAD
=======
  // dwarf_init(&c);
>>>>>>> 2432710b57adca66e8f2f68951c9705b6f317a20
  genprim_builtins(&c);

  // Emit debug info for this compile unit.
  dwarf_init(&c);
  dwarf_compileunit(c.dwarf, program);

  bool ok;

  if(c.library)
    ok = genlib(&c, opt, program);
  else
    ok = genexe(&c, opt, program);

<<<<<<< HEAD
  if(ok)
  {
    ok = dwarf_finalise(c.dwarf);
    ok &= codegen_finalise(program, &c, opt, pass_limit);
  }

  dwarf_cleanup(&c.dwarf);
=======
  // dwarf_cleanup(&c.dwarf);
>>>>>>> 2432710b57adca66e8f2f68951c9705b6f317a20
  codegen_cleanup(&c);
  return ok;
}

LLVMValueRef codegen_addfun(compile_t*c, const char* name, LLVMTypeRef type)
{
  // Add the function and set the calling convention.
  LLVMValueRef fun = LLVMAddFunction(c->module, name, type);

  if(!c->library)
    LLVMSetFunctionCallConv(fun, GEN_CALLCONV);

  if(c->no_restrict)
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

void codegen_startfun(compile_t* c, LLVMValueRef fun)
{
  compile_frame_t* frame = push_frame(c);

  frame->fun = fun;
  frame->restore_builder = LLVMGetInsertBlock(c->builder);

  if(LLVMCountBasicBlocks(fun) == 0)
  {
    LLVMBasicBlockRef block = codegen_block(c, "entry");
    LLVMPositionBuilderAtEnd(c->builder, block);
  }
}

void codegen_pausefun(compile_t* c)
{
  if(c->frame->restore_builder != NULL)
    LLVMPositionBuilderAtEnd(c->builder, c->frame->restore_builder);

  pop_frame(c);
}

void codegen_finishfun(compile_t* c)
{
  compile_frame_t* frame = c->frame;

#ifndef NDEBUG
  if(LLVMVerifyFunction(frame->fun, LLVMPrintMessageAction) == 0)
  {
    LLVMRunFunctionPassManager(c->fpm, frame->fun);
  } else {
    LLVMDumpValue(frame->fun);
  }
#else
  LLVMRunFunctionPassManager(c->fpm, frame->fun);
#endif

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

  if(!c->library)
    LLVMSetInstructionCallConv(result, GEN_CALLCONV);

  return result;
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
