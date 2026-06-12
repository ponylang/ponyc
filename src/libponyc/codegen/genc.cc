#include "genc.h"
#include "genopt.h"
#include "../pkg/package.h"
#include "../pkg/program.h"
#include "../ast/error.h"
#include "../ast/stringtab.h"
#include "paths.h"
#include "ponyassert.h"

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable:4003)
#  pragma warning(disable:4244)
#  pragma warning(disable:4267)
#  pragma warning(disable:4624)
#  pragma warning(disable:4141)
#  pragma warning(disable:4146)
#endif

#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/Version.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/CrashRecoveryContext.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Triple.h>

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <sys/stat.h>

#ifdef PLATFORM_IS_WINDOWS
#  define PATH_SLASH '\\'
#else
#  define PATH_SLASH '/'
#endif


static bool dir_exists(const char* path)
{
  struct stat st;
  return (stat(path, &st) == 0) && S_ISDIR(st.st_mode);
}


// Routes clang diagnostics into ponyc's error collection. Errors and fatals
// become ponyc errors (with the C file and line:column in the message);
// warnings go to stderr so they're visible without failing the build; notes
// follow whichever diagnostic they elaborate on, mirroring clang's own
// presentation.
class PonyDiagConsumer : public clang::DiagnosticConsumer
{
public:
  explicit PonyDiagConsumer(errors_t* errors)
    : errors_(errors), prev_(SINK_NONE)
  {}

  void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
    const clang::Diagnostic& info) override
  {
    // Keep the base class' error/warning counts current.
    clang::DiagnosticConsumer::HandleDiagnostic(level, info);

    llvm::SmallString<256> text;
    info.FormatDiagnostic(text);

    const char* file = NULL;
    unsigned line = 0;
    unsigned col = 0;

    if(info.hasSourceManager() && info.getLocation().isValid())
    {
      const clang::SourceManager& sm = info.getSourceManager();
      clang::PresumedLoc ploc = sm.getPresumedLoc(info.getLocation());

      if(ploc.isValid())
      {
        file = ploc.getFilename();
        line = ploc.getLine();
        col = ploc.getColumn();
      }
    }

    switch(level)
    {
      case clang::DiagnosticsEngine::Error:
      case clang::DiagnosticsEngine::Fatal:
        // errorf_at carries the location in errormsg_t's fields, so shim
        // errors print as "file:line:pos: msg" exactly like native ponyc
        // errors and stay machine-parseable (editors, CI annotators).
        if(file != NULL)
          errorf_at(errors_, file, line, col, "%s", text.c_str());
        else
          errorf(errors_, NULL, "%s", text.c_str());

        prev_ = SINK_ERROR;
        break;

      case clang::DiagnosticsEngine::Note:
        // Notes elaborate on the diagnostic immediately before them, so
        // they follow it to its sink: an error's notes join its error
        // frame, anything else's (a warning's, or the rare note with no
        // preceding diagnostic) go to stderr. prev_ deliberately stays
        // unchanged here: a chain of notes after one diagnostic all belong
        // to it.
        if(prev_ == SINK_ERROR)
        {
          if(file != NULL)
            errorf_at_continue(errors_, file, line, col, "%s",
              text.c_str());
          else
            errorf_continue(errors_, NULL, "%s", text.c_str());
        }
        else
        {
          if(file != NULL)
            fprintf(stderr, "%s:%u:%u: note: %s\n", file, line, col,
              text.c_str());
          else
            fprintf(stderr, "note: %s\n", text.c_str());
        }
        break;

      default:
        if(file != NULL)
          fprintf(stderr, "%s:%u:%u: warning: %s\n", file, line, col,
            text.c_str());
        else
          fprintf(stderr, "warning: %s\n", text.c_str());

        prev_ = SINK_WARNING;
        break;
    }
  }

private:
  enum sink_t
  {
    SINK_NONE,
    SINK_ERROR,
    SINK_WARNING
  };

  errors_t* errors_;
  sink_t prev_;
};


// Mirrors is_cross_compiling in genexe.cc (which takes compile_t; genc runs
// before a compile_t exists). Keep the two in sync.
static bool c_is_cross_compiling(pass_opt_t* opt)
{
  char* default_triple_str = LLVMGetDefaultTargetTriple();
  llvm::Triple target(opt->triple);
  llvm::Triple host(default_triple_str);
  LLVMDisposeMessage(default_triple_str);

  if(target.getArch() != host.getArch())
    return true;

  if(target.isMacOSX() && host.isMacOSX())
    return false;

  return target.getOS() != host.getOS()
    || target.getEnvironment() != host.getEnvironment();
}


