use "format"
use "pony_check"
use "pony_test"

// ---------------------------------------------------------------------------
// Test helper: recording implementation of _ResponseParserNotify
// ---------------------------------------------------------------------------

class \nodoc\ _TestResponseParserNotify is _ResponseParserNotify
  embed responses: Array[(U16, String val, Version, Headers val)] =
    Array[(U16, String val, Version, Headers val)]
  embed body_chunks: Array[Array[U8] val] = Array[Array[U8] val]
  var completed: USize = 0
  embed errors: Array[ParseError] = Array[ParseError]

  fun ref response_received(
    status: U16,
    reason: String val,
    version: Version,
    headers: Headers val)
  =>
    responses.push((status, reason, version, headers))

  fun ref body_chunk(data: Array[U8] val) =>
    body_chunks.push(data)

  fun ref response_complete() =>
    completed = completed + 1

  fun ref parse_error(err: ParseError) =>
    errors.push(err)

  fun ref collected_body_string(): String val =>
    """
    Concatenate all body chunks into a single string.
    """
    let out = String
    for chunk in body_chunks.values() do
      out.append(chunk)
    end
    out.clone()

// ---------------------------------------------------------------------------
// Property-based tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _PropertyValidStatusLineParsesCorrectly
  is Property1[(U16, String val)]
  """
  Valid (status, reason) pairs serialized as HTTP/1.1 status lines parse
  correctly, delivering response_received with matching values and then
  response_complete.
  """
  fun name(): String => "parser/valid_status_line"

  fun gen(): Generator[(U16, String val)] =>
    Generators.zip2[U16, String val](
      Generators.u16(200, 599),
      Generators.one_of[String val](
        [ "OK"; "Not Found"; "Internal Server Error"
          "Created"; "Moved Permanently"; "" ]))

  fun ref property(arg1: (U16, String val), ph: PropertyHelper) =>
    (let status, let reason) = arg1
    let raw: String val =
      "HTTP/1.1 " + status.string() + " " + reason +
      "\r\nContent-Length: 0\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    ph.assert_eq[USize](
      1, notify.responses.size(), "should have 1 response")
    ph.assert_eq[USize](
      1, notify.completed, "should have 1 completion")
    ph.assert_eq[USize](
      0, notify.errors.size(), "should have 0 errors")
    try
      (let s, let r, let v, _) = notify.responses(0)?
      ph.assert_eq[U16](status, s, "status mismatch")
      ph.assert_eq[String val](reason, r, "reason mismatch")
      ph.assert_true(v is HTTP11, "version should be HTTP/1.1")
    else
      ph.fail("could not read response")
    end

class \nodoc\ iso _PropertyInvalidStatusLineRejected
  is Property1[String val]
  """
  Malformed status lines produce parse errors.
  """
  fun name(): String => "parser/invalid_status_line_rejected"

  fun gen(): Generator[String val] =>
    Generators.frequency[String val](
      [ as WeightedGenerator[String val]:
        // No space after version
        (1, Generators.unit[String val]("HTTP/1.1200 OK\r\n\r\n"))
        // Non-numeric status
        (1, Generators.unit[String val]("HTTP/1.1 abc OK\r\n\r\n"))
        // Too short
        (1, Generators.unit[String val]("HTTP/1.1\r\n\r\n"))
        // Two-digit status
        (1, Generators.unit[String val]("HTTP/1.1 20 OK\r\n\r\n"))
        // Random garbage
        (1, Generators.ascii(5, 30)
          .map[String val]({(s) => s + "\r\n\r\n" }))
      ])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover arg1.array().clone() end)

    ph.assert_eq[USize](
      0,
      notify.responses.size(),
      "should have 0 responses for: " + arg1)
    ph.assert_eq[USize](
      1,
      notify.errors.size(),
      "should have 1 error for: " + arg1)

