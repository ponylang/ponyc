use "pony_test"
use "pony_check"
use lint = ".."

class \nodoc\ _TestGlobMatchLiteral is UnitTest
  """Exact literal strings match; non-matching strings don't."""
  fun name(): String => "GlobMatch: literal matching"

  fun apply(h: TestHelper) =>
    h.assert_true(lint.GlobMatch.matches("foo", "foo"))
    h.assert_true(lint.GlobMatch.matches("foo.pony", "foo.pony"))
    h.assert_false(lint.GlobMatch.matches("foo", "bar"))
    h.assert_false(lint.GlobMatch.matches("foo", "foobar"))
    h.assert_false(lint.GlobMatch.matches("foobar", "foo"))
    // Substring doesn't match
    h.assert_false(lint.GlobMatch.matches("oo", "foo"))

class \nodoc\ _TestGlobMatchStar is UnitTest
  """* matches any characters except /."""
  fun name(): String => "GlobMatch: single * wildcard"

  fun apply(h: TestHelper) =>
    // Trailing *
    h.assert_true(lint.GlobMatch.matches("*.o", "foo.o"))
    h.assert_true(lint.GlobMatch.matches("*.o", ".o"))
    h.assert_false(lint.GlobMatch.matches("*.o", "foo.c"))
    // * doesn't cross /
    h.assert_false(lint.GlobMatch.matches("*.o", "src/foo.o"))
    // Leading *
    h.assert_true(lint.GlobMatch.matches("foo*", "foobar"))
    h.assert_true(lint.GlobMatch.matches("foo*", "foo"))
    // Middle *
    h.assert_true(lint.GlobMatch.matches("f*o", "fo"))
    h.assert_true(lint.GlobMatch.matches("f*o", "foo"))
    h.assert_true(lint.GlobMatch.matches("f*o", "fxo"))
    h.assert_true(lint.GlobMatch.matches("f*o", "fxyzo"))
    h.assert_false(lint.GlobMatch.matches("f*o", "f/o"))
    // Just *
    h.assert_true(lint.GlobMatch.matches("*", "anything"))
    h.assert_true(lint.GlobMatch.matches("*", ""))
    h.assert_false(lint.GlobMatch.matches("*", "a/b"))
    // Multiple * in same segment
    h.assert_true(lint.GlobMatch.matches("*.*", "foo.o"))
    h.assert_true(lint.GlobMatch.matches("*.*", ".o"))
    h.assert_false(lint.GlobMatch.matches("*.*", "foo"))
    // * in path
    h.assert_true(lint.GlobMatch.matches("*/*.o", "src/foo.o"))
    h.assert_false(lint.GlobMatch.matches("*/*.o", "src/sub/foo.o"))

class \nodoc\ _TestGlobMatchDoubleStarLeading is UnitTest
  """Leading **/ matches at any directory depth."""
  fun name(): String => "GlobMatch: leading ** wildcard"

  fun apply(h: TestHelper) =>
    h.assert_true(lint.GlobMatch.matches("**/foo", "foo"))
    h.assert_true(lint.GlobMatch.matches("**/foo", "a/foo"))
    h.assert_true(lint.GlobMatch.matches("**/foo", "a/b/foo"))
    h.assert_true(lint.GlobMatch.matches("**/foo", "a/b/c/foo"))
    h.assert_false(lint.GlobMatch.matches("**/foo", "bar"))
    h.assert_false(lint.GlobMatch.matches("**/foo", "a/bar"))
    // With * in the rest
    h.assert_true(lint.GlobMatch.matches("**/*.o", "foo.o"))
    h.assert_true(lint.GlobMatch.matches("**/*.o", "src/foo.o"))
    h.assert_true(lint.GlobMatch.matches("**/*.o", "a/b/foo.o"))
    h.assert_false(lint.GlobMatch.matches("**/*.o", "foo.c"))

class \nodoc\ _TestGlobMatchDoubleStarTrailing is UnitTest
  """Trailing /** matches everything inside."""
  fun name(): String => "GlobMatch: trailing ** wildcard"

  fun apply(h: TestHelper) =>
    h.assert_true(lint.GlobMatch.matches("foo/**", "foo/a"))
    h.assert_true(lint.GlobMatch.matches("foo/**", "foo/a/b"))
    h.assert_true(lint.GlobMatch.matches("foo/**", "foo/a/b/c"))
    h.assert_false(lint.GlobMatch.matches("foo/**", "foo"))
    h.assert_false(lint.GlobMatch.matches("foo/**", "bar/a"))

class \nodoc\ _TestGlobMatchDoubleStarMiddle is UnitTest
  """Middle /**/ matches zero or more path components."""
  fun name(): String => "GlobMatch: middle ** wildcard"

  fun apply(h: TestHelper) =>
    // Zero intermediate components
    h.assert_true(lint.GlobMatch.matches("a/**/z", "a/z"))
    // One intermediate
    h.assert_true(lint.GlobMatch.matches("a/**/z", "a/b/z"))
    // Two intermediate
    h.assert_true(lint.GlobMatch.matches("a/**/z", "a/b/c/z"))
    // No match
    h.assert_false(lint.GlobMatch.matches("a/**/z", "a/b/c"))
    h.assert_false(lint.GlobMatch.matches("a/**/z", "x/z"))
    // ** with * in right part
    h.assert_true(lint.GlobMatch.matches("foo/**/*.txt", "foo/a.txt"))
    h.assert_true(lint.GlobMatch.matches("foo/**/*.txt", "foo/a/b.txt"))
    h.assert_true(lint.GlobMatch.matches("foo/**/*.txt", "foo/a/b/c.txt"))

