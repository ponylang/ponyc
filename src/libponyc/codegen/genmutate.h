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
  const char* replacement);

bool mutate_write_info(mutate_state_t* state, const char* output_dir,
  const char* bin_name);

// A candidate replacement for a binary operator mutation site. Each variant
// carries its own float and integer build functions and a label for the info
// file (e.g. "sub" when the original is "add").
typedef struct mutate_variant_t
{
  LLVMValueRef (*build_f)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*);
  LLVMValueRef (*build_i)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*);
  const char* replacement;
} mutate_variant_t;

// A candidate replacement for a comparison operator mutation site. Each
// variant carries predicates for float, signed-integer, and unsigned-integer
// comparisons, plus a label for the info file (e.g. "ne" when the original
// is "eq").
typedef struct mutate_cmp_variant_t
{
  LLVMRealPredicate cmp_f;
  LLVMIntPredicate cmp_si;
  LLVMIntPredicate cmp_ui;
  const char* replacement;
} mutate_cmp_variant_t;

// A candidate replacement for a div/rem mutation site. Each variant carries
// its own float, unsigned-integer, and signed-integer build functions, the
// is_div flag for overflow handling, and a label for the info file.
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

// A candidate replacement for a unary operator mutation site. Each variant
// carries a label for the info file (e.g. "identity" when the original is
// "neg"). The replacement always returns the operand unchanged.
typedef struct mutate_unary_variant_t
{
  const char* replacement;
} mutate_unary_variant_t;

// Emit a switch/phi that selects between the original binary operator and one
// or more mutant variants at runtime, controlled by @pony_mutant_selector.
// Evaluates operands once, records each mutant in the info file, and returns
// the phi result. Selects the original when no mutant ID matches.
LLVMValueRef mutate_binop(compile_t* c, ast_t* ast,
  ast_t* left, ast_t* right,
  LLVMValueRef (*orig_f)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*),
  LLVMValueRef (*orig_i)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
    const char*),
  const char* original, const char* category,
  mutate_variant_t* variants, size_t count);

// Emit a switch/phi that selects between the original div or rem and a mutant
// variant at runtime. Evaluates operands once, resolves signedness, records
// each mutant, and returns the phi result. Each branch runs the full
// make_divrem_inner logic (constant folding, zero/overflow guards) with its
// own build functions and is_div flag.
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

// Emit a switch/phi that selects between the original comparison and one or
// more mutant variants at runtime. Evaluates operands once, resolves
// signedness, records each mutant, and returns the phi result (always i1).
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

// Emit a switch/phi that selects between the original unary operator result
// and returning the operand unchanged (identity). Records each mutant and
// returns the phi result.
LLVMValueRef mutate_unary(compile_t* c, ast_t* ast,
  LLVMValueRef operand, LLVMValueRef original_result,
  const char* original, const char* category,
  mutate_unary_variant_t* variants, size_t count);

// Declare the @pony_mutant_selector i32 global in the current module.
LLVMValueRef mutate_declare_selector(compile_t* c);

// Declare the @pony_mutate_enabled i32 global in the current module.
LLVMValueRef mutate_declare_enabled(compile_t* c);

// Check whether mutations should be emitted for the given category and
// source location. Returns false when operator or scope filtering excludes
// this site.
bool mutate_site_enabled(compile_t* c, const char* category, ast_t* ast);

PONY_EXTERN_C_END

#endif
