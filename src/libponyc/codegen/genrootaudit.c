#include "genrootaudit.h"
#include "gencall.h"
#include "genexpr.h"
#include "../type/cap.h"
#include "../type/subtype.h"
#include <string.h>

typedef struct root_audit_t
{
  ast_t* body;
  LLVMValueRef method;
  LLVMValueRef roots;
  LLVMValueRef modes;
  size_t count;
  const char* refusal;
} root_audit_t;

static bool scalar_type(compile_t* c, ast_t* type)
{
  return ast_id(type) == TK_NOMINAL &&
    ast_name(ast_child(type)) == c->str_builtin && is_machine_word(type);
}

static bool scalar_fields(compile_t* c, reach_type_t* t)
{
  for(uint32_t i = 0; i < t->field_count; i++)
  {
    if(t->fields[i].embed || !scalar_type(c, t->fields[i].ast))
      return false;
  }
  return true;
}

static void add_root(compile_t* c, LLVMValueRef value, const char* mode)
{
  root_audit_t* a = c->root_audit;
  LLVMValueRef i = LLVMConstInt(c->intptr, a->count++, false);
  LLVMValueRef slot = LLVMBuildInBoundsGEP2(c->builder, c->ptr, a->roots,
    &i, 1, "");
  LLVMBuildStore(c->builder, value, slot);
  slot = LLVMBuildInBoundsGEP2(c->builder, c->ptr, a->modes, &i, 1, "");
  LLVMBuildStore(c->builder, codegen_string(c, mode, strlen(mode)), slot);
}

static const char* root_mode(compile_t* c, ast_t* type)
{
  if(ast_id(type) != TK_NOMINAL || is_bare(type, c->opt))
    return NULL;

  reach_type_t* t = reach_type(c->reach, type, c->opt);
  if(t == NULL)
    return NULL;
  if(t->underlying == TK_ACTOR)
    return "actor";
  if(t->underlying != TK_CLASS)
    return NULL;

  token_id cap = cap_single(type);
  if(cap == TK_TAG)
    return "tag";
  if(!scalar_fields(c, t))
    return NULL;

  switch(cap)
  {
    case TK_VAL: return "val";
    case TK_REF: return "ref";
    case TK_BOX: return "box";
    case TK_ISO: return "iso";
    case TK_TRN: return "trn";
    default: return NULL;
  }
}

static bool simple_value(ast_t* expr)
{
  switch(ast_id(expr))
  {
    case TK_THIS:
    case TK_LETREF:
    case TK_PARAMREF:
    case TK_INT:
    case TK_FLOAT:
    case TK_TRUE:
    case TK_FALSE:
      return true;

    default: return false;
  }
}

static bool simple_construction(compile_t* c, ast_t* expr)
{
  if(ast_id(expr) != TK_CALL)
    return false;

  AST_GET_CHILDREN(expr, postfix, positional, named, question);
  if(ast_id(postfix) != TK_NEWREF || ast_id(named) != TK_NONE ||
    ast_id(question) != TK_NONE)
    return false;

  AST_GET_CHILDREN(postfix, receiver, name);
  if(ast_id(receiver) != TK_TYPEREF || ast_id(name) != TK_ID)
    return false;

  for(ast_t* arg = ast_child(positional); arg != NULL; arg = ast_sibling(arg))
  {
    if(ast_id(arg) != TK_SEQ || ast_childcount(arg) != 1 ||
      !simple_value(ast_child(arg)))
      return false;
  }

  ast_t* type = deferred_reify(c->frame->reify, ast_type(receiver), c->opt);
  reach_type_t* t = reach_type(c->reach, type, c->opt);
  ast_free_unattached(type);
  if(t == NULL || t->underlying != TK_CLASS || !scalar_fields(c, t))
    return false;

  ast_t* def = (ast_t*)ast_data(t->ast);
  ast_t* ctor = ast_get(def, ast_name(name), NULL);
  if(ctor == NULL || ast_id(ctor) != TK_NEW)
    return false;

  ast_t* body = ast_childidx(ctor, 7);
  if(ast_id(body) != TK_SEQ)
    return false;
  for(ast_t* stmt = ast_child(body); stmt != NULL; stmt = ast_sibling(stmt))
  {
    if(ast_id(stmt) != TK_ASSIGN)
      return false;
    AST_GET_CHILDREN(stmt, left, right);
    if((ast_id(left) != TK_FLETREF && ast_id(left) != TK_FVARREF) ||
      ast_id(ast_child(left)) != TK_THIS || !simple_value(right))
      return false;
  }
  return true;
}

