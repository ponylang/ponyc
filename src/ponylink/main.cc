// ponylink — standalone linker for ponyc.
//
// `ponylink <args-file>` reads lld arguments from a file (one per line) and
// calls lld::lldMain(). Running lld in its own process gives it a clean
// address space, so the compiler's allocations don't add to lld's memory.
//
// `ponylink --ponyc-job <options>` (POSIX) runs a link the compiler prepared,
// then does the post-link work: report a failure, remove the bitcode and other
// temporary files, rebuild the macOS dSYM bundle, and move the mutation info
// into place. src/common/ponylink_job.h describes the options.

#include <lld/Common/Driver.h>
#include <llvm/Support/raw_ostream.h>

#include <platform.h>
#include "ponylink_job.h"
#include "remove_tree.h"
#include "forward_signals.h"
#include "link_options.h"
#include "run_command.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <unistd.h>
#endif

LLD_HAS_DRIVER(elf)
LLD_HAS_DRIVER(macho)
LLD_HAS_DRIVER(coff)
LLD_HAS_DRIVER(mingw)
LLD_HAS_DRIVER(wasm)

#ifdef PLATFORM_IS_POSIX_BASED
static bool parse_job(int argc, char* argv[], int first,
  pony_link_options_t& job, std::string& error)
{
  for(int i = first; i < argc; i++)
  {
    std::string opt = argv[i];
    int values = 0;

    if((opt == PONYLINK_OPT_ARGS) || (opt == PONYLINK_OPT_REMOVE) ||
      (opt == PONYLINK_OPT_REMOVE_ON_SUCCESS) ||
      (opt == PONYLINK_OPT_REMOVE_TREE) || (opt == PONYLINK_OPT_DSYM))
      values = 1;
    else if(opt == PONYLINK_OPT_MUTATE)
      values = 2;
    else if(opt == PONYLINK_OPT_VERBOSE)
      job.verbose = true;
    else if(opt == PONYLINK_OPT_ABANDON)
      job.abandon = true;
    else
    {
      error = "unknown option " + opt;
      return false;
    }

    if((i + values) >= argc)
    {
      error = "missing value for " + opt;
      return false;
    }

    if(opt == PONYLINK_OPT_ARGS)
      job.args_file = argv[i + 1];
    else if(opt == PONYLINK_OPT_REMOVE)
      job.remove.push_back(argv[i + 1]);
    else if(opt == PONYLINK_OPT_REMOVE_ON_SUCCESS)
      job.remove_on_success.push_back(argv[i + 1]);
    else if(opt == PONYLINK_OPT_REMOVE_TREE)
      job.remove_tree.push_back(argv[i + 1]);
    else if(opt == PONYLINK_OPT_DSYM)
      job.dsym = argv[i + 1];
    else if(opt == PONYLINK_OPT_MUTATE)
    {
      job.mutate_tmp = argv[i + 1];
      job.mutate_final = argv[i + 2];
    }

    i += values;
  }

  if(job.args_file.empty())
  {
    error = "missing " PONYLINK_OPT_ARGS;
    return false;
  }

  return true;
}

static void remove_on_success(const pony_link_options_t& job)
{
  for(const std::string& f : job.remove_on_success)
    unlink(f.c_str());
}

static void remove_files(const pony_link_options_t& job)
{
  for(const std::string& f : job.remove)
    unlink(f.c_str());
}

// Once lld has produced a new executable whose mutation info won't be moved
// into place, remove the info an earlier build left at the final path: it
// describes a different executable.
static void drop_old_mutation_info(const pony_link_options_t& job)
{
  if(!job.mutate_final.empty())
    unlink(job.mutate_final.c_str());
}

static void remove_trees(const pony_link_options_t& job)
{
  for(const std::string& d : job.remove_tree)
    pony_remove_tree(d.c_str());
}

// Run argv[0] as the current child and wait for it, unless a terminating
// signal has been received. Returns false, without starting anything, if one
// has.
static bool run_child(const char* const argv[], pony_command_result_t* result)
{
  pony_forward_spawn_t started = pony_forward_spawn(argv[0], argv,
    PONY_FORWARD_UNLESS_SIGNALED, PONY_FORWARD_INHERITED_MASK);

  switch(started.kind)
  {
    case pony_forward_spawn_t::CANCELLED:
      return false;

    case pony_forward_spawn_t::NOT_STARTED:
      *result = pony_command_result(pony_command_result_t::NOT_STARTED,
        started.error);
      return true;

    case pony_forward_spawn_t::STARTED:
      break;
  }

  *result = pony_forward_wait(started);
  return true;
}

static void rebuild_dsym(const std::string& exe)
{
  std::string bundle = exe + ".dSYM";
  pony_remove_tree(bundle.c_str());

  const char* dsym_argv[] = {"dsymutil", exe.c_str(), NULL};
  pony_command_result_t result;

  if(!run_child(dsym_argv, &result) || (pony_forward_received() != 0))
    return;

  if(result.kind == pony_command_result_t::EXITED)
  {
    if(result.value != 0)
      fprintf(stderr, "Error:\nponylink: unable to create dsym\n");
  } else if(result.kind == pony_command_result_t::NOT_STARTED) {
    fprintf(stderr, "Error:\nponylink: unable to start dsymutil: %s\n",
      strerror(result.value));
  } else {
    fprintf(stderr, "Error:\nponylink: %s\n", pony_describe_command_status(
      result, "dsymutil", "dsymutil").c_str());
  }
}

