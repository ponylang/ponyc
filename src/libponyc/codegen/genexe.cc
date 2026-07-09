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

#include <llvm/Support/raw_ostream.h>
#include <vector>
#include <string>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <unistd.h>
#  include <errno.h>
#  include <sys/stat.h>
#  include <sys/wait.h>
#  include <dirent.h>
#  include <spawn.h>
#  include <llvm/TargetParser/Triple.h>
#endif

#if defined(PONY_SANITIZER)
// Generated at configure time (top-level CMakeLists.txt, PONY_SANITIZERS_ENABLED
// block): the sanitizer runtime link fragment captured from the compiler driver.
// Exists only in sanitizer builds, so the include is guarded to match.
#  include "sanitizer_link_args.h"
#endif

#if defined(PONY_COVERAGE)
// Generated at configure time (top-level CMakeLists.txt, coverage capture block):
// the coverage runtime (libgcov) link fragment captured from the compiler
// driver. Exists only in gcc coverage builds, so the include is guarded to match.
#  include "coverage_link_args.h"
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

  LLVMTypeRef f_params[1];
  f_params[0] = boolean;

  LLVMTypeRef lfi_type = LLVMStructTypeInContext(c->context, f_params, 1,
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
  reach_method_t* m = reach_method(t_env, TK_NONE, c->str__create, NULL, c->opt);

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
  uint32_t index = reach_vtable_index(t_main, c->str_create, c->opt);
  size_t msg_size = (size_t)LLVMABISizeOfType(c->target_data, msg_type);
  args[0] = LLVMConstInt(c->i32, ponyint_pool_index(msg_size), false);
  args[1] = LLVMConstInt(c->i32, index, false);
  LLVMValueRef msg = gencall_runtime(c, "pony_alloc_msg", args, 2, "");
  LLVMValueRef msg_ptr = msg;

  // Set the message contents.
  LLVMValueRef env_ptr = LLVMBuildStructGEP2(c->builder, msg_type, msg_ptr, 3,
    "");
  LLVMBuildStore(c->builder, env, env_ptr);

  // Trace the message. Pass a null destination: the bootstrap send is not a
  // self-send, so no object should be pinned for it.
  args[0] = ctx;
  args[1] = LLVMConstNull(c->ptr);
  gencall_runtime(c, "pony_gc_send", args, 2, "");

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

  // Start the runtime. The arg count and types here must match libponyrt's
  // pony_start and the prototype in codegen.c.
  args[0] = LLVMConstNull(c->ptr);
  args[1] = make_lang_features_init(c);
  LLVMValueRef start_success = gencall_runtime(c, "pony_start", args, 2, "");

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

#ifdef PLATFORM_IS_POSIX_BASED
// is_cross_compiling and system_triple are shared with gencshim and live in
// genopt (declared in genopt.h); both take pass_opt_t.
// target_libc_is_musl (defined below, declared in genexe.h) is likewise shared
// with gencshim: the interpreter choice and the shim header ordering both call
// it so they can never disagree about the target libc.

static const char* elf_emulation(compile_t* c)
{
  llvm::Triple triple(c->opt->triple);

  // FreeBSD: the _fbsd suffix makes lld set ELFOSABI_FREEBSD. ponyc supports
  // x86-64 only on FreeBSD; other arches return NULL so the link errors
  // cleanly rather than producing a mis-branded binary.
  if(target_is_freebsd(c->opt->triple))
  {
    switch(triple.getArch())
    {
      case llvm::Triple::x86_64: return "elf_x86_64_fbsd";
      default: return NULL;
    }
  }

  // DragonFly: no OSABI suffix exists in lld for DragonFly and DragonFly's
  // kernel accepts ELFOSABI_NONE binaries (existing system binaries are
  // stamped that way). ponyc supports x86-64 only; explicit NULL for other
  // arches prevents non-x86 DragonFly from falling through to the default
  // Linux-style switch below.
  if(target_is_dragonfly(c->opt->triple))
  {
    switch(triple.getArch())
    {
      case llvm::Triple::x86_64: return "elf_x86_64";
      default: return NULL;
    }
  }

  // OpenBSD: `_obsd` suffix tells lld's parseEmulation to set
  // ctx.arg.osabi = ELFOSABI_OPENBSD on the output, which gates emission
  // of OpenBSD-specific program headers (PT_OPENBSD_RANDOMIZE,
  // PT_OPENBSD_MUTABLE, PT_OPENBSD_SYSCALLS) and is what the OpenBSD
  // kernel uses to enable per-binary security features (random retguard
  // cookies, syscall pinning). Stock lld doesn't ship the `_obsd` suffix;
  // ponyc adds it via the lld-openbsd-section-merge.diff patch in
  // lib/llvm/patches, modeled on lld's existing `_fbsd` handling. The
  // alternative path of letting lld infer osabi from input objects only
  // fires when `-m` is absent (Driver.cpp inferMachineType bails when
  // ekind is already set), so the brand must come from the emulation
  // string.
  //
  // ponyc supports x86-64 only on OpenBSD; explicit NULL on other arches
  // errors cleanly rather than mis-branding.
  if(target_is_openbsd(c->opt->triple))
  {
    switch(triple.getArch())
    {
      case llvm::Triple::x86_64: return "elf_x86_64_obsd";
      default: return NULL;
    }
  }

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

static const char* dynamic_linker_path(compile_t* c)
{
  llvm::Triple triple(c->opt->triple);

  // FreeBSD uses a single rtld for all arches (ponyc supports x86-64 only).
  if(target_is_freebsd(c->opt->triple))
    return "/libexec/ld-elf.so.1";

  // DragonFly's base toolchain uses /libexec/ld-elf.so.2 (verified: all
  // base binaries' INTERP points here). /usr/libexec/ld-elf.so.2 is a
  // symlink and works, but /libexec/ is the convention.
  if(target_is_dragonfly(c->opt->triple))
    return "/libexec/ld-elf.so.2";

  // OpenBSD: single rtld at /usr/libexec/ld.so for all arches (ponyc
  // supports x86-64 only). Verified: /libexec/ld.so does NOT exist on
  // OpenBSD; /usr/libexec/ld.so is the only one. Base binaries' INTERP
  // points here.
  if(target_is_openbsd(c->opt->triple))
    return "/usr/libexec/ld.so";

  bool is_musl = target_libc_is_musl(c->opt);

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

// The GNU/Debian-family multiarch architecture component for a target.
// LLVM spells 32-bit little-endian ARM as the raw triple arch — "armv7l",
// "armv6", "armv7", etc. — but Debian/Ubuntu/Raspbian multiarch directories
// (/usr/lib/<tuple>, /usr/lib/gcc/<tuple>/) and GNU cross-toolchain triples
// normalize all of them to "arm"; the EABI float variant is carried in the
// environment (gnueabihf vs gnueabi), not the arch. The other architectures
// this embedded linker supports (see expected_elf_machine: x86_64, aarch64,
// riscv64, riscv32) already match LLVM's spelling, so fall back to
// getArchName() there. system_triple (genopt) carries a twin of this for the
// multiarch tuple it builds; keep the two in sync.
static std::string gnu_multiarch_arch(const llvm::Triple& triple)
{
  switch(triple.getArch())
  {
    case llvm::Triple::arm:
    case llvm::Triple::thumb:
      return "arm";
    default:
      return std::string(triple.getArchName());
  }
}

static uint16_t expected_elf_machine(char* target_triple)
{
  llvm::Triple triple(target_triple);

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

// The standard sysroot library directories, in search order, where a target's
// libc startup objects and shared libraries live. Shared by find_libc_crt_dir
// (which looks for crt1.o/crt0.o) and target_libc_is_musl (which looks for
// libc.so.6) so both search the same set -- including non-usr-merged layouts
// where /lib/<triple> and /usr/lib/<triple> are distinct directories. The lib64
// entries cover Fedora/RHEL and other distros that place 64-bit objects in
// /usr/lib64 rather than /usr/lib/<triple> or /usr/lib.
static void libc_lib_dirs(const char* sysroot, const char* sys_triple,
  strtable_t* strtab, const char* candidates[6])
{
  char buf[PATH_MAX];

  snprintf(buf, sizeof(buf), "%s/usr/lib/%s", sysroot, sys_triple);
  candidates[0] = stringtab(strtab, buf);

  snprintf(buf, sizeof(buf), "%s/usr/lib", sysroot);
  candidates[1] = stringtab(strtab, buf);

  snprintf(buf, sizeof(buf), "%s/lib/%s", sysroot, sys_triple);
  candidates[2] = stringtab(strtab, buf);

  snprintf(buf, sizeof(buf), "%s/lib", sysroot);
  candidates[3] = stringtab(strtab, buf);

  snprintf(buf, sizeof(buf), "%s/usr/lib64", sysroot);
  candidates[4] = stringtab(strtab, buf);

  snprintf(buf, sizeof(buf), "%s/lib64", sysroot);
  candidates[5] = stringtab(strtab, buf);
}

static const char* find_libc_crt_dir(const char* sysroot,
  const char* sys_triple, uint16_t target_machine, strtable_t* strtab)
{
  // Per candidate directory, try crt1.o first (Linux/FreeBSD/DragonFly), then
  // crt0.o (OpenBSD, which has no crt1.o at all). The sentinel is validated
  // against the target architecture via elf_matches_target. On multilib hosts
  // (e.g. an x86_64 system with glibc-devel.i686 installed) /usr/lib/crt1.o is
  // the 32-bit object while /usr/lib64/crt1.o is the 64-bit one -- without arch
  // validation the search would return /usr/lib and the link would fail later
  // inside LLD with an arch-mismatch error. Validating the sentinel alone is
  // sufficient because the multilib/per-OS layouts ship matched sets of startup
  // objects in each directory.
  const char* candidates[6];
  libc_lib_dirs(sysroot, sys_triple, strtab, candidates);

  static const char* sentinels[] = { "crt1.o", "crt0.o" };
  char buf[PATH_MAX];

  for(size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++)
  {
    for(size_t j = 0; j < sizeof(sentinels) / sizeof(sentinels[0]); j++)
    {
      snprintf(buf, sizeof(buf), "%s/%s", candidates[i], sentinels[j]);
      if(file_exists(buf) && elf_matches_target(buf, target_machine))
        return candidates[i];
    }
  }

  return NULL;
}

bool target_libc_is_musl(pass_opt_t* opt)
{
  // Musl is a Linux-only libc. On the BSDs, macOS, and Windows the target is
  // never musl -- and their libc has no libc.so.6, so the glibc fingerprint
  // below must not run there or it would misread every non-glibc libc as musl.
  if(!target_is_linux(opt->triple))
    return false;

  llvm::Triple triple(opt->triple);

  // An explicit --triple is the user's declaration of the target libc. Honor
  // it exactly; a host probe must never override what the user asked for.
  if(opt->user_triple)
    return triple.isMusl();

  // A default triple can occasionally name musl correctly; trust that. When it
  // claims gnu we cannot -- the vendored LLVM reports "gnu" even on musl hosts
  // -- so identify the libc we will actually link instead of trusting it.
  if(triple.isMusl())
    return true;

  // Fingerprint the libc we are about to link. glibc always ships libc.so.6 in
  // one of the standard lib directories; musl never does (its libc is the
  // dynamic loader itself). Probe the same directories find_libc_crt_dir
  // searches -- not just the one holding the crt objects -- so a non-usr-merged
  // layout that splits libc.so.6 (/lib/<triple>) from crt1.o (/usr/lib/<triple>)
  // still reads as glibc. Found anywhere -> glibc; absent everywhere -> musl.
  const char* sys_triple = system_triple(opt);
  const char* sysroot =
    (opt->sysroot != NULL && opt->sysroot[0] != '\0') ? opt->sysroot : "";

  const char* candidates[6];
  libc_lib_dirs(sysroot, sys_triple, opt->strtab, candidates);

  char buf[PATH_MAX];
  for(size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++)
  {
    snprintf(buf, sizeof(buf), "%s/libc.so.6", candidates[i]);
    if(file_exists(buf))
      return false;
  }

  return true;
}

static const char* find_ponyc_crt_dir(ast_t* program, compile_t* c)
{
  uint16_t target_machine = expected_elf_machine(c->opt->triple);
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

// DragonFly-specific GCC runtime directory finder. Two layouts coexist:
//
//   nested (pkg gccNN):  <sysroot>/usr/local/lib/gccNN/gcc/<triple>/<ver>/
//   flat   (base gccNN): <sysroot>/usr/lib/gccNN/
//
// Pkg gcc is preferred because libponyrt is built with it (per BUILD.md).
// The flat (base) layout exists but uses libgcc_pic rather than libgcc_s
// and ships no libatomic/libexecinfo of its own, so an embedded LLD link
// against it will fail to resolve symbols — we warn and proceed (rather
// than abort) so the linker emits the actionable diagnostic rather than
// ponyc failing first with a vaguer one.
//
// The generic find_gcc_lib_dir below assumes <base>/<triple>/<ver>/
// nesting under fixed base_patterns (/usr/lib/gcc, /usr/lib/gcc-cross,
// /usr/local/lib/gcc, ...). Neither DragonFly layout matches: pkg gcc
// installs to /usr/local/lib/gccNN (NN suffix, not the bare /gcc dir),
// and base gcc has no <triple>/<ver>/ nesting. Hence a separate helper.
//
// Return contract: when pass 1 finds a result, the returned path is the
// deepest layer (gccNN/gcc/<triple>/<ver>/) — the gcc-root (where
// libgcc_s.so and libatomic.so live) is exactly 3 levels above it. The
// caller depends on this for -L<gcc_root> and -rpath=<gcc_root>; if the
// return contract changes the call site in link_exe_lld_elf must change
// in lockstep.
static const char* find_dragonfly_gcc_lib_dir(const char* sysroot,
  const char* arch_prefix, strtable_t* strtab)
{
  char buf[PATH_MAX];

  // Pass 1: nested pkg layout. Walk /usr/local/lib/gccNN/gcc/<triple>/<ver>/.
  // Filter <triple> by arch_prefix so a host with both x86_64 and aarch64
  // gcc installs (cross toolchains) picks the right one, mirroring
  // find_gcc_lib_dir below. Today ponyc-on-DragonFly is x86_64-only but
  // the filter is cheap defense in depth.
  int best_gcc_major = -1;
  int best_ver = -1;
  const char* best_dir = NULL;
  size_t prefix_len = strlen(arch_prefix);

  snprintf(buf, sizeof(buf), "%s/usr/local/lib", sysroot);
  DIR* base_dir = opendir(buf);
  if(base_dir != NULL)
  {
    struct dirent* entry;
    while((entry = readdir(base_dir)) != NULL)
    {
      if(strncmp(entry->d_name, "gcc", 3) != 0)
        continue;
      int gcc_major = atoi(entry->d_name + 3);
      if(gcc_major <= 0)
        continue;

      char gcc_subdir[PATH_MAX];
      int n = snprintf(gcc_subdir, sizeof(gcc_subdir),
        "%s/usr/local/lib/%s/gcc", sysroot, entry->d_name);
      if(n < 0 || (size_t)n >= sizeof(gcc_subdir))
        continue;
      DIR* gcc_sub = opendir(gcc_subdir);
      if(gcc_sub == NULL)
        continue;

      struct dirent* triple_entry;
      while((triple_entry = readdir(gcc_sub)) != NULL)
      {
        if(triple_entry->d_name[0] == '.')
          continue;
        if(strncmp(triple_entry->d_name, arch_prefix, prefix_len) != 0)
          continue;

        char triple_dir[PATH_MAX];
        n = snprintf(triple_dir, sizeof(triple_dir), "%s/%s",
          gcc_subdir, triple_entry->d_name);
        if(n < 0 || (size_t)n >= sizeof(triple_dir))
          continue;
        DIR* ver_dir = opendir(triple_dir);
        if(ver_dir == NULL)
          continue;

        struct dirent* ver_entry;
        while((ver_entry = readdir(ver_dir)) != NULL)
        {
          if(ver_entry->d_name[0] == '.')
            continue;
          int ver = atoi(ver_entry->d_name);
          if(ver <= 0)
            continue;

          char libgcc[PATH_MAX];
          n = snprintf(libgcc, sizeof(libgcc), "%s/%s/libgcc.a",
            triple_dir, ver_entry->d_name);
          if(n < 0 || (size_t)n >= sizeof(libgcc))
            continue;
          if(!file_exists(libgcc))
            continue;

          if(gcc_major > best_gcc_major
            || (gcc_major == best_gcc_major && ver > best_ver))
          {
            best_gcc_major = gcc_major;
            best_ver = ver;
            char result[PATH_MAX];
            n = snprintf(result, sizeof(result), "%s/%s",
              triple_dir, ver_entry->d_name);
            if(n < 0 || (size_t)n >= sizeof(result))
              continue;
            best_dir = stringtab(strtab, result);
          }
        }
        closedir(ver_dir);
      }
      closedir(gcc_sub);
    }
    closedir(base_dir);
  }

  if(best_dir != NULL)
    return best_dir;

  // Pass 2: flat base layout. Walk /usr/lib/gccNN/. Base gcc lacks
  // libgcc_s, libatomic, and libexecinfo's static-link companion, so the
  // link will fail downstream — but the linker's "cannot find -lX" output
  // is the actionable diagnostic for the user. Warn loudly here so the
  // root cause is visible before the linker error.
  best_gcc_major = -1;

  snprintf(buf, sizeof(buf), "%s/usr/lib", sysroot);
  base_dir = opendir(buf);
  if(base_dir != NULL)
  {
    struct dirent* entry;
    while((entry = readdir(base_dir)) != NULL)
    {
      if(strncmp(entry->d_name, "gcc", 3) != 0)
        continue;
      int gcc_major = atoi(entry->d_name + 3);
      if(gcc_major <= 0)
        continue;

      char libgcc[PATH_MAX];
      int n = snprintf(libgcc, sizeof(libgcc), "%s/usr/lib/%s/libgcc.a",
        sysroot, entry->d_name);
      if(n < 0 || (size_t)n >= sizeof(libgcc))
        continue;
      if(!file_exists(libgcc))
        continue;

      if(gcc_major > best_gcc_major)
      {
        best_gcc_major = gcc_major;
        char result[PATH_MAX];
        n = snprintf(result, sizeof(result), "%s/usr/lib/%s",
          sysroot, entry->d_name);
        if(n < 0 || (size_t)n >= sizeof(result))
          continue;
        best_dir = stringtab(strtab, result);
      }
    }
    closedir(base_dir);
  }

  if(best_dir != NULL)
  {
    fprintf(stderr,
      "Warning: DragonFly pkg gcc not found; falling back to base GCC at"
      " %s.\n  The link will likely fail to resolve -lgcc_s/-latomic — base"
      " gcc lacks them.\n  Install pkg gcc (e.g. gcc13) to fix.\n",
      best_dir);
  }

  return best_dir;
}

static const char* find_gcc_lib_dir(const char* sysroot,
  const char* arch_prefix, strtable_t* strtab)
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
          best_dir = stringtab(strtab, result);
        }
      }

      closedir(ver_dir);
    }

    closedir(base_dir);
  }

  return best_dir;
}

