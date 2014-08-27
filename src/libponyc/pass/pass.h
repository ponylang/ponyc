#ifndef PASS_H
#define PASS_H

#include "../ast/ast.h"

typedef enum pass_id
{
  PASS_PARSE,
  PASS_SUGAR,
  PASS_SCOPE1,
  PASS_NAME_RESOLUTION,
  PASS_FLATTEN,
  PASS_TRAITS,
  PASS_SCOPE2,
  PASS_EXPR,
  PASS_ALL
} pass_id;

#ifdef PLATFORM_IS_VISUAL_STUDIO
inline pass_id& operator++(pass_id& current)
{
  current = static_cast<pass_id>(current + 1);
  return current;
};

inline pass_id operator++(pass_id& current, int)
{
  pass_id rvalue = current;
  ++current;
  return rvalue;
};
#endif

/** Limit processing to the specified pass. All passes up to and including the
 * specified pass will occur.
 * Returns true on success, false on invalid pass name.
 */
bool limit_passes(const char* pass);

/** Report the name of the specified pass.
 * The returned string is a literal and should not be freed.
 */
const char* pass_name(pass_id pass);

/** Apply the per package passes to the given AST.
 * Returns true on success, false on failure.
 */
bool package_passes(ast_t* package);

/** Apply the per program passes to the given AST.
* Returns true on success, false on failure.
*/
bool program_passes(ast_t* program, int opt, bool print_llvm);

#endif
