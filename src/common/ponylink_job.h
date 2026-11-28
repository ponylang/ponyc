#ifndef PONY_COMMON_PONYLINK_JOB_H
#define PONY_COMMON_PONYLINK_JOB_H

// How a link travels from the compiler (ponyc-compiler) to ponylink, and the
// exit statuses ponylink reports back. POSIX hosts only.
//
// The compiler runs `ponylink --ponyc-job <options>` itself for every link it
// doesn't hand off. When ponyc (the driver) starts the compiler, it passes
// `--link-handoff=<fd>` as the first argument, naming the write end of a pipe.
// The compiler then hands the link of the last package to the driver instead of
// running it: once compilation is done, it writes to the pipe a sequence of
// NUL-terminated strings, ponylink's absolute path followed by ponylink's
// arguments, starting with PONYLINK_JOB_FLAG, and then PONYC_JOB_END. Nothing
// written means there is no link to run. Only the compiler may hold the pipe's
// write end: the driver reads until every holder has closed it. The driver
// rejects a job that doesn't end with PONYC_JOB_END, so a job cut off partway
// is never run; the files it names stay. Once the compiler has exited
// successfully, the driver runs ponylink with the job's arguments (without
// PONYC_JOB_END). If the compiler failed or a terminating signal arrived first,
// the driver runs ponylink with PONYLINK_OPT_ABANDON instead, and if the link
// fails for any reason but PONYLINK_EXIT_USAGE it runs ponylink again with it.
// PONYLINK_OPT_ABANDON goes right after PONYLINK_JOB_FLAG, where it can't be
// read as another option's value.
//
// ponylink's options:
//
//   --args <path>
//     The file of lld arguments, one per line. Required.
//   --verbose
//     Print ponylink's process ID and its parent's.
//   --abandon
//     Don't link: only remove the --remove, --remove-tree and --mutate
//     temporary paths, as after a failed link, and exit 0.
//   --remove <path>
//     Removed once the link finishes, whether it produced an executable or not.
//   --remove-on-success <path>
//     Removed only once the link has produced an executable.
//   --remove-tree <path>
//     Directory removed once the link finishes.
//   --dsym <exe>
//     macOS: <exe>.dSYM is rebuilt once the link has produced an executable.
//   --mutate <path> <final-path>
//     The mutation info, renamed to final-path once the link has produced an
//     executable and removed otherwise.

#include <stdbool.h>

#define PONYC_LINK_HANDOFF_OPT "--link-handoff="

#define PONYLINK_JOB_FLAG "--ponyc-job"

// The last string of a job the compiler hands to the driver. Not passed to
// ponylink.
#define PONYC_JOB_END "--end-of-job"

#define PONYLINK_OPT_ARGS "--args"
#define PONYLINK_OPT_VERBOSE "--verbose"
#define PONYLINK_OPT_ABANDON "--abandon"
#define PONYLINK_OPT_REMOVE "--remove"
#define PONYLINK_OPT_REMOVE_ON_SUCCESS "--remove-on-success"
#define PONYLINK_OPT_REMOVE_TREE "--remove-tree"
#define PONYLINK_OPT_DSYM "--dsym"
#define PONYLINK_OPT_MUTATE "--mutate"

// The link failed: ponylink printed why and removed the temporary files it was
// given.
#define PONYLINK_EXIT_FAILED 255

// The options were bad. ponylink printed why and removed nothing. An
// --abandon run with the same options fails the same way, so after this
// status ponyc (the driver) can't remove a handed-off link's files.
#define PONYLINK_EXIT_USAGE 1

// The link produced an executable but the mutation info couldn't be renamed
// into place. ponylink printed why and finished the rest of its cleanup.
#define PONYLINK_EXIT_MUTATE_INFO 4

// Whether ponylink, exiting with status, has printed why the link failed.
static inline bool ponylink_printed_why(int status)
{
  return (status == PONYLINK_EXIT_FAILED) ||
    (status == PONYLINK_EXIT_MUTATE_INFO);
}

#endif
