use "pony_test"
use "pony_check"

actor \nodoc\ Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() => None

  fun tag tests(test: PonyTest) =>
    // Property tests
    test(Property1UnitTest[(String, String)](_IRegexpIsMatchImpliesSearchProperty))
    test(Property1UnitTest[String](_IRegexpLiteralRoundtripProperty))
    test(Property1UnitTest[(String, String)](_IRegexpMatchSafetyProperty))
    test(Property1UnitTest[String](_IRegexpParserSafetyProperty))
    test(Property1UnitTest[(String, String)](_IRegexpSearchSubstringProperty))
    // Example tests
    test(_TestIRegexpEdgeCases)
    test(_TestIRegexpEscapes)
    test(_TestIRegexpIsMatch)
    test(_TestIRegexpParse)
    test(_TestIRegexpSearch)
    test(_TestIRegexpUnicode)

// ===================================================================
// Generator — I-Regexp
// ===================================================================

primitive \nodoc\ _IRegexpGen
  """Grammar-aware generator for valid I-Regexp patterns."""

  fun apply(max_depth: USize = 2): Generator[String] =>
    let that = this
    Generator[String](
      object is GenObj[String]
        fun generate(rnd: Randomness): String =>
          that._gen(rnd, max_depth)
      end)

  fun _gen(rnd: Randomness, depth: USize): String =>
    if depth == 0 then
      return _gen_atom(rnd)
    end
    match rnd.usize(0, 6)
    | 0 => _gen_atom(rnd)
    | 1 => _gen(rnd, depth - 1) + "|" + _gen(rnd, depth - 1)
    | 2 => _gen_atom(rnd) + _gen_atom(rnd)
    | 3 => "(" + _gen(rnd, depth - 1) + ")" + _gen_quant(rnd)
    | 4 => _gen_atom(rnd) + _gen_quant_nonempty(rnd)
    | 5 => _gen(rnd, depth - 1) + _gen_atom(rnd)
    | 6 => _gen_atom(rnd) + _gen(rnd, depth - 1)
    else _gen_atom(rnd)
    end

  fun _gen_atom(rnd: Randomness): String =>
    match rnd.usize(0, 6)
    | 0 => String.from_array([rnd.u8('a', 'z')])
    | 1 => "."
    | 2 => "\\n"
    | 3 => "\\t"
    | 4 => "[a-z]"
    | 5 => "[^0-9]"
    | 6 => "\\p{L}"
    else "a"
    end

  fun _gen_quant(rnd: Randomness): String =>
    match rnd.usize(0, 4)
    | 0 => ""
    | 1 => "*"
    | 2 => "+"
    | 3 => "?"
    | 4 =>
      let n = rnd.usize(0, 2)
      let m = n + rnd.usize(1, 3)
      "{" + n.string() + "," + m.string() + "}"
    else ""
    end

  fun _gen_quant_nonempty(rnd: Randomness): String =>
    match rnd.usize(0, 4)
    | 0 => "*"
    | 1 => "+"
    | 2 => "?"
    | 3 =>
      let n = rnd.usize(0, 2)
      let m = n + rnd.usize(1, 3)
      "{" + n.string() + "," + m.string() + "}"
    | 4 =>
      let n = rnd.usize(1, 3)
      "{" + n.string() + "}"
    else "*"
    end

// ===================================================================
// Property Tests — I-Regexp
// ===================================================================

class \nodoc\ iso _IRegexpParserSafetyProperty is Property1[String]
  fun name(): String => "iregex/parser-safety"

  fun gen(): Generator[String] =>
    Generators.ascii(0, 50)

  fun ref property(sample: String, ph: PropertyHelper) =>
    // parse() should always return a result, never crash
    match \exhaustive\ IRegexpCompiler.parse(sample)
    | let _: IRegexp => None
    | let _: IRegexpParseError => None
    end

class \nodoc\ iso _IRegexpMatchSafetyProperty is Property1[(String, String)]
  fun name(): String => "iregex/match-safety"

  fun gen(): Generator[(String, String)] =>
    Generators.zip2[String, String](
      _IRegexpGen(2),
      Generators.ascii(0, 30))

  fun ref property(sample: (String, String), ph: PropertyHelper) =>
    (let pattern, let input) = sample
    match \exhaustive\ IRegexpCompiler.parse(pattern)
    | let re: IRegexp =>
      re.is_match(input)
      re.search(input)
    | let e: IRegexpParseError =>
      ph.fail("Generated pattern should be valid: " + pattern
        + " — " + e.string())
    end

