#include "gengc.h"
#include "ponyassert.h"
#include "gencall.h"
#include "genexpr.h"
#include "gentrace.h"
#include "gentype.h"
#include "genfun.h"
#include "genopt.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include "../type/subtype.h"
#include "../type/typealias.h"

bool gengc_contains_pointer(LLVMTypeRef type)
{
  if(LLVMGetTypeKind(type) == LLVMPointerTypeKind)
    return true;
  if(LLVMGetTypeKind(type) == LLVMStructTypeKind)
  {
    for(unsigned i = 0; i < LLVMCountStructElementTypes(type); i++)
    {
      if(gengc_contains_pointer(LLVMStructGetTypeAtIndex(type, i)))
        return true;
    }
  }
  return false;
}

bool gengc_noncapturing_ffi(compile_t* c, const char* name)
{
  static const char* const names[] = {
    "memcmp", "pony_actor_stats", "pony_scheduler_stats",
    "pony_ctx", "pony_os_errno", "pony_os_fstat", "pony_os_lseek", "pony_os_stat",
    "pony_os_stdin_setup", "pony_os_stdout_setup", "pony_triggergc",
    "ponyint_cpu_tick", "ponyint_hash_block",
    "ponyint_hash_block64", "ponyint_o_rdonly"
  };
  if(strcmp(name, "open") == 0 || strcmp(name, "read") == 0 ||
    strcmp(name, "close") == 0 || strcmp(name, "ponyint_clock_gettime") == 0)
    return !target_is_windows(c->opt->triple);
  if(strcmp(name, "_open") == 0 || strcmp(name, "_read") == 0 ||
    strcmp(name, "_close") == 0)
    return target_is_windows(c->opt->triple);
  for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
  {
    if(strcmp(name, names[i]) == 0)
      return true;
  }
  return false;
}

static bool borrowed_ffi_argument(compile_t* c, ast_t* expr)
{
  ast_t* seq = ast_parent(expr);
  if(seq == NULL || ast_id(seq) != TK_SEQ || ast_childlast(seq) != expr)
    return false;
  ast_t* args = ast_parent(seq);
  if(args == NULL || ast_id(args) != TK_POSITIONALARGS)
    return false;
  ast_t* ffi = ast_parent(args);
  return ast_id(ffi) == TK_FFICALL &&
    gengc_noncapturing_ffi(c, ast_name(ast_child(ffi)) + 1);
}

bool gengc_borrowed_address(compile_t* c, ast_t* expr)
{
  ast_t* ref = ast_child(expr);
  return ast_id(ref) == TK_VARREF && borrowed_ffi_argument(c, expr) &&
    is_machine_word(ast_type(ref));
}

static bool borrowed_pointer(compile_t* c, ast_t* expr)
{
  if(borrowed_ffi_argument(c, expr))
    return true;

  ast_t* use = ast_parent(expr);
  if(ast_id(use) == TK_FUNREF && ast_child(use) == expr)
  {
    const char* method = ast_name(ast_childidx(use, 1));
    return strcmp(method, "is_null") == 0 &&
      (is_pointer(ast_type(expr)) || is_nullable_pointer(ast_type(expr)));
  }
  if(ast_id(use) != TK_SEQ || ast_childlast(use) != expr)
    return false;
  ast_t* args = ast_parent(use);
  if(ast_id(args) != TK_POSITIONALARGS || ast_id(ast_parent(args)) != TK_CALL)
    return false;
  ast_t* ctor = ast_child(ast_parent(args));
  if(ast_id(ctor) != TK_NEWREF)
    return false;
  ast_t* receiver = ast_type(ast_child(ctor));
  if(receiver == NULL || ast_id(receiver) != TK_NOMINAL)
    return false;
  AST_GET_CHILDREN(receiver, pkg, id);
  return ast_name(pkg) == c->str_builtin && ast_name(id) == c->str_String &&
    strcmp(ast_name(ast_childidx(ctor, 1)), "copy_cstring") == 0;
}

static bool local_only_borrowed(compile_t* c, const char* name);

