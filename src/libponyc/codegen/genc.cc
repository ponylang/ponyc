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
#include <llvm/Support/VirtualFileSystem.h>
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
// notes attach to the preceding error, mirroring clang's own presentation;
// warnings go to stderr so they're visible without failing the build.
class PonyDiagConsumer : public clang::DiagnosticConsumer
{
public:
  explicit PonyDiagConsumer(errors_t* errors)
    : errors_(errors), saw_error_(false)
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
        if(file != NULL)
          errorf(errors_, file, "%u:%u: %s", line, col, text.c_str());
        else
          errorf(errors_, NULL, "%s", text.c_str());

        saw_error_ = true;
        break;

      case clang::DiagnosticsEngine::Note:
        if(saw_error_)
        {
          if(file != NULL)
            errorf_continue(errors_, file, "%u:%u: %s", line, col,
              text.c_str());
          else
            errorf_continue(errors_, NULL, "%s", text.c_str());
        }
        break;

      default:
        if(file != NULL)
          fprintf(stderr, "%s:%u:%u: warning: %s\n", file, line, col,
            text.c_str());
        else
          fprintf(stderr, "warning: %s\n", text.c_str());
        break;
    }
  }

private:
  errors_t* errors_;
  bool saw_error_;
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

  if(!c_is_cross_compiling(opt))
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


static void push_isystem(pass_opt_t* opt, std::vector<const char*>& args,
  const char* flag, const char* sysroot, const char* suffix)
{
  char buf[FILENAME_MAX];
  snprintf(buf, sizeof(buf), "%s%s", sysroot, suffix);

  if(!dir_exists(buf))
    return;

  args.push_back(flag);
  args.push_back(stringtab(opt->strtab, buf));
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
#ifndef PLATFORM_IS_WINDOWS
    const char* sdk_include = find_macos_sdk_include(opt);

    if(sdk_include == NULL)
    {
      errorf(errors, NULL, "could not find the macOS SDK include path\n"
        "  Install Xcode or CommandLineTools");
      return false;
    }

    args.push_back("-internal-isystem");
    args.push_back(sdk_include);
#endif
    return true;
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

  // Linux and the BSDs share the classic layout.
  push_isystem(opt, args, "-internal-isystem", sysroot, "/usr/local/include");

  char multiarch[FILENAME_MAX];
  snprintf(multiarch, sizeof(multiarch), "/usr/include/%s",
    c_system_triple(opt));
  push_isystem(opt, args, "-internal-externc-isystem", sysroot, multiarch);

  push_isystem(opt, args, "-internal-externc-isystem", sysroot,
    "/usr/include");

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

  args.push_back("-o");
  args.push_back(obj);
  args.push_back("-x");
  args.push_back("c");
  args.push_back(src);

  PonyDiagConsumer consumer(errors);

  clang::CompilerInstance ci;
  ci.createDiagnostics(&consumer, /* ShouldOwnClient */ false);

  if(!clang::CompilerInvocation::CreateFromArgs(ci.getInvocation(), args,
    ci.getDiagnostics()))
    return false;

  // A clang internal error (ICE, report_fatal_error, assertion) would
  // otherwise abort the ponyc process with no attribution. The recovery
  // context turns it into a per-shim failure instead.
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

  return ok && (consumer.getNumErrors() == 0);
}


bool genc(ast_t* program, pass_opt_t* opt)
{
  pony_assert(program != NULL);
  pony_assert(ast_id(program) == TK_PROGRAM);
  pony_assert(opt != NULL);

  errors_t* errors = opt->check.errors;

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

  if(opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Compiling C shims\n");

  // genc runs before codegen initializes LLVM, and clang needs the target
  // backends registered to emit objects. These are idempotent; codegen
  // calls them again later.
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

  if(!opt->release)
    common.push_back("-debug-info-kind=standalone");

  // glibc's headers condition on __GNUC__; the driver always claims this
  // baseline, so mimic it.
  common.push_back("-fgnuc-version=4.2.1");

  common.push_back("-resource-dir");
  common.push_back(resource_dir);

  // Builtin headers first, then the target's system headers, mirroring the
  // driver's order.
  snprintf(buf, sizeof(buf), "%s%cinclude", resource_dir, PATH_SLASH);
  common.push_back("-internal-isystem");
  common.push_back(stringtab(opt->strtab, buf));

  if(!add_system_include_args(opt, sysroot, common, errors))
    return false;

  if(sysroot[0] != '\0')
  {
    common.push_back("-isysroot");
    common.push_back(sysroot);
  }

  const char* output = (opt->output != NULL) ? opt->output : ".";
  bool ok = true;
  size_t index = 0;

  for(ast_t* package = ast_child(program); package != NULL;
    package = ast_sibling(package))
  {
    for(strlist_t* s = package_c_sources(package); s != NULL;
      s = strlist_next(s))
    {
      const char* src = strlist_data(s);

      // Object name: <source base>.shim.<index>.o in the output directory.
      // The index makes names unique when same-named sources appear in
      // different packages; the .shim. marker separates these from the
      // Pony object.
      const char* base = strrchr(src, PATH_SLASH);
      base = (base != NULL) ? base + 1 : src;

      size_t base_len = strlen(base);
      if(base_len > 2 && strcmp(base + base_len - 2, ".c") == 0)
        base_len -= 2;

      snprintf(buf, sizeof(buf), "%s%c%.*s.shim." __zu ".o", output,
        PATH_SLASH, (int)base_len, base, index);
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
