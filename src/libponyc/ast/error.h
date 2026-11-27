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

/// Emit a new error with a filename, a line and position within it, and
/// printf-style var args. For errors in files ponyc has no source_t for
/// (e.g. C shim diagnostics); prints as "file:line:pos: msg" like errors
/// with a source, but with no source-line excerpt.
void errorf_at(errors_t* errors, const char* file, size_t line, size_t pos,
  const char* fmt, ...) __attribute__((format(printf, 5, 6)));

/// Add to the latest error with a filename, line and position, and
/// printf-style var args.
void errorf_at_continue(errors_t* errors, const char* file, size_t line,
  size_t pos, const char* fmt, ...) __attribute__((format(printf, 5, 6)));

/// Emit a new error with a filename and printf-style va_list.
void errorfv(errors_t* errors, const char* file, const char* fmt, va_list ap);

/// Emit a new error with a filename and printf-style var args.
void errorf(errors_t* errors, const char* file, const char* fmt,
  ...) __attribute__((format(printf, 3, 4)));

/// Add to the latest error with a new filename and printf-style var args.
void errorfv_continue(errors_t* errors, const char* file, const char* fmt,
  va_list ap);

/// Add to the latest error with a new filename and printf-style var args.
void errorf_continue(errors_t* errors, const char* file, const char* fmt,
  ...) __attribute__((format(printf, 3, 4)));

/// Append the errors in the second frame (if any) to the first frame.
/// The second frame is left empty.
void errorframe_append(errorframe_t* first, errorframe_t* second);

/// Report all errors (if any) in the given frame.
/// The frame is left empty.
void errorframe_report(errorframe_t* frame, errors_t* errors);

/// Discard any errors in the given frame.
/// The frame is left empty.
void errorframe_discard(errorframe_t* frame);

/// When `accepted`, discard any errors in the given frame, leaving it empty.
/// Returns `accepted`.
bool errorframe_discard_if(bool accepted, errorframe_t* frame);

/* The words that make a message one of the notes on errors in inherited
 * code: "in method 'f' inherited by C from T", and "'_H' is private to
 * package 'lib' and reaches this code through ...". The compiler builds the
 * notes from these words and identifies them by these words. The full-program
 * runner recognises them by the same words, in `_is_note` in
 * test/full-program-runner/_compile_errors.pony, and requires a test's
 * expected errors to list each note it recognises. If these words change and
 * those don't, the runner stops requiring the notes.
 */
#define COPY_NOTE_START "in method '"
#define COPY_NOTE_INHERITED "' inherited by "
#define BLAME_NOTE_REACHES "' and reaches this code through "

/** Add the note naming the inherited method and the types it was inherited
 * through to each error after the first `since` errors that has no such note
 * yet and that `skip` (if given) doesn't exclude. It goes before a note saying
 * how a private type reached the code.
 */
void errors_add_copy_note(errors_t* errors, size_t since,
  bool (*skip)(size_t index, void* ctx), void* ctx, source_t* source,
  size_t line, size_t pos, const char* fmt, ...);

PONY_EXTERN_C_END

#endif