// macOS-to-macOS builds resolve headers through the SDK, which serves
// every arch, so an arm64 mac building x86_64-apple-macosx (arch-cross by
// c_is_cross_compiling) needs no sysroot — the same way the MachO link
// path never consults resolve_sysroot.
static bool c_is_mac_on_mac(pass_opt_t* opt)
{
  char* default_triple_str = LLVMGetDefaultTargetTriple();
  llvm::Triple target(opt->triple);
  llvm::Triple host(default_triple_str);
  LLVMDisposeMessage(default_triple_str);

  return target.isMacOSX() && host.isMacOSX();
}


// Mirrors host_is_musl in genexe.cc (which takes compile_t). Keep the two
// in sync: the vendored LLVM's default triple may claim "gnu" on musl
// systems, so native compilation probes for the musl loader instead of
// trusting the triple.
static bool c_host_is_musl(pass_opt_t* opt)
{
  if(c_is_cross_compiling(opt))
    return false;

  llvm::Triple triple(opt->triple);
  const char* musl_linker = NULL;

  switch(triple.getArch())
  {
    case llvm::Triple::x86_64:
      musl_linker = "/lib/ld-musl-x86_64.so.1";
      break;

    case llvm::Triple::aarch64:
      musl_linker = "/lib/ld-musl-aarch64.so.1";
      break;

    case llvm::Triple::riscv64:
      musl_linker = "/lib/ld-musl-riscv64.so.1";
      break;

    case llvm::Triple::arm:
    case llvm::Triple::thumb:
    {
      struct stat st;
      return (stat("/lib/ld-musl-armhf.so.1", &st) == 0)
        || (stat("/lib/ld-musl-arm.so.1", &st) == 0);
    }

    default:
      return false;
  }

  struct stat st;
  return stat(musl_linker, &st) == 0;
}


// The arch-os-environment form distros use for multiarch directories
// (e.g. x86_64-linux-gnu). Mirrors system_triple in genexe.cc.
static const char* c_system_triple(pass_opt_t* opt)
{
  llvm::Triple triple(opt->triple);
  std::string result = std::string(triple.getArchName()) + "-"
    + std::string(triple.getOSName()) + "-"
    + std::string(triple.getEnvironmentName());
  return stringtab(opt->strtab, result.c_str());
}


// Locate clang's resource directory (builtin headers such as stddef.h).
// It ships at a known offset from the ponyc binary: ../lib/clang/<major>
// in an installation (see the Makefile install target), or
// ../libs/lib/clang/<major> when running from a build tree.
static const char* clang_resource_dir(pass_opt_t* opt, errors_t* errors)
{
  char exec_dir[FILENAME_MAX];

  if(!get_compiler_exe_directory(exec_dir, opt->argv0))
  {
    errorf(errors, NULL, "unable to determine ponyc's own path while "
      "looking for clang's resource directory");
    return NULL;
  }

  char buf[FILENAME_MAX];

  // Installed layout: <ponydir>/bin/ponyc + <ponydir>/lib/clang/<major>.
  snprintf(buf, sizeof(buf), "%s..%clib%cclang%c%d", exec_dir,
    PATH_SLASH, PATH_SLASH, PATH_SLASH, CLANG_VERSION_MAJOR);

  if(dir_exists(buf))
    return stringtab(opt->strtab, buf);

  // Build tree: build/<config>/ponyc + build/libs/lib/clang/<major>.
  snprintf(buf, sizeof(buf), "%s..%clibs%clib%cclang%c%d", exec_dir,
    PATH_SLASH, PATH_SLASH, PATH_SLASH, PATH_SLASH, CLANG_VERSION_MAJOR);

  if(dir_exists(buf))
    return stringtab(opt->strtab, buf);

  snprintf(buf, sizeof(buf), "%s..%clibs%clib64%cclang%c%d", exec_dir,
    PATH_SLASH, PATH_SLASH, PATH_SLASH, PATH_SLASH, CLANG_VERSION_MAJOR);

  if(dir_exists(buf))
    return stringtab(opt->strtab, buf);

  errorf(errors, NULL, "unable to find clang's resource directory "
    "(lib/clang/%d) next to the ponyc installation; C shims need its "
    "builtin headers", CLANG_VERSION_MAJOR);
  return NULL;
}


