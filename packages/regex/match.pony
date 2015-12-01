primitive _Match

class Match
  """
  Contains match data for a combination of a regex and a subject.
  """
  var _match: Pointer[_Match]
  let _subject: ByteSeq
  let _size: U32

  new _create(subject: ByteSeq, m: Pointer[_Match]) =>
    """
    Store a match, a subject, and a size.
    """
    _match = m
    _subject = subject
    _size = @pcre2_get_ovector_count_8[U32](m)

  fun size(): U32 =>
    """
    Returns the capture size of the match.
    """
    _size

  fun start_pos(): USize =>
    """
    Returns the character position of the first character in the match.
    """
    @pcre2_get_startchar_8[USize](_match)

  fun end_pos(): USize =>
    """
    Returns the character position of the last character in the match.
    """
    var len = USize(0)
    @pcre2_substring_length_bynumber_8[I32](_match, U32(0), addressof len)
    start_pos() + (len - 1)

  fun apply[A: (ByteSeq iso & Seq[U8] iso) = String iso](i: U32): A^ ? =>
    """
    Returns a capture by number. Raises an error if the index is out of bounds.
    """
    if i >= _size then
      error
    end

    var len = USize(0)
    var rc = @pcre2_substring_length_bynumber_8[I32](_match, i, addressof len)
    if rc != 0 then error end
    len = len + 1

    let out = recover A(len) end
    rc = @pcre2_substring_copy_bynumber_8[I32](_match, i, out.cstring(),
      addressof len)
    if rc != 0 then error end
    out.truncate(len)
    out

  fun find[A: (ByteSeq iso & Seq[U8] iso) = String iso](name: String box): A^ ?
  =>
    """
    Returns a capture by name. Raises an error if the named capture does not
    exist.
    """
    var len = USize(0)
    let rc = @pcre2_substring_length_byname_8[I32](_match, name.cstring(),
      addressof len)

    if rc != 0 then
      error
    end

    len = len + 1
    let out = recover A(len) end

    @pcre2_substring_copy_byname_8[I32](_match, name.cstring(), out.cstring(),
      addressof len)
    out.truncate(len)
    out

  fun groups(): Array[String] iso^ =>
    """
    Returns all of the captured subgroups.  Groups that failed to capture
    anything will contain the empty string.
    """
    let res = recover Array[String] end
    var i: U32 = 1
    while i < _size do
      try
        let g: String = apply(i)
        res.push(g)
      else
        res.push("")
      end
      i = i + 1
    end
    res

  fun ref dispose() =>
    """
    Free the underlying PCRE2 data.
    """
    if not _match.is_null() then
      @pcre2_match_data_free_8[None](_match)
      _match = Pointer[_Match]
    end

  fun _final() =>
    """
    Free the underlying PCRE2 data.
    """
    if not _match.is_null() then
      @pcre2_match_data_free_8[None](_match)
    end
