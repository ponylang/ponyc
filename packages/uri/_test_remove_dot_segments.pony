use "pony_test"
use "pony_check"

class \nodoc\ iso _PropertyDotSegmentsIdempotent is Property1[String val]
  """
  Applying RemoveDotSegments twice produces the same result as once.
  """
  fun name(): String => "uri/remove_dot_segments/idempotent"

  fun gen(): Generator[String val] =>
    _AnyPathGenerator()

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let once = RemoveDotSegments(arg1)
    let twice = RemoveDotSegments(once)
    ph.assert_eq[String val](
      once, twice, "not idempotent for: " + arg1)

class \nodoc\ iso _PropertyDotSegmentsNoDots is Property1[String val]
  """
  The output of RemoveDotSegments never contains standalone "." or ".."
  segments.
  """
  fun name(): String => "uri/remove_dot_segments/no_dots_in_output"

  fun gen(): Generator[String val] =>
    _DotPathGenerator()

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let result = RemoveDotSegments(arg1)
    // Check that none of the segments are "." or ".."
    ph.assert_true(
      not _has_dot_segment(result),
      "dot segment in output for input: " + arg1 +
        " output: " + result)

  fun _has_dot_segment(path: String val): Bool =>
    """
    Check if path contains a standalone "." or ".." segment.
    A segment is delimited by "/" or string boundaries.
    """
    var start: USize = 0
    var i: USize = 0
    while i <= path.size() do
      // Use if/else instead of Bool.op_or — Pony's `or` is not
      // short-circuit, so `(i == path.size()) or (path(i)? == '/')`
      // would evaluate path(i)? even at end-of-string.
      let at_boundary =
        if i == path.size() then
          true
        else
          try path(i)? == '/' else false end
        end
      if at_boundary then
        let seg = path.substring(start.isize(), i.isize())
        if (consume seg == ".") or
          (path.substring(start.isize(), i.isize()) == "..")
        then
          return true
        end
        start = i + 1
      end
      i = i + 1
    end
    false

class \nodoc\ iso _PropertyDotSegmentsPreservesAbsolute
  is Property1[String val]
  """
  If the input starts with "/" the output also starts with "/".
  """
  fun name(): String => "uri/remove_dot_segments/preserves_absolute"

  fun gen(): Generator[String val] =>
    _DotPathGenerator.absolute()

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let result = RemoveDotSegments(arg1)
    ph.assert_true(
      try result(0)? == '/' else false end,
      "lost leading / for input: " + arg1 +
        " output: " + result)

class \nodoc\ iso _TestRemoveDotSegmentsKnownGood is UnitTest
  """
  RFC 3986 section 5.2.4 examples and edge cases.
  """
  fun name(): String => "uri/remove_dot_segments/known_good"

  fun ref apply(h: TestHelper) =>
    // RFC 3986 section 5.4 resolution examples (path portion)
    _assert(h, "/a/b/c/./../../g", "/a/g")
    _assert(h, "mid/content=5/../6", "mid/6")

    // Excess ".." clamps to root
    _assert(h, "/a/b/../../../g", "/g")

    // Trailing dot segments
    _assert(h, "/.", "/")
    _assert(h, "/..", "/")

    // Bare dot segments
    _assert(h, ".", "")
    _assert(h, "..", "")

    // Leading dot segments
    _assert(h, "./a", "a")
    _assert(h, "../a", "a")

    // Empty path
    _assert(h, "", "")

    // No-dot paths unchanged
    _assert(h, "/a/b/c", "/a/b/c")
    _assert(h, "/", "/")
    _assert(h, "a/b", "a/b")

    // Multiple consecutive dot segments
    _assert(h, "/a/b/c/../../d", "/a/d")
    _assert(h, "/a/b/./c/./d", "/a/b/c/d")
    _assert(h, "/a/b/../c/../d", "/a/d")

    // Trailing slashes preserved
    _assert(h, "/a/b/c/../", "/a/b/")
    _assert(h, "/a/b/c/./", "/a/b/c/")

    // Deep excess ".."
    _assert(h, "/../../../g", "/g")
    _assert(h, "/a/../../../../g", "/g")

    // Mixed leading and internal — step A strips "./" or "../", then
    // the remaining "a/../b" resolves to "/b" because removing "a"
    // from an output with no "/" clears it, leaving the "/" from "/../"
    _assert(h, "./a/../b", "/b")
    _assert(h, "../a/../b", "/b")

    // Dots that are NOT dot segments (contain additional characters)
    _assert(h, "/a.html", "/a.html")
    _assert(h, "/a..b", "/a..b")
    _assert(h, "/a/b.c/d", "/a/b.c/d")
    _assert(h, "/.hidden", "/.hidden")
    _assert(h, "/a/..suffix", "/a/..suffix")

  fun _assert(h: TestHelper, input: String, expected: String) =>
    h.assert_eq[String val](
      expected,
      RemoveDotSegments(input),
      "RemoveDotSegments(" + input + ")")

