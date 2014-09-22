#ifndef SOURCE_H
#define SOURCE_H

#include <stddef.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct source_t
{
  const char* file;  // NULL => from string, not file
  char* m;
  size_t len;
} source_t;

/** Open the file with the given path.
 * Returns the opened source which must be closed later,
 * NULL on failure.
 */
source_t* source_open(const char* file);

/** Create a source based on the given string of code.
 * Intended for testing purposes only.
 * The given string is copied and the original does not need to remain valid
 * beyond this call.
 * Returns the new source which must be closed later.
 */
source_t* source_open_string(const char* source_code);

/** Close the given source.
 * May be called on sources that failed to open.
 */
void source_close(source_t* source);

PONY_EXTERN_C_END

#endif