// The cross-toolchain sysroot to use when cross-compiling without an
// explicit --sysroot: the four standard /usr/<triple> locations, each
// validated by an arch-matching libc crt object. SHARED between the ELF
// linker (resolve_sysroot, below) and the C shim compiler (gencshim's
// c_shim_sysroot) so a cross build resolves the SAME sysroot whether it is
// linking or compiling a shim — one definition, so the two can't drift.
// Returns the sysroot path, or NULL after emitting the "requires --sysroot"
// error when none of the candidates holds a matching libc.
const char* find_cross_toolchain_sysroot(pass_opt_t* opt, errors_t* errors)
{
  const char* sys_triple = system_triple(opt);
  uint16_t target_machine = expected_elf_machine(opt->triple);

  const char* candidates[4];
  char buf[PATH_MAX];

  snprintf(buf, sizeof(buf), "/usr/%s", sys_triple);
  candidates[0] = stringtab(opt->strtab, buf);

  snprintf(buf, sizeof(buf), "/usr/local/%s", sys_triple);
  candidates[1] = stringtab(opt->strtab, buf);

  snprintf(buf, sizeof(buf), "/usr/%s/libc", sys_triple);
  candidates[2] = stringtab(opt->strtab, buf);

  snprintf(buf, sizeof(buf), "/usr/local/%s/libc", sys_triple);
  candidates[3] = stringtab(opt->strtab, buf);

  for(int i = 0; i < 4; i++)
  {
    if(find_libc_crt_dir(candidates[i], sys_triple, target_machine,
      opt->strtab) != NULL)
      return candidates[i];
  }

  errorf(errors, NULL,
    "cross-compiling for %s requires --sysroot=<path>\n"
    "  Searched: /usr/%s/, /usr/local/%s/,\n"
    "           /usr/%s/libc/, /usr/local/%s/libc/\n"
    "  Install a cross-toolchain or specify --sysroot explicitly.",
    opt->triple,
    sys_triple, sys_triple, sys_triple, sys_triple);
  return NULL;
}

