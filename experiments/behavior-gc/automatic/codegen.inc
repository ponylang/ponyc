#include "../type/cap.h"
#include <stdlib.h>
#include <string.h>

static bool participating(compile_t* c, ast_t* method)
{
  if(method == NULL || (ast_id(method) != TK_FUN && ast_id(method) != TK_BE))
    return false;
  const char* file = getenv("GC_FRAME_SOURCE");
  source_t* src = ast_source(method);
  return file != NULL && src != NULL && src->file != NULL &&
    !strcmp(src->file, file) && ast_id(ast_child(method)) != TK_AT &&
    ast_childcount(ast_childidx(method, 2)) == 0 &&
    ast_childcount(ast_childidx(method, 3)) == 0 &&
    ast_id(ast_childidx(method, 6)) == TK_NONE &&
    !ast_has_annotation(method, "noalloc", c->opt->strtab);
}

static ast_t* direct_method(compile_t* c, ast_t* call, ast_t* actor_def)
{
  if(call == NULL || ast_id(call) != TK_CALL)
    return NULL;
  AST_GET_CHILDREN(call, fn, args, named, question);
  ast_t* type = ast_type(call);
  if(ast_id(fn) != TK_FUNREF || ast_id(ast_child(fn)) != TK_THIS ||
    ast_childcount(args) != 0 || ast_id(named) != TK_NONE ||
    ast_id(question) != TK_NONE || type == NULL || ast_id(type) != TK_NOMINAL ||
    ast_name(ast_child(type)) != c->str_builtin ||
    strcmp(ast_name(ast_childidx(type, 1)), "None"))
    return NULL;
  ast_t* name = ast_childidx(fn, 1);
  return ast_id(name) == TK_ID ? ast_get(actor_def, ast_name(name), NULL) : NULL;
}

static bool declares_local(ast_t* expr)
{
  switch(ast_id(expr))
  {
    case TK_LET:
    case TK_VAR:
    case TK_MATCH_CAPTURE:
      return true;
    default: {}
  }
  for(ast_t* child = ast_child(expr); child != NULL; child = ast_sibling(child))
    if(declares_local(child))
      return true;
  return false;
}

static const char* first_local(ast_t* stmt)
{
  if(stmt == NULL || ast_id(stmt) != TK_ASSIGN)
    return NULL;
  AST_GET_CHILDREN(stmt, decl, init);
  if(ast_id(decl) != TK_LET || ast_id(init) != TK_CALL ||
    ast_id(ast_child(init)) != TK_NEWREF || declares_local(init))
    return NULL;
  ast_t* type = ast_type(ast_child(decl));
  if(type == NULL || ast_id(type) != TK_NOMINAL || cap_single(type) != TK_REF ||
    ast_data(type) == NULL || ast_id((ast_t*)ast_data(type)) != TK_CLASS)
    return NULL;
  return ast_name(ast_child(decl));
}

static LLVMValueRef frame_call(compile_t* c, const char* name,
  LLVMValueRef* args, unsigned count)
{
#ifdef FRAME_THRESHOLD_ONLY
  if(strcmp(name, "ponyint_frame_poll"))
    return LLVMConstInt(c->i32, 0, false);
  name = "ponyint_threshold_sample";
#endif
  if(LLVMGetNamedFunction(c->module, name) == NULL)
  {
    LLVMTypeRef types[5];
    for(unsigned i = 0; i < count; i++)
      types[i] = LLVMTypeOf(args[i]);
    LLVMAddFunction(c->module, name, LLVMFunctionType(c->i32, types, count, false));
  }
  return gencall_runtime(c, name, args, (int)count, "");
}

typedef struct frame_site_t
{
  ast_t* actor;
  ast_t* wrapped;
  const char* target;
  const char* local;
  LLVMValueRef permit;
  bool prefix;
} frame_site_t;

static frame_site_t frame_sites(compile_t* c, ast_t* seq)
{
  frame_site_t site = {0};
  ast_t* m = ast_parent(seq);
  if(c->opt->gc_root_audit || c->opt->mutate || !participating(c, m) ||
    ast_childidx(m, 7) != seq || c->frame->reify == NULL)
    return site;
  ast_t* type = c->frame->reify->thistype;
  if(type == NULL || ast_id(type) != TK_NOMINAL || ast_data(type) == NULL ||
    ast_id((ast_t*)ast_data(type)) != TK_ACTOR)
    return site;
  site.actor = (ast_t*)ast_data(type);
  ast_t* first = ast_child(seq);
  site.local = first_local(first);
  if(site.local != NULL)
  {
    ast_t* next = ast_sibling(first);
    ast_t* target = direct_method(c, next, site.actor);
    if(participating(c, target))
    {
      site.wrapped = next;
      site.target = ast_name(ast_childidx(target, 1));
    }
  }
  site.prefix = ast_id(m) == TK_FUN && direct_method(c, first, site.actor) != NULL;
  if(ast_id(m) == TK_BE)
    site.permit = LLVMConstInt(c->i32, UINT32_MAX, false);
  else
  {
    const char* name = ast_name(ast_childidx(m, 1));
    LLVMValueRef args[] = {LLVMGetParam(codegen_fun(c), 0),
      codegen_string(c, name, strlen(name))};
    site.permit = frame_call(c, "ponyint_frame_claim", args, 2);
  }
  return site;
}

static LLVMValueRef frame_before(compile_t* c, frame_site_t* site,
  ast_t* previous, ast_t* child)
{
  if(site->actor == NULL)
    return NULL;
  LLVMValueRef actor = LLVMGetParam(codegen_fun(c), 0);
  if(site->wrapped == child)
  {
    LLVMValueRef slot = codegen_getlocal(c, site->local);
    LLVMValueRef args[] = {actor, site->permit,
      LLVMBuildLoad2(c->builder, c->ptr, slot, ""),
      codegen_string(c, site->target, strlen(site->target))};
    return frame_call(c, "ponyint_frame_push", args, 4);
  }
  if(site->prefix)
  {
    site->prefix = direct_method(c, child, site->actor) != NULL;
    if(site->prefix && previous != NULL)
    {
      LLVMValueRef args[] = {actor, site->permit};
      frame_call(c, "ponyint_frame_poll", args, 2);
    }
  }
  return NULL;
}
