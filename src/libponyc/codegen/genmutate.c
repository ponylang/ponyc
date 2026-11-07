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
  state->next_coverage_id = 0;
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
  const char* replacement, int32_t coverage_counter)
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
  r->coverage_counter = coverage_counter;
  r->file = file;
  r->line = line;
  r->column = column;
  r->category = category;
  r->original = original;
  r->replacement = replacement;
}

int32_t mutate_find_or_record(mutate_state_t* state,
  const char* file, size_t line, size_t column,
  const char* category, const char* original,
  const char* replacement, int32_t coverage_counter,
  int32_t* out_coverage)
{
  for(size_t i = 0; i < state->mutant_count; i++)
  {
    mutant_record_t* r = &state->mutants[i];
    if((r->line == line) && (r->column == column) &&
      (strcmp(r->file, file) == 0) &&
      (strcmp(r->category, category) == 0) &&
      (strcmp(r->original, original) == 0) &&
      (strcmp(r->replacement, replacement) == 0))
    {
      if(out_coverage != NULL)
        *out_coverage = r->coverage_counter;
      return r->id;
    }
  }

  int32_t id = mutate_next_id(state);
  mutate_record(state, id, file, line, column, category, original,
    replacement, coverage_counter);
  if(out_coverage != NULL)
    *out_coverage = coverage_counter;
  return id;
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
  fprintf(f, "  \"total_coverage_counters\": %d,\n",
    state->next_coverage_id);
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
    fprintf(f, ", ");
    fprintf(f, "\"coverage_counter\": %d", r->coverage_counter);

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
  if(strcmp(category, "cr") == 0) return MUTATE_OP_CR;
  if(strcmp(category, "rvr") == 0) return MUTATE_OP_RVR;
  if(strcmp(category, "vcr") == 0) return MUTATE_OP_VCR;
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

    // Empty scope file matches all files.
    if(scope_len == 0)
    {
      if((scope->start_line == 0) && (scope->end_line == 0))
        return true;

      if((line >= scope->start_line) && (line <= scope->end_line))
        return true;

      continue;
    }

    // Find scope->file as a substring of file on path boundaries.
    const char* p = file;
    bool matched = false;

    while(!matched)
    {
      p = strstr(p, scope->file);
      if(p == NULL)
        break;

      bool left_ok =
        (p == file) || (p[-1] == '/') || (p[-1] == '\\');
      const char* end = p + scope_len;
      bool right_ok =
        (*end == '\0') || (*end == '/') || (*end == '\\');

      if(left_ok && right_ok)
        matched = true;
      else
        p++;
    }

    if(!matched)
      continue;

    if((scope->start_line == 0) && (scope->end_line == 0))
      return true;

    if((line >= scope->start_line) && (line <= scope->end_line))
      return true;
  }

  return false;
}

