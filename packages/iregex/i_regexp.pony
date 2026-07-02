class val IRegexpParseError
  """
  Error from parsing or compiling an invalid I-Regexp pattern.
  Covers both parse-time errors (invalid syntax) and compile-time errors
  (e.g., pattern too complex for NFA state limit).
  """
  let message: String
  let offset: USize

  new val create(message': String, offset': USize) =>
    message = message'
    offset = offset'

  fun string(): String iso^ =>
    ("I-Regexp error at offset " + offset.string() + ": " + message).clone()

class val IRegexp
  """
  Compiled I-Regexp pattern for full-string matching and substring search.
  Created via IRegexpCompiler.parse() or .compile().
  """
  let _nfa: _NFA

  new val _create(nfa': _NFA) =>
    _nfa = nfa'

  fun is_match(input: String): Bool =>
    """Test if the entire input string matches the pattern."""
    _NFAExec.is_match(_nfa, input)

  fun search(input: String): Bool =>
    """Test if any substring of input matches the pattern."""
    _NFAExec.search(_nfa, input)

primitive IRegexpCompiler
  """
  Parse and compile an I-Regexp pattern string.
  """

  fun parse(pattern: String): (IRegexp | IRegexpParseError) =>
    """Parse pattern, returning compiled regexp or error."""
    let parser = _IRegexpParser(pattern)
    try
      let ast = parser.parse()?
      try
        let nfa = _NFABuilder.build(ast)?
        IRegexp._create(nfa)
      else
        IRegexpParseError(
          "Pattern too complex: NFA exceeds state limit", 0)
      end
    else
      parser.error_result()
    end

  fun compile(pattern: String): IRegexp ? =>
    """
    Parse pattern, raising on invalid input.

    Convenience wrapper around parse() for call sites that don't need
    the error details — discards the IRegexpParseError and raises instead.
    """
    match \exhaustive\ parse(pattern)
    | let r: IRegexp => r
    | let e: IRegexpParseError => error
    end
