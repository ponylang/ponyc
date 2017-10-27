#include "ast.h"
#include "frame.h"
#include "symtab.h"
#include "token.h"
#include "stringtab.h"
#include "printbuf.h"
#include "../expr/literal.h"
#include "../pass/pass.h"
#include "../pkg/program.h"
#include "../pkg/package.h"
#include "../../libponyrt/gc/serialise.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


/* Every AST node has a parent field, which points to the parent node when
 * there is one. However, sometimes we want a node to have no actual parent,
 * but still know the scope that the node is in. To allow this we use the
 * orphan flag. The field and flag meanings are as follows:
 *
 * Parent field  Orphan flag  Meaning
 * NULL          X            Node has no parent or scope
 * Non-NULL      true         Node has a scope, but no parent
 * Non-NULL      false        Node has a parent (which is also the scope)
 *
 * The parent field and orphan flag should always be considered as a unit. It
 * is STRONGLY recommended to only access them through the provided functions:
 *   hasparent()
 *   set_scope_and_parent()
 *   set_scope_no_parent()
 *   make_orphan_leave_scope()
 *   ast_parent()
 *   ast_set_scope()
 */

/* Every AST node has an annotation_type field, which points to the annotation
 * node when there is one. If there is a type node attached, it is in the
 * annotation_type field of the annotation, when there is one. If there is
 * no annotation, the type will be stored in the annotation_type field.
 *
 * These situations can be distinguished because an annotation must always be a
 * TK_ANNOTATION, and the type may never be a TK_ANNOTATION.
 *
 * It is STRONGLY recommended to only access them through provided functions:
 *   ast_type()
 *   settype()
 *   ast_annotation()
 *   setannotation()
 */

// The private bits of the flags values
enum
{
  AST_ORPHAN = 0x10,
  AST_INHERIT_FLAGS = (AST_FLAG_CAN_ERROR | AST_FLAG_CAN_SEND |
    AST_FLAG_MIGHT_SEND | AST_FLAG_RECURSE_1 | AST_FLAG_RECURSE_2),
  AST_ALL_FLAGS = 0x3FFFFF
};


struct ast_t
{
  token_t* t;
  symtab_t* symtab;
  void* data;
  ast_t* parent;
  ast_t* child;
  ast_t* sibling;
  ast_t* annotation_type;
  uint32_t flags;
};

// Minimal AST structure for signature computation.
typedef struct ast_signature_t
{
  token_signature_t* t;
  struct ast_signature_t* child;
  struct ast_signature_t* sibling;
  struct ast_signature_t* annotation_type;
} ast_signature_t;

static bool ast_cmp(ast_t* a, ast_t* b)
{
  return a == b;
}

DEFINE_LIST(astlist, astlist_t, ast_t, ast_cmp, NULL);

static const char in[] = "  ";
static const size_t in_len = 2;

enum print_special
{
  NOT_SPECIAL,
  SPECIAL_TYPE,
  SPECIAL_ANNOTATION
};

static const char special_char[] = {'(', ')', '[', ']', '\\', '\\'};

static void print(FILE* fp, ast_t* ast, size_t indent, enum print_special kind,
  size_t width);


static void print_token(FILE* fp, token_t* token)
{
  switch(token_get_id(token))
  {
    case TK_STRING:
    {
      char* escaped = token_print_escaped(token);
      fprintf(fp, "\"%s\"", escaped);
      ponyint_pool_free_size(strlen(escaped), escaped);
      break;
    }

    case TK_ID:
      fprintf(fp, "(id %s)", token_print(token));
      break;

    default:
      fprintf(fp, "%s", token_print(token));
      break;
  }
}

static size_t length(ast_t* ast, size_t indent, enum print_special kind)
{
  size_t len = (indent * in_len) + strlen(token_print(ast->t));
  ast_t* child = ast->child;

  if((kind != NOT_SPECIAL) || (child != NULL) || (ast_type(ast) != NULL) ||
    (ast_annotation(ast) != NULL))
    len += 2;

  switch(token_get_id(ast->t))
  {
    case TK_STRING: len += 6; break;
    case TK_ID: len += 5; break;
    default: {}
  }

  if(ast->symtab != NULL)
    len += 6;

  while(child != NULL)
  {
    len += 1 + length(child, 0, NOT_SPECIAL);
    child = child->sibling;
  }

  if(ast_type(ast) != NULL)
    len += 1 + length(ast_type(ast), 0, SPECIAL_TYPE);

  if(ast_annotation(ast) != NULL)
    len += 1 + length(ast_annotation(ast), 0, SPECIAL_ANNOTATION);

  return len;
}

static void print_compact(FILE* fp, ast_t* ast, size_t indent,
  enum print_special kind)
{
  for(size_t i = 0; i < indent; i++)
    fprintf(fp, in);

  ast_t* child = ast->child;
  bool parens = (kind != NOT_SPECIAL) || (child != NULL) ||
    (ast_type(ast) != NULL) || (ast_annotation(ast) != NULL);

  if(parens)
    fputc(special_char[kind * 2], fp);

  print_token(fp, ast->t);

  if(ast->symtab != NULL)
    fprintf(fp, ":scope");

  if(ast_annotation(ast) != NULL)
  {
    fprintf(fp, " ");
    print_compact(fp, ast_annotation(ast), 0, SPECIAL_ANNOTATION);
  }

  while(child != NULL)
  {
    fprintf(fp, " ");
    print_compact(fp, child, 0, NOT_SPECIAL);
    child = child->sibling;
  }

  if(ast_type(ast) != NULL)
  {
    fprintf(fp, " ");
    print_compact(fp, ast_type(ast), 0, SPECIAL_TYPE);
  }

  if(parens)
    fputc(special_char[(kind * 2) + 1], fp);
}

static void print_extended(FILE* fp, ast_t* ast, size_t indent,
  enum print_special kind, size_t width)
{
  for(size_t i = 0; i < indent; i++)
    fprintf(fp, in);

  ast_t* child = ast->child;
  bool parens = (kind != NOT_SPECIAL) || (child != NULL) ||
    (ast_type(ast) != NULL) || (ast_annotation(ast) != NULL);

  if(parens)
    fputc(special_char[kind * 2], fp);

  print_token(fp, ast->t);

  if(ast->symtab != NULL)
    fprintf(fp, ":scope");

  fprintf(fp, "\n");

  if(ast_annotation(ast) != NULL)
    print(fp, ast_annotation(ast), indent + 1, SPECIAL_ANNOTATION, width);

  while(child != NULL)
  {
    print(fp, child, indent + 1, NOT_SPECIAL, width);
    child = child->sibling;
  }

  if(ast_type(ast) != NULL)
    print(fp, ast_type(ast), indent + 1, SPECIAL_TYPE, width);

  if(parens)
  {
    for(size_t i = 0; i < indent; i++)
      fprintf(fp, in);

    fputc(special_char[(kind * 2) + 1], fp);
  }
}