static void observe_statement(compile_t* c, ast_t* stmt)
{
  root_audit_t* a = c->root_audit;
  if(simple_value(stmt))
    return;

  if(ast_id(stmt) != TK_ASSIGN || ast_id(ast_child(stmt)) != TK_LET)
  {
    a->refusal = "unsupported-prefix";
    return;
  }

  AST_GET_CHILDREN(stmt, decl, initializer);
  if(!simple_value(initializer) && !simple_construction(c, initializer))
  {
    a->refusal = "unsupported-prefix";
    return;
  }

  ast_t* id = ast_child(decl);
  ast_t* type = deferred_reify(c->frame->reify, ast_type(id), c->opt);
  if(!scalar_type(c, type))
  {
    const char* mode = root_mode(c, type);
    if(mode == NULL)
      a->refusal = "unsupported-root";
    else
    {
      LLVMValueRef slot = codegen_getlocal(c, ast_name(id));
      add_root(c, LLVMBuildLoad2(c->builder, c->ptr, slot, ""), mode);
    }
  }
  ast_free_unattached(type);
}

void gen_root_audit_site(compile_t* c, ast_t* sequence, ast_t* previous,
  ast_t* next)
{
  root_audit_t* a = c->root_audit;
  if(sequence != a->body)
    return;

  if(previous != NULL && a->refusal == NULL)
    observe_statement(c, previous);

  LLVMValueRef nil = LLVMConstNull(c->ptr);
  bool ready = a->refusal == NULL;
  LLVMValueRef args[] = {
    codegen_ctx(c), LLVMGetParam(codegen_fun(c), 0), a->method,
    LLVMConstInt(c->i32, ast_line(next), false),
    LLVMConstInt(c->i32, ast_pos(next), false),
    ready ? nil : codegen_string(c, a->refusal, strlen(a->refusal)),
    LLVMConstInt(c->intptr, ready ? a->count : 0, false),
    ready ? a->roots : nil, ready ? a->modes : nil
  };
  gencall_runtime(c, "ponyint_gc_root_audit", args, 9, "");
}

LLVMValueRef gen_root_audit_body(compile_t* c, reach_type_t* t,
  reach_method_t* m, ast_t* body)
{
  root_audit_t audit = {0};
  audit.body = body;
  audit.method = codegen_string(c, m->full_name, strlen(m->full_name));
  c->root_audit = &audit;
  if(!scalar_fields(c, t))
    audit.refusal = "receiver-layout";
  else
  {
    audit.roots = LLVMBuildAlloca(c->builder, c->ptr, "roots");
    audit.modes = LLVMBuildAlloca(c->builder, c->ptr, "modes");
    LLVMValueRef fun = codegen_fun(c);
    add_root(c, LLVMGetParam(fun, 0), "receiver");

    for(size_t i = 0; i < m->param_count; i++)
    {
      ast_t* type = m->params[i].ast;
      if(scalar_type(c, type))
        continue;
      const char* mode = root_mode(c, type);
      if(mode == NULL)
      {
        audit.refusal = "unsupported-root";
        break;
      }
      add_root(c, LLVMGetParam(fun, (unsigned)(i + 1)), mode);
    }
  }

  LLVMValueRef result = gen_expr(c, body);
  if(audit.roots != NULL)
  {
    LLVMValueRef count = LLVMConstInt(c->intptr, audit.count, false);
    LLVMSetOperand(audit.roots, 0, count);
    LLVMSetOperand(audit.modes, 0, count);
  }
  c->root_audit = NULL;
  return result;
}