class \nodoc\ iso _PropertyHeadersRoundtrip
  is Property1[Array[(String val, String val)] ref]
  """
  Headers in a response are correctly parsed and available in the
  delivered Headers collection.
  """
  fun name(): String => "parser/headers_roundtrip"

  fun gen(): Generator[Array[(String val, String val)] ref] =>
    let pair_gen =
      Generators.zip2[String val, String val](
        Generators.ascii_letters(1, 10),
        Generators.ascii_letters(1, 20))
    Generators.array_of[
      (String val, String val)](pair_gen, 1, 5)

  fun ref property(
    arg1: Array[(String val, String val)] ref,
    ph: PropertyHelper)
  =>
    var raw: String val = "HTTP/1.1 200 OK\r\n"
    raw = raw + "Content-Length: 0\r\n"
    for (hdr_name, hdr_value) in arg1.values() do
      raw = raw + hdr_name + ": " + hdr_value + "\r\n"
    end
    raw = raw + "\r\n"

    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    ph.assert_eq[USize](
      1, notify.responses.size(), "should have 1 response")
    try
      (_, _, _, let headers) = notify.responses(0)?
      let seen = Array[String val]
      for (hdr_name, hdr_value) in arg1.values() do
        let lower: String val = hdr_name.lower()
        // Skip "content-length" since we added it ourselves
        if lower == "content-length" then continue end
        var already_seen = false
        for s in seen.values() do
          if s == lower then already_seen = true; break end
        end
        if not already_seen then
          seen.push(lower)
          match \exhaustive\ headers.get(hdr_name)
          | let v: String val =>
            ph.assert_eq[String val](
              hdr_value,
              v,
              "header " + hdr_name + " mismatch")
          | None =>
            ph.fail("header " + hdr_name + " not found")
          end
        end
      end
    else
      ph.fail("could not read response")
    end

class \nodoc\ iso _PropertyFixedBodyDelivered
  is Property1[USize]
  """
  Responses with Content-Length have their body delivered completely via
  body_chunk callbacks, followed by response_complete.
  """
  fun name(): String => "parser/fixed_body_delivered"

  fun gen(): Generator[USize] =>
    Generators.usize(1, 200)

  fun ref property(arg1: USize, ph: PropertyHelper) =>
    let body =
      recover val
        let b = Array[U8](arg1)
        var i: USize = 0
        while i < arg1 do
          b.push('A' + (i % 26).u8())
          i = i + 1
        end
        b
      end

    let raw =
      recover val
        let r = String
          .> append("HTTP/1.1 200 OK\r\n")
          .> append("Content-Length: " + arg1.string() + "\r\n")
          .> append("\r\n")
          .> append(body)
        r
      end

    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    ph.assert_eq[USize](
      1, notify.responses.size(), "should have 1 response")
    ph.assert_eq[USize](
      1, notify.completed, "should have 1 completion")
    ph.assert_eq[USize](
      0, notify.errors.size(), "should have 0 errors")

    var total_body: USize = 0
    for chunk in notify.body_chunks.values() do
      total_body = total_body + chunk.size()
    end
    ph.assert_eq[USize](arg1, total_body, "body size mismatch")

    var byte_idx: USize = 0
    for chunk in notify.body_chunks.values() do
      try
        var ci: USize = 0
        while ci < chunk.size() do
          ph.assert_eq[U8](
            body(byte_idx)?,
            chunk(ci)?,
            "body byte mismatch at " + byte_idx.string())
          byte_idx = byte_idx + 1
          ci = ci + 1
        end
      end
    end