static bool has_nomutate(ast_t* ast, strtable_t* strtab)
{
  ast_t* p = ast;
  while(p != NULL)
  {
    token_id id = ast_id(p);
    switch(id)
    {
      case TK_FUN:
      case TK_BE:
      case TK_NEW:
      case TK_CLASS:
      case TK_ACTOR:
      case TK_PRIMITIVE:
      case TK_STRUCT:
        if(ast_has_annotation(p, "nomutate", strtab))
          return true;
        break;

      default:
        break;
    }
    p = ast_parent(p);
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

  if(has_nomutate(ast, c->opt->strtab))
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

  size_t prev_count = c->mutate.mutant_count;
  int32_t cov_id = c->mutate.next_coverage_id;

  int32_t* ids = (int32_t*)ponyint_pool_alloc_size(count * sizeof(int32_t));

  for(size_t i = 0; i < count; i++)
  {
    int32_t out_cov;
    ids[i] = mutate_find_or_record(&c->mutate, file, line, pos,
      category, original, variants[i].replacement, cov_id, &out_cov);
    cov_id = out_cov;
  }

  if(c->mutate.mutant_count > prev_count)
    c->mutate.next_coverage_id++;

  mutate_emit_coverage_inc(c, cov_id);

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

  return mutate_cr_binop(c, ast, l_value, r_value, phi, orig_f, orig_i);
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

  size_t prev_count = c->mutate.mutant_count;
  int32_t cov_id = c->mutate.next_coverage_id;

  int32_t* ids = (int32_t*)ponyint_pool_alloc_size(count * sizeof(int32_t));

  for(size_t i = 0; i < count; i++)
  {
    int32_t out_cov;
    ids[i] = mutate_find_or_record(&c->mutate, file, line, pos,
      category, original, variants[i].replacement, cov_id, &out_cov);
    cov_id = out_cov;
  }

  if(c->mutate.mutant_count > prev_count)
    c->mutate.next_coverage_id++;

  mutate_emit_coverage_inc(c, cov_id);

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

  size_t prev_count = c->mutate.mutant_count;
  int32_t cov_id = c->mutate.next_coverage_id;

  int32_t* ids = (int32_t*)ponyint_pool_alloc_size(count * sizeof(int32_t));

  for(size_t i = 0; i < count; i++)
  {
    int32_t out_cov;
    ids[i] = mutate_find_or_record(&c->mutate, file, line, pos,
      category, original, variants[i].replacement, cov_id, &out_cov);
    cov_id = out_cov;
  }

  if(c->mutate.mutant_count > prev_count)
    c->mutate.next_coverage_id++;

  mutate_emit_coverage_inc(c, cov_id);

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

  size_t prev_count = c->mutate.mutant_count;
  int32_t cov_id = c->mutate.next_coverage_id;

  int32_t* ids = (int32_t*)ponyint_pool_alloc_size(count * sizeof(int32_t));

  for(size_t i = 0; i < count; i++)
  {
    int32_t out_cov;
    ids[i] = mutate_find_or_record(&c->mutate, file, line, pos,
      category, original, variants[i].replacement, cov_id, &out_cov);
    cov_id = out_cov;
  }

  if(c->mutate.mutant_count > prev_count)
    c->mutate.next_coverage_id++;

  mutate_emit_coverage_inc(c, cov_id);

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

static LLVMValueRef build_shift(LLVMBuilderRef builder,
  LLVMValueRef l_value, LLVMValueRef shift_amount,
  bool is_shl, bool sign, bool safe)
{
  LLVMValueRef result;

  if(is_shl)
  {
    result = LLVMBuildShl(builder, l_value, shift_amount, "");
    if(!safe)
    {
      if(sign)
        LLVMSetNoSignedWrap(result);
      else
        LLVMSetNoUnsignedWrap(result);
    }
  }
  else
  {
    if(sign)
      result = LLVMBuildAShr(builder, l_value, shift_amount, "");
    else
      result = LLVMBuildLShr(builder, l_value, shift_amount, "");
    if(!safe)
      LLVMSetIsExact(result);
  }

  return result;
}


LLVMValueRef mutate_shift(compile_t* c, ast_t* ast,
  LLVMValueRef l_value, LLVMValueRef r_value,
  bool is_shl, bool sign, bool safe, unsigned width)
{
  LLVMTypeRef l_type = LLVMTypeOf(l_value);

  LLVMValueRef shift_amount = r_value;
  if(safe)
  {
    LLVMTypeRef r_type = LLVMTypeOf(r_value);
    LLVMValueRef clamp = LLVMConstInt(r_type, width - 1, false);
    LLVMValueRef cmp = LLVMBuildICmp(c->builder, LLVMIntULE, r_value,
      clamp, "");
    shift_amount = LLVMBuildSelect(c->builder, cmp, r_value, clamp, "");
  }

  source_t* source = ast_source(ast);
  const char* file =
    ((source != NULL) && (source->file != NULL)) ? source->file : "";
  size_t line = ast_line(ast);
  size_t pos = ast_pos(ast);

  const char* original = is_shl ? "shl" : "shr";
  const char* replacement = is_shl ? "shr" : "shl";

  size_t prev_count = c->mutate.mutant_count;
  int32_t cov_id = c->mutate.next_coverage_id;

  int32_t out_cov;
  int32_t id = mutate_find_or_record(&c->mutate, file, line, pos,
    "sor", original, replacement, cov_id, &out_cov);
  cov_id = out_cov;

  if(c->mutate.mutant_count > prev_count)
    c->mutate.next_coverage_id++;

  mutate_emit_coverage_inc(c, cov_id);

  LLVMValueRef selector = codegen_resolve_global(c, c->mutant_selector);
  LLVMValueRef sel = LLVMBuildLoad2(c->builder, c->i32,
    selector, "mut.sel");
  LLVMBasicBlockRef orig_bb = codegen_block(c, "mut.orig");
  LLVMBasicBlockRef var_bb = codegen_block(c, "mut.var");
  LLVMBasicBlockRef merge_bb = codegen_block(c, "mut.merge");

  LLVMValueRef sw = LLVMBuildSwitch(c->builder, sel, orig_bb, 1);
  LLVMAddCase(sw, LLVMConstInt(c->i32, id, false), var_bb);

  LLVMPositionBuilderAtEnd(c->builder, orig_bb);
  LLVMValueRef orig_result =
    build_shift(c->builder, l_value, shift_amount, is_shl, sign, safe);
  LLVMBuildBr(c->builder, merge_bb);

  LLVMPositionBuilderAtEnd(c->builder, var_bb);
  LLVMValueRef var_result =
    build_shift(c->builder, l_value, shift_amount, !is_shl, sign, safe);
  LLVMBuildBr(c->builder, merge_bb);

  LLVMPositionBuilderAtEnd(c->builder, merge_bb);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, l_type, "mut.result");
  LLVMValueRef phi_vals[] = {orig_result, var_result};
  LLVMBasicBlockRef phi_bbs[] = {orig_bb, var_bb};
  LLVMAddIncoming(phi, phi_vals, phi_bbs, 2);

  return phi;
}


LLVMValueRef mutate_rvr(compile_t* c, ast_t* ast,
  LLVMValueRef original_value)
{
  if(!mutate_site_enabled(c, "rvr", ast))
    return original_value;

  LLVMTypeRef val_type = LLVMTypeOf(original_value);
  LLVMTypeKind kind = LLVMGetTypeKind(val_type);

  #define MAX_RVR_VARIANTS 2
  LLVMValueRef defaults[MAX_RVR_VARIANTS];
  const char* labels[MAX_RVR_VARIANTS];
  size_t count = 0;

  switch(kind)
  {
    case LLVMIntegerTypeKind:
    {
      unsigned width = LLVMGetIntTypeWidth(val_type);
      if(width == 1)
      {
        defaults[count] = LLVMConstInt(val_type, 1, false);
        labels[count] = "true";
        count++;
        defaults[count] = LLVMConstInt(val_type, 0, false);
        labels[count] = "false";
        count++;
      }
      else
      {
        defaults[count] = LLVMConstInt(val_type, 0, false);
        labels[count] = "0";
        count++;
        defaults[count] = LLVMConstInt(val_type, 1, false);
        labels[count] = "1";
        count++;
      }
      break;
    }

    case LLVMHalfTypeKind:
    case LLVMFloatTypeKind:
    case LLVMDoubleTypeKind:
      defaults[count] = LLVMConstReal(val_type, 0.0);
      labels[count] = "0.0";
      count++;
      defaults[count] = LLVMConstReal(val_type, 1.0);
      labels[count] = "1.0";
      count++;
      break;

    default:
      return original_value;
  }

  if(count == 0)
    return original_value;

  source_t* source = ast_source(ast);
  const char* file =
    ((source != NULL) && (source->file != NULL)) ? source->file : "";
  size_t line = ast_line(ast);
  size_t pos = ast_pos(ast);

  size_t prev_count = c->mutate.mutant_count;
  int32_t cov_id = c->mutate.next_coverage_id;

  int32_t* ids = (int32_t*)ponyint_pool_alloc_size(count * sizeof(int32_t));

  for(size_t i = 0; i < count; i++)
  {
    int32_t out_cov;
    ids[i] = mutate_find_or_record(&c->mutate, file, line, pos,
      "rvr", "return", labels[i], cov_id, &out_cov);
    cov_id = out_cov;
  }

  if(c->mutate.mutant_count > prev_count)
    c->mutate.next_coverage_id++;

  mutate_emit_coverage_inc(c, cov_id);

  LLVMValueRef selector = codegen_resolve_global(c, c->mutant_selector);
  LLVMValueRef sel = LLVMBuildLoad2(c->builder, c->i32,
    selector, "mut.sel");
  LLVMBasicBlockRef orig_bb = codegen_block(c, "rvr.orig");
  LLVMBasicBlockRef merge_bb = codegen_block(c, "rvr.merge");

  LLVMValueRef sw = LLVMBuildSwitch(c->builder, sel, orig_bb,
    (unsigned)count);

  LLVMBasicBlockRef* var_bbs =
    (LLVMBasicBlockRef*)ponyint_pool_alloc_size(
      count * sizeof(LLVMBasicBlockRef));

  for(size_t i = 0; i < count; i++)
  {
    var_bbs[i] = codegen_block(c, "rvr.var");
    LLVMAddCase(sw, LLVMConstInt(c->i32, ids[i], false), var_bbs[i]);
  }

  LLVMPositionBuilderAtEnd(c->builder, orig_bb);
  LLVMBuildBr(c->builder, merge_bb);

  size_t phi_count = count + 1;
  LLVMValueRef* phi_vals =
    (LLVMValueRef*)ponyint_pool_alloc_size(phi_count * sizeof(LLVMValueRef));
  LLVMBasicBlockRef* phi_bbs =
    (LLVMBasicBlockRef*)ponyint_pool_alloc_size(
      phi_count * sizeof(LLVMBasicBlockRef));

  phi_vals[0] = original_value;
  phi_bbs[0] = orig_bb;

  for(size_t i = 0; i < count; i++)
  {
    LLVMPositionBuilderAtEnd(c->builder, var_bbs[i]);
    LLVMBuildBr(c->builder, merge_bb);
    phi_vals[i + 1] = defaults[i];
    phi_bbs[i + 1] = var_bbs[i];
  }

  LLVMPositionBuilderAtEnd(c->builder, merge_bb);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, val_type, "rvr.result");
  LLVMAddIncoming(phi, phi_vals, phi_bbs, (unsigned)phi_count);

  ponyint_pool_free_size(count * sizeof(int32_t), ids);
  ponyint_pool_free_size(count * sizeof(LLVMBasicBlockRef), var_bbs);
  ponyint_pool_free_size(phi_count * sizeof(LLVMValueRef), phi_vals);
  ponyint_pool_free_size(phi_count * sizeof(LLVMBasicBlockRef), phi_bbs);

  return phi;
  #undef MAX_RVR_VARIANTS
}

