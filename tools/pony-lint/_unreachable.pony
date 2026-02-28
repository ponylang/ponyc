use @fprintf[I32](stream: Pointer[U8] tag, fmt: Pointer[U8] tag, ...)
use @pony_os_stderr[Pointer[U8]]()
use @exit[None](code: I32)

primitive _Unreachable
  """
  Crashes the program when a code path that should be unreachable is reached.

  Use in `else` blocks of `try` expressions where bounds are guarded by prior
  checks and the error path is logically impossible.
  """
  fun apply(loc: SourceLoc = __loc): None =>
    let url = "https://github.com/ponylang/ponyc/issues"
    let fmt = "Unreachable at %s:%zu\nPlease report: " + url + "\n"
    @fprintf(
      @pony_os_stderr(),
      fmt.cstring(),
      loc.file().cstring(),
      loc.line())
    @exit(1)