class \nodoc\ iso _IRegexpIsMatchImpliesSearchProperty
  is Property1[(String, String)]

  fun name(): String => "iregex/is_match-implies-search"

  fun gen(): Generator[(String, String)] =>
    Generators.zip2[String, String](
      _IRegexpGen(2),
      Generators.ascii(0, 30))

  fun ref property(sample: (String, String), ph: PropertyHelper) =>
    (let pattern, let input) = sample
    match \exhaustive\ IRegexpCompiler.parse(pattern)
    | let re: IRegexp =>
      if re.is_match(input) then
        ph.assert_true(re.search(input),
          "is_match implies search should also match")
      end
    | let _: IRegexpParseError => None
    end

class \nodoc\ iso _IRegexpLiteralRoundtripProperty is Property1[String]
  fun name(): String => "iregex/literal-roundtrip"

  fun gen(): Generator[String] =>
    Generators.ascii(1, 20)

  fun ref property(sample: String, ph: PropertyHelper) =>
    // Escape metacharacters to make a literal pattern
    let buf = String(sample.size() * 2)
    for byte in sample.values() do
      if (byte == '(') or (byte == ')') or (byte == '*') or (byte == '+')
        or (byte == '.') or (byte == '?') or (byte == '[') or (byte == '\\')
        or (byte == ']') or (byte == '^') or (byte == '{') or (byte == '|')
        or (byte == '}')
      then
        buf.push('\\')
      end
      buf.push(byte)
    end
    let pattern: String val = buf.clone()
    match \exhaustive\ IRegexpCompiler.parse(pattern)
    | let re: IRegexp =>
      ph.assert_true(re.is_match(sample),
        "Escaped literal pattern should match original string")
    | let e: IRegexpParseError =>
      ph.fail("Failed to compile escaped literal: " + e.string())
    end

class \nodoc\ iso _IRegexpSearchSubstringProperty
  is Property1[(String, String)]

  fun name(): String => "iregex/search-substring"

  fun gen(): Generator[(String, String)] =>
    Generators.zip2[String, String](
      Generators.ascii_letters(1, 5),
      Generators.ascii(0, 30))

  fun ref property(sample: (String, String), ph: PropertyHelper) =>
    (let pattern, let input) = sample
    match \exhaustive\ IRegexpCompiler.parse(pattern)
    | let re: IRegexp =>
      let regexp_found = re.search(input)
      let string_found = input.contains(pattern)
      ph.assert_eq[Bool](string_found, regexp_found,
        "Literal search should agree with String.contains for pattern: '"
          + pattern + "' in input")
    | let _: IRegexpParseError =>
      ph.fail("ASCII letter pattern should always be valid: " + pattern)
    end

// ===================================================================
// Example Tests — I-Regexp Parse
// ===================================================================