static const char* resolve_sysroot(compile_t* c, const char* sys_triple,
  errors_t* errors)
{
  uint16_t target_machine = expected_elf_machine(c->opt->triple);

  // If user specified --sysroot, validate it.
  if(c->opt->sysroot != NULL && c->opt->sysroot[0] != '\0')
  {
    if(find_libc_crt_dir(c->opt->sysroot, sys_triple, target_machine, c->opt->strtab) != NULL)
      return c->opt->sysroot;

    errorf(errors, NULL,
      "sysroot '%s' does not contain a libc crt startup object "
      "(crt1.o or crt0.o) matching target architecture '%s'\n"
      "  Searched: %s/usr/lib/%s/, %s/usr/lib/, %s/lib/%s/, %s/lib/,\n"
      "            %s/usr/lib64/, %s/lib64/",
      c->opt->sysroot,
      c->opt->triple,
      c->opt->sysroot, sys_triple,
      c->opt->sysroot,
      c->opt->sysroot, sys_triple,
      c->opt->sysroot,
      c->opt->sysroot,
      c->opt->sysroot);
    return NULL;
  }

  // Native compilation: use host root filesystem.
  if(!is_cross_compiling(c->opt))
    return "";

  // Cross build with no --sysroot: auto-detect from the standard
  // cross-toolchain locations. The shim compiler shares this function.
  return find_cross_toolchain_sysroot(c->opt, errors);
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

  const char* sys_triple = system_triple(c->opt);
  bool is_freebsd = target_is_freebsd(c->opt->triple);
  bool is_dragonfly = target_is_dragonfly(c->opt->triple);
  bool is_openbsd = target_is_openbsd(c->opt->triple);
  // FreeBSD and DragonFly executables are non-PIE by default (matching
  // the base toolchain), and non-PIE is required to link the non-PIC
  // static libraries some programs pull in (e.g. the system libc++ bundled
  // into libponyc-standalone, which the Pony-based tools link). PIE stays
  // the default for dynamic links everywhere else, including OpenBSD which
  // enforces PIE strictly (the kernel refuses to exec non-PIE binaries).
  // This `pie` flag only gates the dynamic-side `-pie` push at the static/
  // dynamic split below; for OpenBSD static, where the binary is still
  // static-PIE, an explicit `-pie` is passed alongside `-static` in the
  // static branch (see comment there for why both flags are needed
  // together).
  bool pie = !c->opt->staticbin && !is_freebsd && !is_dragonfly;

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
  uint16_t target_machine = expected_elf_machine(c->opt->triple);
  const char* libc_crt_dir = find_libc_crt_dir(sysroot, sys_triple,
    target_machine, c->opt->strtab);
  if(libc_crt_dir == NULL)
  {
    errorf(errors, NULL,
      "could not find a libc crt startup object (crt1.o or crt0.o) "
      "matching target architecture '%s' in sysroot '%s'",
      c->opt->triple, sysroot);
    return false;
  }

  // crtbegin/crtend come from ponyc's shipped compiler-rt CRT on most
  // platforms (on Linux they'd otherwise come from GCC, which the embedded
  // linker must not require). On FreeBSD they're part of the base system in
  // /usr/lib alongside crt1.o, so source them from libc_crt_dir and don't
  // ship our own. On DragonFly they're under a pkg-installed gccNN
  // directory (/usr/local/lib/gccNN/gcc/<triple>/<ver>/) or, as a fallback,
  // under base /usr/lib/gccNN/; the DragonFly-specific resolver finds
  // either layout. On OpenBSD they live in /usr/lib alongside crt0.o
  // (base toolchain is clang + compiler-rt), so the FreeBSD shape applies
  // — source from libc_crt_dir. ponyc_crt_dir stays NULL on the BSDs; the
  // libponyrt block below handles that.
  llvm::Triple target_triple(c->opt->triple);
  std::string arch_prefix = gnu_multiarch_arch(target_triple) + "-";

  const char* ponyc_crt_dir = NULL;
  const char* compiler_crt_dir;
  if(is_freebsd || is_openbsd)
  {
    compiler_crt_dir = libc_crt_dir;
  }
  else if(is_dragonfly)
  {
    compiler_crt_dir = find_dragonfly_gcc_lib_dir(sysroot,
      arch_prefix.c_str(), c->opt->strtab);
    if(compiler_crt_dir == NULL)
    {
      errorf(errors, NULL,
        "could not find GCC runtime directory on DragonFly (searched"
        " %s/usr/local/lib/gcc*/gcc/<triple>/<ver>/ and"
        " %s/usr/lib/gcc*/); install pkg gcc (e.g. gcc13)",
        sysroot, sysroot);
      return false;
    }
  }
  else
  {
    ponyc_crt_dir = find_ponyc_crt_dir(program, c);
    if(ponyc_crt_dir == NULL)
    {
      errorf(errors, NULL,
        "could not find compiler-rt CRT objects (crtbeginS.o) in lib paths");
      return false;
    }
    compiler_crt_dir = ponyc_crt_dir;
  }

  // GCC lib dir is optional. On DragonFly the GCC dir is the same as
  // compiler_crt_dir (found above by find_dragonfly_gcc_lib_dir); the
  // generic find_gcc_lib_dir's patterns don't match either DragonFly
  // layout.
  const char* gcc_lib_dir;
  if(is_dragonfly)
  {
    gcc_lib_dir = compiler_crt_dir;
  }
  else
  {
    gcc_lib_dir = find_gcc_lib_dir(sysroot, arch_prefix.c_str(), c->opt->strtab);
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

#if defined(PONY_SANITIZER)
  // FreeBSD: the self-hosted tools link libponyc-standalone.a, which bundles
  // the base system libc++.a — and FreeBSD's libc++.a carries libcxxrt (the
  // C++ ABI) as members (cxxrt_exception.o etc.). The sanitizer fragment
  // spliced below whole-archives libclang_rt.asan, which provides its own
  // __cxa_throw / __cxa_rethrow_primary_exception interceptors; those collide
  // with libcxxrt's, which gets pulled in to satisfy the other C++ ABI symbols
  // (__cxa_begin_catch etc.) the bundled LLVM/lld code references. Allow the
  // duplicate so the asan interceptor (emitted first) wins. Which copy wins is
  // immaterial in practice: the vendored LLVM/lld are built with exceptions
  // disabled (LLVM_ENABLE_EH OFF), so nothing actually throws through
  // __cxa_throw here — the collision is a pure ABI-completeness artifact, not a
  // live unwinding path. Programs that don't bundle a C++ ABI statically — the
  // common case, including any normal Pony program linking only libponyrt —
  // have no duplicate and are unaffected.
  if(is_freebsd)
    args.push_back("--allow-multiple-definition");
#endif

  // OpenBSD's libc CRT defines the program entry point as `__start`
  // (double underscore). lld defaults to `_start` (single underscore) on
  // ELF, so without an explicit override the link fails with an
  // undefined-symbol error for `_start`. Set the entry point on OpenBSD
  // to match what crt0.o/rcrt0.o actually export. Confirmed by the base
  // `cc -v` link recipe on OpenBSD 7.8.
  if(is_openbsd)
  {
    args.push_back("-e");
    args.push_back("__start");
  }

  // Sysroot lets LLD resolve absolute paths in linker scripts (e.g.
  // libc.so referencing /lib/libpthread.so.0) by prepending the sysroot.
  // Our patched LLD falls back to the original path when the
  // sysroot-prefixed path doesn't exist, matching GNU ld behavior.
  snprintf(buf, sizeof(buf), "--sysroot=%s", sysroot);
  args.push_back(stringtab(c->opt->strtab, buf));

  if(c->opt->staticbin)
  {
    args.push_back("-static");
    args.push_back("--as-needed");
    // OpenBSD's static linkage is static-PIE: the startup is rcrt0.o and
    // the binary has a PT_DYNAMIC section for self-relocation. lld with
    // bare -static suppresses PT_DYNAMIC, which leaves the weak _DYNAMIC
    // reference in libpthread.a's rthread_fork.o unresolved (a hard
    // link-time error). Adding -pie alongside -static tells lld "produce
    // static-PIE", which keeps PT_DYNAMIC and lets lld synthesize the
    // _DYNAMIC symbol pointing at it (Writer.cpp:1828-1832).
    if(is_openbsd)
      args.push_back("-pie");
  }
  else
  {
    if(pie)
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
  if(is_openbsd)
  {
    // OpenBSD startup-file naming is unique among the supported targets:
    // dynamic PIE uses crt0.o and static uses rcrt0.o (static-PIE
    // startup; the static binary's DYN type comes from rcrt0.o, not from
    // passing -pie alongside -static). Neither matches Linux/FreeBSD/
    // DragonFly's Scrt1.o/crt1.o naming.
    const char* startup = c->opt->staticbin ? "rcrt0.o" : "crt0.o";
    snprintf(buf, sizeof(buf), "%s/%s", libc_crt_dir, startup);
    args.push_back(stringtab(c->opt->strtab, buf));
  }
  else if(pie)
  {
    // PIE startup: prefer Scrt1.o, fall back to crt1.o.
    snprintf(buf, sizeof(buf), "%s/Scrt1.o", libc_crt_dir);
    if(file_exists(buf))
      args.push_back(stringtab(c->opt->strtab, buf));
    else
    {
      snprintf(buf, sizeof(buf), "%s/crt1.o", libc_crt_dir);
      args.push_back(stringtab(c->opt->strtab, buf));
    }
  }
  else
  {
    // Static or non-PIE dynamic (FreeBSD/DragonFly): plain crt1.o startup.
    snprintf(buf, sizeof(buf), "%s/crt1.o", libc_crt_dir);
    args.push_back(stringtab(c->opt->strtab, buf));
  }

  // OpenBSD doesn't ship crti.o/crtn.o — its CRT model uses crt0.o (or
  // rcrt0.o) + crtbegin.o + crtend.o only. Skip the init wrapper here and
  // the matching finalization wrapper at the end of the link line.
  if(!is_openbsd)
  {
    snprintf(buf, sizeof(buf), "%s/crti.o", libc_crt_dir);
    args.push_back(stringtab(c->opt->strtab, buf));
  }

  if(is_openbsd)
  {
    // OpenBSD uses plain crtbegin.o for both static (rcrt0.o-based
    // static-PIE) and dynamic PIE — the S variant (crtbeginS.o) exists in
    // /usr/lib but base cc -v never selects it; the static-PIE path picks
    // up the same crtbegin.o as the dynamic-PIE path.
    snprintf(buf, sizeof(buf), "%s/crtbegin.o", compiler_crt_dir);
    args.push_back(stringtab(c->opt->strtab, buf));
  }
  else if(c->opt->staticbin && !is_dragonfly)
  {
    snprintf(buf, sizeof(buf), "%s/crtbeginT.o", compiler_crt_dir);
    args.push_back(stringtab(c->opt->strtab, buf));
  }
  else if(pie)
  {
    snprintf(buf, sizeof(buf), "%s/crtbeginS.o", compiler_crt_dir);
    args.push_back(stringtab(c->opt->strtab, buf));
  }
  else
  {
    // Non-PIE dynamic (FreeBSD), or static on DragonFly (no crtbeginT.o
    // exists; gcc's own -static link uses plain crtbegin.o here).
    snprintf(buf, sizeof(buf), "%s/crtbegin.o", compiler_crt_dir);
    args.push_back(stringtab(c->opt->strtab, buf));
  }

  // Library search paths: sysroot dirs first, then GCC, then ponyc/user.
  snprintf(buf, sizeof(buf), "-L%s", libc_crt_dir);
  args.push_back(stringtab(c->opt->strtab, buf));

  if(gcc_lib_dir != NULL)
  {
    snprintf(buf, sizeof(buf), "-L%s", gcc_lib_dir);
    args.push_back(stringtab(c->opt->strtab, buf));

    if(is_dragonfly)
    {
      // Pkg gcc layout: libgcc.a and crtbegin/end live at
      // /usr/local/lib/gccNN/gcc/<triple>/<ver>/, but the shared runtime
      // (libgcc_s.so, libatomic) lives 3 dirs up at /usr/local/lib/gccNN/.
      // Add the root as a -L and as -rpath so libgcc_s.so resolves at
      // runtime (gcc13 doesn't auto-rpath the way base cc does). The flat
      // base layout has no shared libgcc_s anyway — skip both.
      //
      // realpath() the derivation before stamping it into DT_RUNPATH so
      // the binary doesn't carry literal "../../.." segments (visible
      // under readelf -d, and brittle if the binary later moves between
      // sysroots).
      char gcc_root_buf[PATH_MAX];
      snprintf(gcc_root_buf, sizeof(gcc_root_buf), "%s/../../..",
        gcc_lib_dir);
      char gcc_root_canonical[PATH_MAX];
      const char* gcc_root_dir = realpath(gcc_root_buf, gcc_root_canonical)
        != NULL ? gcc_root_canonical : gcc_root_buf;
      char check[PATH_MAX];
      snprintf(check, sizeof(check), "%s/libgcc_s.so", gcc_root_dir);
      if(file_exists(check))
      {
        snprintf(buf, sizeof(buf), "-L%s", gcc_root_dir);
        args.push_back(stringtab(c->opt->strtab, buf));
        args.push_back("-rpath");
        args.push_back(stringtab(c->opt->strtab, gcc_root_dir));
      }
    }
    else
    {
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
        args.push_back(stringtab(c->opt->strtab, buf));
      }
    }
  }

  // Add ponyc lib paths and user lib paths.
  size_t path_count = program_lib_path_count(program);
  for(size_t i = 0; i < path_count; i++)
  {
    const char* path = program_lib_path_at(program, i);
    snprintf(buf, sizeof(buf), "-L%s", path);
    args.push_back(stringtab(c->opt->strtab, buf));

    if(!c->opt->staticbin)
    {
      args.push_back("-rpath");
      args.push_back(path);
    }
  }

  // Standard system library fallback paths. The paths above cover
  // distro-specific locations (libc_crt_dir, gcc_lib_dir, etc.) but miss
  // common install locations like /usr/local/lib (where libraries built
  // from source typically go) and /lib (separate from /usr/lib on systems
  // that haven't done the /usr merge). Without these, -l flags for
  // libraries in these locations fail. No -rpath needed since the runtime
  // linker already knows these paths.
  {
    // /usr/lib is intentionally absent. On non-multilib hosts
    // libc_crt_dir already covers it (directly, or via a multiarch
    // subdirectory of /usr/lib like /usr/lib/x86_64-linux-gnu). On
    // multilib RPM hosts (Fedora, RHEL) libc_crt_dir resolves to
    // /usr/lib64 because /usr/lib holds the 32-bit toolchain, and
    // adding /usr/lib here would inject wrong-arch libraries into
    // a 64-bit link.
    const char* fallback_dirs[] = {
      "%s/lib",
      "%s/usr/local/lib",
      "%s/lib64",
      "%s/usr/lib64",
      "%s/usr/local/lib64",
    };

    struct stat st;
    for(size_t i = 0;
      i < sizeof(fallback_dirs) / sizeof(fallback_dirs[0]); i++)
    {
      snprintf(buf, sizeof(buf), fallback_dirs[i], sysroot);
      if(stat(buf, &st) == 0 && S_ISDIR(st.st_mode))
      {
        char lbuf[PATH_MAX];
        snprintf(lbuf, sizeof(lbuf), "-L%s", buf);
        args.push_back(stringtab(c->opt->strtab, lbuf));
      }
    }
  }

#if defined(PONY_SANITIZER)
  // Sanitizer runtime. When ponyc is built with sanitizers, libponyrt's C code
  // is instrumented, so the executable references __asan_*/ubsan symbols and
  // must link the sanitizer runtime. PONY_SANITIZER_LINK_ARGS is the exact
  // fragment the compiler driver emits for -fsanitize=<PONY_SANITIZER>, captured
  // at ponyc build time (see the PONY_SANITIZERS_ENABLED block in the top-level
  // CMakeLists.txt). Its contents are compiler- and platform-specific:
  //   - Linux clang: whole-archived clang_rt static archives + --dynamic-list
  //     + a few syslibs.
  //   - Linux gcc: libasan_preinit.o + shared -lasan/-lubsan.
  //   - FreeBSD base clang: the asan_static + asan clang_rt archives, each
  //     whole-archived, then --export-dynamic and
  //     --no-as-needed -lpthread -lrt -lm -lexecinfo.
  // The clang_rt archive paths are absolute on the build machine.
  //
  // This function serves the Linux and FreeBSD branches of link_exe — the
  // native ELF sanitizer builds that reach embedded LLD. macOS native
  // sanitizer builds use the same captured fragment but splice it in
  // link_exe_lld_macho (see that function's sanitizer block). DragonFly and
  // OpenBSD native sanitizer builds are rejected at configure time, so they
  // never reach this splice; it is gated on the targets explicitly rather than
  // relying on link_exe's routing alone. The !is_cross_compiling gate
  // additionally keeps the host-arch-absolute fragment out of any cross link
  // routed here.
  //
  // The fragment is spliced as one contiguous block immediately before file_o.
  // That order is correct for every piece on both platforms: objects and static
  // archives that provide symbols (clang's whole-archived clang_rt, gcc's
  // preinit object) must precede the instrumented object/runtime so their
  // members are pulled in, and shared libraries (gcc's -lasan/-lubsan, FreeBSD
  // clang's -lrt/-lpthread/-lm/-lexecinfo) are position-insensitive (DT_NEEDED),
  // so emitting them here rather than after the object is harmless. Any
  // linker-state flags in the fragment (clang's --no-as-needed, FreeBSD clang's
  // --export-dynamic) either re-assert the ELF linker's default state or set a
  // whole-link property the sanitizer runtime needs, so their position in the
  // fragment is immaterial. Pieces ponyc also emits after the object (FreeBSD's
  // -lpthread/-lm/-lexecinfo) simply appear twice, harmlessly.
  //
  // Bare -l fragments (gcc's -lasan/-lubsan, FreeBSD's -lrt/-lpthread/-lm/
  // -lexecinfo — not absolute paths) resolve via the -L search paths ponyc
  // already emits earlier in this function: on Linux the gcc lib dir and the
  // standard system fallbacks, on FreeBSD the -L<libc_crt_dir> (/usr/lib) push.
  // Don't drop those without accounting for the sanitizer runtime.
  if((target_is_linux(c->opt->triple) || target_is_freebsd(c->opt->triple))
    && !is_cross_compiling(c->opt))
  {
    for(size_t i = 0; i < PONY_SANITIZER_LINK_ARGS_COUNT; i++)
      args.push_back(PONY_SANITIZER_LINK_ARGS[i]);
  }
#endif

#if defined(PONY_COVERAGE)
  // Coverage runtime (libgcov). When ponyc is built with gcc coverage
  // (-fprofile-arcs), libponyrt's C objects carry gcov constructors that
  // reference __gcov_init/__gcov_exit from gcc's libgcov, so every Pony program
  // ponyc links needs libgcov on the link line (issue #5434).
  // PONY_COVERAGE_LINK_ARGS is the fragment the gcc driver emits for
  // -fprofile-arcs — just -lgcov — captured at ponyc build time (see the
  // coverage capture block in the top-level CMakeLists.txt). It is defined only
  // in gcc coverage builds; clang coverage (-fprofile-instr-generate) links
  // fine without a splice, so this code is absent there.
  //
  // Gated to the native ELF targets where gcc coverage is allowed and reaches
  // embedded LLD: Linux, FreeBSD, and DragonFly. Unlike the sanitizer gate
  // above, DragonFly is INCLUDED — coverage is allowed there (sanitizers are
  // not), and DragonFly only builds ponyc with gcc, so it is the platform that
  // needs this most. OpenBSD coverage is rejected at configure time; macOS and
  // Windows are not gcc-coverage paths. !is_cross_compiling keeps the host
  // fragment out of any cross link routed here.
  //
  // -lgcov is a bare archive reference, resolved via the gcc lib -L path ponyc
  // already emits earlier in this function (the same dir libgcov.a and the gcc
  // sanitizer libs live in). Position is immaterial: LLD resolves __gcov_init
  // from libgcov.a whether -lgcov precedes or follows the instrumented libponyrt
  // archive, so splicing here (before file_o, alongside the sanitizer fragment)
  // is fine.
  if((target_is_linux(c->opt->triple) || target_is_freebsd(c->opt->triple)
    || target_is_dragonfly(c->opt->triple)) && !is_cross_compiling(c->opt))
  {
    for(size_t i = 0; i < PONY_COVERAGE_LINK_ARGS_COUNT; i++)
      args.push_back(PONY_COVERAGE_LINK_ARGS[i]);
  }
#endif

  // Object file.
  args.push_back(file_o);

  // C shim objects (gencshim), in deterministic package-walk order. Objects are
  // always fully included, so they sit after the Pony object and before the
  // user libraries that may satisfy their references.
  size_t c_object_count = program_c_object_count(program);
  for(size_t i = 0; i < c_object_count; i++)
    args.push_back(program_c_object_at(program, i));

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
      args.push_back(stringtab(c->opt->strtab, buf));
    }
  }

