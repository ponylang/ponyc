#include "genexe.h"

#include <lld/Common/Driver.h>

LLD_HAS_DRIVER(elf)
LLD_HAS_DRIVER(macho)
LLD_HAS_DRIVER(coff)
LLD_HAS_DRIVER(mingw)
LLD_HAS_DRIVER(wasm)

#include "gencall.h"
#include "genfun.h"
#include "genname.h"
#include "genobj.h"
#include "genopt.h"
#include "genprim.h"
#include "../reach/paint.h"
#include "../pkg/package.h"
#include "../pkg/program.h"
#include "../plugin/plugin.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <string.h>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <unistd.h>
#  include <sys/stat.h>
#  include <dirent.h>
#  include <llvm/TargetParser/Triple.h>
#  include <llvm/Support/raw_ostream.h>
#  include <vector>
#  include <string>
#endif

#define STR(x) STR2(x)
#define STR2(x) #x

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

#ifdef PLATFORM_IS_POSIX_BASED
static bool is_cross_compiling(compile_t* c)
{
  char* default_triple_str = LLVMGetDefaultTargetTriple();
  llvm::Triple target(c->opt->triple);
  llvm::Triple host(default_triple_str);
  LLVMDisposeMessage(default_triple_str);

  return target.getArch() != host.getArch()
    || target.getOS() != host.getOS()
    || target.getEnvironment() != host.getEnvironment();
}

static const char* elf_emulation(compile_t* c)
{
  llvm::Triple triple(c->opt->triple);

  switch(triple.getArch())
  {
    case llvm::Triple::x86_64: return "elf_x86_64";
    case llvm::Triple::aarch64: return "aarch64linux";
    case llvm::Triple::riscv64: return "elf64lriscv";
    case llvm::Triple::riscv32: return "elf32lriscv";
    case llvm::Triple::arm:
    case llvm::Triple::thumb: return "armelf_linux_eabi";
    default: return NULL;
  }
}

static bool file_exists(const char* path)
{
  struct stat st;
  return stat(path, &st) == 0;
}

// Check whether the host is actually musl. LLVM's default target triple
// may report "gnu" on musl systems (e.g., Alpine's LLVM reports
// x86_64-unknown-linux-gnu). For native compilation, probe the filesystem
// rather than trusting the triple.
static bool host_is_musl(compile_t* c, llvm::Triple::ArchType arch)
{
  if(is_cross_compiling(c))
    return false;

  const char* musl_linker = NULL;
  switch(arch)
  {
    case llvm::Triple::x86_64:  musl_linker = "/lib/ld-musl-x86_64.so.1"; break;
    case llvm::Triple::aarch64: musl_linker = "/lib/ld-musl-aarch64.so.1"; break;
    case llvm::Triple::riscv64: musl_linker = "/lib/ld-musl-riscv64.so.1"; break;
    case llvm::Triple::arm:
    case llvm::Triple::thumb:
      return file_exists("/lib/ld-musl-armhf.so.1")
        || file_exists("/lib/ld-musl-arm.so.1");
    default: return false;
  }

  return file_exists(musl_linker);
}

static const char* dynamic_linker_path(compile_t* c)
{
  llvm::Triple triple(c->opt->triple);
  bool is_musl = triple.isMusl() || host_is_musl(c, triple.getArch());

  switch(triple.getArch())
  {
    case llvm::Triple::x86_64:
      return is_musl ? "/lib/ld-musl-x86_64.so.1"
                     : "/lib64/ld-linux-x86-64.so.2";
    case llvm::Triple::aarch64:
      return is_musl ? "/lib/ld-musl-aarch64.so.1"
                     : "/lib/ld-linux-aarch64.so.1";
    case llvm::Triple::riscv64:
      return is_musl ? "/lib/ld-musl-riscv64.so.1"
                     : "/lib/ld-linux-riscv64-lp64d.so.1";
    case llvm::Triple::arm:
    case llvm::Triple::thumb:
    {
      llvm::Triple::EnvironmentType env = triple.getEnvironment();
      bool is_hf = (env == llvm::Triple::MuslEABIHF)
        || (env == llvm::Triple::GNUEABIHF);
      if(is_musl)
      {
        return is_hf ? "/lib/ld-musl-armhf.so.1"
                     : "/lib/ld-musl-arm.so.1";
      }
      return is_hf ? "/lib/ld-linux-armhf.so.3"
                   : "/lib/ld-linux.so.3";
    }
    default: return NULL;
  }
}

