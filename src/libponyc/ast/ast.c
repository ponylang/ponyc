#include "ast.h"
#include "symtab.h"
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

  struct ast_t* parent;
  struct ast_t* child;
  struct ast_t* sibling;
  struct ast_t* type;
};

static const char in[] = "  ";
static const size_t in_len = 2;

void print(ast_t* ast, size_t indent, size_t width, bool type);

size_t length(ast_t* ast, size_t indent)
{
  size_t len = (indent * in_len) + strlen(token_string(ast->t));
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

  printf("%s", token_string(ast->t));

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

  printf("%s\n", token_string(ast->t));

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

ast_t* dup(ast_t* parent, ast_t* ast)
{
  if(ast == NULL)
    return NULL;

  ast_t* n = ast_token(token_dup(ast->t));
  n->data = ast->data;
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
  return ast_token(token_from(t, id));
}

ast_t* ast_blank(token_id id)
{
  return ast_token(token_blank(id));
}

ast_t* ast_token(token_t* t)
{
  ast_t* ast = calloc(1, sizeof(ast_t));
  ast->t = t;
  return ast;
}

ast_t* ast_from(ast_t* ast, token_id id)
{
  return ast_token(token_from(ast->t, id));
}

ast_t* ast_from_string(ast_t* ast, const char* id)
{
  if(id == NULL)
    return ast_from(ast, TK_NONE);

  return ast_token(token_from_string(ast->t, id));
}

ast_t* ast_dup(ast_t* ast)
{
  return dup(NULL, ast);
}

void ast_attach(ast_t* ast, void* data)
{
  ast->data = data;
}

void ast_scope(ast_t* ast)
{
  ast->symtab = symtab_new();
}

void ast_setid(ast_t* ast, token_id id)
{
  ast->t->id = id;
}

token_id ast_id(ast_t* ast)
{
  return ast->t->id;
}

size_t ast_line(ast_t* ast)
{
  return ast->t->line;
}

size_t ast_pos(ast_t* ast)
{
  return ast->t->pos;
}

void* ast_data(ast_t* ast)
{
  return ast->data;
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

ast_t* ast_settype(ast_t* ast, ast_t* type)
{
  if(ast->type == type)
  {
    assert(type->parent == ast);
    return type;
  }

  ast_free(ast->type);

  if(type->parent != NULL)
    type = ast_dup(type);

  ast->type = type;
  type->parent = ast;
  return type;
}

ast_t* ast_nearest(ast_t* ast, token_id id)
{
  while((ast != NULL) && (ast->t->id != id))
    ast = ast->parent;

  return ast;
}

ast_t* ast_enclosing_type(ast_t* ast)
{
  while(ast != NULL)
  {
    switch(ast->t->id)
    {
      case TK_TYPE:
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
    switch(ast->t->id)
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
    switch(ast->t->id)
    {
      case TK_NEW:
      case TK_BE:
      case TK_FUN:
      {
        // only if we are in the method body
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
    switch(ast->t->id)
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
    switch(ast->t->id)
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

      case TK_FOR:
      {
        // only if we are in the loop body
        ast_t* body = ast_childidx(ast, 3);
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

    ast = ast->parent;
  } while((ast != NULL) && (ast->t->id != TK_PROGRAM));

  return NULL;
}

bool ast_set(ast_t* ast, const char* name, void* value)
{
  while(ast->symtab == NULL)
    ast = ast->parent;

  return (ast_get(ast, name) == NULL)
    && symtab_add(ast->symtab, name, value);
}

bool ast_merge(ast_t* dst, ast_t* src)
{
  while(dst->symtab == NULL)
    dst = dst->parent;

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
  assert(parent != child);
  assert(parent->child != child);

  if(child->parent != NULL)
    child = ast_dup(child);

  child->parent = parent;
  child->sibling = parent->child;
  parent->child = child;
  return child;
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

  child->parent = parent;

  if(parent->child == NULL)
  {
    parent->child = child;
    return child;
  }

  ast_t* ast = parent->child;
  while(ast->sibling != NULL) { ast = ast->sibling; }
  ast->sibling = child;
  return child;
}

ast_t* ast_swap(ast_t* prev, ast_t* next)
{
  assert(prev->parent != NULL);

  if(prev == next)
    return NULL;

  if(next->parent != NULL)
    next = ast_dup(next);

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
  return prev;
}

void ast_replace(ast_t* prev, ast_t* next)
{
  prev = ast_swap(prev, next);
  ast_free(prev);
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

  switch(ast->t->id)
  {
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
  errorv(ast->t->source, ast->t->line, ast->t->pos, fmt, ap);
  va_end(ap);
}

ast_result_t ast_visit(ast_t* ast, ast_visit_t pre, ast_visit_t post,
  ast_dir_t dir, int verbose)
{
  ast_result_t ret = AST_OK;

  if(pre != NULL)
  {
    switch(pre(ast, verbose))
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
    ast_t* child;

    switch(dir)
    {
      case AST_LEFT: child = ast_child(ast); break;
      case AST_RIGHT: child = ast_childlast(ast); break;
    }

    while(child != NULL)
    {
      ast_t* next;

      switch(dir)
      {
        case AST_LEFT: next = ast_sibling(child); break;
        case AST_RIGHT: next = ast_previous(child); break;
      }

      switch(ast_visit(child, pre, post, dir, verbose))
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
    switch(post(ast, verbose))
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