#if defined(USE_DYNAMIC_TRACE)
  // FreeBSD use=dtrace builds: pull in the DOF object and its probe-
  // registration constructor. `dtrace -G` on FreeBSD rewrites the probe
  // sites inside libponyrt's objects and emits the DOF plus a constructor
  // into a separate libdtrace_probes.a (see src/libponyrt/CMakeLists.txt);
  // the constructor is otherwise unreferenced, so --whole-archive is what
  // keeps it in the link. libdtrace_probes.a sits in the same lib dir as
  // libponyrt.a, so it resolves from the -L paths already pushed above; the
  // DOF object needs libelf, found on the system -L paths. These come before
  // -lponyrt (pushed below). Gated on is_freebsd (target), not the build
  // platform: DragonFly/OpenBSD have no dtrace_probes/libelf and never route
  // here for dtrace builds.
  if(is_freebsd)
  {
    args.push_back("--whole-archive");
    args.push_back("-ldtrace_probes");
    args.push_back("--no-whole-archive");
    args.push_back("-lelf");
  }
#endif

  // Pony runtime.
  if(!c->opt->runtimebc)
  {
    // FreeBSD and DragonFly ship a single libponyrt.a (PIC-enabled) and no
    // compiler-rt CRT, so ponyc_crt_dir is NULL on those targets; resolve
    // -lponyrt via the -L lib paths, with no -pic variant (matching the
    // external BSD path).
    if(ponyc_crt_dir == NULL)
    {
      args.push_back("-lponyrt");
    }
    else
    {
      // Use the full path from ponyc_crt_dir to pick the target-architecture
      // library, not a native-arch copy that might appear earlier in -L.
      const char* rt_name = c->opt->staticbin ? "libponyrt.a"
        : (c->opt->pic ? "libponyrt-pic.a" : "libponyrt.a");

      snprintf(buf, sizeof(buf), "%s/%s", ponyc_crt_dir, rt_name);
      if(file_exists(buf))
      {
        args.push_back(stringtab(c->opt->strtab, buf));
      }
      else
      {
        if(c->opt->staticbin)
          args.push_back("-lponyrt");
        else
          args.push_back(c->opt->pic ? "-lponyrt-pic" : "-lponyrt");
      }
    }
  }

  args.push_back("-lpthread");
  args.push_back("-lm");
  if(is_freebsd)
  {
    // dlopen is in libc and there's no libatomic on FreeBSD. backtrace() is
    // in libexecinfo; the static libexecinfo.a additionally needs libelf.
    args.push_back("-lexecinfo");
    if(c->opt->staticbin)
      args.push_back("-lelf");
  }
  else if(is_dragonfly)
  {
    // dlopen is in libc. backtrace() is in libexecinfo. libatomic isn't
    // shipped in base — it comes from pkg gcc (libgcc_s's adjacent
    // libatomic) or cxx_atomics. The pkg-gcc nested layout puts libatomic
    // at the gcc-root dir added above, so -latomic resolves there. The
    // base-gcc flat fallback has no libatomic; the link will fail
    // alongside the missing -lgcc_s — the user has already been warned
    // that the flat fallback isn't a complete toolchain.
    // DragonFly's libexecinfo has no libelf dependency, so -lelf isn't
    // needed even for static links (and no libelf is installed anyway).
    args.push_back("-lexecinfo");
    args.push_back("-latomic");
  }
  else if(is_openbsd)
  {
    // dlopen is in libc and there's no libatomic in base (matching FreeBSD).
    // No -ldl, no -latomic, and no -lexecinfo HERE — backtrace() is in
    // libexecinfo, but libexecinfo.a's new_handler.o defines
    // std::{get,set}_new_handler as strong T symbols, and libc++abi.a's
    // cxa_*_handlers.o do too. If -lexecinfo precedes -lc++abi in the
    // command line, both .o files get pulled in and lld rejects the
    // duplicate. We avoid this by emitting -lexecinfo inside the runtime
    // block below, after -lc++abi. No -lelf even for static links —
    // libexecinfo.a has no libelf dependency (libelf.a does exist on
    // OpenBSD 7.8, but nothing here uses it).
  }
  else
  {
    args.push_back("-ldl");
    args.push_back("-latomic");
  }

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
  if(is_openbsd)
  {
    // OpenBSD's base toolchain is clang + compiler-rt, not gcc. _Unwind_*
    // symbols (used by Pony's backtrace path through libexecinfo) come
    // from libcompiler_rt, not libgcc/libgcc_s/libunwind — libunwind has
    // no standalone library on OpenBSD; its objects are bundled into
    // libcompiler_rt and libexecinfo.a. Base cc -v uses a
    // `-lcompiler_rt -lc -lcompiler_rt` sandwich so libc and compiler_rt
    // can cross-reference; we wrap it in --start-group/--end-group to
    // make ordering between libc and compiler_rt not load-bearing.
    //
    // Library order inside the group is load-bearing in a way -lpthread/
    // -lm aren't:
    //   -lc++abi    — operator new/delete (weak in stdlib_new_delete.o)
    //                 plus std::{get,set}_new_handler (strong in
    //                 cxa_*_handlers.o). libponyc-standalone aggregates
    //                 LLVM/libc++ object code that requires operator new.
    //   -lexecinfo  — backtrace(); must come AFTER -lc++abi because
    //                 libexecinfo.a's new_handler.o defines
    //                 std::{get,set}_new_handler as strong T symbols too,
    //                 and lld rejects the duplicate if both .o files end
    //                 up in the link. With -lc++abi first, the new_handler
    //                 symbols resolve from libc++abi and libexecinfo's
    //                 new_handler.o is never pulled in.
    //   -lcompiler_rt -lc -lcompiler_rt — the base-cc sandwich for libc
    //                 and the unwinder/builtins.
    args.push_back("--start-group");
    args.push_back("-lc++abi");
    args.push_back("-lexecinfo");
    args.push_back("-lcompiler_rt");
    args.push_back("-lc");
    args.push_back("-lcompiler_rt");
    args.push_back("--end-group");
  }
  else if(c->opt->staticbin)
  {
    args.push_back("--start-group");
    args.push_back("-lgcc");
    args.push_back("-lgcc_eh");
    args.push_back("-lc");
    args.push_back("--end-group");
    // libssp_nonshared is a glibc artifact; the BSDs have no such library.
    if(!target_is_bsd(c->opt->triple))
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
  if(is_openbsd)
  {
    // OpenBSD uses plain crtend.o for both PIE and static (matching the
    // crtbegin.o choice above; base cc -v never selects crtendS.o on
    // OpenBSD even though the S variant exists in /usr/lib).
    snprintf(buf, sizeof(buf), "%s/crtend.o", compiler_crt_dir);
    args.push_back(stringtab(c->opt->strtab, buf));
  }
  else if(pie)
  {
    snprintf(buf, sizeof(buf), "%s/crtendS.o", compiler_crt_dir);
    args.push_back(stringtab(c->opt->strtab, buf));
  }
  else
  {
    // Static or non-PIE dynamic (FreeBSD/DragonFly): plain crtend.o.
    snprintf(buf, sizeof(buf), "%s/crtend.o", compiler_crt_dir);
    args.push_back(stringtab(c->opt->strtab, buf));
  }

  // OpenBSD doesn't ship crtn.o — see the crti.o skip earlier.
  if(!is_openbsd)
  {
    snprintf(buf, sizeof(buf), "%s/crtn.o", libc_crt_dir);
    args.push_back(stringtab(c->opt->strtab, buf));
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

  return true;
}

// gencshim.cc's find_macos_sdk_include resolves the SDK's include directory the
// same way this resolves its lib directory. Keep the two in sync.
static const char* find_macos_sdk_path(strtable_t* strtab)
{
  // Cache the discovered path as a raw string (not interned) so the expensive
  // xcrun probe runs only once per process, but intern into the caller's table
  // on every call. Each compilation has its own interned-string table, so
  // caching an interned pointer would dangle once the first compilation's table
  // is freed.
  static char cached_path[PATH_MAX];
  static bool searched = false;
  static bool found = false;

  if(searched)
    return found ? stringtab(strtab, cached_path) : NULL;
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
        strncpy(cached_path, lib_path, sizeof(cached_path) - 1);
        cached_path[sizeof(cached_path) - 1] = '\0';
        found = true;
        pclose(f);
        return stringtab(strtab, cached_path);
      }
    }
    pclose(f);
  }

  // Hardcoded fallback.
  const char* fallback =
    "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib";

  struct stat st;
  if(stat(fallback, &st) == 0 && S_ISDIR(st.st_mode))
  {
    strncpy(cached_path, fallback, sizeof(cached_path) - 1);
    cached_path[sizeof(cached_path) - 1] = '\0';
    found = true;
    return stringtab(strtab, cached_path);
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
  return stringtab(c->opt->strtab, buf);
}