class \nodoc\ iso _TestIRegexpParse is UnitTest
  fun name(): String => "iregex/parse"

  fun apply(h: TestHelper) =>
    // Valid patterns
    let valid: Array[String] val = [
      "a"; "abc"; "."; "a*"; "a+"; "a?"; "a{2}"; "a{2,4}"; "a{2,}"
      "(a|b)"; "[abc]"; "[a-z]"; "[^a-z]"
      "\\n"; "\\r"; "\\t"
      "\\."; "\\|"; "\\("; "\\)"; "\\*"; "\\+"; "\\?"; "\\["; "\\]"
      "\\^"; "\\\\"; "\\{"; "\\}"; "\\-"
      "\\p{L}"; "\\p{Lu}"; "\\p{Nd}"; "\\P{L}"
      "[a-z0-9]"; "[-a]"; "[a-]"
      "" // empty pattern matches empty string
    ]
    for pattern in valid.values() do
      match \exhaustive\ IRegexpCompiler.parse(pattern)
      | let _: IRegexp => None
      | let e: IRegexpParseError =>
        h.fail("Expected valid: " + pattern + " — " + e.string())
      end
    end

    // Invalid patterns
    let invalid: Array[String] val = [
      "[" // unclosed class
      "(" // unclosed group
      "\\d" // multi-char escape not in I-Regexp
      "\\w" // multi-char escape not in I-Regexp
      "\\s" // multi-char escape not in I-Regexp
      "\\D"; "\\W"; "\\S"
      "[^]" // empty negated class forbidden
      "\\p{}" // empty category name
      "\\p{XYZ}" // too long
      "\\p{Q}" // unknown category
      "[z-a]" // reversed range
      "a{3,1}" // max < min
      "\\" // trailing backslash
      "[[a]]" // bare [ inside class (must be escaped)
      "a{99999}" // quantifier value too large
    ]
    for pattern in invalid.values() do
      match \exhaustive\ IRegexpCompiler.parse(pattern)
      | let _: IRegexpParseError => None
      | let _: IRegexp =>
        h.fail("Expected error for: " + pattern)
      end
    end

    // compile raises on invalid
    h.assert_error({() ? => IRegexpCompiler.compile("\\d")? })

    // compile succeeds on valid
    try
      IRegexpCompiler.compile("abc")?
    else
      h.fail("compile should succeed for 'abc'")
    end

// ===================================================================
// Example Tests — I-Regexp is_match
// ===================================================================

class \nodoc\ iso _TestIRegexpIsMatch is UnitTest
  fun name(): String => "iregex/is_match"

  fun apply(h: TestHelper) =>
    // Literal matching
    _assert_match(h, "abc", "abc", true)
    _assert_match(h, "abc", "ab", false)
    _assert_match(h, "abc", "abcd", false)

    // Dot
    _assert_match(h, "a.c", "abc", true)
    _assert_match(h, "a.c", "aXc", true)
    _assert_match(h, "a.c", "ac", false)
    _assert_match(h, "a.c", "a\nc", false) // dot doesn't match \n
    _assert_match(h, "a.c", "a\rc", false) // dot doesn't match \r

    // Alternation
    _assert_match(h, "a|b", "a", true)
    _assert_match(h, "a|b", "b", true)
    _assert_match(h, "a|b", "c", false)

    // Character classes
    _assert_match(h, "[abc]", "a", true)
    _assert_match(h, "[abc]", "d", false)
    _assert_match(h, "[a-z]", "m", true)
    _assert_match(h, "[a-z]", "A", false)
    _assert_match(h, "[^a-z]", "A", true)
    _assert_match(h, "[^a-z]", "a", false)

    // Quantifiers
    _assert_match(h, "a*", "", true)
    _assert_match(h, "a*", "aaa", true)
    _assert_match(h, "a+", "", false)
    _assert_match(h, "a+", "a", true)
    _assert_match(h, "a+", "aaa", true)
    _assert_match(h, "a?", "", true)
    _assert_match(h, "a?", "a", true)
    _assert_match(h, "a?", "aa", false)
    _assert_match(h, "a{3}", "aaa", true)
    _assert_match(h, "a{3}", "aa", false)
    _assert_match(h, "a{2,4}", "a", false)
    _assert_match(h, "a{2,4}", "aa", true)
    _assert_match(h, "a{2,4}", "aaaa", true)
    _assert_match(h, "a{2,4}", "aaaaa", false)
    _assert_match(h, "a{2,}", "a", false)
    _assert_match(h, "a{2,}", "aa", true)
    _assert_match(h, "a{2,}", "aaaaaa", true)

    // Grouping
    _assert_match(h, "(ab)+", "ab", true)
    _assert_match(h, "(ab)+", "abab", true)
    _assert_match(h, "(ab)+", "a", false)
    _assert_match(h, "(a|b)c", "ac", true)
    _assert_match(h, "(a|b)c", "bc", true)
    _assert_match(h, "(a|b)c", "cc", false)

    // Empty pattern matches empty string
    _assert_match(h, "", "", true)
    _assert_match(h, "", "a", false)

    // Empty branch in alternation
    _assert_match(h, "a|", "", true)
    _assert_match(h, "a|", "a", true)
    _assert_match(h, "|b", "", true)
    _assert_match(h, "|b", "b", true)

  fun _assert_match(
    h: TestHelper,
    pattern: String,
    input: String,
    expected: Bool)
  =>
    match \exhaustive\ IRegexpCompiler.parse(pattern)
    | let re: IRegexp =>
      h.assert_eq[Bool](expected, re.is_match(input),
        "Pattern '" + pattern + "' vs '" + input + "'")
    | let e: IRegexpParseError =>
      h.fail("Failed to compile '" + pattern + "': " + e.string())
    end

