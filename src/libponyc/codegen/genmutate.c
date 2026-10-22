#include "genmutate.h"
#include "codegen.h"
#include "genexpr.h"
#include "genoperator.h"
#include "../ast/ast.h"
#include "../ast/source.h"
#include "../pass/pass.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"

#include <stdio.h>
#include <string.h>

#define INITIAL_ALLOC 64

void mutate_init(mutate_state_t* state)
{
  state->mutants = (mutant_record_t*)ponyint_pool_alloc_size(
    INITIAL_ALLOC * sizeof(mutant_record_t));
  state->mutant_count = 0;
  state->mutant_alloc = INITIAL_ALLOC;
  state->next_id = 1;
}

void mutate_cleanup(mutate_state_t* state)
{
  if(state->mutants != NULL)
  {
    ponyint_pool_free_size(
      state->mutant_alloc * sizeof(mutant_record_t), state->mutants);
    state->mutants = NULL;
  }
  state->mutant_count = 0;
  state->mutant_alloc = 0;
}

int32_t mutate_next_id(mutate_state_t* state)
{
  return state->next_id++;
}

void mutate_record(mutate_state_t* state, int32_t id,
  const char* file, size_t line, size_t column,
  const char* category, const char* original,
  const char* replacement)
{
  if(state->mutant_count >= state->mutant_alloc)
  {
    size_t old_size = state->mutant_alloc * sizeof(mutant_record_t);
    state->mutant_alloc *= 2;
    size_t new_size = state->mutant_alloc * sizeof(mutant_record_t);
    mutant_record_t* new_buf = (mutant_record_t*)ponyint_pool_alloc_size(
      new_size);
    memcpy(new_buf, state->mutants, old_size);
    ponyint_pool_free_size(old_size, state->mutants);
    state->mutants = new_buf;
  }

  mutant_record_t* r = &state->mutants[state->mutant_count++];
  r->id = id;
  r->file = file;
  r->line = line;
  r->column = column;
  r->category = category;
  r->original = original;
  r->replacement = replacement;
}

static size_t json_escape(char* buf, size_t buf_size, const char* str)
{
  size_t pos = 0;
  for(const char* p = str; *p != '\0' && pos < buf_size - 1; p++)
  {
    switch(*p)
    {
      case '\\':
        if(pos + 2 >= buf_size) goto done;
        buf[pos++] = '\\'; buf[pos++] = '\\';
        break;
      case '"':
        if(pos + 2 >= buf_size) goto done;
        buf[pos++] = '\\'; buf[pos++] = '"';
        break;
      case '\n':
        if(pos + 2 >= buf_size) goto done;
        buf[pos++] = '\\'; buf[pos++] = 'n';
        break;
      case '\t':
        if(pos + 2 >= buf_size) goto done;
        buf[pos++] = '\\'; buf[pos++] = 't';
        break;
      case '\r':
        if(pos + 2 >= buf_size) goto done;
        buf[pos++] = '\\'; buf[pos++] = 'r';
        break;
      default:
        if((unsigned char)*p < 0x20)
        {
          if(pos + 6 >= buf_size) goto done;
          pos += snprintf(buf + pos, buf_size - pos, "\\u%04x",
            (unsigned char)*p);
        }
        else
        {
          buf[pos++] = *p;
        }
        break;
    }
  }
done:
  buf[pos] = '\0';
  return pos;
}

static void write_escaped(FILE* f, const char* str)
{
  size_t len = strlen(str);
  size_t buf_size = len * 6 + 1;
  char* buf = (char*)ponyint_pool_alloc_size(buf_size);
  json_escape(buf, buf_size, str);
  fprintf(f, "\"%s\"", buf);
  ponyint_pool_free_size(buf_size, buf);
}