bool mutate_vcr_begin(compile_t* c, ast_t* ast,
  LLVMBasicBlockRef* skip_bb, LLVMBasicBlockRef* merge_bb)
{
  if(!mutate_site_enabled(c, "vcr", ast))
    return false;

  source_t* source = ast_source(ast);
  const char* file =
    ((source != NULL) && (source->file != NULL)) ? source->file : "";
  size_t line = ast_line(ast);
  size_t pos = ast_pos(ast);

  size_t prev_count = c->mutate.mutant_count;
  int32_t cov_id = c->mutate.next_coverage_id;

  int32_t out_cov;
  int32_t id = mutate_find_or_record(&c->mutate, file, line, pos,
    "vcr", "call", "remove", cov_id, &out_cov);
  cov_id = out_cov;

  if(c->mutate.mutant_count > prev_count)
    c->mutate.next_coverage_id++;

  mutate_emit_coverage_inc(c, cov_id);

  LLVMValueRef selector = codegen_resolve_global(c, c->mutant_selector);
  LLVMValueRef sel = LLVMBuildLoad2(c->builder, c->i32,
    selector, "mut.sel");
  LLVMBasicBlockRef call_bb = codegen_block(c, "vcr.call");
  *skip_bb = codegen_block(c, "vcr.skip");
  *merge_bb = codegen_block(c, "vcr.merge");

  LLVMValueRef sw = LLVMBuildSwitch(c->builder, sel, call_bb, 1);
  LLVMAddCase(sw, LLVMConstInt(c->i32, id, false), *skip_bb);

  LLVMPositionBuilderAtEnd(c->builder, call_bb);
  return true;
}