class \nodoc\ iso _PropertyChunkedBodyDelivered
  is Property1[Array[USize] ref]
  """
  Chunked transfer encoding delivers the complete body and
  response_complete.
  """
  fun name(): String => "parser/chunked_body_delivered"

  fun gen(): Generator[Array[USize] ref] =>
    Generators.array_of[USize](Generators.usize(1, 50), 1, 5)

  fun ref property(arg1: Array[USize] ref, ph: PropertyHelper) =>
    var total_size: USize = 0
    var raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: chunked\r\n" +
      "\r\n"

    for size in arg1.values() do
      let chunk_data: String val =
        recover val
          let s = String(size)
          var i: USize = 0
          while i < size do
            s.push('A' + (i % 26).u8())
            i = i + 1
          end
          s
        end
      let hex_size: String val =
        Format.int[USize](size where fmt = FormatHexBare)
      raw = raw + hex_size + "\r\n" + chunk_data + "\r\n"
      total_size = total_size + size
    end
    raw = raw + "0\r\n\r\n"

    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    ph.assert_eq[USize](
      1, notify.responses.size(), "should have 1 response")
    ph.assert_eq[USize](
      1, notify.completed, "should have 1 completion")
    ph.assert_eq[USize](
      0, notify.errors.size(), "should have 0 errors")

    var total_received: USize = 0
    for chunk in notify.body_chunks.values() do
      total_received = total_received + chunk.size()
    end
    ph.assert_eq[USize](
      total_size,
      total_received,
      "total body size mismatch")

class \nodoc\ iso _PropertyStatusLineBoundary
  is Property1[(String val, Bool)]
  """
  Mixed valid/invalid status lines: valid ones produce response_received,
  invalid ones produce parse_error.
  """
  fun name(): String => "parser/status_line_boundary"

  fun gen(): Generator[(String val, Bool)] =>
    let valid_gen: Generator[(String val, Bool)] =
      Generators.one_of[String val](
        [ "200"; "201"; "204"; "301"; "302"; "304"
          "400"; "401"; "403"; "404"; "500"; "502"
          "503"; "599" ])
        .map[(String val, Bool)]({(status) =>
          ("HTTP/1.1 " + status +
            " OK\r\nContent-Length: 0\r\n\r\n", true)
        })

    let invalid_gen: Generator[(String val, Bool)] =
      Generators.frequency[String val](
        [ as WeightedGenerator[String val]:
          (1, Generators.unit[String val](
            "HTTP/2.0 200 OK\r\n\r\n"))
          (1, Generators.unit[String val](
            "HTTP/1.1 abc OK\r\n\r\n"))
          (1, Generators.unit[String val](
            "GARBAGE\r\n\r\n"))
          (1, Generators.unit[String val](
            "HTTP/1.1\r\n\r\n"))
        ]).map[(String val, Bool)](
          {(s) => (s, false) })

    Generators.frequency[(String val, Bool)](
      [ as WeightedGenerator[(String val, Bool)]:
        (1, valid_gen)
        (1, invalid_gen)
      ])

  fun ref property(arg1: (String val, Bool), ph: PropertyHelper) =>
    (let raw, let should_succeed) = arg1
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    if should_succeed then
      ph.assert_eq[USize](
        1,
        notify.responses.size(),
        "valid response should parse")
      ph.assert_eq[USize](
        0,
        notify.errors.size(),
        "valid response should have no errors")
    else
      ph.assert_eq[USize](
        1,
        notify.errors.size(),
        "invalid response should produce error")
    end