bool mutate_write_info(mutate_state_t* state, const char* output_dir,
  const char* bin_name)
{
  size_t dir_len = strlen(output_dir);
  size_t name_len = strlen(bin_name);
  size_t path_len = dir_len + 1 + name_len + 14;
  char* path = (char*)ponyint_pool_alloc_size(path_len + 1);
#ifdef PLATFORM_IS_WINDOWS
  snprintf(path, path_len + 1, "%s\\%s.mutants.json", output_dir, bin_name);
#else
  snprintf(path, path_len + 1, "%s/%s.mutants.json", output_dir, bin_name);
#endif

  FILE* f = fopen(path, "w");
  ponyint_pool_free_size(path_len + 1, path);

  if(f == NULL)
    return false;

  fprintf(f, "{\n");
  fprintf(f, "  \"version\": 1,\n");
  fprintf(f, "  \"binary\": ");
  write_escaped(f, bin_name);
  fprintf(f, ",\n");
  fprintf(f, "  \"total_mutants\": %zu,\n", state->mutant_count);
  fprintf(f, "  \"mutants\": [");

  for(size_t i = 0; i < state->mutant_count; i++)
  {
    mutant_record_t* r = &state->mutants[i];
    if(i > 0) fprintf(f, ",");
    fprintf(f, "\n    {");

    fprintf(f, "\"id\": %d, ", r->id);
    fprintf(f, "\"file\": ");
    write_escaped(f, r->file);
    fprintf(f, ", ");
    fprintf(f, "\"line\": %zu, ", r->line);
    fprintf(f, "\"column\": %zu, ", r->column);
    fprintf(f, "\"category\": ");
    write_escaped(f, r->category);
    fprintf(f, ", ");
    fprintf(f, "\"original\": ");
    write_escaped(f, r->original);
    fprintf(f, ", ");
    fprintf(f, "\"replacement\": ");
    write_escaped(f, r->replacement);

    fprintf(f, "}");
  }

  if(state->mutant_count > 0)
    fprintf(f, "\n  ");
  fprintf(f, "]\n}\n");

  fclose(f);
  return true;
}

static uint8_t category_to_bit(const char* category)
{
  if(strcmp(category, "aor") == 0) return MUTATE_OP_AOR;
  if(strcmp(category, "bor") == 0) return MUTATE_OP_BOR;
  if(strcmp(category, "ror") == 0) return MUTATE_OP_ROR;
  if(strcmp(category, "sor") == 0) return MUTATE_OP_SOR;
  if(strcmp(category, "uoi") == 0) return MUTATE_OP_UOI;
  return 0;
}

static bool scope_matches(pass_opt_t* opt, const char* file, size_t line)
{
  size_t file_len = strlen(file);

  for(size_t i = 0; i < opt->mutate_scope_count; i++)
  {
    mutate_scope_t* scope = &opt->mutate_scopes[i];
    size_t scope_len = strlen(scope->file);

    if(scope_len > file_len)
      continue;

    const char* tail = file + (file_len - scope_len);

    if(strcmp(tail, scope->file) != 0)
      continue;

    // If a prefix exists, it must be preceded by a path separator.
    if((scope_len < file_len) && (tail[-1] != '/') && (tail[-1] != '\\'))
      continue;

    if((scope->start_line == 0) && (scope->end_line == 0))
      return true;

    if((line >= scope->start_line) && (line <= scope->end_line))
      return true;
  }

  return false;
}

bool mutate_site_enabled(compile_t* c, const char* category, ast_t* ast)
{
  if(ast == NULL)
    return false;

  uint8_t bit = category_to_bit(category);
  if((c->opt->mutate_operators & bit) == 0)
    return false;

  if(c->opt->mutate_scope_count > 0)
  {
    source_t* source = ast_source(ast);
    const char* file =
      ((source != NULL) && (source->file != NULL)) ? source->file : "";
    size_t line = ast_line(ast);

    if(!scope_matches(c->opt, file, line))
      return false;
  }

  return true;
}

