#ifndef ASTBUILD_H
#define ASTBUILD_H

#include "stringtab.h"

// Macros for building ASTs

/** The macros below allow for building arbitrarily complex ASTs with a simple,
 * S-expression like syntax.
 *
 * At the tope level there must be exactly one of:
 * BUILD      builds an AST and creates a variable in which to store it.
 * REPLACE    builds an AST with which it replaces the specified existing tree.
 *
 * Within these macros the following are used to build up the tree:
 * NODE       creates a node with a specified token ID and optionally children.
 * TREE       inserts an already built subtree.
 * ID         adds a TK_ID node with the given ID name.
 * NONE       is syntactic sugar to add a TK_NONE node.
 * AST_SCOPE  adds a symbol table to the enclosing node.
 * DATA       sets the data field of the enclosing node.
 */

/** Builds an AST based on the specified existing tree.
 * A variable with the name given by var is defined and the created tree is
 * stored in it.
 * An existing node must be provided in existing, all created nodes are based
 * on this.
 */
#define BUILD(var, existing, ...) \
  ast_t* var; \
  { \
    ast_t* basis_ast = existing; \
    ast_t* parent = NULL; \
    ast_t* last_sibling = NULL; \
    ast_t* node = NULL; \
    __VA_ARGS__ \
    var = parent; \
  }

#define BUILD_NO_DEBUG(var, existing, ...) \
  ast_t* var; \
  { \
    ast_t* existing_no_debug = ast_from(existing, ast_id(existing)); \
    ast_setdebug(existing_no_debug, false); \
    BUILD(build_ast, existing_no_debug, __VA_ARGS__); \
    ast_free(existing_no_debug); \
    var = build_ast; \
  }

/** Builds an AST to replace the specified existing tree.
 * The provided existing must be an ast_t**.
 */
#define REPLACE(existing, ...) \
  { \
    ast_t* basis_ast = *existing; \
    ast_t* parent = NULL; \
    ast_t* last_sibling = NULL; \
    ast_t* node = NULL; \
    __VA_ARGS__ \
    ast_replace(existing, parent); \
  }

/** Add an existing subtree.
 * If the given tree is already part of another tree it will be copied
 * automatically. If it is a complete tree it will not.
 */
#define TREE(tree) \
  { \
    if(parent == NULL) parent = tree; \
    else if(last_sibling == NULL) last_sibling = ast_add(parent, tree); \
    else last_sibling = ast_add_sibling(last_sibling, tree); \
  }

/** Add an existing subtree, clearing the recorded pass reached.
 * If the given tree is already part of another tree it will be copied
 * automatically. If it is a complete tree it will not.
 */
#define TREE_CLEAR_PASS(tree) \
  { \
    if(ast_parent(tree) != NULL) tree = ast_dup(tree); \
    ast_resetpass(tree); \
    TREE(tree); \
  }

/// Add a new node with the specified token ID and optionally children
#define NODE(id, ...) NODE_ERROR_AT(id, basis_ast, __VA_ARGS__)

/// Add a new node with the specified token ID and optionally children with
/// error reports indicating the specified node
#define NODE_ERROR_AT(id, error_at, ...) \
  { \
    node = ast_from(error_at, id); \
    TREE(node); \
    { \
      ast_t* parent = node; \
      ast_t* last_sibling = NULL; \
      ast_t* node = NULL; \
      __VA_ARGS__ \
      ast_inheritflags(parent); \
      (void)parent; \
      (void)last_sibling; \
      (void)node; \
    } \
  }

/// Add a TK_NONE node with no children
#define NONE TREE(ast_from(basis_ast, TK_NONE));

/// Add a TK_ID node with the given ID name
#define ID(name) TREE(ast_from_string(basis_ast, name));

/// Add a TK_ID node with the given ID name and nice name
#define NICE_ID(name, nice) \
  TREE(ast_setdata(ast_from_string(basis_ast, name), (void*)stringtab(nice)));

/// Add a TK_STRING node with the given string value
#define STRING(value) \
  TREE(ast_setid(ast_from_string(basis_ast, value), TK_STRING));

/// Add a TK_INT node with the given integer value
#define INT(value) TREE(ast_from_int(basis_ast, value));

/// Add a symbol table to the enclosing node
#define AST_SCOPE ast_scope(parent);

/// Turn off debug info for the enclosing node
#define AST_NODEBUG ast_setdebug(parent, false);

/// Set the data field of the enclosing node
#define DATA(target) ast_setdata(parent, (void*)(target));


#endif