static void print_verbose(FILE* fp, ast_t* ast, size_t indent,
  enum print_special kind)
{
  for(size_t i = 0; i < indent; i++)
    fprintf(fp, in);

  ast_t* child = ast->child;
  bool parens = (kind != NOT_SPECIAL) || (child != NULL) ||
    (ast_type(ast) != NULL) || (ast_annotation(ast) != NULL);

  if(parens)
    fputc(special_char[kind * 2], fp);

  print_token(fp, ast->t);
  fprintf(fp, ":%p,%0x", ast, ast->flags);

  if(ast->data != NULL)
    fprintf(fp, ":data=%p", ast->data);

  if(ast->symtab != NULL)
  {
    fprintf(fp, ":scope {\n");

    size_t i = HASHMAP_BEGIN;
    symbol_t* sym;

    while((sym = symtab_next(ast->symtab, &i)) != NULL)
      fprintf(fp, "  %s (%d): %p\n", sym->name, sym->status, sym->def);

    fprintf(fp, "}");
  }

  fprintf(fp, "\n");

  if(ast_annotation(ast) != NULL)
    print_verbose(fp, ast_annotation(ast), indent + 1, SPECIAL_ANNOTATION);

  while(child != NULL)
  {
    print_verbose(fp, child, indent + 1, NOT_SPECIAL);
    child = child->sibling;
  }

  if(ast_type(ast) != NULL)
    print_verbose(fp, ast_type(ast), indent + 1, SPECIAL_TYPE);

  if(parens)
  {
    for(size_t i = 0; i < indent; i++)
      fprintf(fp, in);

    fprintf(fp, "%c\n", special_char[(kind * 2) + 1]);
  }
}

static void print(FILE* fp, ast_t* ast, size_t indent, enum print_special kind,
  size_t width)
{
  size_t len = length(ast, indent, kind);

  if(len < width)
    print_compact(fp, ast, indent, kind);
  else
    print_extended(fp, ast, indent, kind, width);

  fprintf(fp, "\n");
}

// Report whether the given node has a valid parent
static bool hasparent(ast_t* ast)
{
  return !ast_checkflag(ast, AST_ORPHAN) && (ast->parent != NULL);
}

// Set both the parent and scope for the given node
static void set_scope_and_parent(ast_t* ast, ast_t* parent)
{
  pony_assert(ast != NULL);

  ast->parent = parent;
  ast_clearflag(ast, AST_ORPHAN);
}

// Set the scope for the given node, but set it no have no parent
static void set_scope_no_parent(ast_t* ast, ast_t* scope)
{
  pony_assert(ast != NULL);

  ast->parent = scope;
  ast_setflag(ast, AST_ORPHAN);
}

// Remove the given node's parent without changing its scope (if any)
static void make_orphan_leave_scope(ast_t* ast)
{
  ast_setflag(ast, AST_ORPHAN);
}

static ast_t* duplicate(ast_t* parent, ast_t* ast)
{
  if(ast == NULL)
    return NULL;

  pony_assert(ast_id(ast) != TK_PROGRAM && ast_id(ast) != TK_PACKAGE &&
    ast_id(ast) != TK_MODULE);

  ast_t* n = ast_token(token_dup(ast->t));
  n->data = ast->data;
  n->flags = ast->flags & AST_ALL_FLAGS;
  // We don't actually want to copy the orphan flag, but the following if
  // always explicitly sets or clears it.

  if(parent == NULL)
    set_scope_no_parent(n, ast->parent);
  else
    set_scope_and_parent(n, parent);

  n->child = duplicate(n, ast->child);

  ast_setannotation(n, duplicate(NULL, ast_annotation(ast)));
  ast_settype(n, duplicate(NULL, ast_type(ast)));

  if(ast->symtab != NULL)
    n->symtab = symtab_dup(ast->symtab);

  if(parent != NULL)
    n->sibling = duplicate(parent, ast->sibling);

  return n;
}

ast_t* ast_new(token_t* t, token_id id)
{
  return ast_token(token_dup_new_id(t, id));
}

ast_t* ast_blank(token_id id)
{
  return ast_token(token_new(id));
}

ast_t* ast_token(token_t* t)
{
  ast_t* ast = POOL_ALLOC(ast_t);
  memset(ast, 0, sizeof(ast_t));
  ast->t = t;

  switch(token_get_id(t))
  {
  case TK_PROGRAM:
    ast->data = program_create();
    break;

  default:
    break;
  }

  return ast;
}

ast_t* ast_from(ast_t* ast, token_id id)
{
  pony_assert(ast != NULL);
  ast_t* new_ast = ast_token(token_dup_new_id(ast->t, id));
  set_scope_no_parent(new_ast, ast->parent);
  return new_ast;
}

ast_t* ast_from_string(ast_t* ast, const char* name)
{
  if(name == NULL)
    return ast_from(ast, TK_NONE);

  token_t* t = token_dup(ast->t);
  token_set_id(t, TK_ID);
  token_set_string(t, name, 0);

  ast_t* new_ast = ast_token(t);
  set_scope_no_parent(new_ast, ast->parent);
  return new_ast;
}

ast_t* ast_from_int(ast_t* ast, uint64_t value)
{
  pony_assert(ast != NULL);
  token_t* t = token_dup(ast->t);
  token_set_id(t, TK_INT);

  lexint_t lexint = {value, 0};
  token_set_int(t, &lexint);

  ast_t* new_ast = ast_token(t);
  set_scope_no_parent(new_ast, ast->parent);
  return new_ast;
}

ast_t* ast_from_float(ast_t* ast, double value)
{
  pony_assert(ast != NULL);
  token_t* t = token_dup(ast->t);
  token_set_id(t, TK_FLOAT);
  token_set_float(t, value);

  ast_t* new_ast = ast_token(t);
  set_scope_no_parent(new_ast, ast->parent);
  return new_ast;
}

ast_t* ast_dup(ast_t* ast)
{
  return duplicate(NULL, ast);
}

void ast_scope(ast_t* ast)
{
  pony_assert(ast != NULL);
  ast->symtab = symtab_new();
}

bool ast_has_scope(ast_t* ast)
{
  pony_assert(ast != NULL);
  return ast->symtab != NULL;
}

void ast_set_scope(ast_t* ast, ast_t* scope)
{
  pony_assert(ast != NULL);
  pony_assert(!hasparent(ast));
  set_scope_no_parent(ast, scope);
}

symtab_t* ast_get_symtab(ast_t* ast)
{
  pony_assert(ast != NULL);
  return ast->symtab;
}

ast_t* ast_setid(ast_t* ast, token_id id)
{
  pony_assert(ast != NULL);
  token_set_id(ast->t, id);
  return ast;
}

void ast_setpos(ast_t* ast, source_t* source, size_t line, size_t pos)
{
  pony_assert(ast != NULL);
  token_set_pos(ast->t, source, line, pos);
}

