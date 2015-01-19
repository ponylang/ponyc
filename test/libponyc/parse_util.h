#ifndef UNIT_PARSE_UTIL_H
#define UNIT_PARSE_UTIL_H

#include <platform.h>
#include "util.h"


/** Check that the given source compiles without error and optionally generates
 * the specified AST.
 * Errors are checked with ASSERTs, call in ASSERT_NO_FATAL_FAILURE.
 * @param expect Description of expected AST, or NULL not to check AST.
 */
void parse_test_good(const char* src, const char* expect);

/** Check that the given source fails compilation with an error.
 * Errors are checked with ASSERTs, call in ASSERT_NO_FATAL_FAILURE.
 */
void parse_test_bad(const char* src);


#endif
