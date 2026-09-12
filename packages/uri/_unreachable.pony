use @fprintf[I32](stream: Pointer[None] tag, fmt: Pointer[U8] tag, ...)
use @pony_os_stderr[Pointer[None]]()
use @exit[None](code: I32)

primitive _Unreachable
  """
  Crash with a diagnostic when code that should be unreachable executes.
  """
  fun apply(loc: SourceLoc = __loc) =>
    @fprintf(
      @pony_os_stderr(),
      ("Unreachable code reached at %s:%zu\n" +
        "Please file a bug at " +
        "https://github.com/ponylang/ponyc/issues\n")
        .cstring(),
      loc.file().cstring(),
      loc.line())
    @exit(1)
