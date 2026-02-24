use @pony_os_stderr[Pointer[U8]]()
use @fprintf[I32](stream: Pointer[U8] tag, fmt: Pointer[U8] tag, ...)
use @exit[None](status: I32)

primitive _Unreachable
  """
  Panic for code paths that should be structurally impossible.

  Prints file and line to stderr, then exits. Use this instead of silently
  swallowing errors when a failure would indicate a bug in the program logic,
  not an expected runtime condition.
  """

  fun apply(loc: SourceLoc = __loc) =>
    @fprintf(@pony_os_stderr(),
      "Unreachable code reached at %s:%lu in %s.%s\nPlease file an issue at https://github.com/ponylang/json-ng/issues\n".cstring(),
      loc.file().cstring(), loc.line(), loc.type_name().cstring(),
      loc.method_name().cstring())
    @exit(1)
