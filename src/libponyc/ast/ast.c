#include "ast.h"
#include "symtab.h"
#include "token.h"
#include "../ds/stringtab.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

struct ast_t
{
  token_t* t;
  symtab_t* symtab;
  void* data;

  struct ast_t* scope;
  struct ast_t* parent;
  struct ast_t* child;
  struct ast_t* sibling;
  struct ast_t* type;
};

static const char in[] = "  ";
static const size_t in_len = 2;

void print(ast_t* ast, size_t indent, size_t width, bool type);


static void print_token(token_t* token)
{
  switch(token_get_id(token))
  {
    case TK_STRING:
      printf("\"\"\"%s\"\"\"", token_print(token));
      break;

    case TK_ID:
      printf("(id %s)", token_print(token));
      break;

    default:
      printf("%s", token_print(token));
      break;
  }
}

size_t length(ast_t* ast, size_t indent)
{
  size_t len = (indent * in_len) + strlen(token_print(ast->t));
  ast_t* child = ast->child;

  if(child != NULL)
    len += 2;

  while(child != NULL)
  {
    len += 1 + length(child, 0);
    child = child->sibling;
  }

  if(ast->type != NULL)
    len += 1 + length(ast->type, 0);

  return len;
}

void print_compact(ast_t* ast, size_t indent, bool type)
{
  for(size_t i = 0; i < indent; i++)
    printf(in);

  ast_t* child = ast->child;
  bool parens = child != NULL;

  if(parens)
    printf(type ? "[" : "(");

  print_token(ast->t);

  if(ast->symtab != NULL)
    printf(":scope");

  while(child != NULL)
  {
    printf(" ");
    print_compact(child, 0, false);
    child = child->sibling;
  }

  if(ast->type != NULL)
  {
    printf(" ");
    print_compact(ast->type, 0, true);
  }

  if(parens)
    printf(type ? "]" : ")");
}

void print_extended(ast_t* ast, size_t indent, size_t width, bool type)
{
  for(size_t i = 0; i < indent; i++)
    printf(in);

  ast_t* child = ast->child;
  bool parens = child != NULL;

  if(parens)
    printf(type ? "[" : "(");

  print_token(ast->t);
  printf("\n");

  while(child != NULL)
  {
    print(child, indent + 1, width, false);
    child = child->sibling;
  }

  if(ast->type != NULL)
    print(ast->type, indent + 1, width, true);

  if(parens)
  {
    for(size_t i = 0; i <= indent; i++)
      printf(in);

    printf(type ? "]" : ")");
  }
}

void print(ast_t* ast, size_t indent, size_t width, bool type)
{
  size_t len = length(ast, indent);

  if(len <= width)
  {
    print_compact(ast, indent, type);
  } else {
    print_extended(ast, indent, width, type);
  }

  printf("\n");
}

static ast_t* dup(ast_t* parent, ast_t* ast)
{
  if(ast == NULL)
    return NULL;

  ast_t* n = ast_token(token_dup(ast->t));
  n->data = ast->data;
  n->scope = (parent != NULL) ? parent : ast->scope;
  n->parent = parent;
  n->child = dup(n, ast->child);
  n->type = dup(n, ast->type);

  if(ast->symtab != NULL)
  {
    n->symtab = symtab_new();
    symtab_merge(n->symtab, ast->symtab);
  }

  if(parent != NULL)
    n->sibling = dup(parent, ast->sibling);

  return n;
}

ast_t* ast_new(token_t* t, token_id id)
{
  return ast_token(token_dup_new_id(t, id));
}

ast_t* ast_blank(token_id id)
{
  return ast_token(token_new(id, NULL));
}

ast_t* ast_token(token_t* t)
{
  ast_t* ast = calloc(1, sizeof(ast_t));
  ast->t = t;
  return ast;
}

ast_t* ast_from(ast_t* ast, token_id id)
{
  assert(ast != NULL);
  ast_t* new_ast = ast_token(token_dup_new_id(ast->t, id));
  new_ast->scope = ast->scope;
  return new_ast;
}