static bool borrowed_result(compile_t* c, ast_t* expr)
{
  if(borrowed_pointer(c, expr))
    return true;
  ast_t* parent = ast_parent(expr);
  if(ast_id(parent) != TK_ASSIGN || ast_childidx(parent, 1) != expr)
    return false;
  ast_t* left = ast_child(parent);
  return ast_id(left) == TK_LET &&
    local_only_borrowed(c, ast_name(ast_child(left)));
}

void gengc_pointer_result(compile_t* c, ast_t* expr, LLVMValueRef value)
{
  if(value == NULL || value == GEN_NOVALUE ||
    value == GEN_NOTNEEDED || LLVMTypeOf(value) != c->ptr ||
    c->frame->reify == NULL)
    return;
  ast_t* receiver = c->frame->reify->thistype;
  if(ast_id(receiver) == TK_NOMINAL)
  {
    AST_GET_CHILDREN(receiver, pkg, id);
    if(ast_name(pkg) == c->str_builtin &&
      (ast_name(id) == c->str_Array || ast_name(id) == c->str_String ||
      ast_name(id) == c->str_Pointer))
      return;

  }
  ast_t* type = ast_type(expr);
  bool raw;
  if(ast_id(type) == TK_NOMINAL)
  {
    raw = is_pointer(type) || is_nullable_pointer(type);
  } else {
    ast_t* reified = deferred_reify(c->frame->reify, type, c->opt);
    raw = is_pointer(reified) || is_nullable_pointer(reified);
    ast_free_unattached(reified);
  }
  if(raw && !LLVMIsNull(value) && !borrowed_result(c, expr))
    gencall_runtime(c, "ponyint_gc_native_alias", NULL, 0, "");
}

static bool contains_local_in_scope(compile_t* c, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_LET:
    case TK_VAR:
    case TK_MATCH_CAPTURE:
      return c == NULL || codegen_getlocal(c, ast_name(ast_child(ast))) != NULL;
    default: {}
  }
  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
  {
    if(contains_local_in_scope(c, child))
      return true;
  }
  return false;
}

static bool contains_local(ast_t* ast)
{
  return contains_local_in_scope(NULL, ast);
}

typedef struct gc_use_t
{
  const char* name;
  size_t last_mention;
  size_t first_consume;
  bool may_escape;
} gc_use_t;

typedef struct gc_statement_t
{
  ast_t* ast;
  size_t index;
  struct gc_statement_t* previous_declaration;
} gc_statement_t;

typedef struct gc_liveness_t
{
  hashmap_t uses;
  hashmap_t statements;
} gc_liveness_t;

typedef struct root_site_t
{
  gc_liveness_t* liveness;
  size_t statement;
} root_site_t;

static bool same_name(void* a, void* b)
{
  return ((const gc_use_t*)a)->name == ((const gc_use_t*)b)->name;
}

static void free_use(void* p)
{
  POOL_FREE(gc_use_t, p);
}

static bool same_statement(void* a, void* b)
{
  return ((gc_statement_t*)a)->ast == ((gc_statement_t*)b)->ast;
}

static void free_statement(void* p)
{
  POOL_FREE(gc_statement_t, p);
}

void gengc_free_liveness(gc_liveness_t* liveness)
{
  if(liveness != NULL)
  {
    ponyint_hashmap_destroy(&liveness->uses, free_use);
    ponyint_hashmap_destroy(&liveness->statements, free_statement);
    POOL_FREE(gc_liveness_t, liveness);
  }
}

static gc_statement_t* get_statement(gc_liveness_t* liveness, ast_t* ast)
{
  gc_statement_t key = {ast, 0, NULL};
  size_t index = HASHMAP_UNKNOWN;
  return (gc_statement_t*)ponyint_hashmap_get(&liveness->statements, &key,
    ponyint_hash_ptr(ast), same_statement, &index);
}

static gc_statement_t* index_statement(root_site_t site, ast_t* ast,
  gc_statement_t* previous_declaration)
{
  gc_statement_t* stmt = POOL_ALLOC(gc_statement_t);
  stmt->ast = ast;
  stmt->index = site.statement;
  stmt->previous_declaration = previous_declaration;
  ponyint_hashmap_put(&site.liveness->statements, stmt, ponyint_hash_ptr(ast),
    same_statement);
  return stmt;
}