static const char* system_triple(compile_t* c)
{
  llvm::Triple triple(c->opt->triple);
  std::string result = std::string(triple.getArchName()) + "-"
    + std::string(triple.getOSName()) + "-"
    + std::string(triple.getEnvironmentName());
  return stringtab(result.c_str());
}

static const char* find_libc_crt_dir(const char* sysroot,
  const char* sys_triple)
{
  // Search candidate paths for libc CRT objects (crt1.o).
  const char* candidates[4];
  char buf[PATH_MAX];

  snprintf(buf, sizeof(buf), "%s/usr/lib/%s", sysroot, sys_triple);
  candidates[0] = stringtab(buf);

  snprintf(buf, sizeof(buf), "%s/usr/lib", sysroot);
  candidates[1] = stringtab(buf);

  snprintf(buf, sizeof(buf), "%s/lib/%s", sysroot, sys_triple);
  candidates[2] = stringtab(buf);

  snprintf(buf, sizeof(buf), "%s/lib", sysroot);
  candidates[3] = stringtab(buf);

  for(int i = 0; i < 4; i++)
  {
    snprintf(buf, sizeof(buf), "%s/crt1.o", candidates[i]);
    if(file_exists(buf))
      return candidates[i];
  }

  return NULL;
}

static uint16_t expected_elf_machine(compile_t* c)
{
  llvm::Triple triple(c->opt->triple);

  switch(triple.getArch())
  {
    case llvm::Triple::x86_64:  return 62;   // EM_X86_64
    case llvm::Triple::aarch64: return 183;  // EM_AARCH64
    case llvm::Triple::riscv64: return 243;  // EM_RISCV
    case llvm::Triple::riscv32: return 243;  // EM_RISCV
    case llvm::Triple::arm:
    case llvm::Triple::thumb:   return 40;   // EM_ARM
    default:                    return 0;
  }
}

static bool elf_matches_target(const char* path, uint16_t target_machine)
{
  // Read the ELF header and check e_machine matches the target.
  // If we can't determine the architecture, accept the file.
  if(target_machine == 0)
    return true;

  FILE* f = fopen(path, "rb");
  if(f == NULL)
    return false;

  unsigned char hdr[20];
  if(fread(hdr, 1, 20, f) < 20)
  {
    fclose(f);
    return false;
  }
  fclose(f);

  // Verify ELF magic.
  if(hdr[0] != 0x7f || hdr[1] != 'E' || hdr[2] != 'L' || hdr[3] != 'F')
    return false;

  // e_machine is at offset 18, 2 bytes, in the ELF header's native
  // byte order. hdr[5] == 1 means little-endian, 2 means big-endian.
  uint16_t machine;
  if(hdr[5] == 1)
    machine = (uint16_t)hdr[18] | ((uint16_t)hdr[19] << 8);
  else
    machine = ((uint16_t)hdr[18] << 8) | (uint16_t)hdr[19];

  return machine == target_machine;
}

static const char* find_ponyc_crt_dir(ast_t* program, compile_t* c)
{
  uint16_t target_machine = expected_elf_machine(c);
  size_t count = program_lib_path_count(program);
  char buf[PATH_MAX];

  for(size_t i = 0; i < count; i++)
  {
    const char* path = program_lib_path_at(program, i);
    snprintf(buf, sizeof(buf), "%s/crtbeginS.o", path);
    if(file_exists(buf) && elf_matches_target(buf, target_machine))
      return path;
  }

  return NULL;
}