token_id ast_id(ast_t* ast)
{
  pony_assert(ast != NULL);
  return token_get_id(ast->t);
}

size_t ast_line(ast_t* ast)
{
  pony_assert(ast != NULL);
  return token_line_number(ast->t);
}

size_t ast_pos(ast_t* ast)
{
  pony_assert(ast != NULL);
  return token_line_position(ast->t);
}

source_t* ast_source(ast_t* ast)
{
  pony_assert(ast != NULL);
  return token_source(ast->t);
}

void* ast_data(ast_t* ast)
{
  if(ast == NULL)
    return NULL;

  return ast->data;
}

ast_t* ast_setdata(ast_t* ast, void* data)
{
  pony_assert(ast != NULL);
  ast->data = data;
  return ast;
}

bool ast_canerror(ast_t* ast)
{
  return ast_checkflag(ast, AST_FLAG_CAN_ERROR) != 0;
}

void ast_seterror(ast_t* ast)
{
  ast_setflag(ast, AST_FLAG_CAN_ERROR);
}

bool ast_cansend(ast_t* ast)
{
  return ast_checkflag(ast, AST_FLAG_CAN_SEND) != 0;
}

void ast_setsend(ast_t* ast)
{
  ast_setflag(ast, AST_FLAG_CAN_SEND);
}

bool ast_mightsend(ast_t* ast)
{
  return ast_checkflag(ast, AST_FLAG_MIGHT_SEND) != 0;
}

void ast_setmightsend(ast_t* ast)
{
  ast_setflag(ast, AST_FLAG_MIGHT_SEND);
}

void ast_clearmightsend(ast_t* ast)
{
  ast_clearflag(ast, AST_FLAG_MIGHT_SEND);
}

void ast_inheritflags(ast_t* ast)
{
  pony_assert(ast != NULL);

  for(ast_t* child = ast->child; child != NULL; child = ast_sibling(child))
    ast_setflag(ast, child->flags & AST_INHERIT_FLAGS);
}

int ast_checkflag(ast_t* ast, uint32_t flag)
{
  pony_assert(ast != NULL);
  pony_assert((flag & AST_ALL_FLAGS) == flag);

  return ast->flags & flag;
}

void ast_setflag(ast_t* ast, uint32_t flag)
{
  pony_assert(ast != NULL);
  pony_assert((flag & AST_ALL_FLAGS) == flag);

  ast->flags |= flag;
}

void ast_clearflag(ast_t* ast, uint32_t flag)
{
  pony_assert(ast != NULL);
  pony_assert((flag & AST_ALL_FLAGS) == flag);

  ast->flags &= ~flag;
}

void ast_resetpass(ast_t* ast, uint32_t flag)
{
  pony_assert((flag & AST_FLAG_PASS_MASK) == flag);

  if(ast == NULL)
    return;

  if(ast_checkflag(ast, AST_FLAG_PRESERVE))
    return;

  ast_clearflag(ast, AST_FLAG_PASS_MASK);
  ast_setflag(ast, flag);
  ast_resetpass(ast_type(ast), flag);

  for(ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p))
    ast_resetpass(p, flag);
}

const char* ast_get_print(ast_t* ast)
{
  pony_assert(ast != NULL);
  return token_print(ast->t);
}

const char* ast_name(ast_t* ast)
{
  pony_assert(ast != NULL);
  return token_string(ast->t);
}


const char* ast_nice_name(ast_t* ast)
{
  pony_assert(ast != NULL);
  pony_assert(ast_id(ast) == TK_ID);

  if(ast->data != NULL)
    return (const char*)ast->data;

  return ast_name(ast);
}


size_t ast_name_len(ast_t* ast)
{
  pony_assert(ast != NULL);
  return token_string_len(ast->t);
}

void ast_set_name(ast_t* ast, const char* name)
{
  pony_assert(ast != NULL);
  token_set_string(ast->t, name, 0);
}

double ast_float(ast_t* ast)
{
  pony_assert(ast != NULL);
  return token_float(ast->t);
}

lexint_t* ast_int(ast_t* ast)
{
  pony_assert(ast != NULL);
  return token_int(ast->t);
}

ast_t* ast_type(ast_t* ast)
{
  pony_assert(ast != NULL);

  // An annotation may never have a type.
  if(ast_id(ast) == TK_ANNOTATION)
    return NULL;

  // If the annotation_type is an annotation, the type node (if any) is in the
  // annotation_type field of the annotation node.
  ast_t* type = ast->annotation_type;
  if((type != NULL) && (ast_id(type) == TK_ANNOTATION))
    type = type->annotation_type;

  return type;
}

static void settype(ast_t* ast, ast_t* type, bool allow_free)
{
  pony_assert(ast != NULL);

  // An annotation may never have a type.
  if(ast_id(ast) == TK_ANNOTATION)
    pony_assert(type == NULL);

  // A type can never be a TK_ANNOTATION.
  if(type != NULL)
    pony_assert(ast_id(type) != TK_ANNOTATION);

  ast_t* prev_type = ast_type(ast);
  if(prev_type == type)
    return;

  if(type != NULL)
  {
    if(hasparent(type))
      type = duplicate(ast, type);

    set_scope_and_parent(type, ast);
  }

  if((ast->annotation_type != NULL) &&
    (ast_id(ast->annotation_type) == TK_ANNOTATION))
    ast->annotation_type->annotation_type = type;
  else
    ast->annotation_type = type;

  if(allow_free)
    ast_free(prev_type);
}

void ast_settype(ast_t* ast, ast_t* type)
{
  settype(ast, type, true);
}

ast_t* ast_annotation(ast_t* ast)
{
  pony_assert(ast != NULL);

  // An annotation may never be annotated.
  if(ast_id(ast) == TK_ANNOTATION)
    return NULL;

  // If annotation_type is an annotation, we return it.
  ast_t* annotation = ast->annotation_type;
  if((annotation != NULL) && (ast_id(annotation) == TK_ANNOTATION))
    return annotation;

  return NULL;
}

void setannotation(ast_t* ast, ast_t* annotation, bool allow_free)
{
  pony_assert(ast != NULL);

  // An annotation may never be annotated.
  if(ast_id(ast) == TK_ANNOTATION)
    pony_assert(annotation == NULL);

  // An annotation must always be a TK_ANNOTATION (or NULL).
  if(annotation != NULL)
    pony_assert(ast_id(annotation) == TK_ANNOTATION);

  ast_t* prev_annotation = ast_annotation(ast);
  if(prev_annotation == annotation)
    return;

  if(annotation != NULL)
  {
    pony_assert(!hasparent(annotation) &&
      (annotation->annotation_type == NULL));

    if(prev_annotation != NULL)
    {
      annotation->annotation_type = prev_annotation->annotation_type;
      prev_annotation->annotation_type = NULL;
    } else {
      annotation->annotation_type = ast->annotation_type;
    }

    ast->annotation_type = annotation;
  } else {
    pony_assert(prev_annotation != NULL);

    ast->annotation_type = prev_annotation->annotation_type;
    prev_annotation->annotation_type = NULL;
  }

  if(allow_free)
    ast_free(prev_annotation);
}

