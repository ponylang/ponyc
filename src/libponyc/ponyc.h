#ifndef PONYC_H
#define PONYC_H

#include <platform.h>
#include "pass/pass.h"

PONY_EXTERN_C_BEGIN

bool ponyc_init(pass_opt_t* options);

/** Tear down what ponyc_init set up, ending with LLVM's global state.
 *
 * Does not print the collected errors: the caller decides where they go
 * relative to its own output, and LLVM must still be alive for anything the
 * caller does beforehand that touches it.
 */
void ponyc_shutdown(pass_opt_t* options);

// ponyc_take_pending_link, ponyc_write_link_job and ponyc_abandon_link are
// defined in codegen/genexe.cc, which defines link_job_t.

/** Take the link that genexe left for the caller to run (when the caller set
 * options->defer_link), or NULL if there is none. The caller owns the result
 * and must pass it to ponyc_write_link_job or ponyc_abandon_link.
 */
link_job_t* ponyc_take_pending_link(pass_opt_t* options);

#ifdef PLATFORM_IS_POSIX_BASED
/** Write the job to fd in the format src/common/ponylink_job.h describes, for
 * ponyc (the driver) to run, then free the job; its files are then the
 * driver's to remove. If the job can't be written in full, it is abandoned
 * (ponyc_abandon_link) and the result is false with errno set. SIGPIPE is
 * ignored in the whole process while the job is written, and its previous
 * disposition restored afterwards.
 */
bool ponyc_write_link_job(link_job_t* job, int fd);
#endif

/** Remove the job's temporary files, as after a failed link, then free the
 * job.
 */
void ponyc_abandon_link(link_job_t* job);

PONY_EXTERN_C_END

#endif