// The sysroot the shim compile resolves system headers against. Shares the
// meaning of the link-side sysroot (genexe.cc resolve_sysroot): --sysroot
// when given, the host root for native builds. The link side additionally
// probes cross-toolchain locations for a usable libc; replicating that here
// is deferred until cross-compiled shims have a real user, so a cross build
// without --sysroot is an error.
static const char* c_shim_sysroot(pass_opt_t* opt, errors_t* errors)
{
  if(opt->sysroot != NULL && opt->sysroot[0] != '\0')
    return stringtab(opt->strtab, opt->sysroot);

  if(!c_is_cross_compiling(opt) || c_is_mac_on_mac(opt))
    return "";

  errorf(errors, NULL,
    "cross-compiling C shims for %s requires --sysroot=<path>", opt->triple);
  return NULL;
}


#ifndef PLATFORM_IS_WINDOWS
// Locate the macOS SDK include directory via xcrun, falling back to the
// CommandLineTools default. Mirrors find_macos_sdk_path in genexe.cc, which
// resolves the SDK's lib directory the same way; keep the two in sync.
static const char* find_macos_sdk_include(pass_opt_t* opt)
{
  static char cached_path[FILENAME_MAX];
  static bool searched = false;
  static bool found = false;

  if(searched)
    return found ? stringtab(opt->strtab, cached_path) : NULL;
  searched = true;

  char sdk_path[FILENAME_MAX];

  FILE* f = popen("xcrun --show-sdk-path 2>/dev/null", "r");
  if(f != NULL)
  {
    if(fgets(sdk_path, sizeof(sdk_path), f) != NULL)
    {
      size_t len = strlen(sdk_path);
      if(len > 0 && sdk_path[len - 1] == '\n')
        sdk_path[len - 1] = '\0';

      char include_path[FILENAME_MAX];
      snprintf(include_path, sizeof(include_path), "%s/usr/include",
        sdk_path);

      if(dir_exists(include_path))
      {
        strncpy(cached_path, include_path, sizeof(cached_path) - 1);
        cached_path[sizeof(cached_path) - 1] = '\0';
        found = true;
        pclose(f);
        return stringtab(opt->strtab, cached_path);
      }
    }
    pclose(f);
  }

  const char* fallback =
    "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include";

  if(dir_exists(fallback))
  {
    strncpy(cached_path, fallback, sizeof(cached_path) - 1);
    cached_path[sizeof(cached_path) - 1] = '\0';
    found = true;
    return stringtab(opt->strtab, cached_path);
  }

  return NULL;
}
#endif


// Returns true when the directory exists and was pushed.
static bool push_isystem(pass_opt_t* opt, std::vector<const char*>& args,
  errors_t* errors, const char* flag, const char* sysroot,
  const char* suffix)
{
  char buf[FILENAME_MAX];
  int written = snprintf(buf, sizeof(buf), "%s%s", sysroot, suffix);

  if((written < 0) || ((size_t)written >= sizeof(buf)))
  {
    // Without this, a too-long sysroot would silently skip the directory
    // and surface later as a confusing header-not-found in the shim.
    errorf(errors, NULL, "system include path '%s%s' is too long", sysroot,
      suffix);
    return false;
  }

  if(!dir_exists(buf))
    return false;

  args.push_back(flag);
  args.push_back(stringtab(opt->strtab, buf));
  return true;
}


