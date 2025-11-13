#include "genexe.h"
#include "gencall.h"
#include "genfun.h"
#include "genobj.h"
#include "genopt.h"
#include "../reach/paint.h"
#include "../pkg/package.h"
#include "../pkg/program.h"
#include "../plugin/plugin.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>

#include <optional>
#include <filesystem>
namespace fs = std::filesystem;

#include <lld/Common/Driver.h>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <unistd.h>
#endif

#if !defined(PONY_COMPILER)
  #error PONY_COMPILER must be defined
  #define PONY_COMPILER ""
#endif

#if !defined(PONY_ARCH)
  #error PONY_ARCH must be defined
  #define PONY_ARCH ""
#endif

#define STR(x) STR2(x)
#define STR2(x) #x


class CaptureOStream : public llvm::raw_ostream {
public:
  std::string data;

  CaptureOStream() : raw_ostream(/*unbuffered=*/true), data() {}

  void write_impl(const char *ptr, size_t size) override {
    data.append(ptr, size);
  }

  uint64_t current_pos() const override { return data.size(); }
};


static LLVMValueRef create_main(compile_t* c, reach_type_t* t,
  LLVMValueRef ctx)
{
  // Create the main actor and become it.
  LLVMValueRef args[3];
  args[0] = ctx;
  args[1] = ((compile_type_t*)t->c_type)->desc;
  args[2] = LLVMConstInt(c->i1, 0, false);
  LLVMValueRef actor = gencall_runtime(c, "pony_create", args, 3, "");

  args[0] = ctx;
  args[1] = actor;
  gencall_runtime(c, "ponyint_become", args, 2, "");

  return actor;
}

static LLVMValueRef make_lang_features_init(compile_t* c)
{
  char* triple = c->opt->triple;

  LLVMTypeRef boolean;

  if(target_is_ppc(triple) && target_is_ilp32(triple) &&
    target_is_macosx(triple))
    boolean = c->i32;
  else
    boolean = c->i8;

  uint32_t desc_table_size = reach_max_type_id(c->reach);
  LLVMValueRef desc_table_lookup_fn = c->desc_table_offset_lookup_fn;

  LLVMTypeRef f_params[5];
  f_params[0] = boolean;
  f_params[1] = boolean;
  f_params[2] = c->ptr;
  f_params[3] = c->intptr;
  f_params[4] = c->descriptor_offset_lookup_fn;

  LLVMTypeRef lfi_type = LLVMStructTypeInContext(c->context, f_params, 5,
    false);

  LLVMBasicBlockRef this_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef entry_block = LLVMGetEntryBasicBlock(codegen_fun(c));
  LLVMValueRef inst = LLVMGetFirstInstruction(entry_block);

  if(inst != NULL)
    LLVMPositionBuilderBefore(c->builder, inst);
  else
    LLVMPositionBuilderAtEnd(c->builder, entry_block);

  LLVMValueRef lfi_object = LLVMBuildAlloca(c->builder, lfi_type, "");

  LLVMPositionBuilderAtEnd(c->builder, this_block);

  LLVMValueRef field = LLVMBuildStructGEP2(c->builder, lfi_type, lfi_object, 0,
    "");
  LLVMBuildStore(c->builder, LLVMConstInt(boolean, 1, false), field);

  field = LLVMBuildStructGEP2(c->builder, lfi_type, lfi_object, 1, "");
  LLVMBuildStore(c->builder, LLVMConstInt(boolean, 1, false), field);

  field = LLVMBuildStructGEP2(c->builder, lfi_type, lfi_object, 2, "");
  LLVMBuildStore(c->builder, c->desc_table, field);

  field = LLVMBuildStructGEP2(c->builder, lfi_type, lfi_object, 3, "");
  LLVMBuildStore(c->builder, LLVMConstInt(c->intptr, desc_table_size, false),
    field);

  field = LLVMBuildStructGEP2(c->builder, lfi_type, lfi_object, 4, "");
  LLVMBuildStore(c->builder, LLVMBuildBitCast(c->builder, desc_table_lookup_fn,
    c->descriptor_offset_lookup_fn, ""), field);

  return lfi_object;
}

