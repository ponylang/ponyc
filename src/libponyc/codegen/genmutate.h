#ifndef CODEGEN_GENMUTATE_H
#define CODEGEN_GENMUTATE_H

#include <platform.h>
#include <llvm-c/Core.h>

PONY_EXTERN_C_BEGIN

typedef struct compile_t compile_t;
typedef struct mutate_state_t mutate_state_t;
typedef struct ast_t ast_t;

void mutate_init(mutate_state_t* state);

void mutate_cleanup(mutate_state_t* state);

int32_t mutate_next_id(mutate_state_t* state);

void mutate_record(mutate_state_t* state, int32_t id,
  const char* file, size_t line, size_t column,
  const char* category, const char* original,
  const char* replacement, int32_t coverage_counter);

// Look up an existing mutant by (file, line, column, category, original,
// replacement). Returns its ID and coverage counter via *out_coverage, or
// allocates a new one and returns coverage_counter via *out_coverage.
int32_t mutate_find_or_record(mutate_state_t* state,
  const char* file, size_t line, size_t column,
  const char* category, const char* original,
  const char* replacement, int32_t coverage_counter,
  int32_t* out_coverage);

bool mutate_write_info(mutate_state_t* state, const char* output_dir,
  const char* bin_name);

// One replacement variant for a binary operator mutation.
typedef struct mutate_variant_t
{
  LLVMValueRef (*build_f)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*);
  LLVMValueRef (*build_i)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*);
  const char* replacement;
} mutate_variant_t;

// One replacement variant for a comparison operator mutation.
typedef struct mutate_cmp_variant_t
{
  LLVMRealPredicate cmp_f;
  LLVMIntPredicate cmp_si;
  LLVMIntPredicate cmp_ui;
  const char* replacement;
} mutate_cmp_variant_t;

// One replacement variant for a div/rem mutation.
typedef struct mutate_divrem_variant_t
{
  LLVMValueRef (*build_f)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*);
  LLVMValueRef (*build_ui)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*);
  LLVMValueRef (*build_si)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*);
  bool is_div;
  const char* replacement;
} mutate_divrem_variant_t;

// One replacement variant for a unary operator mutation.
// The replacement always returns the operand unchanged (identity).
typedef struct mutate_unary_variant_t
{
  const char* replacement;
} mutate_unary_variant_t;

// Emit switch/phi for a binary-operator mutation. Evaluates operands once;
// selects the original when no mutant matches.
LLVMValueRef mutate_binop(compile_t* c, ast_t* ast,
  ast_t* left, ast_t* right,
  LLVMValueRef (*orig_f)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*),
  LLVMValueRef (*orig_i)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*),
  const char* original, const char* category,
  mutate_variant_t* variants, size_t count);

// Emit switch/phi for a div/rem mutation. Each branch runs make_divrem_inner
// with its own build functions and is_div flag.
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
  mutate_divrem_variant_t* variants, size_t count);

// Emit switch/phi for a comparison mutation. Evaluates operands once;
// result is always i1.
LLVMValueRef mutate_cmp(compile_t* c, ast_t* ast,
  ast_t* left, ast_t* right,
  LLVMRealPredicate orig_f, LLVMIntPredicate orig_si, LLVMIntPredicate orig_ui,
  const char* original, const char* category, bool safe,
  mutate_cmp_variant_t* variants, size_t count);

// Like mutate_cmp, but takes pre-evaluated operand values instead of AST
// nodes. The source-location AST is used only for the JSON record.
LLVMValueRef mutate_cmp_values(compile_t* c, ast_t* ast,
  LLVMValueRef l_value, LLVMValueRef r_value, bool sign,
  LLVMRealPredicate orig_f, LLVMIntPredicate orig_si, LLVMIntPredicate orig_ui,
  const char* original, const char* category, bool safe,
  mutate_cmp_variant_t* variants, size_t count);

// Emit switch/phi for a unary-operator mutation.
LLVMValueRef mutate_unary(compile_t* c, ast_t* ast,
  LLVMValueRef operand, LLVMValueRef original_result,
  const char* original, const char* category,
  mutate_unary_variant_t* variants, size_t count);

// Emit switch/phi that swaps shl and shr. Handles safe-mode clamping,
// signed vs unsigned right shifts, and is_exact / no-wrap flags.
LLVMValueRef mutate_shift(compile_t* c, ast_t* ast,
  LLVMValueRef l_value, LLVMValueRef r_value,
  bool is_shl, bool sign, bool safe, unsigned width);

// Emit return-value-replacement mutation. Returns original_value unchanged
// if the type has no known defaults.
LLVMValueRef mutate_rvr(compile_t* c, ast_t* ast,
  LLVMValueRef original_value);

// Begin a void-call-removal mutation. Returns true if a mutation was
// emitted; the caller emits the call, then calls mutate_vcr_end.
bool mutate_vcr_begin(compile_t* c, ast_t* ast,
  LLVMBasicBlockRef* skip_bb, LLVMBasicBlockRef* merge_bb);

// Finish a void-call-removal mutation. Returns a phi merging call_result
// with a None instance so the caller avoids a dominance violation.
LLVMValueRef mutate_vcr_end(compile_t* c, LLVMValueRef call_result,
  LLVMBasicBlockRef skip_bb, LLVMBasicBlockRef merge_bb);

// Emit constant-replacement mutations for binary operator operands. Returns
// original_result unchanged if no compile-time constants are found.
LLVMValueRef mutate_cr_binop(compile_t* c, ast_t* ast,
  LLVMValueRef l_value, LLVMValueRef r_value,
  LLVMValueRef original_result,
  LLVMValueRef (*build_f)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*),
  LLVMValueRef (*build_i)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*));

// Declare the @pony_mutant_selector i32 global in the current module.
LLVMValueRef mutate_declare_selector(compile_t* c);

// Declare the @pony_mutate_enabled i32 global in the current module.
LLVMValueRef mutate_declare_enabled(compile_t* c);

// Declare the @pony_coverage_counters ptr global in the current module.
LLVMValueRef mutate_declare_coverage(compile_t* c);

// Emit an atomic increment of the coverage counter at cov_id.
void mutate_emit_coverage_inc(compile_t* c, int32_t cov_id);

// Check whether mutations should be emitted for the given category and
// source location. Returns false when operator or scope filtering excludes
// this site.
bool mutate_site_enabled(compile_t* c, const char* category, ast_t* ast);

PONY_EXTERN_C_END

#endif