static bool record_uses(compile_t* c, root_site_t site, ast_t* ast)
{
  const char* name = NULL;
  bool consume = ast_id(ast) == TK_CONSUME;
  ast_t* ref = consume ? ast_childidx(ast, 1) : ast;
  switch(ast_id(ref))
  {
    case TK_THIS: name = c->str_this; break;
    case TK_ID:
      if(!consume)
        name = ast_name(ref);
      break;
    case TK_LETREF:
      name = ast_name(ast_child(ref));
      break;
    case TK_PARAMREF:
    case TK_VARREF:
      if(consume)
        name = ast_name(ast_child(ref));
      break;
    default: {}
  }
  if(name != NULL)
  {
    gc_use_t key = {name, site.statement, SIZE_MAX, false};
    size_t index = HASHMAP_UNKNOWN;
    size_t hash = ponyint_hash_ptr(name);
    gc_use_t* use = (gc_use_t*)ponyint_hashmap_get(&site.liveness->uses, &key,
      hash, same_name, &index);
    if(use == NULL)
    {
      use = POOL_ALLOC(gc_use_t);
      *use = key;
      ponyint_hashmap_putindex(&site.liveness->uses, use, hash, same_name,
        index);
    }
    use->last_mention = site.statement;
    if(ast_id(ast) == TK_LETREF && !borrowed_pointer(c, ast))
      use->may_escape = true;
    if(consume && use->first_consume == SIZE_MAX)
      use->first_consume = site.statement;
  }
  bool declares_local = ast_id(ast) == TK_LET || ast_id(ast) == TK_VAR ||
    ast_id(ast) == TK_MATCH_CAPTURE;
  ast_t* first = ast_child(ast);
  bool sequence = ast_id(ast) == TK_SEQ && first != NULL &&
    ast_sibling(first) != NULL;
  gc_statement_t* previous = NULL;
  for(ast_t* child = first; child != NULL; child = ast_sibling(child))
  {
    bool declares = record_uses(c, site, child);
    if(sequence)
    {
      gc_statement_t* stmt = index_statement(site, child, previous);
      if(declares)
        previous = stmt;
    }
    declares_local |= declares;
  }
  return declares_local;
}

// A nested loop can revisit an earlier use in the same top-level statement.
static gc_liveness_t* method_liveness(compile_t* c, compile_frame_t* f)
{
  if(f->gc_liveness == NULL)
  {
    gc_liveness_t* live = POOL_ALLOC(gc_liveness_t);
    f->gc_liveness = live;
    ponyint_hashmap_init(&live->uses, 0);
    ponyint_hashmap_init(&live->statements, 0);
    root_site_t pos = {live, 0};
    gc_statement_t* previous = NULL;
    ast_t* body = ast_childidx(f->reify->ast, 7);
    for(ast_t* s = ast_child(body); s != NULL; s = ast_sibling(s))
    {
      gc_statement_t* stmt = index_statement(pos, s, previous);
      if(record_uses(c, pos, s))
        previous = stmt;
      pos.statement++;
    }
  }
  return f->gc_liveness;
}

static gc_use_t* get_use(gc_liveness_t* live, const char* name)
{
  gc_use_t key = {name, 0, 0, false};
  size_t index = HASHMAP_UNKNOWN;
  return (gc_use_t*)ponyint_hashmap_get(&live->uses, &key,
    ponyint_hash_ptr(name), same_name, &index);
}

static bool local_only_borrowed(compile_t* c, const char* name)
{
  gc_use_t* use = get_use(method_liveness(c, c->frame->function), name);
  return use == NULL || !use->may_escape;
}

typedef struct stack_root_t
{
  LLVMValueRef slot;
  ast_t* type;
  struct stack_root_t* next;
} stack_root_t;

static void free_roots(stack_root_t* roots)
{
  while(roots != NULL)
  {
    stack_root_t* next = roots->next;
    ast_free_unattached(roots->type);
    POOL_FREE(stack_root_t, roots);
    roots = next;
  }
}