LLVMValueRef gen_main(compile_t* c, reach_type_t* t_main, reach_type_t* t_env)
{
  LLVMTypeRef params[3];
  params[0] = c->i32;
  params[1] = c->ptr;
  params[2] = c->ptr;

  LLVMTypeRef ftype = LLVMFunctionType(c->i32, params, 3, false);
  LLVMValueRef func = LLVMAddFunction(c->module, "main", ftype);

  codegen_startfun(c, func, NULL, NULL, NULL, false);

  LLVMBasicBlockRef start_fail_block = codegen_block(c, "start_fail");
  LLVMBasicBlockRef post_block = codegen_block(c, "post");

  LLVMValueRef args[5];
  args[0] = LLVMGetParam(func, 0);
  LLVMSetValueName(args[0], "argc");

  args[1] = LLVMGetParam(func, 1);
  LLVMSetValueName(args[1], "argv");

  args[2] = LLVMGetParam(func, 2);
  LLVMSetValueName(args[2], "envp");

  // Initialise the pony runtime with argc and argv, getting a new argc.
  args[0] = gencall_runtime(c, "pony_init", args, 2, "argc");

  // Create the main actor and become it.
  LLVMValueRef ctx = gencall_runtime(c, "pony_ctx", NULL, 0, "");
  codegen_setctx(c, ctx);
  LLVMValueRef main_actor = create_main(c, t_main, ctx);

  // Create an Env on the main actor's heap.
  reach_method_t* m = reach_method(t_env, TK_NONE, c->str__create, NULL);

  LLVMValueRef env_args[4];
  env_args[0] = gencall_alloc(c, t_env, NULL);
  env_args[1] = args[0];
  env_args[2] = args[1];
  env_args[3] = args[2];
  codegen_call(c,
    LLVMGlobalGetValueType(((compile_method_t*)m->c_method)->func),
    ((compile_method_t*)m->c_method)->func, env_args, 4, true);
  LLVMValueRef env = env_args[0];

  // Run primitive initialisers using the main actor's heap.
  if(c->primitives_init != NULL)
    LLVMBuildCall2(c->builder, LLVMGlobalGetValueType(c->primitives_init),
      c->primitives_init, NULL, 0, "");

  // Create a type for the message.
  LLVMTypeRef f_params[4];
  f_params[0] = c->i32;
  f_params[1] = c->i32;
  f_params[2] = c->ptr;
  f_params[3] = LLVMTypeOf(env);
  LLVMTypeRef msg_type = LLVMStructTypeInContext(c->context, f_params, 4,
    false);

  // Allocate the message, setting its size and ID.
  uint32_t index = reach_vtable_index(t_main, c->str_create);
  size_t msg_size = (size_t)LLVMABISizeOfType(c->target_data, msg_type);
  args[0] = LLVMConstInt(c->i32, ponyint_pool_index(msg_size), false);
  args[1] = LLVMConstInt(c->i32, index, false);
  LLVMValueRef msg = gencall_runtime(c, "pony_alloc_msg", args, 2, "");
  LLVMValueRef msg_ptr = msg;

  // Set the message contents.
  LLVMValueRef env_ptr = LLVMBuildStructGEP2(c->builder, msg_type, msg_ptr, 3,
    "");
  LLVMBuildStore(c->builder, env, env_ptr);

  // Trace the message.
  args[0] = ctx;
  gencall_runtime(c, "pony_gc_send", args, 1, "");

  args[0] = ctx;
  args[1] = env;
  args[2] = ((compile_type_t*)t_env->c_type)->desc;
  args[3] = LLVMConstInt(c->i32, PONY_TRACE_IMMUTABLE, false);
  gencall_runtime(c, "pony_traceknown", args, 4, "");

  args[0] = ctx;
  gencall_runtime(c, "pony_send_done", args, 1, "");

  // Send the message.
  args[0] = ctx;
  args[1] = main_actor;
  args[2] = msg;
  args[3] = msg;
  args[4] = LLVMConstInt(c->i1, 1, false);
  gencall_runtime(c, "pony_sendv_single", args, 5, "");

  // unbecome the main actor before starting the runtime to set sched->ctx.current = NULL
  args[0] = ctx;
  args[1] = LLVMConstNull(c->ptr);
  gencall_runtime(c, "ponyint_become", args, 2, "");

  // Start the runtime.
  args[0] = LLVMConstInt(c->i1, 0, false);
  args[1] = LLVMConstNull(c->ptr);
  args[2] = make_lang_features_init(c);
  LLVMValueRef start_success = gencall_runtime(c, "pony_start", args, 3, "");

  LLVMBuildCondBr(c->builder, start_success, post_block, start_fail_block);

  LLVMPositionBuilderAtEnd(c->builder, start_fail_block);

  const char error_msg_str[] = "Error: couldn't start runtime!";

  args[0] = codegen_string(c, error_msg_str, sizeof(error_msg_str) - 1);
  gencall_runtime(c, "puts", args, 1, "");
  LLVMBuildBr(c->builder, post_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);

  // Run primitive finalisers. We create a new main actor as a context to run
  // the finalisers in, but we do not initialise or schedule it.
  if(c->primitives_final != NULL)
  {
    LLVMValueRef final_actor = create_main(c, t_main, ctx);
    LLVMBuildCall2(c->builder, LLVMGlobalGetValueType(c->primitives_final),
      c->primitives_final, NULL, 0, "");
    args[0] = final_actor;
    gencall_runtime(c, "ponyint_destroy", args, 1, "");
  }

  args[0] = ctx;
  args[1] = LLVMConstNull(c->ptr);
  gencall_runtime(c, "ponyint_become", args, 2, "");

  LLVMValueRef rc = gencall_runtime(c, "pony_get_exitcode", NULL, 0, "");
  LLVMValueRef minus_one = LLVMConstInt(c->i32, (unsigned long long)-1, true);
  rc = LLVMBuildSelect(c->builder, start_success, rc, minus_one, "");

  // Return the runtime exit code.
  genfun_build_ret(c, rc);

  codegen_finishfun(c);

  // External linkage for main().
  LLVMSetLinkage(func, LLVMExternalLinkage);

  return func;
}

