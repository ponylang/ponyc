use "pony_test"
use "pony_check"

class \nodoc\ iso _PropertyPathSegmentCount is Property1[String val]
  """
  PathSegments count equals the number of `/`-delimited parts in a valid path.
  """
  fun name(): String => "uri/path_segments/segment_count"

  fun gen(): Generator[String val] =>
    Generators.one_of[String val](
      [
        ""; "/"; "/a"; "/a/b"; "/a/b/c"; "a"; "a/b"; "/a/b/c/d/e"
        "/index.html"; "/path/to/resource"
      ])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    // Count expected segments: split on '/'
    var expected: USize = 1
    for c in arg1.values() do
      if c == '/' then expected = expected + 1 end
    end
    match \exhaustive\ PathSegments(arg1)
    | let segs: Array[String val] val =>
      ph.assert_eq[USize](
        expected, segs.size(), "segment count mismatch for: " + arg1)
    | let err: InvalidPercentEncoding val =>
      ph.fail("unexpected error for: " + arg1)
    end

class \nodoc\ iso _PropertyPathSegmentRoundtrip is Property1[String val]
  """
  Percent-encoding segments and joining with `/` reconstructs a path that
  produces the same segments when re-parsed.
  """
  fun name(): String => "uri/path_segments/roundtrip"

  fun gen(): Generator[String val] =>
    Generators.one_of[String val](
      [
        ""; "/"; "/a"; "/a/b"; "/a/b/c"; "relative"; "a/b"
        "/path/to/resource"; "/index.html"
      ])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    match \exhaustive\ PathSegments(arg1)
    | let segs: Array[String val] val =>
      // Re-encode and join
      let parts = Array[String val](segs.size())
      for seg in segs.values() do
        parts.push(PercentEncode(seg, URIPartPath))
      end
      let rejoined = "/".join(parts.values())
      match \exhaustive\ PathSegments(consume rejoined)
      | let segs2: Array[String val] val =>
        ph.assert_eq[USize](
          segs.size(),
          segs2.size(),
          "roundtrip segment count mismatch for: " + arg1)
        var i: USize = 0
        while i < segs.size() do
          try
            ph.assert_eq[String val](
              segs(i)?,
              segs2(i)?,
              "roundtrip segment " + i.string() +
                " mismatch for: " + arg1)
          end
          i = i + 1
        end
      | let err: InvalidPercentEncoding val =>
        ph.fail("roundtrip reparse failed for: " + arg1)
      end
    | let err: InvalidPercentEncoding val =>
      ph.fail("initial parse failed for: " + arg1)
    end

class \nodoc\ iso _PropertyPathSegmentInvalidRejected
  is Property1[String val]
  """Paths with invalid percent-encoding produce InvalidPercentEncoding."""
  fun name(): String => "uri/path_segments/invalid_rejected"

  fun gen(): Generator[String val] =>
    Generators.one_of[String val](
      [
        "/a%2"; "/a%GG/b"; "/path/%"; "/%XX"; "/a/b%2"
      ])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    match \exhaustive\ PathSegments(arg1)
    | let segs: Array[String val] val =>
      ph.fail("expected error for: " + arg1)
    | let err: InvalidPercentEncoding val =>
      ph.assert_true(true)
    end

class \nodoc\ iso _TestPathSegmentsKnownGood is UnitTest
  """Known path segment decomposition cases."""
  fun name(): String => "uri/path_segments/known_good"

  fun ref apply(h: TestHelper) =>
    // Absolute path
    _assert_segments(h, "/a/b/c", [""; "a"; "b"; "c"])

    // Relative path
    _assert_segments(h, "a/b", ["a"; "b"])

    // Root path
    _assert_segments(h, "/", [""; ""])

    // Empty path
    _assert_segments(h, "", [""])

    // Single segment absolute
    _assert_segments(h, "/index.html", [""; "index.html"])

    // Trailing slash
    _assert_segments(h, "/a/b/", [""; "a"; "b"; ""])

    // Percent-encoded slash preserved within segment
    match \exhaustive\ PathSegments("/a%2Fb/c")
    | let segs: Array[String val] val =>
      h.assert_eq[USize](3, segs.size())
      try
        h.assert_eq[String val]("", segs(0)?)
        h.assert_eq[String val](
          "a/b",
          segs(1)?,
          "%2F should decode to / within segment")
        h.assert_eq[String val]("c", segs(2)?)
      end
    | let err: InvalidPercentEncoding val =>
      h.fail("unexpected error for %2F test")
    end

    // Percent-encoded space
    match \exhaustive\ PathSegments("/hello%20world")
    | let segs: Array[String val] val =>
      try
        h.assert_eq[String val]("hello world", segs(1)?)
      end
    | let err: InvalidPercentEncoding val =>
      h.fail("unexpected error for %20 test")
    end

  fun _assert_segments(
    h: TestHelper,
    input: String val,
    expected: Array[String val] val)
  =>
    match \exhaustive\ PathSegments(input)
    | let segs: Array[String val] val =>
      h.assert_eq[USize](
        expected.size(),
        segs.size(),
        "segment count mismatch for: " + input)
      var i: USize = 0
      while i < expected.size() do
        try
          h.assert_eq[String val](
            expected(i)?,
            segs(i)?,
            "segment " + i.string() + " mismatch for: " + input)
        end
        i = i + 1
      end
    | let err: InvalidPercentEncoding val =>
      h.fail("parse failed for: " + input)
    end