// Run a command and wait for it to finish. The executable is looked up on
// PATH (like the shell did) and the arguments are passed as an explicit argv
// terminated by a NULL element, so no shell is involved and nothing in the
// argument list — including the user-controlled output path — is subject to
// shell parsing. Returns the command's exit status, or -1 if the process
// could not be spawned or did not exit normally.
static int run_command(const char* const argv[])
{
  extern char** environ;

  pid_t pid;
  int err = posix_spawnp(&pid, argv[0], NULL, NULL,
    (char* const*)argv, environ);
  if(err != 0)
    return -1;

  // Retry on EINTR so a signal delivered while we wait doesn't masquerade as
  // a tool failure. system(), which this replaces, did the same internally.
  int status;
  int wait_result;
  do
  {
    wait_result = waitpid(pid, &status, 0);
  } while((wait_result == -1) && (errno == EINTR));

  if(wait_result == -1)
    return -1;

  if(!WIFEXITED(status))
    return -1;

  return WEXITSTATUS(status);
}

// Recursively delete a directory tree using direct filesystem calls — no
// shell, no external `rm`, so the path is only ever a syscall argument and
// can carry no injection. Symlinks are removed as links, never followed
// (lstat reports a symlink as a non-directory, so it is unlinked rather than
// descended into). Best effort: errors are ignored, matching the `rm -rf`
// this replaces — a missing bundle is not an error, and dsymutil overwrites
// whatever remains.
static void remove_directory_tree(const char* path)
{
  DIR* dir = opendir(path);
  if(dir == NULL)
  {
    // Not an openable directory: either the path is absent (nothing to do)
    // or a plain file sits where the bundle should be. unlink removes the
    // file case and harmlessly no-ops when the path is absent.
    unlink(path);
    return;
  }

  // Each entry is removed as readdir returns it. Deleting the just-returned
  // entry and continuing to iterate is the standard recursive-delete idiom
  // (glibc, macOS, and the BSDs all tolerate it); don't "fix" this into a
  // collect-then-delete pass.
  struct dirent* entry;
  while((entry = readdir(dir)) != NULL)
  {
    if((strcmp(entry->d_name, ".") == 0) ||
      (strcmp(entry->d_name, "..") == 0))
      continue;

    char child[PATH_MAX];
    int n = snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);

    // Skip rather than act on a truncated path: a shortened path could name a
    // different filesystem object, and this code deletes.
    if((n < 0) || ((size_t)n >= sizeof(child)))
      continue;

    struct stat st;
    if((lstat(child, &st) == 0) && S_ISDIR(st.st_mode))
      remove_directory_tree(child);
    else
      unlink(child);
  }

  closedir(dir);
  rmdir(path);
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

  const char* sdk_lib_path = find_macos_sdk_path(c->opt->strtab);
  if(sdk_lib_path == NULL)
  {
    errorf(errors, NULL,
      "could not find macOS SDK library path\n"
      "  Install Xcode or CommandLineTools");
    return false;
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
  args.push_back(stringtab(c->opt->strtab, buf));

  // Ponyc lib paths and user lib paths.
  size_t path_count = program_lib_path_count(program);
  for(size_t i = 0; i < path_count; i++)
  {
    const char* path = program_lib_path_at(program, i);
    snprintf(buf, sizeof(buf), "-L%s", path);
    args.push_back(stringtab(c->opt->strtab, buf));
  }

  // Standard library fallback paths for Homebrew and other system
  // locations. The SDK path above only covers the Xcode/CLT SDK; libraries
  // installed via Homebrew or from source end up in these locations.
  {
    const char* fallback_dirs[] = {
      "/opt/homebrew/lib",
      "/usr/local/lib",
    };

    struct stat st;
    for(size_t i = 0;
      i < sizeof(fallback_dirs) / sizeof(fallback_dirs[0]); i++)
    {
      if(stat(fallback_dirs[i], &st) == 0 && S_ISDIR(st.st_mode))
      {
        snprintf(buf, sizeof(buf), "-L%s", fallback_dirs[i]);
        args.push_back(stringtab(c->opt->strtab, buf));
      }
    }
  }

  // Object file.
  args.push_back(file_o);

  // C shim objects (gencshim), in deterministic package-walk order. Objects are
  // always fully included, so they sit after the Pony object and before the
  // user libraries that may satisfy their references.
  size_t c_object_count = program_c_object_count(program);
  for(size_t i = 0; i < c_object_count; i++)
    args.push_back(program_c_object_at(program, i));

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
      args.push_back(stringtab(c->opt->strtab, buf));
    }
  }

