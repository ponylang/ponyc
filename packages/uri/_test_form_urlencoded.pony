use "pony_test"
use "pony_check"

class \nodoc\ iso _PropertyFormURLEncodedRoundtrip
  is Property1[Array[(String val, String val)] val]
  """
  Generated key-value pairs serialized as `k=v&k2=v2` parse back to
  matching pairs.
  """
  fun name(): String => "uri/form_urlencoded/roundtrip"

  fun gen(): Generator[Array[(String val, String val)] val] =>
    let key_gen =
      Generators.one_of[String val](
        ["key"; "name"; "q"; "page"; "id"; "foo"; "bar"])
    let val_gen =
      Generators.one_of[String val](
        ["value"; "1"; "hello"; ""; "test"; "42"; "abc"])
    let pair_gen = Generators.zip2[String val, String val](key_gen, val_gen)
    Generators.array_of[
      (String val, String val)](pair_gen where from = 0, to = 5)
      .map[Array[(String val, String val)] val](
        {(arr: Array[(String val, String val)] ref)
          : Array[(String val, String val)] val
        =>
          let out =
            recover iso Array[(String val, String val)](arr.size())
          end
          for pair in arr.values() do
            out.push(pair)
          end
          consume out
        })

  fun ref property(
    arg1: Array[(String val, String val)] val,
    ph: PropertyHelper)
  =>
    // Serialize
    let parts = Array[String val](arg1.size())
    for (k, v) in arg1.values() do
      parts.push(k + "=" + v)
    end
    let query = "&".join(parts.values())

    match \exhaustive\ ParseFormURLEncoded(consume query)
    | let parsed: FormURLEncoded val =>
      ph.assert_eq[USize](
        arg1.size(),
        parsed.size(),
        "pair count mismatch")
      var i: USize = 0
      while i < arg1.size() do
        try
          (let ek, let ev) = arg1(i)?
          (let pk, let pv) = parsed(i)?
          ph.assert_eq[String val](
            ek, pk, "key mismatch at " + i.string())
          ph.assert_eq[String val](
            ev, pv, "value mismatch at " + i.string())
        end
        i = i + 1
      end
    | let err: InvalidPercentEncoding val =>
      ph.fail("roundtrip parse failed")
    end

class \nodoc\ iso _PropertyFormURLEncodedPlusDecodes is Property1[String val]
  """`+` in query values decodes as space."""
  fun name(): String => "uri/form_urlencoded/plus_decodes"

  fun gen(): Generator[String val] =>
    Generators.one_of[String val](
      ["hello+world"; "a+b+c"; "+"; "no+spaces+here"; "++"])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let query = "key=" + arg1
    match \exhaustive\ ParseFormURLEncoded(consume query)
    | let parsed: FormURLEncoded val =>
      try
        (_, let v) = parsed(0)?
        let exp = String(arg1.size())
        for c in arg1.values() do
          if c == '+' then exp.push(' ') else exp.push(c) end
        end
        ph.assert_eq[String val](
          exp.clone(), v, "+ should decode as space in: " + arg1)
      end
    | let err: InvalidPercentEncoding val =>
      ph.fail("unexpected error for: " + arg1)
    end

class \nodoc\ iso _PropertyFormURLEncodedInvalidRejected
  is Property1[String val]
  """Strings with invalid percent-encoding produce errors."""
  fun name(): String => "uri/form_urlencoded/invalid_rejected"

  fun gen(): Generator[String val] =>
    Generators.one_of[String val](
      [
        "key=%GG"; "k=%2"; "a=b&c=%"; "bad=%XX&good=1"; "%ZZ=val"
      ])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    match \exhaustive\ ParseFormURLEncoded(arg1)
    | let parsed: FormURLEncoded val =>
      ph.fail("expected error for: " + arg1)
    | let err: InvalidPercentEncoding val =>
      ph.assert_true(true)
    end

class \nodoc\ iso _TestFormURLEncodedKnownGood is UnitTest
  """Known form-urlencoded parsing cases."""
  fun name(): String => "uri/form_urlencoded/known_good"

  fun ref apply(h: TestHelper) =>
    // Simple key-value pairs
    _assert_params(
      h, "a=1&b=2", [("a", "1"); ("b", "2")])

    // Plus as space
    _assert_params(
      h, "key=hello+world", [("key", "hello world")])

    // Duplicate keys preserved in order
    _assert_params(
      h, "a=1&a=2", [("a", "1"); ("a", "2")])

    // Empty string produces empty FormURLEncoded
    _assert_params(h, "", Array[(String val, String val)](0))

    // Key without value (no =)
    _assert_params(
      h, "key", [("key", "")])

    // Key with empty value
    _assert_params(
      h, "key=", [("key", "")])

    // Multiple keys without values
    _assert_params(
      h, "a&b&c", [("a", ""); ("b", ""); ("c", "")])

    // Percent-encoded key and value
    _assert_params(
      h, "hello%20world=foo%26bar", [("hello world", "foo&bar")])

    // Value with multiple = signs (only first splits)
    _assert_params(
      h, "key=a=b=c", [("key", "a=b=c")])

    // Mixed forms
    _assert_params(
      h, "a=1&b&c=3", [("a", "1"); ("b", ""); ("c", "3")])

  fun _assert_params(
    h: TestHelper,
    input: String val,
    expected: Array[(String val, String val)] val)
  =>
    match \exhaustive\ ParseFormURLEncoded(input)
    | let parsed: FormURLEncoded val =>
      h.assert_eq[USize](
        expected.size(),
        parsed.size(),
        "pair count mismatch for: " + input)
      var i: USize = 0
      while i < expected.size() do
        try
          (let ek, let ev) = expected(i)?
          (let pk, let pv) = parsed(i)?
          h.assert_eq[String val](
            ek,
            pk,
            "key mismatch at " + i.string() + " for: " + input)
          h.assert_eq[String val](
            ev,
            pv,
            "value mismatch at " + i.string() + " for: " + input)
        end
        i = i + 1
      end
    | let err: InvalidPercentEncoding val =>
      h.fail("parse failed for: " + input + ": " + err.string())
    end