#if defined(PLATFORM_IS_LINUX) || defined(PLATFORM_IS_BSD)
static const char* env_cc_or_pony_compiler(bool* out_fallback_linker)
{
  const char* cc = getenv("CC");
  if(cc == NULL)
  {
    *out_fallback_linker = true;
    return PONY_COMPILER;
  }
  return cc;
}
#endif

/*
 * search_path (called from search_paths)
 *  - A single tuple (path, depth) to search
 *  - The target filename to find
 *  - Whether to include the filename or not
 *
 * There are two types of searches that we do.  One to find the paths
 * so we can add them via -L (libpthread, libm, librt, libatomic etc),
 * the other for identifying the paths to specific object files such
 * as crtn.o et al).
 *
 * I think this should be refactored into two separate functions as
 * we should probably deduplicate the -L paths before push_backing them
 * onto args.
 */
std::optional<std::string> search_path(std::tuple<fs::path, int> search_path,
                                       std::string targetstring,
                                       bool include_filename)
{
  std::optional<std::string> result = std::nullopt;

  fs::directory_options options =
    (fs::directory_options::follow_directory_symlink |
     fs::directory_options::skip_permission_denied);

  /*
   * Even though we instruct recursive_directory_iterator to just skip directories
   * that we don't have permission to enter, if we don't have permission for the
   * initially specified file path, or the filepath doesn't exist - then it will
   * raise an exception.
   *
   * This is expected as we'll almost certainly search paths that don't exist on
   * other distributions.
   */
  std::error_code ec;
  for (auto iter = fs::recursive_directory_iterator(std::get<0>(search_path), options, ec);
       iter != fs::recursive_directory_iterator(); ++iter) {
    if (iter.depth() == std::get<1>(search_path)) {
      iter.disable_recursion_pending();
    };

    // Detection of the simplest form of loop.
    if (fs::is_symlink(iter->path())) {
      if (fs::read_symlink(iter->path()) == "..") {
        iter.disable_recursion_pending();
      };
    };

    if (iter->path().filename() == targetstring) {
      fprintf(stderr, "search_path:FOUND:%s\n", iter->path().c_str());
      fs::path res = iter->path();
      if (include_filename) {
        result = res;
      } else {
        result = res.remove_filename();
      };
      /*
       * We do not break here, as in cases where we have multiple entries,
       * for example:
       *   /usr/lib/gcc/x86_64-linux-gnu/13/
       *   /usr/lib/gcc/x86_64-linux-gnu/14/
       * … we want the "latest" version if we can (even though semver won't
       *     guarantee to do that)
       */
    };
  };
  return result;
}

/*
 * search_paths
 *  - A vector of tuples (path, depth) to search through
 *  - The target filename to find
 *  - Whether to include the filename or not
 *
 * There are two types of searches that we do.  One to find the paths
 * so we can add them via -L (libpthread, libm, librt, libatomic etc),
 * the other for identifying the paths to specific object files such
 * as crtn.o et al).
 *
 * I think this should be refactored into two separate functions as
 * we should probably deduplicate the -L paths before push_backing them
 * onto args.
 */
char* search_paths(std::vector<std::tuple<std::string, int>> spaths, std::string wanted, bool include_filename) {
  char* retme = NULL;
  for (const std::tuple<std::string, int>& spath : spaths) {
    std::optional<std::string> opt_result = search_path(spath, wanted, include_filename);
    if (opt_result) {
      /* This needs to be allocated using pony_alloc et al */
      retme = new char[(*opt_result).size() + 1];
      std::strcpy(retme, (*opt_result).c_str());
//      return result;
    };
  };
  if (retme == NULL) {
    fprintf(stderr, "XXXXX Unable to find %s\n", wanted.c_str());
  } else {
    fprintf(stderr, "Returning: %s\n", retme);
  };

  return retme;
}
LLD_HAS_DRIVER(elf)

// TODO: this is an awful hack. It is defined in program.cc
// but to expose in the header, it needs to include vector but that
// means everything that uses program.h needs to be switched to cpp from c.
// not something i want to take on at the moment. might be small. might not be.
// it's unclear.
// perhaps all that comes out of program anyway.
void program_lib_build_args_embedded(
  std::vector<const char *>* args,
  ast_t* program, pass_opt_t* opt,
  const char* path_preamble, const char* rpath_preamble,
  const char* global_preamble, const char* global_postamble,
  const char* lib_premable, const char* lib_postamble);