// ===================================================================
// Example Tests — I-Regexp search
// ===================================================================

class \nodoc\ iso _TestIRegexpSearch is UnitTest
  fun name(): String => "iregex/search"

  fun apply(h: TestHelper) =>
    // Substring matching
    _assert_search(h, "abc", "xabcx", true)
    _assert_search(h, "abc", "xabx", false)
    _assert_search(h, "a.c", "xabcx", true)
    _assert_search(h, "[0-9]+", "abc123def", true)
    _assert_search(h, "[0-9]+", "abcdef", false)

    // search matches at start/end
    _assert_search(h, "abc", "abcdef", true)
    _assert_search(h, "abc", "defabc", true)

    // Empty pattern always matches
    _assert_search(h, "", "anything", true)
    _assert_search(h, "", "", true)

    // Full-string match is also a search match
    _assert_search(h, "abc", "abc", true)

    // is_match implies search
    _assert_search(h, "a{2,4}", "aaa", true)

  fun _assert_search(
    h: TestHelper,
    pattern: String,
    input: String,
    expected: Bool)
  =>
    match \exhaustive\ IRegexpCompiler.parse(pattern)
    | let re: IRegexp =>
      h.assert_eq[Bool](expected, re.search(input),
        "Search '" + pattern + "' in '" + input + "'")
    | let e: IRegexpParseError =>
      h.fail("Failed to compile '" + pattern + "': " + e.string())
    end

// ===================================================================
// Example Tests — I-Regexp Unicode
// ===================================================================

class \nodoc\ iso _TestIRegexpUnicode is UnitTest
  fun name(): String => "iregex/unicode"

  fun apply(h: TestHelper) =>
    // \p{Lu} matches uppercase letters
    _assert_match(h, "\\p{Lu}", "A", true)
    _assert_match(h, "\\p{Lu}", "a", false)
    _assert_match(h, "\\p{Lu}", "1", false)

    // \p{L} matches any letter
    _assert_match(h, "\\p{L}", "A", true)
    _assert_match(h, "\\p{L}", "a", true)
    _assert_match(h, "\\p{L}", "1", false)

    // \p{Nd} matches decimal digits
    _assert_match(h, "\\p{Nd}", "5", true)
    _assert_match(h, "\\p{Nd}", "a", false)

    // \P{L} matches non-letters (complement)
    _assert_match(h, "\\P{L}", "1", true)
    _assert_match(h, "\\P{L}", "a", false)

    // Multi-byte codepoints: é (U+00E9) is lowercase letter
    let e_acute: String val = recover val String.from_utf32(0xE9) end
    _assert_match(h, "\\p{Ll}", e_acute, true)
    _assert_match(h, "\\p{Lu}", e_acute, false)

    // Emoji: U+1F600 is in So (Symbol, Other)
    let grinning: String val = recover val String.from_utf32(0x1F600) end
    _assert_match(h, "\\p{So}", grinning, true)
    _assert_match(h, "\\p{L}", grinning, false)

    // Dot matches emoji (not \n or \r)
    _assert_match(h, ".", grinning, true)

  fun _assert_match(
    h: TestHelper,
    pattern: String,
    input: String,
    expected: Bool)
  =>
    match \exhaustive\ IRegexpCompiler.parse(pattern)
    | let re: IRegexp =>
      h.assert_eq[Bool](expected, re.is_match(input),
        "Pattern '" + pattern + "' vs input")
    | let e: IRegexpParseError =>
      h.fail("Failed to compile '" + pattern + "': " + e.string())
    end

// ===================================================================
// Example Tests — I-Regexp Escapes
// ===================================================================