ast_t* ast_from_string(ast_t* ast, const char* id)
{
  if(id == NULL)
    return ast_from(ast, TK_NONE);

  token_t* t = token_dup(ast->t);
  token_set_id(t, TK_ID);
  token_set_string(t, id);

  ast_t* new_ast = ast_token(t);
  new_ast->scope = ast->scope;
  return new_ast;
}

ast_t* ast_dup(ast_t* ast)
{
  return dup(NULL, ast);
}

void ast_scope(ast_t* ast)
{
  ast->symtab = symtab_new();
}

bool ast_has_scope(ast_t* ast)
{
  return ast->symtab != NULL;
}

void ast_setid(ast_t* ast, token_id id)
{
  token_set_id(ast->t, id);
}

token_id ast_id(ast_t* ast)
{
  return token_get_id(ast->t);
}

size_t ast_line(ast_t* ast)
{
  return token_line_number(ast->t);
}

size_t ast_pos(ast_t* ast)
{
  return token_line_position(ast->t);
}

void* ast_data(ast_t* ast)
{
  return ast->data;
}

void ast_setdata(ast_t* ast, void* data)
{
  ast->data = data;
}

bool ast_canerror(ast_t* ast)
{
  assert((ast->data == NULL) || (ast->data == (void*)1));
  return ast->data == (void*)1;
}

void ast_seterror(ast_t* ast)
{
  assert(ast->data == NULL);
  ast->data = (void*)1;
}

void ast_inheriterror(ast_t* ast)
{
  if(ast_canerror(ast))
    return;

  ast_t* child = ast->child;

  while(child != NULL)
  {
    if(ast_canerror(child))
    {
      ast_seterror(ast);
      return;
    }

    child = ast_sibling(child);
  }
}

const char* ast_get_print(ast_t* ast)
{
  return token_print(ast->t);
}

const char* ast_name(ast_t* ast)
{
  return token_string(ast->t);
}

double ast_float(ast_t* ast)
{
  return token_float(ast->t);
}

size_t ast_int(ast_t* ast)
{
  return token_int(ast->t);
}

ast_t* ast_type(ast_t* ast)
{
  return ast->type;
}

void ast_settype(ast_t* ast, ast_t* type)
{
  if(ast->type == type)
    return;

  ast_free(ast->type);

  if(type != NULL)
  {
    if(type->parent != NULL)
      type = ast_dup(type);

    type->scope = ast;
    type->parent = ast;
  }

  ast->type = type;
}

ast_t* ast_nearest(ast_t* ast, token_id id)
{
  while((ast != NULL) && (token_get_id(ast->t) != id))
    ast = ast->parent;

  return ast;
}

ast_t* ast_enclosing_type(ast_t* ast)
{
  while(ast != NULL)
  {
    switch(token_get_id(ast->t))
    {
      case TK_TRAIT:
      case TK_CLASS:
      case TK_ACTOR:
        return ast;

      default: {}
    }

    ast = ast->parent;
  }

  return NULL;
}

ast_t* ast_enclosing_method(ast_t* ast)
{
  while(ast != NULL)
  {
    switch(token_get_id(ast->t))
    {
      case TK_NEW:
      case TK_BE:
      case TK_FUN:
        return ast;

      default: {}
    }

    ast = ast->parent;
  }

  return NULL;
}

ast_t* ast_enclosing_method_type(ast_t* ast)
{
  ast_t* last = NULL;

  while(ast != NULL)
  {
    switch(token_get_id(ast->t))
    {
      case TK_NEW:
      case TK_BE:
      case TK_FUN:
      {
        // only if we are in the method return type
        ast_t* type = ast_childidx(ast, 4);

        if(type == last)
          return ast;
        break;
      }

      default: {}
    }

    last = ast;
    ast = ast->parent;
  }

  return NULL;
}

ast_t* ast_enclosing_method_body(ast_t* ast)
{
  ast_t* last = NULL;

  while(ast != NULL)
  {
    switch(token_get_id(ast->t))
    {
      case TK_NEW:
      case TK_BE:
      case TK_FUN:
      {
        // only if we are in the method body
        ast_t* body = ast_childidx(ast, 6);

        if(body == last)
          return ast;
        break;
      }

      default: {}
    }

    last = ast;
    ast = ast->parent;
  }

  return NULL;
}