static const char* find_gcc_lib_dir(const char* sysroot,
  const char* arch_prefix)
{
  // Search for GCC runtime library directory containing libgcc.a.
  // Check multiple base paths and pick the highest version.
  //
  // GCC installs its runtime libraries under <base>/<triple>/<version>/.
  // The triple directory name varies across distros — it may be the
  // stripped triple (x86_64-linux-gnu on Ubuntu), the vendor triple
  // (x86_64-alpine-linux-musl on Alpine), or something else entirely
  // (x86_64-pc-linux-gnu on Arch). Rather than guessing which vendor
  // string GCC uses, enumerate all subdirectories under each base path
  // and check each for versioned libgcc.a. Filter by arch_prefix to
  // avoid picking up cross-compiler directories.
  //
  // Debian/Ubuntu cross-compiler packages install to /usr/lib/gcc-cross/
  // rather than /usr/lib/gcc/. Custom GCC cross-compiler builds (e.g. the
  // arm/armhf CI containers) install to /usr/local/lib/gcc/.
  const char* base_patterns[8];
  int pattern_count = 0;

  char sysroot_rel[PATH_MAX];
  char sysroot_rel_cross[PATH_MAX];
  char sysroot_parent_rel[PATH_MAX];
  char sysroot_parent_rel_cross[PATH_MAX];

  base_patterns[pattern_count++] = "/usr/lib/gcc";
  base_patterns[pattern_count++] = "/usr/lib/gcc-cross";
  base_patterns[pattern_count++] = "/usr/local/lib/gcc";
  base_patterns[pattern_count++] = "/usr/local/lib/gcc-cross";

  // <sysroot>/../lib/gcc (handles sysroot=/usr/<triple>)
  snprintf(sysroot_rel, sizeof(sysroot_rel), "%s/../lib/gcc", sysroot);
  base_patterns[pattern_count++] = sysroot_rel;

  snprintf(sysroot_rel_cross, sizeof(sysroot_rel_cross),
    "%s/../lib/gcc-cross", sysroot);
  base_patterns[pattern_count++] = sysroot_rel_cross;

  // <sysroot>/../../lib/gcc (handles sysroot=/usr/local/<triple>/libc)
  snprintf(sysroot_parent_rel, sizeof(sysroot_parent_rel),
    "%s/../../lib/gcc", sysroot);
  base_patterns[pattern_count++] = sysroot_parent_rel;

  snprintf(sysroot_parent_rel_cross, sizeof(sysroot_parent_rel_cross),
    "%s/../../lib/gcc-cross", sysroot);
  base_patterns[pattern_count++] = sysroot_parent_rel_cross;

  const char* best_dir = NULL;
  int best_version = -1;

  for(int p = 0; p < pattern_count; p++)
  {
    // Enumerate triple directories under this base path.
    DIR* base_dir = opendir(base_patterns[p]);
    if(base_dir == NULL)
      continue;

    size_t prefix_len = strlen(arch_prefix);

    struct dirent* triple_entry;
    while((triple_entry = readdir(base_dir)) != NULL)
    {
      if(triple_entry->d_name[0] == '.')
        continue;

      // Skip cross-compiler directories for a different architecture.
      if(strncmp(triple_entry->d_name, arch_prefix, prefix_len) != 0)
        continue;

      char triple_dir[PATH_MAX];
      snprintf(triple_dir, sizeof(triple_dir), "%s/%s",
        base_patterns[p], triple_entry->d_name);

      // Search version directories within this triple directory.
      DIR* ver_dir = opendir(triple_dir);
      if(ver_dir == NULL)
        continue;

      struct dirent* ver_entry;
      while((ver_entry = readdir(ver_dir)) != NULL)
      {
        if(ver_entry->d_name[0] == '.')
          continue;

        // Parse leading numeric component for version comparison.
        int version = atoi(ver_entry->d_name);
        if(version <= 0)
          continue;

        char candidate[PATH_MAX];
        snprintf(candidate, sizeof(candidate), "%s/%s/libgcc.a",
          triple_dir, ver_entry->d_name);

        if(file_exists(candidate) && version > best_version)
        {
          best_version = version;
          char result[PATH_MAX];
          snprintf(result, sizeof(result), "%s/%s",
            triple_dir, ver_entry->d_name);
          best_dir = stringtab(result);
        }
      }

      closedir(ver_dir);
    }

    closedir(base_dir);
  }

  return best_dir;
}

