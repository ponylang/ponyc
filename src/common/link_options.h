#ifndef PONY_COMMON_LINK_OPTIONS_H
#define PONY_COMMON_LINK_OPTIONS_H

#include <platform.h>

#ifndef __cplusplus
#  error "link_options.h is C++"
#endif

#include "ponylink_job.h"

#include <string>
#include <vector>

#ifdef PLATFORM_IS_POSIX_BASED
#  include "remove_tree.h"
#  include <unistd.h>
#endif

// ponylink's options for one link, as ponylink_job.h describes them.
struct pony_link_options_t
{
  // --args.
  std::string args_file;
  bool verbose = false;
  bool abandon = false;
  std::vector<std::string> remove;
  std::vector<std::string> remove_on_success;
  std::vector<std::string> remove_tree;
  // --dsym, when not empty.
  std::string dsym;
  // --mutate, when mutate_tmp is not empty.
  std::string mutate_tmp;
  std::string mutate_final;
};

// The NULL-terminated argument vector that runs the ponylink binary at
// ponylink_path with options. The pointers stay valid while ponylink_path and
// options do.
static inline std::vector<const char*> pony_link_job_argv(
  const std::string& ponylink_path, const pony_link_options_t& options)
{
  std::vector<const char*> argv;
  argv.push_back(ponylink_path.c_str());
  argv.push_back(PONYLINK_JOB_FLAG);

  if(options.abandon)
    argv.push_back(PONYLINK_OPT_ABANDON);

  argv.push_back(PONYLINK_OPT_ARGS);
  argv.push_back(options.args_file.c_str());

  if(options.verbose)
    argv.push_back(PONYLINK_OPT_VERBOSE);

  for(const std::string& f : options.remove)
  {
    argv.push_back(PONYLINK_OPT_REMOVE);
    argv.push_back(f.c_str());
  }

  for(const std::string& d : options.remove_tree)
  {
    argv.push_back(PONYLINK_OPT_REMOVE_TREE);
    argv.push_back(d.c_str());
  }

  if(!options.mutate_tmp.empty())
  {
    argv.push_back(PONYLINK_OPT_MUTATE);
    argv.push_back(options.mutate_tmp.c_str());
    argv.push_back(options.mutate_final.c_str());
  }

  for(const std::string& f : options.remove_on_success)
  {
    argv.push_back(PONYLINK_OPT_REMOVE_ON_SUCCESS);
    argv.push_back(f.c_str());
  }

  if(!options.dsym.empty())
  {
    argv.push_back(PONYLINK_OPT_DSYM);
    argv.push_back(options.dsym.c_str());
  }

  argv.push_back(NULL);
  return argv;
}

#ifdef PLATFORM_IS_POSIX_BASED
// Remove the link's temporary paths: the remove and remove_tree paths and the
// mutation info's temporary file.
static inline void pony_link_remove_temporaries(
  const pony_link_options_t& options)
{
  if(!options.mutate_tmp.empty())
    unlink(options.mutate_tmp.c_str());

  for(const std::string& f : options.remove)
    unlink(f.c_str());

  for(const std::string& d : options.remove_tree)
    pony_remove_tree(d.c_str());
}
#endif

#endif