void ast_setannotation(ast_t* ast, ast_t* annotation)
{
  setannotation(ast, annotation, true);
}

ast_t* ast_consumeannotation(ast_t* ast)
{
  ast_t* prev_annotation = ast_annotation(ast);

  setannotation(ast, NULL, false);

  return prev_annotation;
}

bool ast_has_annotation(ast_t* ast, const char* name)
{
  pony_assert(ast != NULL);

  ast_t* annotation = ast_annotation(ast);

  if((annotation != NULL) && (ast_id(annotation) == TK_ANNOTATION))
  {
    const char* strtab_name = stringtab(name);
    ast_t* elem = ast_child(annotation);
    while(elem != NULL)
    {
      if(ast_name(elem) == strtab_name)
        return true;
      elem = ast_sibling(elem);
    }
  }

  return false;
}

void ast_erase(ast_t* ast)
{
  pony_assert(ast != NULL);

  ast_t* child = ast->child;

  while(child != NULL)
  {
    ast_t* next = child->sibling;
    ast_free(child);
    child = next;
  }

  ast->child = NULL;
  token_set_id(ast->t, TK_NONE);
}

ast_t* ast_nearest(ast_t* ast, token_id id)
{
  while((ast != NULL) && (token_get_id(ast->t) != id))
    ast = ast->parent;

  return ast;
}

ast_t* ast_try_clause(ast_t* ast, size_t* clause)
{
  ast_t* last = NULL;

  while(ast != NULL)
  {
    switch(token_get_id(ast->t))
    {
      case TK_TRY:
      case TK_TRY_NO_CHECK:
      {
        *clause = ast_index(last);
        return ast;
      }

      default: {}
    }

    last = ast;
    ast = ast_parent(ast);
  }

  return NULL;
}

ast_t* ast_parent(ast_t* ast)
{
  if(ast_checkflag(ast, AST_ORPHAN))
    return NULL;

  return ast->parent;
}

ast_t* ast_child(ast_t* ast)
{
  pony_assert(ast != NULL);
  return ast->child;
}

ast_t* ast_childidx(ast_t* ast, size_t idx)
{
  pony_assert(ast != NULL);
  ast_t* child = ast->child;

  while((child != NULL) && (idx > 0))
  {
    child = child->sibling;
    idx--;
  }

  return child;
}

ast_t* ast_childlast(ast_t* ast)
{
  ast_t* child = ast->child;

  if(child == NULL)
    return NULL;

  while(child->sibling != NULL)
    child = child->sibling;

  return child;
}

size_t ast_childcount(ast_t* ast)
{
  ast_t* child = ast->child;
  size_t count = 0;

  while(child != NULL)
  {
    count++;
    child = child->sibling;
  }

  return count;

}

ast_t* ast_sibling(ast_t* ast)
{
  return ast->sibling;
}

ast_t* ast_previous(ast_t* ast)
{
  pony_assert(ast != NULL);
  pony_assert(hasparent(ast));

  ast_t* last = NULL;
  ast_t* cur = ast->parent->child;

  while(cur != ast)
  {
    last = cur;
    cur = cur->sibling;
  }

  return last;
}

size_t ast_index(ast_t* ast)
{
  pony_assert(ast != NULL);
  pony_assert(hasparent(ast));

  ast_t* child = ast->parent->child;
  size_t idx = 0;

  while(child != ast)
  {
    child = child->sibling;
    idx++;
  }

  return idx;
}

ast_t* ast_get(ast_t* ast, const char* name, sym_status_t* status)
{
  // Searches all parent scopes, but not the program scope, because the name
  // space for paths is separate from the name space for all other IDs.
  // If called directly on the program scope, searches it.
  if(status != NULL)
    *status = SYM_NONE;

  do
  {
    if(ast->symtab != NULL)
    {
      sym_status_t status2;
      ast_t* value = (ast_t*)symtab_find(ast->symtab, name, &status2);

      if((status != NULL) && (*status == SYM_NONE))
        *status = status2;

      if(value != NULL)
        return value;
    }

    ast = ast->parent;
  } while((ast != NULL) && (token_get_id(ast->t) != TK_PROGRAM));

  return NULL;
}

ast_t* ast_get_case(ast_t* ast, const char* name, sym_status_t* status)
{
  // Same as ast_get, but is partially case insensitive. That is, type names
  // are compared as uppercase and other symbols are compared as lowercase.
  if(status != NULL)
    *status = SYM_NONE;

  do
  {
    if(ast->symtab != NULL)
    {
      sym_status_t status2;
      ast_t* value = (ast_t*)symtab_find_case(ast->symtab, name, &status2);

      if((status != NULL) && (*status == SYM_NONE))
        *status = status2;

      if(value != NULL)
        return value;
    }

    ast = ast->parent;
  } while((ast != NULL) && (token_get_id(ast->t) != TK_PROGRAM));

  return NULL;
}

bool ast_set(ast_t* ast, const char* name, ast_t* value, sym_status_t status,
  bool allow_shadowing)
{
  while(ast->symtab == NULL)
    ast = ast->parent;

  ast_t* find;

  if(allow_shadowing)
  {
    // Only check the local scope.
    find = symtab_find_case(ast->symtab, name, NULL);
  } else {
    // Check the local scope and all parent scopes.
    find = ast_get_case(ast, name, NULL);
  }

  // Pretend we succeeded if the mapping in the symbol table wouldn't change.
  if(find == value)
    return true;

  if(find != NULL)
    return false;

  return symtab_add(ast->symtab, name, value, status);
}

void ast_setstatus(ast_t* ast, const char* name, sym_status_t status)
{
  while(ast->symtab == NULL)
    ast = ast->parent;

  symtab_set_status(ast->symtab, name, status);
}

void ast_inheritstatus(ast_t* dst, ast_t* src)
{
  if(dst == NULL)
    return;

  while(dst->symtab == NULL)
    dst = dst->parent;

  symtab_inherit_status(dst->symtab, src->symtab);
}

void ast_inheritbranch(ast_t* dst, ast_t* src)
{
  pony_assert(dst->symtab != NULL);
  pony_assert(src->symtab != NULL);

  symtab_inherit_branch(dst->symtab, src->symtab);
}

void ast_consolidate_branches(ast_t* ast, size_t count)
{
  pony_assert(hasparent(ast));

  size_t i = HASHMAP_BEGIN;
  symbol_t* sym;

  while((sym = symtab_next(ast->symtab, &i)) != NULL)
  {
    sym_status_t status;
    ast_get(ast->parent, sym->name, &status);

    if(sym->status == SYM_UNDEFINED)
    {
      pony_assert(sym->branch_count <= count);

      if((status == SYM_DEFINED) || (sym->branch_count == count))
      {
        // If we see it as defined from a parent scope, always end up defined.
        // If we it's defined in all the branched, it's also defined.
        sym->status = SYM_DEFINED;
      } else {
        // It wasn't defined in all branches. Stay undefined.
        sym->branch_count = 0;
      }
    }
  }
}

