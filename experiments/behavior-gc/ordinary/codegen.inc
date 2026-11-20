#include <stdlib.h>
#include <string.h>

static bool ordinary_body(compile_t* c, ast_t* seq)
{
  const char* selected = getenv("ORDINARY_SOURCE");
  if(selected == NULL || c->opt->gc_root_audit || c->opt->mutate)
    return false;
  ast_t* method = ast_parent(seq);
  if(method == NULL || ast_id(method) != TK_BE || ast_childidx(method, 7) != seq
    || ast_childcount(ast_childidx(method, 3)) != 0 || ast_childcount(seq) < 2
    || ast_has_annotation(method, "noalloc", c->opt->strtab))
    return false;
  source_t* src = ast_source(method);
  if(src == NULL || src->file == NULL || strcmp(src->file, selected) != 0)
    return false;
  for(ast_t* stmt = ast_child(seq); stmt; stmt = ast_sibling(stmt))
  {
    if(ast_id(stmt) != TK_CALL)
      return false;
    AST_GET_CHILDREN(stmt, postfix, args, named, question);
    ast_t* type = ast_type(stmt);
    if(ast_id(postfix) != TK_FUNREF || ast_id(ast_child(postfix)) != TK_THIS
      || ast_id(ast_childidx(postfix, 1)) != TK_ID || ast_childcount(args) != 0
      || ast_id(named) != TK_NONE || ast_id(question) != TK_NONE
      || type == NULL || ast_id(type) != TK_NOMINAL
      || ast_name(ast_child(type)) != c->str_builtin
      || strcmp(ast_name(ast_childidx(type, 1)), "None") != 0)
      return false;
  }
  return true;
}

static void ordinary_emit(compile_t* c, ast_t* next)
{
  const char* name = "ponyint_ordinary_poll";
  if(LLVMGetNamedFunction(c->module, name) == NULL)
  {
    LLVMTypeRef params[] = {c->ptr, c->ptr, c->i32};
    LLVMAddFunction(c->module, name,
      LLVMFunctionType(LLVMVoidTypeInContext(c->context), params, 3, false));
  }
  LLVMValueRef args[] = {codegen_ctx(c), LLVMGetParam(codegen_fun(c), 0),
    LLVMConstInt(c->i32, ast_line(next), false)};
  gencall_runtime(c, name, args, 3, "");
}