static const char* resolve_sysroot(compile_t* c, const char* sys_triple,
  errors_t* errors)
{
  // If user specified --sysroot, validate it.
  if(c->opt->sysroot != NULL && c->opt->sysroot[0] != '\0')
  {
    if(find_libc_crt_dir(c->opt->sysroot, sys_triple) != NULL)
      return c->opt->sysroot;

    errorf(errors, NULL,
      "sysroot '%s' does not contain libc CRT objects (crt1.o)\n"
      "  Searched: %s/usr/lib/%s/, %s/usr/lib/, %s/lib/%s/, %s/lib/",
      c->opt->sysroot,
      c->opt->sysroot, sys_triple,
      c->opt->sysroot,
      c->opt->sysroot, sys_triple,
      c->opt->sysroot);
    return NULL;
  }

  // Native compilation: use host root filesystem.
  if(!is_cross_compiling(c))
    return "";

  // Auto-detect from common cross-toolchain locations.
  const char* candidates[4];
  char buf[PATH_MAX];

  snprintf(buf, sizeof(buf), "/usr/%s", sys_triple);
  candidates[0] = stringtab(buf);

  snprintf(buf, sizeof(buf), "/usr/local/%s", sys_triple);
  candidates[1] = stringtab(buf);

  snprintf(buf, sizeof(buf), "/usr/%s/libc", sys_triple);
  candidates[2] = stringtab(buf);

  snprintf(buf, sizeof(buf), "/usr/local/%s/libc", sys_triple);
  candidates[3] = stringtab(buf);

  for(int i = 0; i < 4; i++)
  {
    if(find_libc_crt_dir(candidates[i], sys_triple) != NULL)
      return candidates[i];
  }

  errorf(errors, NULL,
    "cross-compiling for %s requires --sysroot=<path>\n"
    "  Searched: /usr/%s/, /usr/local/%s/,\n"
    "           /usr/%s/libc/, /usr/local/%s/libc/\n"
    "  Install a cross-toolchain or specify --sysroot explicitly.",
    c->opt->triple,
    sys_triple, sys_triple, sys_triple, sys_triple);
  return NULL;
}