// ---------------------------------------------------------------------------
// Example-based tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _TestParserKnownGoodResponses is UnitTest
  """
  Verify exact callback sequences for well-known HTTP responses.
  """
  fun name(): String => "parser/known_good_responses"

  fun apply(h: TestHelper) =>
    _test_simple_200(h)
    _test_404_with_body(h)
    _test_301_redirect(h)

  fun _test_simple_200(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 5\r\n" +
      "\r\n" +
      "Hello"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "200: 1 response")
    h.assert_eq[USize](1, notify.completed, "200: 1 completion")
    try
      (let s, let r, let v, _) = notify.responses(0)?
      h.assert_eq[U16](200, s, "status should be 200")
      h.assert_eq[String val]("OK", r, "reason should be OK")
      h.assert_true(v is HTTP11, "version should be HTTP/1.1")
    else
      h.fail("200: could not read response")
    end
    h.assert_eq[String val](
      "Hello", notify.collected_body_string(), "body should be Hello")

  fun _test_404_with_body(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 404 Not Found\r\n" +
      "Content-Length: 9\r\n" +
      "\r\n" +
      "Not Found"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "404: 1 response")
    h.assert_eq[USize](1, notify.completed, "404: 1 completion")
    try
      (let s, let r, _, _) = notify.responses(0)?
      h.assert_eq[U16](404, s, "status should be 404")
      h.assert_eq[String val]("Not Found", r)
    else
      h.fail("404: could not read response")
    end
    h.assert_eq[String val](
      "Not Found", notify.collected_body_string())

  fun _test_301_redirect(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 301 Moved Permanently\r\n" +
      "Location: https://example.com/new\r\n" +
      "Content-Length: 0\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "301: 1 response")
    h.assert_eq[USize](1, notify.completed, "301: 1 completion")
    try
      (let s, _, _, let hdrs) = notify.responses(0)?
      h.assert_eq[U16](301, s)
      h.assert_eq[String val](
        "https://example.com/new",
        match \exhaustive\ hdrs.get("Location")
        | let v: String val => v
        else "" end)
    else
      h.fail("301: could not read response")
    end

class \nodoc\ iso _TestIncrementalByteByByte is UnitTest
  """
  Feed a complete response one byte at a time. Verify the parser produces
  the same result as feeding all at once.
  """
  fun name(): String => "parser/incremental_byte_by_byte"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 5\r\n" +
      "\r\n" +
      "Hello"

    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)

    var i: USize = 0
    let arr = raw.array()
    while i < arr.size() do
      try
        let byte =
          recover iso
            let a = Array[U8](1)
              .> push(arr(i)?)
            a
          end
        parser.parse(consume byte)
      end
      i = i + 1
    end

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    h.assert_eq[USize](0, notify.errors.size(), "0 errors")
    h.assert_eq[String val](
      "Hello", notify.collected_body_string(), "body should be Hello")

class \nodoc\ iso _TestSizeLimitStatusLine is UnitTest
  """Status line exceeding small limit -> TooLarge."""
  fun name(): String => "parser/size_limit_status_line"

  fun apply(h: TestHelper) =>
    let config = _ParserConfig(where max_status_line_size' = 16)
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify, config)

    let raw = "HTTP/1.1 200 OK with a long reason\r\n\r\n"
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.errors.size(), "should have 1 error")
    try
      h.assert_true(notify.errors(0)? is TooLarge, "should be TooLarge")
    end

class \nodoc\ iso _TestSizeLimitHeaders is UnitTest
  """Headers exceeding small limit -> TooLarge."""
  fun name(): String => "parser/size_limit_headers"

  fun apply(h: TestHelper) =>
    let config = _ParserConfig(where max_header_size' = 32)
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify, config)

    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "X-Very-Long-Header-Name: very-long-value-that-exceeds-limit\r\n" +
      "\r\n"
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.errors.size(), "should have 1 error")
    try
      h.assert_true(notify.errors(0)? is TooLarge, "should be TooLarge")
    end

class \nodoc\ iso _TestSizeLimitBody is UnitTest
  """Content-Length or chunked body exceeding limit -> BodyTooLarge."""
  fun name(): String => "parser/size_limit_body"

  fun apply(h: TestHelper) =>
    let config = _ParserConfig(where max_body_size' = 5)

    // Fixed body: Content-Length 10 > max_body_size 5
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify, config)
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 10\r\n\r\n" +
      "0123456789"
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](
      1,
      notify.errors.size(),
      "fixed body: should have 1 error")
    try
      h.assert_true(
        notify.errors(0)? is BodyTooLarge,
        "fixed body: should be BodyTooLarge")
    end

    // Chunked body: 10 bytes > max_body_size 5
    let notify2: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser2 = _ResponseParser(notify2, config)
    let raw2: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: chunked\r\n\r\n" +
      "a\r\n0123456789\r\n0\r\n\r\n"
    parser2.parse(recover raw2.array().clone() end)

    h.assert_eq[USize](
      1,
      notify2.errors.size(),
      "chunked body: should have 1 error")
    try
      h.assert_true(
        notify2.errors(0)? is BodyTooLarge,
        "chunked body: should be BodyTooLarge")
    end

