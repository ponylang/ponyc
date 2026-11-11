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

// Reuse an ID and counter for the same owner, source site, and replacement.
// Each distinct mutant has its own counter. owner is the implementing type
// definition; shared marks generic sites, including single instantiations.
int32_t mutate_find_or_record(mutate_state_t* state, ast_t* owner, bool shared,
  const char* file, size_t line, size_t column,
  const char* category, const char* original,
  const char* replacement, int32_t* out_coverage);

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
  const char* category;
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
  int safe;
  const char* replacement;
  const char* category;
} mutate_divrem_variant_t;

// One replacement variant for a unary operator mutation.
typedef struct mutate_unary_variant_t
{
  LLVMValueRef (*build_f)(LLVMBuilderRef, LLVMValueRef, const char*);
  LLVMValueRef (*build_i)(LLVMBuilderRef, LLVMValueRef, const char*);
  const char* replacement;
  const char* category;
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

// Record one BOR mutant for a Bool short-circuit site and return the
// flipped branch condition: l_value XOR (selector == this_record's_id).
LLVMValueRef mutate_sc_flip(compile_t* c, ast_t* ast, bool is_and,
  LLVMValueRef l_value);

// Record the two BOR mutants (xor->and, xor->or) for a Bool xor site.
// Records both mutant IDs and increments their coverage counters.
void mutate_sc_xor_record(compile_t* c, ast_t* ast,
  int32_t* and_id_out, int32_t* or_id_out);

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

// Emit switch/phi for a comparison mutation. Takes pre-evaluated operand
// values; the source-location AST is used only for the JSON record.
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

// Emit switch/phi for shift mutations. SOR swaps shl/shr; AMR toggles
// wrapping/unsafe mode. Handles clamping, sign, and is_exact / no-wrap flags.
LLVMValueRef mutate_shift(compile_t* c, ast_t* ast,
  LLVMValueRef l_value, LLVMValueRef r_value,
  bool is_shl, bool sign, bool safe, unsigned width,
  bool do_sor, bool do_amr);

// Emit UOI-insertion mutations on an arithmetic result: negation for all
// numeric types, bitwise complement for integers. Skips when both operands
// are compile-time constants. r_value is NULL for unary operators.
LLVMValueRef mutate_uoi_insert(compile_t* c, ast_t* ast,
  LLVMValueRef result, LLVMValueRef l_value, LLVMValueRef r_value);

// Emit return-value-replacement mutation. Returns original_value unchanged
// if the type has no known defaults. return_type is the function's declared
// return type (determines which replacements to offer). body_type is the
// Pony type of the expression being returned (used to suppress
// constant-equivalent replacements for None-typed expressions).
LLVMValueRef mutate_rvr(compile_t* c, ast_t* ast,
  LLVMValueRef original_value, ast_t* return_type, ast_t* body_type);

// Begin a void-call-removal mutation. Returns true if a mutation was
// emitted; the caller emits the call, then calls mutate_vcr_end.
bool mutate_vcr_begin(compile_t* c, ast_t* ast,
  LLVMBasicBlockRef* skip_bb, LLVMBasicBlockRef* merge_bb);

// Finish a void-call-removal mutation. Returns a phi merging call_result
// with a None instance so the caller avoids a dominance violation.
LLVMValueRef mutate_vcr_end(compile_t* c, LLVMValueRef call_result,
  LLVMBasicBlockRef skip_bb, LLVMBasicBlockRef merge_bb);

// Emit constant-replacement mutations for binary operator operands.
// left_ast/right_ast are the operand AST nodes (may be TK_SEQ-wrapped).
// Returns original_result unchanged if no operand is a source literal.
LLVMValueRef mutate_cr_binop(compile_t* c, ast_t* ast,
  ast_t* left_ast, ast_t* right_ast,
  LLVMValueRef l_value, LLVMValueRef r_value,
  LLVMValueRef original_result,
  LLVMValueRef (*build_f)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*),
  LLVMValueRef (*build_i)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*));

// Emit constant-replacement mutations for div/rem operands.
// left_ast/right_ast are the operand AST nodes. Each variant is built
// through make_divrem_inner so division by a mutated-to-zero constant
// gets Pony's safe-division semantics.
LLVMValueRef mutate_cr_divrem(compile_t* c, ast_t* ast,
  ast_t* left_ast, ast_t* right_ast,
  LLVMValueRef l_value, LLVMValueRef r_value,
  LLVMValueRef original_result,
  bool sign, bool safe, bool is_div,
  LLVMValueRef (*build_f)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*),
  LLVMValueRef (*build_ui)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*),
  LLVMValueRef (*build_si)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*));

// Emit constant-replacement mutations for shift operands. Rebuilds the
// shift (including clamp for safe shifts) with each replaced literal.
// Variants that would shift >= width are silently dropped.
LLVMValueRef mutate_cr_shift(compile_t* c, ast_t* ast,
  ast_t* left_ast, ast_t* right_ast,
  LLVMValueRef l_value, LLVMValueRef r_value,
  LLVMValueRef original_result,
  bool is_shl, bool sign, bool safe, unsigned width);

// Emit constant-replacement mutations for comparison operands. Rebuilds
// the comparison with make_cmp_value for each replaced literal.
LLVMValueRef mutate_cr_cmp(compile_t* c, ast_t* ast,
  ast_t* left_ast, ast_t* right_ast,
  LLVMValueRef l_value, LLVMValueRef r_value,
  LLVMValueRef original_result,
  bool sign, LLVMRealPredicate cmp_f,
  LLVMIntPredicate cmp_si, LLVMIntPredicate cmp_ui, bool safe);

// Emit constant-replacement mutations for a unary operator's operand.
LLVMValueRef mutate_cr_unary(compile_t* c, ast_t* ast,
  ast_t* operand_ast, LLVMValueRef operand,
  LLVMValueRef original_result,
  LLVMValueRef (*build_f)(LLVMBuilderRef, LLVMValueRef, const char*),
  LLVMValueRef (*build_i)(LLVMBuilderRef, LLVMValueRef, const char*));

// Compute the shadow for a safe integer divrem. Returns v_min (for div)
// or zero (for rem) when the operands are min/-1; NULL otherwise.
LLVMValueRef mutate_safe_divrem_shadow(
  LLVMBuilderRef builder,
  LLVMValueRef l_shadow, LLVMValueRef r_shadow, bool sign, bool is_div);

// Return the constant a value would have without CR instrumentation.
// An LLVM constant is its own shadow; a CR phi's shadow is the original.
// Returns NULL when the value has no known shadow.
LLVMValueRef mutate_shadow_get(compile_t* c, LLVMValueRef val);

// Register a shadow: val -> shadow. Ignored when not mutating.
void mutate_shadow_set(compile_t* c, LLVMValueRef val, LLVMValueRef shadow);

// Clear the shadow map (called per function).
void mutate_shadow_clear(compile_t* c);

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