static bool link_exe_lld_elf(compile_t* c, ast_t* program,
  const char* file_o)
{
  errors_t* errors = c->opt->check.errors;

  const char* file_exe =
    suffix_filename(c, c->opt->output, "", c->filename, "");

  if(c->opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Linking %s\n", file_exe);

  program_lib_build_args_embedded(program, c->opt);

  const char* sys_triple = system_triple(c);

  // Resolve sysroot.
  const char* sysroot = resolve_sysroot(c, sys_triple, errors);
  if(sysroot == NULL)
    return false;

  // Get target-derived values.
  const char* emulation = elf_emulation(c);
  if(emulation == NULL)
  {
    errorf(errors, NULL,
      "unsupported architecture for embedded LLD: %s", c->opt->triple);
    return false;
  }

  const char* dynlinker = dynamic_linker_path(c);
  if(!c->opt->staticbin && dynlinker == NULL)
  {
    errorf(errors, NULL,
      "unsupported architecture for dynamic linking: %s", c->opt->triple);
    return false;
  }

  // Find CRT directories.
  const char* libc_crt_dir = find_libc_crt_dir(sysroot, sys_triple);
  if(libc_crt_dir == NULL)
  {
    errorf(errors, NULL,
      "could not find libc CRT objects in sysroot '%s'", sysroot);
    return false;
  }

  const char* ponyc_crt_dir = find_ponyc_crt_dir(program, c);
  if(ponyc_crt_dir == NULL)
  {
    errorf(errors, NULL,
      "could not find compiler-rt CRT objects (crtbeginS.o) in lib paths");
    return false;
  }

  // GCC lib dir is optional.
  llvm::Triple target_triple(c->opt->triple);
  std::string arch_prefix = std::string(target_triple.getArchName()) + "-";
  const char* gcc_lib_dir = find_gcc_lib_dir(sysroot, arch_prefix.c_str());

  if(c->opt->link_ldcmd != NULL)
  {
    fprintf(stderr,
      "Warning: --link-ldcmd is ignored when using embedded LLD\n");
  }

  // Build argument vector.
  std::vector<const char*> args;
  char buf[PATH_MAX];

  args.push_back("ld.lld");
  args.push_back("-m");
  args.push_back(emulation);
  args.push_back("-z");
  args.push_back("relro");

  if(c->opt->staticbin)
  {
    args.push_back("-z");
    args.push_back("now");
  }

  args.push_back("--hash-style=both");
  args.push_back("--build-id");
  args.push_back("--eh-frame-hdr");

  // Pass --sysroot so LLD can resolve absolute paths in linker scripts
  // (e.g. libc.so referencing /lib/libc.so.6). However, some cross
  // toolchains (Debian gcc-cross packages) generate linker scripts with
  // fully-qualified paths that already include the sysroot prefix. LLD
  // doesn't fall back to the original path after prepending sysroot
  // (unlike GNU ld), so --sysroot would cause double-prepending. Detect
  // this by checking whether libc.so contains the sysroot path.
  {
    bool needs_sysroot = true;
    char libc_so_path[PATH_MAX];
    snprintf(libc_so_path, sizeof(libc_so_path), "%s/libc.so", libc_crt_dir);

    FILE* f = fopen(libc_so_path, "r");
    if(f != NULL)
    {
      char line[1024];
      while(fgets(line, sizeof(line), f) != NULL)
      {
        if(strstr(line, sysroot) != NULL)
        {
          needs_sysroot = false;
          break;
        }
      }
      fclose(f);
    }

    if(needs_sysroot)
    {
      snprintf(buf, sizeof(buf), "--sysroot=%s", sysroot);
      args.push_back(stringtab(buf));
    }
  }

  if(c->opt->staticbin)
  {
    args.push_back("-static");
    args.push_back("--as-needed");
  }
  else
  {
    args.push_back("-pie");
    args.push_back("-dynamic-linker");
    args.push_back(dynlinker);
  }

// The use of NDEBUG instead of PONY_NDEBUG here is intentional.
#ifndef NDEBUG
  args.push_back("--export-dynamic");
#endif

  if(c->opt->strip_debug)
    args.push_back("--strip-debug");

  // CRT startup objects.
  if(c->opt->staticbin)
  {
    snprintf(buf, sizeof(buf), "%s/crt1.o", libc_crt_dir);
    args.push_back(stringtab(buf));
  }
  else
  {
    // Prefer Scrt1.o (PIE startup) if available, fall back to crt1.o.
    snprintf(buf, sizeof(buf), "%s/Scrt1.o", libc_crt_dir);
    if(file_exists(buf))
      args.push_back(stringtab(buf));
    else
    {
      snprintf(buf, sizeof(buf), "%s/crt1.o", libc_crt_dir);
      args.push_back(stringtab(buf));
    }
  }

  snprintf(buf, sizeof(buf), "%s/crti.o", libc_crt_dir);
  args.push_back(stringtab(buf));

  if(c->opt->staticbin)
  {
    snprintf(buf, sizeof(buf), "%s/crtbeginT.o", ponyc_crt_dir);
    args.push_back(stringtab(buf));
  }
  else
  {
    snprintf(buf, sizeof(buf), "%s/crtbeginS.o", ponyc_crt_dir);
    args.push_back(stringtab(buf));
  }

  // Library search paths: sysroot dirs first, then GCC, then ponyc/user.
  snprintf(buf, sizeof(buf), "-L%s", libc_crt_dir);
  args.push_back(stringtab(buf));

  if(gcc_lib_dir != NULL)
  {
    snprintf(buf, sizeof(buf), "-L%s", gcc_lib_dir);
    args.push_back(stringtab(buf));

    // GCC installs shared runtime libraries (libatomic, libgcc_s) in
    // <prefix>/<triple>/lib/ while static libraries (libgcc.a) go in
    // <prefix>/lib/gcc[-cross]/<triple>/<version>/. Derive the former
    // from the latter.
    char gcc_target_lib[PATH_MAX];
    snprintf(gcc_target_lib, sizeof(gcc_target_lib),
      "%s/../../../../%s/lib", gcc_lib_dir, sys_triple);

    struct stat gcc_target_stat;
    if(stat(gcc_target_lib, &gcc_target_stat) == 0
      && S_ISDIR(gcc_target_stat.st_mode))
    {
      snprintf(buf, sizeof(buf), "-L%s", gcc_target_lib);
      args.push_back(stringtab(buf));
    }
  }

  // Add ponyc lib paths and user lib paths.
  size_t path_count = program_lib_path_count(program);
  for(size_t i = 0; i < path_count; i++)
  {
    const char* path = program_lib_path_at(program, i);
    snprintf(buf, sizeof(buf), "-L%s", path);
    args.push_back(stringtab(buf));

    if(!c->opt->staticbin)
    {
      args.push_back("-rpath");
      args.push_back(path);
    }
  }

  // Object file.
  args.push_back(file_o);

  // User libraries.
  size_t lib_count = program_lib_count(program);
  for(size_t i = 0; i < lib_count; i++)
  {
    const char* lib = program_lib_at(program, i);
    if(is_path_absolute(lib))
    {
      args.push_back(lib);
    }
    else
    {
      snprintf(buf, sizeof(buf), "-l%s", lib);
      args.push_back(stringtab(buf));
    }
  }

  // Pony runtime. Use the full path from ponyc_crt_dir to ensure we
  // pick the target-architecture library, not a native-arch copy that
  // might appear earlier in the -L search path.
  if(!c->opt->runtimebc)
  {
    const char* rt_name = c->opt->staticbin ? "libponyrt.a"
      : (c->opt->pic ? "libponyrt-pic.a" : "libponyrt.a");

    snprintf(buf, sizeof(buf), "%s/%s", ponyc_crt_dir, rt_name);
    if(file_exists(buf))
    {
      args.push_back(stringtab(buf));
    }
    else
    {
      if(c->opt->staticbin)
        args.push_back("-lponyrt");
      else
        args.push_back(c->opt->pic ? "-lponyrt-pic" : "-lponyrt");
    }
  }

  args.push_back("-lpthread");
  args.push_back("-lm");
  args.push_back("-ldl");
  args.push_back("-latomic");

  // ARM-specific exclude-libs.
  if(target_is_arm(c->opt->triple))
  {
    args.push_back("--exclude-libs");
    args.push_back("libgcc.a");
    args.push_back("--exclude-libs");
    args.push_back("libgcc_real.a");
    args.push_back("--exclude-libs");
    args.push_back("libgnustl_shared.so");
    args.push_back("--exclude-libs");
    args.push_back("libunwind.a");
  }

  // GCC runtime and libc linkage.
  if(c->opt->staticbin)
  {
    args.push_back("--start-group");
    args.push_back("-lgcc");
    args.push_back("-lgcc_eh");
    args.push_back("-lc");
    args.push_back("--end-group");
    args.push_back("-lssp_nonshared");
  }
  else
  {
    args.push_back("-lgcc");
    args.push_back("--as-needed");
    args.push_back("-lgcc_s");
    args.push_back("--no-as-needed");
    args.push_back("-lc");
  }

  // CRT finalization objects.
  if(c->opt->staticbin)
  {
    snprintf(buf, sizeof(buf), "%s/crtend.o", ponyc_crt_dir);
    args.push_back(stringtab(buf));
  }
  else
  {
    snprintf(buf, sizeof(buf), "%s/crtendS.o", ponyc_crt_dir);
    args.push_back(stringtab(buf));
  }

  snprintf(buf, sizeof(buf), "%s/crtn.o", libc_crt_dir);
  args.push_back(stringtab(buf));

  args.push_back("-o");
  args.push_back(file_exe);

  // Log the command if verbose.
  if(c->opt->verbosity >= VERBOSITY_TOOL_INFO)
  {
    std::string cmd;
    for(size_t i = 0; i < args.size(); i++)
    {
      if(i > 0) cmd += " ";
      cmd += args[i];
    }
    fprintf(stderr, "%s\n", cmd.c_str());
  }

  // Invoke LLD.
  std::vector<const char*> lld_args(args.begin(), args.end());
  std::string lld_stdout_str;
  std::string lld_stderr_str;
  llvm::raw_string_ostream lld_stdout(lld_stdout_str);
  llvm::raw_string_ostream lld_stderr(lld_stderr_str);

  lld::Result result = lld::lldMain(
    lld_args,
    lld_stdout,
    lld_stderr,
    {{lld::WinLink, &lld::coff::link},
     {lld::Gnu, &lld::elf::link},
     {lld::Darwin, &lld::macho::link},
     {lld::MinGW, &lld::mingw::link},
     {lld::Wasm, &lld::wasm::link}});

  if(result.retCode != 0)
  {
    errorf(errors, NULL, "unable to link: %s", lld_stderr_str.c_str());
    return false;
  }

  return true;
}

static const char* find_macos_sdk_path()
{
  static const char* cached = NULL;
  static bool searched = false;

  if(searched)
    return cached;
  searched = true;

  char sdk_path[PATH_MAX];

  // Try xcrun first.
  FILE* f = popen("xcrun --show-sdk-path 2>/dev/null", "r");
  if(f != NULL)
  {
    if(fgets(sdk_path, sizeof(sdk_path), f) != NULL)
    {
      // Strip trailing newline.
      size_t len = strlen(sdk_path);
      if(len > 0 && sdk_path[len - 1] == '\n')
        sdk_path[len - 1] = '\0';

      char lib_path[PATH_MAX];
      snprintf(lib_path, sizeof(lib_path), "%s/usr/lib", sdk_path);

      struct stat st;
      if(stat(lib_path, &st) == 0 && S_ISDIR(st.st_mode))
      {
        cached = stringtab(lib_path);
        pclose(f);
        return cached;
      }
    }
    pclose(f);
  }

  // Hardcoded fallback (same as legacy code).
  const char* fallback =
    "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib";

  struct stat st;
  if(stat(fallback, &st) == 0 && S_ISDIR(st.st_mode))
  {
    cached = stringtab(fallback);
    return cached;
  }

  return NULL;
}

static const char* macho_arch_name(compile_t* c)
{
  llvm::Triple triple(c->opt->triple);

  switch(triple.getArch())
  {
    case llvm::Triple::aarch64: return "arm64";
    case llvm::Triple::x86_64: return "x86_64";
    default: return NULL;
  }
}

static const char* macho_platform_version(compile_t* c)
{
  llvm::Triple triple(c->opt->triple);

  llvm::VersionTuple ver = triple.getOSVersion();
  unsigned major = ver.getMajor();
  unsigned minor = ver.getMinor().value_or(0);
  unsigned micro = ver.getSubminor().value_or(0);

  if(major == 0)
    return NULL;

  char buf[32];
  snprintf(buf, sizeof(buf), "%u.%u.%u", major, minor, micro);
  return stringtab(buf);
}

static bool link_exe_lld_macho(compile_t* c, ast_t* program,
  const char* file_o)
{
  errors_t* errors = c->opt->check.errors;

  const char* file_exe =
    suffix_filename(c, c->opt->output, "", c->filename, "");

  if(c->opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Linking %s\n", file_exe);

  program_lib_build_args_embedded(program, c->opt);

  const char* macho_arch = macho_arch_name(c);
  if(macho_arch == NULL)
  {
    errorf(errors, NULL,
      "unsupported architecture for embedded LLD Mach-O: %s", c->opt->triple);
    return false;
  }

  const char* platform_ver = macho_platform_version(c);
  if(platform_ver == NULL)
  {
    errorf(errors, NULL,
      "could not determine macOS platform version from triple: %s",
      c->opt->triple);
    return false;
  }

  const char* sdk_lib_path = find_macos_sdk_path();
  if(sdk_lib_path == NULL)
  {
    errorf(errors, NULL,
      "could not find macOS SDK library path\n"
      "  Install Xcode or CommandLineTools, or use --linker to specify an"
      " external linker");
    return false;
  }

  if(c->opt->link_ldcmd != NULL)
  {
    fprintf(stderr,
      "Warning: --link-ldcmd is ignored when using embedded LLD\n");
  }

  // Build argument vector.
  std::vector<const char*> args;
  char buf[PATH_MAX];

  args.push_back("ld64.lld");
  args.push_back("-execute");
  args.push_back("-arch");
  args.push_back(macho_arch);

  args.push_back("-platform_version");
  args.push_back("macos");
  args.push_back(platform_ver);
  args.push_back("0.0.0");

  // SDK library path.
  snprintf(buf, sizeof(buf), "-L%s", sdk_lib_path);
  args.push_back(stringtab(buf));

  // Ponyc lib paths and user lib paths.
  size_t path_count = program_lib_path_count(program);
  for(size_t i = 0; i < path_count; i++)
  {
    const char* path = program_lib_path_at(program, i);
    snprintf(buf, sizeof(buf), "-L%s", path);
    args.push_back(stringtab(buf));
  }

  // Object file.
  args.push_back(file_o);

  // User libraries.
  size_t lib_count = program_lib_count(program);
  for(size_t i = 0; i < lib_count; i++)
  {
    const char* lib = program_lib_at(program, i);
    if(is_path_absolute(lib))
    {
      args.push_back(lib);
    }
    else
    {
      snprintf(buf, sizeof(buf), "-l%s", lib);
      args.push_back(stringtab(buf));
    }
  }

  // System library and Pony runtime.
  args.push_back("-lSystem");

  if(!c->opt->runtimebc)
  {
    args.push_back(c->opt->pic ? "-lponyrt-pic" : "-lponyrt");
  }

  args.push_back("-o");
  args.push_back(file_exe);

  // Log the command if verbose.
  if(c->opt->verbosity >= VERBOSITY_TOOL_INFO)
  {
    std::string cmd;
    for(size_t i = 0; i < args.size(); i++)
    {
      if(i > 0) cmd += " ";
      cmd += args[i];
    }
    fprintf(stderr, "%s\n", cmd.c_str());
  }

  // Invoke LLD.
  std::vector<const char*> lld_args(args.begin(), args.end());
  std::string lld_stdout_str;
  std::string lld_stderr_str;
  llvm::raw_string_ostream lld_stdout(lld_stdout_str);
  llvm::raw_string_ostream lld_stderr(lld_stderr_str);

  lld::Result result = lld::lldMain(
    lld_args,
    lld_stdout,
    lld_stderr,
    {{lld::WinLink, &lld::coff::link},
     {lld::Gnu, &lld::elf::link},
     {lld::Darwin, &lld::macho::link},
     {lld::MinGW, &lld::mingw::link},
     {lld::Wasm, &lld::wasm::link}});

  if(result.retCode != 0)
  {
    errorf(errors, NULL, "unable to link: %s", lld_stderr_str.c_str());
    return false;
  }

  // Run dsymutil unless stripping debug info.
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

  return true;
}
#endif

static bool link_exe(compile_t* c, ast_t* program,
  const char* file_o)
{
  errors_t* errors = c->opt->check.errors;

  // Use embedded LLD for Linux and macOS targets unless --linker escape hatch
  // is specified. Sanitizer builds fall back to the system compiler driver for
  // native compilation since sanitizer runtime libraries need compiler-specific
  // link logic; cross-compilation still uses LLD.
#ifdef PLATFORM_IS_POSIX_BASED
  if(c->opt->linker == NULL
    && target_is_linux(c->opt->triple)
#if defined(PONY_SANITIZER)
    && is_cross_compiling(c)
#endif
    )
  {
    return link_exe_lld_elf(c, program, file_o);
  }

  if(c->opt->linker == NULL
    && target_is_macosx(c->opt->triple)
#if defined(PONY_SANITIZER)
    && is_cross_compiling(c)
#endif
    )
  {
    return link_exe_lld_macho(c, program, file_o);
  }
#endif

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
