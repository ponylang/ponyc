#include "genmatch.h"
#include "gentype.h"
#include "genexpr.h"
#include "../type/subtype.h"

LLVMValueRef gen_match(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, match_expr, cases, else_expr);
  ast_t* match_type = ast_type(match_expr);
  LLVMValueRef match_value = gen_expr(c, match_expr);

  // TODO: remove this
  (void)match_value;

  LLVMBasicBlockRef case_block = codegen_block(c, "case");
  LLVMBasicBlockRef else_block = codegen_block(c, "match_else");
  LLVMBasicBlockRef next_block;

  ast_t* the_case = ast_child(cases);
  LLVMBuildBr(c->builder, case_block);
  LLVMPositionBuilderAtEnd(c->builder, case_block);

  while(the_case != NULL)
  {
    ast_t* next_case = ast_sibling(the_case);

    if(next_case != NULL)
      next_block = codegen_block(c, "case");
    else
      next_block = else_block;

    AST_GET_CHILDREN(the_case, pattern, guard, body);

    ast_t* pattern_type = ast_type(pattern);

    // If the match expression isn't a subtype of the pattern expression,
    // check at runtime if the type matches.
    if(!is_subtype(match_type, pattern_type))
    {
      gentype_t pattern_g;

      if(!gentype(c, pattern_type, &pattern_g))
        return NULL;

      // TODO: runtime type check

      /* Cases
       *
       * Pattern is a single type
       *  can't be a structural type
       *  concrete
       *    prevent in type checker if !is_subtype(pattern_type, match_type)
       *    otherwise, check if descriptor is the same
       *  trait
       *    prevent in type checker if concrete match_type and
       *      !is_subtype(match_type, pattern_type)
       *    otherwise, check trait list in descriptor
       *  union
       *    if all elements are concrete, prevent in type checker if
       *      no element is a subtype of the match type
       *    if the match type is concrete, prevent if no element of the
       *      match type is a subtype of the pattern type
       *    otherwise, check each element, succeeding on first match
       *  isect
       *    prevent if any concrete element is not a subtype of the match type
       *
       *
       * Assuming the type checker has done its business...
       *
       * class, actor, primitive
       *   check for a descriptor match
       * trait
       *   check trait list in descriptor
       * union
       *   check each element, succeed on first match
       *   check concrete elements first for speed
       * isect
       *   check each element, fail on first non-match
       *   check concrete elements first for speed
       * tuple
       *   the match value has to be a tuple of the right length
       *
       *
       * structural
       *   fail
       *   would have to check vtable and correct for colouring
       */
    }

    the_case = next_case;
  }

  return NULL;
}