#if defined(PONY_SANITIZER)
  // Sanitizer runtime. The fragment captured at configure time
  // (PONY_SANITIZER_LINK_ARGS) contains the Apple clang driver's sanitizer
  // additions: an absolute path to the clang_rt dylib plus -rpath entries.
  // Apple clang folds UBSan into ASan when both are enabled, so
  // -fsanitize=address,undefined produces the same fragment as address alone.
  // The fragment uses Mach-O linker flags compatible with ld64.lld.
  // Only splice for native builds — the host-arch-absolute paths are wrong
  // for cross links.
  if(target_is_macosx(c->opt->triple) && !is_cross_compiling(c->opt))
  {
    for(size_t i = 0; i < PONY_SANITIZER_LINK_ARGS_COUNT; i++)
      args.push_back(PONY_SANITIZER_LINK_ARGS[i]);
  }
#endif

  // System library and Pony runtime.
  args.push_back("-lSystem");

  if(!c->opt->runtimebc)
  {
    // macOS has no separate PIC runtime library — all code is PIC by default.
    args.push_back("-lponyrt");
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
    // Remove any stale dSYM bundle, then regenerate it. The bundle is removed
    // in-process with direct filesystem calls (no shell, no external rm), and
    // dsymutil is invoked via run_command with an explicit argv rather than
    // system(). Either way the user-controlled output path is only ever a
    // syscall or single-argument value, never interpreted by a shell.
    size_t dsym_path_len = strlen(file_exe) + 6; // ".dSYM" + NUL
    char* dsym_path = (char*)ponyint_pool_alloc_size(dsym_path_len);
    snprintf(dsym_path, dsym_path_len, "%s.dSYM", file_exe);

    remove_directory_tree(dsym_path);

    const char* dsymutil_argv[] = {"dsymutil", file_exe, NULL};
    if(run_command(dsymutil_argv) != 0)
      errorf(errors, NULL, "unable to create dsym");

    ponyint_pool_free_size(dsym_path_len, dsym_path);
  }

  return true;
}
#endif