static bool new_link_exe(compile_t* c, ast_t* program, const char* file_o)
{
  errors_t* errors = c->opt->check.errors;


/*
 * We do the known cases first, and set appropriate depths.
 *
 * The reason we limit the depth to 1 in the second+third entry
 * is that in the case of a multilib installation, the search
 * will find /usr/lib/gcc/<triple>/<gccversion>/32/filename,
 * when we just want  gcc/<triple>/<gccversion>/filename.
 *
 * Including that 32 bit version is going to mean we have a
 * hard time.
 *
 * We should probably also prepend this vector with the PATHs
 * that are in the environmental variable LIBRARY_PATH.
 */
  std::string cxx_triple = c->opt->triple;
  std::vector<std::tuple<std::string, int>> spaths =
    {
      std::make_tuple("/usr/lib/x86_64-linux-gnu", 0),       // Ubuntu, Debian
      std::make_tuple("/usr/lib/x86_64-pc-linux-gnu", 0),       // Ubuntu, Debian
      std::make_tuple("/usr/lib/x86_64-unknown-linux-gnu", 0),       // Ubuntu, Debian
                                                                     //
      std::make_tuple("/usr/lib/gcc/x86_64-linux-gnu", 1),   // Ubuntu, Debian, Arch
      std::make_tuple("/usr/lib/gcc/x86_64-pc-linux-gnu", 1),   // Ubuntu, Debian, Arch
      std::make_tuple("/usr/lib/gcc/x86_64-unknown-linux-gnu", 1),   // Ubuntu, Debian, Arch
                                                                     //
      std::make_tuple("/usr/lib64/gcc/x86_64-linux-gnu", 1), // Ubuntu, Arch
      std::make_tuple("/usr/lib64/gcc/x86_64-pc-linux-gnu", 1), // Ubuntu, Arch
      std::make_tuple("/usr/lib64/gcc/x86_64-unknown-linux-gnu", 1), // Ubuntu, Arch
                                                                     //
      std::make_tuple("/lib/gcc/x86_64-linux-gnu", 1),       // Ubuntu, Arch
      std::make_tuple("/lib/gcc/x86_64-pc-linux-gnu", 1),       // Ubuntu, Arch
      std::make_tuple("/lib/gcc/x86_64-unknown-linux-gnu", 1),       // Ubuntu, Arch
                                                                     //
      std::make_tuple("/lib/", 32),                       // Inventory time
      std::make_tuple("/usr/lib/", 32),                   // Inventory time
      std::make_tuple("/lib64/", 32),                     // Inventory time
      std::make_tuple("/usr/lib64/", 32),                 // Inventory time
    };


  // Collect the arguments and linker flavor we will pass to the linker.
  std::vector<const char *> args;
  const char* link_flavor = "unknown";

  const char* file_exe =
    suffix_filename(c, c->opt->output, "", c->filename, "");

  if(c->opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Linking %s\n", file_exe);

  fprintf(stderr, "Triple: %s\n", c->opt->triple);

  if (target_is_linux(c->opt->triple)) {
    args.push_back("ld.lld");

    // TODO: me specific
    // not all might be needed. last definitely is for now.
    // args.push_back("-L");
    // args.push_back("/home/sean/code/ponylang/ponyc/build/debug/");

    char* result_patha = search_paths(spaths, "libpthread.a", false);
    if (result_patha != NULL) {
      args.push_back("-L");
      args.push_back(result_patha);
    };

    char* result_pathb = search_paths(spaths, "libm.a", false);
    if (result_pathb != NULL) {
      args.push_back("-L");
      args.push_back(result_pathb);
    };

    char* result_pathc = search_paths(spaths, "libatomic.a", false);
    if (result_pathc != NULL) {
      args.push_back("-L");
      args.push_back(result_pathc);
    };

    char* result_pathd = search_paths(spaths, "libc.a", false);
    if (result_pathd != NULL) {
      args.push_back("-L");
      args.push_back(result_pathd);
    };

    char* result_pathe = search_paths(spaths, "libgcc.a", false);
    if (result_pathe != NULL) {
      args.push_back("-L");
      args.push_back(result_pathe);
    };

    char* result_pathf = search_paths(spaths, "libgcc_s.so", false);
    if (result_pathf != NULL) {
      args.push_back("-L");
      args.push_back(result_pathf);
    };
//    args.push_back("-L");
//    args.push_back("/usr/lib/gcc/x86_64-linux-gnu/13");
//    args.push_back("-L");
//    args.push_back("/lib");
//    args.push_back("-L");
//    args.push_back("/usr/lib");
//    args.push_back("-L");
//    args.push_back("/usr/lib64");
    // args.push_back("-L");
    // args.push_back("/usr/local/lib");
//    args.push_back("-L");
//    args.push_back("/usr/lib/gcc/x86_64-pc-linux-gnu/15.2.1/");

    if (target_is_musl(c->opt->triple)) {
      args.push_back("-z");
      args.push_back("now");
    }
    // TODO: joe had - maybe turn back on
    // if (target_is_linux(c->opt->triple)) {
    //   args.push_back("-z");
    //   args.push_back("relro");
    // }
    args.push_back("--hash-style=both");
    args.push_back("--eh-frame-hdr");

    if (target_is_x86(c->opt->triple)) {
      args.push_back("-m");
      args.push_back("elf_x86_64");
      args.push_back("-dynamic-linker");
      args.push_back("/usr/lib64/ld-linux-x86-64.so.2");
    } else if (target_is_arm(c->opt->triple)) {
      args.push_back("-m");
      args.push_back("aarch64linux");
      args.push_back("-dynamic-linker");
      args.push_back("/usr/lib/ld-linux-aarch64.so.1");
    } else {
      errorf(errors, NULL, "Linking with lld isn't yet supported for %s",
        c->opt->triple);
      return false;
    }

    // TODO: this is all very specific
    args.push_back("-pie");
//    args.push_back("-dynamic-linker");
//    args.push_back("/usr/lib64/ld-linux-x86-64.so.2");

    // works:
    // ld.lld -L /home/sean/code/ponylang/ponyc/build/debug/ -L /lib -L /usr/lib/ -L /usr/lib64 -L /usr/local/lib -L /usr/lib/gcc/x86_64-pc-linux-gnu/15.2.1/ --hash-style=both --eh-frame-hdr -m elf_x86_64 -pie -dynamic-linker /lib64/ld-linux-x86-64.so.2 -o fib /usr/lib64/Scrt1.o /usr/lib64/crti.o /usr/lib64/gcc/x86_64-pc-linux-gnu/15.2.1/crtbeginS.o fib.o -lpthread -lgcc_s -lrt -lponyrt-pic -lm -ldl -latomic -lgcc -lgcc_s -lc  /usr/lib64/gcc/x86_64-pc-linux-gnu/15.2.1/crtendS.o /usr/lib64/crtn.o

//    char* dynamic_linker = 
//    args.push_back("/lib64/ld-linux-x86-64.so.2");

    args.push_back("-o");
    args.push_back(file_exe);

    char* scrt1 = search_paths(spaths, "Scrt1.o", true);
    if (scrt1 != NULL) {
      args.push_back(scrt1);
    };

    char* crti = search_paths(spaths, "crti.o", true);
    if (crti != NULL) {
      args.push_back(crti);
    };

    char* crtbegins = search_paths(spaths, "crtbeginS.o", true);
    if (crtbegins != NULL) {
      args.push_back(crtbegins);
    };
//    args.push_back("/usr/lib/x86_64-linux-gnu/crti.o");
//    args.push_back("/usr/lib/gcc/x86_64-linux-gnu/13/crtbeginS.o");

    args.push_back(file_o);

    program_lib_build_args_embedded(&args, program, c->opt, "-L", "-rpath",
      "--start-group", "--end-group", "-l", "");

    // TODO: should handle cross compilation
    // const char* arch = c->opt->link_arch != NULL ? c->opt->link_arch : PONY_ARCH;

    // TODO: joe had. maybe turn back on
    // args.push_back(
    //   target_is_x86(c->opt->triple)
    //     ? "-plugin-opt=mcpu=x86-64"
    //     : "-plugin-opt=mcpu=aarch64"
    //   );
    // args.push_back(
    //   c->opt->release
    //     ? "-plugin-opt=O3"
    //     : "-plugin-opt=O0"
    // );
    // args.push_back("-plugin-opt=thinlto");

    args.push_back("-lpthread");
    args.push_back("-lgcc_s");
    c->opt->pic ? args.push_back("-lponyrt-pic") : args.push_back("-lponyrt");
    args.push_back("-lm");
    args.push_back("-ldl");
    args.push_back("-latomic");
    args.push_back("-lgcc");
    args.push_back("-lgcc_s");
    args.push_back("-lc");

    // TODO: legacy does
    // program_lib_build_args(program, c->opt, "-L", "-Wl,-rpath,",
    //"-Wl,--start-group ", "-Wl,--end-group ", "-l", "");
    // const char* lib_args = program_lib_args(program);
    //
    // For lld that is:
    // program_lib_build_args(program, c->opt, "-L", "-rpath,",
    //"--start-group ", "--end-group ", "-l", "");
    // const char* lib_args = program_lib_args(program);
    //

    // clang spits out:
    // "/usr/bin/ld.lld" "--hash-style=gnu" "--build-id" "--eh-frame-hdr" "-m" "elf_x86_64" "-pie" "-dynamic-linker" "/lib64/ld-linux-x86-64.so.2" "-o" "./fib" "/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/15.2.1/../../../../lib64/Scrt1.o" "/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/15.2.1/../../../../lib64/crti.o" "/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/15.2.1/crtbeginS.o" "-L/usr/local/lib/pony/0.58.11-5f1ba5df/bin/" "-L/usr/local/lib/pony/0.58.11-5f1ba5df/bin/../lib/native" "-L/usr/local/lib/pony/0.58.11-5f1ba5df/bin/../packages" "-L/usr/local/lib/pony/0.58.11-5f1ba5df/packages/" "-L/usr/local/lib" "-L/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/15.2.1" "-L/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/15.2.1/../../../../lib64" "-L/lib/../lib64" "-L/usr/lib64" "-L/lib" "-L/usr/lib" "./fib.o" "-lpthread" "-rpath" "/usr/local/lib/pony/0.58.11-5f1ba5df/bin/" "-rpath" "/usr/local/lib/pony/0.58.11-5f1ba5df/bin/../lib/native" "-rpath" "/usr/local/lib/pony/0.58.11-5f1ba5df/bin/../packages" "-rpath" "/usr/local/lib/pony/0.58.11-5f1ba5df/packages/" "-rpath" "/usr/local/lib" "--start-group" "-lrt" "--end-group" "-lponyrt-pic" "-lm" "-ldl" "-latomic" "-lgcc" "--as-needed" "-lgcc_s" "--no-as-needed" "-lc" "-lgcc" "--as-needed" "-lgcc_s" "--no-as-needed" "/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/15.2.1/crtendS.o" "/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/15.2.1/../../../../lib64/crtn.o"

    /* TODO all this probably needs to go somewhere else */
    if (c->opt->staticbin)
      args.push_back("-static");
    // TODO: does musl need this?
    if (target_is_musl(c->opt->triple))
      args.push_back("-lexecinfo");

    // TODO: BSD should be handling dtrace_args here

    // TODO: should handle
    //#if defined(PONY_SANITIZER)
    //  "-fsanitize=" PONY_SANITIZER;
    // #else
    //  "";
    // #endif

    // TODO: arm linker args

    // TODO: LTO

    // TODO this should go at the end like it is
    char* crtends = search_paths(spaths, "crtendS.o", true);
    if (crtends != NULL) {
      args.push_back(crtends);
    };
//    args.push_back("/usr/lib/gcc/x86_64-linux-gnu/13/crtendS.o");
    char* crtn = search_paths(spaths, "crtn.o", true);
    if (crtn != NULL) {
      args.push_back(crtn);
    };
//    args.push_back("/usr/lib/x86_64-linux-gnu/crtn.o");

  // TODO: MacOS, Windows, BSD, etc
  } else {
    errorf(errors, NULL, "Linking with lld isn't yet supported for %s",
      c->opt->triple);
    return false;
  }

  // Create output streams that capture stdout and stderr info to strings.
  CaptureOStream output;

  // Invoke the linker.
  lld::Result result = lld::lldMain(args, output, output, {{lld::Gnu, &lld::elf::link}});
  bool link_result = (result.retCode == 0);

  // Show an informative error if linking failed, showing both the args passed
  // as well as the output that we captured from the linker attempt.
  if (!link_result) {
    output << "\nLinking was attempted with these linker args:\n";
    for (auto it = args.begin(); it != args.end(); ++it) {
      output << *it << "\n";
    }

    // printf("\n\n\n%s\n", output.data.data());
    errorf(errors, NULL, "Failed to link with embedded lld: \n\n%s",
      output.data.data());
  }

  return link_result;
}

static bool legacy_link_exe(compile_t* c, ast_t* program,
  const char* file_o)
{
  errors_t* errors = c->opt->check.errors;

  const char* ponyrt = c->opt->runtimebc ? "" :
#if defined(PLATFORM_IS_WINDOWS)
    "libponyrt.lib";
#elif defined(PLATFORM_IS_LINUX)
    c->opt->pic ? "-lponyrt-pic" : "-lponyrt";
#else
    "-lponyrt";
#endif

#if defined(PLATFORM_IS_MACOSX)
  char* arch = strchr(c->opt->triple, '-');

  if(arch == NULL)
  {
    errorf(errors, NULL, "couldn't determine architecture from %s",
      c->opt->triple);
    return false;
  }

  const char* file_exe =
    suffix_filename(c, c->opt->output, "", c->filename, "");

  if(c->opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Linking %s\n", file_exe);

  program_lib_build_args(program, c->opt, "-L", NULL, "", "", "-l", "");
  const char* lib_args = program_lib_args(program);

  size_t arch_len = arch - c->opt->triple;
  const char* linker = c->opt->linker != NULL ? c->opt->linker : "ld";
  const char* sanitizer_arg =
#if defined(PONY_SANITIZER)
    "-fsanitize=" PONY_SANITIZER;
#else
    "";
#endif

  size_t ld_len = 256 + arch_len + strlen(linker) + strlen(file_exe) +
    strlen(file_o) + strlen(lib_args) + strlen(sanitizer_arg);

  char* ld_cmd = (char*)ponyint_pool_alloc_size(ld_len);

  snprintf(ld_cmd, ld_len,
    "%s -execute -arch %.*s "
    "-o %s %s %s %s "
    "-L/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib -lSystem %s -platform_version macos '" STR(PONY_OSX_PLATFORM) "' '0.0.0'",
           linker, (int)arch_len, c->opt->triple, file_exe, file_o,
           lib_args, ponyrt, sanitizer_arg
    );

  if(c->opt->verbosity >= VERBOSITY_TOOL_INFO)
    fprintf(stderr, "%s\n", ld_cmd);

  if(system(ld_cmd) != 0)
  {
    errorf(errors, NULL, "unable to link: %s", ld_cmd);
    ponyint_pool_free_size(ld_len, ld_cmd);
    return false;
  }

  ponyint_pool_free_size(ld_len, ld_cmd);

  if(!c->opt->strip_debug)
  {
    size_t dsym_len = 16 + strlen(file_exe);
    char* dsym_cmd = (char*)ponyint_pool_alloc_size(dsym_len);

    snprintf(dsym_cmd, dsym_len, "rm -rf %s.dSYM", file_exe);
    system(dsym_cmd);

    snprintf(dsym_cmd, dsym_len, "dsymutil %s", file_exe);

    if(system(dsym_cmd) != 0)
      errorf(errors, NULL, "unable to create dsym");

    ponyint_pool_free_size(dsym_len, dsym_cmd);
  }

#elif defined(PLATFORM_IS_LINUX) || defined(PLATFORM_IS_BSD)
  const char* file_exe =
    suffix_filename(c, c->opt->output, "", c->filename, "");

  if(c->opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Linking %s\n", file_exe);

  program_lib_build_args(program, c->opt, "-L", "-Wl,-rpath,",
    "-Wl,--start-group ", "-Wl,--end-group ", "-l", "");
  const char* lib_args = program_lib_args(program);

  const char* arch = c->opt->link_arch != NULL ? c->opt->link_arch : PONY_ARCH;
  bool fallback_linker = false;
  const char* linker = c->opt->linker != NULL ? c->opt->linker :
    env_cc_or_pony_compiler(&fallback_linker);
  const char* mcx16_arg = (target_is_lp64(c->opt->triple)
    && target_is_x86(c->opt->triple)) ? "-mcx16" : "";
  const char* fuseldcmd = c->opt->link_ldcmd != NULL ? c->opt->link_ldcmd :
    "";
  const char* fuseld = strlen(fuseldcmd) ? "-fuse-ld=" : "";
  const char* ldl = target_is_linux(c->opt->triple) ? "-ldl" : "";
  const char* atomic =
    (target_is_linux(c->opt->triple) || target_is_dragonfly(c->opt->triple))
    ? "-latomic" : "";
  const char* staticbin = c->opt->staticbin ? "-static" : "";
  const char* dtrace_args =
#if defined(PLATFORM_IS_BSD) && defined(USE_DYNAMIC_TRACE)
   "-Wl,--whole-archive -ldtrace_probes -Wl,--no-whole-archive -lelf";
#else
    "";
#endif
  const char* lexecinfo =
#if (defined(PLATFORM_IS_BSD))
   "-lexecinfo";
#else
    "";
#endif

  const char* sanitizer_arg =
#if defined(PONY_SANITIZER)
    "-fsanitize=" PONY_SANITIZER;
#else
    "";
#endif

  const char* arm_linker_args = target_is_arm(c->opt->triple)
    ? " -Wl,--exclude-libs,libgcc.a -Wl,--exclude-libs,libgcc_real.a -Wl,--exclude-libs,libgnustl_shared.so -Wl,--exclude-libs,libunwind.a"
    : "";

  size_t ld_len = 512 + strlen(file_exe) + strlen(file_o) + strlen(lib_args)
                  + strlen(arch) + strlen(mcx16_arg) + strlen(fuseld)
                  + strlen(ldl) + strlen(atomic) + strlen(staticbin)
                  + strlen(dtrace_args) + strlen(lexecinfo) + strlen(fuseldcmd)
                  + strlen(sanitizer_arg) + strlen(arm_linker_args);

  char* ld_cmd = (char*)ponyint_pool_alloc_size(ld_len);

#ifdef PONY_USE_LTO
  if (strcmp(arch, "x86_64") == 0)
    arch = "x86-64";
#endif
  snprintf(ld_cmd, ld_len, "%s -o %s -O3 -march=%s "
    "%s "
#ifdef PONY_USE_LTO
    "-flto -fuse-linker-plugin "
#endif
// The use of NDEBUG instead of PONY_NDEBUG here is intentional.
#ifndef NDEBUG
    // Allows the implementation of `pony_assert` to correctly get symbol names
    // for backtrace reporting.
    "-rdynamic "
#endif
#ifdef PLATFORM_IS_OPENBSD
    // On OpenBSD, the unwind symbols are contained within libc++abi.
    "%s %s%s %s %s -lpthread %s %s %s -lm -lc++abi %s %s %s "
#else
    "%s %s%s %s %s -lpthread %s %s %s -lm %s %s %s "
#endif
    "%s",
    linker, file_exe, arch, mcx16_arg, staticbin, fuseld, fuseldcmd, file_o,
    arm_linker_args,
    lib_args, dtrace_args, ponyrt, ldl, lexecinfo, atomic, sanitizer_arg
    );

  if(c->opt->verbosity >= VERBOSITY_TOOL_INFO)
    fprintf(stderr, "%s\n", ld_cmd);

  if(system(ld_cmd) != 0)
  {
    if((c->opt->verbosity >= VERBOSITY_MINIMAL) && fallback_linker)
    {
      fprintf(stderr,
        "Warning: environment variable $CC undefined, using %s as the linker\n",
        PONY_COMPILER);
    }

    errorf(errors, NULL, "unable to link: %s", ld_cmd);
    ponyint_pool_free_size(ld_len, ld_cmd);
    return false;
  }

  ponyint_pool_free_size(ld_len, ld_cmd);
#elif defined(PLATFORM_IS_WINDOWS)
  vcvars_t vcvars;

  if(!vcvars_get(c, &vcvars, errors))
  {
    errorf(errors, NULL, "unable to link: no vcvars");
    return false;
  }

  const char* file_exe = suffix_filename(c, c->opt->output, "", c->filename,
    ".exe");
  if(c->opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Linking %s\n", file_exe);

  program_lib_build_args(program, c->opt,
    "/LIBPATH:", NULL, "", "", "", ".lib");
  const char* lib_args = program_lib_args(program);

  char ucrt_lib[MAX_PATH + 12];
  if (strlen(vcvars.ucrt) > 0)
    snprintf(ucrt_lib, MAX_PATH + 12, "/LIBPATH:\"%s\"", vcvars.ucrt);
  else
    ucrt_lib[0] = '\0';


#ifdef _M_ARM64
  const char* arch = "ARM64";
#elif defined(_M_X64)
  const char* arch="x64";
#else
  const char* arch = "";
#endif

  size_t ld_len = 253 + strlen(arch) + strlen(file_exe) + strlen(file_o) +
    strlen(vcvars.kernel32) + strlen(vcvars.msvcrt) + strlen(lib_args);
  char* ld_cmd = (char*)ponyint_pool_alloc_size(ld_len);

  char* linker = vcvars.link;
  if (c->opt->linker != NULL && strlen(c->opt->linker) > 0)
    linker = c->opt->linker;

  while (true)
  {
    size_t num_written = snprintf(ld_cmd, ld_len,
      "cmd /C \"\"%s\" /DEBUG /NOLOGO /MACHINE:%s /ignore:4099 "
      "/OUT:%s "
      "%s %s "
      "/LIBPATH:\"%s\" "
      "/LIBPATH:\"%s\" "
      "%s %s %s \"",
      linker, arch, file_exe, file_o, ucrt_lib, vcvars.kernel32,
      vcvars.msvcrt, lib_args, vcvars.default_libs, ponyrt
    );

    if (num_written < ld_len)
      break;

    ponyint_pool_free_size(ld_len, ld_cmd);
    ld_len += 256;
    ld_cmd = (char*)ponyint_pool_alloc_size(ld_len);
  }

  if(c->opt->verbosity >= VERBOSITY_TOOL_INFO)
    fprintf(stderr, "%s\n", ld_cmd);

  int result = system(ld_cmd);
  if (result != 0)
  {
    errorf(errors, NULL, "unable to link: %s: %d", ld_cmd, result);
    ponyint_pool_free_size(ld_len, ld_cmd);
    return false;
  }

  ponyint_pool_free_size(ld_len, ld_cmd);
#endif

  return true;
}

static bool link_exe(compile_t* c, ast_t* program, const char* file_o)
{
    if (target_is_linux(c->opt->triple))
      return new_link_exe(c, program, file_o);
    else
      return legacy_link_exe(c, program, file_o);
}

bool genexe(compile_t* c, ast_t* program)
{
  errors_t* errors = c->opt->check.errors;

  // The first package is the main package. It has to have a Main actor.
  const char* main_actor = c->str_Main;
  const char* env_class = c->str_Env;
  const char* package_name = c->filename;

  if((c->opt->bin_name != NULL) && (strlen(c->opt->bin_name) > 0))
    c->filename = c->opt->bin_name;

  ast_t* package = ast_child(program);
  ast_t* main_def = ast_get(package, main_actor, NULL);

  if(main_def == NULL)
  {
    errorf(errors, NULL, "no Main actor found in package '%s'", package_name);
    return false;
  }

  // Generate the Main actor and the Env class.
  ast_t* main_ast = type_builtin(c->opt, main_def, main_actor);
  ast_t* env_ast = type_builtin(c->opt, main_def, env_class);

  deferred_reification_t* main_create = lookup(c->opt, main_ast, main_ast,
    c->str_create);

  if(main_create == NULL)
  {
    ast_free(main_ast);
    ast_free(env_ast);
    return false;
  }

  deferred_reify_free(main_create);

  if(c->opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, " Reachability\n");
  reach(c->reach, main_ast, c->str_create, NULL, c->opt);
  reach(c->reach, main_ast, stringtab("runtime_override_defaults"), NULL, c->opt);
  reach(c->reach, env_ast, c->str__create, NULL, c->opt);

  if(c->opt->limit == PASS_REACH)
  {
    ast_free(main_ast);
    ast_free(env_ast);
    return true;
  }

  if(c->opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, " Selector painting\n");
  paint(&c->reach->types);

  plugin_visit_reach(c->reach, c->opt, true);

  if(c->opt->limit == PASS_PAINT)
  {
    ast_free(main_ast);
    ast_free(env_ast);
    return true;
  }

  if(!gentypes(c))
  {
    ast_free(main_ast);
    ast_free(env_ast);
    return false;
  }

  if(c->opt->verbosity >= VERBOSITY_ALL)
    reach_dump(c->reach);

  reach_type_t* t_main = reach_type(c->reach, main_ast);
  reach_type_t* t_env = reach_type(c->reach, env_ast);

  ast_free(main_ast);
  ast_free(env_ast);

  if((t_main == NULL) || (t_env == NULL))
    return false;

  gen_main(c, t_main, t_env);

  plugin_visit_compile(c, c->opt);

  if(!genopt(c, true))
    return false;

  if(c->opt->runtimebc)
  {
    if(!codegen_merge_runtime_bitcode(c))
      return false;

    // Rerun the optimiser without the Pony-specific optimisation passes.
    // Inlining runtime functions can screw up these passes so we can't
    // run the optimiser only once after merging.
    if(!genopt(c, false))
      return false;
  }

  const char* file_o = genobj(c);

  if(file_o == NULL)
    return false;

  if(c->opt->limit < PASS_ALL)
    return true;

  if(!link_exe(c, program, file_o))
    return false;

#ifdef PLATFORM_IS_WINDOWS
  _unlink(file_o);
#else
  unlink(file_o);
#endif

  return true;
}