class \nodoc\ _TestGlobMatchDoubleStarNotComponent is UnitTest
  """** not as a full path component is treated as two *s (same as one *)."""
  fun name(): String => "GlobMatch: ** not a full component"

  fun apply(h: TestHelper) =>
    // a** is like a*
    h.assert_true(lint.GlobMatch.matches("a**", "abc"))
    h.assert_false(lint.GlobMatch.matches("a**", "a/b"))
    // **b is like *b
    h.assert_true(lint.GlobMatch.matches("**b", "xb"))
    h.assert_false(lint.GlobMatch.matches("**b", "a/b"))

class \nodoc\ _TestGlobMatchDoubleStarAlone is UnitTest
  """** alone matches everything including paths with /."""
  fun name(): String => "GlobMatch: ** matches everything"

  fun apply(h: TestHelper) =>
    h.assert_true(lint.GlobMatch.matches("**", ""))
    h.assert_true(lint.GlobMatch.matches("**", "foo"))
    h.assert_true(lint.GlobMatch.matches("**", "foo/bar"))
    h.assert_true(lint.GlobMatch.matches("**", "a/b/c/d"))

class \nodoc\ _TestGlobMatchEdgeCases is UnitTest
  """Edge cases: empty pattern, empty text."""
  fun name(): String => "GlobMatch: edge cases"

  fun apply(h: TestHelper) =>
    // Empty pattern matches only empty text
    h.assert_true(lint.GlobMatch.matches("", ""))
    h.assert_false(lint.GlobMatch.matches("", "foo"))
    // Empty text matches only empty pattern or * or **
    h.assert_true(lint.GlobMatch.matches("*", ""))
    h.assert_true(lint.GlobMatch.matches("**", ""))
    h.assert_false(lint.GlobMatch.matches("foo", ""))
    // Multiple ** components
    h.assert_true(lint.GlobMatch.matches("**/a/**/b", "a/b"))
    h.assert_true(lint.GlobMatch.matches("**/a/**/b", "x/a/y/b"))
    h.assert_true(lint.GlobMatch.matches("**/a/**/b", "x/y/a/z/w/b"))

class \nodoc\ _TestGlobMatchLiteralSelfMatchProperty is Property1[String]
  """Any string without glob metacharacters matches itself."""

  fun name(): String =>
    "GlobMatch: literal self-match property"

  fun gen(): Generator[String] =>
    Generator[String](
      object is GenObj[String]
        fun generate(rnd: Randomness): GenerateResult[String] =>
          // Generate strings without * or /
          let len = rnd.u8(0, 20).usize()
          let s = recover iso String(len) end
          var i: USize = 0
          while i < len do
            // ASCII printable chars excluding * and /
            var ch = rnd.u8(33, 126)
            if ch == '*' then ch = 'a' end
            if ch == '/' then ch = 'b' end
            s.push(ch)
            i = i + 1
          end
          consume s
      end)

  fun ref property(arg1: String, ph: PropertyHelper) =>
    ph.assert_true(
      lint.GlobMatch.matches(arg1, arg1),
      "Expected self-match: " + arg1)

class \nodoc\ _TestGlobMatchStarNoCrossSlashProperty is Property1[String]
  """* matches a string iff it contains no /."""

  fun name(): String =>
    "GlobMatch: * never crosses / property"

  fun gen(): Generator[String] =>
    Generator[String](
      object is GenObj[String]
        fun generate(rnd: Randomness): GenerateResult[String] =>
          let len = rnd.u8(0, 15).usize()
          let s = recover iso String(len) end
          var i: USize = 0
          while i < len do
            // Mix of alphanumeric, /, and other chars
            let mode = rnd.u8(0, 3)
            if mode == 0 then
              s.push('/')
            else
              s.push('a' + rnd.u8(0, 25))
            end
            i = i + 1
          end
          consume s
      end)

  fun ref property(arg1: String, ph: PropertyHelper) =>
    let expected = not arg1.contains("/")
    ph.assert_eq[Bool](
      expected,
      lint.GlobMatch.matches("*", arg1),
      "* vs '" + arg1 + "': expected "
        + if expected then "match" else "no match" end)

class \nodoc\ _TestGlobMatchDoubleStarMatchesAllProperty is Property1[String]
  """** matches every string."""

  fun name(): String =>
    "GlobMatch: ** matches everything property"

  fun gen(): Generator[String] =>
    Generator[String](
      object is GenObj[String]
        fun generate(rnd: Randomness): GenerateResult[String] =>
          let len = rnd.u8(0, 30).usize()
          let s = recover iso String(len) end
          var i: USize = 0
          while i < len do
            let mode = rnd.u8(0, 3)
            if mode == 0 then
              s.push('/')
            else
              s.push('a' + rnd.u8(0, 25))
            end
            i = i + 1
          end
          consume s
      end)

  fun ref property(arg1: String, ph: PropertyHelper) =>
    ph.assert_true(
      lint.GlobMatch.matches("**", arg1),
      "Expected ** to match: " + arg1)
