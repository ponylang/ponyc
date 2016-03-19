"""
# Regex package

The Regex package provides support for Perl compatible regular expressions.

## Example program

```pony
use "regex"

actor Main
  new create(env: Env) =>
    try
      let r = Regex("\\d+")

      if r == "1234" then
        env.out.print("1234 is a series of numbers")
      end

      if r != "Not a number" then
        env.out.print("'Not a number' is not a series of numbers")
      end

      let matched = r("There are 02 numbers in here.")
      env.out.print(matched(0) + " was matched")
      env.out.print("The match started at " + matched.start_pos().string())
      env.out.print("The match ended at " + matched.end_pos().string())
    end

    try
      let r = Regex("(\\d+)?\\.(\\d+)?")
      let matched = r("123.456")
      env.out.print(matched(0) + " was matched")
      env.out.print("The first match was " + matched(1))
      env.out.print("The second match was " + matched(2))
    end
```

"""

use "lib:pcre2-8"

primitive _Pattern

primitive _PCRE2
  fun utf(): U32 => 0x00080000                // PCRE2_UTF
  fun substitute_global(): U32 => 0x00000100  // PCRE2_SUBSTITUTE_GLOBAL
  fun not_empty(): U32 => 0x00000004          // PCRE2_NOTEMPTY

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
    let opt: U32 = _PCRE2.utf()
    var err: I32 = 0
    var erroffset: USize = 0

    _pattern = @pcre2_compile_8[Pointer[_Pattern]](from.cstring(), from.size(),
        _PCRE2.utf(), addressof err, addressof erroffset, Pointer[U8])

    if _pattern.is_null() then
      error
    end

    _jit = jit and (@pcre2_jit_compile_8[I32](_pattern, U32(1)) == 0)

  fun eq(subject: ByteSeq box): Bool =>
    """
    Return true on a successful match, false otherwise.
    """
    try
      let m = _match(subject, 0, 0)
      @pcre2_match_data_free_8[None](m)
      true
    else
      false
    end

  fun ne(subject: ByteSeq box): Bool =>
    """
    Return false on a successful match, true otherwise.
    """
    not eq(subject)

  fun apply(subject: ByteSeq, offset: USize = 0): Match^ ? =>
    """
    Match the supplied string, starting at the given offset. Returns a Match
    object that can give precise match details. Raises an error if there is no
    match.
    """
    let m = _match(subject, offset, U32(0))
    Match._create(subject, m)

  fun replace[A: (Seq[U8] iso & ByteSeq iso) = String iso](subject: ByteSeq,
    value: ByteSeq box, offset: USize = 0, global: Bool = false): A^ ?
  =>
    """
    Perform a match on the subject, starting at the given offset, and create
    a new string using the value as a replacement for what was matched. Raise
    an error if there is no match.
    """
    if _pattern.is_null() then
      error
    end

    var opt = if global then
      _PCRE2.substitute_global()
    else
      U32(0)
    end

    var len = subject.size().max(64)
    let out = recover A(len) end
    len = out.space()
    var rc = I32(0)

    repeat
      rc = @pcre2_substitute_8[I32](_pattern,
        subject.cstring(), subject.size(), offset, opt, Pointer[U8],
        Pointer[U8], value.cstring(), value.size(), out.cstring(),
        addressof len)

      if rc == -48 then
        len = len * 2
        out.reserve(len)
        len = out.space()
      end
    until rc != -48 end

    if rc <= 0 then
      error
    end

    out.truncate(len)
    out

  fun split(subject: String, offset: USize = 0): Array[String] iso^ ? =>
    """
    Split subject by non-empty occurrences of this pattern, returning a list
    of the substrings.
    """
    if _pattern.is_null() then
      error
    end

    let out = recover Array[String] end
    var off = offset

    try
      while off < subject.size() do
        let m' = _match(subject, off, _PCRE2.not_empty())
        let m = Match._create(subject, m')
        let off' = m.start_pos()
        out.push(subject.substring(off.isize(), off'.isize()))
        off = m.end_pos() + 1
      end
    else
      out.push(subject.substring(off.isize()))
    end

    out

  fun index(name: String box): USize ? =>
    """
    Returns the index of a named capture. Raises an error if the named capture
    does not exist.
    """
    let rc = @pcre2_substring_number_from_name[I32](_pattern, name.cstring())

    if rc < 0 then
      error
    end

    rc.usize()

  fun ref dispose() =>
    """
    Free the underlying PCRE2 data.
    """
    if not _pattern.is_null() then
      @pcre2_code_free_8[None](_pattern)
      _pattern = Pointer[_Pattern]
    end

  fun _match(subject: ByteSeq box, offset: USize, options: U32):
    Pointer[_Match]?
  =>
    """
    Match the subject and keep the capture results. Raises an error if there
    is no match.
    """
    if _pattern.is_null() then
      error
    end

    let m = @pcre2_match_data_create_from_pattern_8[Pointer[_Match]](_pattern,
      Pointer[U8])

    let rc = if _jit then
      @pcre2_jit_match_8[I32](_pattern, subject.cstring(), subject.size(),
        offset, options, m, Pointer[U8])
    else
      @pcre2_match_8[I32](_pattern, subject.cstring(), subject.size(), offset,
        options, m, Pointer[U8])
    end

    if rc <= 0 then
      @pcre2_match_data_free_8[None](m)
      error
    end
    m

  fun _final() =>
    """
    Free the underlying PCRE2 data.
    """
    if not _pattern.is_null() then
      @pcre2_code_free_8[None](_pattern)
    end