bool ast_canmerge(ast_t* dst, ast_t* src)
{
  while(dst->symtab == NULL)
    dst = dst->parent;

  return symtab_can_merge_public(dst->symtab, src->symtab);
}

bool ast_merge(ast_t* dst, ast_t* src)
{
  while(dst->symtab == NULL)
    dst = dst->parent;

  return symtab_merge_public(dst->symtab, src->symtab);
}

bool ast_within_scope(ast_t* outer, ast_t* inner, const char* name)
{
  do
  {
    if(inner->symtab != NULL)
    {
      sym_status_t status2;
      ast_t* value = (ast_t*)symtab_find(inner->symtab, name, &status2);

      if(value != NULL)
        return true;
    }

    if(inner == outer)
      break;

    inner = inner->parent;
  } while((inner != NULL) && (token_get_id(inner->t) != TK_PROGRAM));

  return false;
}

bool ast_all_consumes_in_scope(ast_t* outer, ast_t* inner, errorframe_t* errorf)
{
  ast_t* from = inner;
  bool ok = true;

  do
  {
    if(inner->symtab != NULL)
    {
      size_t i = HASHMAP_BEGIN;
      symbol_t* sym;

      while((sym = symtab_next(inner->symtab, &i)) != NULL)
      {
        // If it's consumed, and has a null value, it's from outside of this
        // scope. We need to know if it's outside the loop scope.
        if((sym->status == SYM_CONSUMED) && (sym->def == NULL))
        {
          if(!ast_within_scope(outer, inner, sym->name))
          {
            ast_t* def = ast_get(inner, sym->name, NULL);
            if(errorf != NULL)
            {
              ast_error_frame(errorf, from,
                "identifier '%s' is consumed when the loop exits", sym->name);
              ast_error_frame(errorf, def,
                "consumed identifier is defined here");
            }
            ok = false;
          }
        }
      }
    }

    if(inner == outer)
      break;

    inner = inner->parent;
  } while((inner != NULL) && (token_get_id(inner->t) != TK_PROGRAM));

  return ok;
}

void ast_clear(ast_t* ast)
{
  if(ast->symtab != NULL)
  {
    symtab_free(ast->symtab);
    ast_scope(ast);
  }

  ast_t* child = ast->child;

  while(child != NULL)
  {
    ast_clear(child);
    child = child->sibling;
  }
}

void ast_clear_local(ast_t* ast)
{
  if(ast->symtab != NULL)
  {
    symtab_free(ast->symtab);
    ast_scope(ast);
  }
}

ast_t* ast_add(ast_t* parent, ast_t* child)
{
  pony_assert(parent != NULL);
  pony_assert(parent != child);
  pony_assert(parent->child != child);

  if(hasparent(child))
    child = ast_dup(child);

  set_scope_and_parent(child, parent);
  child->sibling = parent->child;
  parent->child = child;
  return child;
}

ast_t* ast_add_sibling(ast_t* older_sibling, ast_t* new_sibling)
{
  pony_assert(older_sibling != NULL);
  pony_assert(new_sibling != NULL);
  pony_assert(older_sibling != new_sibling);
  pony_assert(hasparent(older_sibling));

  if(hasparent(new_sibling))
    new_sibling = ast_dup(new_sibling);

  pony_assert(new_sibling->sibling == NULL);

  set_scope_and_parent(new_sibling, older_sibling->parent);
  new_sibling->sibling = older_sibling->sibling;
  older_sibling->sibling = new_sibling;
  return new_sibling;
}

ast_t* ast_pop(ast_t* parent)
{
  ast_t* child = parent->child;

  if(child != NULL)
  {
    parent->child = child->sibling;
    child->sibling = NULL;
    make_orphan_leave_scope(child);
  }

  return child;
}

ast_t* ast_append(ast_t* parent, ast_t* child)
{
  pony_assert(parent != NULL);
  pony_assert(child != NULL);
  pony_assert(parent != child);

  if(hasparent(child))
    child = ast_dup(child);

  set_scope_and_parent(child, parent);

  if(parent->child == NULL)
  {
    parent->child = child;
    return child;
  }

  ast_t* ast = parent->child;

  while(ast->sibling != NULL)
    ast = ast->sibling;

  ast->sibling = child;
  return child;
}

ast_t* ast_list_append(ast_t* parent, ast_t** last_pointer, ast_t* new_child)
{
  pony_assert(parent != NULL);
  pony_assert(last_pointer != NULL);
  pony_assert(new_child != NULL);

  if(*last_pointer == NULL)
  {
    // This is the first child for the parent
    pony_assert(parent->child == NULL);
    *last_pointer = ast_add(parent, new_child);
  }
  else
  {
    // This is not the first child, append to list
    pony_assert(parent->child != NULL);
    pony_assert((*last_pointer)->sibling == NULL);
    *last_pointer = ast_add_sibling(*last_pointer, new_child);
  }

  return *last_pointer;
}

void ast_remove(ast_t* ast)
{
  pony_assert(ast != NULL);
  pony_assert(hasparent(ast));

  ast_t* last = ast_previous(ast);

  if(last != NULL)
    last->sibling = ast->sibling;
  else
    ast->parent->child = ast->sibling;

  ast->parent = NULL;
  ast->sibling = NULL;
  ast_free(ast);
}

void ast_swap(ast_t* prev, ast_t* next)
{
  pony_assert(prev != NULL);
  pony_assert(prev != next);

  ast_t* parent = ast_parent(prev);
  pony_assert(parent != NULL);

  if(hasparent(next))
    next = ast_dup(next);

  if(ast_type(parent) == prev)
  {
    settype(parent, next, false);
  } else {
    ast_t* last = ast_previous(prev);

    if(last != NULL)
      last->sibling = next;
    else
      parent->child = next;

    next->sibling = prev->sibling;
  }

  set_scope_and_parent(next, parent);

  prev->sibling = NULL;
  make_orphan_leave_scope(prev);
}

void ast_replace(ast_t** prev, ast_t* next)
{
  if(*prev == next)
    return;

  if(hasparent(next))
    next = ast_dup(next);

  if(hasparent(*prev))
    ast_swap(*prev, next);

  ast_free(*prev);
  *prev = next;
}