LLVMValueRef mutate_vcr_end(compile_t* c, LLVMValueRef call_result,
  LLVMBasicBlockRef skip_bb, LLVMBasicBlockRef merge_bb)
{
  LLVMBasicBlockRef call_bb = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, merge_bb);

  LLVMPositionBuilderAtEnd(c->builder, skip_bb);
  LLVMValueRef none_val = codegen_resolve_global(c, c->none_instance);
  LLVMBuildBr(c->builder, merge_bb);

  LLVMPositionBuilderAtEnd(c->builder, merge_bb);

  LLVMTypeRef result_type = LLVMTypeOf(call_result);
  LLVMValueRef skip_val = LLVMBuildBitCast(c->builder, none_val,
    result_type, "");
  LLVMValueRef phi = LLVMBuildPhi(c->builder, result_type, "vcr.result");
  LLVMValueRef vals[2] = { call_result, skip_val };
  LLVMBasicBlockRef bbs[2] = { call_bb, skip_bb };
  LLVMAddIncoming(phi, vals, bbs, 2);

  return phi;
}

// Collect constant-replacement variants for one operand. The caller must
// recompute the operation with each replacement value.
static size_t cr_variants_for_operand(LLVMValueRef operand,
  LLVMValueRef* out_vals, const char** out_labels, size_t max_variants)
{
  size_t n = 0;
  LLVMTypeRef type = LLVMTypeOf(operand);

  if(LLVMIsAConstantInt(operand))
  {
    long long val = LLVMConstIntGetSExtValue(operand);
    unsigned width = LLVMGetIntTypeWidth(type);

    if(width == 1)
    {
      // Boolean: true↔false
      if(n < max_variants)
      {
        out_vals[n] = LLVMConstInt(type, val ? 0 : 1, false);
        out_labels[n] = val ? "true->false" : "false->true";
        n++;
      }
    }
    else
    {
      // Integer: 0↔1, n→n+1, n→n-1
      if(val == 0)
      {
        if(n < max_variants)
        {
          out_vals[n] = LLVMConstInt(type, 1, false);
          out_labels[n] = "0->1";
          n++;
        }
      }
      else if(val == 1)
      {
        if(n < max_variants)
        {
          out_vals[n] = LLVMConstInt(type, 0, false);
          out_labels[n] = "1->0";
          n++;
        }
      }
      else
      {
        if(n < max_variants)
        {
          out_vals[n] = LLVMConstInt(type, (unsigned long long)(val + 1), true);
          out_labels[n] = "n->n+1";
          n++;
        }
        if(n < max_variants)
        {
          out_vals[n] = LLVMConstInt(type, (unsigned long long)(val - 1), true);
          out_labels[n] = "n->n-1";
          n++;
        }
      }
    }
  }
  else if(LLVMIsAConstantFP(operand))
  {
    double val = 0.0;
    LLVMBool loses_info;

    if(LLVMGetTypeKind(type) == LLVMFloatTypeKind)
      val = (double)LLVMConstRealGetDouble(operand, &loses_info);
    else if(LLVMGetTypeKind(type) == LLVMDoubleTypeKind)
      val = LLVMConstRealGetDouble(operand, &loses_info);
    else if(LLVMGetTypeKind(type) == LLVMHalfTypeKind)
      val = LLVMConstRealGetDouble(operand, &loses_info);

    if(val == 0.0)
    {
      if(n < max_variants)
      {
        out_vals[n] = LLVMConstReal(type, 1.0);
        out_labels[n] = "0.0->1.0";
        n++;
      }
    }
    else if(val == 1.0)
    {
      if(n < max_variants)
      {
        out_vals[n] = LLVMConstReal(type, 0.0);
        out_labels[n] = "1.0->0.0";
        n++;
      }
    }
    else
    {
      if(n < max_variants)
      {
        out_vals[n] = LLVMConstReal(type, -val);
        out_labels[n] = "x->-x";
        n++;
      }
    }
  }

  return n;
}