static bool traceable_slot(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(type);
      if(unfolded == NULL)
        return false;
      bool traceable = traceable_slot(unfolded);
      ast_free_unattached(unfolded);
      return traceable;
    }
    case TK_NOMINAL:
      return !is_pointer(type) && !is_nullable_pointer(type) &&
        ast_id((ast_t*)ast_data(type)) != TK_STRUCT;
    case TK_TUPLETYPE:
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      for(ast_t* t = ast_child(type); t != NULL; t = ast_sibling(t))
      {
        if(!traceable_slot(t))
          return false;
      }
      return true;
    default:
      return false;
  }
}

static bool add_root(compile_t* c, root_site_t position, const char* name,
  ast_t* type, stack_root_t** roots)
{
  bool eligible = traceable_slot(type);
  bool actor = ast_id(type) == TK_NOMINAL &&
    ast_id((ast_t*)ast_data(type)) == TK_ACTOR;
  if(eligible && !actor && gentrace_needed(c, type, type))
  {
    gc_use_t* use = get_use(position.liveness, name);
    LLVMValueRef slot = codegen_getlocal(c, name);
    eligible = slot != NULL &&
      (use == NULL || use->first_consume > position.statement);
    if(eligible && use != NULL && use->last_mention >= position.statement)
    {
      stack_root_t* r = POOL_ALLOC(stack_root_t);
      r->slot = slot;
      r->type = type;
      r->next = *roots;
      *roots = r;
      return true;
    }
  }
  ast_free_unattached(type);
  return eligible;
}

static bool add_decl(compile_t* c, root_site_t position, ast_t* decl,
  stack_root_t** roots)
{
  ast_t* id = ast_child(decl);
  ast_t* type = ast_id(decl) == TK_PARAM ? ast_childidx(decl, 1) :
    ast_type(id);
  return add_root(c, position, ast_name(id),
    deferred_reify(c->frame->reify, type, c->opt), roots);
}

static bool previous_roots(compile_t* c, root_site_t position, ast_t* seq,
  ast_t* site, stack_root_t** roots)
{
  if(ast_child(seq) == site)
    return true;
  gc_statement_t* current = get_statement(position.liveness, site);
  pony_assert(current != NULL);
  for(gc_statement_t* prior = current->previous_declaration; prior != NULL;
    prior = prior->previous_declaration)
  {
    ast_t* stmt = prior->ast;
    if(ast_id(stmt) == TK_ASSIGN)
    {
      AST_GET_CHILDREN(stmt, left, right);
      if(ast_id(left) == TK_LET || ast_id(left) == TK_VAR)
      {
        if(contains_local_in_scope(c, right) ||
          !add_decl(c, position, left, roots))
          return false;
        continue;
      }
    }
    if(contains_local_in_scope(c, stmt))
      return false;
  }
  return true;
}

static bool pattern_roots(compile_t* c, root_site_t position, ast_t* pattern,
  stack_root_t** roots)
{
  if(ast_id(pattern) == TK_MATCH_CAPTURE)
    return add_decl(c, position, pattern, roots);

  if(ast_id(pattern) != TK_TUPLE && ast_id(pattern) != TK_SEQ)
    return !contains_local(pattern);

  for(ast_t* p = ast_child(pattern); p != NULL; p = ast_sibling(p))
  {
    if(!pattern_roots(c, position, p, roots))
      return false;
  }
  return true;
}