class \nodoc\ iso _TestURIQueryParams is UnitTest
  """URI.query_params() convenience method."""
  fun name(): String => "uri/form_urlencoded/uri_query_params"

  fun ref apply(h: TestHelper) =>
    // URI with query string returns parsed params
    let with_query = URI(None, None, "/path", "a=1&b=2", None)
    match \exhaustive\ with_query.query_params()
    | let params: FormURLEncoded val =>
      h.assert_eq[USize](2, params.size(), "should have 2 params")
      try
        h.assert_eq[String val]("a", params(0)?._1)
        h.assert_eq[String val]("1", params(0)?._2)
        h.assert_eq[String val]("b", params(1)?._1)
        h.assert_eq[String val]("2", params(1)?._2)
      else
        h.fail("could not read params")
      end
    | None =>
      h.fail("expected params, got None")
    end

    // URI without query string returns None
    let no_query = URI(None, None, "/path", None, None)
    match \exhaustive\ no_query.query_params()
    | let _: FormURLEncoded val =>
      h.fail("expected None for no query")
    | None => None // expected
    end

    // URI with empty query string returns empty FormURLEncoded
    let empty_query = URI(None, None, "/path", "", None)
    match \exhaustive\ empty_query.query_params()
    | let params: FormURLEncoded val =>
      h.assert_eq[USize](0, params.size(), "empty query = 0 params")
    | None =>
      h.fail("expected empty FormURLEncoded, got None")
    end

    // URI with invalid percent-encoding in query returns None
    let bad_encoding = URI(None, None, "/path", "key=%GG", None)
    match \exhaustive\ bad_encoding.query_params()
    | let _: FormURLEncoded val =>
      h.fail("expected None for bad encoding")
    | None => None // expected
    end

class \nodoc\ iso _TestFormURLEncodedGet is UnitTest
  """FormURLEncoded.get() returns first value for key."""
  fun name(): String => "uri/form_urlencoded/get"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ ParseFormURLEncoded("a=1&b=2&a=3")
    | let params: FormURLEncoded val =>
      // Key present — returns first value
      h.assert_eq[String val](
        "1",
        try params.get("a") as String else "" end,
        "get should return first value for duplicate key")
      h.assert_eq[String val](
        "2",
        try params.get("b") as String else "" end,
        "get should return value for unique key")

      // Key absent — returns None
      match \exhaustive\ params.get("missing")
      | let _: String => h.fail("expected None for missing key")
      | None => None // expected
      end
    | let err: InvalidPercentEncoding val =>
      h.fail("unexpected parse error")
    end

class \nodoc\ iso _TestFormURLEncodedGetAll is UnitTest
  """FormURLEncoded.get_all() returns all values for key."""
  fun name(): String => "uri/form_urlencoded/get_all"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ ParseFormURLEncoded("a=1&b=2&a=3")
    | let params: FormURLEncoded val =>
      // Multiple values
      let a_vals = params.get_all("a")
      h.assert_eq[USize](2, a_vals.size(), "should have 2 values for a")
      try
        h.assert_eq[String val]("1", a_vals(0)?)
        h.assert_eq[String val]("3", a_vals(1)?)
      else
        h.fail("could not read a values")
      end

      // Single value
      let b_vals = params.get_all("b")
      h.assert_eq[USize](1, b_vals.size(), "should have 1 value for b")

      // Absent key
      let missing = params.get_all("missing")
      h.assert_eq[USize](
        0, missing.size(), "absent key should return empty array")
    | let err: InvalidPercentEncoding val =>
      h.fail("unexpected parse error")
    end

class \nodoc\ iso _TestFormURLEncodedContains is UnitTest
  """FormURLEncoded.contains() checks key presence."""
  fun name(): String => "uri/form_urlencoded/contains"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ ParseFormURLEncoded("a=1&b=2")
    | let params: FormURLEncoded val =>
      h.assert_true(params.contains("a"), "should contain a")
      h.assert_true(params.contains("b"), "should contain b")
      h.assert_false(params.contains("c"), "should not contain c")
    | let err: InvalidPercentEncoding val =>
      h.fail("unexpected parse error")
    end

class \nodoc\ iso _TestFormURLEncodedSize is UnitTest
  """FormURLEncoded.size() reflects pair count including duplicates."""
  fun name(): String => "uri/form_urlencoded/size"

  fun ref apply(h: TestHelper) =>
    // Empty
    match \exhaustive\ ParseFormURLEncoded("")
    | let params: FormURLEncoded val =>
      h.assert_eq[USize](0, params.size(), "empty = 0")
    | let err: InvalidPercentEncoding val =>
      h.fail("unexpected parse error")
    end

    // With duplicates — counts each pair
    match \exhaustive\ ParseFormURLEncoded("a=1&a=2&b=3")
    | let params: FormURLEncoded val =>
      h.assert_eq[USize](
        3, params.size(), "duplicates count separately")
    | let err: InvalidPercentEncoding val =>
      h.fail("unexpected parse error")
    end