// -- Generators --
primitive \nodoc\ _DotPathGenerator
  """
  Generates paths likely to contain dot segments. Mixes fixed RFC examples
  with randomly composed paths built from segments including ".", "..", and
  normal path components.
  """
  fun apply(): Generator[String val] =>
    Generators.frequency[String val](
      [
        as WeightedGenerator[String val]:
        (1, Generators.one_of[String val](
          [
            "/a/b/c/./../../g"
            "mid/content=5/../6"
            "/a/b/../../../g"
            "/."
            "/.."
            "."
            ".."
            "./a"
            "../a"
            ""
            "/a/b/c"
            "/"
            "/a/../b/./c"
            "/../../../g"
            "/./g"
            "/../g"
            "/a/b/c/../d"
            "a/b/../c"
          ]))
        (2, _random_dot_path())
      ])

  fun absolute(): Generator[String val] =>
    """
    Generates absolute paths (starting with "/") that contain dot segments.
    """
    Generators.frequency[String val](
      [
        as WeightedGenerator[String val]:
        (1, Generators.one_of[String val](
          [
            "/a/b/c/./../../g"
            "/a/b/../../../g"
            "/."
            "/.."
            "/a/../b/./c"
            "/../../../g"
            "/./g"
            "/../g"
            "/a/b/c/../d"
            "/a/b/c"
            "/"
          ]))
        (2, Generators.map2[String val, String val, String val](
          Generators.one_of[String val](
            ["/a"; "/b"; "/c"; "/x/y"; "/foo/bar"]),
          Generators.one_of[String val](
            [ "/./"; "/../"; "/."; "/.."; "/./a"; "/../b"
              "/a/./b"; "/a/../b"; "/./../"]),
          {(prefix, suffix) => prefix + suffix }))
      ])

  fun _random_dot_path(): Generator[String val] =>
    Generators.map2[String val, String val, String val](
      Generators.one_of[String val](
        ["/a"; "/b"; "/c"; ""; "/x/y"; "/foo"; "a"; "a/b"]),
      Generators.one_of[String val](
        [ "/./"; "/../"; "/."; "/.."; "/./a"; "/../b"
          "/a/./b"; "/a/../b"; "/./../"; "/../../"
          "./x"; "../x"]),
      {(prefix, suffix) => prefix + suffix })

primitive \nodoc\ _AnyPathGenerator
  """
  Generates a mix of dot-containing paths and plain paths (no dots).
  Used for the idempotency property test.
  """
  fun apply(): Generator[String val] =>
    Generators.frequency[String val](
      [
        as WeightedGenerator[String val]:
        (1, _DotPathGenerator())
        (1, Generators.one_of[String val](
          [ "/a/b/c"; "/foo"; "bar/baz"; ""; "/"; "/index.html"
            "/a.b/c.d"; "relative"; "/path/to/resource"
            "no-dots-here"; "/simple"]))
      ])