static bool collect_roots(compile_t* c, ast_t* site, stack_root_t** roots)
{
  compile_frame_t* f = c->frame->function;
  if(f->gc_actor == NULL)
    return false;
  ast_t* method = f->reify->ast;
  ast_t* body = ast_childidx(method, 7);
  ast_t* through = site;
  while(through != NULL && ast_parent(through) != body)
    through = ast_parent(through);
  if(through == NULL)
    return false;
  if(ast_has_annotation(method, "noalloc", c->opt->strtab))
    return false;
  if(ast_id(site) == TK_WHILE && contains_local(ast_child(site)))
    return false;

  gc_liveness_t* live = method_liveness(c, f);
  root_site_t position = {live, get_statement(live, through)->index};
  if(f->gc_receiver != NULL &&
    !add_root(c, position, c->str_this, ast_dup(f->gc_receiver), roots))
    return false;
  ast_t* params = ast_childidx(method, 3);
  for(ast_t* param = ast_child(params); param != NULL; param = ast_sibling(param))
  {
    if(!add_decl(c, position, param, roots))
      return false;
  }

  while(true)
  {
    ast_t* seq = ast_parent(site);
    if(seq == NULL)
      return false;
    if((ast_id(seq) == TK_ASSIGN || ast_id(seq) == TK_RECOVER) &&
      site == ast_childidx(seq, 1))
    {
      site = seq;
      continue;
    }
    if(ast_id(seq) == TK_MATCH && site == ast_child(seq))
    {
      site = seq;
      continue;
    }
    if(ast_id(seq) != TK_SEQ || !previous_roots(c, position, seq, site, roots))
      return false;
    if(seq == body)
      return true;
    site = ast_parent(seq);
    switch(ast_id(site))
    {
      case TK_SEQ:
        site = seq;
        continue;
      case TK_POSITIONALARGS:
      {
        gc_argument_t* arg = c->gc_arguments;
        while(arg != NULL && arg->expression != seq)
          arg = arg->previous;
        if(arg == NULL)
          return false;
        site = ast_parent(site);
        break;
      }
      case TK_IF:
      case TK_WHILE:
        if(seq == ast_child(site) || contains_local(ast_child(site)))
          return false;
        break;
      case TK_TRY:
      case TK_TRY_NO_CHECK:
        if(seq == ast_childidx(site, 2))
          return false;
        break;
      case TK_RECOVER:
        break;
      case TK_CASE:
      {
        AST_GET_CHILDREN(site, pattern, guard, arm);
        if(seq != arm || contains_local(guard) ||
          !pattern_roots(c, position, pattern, roots))
          return false;
        site = ast_parent(ast_parent(site));
        if(contains_local(ast_child(site)))
          return false;
        break;
      }
      case TK_MATCH:
        if(seq != ast_child(site) && (seq != ast_childidx(site, 2) ||
          contains_local(ast_child(site))))
          return false;
        break;
      case TK_REPEAT:
        if(seq != ast_child(site))
          return false;
        break;
      default:
        return false;
    }
  }
}

// A generated root is borrowed until the synchronous collection or nested
// call returns. Keep this use distinct from program uses during stack
// promotion. OptimizeGcCalls removes the identity afterwards.
static LLVMValueRef borrow_root(compile_t* c, LLVMValueRef value)
{
  if(!c->opt->release || LLVMTypeOf(value) != c->ptr)
    return value;

  LLVMTypeRef fn = LLVMFunctionType(c->ptr, &c->ptr, 1, false);
  LLVMValueRef identity = LLVMGetInlineAsm(fn, "", 0, "=r,0", 4,
    true, false, LLVMInlineAsmDialectATT, false);
  LLVMValueRef borrow = LLVMBuildCall2(c->builder, fn, identity, &value, 1,
    "gc_root");
  LLVMSetMetadataStr(borrow, "pony.gc.borrow",
    LLVMMDNodeInContext(c->context, NULL, 0));
  return borrow;
}

static void trace_value(compile_t* c, LLVMValueRef ctx, LLVMValueRef value,
  ast_t* type)
{
  reach_type_t* t = reach_type(c->reach, type, c->opt);
  compile_type_t* ct = (compile_type_t*)t->c_type;
  value = gen_assign_cast(c, ct->use_type, value, type);
  gentrace(c, ctx, value, value, type, type);
}