static int run_job(int argc, char* argv[])
{
  const char* self = argv[0];
  pony_link_options_t job;
  std::string error;

  if(!parse_job(argc, argv, 2, job, error))
  {
    fprintf(stderr, "Error:\nponylink: %s\n", error.c_str());
    return PONYLINK_EXIT_USAGE;
  }

  if(job.abandon)
  {
    // --abandon often follows a terminating signal; another one mustn't stop
    // the removal partway, and neither must a write to a closed stderr.
    const int ignored[] = {SIGINT, SIGQUIT, SIGTERM, SIGHUP, SIGPIPE};
    for(int sig : ignored)
      signal(sig, SIG_IGN);
  }

  if(job.verbose)
    fprintf(stderr, "ponylink pid %d, parent %d\n", (int)getpid(),
      (int)getppid());

  if(job.abandon)
  {
    pony_link_remove_temporaries(job);
    return 0;
  }

  if(!pony_forward_start())
  {
    fprintf(stderr, "Error:\nponylink: unable to set up signal handling: %s\n",
      strerror(errno));
    pony_link_remove_temporaries(job);
    return PONYLINK_EXIT_FAILED;
  }

  // The link runs in a child so a signal that kills lld, such as the kernel's
  // out-of-memory killer, can still be reported and cleaned up after.
  const char* link_argv[] = {self, job.args_file.c_str(), NULL};
  pony_command_result_t result;
  bool ran = run_child(link_argv, &result);

  if(!ran || !result.succeeded())
  {
    int sig = pony_forward_received();

    if(sig == 0)
      fprintf(stderr, "Error:\n%s\n",
        pony_describe_link_status(result, self).c_str());

    pony_link_remove_temporaries(job);

    if(sig != 0)
      pony_forward_reraise(sig);

    return PONYLINK_EXIT_FAILED;
  }

  if(!job.dsym.empty() && (pony_forward_received() == 0))
    rebuild_dsym(job.dsym);

  int sig = pony_forward_received();
  if(sig != 0)
  {
    // The executable stays, without its mutation info.
    pony_link_remove_temporaries(job);
    drop_old_mutation_info(job);
    pony_forward_reraise(sig);
  }

  remove_trees(job);

  bool mutate_ok = job.mutate_tmp.empty() ||
    (rename(job.mutate_tmp.c_str(), job.mutate_final.c_str()) == 0);

  if(!mutate_ok)
  {
    fprintf(stderr, "Error:\nponylink: couldn't write mutation info file %s;"
      " the executable was linked\n", job.mutate_final.c_str());
    unlink(job.mutate_tmp.c_str());
    drop_old_mutation_info(job);
  }

  remove_on_success(job);
  remove_files(job);

  sig = pony_forward_received();
  if(sig != 0)
    pony_forward_reraise(sig);

  return mutate_ok ? 0 : PONYLINK_EXIT_MUTATE_INFO;
}
#endif

int main(int argc, char* argv[])
{
#ifdef PLATFORM_IS_POSIX_BASED
  if((argc >= 2) && (strcmp(argv[1], PONYLINK_JOB_FLAG) == 0))
    return run_job(argc, argv);
#endif

  if(argc != 2)
  {
#ifdef PLATFORM_IS_POSIX_BASED
    fprintf(stderr, "usage: %s <args-file>\n"
      "       %s " PONYLINK_JOB_FLAG " " PONYLINK_OPT_ARGS " <args-file>"
      " [options]\n", argv[0], argv[0]);
#else
    fprintf(stderr, "usage: %s <args-file>\n", argv[0]);
#endif
    return 1;
  }

  std::ifstream f(argv[1]);
  if(!f)
  {
    fprintf(stderr, "ponylink: cannot open %s: %s\n",
      argv[1], strerror(errno));
    return 1;
  }

  std::vector<std::string> arg_strings;
  std::string line;
  while(std::getline(f, line))
  {
    if(!line.empty())
      arg_strings.push_back(std::move(line));
  }

  std::vector<const char*> args;
  for(auto& s : arg_strings)
    args.push_back(s.c_str());

  std::string lld_stderr_str;
  llvm::raw_string_ostream lld_stderr(lld_stderr_str);

  lld::Result result = lld::lldMain(
    args,
    llvm::outs(),
    lld_stderr,
    {{lld::WinLink, &lld::coff::link},
     {lld::Gnu, &lld::elf::link},
     {lld::Darwin, &lld::macho::link},
     {lld::MinGW, &lld::mingw::link},
     {lld::Wasm, &lld::wasm::link}});

  if(!lld_stderr_str.empty())
    fprintf(stderr, "%s", lld_stderr_str.c_str());

  return result.retCode;
}