LLVMValueRef mutate_binop(compile_t* c, ast_t* ast,
  ast_t* left, ast_t* right,
  LLVMValueRef (*orig_f)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*),
  LLVMValueRef (*orig_i)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*),
  const char* original, const char* category,
  mutate_variant_t* variants, size_t count)
{
  pony_assert(count > 0);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  bool fp = is_fp(l_value);
  LLVMTypeRef result_type = LLVMTypeOf(l_value);

  source_t* source = ast_source(ast);
  const char* file =
    ((source != NULL) && (source->file != NULL)) ? source->file : "";
  size_t line = ast_line(ast);
  size_t pos = ast_pos(ast);

  int32_t* ids = (int32_t*)ponyint_pool_alloc_size(count * sizeof(int32_t));

  for(size_t i = 0; i < count; i++)
  {
    ids[i] = mutate_next_id(&c->mutate);
    mutate_record(&c->mutate, ids[i], file, line, pos,
      category, original, variants[i].replacement);
  }

  LLVMValueRef selector = codegen_resolve_global(c, c->mutant_selector);
  LLVMValueRef sel = LLVMBuildLoad2(c->builder, c->i32,
    selector, "mut.sel");
  LLVMBasicBlockRef orig_bb = codegen_block(c, "mut.orig");
  LLVMBasicBlockRef merge_bb = codegen_block(c, "mut.merge");

  LLVMValueRef sw = LLVMBuildSwitch(c->builder, sel, orig_bb,
    (unsigned)count);

  LLVMBasicBlockRef* var_bbs =
    (LLVMBasicBlockRef*)ponyint_pool_alloc_size(
      count * sizeof(LLVMBasicBlockRef));

  for(size_t i = 0; i < count; i++)
  {
    var_bbs[i] = codegen_block(c, "mut.var");
    LLVMAddCase(sw, LLVMConstInt(c->i32, ids[i], false), var_bbs[i]);
  }

  LLVMPositionBuilderAtEnd(c->builder, orig_bb);
  LLVMValueRef orig_result = fp
    ? orig_f(c->builder, l_value, r_value, "")
    : orig_i(c->builder, l_value, r_value, "");
  LLVMBuildBr(c->builder, merge_bb);

  // count + 1: original + variants
  size_t phi_count = count + 1;
  LLVMValueRef* phi_vals =
    (LLVMValueRef*)ponyint_pool_alloc_size(phi_count * sizeof(LLVMValueRef));
  LLVMBasicBlockRef* phi_bbs =
    (LLVMBasicBlockRef*)ponyint_pool_alloc_size(
      phi_count * sizeof(LLVMBasicBlockRef));

  phi_vals[0] = orig_result;
  phi_bbs[0] = orig_bb;

  for(size_t i = 0; i < count; i++)
  {
    LLVMPositionBuilderAtEnd(c->builder, var_bbs[i]);
    LLVMValueRef var_result = fp
      ? variants[i].build_f(c->builder, l_value, r_value, "")
      : variants[i].build_i(c->builder, l_value, r_value, "");
    LLVMBuildBr(c->builder, merge_bb);
    phi_vals[i + 1] = var_result;
    phi_bbs[i + 1] = var_bbs[i];
  }

  LLVMPositionBuilderAtEnd(c->builder, merge_bb);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, result_type, "mut.result");
  LLVMAddIncoming(phi, phi_vals, phi_bbs, (unsigned)phi_count);

  ponyint_pool_free_size(count * sizeof(int32_t), ids);
  ponyint_pool_free_size(count * sizeof(LLVMBasicBlockRef), var_bbs);
  ponyint_pool_free_size(phi_count * sizeof(LLVMValueRef), phi_vals);
  ponyint_pool_free_size(phi_count * sizeof(LLVMBasicBlockRef), phi_bbs);

  return phi;
}