static void finish_with_roots(compile_t* c, LLVMValueRef ctx, stack_root_t* roots)
{
  if(roots == NULL)
  {
    gencall_runtime(c, "ponyint_gc_finish", &ctx, 1, "");
    return;
  }

  unsigned n = 1;
  for(stack_root_t* r = roots; r != NULL; r = r->next)
    n++;
  size_t bytes = n * sizeof(LLVMTypeRef);
  LLVMTypeRef* params = (LLVMTypeRef*)ponyint_pool_alloc_size(bytes);
  size_t args_size = n * sizeof(LLVMValueRef);
  LLVMValueRef* args = (LLVMValueRef*)ponyint_pool_alloc_size(args_size);
  params[0] = c->ptr;
  args[0] = ctx;
  unsigned i = 1;
  for(stack_root_t* r = roots; r != NULL; r = r->next, i++)
  {
    params[i] = LLVMGetAllocatedType(r->slot);
    args[i] = borrow_root(c, LLVMBuildLoad2(c->builder, params[i], r->slot, ""));
  }
  LLVMTypeRef type = LLVMFunctionType(c->void_type, params, n, false);
  LLVMValueRef fun = LLVMAddFunction(c->module, "gc_finish_roots", type);
  LLVMSetLinkage(fun, LLVMPrivateLinkage);
  LLVM_DECLARE_ATTRIBUTEREF(cold, cold, 0);
  LLVM_DECLARE_ATTRIBUTEREF(noinline_attr, noinline, 0);
  LLVM_DECLARE_ATTRIBUTEREF(scoped, captures, 0);
  LLVMAddAttributeAtIndex(fun, LLVMAttributeFunctionIndex, cold);
  LLVMAddAttributeAtIndex(fun, LLVMAttributeFunctionIndex, noinline_attr);
  LLVMBasicBlockRef caller = LLVMGetInsertBlock(c->builder);
  LLVMMetadataRef location = LLVMGetCurrentDebugLocation2(c->builder);
  LLVMSetCurrentDebugLocation2(c->builder, NULL);
  codegen_startfun(c, fun, NULL, NULL, NULL, false);
  LLVMValueRef marking = LLVMGetParam(fun, 0);
  i = 1;
  for(stack_root_t* r = roots; r != NULL; r = r->next, i++)
  {
    if(params[i] == c->ptr)
      LLVMAddAttributeAtIndex(fun, i + 1, scoped);
    trace_value(c, marking, LLVMGetParam(fun, i), r->type);
  }
  gencall_runtime(c, "ponyint_gc_finish", &marking, 1, "");
  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
  LLVMPositionBuilderAtEnd(c->builder, caller);
  LLVMSetCurrentDebugLocation2(c->builder, location);
  LLVMBuildCall2(c->builder, type, fun, args, n, "");
  ponyint_pool_free_size(args_size, args);
  ponyint_pool_free_size(bytes, params);
}

bool gengc_poll(compile_t* c, ast_t* statement)
{
  stack_root_t* roots = NULL;
  bool eligible = collect_roots(c, statement, &roots);
  if(eligible)
  {
    compile_frame_t* f = c->frame->function;
    LLVMValueRef args[] = {f->gc_actor,
      f->gc_parent != NULL ? f->gc_parent : LLVMConstNull(c->ptr)};
    LLVMValueRef ctx = gencall_runtime(c, "ponyint_gc_poll", args, 2, "");
    LLVMBasicBlockRef marking = codegen_block(c, "gc_trace");
    LLVMBasicBlockRef resume = codegen_block(c, "gc_resume");
    LLVMBuildCondBr(c->builder, LLVMBuildIsNotNull(c->builder, ctx, ""),
      marking, resume);
    LLVMPositionBuilderAtEnd(c->builder, marking);
    finish_with_roots(c, ctx, roots);
    LLVMBuildBr(c->builder, resume);
    LLVMPositionBuilderAtEnd(c->builder, resume);
  }
  free_roots(roots);
  return eligible;
}

static LLVMValueRef frame_slot(compile_t* c, LLVMTypeRef type,
  LLVMValueRef frame, unsigned index)
{
  LLVMValueRef offsets[] = {LLVMConstInt(c->i32, 0, false),
    LLVMConstInt(c->i32, index, false)};
  return LLVMBuildInBoundsGEP2(c->builder, type, frame, offsets, 2, "");
}

static bool native_signature(LLVMValueRef fun)
{
  LLVMTypeRef ret = LLVMGetReturnType(LLVMGlobalGetValueType(fun));
  return LLVMCountParams(fun) <= 6 && LLVMGetTypeKind(ret) != LLVMStructTypeKind;
}