#ifdef PLATFORM_IS_WINDOWS
static bool link_exe_lld_coff(compile_t* c, ast_t* program,
  const char* file_o)
{
  errors_t* errors = c->opt->check.errors;

  vcvars_t vcvars;

  if(!vcvars_get(c->opt, &vcvars, errors))
  {
    errorf(errors, NULL, "unable to link: no vcvars");
    return false;
  }

  const char* file_exe = suffix_filename(c, c->opt->output, "", c->filename,
    ".exe");

  if(c->opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Linking %s\n", file_exe);

  program_lib_build_args_embedded(program, c->opt);

#ifdef _M_ARM64
  const char* arch = "ARM64";
#elif defined(_M_X64)
  const char* arch = "x64";
#else
  const char* arch = "";
#endif

  // Build argument vector.
  std::vector<const char*> args;
  char buf[MAX_PATH + 32];

  args.push_back("lld-link");
  args.push_back("/DEBUG");
  args.push_back("/NOLOGO");

  snprintf(buf, sizeof(buf), "/MACHINE:%s", arch);
  args.push_back(stringtab(c->opt->strtab, buf));

  args.push_back("/ignore:4099");

  snprintf(buf, sizeof(buf), "/OUT:%s", file_exe);
  args.push_back(stringtab(c->opt->strtab, buf));

  // Object file.
  args.push_back(file_o);

  // C shim objects (gencshim), in deterministic package-walk order. These are
  // absolute object paths, so they don't depend on the /LIBPATH entries
  // pushed below; only library-name resolution does.
  size_t c_object_count = program_c_object_count(program);
  for(size_t i = 0; i < c_object_count; i++)
    args.push_back(program_c_object_at(program, i));

  // UCRT library path (Windows 10+ SDK).
  if(strlen(vcvars.ucrt) > 0)
  {
    snprintf(buf, sizeof(buf), "/LIBPATH:%s", vcvars.ucrt);
    args.push_back(stringtab(c->opt->strtab, buf));
  }

  // Windows SDK kernel32 path.
  snprintf(buf, sizeof(buf), "/LIBPATH:%s", vcvars.kernel32);
  args.push_back(stringtab(c->opt->strtab, buf));

  // MSVC runtime lib path.
  snprintf(buf, sizeof(buf), "/LIBPATH:%s", vcvars.msvcrt);
  args.push_back(stringtab(c->opt->strtab, buf));

  // User library search paths.
  size_t path_count = program_lib_path_count(program);
  for(size_t i = 0; i < path_count; i++)
  {
    const char* path = program_lib_path_at(program, i);
    snprintf(buf, sizeof(buf), "/LIBPATH:%s", path);
    args.push_back(stringtab(c->opt->strtab, buf));
  }

  // User libraries (append .lib suffix for non-absolute paths).
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
      snprintf(buf, sizeof(buf), "%s.lib", lib);
      args.push_back(stringtab(c->opt->strtab, buf));
    }
  }

  // Default Windows system libraries.
  // vcvars.default_libs is a space-separated string; tokenize into
  // individual arguments for embedded LLD.
  char default_libs_copy[MAX_PATH];
  strncpy(default_libs_copy, vcvars.default_libs, MAX_PATH - 1);
  default_libs_copy[MAX_PATH - 1] = '\0';

  char* tok = strtok(default_libs_copy, " ");
  while(tok != NULL)
  {
    args.push_back(stringtab(c->opt->strtab, tok));
    tok = strtok(NULL, " ");
  }

  // Pony runtime.
  if(!c->opt->runtimebc)
  {
    args.push_back("libponyrt.lib");
  }

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
#endif