LLVMValueRef mutate_divrem(compile_t* c, ast_t* ast,
  ast_t* left, ast_t* right,
  LLVMValueRef (*orig_f)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*),
  LLVMValueRef (*orig_ui)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*),
  LLVMValueRef (*orig_si)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*),
  bool safe, bool is_div,
  const char* original, const char* category,
  mutate_divrem_variant_t* variants, size_t count)
{
  pony_assert(count > 0);

  ast_t* type = deferred_reify(c->frame->reify, ast_type(left), c->opt);
  bool sign = is_signed(type);
  ast_free_unattached(type);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  LLVMTypeRef result_type = LLVMTypeOf(l_value);

  source_t* source = ast_source(ast);
  const char* file =
    ((source != NULL) && (source->file != NULL)) ? source->file : "";
  size_t line = ast_line(ast);
  size_t pos = ast_pos(ast);

  int32_t* ids = (int32_t*)ponyint_pool_alloc_size(count * sizeof(int32_t));

  for(size_t i = 0; i < count; i++)
  {
    ids[i] = mutate_next_id(&c->mutate);
    mutate_record(&c->mutate, ids[i], file, line, pos,
      category, original, variants[i].replacement);
  }

  LLVMValueRef selector = codegen_resolve_global(c, c->mutant_selector);
  LLVMValueRef sel = LLVMBuildLoad2(c->builder, c->i32,
    selector, "mut.sel");
  LLVMBasicBlockRef orig_bb = codegen_block(c, "mut.orig");
  LLVMBasicBlockRef merge_bb = codegen_block(c, "mut.merge");

  LLVMValueRef sw = LLVMBuildSwitch(c->builder, sel, orig_bb,
    (unsigned)count);

  LLVMBasicBlockRef* var_bbs =
    (LLVMBasicBlockRef*)ponyint_pool_alloc_size(
      count * sizeof(LLVMBasicBlockRef));

  for(size_t i = 0; i < count; i++)
  {
    var_bbs[i] = codegen_block(c, "mut.var");
    LLVMAddCase(sw, LLVMConstInt(c->i32, ids[i], false), var_bbs[i]);
  }

  LLVMPositionBuilderAtEnd(c->builder, orig_bb);
  LLVMValueRef orig_result = make_divrem_inner(c, l_value, r_value, sign,
    orig_f, orig_ui, orig_si, safe, is_div);
  LLVMBuildBr(c->builder, merge_bb);
  LLVMBasicBlockRef orig_exit = LLVMGetInsertBlock(c->builder);

  size_t phi_count = count + 1;
  LLVMValueRef* phi_vals =
    (LLVMValueRef*)ponyint_pool_alloc_size(phi_count * sizeof(LLVMValueRef));
  LLVMBasicBlockRef* phi_bbs =
    (LLVMBasicBlockRef*)ponyint_pool_alloc_size(
      phi_count * sizeof(LLVMBasicBlockRef));

  phi_vals[0] = orig_result;
  phi_bbs[0] = orig_exit;

  for(size_t i = 0; i < count; i++)
  {
    LLVMPositionBuilderAtEnd(c->builder, var_bbs[i]);
    LLVMValueRef var_result = make_divrem_inner(c, l_value, r_value, sign,
      variants[i].build_f, variants[i].build_ui, variants[i].build_si,
      safe, variants[i].is_div);
    LLVMBuildBr(c->builder, merge_bb);
    phi_vals[i + 1] = var_result;
    phi_bbs[i + 1] = LLVMGetInsertBlock(c->builder);
  }

  LLVMPositionBuilderAtEnd(c->builder, merge_bb);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, result_type, "mut.result");
  LLVMAddIncoming(phi, phi_vals, phi_bbs, (unsigned)phi_count);

  ponyint_pool_free_size(count * sizeof(int32_t), ids);
  ponyint_pool_free_size(count * sizeof(LLVMBasicBlockRef), var_bbs);
  ponyint_pool_free_size(phi_count * sizeof(LLVMValueRef), phi_vals);
  ponyint_pool_free_size(phi_count * sizeof(LLVMBasicBlockRef), phi_bbs);

  return phi;
}