void ast_reorder_children(ast_t* ast, const size_t* new_order,
  ast_t** shuffle_space)
{
  pony_assert(ast != NULL);
  pony_assert(new_order != NULL);
  pony_assert(shuffle_space != NULL);

  size_t count = ast_childcount(ast);

  for(size_t i = 0; i < count; i++)
    shuffle_space[i] = ast_pop(ast);

  for(size_t i = 0; i < count; i++)
  {
    size_t index = new_order[i];
    pony_assert(index < count);
    ast_t* t = shuffle_space[index];
    pony_assert(t != NULL);
    ast_append(ast, t);
    shuffle_space[index] = NULL;
  }
}

void ast_free(ast_t* ast)
{
  if(ast == NULL)
    return;

  ast_t* child = ast->child;
  ast_t* next;

  while(child != NULL)
  {
    next = child->sibling;
    ast_free(child);
    child = next;
  }

  ast_settype(ast, NULL);
  ast_setannotation(ast, NULL);

  switch(token_get_id(ast->t))
  {
    case TK_PROGRAM:
      program_free((program_t*)ast->data);
      break;

    case TK_PACKAGE:
      package_free((package_t*)ast->data);
      break;

    case TK_MODULE:
      source_close((source_t*)ast->data);
      break;

    default:
      break;
  }

  token_free(ast->t);
  symtab_free(ast->symtab);
  POOL_FREE(ast_t, ast);
}

void ast_free_unattached(ast_t* ast)
{
  if((ast != NULL) && !hasparent(ast))
    ast_free(ast);
}

void ast_print(ast_t* ast, size_t width)
{
  ast_fprint(stdout, ast, width);
}

void ast_printverbose(ast_t* ast)
{
  ast_fprintverbose(stdout, ast);
}

void ast_fprint(FILE* fp, ast_t* ast, size_t width)
{
  if(ast == NULL)
    return;

  print(fp, ast, 0, NOT_SPECIAL, width);
  fprintf(fp, "\n");
}

void ast_fprintverbose(FILE* fp, ast_t* ast)
{
  if(ast == NULL)
    return;

  print_verbose(fp, ast, 0, NOT_SPECIAL);
}

static void print_type(printbuf_t* buffer, ast_t* type);

static void print_typeexpr(printbuf_t* buffer, ast_t* type, const char* sep,
  bool square)
{
  if(square)
    printbuf(buffer, "[");
  else
    printbuf(buffer, "(");

  ast_t* child = ast_child(type);

  while(child != NULL)
  {
    ast_t* next = ast_sibling(child);
    print_type(buffer, child);

    if(next != NULL)
      printbuf(buffer, "%s", sep);

    child = next;
  }

  if(square)
    printbuf(buffer, "]");
  else
    printbuf(buffer, ")");
}

static void print_type(printbuf_t* buffer, ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_NOMINAL:
    {
      AST_GET_CHILDREN(type, package, id, typeargs, cap, ephemeral);
      ast_t* origpkg = ast_sibling(ephemeral);

      if(origpkg != NULL && ast_id(origpkg) != TK_NONE)
        printbuf(buffer, "%s.", ast_name(origpkg));

      ast_t* def = (ast_t*)ast_data(type);
      if(def != NULL)
        id = ast_child(def);

      printbuf(buffer, "%s", ast_nice_name(id));

      if(ast_id(typeargs) != TK_NONE)
        print_typeexpr(buffer, typeargs, ", ", true);

      if(ast_id(cap) != TK_NONE)
        printbuf(buffer, " %s", token_print(cap->t));

      if(ast_id(ephemeral) != TK_NONE)
        printbuf(buffer, "%s", token_print(ephemeral->t));

      break;
    }

    case TK_UNIONTYPE:
      print_typeexpr(buffer, type, " | ", false);
      break;

    case TK_ISECTTYPE:
      print_typeexpr(buffer, type, " & ", false);
      break;

    case TK_TUPLETYPE:
      print_typeexpr(buffer, type, ", ", false);
      break;

    case TK_TYPEPARAMREF:
    {
      AST_GET_CHILDREN(type, id, cap, ephemeral);
      printbuf(buffer, "%s", ast_nice_name(id));

      if(ast_id(cap) != TK_NONE)
        printbuf(buffer, " %s", token_print(cap->t));

      if(ast_id(ephemeral) != TK_NONE)
        printbuf(buffer, " %s", token_print(ephemeral->t));

      break;
    }

    case TK_ARROW:
    {
      AST_GET_CHILDREN(type, left, right);
      print_type(buffer, left);
      printbuf(buffer, "->");
      print_type(buffer, right);
      break;
    }

    case TK_THISTYPE:
      printbuf(buffer, "this");
      break;

    case TK_DONTCARETYPE:
      printbuf(buffer, "_");
      break;

    case TK_FUNTYPE:
      printbuf(buffer, "function");
      break;

    case TK_INFERTYPE:
      printbuf(buffer, "to_infer");
      break;

    case TK_ERRORTYPE:
      printbuf(buffer, "<type error>");
      break;

    case TK_NONE:
      break;

    default:
      printbuf(buffer, "%s", token_print(type->t));
  }
}

const char* ast_print_type(ast_t* type)
{
  printbuf_t* buffer = printbuf_new();
  print_type(buffer, type);

  const char* s = stringtab(buffer->m);
  printbuf_free(buffer);

  return s;
}

void ast_error(errors_t* errors, ast_t* ast, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  errorv(errors, token_source(ast->t), token_line_number(ast->t),
    token_line_position(ast->t), fmt, ap);
  va_end(ap);
}

void ast_error_continue(errors_t* errors, ast_t* ast, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  errorv_continue(errors, token_source(ast->t), token_line_number(ast->t),
    token_line_position(ast->t), fmt, ap);
  va_end(ap);
}

void ast_error_frame(errorframe_t* frame, ast_t* ast, const char* fmt, ...)
{
  pony_assert(frame != NULL);
  pony_assert(ast != NULL);
  pony_assert(fmt != NULL);

  va_list ap;
  va_start(ap, fmt);
  errorframev(frame, token_source(ast->t), token_line_number(ast->t),
    token_line_position(ast->t), fmt, ap);
  va_end(ap);
}

void ast_get_children(ast_t* parent, size_t child_count,
  ast_t*** out_children)
{
  pony_assert(parent != NULL);
  pony_assert(child_count > 0);
  pony_assert(out_children != NULL);

  ast_t* p = parent->child;

  for(size_t i = 0; i < child_count; i++)
  {
    pony_assert(p != NULL);

    if(out_children[i] != NULL)
      *(out_children[i]) = p;

    p = p->sibling;
  }
}

void ast_extract_children(ast_t* parent, size_t child_count,
  ast_t*** out_children)
{
  pony_assert(parent != NULL);
  pony_assert(child_count > 0);
  pony_assert(out_children != NULL);

  ast_t* p = parent->child;
  ast_t* last_remaining_sibling = NULL;

  for(size_t i = 0; i < child_count; i++)
  {
    pony_assert(p != NULL);
    ast_t* next = p->sibling;

    if(out_children[i] != NULL)
    {
      if(last_remaining_sibling != NULL)
        last_remaining_sibling->sibling = p->sibling;
      else
        parent->child = p->sibling;

      make_orphan_leave_scope(p);
      p->sibling = NULL;
      *(out_children[i]) = p;
    }
    else
    {
      last_remaining_sibling = p;
    }

    p = next;
  }
}

