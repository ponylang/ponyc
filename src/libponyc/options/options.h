#ifndef PONYC_OPTIONS_H
#define PONYC_OPTIONS_H

#include "../libponyc/pass/pass.h"
#include "../libponyrt/options/options.h"

#include <stdint.h>
#include <stdbool.h>

opt_arg_t* ponyc_opt_std_args(void);

void ponyc_print_std_usage();

bool ponyc_opt_process(opt_state_t* s, pass_opt_t* opt,
       /*OUT*/ bool* print_usage,
       /*OUT*/ bool* print_program_ast,
       /*OUT*/ bool* print_package_ast);

#endif
