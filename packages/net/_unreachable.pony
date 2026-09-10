use @exit[None](status: I32)
use @fprintf[I32](stream: Pointer[None] tag, fmt: Pointer[U8] tag, ...)
use @pony_os_stderr[Pointer[None]]()

primitive _Unreachable
  """
  To be used in places that the compiler can't prove is unreachable but we are
  certain is unreachable and if we reach it, we'd be silently hiding a bug.
  """
  fun apply(loc: SourceLoc = __loc) =>
    @fprintf(
      @pony_os_stderr(),
      ("The unreachable was reached in %s at line %s\n" +
        "Please open an issue at https://github.com/ponylang/ponyc/issues")
        .cstring(),
      loc.file().cstring(),
      loc.line().string().cstring())
    @exit(1)