class \nodoc\ iso _TestSizeLimitCloseDelimitedBody is UnitTest
  """Close-delimited body exceeding limit -> BodyTooLarge."""
  fun name(): String => "parser/size_limit_close_delimited_body"

  fun apply(h: TestHelper) =>
    let config = _ParserConfig(where max_body_size' = 5)
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify, config)

    // Response with no CL/chunked => close-delimited
    let raw: String val =
      "HTTP/1.1 200 OK\r\n\r\n" +
      "0123456789"  // 10 bytes > max 5
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.errors.size(), "should have 1 error")
    try
      h.assert_true(
        notify.errors(0)? is BodyTooLarge,
        "should be BodyTooLarge")
    end

class \nodoc\ iso _TestInvalidContentLength is UnitTest
  """Non-numeric Content-Length -> InvalidContentLength."""
  fun name(): String => "parser/invalid_content_length"

  fun apply(h: TestHelper) =>
    let raw = "HTTP/1.1 200 OK\r\nContent-Length: abc\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.errors.size(), "should have 1 error")
    try
      h.assert_true(
        notify.errors(0)? is InvalidContentLength,
        "should be InvalidContentLength")
    end

class \nodoc\ iso _TestInvalidChunkSize is UnitTest
  """Non-hex chunk size -> InvalidChunk."""
  fun name(): String => "parser/invalid_chunk_size"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: chunked\r\n\r\n" +
      "xyz\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.errors.size(), "should have 1 error")
    try
      h.assert_true(
        notify.errors(0)? is InvalidChunk,
        "should be InvalidChunk")
    end

class \nodoc\ iso _TestChunkedWithTrailers is UnitTest
  """Chunked response with trailer headers -> trailers skipped, completes."""
  fun name(): String => "parser/chunked_with_trailers"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: chunked\r\n\r\n" +
      "5\r\nHello\r\n" +
      "0\r\n" +
      "Trailer-Header: value\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    h.assert_eq[USize](0, notify.errors.size(), "0 errors")
    h.assert_eq[String val](
      "Hello", notify.collected_body_string())

class \nodoc\ iso _TestHTTP10Version is UnitTest
  """HTTP/1.0 response -> version is HTTP10."""
  fun name(): String => "parser/http10_version"

  fun apply(h: TestHelper) =>
    let raw = "HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size())
    try
      h.assert_true(
        notify.responses(0)?._3 is HTTP10,
        "version should be HTTP/1.0")
    end

class \nodoc\ iso _TestInvalidVersion is UnitTest
  """Bad version string -> InvalidVersion."""
  fun name(): String => "parser/invalid_version"

  fun apply(h: TestHelper) =>
    _test_version(h, "HTTP/2.0")
    _test_version(h, "HTTP/1.2")
    _test_version(h, "HTXP/1.1")
    _test_version(h, "HTTP/1")

  fun _test_version(h: TestHelper, ver: String val) =>
    let raw: String val = ver + " 200 OK\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)
    h.assert_eq[USize](
      1,
      notify.errors.size(),
      "should have 1 error for " + ver)
    try
      let err = notify.errors(0)?
      h.assert_true(
        (err is InvalidVersion) or (err is InvalidStatusLine),
        "should be InvalidVersion or InvalidStatusLine for " + ver)
    end

class \nodoc\ iso _TestNoBodyHead is UnitTest
  """HEAD response with Content-Length -> body suppressed."""
  fun name(): String => "parser/no_body_head"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 1000\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.expect_response(HEAD)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    h.assert_eq[USize](0, notify.body_chunks.size(), "0 body chunks")

class \nodoc\ iso _TestNoBody204 is UnitTest
  """204 No Content -> no body regardless of headers."""
  fun name(): String => "parser/no_body_204"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 204 No Content\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    h.assert_eq[USize](0, notify.body_chunks.size(), "0 body chunks")

class \nodoc\ iso _TestNoBody304 is UnitTest
  """304 Not Modified -> no body regardless of headers."""
  fun name(): String => "parser/no_body_304"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 304 Not Modified\r\n" +
      "Content-Length: 500\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    h.assert_eq[USize](0, notify.body_chunks.size(), "0 body chunks")

