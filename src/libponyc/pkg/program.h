#ifndef PROGRAM_H
#define PROGRAM_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

typedef struct program_t program_t;

/// Create a program_t to add to a TK_PROGRAM AST node.
program_t* program_create();

/// Free the given program_t.
void program_free(program_t* program);

/// Assign the next module ID from the program at the root of the given AST.
/// The actual AST node passed in may be anywhere in the tree.
uint32_t program_assign_module_id(ast_t* ast);

/// Process a "lib:" scheme use command.
bool use_library(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options);

/// Process a "path:" scheme use command.
bool use_path(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options);

/** Build the required linker arguments based on the libraries we're using.
 * Once this has been called no more calls to use_library() are permitted.
 */
void program_lib_build_args(ast_t* program, pass_opt_t* opt,
  const char* path_preamble, const char* rpath_preamble,
  const char* global_preamble, const char* global_postamble,
  const char* lib_premable, const char* lib_postamble);

/** Report the command line options for the user specified libraries, if any.
 * May only be called after program_lib_build_args().
 * The returned string is valid for the lifetime of the associated AST and
 * should not be freed by the caller.
 */
const char* program_lib_args(ast_t* program);

/** Build library arguments for embedded LLD linking.
 * Collects all library paths (source-defined and CLI/PONYPATH) and library
 * names into structured arrays without shell-specific formatting.
 * Mutually exclusive with program_lib_build_args() — call one or the other.
 */
void program_lib_build_args_embedded(ast_t* program, pass_opt_t* opt);

/** Has gencshim already compiled this program's shims? Makes gencshim idempotent
 * when ast_passes_program is run again on the same program (e.g. a
 * resumable compile session). */
bool program_gencshim_done(ast_t* program);

/** Mark the program's shims as compiled. */
void program_set_gencshim_done(ast_t* program);

/** Record a C shim object emitted by gencshim. Paths accumulate in gencshim's
 * module-walk order, which the platform linkers preserve. */
void program_add_c_object(ast_t* program, const char* path);

/** Return the number of C shim objects recorded by gencshim. */
size_t program_c_object_count(ast_t* program);

/** Return a C shim object path by index. */
const char* program_c_object_at(ast_t* program, size_t index);

/** Return the number of library search paths.
 * Only valid after program_lib_build_args_embedded(). */
size_t program_lib_path_count(ast_t* program);

/** Return a library search path by index (unquoted, no -L prefix). */
const char* program_lib_path_at(ast_t* program, size_t index);

/** Return the number of user-specified libraries.
 * Only valid after program_lib_build_args_embedded(). */
size_t program_lib_count(ast_t* program);

/** Return a library name by index (unquoted, no -l prefix or .lib suffix). */
const char* program_lib_at(ast_t* program, size_t index);

void program_dump(ast_t* program);

/** Scheduling of anonymous classes made from literals in trait text.
 *
 * A trait or interface check is the expr pass on its entity. A class whose
 * provides closure holds a trait being checked waits on a queue until no
 * trait check is running, then takes the trait-view bodies of its inherited
 * methods and its own expr pass.
 */
void program_trait_check_begin(ast_t* entity, size_t error_count);
void program_trait_check_end(ast_t* entity, size_t error_count);

/** Whether a trait's or interface's check reported errors. */
bool program_trait_check_failed(ast_t* entity);

/** Whether the error with the given index (in report order) gets no note
 * naming an inherited method it was found in: it was reported by a trait's
 * or interface's own check, or by a check of a default argument started on
 * demand, neither of which is the inherited method's code.
 */
bool program_error_without_copy_note(ast_t* anchor, size_t index);

/** Record that the errors with indexes from `start` up to `end` get no note
 * naming an inherited method (see program_error_without_copy_note).
 */
void program_add_unnoted_range(ast_t* anchor, size_t start, size_t end);

/** Whether the check of this trait or interface has begun and not ended. */
bool program_trait_check_in_progress(ast_t* entity);

/** Whether a trait check or a default argument check is running. */
bool program_any_trait_check(ast_t* ast);

/** Default argument checks, innermost first. `anchor` is any node of the
 * program.
 */
void program_default_check_begin(ast_t* anchor, ast_t* param);
bool program_default_check_end(ast_t* anchor, ast_t* param);
bool program_default_check_in_progress(ast_t* anchor, ast_t* param);
bool program_any_default_check(ast_t* anchor);
astlist_t* program_default_checks(ast_t* anchor);

/** The queue of classes waiting until no trait check is running (see above).
 * program_next_deferred removes and returns the first class waiting, or NULL
 * when none is. The draining flag is set while the waiting classes are being
 * checked.
 */
void program_defer_class(ast_t* entity);
bool program_is_deferred(ast_t* entity);
ast_t* program_next_deferred(ast_t* ast);
bool program_draining(ast_t* ast);
void program_set_draining(ast_t* ast, bool draining);

/** Inherited methods being checked, for the note added to errors in them.
 * `anchor` is any node of the program.
 */
void program_push_copy_check(ast_t* anchor, ast_t* entity, ast_t* method,
  size_t count);
bool program_pop_copy_check(ast_t* anchor, ast_t* method, ast_t** entity,
  size_t* count);

/** The type and inherited method that held the literal an ANON_COPY class was
 * made from, and the body in that method that holds the literal: the
 * innermost iftype branch or method body around it.
 */
void program_set_anon_host(ast_t* anon, ast_t* entity, ast_t* method,
  ast_t* site);
bool program_anon_host(ast_t* anon, ast_t** entity, ast_t** method);
ast_t* program_anon_site(ast_t* anon);

/** Templates of trait and interface methods: closed copies taken when a
 * trait's methods are complete and bound where written, before any check.
 * Inherited copies for types checked after the trait are made from them.
 */
void program_set_template(ast_t* anchor, ast_t* method, ast_t* copy);
ast_t* program_template(ast_t* anchor, ast_t* method);

/** The class a literal written in trait text makes in its trait's own check,
 * keyed by where the literal is written. `origins` lists, for each of the
 * class's type parameters, the trait's or method's parameter it was made from
 * (a TK_TYPEPARAMS of TK_TYPEPARAMREFs); the program owns it.
 */
void program_set_literal_link(ast_t* literal, ast_t* def, ast_t* origins);

/** The class recorded by program_set_literal_link for the literal written
 * where `literal` is, with its `origins`, or NULL if none is recorded.
 */
ast_t* program_literal_link(ast_t* anchor, ast_t* literal, ast_t** origins);

/** Classes made from a literal in inherited copies, keyed by where the
 * literal is written, that another copy of the literal may share, with the
 * literal's members as they were written in that copy (the program owns
 * them). program_next_copy_literal gives the next class after `after` (NULL
 * for the first), or NULL.
 */
void program_add_copy_literal(ast_t* literal, ast_t* def, ast_t* text);
ast_t* program_next_copy_literal(ast_t* anchor, ast_t* literal, ast_t* after);
ast_t* program_copy_literal_text(ast_t* anchor, ast_t* def);

/** The one renamed copy of a method's type parameter: a type parameter of its
 * own, not in any tree, that the program owns. Made the first time it is
 * asked for.
 */
ast_t* program_alpha_typeparam(ast_t* typeparam);

PONY_EXTERN_C_END

#endif