static LLVMValueRef caller_frame(compile_t* c, LLVMValueRef parent,
  stack_root_t* roots, LLVMMetadataRef* layout, LLVMValueRef target,
  LLVMValueRef* storage)
{
  if(roots == NULL)
    return parent;
  unsigned n = 2;
  for(stack_root_t* r = roots; r != NULL; r = r->next)
    n++;
  size_t bytes = n * sizeof(LLVMTypeRef);
  LLVMTypeRef* fields = (LLVMTypeRef*)ponyint_pool_alloc_size(bytes);
  fields[0] = c->ptr;
  fields[1] = c->ptr;
  unsigned index = 2;
  for(stack_root_t* r = roots; r != NULL; r = r->next)
    fields[index++] = LLVMGetAllocatedType(r->slot);
  LLVMTypeRef type = LLVMStructTypeInContext(c->context, fields, n, false);
  ponyint_pool_free_size(bytes, fields);
  LLVMBasicBlockRef caller = LLVMGetInsertBlock(c->builder);
  LLVMMetadataRef location = LLVMGetCurrentDebugLocation2(c->builder);
  LLVMPositionBuilderBefore(c->builder,
    LLVMGetFirstInstruction(LLVMGetEntryBasicBlock(codegen_fun(c))));
  LLVMSetCurrentDebugLocation2(c->builder, NULL);
  LLVMValueRef frame = LLVMBuildAlloca(c->builder, type, "gc_frame");
  *storage = frame;
  LLVMValueRef trace = LLVMAddFunction(c->module, "gc_trace_frame", c->trace_fn);
  LLVMSetLinkage(trace, LLVMPrivateLinkage);
  if(c->opt->release && target_is_linux(c->opt->triple) &&
    target_is_x86(c->opt->triple) && !target_is_ilp32(c->opt->triple) &&
    native_signature(target) && native_signature(codegen_fun(c)))
  {
    LLVMMetadataRef fields_md[] = {LLVMValueAsMetadata(trace),
      LLVMValueAsMetadata(LLVMConstInt(c->i32, n, false))};
    *layout = LLVMMDNodeInContext2(c->context, fields_md, 2);
  }
  codegen_startfun(c, trace, NULL, NULL, NULL, false);
  LLVMValueRef ctx = LLVMGetParam(trace, 0);
  LLVMValueRef record = LLVMGetParam(trace, 1);
  index = 2;
  for(stack_root_t* r = roots; r != NULL; r = r->next)
  {
    LLVMValueRef field = frame_slot(c, type, record, index++);
    trace_value(c, ctx, LLVMBuildLoad2(c->builder,
      LLVMGetAllocatedType(r->slot), field, ""), r->type);
  }
  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);

  LLVMPositionBuilderAtEnd(c->builder, caller);
  LLVMSetCurrentDebugLocation2(c->builder, location);
  gencall_lifetime_start(c, frame);
  LLVMBuildStore(c->builder, parent, frame_slot(c, type, frame, 0));
  LLVMBuildStore(c->builder, trace, frame_slot(c, type, frame, 1));
  index = 2;
  for(stack_root_t* r = roots; r != NULL; r = r->next)
  {
    LLVMValueRef value = LLVMBuildLoad2(c->builder,
      LLVMGetAllocatedType(r->slot), r->slot, "");
    LLVMBuildStore(c->builder, borrow_root(c, value),
      frame_slot(c, type, frame, index++));
  }
  if(*layout != NULL)
  {
    // The link-time choice can replace this address with a tagged frame
    // pointer. Earlier optimizers must not infer alignment from the alloca.
    LLVMTypeRef fn = LLVMFunctionType(c->ptr, &c->ptr, 1, false);
    LLVMValueRef identity = LLVMGetInlineAsm(fn, "", 0, "=r,0", 4,
      false, false, LLVMInlineAsmDialectATT, false);
    return LLVMBuildCall2(c->builder, fn, identity, &frame, 1, "gc_record");
  }
  return frame;
}