LLVMValueRef mutate_cmp_values(compile_t* c, ast_t* ast,
  LLVMValueRef l_value, LLVMValueRef r_value, bool sign,
  LLVMRealPredicate orig_f, LLVMIntPredicate orig_si, LLVMIntPredicate orig_ui,
  const char* original, const char* category, bool safe,
  mutate_cmp_variant_t* variants, size_t count)
{
  pony_assert(count > 0);

  source_t* source = ast_source(ast);
  const char* file =
    ((source != NULL) && (source->file != NULL)) ? source->file : "";
  size_t line = ast_line(ast);
  size_t pos = ast_pos(ast);

  int32_t* ids = (int32_t*)ponyint_pool_alloc_size(count * sizeof(int32_t));

  for(size_t i = 0; i < count; i++)
  {
    ids[i] = mutate_next_id(&c->mutate);
    mutate_record(&c->mutate, ids[i], file, line, pos,
      category, original, variants[i].replacement);
  }

  LLVMValueRef selector = codegen_resolve_global(c, c->mutant_selector);
  LLVMValueRef sel = LLVMBuildLoad2(c->builder, c->i32,
    selector, "mut.sel");
  LLVMBasicBlockRef orig_bb = codegen_block(c, "mut.orig");
  LLVMBasicBlockRef merge_bb = codegen_block(c, "mut.merge");

  LLVMValueRef sw = LLVMBuildSwitch(c->builder, sel, orig_bb,
    (unsigned)count);

  LLVMBasicBlockRef* var_bbs =
    (LLVMBasicBlockRef*)ponyint_pool_alloc_size(
      count * sizeof(LLVMBasicBlockRef));

  for(size_t i = 0; i < count; i++)
  {
    var_bbs[i] = codegen_block(c, "mut.var");
    LLVMAddCase(sw, LLVMConstInt(c->i32, ids[i], false), var_bbs[i]);
  }

  LLVMPositionBuilderAtEnd(c->builder, orig_bb);
  LLVMValueRef orig_result = make_cmp_value(c, sign, l_value, r_value,
    orig_f, orig_si, orig_ui, safe);
  LLVMBuildBr(c->builder, merge_bb);

  size_t phi_count = count + 1;
  LLVMValueRef* phi_vals =
    (LLVMValueRef*)ponyint_pool_alloc_size(phi_count * sizeof(LLVMValueRef));
  LLVMBasicBlockRef* phi_bbs =
    (LLVMBasicBlockRef*)ponyint_pool_alloc_size(
      phi_count * sizeof(LLVMBasicBlockRef));

  phi_vals[0] = orig_result;
  phi_bbs[0] = orig_bb;

  for(size_t i = 0; i < count; i++)
  {
    LLVMPositionBuilderAtEnd(c->builder, var_bbs[i]);
    LLVMValueRef var_result = make_cmp_value(c, sign, l_value, r_value,
      variants[i].cmp_f, variants[i].cmp_si, variants[i].cmp_ui, safe);
    LLVMBuildBr(c->builder, merge_bb);
    phi_vals[i + 1] = var_result;
    phi_bbs[i + 1] = var_bbs[i];
  }

  LLVMPositionBuilderAtEnd(c->builder, merge_bb);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->i1, "mut.result");
  LLVMAddIncoming(phi, phi_vals, phi_bbs, (unsigned)phi_count);

  ponyint_pool_free_size(count * sizeof(int32_t), ids);
  ponyint_pool_free_size(count * sizeof(LLVMBasicBlockRef), var_bbs);
  ponyint_pool_free_size(phi_count * sizeof(LLVMValueRef), phi_vals);
  ponyint_pool_free_size(phi_count * sizeof(LLVMBasicBlockRef), phi_bbs);

  return phi;
}

LLVMValueRef mutate_cmp(compile_t* c, ast_t* ast,
  ast_t* left, ast_t* right,
  LLVMRealPredicate orig_f, LLVMIntPredicate orig_si, LLVMIntPredicate orig_ui,
  const char* original, const char* category, bool safe,
  mutate_cmp_variant_t* variants, size_t count)
{
  ast_t* type = deferred_reify(c->frame->reify, ast_type(left), c->opt);
  bool sign = is_signed(type);
  ast_free_unattached(type);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  return mutate_cmp_values(c, ast, l_value, r_value, sign,
    orig_f, orig_si, orig_ui, original, category, safe, variants, count);
}