class \nodoc\ iso _TestIRegexpEscapes is UnitTest
  fun name(): String => "iregex/escapes"

  fun apply(h: TestHelper) =>
    // Control character escapes
    _assert_match(h, "\\n", "\n", true)
    _assert_match(h, "\\r", "\r", true)
    _assert_match(h, "\\t", "\t", true)
    _assert_match(h, "\\n", "n", false)

    // Escaped metacharacters
    _assert_match(h, "\\|", "|", true)
    _assert_match(h, "\\.", ".", true)
    _assert_match(h, "\\.", "a", false) // escaped dot is literal
    _assert_match(h, "\\(", "(", true)
    _assert_match(h, "\\)", ")", true)
    _assert_match(h, "\\*", "*", true)
    _assert_match(h, "\\+", "+", true)
    _assert_match(h, "\\?", "?", true)
    _assert_match(h, "\\[", "[", true)
    _assert_match(h, "\\\\", "\\", true)
    _assert_match(h, "\\]", "]", true)
    _assert_match(h, "\\^", "^", true)
    _assert_match(h, "\\{", "{", true)
    _assert_match(h, "\\}", "}", true)
    _assert_match(h, "\\-", "-", true)

    // \p{Zs} matches space characters
    _assert_match(h, "\\p{Zs}", " ", true)
    _assert_match(h, "\\p{Zs}", "a", false)

  fun _assert_match(
    h: TestHelper,
    pattern: String,
    input: String,
    expected: Bool)
  =>
    match \exhaustive\ IRegexpCompiler.parse(pattern)
    | let re: IRegexp =>
      h.assert_eq[Bool](expected, re.is_match(input),
        "Pattern '" + pattern + "' vs input")
    | let e: IRegexpParseError =>
      h.fail("Failed to compile '" + pattern + "': " + e.string())
    end

// ===================================================================
// Example Tests — I-Regexp Edge Cases
// ===================================================================

class \nodoc\ iso _TestIRegexpEdgeCases is UnitTest
  fun name(): String => "iregex/edge-cases"

  fun apply(h: TestHelper) =>
    // Nested groups
    _assert_match(h, "((a))", "a", true)
    _assert_match(h, "((a)(b))", "ab", true)

    // Complex alternation
    _assert_match(h, "a|b|c|d", "c", true)
    _assert_match(h, "a|b|c|d", "e", false)

    // Hyphen positioning in character classes
    _assert_match(h, "[-a]", "-", true)
    _assert_match(h, "[-a]", "a", true)
    _assert_match(h, "[a-]", "-", true)
    _assert_match(h, "[a-]", "a", true)
    _assert_match(h, "[a-]", "b", false)

    // Dot in character class (literal, not wildcard)
    _assert_match(h, "[.]", ".", true)
    _assert_match(h, "[.]", "a", false)

    // {0,0} matches empty string only
    _assert_match(h, "a{0,0}", "", true)
    _assert_match(h, "a{0,0}", "a", false)

    // {0} matches empty string only
    _assert_match(h, "a{0}", "", true)
    _assert_match(h, "a{0}", "a", false)

    // High repeat counts
    try
      let re = IRegexpCompiler.compile("a{100}")?
      let s100: String val = "a" * 100
      let s99: String val = "a" * 99
      h.assert_true(re.is_match(s100))
      h.assert_false(re.is_match(s99))
    else
      h.fail("a{100} should compile")
    end

    // Unicode property in character class
    _assert_match(h, "[\\p{Lu}0-9]", "A", true)
    _assert_match(h, "[\\p{Lu}0-9]", "5", true)
    _assert_match(h, "[\\p{Lu}0-9]", "a", false)

    // Negated class with ranges
    _assert_match(h, "[^a-zA-Z]", "1", true)
    _assert_match(h, "[^a-zA-Z]", "a", false)
    _assert_match(h, "[^a-zA-Z]", "Z", false)

  fun _assert_match(
    h: TestHelper,
    pattern: String,
    input: String,
    expected: Bool)
  =>
    match \exhaustive\ IRegexpCompiler.parse(pattern)
    | let re: IRegexp =>
      h.assert_eq[Bool](expected, re.is_match(input),
        "Pattern '" + pattern + "' vs '" + input + "'")
    | let e: IRegexpParseError =>
      h.fail("Failed to compile '" + pattern + "': " + e.string())
    end