LLVMValueRef mutate_cr_binop(compile_t* c, ast_t* ast,
  LLVMValueRef l_value, LLVMValueRef r_value,
  LLVMValueRef original_result,
  LLVMValueRef (*build_f)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*),
  LLVMValueRef (*build_i)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*))
{
  if(!mutate_site_enabled(c, "cr", ast))
    return original_result;

  // Collect variants for left and right operands.
  #define MAX_CR_VARIANTS 8
  LLVMValueRef l_alts[MAX_CR_VARIANTS];
  const char* l_labels[MAX_CR_VARIANTS];
  size_t l_count = cr_variants_for_operand(l_value, l_alts, l_labels,
    MAX_CR_VARIANTS);

  LLVMValueRef r_alts[MAX_CR_VARIANTS];
  const char* r_labels[MAX_CR_VARIANTS];
  size_t r_count = cr_variants_for_operand(r_value, r_alts, r_labels,
    MAX_CR_VARIANTS);

  size_t total = l_count + r_count;
  if(total == 0)
    return original_result;

  bool fp = is_fp(l_value);
  LLVMTypeRef result_type = LLVMTypeOf(original_result);

  source_t* source = ast_source(ast);
  const char* file =
    ((source != NULL) && (source->file != NULL)) ? source->file : "";
  size_t line = ast_line(ast);
  size_t pos = ast_pos(ast);

  size_t prev_count = c->mutate.mutant_count;
  int32_t cov_id = c->mutate.next_coverage_id;

  int32_t* ids = (int32_t*)ponyint_pool_alloc_size(total * sizeof(int32_t));

  for(size_t i = 0; i < l_count; i++)
  {
    int32_t out_cov;
    ids[i] = mutate_find_or_record(&c->mutate, file, line, pos,
      "cr", "const", l_labels[i], cov_id, &out_cov);
    cov_id = out_cov;
  }

  for(size_t i = 0; i < r_count; i++)
  {
    int32_t out_cov;
    ids[l_count + i] = mutate_find_or_record(&c->mutate, file, line, pos,
      "cr", "const", r_labels[i], cov_id, &out_cov);
    cov_id = out_cov;
  }

  if(c->mutate.mutant_count > prev_count)
    c->mutate.next_coverage_id++;

  mutate_emit_coverage_inc(c, cov_id);

  LLVMValueRef selector = codegen_resolve_global(c, c->mutant_selector);
  LLVMValueRef sel = LLVMBuildLoad2(c->builder, c->i32,
    selector, "mut.sel");
  LLVMBasicBlockRef orig_bb = codegen_block(c, "cr.orig");
  LLVMBasicBlockRef merge_bb = codegen_block(c, "cr.merge");

  LLVMValueRef sw = LLVMBuildSwitch(c->builder, sel, orig_bb,
    (unsigned)total);

  LLVMBasicBlockRef* var_bbs =
    (LLVMBasicBlockRef*)ponyint_pool_alloc_size(
      total * sizeof(LLVMBasicBlockRef));

  for(size_t i = 0; i < total; i++)
  {
    var_bbs[i] = codegen_block(c, "cr.var");
    LLVMAddCase(sw, LLVMConstInt(c->i32, ids[i], false), var_bbs[i]);
  }

  LLVMPositionBuilderAtEnd(c->builder, orig_bb);
  LLVMBuildBr(c->builder, merge_bb);

  size_t phi_count = total + 1;
  LLVMValueRef* phi_vals =
    (LLVMValueRef*)ponyint_pool_alloc_size(phi_count * sizeof(LLVMValueRef));
  LLVMBasicBlockRef* phi_bbs =
    (LLVMBasicBlockRef*)ponyint_pool_alloc_size(
      phi_count * sizeof(LLVMBasicBlockRef));

  phi_vals[0] = original_result;
  phi_bbs[0] = orig_bb;

  for(size_t i = 0; i < l_count; i++)
  {
    LLVMPositionBuilderAtEnd(c->builder, var_bbs[i]);
    LLVMValueRef var_result = fp
      ? build_f(c->builder, l_alts[i], r_value, "")
      : build_i(c->builder, l_alts[i], r_value, "");
    LLVMBuildBr(c->builder, merge_bb);
    phi_vals[i + 1] = var_result;
    phi_bbs[i + 1] = var_bbs[i];
  }

  for(size_t i = 0; i < r_count; i++)
  {
    LLVMPositionBuilderAtEnd(c->builder, var_bbs[l_count + i]);
    LLVMValueRef var_result = fp
      ? build_f(c->builder, l_value, r_alts[i], "")
      : build_i(c->builder, l_value, r_alts[i], "");
    LLVMBuildBr(c->builder, merge_bb);
    phi_vals[l_count + i + 1] = var_result;
    phi_bbs[l_count + i + 1] = var_bbs[l_count + i];
  }

  LLVMPositionBuilderAtEnd(c->builder, merge_bb);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, result_type, "cr.result");
  LLVMAddIncoming(phi, phi_vals, phi_bbs, (unsigned)phi_count);

  ponyint_pool_free_size(total * sizeof(int32_t), ids);
  ponyint_pool_free_size(total * sizeof(LLVMBasicBlockRef), var_bbs);
  ponyint_pool_free_size(phi_count * sizeof(LLVMValueRef), phi_vals);
  ponyint_pool_free_size(phi_count * sizeof(LLVMBasicBlockRef), phi_bbs);

  return phi;
  #undef MAX_CR_VARIANTS
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

LLVMValueRef mutate_declare_coverage(compile_t* c)
{
  LLVMValueRef g = LLVMAddGlobal(c->module, c->ptr,
    "pony_coverage_counters");
  LLVMSetLinkage(g, LLVMExternalLinkage);
  return g;
}

void mutate_emit_coverage_inc(compile_t* c, int32_t cov_id)
{
  LLVMValueRef base = LLVMBuildLoad2(c->builder, c->ptr,
    codegen_resolve_global(c, c->coverage_counters), "cov.base");
  LLVMValueRef idx = LLVMConstInt(c->i64, (unsigned long long)cov_id, false);
  LLVMValueRef elem = LLVMBuildInBoundsGEP2(c->builder, c->i64,
    base, &idx, 1, "cov.elem");
  LLVMBuildAtomicRMW(c->builder, LLVMAtomicRMWBinOpAdd,
    elem, LLVMConstInt(c->i64, 1, false),
    LLVMAtomicOrderingMonotonic, false);
}
