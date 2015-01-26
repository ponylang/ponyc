#include "ast.h"
#include "frame.h"
#include "symtab.h"
#include "token.h"
#include "stringtab.h"
#include "printbuf.h"
#include "../pass/pass.h"
#include "../pkg/program.h"
#include "../pkg/package.h"
#include "../../libponyrt/mem/pool.h"
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
static size_t width = 80;

static void print(ast_t* ast, size_t indent, bool type);


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

static size_t length(ast_t* ast, size_t indent, bool type)
{
  size_t len = (indent * in_len) + strlen(token_print(ast->t));
  ast_t* child = ast->child;

  if(type || (child != NULL) || (ast->type != NULL))
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
    len += 1 + length(child, 0, false);
    child = child->sibling;
  }

  if(ast->type != NULL)
    len += 1 + length(ast->type, 0, true);

  return len;
}

static void print_compact(ast_t* ast, size_t indent, bool type)
{
  for(size_t i = 0; i < indent; i++)
    printf(in);

  ast_t* child = ast->child;
  bool parens = type || (child != NULL) || (ast->type != NULL);

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

static void print_extended(ast_t* ast, size_t indent, bool type)
{
  for(size_t i = 0; i < indent; i++)
    printf(in);

  ast_t* child = ast->child;
  bool parens = type || (child != NULL) || (ast->type != NULL);

  if(parens)
    printf(type ? "[" : "(");

  print_token(ast->t);

  if(ast->symtab != NULL)
    printf(":scope");

  printf("\n");

  while(child != NULL)
  {
    print(child, indent + 1, false);
    child = child->sibling;
  }

  if(ast->type != NULL)
    print(ast->type, indent + 1, true);

  if(parens || type)
  {
    for(size_t i = 0; i < indent; i++)
      printf(in);

    printf(type ? "]" : ")");
  }
}

static void print(ast_t* ast, size_t indent, bool type)
{
  size_t len = length(ast, indent, type);

  if(len < width)
  {
    print_compact(ast, indent, type);
  } else {
    print_extended(ast, indent, type);
  }

  printf("\n");
}

static ast_t* duplicate(ast_t* parent, ast_t* ast)
{
  if(ast == NULL)
    return NULL;

  ast_t* n = ast_token(token_dup(ast->t));
  n->data = ast->data;
  n->scope = (parent != NULL) ? parent : ast->scope;
  n->parent = parent;
  n->child = duplicate(n, ast->child);
  n->type = duplicate(n, ast->type);

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
  return ast_token(token_new(id, NULL));
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
  assert(ast != NULL);
  ast_t* new_ast = ast_token(token_dup_new_id(ast->t, id));
  new_ast->scope = ast->scope;
  return new_ast;
}

ast_t* ast_from_string(ast_t* ast, const char* name)
{
  if(name == NULL)
    return ast_from(ast, TK_NONE);

  token_t* t = token_dup(ast->t);
  token_set_id(t, TK_ID);
  token_set_string(t, name);

  ast_t* new_ast = ast_token(t);
  new_ast->scope = ast->scope;
  return new_ast;
}

ast_t* ast_from_int(ast_t* ast, uint64_t value)
{
  token_t* t = token_dup(ast->t);
  token_set_id(t, TK_INT);
  token_set_int(t, value);

  ast_t* new_ast = ast_token(t);
  new_ast->scope = ast->scope;
  return new_ast;
}

ast_t* ast_dup(ast_t* ast)
{
  return duplicate(NULL, ast);
}

void ast_scope(ast_t* ast)
{
  ast->symtab = symtab_new();
}

bool ast_has_scope(ast_t* ast)
{
  return ast->symtab != NULL;
}

symtab_t* ast_get_symtab(ast_t* ast)
{
  assert(ast != NULL);
  return ast->symtab;
}

void ast_setid(ast_t* ast, token_id id)
{
  token_set_id(ast->t, id);
}

void ast_setpos(ast_t* ast, size_t line, size_t pos)
{
  token_set_pos(ast->t, line, pos);
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

bool ast_is_first_on_line(ast_t* ast)
{
  return token_is_first_on_line(ast->t);
}

void* ast_data(ast_t* ast)
{
  if(ast == NULL)
    return NULL;

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

void ast_set_name(ast_t* ast, const char* name)
{
  token_set_string(ast->t, name);
}

double ast_float(ast_t* ast)
{
  return token_float(ast->t);
}

__int128_t ast_int(ast_t* ast)
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

void ast_erase(ast_t* ast)
{
  assert(ast != NULL);

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
  assert(ast != NULL);
  return ast->child;
}

ast_t* ast_childidx(ast_t* ast, size_t idx)
{
  assert(ast != NULL);
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

    ast = ast->scope;
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

    ast = ast->scope;
  } while((ast != NULL) && (token_get_id(ast->t) != TK_PROGRAM));

  return NULL;
}

bool ast_set(ast_t* ast, const char* name, ast_t* value, sym_status_t status)
{
  while(ast->symtab == NULL)
    ast = ast->scope;

  return (ast_get_case(ast, name, NULL) == NULL)
    && symtab_add(ast->symtab, name, value, status);
}

void ast_setstatus(ast_t* ast, const char* name, sym_status_t status)
{
  while(ast->symtab == NULL)
    ast = ast->scope;

  symtab_set_status(ast->symtab, name, status);
}

void ast_inheritstatus(ast_t* dst, ast_t* src)
{
  if(dst == NULL)
    return;

  while(dst->symtab == NULL)
    dst = dst->scope;

  symtab_inherit_status(dst->symtab, src->symtab);
}

void ast_inheritbranch(ast_t* dst, ast_t* src)
{
  symtab_inherit_branch(dst->symtab, src->symtab);
}

void ast_consolidate_branches(ast_t* ast, size_t count)
{
  symtab_consolidate_branches(ast->symtab, count);
}

bool ast_merge(ast_t* dst, ast_t* src)
{
  while(dst->symtab == NULL)
    dst = dst->scope;

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

    inner = inner->scope;
  } while((inner != NULL) && (token_get_id(inner->t) != TK_PROGRAM));

  return false;
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

void ast_remove(ast_t* ast)
{
  assert(ast != NULL);
  assert(ast->parent != NULL);

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
  if(*prev == next)
    return;

  if((*prev)->parent != NULL)
  {
    if(next->parent != NULL)
      next = ast_dup(next);

    ast_swap(*prev, next);
  }

  ast_free(*prev);
  *prev = next;
}

void ast_reorder_children(ast_t* ast, const size_t* new_order)
{
  assert(ast != NULL);
  assert(new_order != NULL);

  size_t count = ast_childcount(ast);
  VLA(ast_t*, children, count);

  for(size_t i = 0; i < count; i++)
    children[i] = ast_pop(ast);

  for(size_t i = 0; i < count; i++)
  {
    size_t index = new_order[i];
    assert(index < count);
    ast_t* t = children[index];
    assert(t != NULL);
    ast_append(ast, t);
    children[index] = NULL;
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

  ast_free(ast->type);

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
  if((ast != NULL) && (ast->parent == NULL))
    ast_free(ast);
}

void ast_print(ast_t* ast)
{
  if(ast == NULL)
    return;

  print(ast, 0, false);
  printf("\n");
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
      AST_GET_CHILDREN(type, package, id, typeargs, cap, ephemeral, origpkg);

      if(ast_id(origpkg) != TK_NONE)
        printbuf(buffer, "%s.", ast_name(origpkg));

      printbuf(buffer, "%s", ast_name(id));

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
      printbuf(buffer, "%s", ast_name(id));

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

    case TK_DONTCARE:
      printbuf(buffer, "_");
      break;

    case TK_FUNTYPE:
      printbuf(buffer, "function");
      break;

    default:
      assert(0);
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

void ast_setwidth(size_t w)
{
  width = w;
}

void ast_error(ast_t* ast, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  errorv(token_source(ast->t), token_line_number(ast->t),
    token_line_position(ast->t), fmt, ap);
  va_end(ap);
}

ast_result_t ast_visit(ast_t** ast, ast_visit_t pre, ast_visit_t post,
  pass_opt_t* options)
{
  typecheck_t* t = &options->check;
  bool pop = frame_push(t, *ast);

  ast_result_t ret = AST_OK;

  if(pre != NULL)
  {
    switch(pre(ast, options))
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

      switch(ast_visit(&child, pre, post, options))
      {
        case AST_OK:
          break;

        case AST_ERROR:
          ret = AST_ERROR;
          break;

        case AST_FATAL:
          return AST_FATAL;
      }

      if(next == NULL)
        child = ast_sibling(child);
      else
        child = next;
    }
  }

  if(post != NULL)
  {
    switch(post(ast, options))
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

  if(pop)
    frame_pop(t);

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
