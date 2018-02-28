#ifndef PLUGIN_PLUGIN_H
#define PLUGIN_PLUGIN_H

#include <platform.h>
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

// Plugins are shared objects/DLLs loaded through dlopen/LoadLibrary depending
// on the platform. Plugins can supply the following functions. It isn't
// mandatory to supply all of them.
//
// - bool pony_plugin_init(pass_opt_t* opt, void** user_data)
//
//    Called right after the plugin has been loaded. Should return true if
//    initialisation was successful, false otherwise. Detailed initialisation
//    errors can be reported through opt or outputted directly. Arbitrary data
//    can be stored in user_data.
//
// - void pony_plugin_final(pass_opt_t* opt, void* user_data)
//
//    Called right before the plugin is unloaded. This shouldn't report errors.
//
// - const char* pony_plugin_help(void* user_data)
//
//    Should return a string to be displayed alongside the compiler help when
//    invoking `ponyc --help`. If the string is dynamically allocated, the
//    plugin has the responsibility to free it in pony_plugin_final.
//
// - bool pony_plugin_parse_options(pass_opt_t* opt, int* argc, char** argv,
//     void* user_data)
//
//    Allows the plugin to parse command line options. argv contains all
//    options that weren't recognised by the compiler or by previously loaded
//    plugins. The function must remove the arguments that it recognised from
//    argv and update argc accordingly. No other modification is allowed. It is
//    recommended to use the options module in libponyrt (not libponyc), which
//    meets the above requirement. Should return true if parsing was successful,
//    false if it wasn't. Note that unrecognised options shouldn't count as
//    unsucessful parsing. Examples of failures include bad formatting of
//    recognised option arguments.
//    Note that as stated above, plugins will consume command line options based
//    on their loading order, which corresponds to their order of appearance in
//    the --plugin command line option.
//
// - void pony_plugin_visit_ast(const ast_t* ast, pass_opt_t* opt, pass_id pass,
//     void* user_data)
//
//    Called after each AST pass. The AST is the program AST. pass is the last
//    pass the AST ran through. This shouldn't report errors.
//
// - void pony_plugin_visit_reach(const reach_t* r, pass_opt_t* opt,
//     bool built_vtables, void* user_data)
//
//    Called after reachability analysis. built_vtables is true if vtable IDs
//    have been assigned, false otherwise. This shouldn't report errors.
//
// - void pony_plugin_visit_compile(const compile_t* c, pass_opt_t* opt,
//     void* user_data)
//
//    Called after LLVM code generation. This shouldn't report errors.
//
// Note that only read-only plugins are supported. A plugin **must not** modify
// the compiler data structures passed to it unless stated otherwise.

typedef struct ast_t ast_t;
typedef struct reach_t reach_t;
typedef struct compile_t compile_t;

bool plugin_load(pass_opt_t* opt, const char* paths);

void plugin_print_help(pass_opt_t* opt);

bool plugin_parse_options(pass_opt_t* opt, int* argc, char** argv);

void plugin_visit_ast(const ast_t* ast, pass_opt_t* opt, pass_id pass);

void plugin_visit_reach(const reach_t* r, pass_opt_t* opt, bool built_vtables);

void plugin_visit_compile(const compile_t* c, pass_opt_t* opt);

void plugin_unload(pass_opt_t* opt);

PONY_EXTERN_C_END

#endif
