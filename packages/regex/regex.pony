use "lib:pcre2-8"

primitive _Pattern
primitive _Match

class Regex
  """
  A perl compatible regular expression. This uses the PCRE2 library, and
  attempts to enable JIT matching whenever possible.

  TODO: find-all, auto-jit on multiple uses
  use eq instead of apply
  """
  var _pattern: Pointer[_Pattern]
  var _match: Pointer[_Match]
  let _jit: Bool
  var _count: U64 = 0

  new create(from: Bytes, jit: Bool = true) ? =>
    """
    Compile a regular expression. Raises an error for an invalid expression.
    """
    let opt: U32 = 0x00080000 // PCRE2_UTF
    var err: I32 = 0
    var erroffset: U64 = 0

    _pattern = @pcre2_compile_8[Pointer[_Pattern]](from.cstring(), from.size(),
      opt, &err, &erroffset, Pointer[U8])

    Fact(not _pattern.is_null())

    _jit = jit and (@pcre2_jit_compile_8[I32](_pattern, U32(1)) == 0)
    _match = @pcre2_match_data_create_from_pattern_8[Pointer[_Match]](_pattern,
      Pointer[U8])

  fun ref apply(subject: Bytes, offset: U64 = 0): Bool =>
    """
    Match the supplied string, starting at the given offset.
    """
    let rc = if _jit then
      @pcre2_jit_match_8[I32](_pattern, subject.cstring(), subject.size(),
        offset, U32(0), _match, Pointer[U8])
    else
      @pcre2_match_8[I32](_pattern, subject.cstring(), subject.size(), offset,
        U32(0), _match, Pointer[U8])
    end

    _count = rc.max(0).u64()
    rc > 0

  fun ref update(subject: Bytes, offset: U64 = 0, global: Bool = false,
    value: Bytes): String iso^
  =>
    """
    Perform a match on the subject, starting at the given offset, and create
    a new string using the value as a replacement for what was matched.
    """
    var opt = U32(0)
    if global then opt = opt or 0x00000100 end // PCRE2_SUBSTITUTE_GLOBAL

    var len = subject.size().next_pow2()
    let out = recover String(len - 1) end
    var rc = I32(0)

    repeat
      rc = @pcre2_substitute_8[I32](_pattern,
        subject.cstring(), subject.size(), offset, opt, _match, Pointer[U8],
        value.cstring(), value.size(), out.cstring(), &len)

      if rc == -48 then
        len = len * 2
        out.reserve(len - 1)
      end
    until rc != -48 end

    _count = rc.max(0).u64()
    out.truncate(len)
    out

  fun count(): U64 =>
    """
    Returns the capture count of the most recent match. This will be zero if
    the match failed.
    """
    _count

  fun index(i: U64): String iso^ ? =>
    """
    Returns a capture by number. Raises an error if the index is out of bounds.
    """
    if i >= _count then
      error
    end

    var len = U64(0)
    @pcre2_substring_length_bynumber_8[I32](_match, i.u32(), &len)

    let out = recover String(len) end
    len = len + 1

    @pcre2_substring_copy_bynumber_8[I32](_match, i.u32(), out.cstring(), &len)
    out.truncate(len)
    out

  fun index_of(name: String box): U64 ? =>
    """
    Returns the index of a named capture. Raises an error if the named capture
    does not exist.
    """
    let rc = @pcre2_substring_number_from_name[I32](_pattern, name.cstring())

    if rc < 0 then
      error
    end
    rc.u64()

  fun find(name: String box): String iso^ ? =>
    """
    Returns a capture by name. Raises an error if the named capture does not
    exist.
    """
    var len = U64(0)
    let rc = @pcre2_substring_length_byname_8[I32](_match, name.cstring(),
      &len)

    if rc != 0 then
      error
    end

    let out = recover String(len) end
    len = len + 1

    @pcre2_substring_copy_byname_8[I32](_match, name.cstring(), out.cstring(),
      &len)
    out.truncate(len)
    out

  fun ref dispose() =>
    """
    Free the underlying PCRE2 data.
    """
    if not _pattern.is_null() then
      @pcre2_code_free_8[None](_pattern)
      _pattern = Pointer[_Pattern]

      @pcre2_match_data_free_8[None](_match)
      _match = Pointer[_Match]
    end