static void ast_signature_serialise_trace(pony_ctx_t* ctx, void* object)
{
  ast_t* ast = (ast_t*)object;

  // Ignore the data. We don't want to cross package boundaries.
  // The symtab, parent and type don't provide additional information to the
  // signature so we ignore them as well.

  token_id id = ast_id(ast);
  bool docstring = false;

  if(id == TK_STRING)
  {
    switch(ast_id(ast_parent(ast)))
    {
      case TK_MODULE:
      case TK_NEW:
      case TK_FUN:
      case TK_BE:
      case TK_TYPE:
      case TK_INTERFACE:
      case TK_TRAIT:
      case TK_PRIMITIVE:
      case TK_STRUCT:
      case TK_CLASS:
      case TK_ACTOR:
        docstring = true;
        break;

      default: {}
    }
  }

  pony_traceknown(ctx, ast->t, docstring ?
    token_docstring_signature_pony_type() : token_signature_pony_type(),
    PONY_TRACE_MUTABLE);

  if(id == TK_NOMINAL)
  {
    pony_assert(ast->child != NULL);

    pony_traceknown(ctx, ast->child, (ast_id(ast->child) == TK_ID) ?
      ast_nominal_pkg_id_signature_pony_type() : ast_signature_pony_type(),
      PONY_TRACE_MUTABLE);
  } else if(ast->child != NULL) {
    pony_traceknown(ctx, ast->child, ast_signature_pony_type(),
      PONY_TRACE_MUTABLE);
  }

  if((id != TK_PACKAGE) && (ast->sibling != NULL))
    pony_traceknown(ctx, ast->sibling, ast_signature_pony_type(),
      PONY_TRACE_MUTABLE);

  // Don't use ast->annotation_type directly. It could be a type, and we don't
  // want to serialise types.
  ast_t* annotation = ast_annotation(ast);

  if(annotation != NULL)
    pony_traceknown(ctx, annotation, ast_signature_pony_type(),
      PONY_TRACE_MUTABLE);
}

static void ast_signature_serialise(pony_ctx_t* ctx, void* object, void* buf,
  size_t offset, int mutability)
{
  (void)mutability;

  ast_t* ast = (ast_t*)object;
  ast_signature_t* dst = (ast_signature_t*)((uintptr_t)buf + offset);

  dst->t = (token_signature_t*)pony_serialise_offset(ctx, ast->t);
  dst->child = (ast_signature_t*)pony_serialise_offset(ctx, ast->child);

  if(ast_id(ast) != TK_PACKAGE)
    dst->sibling = (ast_signature_t*)pony_serialise_offset(ctx, ast->sibling);
  else
    dst->sibling = NULL;

  ast_t* annotation = ast_annotation(ast);

  dst->annotation_type = (ast_signature_t*)pony_serialise_offset(ctx,
    annotation);
}

static pony_type_t ast_signature_pony =
{
  0,
  sizeof(ast_signature_t),
  0,
  0,
  NULL,
  NULL,
  ast_signature_serialise_trace,
  ast_signature_serialise,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};

pony_type_t* ast_signature_pony_type()
{
  return &ast_signature_pony;
}

// Special case serialisation for package IDs nodes as TK_NOMINAL children.
static void ast_nominal_pkg_id_signature_serialise_trace(pony_ctx_t* ctx,
  void* object)
{
  ast_t* ast = (ast_t*)object;

  pony_assert(ast_id(ast) == TK_ID);
  ast_t* parent = ast->parent;
  (void)parent;
  pony_assert((parent != NULL) && (ast_id(parent) == TK_NOMINAL) &&
    (ast == parent->child));

  // Ignore the token. We'll setup a fake token directly referencing the
  // associated package later.

  pony_assert(ast->child == NULL);
  pony_assert(ast->sibling != NULL);

  pony_traceknown(ctx, ast->sibling, ast_signature_pony_type(),
    PONY_TRACE_MUTABLE);

  ast_t* annotation = ast_annotation(ast);

  if(annotation != NULL)
    pony_traceknown(ctx, annotation, ast_signature_pony_type(),
      PONY_TRACE_MUTABLE);
}