// System include directories for the target, derived from the same sysroot
// the embedded linker uses — hand-rolled like LLD rather than driven through
// clang's Driver, so the headers the shim compiles against stay consistent
// with the libc the link resolves against (see #5468, decision 1). The
// order mirrors what clang's driver emits for these targets: /usr/local,
// the multiarch directory, then the libc include root. Directories that
// don't exist are skipped.
static bool add_system_include_args(pass_opt_t* opt, const char* sysroot,
  std::vector<const char*>& args, errors_t* errors)
{
  if(target_is_macosx(opt->triple))
  {
#ifdef PLATFORM_IS_WINDOWS
    // The SDK discovery below is POSIX (popen/xcrun); rather than silently
    // compiling with no system headers, say so.
    errorf(errors, NULL, "compiling C shims for a macOS target is not "
      "supported from a Windows host");
    return false;
#else
    // An explicit --sysroot (a cross build, or an unusual native setup)
    // takes precedence over probing for a local SDK, whose headers would
    // be the wrong ones.
    if(sysroot[0] != '\0')
    {
      if(push_isystem(opt, args, errors, "-internal-isystem", sysroot,
        "/usr/include"))
        return true;

      errorf(errors, NULL, "sysroot '%s' has no usr/include directory",
        sysroot);
      return false;
    }

    const char* sdk_include = find_macos_sdk_include(opt);

    if(sdk_include == NULL)
    {
      errorf(errors, NULL, "could not find the macOS SDK include path\n"
        "  Install Xcode or CommandLineTools");
      return false;
    }

    args.push_back("-internal-isystem");
    args.push_back(sdk_include);
    return true;
#endif
  }

  if(target_is_windows(opt->triple))
  {
    // MSVC / Windows SDK include discovery (the compile-side mirror of the
    // vcvars lib-dir discovery the COFF linker uses) is not implemented
    // yet. Tracked for the Windows platform work; failing here is clearer
    // than letting every #include in a shim fail one header at a time.
    errorf(errors, NULL, "C shims are not yet supported when targeting "
      "Windows: MSVC include discovery is not implemented");
    return false;
  }

  // Linux and the BSDs share the classic layout; the order and the
  // directory set mirror what the clang driver emits for these targets
  // (including <sysroot>/include, which musl-style sysroots use).
  bool any = false;

  any |= push_isystem(opt, args, errors, "-internal-isystem", sysroot,
    "/usr/local/include");

  char multiarch[FILENAME_MAX];
  int written = snprintf(multiarch, sizeof(multiarch), "/usr/include/%s",
    c_system_triple(opt));

  if((written >= 0) && ((size_t)written < sizeof(multiarch)))
    any |= push_isystem(opt, args, errors, "-internal-externc-isystem",
      sysroot, multiarch);

  any |= push_isystem(opt, args, errors, "-internal-externc-isystem",
    sysroot, "/include");

  any |= push_isystem(opt, args, errors, "-internal-externc-isystem",
    sysroot, "/usr/include");

  // Per-directory skipping is right (multiarch and /usr/local are
  // legitimately absent on many systems), but an explicit --sysroot that
  // yields nothing at all deserves a direct answer, like the macOS branch
  // gives, instead of header-not-found errors from the shim.
  if(!any && (sysroot[0] != '\0'))
  {
    errorf(errors, NULL, "sysroot '%s' has no usable include directories\n"
      "  Searched: usr/local/include, usr/include/<triple>, include, "
      "usr/include", sysroot);
    return false;
  }

  return true;
}


// Compile one C shim source to an object file with the embedded clang.
static bool compile_shim(pass_opt_t* opt, ast_t* package, const char* src,
  const char* obj, const std::vector<const char*>& common_args)
{
  errors_t* errors = opt->check.errors;

  std::vector<const char*> args(common_args);

  // Per-package user flags, in deterministic insertion order (see the
  // package_t comment in package.c).
  for(strlist_t* p = package_c_defines(package); p != NULL;
    p = strlist_next(p))
  {
    std::string define = std::string("-D") + strlist_data(p);
    args.push_back(stringtab(opt->strtab, define.c_str()));
  }

  for(strlist_t* p = package_c_includes(package); p != NULL;
    p = strlist_next(p))
  {
    std::string include = std::string("-I") + strlist_data(p);
    args.push_back(stringtab(opt->strtab, include.c_str()));
  }

  // Without this, debug info names every shim's compile unit "<stdin>"
  // (cc1 default); gdb's source listings, coverage, and profilers then
  // can't tell shims apart.
  const char* src_base = strrchr(src, PATH_SLASH);
  src_base = (src_base != NULL) ? src_base + 1 : src;
  args.push_back("-main-file-name");
  args.push_back(src_base);

  args.push_back("-o");
  args.push_back(obj);
  args.push_back("-x");
  args.push_back("c");
  args.push_back(src);

  PonyDiagConsumer consumer(errors);

  // Invocation parsing runs outside the crash-recovery context below: genc
  // builds this argv itself, so a malformed-argv abort would be a ponyc bug
  // to fix, not hostile input to contain (only the string values inside
  // fixed flags are user-controlled, and clang diagnoses rather than
  // crashes on bad values).
  clang::CompilerInstance ci;
  ci.createDiagnostics(&consumer, /* ShouldOwnClient */ false);

  if(!clang::CompilerInvocation::CreateFromArgs(ci.getInvocation(), args,
    ci.getDiagnostics()))
  {
    // The argv is built by genc, so a parse failure is a ponyc bug; make
    // sure it can't fail the build silently if clang didn't say anything.
    if(consumer.getNumErrors() == 0)
      errorf(errors, src, "internal error: the embedded clang rejected the "
        "compiler arguments for this C shim");

    return false;
  }

  // A clang internal error that crashes or aborts (ICE, assertion) would
  // otherwise take the ponyc process down with no attribution; the
  // recovery context turns those into a per-shim failure. Not contained:
  // the exit()-flavored llvm::report_fatal_error paths, which terminate
  // the process before the context can intervene — the same exposure
  // Pony's own codegen has.
  bool ok = false;
  llvm::CrashRecoveryContext crc;

  bool ran = crc.RunSafely([&]() {
    clang::EmitObjAction action;
    ok = ci.ExecuteAction(action);
  });

  if(!ran)
  {
    errorf(errors, src, "internal error while compiling C shim (the "
      "embedded clang crashed); please report this at "
      "https://github.com/ponylang/ponyc/issues");
    return false;
  }

  // Mirror the CreateFromArgs fallback above: a failure that produced no
  // diagnostic must not fail the build silently.
  if(!ok && consumer.getNumErrors() == 0)
    errorf(errors, src, "internal error: the embedded clang failed without "
      "reporting an error");

  return ok && (consumer.getNumErrors() == 0);
}