class \nodoc\ iso _TestCloseDelimitedBody is UnitTest
  """
  Response with no CL/chunked: body delivered via chunks, completed by
  connection_closed().
  """
  fun name(): String => "parser/close_delimited_body"

  fun apply(h: TestHelper) =>
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)

    // First chunk of data
    let raw: String val = "HTTP/1.1 200 OK\r\n\r\nHello"
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](
      0,
      notify.completed,
      "not completed yet (close-delimited)")

    // More data
    parser.parse(recover iso " World".array().clone() end)

    // Connection close signals end of body
    parser.connection_closed()

    h.assert_eq[USize](1, notify.completed, "completed after close")
    h.assert_eq[String val](
      "Hello World", notify.collected_body_string())

class \nodoc\ iso _TestContentLengthZero is UnitTest
  """Content-Length: 0 -> response_complete immediately (no body)."""
  fun name(): String => "parser/content_length_zero"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 0\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    h.assert_eq[USize](0, notify.body_chunks.size(), "0 body chunks")

class \nodoc\ iso _TestContentLengthAndChunked is UnitTest
  """
  Both Content-Length and Transfer-Encoding: chunked -> chunked takes
  precedence per RFC 7230 section 3.3.3.
  """
  fun name(): String => "parser/content_length_and_chunked"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 100\r\n" +
      "Transfer-Encoding: chunked\r\n" +
      "\r\n" +
      "5\r\nHello\r\n0\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    h.assert_eq[USize](0, notify.errors.size(), "0 errors")
    // Body is 5 bytes (chunked), not 100 (Content-Length)
    h.assert_eq[String val](
      "Hello", notify.collected_body_string())

class \nodoc\ iso _TestDuplicateContentLength is UnitTest
  """Two Content-Length headers with differing values -> error."""
  fun name(): String => "parser/duplicate_content_length"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 10\r\n" +
      "Content-Length: 20\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.errors.size(), "should have 1 error")
    try
      h.assert_true(
        notify.errors(0)? is InvalidContentLength,
        "should be InvalidContentLength")
    end

class \nodoc\ iso _TestDataAfterError is UnitTest
  """
  Feed data after a parse error -> no additional callbacks fired
  (verifies _failed short-circuit).
  """
  fun name(): String => "parser/data_after_error"

  fun apply(h: TestHelper) =>
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)

    // Cause an error
    let bad = "GARBAGE_NOT_HTTP\r\n\r\n"
    parser.parse(recover bad.array().clone() end)
    h.assert_eq[USize](1, notify.errors.size(), "1 error after bad data")

    // Feed valid data after error
    let good = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"
    parser.parse(recover good.array().clone() end)

    // Should still have exactly 1 error and 0 responses
    h.assert_eq[USize](
      1,
      notify.errors.size(),
      "still 1 error after second parse")
    h.assert_eq[USize](
      0,
      notify.responses.size(),
      "0 responses (parser is failed)")

class \nodoc\ iso _Test1xxSkipped is UnitTest
  """100 Continue followed by 200 OK: only 200 delivered to handler."""
  fun name(): String => "parser/1xx_skipped"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 100 Continue\r\n" +
      "\r\n" +
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 2\r\n" +
      "\r\n" +
      "OK"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    // Only the 200 should be delivered
    h.assert_eq[USize](
      1,
      notify.responses.size(),
      "should have 1 response (not 2)")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    try
      h.assert_eq[U16](
        200,
        notify.responses(0)?._1,
        "delivered response should be 200")
    end
    h.assert_eq[String val](
      "OK", notify.collected_body_string())

class \nodoc\ iso _TestMultiple1xx is UnitTest
  """Multiple 1xx responses before final response."""
  fun name(): String => "parser/multiple_1xx"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 100 Continue\r\n\r\n" +
      "HTTP/1.1 102 Processing\r\n\r\n" +
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 4\r\n" +
      "\r\n" +
      "Done"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](
      1, notify.responses.size(), "should have 1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    try
      h.assert_eq[U16](200, notify.responses(0)?._1)
    end
    h.assert_eq[String val](
      "Done", notify.collected_body_string())