ast_t* ast_enclosing_loop(ast_t* ast)
{
  ast_t* last = NULL;

  while(ast != NULL)
  {
    switch(token_get_id(ast->t))
    {
      case TK_WHILE:
      {
        // only if we are in the loop body
        ast_t* body = ast_childidx(ast, 1);
        assert(ast_id(body) == TK_SEQ);

        if(body == last)
          return ast;
        break;
      }

      case TK_REPEAT:
      {
        // only if we are in the loop body
        ast_t* body = ast_child(ast);
        assert(ast_id(body) == TK_SEQ);

        if(body == last)
          return ast;
        break;
      }

      default: {}
    }

    last = ast;
    ast = ast->parent;
  }

  return NULL;
}

ast_t* ast_enclosing_constraint(ast_t* ast)
{
  ast_t* last = NULL;

  while(ast != NULL)
  {
    switch(token_get_id(ast->t))
    {
      case TK_STRUCTURAL:
        // if we're inside a structural type, we aren't a nominal constraint
        return NULL;

      case TK_TYPEPARAM:
      {
        // only if we are in the constraint
        ast_t* constraint = ast_childidx(ast, 1);

        if(constraint == last)
          return ast;
        break;
      }

      default: {}
    }

    last = ast;
    ast = ast->parent;
  }

  return NULL;
}

ast_t* ast_parent(ast_t* ast)
{
  return ast->parent;
}

ast_t* ast_child(ast_t* ast)
{
  return ast->child;
}

ast_t* ast_childidx(ast_t* ast, size_t idx)
{
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
  ast_t* child = ast->parent->child;
  size_t idx = 0;

  while(child != ast)
  {
    child = child->sibling;
    idx++;
  }

  return idx;
}

void* ast_get(ast_t* ast, const char* name)
{
  /* searches all parent scopes, but not the program scope, because the name
   * space for paths is separate from the name space for all other IDs.
   * if called directly on the program scope, searches it.
   */
  do
  {
    if(ast->symtab != NULL)
    {
      void* value = symtab_get(ast->symtab, name);

      if(value != NULL)
        return value;
    }

    ast = ast->scope;
  } while((ast != NULL) && (token_get_id(ast->t) != TK_PROGRAM));

  return NULL;
}

bool ast_set(ast_t* ast, const char* name, void* value)
{
  while(ast->symtab == NULL)
    ast = ast->scope;

  return (ast_get(ast, name) == NULL)
    && symtab_add(ast->symtab, name, value);
}

