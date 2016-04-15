#ifndef ERROR_H
#define ERROR_H

#include "source.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include <platform.h>

PONY_EXTERN_C_BEGIN

/** Errors may be reported individually using error and ast_error:
 *
 *   error(errors, source, line, pos, "That went wrong");
 *
 *   ast_error(errors, ast, "This also went wrong");
 *
 * Errors may get additional info using error_continue and ast_error_continue:
 *
 *   error(errors, source, line, pos, "that went wrong");
 *   error_continue(errors, source, line, pos, "it should match this here");
 *
 *   ast_error(errors, ast, "this also went wrong");
 *   ast_error_continue(errors, ast2, "and it should match this here");
 *
 * These error and info units may also be carried around as explicit frames,
 * then later reported explicitly.
 *
 *   errorframe_t frame = NULL;
 *   ast_error_frame(&frame, ast, "that went wrong");
 *   ast_error_frame(&frame, ast2, "it should match here");
 *   errorframe_report(&frame, errors);
 */

typedef struct errors_t errors_t;

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


/// Allocate an empty errors collection.
errors_t* errors_alloc();

/// Free an errors collection allocated with errors_alloc.
/// Also frees all individual contained error objects.
void errors_free(errors_t* errors);

/// Return the first of the stored errors.
errormsg_t* errors_get_first(errors_t* errors);

/// Get the current number of stored errors.
size_t errors_get_count(errors_t* errors);

/// Configure whether errors should be printed immediately as well as deferred.
void errors_set_immediate(errors_t* errors, bool immediate);

/// Configure the stream to which errors will be printed (stdout by default).
void errors_set_output_stream(errors_t* errors, FILE* fp);

/// Print the collected errors (unless they were set to print immediately).
void errors_print(errors_t* errors);

/// Emit a new error with a location and printf-style va_list.
void errorv(errors_t* errors, source_t* source, size_t line, size_t pos,
  const char* fmt, va_list ap);

/// Emit a new error with a location and printf-style var args.
void error(errors_t* errors, source_t* source, size_t line, size_t pos,
  const char* fmt, ...) __attribute__((format(printf, 5, 6)));

/// Add to the latest error with a new location and printf-style va_list.
void errorv_continue(errors_t* errors, source_t* source, size_t line,
  size_t pos, const char* fmt, va_list ap);

/// Add to the latest error with a new location and printf-style var args.
void error_continue(errors_t* errors, source_t* source, size_t line, size_t pos,
  const char* fmt, ...) __attribute__((format(printf, 5, 6)));

/// Create or add to the given frame a new location and printf-style va_list.
void errorframev(errorframe_t* frame, source_t* source, size_t line,
  size_t pos, const char* fmt, va_list ap);

/// Create or add to the given frame a new location and printf-style va_list.
void errorframe(errorframe_t* frame, source_t* source, size_t line, size_t pos,
  const char* fmt, ...) __attribute__((format(printf, 5, 6)));

/// Emit a new error with a filename and printf-style va_list.
void errorfv(errors_t* errors, const char* file, const char* fmt, va_list ap);

/// Emit a new error with a filename and printf-style var args.
void errorf(errors_t* errors, const char* file, const char* fmt,
  ...) __attribute__((format(printf, 3, 4)));

/// Append the errors in the second frame (if any) to the first frame.
/// The second frame is left empty.
void errorframe_append(errorframe_t* first, errorframe_t* second);

/// Check if there are any errors in the given frame.
bool errorframe_has_errors(errorframe_t* frame);

/// Report all errors (if any) in the given frame.
/// The frame is left empty.
void errorframe_report(errorframe_t* frame, errors_t* errors);

/// Discard any errors in the given frame.
/// The frame is left empty.
void errorframe_discard(errorframe_t* frame);

PONY_EXTERN_C_END

#endif
