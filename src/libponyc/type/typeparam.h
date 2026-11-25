#ifndef TYPE_TYPEPARAM_H
#define TYPE_TYPEPARAM_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

// The raw constraint. Returns null if unconstrained.
ast_t* typeparam_constraint(ast_t* typeparamref);

// Resolve a type-parameter definition (TK_TYPEPARAM) to the canonical root of
// its ast_data chain. Every TK_TYPEPARAM's data points one step toward the
// original definition set during the scope pass; copies — made via ast_dup or
// collect_type_params — retain that link, so a parameter bound through several
// layers (e.g. an iftype condition in a lambda inside a generic method) forms
// a chain that a single dereference would not fully resolve. Comparing two
// roots tests whether the parameters denote the same type variable. A null
// input or a null link stops the walk, so the result is null only when 'def'
// is.
ast_t* typeparam_root(ast_t* def);

// The position in `typeparams` of the type parameter whose root is `root`, or
// -1 if there is none.
int typeparam_root_index(ast_t* typeparams, ast_t* root);

typedef enum
{
  // Neither reference is to a method's own type parameter.
  TYPEPARAM_MATCH_NEITHER_METHOD,
  // Both are to a method's own type parameter, in the same position.
  TYPEPARAM_MATCH_SAME,
  // Both are to a method's own type parameter, in different positions, or
  // only one of them is to a method's own type parameter.
  TYPEPARAM_MATCH_DIFFERENT
} typeparam_match_t;

// Compares two type parameter references, each in one of two methods being
// compared. The methods' own type parameters correspond by position, whatever
// they are named. typeparam_method_index in typeparam.c says which references
// count as a method's own.
typeparam_match_t typeparam_method_refs_compare(ast_t* a, ast_t* b);

// The scope key for an inherited receiver narrowing whose name is already
// bound to a different declaration.
const char* typeparam_receiver_key(strtable_t* strtab, const char* name);

// Find the nearest binding of the reference's declaration, falling back to its
// stored definition. Resolve unbound references by name.
ast_t* typeparam_resolve(ast_t* ref, ast_t* scope, strtable_t* strtab);

/**
 * If the upper bounds of a typeparamref is a subtype of a type T, then every
 * possible binding of the typeparamref is a subtype of T.
 */
ast_t* typeparam_upper(ast_t* typeparamref);

/**
 * If a type T is a subtype of the lower bounds of a typeparamref, it is
 * always a subtype of the typeparamref. Returns null if there is no lower
 * bounds.
 */
ast_t* typeparam_lower(ast_t* typeparamref);

/**
 * Set the refcap of a typeparamref to be the set of all refcaps that could
 * be bound.
 */
void typeparam_set_cap(ast_t* typeparamref);

/**
 * Substitute TK_TYPEPARAMREF nodes in a type AST with references to narrowed
 * type parameters from an iftype's typeparam_store. Modifies `type` in place.
 * Returns true when at least one substitution was made.
 */
bool typeparam_narrow(ast_t* type, ast_t* typeparam_store);

/**
 * The constraint of the typeparam in the current scope.
 */
ast_t* typeparam_current(pass_opt_t* opt, ast_t* typeparamref, ast_t* scope);

/**
 * For each top-level TK_TYPEPARAMREF in `type` that has a narrowed iftype
 * constraint in scope, wraps it in an explicit intersection so the constraint
 * survives sanitise_type. Does not recurse into TK_NOMINAL type arguments;
 * those are covered by the expr_object type-parameter narrowing in lambda.c.
 */
ast_t* typeparam_embed_narrowing(pass_opt_t* opt, ast_t* type, ast_t* scope);

PONY_EXTERN_C_END

#endif