bool ast_merge(ast_t* dst, ast_t* src)
{
  while(dst->symtab == NULL)
    dst = dst->scope;

  return symtab_merge(dst->symtab, src->symtab);
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

ast_t* ast_add(ast_t* parent, ast_t* child)
{
  assert(parent != NULL);
  assert(parent != child);
  assert(parent->child != child);

  if(child->parent != NULL)
    child = ast_dup(child);

  child->scope = parent;
  child->parent = parent;
  child->sibling = parent->child;
  parent->child = child;
  return child;
}

ast_t* ast_add_sibling(ast_t* older_sibling, ast_t* new_sibling)
{
  assert(older_sibling != NULL);
  assert(new_sibling != NULL);
  assert(older_sibling != new_sibling);
  assert(older_sibling->sibling == NULL);

  if(new_sibling->parent != NULL)
    new_sibling = ast_dup(new_sibling);

  new_sibling->scope = older_sibling->scope;
  new_sibling->parent = older_sibling->parent;
  older_sibling->sibling = new_sibling;
  return new_sibling;
}

ast_t* ast_pop(ast_t* parent)
{
  ast_t* child = parent->child;

  if(child != NULL)
  {
    parent->child = child->sibling;
    child->parent = NULL;
    child->sibling = NULL;
  }

  return child;
}

ast_t* ast_append(ast_t* parent, ast_t* child)
{
  assert(parent != child);

  if(child->parent != NULL)
    child = ast_dup(child);

  child->scope = parent;
  child->parent = parent;

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

void ast_swap(ast_t* prev, ast_t* next)
{
  assert(prev->parent != NULL);
  assert(prev != next);

  if(next->parent != NULL)
    next = ast_dup(next);

  next->scope = prev->parent;
  next->parent = prev->parent;

  if(prev->parent->type == prev)
  {
    prev->parent->type = next;
  } else {
    ast_t* last = ast_previous(prev);

    if(last != NULL)
      last->sibling = next;
    else
      prev->parent->child = next;

    next->sibling = prev->sibling;
  }

  prev->parent = NULL;
  prev->sibling = NULL;
}

void ast_replace(ast_t** prev, ast_t* next)
{
  if((*prev)->parent != NULL)
    ast_swap(*prev, next);

  ast_free(*prev);
  *prev = next;
}

void ast_reverse(ast_t* ast)
{
  if(ast == NULL)
    return;

  ast_t* cur = ast->child;
  ast_t* next;
  ast_t* last = NULL;

  while(cur != NULL)
  {
    assert(cur->parent == ast);

    ast_reverse(cur);
    next = cur->sibling;
    cur->sibling = last;
    last = cur;
    cur = next;
  }

  ast->child = last;
}

void ast_print(ast_t* ast, size_t width)
{
  if(ast == NULL)
    return;

  print(ast, 0, width, false);
  printf("\n");
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

  ast_free(ast->type);

  switch(token_get_id(ast->t))
  {
    case TK_PACKAGE:
      free(ast->data);
      break;

    case TK_MODULE:
      source_close(ast->data);
      break;

    default:
      break;
  }

  token_free(ast->t);
  symtab_free(ast->symtab);
}

void ast_free_unattached(ast_t* ast)
{
  if((ast != NULL) && (ast->parent == NULL))
    ast_free(ast);
}

void ast_error(ast_t* ast, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  errorv(token_source(ast->t), token_line_number(ast->t),
    token_line_position(ast->t), fmt, ap);
  va_end(ap);
}

ast_result_t ast_visit(ast_t** ast, ast_visit_t pre, ast_visit_t post)
{
  ast_result_t ret = AST_OK;

  if(pre != NULL)
  {
    switch(pre(ast))
    {
      case AST_OK:
        break;

      case AST_ERROR:
        ret = AST_ERROR;
        break;

      case AST_FATAL:
        return AST_FATAL;
    }
  }

  if((pre != NULL) || (post != NULL))
  {
    ast_t* child = ast_child(*ast);

    while(child != NULL)
    {
      ast_t* next = ast_sibling(child);

      switch(ast_visit(&child, pre, post))
      {
        case AST_OK:
          break;

        case AST_ERROR:
          ret = AST_ERROR;
          break;

        case AST_FATAL:
          return AST_FATAL;
      }

      child = next;
    }
  }

  if(post != NULL)
  {
    switch(post(ast))
    {
      case AST_OK:
        break;

      case AST_ERROR:
        ret = AST_ERROR;
        break;

      case AST_FATAL:
        return AST_FATAL;
    }
  }

  return ret;
}

void ast_get_children(ast_t* parent, size_t child_count,
  ast_t*** out_children)
{
  assert(parent != NULL);
  assert(child_count > 0);
  assert(out_children != NULL);

  ast_t* p = parent->child;

  for(size_t i = 0; i < child_count; i++)
  {
    assert(p != NULL);

    if(out_children[i] != NULL)
      *(out_children[i]) = p;

    p = p->sibling;
  }
}

void ast_extract_children(ast_t* parent, size_t child_count,
  ast_t*** out_children)
{
  assert(parent != NULL);
  assert(child_count > 0);
  assert(out_children != NULL);

  ast_t* p = parent->child;
  ast_t* last_remaining_sibling = NULL;

  for(size_t i = 0; i < child_count; i++)
  {
    assert(p != NULL);
    ast_t* next = p->sibling;

    if(out_children[i] != NULL)
    {
      if(last_remaining_sibling != NULL)
        last_remaining_sibling->sibling = p->sibling;
      else
        parent->child = p->sibling;

      p->parent = NULL;
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