bool genc(ast_t* program, pass_opt_t* opt)
{
  pony_assert(program != NULL);
  pony_assert(ast_id(program) == TK_PROGRAM);
  pony_assert(opt != NULL);

  errors_t* errors = opt->check.errors;

  // genc is not an AST pass with per-node re-entry flags, so guard
  // explicitly: a second ast_passes_program over the same program (a
  // resumable compile session) must not recompile shims and record their
  // objects a second time. Marked done up front — even a failed run has
  // already recorded its successfully-compiled objects.
  if(program_genc_done(program))
    return true;

  program_set_genc_done(program);

  // Fast path: most programs have no shims, and they must not pay for (or
  // depend on) resource-dir and sysroot discovery.
  bool any = false;

  for(ast_t* package = ast_child(program); package != NULL;
    package = ast_sibling(package))
  {
    if(package_c_sources(package) != NULL)
    {
      any = true;
      break;
    }
  }

  if(!any)
    return true;

  // The target description comes from codegen_pass_init; every in-tree
  // driver runs it at startup. An embedder that skips it would otherwise
  // crash on the NULL triple below instead of getting an attributed abort.
  pony_assert(opt->triple != NULL);

  if(opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Compiling C shims\n");

  // In-tree drivers (ponyc's main, the test harness) register the LLVM
  // target backends at process startup via codegen_llvm_init, before genc
  // can run. These idempotent calls are defensive coverage for libponyc
  // embedders that skip that init; clang needs the backends registered to
  // emit objects.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  llvm::CrashRecoveryContext::Enable();

  const char* resource_dir = clang_resource_dir(opt, errors);

  if(resource_dir == NULL)
    return false;

  const char* sysroot = c_shim_sysroot(opt, errors);

  if(sysroot == NULL)
    return false;

  // Arguments shared by every shim in the program: the cc1 target and
  // header-search setup, matching the target the Pony codegen uses.
  std::vector<const char*> common;
  char buf[FILENAME_MAX];

  common.push_back("-triple");
  common.push_back(opt->triple);

  if(opt->cpu != NULL && opt->cpu[0] != '\0')
  {
    common.push_back("-target-cpu");
    common.push_back(opt->cpu);
  }

  if(opt->features != NULL && opt->features[0] != '\0')
  {
    // The features string is comma-separated (the LLVM target-machine
    // form); cc1 takes one -target-feature per feature.
    const char* p = opt->features;

    while(*p != '\0')
    {
      const char* comma = strchr(p, ',');
      size_t len = (comma != NULL) ? (size_t)(comma - p) : strlen(p);

      if(len > 0 && len < sizeof(buf))
      {
        memcpy(buf, p, len);
        buf[len] = '\0';
        common.push_back("-target-feature");
        common.push_back(stringtab(opt->strtab, buf));
      }

      p = (comma != NULL) ? comma + 1 : p + len;
    }
  }

  if(opt->abi != NULL && opt->abi[0] != '\0')
  {
    common.push_back("-target-abi");
    common.push_back(opt->abi);
  }

  if(opt->pic)
  {
    common.push_back("-mrelocation-model");
    common.push_back("pic");
    common.push_back("-pic-level");
    common.push_back("2");
  }

  common.push_back(opt->release ? "-O2" : "-O0");

  if(!opt->release && !opt->strip_debug)
    common.push_back("-debug-info-kind=standalone");

  // Driver hygiene defaults that cc1 alone omits. Unwind tables keep
  // backtraces (crash reporters, perf, gdb) working through shim frames in
  // optimised builds; the frame pointer matches the driver's -O0 behavior.
  common.push_back("-funwind-tables=2");

  if(!opt->release)
    common.push_back("-mframe-pointer=all");

  // Our consumer formats every diagnostic itself; without this, clang's
  // caret printer also emits its own "N errors generated." tally to stderr.
  common.push_back("-fno-caret-diagnostics");

  // glibc's headers condition on __GNUC__; the driver always claims this
  // baseline, so mimic it.
  common.push_back("-fgnuc-version=4.2.1");

  common.push_back("-resource-dir");
  common.push_back(resource_dir);

  // Builtin headers and system headers, ordered the way the driver orders
  // them: builtins first in general, but on musl AFTER the system
  // directories — musl systems prefer /usr/include's copies of the shared
  // headers, and the vendored driver does the same (clang's
  // ToolChains/Linux.cpp). Diverging here would give Alpine shims
  // different headers than every other clang compile on that platform.
  bool builtin_include_last;
  {
    llvm::Triple target_triple(opt->triple);
    builtin_include_last = target_triple.isMusl() || c_host_is_musl(opt);
  }

  int written = snprintf(buf, sizeof(buf), "%s%cinclude", resource_dir,
    PATH_SLASH);

  if((written < 0) || ((size_t)written >= sizeof(buf)))
  {
    errorf(errors, NULL, "clang's resource include path is too long");
    return false;
  }

  const char* builtin_include = stringtab(opt->strtab, buf);

  if(!builtin_include_last)
  {
    common.push_back("-internal-isystem");
    common.push_back(builtin_include);
  }

  if(!add_system_include_args(opt, sysroot, common, errors))
    return false;

  if(builtin_include_last)
  {
    common.push_back("-internal-isystem");
    common.push_back(builtin_include);
  }

  if(sysroot[0] != '\0')
  {
    common.push_back("-isysroot");
    common.push_back(sysroot);
  }

  const char* output = (opt->output != NULL) ? opt->output : ".";

  // codegen creates the output directory too, but genc runs first; without
  // this, -o <new-dir> would fail only for programs with shims.
  pony_mkdir(output);

  // Object names carry the program name (like the Pony object does) so two
  // programs sharing a shim package and an output directory don't collide
  // on shim objects.
  const char* prog_name;

  if((opt->bin_name != NULL) && (opt->bin_name[0] != '\0'))
    prog_name = opt->bin_name;
  else
    prog_name = package_filename(ast_child(program));

  bool ok = true;
  size_t index = 0;

  for(ast_t* package = ast_child(program); package != NULL;
    package = ast_sibling(package))
  {
    for(strlist_t* s = package_c_sources(package); s != NULL;
      s = strlist_next(s))
    {
      const char* src = strlist_data(s);

      // Object name: <program>-<source base>.shim.<index>.o in the output
      // directory. The index makes names unique when same-named sources
      // appear in different packages; the .shim. marker separates these
      // from the Pony object.
      const char* base = strrchr(src, PATH_SLASH);
      base = (base != NULL) ? base + 1 : src;

      size_t base_len = strlen(base);
      if(base_len > 2 && strcmp(base + base_len - 2, ".c") == 0)
        base_len -= 2;

      int written = snprintf(buf, sizeof(buf),
        "%s%c%s-%.*s.shim." __zu ".o", output, PATH_SLASH, prog_name,
        (int)base_len, base, index);

      if((written < 0) || ((size_t)written >= sizeof(buf)))
      {
        errorf(errors, src, "output path for C shim object is too long");
        ok = false;
        index++;
        continue;
      }

      const char* obj = stringtab(opt->strtab, buf);

      if(opt->verbosity >= VERBOSITY_INFO)
        fprintf(stderr, " %s\n", src);

      if(compile_shim(opt, package, src, obj, common))
        program_add_c_object(program, obj);
      else
        ok = false;

      index++;
    }
  }

  return ok;
}
