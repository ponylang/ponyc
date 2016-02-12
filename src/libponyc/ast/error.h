#ifndef ERROR_H
#define ERROR_H

#include "source.h"
#include <stddef.h>
#include <stdarg.h>

#include <platform.h>

PONY_EXTERN_C_BEGIN

/** Errors may be reported individually or grouped together into frames.
 *
 * An error frame is simply a list of errors that are reported as a unit and
 * are counted as a single error. The first error in a frame is reported as the
 * actual "error" and the remainder (if any) are considered "additional
 * information".
 *
 * Frames may be built up one error at a time and may also be combined with
 * each other.
 *
 * Once a frame is fully built it can be reported. Alternatively a frame may be
 * discarded, which frees any errors within it without reporting them.
 *
 * Simple example usage:
 *   errorframe_t frame = NULL;
 *   ast_error_frame(&frame, ast, "That went wrong");
 *   errorframef(&frame, NULL, "Usage information");
 *   errorframe_report(&frame);
 */

typedef struct errormsg_t
{
  const char* file;
  size_t line;
  size_t pos;
  const char* msg;
  const char* source;
  struct errormsg_t* frame;
  struct errormsg_t* next;
} errormsg_t;

typedef errormsg_t* errorframe_t;

errormsg_t* get_errors();

size_t get_error_count();

void free_errors();

void print_errors();

void errorv(source_t* source, size_t line, size_t pos, const char* fmt,
  va_list ap);

void error(source_t* source, size_t line, size_t pos,
  const char* fmt, ...) __attribute__((format(printf, 4, 5)));

void errorframev(errorframe_t* frame, source_t* source, size_t line,
  size_t pos, const char* fmt, va_list ap);

void errorframe(errorframe_t* frame, source_t* source, size_t line, size_t pos,
  const char* fmt, ...) __attribute__((format(printf, 5, 6)));

void errorfv(const char* file, const char* fmt, va_list ap);

void errorf(const char* file, const char* fmt,
  ...) __attribute__((format(printf, 2, 3)));

void errorframefv(errorframe_t* frame, const char* file, const char* fmt,
  va_list ap);

void errorframef(errorframe_t* frame, const char* file, const char* fmt,
  ...) __attribute__((format(printf, 3, 4)));

/// Append the errors in the second frame (if any) to the first frame.
/// The second frame is left empty.
void errorframe_append(errorframe_t* first, errorframe_t* second);

/// Check if there are any errors in the given frame.
bool errorframe_has_errors(errorframe_t* frame);

/// Report all errors (if any) in the given frame.
/// The frame is left empty.
void errorframe_report(errorframe_t* frame);

/// Discard any errors in the given frame.
/// The frame is left empty.
void errorframe_discard(errorframe_t* frame);

/// Configure whether errors should be printed immediately as well as deferred
void error_set_immediate(bool immediate);

PONY_EXTERN_C_END

#endif