bool gengc_allocating_expr(ast_t* expr)
{
  switch(ast_id(expr))
  {
    case TK_CALL:
    {
      token_id ref = ast_id(ast_child(expr));
      if(ref != TK_BEREF && ref != TK_BECHAIN)
        return true;
      // A send allocates its message from the pool. Its arguments and
      // receiver may still allocate on the sending actor's heap.
      break;
    }
    case TK_WHILE:
    case TK_REPEAT:
      return true;
    default: {}
  }
  for(ast_t* child = ast_child(expr); child != NULL; child = ast_sibling(child))
  {
    if(gengc_allocating_expr(child))
      return true;
  }
  return false;
}

static bool method_eligible(compile_t* c, reach_type_t* t, reach_method_t* m)
{
  if(m->intrinsic || m->forwarding || m->cap == TK_AT ||
    ast_id(m->fun->ast) != TK_FUN ||
    ast_name(ast_childidx(m->fun->ast, 1)) == c->str__final ||
    ast_has_annotation(m->fun->ast, "noalloc", c->opt->strtab))
    return false;
  AST_GET_CHILDREN(t->ast, module, id);
  if(ast_name(module) == c->str_builtin &&
    (ast_name(id) == c->str_Array || ast_name(id) == c->str_String))
    return false;
  return gengc_allocating_expr(ast_childidx(m->fun->ast, 7));
}

bool gengc_method_eligible(compile_t* c, reach_type_t* t, reach_method_t* m)
{
  compile_method_t* cm = (compile_method_t*)m->c_method;
  if(!cm->gc_eligibility_checked)
  {
    cm->gc_eligible = method_eligible(c, t, m);
    cm->gc_eligibility_checked = true;
  }
  return cm->gc_eligible;
}

static bool has_enclosing_poll(compile_t* c)
{
  for(compile_frame_t* f = c->frame; !f->is_function; f = f->prev)
  {
    if(f->gc_loop_poll)
      return true;
  }
  return false;
}

LLVMValueRef gengc_call(compile_t* c, ast_t* site, reach_type_t* t,
  reach_method_t* m, LLVMValueRef* args, unsigned n)
{
  if(c->frame->function->gc_actor == NULL || m->cap == TK_AT ||
    ast_id(m->fun->ast) != TK_FUN || contains_local(site))
    return NULL;
  stack_root_t* roots = NULL;
  if(!collect_roots(c, site, &roots))
  {
    free_roots(roots);
    return NULL;
  }
  LLVMValueRef target = NULL;
  switch(t->underlying)
  {
    case TK_CLASS:
    case TK_ACTOR:
    case TK_PRIMITIVE:
      if(gengc_method_eligible(c, t, m))
        target = genfun_gc_target(c, t, m);
      break;
    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      target = gencall_gc_dispatch(c, t, m, has_enclosing_poll(c));
      break;
    default: {}
  }
  if(target == NULL)
  {
    free_roots(roots);
    return NULL;
  }
  compile_frame_t* f = c->frame->function;
  LLVMValueRef parent = f->gc_parent != NULL ? f->gc_parent :
    LLVMConstNull(c->ptr);
  size_t bytes = (n + 2) * sizeof(LLVMValueRef);
  LLVMValueRef* values = (LLVMValueRef*)ponyint_pool_alloc_size(bytes);
  memcpy(values, args, n * sizeof(LLVMValueRef));
  values[n] = f->gc_actor;
  LLVMMetadataRef layout = NULL;
  LLVMValueRef storage = NULL;
  values[n + 1] = caller_frame(c, parent, roots, &layout, target, &storage);
  free_roots(roots);
  LLVMValueRef result = codegen_call(c, LLVMGlobalGetValueType(target),
    target, values, n + 2, true);
  if(storage != NULL)
    gencall_lifetime_end(c, storage);
  if(layout != NULL && LLVMIsACallInst(result) != NULL)
    LLVMSetMetadata(result, LLVMGetMDKindIDInContext(c->context,
      "pony.gc.frame", 13), LLVMMetadataAsValue(c->context, layout));
  ponyint_pool_free_size(bytes, values);
  return result;
}