static void ast_nominal_pkg_id_signature_serialise(pony_ctx_t* ctx,
  void* object, void* buf, size_t offset, int mutability)
{
  (void)mutability;

  ast_t* ast = (ast_t*)object;
  ast_signature_t* dst = (ast_signature_t*)((uintptr_t)buf + offset);

  ast_t* def = (ast_t*)ast_data(ast_parent(ast));
  pony_assert(def != NULL);
  ast_t* pkg_ast = ast_nearest(def, TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(pkg_ast);
  pony_assert(pkg != NULL);
  dst->t = (token_signature_t*)pony_serialise_offset(ctx, pkg);

  dst->child = (ast_signature_t*)pony_serialise_offset(ctx, ast->child);
  dst->sibling = (ast_signature_t*)pony_serialise_offset(ctx, ast->sibling);

  ast_t* annotation = ast_annotation(ast);

  dst->annotation_type = (ast_signature_t*)pony_serialise_offset(ctx,
    annotation);
}

static pony_type_t ast_nominal_pkg_id_signature_pony =
{
  0,
  sizeof(ast_signature_t),
  0,
  0,
  NULL,
  NULL,
  ast_nominal_pkg_id_signature_serialise_trace,
  ast_nominal_pkg_id_signature_serialise,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};

pony_type_t* ast_nominal_pkg_id_signature_pony_type()
{
  return &ast_nominal_pkg_id_signature_pony;
}

static void ast_serialise_trace_data(pony_ctx_t* ctx, ast_t* ast)
{
  if(ast->data == NULL)
    return;

  switch(ast_id(ast))
  {
    case TK_USE:
    case TK_NOMINAL:
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    case TK_TYPEPARAM:
    case TK_TYPEPARAMREF:
    case TK_REFERENCE:
    case TK_PACKAGEREF:
    case TK_TYPEREF:
    case TK_PARAMREF:
    case TK_VARREF:
    case TK_LETREF:
    case TK_FVARREF:
    case TK_FLETREF:
    case TK_EMBEDREF:
    case TK_NEWREF:
    case TK_NEWBEREF:
    case TK_BEREF:
    case TK_FUNREF:
    case TK_NEWAPP:
    case TK_BEAPP:
    case TK_FUNAPP:
    case TK_BECHAIN:
    case TK_FUNCHAIN:
    case TK_DOT:
    case TK_FFICALL:
    case TK_LITERALBRANCH:
      pony_traceknown(ctx, ast->data, ast_pony_type(), PONY_TRACE_MUTABLE);
      break;

    case TK_ID:
    case TK_OBJECT:
      string_trace(ctx, (const char*)ast->data);
      break;

    case TK_PROGRAM:
      pony_traceknown(ctx, ast->data, program_pony_type(), PONY_TRACE_MUTABLE);
      break;

    case TK_PACKAGE:
      pony_traceknown(ctx, ast->data, package_pony_type(), PONY_TRACE_MUTABLE);
      break;

    case TK_MODULE:
      pony_traceknown(ctx, ast->data, source_pony_type(), PONY_TRACE_MUTABLE);
      break;

    default: {}
  }
}

static void ast_serialise_data(pony_ctx_t* ctx, ast_t* ast, ast_t* dst)
{
  switch(ast_id(ast))
  {
    case TK_USE:
    case TK_NOMINAL:
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    case TK_TYPEPARAM:
    case TK_TYPEPARAMREF:
    case TK_REFERENCE:
    case TK_PACKAGEREF:
    case TK_TYPEREF:
    case TK_PARAMREF:
    case TK_VARREF:
    case TK_LETREF:
    case TK_FVARREF:
    case TK_FLETREF:
    case TK_EMBEDREF:
    case TK_NEWREF:
    case TK_NEWBEREF:
    case TK_BEREF:
    case TK_FUNREF:
    case TK_NEWAPP:
    case TK_BEAPP:
    case TK_FUNAPP:
    case TK_BECHAIN:
    case TK_FUNCHAIN:
    case TK_DOT:
    case TK_FFICALL:
    case TK_LITERALBRANCH:
    case TK_ID:
    case TK_OBJECT:
    case TK_PROGRAM:
    case TK_PACKAGE:
    case TK_MODULE:
      dst->data = (void*)pony_serialise_offset(ctx, ast->data);
      break;

    case TK_OPERATORLITERAL:
      operatorliteral_serialise_data(ast, dst);
      break;

    default:
      dst->data = NULL;
      break;
  }
}

static void ast_deserialise_data(pony_ctx_t* ctx, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_USE:
    case TK_NOMINAL:
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    case TK_TYPEPARAM:
    case TK_TYPEPARAMREF:
    case TK_REFERENCE:
    case TK_PACKAGEREF:
    case TK_TYPEREF:
    case TK_PARAMREF:
    case TK_VARREF:
    case TK_LETREF:
    case TK_FVARREF:
    case TK_FLETREF:
    case TK_EMBEDREF:
    case TK_NEWREF:
    case TK_NEWBEREF:
    case TK_BEREF:
    case TK_FUNREF:
    case TK_NEWAPP:
    case TK_BEAPP:
    case TK_FUNAPP:
    case TK_BECHAIN:
    case TK_FUNCHAIN:
    case TK_DOT:
    case TK_FFICALL:
    case TK_LITERALBRANCH:
      ast->data = pony_deserialise_offset(ctx, ast_pony_type(),
        (uintptr_t)ast->data);
      break;

    case TK_ID:
    case TK_OBJECT:
      ast->data = (void*)string_deserialise_offset(ctx, (uintptr_t)ast->data);
      break;

    case TK_PROGRAM:
      ast->data = pony_deserialise_offset(ctx, program_pony_type(),
        (uintptr_t)ast->data);
      break;

    case TK_PACKAGE:
      ast->data = pony_deserialise_offset(ctx, package_pony_type(),
        (uintptr_t)ast->data);
      break;

    case TK_MODULE:
      ast->data = pony_deserialise_offset(ctx, source_pony_type(),
        (uintptr_t)ast->data);
      break;

    case TK_OPERATORLITERAL:
      operatorliteral_deserialise_data(ast);
      break;

    default: {}
  }
}

static void ast_serialise_trace(pony_ctx_t* ctx, void* object)
{
  ast_t* ast = (ast_t*)object;

  pony_traceknown(ctx, ast->t, token_pony_type(), PONY_TRACE_MUTABLE);
  ast_serialise_trace_data(ctx, ast);

  if(ast->symtab != NULL)
    pony_traceknown(ctx, ast->symtab, symtab_pony_type(), PONY_TRACE_MUTABLE);

  if(ast->parent != NULL)
    pony_traceknown(ctx, ast->parent, ast_pony_type(), PONY_TRACE_MUTABLE);

  if(ast->child != NULL)
    pony_traceknown(ctx, ast->child, ast_pony_type(), PONY_TRACE_MUTABLE);

  if(ast->sibling != NULL)
    pony_traceknown(ctx, ast->sibling, ast_pony_type(), PONY_TRACE_MUTABLE);

  if(ast->annotation_type != NULL)
    pony_traceknown(ctx, ast->annotation_type, ast_pony_type(), PONY_TRACE_MUTABLE);
}

static void ast_serialise(pony_ctx_t* ctx, void* object, void* buf,
  size_t offset, int mutability)
{
  (void)mutability;

  ast_t* ast = (ast_t*)object;
  ast_t* dst = (ast_t*)((uintptr_t)buf + offset);

  dst->t = (token_t*)pony_serialise_offset(ctx, ast->t);
  ast_serialise_data(ctx, ast, dst);
  dst->symtab = (symtab_t*)pony_serialise_offset(ctx, ast->symtab);
  dst->parent = (ast_t*)pony_serialise_offset(ctx, ast->parent);
  dst->child = (ast_t*)pony_serialise_offset(ctx, ast->child);
  dst->sibling = (ast_t*)pony_serialise_offset(ctx, ast->sibling);
  dst->annotation_type = (ast_t*)pony_serialise_offset(ctx, ast->annotation_type);
  dst->flags = ast->flags;
}

static void ast_deserialise(pony_ctx_t* ctx, void* object)
{
  ast_t* ast = (ast_t*)object;

  ast->t = (token_t*)pony_deserialise_offset(ctx, token_pony_type(),
    (uintptr_t)ast->t);
  ast_deserialise_data(ctx, ast);
  ast->symtab = (symtab_t*)pony_deserialise_offset(ctx, symtab_pony_type(),
    (uintptr_t)ast->symtab);
  ast->parent = (ast_t*)pony_deserialise_offset(ctx, ast_pony_type(),
    (uintptr_t)ast->parent);
  ast->child = (ast_t*)pony_deserialise_offset(ctx, ast_pony_type(),
    (uintptr_t)ast->child);
  ast->sibling = (ast_t*)pony_deserialise_offset(ctx, ast_pony_type(),
    (uintptr_t)ast->sibling);
  ast->annotation_type = (ast_t*)pony_deserialise_offset(ctx, ast_pony_type(),
    (uintptr_t)ast->annotation_type);
}

static pony_type_t ast_pony =
{
  0,
  sizeof(ast_t),
  0,
  0,
  NULL,
  NULL,
  ast_serialise_trace,
  ast_serialise,
  ast_deserialise,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};

pony_type_t* ast_pony_type()
{
  return &ast_pony;
}
