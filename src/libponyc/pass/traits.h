#ifndef PASS_TRAITS_H
#define PASS_TRAITS_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

/** Process traits provided by each actor, class and data class, adding the
 * required methods.
 */
ast_result_t pass_traits(ast_t** astp, pass_opt_t* options);

/** Internal annotations recording what kind of anonymous class a literal made.
 * A class made from a literal written in a trait's or interface's own text has
 * ANON_TRAIT; one made from such a literal in a copy of the method held by a
 * type the program writes (or nested in such a class's own text) has
 * ANON_COPY. A class made from a literal in ordinary code has neither.
 */
#define ANON_TRAIT_ANNOTATION "ponyint_anon_trait"
#define ANON_COPY_ANNOTATION "ponyint_anon_copy"

/** The internal annotation on a method of an ANON_TRAIT or ANON_COPY class
 * that holds a copy of the providing trait's already checked method. Such a
 * method is not checked again.
 */
#define TRAIT_VIEW_ANNOTATION "ponyint_traitview"

/** The internal annotation on a default argument copied into a call. */
#define COPIED_DEFAULT_ANNOTATION "ponyint_copied_default"

/** Whether a method holds a trait-view copy (TRAIT_VIEW_ANNOTATION).
 */
bool is_trait_view_method(ast_t* method, pass_opt_t* opt);

/** Whether code checked in `method` of `entity` is inherited code: a method
 * whose body a trait or interface supplied, checked for the type that
 * inherits it, or a member of a class made from a literal in such code.
 * Diagnostics that only say some code is redundant are reported by the
 * providing trait's own check, not for each inheriting type.
 */
bool is_copy_method(ast_t* entity, ast_t* method, pass_opt_t* opt);

/** Whether an iftype's or a guard's condition `subject <: super` holds, decided
 * as code generation and reachability choose the branch it guards (with
 * capabilities compared exactly), so that a branch the type check takes to
 * be unable to run is one that is never generated.
 */
bool iftype_holds(ast_t* subject, ast_t* super, pass_opt_t* opt);

/** The type that holds a method, whether or not the method is in a method
 * group, or NULL.
 */
ast_t* method_entity(ast_t* method);

/** is_copy_method for the method and type being checked. */
bool is_copy_context(pass_opt_t* opt);

/** Called when a pass begins and ends checking a method. Errors reported while
 * an inherited method is checked get the note "in method 'M' inherited by INH
 * from T" at the provides entry the method arrived through.
 */
void copy_check_enter(ast_t* method, pass_opt_t* opt);
void copy_check_leave(ast_t* method, pass_opt_t* opt);

/** Whether `body` is code that can't run: the then branch of an iftype whose
 * condition is false for its concrete subject, the else branch of one whose
 * condition always holds, or the body of a method whose guard is false for its
 * concrete subjects. In inherited code such a body is checked as the providing
 * trait wrote it and is not verified or generated.
 */
bool is_decided_false_branch(ast_t* body, pass_opt_t* opt);

/** Whether `ast` is inside code that can't run: a decided-false branch
 * (is_decided_false_branch) of its method, or a method of a class made from a
 * literal written in such code.
 */
bool is_in_unrunnable_code(ast_t* ast, pass_opt_t* opt);

/** Whether `ast` is a branch that can't run in code whose types were
 * substituted: inherited code (is_copy_context) or a default argument copied
 * into a call (COPIED_DEFAULT_ANNOTATION). Such a branch is not verified.
 */
bool is_unverified_branch(ast_t* ast, pass_opt_t* opt);

/** Whether `entity` is a class made from a literal in inherited code that
 * can't run (is_decided_false_branch), so no instance of it is ever made.
 */
bool is_unreachable_anon_class(ast_t* entity, pass_opt_t* opt);

/** In inherited code inside an iftype whose narrowing need not hold for the
 * substituted types, whether `sub` passes as `super` once intersected with
 * the narrowing, as the providing trait's own check passed it. False outside
 * inherited code.
 */
bool is_subtype_via_narrowing(pass_opt_t* opt, ast_t* at, ast_t* sub,
  ast_t* super);

/** Called with a narrowing's subject, its supertype and the type parameters it
 * narrows; returns true to stop the walk.
 */
typedef bool (*narrowing_fn)(ast_t* subject, ast_t* super, ast_t* typeparams,
  void* ctx);

/** In inherited code, calls `fn` for each narrowing around `at` that need not
 * hold for the substituted types, from the innermost iftype out to the
 * method's guard. Returns true when `fn` did. False outside inherited code.
 */
bool visit_copy_narrowings(pass_opt_t* opt, ast_t* at, narrowing_fn fn,
  void* ctx);

/** For a method an entity inherited, the name of the trait in the entity's
 * provides list it arrived through; NULL for the entity's own method.
 */
const char* inherited_provider_name(ast_t* entity, ast_t* method);

/** In a method a named type inherited, the type the providing trait's own
 * check gave the node written at the same place as `node`, with the trait's
 * type parameters replaced by the inheriting type's arguments; NULL when it
 * isn't available. The result is a new type that the caller owns.
 */
ast_t* provider_view_type(pass_opt_t* opt, ast_t* node);

/** For a method an entity inherited, or a method of a class made from a
 * literal in one, the inheriting type's provides entry it arrived through;
 * NULL for other methods.
 */
ast_t* inherited_provides_entry(ast_t* entity, ast_t* method,
  pass_opt_t* opt);

/** Give a deferred class's inherited methods their bodies, copied from the
 * providers' checked methods, once the providers' checks have finished.
 */
bool attach_trait_view_bodies(ast_t* entity, pass_opt_t* opt);

PONY_EXTERN_C_END

#endif