static bool link_exe(compile_t* c, ast_t* program,
  const char* file_o)
{
  // Link with embedded LLD, selecting the driver by target triple. On Linux,
  // FreeBSD, and macOS, native sanitizer builds use embedded LLD too: the ELF
  // and Mach-O link functions splice the sanitizer runtime from a fragment
  // captured at ponyc build time. DragonFly and OpenBSD native sanitizer
  // builds are rejected at configure time. Native gcc coverage builds
  // (Linux/FreeBSD/DragonFly) likewise splice the libgcov fragment in
  // link_exe_lld_elf (see its PONY_COVERAGE block). Cross-compilation always
  // uses LLD.
#ifdef PLATFORM_IS_POSIX_BASED
  if(target_is_linux(c->opt->triple))
    return link_exe_lld_elf(c, program, file_o);

  if(target_is_macosx(c->opt->triple))
    return link_exe_lld_macho(c, program, file_o);

  // FreeBSD always links through embedded LLD, including use=dtrace builds:
  // link_exe_lld_elf adds the -ldtrace_probes/-lelf linking dtrace needs.
  // DragonFly and OpenBSD route here only when ponyc was NOT built with
  // dtrace — they have no dtrace_probes/libelf, so their use=dtrace builds
  // fall through to the error below (a configure-time rejection is tracked in
  // #5447).
  //
  // In a sanitizer build, native FreeBSD also routes here: link_exe_lld_elf
  // splices in the captured sanitizer fragment, verified against FreeBSD base
  // clang. Native DragonFly and OpenBSD sanitizer builds are rejected at
  // configure time, so the sanitizer sub-gate admits only native FreeBSD (and
  // any cross link, whose runtime is host-arch-absolute and handled by the
  // !is_cross_compiling gate at the splice).
  bool bsd_embed = target_is_freebsd(c->opt->triple)
#if !defined(USE_DYNAMIC_TRACE)
    || target_is_dragonfly(c->opt->triple)
    || target_is_openbsd(c->opt->triple)
#endif
    ;
  if(bsd_embed
#if defined(PONY_SANITIZER)
    && (is_cross_compiling(c->opt) || target_is_freebsd(c->opt->triple))
#endif
    )
  {
    return link_exe_lld_elf(c, program, file_o);
  }

  errorf(c->opt->check.errors, NULL,
    "ponyc has no linker support for target %s", c->opt->triple);
  return false;
#elif defined(PLATFORM_IS_WINDOWS)
  return link_exe_lld_coff(c, program, file_o);
#else
  errorf(c->opt->check.errors, NULL,
    "ponyc has no linker support for target %s", c->opt->triple);
  return false;
#endif
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
  reach(c->reach, main_ast, stringtab(c->opt->strtab, "runtime_override_defaults"), NULL, c->opt);
  reach(c->reach, env_ast, c->str__create, NULL, c->opt);

  // reach() can't signal failure through its void return. If it aborted on an
  // over-large generic instantiation it left stub types behind and already
  // reported the error, so fail now — before painting/codegen, which would
  // touch those stubs.
  if(reach_limit_exceeded(c->reach))
  {
    ast_free(main_ast);
    ast_free(env_ast);
    return false;
  }

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

  reach_type_t* t_main = reach_type(c->reach, main_ast, c->opt);
  reach_type_t* t_env = reach_type(c->reach, env_ast, c->opt);

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

  // Shim objects share the Pony object's lifetime: removed only here, after
  // a successful PASS_ALL link. Under --pass c/obj/asm/ir they persist
  // (handing objects to another linker is the point of those modes), and a
  // failed link leaves them too.
  size_t c_object_count = program_c_object_count(program);

#ifdef PLATFORM_IS_WINDOWS
  _unlink(file_o);

  for(size_t i = 0; i < c_object_count; i++)
    _unlink(program_c_object_at(program, i));
#else
  unlink(file_o);

  for(size_t i = 0; i < c_object_count; i++)
    unlink(program_c_object_at(program, i));
#endif

  return true;
}
