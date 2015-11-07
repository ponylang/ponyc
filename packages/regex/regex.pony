use "lib:pcre2-8"

primitive _Pattern

class Regex
  """
  A perl compatible regular expression. This uses the PCRE2 library, and
  attempts to enable JIT matching whenever possible.
  """
  var _pattern: Pointer[_Pattern]
  let _jit: Bool

  new create(from: ByteSeq box, jit: Bool = true) ? =>
    """
    Compile a regular expression. Raises an error for an invalid expression.
    """
    let opt: U32 = 0x00080000 // PCRE2_UTF
    var err: I32 = 0
    var erroffset: U64 = 0

    _pattern = @pcre2_compile_8[Pointer[_Pattern]](from.cstring(), from.size(),
      opt, addressof err, addressof erroffset, Pointer[U8])

    if _pattern.is_null() then
      error
    end

    _jit = jit and (@pcre2_jit_compile_8[I32](_pattern, U32(1)) == 0)

  fun eq(subject: ByteSeq box): Bool =>
    """
    Return true on a successful match, false otherwise.
    """
    if _pattern.is_null() then
      return false
    end

    let m = @pcre2_match_data_create_from_pattern_8[Pointer[_Match]](_pattern,
      Pointer[U8])

    let rc = if _jit then
      @pcre2_jit_match_8[I32](_pattern, subject.cstring(), subject.size(),
        U64(0), U32(0), m, Pointer[U8])
    else
      @pcre2_match_8[I32](_pattern, subject.cstring(), subject.size(), U64(0),
        U32(0), m, Pointer[U8])
    end

    @pcre2_match_data_free_8[None](m)
    rc > 0

  fun ne(subject: ByteSeq box): Bool =>
    """
    Return false on a successful match, true otherwise.
    """
    not eq(subject)

  fun apply(subject: ByteSeq, offset: U64 = 0): Match iso^ ? =>
    """
    Match the supplied string, starting at the given offset. Returns a Match
    object that can give precise match details. Raises an error if there is no
    match.

    TODO: global match
    """
    if _pattern.is_null() then
      error
    end

    Match._create(_pattern, _jit, subject, offset)

  fun replace[A: (Seq[U8] iso & ByteSeq iso) = String iso](subject: ByteSeq,
    value: ByteSeq box, offset: U64 = 0, global: Bool = false): A^ ?
  =>
    """
    Perform a match on the subject, starting at the given offset, and create
    a new string using the value as a replacement for what was matched. Raise
    an error if there is no match.
    """
    if _pattern.is_null() then
      error
    end

    var opt = U32(0)
    if global then opt = opt or 0x00000100 end // PCRE2_SUBSTITUTE_GLOBAL

    var len = subject.size().max(64)
    let out = recover A(len) end
    var rc = I32(0)

    repeat
      rc = @pcre2_substitute_8[I32](_pattern,
        subject.cstring(), subject.size(), offset, opt, Pointer[U8],
        Pointer[U8], value.cstring(), value.size(), out.cstring(),
        addressof len)

      if rc == -48 then
        len = len * 2
        out.reserve(len)
      end
    until rc != -48 end

    if rc <= 0 then
      error
    end

    out.truncate(len)
    out

  fun split(subject: String, offset: U64 = 0): Array[String] iso^ ?
  =>
    """
    Split subject by the occurrences of this pattern, returning a list of the
    substrings.
    """
    if _pattern.is_null() then
      error
    end

    let out = recover Array[String] end

    var off = offset
    try
      while off < subject.size() do
        let m = apply(subject, off)
        let off' = m.start_pos() - 1
        out.push(subject.substring(off.i64(), off'.i64()))
        off = m.end_pos() + 1
      end
    else
      out.push(subject.substring(off.i64(), -1))
    end

    out


  fun index(name: String box): U64 ? =>
    """
    Returns the index of a named capture. Raises an error if the named capture
    does not exist.
    """
    let rc = @pcre2_substring_number_from_name[I32](_pattern, name.cstring())

    if rc < 0 then
      error
    end

    rc.u64()

  fun ref dispose() =>
    """
    Free the underlying PCRE2 data.
    """
    if not _pattern.is_null() then
      @pcre2_code_free_8[None](_pattern)
      _pattern = Pointer[_Pattern]
    end

  fun _final() =>
    """
    Free the underlying PCRE2 data.
    """
    if not _pattern.is_null() then
      @pcre2_code_free_8[None](_pattern)
    end