LLVMValueRef mutate_unary(compile_t* c, ast_t* ast,
  LLVMValueRef operand, LLVMValueRef original_result,
  const char* original, const char* category,
  mutate_unary_variant_t* variants, size_t count)
{
  pony_assert(count > 0);

  source_t* source = ast_source(ast);
  const char* file =
    ((source != NULL) && (source->file != NULL)) ? source->file : "";
  size_t line = ast_line(ast);
  size_t pos = ast_pos(ast);

  int32_t* ids = (int32_t*)ponyint_pool_alloc_size(count * sizeof(int32_t));

  for(size_t i = 0; i < count; i++)
  {
    ids[i] = mutate_next_id(&c->mutate);
    mutate_record(&c->mutate, ids[i], file, line, pos,
      category, original, variants[i].replacement);
  }

  LLVMValueRef selector = codegen_resolve_global(c, c->mutant_selector);
  LLVMValueRef sel = LLVMBuildLoad2(c->builder, c->i32,
    selector, "mut.sel");
  LLVMBasicBlockRef orig_bb = codegen_block(c, "mut.orig");
  LLVMBasicBlockRef merge_bb = codegen_block(c, "mut.merge");

  LLVMValueRef sw = LLVMBuildSwitch(c->builder, sel, orig_bb,
    (unsigned)count);

  LLVMBasicBlockRef* var_bbs =
    (LLVMBasicBlockRef*)ponyint_pool_alloc_size(
      count * sizeof(LLVMBasicBlockRef));

  for(size_t i = 0; i < count; i++)
  {
    var_bbs[i] = codegen_block(c, "mut.var");
    LLVMAddCase(sw, LLVMConstInt(c->i32, ids[i], false), var_bbs[i]);
  }

  LLVMPositionBuilderAtEnd(c->builder, orig_bb);
  LLVMBuildBr(c->builder, merge_bb);

  LLVMTypeRef result_type = LLVMTypeOf(original_result);

  size_t phi_count = count + 1;
  LLVMValueRef* phi_vals =
    (LLVMValueRef*)ponyint_pool_alloc_size(phi_count * sizeof(LLVMValueRef));
  LLVMBasicBlockRef* phi_bbs =
    (LLVMBasicBlockRef*)ponyint_pool_alloc_size(
      phi_count * sizeof(LLVMBasicBlockRef));

  phi_vals[0] = original_result;
  phi_bbs[0] = orig_bb;

  for(size_t i = 0; i < count; i++)
  {
    LLVMPositionBuilderAtEnd(c->builder, var_bbs[i]);
    LLVMBuildBr(c->builder, merge_bb);
    phi_vals[i + 1] = operand;
    phi_bbs[i + 1] = var_bbs[i];
  }

  LLVMPositionBuilderAtEnd(c->builder, merge_bb);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, result_type, "mut.result");
  LLVMAddIncoming(phi, phi_vals, phi_bbs, (unsigned)phi_count);

  ponyint_pool_free_size(count * sizeof(int32_t), ids);
  ponyint_pool_free_size(count * sizeof(LLVMBasicBlockRef), var_bbs);
  ponyint_pool_free_size(phi_count * sizeof(LLVMValueRef), phi_vals);
  ponyint_pool_free_size(phi_count * sizeof(LLVMBasicBlockRef), phi_bbs);

  return phi;
}

LLVMValueRef mutate_declare_selector(compile_t* c)
{
  LLVMValueRef g = LLVMAddGlobal(c->module, c->i32,
    "pony_mutant_selector");
  LLVMSetLinkage(g, LLVMExternalLinkage);
  return g;
}

LLVMValueRef mutate_declare_enabled(compile_t* c)
{
  LLVMValueRef g = LLVMAddGlobal(c->module, c->i32,
    "pony_mutate_enabled");
  LLVMSetLinkage(g, LLVMExternalLinkage);
  return g;
}
